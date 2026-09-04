<?php
/**
 * HRM-FND-004 bank-account verification-evidence registration command.
 *
 * Custody-only: records externally verified opaque provenance after the exact
 * central approval has completed. It deliberately does not promote bank verification/approval state or expose protected bank material.
 */


$hrm_bank_account_verification_evidence_repo_root = dirname(dirname(dirname(__DIR__)));
if (!function_exists('hrm_bank_account_verification_request_positive_int')) {
    $candidate = isset($path_to_root)
        ? $path_to_root.'/hrm/includes/db/person_bank_account_verification_request_db.inc'
        : '';
    if ($candidate === '' || !is_file($candidate))
        $candidate = $hrm_bank_account_verification_evidence_repo_root.'/hrm/includes/db/person_bank_account_verification_request_db.inc';
    if (is_file($candidate))
        include_once($candidate);
}
unset($hrm_bank_account_verification_evidence_repo_root, $candidate);

if (!defined('HRM_BANK_ACCOUNT_VERIFICATION_EVIDENCE_REGISTRATION_ACTION_CLASS'))
    define('HRM_BANK_ACCOUNT_VERIFICATION_EVIDENCE_REGISTRATION_ACTION_CLASS', 'hrm.person_bank_account.verification.evidence.register');
if (!defined('HRM_BANK_ACCOUNT_VERIFICATION_EVIDENCE_REGISTRATION_HASH_CONTRACT'))
    define('HRM_BANK_ACCOUNT_VERIFICATION_EVIDENCE_REGISTRATION_HASH_CONTRACT', 'notrinoserp.hrm-fnd-004.person-bank-verification-evidence-registration');
if (!defined('HRM_BANK_ACCOUNT_VERIFICATION_EVIDENCE_REGISTRATION_HASH_VERSION'))
    define('HRM_BANK_ACCOUNT_VERIFICATION_EVIDENCE_REGISTRATION_HASH_VERSION', 1);

/** Validate and normalize the bounded external evidence-source token.
 * @param mixed $value
 * @return string|false
 */
function hrm_bank_account_verification_evidence_source($value)
{
    $value = is_string($value) ? trim($value) : '';
    return preg_match('/^[a-z][a-z0-9_]{0,31}$/D', $value) ? $value : false;
}

/** Validate one canonical lowercase SHA-256 hex digest.
 * @param mixed $value
 * @return string|false
 */
function hrm_bank_account_verification_evidence_hex64($value)
{
    $value = is_string($value) ? trim($value) : '';
    return preg_match('/^[0-9a-f]{64}$/D', $value) ? $value : false;
}

/** Build the deterministic action hash bound to one exact evidence-registration attempt.
 * @return string|false
 */
function hrm_bank_account_verification_evidence_action_hash(
    $company, $request_id, $request_row_version, $person_bank_account_id, $bank_account_row_version,
    $approval_draft_id, $verification_method, $evidence_source, $evidence_reference_hash,
    $evidence_payload_hash, $verified_at_epoch
) {
    $request_id = hrm_bank_account_verification_request_positive_int($request_id);
    $request_row_version = hrm_bank_account_verification_request_positive_int($request_row_version);
    $person_bank_account_id = hrm_bank_account_verification_request_positive_int($person_bank_account_id);
    $bank_account_row_version = hrm_bank_account_verification_request_positive_int($bank_account_row_version);
    $approval_draft_id = hrm_bank_account_verification_request_positive_int($approval_draft_id);
    $verification_method = hrm_bank_account_verification_request_method($verification_method);
    $evidence_source = hrm_bank_account_verification_evidence_source($evidence_source);
    $evidence_reference_hash = hrm_bank_account_verification_evidence_hex64($evidence_reference_hash);
    $evidence_payload_hash = hrm_bank_account_verification_evidence_hex64($evidence_payload_hash);
    if ((int)$company < 0 || $request_id === false || $request_row_version === false
        || $person_bank_account_id === false || $bank_account_row_version === false || $approval_draft_id === false
        || $verification_method === false || $evidence_source === false
        || $evidence_reference_hash === false || $evidence_payload_hash === false
        || !is_int($verified_at_epoch) || $verified_at_epoch <= 0)
        return false;

    return hash('sha256',
        HRM_BANK_ACCOUNT_VERIFICATION_EVIDENCE_REGISTRATION_HASH_CONTRACT
        .'|v='.HRM_BANK_ACCOUNT_VERIFICATION_EVIDENCE_REGISTRATION_HASH_VERSION
        .'|company='.(int)$company
        .'|action='.HRM_BANK_ACCOUNT_VERIFICATION_EVIDENCE_REGISTRATION_ACTION_CLASS
        .'|verification_request_id='.$request_id
        .'|request_row_version='.$request_row_version
        .'|person_bank_account_id='.$person_bank_account_id
        .'|bank_account_row_version='.$bank_account_row_version
        .'|approval_draft_id='.$approval_draft_id
        .'|verification_method='.$verification_method
        .'|evidence_source='.$evidence_source
        .'|evidence_reference_hash='.$evidence_reference_hash
        .'|evidence_payload_hash='.$evidence_payload_hash
        .'|verified_at='.gmdate('Y-m-d H:i:s', $verified_at_epoch)
    );
}

/** Lock and return the exact verification request row.
 * @param int $request_id
 * @return array|false
 */
function hrm_bank_account_verification_evidence_request_for_update($request_id)
{
    $result = db_query("SELECT verification_request_id,person_bank_account_id,bank_account_row_version,verification_method,"
        ."request_status,request_row_version,requested_at,requested_by FROM ".TB_PREF
        ."hrm_person_bank_account_verification_requests WHERE verification_request_id=".(int)$request_id
        ." LIMIT 2 FOR UPDATE", 'could not lock Person bank-account verification request for evidence registration');
    if (!$result) return false;
    $row = db_fetch_assoc($result);
    return $row && db_fetch_assoc($result) === false ? $row : false;
}

/** Lock and return the central approval draft used by this registration.
 * @param int $draft_id
 * @return array|false
 */
function hrm_bank_account_verification_evidence_draft_for_update($draft_id)
{
    $result = db_query("SELECT id,trans_type,draft_data,status,submitted_by,approved_trans_no,completed_date "
        ."FROM ".TB_PREF."approval_drafts WHERE id=".(int)$draft_id." LIMIT 2 FOR UPDATE",
        'could not lock approval draft for bank-account verification evidence');
    if (!$result) return false;
    $row = db_fetch_assoc($result);
    return $row && db_fetch_assoc($result) === false ? $row : false;
}

/** Lock and return the final approval action for the completed draft.
 * @param int $draft_id
 * @return array|null|false
 */
function hrm_bank_account_verification_evidence_final_checker_for_update($draft_id)
{
    $result = db_query("SELECT id,user_id,level,action_date FROM ".TB_PREF."approval_actions "
        ."WHERE draft_id=".(int)$draft_id." AND action_type='approve' "
        ."ORDER BY level DESC,id DESC LIMIT 2 FOR UPDATE",
        'could not lock final approval action for bank-account verification evidence');
    if (!$result) return false;
    $row = db_fetch_assoc($result);
    if (!$row) return null;
    return $row;
}

/** Lock any evidence row already registered for the request.
 * @param int $request_id
 * @return array|null|false
 */
function hrm_bank_account_verification_evidence_existing_for_update($request_id)
{
    $result = db_query("SELECT verification_evidence_id,verification_request_id,person_bank_account_id,bank_account_row_version,"
        ."request_row_version,approval_draft_id,verification_method,evidence_source,evidence_reference_hash,"
        ."evidence_payload_hash,checker_assurance_event_id,verified_at,evidence_row_version,recorded_by FROM "
        .TB_PREF."hrm_person_bank_account_verification_evidence WHERE verification_request_id=".(int)$request_id
        ." LIMIT 2 FOR UPDATE", 'could not lock existing bank-account verification evidence');
    if (!$result) return false;
    $row = db_fetch_assoc($result);
    if (!$row) return null;
    return db_fetch_assoc($result) === false ? $row : false;
}

/** Lock the single action-bound assurance event eligible for consumption.
 * @param int $session_id
 * @param string $action_hash
 * @return array|null|false
 */
function hrm_bank_account_verification_evidence_assurance_event_for_update($session_id, $action_hash)
{
    if ((int)$session_id <= 0 || !hrm_bank_account_verification_evidence_hex64($action_hash))
        return false;
    $result = db_query("SELECT assurance_event_id,session_id,assurance_level,assurance_method,action_class,"
        ."action_hash,issued_at,expires_at,consumed_at FROM ".TB_PREF."user_session_assurance_events "
        ."WHERE session_id=".(int)$session_id." AND action_class="
        .db_escape(HRM_BANK_ACCOUNT_VERIFICATION_EVIDENCE_REGISTRATION_ACTION_CLASS)
        ." AND action_hash=".db_escape($action_hash)
        ." ORDER BY assurance_event_id DESC LIMIT 2 FOR UPDATE",
        'could not lock bank-account verification evidence assurance event');
    if (!$result) return false;
    $row = db_fetch_assoc($result);
    if (!$row) return null;
    return db_fetch_assoc($result) === false ? $row : false;
}

/** Consume fresh level-two assurance and restore the durable session to level one.
 * @param int $company
 * @param string $action_hash
 * @param int $now_epoch
 * @return array|false
 */
function hrm_bank_account_verification_evidence_consume_assurance($company, $action_hash, $now_epoch)
{
    $session_id = federation_assurance_consumer_current_session_id();
    if ($session_id <= 0) return false;
    $custody = federation_assurance_consumer_custody_for_update($session_id);
    if (!federation_assurance_consumer_current_custody_valid($custody, $now_epoch, true))
        return false;
    $event = hrm_bank_account_verification_evidence_assurance_event_for_update($session_id, $action_hash);
    if (!is_array($event) || (int)$event['session_id'] !== $session_id
        || (int)$event['assurance_level'] !== 2 || (string)$event['assurance_method'] !== 'oidc_acr_amr'
        || (string)$event['action_class'] !== HRM_BANK_ACCOUNT_VERIFICATION_EVIDENCE_REGISTRATION_ACTION_CLASS
        || !session_registry_hash_equals((string)$event['action_hash'], $action_hash)
        || $event['consumed_at'] !== null)
        return false;

    $issued = strtotime((string)$event['issued_at'].' UTC');
    $expires = strtotime((string)$event['expires_at'].' UTC');
    $session_issued = strtotime((string)$custody['assurance_issued_at'].' UTC');
    $session_expires = strtotime((string)$custody['assurance_expires_at'].' UTC');
    if ($issued === false || $expires === false || $session_issued === false || $session_expires === false
        || $issued !== $session_issued || $expires !== $session_expires
        || $now_epoch < $issued || $now_epoch >= $expires)
        return false;

    $consumed_at = gmdate('Y-m-d H:i:s', $now_epoch);
    if (!db_query("UPDATE ".TB_PREF."user_session_assurance_events SET consumed_at=".db_escape($consumed_at)
        ." WHERE assurance_event_id=".(int)$event['assurance_event_id']." AND consumed_at IS NULL"
        ." AND expires_at>".db_escape($consumed_at),
        'could not consume bank-account verification evidence assurance event')
        || !function_exists('db_num_affected_rows') || (int)db_num_affected_rows() !== 1)
        return false;

    $restore = federation_assurance_consumer_restore_level_one_sql($custody, $now_epoch);
    if ($restore === false
        || !db_query("UPDATE ".TB_PREF."user_sessions SET ".$restore." WHERE id=".$session_id
            ." AND revoked_at IS NULL AND assurance_level=2 AND assurance_method='oidc_acr_amr'"
            ." AND assurance_issued_at=".db_escape((string)$custody['assurance_issued_at'])
            ." AND assurance_expires_at=".db_escape((string)$custody['assurance_expires_at']),
            'could not reset bank-account verification evidence assurance')
        || (int)db_num_affected_rows() !== 1)
        return false;

    if (append_domain_audit_event(array(
        'company_id'=>(int)$company,'legal_entity_id'=>0,'entity_type'=>'security_session_assurance',
        'entity_key'=>array('assurance_event_id'=>(int)$event['assurance_event_id'],'session_id'=>$session_id),
        'action'=>'security.session.assurance.consumed',
        'reason_code'=>'hrm_fnd_004_bank_verification_evidence_registration',
        'before'=>array('assurance_level'=>2),'after'=>array('assurance_level'=>1),
        'restricted_diff'=>array(
            'action_class'=>HRM_BANK_ACCOUNT_VERIFICATION_EVIDENCE_REGISTRATION_ACTION_CLASS,
            'action_hash'=>$action_hash,'single_use_event_consumed'=>true,
            'session_assurance_reset_to_level_one'=>true,'bank_value_stored'=>false,'bank_value_revealed'=>false,
            'bank_verification_promoted'=>false,'bank_approval_promoted'=>false,
            'payment_binding_authority_changed'=>false,'payment_execution_authority_changed'=>false,
            'payroll_authority_changed'=>false,'accounting_authority_changed'=>false
        )
    )) === false)
        return false;

    return array('assurance_event_id'=>(int)$event['assurance_event_id'],'consumed_at'=>$consumed_at);
}

/** Compare an existing evidence row to the immutable exact-retry payload.
 * @param array $row
 * @param array $expected
 * @return bool
 */
function hrm_bank_account_verification_evidence_exact_retry($row, $expected)
{
    if (!is_array($row)) return false;
    foreach ($expected as $key=>$value) {
        if (!array_key_exists($key, $row) || (string)$row[$key] !== (string)$value)
            return false;
    }
    return true;
}

/** Register one opaque external verification-evidence record without promoting bank state.
 * @return array|false
 */
function register_hrm_person_bank_account_verification_evidence(
    $verification_request_id, $request_row_version, $approval_draft_id, $verification_method,
    $evidence_source, $evidence_reference_hash, $evidence_payload_hash, $verified_at_epoch,
    $now_epoch = null
) {
    $verification_request_id = hrm_bank_account_verification_request_positive_int($verification_request_id);
    $request_row_version = hrm_bank_account_verification_request_positive_int($request_row_version);
    $approval_draft_id = hrm_bank_account_verification_request_positive_int($approval_draft_id);
    $verification_method = hrm_bank_account_verification_request_method($verification_method);
    $evidence_source = hrm_bank_account_verification_evidence_source($evidence_source);
    $evidence_reference_hash = hrm_bank_account_verification_evidence_hex64($evidence_reference_hash);
    $evidence_payload_hash = hrm_bank_account_verification_evidence_hex64($evidence_payload_hash);
    $now = $now_epoch === null ? time() : $now_epoch;
    $actor = hrm_bank_account_verification_request_actor_id();
    $company = function_exists('user_company') ? (int)user_company() : 0;
    if ($verification_request_id === false || $request_row_version === false || $approval_draft_id === false
        || $verification_method === false || $evidence_source === false
        || $evidence_reference_hash === false || $evidence_payload_hash === false
        || !is_int($verified_at_epoch) || $verified_at_epoch <= 0 || !is_int($now) || $now <= 0
        || $verified_at_epoch > $now || $actor <= 0 || !hrm_bank_account_verification_request_permission())
        return false;

    begin_transaction();
    $ok=false; $result_value=false;
    do {
        $request = hrm_bank_account_verification_evidence_request_for_update($verification_request_id);
        if (!is_array($request) || (int)$request['request_row_version'] !== $request_row_version
            || (string)$request['request_status'] !== 'approved'
            || (string)$request['verification_method'] !== $verification_method
            || (int)$request['requested_by'] <= 0 || (int)$request['requested_by'] === $actor)
            break;
        $requested_at = strtotime((string)$request['requested_at'].' UTC');
        if ($requested_at === false || $verified_at_epoch < $requested_at)
            break;

        $bank = hrm_bank_account_verification_request_bank_for_update((int)$request['person_bank_account_id']);
        if (!is_array($bank)
            || (int)$bank['row_version'] !== (int)$request['bank_account_row_version']
            || $bank['effective_to'] !== null || (string)$bank['source'] !== 'governed_supersession'
            || (int)$bank['supersedes_person_bank_account_id'] <= 0
            || (int)$bank['created_by'] <= 0 || (int)$bank['created_by'] === (int)$request['requested_by']
            || (int)$bank['created_by'] === $actor
            || (string)$bank['verification_status'] !== 'unverified'
            || (string)$bank['verification_method'] !== ''
            || (string)$bank['approval_status'] !== 'unapproved')
            break;

        $draft = hrm_bank_account_verification_evidence_draft_for_update($approval_draft_id);
        if (!is_array($draft) || !defined('ST_HRM_BANK_ACCOUNT_VERIFICATION')
            || (int)$draft['trans_type'] !== (int)ST_HRM_BANK_ACCOUNT_VERIFICATION
            || (int)$draft['status'] !== 1 || (int)$draft['submitted_by'] !== (int)$request['requested_by']
            || (int)$draft['approved_trans_no'] !== $verification_request_id
            || $draft['completed_date'] === null)
            break;
        $draft_data = json_decode((string)$draft['draft_data'], true);
        if (!is_array($draft_data)
            || !isset($draft_data['verification_request_id'],$draft_data['person_bank_account_id'],
                $draft_data['bank_account_row_version'],$draft_data['verification_method'])
            || (int)$draft_data['verification_request_id'] !== $verification_request_id
            || (int)$draft_data['person_bank_account_id'] !== (int)$request['person_bank_account_id']
            || (int)$draft_data['bank_account_row_version'] !== (int)$request['bank_account_row_version']
            || (string)$draft_data['verification_method'] !== $verification_method)
            break;

        $checker = hrm_bank_account_verification_evidence_final_checker_for_update($approval_draft_id);
        if (!is_array($checker) || (int)$checker['user_id'] !== $actor
            || (int)$checker['user_id'] === (int)$request['requested_by']
            || (int)$checker['user_id'] === (int)$bank['created_by'])
            break;

        $verified_at = gmdate('Y-m-d H:i:s', $verified_at_epoch);
        $expected = array(
            'verification_request_id'=>$verification_request_id,
            'person_bank_account_id'=>(int)$request['person_bank_account_id'],
            'bank_account_row_version'=>(int)$request['bank_account_row_version'],
            'request_row_version'=>$request_row_version,
            'approval_draft_id'=>$approval_draft_id,'verification_method'=>$verification_method,
            'evidence_source'=>$evidence_source,'evidence_reference_hash'=>$evidence_reference_hash,
            'evidence_payload_hash'=>$evidence_payload_hash,'verified_at'=>$verified_at,
            'evidence_row_version'=>1,'recorded_by'=>$actor
        );
        $existing = hrm_bank_account_verification_evidence_existing_for_update($verification_request_id);
        if ($existing === false) break;
        if (is_array($existing)) {
            if (!hrm_bank_account_verification_evidence_exact_retry($existing, $expected))
                break;
            $ok=true;
            $result_value=array('verification_evidence_id'=>(int)$existing['verification_evidence_id'],
                'status'=>'already_registered','action_hash'=>null);
            break;
        }

        $action_hash = hrm_bank_account_verification_evidence_action_hash(
            $company,$verification_request_id,$request_row_version,(int)$request['person_bank_account_id'],
            (int)$request['bank_account_row_version'],$approval_draft_id,$verification_method,$evidence_source,
            $evidence_reference_hash,$evidence_payload_hash,$verified_at_epoch
        );
        $assurance = $action_hash === false ? false
            : hrm_bank_account_verification_evidence_consume_assurance($company,$action_hash,$now);
        if (!is_array($assurance)) break;

        if (!db_query("INSERT INTO ".TB_PREF."hrm_person_bank_account_verification_evidence"
            ."(verification_request_id,person_bank_account_id,bank_account_row_version,request_row_version,approval_draft_id,"
            ."verification_method,evidence_source,evidence_reference_hash,evidence_payload_hash,"
            ."checker_assurance_event_id,verified_at,evidence_row_version,recorded_by) VALUES ("
            .$verification_request_id.",".(int)$request['person_bank_account_id'].",".(int)$request['bank_account_row_version']
            .",".$request_row_version.",".$approval_draft_id.",".db_escape($verification_method).","
            .db_escape($evidence_source).",".db_escape($evidence_reference_hash).",".db_escape($evidence_payload_hash)
            .",".(int)$assurance['assurance_event_id'].",".db_escape($verified_at).",1,".$actor.")",
            'could not register bank-account verification evidence'))
            break;
        $evidence_id=(int)db_insert_id();
        if ($evidence_id <= 0) break;

        if (append_domain_audit_event(array(
            'company_id'=>$company,'legal_entity_id'=>0,
            'entity_type'=>'hrm_person_bank_account_verification_evidence',
            'entity_key'=>array('verification_evidence_id'=>$evidence_id,
                'verification_request_id'=>$verification_request_id),
            'action'=>'hrm.person_bank_account.verification_evidence.registered',
            'reason_code'=>'hrm_fnd_004_bank_verification_evidence_registration',
            'before'=>array(),'after'=>array('evidence_row_version'=>1),
            'restricted_diff'=>array(
                'verification_request_id'=>$verification_request_id,
                'request_row_version'=>$request_row_version,
                'person_bank_account_id'=>(int)$request['person_bank_account_id'],
                'bank_account_row_version'=>(int)$request['bank_account_row_version'],
                'approval_draft_id'=>$approval_draft_id,
                'verification_method'=>$verification_method,'evidence_source'=>$evidence_source,
                'evidence_reference_hash'=>$evidence_reference_hash,'evidence_payload_hash'=>$evidence_payload_hash,
                'checker_assurance_event_id'=>(int)$assurance['assurance_event_id'],
                'verified_at'=>$verified_at,'evidence_row_version'=>1,
                'requester_user_id'=>(int)$request['requested_by'],
                'original_supersession_maker_user_id'=>(int)$bank['created_by'],
                'final_checker_user_id'=>$actor,'central_approval_completed'=>true,
                'maker_requester_checker_separated'=>true,'bank_value_stored'=>false,
                'bank_value_revealed'=>false,'bank_verification_promoted'=>false,
                'bank_approval_promoted'=>false,'payment_binding_authority_changed'=>false,
                'payment_execution_authority_changed'=>false,'payroll_authority_changed'=>false,
                'accounting_authority_changed'=>false
            )
        )) === false)
            break;

        $ok=true;
        $result_value=array('verification_evidence_id'=>$evidence_id,'status'=>'registered','action_hash'=>$action_hash);
    } while (false);

    if ($ok) commit_transaction(); else cancel_transaction();
    return $ok ? $result_value : false;
}

/** Lock one exact registered verification-evidence row for bank-account promotion.
 * @param int $evidence_id
 * @return array|false
 */
function hrm_bank_account_verification_promotion_evidence_for_update($evidence_id)
{
    $result = db_query("SELECT verification_evidence_id,verification_request_id,person_bank_account_id,bank_account_row_version,"
        ."request_row_version,approval_draft_id,verification_method,evidence_source,evidence_reference_hash,"
        ."evidence_payload_hash,checker_assurance_event_id,verified_at,evidence_row_version,recorded_at,recorded_by FROM "
        .TB_PREF."hrm_person_bank_account_verification_evidence WHERE verification_evidence_id=".(int)$evidence_id
        ." LIMIT 2 FOR UPDATE", 'could not lock bank-account verification evidence for promotion');
    if (!$result) return false;
    $row = db_fetch_assoc($result);
    return $row && db_fetch_assoc($result) === false ? $row : false;
}

/** Lock the already-consumed checker assurance event bound to registered bank evidence.
 * @param int $assurance_event_id
 * @return array|false
 */
function hrm_bank_account_verification_promotion_assurance_for_update($assurance_event_id)
{
    $result = db_query("SELECT assurance_event_id,session_id,assurance_level,assurance_method,action_class,"
        ."action_hash,issued_at,expires_at,consumed_at FROM ".TB_PREF."user_session_assurance_events "
        ."WHERE assurance_event_id=".(int)$assurance_event_id." LIMIT 2 FOR UPDATE",
        'could not lock consumed checker assurance for bank-account promotion');
    if (!$result) return false;
    $row = db_fetch_assoc($result);
    return $row && db_fetch_assoc($result) === false ? $row : false;
}

/**
 * Promote one exact current governed bank-account successor from
 * unverified/unapproved to verified/approved.
 *
 * No second assurance event is consumed. The command proves custody of the
 * exact already-consumed evidence-registration assurance event, then performs
 * one compare-and-set metadata transition and one restricted PAY-AUD append.
 * Protected bank material is neither decrypted nor copied.
 *
 * @return array|false
 */
function promote_hrm_person_bank_account_verification(
    $verification_evidence_id, $evidence_row_version, $request_row_version, $bank_account_row_version
) {
    $verification_evidence_id = hrm_bank_account_verification_request_positive_int($verification_evidence_id);
    $evidence_row_version = hrm_bank_account_verification_request_positive_int($evidence_row_version);
    $request_row_version = hrm_bank_account_verification_request_positive_int($request_row_version);
    $bank_account_row_version = hrm_bank_account_verification_request_positive_int($bank_account_row_version);
    $actor = hrm_bank_account_verification_request_actor_id();
    $company = function_exists('user_company') ? (int)user_company() : 0;
    if ($verification_evidence_id === false || $evidence_row_version === false
        || $request_row_version === false || $bank_account_row_version === false
        || $actor <= 0 || !hrm_bank_account_verification_request_permission())
        return false;

    begin_transaction();
    $ok = false;
    $result_value = false;
    do {
        $evidence = hrm_bank_account_verification_promotion_evidence_for_update($verification_evidence_id);
        if (!is_array($evidence)
            || (int)$evidence['evidence_row_version'] !== $evidence_row_version
            || (int)$evidence['request_row_version'] !== $request_row_version
            || (int)$evidence['bank_account_row_version'] !== $bank_account_row_version
            || (int)$evidence['recorded_by'] !== $actor)
            break;

        $request = hrm_bank_account_verification_evidence_request_for_update((int)$evidence['verification_request_id']);
        if (!is_array($request)
            || (int)$request['request_row_version'] !== $request_row_version
            || (string)$request['request_status'] !== 'approved'
            || (int)$request['person_bank_account_id'] !== (int)$evidence['person_bank_account_id']
            || (int)$request['bank_account_row_version'] !== $bank_account_row_version
            || (string)$request['verification_method'] !== (string)$evidence['verification_method']
            || (int)$request['requested_by'] <= 0 || (int)$request['requested_by'] === $actor)
            break;

        $draft = hrm_bank_account_verification_evidence_draft_for_update((int)$evidence['approval_draft_id']);
        if (!is_array($draft) || !defined('ST_HRM_BANK_ACCOUNT_VERIFICATION')
            || (int)$draft['trans_type'] !== (int)ST_HRM_BANK_ACCOUNT_VERIFICATION
            || (int)$draft['status'] !== 1
            || (int)$draft['submitted_by'] !== (int)$request['requested_by']
            || (int)$draft['approved_trans_no'] !== (int)$request['verification_request_id']
            || $draft['completed_date'] === null)
            break;
        $draft_data = json_decode((string)$draft['draft_data'], true);
        if (!is_array($draft_data)
            || !isset($draft_data['verification_request_id'],$draft_data['person_bank_account_id'],
                $draft_data['bank_account_row_version'],$draft_data['verification_method'])
            || (int)$draft_data['verification_request_id'] !== (int)$request['verification_request_id']
            || (int)$draft_data['person_bank_account_id'] !== (int)$evidence['person_bank_account_id']
            || (int)$draft_data['bank_account_row_version'] !== $bank_account_row_version
            || (string)$draft_data['verification_method'] !== (string)$evidence['verification_method'])
            break;

        $checker = hrm_bank_account_verification_evidence_final_checker_for_update((int)$evidence['approval_draft_id']);
        if (!is_array($checker) || (int)$checker['user_id'] !== $actor)
            break;

        $verified_at_epoch = strtotime((string)$evidence['verified_at'].' UTC');
        if ($verified_at_epoch === false || $verified_at_epoch <= 0)
            break;
        $expected_action_hash = hrm_bank_account_verification_evidence_action_hash(
            $company, (int)$evidence['verification_request_id'], $request_row_version,
            (int)$evidence['person_bank_account_id'], $bank_account_row_version, (int)$evidence['approval_draft_id'],
            (string)$evidence['verification_method'], (string)$evidence['evidence_source'],
            (string)$evidence['evidence_reference_hash'], (string)$evidence['evidence_payload_hash'],
            $verified_at_epoch
        );
        $assurance = hrm_bank_account_verification_promotion_assurance_for_update(
            (int)$evidence['checker_assurance_event_id']
        );
        if ($expected_action_hash === false || !is_array($assurance)
            || (int)$assurance['assurance_event_id'] !== (int)$evidence['checker_assurance_event_id']
            || (int)$assurance['assurance_level'] !== 2
            || (string)$assurance['assurance_method'] !== 'oidc_acr_amr'
            || (string)$assurance['action_class'] !== HRM_BANK_ACCOUNT_VERIFICATION_EVIDENCE_REGISTRATION_ACTION_CLASS
            || !session_registry_hash_equals((string)$assurance['action_hash'], $expected_action_hash)
            || $assurance['consumed_at'] === null)
            break;

        $bank = hrm_bank_account_verification_request_bank_for_update((int)$evidence['person_bank_account_id']);
        if (!is_array($bank) || $bank['effective_to'] !== null
            || (string)$bank['source'] !== 'governed_supersession'
            || (int)$bank['supersedes_person_bank_account_id'] <= 0
            || (int)$bank['created_by'] <= 0
            || (int)$bank['created_by'] === (int)$request['requested_by']
            || (int)$bank['created_by'] === $actor)
            break;

        if ((int)$bank['row_version'] === $bank_account_row_version + 1
            && (string)$bank['verification_status'] === 'verified'
            && (string)$bank['approval_status'] === 'approved'
            && (string)$bank['verification_method'] === (string)$evidence['verification_method']) {
            $ok = true;
            $result_value = array(
                'person_bank_account_id'=>(int)$evidence['person_bank_account_id'],
                'verification_evidence_id'=>$verification_evidence_id,
                'bank_account_row_version'=>$bank_account_row_version + 1,
                'status'=>'already_promoted'
            );
            break;
        }

        if ((int)$bank['row_version'] !== $bank_account_row_version
            || (string)$bank['verification_status'] !== 'unverified'
            || (string)$bank['approval_status'] !== 'unapproved'
            || (string)$bank['verification_method'] !== '')
            break;

        if (!db_query("UPDATE ".TB_PREF."hrm_person_bank_accounts SET verification_status='verified',"
            ."verification_method=".db_escape((string)$evidence['verification_method'])
            .",approval_status='approved',row_version=row_version+1 WHERE person_bank_account_id=".(int)$evidence['person_bank_account_id']
            ." AND row_version=".$bank_account_row_version." AND effective_to IS NULL"
            ." AND source='governed_supersession' AND supersedes_person_bank_account_id IS NOT NULL"
            ." AND verification_status='unverified' AND approval_status='unapproved' AND verification_method=''",
            'could not promote HRM Person bank-account verification state')
            || !function_exists('db_num_affected_rows') || (int)db_num_affected_rows() !== 1)
            break;

        if (append_domain_audit_event(array(
            'company_id'=>$company,'legal_entity_id'=>0,
            'entity_type'=>'hrm_person_bank_account_verification',
            'entity_key'=>array('person_bank_account_id'=>(int)$evidence['person_bank_account_id'],
                'verification_evidence_id'=>$verification_evidence_id),
            'action'=>'hrm.person_bank_account.verification.promoted',
            'reason_code'=>'hrm_fnd_004_bank_verification_promotion',
            'before'=>array('verification_status'=>'unverified','approval_status'=>'unapproved',
                'row_version'=>$bank_account_row_version),
            'after'=>array('verification_status'=>'verified','approval_status'=>'approved',
                'verification_method'=>(string)$evidence['verification_method'],
                'row_version'=>$bank_account_row_version+1),
            'restricted_diff'=>array(
                'verification_request_id'=>(int)$evidence['verification_request_id'],
                'request_row_version'=>$request_row_version,
                'verification_evidence_id'=>$verification_evidence_id,
                'evidence_row_version'=>$evidence_row_version,
                'approval_draft_id'=>(int)$evidence['approval_draft_id'],
                'checker_assurance_event_id'=>(int)$evidence['checker_assurance_event_id'],
                'requester_user_id'=>(int)$request['requested_by'],
                'original_supersession_maker_user_id'=>(int)$bank['created_by'],
                'final_checker_user_id'=>$actor,
                'maker_requester_checker_separated'=>true,
                'checker_assurance_preconsumed'=>true,'final_checker_bound'=>true,
                'verification_method'=>(string)$evidence['verification_method'],
                'evidence_source'=>(string)$evidence['evidence_source'],
                'evidence_reference_hash'=>(string)$evidence['evidence_reference_hash'],
                'evidence_payload_hash'=>(string)$evidence['evidence_payload_hash'],
                'bank_value_stored'=>false,'bank_value_revealed'=>false,
                'request_state_changed'=>false,'approval_draft_state_changed'=>false,
                'new_assurance_event_consumed'=>false,'payment_election_changed'=>false,
                'payment_allocation_changed'=>false,'payment_binding_authority_changed'=>false,
                'payment_execution_authority_changed'=>false,'payroll_authority_changed'=>false,
                'accounting_authority_changed'=>false
            )
        )) === false)
            break;

        $ok = true;
        $result_value = array(
            'person_bank_account_id'=>(int)$evidence['person_bank_account_id'],
            'verification_evidence_id'=>$verification_evidence_id,
            'bank_account_row_version'=>$bank_account_row_version+1,
            'verification_status'=>'verified','approval_status'=>'approved',
            'status'=>'promoted'
        );
    } while (false);

    if ($ok) commit_transaction(); else cancel_transaction();
    return $ok ? $result_value : false;
}
