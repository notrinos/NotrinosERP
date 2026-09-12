<?php
/**
 * HRM-FND-005 CSV Employee Hire bulk-onboarding adapter foundation.
 *
 * This helper deliberately owns no browser/import route. It accepts only an
 * already-separated list of new-Hire payloads, validates the complete batch
 * before mutation, assigns opaque server-session retry keys per Employee code,
 * and atomically stages/submits one existing central maker/checker draft per
 * row. Final approval/execution remains row-scoped and independent.
 */

if (!function_exists('submit_staged_hrm_lifecycle_employee_hire_for_approval'))
    include_once($path_to_root.'/hrm/includes/db/lifecycle_hire_approval_db.inc');

if (!defined('HRM_LIFECYCLE_HIRE_CSV_SESSION_NAMESPACE'))
    define('HRM_LIFECYCLE_HIRE_CSV_SESSION_NAMESPACE', 'hrm_fnd_005_employee_hire_csv_batch_v1');
if (!defined('HRM_LIFECYCLE_HIRE_CSV_MAX_NEW_HIRE_ROWS'))
    define('HRM_LIFECYCLE_HIRE_CSV_MAX_NEW_HIRE_ROWS', 200);

/** @return array */
function &hrm_lifecycle_hire_csv_session_store()
{
    if (!isset($_SESSION[HRM_LIFECYCLE_HIRE_CSV_SESSION_NAMESPACE])
        || !is_array($_SESSION[HRM_LIFECYCLE_HIRE_CSV_SESSION_NAMESPACE]))
        $_SESSION[HRM_LIFECYCLE_HIRE_CSV_SESSION_NAMESPACE] = array();
    return $_SESSION[HRM_LIFECYCLE_HIRE_CSV_SESSION_NAMESPACE];
}

/** @return string|false */
function hrm_lifecycle_hire_csv_employee_code($employee_id)
{
    $employee_id = trim((string)$employee_id);
    return preg_match('/^[A-Za-z0-9_-]{1,20}$/D', $employee_id) === 1 ? $employee_id : false;
}

/** @return string */
function hrm_lifecycle_hire_csv_slot($employee_id)
{
    return hash('sha256', strtolower((string)$employee_id));
}

/** @return array|false */
function hrm_lifecycle_hire_csv_command_state($command_id)
{
    $command_id = hrm_lifecycle_hire_payload_positive_int($command_id);
    $actor = hrm_lifecycle_hire_payload_actor_id();
    if ($command_id === false || $actor <= 0)
        return false;
    $result = db_query("SELECT hire_command_id,command_status,row_version,requested_by,approval_draft_id,request_hash "
        ."FROM ".TB_PREF."hrm_lifecycle_hire_commands WHERE hire_command_id=".$command_id." LIMIT 1",
        'could not read CSV Employee Hire command state');
    $row = $result ? db_fetch_assoc($result) : false;
    return $row && (int)$row['requested_by'] === $actor ? $row : false;
}

/** @return string|false */
function hrm_lifecycle_hire_csv_retry_key($employee_id)
{
    $employee_id = hrm_lifecycle_hire_csv_employee_code($employee_id);
    if ($employee_id === false || hrm_lifecycle_hire_payload_actor_id() <= 0
        || !hrm_lifecycle_hire_payload_permission())
        return false;
    $store =& hrm_lifecycle_hire_csv_session_store();
    $slot = hrm_lifecycle_hire_csv_slot($employee_id);
    if (isset($store[$slot]) && is_array($store[$slot])) {
        $entry = $store[$slot];
        $key = isset($entry['idempotency_key']) ? (string)$entry['idempotency_key'] : '';
        $command_id = isset($entry['hire_command_id']) ? (int)$entry['hire_command_id'] : 0;
        if ($key === '' || hrm_lifecycle_hire_payload_idempotency_hash($key) === false)
            return false;
        if ($command_id <= 0)
            return $key;
        $state = hrm_lifecycle_hire_csv_command_state($command_id);
        if ($state === false) {
            // A prior outer batch transaction may have rolled back after the
            // session slot was bound. Releasing only that stale slot is safe.
            unset($store[$slot]);
        } elseif (in_array((string)$state['command_status'], array('staged','pending'), true)) {
            return $key;
        } elseif (in_array((string)$state['command_status'], array('completed','rejected','cancelled'), true)) {
            unset($store[$slot]);
        } else {
            return false;
        }
    }
    try {
        $key = 'hire-csv-'.bin2hex(random_bytes(24));
    } catch (Throwable $e) {
        return false;
    }
    $store[$slot] = array('idempotency_key'=>$key,'hire_command_id'=>0);
    return $key;
}

/** @return bool */
function hrm_lifecycle_hire_csv_bind_command($employee_id, $idempotency_key, $command_id)
{
    $employee_id = hrm_lifecycle_hire_csv_employee_code($employee_id);
    $command_id = hrm_lifecycle_hire_payload_positive_int($command_id);
    if ($employee_id === false || $command_id === false
        || hrm_lifecycle_hire_payload_idempotency_hash($idempotency_key) === false)
        return false;
    $store =& hrm_lifecycle_hire_csv_session_store();
    $slot = hrm_lifecycle_hire_csv_slot($employee_id);
    if (!isset($store[$slot]) || !is_array($store[$slot])
        || !isset($store[$slot]['idempotency_key'])
        || !hash_equals((string)$store[$slot]['idempotency_key'], (string)$idempotency_key))
        return false;
    $store[$slot]['hire_command_id'] = (int)$command_id;
    return true;
}

/**
 * Validate every new-Hire row before the first database mutation.
 * Duplicate Employee codes are case-insensitively rejected to match the
 * practical uniqueness boundary of legacy Employee identifiers.
 *
 * @return array|false
 */
function hrm_lifecycle_hire_csv_batch_plan($rows)
{
    if (!is_array($rows) || !$rows || count($rows) > HRM_LIFECYCLE_HIRE_CSV_MAX_NEW_HIRE_ROWS)
        return false;
    $seen = array(); $plan = array();
    foreach ($rows as $index=>$payload) {
        $canonical = hrm_lifecycle_hire_payload_canonicalize($payload);
        if ($canonical === false)
            return false;
        $employee_id = hrm_lifecycle_hire_csv_employee_code($canonical['employee_id']);
        $effective_date = hrm_lifecycle_hire_payload_date($canonical['hire_date']);
        $dedupe = $employee_id === false ? '' : strtolower($employee_id);
        if ($employee_id === false || $effective_date === false || isset($seen[$dedupe]))
            return false;
        $seen[$dedupe] = true;
        $plan[] = array('source_index'=>(int)$index,'employee_id'=>$employee_id,
            'employee_code_hash'=>hash('sha256',$employee_id),'effective_date'=>$effective_date,'payload'=>$canonical);
    }
    return $plan;
}

/** @return bool */
function hrm_lifecycle_hire_csv_pending_payload_matches($command_id, $canonical, $effective_date)
{
    $loaded = load_hrm_lifecycle_employee_hire_payload_for_update($command_id, array('pending'));
    if (!$loaded || !isset($loaded['command'],$loaded['payload']) || !is_array($loaded['payload']))
        return false;
    $expected_json = hrm_lifecycle_hire_payload_canonical_json($canonical);
    $actual_json = hrm_lifecycle_hire_payload_canonical_json($loaded['payload']);
    $company = function_exists('user_company') ? (int)user_company() : 0;
    $expected_hash = $expected_json === false ? false
        : hrm_lifecycle_hire_payload_request_hash($company, $effective_date, $expected_json);
    return is_string($expected_hash) && is_string($actual_json)
        && substr((string)$loaded['command']['effective_at'],0,10) === (string)$effective_date
        && function_exists('domain_audit_hash_equals')
        && domain_audit_hash_equals((string)$loaded['command']['request_hash'], $expected_hash)
        && domain_audit_hash_equals(hash('sha256',$actual_json), hash('sha256',$expected_json));
}

/** @return array|false */
function hrm_lifecycle_hire_csv_submit_one($item)
{
    if (!is_array($item) || !isset($item['employee_id'],$item['effective_date'],$item['payload']))
        return false;
    $key = hrm_lifecycle_hire_csv_retry_key($item['employee_id']);
    if ($key === false)
        return false;
    $store =& hrm_lifecycle_hire_csv_session_store();
    $slot = hrm_lifecycle_hire_csv_slot($item['employee_id']);
    $command_id = isset($store[$slot]['hire_command_id']) ? (int)$store[$slot]['hire_command_id'] : 0;
    if ($command_id > 0) {
        $state = hrm_lifecycle_hire_csv_command_state($command_id);
        if ($state && (string)$state['command_status'] === 'pending') {
            if (!hrm_lifecycle_hire_csv_pending_payload_matches($command_id, $item['payload'], $item['effective_date']))
                return false;
            $submitted = submit_staged_hrm_lifecycle_employee_hire_for_approval($command_id);
            return is_array($submitted) && isset($submitted['status']) && (string)$submitted['status'] === 'pending'
                ? $submitted : false;
        }
    }
    $staged = stage_hrm_lifecycle_employee_hire_payload($item['payload'], $item['effective_date'], $key);
    if (!is_array($staged) || !isset($staged['hire_command_id'])
        || !hrm_lifecycle_hire_csv_bind_command($item['employee_id'], $key, $staged['hire_command_id']))
        return false;
    $submitted = submit_staged_hrm_lifecycle_employee_hire_for_approval($staged['hire_command_id']);
    if (!is_array($submitted) || !isset($submitted['hire_command_id'],$submitted['approval_draft_id'],$submitted['status'])
        || (int)$submitted['hire_command_id'] !== (int)$staged['hire_command_id']
        || (string)$submitted['status'] !== 'pending')
        return false;
    return $submitted;
}

/**
 * Atomically stage + submit one approval draft per new-Hire CSV row.
 * This is ingestion atomicity only. Terminal checker decisions/execution remain
 * independent per Employee and there is intentionally no batch approval draft.
 * No existing-Employee update is accepted by this foundation helper.
 *
 * @return array|false
 */
function submit_hrm_lifecycle_employee_hire_csv_batch($rows)
{
    $plan = hrm_lifecycle_hire_csv_batch_plan($rows);
    if ($plan === false || !hrm_lifecycle_hire_payload_permission()
        || !hrm_lifecycle_hire_approval_workflow_is_maker_checker())
        return false;
    $results = array();
    begin_transaction();
    foreach ($plan as $item) {
        $submitted = hrm_lifecycle_hire_csv_submit_one($item);
        if ($submitted === false) {
            cancel_transaction();
            return false;
        }
        $results[] = array('source_index'=>$item['source_index'],
            'employee_code_hash'=>$item['employee_code_hash'],
            'hire_command_id'=>(int)$submitted['hire_command_id'],
            'approval_draft_id'=>(int)$submitted['approval_draft_id'],
            'status'=>'pending','exact_retry'=>!empty($submitted['exact_retry']));
    }
    commit_transaction();
    return array('status'=>'pending','new_hire_rows'=>count($results),'approval_drafts'=>count($results),
        'batch_submission_atomic'=>true,'terminal_execution_atomicity'=>'per_hire_row','items'=>$results);
}
