<?php
/** PAY-GL-001 completion schema metadata/install helpers for 1.0.833-1.0.837. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
include_once(dirname(__FILE__).'/pay_gl_001_schema_db.inc');

function pay_gl_001_completion_groups(){return array(
 'mapping_governance'=>array('sql'=>'sql/upgrade_pay_gl_001_mapping_governance.sql','tables'=>array(
  'hrm_pay_gl_001_mapping_versions'=>array('mapping_version_id','company_id','legal_entity_id','mapping_key','effective_from','effective_to','maker_id','checker_id','approved_at','mapping_snapshot_sha256','created_at'),
  'hrm_pay_gl_001_mapping_rules'=>array('mapping_rule_id','mapping_version_id','rule_no','source_kind','source_key','entry_side','account_code','dimension1_axis_code','dimension2_axis_code','suspense_allowed','rule_sha256','created_at'))),
 'writer_reconciliation'=>array('sql'=>'sql/upgrade_pay_gl_001_journal_writer_reconciliation.sql','tables'=>array(
  'hrm_pay_gl_001_reconciliations'=>array('reconciliation_id','journal_intent_id','journal_sha256','line_set_sha256','allocation_set_sha256','native_projection_set_sha256','debit_minor','credit_minor','status','reconciliation_sha256','created_by','created_at'),
  'hrm_pay_gl_001_reviews'=>array('review_id','reconciliation_id','decision','reviewer_id','reviewed_at','review_sha256','created_at'))),
 'posting_bridge'=>array('sql'=>'sql/upgrade_pay_gl_001_posting_bridge_authority.sql','tables'=>array(
  'hrm_pay_gl_001_posting_authorities'=>array('authority_id','journal_intent_id','reconciliation_id','review_id','command_key_sha256','executor_id','authorized_at','authority_sha256','created_at'),
  'hrm_pay_gl_001_gl_postings'=>array('posting_id','journal_intent_id','authority_id','posting_kind','predecessor_posting_id','effect_key','gl_type','gl_trans_no','journal_date','posting_sha256','posted_by','posted_at','created_at'),
  'hrm_pay_gl_001_gl_posting_lines'=>array('posting_line_id','posting_id','journal_line_id','allocation_id','core_dimension_id','core_dimension2_id','gl_counter','source_amount_minor','home_amount_minor','projection_sha256','created_at'))),
 'reversal_recovery'=>array('sql'=>'sql/upgrade_pay_gl_001_reversal_repost_recovery.sql','tables'=>array(
  'hrm_pay_gl_001_recovery_commands'=>array('recovery_command_id','company_id','journal_intent_id','target_posting_id','command_kind','recovery_date','command_key_sha256','reason_code','maker_id','reviewer_id','approved_at','command_sha256','created_at'),
  'hrm_pay_gl_001_reversal_repost_links'=>array('recovery_link_id','recovery_command_id','predecessor_posting_id','successor_posting_id','link_kind','effect_key','evidence_sha256','created_at'))),
 'legacy_cutover'=>array('sql'=>'sql/upgrade_pay_gl_001_legacy_cutover.sql','tables'=>array(
  'hrm_pay_gl_001_cutovers'=>array('cutover_id','company_id','legal_entity_id','payroll_group_id','effective_from','mapping_snapshot_sha256','approved_by','reviewer_id','approved_at','cutover_sha256','created_at')))
 );}

function pay_gl_001_completion_base_columns(){return array(
'hrm_pay_gl_001_journal_intents'=>array('journal_intent_id','company_id','legal_entity_id','payroll_group_id','payroll_period_id','result_set_sha256','approval_sha256','journal_date','currency_code','mapping_snapshot_sha256','line_set_sha256','allocation_set_sha256','debit_minor','credit_minor','journal_sha256','prepared_by','prepared_at','created_at'),
'hrm_pay_gl_001_journal_lines'=>array('journal_line_id','journal_intent_id','source_result_sha256','source_line_key','line_no','account_code','entry_side','amount_minor','memo_code','exception_reason_code','exception_approved_by','mapping_rule_sha256','canonical_line_sha256','created_at'),
'hrm_pay_gl_001_allocations'=>array('allocation_id','journal_line_id','allocation_no','allocated_amount_minor','allocation_weight_ppm','allocation_sha256','created_at'),
'hrm_pay_gl_001_allocation_axes'=>array('allocation_axis_id','allocation_id','axis_no','axis_code','axis_value_key','axis_value_sha256','core_slot_hint','axis_sha256','created_at'));}

function pay_gl_001_completion_require_foundation_or_extended_base($patch){$cols=pay_gl_001_table_columns('hrm_pay_gl_001_journal_lines');$final=pay_gl_001_completion_base_columns();if(is_array($cols)&&$cols===$final['hrm_pay_gl_001_journal_lines'])return pay_gl_001_completion_require_base($patch);return pay_gl_001_require_schema($patch);}
function pay_gl_001_completion_trigger_code($t){$m=array(
 'hrm_pay_gl_001_mapping_versions'=>'mv','hrm_pay_gl_001_mapping_rules'=>'mr','hrm_pay_gl_001_reconciliations'=>'rc','hrm_pay_gl_001_reviews'=>'rv',
 'hrm_pay_gl_001_posting_authorities'=>'pa','hrm_pay_gl_001_gl_postings'=>'gp','hrm_pay_gl_001_gl_posting_lines'=>'gpl',
 'hrm_pay_gl_001_recovery_commands'=>'gc','hrm_pay_gl_001_reversal_repost_links'=>'gr','hrm_pay_gl_001_cutovers'=>'co');return $m[$t]??false;}
function pay_gl_001_completion_trigger_valid($t,$op){$p=hrm_migration_active_table_prefix();$c=pay_gl_001_completion_trigger_code($t);$phys=hrm_migration_physical_table_name($t);if($p===false||$c===false||$phys===false)return false;$name=$p.'pgl1_'.$c.'_'.(strtoupper($op)==='UPDATE'?'u':'d');$q=db_query('SELECT EVENT_MANIPULATION,EVENT_OBJECT_TABLE,ACTION_TIMING,ACTION_STATEMENT FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA=DATABASE() AND TRIGGER_NAME='.db_escape($name).' LIMIT 2');if(!$q||db_num_rows($q)!==1)return false;$r=db_fetch_assoc($q);return strtoupper((string)$r['EVENT_MANIPULATION'])===strtoupper($op)&&(string)$r['EVENT_OBJECT_TABLE']===$phys&&strtoupper((string)$r['ACTION_TIMING'])==='BEFORE'&&strpos((string)$r['ACTION_STATEMENT'],'PAY-GL-001 immutable')!==false;}
function pay_gl_001_completion_require_base($patch){foreach(pay_gl_001_completion_base_columns() as $t=>$cols){if(pay_gl_001_table_columns($t)!==$cols||!pay_gl_001_table_innodb($t))return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-GL-001 final base projection mismatch: '.$t);foreach(array('UPDATE','DELETE') as $op)if(!pay_gl_001_trigger_valid($t,$op))return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-GL-001 base immutable trigger mismatch: '.$t.'.'.$op);}return true;}
function pay_gl_001_completion_require_groups($patch,array $names){$groups=pay_gl_001_completion_groups();if(!pay_gl_001_completion_require_base($patch))return false;foreach($names as $name){if(!isset($groups[$name]))return false;foreach($groups[$name]['tables'] as $t=>$cols){if(pay_gl_001_table_columns($t)!==$cols||!pay_gl_001_table_innodb($t))return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-GL-001 completion table projection mismatch: '.$t);foreach(array('UPDATE','DELETE') as $op)if(!pay_gl_001_completion_trigger_valid($t,$op))return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-GL-001 completion immutable trigger mismatch: '.$t.'.'.$op);}}return true;}

function pay_gl_001_completion_install_group($patch,$name){
 global $path_to_root;$groups=pay_gl_001_completion_groups();if(!isset($groups[$name]))return false;
 $raw=@file_get_contents($path_to_root.'/'.$groups[$name]['sql']);if(!is_string($raw)||strpos($raw,'-- PAY-GL-001 GROUP '.$name)===false)return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-GL-001 completion schema source missing: '.$name);
 $p=hrm_migration_active_table_prefix();if($p===false)return false;
 $raw=preg_replace('/^--.*$/m','',$raw);$stmts=preg_split('/;\s*(?:\r?\n|$)/',$raw);
 foreach($stmts as $s){$s=trim($s);if($s===''||strpos($s,'CREATE TRIGGER')===0)continue;
  if($name==='mapping_governance'&&stripos($s,'ALTER TABLE')===0){$cols=pay_gl_001_table_columns('hrm_pay_gl_001_journal_lines');if(is_array($cols)&&in_array('source_result_sha256',$cols,true))continue;}
  $s=str_replace('`0_','`'.$p,$s);if(!db_query($s))return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-GL-001 completion schema statement failed: '.$name);
 }
 foreach($groups[$name]['tables'] as $t=>$cols)foreach(array('UPDATE','DELETE') as $op){if(pay_gl_001_completion_trigger_valid($t,$op))continue;$code=pay_gl_001_completion_trigger_code($t);$tn=$p.'pgl1_'.$code.'_'.($op==='UPDATE'?'u':'d');$phys=hrm_migration_physical_table_name($t);$msg='PAY-GL-001 immutable '.$t.' row cannot be '.strtolower($op).'d';$sql='CREATE TRIGGER `'.$tn.'` BEFORE '.$op.' ON `'.$phys.'` FOR EACH ROW SIGNAL SQLSTATE \'45000\' SET MESSAGE_TEXT='.db_escape($msg);if(!db_query($sql))return false;}
 return true;
}
?>
