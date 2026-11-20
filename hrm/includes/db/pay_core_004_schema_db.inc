<?php
/** Frozen schema helpers for PAY-CORE-004 zero-seed immutable execution-record custody. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');

function hrm_pay_core_004_core_columns(){return array(
'hrm_pay_core_004_snapshots'=>array('snapshot_id','payroll_relationship_id','assignment_id','calendar_period_id','run_identity','legal_entity_id','currency','approved_inputs_sha256','element_versions_sha256','eligibility_sha256','rule_artifacts_sha256','source_events_sha256','configuration_sha256','snapshot_payload_json','snapshot_sha256','sealed_by','sealed_at'),
'hrm_pay_core_004_results'=>array('result_id','snapshot_id','snapshot_sha256','currency','gross_amount','deduction_amount','net_amount','line_set_sha256','trace_set_sha256','result_sha256','executor_id','executed_at'),
'hrm_pay_core_004_result_lines'=>array('result_line_id','result_id','line_no','element_version_id','line_kind','currency','amount','quantity','rate','source_sha256','line_sha256','created_at'),
'hrm_pay_core_004_traces'=>array('trace_id','result_id','trace_seq','trace_kind','artifact_sha256','input_sha256','output_sha256','value_mode','value_digest','trace_sha256','created_at'));
}
function hrm_pay_core_004_integrity_columns(){return array('hrm_pay_core_004_integrity_evidence'=>array('integrity_evidence_id','result_id','snapshot_sha256','result_sha256','line_set_sha256','trace_set_sha256','verification_status','evidence_sha256','integrity_sha256','verified_by','verified_at'));}
function hrm_pay_core_004_lineage_columns(){return array('hrm_pay_core_004_correction_lineage'=>array('lineage_id','predecessor_result_id','successor_result_id','lineage_kind','reason_code','approval_evidence_sha256','lineage_sha256','checker_id','created_at'));}
function hrm_pay_core_004_legacy_provenance_columns(){return array('hrm_pay_core_004_legacy_provenance_gaps'=>array('legacy_provenance_id','legacy_source_kind','legacy_source_id','legacy_source_sha256','provenance_state','gap_codes','evidence_sha256','provenance_sha256','recorded_by','recorded_at'));}
function hrm_pay_core_004_recovery_archive_columns(){return array(
'hrm_pay_core_004_recovery_evidence'=>array('recovery_evidence_id','result_id','integrity_evidence_id','recovery_state','action_token','evidence_sha256','recovery_sha256','reviewer_id','created_at'),
'hrm_pay_core_004_archive_evidence'=>array('archive_evidence_id','result_id','integrity_evidence_id','archive_state','archive_locator_sha256','archive_payload_sha256','evidence_sha256','archive_sha256','reviewer_id','created_at'));
}
function hrm_pay_core_004_all_columns(){return array_merge(hrm_pay_core_004_core_columns(),hrm_pay_core_004_integrity_columns(),hrm_pay_core_004_lineage_columns(),hrm_pay_core_004_legacy_provenance_columns(),hrm_pay_core_004_recovery_archive_columns());}
