<?php
/** PAY-REC-001 read-only privacy-aware evidence export over accepted immutable reconciliation custody. */
include_once(dirname(__FILE__).'/pay_rec_001_evidence_export_db.inc');
include_once(dirname(__FILE__).'/pay_rec_001_evidence_runtime_db.inc');

function pay_rec_001_evidence_export_runtime_ready(&$error){$v=pay_rec_001_runtime_database_version();return is_string($v)&&version_compare($v,'1.0.1059','>=')?true:pay_rec_001_runtime_fail($error,'pay_rec_001_evidence_export_runtime_not_activated');}
/** Return a hash-bound aggregate-only export. No files, network calls or mutation occur here. */
function pay_rec_001_build_privacy_evidence_export($reconciliation_profile_id,&$error=null)
{
    $error=null;if(!pay_rec_001_evidence_export_runtime_ready($error)||!pay_rec_001_runtime_require_access(PAY_REC_001_VIEW_PERMISSION,$error))return false;
    $profile_id=pay_rec_001_uint($reconciliation_profile_id);if($profile_id===false||$profile_id<=0)return pay_rec_001_runtime_fail($error,'privacy_export_profile_id_invalid');
    $profile=pay_rec_001_runtime_one('SELECT reconciliation_profile_id,company_id,legal_entity_id,payroll_period_id,pay_core_run_id,filing_obligation_id,currency_code,policy_version,tolerance_sha256,rounding_sha256,filing_completion_policy_sha256,payroll_scope_sha256,profile_sha256,prepared_at FROM '.TB_PREF.'hrm_pay_rec_001_profiles WHERE reconciliation_profile_id='.(int)$profile_id.' AND company_id='.(int)pay_rec_001_runtime_company().' LIMIT 2');
    if(!$profile||!pay_rec_001_runtime_scope_company($profile['company_id'],$error))return $profile?false:pay_rec_001_runtime_fail($error,'privacy_export_profile_not_found');
    $latest=pay_rec_001_runtime_one('SELECT close_checklist_id FROM '.TB_PREF.'hrm_pay_rec_001_state_events WHERE reconciliation_profile_id='.(int)$profile_id.' AND company_id='.(int)$profile['company_id'].' ORDER BY event_sequence DESC LIMIT 1');
    if(!$latest)return pay_rec_001_runtime_fail($error,'privacy_export_state_event_missing');
    $checklist=pay_rec_001_runtime_one('SELECT close_checklist_id,payroll_evidence_set_id,gl_evidence_set_id,payment_evidence_set_id,filing_evidence_set_id,filing_completion_receipt_sha256,readiness_sha256,checklist_sha256,prepared_at FROM '.TB_PREF.'hrm_pay_rec_001_checklists WHERE close_checklist_id='.(int)$latest['close_checklist_id'].' AND reconciliation_profile_id='.(int)$profile_id.' LIMIT 2');
    if(!$checklist)return pay_rec_001_runtime_fail($error,'privacy_export_checklist_not_found');
    $ids=array((int)$checklist['payroll_evidence_set_id'],(int)$checklist['gl_evidence_set_id'],(int)$checklist['payment_evidence_set_id'],(int)$checklist['filing_evidence_set_id']);
    $sets=pay_rec_001_runtime_rows('SELECT reconciliation_evidence_set_id,evidence_kind,membership_sha256,evidence_sha256,item_count,amount_minor,status_token,current_evidence,source_contract_sha256,recomputed_at FROM '.TB_PREF.'hrm_pay_rec_001_evidence_sets WHERE reconciliation_profile_id='.(int)$profile_id.' AND reconciliation_evidence_set_id IN ('.implode(',',$ids).') ORDER BY FIELD(evidence_kind,\'payroll\',\'gl\',\'payment\',\'filing\')');
    if(!is_array($sets)||count($sets)!==4)return pay_rec_001_runtime_fail($error,'privacy_export_evidence_sets_not_found');
    $review=pay_rec_001_runtime_one('SELECT reconciliation_review_id,close_checklist_id,checklist_sha256,readiness_sha256,decision,review_evidence_sha256,review_sha256,reviewed_at FROM '.TB_PREF.'hrm_pay_rec_001_reviews WHERE close_checklist_id='.(int)$checklist['close_checklist_id'].' LIMIT 2');
    if(!$review)return pay_rec_001_runtime_fail($error,'privacy_export_review_not_found');
    $approval=pay_rec_001_runtime_one('SELECT reconciliation_approval_id,reconciliation_review_id,close_checklist_id,review_sha256,decision,assurance_evidence_sha256,approval_sha256,approved_at FROM '.TB_PREF.'hrm_pay_rec_001_approvals WHERE reconciliation_review_id='.(int)$review['reconciliation_review_id'].' AND close_checklist_id='.(int)$checklist['close_checklist_id'].' LIMIT 2');
    if(!$approval)return pay_rec_001_runtime_fail($error,'privacy_export_approval_not_found');
    $events=pay_rec_001_runtime_rows('SELECT event_sequence,prior_event_sha256,event_type,resulting_state,reason_code,checklist_sha256,review_sha256,approval_sha256,transition_evidence_sha256,event_sha256,acted_at FROM '.TB_PREF.'hrm_pay_rec_001_state_events WHERE reconciliation_profile_id='.(int)$profile_id.' AND company_id='.(int)$profile['company_id'].' AND pay_core_run_id='.(int)$profile['pay_core_run_id'].' AND filing_obligation_id='.(int)$profile['filing_obligation_id'].' ORDER BY event_sequence ASC');
    if(!is_array($events)||!count($events))return pay_rec_001_runtime_fail($error,'privacy_export_state_event_missing');
    $out=pay_rec_001_build_privacy_export_from_rows($profile,$sets,$checklist,$review,$approval,$events,$error);return $out===false?false:$out;
}
?>
