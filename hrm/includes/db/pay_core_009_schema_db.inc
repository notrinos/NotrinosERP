<?php
/** PAY-CORE-009 authoritative mutable state plus immutable operational evidence. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
function hrm_pay_core_009_state_columns(){return array(
'hrm_pay_core_009_runs'=>array('run_id','run_key','run_type','payroll_period_id','legal_entity_id','calendar_period_id','period_name','from_date','to_date','chunk_size','request_sha256','state','state_version','state_sha256','checkpoint_no','lease_worker_sha256','lease_token_sha256','lease_expires_at','heartbeat_at','created_by','created_at','updated_at'),
'hrm_pay_core_009_items'=>array('run_item_id','run_id','ordinal_no','employee_id','employee_id_sha256','legal_entity_id','payroll_relationship_id','worker_id','person_id','assignment_id','calendar_period_id','object_scope_sha256','input_sha256','item_state','state_version','item_state_sha256','attempt_no','result_sha256','result_payload_json','lease_token_sha256','last_error_code','created_at','updated_at'));
}
function hrm_pay_core_009_event_columns(){return array(
'hrm_pay_core_009_lease_events'=>array('lease_event_id','run_id','event_kind','worker_sha256','lease_token_sha256','acquired_at','expires_at','lease_event_sha256','created_by','created_at'),
'hrm_pay_core_009_attempts'=>array('attempt_id','run_id','run_item_id','attempt_no','command_key','worker_sha256','predecessor_state_sha256','outcome','result_sha256','error_code','evidence_sha256','attempt_sha256','executed_by','created_at'),
'hrm_pay_core_009_checkpoints'=>array('checkpoint_id','run_id','checkpoint_no','completed_items','failed_items','quarantined_items','item_set_sha256','state_sha256','evidence_sha256','checkpoint_sha256','created_by','created_at'),
'hrm_pay_core_009_transitions'=>array('transition_id','run_id','from_state','to_state','expected_version','resulting_version','command_key','predecessor_state_sha256','resulting_state_sha256','transition_sha256','actor_id','created_at'),
'hrm_pay_core_009_quarantine'=>array('quarantine_id','run_id','run_item_id','attempt_sha256','reason_code','evidence_sha256','quarantine_sha256','quarantined_by','created_at'),
'hrm_pay_core_009_recovery_evidence'=>array('recovery_evidence_id','run_id','action_token','predecessor_state_sha256','evidence_sha256','recovery_sha256','executed_by','created_at'));
}
function hrm_pay_core_009_all_columns(){return array_merge(hrm_pay_core_009_state_columns(),hrm_pay_core_009_event_columns());}
function hrm_pay_core_009_trigger_codes(){return array('hrm_pay_core_009_lease_events'=>'lea','hrm_pay_core_009_attempts'=>'att','hrm_pay_core_009_checkpoints'=>'chk','hrm_pay_core_009_transitions'=>'trs','hrm_pay_core_009_quarantine'=>'qua','hrm_pay_core_009_recovery_evidence'=>'rcv');}
function hrm_pay_core_009_required_indexes(){return array(
'hrm_pay_core_009_runs'=>array('PRIMARY','uq_pc9_run_key','uq_pc9_request','uq_pc9_payroll_period','ix_pc9_run_state_lease','ix_pc9_run_scope'),
'hrm_pay_core_009_items'=>array('PRIMARY','uq_pc9_item_ordinal','uq_pc9_item_employee','ix_pc9_item_state','ix_pc9_item_scope'),
'hrm_pay_core_009_lease_events'=>array('PRIMARY','uq_pc9_lease_sha','ix_pc9_lease_run'),
'hrm_pay_core_009_attempts'=>array('PRIMARY','uq_pc9_attempt_sha','uq_pc9_attempt_no','ix_pc9_attempt_run'),
'hrm_pay_core_009_checkpoints'=>array('PRIMARY','uq_pc9_checkpoint_sha','uq_pc9_checkpoint_no'),
'hrm_pay_core_009_transitions'=>array('PRIMARY','uq_pc9_transition_sha','uq_pc9_transition_version'),
'hrm_pay_core_009_quarantine'=>array('PRIMARY','uq_pc9_quarantine_sha','uq_pc9_quarantine_attempt'),
'hrm_pay_core_009_recovery_evidence'=>array('PRIMARY','uq_pc9_recovery_sha'));
}
?>
