<?php
/** PAY-CORE-003 successor immutable governance/conversion/parity custody installer. */
include_once(dirname(__FILE__).'/pay_core_003_schema_db.inc');
function hrm_pay_core_003_successor_expected_columns(){return array(
'hrm_pay_core_003_legacy_mapping_plans'=>array('mapping_plan_id','source_kind','source_row_id','source_sha256','target_kind','target_payload_json','target_payload_sha256','supersedes_mapping_plan_id','mapping_plan_sha256','maker_id','created_at'),
'hrm_pay_core_003_legacy_mapping_approvals'=>array('mapping_approval_id','mapping_plan_id','mapping_plan_sha256','decision_token','evidence_sha256','approval_sha256','checker_id','approved_at'),
'hrm_pay_core_003_legacy_conversion_receipts'=>array('conversion_receipt_id','mapping_plan_id','mapping_approval_id','source_kind','source_row_id','source_sha256','target_kind','target_set_json','target_set_sha256','conversion_sha256','executor_id','executed_at'),
'hrm_pay_core_003_legacy_conversion_reconciliations'=>array('reconciliation_id','conversion_receipt_id','source_kind','source_row_id','source_sha256','target_set_sha256','status_token','evidence_sha256','recovery_action','supersedes_reconciliation_id','reconciliation_sha256','reviewer_id','created_at'),
'hrm_pay_core_003_compatibility_parity_evidence'=>array('parity_evidence_id','conversion_receipt_id','source_kind','source_row_id','source_sha256','target_set_sha256','projection_sha256','status_token','evidence_sha256','verified_by','verified_at','parity_sha256'));
}
function hrm_pay_core_003_successor_trigger_codes(){return array('hrm_pay_core_003_legacy_mapping_plans'=>'mpl','hrm_pay_core_003_legacy_mapping_approvals'=>'map','hrm_pay_core_003_legacy_conversion_receipts'=>'cvr','hrm_pay_core_003_legacy_conversion_reconciliations'=>'rcn','hrm_pay_core_003_compatibility_parity_evidence'=>'pry');}
function hrm_pay_core_003_install_successor_subset($patch,$sql_file,array $tables){$all=hrm_pay_core_003_successor_expected_columns();$codes=hrm_pay_core_003_successor_trigger_codes();$cols=array();$tc=array();foreach($tables as $t){if(!isset($all[$t],$codes[$t]))return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CORE-003 successor table contract unknown: '.$t);$cols[$t]=$all[$t];$tc[$t]=$codes[$t];}if(!hrm_pay_core_003_install_tables($patch,$sql_file,$cols,$tc))return false;return hrm_pay_core_003_require_zero_rows($patch,$tables);}
