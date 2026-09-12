<?php
/**
 * HRM-FND-005 Employee Hire browser onboarding adapter.
 *
 * The browser never owns Hire mutation authority. It keeps an opaque
 * server-session idempotency key, stages the exact canonical onboarding payload
 * into the accepted encrypted custody, then binds that command to the accepted
 * central maker/checker approval transaction. No request-controlled command,
 * draft, request-hash, ciphertext, or key identity is accepted.
 */

include_once($path_to_root.'/hrm/includes/db/lifecycle_hire_approval_db.inc');

if (!defined('HRM_LIFECYCLE_HIRE_BROWSER_SESSION_NAMESPACE'))
    define('HRM_LIFECYCLE_HIRE_BROWSER_SESSION_NAMESPACE', 'hrm_fnd_005_employee_hire_browser_v1');

/** @return int */
function hrm_lifecycle_hire_browser_actor_id()
{
    return hrm_lifecycle_hire_payload_actor_id();
}

/** @return string|false */
function hrm_lifecycle_hire_browser_employee_code($employee_id)
{
    $employee_id = trim((string)$employee_id);
    return preg_match('/^[A-Za-z0-9_-]{1,20}$/D', $employee_id) === 1 ? $employee_id : false;
}

/** @return array */
function &hrm_lifecycle_hire_browser_session_store()
{
    if (!isset($_SESSION[HRM_LIFECYCLE_HIRE_BROWSER_SESSION_NAMESPACE])
        || !is_array($_SESSION[HRM_LIFECYCLE_HIRE_BROWSER_SESSION_NAMESPACE]))
        $_SESSION[HRM_LIFECYCLE_HIRE_BROWSER_SESSION_NAMESPACE] = array();
    return $_SESSION[HRM_LIFECYCLE_HIRE_BROWSER_SESSION_NAMESPACE];
}

/** @return array|false */
function hrm_lifecycle_hire_browser_command_state($command_id)
{
    $command_id = hrm_lifecycle_hire_payload_positive_int($command_id);
    $actor = hrm_lifecycle_hire_browser_actor_id();
    if ($command_id === false || $actor <= 0)
        return false;
    $result = db_query("SELECT hire_command_id,command_status,row_version,requested_by,approval_draft_id "
        ."FROM ".TB_PREF."hrm_lifecycle_hire_commands WHERE hire_command_id=".$command_id." LIMIT 1",
        'could not read Employee Hire browser command state');
    $row = $result ? db_fetch_assoc($result) : false;
    if (!$row || (int)$row['requested_by'] !== $actor)
        return false;
    return $row;
}

/**
 * Return one opaque server-owned retry key for an Employee code.
 * Terminal commands release the slot; staged/pending commands retain it.
 *
 * @return string|false
 */
function hrm_lifecycle_hire_browser_idempotency_key($employee_id)
{
    $employee_id = hrm_lifecycle_hire_browser_employee_code($employee_id);
    $actor = hrm_lifecycle_hire_browser_actor_id();
    if ($employee_id === false || $actor <= 0 || !hrm_lifecycle_hire_payload_permission())
        return false;
    $store =& hrm_lifecycle_hire_browser_session_store();
    $slot = hash('sha256', $employee_id);
    if (isset($store[$slot]) && is_array($store[$slot])) {
        $entry = $store[$slot];
        $key = isset($entry['idempotency_key']) ? (string)$entry['idempotency_key'] : '';
        $command_id = isset($entry['hire_command_id']) ? (int)$entry['hire_command_id'] : 0;
        if ($key === '' || hrm_lifecycle_hire_payload_idempotency_hash($key) === false)
            return false;
        if ($command_id <= 0)
            return $key;
        $state = hrm_lifecycle_hire_browser_command_state($command_id);
        if ($state === false)
            return false;
        if (in_array((string)$state['command_status'], array('staged','pending'), true))
            return $key;
        if (!in_array((string)$state['command_status'], array('completed','rejected','cancelled'), true))
            return false;
        unset($store[$slot]);
    }
    try {
        $key = 'hire-browser-'.bin2hex(random_bytes(24));
    } catch (Throwable $e) {
        return false;
    }
    $store[$slot] = array('idempotency_key'=>$key,'hire_command_id'=>0);
    return $key;
}

/** @return bool */
function hrm_lifecycle_hire_browser_bind_command($employee_id, $idempotency_key, $command_id)
{
    $employee_id = hrm_lifecycle_hire_browser_employee_code($employee_id);
    $command_id = hrm_lifecycle_hire_payload_positive_int($command_id);
    if ($employee_id === false || $command_id === false
        || hrm_lifecycle_hire_payload_idempotency_hash($idempotency_key) === false)
        return false;
    $store =& hrm_lifecycle_hire_browser_session_store();
    $slot = hash('sha256', $employee_id);
    if (!isset($store[$slot]) || !is_array($store[$slot])
        || !isset($store[$slot]['idempotency_key'])
        || !hash_equals((string)$store[$slot]['idempotency_key'], (string)$idempotency_key))
        return false;
    $store[$slot]['hire_command_id'] = (int)$command_id;
    return true;
}

/**
 * Stage + submit one browser Hire request. Failed approval submission leaves the
 * exact staged command recoverable through the same server-owned retry key.
 *
 * @return array|false
 */
function submit_hrm_lifecycle_employee_hire_browser($payload, $effective_date)
{
    $canonical = hrm_lifecycle_hire_payload_canonicalize($payload);
    $effective_date = hrm_lifecycle_hire_payload_date($effective_date);
    if ($canonical === false || $effective_date === false
        || (string)$canonical['hire_date'] !== $effective_date)
        return false;
    $key = hrm_lifecycle_hire_browser_idempotency_key($canonical['employee_id']);
    if ($key === false)
        return false;
    $staged = stage_hrm_lifecycle_employee_hire_payload($canonical, $effective_date, $key);
    if (!is_array($staged) || !isset($staged['hire_command_id'])
        || !hrm_lifecycle_hire_browser_bind_command($canonical['employee_id'], $key, $staged['hire_command_id']))
        return false;
    $submitted = submit_staged_hrm_lifecycle_employee_hire_for_approval($staged['hire_command_id']);
    if (!is_array($submitted) || !isset($submitted['hire_command_id'],$submitted['approval_draft_id'])
        || (int)$submitted['hire_command_id'] !== (int)$staged['hire_command_id']
        || (string)$submitted['status'] !== 'pending')
        return false;
    return $submitted;
}
