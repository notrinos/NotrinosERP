<?php
/** HRM-FND-004 Person bank-account verification-request browser continuation read helpers. */
$hrm_bank_request_browser_root = isset($path_to_root) ? $path_to_root : dirname(dirname(dirname(__DIR__)));
$hrm_bank_request_browser_command = $hrm_bank_request_browser_root.'/hrm/includes/db/person_bank_account_verification_request_db.inc';
if (is_file($hrm_bank_request_browser_command)) include_once($hrm_bank_request_browser_command);
unset($hrm_bank_request_browser_root, $hrm_bank_request_browser_command);

function hrm_bank_account_verification_request_browser_permission()
{
    return function_exists('hrm_bank_account_verification_request_permission')
        && hrm_bank_account_verification_request_permission();
}

function hrm_bank_account_verification_request_browser_employee_ref($value)
{
    if (!is_scalar($value)) return false;
    $value = trim((string)$value);
    return $value !== '' && strlen($value) <= 64 && preg_match('/^[A-Za-z0-9_.:@-]+$/D', $value) === 1 ? $value : false;
}

/**
 * Resolve one exact current governed-supersession bank successor for a selected employee.
 * Only masked/governance metadata leaves this helper. Protected bank material is never selected.
 * The original supersession maker is excluded before any OIDC step-up is started.
 */
function hrm_bank_account_verification_request_browser_successor($employee_ref, $person_bank_account_id, $row_version)
{
    if (!hrm_bank_account_verification_request_browser_permission() || !defined('TB_PREF')
        || !function_exists('hrm_bank_account_verification_request_actor_id')) return false;
    $person_bank_account_id = function_exists('hrm_bank_account_verification_request_positive_int')
        ? hrm_bank_account_verification_request_positive_int($person_bank_account_id) : false;
    $row_version = function_exists('hrm_bank_account_verification_request_positive_int')
        ? hrm_bank_account_verification_request_positive_int($row_version) : false;
    $employee_ref = hrm_bank_account_verification_request_browser_employee_ref($employee_ref);
    $actor = hrm_bank_account_verification_request_actor_id();
    if ($person_bank_account_id === false || $row_version === false || $employee_ref === false || $actor <= 0) return false;
    $column = is_numeric($employee_ref) ? 'employee_number' : 'employee_id';
    $employee = is_numeric($employee_ref) ? (int)$employee_ref : $employee_ref;
    $sql = "SELECT b.person_bank_account_id,b.person_id,b.masked_account,b.masked_routing,b.payment_method,"
        ."b.verification_status,b.verification_method,b.approval_status,b.row_version,b.supersedes_person_bank_account_id "
        ."FROM ".TB_PREF."hrm_employee_worker_map m INNER JOIN ".TB_PREF."hrm_workers w ON w.worker_id=m.worker_id "
        ."INNER JOIN ".TB_PREF."hrm_person_bank_accounts b ON b.person_id=w.person_id "
        ."WHERE m.".$column."=".db_escape($employee)." AND m.retired_at IS NULL "
        ."AND b.person_bank_account_id=".$person_bank_account_id." AND b.row_version=".$row_version
        ." AND b.effective_to IS NULL AND b.source='governed_supersession' "
        ."AND b.supersedes_person_bank_account_id IS NOT NULL AND b.created_by>0 AND b.created_by<>".$actor
        ." AND b.verification_status='unverified' AND b.approval_status='unapproved' "
        ."AND NOT EXISTS (SELECT 1 FROM ".TB_PREF."hrm_person_bank_account_verification_requests r "
        ."WHERE r.person_bank_account_id=b.person_bank_account_id AND r.bank_account_row_version=b.row_version "
        ."AND r.request_status IN ('pending','approved')) LIMIT 2";
    $result = db_query($sql, 'could not resolve governed bank successor for verification request');
    if (!$result) return false;
    $row = db_fetch_assoc($result);
    if (!$row || db_fetch_assoc($result) !== false) return false;
    return array(
        'person_bank_account_id'=>(int)$row['person_bank_account_id'],'person_id'=>(int)$row['person_id'],
        'masked_account'=>(string)$row['masked_account'],'masked_routing'=>(string)$row['masked_routing'],
        'payment_method'=>(int)$row['payment_method'],'verification_status'=>(string)$row['verification_status'],
        'approval_status'=>(string)$row['approval_status'],'row_version'=>(int)$row['row_version'],
        'supersedes_person_bank_account_id'=>(int)$row['supersedes_person_bank_account_id']
    );
}

/** Return only exact eligible governed bank successors as masked metadata. */
function get_hrm_bank_account_verification_request_browser_candidates_masked($employee_ref)
{
    if (!hrm_bank_account_verification_request_browser_permission() || !defined('TB_PREF')
        || !function_exists('hrm_bank_account_verification_request_actor_id')) return false;
    $employee_ref = hrm_bank_account_verification_request_browser_employee_ref($employee_ref);
    $actor = hrm_bank_account_verification_request_actor_id();
    if ($employee_ref === false || $actor <= 0) return false;
    $column = is_numeric($employee_ref) ? 'employee_number' : 'employee_id';
    $employee = is_numeric($employee_ref) ? (int)$employee_ref : $employee_ref;
    $sql = "SELECT b.person_bank_account_id,b.masked_account,b.masked_routing,b.payment_method,b.row_version,b.supersedes_person_bank_account_id "
        ."FROM ".TB_PREF."hrm_employee_worker_map m INNER JOIN ".TB_PREF."hrm_workers w ON w.worker_id=m.worker_id "
        ."INNER JOIN ".TB_PREF."hrm_person_bank_accounts b ON b.person_id=w.person_id "
        ."WHERE m.".$column."=".db_escape($employee)." AND m.retired_at IS NULL AND b.effective_to IS NULL "
        ."AND b.source='governed_supersession' AND b.supersedes_person_bank_account_id IS NOT NULL "
        ."AND b.created_by>0 AND b.created_by<>".$actor
        ." AND b.verification_status='unverified' AND b.approval_status='unapproved' "
        ."AND NOT EXISTS (SELECT 1 FROM ".TB_PREF."hrm_person_bank_account_verification_requests r "
        ."WHERE r.person_bank_account_id=b.person_bank_account_id AND r.bank_account_row_version=b.row_version "
        ."AND r.request_status IN ('pending','approved')) ORDER BY b.person_bank_account_id";
    $result = db_query($sql, 'could not list governed bank successors eligible for verification request');
    if (!$result) return false;
    $rows = array();
    while ($row = db_fetch_assoc($result)) {
        $rows[] = array('person_bank_account_id'=>(int)$row['person_bank_account_id'],
            'masked_account'=>(string)$row['masked_account'],'masked_routing'=>(string)$row['masked_routing'],
            'payment_method'=>(int)$row['payment_method'],'row_version'=>(int)$row['row_version'],
            'supersedes_person_bank_account_id'=>(int)$row['supersedes_person_bank_account_id']);
    }
    return $rows;
}

/**
 * Derive the exact already-accepted bank request-command action hash.
 * No fallback hash algorithm is invented: if the accepted helper is unavailable, fail closed.
 */
function hrm_bank_account_verification_request_prepare_browser_action($employee_ref, $person_bank_account_id, $row_version, $verification_method)
{
    $successor = hrm_bank_account_verification_request_browser_successor($employee_ref, $person_bank_account_id, $row_version);
    $verification_method = function_exists('hrm_bank_account_verification_request_method')
        ? hrm_bank_account_verification_request_method($verification_method) : false;
    if (!is_array($successor) || $verification_method === false
        || !function_exists('hrm_bank_account_verification_request_action_hash')) return false;
    $company = function_exists('user_company') ? (int)user_company() : 0;
    $action_hash = hrm_bank_account_verification_request_action_hash($company,
        (int)$successor['person_bank_account_id'], (int)$successor['row_version'], $verification_method);
    if (!is_string($action_hash) || preg_match('/^[a-f0-9]{64}$/D', $action_hash) !== 1) return false;
    $action_class = defined('HRM_BANK_ACCOUNT_VERIFICATION_REQUEST_ACTION_CLASS')
        ? (string)HRM_BANK_ACCOUNT_VERIFICATION_REQUEST_ACTION_CLASS : 'hrm.person_bank_account.verification.request';
    if ($action_class !== 'hrm.person_bank_account.verification.request') return false;
    return array('company'=>$company,'employee_ref'=>$employee_ref,
        'person_bank_account_id'=>(int)$successor['person_bank_account_id'],
        'bank_account_row_version'=>(int)$successor['row_version'],
        'masked_account'=>(string)$successor['masked_account'],'masked_routing'=>(string)$successor['masked_routing'],
        'verification_method'=>$verification_method,'action_class'=>$action_class,'action_hash'=>$action_hash);
}
