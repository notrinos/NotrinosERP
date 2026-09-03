<?php
/** HRM-FND-004 verification-request browser continuation read helpers. */
$hrm_identifier_request_browser_root = isset($path_to_root) ? $path_to_root : dirname(dirname(dirname(__DIR__)));
$hrm_identifier_request_browser_command = $hrm_identifier_request_browser_root.'/hrm/includes/db/person_identifier_verification_request_db.inc';
if (is_file($hrm_identifier_request_browser_command)) include_once($hrm_identifier_request_browser_command);
unset($hrm_identifier_request_browser_root, $hrm_identifier_request_browser_command);

if (!defined('HRM_IDENTIFIER_VERIFICATION_REQUEST_BROWSER_METHOD_MAX'))
    define('HRM_IDENTIFIER_VERIFICATION_REQUEST_BROWSER_METHOD_MAX', 32);

function hrm_identifier_verification_request_browser_permission()
{
    return function_exists('hrm_identifier_verification_request_permission')
        && hrm_identifier_verification_request_permission();
}

function hrm_identifier_verification_request_browser_employee_ref($value)
{
    if (!is_scalar($value)) return false;
    $value = trim((string)$value);
    return $value !== '' && strlen($value) <= 64 && preg_match('/^[A-Za-z0-9_.:@-]+$/D', $value) === 1 ? $value : false;
}

function hrm_identifier_verification_request_browser_method($value)
{
    if (!is_scalar($value)) return false;
    $value = strtolower(trim((string)$value));
    return $value !== '' && strlen($value) <= HRM_IDENTIFIER_VERIFICATION_REQUEST_BROWSER_METHOD_MAX
        && preg_match('/^[a-z0-9_.:-]+$/D', $value) === 1 ? $value : false;
}

/**
 * Resolve one exact current governed-supersession successor for a selected employee.
 * Only masked/governance metadata leaves this helper. Protected identifier material is never selected.
 */
function hrm_identifier_verification_request_browser_successor($employee_ref, $identifier_id, $row_version)
{
    if (!hrm_identifier_verification_request_browser_permission() || !defined('TB_PREF')) return false;
    $identifier_id = function_exists('hrm_identifier_verification_request_positive_int')
        ? hrm_identifier_verification_request_positive_int($identifier_id) : false;
    $row_version = function_exists('hrm_identifier_verification_request_positive_int')
        ? hrm_identifier_verification_request_positive_int($row_version) : false;
    $employee_ref = hrm_identifier_verification_request_browser_employee_ref($employee_ref);
    if ($identifier_id === false || $row_version === false || $employee_ref === false) return false;
    $column = is_numeric($employee_ref) ? 'employee_number' : 'employee_id';
    $employee = is_numeric($employee_ref) ? (int)$employee_ref : $employee_ref;
    $sql = "SELECT i.identifier_id,i.person_id,i.identifier_type,i.masked_value,i.issuing_jurisdiction,i.valid_to,"
        ."i.effective_from,i.verification_status,i.verification_method,i.approval_status,i.row_version,i.supersedes_identifier_id "
        ."FROM ".TB_PREF."hrm_employee_worker_map m INNER JOIN ".TB_PREF."hrm_workers w ON w.worker_id=m.worker_id "
        ."INNER JOIN ".TB_PREF."hrm_person_identifiers i ON i.person_id=w.person_id "
        ."WHERE m.".$column."=".db_escape($employee)." AND m.retired_at IS NULL AND i.identifier_id=".$identifier_id
        ." AND i.row_version=".$row_version." AND i.effective_to IS NULL AND i.source='governed_supersession' "
        ."AND i.supersedes_identifier_id IS NOT NULL AND i.verification_status='unverified' AND i.approval_status='unapproved' "
        ."AND NOT EXISTS (SELECT 1 FROM ".TB_PREF."hrm_person_identifier_verification_requests r WHERE r.identifier_id=i.identifier_id "
        ."AND r.identifier_row_version=i.row_version AND r.request_status IN ('pending','approved')) LIMIT 2";
    $result = db_query($sql, 'could not resolve governed supersession successor for verification request');
    if (!$result) return false;
    $row = db_fetch_assoc($result);
    if (!$row || db_fetch_assoc($result) !== false) return false;
    return array(
        'identifier_id'=>(int)$row['identifier_id'],'person_id'=>(int)$row['person_id'],
        'identifier_type'=>(string)$row['identifier_type'],'masked_value'=>(string)$row['masked_value'],
        'issuing_jurisdiction'=>(string)$row['issuing_jurisdiction'],
        'valid_to'=>$row['valid_to'] === null ? null : (string)$row['valid_to'],
        'verification_status'=>(string)$row['verification_status'],'approval_status'=>(string)$row['approval_status'],
        'row_version'=>(int)$row['row_version'],'supersedes_identifier_id'=>(int)$row['supersedes_identifier_id']
    );
}

/** Return only exact eligible governed successors as masked metadata. */
function get_hrm_identifier_verification_request_browser_candidates_masked($employee_ref)
{
    if (!hrm_identifier_verification_request_browser_permission() || !defined('TB_PREF')) return false;
    $employee_ref = hrm_identifier_verification_request_browser_employee_ref($employee_ref);
    if ($employee_ref === false) return false;
    $column = is_numeric($employee_ref) ? 'employee_number' : 'employee_id';
    $employee = is_numeric($employee_ref) ? (int)$employee_ref : $employee_ref;
    $sql = "SELECT i.identifier_id,i.identifier_type,i.masked_value,i.issuing_jurisdiction,i.valid_to,i.row_version,i.supersedes_identifier_id "
        ."FROM ".TB_PREF."hrm_employee_worker_map m INNER JOIN ".TB_PREF."hrm_workers w ON w.worker_id=m.worker_id "
        ."INNER JOIN ".TB_PREF."hrm_person_identifiers i ON i.person_id=w.person_id "
        ."WHERE m.".$column."=".db_escape($employee)." AND m.retired_at IS NULL AND i.effective_to IS NULL "
        ."AND i.source='governed_supersession' AND i.supersedes_identifier_id IS NOT NULL "
        ."AND i.verification_status='unverified' AND i.approval_status='unapproved' "
        ."AND NOT EXISTS (SELECT 1 FROM ".TB_PREF."hrm_person_identifier_verification_requests r WHERE r.identifier_id=i.identifier_id "
        ."AND r.identifier_row_version=i.row_version AND r.request_status IN ('pending','approved')) "
        ."ORDER BY i.identifier_type,i.identifier_id";
    $result = db_query($sql, 'could not list governed successors eligible for verification request');
    if (!$result) return false;
    $rows = array();
    while ($row = db_fetch_assoc($result)) {
        $rows[] = array('identifier_id'=>(int)$row['identifier_id'],'identifier_type'=>(string)$row['identifier_type'],
            'masked_value'=>(string)$row['masked_value'],'issuing_jurisdiction'=>(string)$row['issuing_jurisdiction'],
            'valid_to'=>$row['valid_to'] === null ? null : (string)$row['valid_to'],'row_version'=>(int)$row['row_version'],
            'supersedes_identifier_id'=>(int)$row['supersedes_identifier_id']);
    }
    return $rows;
}

/**
 * Derive the exact already-accepted request-command action hash.
 * No fallback hash algorithm is invented: if the accepted helper is unavailable, fail closed.
 */
function hrm_identifier_verification_request_prepare_browser_action($employee_ref, $identifier_id, $row_version, $verification_method)
{
    $successor = hrm_identifier_verification_request_browser_successor($employee_ref, $identifier_id, $row_version);
    $verification_method = hrm_identifier_verification_request_browser_method($verification_method);
    if (!is_array($successor) || $verification_method === false
        || !function_exists('hrm_identifier_verification_request_action_hash')) return false;
    $company = function_exists('user_company') ? (int)user_company() : 0;
    $action_hash = hrm_identifier_verification_request_action_hash($company, (int)$successor['identifier_id'],
        (int)$successor['row_version'], $verification_method);
    if (!is_string($action_hash) || preg_match('/^[a-f0-9]{64}$/D', $action_hash) !== 1) return false;
    $action_class = defined('HRM_IDENTIFIER_VERIFICATION_REQUEST_ACTION_CLASS')
        ? (string)HRM_IDENTIFIER_VERIFICATION_REQUEST_ACTION_CLASS : 'hrm.person.identifier.verification.request';
    if ($action_class !== 'hrm.person.identifier.verification.request') return false;
    return array('company'=>$company,'employee_ref'=>$employee_ref,'identifier_id'=>(int)$successor['identifier_id'],
        'identifier_row_version'=>(int)$successor['row_version'],'identifier_type'=>(string)$successor['identifier_type'],
        'masked_value'=>(string)$successor['masked_value'],'verification_method'=>$verification_method,
        'action_class'=>$action_class,'action_hash'=>$action_hash);
}
