<?php
/** PAY-REC-001 privacy-aware export source/readiness guards. No schema writer authority. */
include_once(dirname(__FILE__).'/pay_rec_001_close_state_db.inc');
include_once($path_to_root.'/includes/pay_rec_001_evidence_export_contract.inc');
function pay_rec_001_evidence_export_contract_required_for_database_version($version){return version_compare((string)$version,'1.0.1058','>=');}
function pay_rec_001_evidence_export_runtime_required_for_database_version($version){return version_compare((string)$version,'1.0.1059','>=');}
function pay_rec_001_completion_readiness_required_for_database_version($version){return version_compare((string)$version,'1.0.1060','>=');}
function pay_rec_001_require_evidence_export_contract_source($patch)
{
    $c=pay_rec_001_evidence_export_contract();
    if(($c['export_mode']??'')!=='PRIVACY_MINIMIZED_HASH_BOUND_RECONCILIATION_EVIDENCE'||empty($c['company_scope_required'])||empty($c['aggregate_only'])||empty($c['hash_bound'])||empty($c['read_only'])||($c['view_permission']??'')!==PAY_REC_001_VIEW_PERMISSION)return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-REC-001 evidence-export contract incomplete.');
    foreach(array('actor_identifiers_exported','employee_identifiers_exported','bank_details_exported','filing_payload_exported','taxpayer_values_exported','formula_or_salary_detail_exported','raw_evidence_payload_exported','filesystem_write_allowed','network_authority_allowed','production_close_allowed','payroll_state_mutation_allowed','gl_posting_authority_allowed','payment_execution_authority_allowed','filing_submission_authority_allowed','payroll_calculation_authority_allowed','historical_payroll_accounting_filing_mutation_allowed','automatic_role_grant_allowed') as $k)if(!array_key_exists($k,$c)||$c[$k]!==false)return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-REC-001 evidence-export authority ceiling mismatch: '.$k);
    return true;
}
function pay_rec_001_require_evidence_export_runtime_source($patch)
{
    global $path_to_root;$raw=@file_get_contents($path_to_root.'/hrm/includes/db/pay_rec_001_evidence_export_runtime_db.inc');if(!is_string($raw))return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-REC-001 evidence-export runtime source missing.');
    foreach(array('pay_rec_001_build_privacy_evidence_export','PAY_REC_001_VIEW_PERMISSION','pay_rec_001_runtime_scope_company','pay_rec_001_build_privacy_export_from_rows','hrm_pay_rec_001_profiles','hrm_pay_rec_001_evidence_sets','hrm_pay_rec_001_checklists','hrm_pay_rec_001_reviews','hrm_pay_rec_001_approvals','hrm_pay_rec_001_state_events','company_id','reconciliation_profile_id') as $token)if(strpos($raw,$token)===false)return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-REC-001 evidence-export runtime token missing: '.$token);
    foreach(array('INSERT INTO','UPDATE ','DELETE FROM','REPLACE INTO','file_put_contents(','fwrite(','curl_','fsockopen(','stream_socket_client(','openssl_sign(','add_gl_trans(','write_bank_transaction(','calculate_employee_payslip(','pay_fil_001_submit') as $token)if(stripos($raw,$token)!==false)return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-REC-001 evidence-export runtime exceeds read-only authority: '.$token);
    foreach(array('prepared_by','recomputed_by','reviewed_by','approved_by','acted_by','employee_id','bank_account','account_number','taxpayer','payload_xml','formula_text','salary_amount') as $token)if(stripos($raw,$token)!==false)return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-REC-001 evidence-export runtime reads forbidden privacy field/token: '.$token);
    return true;
}
function pay_rec_001_require_completion_readiness_source($patch)
{
    if(!pay_rec_001_require_evidence_export_contract_source($patch)||!pay_rec_001_require_close_state_contract_source($patch)||!pay_rec_001_require_custody_schema($patch)||!pay_rec_001_require_review_schema($patch)||!pay_rec_001_require_close_state_schema($patch)||!pay_rec_001_require_evidence_runtime_source($patch)||!pay_rec_001_require_close_state_runtime_source($patch)||!pay_rec_001_require_evidence_export_runtime_source($patch))return false;
    return true;
}
?>
