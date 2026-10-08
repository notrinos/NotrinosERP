<?php
/** PAY-CON-001 named-provider admission evidence custody/readiness guards. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
include_once(dirname(__FILE__).'/pay_con_001_readiness_db.inc');
include_once($path_to_root.'/includes/pay_con_001_admission_contract.inc');

function pay_con_001_admission_table(){return 'hrm_pay_con_001_provider_admission_reviews';}
function pay_con_001_admission_expected_columns(){return array('provider_admission_review_id','company_id','provider_code','jurisdiction_code','delivery_model_code','residency_region_code','vendor_due_diligence_sha256','legal_review_sha256','security_review_sha256','data_residency_review_sha256','delivery_model_disclosure_sha256','provider_contract_sha256','source_provenance_sha256','requested_by','reviewed_by','approved_by','decision_code','reviewed_at','approved_at','admission_review_sha256','created_at');}

function pay_con_001_admission_require_contract($patch)
{
    $c=pay_con_001_admission_contract();
    foreach(array('named_provider_required_for_admission','vendor_due_diligence_required','legal_review_required','security_review_required','data_residency_review_required','delivery_model_disclosure_required','provider_contract_evidence_required','source_provenance_required','independent_reviewer_required','independent_approver_required','review_hash_binding_required','approved_decision_required_before_profile_writer','mapping_approval_required_before_exchange') as $k)
        if(!isset($c[$k])||$c[$k]!==true)return pay_con_001_fail_patch($patch,'PAY-CON-001 admission prerequisite missing: '.$k.'.');
    foreach(array('provider_admission_review_writer_allowed','provider_profile_writer_allowed','mapping_version_writer_allowed','provider_adapter_runtime_allowed','external_http_route_allowed','connector_secret_custody_allowed','outbound_network_authority_allowed','inbound_network_authority_allowed','shadow_export_runtime_allowed','shadow_import_runtime_allowed','provider_payroll_execution_authority_allowed','payroll_result_authority_allowed','payroll_state_mutation_allowed','gl_posting_authority_allowed','payment_execution_authority_allowed','filing_submission_authority_allowed','approval_bypass_allowed','audit_bypass_allowed','cross_company_authority_allowed','historical_payroll_accounting_filing_mutation_allowed','production_provider_exchange_allowed') as $k)
        if(!array_key_exists($k,$c)||$c[$k]!==false)return pay_con_001_fail_patch($patch,'PAY-CON-001 admission authority ceiling violated: '.$k.'.');
    if(($c['admission_status']??'')!=='EVIDENCE_CUSTODY_READY_NO_PROVIDER_ADMITTED')return pay_con_001_fail_patch($patch,'PAY-CON-001 admission status must remain non-admitting.');
    return true;
}

/**
 * Verify the exact immutable denial statement used by an admission trigger.
 *
 * @param string $action_statement Trigger action from INFORMATION_SCHEMA.
 * @param string $operation Triggered SQL operation.
 * @return bool
 */
function pay_con_001_admission_trigger_statement_valid($action_statement,$operation)
{
    if(!in_array($operation,array('UPDATE','DELETE'),true))return false;
    $expected="SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CON-001 immutable ".pay_con_001_admission_table().' row cannot be '.strtolower($operation)."d'";
    $normalized=preg_replace('/\s+/',' ',trim((string)$action_statement));
    $actual=is_string($normalized)?preg_replace('/\s*=\s*/','=',$normalized):false;
    return is_string($actual)&&strtolower($actual)===strtolower($expected);
}

function pay_con_001_admission_trigger_valid($operation)
{
    if(!in_array($operation,array('UPDATE','DELETE'),true))return false;
    $prefix=hrm_migration_active_table_prefix();$physical=hrm_migration_physical_table_name(pay_con_001_admission_table());if($prefix===false||$physical===false)return false;
    $suffix=$operation==='UPDATE'?'u':'d';$name=$prefix.'pcon1_par_'.$suffix;
    $q=db_query("SELECT EVENT_MANIPULATION,ACTION_TIMING,EVENT_OBJECT_TABLE,ACTION_STATEMENT FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA=DATABASE() AND TRIGGER_NAME=".db_escape($name));$r=$q?db_fetch_assoc($q):false;
    return $r&&strtoupper((string)$r['EVENT_MANIPULATION'])===$operation&&strtoupper((string)$r['ACTION_TIMING'])==='BEFORE'&&(string)$r['EVENT_OBJECT_TABLE']===$physical&&pay_con_001_admission_trigger_statement_valid($r['ACTION_STATEMENT'],$operation);
}

function pay_con_001_admission_create_trigger($patch,$operation)
{
    if(pay_con_001_admission_trigger_valid($operation))return true;$prefix=hrm_migration_active_table_prefix();$physical=hrm_migration_physical_table_name(pay_con_001_admission_table());if($prefix===false||$physical===false)return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CON-001 admission trigger target invalid.');
    $suffix=$operation==='UPDATE'?'u':'d';$name=$prefix.'pcon1_par_'.$suffix;$message='PAY-CON-001 immutable '.pay_con_001_admission_table().' row cannot be '.strtolower($operation).'d';
    if(!db_query('CREATE TRIGGER `'.$name.'` BEFORE '.$operation.' ON `'.$physical.'` FOR EACH ROW SIGNAL SQLSTATE \'45000\' SET MESSAGE_TEXT='.db_escape($message)))return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CON-001 admission trigger creation failed: '.$name.'.');
    return pay_con_001_admission_trigger_valid($operation);
}

function pay_con_001_admission_require_schema($patch)
{
    $table=pay_con_001_admission_table();$cols=pay_con_001_table_columns($table);if($cols!==pay_con_001_admission_expected_columns()||!pay_con_001_table_innodb($table))return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CON-001 admission-review table projection mismatch.');
    foreach(array('UPDATE','DELETE') as $op)if(!pay_con_001_admission_trigger_valid($op))return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CON-001 admission immutable trigger mismatch: '.$op.'.');
    $physical=hrm_migration_physical_table_name($table);$q=$physical!==false?db_query('SELECT COUNT(*) row_count FROM `'.$physical.'`'):false;$r=$q?db_fetch_assoc($q):false;if(!$r||(int)$r['row_count']!==0)return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CON-001 admission custody must remain zero-seed before named-provider evidence is supplied.');
    return true;
}

function pay_con_001_admission_install_schema($patch)
{
    global $path_to_root;$raw=@file_get_contents($path_to_root.'/sql/upgrade_pay_con_001_provider_admission_schema.sql');if(!is_string($raw)||strpos($raw,'-- PAY-CON-001 GROUP provider_admission_review_custody')===false)return pay_con_001_fail_patch($patch,'PAY-CON-001 admission schema source missing.');
    $prefix=hrm_migration_active_table_prefix();if($prefix===false)return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CON-001 table prefix unavailable.');
    $stripped=preg_replace('/^--.*$/m','',$raw);$statements=preg_split('/;\s*(?:\r?\n|$)/',$stripped);foreach($statements as $statement){$statement=trim($statement);if($statement===''||strpos($statement,'CREATE TRIGGER')===0)continue;$statement=str_replace('`0_','`'.$prefix,$statement);if(!db_query($statement))return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CON-001 admission schema statement failed.');}
    foreach(array('UPDATE','DELETE') as $op)if(!pay_con_001_admission_create_trigger($patch,$op))return false;
    return pay_con_001_admission_require_schema($patch);
}
?>
