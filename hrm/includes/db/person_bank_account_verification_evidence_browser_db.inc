<?php
/**
 * HRM-FND-004 read-only browser projection for final-checker bank evidence registration.
 *
 * Returns masked/governance metadata only. Bank ciphertext, lookup tokens, key
 * material and raw external evidence are never selected by this helper.
 */

$hrm_bank_account_verification_evidence_browser_repo_root = dirname(dirname(dirname(__DIR__)));
if (!function_exists('hrm_bank_account_verification_evidence_action_hash')) {
    $candidate = isset($path_to_root)
        ? $path_to_root.'/hrm/includes/db/person_bank_account_verification_evidence_db.inc'
        : '';
    if ($candidate === '' || !is_file($candidate))
        $candidate = $hrm_bank_account_verification_evidence_browser_repo_root.'/hrm/includes/db/person_bank_account_verification_evidence_db.inc';
    if (is_file($candidate))
        include_once($candidate);
}
unset($hrm_bank_account_verification_evidence_browser_repo_root, $candidate);

function hrm_bank_account_verification_evidence_browser_employee_ref($value)
{
    if (!is_scalar($value)) return false;
    $value = trim((string)$value);
    return $value !== '' && strlen($value) <= 64
        && preg_match('/^[A-Za-z0-9_.:@-]+$/D', $value) === 1 ? $value : false;
}

/** Validate one UTC timestamp entered by the final checker as YYYY-MM-DD HH:MM:SS. */
function hrm_bank_account_verification_evidence_browser_verified_at_epoch($value)
{
    if (!is_string($value) || preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/D', $value) !== 1)
        return false;
    $zone = new DateTimeZone('UTC');
    $dt = DateTime::createFromFormat('!Y-m-d H:i:s', $value, $zone);
    $errors = DateTime::getLastErrors();
    if (!$dt || ($errors !== false && ((int)$errors['warning_count'] !== 0 || (int)$errors['error_count'] !== 0))
        || $dt->format('Y-m-d H:i:s') !== $value)
        return false;
    $epoch = $dt->getTimestamp();
    return is_int($epoch) && $epoch > 0 ? $epoch : false;
}

/**
 * Return exact current approved request/draft/final-checker custody for one employee.
 * The caller may pass 0 for request/version/draft to enumerate eligible candidates.
 * No protected bank value is selected.
 */
function hrm_bank_account_verification_evidence_browser_rows($employee_ref, $request_id = 0, $request_row_version = 0, $approval_draft_id = 0)
{
    $employee_ref = hrm_bank_account_verification_evidence_browser_employee_ref($employee_ref);
    $request_id = (int)$request_id;
    $request_row_version = (int)$request_row_version;
    $approval_draft_id = (int)$approval_draft_id;
    $actor = function_exists('hrm_bank_account_verification_request_actor_id')
        ? hrm_bank_account_verification_request_actor_id() : 0;
    if ($employee_ref === false || $actor <= 0 || !defined('TB_PREF') || !defined('ST_HRM_BANK_ACCOUNT_VERIFICATION'))
        return false;
    $employee_column = is_numeric($employee_ref) ? 'employee_number' : 'employee_id';
    $employee_value = is_numeric($employee_ref) ? (int)$employee_ref : $employee_ref;

    $where = array(
        'm.retired_at IS NULL',
        'm.'.$employee_column.'='.db_escape($employee_value),
        "r.request_status='approved'",
        "b.verification_status='unverified'",
        "b.verification_method=''",
        "b.approval_status='unapproved'",
        'b.effective_to IS NULL',
        "b.source='governed_supersession'",
        'b.supersedes_person_bank_account_id IS NOT NULL',
        'b.supersedes_person_bank_account_id>0',
        'b.row_version=r.bank_account_row_version',
        'b.created_by>0',
        'b.created_by<>r.requested_by',
        'b.created_by<>'.$actor,
        'd.trans_type='.(int)ST_HRM_BANK_ACCOUNT_VERIFICATION,
        'd.status=1',
        'd.completed_date IS NOT NULL',
        'd.submitted_by=r.requested_by',
        'd.approved_trans_no=r.verification_request_id',
        'a.user_id='.$actor,
        'r.requested_by<>'.$actor,
        'e.verification_evidence_id IS NULL'
    );
    if ($request_id > 0) $where[] = 'r.verification_request_id='.$request_id;
    if ($request_row_version > 0) $where[] = 'r.request_row_version='.$request_row_version;
    if ($approval_draft_id > 0) $where[] = 'd.id='.$approval_draft_id;

    $sql = "SELECT r.verification_request_id,r.person_bank_account_id,r.bank_account_row_version,r.verification_method,"
        ."r.request_status,r.request_row_version,r.requested_at,r.requested_by,"
        ."b.masked_account,b.masked_routing,b.payment_method,b.verification_status,b.approval_status,b.row_version,"
        ."b.supersedes_person_bank_account_id,"
        ."d.id approval_draft_id,d.draft_data,d.status approval_status_code,d.submitted_by,d.approved_trans_no,d.completed_date,"
        ."a.id final_checker_action_id,a.user_id final_checker_user_id,a.level final_checker_level,a.action_date final_checker_action_date "
        ."FROM ".TB_PREF."employees emp "
        ."INNER JOIN ".TB_PREF."hrm_employee_worker_map m ON m.employee_number=emp.employee_number "
        ."INNER JOIN ".TB_PREF."hrm_workers w ON w.worker_id=m.worker_id "
        ."INNER JOIN ".TB_PREF."hrm_person_bank_account_verification_requests r ON r.person_bank_account_id IN ("
            ."SELECT pb.person_bank_account_id FROM ".TB_PREF."hrm_person_bank_accounts pb WHERE pb.person_id=w.person_id) "
        ."INNER JOIN ".TB_PREF."hrm_person_bank_accounts b ON b.person_bank_account_id=r.person_bank_account_id "
        ."INNER JOIN ".TB_PREF."approval_drafts d ON d.approved_trans_no=r.verification_request_id "
        ."INNER JOIN ".TB_PREF."approval_actions a ON a.id=(SELECT aa.id FROM ".TB_PREF."approval_actions aa "
            ."WHERE aa.draft_id=d.id AND aa.action_type='approve' ORDER BY aa.level DESC,aa.id DESC LIMIT 1) "
        ."LEFT JOIN ".TB_PREF."hrm_person_bank_account_verification_evidence e ON e.verification_request_id=r.verification_request_id "
        ."WHERE ".implode(' AND ', $where)." ORDER BY r.verification_request_id DESC LIMIT 100";
    $result = db_query($sql, 'could not read bank-account verification evidence browser candidates');
    if (!$result) return false;
    $rows = array();
    while ($row = db_fetch_assoc($result)) {
        $draft = json_decode((string)$row['draft_data'], true);
        if (!is_array($draft)
            || !isset($draft['verification_request_id'],$draft['person_bank_account_id'],$draft['bank_account_row_version'],$draft['verification_method'])
            || (int)$draft['verification_request_id'] !== (int)$row['verification_request_id']
            || (int)$draft['person_bank_account_id'] !== (int)$row['person_bank_account_id']
            || (int)$draft['bank_account_row_version'] !== (int)$row['bank_account_row_version']
            || (string)$draft['verification_method'] !== (string)$row['verification_method'])
            continue;
        unset($row['draft_data']);
        $rows[] = $row;
    }
    return $rows;
}

function hrm_bank_account_verification_evidence_browser_candidate($employee_ref, $request_id, $request_row_version, $approval_draft_id)
{
    $request_id = function_exists('hrm_bank_account_verification_request_positive_int')
        ? hrm_bank_account_verification_request_positive_int($request_id) : false;
    $request_row_version = function_exists('hrm_bank_account_verification_request_positive_int')
        ? hrm_bank_account_verification_request_positive_int($request_row_version) : false;
    $approval_draft_id = function_exists('hrm_bank_account_verification_request_positive_int')
        ? hrm_bank_account_verification_request_positive_int($approval_draft_id) : false;
    if ($request_id === false || $request_row_version === false || $approval_draft_id === false)
        return false;
    $rows = hrm_bank_account_verification_evidence_browser_rows($employee_ref, $request_id, $request_row_version, $approval_draft_id);
    return is_array($rows) && count($rows) === 1 ? $rows[0] : false;
}

function get_hrm_bank_account_verification_evidence_browser_candidates_masked($employee_ref)
{
    $rows = hrm_bank_account_verification_evidence_browser_rows($employee_ref);
    return is_array($rows) ? $rows : array();
}

/** Rebuild the accepted evidence-registration hash from exact current final-checker custody. */
function hrm_bank_account_verification_evidence_prepare_browser_action(
    $employee_ref, $request_id, $request_row_version, $approval_draft_id,
    $evidence_source, $evidence_reference_hash, $evidence_payload_hash, $verified_at_epoch,
    $now_epoch = null
) {
    $candidate = hrm_bank_account_verification_evidence_browser_candidate(
        $employee_ref, $request_id, $request_row_version, $approval_draft_id
    );
    $now = $now_epoch === null ? time() : $now_epoch;
    $source = function_exists('hrm_bank_account_verification_evidence_source')
        ? hrm_bank_account_verification_evidence_source($evidence_source) : false;
    $reference_hash = function_exists('hrm_bank_account_verification_evidence_hex64')
        ? hrm_bank_account_verification_evidence_hex64($evidence_reference_hash) : false;
    $payload_hash = function_exists('hrm_bank_account_verification_evidence_hex64')
        ? hrm_bank_account_verification_evidence_hex64($evidence_payload_hash) : false;
    if (!is_array($candidate) || $source === false || $reference_hash === false || $payload_hash === false
        || !is_int($verified_at_epoch) || $verified_at_epoch <= 0 || !is_int($now) || $now <= 0 || $verified_at_epoch > $now)
        return false;
    $requested_at = strtotime((string)$candidate['requested_at'].' UTC');
    if ($requested_at === false || $verified_at_epoch < $requested_at)
        return false;
    $company = function_exists('user_company') ? (int)user_company() : -1;
    $action_hash = function_exists('hrm_bank_account_verification_evidence_action_hash')
        ? hrm_bank_account_verification_evidence_action_hash(
            $company,(int)$candidate['verification_request_id'],(int)$candidate['request_row_version'],
            (int)$candidate['person_bank_account_id'],(int)$candidate['bank_account_row_version'],
            (int)$candidate['approval_draft_id'],(string)$candidate['verification_method'],$source,
            $reference_hash,$payload_hash,$verified_at_epoch
        ) : false;
    if ($action_hash === false || !defined('HRM_BANK_ACCOUNT_VERIFICATION_EVIDENCE_REGISTRATION_ACTION_CLASS'))
        return false;
    return array(
        'company'=>$company,'employee_ref'=>(string)$employee_ref,
        'verification_request_id'=>(int)$candidate['verification_request_id'],
        'request_row_version'=>(int)$candidate['request_row_version'],
        'approval_draft_id'=>(int)$candidate['approval_draft_id'],
        'person_bank_account_id'=>(int)$candidate['person_bank_account_id'],
        'bank_account_row_version'=>(int)$candidate['bank_account_row_version'],
        'masked_account'=>(string)$candidate['masked_account'],'masked_routing'=>(string)$candidate['masked_routing'],
        'payment_method'=>(string)$candidate['payment_method'],'verification_method'=>(string)$candidate['verification_method'],
        'evidence_source'=>$source,'evidence_reference_hash'=>$reference_hash,'evidence_payload_hash'=>$payload_hash,
        'verified_at_epoch'=>$verified_at_epoch,'action_class'=>HRM_BANK_ACCOUNT_VERIFICATION_EVIDENCE_REGISTRATION_ACTION_CLASS,
        'action_hash'=>$action_hash
    );
}
