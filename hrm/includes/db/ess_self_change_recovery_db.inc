<?php
/** HRM-ESS-001 terminal recovery semantics for bounded self-profile requests. */
if (!function_exists('execute_hrm_ess_self_profile_change'))
    include_once($path_to_root.'/hrm/includes/db/ess_self_change_executor_db.inc');

function hrm_ess_self_change_terminal_reason_is_valid($reason)
{
    return is_string($reason) && preg_match('/^[a-z0-9][a-z0-9_.:-]{2,63}$/D', $reason) === 1;
}

/** Validate immutable envelope/digest custody without requiring payload-key availability. */
function hrm_ess_self_change_envelope_matches($request)
{
    if (!is_array($request) || !isset($request['payload_ciphertext'],$request['payload_digest'],$request['request_digest'],
        $request['baseline_auth_tag'],$request['request_nonce_hash']))
        return false;
    return hrm_ess_self_change_digest_is_valid((string)$request['payload_digest'])
        && hash_equals((string)$request['payload_digest'], hash('sha256', (string)$request['payload_ciphertext']))
        && hrm_ess_self_change_digest_is_valid((string)$request['baseline_auth_tag'])
        && hrm_ess_self_change_digest_is_valid((string)$request['request_digest'])
        && hash_equals((string)$request['request_digest'], hrm_ess_self_change_request_digest(
            (int)$request['user_id'], (int)$request['worker_id'], (string)$request['payload_digest'],
            (string)$request['baseline_auth_tag'], (string)$request['request_nonce_hash']));
}

function hrm_ess_self_change_terminal_audit($request, $action, $from, $to, $reason, $actor)
{
    return append_domain_audit_event(array(
        'company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,
        'entity_type'=>'hrm_ess_self_change_request','entity_key'=>array('request_id'=>(int)$request['request_id'],'worker_id'=>(int)$request['worker_id']),
        'action'=>$action,'reason_code'=>$reason,'before'=>array('request_status'=>$from),
        'after'=>array('request_status'=>$to,'actor_id'=>(int)$actor),
        'restricted_diff'=>array('values_logged'=>false,'profile_mutation_performed'=>false,'terminal_state'=>true,
            'payload_decryption_required'=>false,'bank_tax_fields_allowed'=>false,'compensation_fields_allowed'=>false,
            'historical_payroll_accounting_rows_rewritten'=>0)
    )) !== false;
}

/** Distinct authorized checker may reject a draft or approved-but-not-applied request. */
function reject_hrm_ess_self_profile_change($request_id, $reason_code)
{
    $actor=hrm_ess_self_change_actor_id(); $request_id=(int)$request_id; $reason_code=(string)$reason_code;
    if ($actor<=0 || $request_id<=0 || !hrm_ess_self_change_user_can_approve()
        || !hrm_ess_self_change_terminal_reason_is_valid($reason_code)) return false;
    begin_transaction();
    $request=get_hrm_ess_self_change_request($request_id,true);
    if (!is_array($request) || (int)$request['requested_by']===$actor || !hrm_ess_self_change_envelope_matches($request)) {
        cancel_transaction(); return false;
    }
    if ((string)$request['request_status']==='rejected') { commit_transaction(); return true; }
    if (!in_array((string)$request['request_status'],array('draft','approved'),true)) { cancel_transaction(); return false; }
    $receipts=db_query('SELECT receipt_id FROM '.TB_PREF.'hrm_ess_self_change_execution_receipts WHERE request_id='.$request_id.' FOR UPDATE');
    if (!$receipts || db_num_rows($receipts)!==0) { cancel_transaction(); return false; }
    $from=(string)$request['request_status']; $version=(int)$request['row_version'];
    $ok=db_query('UPDATE '.TB_PREF."hrm_ess_self_change_requests SET request_status='rejected',row_version=row_version+1 WHERE request_id=".$request_id
        .' AND request_status='.db_escape($from).' AND row_version='.$version,'could not reject ESS self change request')!==false;
    if (!$ok || (function_exists('db_num_affected_rows')&&db_num_affected_rows()!==1)
        || !hrm_ess_self_change_terminal_audit($request,'hrm.ess.self_change.rejected',$from,'rejected',$reason_code,$actor)) {
        cancel_transaction(); return false;
    }
    commit_transaction(); return true;
}

/** Current-session owner may withdraw a draft or approved-but-not-applied request. */
function cancel_hrm_ess_self_profile_change($request_id, $reason_code)
{
    $actor=hrm_ess_self_change_actor_id(); $request_id=(int)$request_id; $reason_code=(string)$reason_code;
    if ($actor<=0 || $request_id<=0 || !hrm_ess_self_change_user_can_request()
        || !hrm_ess_self_change_terminal_reason_is_valid($reason_code)) return false;
    begin_transaction();
    $request=get_hrm_ess_self_change_request($request_id,true);
    if (!is_array($request) || (int)$request['user_id']!==$actor || (int)$request['requested_by']!==$actor
        || !hrm_ess_self_change_envelope_matches($request)) { cancel_transaction(); return false; }
    $scope=get_hrm_ess_current_self_scope();
    if (!is_array($scope) || (int)$scope['user_id']!==$actor || (int)$scope['worker_id']!==(int)$request['worker_id']) {
        cancel_transaction(); return false;
    }
    if ((string)$request['request_status']==='cancelled') { commit_transaction(); return true; }
    if (!in_array((string)$request['request_status'],array('draft','approved'),true)) { cancel_transaction(); return false; }
    $receipts=db_query('SELECT receipt_id FROM '.TB_PREF.'hrm_ess_self_change_execution_receipts WHERE request_id='.$request_id.' FOR UPDATE');
    if (!$receipts || db_num_rows($receipts)!==0) { cancel_transaction(); return false; }
    $from=(string)$request['request_status']; $version=(int)$request['row_version'];
    $ok=db_query('UPDATE '.TB_PREF."hrm_ess_self_change_requests SET request_status='cancelled',row_version=row_version+1 WHERE request_id=".$request_id
        .' AND request_status='.db_escape($from).' AND row_version='.$version,'could not cancel ESS self change request')!==false;
    if (!$ok || (function_exists('db_num_affected_rows')&&db_num_affected_rows()!==1)
        || !hrm_ess_self_change_terminal_audit($request,'hrm.ess.self_change.cancelled',$from,'cancelled',$reason_code,$actor)) {
        cancel_transaction(); return false;
    }
    commit_transaction(); return true;
}

/** Authorized internal sweep terminally expires unexecuted draft/approved requests. */
function expire_hrm_ess_self_profile_changes($limit = 50)
{
    $actor=hrm_ess_self_change_actor_id(); $limit=(int)$limit;
    if ($actor<=0 || $limit<1 || $limit>100 || !hrm_ess_self_change_user_can_execute()) return false;
    begin_transaction();
    $result=db_query('SELECT r.* FROM '.TB_PREF."hrm_ess_self_change_requests r WHERE r.request_status IN ('draft','approved') "
        .'AND r.expires_at<=UTC_TIMESTAMP() ORDER BY r.request_id ASC LIMIT '.$limit.' FOR UPDATE',
        'could not lock expired ESS self change requests');
    if (!$result) { cancel_transaction(); return false; }
    $count=0;
    while ($request=db_fetch_assoc($result)) {
        $receipt=db_query('SELECT receipt_id FROM '.TB_PREF.'hrm_ess_self_change_execution_receipts WHERE request_id='.(int)$request['request_id'].' LIMIT 1 FOR UPDATE',
            'could not verify ESS self change execution receipt absence');
        if (!$receipt || db_num_rows($receipt)!==0 || !hrm_ess_self_change_envelope_matches($request)) { cancel_transaction(); return false; }
        $from=(string)$request['request_status']; $version=(int)$request['row_version']; $id=(int)$request['request_id'];
        $ok=db_query('UPDATE '.TB_PREF."hrm_ess_self_change_requests SET request_status='expired',row_version=row_version+1 WHERE request_id=".$id
            .' AND request_status='.db_escape($from).' AND row_version='.$version,'could not expire ESS self change request')!==false;
        if (!$ok || (function_exists('db_num_affected_rows')&&db_num_affected_rows()!==1)
            || !hrm_ess_self_change_terminal_audit($request,'hrm.ess.self_change.expired',$from,'expired','request_expired',$actor)) {
            cancel_transaction(); return false;
        }
        $count++;
    }
    commit_transaction(); return $count;
}
