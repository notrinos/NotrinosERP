<?php
/**
 * HRM-FND-004 read-only browser projection for evidence-bound bank-account promotion.
 *
 * Only masked/governance metadata is returned. The helper reads the opaque
 * evidence hashes solely to recompute the already-consumed evidence-registration
 * assurance action hash, then strips those hashes before returning rows to UI.
 * Bank ciphertext, lookup tokens, key material and plaintext are never selected.
 */

$hrm_bank_account_verification_promotion_browser_repo_root = dirname(dirname(dirname(__DIR__)));
if (!function_exists('promote_hrm_person_bank_account_verification')) {
    $candidate = isset($path_to_root)
        ? $path_to_root.'/hrm/includes/db/person_bank_account_verification_evidence_db.inc'
        : '';
    if ($candidate === '' || !is_file($candidate))
        $candidate = $hrm_bank_account_verification_promotion_browser_repo_root.'/hrm/includes/db/person_bank_account_verification_evidence_db.inc';
    if (is_file($candidate))
        include_once($candidate);
}
unset($hrm_bank_account_verification_promotion_browser_repo_root, $candidate);

function hrm_bank_account_verification_promotion_browser_employee_ref($value)
{
    if (!is_scalar($value)) return false;
    $value = trim((string)$value);
    return $value !== '' && strlen($value) <= 64
        && preg_match('/^[A-Za-z0-9_.:@-]+$/D', $value) === 1 ? $value : false;
}

function hrm_bank_account_verification_promotion_browser_hash_equals($known, $actual)
{
    if (function_exists('session_registry_hash_equals'))
        return session_registry_hash_equals((string)$known, (string)$actual);
    return is_string($known) && is_string($actual) && hash_equals($known, $actual);
}

/**
 * Return exact evidence/request/draft/final-checker/consumed-assurance custody.
 * Pass zero ids/versions to enumerate eligible candidates.
 */
function hrm_bank_account_verification_promotion_browser_rows(
    $employee_ref, $evidence_id = 0, $evidence_row_version = 0,
    $request_row_version = 0, $bank_account_row_version = 0
) {
    $employee_ref = hrm_bank_account_verification_promotion_browser_employee_ref($employee_ref);
    $evidence_id = (int)$evidence_id;
    $evidence_row_version = (int)$evidence_row_version;
    $request_row_version = (int)$request_row_version;
    $bank_account_row_version = (int)$bank_account_row_version;
    $actor = function_exists('hrm_bank_account_verification_request_actor_id')
        ? hrm_bank_account_verification_request_actor_id() : 0;
    if ($employee_ref === false || $actor <= 0 || !defined('TB_PREF')
        || !defined('ST_HRM_BANK_ACCOUNT_VERIFICATION')
        || !defined('HRM_BANK_ACCOUNT_VERIFICATION_EVIDENCE_REGISTRATION_ACTION_CLASS'))
        return false;

    $employee_column = is_numeric($employee_ref) ? 'employee_number' : 'employee_id';
    $employee_value = is_numeric($employee_ref) ? (int)$employee_ref : $employee_ref;
    $where = array(
        'm.retired_at IS NULL',
        'm.'.$employee_column.'='.db_escape($employee_value),
        "r.request_status='approved'",
        'r.requested_by<>'.$actor,
        'b.effective_to IS NULL',
        "b.source='governed_supersession'",
        'b.supersedes_person_bank_account_id IS NOT NULL',
        'b.supersedes_person_bank_account_id>0',
        'b.created_by>0',
        'b.created_by<>r.requested_by',
        'b.created_by<>'.$actor,
        "((b.row_version=e.bank_account_row_version AND b.verification_status='unverified' AND b.approval_status='unapproved' AND b.verification_method='')"
            ." OR (b.row_version=e.bank_account_row_version+1 AND b.verification_status='verified' AND b.approval_status='approved' AND b.verification_method=e.verification_method))",
        'r.person_bank_account_id=e.person_bank_account_id',
        'r.bank_account_row_version=e.bank_account_row_version',
        'r.request_row_version=e.request_row_version',
        'r.verification_request_id=e.verification_request_id',
        'r.verification_method=e.verification_method',
        'd.id=e.approval_draft_id',
        'd.trans_type='.(int)ST_HRM_BANK_ACCOUNT_VERIFICATION,
        'd.status=1',
        'd.completed_date IS NOT NULL',
        'd.submitted_by=r.requested_by',
        'd.approved_trans_no=r.verification_request_id',
        'a.user_id='.$actor,
        'e.recorded_by='.$actor,
        's.assurance_event_id=e.checker_assurance_event_id',
        's.assurance_level=2',
        "s.assurance_method='oidc_acr_amr'",
        's.action_class='.db_escape(HRM_BANK_ACCOUNT_VERIFICATION_EVIDENCE_REGISTRATION_ACTION_CLASS),
        's.consumed_at IS NOT NULL'
    );
    if ($evidence_id > 0) $where[] = 'e.verification_evidence_id='.$evidence_id;
    if ($evidence_row_version > 0) $where[] = 'e.evidence_row_version='.$evidence_row_version;
    if ($request_row_version > 0) $where[] = 'e.request_row_version='.$request_row_version;
    if ($bank_account_row_version > 0) $where[] = 'e.bank_account_row_version='.$bank_account_row_version;

    $sql = "SELECT e.verification_evidence_id,e.verification_request_id,e.person_bank_account_id,e.bank_account_row_version,"
        ."e.request_row_version,e.approval_draft_id,e.verification_method,e.evidence_source,e.evidence_reference_hash,"
        ."e.evidence_payload_hash,e.checker_assurance_event_id,e.verified_at,e.evidence_row_version,e.recorded_at,e.recorded_by,"
        ."b.masked_account,b.masked_routing,b.payment_method,b.row_version bank_account_current_row_version,"
        ."b.verification_status bank_account_current_verification_status,b.approval_status bank_account_current_approval_status,"
        ."b.verification_method bank_account_current_verification_method,b.supersedes_person_bank_account_id,b.created_by original_supersession_maker_user_id,"
        ."r.requested_by,d.draft_data,d.completed_date,"
        ."a.id final_checker_action_id,a.user_id final_checker_user_id,a.level final_checker_level,a.action_date final_checker_action_date,"
        ."s.assurance_level,s.assurance_method,s.action_class,s.action_hash,s.issued_at assurance_issued_at,"
        ."s.expires_at assurance_expires_at,s.consumed_at assurance_consumed_at "
        ."FROM ".TB_PREF."employees emp "
        ."INNER JOIN ".TB_PREF."hrm_employee_worker_map m ON m.employee_number=emp.employee_number "
        ."INNER JOIN ".TB_PREF."hrm_workers w ON w.worker_id=m.worker_id "
        ."INNER JOIN ".TB_PREF."hrm_person_bank_accounts b ON b.person_id=w.person_id "
        ."INNER JOIN ".TB_PREF."hrm_person_bank_account_verification_requests r ON r.person_bank_account_id=b.person_bank_account_id "
        ."INNER JOIN ".TB_PREF."hrm_person_bank_account_verification_evidence e ON e.verification_request_id=r.verification_request_id "
        ."INNER JOIN ".TB_PREF."approval_drafts d ON d.id=e.approval_draft_id "
        ."INNER JOIN ".TB_PREF."approval_actions a ON a.id=(SELECT aa.id FROM ".TB_PREF."approval_actions aa "
            ."WHERE aa.draft_id=d.id AND aa.action_type='approve' ORDER BY aa.level DESC,aa.id DESC LIMIT 1) "
        ."INNER JOIN ".TB_PREF."user_session_assurance_events s ON s.assurance_event_id=e.checker_assurance_event_id "
        ."WHERE ".implode(' AND ', $where)." ORDER BY e.verification_evidence_id DESC LIMIT 100";
    $result = db_query($sql, 'could not read bank-account verification promotion browser candidates');
    if (!$result) return false;

    $company = function_exists('user_company') ? (int)user_company() : -1;
    if ($company < 0) return false;
    $rows = array();
    while ($row = db_fetch_assoc($result)) {
        $draft = json_decode((string)$row['draft_data'], true);
        if (!is_array($draft)
            || !isset($draft['verification_request_id'],$draft['person_bank_account_id'],
                $draft['bank_account_row_version'],$draft['verification_method'])
            || (int)$draft['verification_request_id'] !== (int)$row['verification_request_id']
            || (int)$draft['person_bank_account_id'] !== (int)$row['person_bank_account_id']
            || (int)$draft['bank_account_row_version'] !== (int)$row['bank_account_row_version']
            || (string)$draft['verification_method'] !== (string)$row['verification_method'])
            continue;
        $verified_at_epoch = strtotime((string)$row['verified_at'].' UTC');
        if ($verified_at_epoch === false || $verified_at_epoch <= 0) continue;
        $expected_hash = hrm_bank_account_verification_evidence_action_hash(
            $company, (int)$row['verification_request_id'], (int)$row['request_row_version'],
            (int)$row['person_bank_account_id'], (int)$row['bank_account_row_version'], (int)$row['approval_draft_id'],
            (string)$row['verification_method'], (string)$row['evidence_source'],
            (string)$row['evidence_reference_hash'], (string)$row['evidence_payload_hash'], $verified_at_epoch
        );
        if (!is_string($expected_hash)
            || !hrm_bank_account_verification_promotion_browser_hash_equals((string)$row['action_hash'], $expected_hash))
            continue;

        $row['promotion_state'] = (int)$row['bank_account_current_row_version'] === (int)$row['bank_account_row_version'] + 1
            ? 'already_promoted' : 'pending';
        unset($row['draft_data'], $row['evidence_reference_hash'], $row['evidence_payload_hash'], $row['action_hash'],
            $row['bank_account_current_verification_status'], $row['bank_account_current_approval_status'],
            $row['bank_account_current_verification_method']);
        $row['consumed_registration_assurance'] = true;
        $rows[] = $row;
    }
    return $rows;
}

function hrm_bank_account_verification_promotion_browser_candidate(
    $employee_ref, $evidence_id, $evidence_row_version, $request_row_version, $bank_account_row_version
) {
    $evidence_id = function_exists('hrm_bank_account_verification_request_positive_int')
        ? hrm_bank_account_verification_request_positive_int($evidence_id) : false;
    $evidence_row_version = function_exists('hrm_bank_account_verification_request_positive_int')
        ? hrm_bank_account_verification_request_positive_int($evidence_row_version) : false;
    $request_row_version = function_exists('hrm_bank_account_verification_request_positive_int')
        ? hrm_bank_account_verification_request_positive_int($request_row_version) : false;
    $bank_account_row_version = function_exists('hrm_bank_account_verification_request_positive_int')
        ? hrm_bank_account_verification_request_positive_int($bank_account_row_version) : false;
    if ($evidence_id === false || $evidence_row_version === false
        || $request_row_version === false || $bank_account_row_version === false)
        return false;
    $rows = hrm_bank_account_verification_promotion_browser_rows(
        $employee_ref, $evidence_id, $evidence_row_version, $request_row_version, $bank_account_row_version
    );
    return is_array($rows) && count($rows) === 1 ? $rows[0] : false;
}

function get_hrm_bank_account_verification_promotion_browser_candidates_masked($employee_ref)
{
    $rows = hrm_bank_account_verification_promotion_browser_rows($employee_ref);
    return is_array($rows) ? $rows : array();
}
