<?php
/** PAY-REC-001 append-only development close-state, concurrency and recovery contract. Pure helpers only. */
include_once(dirname(__FILE__).'/pay_rec_001_evidence_review_contract.inc');
if (!defined('PAY_REC_001_CLOSE_STATE_CONTRACT_VERSION')) define('PAY_REC_001_CLOSE_STATE_CONTRACT_VERSION','1.0');

function pay_rec_001_close_state_contract()
{
    return array(
        'contract_version'=>PAY_REC_001_CLOSE_STATE_CONTRACT_VERSION,
        'runtime_status'=>'APPEND_ONLY_DEVELOPMENT_CLOSE_RECOVERY_ENABLED_PRODUCTION_CLOSE_DISABLED',
        'currency_scope'=>array('VND'),
        'event_types'=>array('development_close','reopen_for_correction','mark_reversal_required'),
        'states'=>array('DEVELOPMENT_CLOSED','REOPENED_FOR_CORRECTION','REVERSAL_REQUIRED'),
        'recovery_reason_codes'=>array('source_correction','payment_return','filing_correction','payroll_reversal','manual_recovery'),
        'close_permission'=>PAY_REC_001_CLOSE_PERMISSION,
        'recover_permission'=>PAY_REC_001_RECOVER_PERMISSION,
        'append_only_state_event_required'=>true,
        'immutable_state_event_required'=>true,
        'serialize_per_pay_run_obligation_required'=>true,
        'fresh_revalidation_before_close_required'=>true,
        'approved_review_approval_required'=>true,
        'closer_approver_separation_required'=>true,
        'exact_idempotent_replay_required'=>true,
        'recovery_requires_current_closed_event'=>true,
        'reclose_requires_new_approval'=>true,
        'development_close_evidence_enabled'=>true,
        'production_close_allowed'=>false,
        'payroll_state_mutation_allowed'=>false,
        'gl_posting_authority_allowed'=>false,
        'payment_execution_authority_allowed'=>false,
        'filing_submission_authority_allowed'=>false,
        'network_authority_allowed'=>false,
        'payroll_calculation_authority_allowed'=>false,
        'historical_payroll_accounting_filing_mutation_allowed'=>false,
        'automatic_role_grant_allowed'=>false
    );
}
function pay_rec_001_close_state_contract_sha256(){return pay_rec_001_hash(pay_rec_001_close_state_contract());}
function pay_rec_001_close_state_for_event($event_type)
{
    $map=array('development_close'=>'DEVELOPMENT_CLOSED','reopen_for_correction'=>'REOPENED_FOR_CORRECTION','mark_reversal_required'=>'REVERSAL_REQUIRED');
    return isset($map[$event_type])?$map[$event_type]:false;
}
function pay_rec_001_close_state_transition_allowed($prior_state,$event_type)
{
    if($event_type==='development_close')return $prior_state===null||$prior_state===''||$prior_state==='REOPENED_FOR_CORRECTION'||$prior_state==='REVERSAL_REQUIRED';
    if($event_type==='reopen_for_correction'||$event_type==='mark_reversal_required')return $prior_state==='DEVELOPMENT_CLOSED';
    return false;
}
/** Canonical append-only state-event body. Timestamp is intentionally outside the signed descriptor. */
function pay_rec_001_close_state_event_descriptor(array $scope,$event_sequence,$prior_event_sha256,$event_type,$reason_code,array $bindings,$actor_id,&$error=null)
{
    $error=null;
    foreach(array('company_id','legal_entity_id','payroll_period_id','pay_core_run_id','filing_obligation_id') as $k)if(!array_key_exists($k,$scope))return pay_rec_001_fail($error,'reconciliation_state_scope_incomplete');
    foreach(array('reconciliation_profile_id','close_checklist_id','reconciliation_review_id','reconciliation_approval_id','checklist_sha256','review_sha256','approval_sha256','transition_evidence_sha256') as $k)if(!array_key_exists($k,$bindings))return pay_rec_001_fail($error,'reconciliation_state_binding_incomplete');
    $ids=array();foreach(array('company_id','legal_entity_id','payroll_period_id','pay_core_run_id','filing_obligation_id') as $k){$ids[$k]=pay_rec_001_uint($scope[$k]);if($ids[$k]===false||($k!=='company_id'&&$ids[$k]<=0))return pay_rec_001_fail($error,'reconciliation_state_scope_invalid');}
    $seq=pay_rec_001_uint($event_sequence);$actor=pay_rec_001_uint($actor_id);$state=pay_rec_001_close_state_for_event($event_type);$reason=pay_rec_001_token($reason_code,48);
    if($seq===false||$seq<=0||$actor===false||$actor<=0||$state===false||$reason===false||!pay_rec_001_sha($prior_event_sha256))return pay_rec_001_fail($error,'reconciliation_state_event_invalid');
    if($seq===1&&!hash_equals(str_repeat('0',64),(string)$prior_event_sha256))return pay_rec_001_fail($error,'reconciliation_state_genesis_invalid');
    foreach(array('reconciliation_profile_id','close_checklist_id','reconciliation_review_id','reconciliation_approval_id') as $k){$bindings[$k]=pay_rec_001_uint($bindings[$k]);if($bindings[$k]===false||$bindings[$k]<=0)return pay_rec_001_fail($error,'reconciliation_state_binding_invalid');}
    foreach(array('checklist_sha256','review_sha256','approval_sha256','transition_evidence_sha256') as $k)if(!pay_rec_001_sha($bindings[$k]))return pay_rec_001_fail($error,'reconciliation_state_binding_invalid');
    if($event_type==='development_close'&&$reason!=='approved_reconciliation')return pay_rec_001_fail($error,'reconciliation_close_reason_invalid');
    if($event_type!=='development_close'&&!in_array($reason,pay_rec_001_close_state_contract()['recovery_reason_codes'],true))return pay_rec_001_fail($error,'reconciliation_recovery_reason_invalid');
    $body=$ids+array(
        'reconciliation_profile_id'=>$bindings['reconciliation_profile_id'],'close_checklist_id'=>$bindings['close_checklist_id'],'reconciliation_review_id'=>$bindings['reconciliation_review_id'],'reconciliation_approval_id'=>$bindings['reconciliation_approval_id'],
        'event_sequence'=>$seq,'prior_event_sha256'=>(string)$prior_event_sha256,'event_type'=>$event_type,'resulting_state'=>$state,'reason_code'=>$reason,
        'checklist_sha256'=>(string)$bindings['checklist_sha256'],'review_sha256'=>(string)$bindings['review_sha256'],'approval_sha256'=>(string)$bindings['approval_sha256'],'transition_evidence_sha256'=>(string)$bindings['transition_evidence_sha256'],'acted_by'=>$actor
    );
    $body['event_sha256']=pay_rec_001_hash($body);return $body;
}
?>
