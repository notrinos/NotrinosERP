<?php
/** PAY-CTRY-007 exact zero-seed append-only catalog, recipient lifecycle and delivery-outbox schema contract. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
include_once($path_to_root.'/includes/pay_ctry_007_contract.inc');
function pay_ctry_007_schema_groups(){return array('catalog_notice'=>array('pay_ctry_catalog_releases','pay_ctry_catalog_release_admissions','pay_ctry_update_notices','pay_ctry_update_notice_applicability'),'recipient_lifecycle'=>array('pay_ctry_update_notice_audiences','pay_ctry_update_notice_recipient_events','pay_ctry_update_notice_acknowledgements','pay_ctry_update_notice_lifecycle_events'),'delivery_outbox'=>array('pay_ctry_update_notice_delivery_outbox','pay_ctry_update_notice_delivery_attempts'));}
function pay_ctry_007_schema_columns(){return array(
'pay_ctry_catalog_releases'=>array('catalog_release_id','catalog_code','release_no','predecessor_catalog_release_id','predecessor_release_sha256','catalog_root_sha256','released_at','publisher_id','signer_id','trust_key_id','signature_algorithm','signature_payload_sha256','signature_base64','release_sha256','received_by','created_at'),
'pay_ctry_catalog_release_admissions'=>array('catalog_admission_id','catalog_release_id','decision','signature_verified','trust_snapshot_sha256','evidence_sha256','maker_id','checker_id','decided_at','admission_sha256','created_at'),
'pay_ctry_update_notices'=>array('update_notice_id','notice_key','catalog_release_id','pack_code','current_pack_version','current_semantic_root','current_evidence_root','successor_pack_version','successor_semantic_root','successor_evidence_root','severity','deadline_at','required_action','applicability_sha256','impact_sha256','dedupe_key','notice_sha256','issued_at','created_by','created_at'),
'pay_ctry_update_notice_applicability'=>array('notice_applicability_id','update_notice_id','company_id','scope_type','legal_entity_id','payroll_scope_key','current_binding_sha256','run_snapshot_sha256','affected_open_run_count','affected_future_run_count','impact_sha256','applicability_sha256','created_at'),
'pay_ctry_update_notice_audiences'=>array('notice_audience_id','update_notice_id','notice_applicability_id','audience_key','audience_type','audience_scope_sha256','recipient_set_sha256','audience_sha256','selected_by','selected_at','created_at'),
'pay_ctry_update_notice_recipient_events'=>array('recipient_event_id','notice_audience_id','update_notice_id','notice_applicability_id','recipient_user_id','recipient_event_key','event_no','predecessor_recipient_event_id','predecessor_recipient_event_sha256','event_type','reason_code','recipient_event_sha256','acted_by','occurred_at','created_at'),
'pay_ctry_update_notice_acknowledgements'=>array('notice_acknowledgement_id','recipient_event_id','update_notice_id','notice_applicability_id','recipient_user_id','acknowledgement_type','acknowledged_at','acknowledgement_sha256','actor_id','created_at'),
'pay_ctry_update_notice_lifecycle_events'=>array('notice_lifecycle_event_id','update_notice_id','notice_applicability_id','recipient_event_id','lifecycle_event_key','event_type','due_at','reason_code','evidence_sha256','lifecycle_sha256','actor_id','occurred_at','created_at'),
'pay_ctry_update_notice_delivery_outbox'=>array('delivery_outbox_id','recipient_event_id','update_notice_id','notice_applicability_id','recipient_user_id','outbox_key','channel','payload_sha256','outbox_sha256','queued_by','queued_at','created_at'),
'pay_ctry_update_notice_delivery_attempts'=>array('delivery_attempt_id','delivery_outbox_id','attempt_no','provider_code','delivery_status','result_sha256','error_code','delivery_attempt_sha256','attempted_by','attempted_at','created_at'));
}
function pay_ctry_007_primary_keys(){return array('pay_ctry_catalog_releases'=>array('catalog_release_id'),'pay_ctry_catalog_release_admissions'=>array('catalog_admission_id'),'pay_ctry_update_notices'=>array('update_notice_id'),'pay_ctry_update_notice_applicability'=>array('notice_applicability_id'),'pay_ctry_update_notice_audiences'=>array('notice_audience_id'),'pay_ctry_update_notice_recipient_events'=>array('recipient_event_id'),'pay_ctry_update_notice_acknowledgements'=>array('notice_acknowledgement_id'),'pay_ctry_update_notice_lifecycle_events'=>array('notice_lifecycle_event_id'),'pay_ctry_update_notice_delivery_outbox'=>array('delivery_outbox_id'),'pay_ctry_update_notice_delivery_attempts'=>array('delivery_attempt_id'));}
function pay_ctry_007_required_indexes(){return array(
'pay_ctry_catalog_releases'=>array('uq_pct7_cr_release'=>array(1,array('catalog_code','release_no')),'uq_pct7_cr_sha'=>array(1,array('release_sha256')),'uq_pct7_cr_pred'=>array(1,array('predecessor_catalog_release_id')),'ix_pct7_cr_root'=>array(0,array('catalog_root_sha256','released_at')),'fk_pct7_cr_publisher'=>array(0,array('publisher_id')),'fk_pct7_cr_signer'=>array(0,array('signer_id')),'fk_pct7_cr_key'=>array(0,array('trust_key_id'))),
'pay_ctry_catalog_release_admissions'=>array('uq_pct7_ca_release'=>array(1,array('catalog_release_id')),'uq_pct7_ca_sha'=>array(1,array('admission_sha256')),'ix_pct7_ca_checker'=>array(0,array('checker_id','decided_at'))),
'pay_ctry_update_notices'=>array('uq_pct7_un_key'=>array(1,array('notice_key')),'uq_pct7_un_dedupe'=>array(1,array('dedupe_key')),'uq_pct7_un_sha'=>array(1,array('notice_sha256')),'ix_pct7_un_pack'=>array(0,array('pack_code','current_pack_version','successor_pack_version')),'ix_pct7_un_deadline'=>array(0,array('severity','deadline_at')),'fk_pct7_un_release'=>array(0,array('catalog_release_id'))),
'pay_ctry_update_notice_applicability'=>array('uq_pct7_na_sha'=>array(1,array('applicability_sha256')),'ix_pct7_na_notice'=>array(0,array('update_notice_id','company_id')),'ix_pct7_na_scope'=>array(0,array('company_id','scope_type','legal_entity_id','payroll_scope_key'))),
'pay_ctry_update_notice_audiences'=>array('uq_pct7_au_key'=>array(1,array('audience_key')),'uq_pct7_au_sha'=>array(1,array('audience_sha256')),'ix_pct7_au_notice'=>array(0,array('update_notice_id','notice_applicability_id')),'ix_pct7_au_type'=>array(0,array('audience_type','selected_at')),'fk_pct7_au_app'=>array(0,array('notice_applicability_id'))),
'pay_ctry_update_notice_recipient_events'=>array('uq_pct7_re_key'=>array(1,array('recipient_event_key')),'uq_pct7_re_stream'=>array(1,array('notice_audience_id','recipient_user_id','event_no')),'uq_pct7_re_pred'=>array(1,array('predecessor_recipient_event_id')),'uq_pct7_re_sha'=>array(1,array('recipient_event_sha256')),'ix_pct7_re_user'=>array(0,array('recipient_user_id','occurred_at')),'ix_pct7_re_notice'=>array(0,array('update_notice_id','notice_applicability_id')),'fk_pct7_re_app'=>array(0,array('notice_applicability_id'))),
'pay_ctry_update_notice_acknowledgements'=>array('uq_pct7_ack_recipient'=>array(1,array('recipient_event_id')),'uq_pct7_ack_sha'=>array(1,array('acknowledgement_sha256')),'ix_pct7_ack_user'=>array(0,array('recipient_user_id','acknowledged_at')),'ix_pct7_ack_notice'=>array(0,array('update_notice_id','notice_applicability_id')),'fk_pct7_ack_app'=>array(0,array('notice_applicability_id'))),
'pay_ctry_update_notice_lifecycle_events'=>array('uq_pct7_le_key'=>array(1,array('lifecycle_event_key')),'uq_pct7_le_sha'=>array(1,array('lifecycle_sha256')),'ix_pct7_le_notice'=>array(0,array('update_notice_id','notice_applicability_id','occurred_at')),'ix_pct7_le_recipient'=>array(0,array('recipient_event_id','event_type','occurred_at')),'fk_pct7_le_app'=>array(0,array('notice_applicability_id'))),
'pay_ctry_update_notice_delivery_outbox'=>array('uq_pct7_ob_key'=>array(1,array('outbox_key')),'uq_pct7_ob_recipient_channel'=>array(1,array('recipient_event_id','channel')),'uq_pct7_ob_sha'=>array(1,array('outbox_sha256')),'ix_pct7_ob_user'=>array(0,array('recipient_user_id','channel','queued_at')),'ix_pct7_ob_notice'=>array(0,array('update_notice_id','notice_applicability_id')),'fk_pct7_ob_app'=>array(0,array('notice_applicability_id'))),
'pay_ctry_update_notice_delivery_attempts'=>array('uq_pct7_da_attempt'=>array(1,array('delivery_outbox_id','attempt_no')),'uq_pct7_da_sha'=>array(1,array('delivery_attempt_sha256')),'ix_pct7_da_status'=>array(0,array('delivery_outbox_id','delivery_status','attempted_at'))));}
function pay_ctry_007_required_foreign_keys(){return array(
'pay_ctry_catalog_releases'=>array('fk_pct7_cr_pred'=>array('predecessor_catalog_release_id','pay_ctry_catalog_releases','catalog_release_id'),'fk_pct7_cr_publisher'=>array('publisher_id','pay_ctry_publishers','publisher_id'),'fk_pct7_cr_signer'=>array('signer_id','pay_ctry_signers','signer_id'),'fk_pct7_cr_key'=>array('trust_key_id','pay_ctry_trust_keys','trust_key_id')),
'pay_ctry_catalog_release_admissions'=>array('fk_pct7_ca_release'=>array('catalog_release_id','pay_ctry_catalog_releases','catalog_release_id')),
'pay_ctry_update_notices'=>array('fk_pct7_un_release'=>array('catalog_release_id','pay_ctry_catalog_releases','catalog_release_id')),
'pay_ctry_update_notice_applicability'=>array('fk_pct7_na_notice'=>array('update_notice_id','pay_ctry_update_notices','update_notice_id')),
'pay_ctry_update_notice_audiences'=>array('fk_pct7_au_notice'=>array('update_notice_id','pay_ctry_update_notices','update_notice_id'),'fk_pct7_au_app'=>array('notice_applicability_id','pay_ctry_update_notice_applicability','notice_applicability_id')),
'pay_ctry_update_notice_recipient_events'=>array('fk_pct7_re_audience'=>array('notice_audience_id','pay_ctry_update_notice_audiences','notice_audience_id'),'fk_pct7_re_notice'=>array('update_notice_id','pay_ctry_update_notices','update_notice_id'),'fk_pct7_re_app'=>array('notice_applicability_id','pay_ctry_update_notice_applicability','notice_applicability_id'),'fk_pct7_re_pred'=>array('predecessor_recipient_event_id','pay_ctry_update_notice_recipient_events','recipient_event_id')),
'pay_ctry_update_notice_acknowledgements'=>array('fk_pct7_ack_recipient'=>array('recipient_event_id','pay_ctry_update_notice_recipient_events','recipient_event_id'),'fk_pct7_ack_notice'=>array('update_notice_id','pay_ctry_update_notices','update_notice_id'),'fk_pct7_ack_app'=>array('notice_applicability_id','pay_ctry_update_notice_applicability','notice_applicability_id')),
'pay_ctry_update_notice_lifecycle_events'=>array('fk_pct7_le_notice'=>array('update_notice_id','pay_ctry_update_notices','update_notice_id'),'fk_pct7_le_app'=>array('notice_applicability_id','pay_ctry_update_notice_applicability','notice_applicability_id'),'fk_pct7_le_recipient'=>array('recipient_event_id','pay_ctry_update_notice_recipient_events','recipient_event_id')),
'pay_ctry_update_notice_delivery_outbox'=>array('fk_pct7_ob_recipient'=>array('recipient_event_id','pay_ctry_update_notice_recipient_events','recipient_event_id'),'fk_pct7_ob_notice'=>array('update_notice_id','pay_ctry_update_notices','update_notice_id'),'fk_pct7_ob_app'=>array('notice_applicability_id','pay_ctry_update_notice_applicability','notice_applicability_id')),
'pay_ctry_update_notice_delivery_attempts'=>array('fk_pct7_da_outbox'=>array('delivery_outbox_id','pay_ctry_update_notice_delivery_outbox','delivery_outbox_id')));}
function pay_ctry_007_trigger_codes(){return array('pay_ctry_catalog_releases'=>'cr','pay_ctry_catalog_release_admissions'=>'ca','pay_ctry_update_notices'=>'un','pay_ctry_update_notice_applicability'=>'na','pay_ctry_update_notice_audiences'=>'au','pay_ctry_update_notice_recipient_events'=>'re','pay_ctry_update_notice_acknowledgements'=>'ak','pay_ctry_update_notice_lifecycle_events'=>'le','pay_ctry_update_notice_delivery_outbox'=>'ob','pay_ctry_update_notice_delivery_attempts'=>'da');}
function pay_ctry_007_table_columns($table){$physical=hrm_migration_physical_table_name($table);if($physical===false)return false;$q=db_query('SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($physical).' ORDER BY ORDINAL_POSITION');if(!$q)return false;$o=array();while($r=db_fetch_assoc($q))$o[]=(string)$r['COLUMN_NAME'];return $o;}
function pay_ctry_007_table_primary_key($table){$physical=hrm_migration_physical_table_name($table);if($physical===false)return false;$q=db_query("SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME=".db_escape($physical)." AND INDEX_NAME='PRIMARY' ORDER BY SEQ_IN_INDEX");if(!$q)return false;$o=array();while($r=db_fetch_assoc($q))$o[]=(string)$r['COLUMN_NAME'];return $o;}
function pay_ctry_007_table_engine($table){$physical=hrm_migration_physical_table_name($table);if($physical===false)return false;$q=db_query('SELECT ENGINE FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($physical).' LIMIT 1');if(!$q||db_num_rows($q)!==1)return false;$r=db_fetch_assoc($q);return $r?(string)$r['ENGINE']:false;}
/**
 * Normalize information-schema rows to stable PAY-CTRY-007 index attributes.
 *
 * @param array $rows Rows containing index metadata.
 * @return array|false Index-name map with uniqueness, type, ordered columns and prefix lengths.
 */
function pay_ctry_007_canonical_index_projection(array $rows)
{
    $indexes = array();
    foreach ($rows as $row) {
        if (!isset($row['INDEX_NAME'], $row['NON_UNIQUE'], $row['SEQ_IN_INDEX'], $row['COLUMN_NAME'], $row['INDEX_TYPE'])) {
            if (isset($row['INDEX_NAME']) && strcasecmp((string)$row['INDEX_NAME'], 'PRIMARY') === 0) {
                continue;
            }
            return false;
        }
        $name = (string)$row['INDEX_NAME'];
        if (strcasecmp($name, 'PRIMARY') === 0) {
            continue;
        }
        $sequence = (int)$row['SEQ_IN_INDEX'];
        $nonUnique = (int)$row['NON_UNIQUE'];
        $column = (string)$row['COLUMN_NAME'];
        $type = strtoupper(trim((string)$row['INDEX_TYPE']));
        $prefix = $row['SUB_PART'] === null || $row['SUB_PART'] === '' ? null : (int)$row['SUB_PART'];
        if ($name === '' || $sequence < 1 || ($nonUnique !== 0 && $nonUnique !== 1)
            || $column === '' || $type === '' || ($prefix !== null && $prefix < 1)) {
            return false;
        }
        $unique = $nonUnique === 0 ? 1 : 0;
        if (!isset($indexes[$name])) {
            $indexes[$name] = array('unique'=>$unique, 'type'=>$type, 'columns'=>array(), 'prefixes'=>array());
        } elseif ($indexes[$name]['unique'] !== $unique || $indexes[$name]['type'] !== $type) {
            return false;
        }
        if (isset($indexes[$name]['columns'][$sequence])) {
            return false;
        }
        $indexes[$name]['columns'][$sequence] = $column;
        $indexes[$name]['prefixes'][$sequence] = $prefix;
    }

    ksort($indexes, SORT_STRING);
    $projection = array();
    foreach ($indexes as $name=>$index) {
        ksort($index['columns'], SORT_NUMERIC);
        ksort($index['prefixes'], SORT_NUMERIC);
        $expectedSequence = 1;
        foreach ($index['columns'] as $sequence=>$column) {
            if ((int)$sequence !== $expectedSequence) {
                return false;
            }
            $expectedSequence++;
        }
        $projection[$name] = array(
            $index['unique'],
            $index['type'],
            array_values($index['columns']),
            array_values($index['prefixes'])
        );
    }
    return $projection;
}

function pay_ctry_007_table_indexes($table)
{
    $physical = hrm_migration_physical_table_name($table);
    if ($physical === false) {
        return false;
    }
    $q = db_query('SELECT INDEX_NAME,NON_UNIQUE,SEQ_IN_INDEX,COLUMN_NAME,SUB_PART,INDEX_TYPE'
        .' FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($physical));
    if (!$q) {
        return false;
    }
    $rows = array();
    while ($row = db_fetch_assoc($q)) {
        $rows[] = $row;
    }
    return pay_ctry_007_canonical_index_projection($rows);
}
function pay_ctry_007_table_foreign_keys($table){$physical=hrm_migration_physical_table_name($table);$prefix=hrm_migration_active_table_prefix();if($physical===false||$prefix===false)return false;$q=db_query('SELECT CONSTRAINT_NAME,COLUMN_NAME,REFERENCED_TABLE_NAME,REFERENCED_COLUMN_NAME FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($physical).' AND REFERENCED_TABLE_NAME IS NOT NULL ORDER BY CONSTRAINT_NAME,ORDINAL_POSITION');if(!$q)return false;$o=array();while($r=db_fetch_assoc($q)){$ref=(string)$r['REFERENCED_TABLE_NAME'];if(strpos($ref,$prefix)===0)$ref=substr($ref,strlen($prefix));$o[(string)$r['CONSTRAINT_NAME']]=array((string)$r['COLUMN_NAME'],$ref,(string)$r['REFERENCED_COLUMN_NAME']);}ksort($o);return $o;}
function pay_ctry_007_trigger_name($table,$operation){$codes=pay_ctry_007_trigger_codes();$prefix=hrm_migration_active_table_prefix();return isset($codes[$table])&&$prefix!==false?$prefix.'ct7_'.$codes[$table].'_'.(strtoupper($operation)==='UPDATE'?'u':'d'):false;}
function pay_ctry_007_trigger_message($table,$operation){return 'PAY-CTRY-007 immutable '.$table.' row cannot be '.(strtoupper($operation)==='UPDATE'?'updated':'deleted');}
function pay_ctry_007_trigger_state($table,$operation){$physical=hrm_migration_physical_table_name($table);$name=pay_ctry_007_trigger_name($table,$operation);if($physical===false||$name===false)return false;$q=db_query('SELECT EVENT_MANIPULATION,EVENT_OBJECT_TABLE,ACTION_TIMING,ACTION_STATEMENT FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA=DATABASE() AND TRIGGER_NAME='.db_escape($name).' LIMIT 2');if(!$q)return false;if(db_num_rows($q)===0)return 'absent';if(db_num_rows($q)!==1)return 'conflict';$r=db_fetch_assoc($q);$actual=strtoupper(rtrim(preg_replace('/\s+/',' ',trim((string)$r['ACTION_STATEMENT'])),';'));$expected=strtoupper("SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='".pay_ctry_007_trigger_message($table,$operation)."'");return strtoupper((string)$r['EVENT_MANIPULATION'])===strtoupper($operation)&&(string)$r['EVENT_OBJECT_TABLE']===$physical&&strtoupper((string)$r['ACTION_TIMING'])==='BEFORE'&&$actual===$expected?'valid':'conflict';}
function pay_ctry_007_create_trigger($patch,$table,$operation){$state=pay_ctry_007_trigger_state($table,$operation);if($state==='valid')return true;if($state!=='absent')return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CTRY-007 immutable trigger conflict: '.$table.'.'.$operation);$physical=hrm_migration_physical_table_name($table);$name=pay_ctry_007_trigger_name($table,$operation);$sql='CREATE TRIGGER `'.$name.'` BEFORE '.strtoupper($operation).' ON `'.$physical.'` FOR EACH ROW SIGNAL SQLSTATE \'45000\' SET MESSAGE_TEXT='.db_escape(pay_ctry_007_trigger_message($table,$operation));if(!db_query($sql))return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CTRY-007 immutable trigger creation failed: '.$table.'.'.$operation);return pay_ctry_007_trigger_state($table,$operation)==='valid';}
function pay_ctry_007_schema_group_block($group){global $path_to_root;$files=array('catalog_notice'=>'upgrade_pay_ctry_007_schema.sql','recipient_lifecycle'=>'upgrade_pay_ctry_007_recipient_lifecycle_schema.sql','delivery_outbox'=>'upgrade_pay_ctry_007_delivery_outbox_schema.sql');if(!isset($files[$group]))return false;$raw=@file_get_contents($path_to_root.'/sql/'.$files[$group]);if(!is_string($raw)||$raw==='')return false;$start='-- PAY-CTRY-007 GROUP '.$group;$end='-- PAY-CTRY-007 END GROUP '.$group;$a=strpos($raw,$start);$b=strpos($raw,$end);if($a===false||$b===false||$b<=$a)return false;return substr($raw,$a+strlen($start),$b-($a+strlen($start)));}
function pay_ctry_007_require_table_shape($patch,$table){$cols=pay_ctry_007_schema_columns();$pks=pay_ctry_007_primary_keys();$ix=pay_ctry_007_required_indexes();$fk=pay_ctry_007_required_foreign_keys();if(!isset($cols[$table],$pks[$table],$ix[$table],$fk[$table]))return false;if(pay_ctry_007_table_columns($table)!==$cols[$table]||pay_ctry_007_table_primary_key($table)!==$pks[$table]||strcasecmp((string)pay_ctry_007_table_engine($table),'InnoDB')!==0)return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CTRY-007 table projection mismatch: '.$table);$actual=pay_ctry_007_table_indexes($table);$expected=array();foreach($ix[$table] as $name=>$spec)$expected[$name]=array((int)$spec[0],'BTREE',$spec[1],array_fill(0,count($spec[1]),null));ksort($actual);ksort($expected);if($actual!==$expected)return hrm_migration_fail($patch,HRM_MIGRATION_INDEX_MISMATCH,'PAY-CTRY-007 index projection mismatch: '.$table);$a=pay_ctry_007_table_foreign_keys($table);$b=$fk[$table];ksort($a);ksort($b);if($a!==$b)return hrm_migration_fail($patch,HRM_MIGRATION_FK_MISMATCH,'PAY-CTRY-007 foreign-key projection mismatch: '.$table);foreach(array('UPDATE','DELETE') as $op)if(pay_ctry_007_trigger_state($table,$op)!=='valid')return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CTRY-007 immutable trigger mismatch: '.$table.'.'.$op);return true;}
function pay_ctry_007_install_schema_group($patch,$group){$groups=pay_ctry_007_schema_groups();if(!isset($groups[$group]))return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CTRY-007 schema group unknown: '.$group);$block=pay_ctry_007_schema_group_block($group);if($block===false)return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CTRY-007 schema source missing.');$prefix=hrm_migration_active_table_prefix();if($prefix===false)return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CTRY-007 table prefix invalid.');$statements=preg_split('/;\s*(?:\r?\n|$)/',$block);foreach($statements as $statement){$statement=trim($statement);if($statement===''||strpos($statement,'CREATE TRIGGER')===0)continue;$statement=str_replace('`0_','`'.$prefix,$statement);if(!db_query($statement))return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CTRY-007 schema statement failed.');}foreach($groups[$group] as $table){foreach(array('UPDATE','DELETE') as $op)if(!pay_ctry_007_create_trigger($patch,$table,$op))return false;if(!pay_ctry_007_require_table_shape($patch,$table))return false;}return true;}
function pay_ctry_007_require_schema_group($patch,$group){$groups=pay_ctry_007_schema_groups();if(!isset($groups[$group]))return false;foreach($groups[$group] as $table)if(!pay_ctry_007_require_table_shape($patch,$table))return false;return true;}
?>
