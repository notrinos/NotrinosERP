<?php
/** PAY-CTRY-002 exact lifecycle/staging schema installer/validator. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
function pay_ctry_002_schema_groups(){return array(
 'site_lifecycle'=>array('pay_ctry_site_operation_journal','pay_ctry_site_installations'),
 'impact_simulation'=>array('pay_ctry_activation_impact_simulations'),
 'activation_proposal'=>array('pay_ctry_activation_proposals'),
 'activation_approval'=>array('pay_ctry_activation_approvals'),
 'activation_execution'=>array('pay_ctry_activations'),
 'activation_binding'=>array('pay_ctry_activation_bindings'),
 'activation_suspension'=>array('pay_ctry_activation_suspensions'),
 'activation_outbox'=>array('pay_ctry_activation_outbox','pay_ctry_activation_delivery_attempts'),
 'activation_run_guard'=>array('pay_ctry_activation_run_bindings'),
 'activation_revocation'=>array('pay_ctry_activation_revocations')
);}
function pay_ctry_002_schema_columns(){return array(
 'pay_ctry_site_operation_journal'=>array('operation_event_id','operation_key','event_no','operation_type','event_type','pack_version_id','predecessor_pack_version_id','verification_report_id','evidence_sha256','actor_id','created_at'),
 'pay_ctry_site_installations'=>array('site_installation_id','pack_version_id','predecessor_site_installation_id','operation_event_id','installation_kind','semantic_root','evidence_root','receipt_sha256','installed_by','created_at'),
 'pay_ctry_activation_impact_simulations'=>array('impact_simulation_id','impact_key','site_installation_id','company_id','scope_type','legal_entity_id','payroll_scope_key','action_type','effective_from','effective_to','dependency_lock_sha256','binding_snapshot_sha256','open_run_count','open_approval_count','downstream_intent_count','blocking_finding_count','impact_payload_sha256','impact_sha256','simulated_by','created_at'),
 'pay_ctry_activation_proposals'=>array('activation_proposal_id','proposal_key','site_installation_id','impact_simulation_id','company_id','scope_type','legal_entity_id','payroll_scope_key','action_type','effective_from','effective_to','dependency_lock_sha256','binding_snapshot_sha256','impact_sha256','proposal_sha256','maker_id','created_at'),
 'pay_ctry_activation_approvals'=>array('activation_approval_id','activation_proposal_id','proposal_sha256','decision_token','evidence_sha256','approval_sha256','checker_id','created_at'),
 'pay_ctry_activations'=>array('activation_id','activation_key','activation_proposal_id','activation_approval_id','site_installation_id','company_id','scope_type','legal_entity_id','payroll_scope_key','action_type','effective_from','effective_to','dependency_lock_sha256','binding_snapshot_sha256','proposal_sha256','approval_sha256','assurance_event_id','execution_action_sha256','activation_sha256','executor_id','created_at'),
 'pay_ctry_activation_bindings'=>array('binding_id','activation_id','predecessor_binding_id','site_installation_id','company_id','scope_type','legal_entity_id','payroll_scope_key','action_type','effective_from','effective_to','activation_sha256','binding_sha256','bound_by','created_at'),
 'pay_ctry_activation_suspensions'=>array('suspension_id','suspension_key','activation_id','binding_id','company_id','scope_type','legal_entity_id','payroll_scope_key','action_type','effective_at','reason_code','reason_sha256','assurance_event_id','action_sha256','suspension_sha256','acted_by','created_at'),
 'pay_ctry_activation_outbox'=>array('outbox_id','outbox_key','event_type','source_type','source_id','activation_id','suspension_id','company_id','scope_type','legal_entity_id','payroll_scope_key','effective_at','payload_sha256','outbox_sha256','created_by','created_at'),
 'pay_ctry_activation_delivery_attempts'=>array('delivery_attempt_id','outbox_id','attempt_no','target_code','delivery_status','result_sha256','error_code','evidence_sha256','attempted_by','created_at'),
 'pay_ctry_activation_run_bindings'=>array('run_binding_id','run_id','binding_id','activation_id','company_id','scope_type','legal_entity_id','payroll_scope_key','from_date','to_date','resolution_state','binding_sha256','snapshot_sha256','captured_by','created_at'),
 'pay_ctry_activation_revocations'=>array('revocation_id','revocation_key','activation_id','binding_id','company_id','scope_type','legal_entity_id','payroll_scope_key','action_type','effective_at','reason_code','reason_sha256','assurance_event_id','revocation_sha256','revoked_by','created_at')
);}
function pay_ctry_002_primary_keys(){return array(
 'pay_ctry_site_operation_journal'=>array('operation_event_id'),
 'pay_ctry_site_installations'=>array('site_installation_id'),
 'pay_ctry_activation_impact_simulations'=>array('impact_simulation_id'),
 'pay_ctry_activation_proposals'=>array('activation_proposal_id'),
 'pay_ctry_activation_approvals'=>array('activation_approval_id'),
 'pay_ctry_activations'=>array('activation_id'),
 'pay_ctry_activation_bindings'=>array('binding_id'),
 'pay_ctry_activation_suspensions'=>array('suspension_id'),
 'pay_ctry_activation_outbox'=>array('outbox_id'),
 'pay_ctry_activation_delivery_attempts'=>array('delivery_attempt_id'),
 'pay_ctry_activation_run_bindings'=>array('run_binding_id'),
 'pay_ctry_activation_revocations'=>array('revocation_id')
);}
function pay_ctry_002_required_indexes(){return array(
 'pay_ctry_site_operation_journal'=>array(
  'uq_pct2_op_event'=>array(1,array('operation_key','event_no')),
  'uq_pct2_op_evidence'=>array(1,array('evidence_sha256')),
  'ix_pct2_op_pack'=>array(0,array('pack_version_id','event_type','created_at')),
  'ix_pct2_op_pred'=>array(0,array('predecessor_pack_version_id')),
  'ix_pct2_op_report'=>array(0,array('verification_report_id'))
 ),
 'pay_ctry_site_installations'=>array(
  'uq_pct2_si_event'=>array(1,array('operation_event_id')),
  'uq_pct2_si_receipt'=>array(1,array('receipt_sha256')),
  'ix_pct2_si_pack'=>array(0,array('pack_version_id','created_at')),
  'ix_pct2_si_pred'=>array(0,array('predecessor_site_installation_id'))
 ),
 'pay_ctry_activation_impact_simulations'=>array(
  'uq_pct2_im_key'=>array(1,array('impact_key')),
  'uq_pct2_im_hash'=>array(1,array('impact_sha256')),
  'ix_pct2_im_install'=>array(0,array('site_installation_id','created_at')),
  'ix_pct2_im_scope'=>array(0,array('company_id','scope_type','legal_entity_id','payroll_scope_key','effective_from'))
 ),
 'pay_ctry_activation_proposals'=>array(
  'uq_pct2_ap_key'=>array(1,array('proposal_key')),
  'uq_pct2_ap_hash'=>array(1,array('proposal_sha256')),
  'ix_pct2_ap_install'=>array(0,array('site_installation_id','created_at')),
  'ix_pct2_ap_impact'=>array(0,array('impact_simulation_id')),
  'ix_pct2_ap_scope'=>array(0,array('company_id','scope_type','legal_entity_id','payroll_scope_key','effective_from'))
 ),
 'pay_ctry_activation_approvals'=>array(
  'uq_pct2_aa_proposal'=>array(1,array('activation_proposal_id')),
  'uq_pct2_aa_hash'=>array(1,array('approval_sha256')),
  'ix_pct2_aa_checker'=>array(0,array('checker_id','created_at'))
 ),
 'pay_ctry_activations'=>array(
  'uq_pct2_ac_key'=>array(1,array('activation_key')),
  'uq_pct2_ac_proposal'=>array(1,array('activation_proposal_id')),
  'uq_pct2_ac_approval'=>array(1,array('activation_approval_id')),
  'uq_pct2_ac_hash'=>array(1,array('activation_sha256')),
  'ix_pct2_ac_install'=>array(0,array('site_installation_id','created_at')),
  'ix_pct2_ac_scope'=>array(0,array('company_id','scope_type','legal_entity_id','payroll_scope_key','effective_from'))
 ),
 'pay_ctry_activation_bindings'=>array(
  'uq_pct2_bd_activation'=>array(1,array('activation_id')),
  'uq_pct2_bd_hash'=>array(1,array('binding_sha256')),
  'ix_pct2_bd_pred'=>array(0,array('predecessor_binding_id')),
  'ix_pct2_bd_install'=>array(0,array('site_installation_id','created_at')),
  'ix_pct2_bd_scope'=>array(0,array('company_id','scope_type','legal_entity_id','payroll_scope_key','effective_from'))
 ),
 'pay_ctry_activation_suspensions'=>array(
  'uq_pct2_ss_key'=>array(1,array('suspension_key')),
  'uq_pct2_ss_hash'=>array(1,array('suspension_sha256')),
  'ix_pct2_ss_activation'=>array(0,array('activation_id','effective_at')),
  'ix_pct2_ss_binding'=>array(0,array('binding_id','effective_at')),
  'ix_pct2_ss_scope'=>array(0,array('company_id','scope_type','legal_entity_id','payroll_scope_key','effective_at'))
 ),
 'pay_ctry_activation_outbox'=>array(
  'uq_pct2_ob_key'=>array(1,array('outbox_key')),
  'uq_pct2_ob_hash'=>array(1,array('outbox_sha256')),
  'ix_pct2_ob_source'=>array(0,array('source_type','source_id','created_at')),
  'ix_pct2_ob_scope'=>array(0,array('company_id','scope_type','legal_entity_id','payroll_scope_key','effective_at')),
  'fk_pct2_ob_activation'=>array(0,array('activation_id')),
  'fk_pct2_ob_suspension'=>array(0,array('suspension_id'))
 ),
 'pay_ctry_activation_delivery_attempts'=>array(
  'uq_pct2_da_attempt'=>array(1,array('outbox_id','attempt_no')),
  'uq_pct2_da_evidence'=>array(1,array('evidence_sha256')),
  'ix_pct2_da_status'=>array(0,array('outbox_id','delivery_status','created_at'))
 ),
 'pay_ctry_activation_run_bindings'=>array(
  'uq_pct2_rg_run'=>array(1,array('run_id')),
  'uq_pct2_rg_hash'=>array(1,array('snapshot_sha256')),
  'ix_pct2_rg_binding'=>array(0,array('binding_id','created_at')),
  'ix_pct2_rg_scope'=>array(0,array('company_id','scope_type','legal_entity_id','payroll_scope_key','from_date')),
  'fk_pct2_rg_activation'=>array(0,array('activation_id'))
 ),
 'pay_ctry_activation_revocations'=>array(
  'uq_pct2_rv_key'=>array(1,array('revocation_key')),
  'uq_pct2_rv_hash'=>array(1,array('revocation_sha256')),
  'ix_pct2_rv_activation'=>array(0,array('activation_id','effective_at')),
  'ix_pct2_rv_binding'=>array(0,array('binding_id','effective_at')),
  'ix_pct2_rv_scope'=>array(0,array('company_id','scope_type','legal_entity_id','payroll_scope_key','effective_at'))
 )
);}
function pay_ctry_002_required_foreign_keys(){return array(
 'pay_ctry_site_operation_journal'=>array(
  'fk_pct2_op_pack'=>array('pack_version_id','pay_ctry_pack_versions','pack_version_id'),
  'fk_pct2_op_pred'=>array('predecessor_pack_version_id','pay_ctry_pack_versions','pack_version_id'),
  'fk_pct2_op_report'=>array('verification_report_id','pay_ctry_verification_reports','verification_report_id')
 ),
 'pay_ctry_site_installations'=>array(
  'fk_pct2_si_pack'=>array('pack_version_id','pay_ctry_pack_versions','pack_version_id'),
  'fk_pct2_si_pred'=>array('predecessor_site_installation_id','pay_ctry_site_installations','site_installation_id'),
  'fk_pct2_si_event'=>array('operation_event_id','pay_ctry_site_operation_journal','operation_event_id')
 ),
 'pay_ctry_activation_impact_simulations'=>array(
  'fk_pct2_im_install'=>array('site_installation_id','pay_ctry_site_installations','site_installation_id')
 ),
 'pay_ctry_activation_proposals'=>array(
  'fk_pct2_ap_install'=>array('site_installation_id','pay_ctry_site_installations','site_installation_id'),
  'fk_pct2_ap_impact'=>array('impact_simulation_id','pay_ctry_activation_impact_simulations','impact_simulation_id')
 ),
 'pay_ctry_activation_approvals'=>array(
  'fk_pct2_aa_proposal'=>array('activation_proposal_id','pay_ctry_activation_proposals','activation_proposal_id')
 ),
 'pay_ctry_activations'=>array(
  'fk_pct2_ac_proposal'=>array('activation_proposal_id','pay_ctry_activation_proposals','activation_proposal_id'),
  'fk_pct2_ac_approval'=>array('activation_approval_id','pay_ctry_activation_approvals','activation_approval_id'),
  'fk_pct2_ac_install'=>array('site_installation_id','pay_ctry_site_installations','site_installation_id')
 ),
 'pay_ctry_activation_bindings'=>array(
  'fk_pct2_bd_activation'=>array('activation_id','pay_ctry_activations','activation_id'),
  'fk_pct2_bd_pred'=>array('predecessor_binding_id','pay_ctry_activation_bindings','binding_id'),
  'fk_pct2_bd_install'=>array('site_installation_id','pay_ctry_site_installations','site_installation_id')
 ),
 'pay_ctry_activation_suspensions'=>array(
  'fk_pct2_ss_activation'=>array('activation_id','pay_ctry_activations','activation_id'),
  'fk_pct2_ss_binding'=>array('binding_id','pay_ctry_activation_bindings','binding_id')
 ),
 'pay_ctry_activation_outbox'=>array(
  'fk_pct2_ob_activation'=>array('activation_id','pay_ctry_activations','activation_id'),
  'fk_pct2_ob_suspension'=>array('suspension_id','pay_ctry_activation_suspensions','suspension_id')
 ),
 'pay_ctry_activation_delivery_attempts'=>array(
  'fk_pct2_da_outbox'=>array('outbox_id','pay_ctry_activation_outbox','outbox_id')
 ),
 'pay_ctry_activation_run_bindings'=>array(
  'fk_pct2_rg_run'=>array('run_id','hrm_pay_core_009_runs','run_id'),
  'fk_pct2_rg_binding'=>array('binding_id','pay_ctry_activation_bindings','binding_id'),
  'fk_pct2_rg_activation'=>array('activation_id','pay_ctry_activations','activation_id')
 ),
 'pay_ctry_activation_revocations'=>array(
  'fk_pct2_rv_activation'=>array('activation_id','pay_ctry_activations','activation_id'),
  'fk_pct2_rv_binding'=>array('binding_id','pay_ctry_activation_bindings','binding_id')
 )
);}
function pay_ctry_002_trigger_codes(){return array(
 'pay_ctry_site_operation_journal'=>'oj','pay_ctry_site_installations'=>'si',
 'pay_ctry_activation_impact_simulations'=>'im','pay_ctry_activation_proposals'=>'ap','pay_ctry_activation_approvals'=>'aa',
 'pay_ctry_activations'=>'ac','pay_ctry_activation_bindings'=>'bd',
 'pay_ctry_activation_suspensions'=>'ss','pay_ctry_activation_outbox'=>'ob',
 'pay_ctry_activation_delivery_attempts'=>'da','pay_ctry_activation_run_bindings'=>'rg',
 'pay_ctry_activation_revocations'=>'rv');}
function pay_ctry_002_table_columns($table){if(!hrm_migration_table_exists($table))return false;$q=db_query('SHOW COLUMNS FROM '.TB_PREF.$table);if(!$q)return false;$o=array();while($r=db_fetch_assoc($q))$o[]=(string)$r['Field'];return $o;}
function pay_ctry_002_table_primary_key($table){$p=hrm_migration_physical_table_name($table);if($p===false)return false;$q=db_query('SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($p).' AND INDEX_NAME=\'PRIMARY\' ORDER BY SEQ_IN_INDEX');if(!$q)return false;$o=array();while($r=db_fetch_assoc($q))$o[]=(string)$r['COLUMN_NAME'];return $o;}
function pay_ctry_002_table_engine($table){$p=hrm_migration_physical_table_name($table);if($p===false)return false;$q=db_query('SELECT ENGINE FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($p).' LIMIT 1');if(!$q||db_num_rows($q)!==1)return false;$r=db_fetch_assoc($q);return $r?(string)$r['ENGINE']:false;}
function pay_ctry_002_table_indexes($table){$p=hrm_migration_physical_table_name($table);if($p===false)return false;$q=db_query('SELECT INDEX_NAME,NON_UNIQUE,SEQ_IN_INDEX,COLUMN_NAME FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($p).' ORDER BY INDEX_NAME,SEQ_IN_INDEX');if(!$q)return false;$o=array();while($r=db_fetch_assoc($q)){if($r['INDEX_NAME']==='PRIMARY')continue;$n=(string)$r['INDEX_NAME'];if(!isset($o[$n]))$o[$n]=array((int)$r['NON_UNIQUE']===0?1:0,array());$o[$n][1][]=(string)$r['COLUMN_NAME'];}ksort($o);return $o;}
function pay_ctry_002_table_foreign_keys($table){$p=hrm_migration_physical_table_name($table);$prefix=hrm_migration_active_table_prefix();if($p===false||$prefix===false)return false;$q=db_query('SELECT CONSTRAINT_NAME,COLUMN_NAME,REFERENCED_TABLE_NAME,REFERENCED_COLUMN_NAME FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($p).' AND REFERENCED_TABLE_NAME IS NOT NULL ORDER BY CONSTRAINT_NAME,ORDINAL_POSITION');if(!$q)return false;$o=array();while($r=db_fetch_assoc($q)){$ref=(string)$r['REFERENCED_TABLE_NAME'];if(strpos($ref,$prefix)===0)$ref=substr($ref,strlen($prefix));$o[(string)$r['CONSTRAINT_NAME']]=array((string)$r['COLUMN_NAME'],$ref,(string)$r['REFERENCED_COLUMN_NAME']);}ksort($o);return $o;}
function pay_ctry_002_trigger_name($table,$op){$codes=pay_ctry_002_trigger_codes();$prefix=hrm_migration_active_table_prefix();return isset($codes[$table])&&$prefix!==false?$prefix.'ct2_'.$codes[$table].'_'.(strtoupper($op)==='UPDATE'?'u':'d'):false;}
function pay_ctry_002_trigger_message($table,$op){return 'PAY-CTRY-002 immutable '.$table.' row cannot be '.(strtoupper($op)==='UPDATE'?'updated':'deleted');}
function pay_ctry_002_trigger_state($table,$op){$physical=hrm_migration_physical_table_name($table);$name=pay_ctry_002_trigger_name($table,$op);if($physical===false||$name===false)return false;$q=db_query('SELECT EVENT_MANIPULATION,EVENT_OBJECT_TABLE,ACTION_TIMING,ACTION_STATEMENT FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA=DATABASE() AND TRIGGER_NAME='.db_escape($name).' LIMIT 2');if(!$q)return false;if(db_num_rows($q)===0)return 'absent';if(db_num_rows($q)!==1)return 'conflict';$r=db_fetch_assoc($q);$actual=strtoupper(rtrim(preg_replace('/\s+/',' ',trim((string)$r['ACTION_STATEMENT'])),';'));$expected=strtoupper("SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='".pay_ctry_002_trigger_message($table,$op)."'");return strtoupper((string)$r['EVENT_MANIPULATION'])===strtoupper($op)&&(string)$r['EVENT_OBJECT_TABLE']===$physical&&strtoupper((string)$r['ACTION_TIMING'])==='BEFORE'&&$actual===$expected?'valid':'conflict';}
function pay_ctry_002_create_trigger($patch,$table,$op){$state=pay_ctry_002_trigger_state($table,$op);if($state==='valid')return true;if($state!=='absent')return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CTRY-002 immutable trigger conflict: '.$table.'.'.$op);$physical=hrm_migration_physical_table_name($table);$name=pay_ctry_002_trigger_name($table,$op);$sql="CREATE TRIGGER `".$name."` BEFORE ".strtoupper($op)." ON `".$physical."` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT=".db_escape(pay_ctry_002_trigger_message($table,$op));if(!db_query($sql))return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CTRY-002 immutable trigger creation failed: '.$table.'.'.$op);return pay_ctry_002_trigger_state($table,$op)==='valid';}
function pay_ctry_002_schema_group_block($group){global $path_to_root;$raw=@file_get_contents($path_to_root.'/sql/upgrade_pay_ctry_002_schema.sql');if(!is_string($raw)||$raw==='')return false;$start='-- PAY-CTRY-002 GROUP '.$group;$end='-- PAY-CTRY-002 END GROUP '.$group;$a=strpos($raw,$start);$b=strpos($raw,$end);if($a===false||$b===false||$b<=$a)return false;return substr($raw,$a+strlen($start),$b-($a+strlen($start)));}
function pay_ctry_002_require_table_shape($patch,$table){$cols=pay_ctry_002_schema_columns();$pks=pay_ctry_002_primary_keys();$indexes=pay_ctry_002_required_indexes();$fks=pay_ctry_002_required_foreign_keys();if(!isset($cols[$table],$pks[$table],$indexes[$table],$fks[$table]))return false;if(pay_ctry_002_table_columns($table)!==$cols[$table]||pay_ctry_002_table_primary_key($table)!==$pks[$table]||strcasecmp((string)pay_ctry_002_table_engine($table),'InnoDB')!==0)return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CTRY-002 table projection mismatch: '.$table);$got=pay_ctry_002_table_indexes($table);$want=$indexes[$table];ksort($got);ksort($want);if($got!==$want)return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CTRY-002 exact index projection mismatch: '.$table);$gf=pay_ctry_002_table_foreign_keys($table);$wantf=$fks[$table];ksort($gf);ksort($wantf);if($gf!==$wantf)return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CTRY-002 exact foreign-key projection mismatch: '.$table);return true;}
function pay_ctry_002_require_zero_seed($patch,$group){$groups=pay_ctry_002_schema_groups();if(!isset($groups[$group]))return false;foreach($groups[$group] as $table){if(hrm_migration_table_exists($table)){ $q=db_query('SELECT COUNT(*) c FROM '.TB_PREF.$table);$r=$q?db_fetch_assoc($q):false;if(!$r||(int)$r['c']!==0)return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CTRY-002 lifecycle schema must be zero-seed: '.$table);}}return true;}
function pay_ctry_002_install_schema_group($patch,$group){$groups=pay_ctry_002_schema_groups();if(!isset($groups[$group]))return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CTRY-002 schema group unknown: '.$group);if(!pay_ctry_002_require_zero_seed($patch,$group))return false;$block=pay_ctry_002_schema_group_block($group);if($block===false)return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CTRY-002 schema source missing: '.$group);$prefix=hrm_migration_active_table_prefix();if($prefix===false)return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CTRY-002 table prefix invalid.');$statements=preg_split('/;\s*(?:\r?\n|$)/',$block);foreach($statements as $stmt){$stmt=trim($stmt);if($stmt===''||strpos($stmt,'CREATE TRIGGER')===0)continue;$stmt=str_replace('`0_','`'.$prefix,$stmt);if(!db_query($stmt))return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CTRY-002 schema statement failed for group '.$group.'.');}foreach($groups[$group] as $table){if(!pay_ctry_002_require_table_shape($patch,$table))return false;foreach(array('UPDATE','DELETE') as $op)if(!pay_ctry_002_create_trigger($patch,$table,$op))return false;}return pay_ctry_002_require_zero_seed($patch,$group);}
function pay_ctry_002_require_schema_group($patch,$group){$groups=pay_ctry_002_schema_groups();if(!isset($groups[$group]))return false;foreach($groups[$group] as $table){if(!pay_ctry_002_require_table_shape($patch,$table))return false;foreach(array('UPDATE','DELETE') as $op)if(pay_ctry_002_trigger_state($table,$op)!=='valid')return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CTRY-002 immutable trigger mismatch: '.$table.'.'.$op);}return true;}
?>
