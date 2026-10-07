<?php
/** PAY-REC-001 privacy-aware reconciliation evidence export contract. Pure helpers only. */
include_once(dirname(__FILE__).'/pay_rec_001_close_state_contract.inc');
if (!defined('PAY_REC_001_EVIDENCE_EXPORT_CONTRACT_VERSION')) define('PAY_REC_001_EVIDENCE_EXPORT_CONTRACT_VERSION','1.0');

function pay_rec_001_evidence_export_contract()
{
    return array(
        'contract_version'=>PAY_REC_001_EVIDENCE_EXPORT_CONTRACT_VERSION,
        'export_mode'=>'PRIVACY_MINIMIZED_HASH_BOUND_RECONCILIATION_EVIDENCE',
        'view_permission'=>PAY_REC_001_VIEW_PERMISSION,
        'company_scope_required'=>true,
        'aggregate_only'=>true,
        'hash_bound'=>true,
        'read_only'=>true,
        'actor_identifiers_exported'=>false,
        'employee_identifiers_exported'=>false,
        'bank_details_exported'=>false,
        'filing_payload_exported'=>false,
        'taxpayer_values_exported'=>false,
        'formula_or_salary_detail_exported'=>false,
        'raw_evidence_payload_exported'=>false,
        'filesystem_write_allowed'=>false,
        'network_authority_allowed'=>false,
        'production_close_allowed'=>false,
        'payroll_state_mutation_allowed'=>false,
        'gl_posting_authority_allowed'=>false,
        'payment_execution_authority_allowed'=>false,
        'filing_submission_authority_allowed'=>false,
        'payroll_calculation_authority_allowed'=>false,
        'historical_payroll_accounting_filing_mutation_allowed'=>false,
        'automatic_role_grant_allowed'=>false
    );
}
function pay_rec_001_evidence_export_contract_sha256(){return pay_rec_001_hash(pay_rec_001_evidence_export_contract());}
function pay_rec_001_export_fail(&$error,$code){$error=(string)$code;return false;}
function pay_rec_001_export_exact_keys(array $row,array $keys)
{
    $actual=array_keys($row);sort($actual,SORT_STRING);$expected=$keys;sort($expected,SORT_STRING);return $actual===$expected;
}
function pay_rec_001_export_valid_timestamp($value){return is_string($value)&&preg_match('/^\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}$/',$value)===1;}
/** Build the only exportable PAY-REC envelope from already-scoped immutable custody rows. */
function pay_rec_001_build_privacy_export_from_rows(array $profile,array $sets,array $checklist,array $review,array $approval,array $events,&$error=null)
{
    $error=null;
    $profile_keys=array('reconciliation_profile_id','company_id','legal_entity_id','payroll_period_id','pay_core_run_id','filing_obligation_id','currency_code','policy_version','tolerance_sha256','rounding_sha256','filing_completion_policy_sha256','payroll_scope_sha256','profile_sha256','prepared_at');
    if(!pay_rec_001_export_exact_keys($profile,$profile_keys))return pay_rec_001_export_fail($error,'privacy_export_profile_shape_invalid');
    foreach(array('reconciliation_profile_id','legal_entity_id','payroll_period_id','pay_core_run_id','filing_obligation_id') as $k)if(pay_rec_001_uint($profile[$k])===false||(int)$profile[$k]<=0)return pay_rec_001_export_fail($error,'privacy_export_profile_scope_invalid');
    if(pay_rec_001_uint($profile['company_id'])===false||$profile['currency_code']!=='VND'||pay_rec_001_token($profile['policy_version'],32)===false||!pay_rec_001_export_valid_timestamp($profile['prepared_at']))return pay_rec_001_export_fail($error,'privacy_export_profile_invalid');
    foreach(array('tolerance_sha256','rounding_sha256','filing_completion_policy_sha256','payroll_scope_sha256','profile_sha256') as $k)if(!pay_rec_001_sha($profile[$k]))return pay_rec_001_export_fail($error,'privacy_export_profile_hash_invalid');

    $set_keys=array('reconciliation_evidence_set_id','evidence_kind','membership_sha256','evidence_sha256','item_count','amount_minor','status_token','current_evidence','source_contract_sha256','recomputed_at');
    $by_kind=array();
    foreach($sets as $set){
        if(!is_array($set)||!pay_rec_001_export_exact_keys($set,$set_keys))return pay_rec_001_export_fail($error,'privacy_export_evidence_shape_invalid');
        if(!in_array($set['evidence_kind'],array('payroll','gl','payment','filing'),true)||isset($by_kind[$set['evidence_kind']]))return pay_rec_001_export_fail($error,'privacy_export_evidence_kind_invalid');
        if(pay_rec_001_uint($set['reconciliation_evidence_set_id'])===false||(int)$set['reconciliation_evidence_set_id']<=0||pay_rec_001_uint($set['item_count'])===false||(int)$set['item_count']<0||!is_numeric($set['amount_minor'])||$set['status_token']!=='matched'||(int)$set['current_evidence']!==1||!pay_rec_001_export_valid_timestamp($set['recomputed_at']))return pay_rec_001_export_fail($error,'privacy_export_evidence_invalid');
        foreach(array('membership_sha256','evidence_sha256','source_contract_sha256') as $k)if(!pay_rec_001_sha($set[$k]))return pay_rec_001_export_fail($error,'privacy_export_evidence_hash_invalid');
        $by_kind[$set['evidence_kind']]=$set;
    }
    foreach(array('payroll','gl','payment','filing') as $kind)if(!isset($by_kind[$kind]))return pay_rec_001_export_fail($error,'privacy_export_evidence_set_missing');

    $check_keys=array('close_checklist_id','payroll_evidence_set_id','gl_evidence_set_id','payment_evidence_set_id','filing_evidence_set_id','filing_completion_receipt_sha256','readiness_sha256','checklist_sha256','prepared_at');
    if(!pay_rec_001_export_exact_keys($checklist,$check_keys)||!pay_rec_001_export_valid_timestamp($checklist['prepared_at']))return pay_rec_001_export_fail($error,'privacy_export_checklist_invalid');
    $kind_to_id=array('payroll'=>'payroll_evidence_set_id','gl'=>'gl_evidence_set_id','payment'=>'payment_evidence_set_id','filing'=>'filing_evidence_set_id');
    if(pay_rec_001_uint($checklist['close_checklist_id'])===false||(int)$checklist['close_checklist_id']<=0)return pay_rec_001_export_fail($error,'privacy_export_checklist_invalid');
    foreach($kind_to_id as $kind=>$id_key)if((string)$checklist[$id_key] !== (string)$by_kind[$kind]['reconciliation_evidence_set_id'])return pay_rec_001_export_fail($error,'privacy_export_checklist_membership_mismatch');
    foreach(array('filing_completion_receipt_sha256','readiness_sha256','checklist_sha256') as $k)if(!pay_rec_001_sha($checklist[$k]))return pay_rec_001_export_fail($error,'privacy_export_checklist_hash_invalid');

    $review_keys=array('reconciliation_review_id','close_checklist_id','checklist_sha256','readiness_sha256','decision','review_evidence_sha256','review_sha256','reviewed_at');
    $approval_keys=array('reconciliation_approval_id','reconciliation_review_id','close_checklist_id','review_sha256','decision','assurance_evidence_sha256','approval_sha256','approved_at');
    if(!pay_rec_001_export_exact_keys($review,$review_keys)||!pay_rec_001_export_exact_keys($approval,$approval_keys))return pay_rec_001_export_fail($error,'privacy_export_review_shape_invalid');
    foreach(array($review['reconciliation_review_id'],$approval['reconciliation_approval_id']) as $id)if(pay_rec_001_uint($id)===false||(int)$id<=0)return pay_rec_001_export_fail($error,'privacy_export_review_identity_invalid');
    if((string)$review['close_checklist_id']!==(string)$checklist['close_checklist_id']||(string)$approval['close_checklist_id']!==(string)$checklist['close_checklist_id']||(string)$approval['reconciliation_review_id']!==(string)$review['reconciliation_review_id']||$review['decision']!=='approved'||$approval['decision']!=='approved'||!pay_rec_001_export_valid_timestamp($review['reviewed_at'])||!pay_rec_001_export_valid_timestamp($approval['approved_at']))return pay_rec_001_export_fail($error,'privacy_export_review_approval_invalid');
    foreach(array('checklist_sha256','readiness_sha256','review_evidence_sha256','review_sha256') as $k)if(!pay_rec_001_sha($review[$k]))return pay_rec_001_export_fail($error,'privacy_export_review_hash_invalid');
    foreach(array('review_sha256','assurance_evidence_sha256','approval_sha256') as $k)if(!pay_rec_001_sha($approval[$k]))return pay_rec_001_export_fail($error,'privacy_export_approval_hash_invalid');
    if(!hash_equals((string)$review['checklist_sha256'],(string)$checklist['checklist_sha256'])||!hash_equals((string)$review['readiness_sha256'],(string)$checklist['readiness_sha256'])||!hash_equals((string)$approval['review_sha256'],(string)$review['review_sha256']))return pay_rec_001_export_fail($error,'privacy_export_review_binding_mismatch');

    if(!$events)return pay_rec_001_export_fail($error,'privacy_export_state_event_missing');
    $event_keys=array('event_sequence','prior_event_sha256','event_type','resulting_state','reason_code','checklist_sha256','review_sha256','approval_sha256','transition_evidence_sha256','event_sha256','acted_at');
    $prior=str_repeat('0',64);$prior_state=null;$seq=0;$safe_events=array();
    foreach($events as $event){
        if(!is_array($event)||!pay_rec_001_export_exact_keys($event,$event_keys))return pay_rec_001_export_fail($error,'privacy_export_state_event_shape_invalid');
        $seq++;if((int)$event['event_sequence']!==$seq||!hash_equals($prior,(string)$event['prior_event_sha256'])||!pay_rec_001_close_state_transition_allowed($prior_state,$event['event_type'])||pay_rec_001_close_state_for_event($event['event_type'])!==$event['resulting_state']||!pay_rec_001_export_valid_timestamp($event['acted_at']))return pay_rec_001_export_fail($error,'privacy_export_state_chain_invalid');
        $reason=pay_rec_001_token($event['reason_code'],48);if($reason===false||($event['event_type']==='development_close'?$reason!=='approved_reconciliation':!in_array($reason,pay_rec_001_close_state_contract()['recovery_reason_codes'],true)))return pay_rec_001_export_fail($error,'privacy_export_state_reason_invalid');
        foreach(array('prior_event_sha256','checklist_sha256','review_sha256','approval_sha256','transition_evidence_sha256','event_sha256') as $k)if(!pay_rec_001_sha($event[$k]))return pay_rec_001_export_fail($error,'privacy_export_state_hash_invalid');
        $prior=(string)$event['event_sha256'];$prior_state=$event['resulting_state'];$safe_events[]=$event;
    }
    $latest=end($safe_events);
    if(!hash_equals((string)$latest['checklist_sha256'],(string)$checklist['checklist_sha256'])||!hash_equals((string)$latest['review_sha256'],(string)$review['review_sha256'])||!hash_equals((string)$latest['approval_sha256'],(string)$approval['approval_sha256']))return pay_rec_001_export_fail($error,'privacy_export_latest_binding_mismatch');
    $body=array(
        'export_contract_sha256'=>pay_rec_001_evidence_export_contract_sha256(),
        'classification'=>'PRIVACY_MINIMIZED_RECONCILIATION_EVIDENCE',
        'profile'=>$profile,
        'evidence_sets'=>array($by_kind['payroll'],$by_kind['gl'],$by_kind['payment'],$by_kind['filing']),
        'checklist'=>$checklist,
        'review'=>$review,
        'approval'=>$approval,
        'state_events'=>$safe_events,
        'current_state'=>$latest['resulting_state'],
        'redaction_manifest'=>array(
            'actor_identifiers'=>'omitted','employee_identifiers'=>'not_read','bank_details'=>'not_read','filing_payload'=>'not_read',
            'taxpayer_values'=>'not_read','formula_salary_detail'=>'not_read','raw_evidence_payload'=>'not_read','secrets_credentials'=>'not_read'
        ),
        'production_close_authorized'=>false,'historical_mutation_authorized'=>false,'network_authorized'=>false
    );
    $body['export_sha256']=pay_rec_001_hash($body);return $body;
}
?>
