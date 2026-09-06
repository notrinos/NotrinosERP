<?php
/**
 * HRM-FND-005 Employee Transfer browser cutover/recovery helpers.
 *
 * This source is intentionally read-only except for server-session custody of
 * the opaque idempotency key. Browser state reconciliation never mutates HR,
 * approval, payroll, payment, bank, GL or accounting rows. The application
 * route must submit through submit_hrm_lifecycle_employee_transfer(); there is
 * no direct employee/Assignment mutation fallback.
 */
$hrm_lifecycle_transfer_browser_root = isset($path_to_root)
    ? $path_to_root : dirname(dirname(dirname(__DIR__)));
$hrm_lifecycle_transfer_browser_dependencies = array(
    '/hrm/includes/db/lifecycle_transfer_command_db.inc',
    '/hrm/includes/db/employee_person_worker_db.inc',
    '/includes/approval/db/approval_db.inc',
);
foreach ($hrm_lifecycle_transfer_browser_dependencies as $dependency) {
    $candidate = $hrm_lifecycle_transfer_browser_root.$dependency;
    if (is_file($candidate))
        include_once($candidate);
}
unset($hrm_lifecycle_transfer_browser_root, $hrm_lifecycle_transfer_browser_dependencies, $dependency, $candidate);

if (!defined('HRM_LIFECYCLE_TRANSFER_BROWSER_SESSION_NAMESPACE'))
    define('HRM_LIFECYCLE_TRANSFER_BROWSER_SESSION_NAMESPACE', 'hrm_fnd_005_employee_transfer_browser_idempotency');

/** @return string|false */
function hrm_lifecycle_transfer_browser_employee_id($employee_id)
{
    $employee_id = trim((string)$employee_id);
    if ($employee_id === '' || strlen($employee_id) > 20)
        return false;
    return $employee_id;
}

/** @return string */
function hrm_lifecycle_transfer_browser_session_slot($employee_id)
{
    return hash('sha256', 'employee_transfer|'.(string)$employee_id);
}

/**
 * Return or create one opaque server-owned idempotency key for this employee.
 * The key is never accepted from request data and is never rendered to HTML.
 *
 * @param string $employee_id
 * @return string|false
 */
function hrm_lifecycle_transfer_browser_idempotency_key($employee_id)
{
    $employee_id = hrm_lifecycle_transfer_browser_employee_id($employee_id);
    if ($employee_id === false)
        return false;
    if (!isset($_SESSION) || !is_array($_SESSION))
        return false;
    if (!isset($_SESSION[HRM_LIFECYCLE_TRANSFER_BROWSER_SESSION_NAMESPACE])
        || !is_array($_SESSION[HRM_LIFECYCLE_TRANSFER_BROWSER_SESSION_NAMESPACE]))
        $_SESSION[HRM_LIFECYCLE_TRANSFER_BROWSER_SESSION_NAMESPACE] = array();
    $slot = hrm_lifecycle_transfer_browser_session_slot($employee_id);
    if (isset($_SESSION[HRM_LIFECYCLE_TRANSFER_BROWSER_SESSION_NAMESPACE][$slot])) {
        $existing = $_SESSION[HRM_LIFECYCLE_TRANSFER_BROWSER_SESSION_NAMESPACE][$slot];
        if (is_string($existing) && strlen($existing) >= 32 && strlen($existing) <= 128
            && preg_match('/^[A-Za-z0-9._:-]+$/D', $existing) === 1)
            return $existing;
        unset($_SESSION[HRM_LIFECYCLE_TRANSFER_BROWSER_SESSION_NAMESPACE][$slot]);
    }

    $bytes = false;
    if (function_exists('random_bytes')) {
        try { $bytes = random_bytes(32); } catch (Exception $e) { $bytes = false; }
        catch (Throwable $e) { $bytes = false; }
    }
    if ($bytes === false && function_exists('openssl_random_pseudo_bytes')) {
        $strong = false;
        $candidate = openssl_random_pseudo_bytes(32, $strong);
        if ($candidate !== false && $strong)
            $bytes = $candidate;
    }
    if ($bytes === false)
        return false;
    $key = bin2hex($bytes);
    if (strlen($key) !== 64)
        return false;
    $_SESSION[HRM_LIFECYCLE_TRANSFER_BROWSER_SESSION_NAMESPACE][$slot] = $key;
    return $key;
}

/** @return void */
function hrm_lifecycle_transfer_browser_forget_idempotency_key($employee_id)
{
    $employee_id = hrm_lifecycle_transfer_browser_employee_id($employee_id);
    if ($employee_id === false || !isset($_SESSION[HRM_LIFECYCLE_TRANSFER_BROWSER_SESSION_NAMESPACE])
        || !is_array($_SESSION[HRM_LIFECYCLE_TRANSFER_BROWSER_SESSION_NAMESPACE]))
        return;
    unset($_SESSION[HRM_LIFECYCLE_TRANSFER_BROWSER_SESSION_NAMESPACE][
        hrm_lifecycle_transfer_browser_session_slot($employee_id)
    ]);
    if (!$_SESSION[HRM_LIFECYCLE_TRANSFER_BROWSER_SESSION_NAMESPACE])
        unset($_SESSION[HRM_LIFECYCLE_TRANSFER_BROWSER_SESSION_NAMESPACE]);
}

/**
 * Reconcile browser-visible state for one Worker without exposing another
 * maker's command identifiers or request details.
 *
 * status values:
 *   none          no pending command and no own terminal command
 *   pending       current actor owns the one valid pending command
 *   blocked       another maker owns the one valid pending command
 *   completed     current actor's latest command completed
 *   rejected      current actor's latest command was rejected
 *   cancelled     current actor's latest command was cancelled
 *   inconsistent  command/approval custody is malformed; fail closed
 * No rejected request payload or approval comment is copied into browser recovery state.
 *
 * @param string $employee_id
 * @return array
 */
function get_hrm_lifecycle_employee_transfer_browser_status($employee_id)
{
    $none = array('status'=>'none','own'=>false,'lifecycle_command_id'=>0,'approval_draft_id'=>0);
    $employee_id = hrm_lifecycle_transfer_browser_employee_id($employee_id);
    $actor = function_exists('hrm_lifecycle_transfer_actor_id') ? hrm_lifecycle_transfer_actor_id() : 0;
    if ($employee_id === false || $actor <= 0 || !function_exists('hrm_lifecycle_transfer_permission')
        || !hrm_lifecycle_transfer_permission() || !function_exists('get_hrm_person_worker_binding'))
        return array_merge($none, array('status'=>'inconsistent'));

    $binding = get_hrm_person_worker_binding($employee_id, false);
    if (!$binding || !empty($binding['retired_at']) || (int)$binding['worker_id'] <= 0)
        return $none;

    $sql = "SELECT c.lifecycle_command_id,c.worker_id,c.command_status,c.effective_at,c.approval_draft_id,"
        ."c.row_version,c.requested_by,c.completed_at,c.completed_by,d.id approval_row_id,d.trans_type,"
        ."d.status approval_status,d.submitted_by FROM ".TB_PREF."hrm_lifecycle_commands c "
        ."LEFT JOIN ".TB_PREF."approval_drafts d ON d.id=c.approval_draft_id "
        ."WHERE c.worker_id=".(int)$binding['worker_id']
        ." AND c.command_type=".db_escape(HRM_LIFECYCLE_TRANSFER_COMMAND_TYPE)
        ." ORDER BY c.lifecycle_command_id DESC";
    $result = db_query($sql, 'could not reconcile Employee Transfer browser command status');
    if (!$result)
        return array_merge($none, array('status'=>'inconsistent'));

    $pending = array();
    $latest_own_terminal = false;
    while ($row = db_fetch_assoc($result)) {
        $status = (string)$row['command_status'];
        $row_version = (int)$row['row_version'];
        $draft_id = (int)$row['approval_draft_id'];
        $draft_ok = $draft_id > 0 && (int)$row['approval_row_id'] === $draft_id
            && defined('ST_HRM_LIFECYCLE_TRANSFER')
            && (int)$row['trans_type'] === (int)ST_HRM_LIFECYCLE_TRANSFER
            && (int)$row['submitted_by'] === (int)$row['requested_by']
            && (int)$row['requested_by'] > 0;
        $expected_approval = array(
            'pending'=>defined('APPROVAL_STATUS_PENDING') ? APPROVAL_STATUS_PENDING : 0,
            'completed'=>defined('APPROVAL_STATUS_APPROVED') ? APPROVAL_STATUS_APPROVED : 1,
            'rejected'=>defined('APPROVAL_STATUS_REJECTED') ? APPROVAL_STATUS_REJECTED : 2,
            'cancelled'=>defined('APPROVAL_STATUS_CANCELLED') ? APPROVAL_STATUS_CANCELLED : 3,
        );
        if (!isset($expected_approval[$status]) || !$draft_ok
            || (int)$row['approval_status'] !== (int)$expected_approval[$status]
            || ($status === 'pending' && $row_version !== 1)
            || ($status !== 'pending' && $row_version !== 2))
            return array_merge($none, array('status'=>'inconsistent'));
        if ($status === 'pending')
            $pending[] = $row;
        elseif ($latest_own_terminal === false && (int)$row['requested_by'] === $actor)
            $latest_own_terminal = $row;
    }

    if (count($pending) > 1)
        return array_merge($none, array('status'=>'inconsistent'));
    if (count($pending) === 1) {
        $row = $pending[0];
        if ((int)$row['requested_by'] !== $actor)
            return array('status'=>'blocked','own'=>false,'lifecycle_command_id'=>0,'approval_draft_id'=>0);
        return array(
            'status'=>'pending','own'=>true,
            'lifecycle_command_id'=>(int)$row['lifecycle_command_id'],
            'approval_draft_id'=>(int)$row['approval_draft_id'],
        );
    }
    if ($latest_own_terminal !== false) {
        return array(
            'status'=>(string)$latest_own_terminal['command_status'],'own'=>true,
            'lifecycle_command_id'=>(int)$latest_own_terminal['lifecycle_command_id'],
            'approval_draft_id'=>(int)$latest_own_terminal['approval_draft_id'],
        );
    }
    return $none;
}
