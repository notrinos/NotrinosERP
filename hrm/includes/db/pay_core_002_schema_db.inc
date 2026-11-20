<?php
/** Frozen schema helpers for PAY-CORE-002 additive immutable custody. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
function hrm_pay_core_002_trigger_name($code,$op){$prefix=hrm_migration_active_table_prefix();$suffix=strtoupper($op)==='UPDATE'?'u':'d';if($prefix===false||!preg_match('/^[a-z0-9_]{1,12}$/D',$code))return false;$n=$prefix.'pc2_'.$code.'_'.$suffix;return strlen($n)<=64?$n:false;}
function hrm_pay_core_002_expected_columns(){return array(
'hrm_payroll_calendars'=>array('calendar_id','calendar_code','calendar_name','legal_entity_id','calendar_sha256','created_by','created_at'),
'hrm_payroll_calendar_versions'=>array('calendar_version_id','calendar_id','version_no','frequency_token','effective_from','effective_to','timezone_name','version_sha256','created_by','created_at'),
'hrm_payroll_calendar_periods'=>array('calendar_period_id','calendar_version_id','period_key','period_start','period_end','cutoff_date','pay_date','post_date','file_date','run_type','period_sha256','created_by','created_at'),
'hrm_payroll_groups'=>array('payroll_group_id','group_code','group_name','legal_entity_id','group_sha256','created_by','created_at'),
'hrm_payroll_group_versions'=>array('group_version_id','payroll_group_id','version_no','calendar_version_id','effective_from','effective_to','version_sha256','created_by','created_at'),
'hrm_payroll_relationships'=>array('payroll_relationship_id','worker_id','employment_id','legal_entity_id','relationship_key','effective_from','effective_to','relationship_sha256','created_by','created_at'),
'hrm_payroll_relationship_members'=>array('relationship_member_id','payroll_relationship_id','assignment_id','effective_from','effective_to','member_sha256','created_by','created_at'),
'hrm_payroll_group_enrollments'=>array('enrollment_id','payroll_relationship_id','group_version_id','effective_from','effective_to','enrollment_sha256','created_by','created_at'));
}
