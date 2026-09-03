<?php
/**
 * HRM-FND-004 bank-account verification-request command.
 *
 * The maker command creates only a pending verification request and central
 * ApprovalWorkflow draft after consuming one exact action-bound PAY-SEC-004
 * level-two assurance event. Checker approval/rejection changes only request
 * state; bank-account verification/approval state remains untouched for the
 * later promotion slice. No bank value is decrypted or copied here.
 */

$hrm_bank_account_verification_request_repo_root = dirname(dirname(dirname(__DIR__)));
$hrm_bank_account_verification_request_dependencies = array(
    array('/hrm/includes/hrm_security.inc', 'function', 'hrm_user_can_access_sensitive_field'),
    array('/includes/federation_assurance_consumer.inc', 'function', 'federation_assurance_consumer_current_session_id'),
    array('/includes/approval/approval_workflow.inc', 'class', 'ApprovalWorkflow'),
    array('/includes/approval/db/approval_rules_db.inc', 'function', 'get_approval_workflow_by_type'),
    array('/includes/db/domain_audit_db.inc', 'function', 'append_domain_audit_event'),
);
foreach ($hrm_bank_account_verification_request_dependencies as $dependency) {
    $loaded = $dependency[1] === 'class'
        ? class_exists($dependency[2], false)
        : function_exists($dependency[2]);
    if ($loaded)
        continue;
    $candidate = $path_to_root.$dependency[0];
    if (!is_file($candidate))
        $candidate = $hrm_bank_account_verification_request_repo_root.$dependency[0];
    if (is_file($candidate))
        include_once($candidate);
}
unset($hrm_bank_account_verification_request_dependencies, $hrm_bank_account_verification_request_repo_root, $dependency, $loaded, $candidate);

if (!defined('HRM_BANK_ACCOUNT_VERIFICATION_REQUEST_ACTION_CLASS'))
    define('HRM_BANK_ACCOUNT_VERIFICATION_REQUEST_ACTION_CLASS', 'hrm.person_bank_account.verification.request');
if (!defined('HRM_BANK_ACCOUNT_VERIFICATION_REQUEST_HASH_CONTRACT'))
    define('HRM_BANK_ACCOUNT_VERIFICATION_REQUEST_HASH_CONTRACT', 'notrinoserp.hrm-fnd-004.person-bank-verification-request');
if (!defined('HRM_BANK_ACCOUNT_VERIFICATION_REQUEST_HASH_VERSION'))
    define('HRM_BANK_ACCOUNT_VERIFICATION_REQUEST_HASH_VERSION', '1');

function hrm_bank_account_verification_request_method($value)
{
    return is_string($value) && strlen($value) >= 1 && strlen($value) <= 32
        && preg_match('/^[a-z][a-z0-9_]{0,31}$/D', $value) === 1 ? $value : false;
}

function hrm_bank_account_verification_request_positive_int($value)
{
    if (is_int($value))
        return $value > 0 ? $value : false;
    if (!is_string($value) || preg_match('/^[1-9][0-9]*$/D', $value) !== 1)
        return false;
    $integer = (int)$value;
    return $integer > 0 && (string)$integer === $value ? $integer : false;
}

function hrm_bank_account_verification_request_actor_id()
{
    return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
        && isset($_SESSION['wa_current_user']->user) ? (int)$_SESSION['wa_current_user']->user : 0;
}

function hrm_bank_account_verification_request_action_hash($company, $person_bank_account_id, $bank_account_row_version, $verification_method)
{
    $company = (int)$company;
    $person_bank_account_id = hrm_bank_account_verification_request_positive_int($person_bank_account_id);
    $bank_account_row_version = hrm_bank_account_verification_request_positive_int($bank_account_row_version);
    $verification_method = hrm_bank_account_verification_request_method($verification_method);
    if ($company < 0 || $person_bank_account_id === false || $bank_account_row_version === false
        || $verification_method === false)
        return false;
    return hash('sha256',
        HRM_BANK_ACCOUNT_VERIFICATION_REQUEST_HASH_CONTRACT
        .'|v='.HRM_BANK_ACCOUNT_VERIFICATION_REQUEST_HASH_VERSION
        .'|company='.$company
        .'|action='.HRM_BANK_ACCOUNT_VERIFICATION_REQUEST_ACTION_CLASS
        .'|person_bank_account_id='.$person_bank_account_id
        .'|bank_account_row_version='.$bank_account_row_version
        .'|verification_method='.$verification_method
    );
}

function hrm_bank_account_verification_request_permission()
{
    return function_exists('hrm_user_can_access_sensitive_field')
        && hrm_user_can_access_sensitive_field(
            HRM_FIELD_RESTRICTED_FINANCIAL,
            HRM_FIELD_ACTION_SUPERSEDE
        );
}

function hrm_bank_account_verification_request_bank_for_update($person_bank_account_id)
{
    $person_bank_account_id = (int)$person_bank_account_id;
    if ($person_bank_account_id <= 0)
        return false;
    $result = db_query("SELECT person_bank_account_id,person_id,supersedes_person_bank_account_id,effective_to,verification_status,"
        ."verification_method,approval_status,row_version,source,created_by FROM ".TB_PREF."hrm_person_bank_accounts "
        ."WHERE person_bank_account_id=".$person_bank_account_id." LIMIT 2 FOR UPDATE",
        'could not lock HRM bank-account verification-request target');
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    return $row && db_fetch_assoc($result) === false ? $row : false;
}

function hrm_bank_account_verification_request_existing_active($person_bank_account_id, $row_version)
{
    $result = db_query("SELECT verification_request_id FROM ".TB_PREF."hrm_person_bank_account_verification_requests "
        ."WHERE person_bank_account_id=".(int)$person_bank_account_id." AND bank_account_row_version=".(int)$row_version
        ." AND request_status IN ('pending','approved') ORDER BY verification_request_id DESC LIMIT 2 FOR UPDATE",
        'could not verify existing HRM bank-account verification request');
    if (!$result)
        return false;
    $first = db_fetch_assoc($result);
    if (!$first)
        return null;
    return db_fetch_assoc($result) === false ? (int)$first['verification_request_id'] : false;
}

function hrm_bank_account_verification_request_workflow_is_maker_checker()
{
    if (!defined('ST_HRM_BANK_ACCOUNT_VERIFICATION') || !function_exists('get_approval_workflow_by_type'))
        return false;
    $workflow = get_approval_workflow_by_type(ST_HRM_BANK_ACCOUNT_VERIFICATION);
    if (!$workflow || (int)$workflow['is_active'] !== 1 || (int)$workflow['allow_self_approve'] !== 0)
        return false;
    $levels = get_approval_levels((int)$workflow['id'], true);
    if (!$levels)
        return false;
    $count = 0;
    while ($level = db_fetch($levels)) {
        $count++;
        if ((int)$level['is_active'] !== 1 || (int)$level['role_id'] <= 0
            || (int)$level['min_approvers'] < 1 || (float)$level['amount_threshold'] != 0.0)
            return false;
    }
    return $count > 0;
}

function hrm_bank_account_verification_request_assurance_event_for_update($session_id, $action_hash)
{
    if ((int)$session_id <= 0 || !federation_assurance_consumer_hex64($action_hash))
        return false;
    $result = db_query("SELECT assurance_event_id,session_id,assurance_level,assurance_method,action_class,"
        ."action_hash,nonce_hash,issued_at,expires_at,consumed_at FROM ".TB_PREF."user_session_assurance_events "
        ."WHERE session_id=".(int)$session_id." AND action_class="
        .db_escape(HRM_BANK_ACCOUNT_VERIFICATION_REQUEST_ACTION_CLASS)." AND action_hash=".db_escape($action_hash)
        ." ORDER BY assurance_event_id DESC LIMIT 2 FOR UPDATE",
        'could not lock HRM bank-account verification-request assurance event');
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    if (!$row)
        return null;
    return db_fetch_assoc($result) === false ? $row : false;
}

function hrm_bank_account_verification_request_consume_assurance($company, $action_hash, $now_epoch)
{
    $session_id = federation_assurance_consumer_current_session_id();
    if ($session_id <= 0)
        return false;
    $custody = federation_assurance_consumer_custody_for_update($session_id);
    if (!federation_assurance_consumer_current_custody_valid($custody, $now_epoch, true))
        return false;
    $event = hrm_bank_account_verification_request_assurance_event_for_update($session_id, $action_hash);
    if (!is_array($event)
        || (int)$event['session_id'] !== $session_id
        || (int)$event['assurance_level'] !== 2
        || (string)$event['assurance_method'] !== 'oidc_acr_amr'
        || (string)$event['action_class'] !== HRM_BANK_ACCOUNT_VERIFICATION_REQUEST_ACTION_CLASS
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
        ." WHERE assurance_event_id=".(int)$event['assurance_event_id']
        ." AND consumed_at IS NULL AND expires_at>".db_escape($consumed_at),
        'could not consume HRM bank-account verification-request assurance event')
        || !function_exists('db_num_affected_rows') || (int)db_num_affected_rows() !== 1)
        return false;

    $restore = federation_assurance_consumer_restore_level_one_sql($custody, $now_epoch);
    if ($restore === false
        || !db_query("UPDATE ".TB_PREF."user_sessions SET ".$restore." WHERE id=".$session_id
            ." AND revoked_at IS NULL AND assurance_level=2 AND assurance_method='oidc_acr_amr'"
            ." AND assurance_issued_at=".db_escape((string)$custody['assurance_issued_at'])
            ." AND assurance_expires_at=".db_escape((string)$custody['assurance_expires_at']),
            'could not reset HRM bank-account verification-request assurance')
        || (int)db_num_affected_rows() !== 1)
        return false;

    if (append_domain_audit_event(array(
        'company_id'=>(int)$company,'legal_entity_id'=>0,
        'entity_type'=>'security_session_assurance',
        'entity_key'=>array('assurance_event_id'=>(int)$event['assurance_event_id'],'session_id'=>$session_id),
        'action'=>'security.session.assurance.consumed',
        'reason_code'=>'hrm_fnd_004_person_bank_verification_request',
        'before'=>array('assurance_level'=>2),'after'=>array('assurance_level'=>1),
        'restricted_diff'=>array(
            'action_class'=>HRM_BANK_ACCOUNT_VERIFICATION_REQUEST_ACTION_CLASS,
            'action_hash'=>(string)$action_hash,
            'single_use_event_consumed'=>true,
            'session_assurance_reset_to_level_one'=>true,
            'bank_value_stored'=>false,'bank_value_revealed'=>false,
            'bank_verification_promoted'=>false,'bank_approval_promoted'=>false,
            'payment_authority_changed'=>false,'payroll_authority_changed'=>false,
            'accounting_authority_changed'=>false
        )
    )) === false)
        return false;
    return array('assurance_event_id'=>(int)$event['assurance_event_id'],'consumed_at'=>$consumed_at);
}

function hrm_bank_account_verification_request_audit($action, $reason, $request, $extra)
{
    $restricted = array(
        'verification_request_id'=>(int)$request['verification_request_id'],
        'person_bank_account_id'=>(int)$request['person_bank_account_id'],
        'bank_account_row_version'=>(int)$request['bank_account_row_version'],
        'verification_method'=>(string)$request['verification_method'],
        'request_status'=>(string)$request['request_status'],
        'request_row_version'=>(int)$request['request_row_version'],
        'bank_value_stored'=>false,'bank_value_revealed'=>false,
        'bank_verification_promoted'=>false,'bank_approval_promoted'=>false,
        'payment_authority_changed'=>false,'payroll_authority_changed'=>false,
        'accounting_authority_changed'=>false
    );
    foreach ($extra as $key=>$value)
        $restricted[$key]=$value;
    return append_domain_audit_event(array(
        'company_id'=>function_exists('user_company')?(int)user_company():0,
        'legal_entity_id'=>0,
        'entity_type'=>'hrm_person_bank_account_verification_request',
        'entity_key'=>array('verification_request_id'=>(int)$request['verification_request_id']),
        'action'=>$action,'reason_code'=>$reason,
        'before'=>array(),'after'=>array('request_status'=>(string)$request['request_status']),
        'restricted_diff'=>$restricted
    )) !== false;
}

function submit_hrm_person_bank_account_verification_request($person_bank_account_id, $bank_account_row_version, $verification_method, $now_epoch = null)
{
    global $transaction_level;
    $person_bank_account_id = hrm_bank_account_verification_request_positive_int($person_bank_account_id);
    $bank_account_row_version = hrm_bank_account_verification_request_positive_int($bank_account_row_version);
    $verification_method = hrm_bank_account_verification_request_method($verification_method);
    $now = $now_epoch === null ? time() : $now_epoch;
    $actor = hrm_bank_account_verification_request_actor_id();
    $company = function_exists('user_company') ? (int)user_company() : 0;
    if ($person_bank_account_id === false || $bank_account_row_version === false || $verification_method === false
        || !is_int($now) || $now <= 0 || $actor <= 0 || !hrm_bank_account_verification_request_permission()
        || !hrm_bank_account_verification_request_workflow_is_maker_checker())
        return false;

    begin_transaction();
    $ok = false;
    $result_value = false;
    do {
        $bank = hrm_bank_account_verification_request_bank_for_update($person_bank_account_id);
        if (!is_array($bank)
            || (int)$bank['row_version'] !== $bank_account_row_version
            || $bank['effective_to'] !== null
            || (string)$bank['source'] !== 'governed_supersession'
            || (int)$bank['supersedes_person_bank_account_id'] <= 0
            || (int)$bank['created_by'] <= 0
            || (int)$bank['created_by'] === $actor
            || (string)$bank['verification_status'] !== 'unverified'
            || (string)$bank['verification_method'] !== ''
            || (string)$bank['approval_status'] !== 'unapproved')
            break;
        $existing = hrm_bank_account_verification_request_existing_active($person_bank_account_id, $bank_account_row_version);
        if ($existing === false || $existing !== null)
            break;
        $action_hash = hrm_bank_account_verification_request_action_hash(
            $company, $person_bank_account_id, $bank_account_row_version, $verification_method
        );
        if ($action_hash === false
            || hrm_bank_account_verification_request_consume_assurance($company, $action_hash, $now) === false)
            break;

        $requested_at = gmdate('Y-m-d H:i:s', $now);
        if (!db_query("INSERT INTO ".TB_PREF."hrm_person_bank_account_verification_requests"
            ."(person_bank_account_id,bank_account_row_version,verification_method,request_status,request_row_version,requested_at,requested_by) VALUES ("
            .$person_bank_account_id.",".$bank_account_row_version.",".db_escape($verification_method).",'pending',1,"
            .db_escape($requested_at).",".$actor.")",
            'could not create HRM bank-account verification request'))
            break;
        $request_id = db_insert_id();
        if ((int)$request_id <= 0)
            break;

        $approval = get_approval_workflow_service();
        $approval_result = $approval->submit(
            ST_HRM_BANK_ACCOUNT_VERIFICATION,
            array(
                'verification_request_id'=>(int)$request_id,
                'person_bank_account_id'=>$person_bank_account_id,
                'bank_account_row_version'=>$bank_account_row_version,
                'verification_method'=>$verification_method
            ),
            0,
            array('summary'=>sprintf(_('Bank account verification request #%d'), (int)$request_id))
        );
        if (!is_array($approval_result) || !isset($approval_result['status'],$approval_result['draft_id'])
            || $approval_result['status'] !== 'pending' || (int)$approval_result['draft_id'] <= 0)
            break;

        $request = array(
            'verification_request_id'=>(int)$request_id,'person_bank_account_id'=>$person_bank_account_id,
            'bank_account_row_version'=>$bank_account_row_version,'verification_method'=>$verification_method,
            'request_status'=>'pending','request_row_version'=>1
        );
        if (!hrm_bank_account_verification_request_audit(
            'hrm.person_bank_account.verification_request.submitted',
            'hrm_fnd_004_person_bank_verification_request',
            $request,
            array('approval_draft_id'=>(int)$approval_result['draft_id'],'requester_user_id'=>$actor,
                'original_supersession_maker_user_id'=>(int)$bank['created_by'],
                'requester_separated_from_original_maker'=>true,
                'central_approval_workflow'=>true,'maker_checker_required'=>true)
        ))
            break;
        $ok = true;
        $result_value = array(
            'verification_request_id'=>(int)$request_id,
            'approval_draft_id'=>(int)$approval_result['draft_id'],
            'status'=>'pending','action_hash'=>$action_hash
        );
    } while (false);

    if ($ok)
        commit_transaction();
    else
        cancel_transaction();
    return $ok ? $result_value : false;
}

function hrm_bank_account_verification_request_from_draft($draft_data, $for_update = true)
{
    if (!is_array($draft_data) || !isset($draft_data['verification_request_id'],$draft_data['person_bank_account_id'],
        $draft_data['bank_account_row_version'],$draft_data['verification_method']))
        return false;
    $request_id = hrm_bank_account_verification_request_positive_int($draft_data['verification_request_id']);
    $person_bank_account_id = hrm_bank_account_verification_request_positive_int($draft_data['person_bank_account_id']);
    $row_version = hrm_bank_account_verification_request_positive_int($draft_data['bank_account_row_version']);
    $method = hrm_bank_account_verification_request_method($draft_data['verification_method']);
    if ($request_id === false || $person_bank_account_id === false || $row_version === false || $method === false)
        return false;
    $sql = "SELECT verification_request_id,person_bank_account_id,bank_account_row_version,verification_method,request_status,"
        ."request_row_version,requested_at,requested_by FROM ".TB_PREF."hrm_person_bank_account_verification_requests "
        ."WHERE verification_request_id=".$request_id." LIMIT 2".($for_update ? ' FOR UPDATE' : '');
    $result = db_query($sql, 'could not lock HRM bank-account verification request');
    if (!$result)
        return false;
    $request = db_fetch_assoc($result);
    if (!$request || db_fetch_assoc($result) !== false
        || (int)$request['person_bank_account_id'] !== $person_bank_account_id
        || (int)$request['bank_account_row_version'] !== $row_version
        || (string)$request['verification_method'] !== $method)
        return false;
    return $request;
}

function execute_approved_bank_account_verification_request($draft_data, $reserved_trans_no, $draft_id)
{
    if (!hrm_bank_account_verification_request_permission())
        return false;
    $request = hrm_bank_account_verification_request_from_draft($draft_data, true);
    $actor = hrm_bank_account_verification_request_actor_id();
    if (!$request || $actor <= 0 || (int)$request['requested_by'] === $actor
        || (string)$request['request_status'] !== 'pending')
        return false;
    $bank = hrm_bank_account_verification_request_bank_for_update((int)$request['person_bank_account_id']);
    if (!$bank || (int)$bank['row_version'] !== (int)$request['bank_account_row_version']
        || $bank['effective_to'] !== null || (string)$bank['source'] !== 'governed_supersession'
        || (int)$bank['supersedes_person_bank_account_id'] <= 0
        || (int)$bank['created_by'] <= 0 || (int)$bank['created_by'] === $actor
        || (string)$bank['verification_status'] !== 'unverified'
        || (string)$bank['verification_method'] !== ''
        || (string)$bank['approval_status'] !== 'unapproved')
        return false;
    if (!db_query("UPDATE ".TB_PREF."hrm_person_bank_account_verification_requests SET request_status='approved',"
        ."request_row_version=request_row_version+1 WHERE verification_request_id=".(int)$request['verification_request_id']
        ." AND request_status='pending' AND request_row_version=".(int)$request['request_row_version'],
        'could not approve HRM bank-account verification request')
        || (int)db_num_affected_rows() !== 1)
        return false;
    $request['request_status']='approved';
    $request['request_row_version']=(int)$request['request_row_version']+1;
    return hrm_bank_account_verification_request_audit(
        'hrm.person_bank_account.verification_request.approved',
        'hrm_fnd_004_person_bank_verification_request_approval',
        $request,
        array('approval_draft_id'=>(int)$draft_id,'checker_user_id'=>$actor,
            'original_supersession_maker_user_id'=>(int)$bank['created_by'],
            'maker_checker_separated'=>true,'checker_separated_from_original_maker'=>true)
    ) ? (int)$request['verification_request_id'] : false;
}

function reject_bank_account_verification_request_draft($draft_data, $draft_id, $comments)
{
    if (!hrm_bank_account_verification_request_permission())
        return false;
    $request = hrm_bank_account_verification_request_from_draft($draft_data, true);
    $actor = hrm_bank_account_verification_request_actor_id();
    if (!$request || $actor <= 0 || (int)$request['requested_by'] === $actor
        || (string)$request['request_status'] !== 'pending')
        return false;
    $bank = hrm_bank_account_verification_request_bank_for_update((int)$request['person_bank_account_id']);
    if (!$bank || (int)$bank['row_version'] !== (int)$request['bank_account_row_version']
        || $bank['effective_to'] !== null || (string)$bank['source'] !== 'governed_supersession'
        || (int)$bank['supersedes_person_bank_account_id'] <= 0
        || (int)$bank['created_by'] <= 0 || (int)$bank['created_by'] === $actor
        || (string)$bank['verification_status'] !== 'unverified'
        || (string)$bank['verification_method'] !== ''
        || (string)$bank['approval_status'] !== 'unapproved')
        return false;
    if (!db_query("UPDATE ".TB_PREF."hrm_person_bank_account_verification_requests SET request_status='rejected',"
        ."request_row_version=request_row_version+1 WHERE verification_request_id=".(int)$request['verification_request_id']
        ." AND request_status='pending' AND request_row_version=".(int)$request['request_row_version'],
        'could not reject HRM bank-account verification request')
        || (int)db_num_affected_rows() !== 1)
        return false;
    $request['request_status']='rejected';
    $request['request_row_version']=(int)$request['request_row_version']+1;
    return hrm_bank_account_verification_request_audit(
        'hrm.person_bank_account.verification_request.rejected',
        'hrm_fnd_004_person_bank_verification_request_rejection',
        $request,
        array('approval_draft_id'=>(int)$draft_id,'checker_user_id'=>$actor,
            'original_supersession_maker_user_id'=>(int)$bank['created_by'],
            'maker_checker_separated'=>true,'checker_separated_from_original_maker'=>true,
            'comment_text_stored'=>false)
    );
}

function cancel_bank_account_verification_request_draft($draft_data, $draft_id, $comments)
{
    if (!hrm_bank_account_verification_request_permission())
        return false;
    $request = hrm_bank_account_verification_request_from_draft($draft_data, true);
    $actor = hrm_bank_account_verification_request_actor_id();
    if (!$request || $actor <= 0 || (int)$request['requested_by'] !== $actor
        || (string)$request['request_status'] !== 'pending')
        return false;
    $bank = hrm_bank_account_verification_request_bank_for_update((int)$request['person_bank_account_id']);
    if (!$bank || (int)$bank['row_version'] !== (int)$request['bank_account_row_version']
        || $bank['effective_to'] !== null || (string)$bank['source'] !== 'governed_supersession'
        || (int)$bank['supersedes_person_bank_account_id'] <= 0
        || (int)$bank['created_by'] <= 0 || (int)$bank['created_by'] === $actor
        || (string)$bank['verification_status'] !== 'unverified'
        || (string)$bank['verification_method'] !== ''
        || (string)$bank['approval_status'] !== 'unapproved')
        return false;
    if (!db_query("UPDATE ".TB_PREF."hrm_person_bank_account_verification_requests SET request_status='cancelled',"
        ."request_row_version=request_row_version+1 WHERE verification_request_id=".(int)$request['verification_request_id']
        ." AND request_status='pending' AND request_row_version=".(int)$request['request_row_version'],
        'could not cancel HRM bank-account verification request')
        || (int)db_num_affected_rows() !== 1)
        return false;
    $request['request_status']='cancelled';
    $request['request_row_version']=(int)$request['request_row_version']+1;
    return hrm_bank_account_verification_request_audit(
        'hrm.person_bank_account.verification_request.cancelled',
        'hrm_fnd_004_person_bank_verification_request_cancellation',
        $request,
        array('approval_draft_id'=>(int)$draft_id,'cancelled_by_requester'=>true,
            'original_supersession_maker_user_id'=>(int)$bank['created_by'],
            'requester_separated_from_original_maker'=>true,'comment_text_stored'=>false)
    );
}
