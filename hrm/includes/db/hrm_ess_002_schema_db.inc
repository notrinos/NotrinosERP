<?php
/** Exact HRM-ESS-002 successor-custody schema contract. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');

/** Return the exact custody table columns. */
function hrm_ess_002_schema_columns()
{
    return array(
        'hrm_ess_002_delegation_capabilities' => array('capability_id', 'delegation_id', 'legal_entity_id', 'capability_code', 'effective_from', 'effective_to', 'grant_reason_sha256', 'capability_sha256', 'granted_by', 'created_at'),
        'hrm_ess_002_delegation_revocations' => array('revocation_id', 'capability_id', 'revoked_at', 'reason_sha256', 'revocation_sha256', 'revoked_by', 'created_at'),
        'hrm_ess_002_command_receipts' => array('receipt_id', 'actor_user_id', 'worker_id', 'assignment_id', 'legal_entity_id', 'command_type', 'idempotency_hash', 'payload_sha256', 'result_json', 'created_at')
    );
}

/** Return required non-primary index definitions. */
function hrm_ess_002_required_indexes()
{
    return array(
        'hrm_ess_002_delegation_capabilities' => array('uq_ess2_cap_sha' => array(1, array('capability_sha256')), 'uq_ess2_cap_scope' => array(1, array('delegation_id', 'legal_entity_id', 'capability_code', 'effective_from')), 'ix_ess2_cap_asof' => array(0, array('delegation_id', 'capability_code', 'effective_from', 'effective_to'))),
        'hrm_ess_002_delegation_revocations' => array('uq_ess2_rev_sha' => array(1, array('revocation_sha256')), 'uq_ess2_rev_cap' => array(1, array('capability_id')), 'ix_ess2_rev_time' => array(0, array('capability_id', 'revoked_at'))),
        'hrm_ess_002_command_receipts' => array('uq_ess2_cmd_idem' => array(1, array('idempotency_hash')), 'ix_ess2_cmd_worker' => array(0, array('worker_id', 'command_type', 'created_at')), 'ix_ess2_cmd_assignment' => array(0, array('assignment_id')), 'ix_ess2_cmd_legal_entity' => array(0, array('legal_entity_id')))
    );
}

/** Return exact required foreign-key projections. */
function hrm_ess_002_required_foreign_keys()
{
    return array(
        'hrm_ess_002_delegation_capabilities' => array('fk_ess2_cap_delegation' => array('delegation_id', 'hrm_ess_manager_delegations', 'delegation_id')),
        'hrm_ess_002_delegation_revocations' => array('fk_ess2_rev_capability' => array('capability_id', 'hrm_ess_002_delegation_capabilities', 'capability_id')),
        'hrm_ess_002_command_receipts' => array('fk_ess2_cmd_worker' => array('worker_id', 'hrm_workers', 'worker_id'), 'fk_ess2_cmd_assignment' => array('assignment_id', 'hrm_assignments', 'assignment_id'), 'fk_ess2_cmd_legal_entity' => array('legal_entity_id', 'hrm_legal_entities', 'legal_entity_id'))
    );
}

/** Return exact expected types for custody keys and foreign-key columns. */
function hrm_ess_002_required_column_types()
{
    return array(
        'hrm_ess_002_delegation_capabilities' => array('capability_id' => 'bigint unsigned', 'delegation_id' => 'bigint unsigned', 'legal_entity_id' => 'bigint unsigned'),
        'hrm_ess_002_delegation_revocations' => array('revocation_id' => 'bigint unsigned', 'capability_id' => 'bigint unsigned'),
        'hrm_ess_002_command_receipts' => array('receipt_id' => 'bigint unsigned', 'worker_id' => 'bigint unsigned', 'assignment_id' => 'bigint unsigned', 'legal_entity_id' => 'bigint unsigned')
    );
}

/** Return the immutable denial-trigger code assigned to each custody table. */
function hrm_ess_002_trigger_codes()
{
    return array('hrm_ess_002_delegation_capabilities' => 'cap', 'hrm_ess_002_delegation_revocations' => 'rev', 'hrm_ess_002_command_receipts' => 'cmd');
}

/** Return column names for an installed ESS-002 table. */
function hrm_ess_002_table_columns($table)
{
    if (!hrm_migration_table_exists($table))
        return false;
    $q = db_query('SHOW COLUMNS FROM '.TB_PREF.$table);
    if (!$q)
        return false;
    $out = array();
    while ($row = db_fetch_assoc($q))
        $out[] = (string)$row['Field'];
    return $out;
}

/** Return normalized SQL column types for an installed table. */
function hrm_ess_002_table_column_types($table)
{
    $physical = hrm_migration_physical_table_name($table);
    if ($physical === false)
        return false;
    $q = db_query('SELECT COLUMN_NAME,COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($physical).' ORDER BY ORDINAL_POSITION');
    if (!$q)
        return false;
    $out = array();
    while ($row = db_fetch_assoc($q))
        $out[(string)$row['COLUMN_NAME']] = hrm_ess_002_normalize_column_type((string)$row['COLUMN_TYPE']);
    return $out;
}

/** Normalize integer display widths so MySQL and MariaDB metadata compare equally. */
function hrm_ess_002_normalize_column_type($type)
{
    $type = strtolower(trim((string)$type));
    return preg_replace('/^((?:tiny|small|medium|big)?int)(?:\([0-9]+\))?(\s+unsigned)?$/', '$1$2', $type);
}

/** Return all non-primary indexes for an installed ESS-002 table. */
function hrm_ess_002_table_indexes($table)
{
    $q = db_query('SHOW INDEX FROM '.TB_PREF.$table);
    if (!$q)
        return false;
    $out = array();
    while ($row = db_fetch_assoc($q)) {
        $name = (string)$row['Key_name'];
        if ($name === 'PRIMARY')
            continue;
        if (!isset($out[$name]))
            $out[$name] = array('unique' => (int)$row['Non_unique'] === 0, 'columns' => array());
        $out[$name]['columns'][(int)$row['Seq_in_index']] = (string)$row['Column_name'];
    }
    foreach ($out as $name => $index) {
        ksort($index['columns']);
        $out[$name]['columns'] = array_values($index['columns']);
    }
    return $out;
}

/** Return foreign-key definitions for an installed ESS-002 table. */
function hrm_ess_002_table_foreign_keys($table)
{
    $physical = hrm_migration_physical_table_name($table);
    if ($physical === false)
        return false;
    $q = db_query('SELECT CONSTRAINT_NAME,COLUMN_NAME,REFERENCED_TABLE_NAME,REFERENCED_COLUMN_NAME FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($physical).' AND REFERENCED_TABLE_NAME IS NOT NULL ORDER BY CONSTRAINT_NAME,ORDINAL_POSITION');
    if (!$q)
        return false;
    $out = array();
    while ($row = db_fetch_assoc($q))
        $out[(string)$row['CONSTRAINT_NAME']] = array((string)$row['COLUMN_NAME'], preg_replace('/^'.preg_quote(hrm_migration_active_table_prefix(), '/').'/', '', (string)$row['REFERENCED_TABLE_NAME']), (string)$row['REFERENCED_COLUMN_NAME']);
    ksort($out);
    return $out;
}

/** Return the storage engine for an installed table. */
function hrm_ess_002_table_engine($table)
{
    $physical = hrm_migration_physical_table_name($table);
    if ($physical === false)
        return false;
    $q = db_query('SELECT ENGINE FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($physical).' LIMIT 1');
    if (!$q || db_num_rows($q) !== 1)
        return false;
    $row = db_fetch_assoc($q);
    return $row ? (string)$row['ENGINE'] : false;
}

/** Return true only when every defined non-primary index matches exactly. */
function hrm_ess_002_indexes_match_contract($table)
{
    $actual = hrm_ess_002_table_indexes($table);
    $required = hrm_ess_002_required_indexes();
    if ($actual === false || !isset($required[$table]) || count($actual) !== count($required[$table]))
        return false;
    foreach ($required[$table] as $name => $shape) {
        if (!isset($actual[$name]) || ($actual[$name]['unique'] ? 1 : 0) !== (int)$shape[0] || $actual[$name]['columns'] !== $shape[1])
            return false;
    }
    return true;
}

/** Return true only when every foreign key matches exactly, with no extras. */
function hrm_ess_002_foreign_keys_match_contract($table)
{
    $actual = hrm_ess_002_table_foreign_keys($table);
    $required = hrm_ess_002_required_foreign_keys();
    if ($actual === false || !isset($required[$table]))
        return false;
    $expected = $required[$table];
    ksort($expected);
    return $actual === $expected;
}

/** Return the exact prefix-aware trigger name for a table code and event. */
function hrm_ess_002_trigger_name($code, $op)
{
    $prefix = hrm_migration_active_table_prefix();
    if ($prefix === false)
        return false;
    return $prefix.'ess2_'.$code.'_'.(strtoupper($op) === 'UPDATE' ? 'u' : 'd');
}

/** Build the exact immutable denial action required for a custody trigger. */
function hrm_ess_002_trigger_contract_action($code, $op)
{
    $labels = array('cap' => 'delegation capability', 'rev' => 'delegation revocation', 'cmd' => 'command receipt');
    if (!isset($labels[$code]))
        return false;
    $verb = strtoupper($op) === 'UPDATE' ? 'updated' : 'deleted';
    return "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-ESS-002 immutable ".$labels[$code].' cannot be '.$verb."'";
}

/** Return valid, absent, conflicting, or unreadable state for one trigger. */
function hrm_ess_002_trigger_state($table, $code, $op)
{
    $physical = hrm_migration_physical_table_name($table);
    $name = hrm_ess_002_trigger_name($code, $op);
    if ($physical === false || $name === false)
        return false;
    $q = db_query('SELECT EVENT_MANIPULATION,EVENT_OBJECT_TABLE,ACTION_TIMING,ACTION_STATEMENT FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA=DATABASE() AND TRIGGER_NAME='.db_escape($name).' LIMIT 2');
    if (!$q)
        return false;
    if (db_num_rows($q) === 0)
        return 'absent';
    if (db_num_rows($q) !== 1)
        return 'conflict';
    $row = db_fetch_assoc($q);
    $actual = strtoupper(rtrim(preg_replace('/\s+/', ' ', trim((string)$row['ACTION_STATEMENT'])), ';'));
    $expected = strtoupper(hrm_ess_002_trigger_contract_action($code, $op));
    return strtoupper((string)$row['EVENT_MANIPULATION']) === strtoupper($op) && (string)$row['EVENT_OBJECT_TABLE'] === $physical && strtoupper((string)$row['ACTION_TIMING']) === 'BEFORE' && $actual === $expected ? 'valid' : 'conflict';
}

/** Return whether one installed trigger exactly matches its immutable contract. */
function hrm_ess_002_trigger_exists($table, $code, $op)
{
    return hrm_ess_002_trigger_state($table, $code, $op) === 'valid';
}

/** Validate a safe, empty partial-table recovery for the pre-fix ESS-002 DDL. */
function hrm_ess_002_recover_empty_partial_entity_type($patch)
{
    $table = 'hrm_ess_002_delegation_capabilities';
    if (!hrm_migration_table_exists($table))
        return true;
    $types = hrm_ess_002_table_column_types($table);
    if ($types === false || !isset($types['legal_entity_id']))
        return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-ESS-002 partial capability table metadata is unreadable.');
    if ($types['legal_entity_id'] === 'bigint unsigned')
        return true;
    if ($types['legal_entity_id'] !== 'int unsigned')
        return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-ESS-002 partial capability table has an unrecognized legal-entity key type.');

    $expected_columns = hrm_ess_002_schema_columns();
    if (hrm_ess_002_table_columns($table) !== $expected_columns[$table]
        || hrm_ess_002_table_engine($table) !== 'InnoDB'
        || !hrm_ess_002_indexes_match_contract($table)
        || !hrm_ess_002_foreign_keys_match_contract($table))
        return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'HRM-ESS-002 partial capability table does not match the recoverable failed-install projection.');

    foreach (array('UPDATE', 'DELETE') as $op) {
        $state = hrm_ess_002_trigger_state($table, 'cap', $op);
        if ($state !== 'absent' && $state !== 'valid')
            return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'HRM-ESS-002 partial capability table has a conflicting immutable trigger.');
    }

    $parent_types = hrm_ess_002_table_column_types('hrm_legal_entities');
    if ($parent_types === false || !isset($parent_types['legal_entity_id']) || $parent_types['legal_entity_id'] !== 'bigint unsigned')
        return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-ESS-002 legal-entity key does not match the registered bigint unsigned contract.');

    $physical = hrm_migration_physical_table_name($table);
    $count = db_query('SELECT COUNT(*) AS row_count FROM `'.$physical.'`');
    if (!$count || db_num_rows($count) !== 1)
        return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-ESS-002 partial capability row count is unreadable.');
    $count_row = db_fetch_assoc($count);
    if (!$count_row || (int)$count_row['row_count'] !== 0)
        return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'HRM-ESS-002 partial capability table is non-empty; type repair is denied.');

    if (!db_query('ALTER TABLE `'.$physical.'` MODIFY COLUMN `legal_entity_id` BIGINT(20) UNSIGNED NOT NULL'))
        return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-ESS-002 empty partial capability type repair failed.');
    $types = hrm_ess_002_table_column_types($table);
    if ($types === false || !isset($types['legal_entity_id']) || $types['legal_entity_id'] !== 'bigint unsigned')
        return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-ESS-002 empty partial capability type repair did not persist.');

    $patch->log_error('Recovered the empty ESS-002 capability table left by the rejected int-to-bigint foreign-key definition.', 'Info');
    return true;
}

/** Require the exact columns, types, indexes, foreign keys, and denial triggers. */
function hrm_ess_002_require_schema($patch, $triggers = true)
{
    $column_types = hrm_ess_002_required_column_types();
    foreach (hrm_ess_002_schema_columns() as $table => $columns) {
        if (hrm_ess_002_table_columns($table) !== $columns)
            return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-ESS-002 table projection mismatch: '.$table);
        $types = hrm_ess_002_table_column_types($table);
        if ($types === false || !isset($column_types[$table]))
            return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-ESS-002 column types are unreadable: '.$table);
        foreach ($column_types[$table] as $name => $expected_type) {
            if (!isset($types[$name]) || $types[$name] !== $expected_type)
                return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-ESS-002 key type mismatch: '.$table.'.'.$name);
        }
        if (hrm_ess_002_table_engine($table) !== 'InnoDB')
            return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-ESS-002 table engine mismatch: '.$table);
        if (!hrm_ess_002_indexes_match_contract($table))
            return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-ESS-002 index projection mismatch: '.$table);
        if (!hrm_ess_002_foreign_keys_match_contract($table))
            return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-ESS-002 FK projection mismatch: '.$table);
        if ($triggers) {
            foreach (array('UPDATE', 'DELETE') as $op) {
                if (!hrm_ess_002_trigger_exists($table, hrm_ess_002_trigger_codes()[$table], $op))
                    return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'HRM-ESS-002 immutable trigger mismatch: '.$table.'.'.$op);
            }
        }
    }
    return true;
}

/** Install or resume the exact ESS-002 custody tables and immutable triggers. */
function hrm_ess_002_install_schema($patch)
{
    global $path_to_root;
    $sql = @file_get_contents($path_to_root.'/sql/upgrade_hrm_ess_002_schema.sql');
    if (!is_string($sql) || $sql === '')
        return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'HRM-ESS-002 schema SQL missing.');
    $prefix = hrm_migration_active_table_prefix();
    if ($prefix === false)
        return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-ESS-002 prefix invalid.');

    $references = array(
        'hrm_ess_manager_delegations' => array('delegation_id' => 'bigint unsigned'),
        'hrm_workers' => array('worker_id' => 'bigint unsigned'),
        'hrm_assignments' => array('assignment_id' => 'bigint unsigned'),
        'hrm_legal_entities' => array('legal_entity_id' => 'bigint unsigned')
    );
    foreach ($references as $table => $columns) {
        $types = hrm_ess_002_table_column_types($table);
        if ($types === false)
            return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-ESS-002 referenced key metadata is unreadable: '.$table);
        foreach ($columns as $name => $expected_type) {
            if (!isset($types[$name]) || $types[$name] !== $expected_type)
                return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-ESS-002 referenced key type mismatch: '.$table.'.'.$name);
        }
    }
    if (!hrm_ess_002_recover_empty_partial_entity_type($patch))
        return false;

    $sql = str_replace('`0_', '`'.$prefix, $sql);
    foreach (preg_split('/;\s*(?:\r?\n|$)/', $sql) as $statement) {
        $statement = trim($statement);
        if ($statement === '')
            continue;
        if (stripos($statement, 'CREATE TABLE `') === 0) {
            $statement = preg_replace('/^CREATE TABLE /i', 'CREATE TABLE IF NOT EXISTS ', $statement, 1);
            if (!db_query($statement))
                return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-ESS-002 schema table recovery failed.');
            continue;
        }
        if (preg_match('/^CREATE TRIGGER `'.preg_quote($prefix, '/').'ess2_(cap|rev|cmd)_([ud])`/i', $statement, $match) === 1) {
            $code = strtolower($match[1]);
            $op = strtolower($match[2]) === 'u' ? 'UPDATE' : 'DELETE';
            $table = array('cap' => 'hrm_ess_002_delegation_capabilities', 'rev' => 'hrm_ess_002_delegation_revocations', 'cmd' => 'hrm_ess_002_command_receipts')[$code];
            $state = hrm_ess_002_trigger_state($table, $code, $op);
            if ($state === 'valid')
                continue;
            if ($state === 'conflict' || $state === false)
                return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'HRM-ESS-002 immutable trigger conflicts with the required contract.');
            if (!db_query($statement))
                return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-ESS-002 immutable trigger recovery failed.');
            continue;
        }
        return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'HRM-ESS-002 schema statement is outside the approved table/trigger contract.');
    }
    return hrm_ess_002_require_schema($patch, true);
}
?>
