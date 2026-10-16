<?php
/**
 * Consolidated official 1.0.0 (3ce21901) to 1.1.0 (afb872fa) release migration.
 * Discovered by the existing Software Upgrade screen; one invocation per company.
 * Development-only 1.0.x migrations must not be packaged alongside this installer.
 */
class fa_patch_release_1_1_0 extends fa_patch
{
    public $previous = '1.0';
    public $version = '1.1.0';
    public $description = 'Upgrade official NotrinosERP 1.0.0 to 1.1.0.';
    public $sql = 'upgrade_1_1_0.sql';
    public $max_upgrade_time = 1800;
    private $manifest = null;
    private $schema_started = false;
    private $upgrade_lock = null;
    private $baseline_repairs = array();
    private $manifest_sha256 = 'e3ec8136d15202273025c0ee9e73346024ff936ff8d19b1e1b8194b1e89c0b1b';

    /** Read the immutable release inventory without loading development migrations. */
    function release_manifest()
    {
        global $path_to_root;
        if ($this->manifest === null) {
            $json = @file_get_contents($path_to_root.'/sql/release_1_1_0.json');
            if (!is_string($json) || !hash_equals($this->manifest_sha256,
                hash('sha256', str_replace("\r\n", "\n", $json))))
                return false;
            $this->manifest = json_decode($json, true);
        }
        return is_array($this->manifest) && isset($this->manifest['release'])
            && $this->manifest['release'] === $this->version ? $this->manifest : false;
    }

    /** Quote a physical name from the verified company connection and release inventory. */
    function physical_name($name)
    {
        return '`'.str_replace('`', '``',
            $this->companies[$this->cur_company]['tbpref'].$name).'`';
    }

    /** Read one scalar result; a query failure is never interpreted as an empty result. */
    function scalar($sql)
    {
        $result = db_query($sql);
        if (!$result || !($row = db_fetch_row($result)))
            return false;
        return $row[0];
    }

    /** Verify the exact supported starting schema and package before any database DDL. */
    function prepare()
    {
        global $path_to_root, $db_version, $src_version;
        $this->baseline_repairs = array();
        $manifest = $this->release_manifest();
        if (!$manifest || $db_version !== $this->version || $src_version !== $this->version)
            return $this->log_error('Install the complete official 1.1.0 source package before upgrading.');
        if ((int)user_company() !== (int)$this->cur_company)
            return $this->log_error('Log in to the company being upgraded before selecting Upgrade; HRM conversion is company-scoped.');
        $sql = @file_get_contents($path_to_root.'/sql/'.$this->sql);
        if (!is_string($sql) || !hash_equals($manifest['sql_sha256'], hash('sha256', str_replace("\r\n", "\n", $sql))))
            return $this->log_error('Release SQL is missing or differs from the release inventory.');
        foreach ($manifest['source_sha256'] as $relative => $expected) {
            $source = @file_get_contents($path_to_root.'/'.$relative);
            if (!is_string($source) || !hash_equals($expected, hash('sha256', str_replace("\r\n", "\n", $source))))
                return $this->log_error('Release source is missing or modified: '.$relative);
        }
        foreach (glob($path_to_root.'/sql/upgrade_*.php') as $candidate) {
            if (basename($candidate) !== 'upgrade_1_1_0.php')
                return $this->log_error('This official release cannot be mixed with development migration installers.');
        }
        $database = $this->scalar('SELECT DATABASE()');
        if ($database === false)
            return $this->log_error('Cannot determine the active company database.');
        $this->upgrade_lock = 'nerp-release-'.substr(hash('sha256',
            $database.'|'.$this->companies[$this->cur_company]['tbpref']), 0, 40);
        if ((string)$this->scalar('SELECT GET_LOCK('.db_escape($this->upgrade_lock).',0)') !== '1') {
            $this->upgrade_lock = null;
            return $this->log_error('Another upgrade owns this company database.');
        }
        foreach ($manifest['baseline_columns'] as $table => $expected) {
            $result = db_query('SHOW COLUMNS FROM '.$this->physical_name($table));
            if (!$result)
                return $this->log_error('Official 1.0.0 table is missing: '.$table);
            $actual = array();
            $metadata = array();
            while ($row = db_fetch_assoc($result)) {
                $actual[] = $row['Field'];
                $metadata[$row['Field']] = $row;
            }
            if (!$this->baseline_columns_supported($table, $expected, $actual))
                return $this->log_error('Starting schema differs from official 1.0.0: '.$table
                    .'. Missing columns: '.implode(', ', array_diff($expected, $actual))
                    .'; unexpected columns: '.implode(', ', array_diff($actual, $expected))
                    .'. Only the documented legacy warranty/attachment omissions are supported.');
            if (!$this->queue_baseline_repairs($table, $metadata))
                return false;
        }
        $prefix = $this->companies[$this->cur_company]['tbpref'];
        $existing_tables = $this->schema_object_names('TABLES', 'TABLE_SCHEMA', 'TABLE_NAME');
        if ($existing_tables === false)
            return $this->log_error('Cannot inventory existing company tables.');
        foreach ($manifest['new_tables'] as $table) {
            if (isset($existing_tables[$prefix.$table]))
                return $this->log_error('Release table already exists: '.$table
                    .'. An interrupted upgrade requires recovery from the paired security backup.');
        }
        $existing_objects = array(
            'TRIGGER' => $this->schema_object_names('TRIGGERS', 'TRIGGER_SCHEMA', 'TRIGGER_NAME'),
            'VIEW' => $this->schema_object_names('VIEWS', 'TABLE_SCHEMA', 'TABLE_NAME'),
            'PROCEDURE' => $this->schema_object_names('ROUTINES', 'ROUTINE_SCHEMA', 'ROUTINE_NAME')
        );
        if (in_array(false, $existing_objects, true))
            return $this->log_error('Cannot inventory existing company SQL objects.');
        foreach ($manifest['new_objects'] as $object) {
            if (isset($existing_objects[$object['type']][$prefix.$object['name']]))
                return $this->log_error('Conflicting release SQL object: '.$object['name']);
        }
        $this->log_error('Verified official 1.0.0 schema and afb872fa source inventory.', 'Info');
        return true;
    }

    /** Read each metadata catalog once instead of repeating a scan for every release object. */
    function schema_object_names($catalog, $schema_column, $name_column)
    {
        $result = db_query('SELECT '.$name_column.' FROM INFORMATION_SCHEMA.'.$catalog
            .' WHERE '.$schema_column.'=DATABASE()');
        if (!$result)
            return false;
        $names = array();
        while ($row = db_fetch_row($result))
            $names[$row[0]] = true;
        return $names;
    }

    /** Accept only the exact column omissions documented in pre-1.0.0 commits. */
    function baseline_columns_supported($table, $expected, $actual)
    {
        $legacy_columns = array(
            // 3ed7a7ab: running companies required these two manual ALTERs.
            'warranty_provision_log' => array('dimension_id', 'dimension2_id'),
            // 71cac7cd: legacy employee documents gained core-attachment linkage.
            'employee_documents' => array('attachment_id')
        );
        $missing = array_values(array_diff($expected, $actual));
        $allowed = isset($legacy_columns[$table]) ? $legacy_columns[$table] : array();
        return !array_diff($missing, $allowed)
            && $actual === array_values(array_diff($expected, $missing));
    }

    /** Inventory a historical index; add it after backup or reject a conflicting definition. */
    function queue_baseline_index($table, $name, $column, $unique)
    {
        $result = db_query('SHOW INDEX FROM '.$this->physical_name($table));
        if (!$result)
            return $this->log_error('Cannot inventory legacy index: '.$table.'.'.$name);
        $found = array();
        while ($row = db_fetch_assoc($result)) {
            if ($row['Key_name'] === $name)
                $found[] = $row;
        }
        if ($found) {
            return (count($found) === 1 && $found[0]['Column_name'] === $column
                && (int)$found[0]['Non_unique'] === ($unique ? 0 : 1)
                && $found[0]['Sub_part'] === null)
                || $this->log_error('Conflicting legacy index: '.$table.'.'.$name);
        }
        $this->baseline_repairs[] = 'ALTER TABLE '.$this->physical_name($table)
            .' ADD '.($unique ? 'UNIQUE ' : '').'KEY `'.$name.'` (`'.$column.'`)';
        return true;
    }

    /** Plan the known historical repairs without changing the starting database. */
    function queue_baseline_repairs($table, $columns)
    {
        if ($table === 'warranty_provision_log') {
            $previous = 'gl_trans_id';
            foreach (array('dimension_id', 'dimension2_id') as $column) {
                if (!isset($columns[$column])) {
                    $this->baseline_repairs[] = 'ALTER TABLE '.$this->physical_name($table)
                        .' ADD COLUMN `'.$column.'` INT(11) NOT NULL DEFAULT 0 AFTER `'.$previous.'`';
                } elseif (!preg_match('/^int(?:\(\d+\))?$/i', $columns[$column]['Type'])
                    || $columns[$column]['Null'] !== 'NO'
                    || (string)$columns[$column]['Default'] !== '0') {
                    return $this->log_error('Conflicting legacy warranty dimension: '.$column);
                }
                $previous = $column;
            }
            return $this->queue_baseline_index($table, 'idx_wp_dim', 'dimension_id', false);
        }
        if ($table === 'employee_documents') {
            if (!isset($columns['attachment_id'])) {
                $this->baseline_repairs[] = 'ALTER TABLE '.$this->physical_name($table)
                    .' ADD COLUMN `attachment_id` INT(11) UNSIGNED NULL AFTER `file_path`';
            } elseif (!preg_match('/^int(?:\(\d+\))? unsigned$/i', $columns['attachment_id']['Type'])
                || $columns['attachment_id']['Null'] !== 'YES') {
                return $this->log_error('Conflicting legacy employee-document attachment linkage.');
            }
            return $this->queue_baseline_index($table, 'employee_document_attachment_uq', 'attachment_id', true);
        }
        if ($table === 'attachments') {
            foreach (array('description'=>200, 'unique_name'=>256, 'filename'=>200) as $column => $length) {
                if (!preg_match('/^varchar\((\d+)\)$/i', $columns[$column]['Type'], $match)
                    || $columns[$column]['Null'] !== 'NO' || $columns[$column]['Default'] !== '')
                    return $this->log_error('Conflicting legacy attachment column: '.$column);
                if ((int)$match[1] < $length) {
                    $this->baseline_repairs[] = 'ALTER TABLE '.$this->physical_name($table)
                        .' MODIFY COLUMN `'.$column.'` VARCHAR('.$length.") NOT NULL DEFAULT ''";
                }
            }
        }
        return true;
    }

    /** Always use strict import: duplicate or conflicting schema must stop this release. */
    function sql_install($company, $force = false)
    {
        if (!$this->backup_created || !$this->backup)
            return $this->log_error('Historical schema repairs require a completed security backup.');
        $this->schema_started = true;
        foreach ($this->baseline_repairs as $statement) {
            if (!db_query($statement))
                return $this->log_error('A historical warranty/attachment schema repair failed; restoring the security backup.');
        }
        if ($this->baseline_repairs)
            $this->log_error('Applied '.count($this->baseline_repairs).' documented legacy warranty/attachment schema repairs after backup.', 'Info');
        return parent::sql_install($company, false);
    }

    /** Preserve framework recovery when a conversion helper throws an exception. */
    function upgrade_company($company, $force = false)
    {
        try {
            return parent::upgrade_company($company, false);
        } catch (Exception $exception) {
            return $this->recover_exception();
        } catch (Throwable $exception) {
            return $this->recover_exception();
        }
    }

    /** Keep exception details and restricted field values out of upgrade logs. */
    function recover_exception()
    {
        cancel_transaction();
        $this->log_error('Release execution raised an exception. The release version was not accepted; recovery follows.');
        if ($this->backup_created)
            $this->set_recovery_state('INSTALL_FAILED_AFTER_BACKUP');
        else
            $this->set_recovery_state('PREPARE_FAILED_BEFORE_MUTATION');
        $this->post_install(false);
        return false;
    }

    /** Load existing production conversion helpers only after their tables exist. */
    function load_conversion_helpers()
    {
        global $path_to_root;
        foreach (array('employee_db.inc','grade_db.inc','position_db.inc',
            'employee_document_migration.inc') as $file)
            include_once($path_to_root.'/hrm/includes/db/'.$file);
    }

    /** Seed departments in parent-first order and reject dangling or cyclic hierarchies. */
    function seed_departments()
    {
        $result = db_query('SELECT department_id,parent_department_id FROM '.TB_PREF.'departments ORDER BY department_id');
        if (!$result)
            return false;
        $pending = array();
        while ($row = db_fetch_assoc($result))
            $pending[(int)$row['department_id']] = $row['parent_department_id'];
        $done = array();
        while ($pending) {
            $progress = false;
            foreach ($pending as $id => $parent) {
                if ($parent !== null && (int)$parent !== 0 && !isset($done[(int)$parent]))
                    continue;
                if (!synchronize_hrm_organization_unit_from_department($id, array('release_seed'), 'create'))
                    return false;
                $done[$id] = true;
                unset($pending[$id]);
                $progress = true;
            }
            if (!$progress)
                return $this->log_error('Department hierarchy is cyclic or has a missing parent.');
        }
        return true;
    }

    /** Create normalized reference data through the existing audited projection writers. */
    function seed_reference_data()
    {
        $result = db_query('SELECT name,value FROM '.TB_PREF."sys_prefs WHERE name IN ('coy_name','coy_no','gst_no','domicile','curr_default')");
        if (!$result)
            return false;
        $preferences = array();
        while ($row = db_fetch_assoc($result))
            $preferences[$row['name']] = $row['value'];
        $entity = hrm_legal_entity_projection_from_company_prefs($preferences);
        if (!$entity)
            return $this->log_error('Company preferences cannot produce a valid default Legal Entity.');
        $columns = array();
        $values = array();
        foreach ($entity as $column => $value) {
            $columns[] = '`'.$column.'`';
            $values[] = $value === null ? 'NULL' : db_escape($value);
        }
        if (!db_query('INSERT INTO '.TB_PREF.'hrm_legal_entities ('.implode(',', $columns)
            .',created_by) VALUES ('.implode(',', $values).','.(int)hrm_person_worker_actor_id().')')
            || !$this->seed_departments())
            return false;
        foreach (array(
            array('pay_grades','grade_id','synchronize_hrm_grade_from_pay_grade'),
            array('positions','position_id','synchronize_hrm_position_from_legacy_position')
        ) as $reference) {
            $result = db_query('SELECT '.$reference[1].' FROM '.TB_PREF.$reference[0].' ORDER BY '.$reference[1]);
            if (!$result)
                return false;
            while ($row = db_fetch_assoc($result)) {
                if (!call_user_func($reference[2], (int)$row[$reference[1]], array('release_seed'), 'create'))
                    return false;
            }
        }
        return true;
    }

    /** Preserve each legacy employee and create its audited, encrypted owner projections. */
    function seed_employees($company)
    {
        if (!db_query('INSERT INTO '.TB_PREF.'hrm_employee_projection_policy'
            .'(policy_id,company_id,mode,revision) VALUES(1,'.(int)$company.",'shadow',1)"))
            return false;
        $result = db_query('SELECT * FROM '.TB_PREF.'employees ORDER BY employee_number');
        if (!$result)
            return false;
        $count = 0;
        while ($employee = db_fetch_assoc($result)) {
            set_time_limit($this->max_upgrade_time);
            if (!create_hrm_person_worker_from_employee($employee, true, 'legacy_employee_seed')
                || create_hrm_employment_from_employee($employee, true, 'legacy_employee_seed', true) === false
                || create_hrm_contract_from_employee($employee, true, 'legacy_employee_seed', true) === false
                || create_hrm_assignment_from_employee($employee, true, 'legacy_employee_seed', true) === false
                || !synchronize_hrm_person_sensitive_from_employee($employee, true, 'legacy_employee_seed', false)
                || !synchronize_hrm_person_private_profile_from_employee($employee, true, 'legacy_employee_seed')
                || !synchronize_hrm_person_bank_from_employee($employee, true, 'legacy_employee_seed', false)
                || hrm_compatibility_profile_append($employee, 'migration') === false)
                return $this->log_error('An employee owner projection could not be preserved. No release version was accepted.');
            ++$count;
        }
        $scan = hrm_employee_projection_scan(false);
        if (!$scan || (int)$scan['compared'] !== $count || (int)$scan['divergent'] !== 0)
            return $this->log_error('Employee compatibility parity verification failed.');
        if (!db_query('UPDATE '.TB_PREF."hrm_employee_projection_policy SET mode='active',revision=2 WHERE policy_id=1"))
            return false;
        $this->log_error('Employee owner projections verified: '.(int)$count.'; divergent=0.', 'Info');
        return true;
    }

    /** Complete data conversion and audit before the framework publishes version_id. */
    function install($company, $force = false)
    {
        $this->load_conversion_helpers();
        // Private keys remain outside the web root and must accompany all later backups.
        if (domain_audit_signing_key($company, true) === false
            || hrm_person_sensitive_active_key($company, true) === false)
            return $this->log_error('Cannot initialize protected HRM/audit key storage. Preserve any created keys with the backup.');
        if (!begin_transaction())
            return $this->log_error('Cannot start the release data-conversion transaction.');
        $ok = $this->seed_reference_data() && $this->seed_employees($company);
        $evidence = array();
        if ($ok)
            $ok = migrate_all_employee_document_content($company, 'release_1_1_0', $evidence);
        if ($ok)
            $ok = append_domain_audit_event(array(
                'company_id'=>(int)$company, 'legal_entity_id'=>0,
                'entity_type'=>'software_release', 'entity_key'=>array('version'=>'1.1.0'),
                'action'=>'software.release.upgraded', 'reason_code'=>'release_1_1_0',
                'before'=>array('version'=>'1.0'), 'after'=>array('version'=>'1.1.0'),
                'restricted_diff'=>array('employee_parity_verified'=>true,
                    'payroll_history_rewritten'=>false, 'role_grants_added'=>false))) !== false;
        if (!$ok) {
            cancel_transaction();
            return $this->log_error('Release conversion failed; use the security backup and retain private keys/files for recovery.');
        }
        if (!commit_transaction()) {
            cancel_transaction();
            return $this->log_error('Release data conversion could not be committed.');
        }
        // Only now can an unpredictable, non-null per-document unique key be enforced.
        if (!db_query('ALTER TABLE '.TB_PREF.'employee_documents MODIFY COLUMN access_key char(64) NOT NULL,'
            .' ADD UNIQUE KEY employee_document_access_key_uq (access_key)'))
            return false;
        $this->log_error('Document migration totals: '.json_encode($evidence)
            .'. Source bytes and private recovery files are retained.', 'Info');
        return true;
    }

    /**
     * Remove only release objects proven absent at preflight, then restore the
     * original company. Never remove private keys, source attachments or files.
     */
    function restore_security_backup()
    {
        if (!$this->backup)
            return false;
        if ($this->schema_started) {
            cancel_transaction();
            $manifest = $this->release_manifest();
            $checks = $this->scalar('SELECT @@FOREIGN_KEY_CHECKS');
            if ($checks === false || !db_query('SET FOREIGN_KEY_CHECKS=0'))
                return $this->log_error('Cannot prepare complete release rollback; restore the backup into a clean database.');
            $ok = true;
            foreach (array_reverse($manifest['new_objects']) as $object) {
                if (!db_query('DROP '.$object['type'].' IF EXISTS '.$this->physical_name($object['name'])))
                    $ok = false;
            }
            foreach (array_reverse($manifest['new_tables']) as $table) {
                if (!db_query('DROP TABLE IF EXISTS '.$this->physical_name($table)))
                    $ok = false;
            }
            if (!db_query('SET FOREIGN_KEY_CHECKS='.(int)$checks))
                $ok = false;
            if (!$ok)
                return $this->log_error('Release rollback was incomplete; restore the paired backup into a clean database.');
        }
        return parent::restore_security_backup();
    }

    /** Release the company lock before the framework restores the caller connection. */
    function post_install($result = true)
    {
        // Parent post_install can restore after failure to persist the final version.
        $accepted = parent::post_install($result);
        // set_global_connection closes the old company connection and its named locks.
        $this->upgrade_lock = null;
        return $accepted;
    }

    /** Keep the existing screen action and explain the release and recovery boundary. */
    function show_params($company)
    {
        display_note(_('Official 1.0.0 to 1.1.0: one Upgrade per company. Install the full release files first. Keep a complete database and company-file backup. This creates a security backup, migrates HRM data and verifies employee parity. Keep the resulting private keys and files with future backups.'));
    }
}

$install = new fa_patch_release_1_1_0();
