<?php
/** PAY-REC-001 source-only architecture readiness checks. No schema or runtime writer authority. */
include_once($path_to_root.'/includes/pay_rec_001_contract.inc');
include_once($path_to_root.'/hrm/includes/db/pay_fil_001_tt80_generated_validation_db.inc');
function pay_rec_001_filing_lineage_contract_required_for_database_version($version){return version_compare((string)$version,'1.0.1046','>=');}
function pay_rec_001_filing_completion_policy_required_for_database_version($version){return version_compare((string)$version,'1.0.1047','>=');}
function pay_rec_001_architecture_contract_required_for_database_version($version){return version_compare((string)$version,'1.0.1048','>=');}
function pay_rec_001_fail_patch($patch,$message){return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,$message);}
function pay_rec_001_require_filing_lineage_contract_source($patch)
{
    global $path_to_root;$c=pay_rec_001_filing_monetary_lineage_contract();
    if($c['monetary_lineage_mode']!=='EXACT_PAY_CORE_RESULT_LINE_TO_TT80_PERSON_FACT'||$c['currency_code']!=='VND'||$c['automatic_semantic_inference_allowed']!==false||$c['runtime_close_authority_allowed']!==false)return pay_rec_001_fail_patch($patch,'PAY-REC-001 filing monetary-lineage authority invalid.');
    $raw=@file_get_contents($path_to_root.'/includes/pay_rec_001_filing_monetary_lineage_contract.inc');foreach(array('EXACT_PAY_CORE_RESULT_LINE_TO_TT80_PERSON_FACT','classification_review_required','predecessor_result_binding_required_for_correction','pay_rec_001_verify_filing_monetary_lineage','pay_rec_001_validate_tt80_person_fact','pay_rec_001_vnd_amount_minor','hrm_pay_core_004_verify_result_bundle') as $token)if(!is_string($raw)||strpos($raw,$token)===false)return pay_rec_001_fail_patch($patch,'PAY-REC-001 filing monetary-lineage contract incomplete: '.$token.'.');
    foreach(array('db_query(','INSERT INTO','UPDATE ','DELETE FROM','curl_','fsockopen(','stream_socket_client(','file_put_contents(','openssl_sign(') as $token)if(stripos($raw,$token)!==false)return pay_rec_001_fail_patch($patch,'PAY-REC-001 filing monetary-lineage contract acquired forbidden authority: '.$token.'.');return true;
}
function pay_rec_001_require_filing_completion_policy_source($patch)
{
    global $path_to_root;if(!pay_rec_001_require_filing_lineage_contract_source($patch))return false;$p=pay_rec_001_filing_completion_policy();
    if($p['development_terminal_state']!=='LOCAL_VALIDATION_ACCEPTED_FOR_RECONCILIATION'||$p['production_statutory_acceptance_claimed']!==false||$p['production_close_allowed']!==false||$p['portal_submission_authority_allowed']!==false)return pay_rec_001_fail_patch($patch,'PAY-REC-001 filing completion policy authority invalid.');
    $raw=@file_get_contents($path_to_root.'/includes/pay_rec_001_filing_completion_policy_contract.inc');foreach(array('LOCAL_DEVELOPMENT_RECONCILIATION','LOCAL_VALIDATION_ACCEPTED_FOR_RECONCILIATION','production_close_allowed','development_reconciliation_only_not_statutory_acceptance') as $token)if(!is_string($raw)||strpos($raw,$token)===false)return pay_rec_001_fail_patch($patch,'PAY-REC-001 filing completion policy incomplete: '.$token.'.');
    foreach(array('db_query(','INSERT INTO','UPDATE ','DELETE FROM','curl_','fsockopen(','stream_socket_client(','file_put_contents(','openssl_sign(') as $token)if(stripos($raw,$token)!==false)return pay_rec_001_fail_patch($patch,'PAY-REC-001 filing completion policy acquired forbidden authority: '.$token.'.');return true;
}
function pay_rec_001_require_architecture_contract_source($patch)
{
    global $path_to_root;if(!pay_rec_001_require_filing_completion_policy_source($patch))return false;$c=pay_rec_001_authority_contract();
    if($c['evidence_sets']!==array('payroll','gl','payment','filing')||$c['reviewer_approver_separation_required']!==true||$c['implicit_tolerance_allowed']!==false||$c['schema_writer_enabled']!==false||$c['production_close_runtime_enabled']!==false)return pay_rec_001_fail_patch($patch,'PAY-REC-001 reconciliation authority invalid.');
    $raw=@file_get_contents($path_to_root.'/includes/pay_rec_001_contract.inc');foreach(array('partial_payment_close_denied','filing_correction_pending_close_denied','stale_evidence_close_denied','development_close_state_mutation_enabled','pay_rec_001_validate_filing_completion_receipt','pay_rec_001_evaluate_development_close_readiness') as $token)if(!is_string($raw)||strpos($raw,$token)===false)return pay_rec_001_fail_patch($patch,'PAY-REC-001 architecture contract incomplete: '.$token.'.');
    foreach(array('db_query(','INSERT INTO','UPDATE ','DELETE FROM','curl_','fsockopen(','stream_socket_client(','file_put_contents(','openssl_sign(') as $token)if(stripos($raw,$token)!==false)return pay_rec_001_fail_patch($patch,'PAY-REC-001 architecture contract acquired forbidden authority: '.$token.'.');return true;
}
?>
