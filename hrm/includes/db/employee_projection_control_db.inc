<?php
/** Company-scoped FND-006 activation, read validation and forward recovery. */
include_once(dirname(__FILE__).'/employee_compatibility_projector_db.inc');

/**
 * Inspect current compatibility without copying any legacy fact into an owner.
 *
 * Control/closure callers keep the historical locking behavior. Operational
 * readers use the non-locking path: their purpose is verification, not
 * serialization, and taking Person/Worker/Employee locks for every company row
 * on every page load creates avoidable contention.
 */
function hrm_employee_projection_scan($record = false, $lock_rows = true)
{
    $policy = hrm_employee_projection_policy();
    if (!$policy) return false;
    if ($record) $lock_rows = true;
    $rows = db_query('SELECT employee_number FROM '.TB_PREF.'employees ORDER BY employee_number');
    if (!$rows) return false;
    $summary = array('compared'=>0,'divergent'=>0);
    while ($row = db_fetch_assoc($rows)) {
        $number = (int)$row['employee_number'];
        $binding = $lock_rows
            ? hrm_employment_lock_binding_for_interval_policy($number)
            : get_hrm_person_worker_binding($number, false);
        if ($binding && $binding['retired_at'] !== null) $binding = false;
        $stored = $binding ? hrm_employee_projection_one('SELECT * FROM '.TB_PREF.'employees WHERE employee_number='.$number
            .($lock_rows ? ' FOR UPDATE' : '')) : false;
        $expected = $stored ? hrm_employee_projection_build($number) : false;
        $fields = $expected ? hrm_employee_projection_difference($stored,$expected) : false;
        if ($fields === false) return false;
        ++$summary['compared'];
        if ($fields) ++$summary['divergent'];
        if ($record && !hrm_employee_projection_audit($number, $fields ? 'divergence':'parity', $fields, $policy['mode'])) return false;
    }
    return $summary;
}

/**
 * Produce a deterministic in-memory SHA-256 fingerprint of every table that can
 * change the current Employee compatibility projection.
 *
 * Raw field values are never returned, logged or persisted. The digest is used
 * only as a dirty detector for the reader parity cache. If any relevant state
 * changes, the next active reader performs the full authoritative comparison.
 */
function hrm_employee_projection_state_fingerprint()
{
    if (!function_exists('hash_init') || hrm_person_worker_current_company_id() === false)
        return false;
    $sources = array(
        array('employees','employee_number'),
        array('hrm_employee_worker_map','mapping_id'),
        array('hrm_workers','worker_id'),
        array('hrm_person_canonical_link_workers','link_worker_id'),
        array('hrm_person_canonical_links','link_id'),
        array('hrm_person_names','name_id'),
        array('hrm_employments','employment_id'),
        array('hrm_contracts','contract_id'),
        array('hrm_assignments','assignment_id'),
        array('hrm_worker_profile_versions','profile_version_id'),
        array('hrm_person_contacts','contact_id'),
        array('hrm_person_identifiers','identifier_id'),
        array('hrm_person_addresses','address_id'),
        array('hrm_person_emergency_contacts','emergency_contact_id'),
        array('hrm_person_bank_accounts','person_bank_account_id'),
        array('hrm_lifecycle_rehire_executions','lifecycle_command_id')
    );
    $ctx = hash_init('sha256');
    if ($ctx === false) return false;
    foreach ($sources as $source) {
        list($table,$order) = $source;
        if (!hrm_person_worker_table_exists($table)) return false;
        $rows = db_query('SELECT * FROM '.TB_PREF.$table.' ORDER BY '.$order,
            'could not fingerprint Employee compatibility state');
        if (!$rows) return false;
        hash_update($ctx, "table\0".$table."\0");
        while ($row = db_fetch_assoc($rows)) {
            foreach ($row as $field=>$value) {
                hash_update($ctx, (string)$field."\0");
                if ($value === null) {
                    hash_update($ctx, "N\0");
                } else {
                    $value = (string)$value;
                    hash_update($ctx, 'V'.strlen($value).':');
                    hash_update($ctx, $value);
                    hash_update($ctx, "\0");
                }
            }
            hash_update($ctx, "\n");
        }
    }
    if (function_exists('hrm_person_sensitive_keyring_paths')) {
        $paths = hrm_person_sensitive_keyring_paths();
        if ($paths === false || empty($paths['manifest']) || !is_file($paths['manifest']))
            return false;
        $keyring = @file_get_contents($paths['manifest']);
        if ($keyring === false) return false;
        hash_update($ctx, "keyring\0".strlen($keyring).':');
        hash_update($ctx, $keyring);
    }
    return hash_final($ctx);
}

function hrm_employee_projection_reader_cache_get($policy)
{
    if (!is_array($policy) || !isset($policy['company_id'],$policy['revision'])) return false;
    $company = (int)$policy['company_id'];
    $cache = isset($GLOBALS['hrm_employee_projection_reader_cache'][$company])
        ? $GLOBALS['hrm_employee_projection_reader_cache'][$company] : false;
    if (isset($_SESSION) && is_array($_SESSION)
        && isset($_SESSION['hrm_fnd_006_reader_parity'][$company]))
        $cache = $_SESSION['hrm_fnd_006_reader_parity'][$company];
    return is_array($cache) ? $cache : false;
}

function hrm_employee_projection_reader_cache_set($policy, $fingerprint)
{
    if (!is_array($policy) || !isset($policy['company_id'],$policy['revision'])
        || !is_string($fingerprint) || preg_match('/^[a-f0-9]{64}$/D',$fingerprint) !== 1)
        return false;
    $company = (int)$policy['company_id'];
    $cache = array('revision'=>(int)$policy['revision'],'fingerprint'=>$fingerprint);
    if (!isset($GLOBALS['hrm_employee_projection_reader_cache']) || !is_array($GLOBALS['hrm_employee_projection_reader_cache']))
        $GLOBALS['hrm_employee_projection_reader_cache'] = array();
    $GLOBALS['hrm_employee_projection_reader_cache'][$company] = $cache;
    if (isset($_SESSION) && is_array($_SESSION)) {
        if (!isset($_SESSION['hrm_fnd_006_reader_parity']) || !is_array($_SESSION['hrm_fnd_006_reader_parity']))
            $_SESSION['hrm_fnd_006_reader_parity'] = array();
        $_SESSION['hrm_fnd_006_reader_parity'][$company] = $cache;
    }
    return true;
}

function hrm_employee_projection_reader_cache_clear($company = null)
{
    if ($company === null) $company = hrm_person_worker_current_company_id();
    if ($company === false) return;
    $company = (int)$company;
    if (isset($GLOBALS['hrm_employee_projection_reader_cache'][$company]))
        unset($GLOBALS['hrm_employee_projection_reader_cache'][$company]);
    if (isset($_SESSION) && is_array($_SESSION)
        && isset($_SESSION['hrm_fnd_006_reader_parity'][$company]))
        unset($_SESSION['hrm_fnd_006_reader_parity'][$company]);
}

/** Audited operator scan and revision-bound policy transition; activation requires parity. */
function hrm_employee_projection_control($mode, $revision)
{
    if (!function_exists('user_check_access') || !user_check_access('SA_HRSETTINGS')
        || !user_check_access('SA_EMPLOYEE') || hrm_person_worker_actor_id() <= 0
        || !in_array($mode,array('shadow','active','fallback'),true)) return false;
    begin_transaction();
    $policy = hrm_employee_projection_policy(true);
    $ok = $policy && (int)$policy['revision'] === (int)$revision;
    $scan = $ok ? hrm_employee_projection_scan(true, true) : false;
    $ok = $scan !== false && ($mode !== 'active' || $scan['divergent'] === 0);
    $next_revision = (int)$revision;
    if ($ok && $mode !== $policy['mode']) {
        $ok = db_query('UPDATE '.TB_PREF.'hrm_employee_projection_policy SET mode='.db_escape($mode)
            .',revision=revision+1 WHERE policy_id=1 AND revision='.(int)$revision) !== false;
        if ($ok) {
            ++$next_revision;
            $ok = append_domain_audit_event(array('company_id'=>hrm_person_worker_current_company_id(),
                'legal_entity_id'=>0,'entity_type'=>'hrm_employee_projection_policy','entity_key'=>array('policy_id'=>1),
                'action'=>'hrm.employee_projection.policy.changed','reason_code'=>'hrm_fnd_006_company_control',
                'before'=>array('mode'=>$policy['mode'],'revision'=>(int)$revision),
                'after'=>array('mode'=>$mode,'revision'=>$next_revision),
                'restricted_diff'=>array('compared'=>$scan['compared'],'divergent'=>$scan['divergent'],'legacy_writer_enabled'=>false))) !== false;
        }
    }
    $verified_fingerprint = false;
    if ($ok && $mode === 'active') {
        $verified_fingerprint = hrm_employee_projection_state_fingerprint();
        $ok = $verified_fingerprint !== false;
    }
    if ($ok) {
        commit_transaction();
        $cache_policy = $policy;
        $cache_policy['mode'] = $mode;
        $cache_policy['revision'] = $next_revision;
        if ($mode === 'active') {
            hrm_employee_projection_reader_cache_set($cache_policy, $verified_fingerprint);
        } else {
            hrm_employee_projection_reader_cache_clear((int)$policy['company_id']);
        }
    } else {
        cancel_transaction();
        // A refused/revision-stale control request is a rolled-back no-op. Keep
        // any previously verified active reader cache: the next reader still
        // checks policy revision and the deterministic state fingerprint, so a
        // real policy/data change invalidates it without forcing a full-company
        // rebuild after an unchanged failed request.
    }
    return $ok ? $scan : false;
}

/** Validate one payroll input under the existing caller transaction in every mode. */
function hrm_employee_projection_payroll_ready($employee_id)
{
    $binding = get_hrm_person_worker_binding($employee_id);
    $binding = $binding ? hrm_employment_lock_binding_for_interval_policy((int)$binding['employee_number']) : false;
    if (!$binding || !hrm_employee_projection_policy()) return false;
    $number = (int)$binding['employee_number'];
    $stored = hrm_employee_projection_one('SELECT * FROM '.TB_PREF.'employees WHERE employee_number='.$number.' FOR UPDATE');
    $expected = $stored ? hrm_employee_projection_build($number) : false;
    return $expected && hrm_employee_projection_difference($stored,$expected) === array();
}

/**
 * Require verified active compatibility for current operational readers.
 *
 * Active control and the FND-006 closure already perform a full company scan.
 * Repeating the complete owner/decryption rebuild on every UI request is both
 * redundant and unbounded. A deterministic state fingerprint makes unchanged
 * requests O(table-read) instead of O(employee x owner/decryption queries).
 * Any relevant state change invalidates the cache and forces a fresh, read-only
 * authoritative comparison before the request may continue.
 */
function hrm_employee_projection_require_readable()
{
    $policy = hrm_employee_projection_policy();
    if (!$policy) throw new RuntimeException('Employee compatibility policy is unavailable.');
    if ($policy['mode'] !== 'active') {
        hrm_employee_projection_reader_cache_clear((int)$policy['company_id']);
        return true;
    }

    $fingerprint = hrm_employee_projection_state_fingerprint();
    if ($fingerprint === false)
        throw new RuntimeException('Employee compatibility state fingerprint is unavailable.');
    $cached = hrm_employee_projection_reader_cache_get($policy);
    if ($cached && (int)$cached['revision'] === (int)$policy['revision']
        && isset($cached['fingerprint']) && (string)$cached['fingerprint'] === $fingerprint)
        return true;

    begin_transaction();
    $locked_policy = hrm_employee_projection_policy(true);
    $same_policy = $locked_policy
        && (int)$locked_policy['company_id'] === (int)$policy['company_id']
        && (int)$locked_policy['revision'] === (int)$policy['revision']
        && $locked_policy['mode'] === 'active';
    $scan = $same_policy ? hrm_employee_projection_scan(false, false) : false;
    $verified_fingerprint = $scan !== false && $scan['divergent'] === 0
        ? hrm_employee_projection_state_fingerprint() : false;
    if ($verified_fingerprint !== false) {
        commit_transaction();
        hrm_employee_projection_reader_cache_set($locked_policy, $verified_fingerprint);
        return true;
    }
    cancel_transaction();
    hrm_employee_projection_reader_cache_clear((int)$policy['company_id']);
    throw new RuntimeException('Employee compatibility differs from its owners. Reconcile in Employee Compatibility before continuing.');
}

/** Initialize only empty fresh-install HRM state; upgrade seeding uses its registered edge. */
function initialize_hrm_fresh_employee_projection()
{
    if (!hrm_compatibility_profile_ready()) return false;
    foreach (array('employees','hrm_worker_profile_versions','hrm_employee_projection_policy') as $table) {
        $row = hrm_employee_projection_one('SELECT COUNT(*) n FROM '.TB_PREF.$table);
        if (!$row || (int)$row['n'] !== 0) return false;
    }
    $company = hrm_person_worker_current_company_id();
    return $company !== false && db_query('INSERT INTO '.TB_PREF."hrm_employee_projection_policy(policy_id,company_id,mode,revision) VALUES(1,".(int)$company.",'shadow',1)") !== false;
}
