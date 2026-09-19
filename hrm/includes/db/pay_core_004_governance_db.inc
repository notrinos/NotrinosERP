<?php
/**
 * PAY-CORE-004 governed append-only execution-record writers.
 *
 * This file is deliberately not loaded by payroll_engine.inc. It persists
 * immutable custody evidence only; it does not calculate payroll, post GL,
 * execute payments, mutate historical payslips or transfer production authority.
 */
include_once(dirname(dirname(__FILE__)).'/payroll/pay_core_004_contract.inc');
include_once(dirname(__FILE__).'/pay_core_004_schema_db.inc');

function hrm_pay_core_004_actor(){return isset($_SESSION['wa_current_user']->user)?(int)$_SESSION['wa_current_user']->user:0;}
function hrm_pay_core_004_access($permission){return function_exists('user_check_access')&&user_check_access($permission);}
function hrm_pay_core_004_token($value,$max=64){$value=trim((string)$value);return $value!==''&&strlen($value)<=$max&&preg_match('/^[A-Za-z0-9_.:-]+$/D',$value)===1?$value:false;}
function hrm_pay_core_004_manage_transaction(){return empty($GLOBALS['transaction_level']);}

function hrm_pay_core_004_normalize_hash_list($values,&$error=null)
{
    $error=null;
    if(!is_array($values)){$error='hash_list_invalid';return false;}
    $seen=array();$out=array();
    foreach($values as $value){if(!hrm_pay_core_004_is_sha256($value)){$error='hash_list_invalid';return false;}if(isset($seen[$value])){$error='hash_list_duplicate';return false;}$seen[$value]=true;$out[]=$value;}
    sort($out,SORT_STRING);return $out;
}

function hrm_pay_core_004_snapshot_material($scope,$identity_sets,&$error=null)
{
    $error=null;
    $required=array('approved_inputs','element_versions','eligibility','rule_artifacts','source_events','configuration');
    if(!is_array($scope)||!is_array($identity_sets)){$error='snapshot_material_invalid';return false;}
    $sets=array();
    foreach($required as $name){if(!array_key_exists($name,$identity_sets)){$error='snapshot_set_missing';return false;}$sets[$name]=hrm_pay_core_004_normalize_hash_list($identity_sets[$name],$set_error);if($sets[$name]===false){$error=$name.'_'.$set_error;return false;}}
    $descriptor=array(
        'payroll_relationship_id'=>isset($scope['payroll_relationship_id'])?(int)$scope['payroll_relationship_id']:0,
        'assignment_id'=>array_key_exists('assignment_id',$scope)&&$scope['assignment_id']!==null?(int)$scope['assignment_id']:null,
        'calendar_period_id'=>isset($scope['calendar_period_id'])?(int)$scope['calendar_period_id']:0,
        'run_identity'=>isset($scope['run_identity'])?(string)$scope['run_identity']:'',
        'legal_entity_id'=>isset($scope['legal_entity_id'])?(int)$scope['legal_entity_id']:0,
        'currency'=>isset($scope['currency'])?(string)$scope['currency']:'',
        'approved_inputs_sha256'=>hrm_pay_core_004_hash_set($sets['approved_inputs']),
        'element_versions_sha256'=>hrm_pay_core_004_hash_set($sets['element_versions']),
        'eligibility_sha256'=>hrm_pay_core_004_hash_set($sets['eligibility']),
        'rule_artifacts_sha256'=>hrm_pay_core_004_hash_set($sets['rule_artifacts']),
        'source_events_sha256'=>hrm_pay_core_004_hash_set($sets['source_events']),
        'configuration_sha256'=>hrm_pay_core_004_hash_set($sets['configuration']),
    );
    if(!hrm_pay_core_004_validate_snapshot_descriptor($descriptor)){$error='snapshot_descriptor_invalid';return false;}
    $snapshot_sha=hrm_pay_core_004_snapshot_sha256($descriptor);
    $payload=hrm_pay_core_004_canonical_json(array('descriptor'=>$descriptor,'identity_sets'=>$sets));
    if($snapshot_sha===false||$payload===false){$error='snapshot_canonicalization_failed';return false;}
    return array('descriptor'=>$descriptor,'identity_sets'=>$sets,'payload_json'=>$payload,'snapshot_sha256'=>$snapshot_sha);
}

function hrm_pay_core_004_read_one($sql)
{
    $q=db_query($sql);return $q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
}

function hrm_pay_core_004_seal_snapshot($scope,$identity_sets,&$error=null)
{
    $error=null;$actor=hrm_pay_core_004_actor();
    if($actor<=0||!hrm_pay_core_004_access('SA_HRM_SEAL_PAY_CORE_RESULT')){$error='access_denied';return false;}
    $material=hrm_pay_core_004_snapshot_material($scope,$identity_sets,$error);if($material===false)return false;
    $d=$material['descriptor'];$manage=hrm_pay_core_004_manage_transaction();if($manage)begin_transaction();
    $rel=hrm_pay_core_004_read_one('SELECT payroll_relationship_id,legal_entity_id FROM '.TB_PREF.'hrm_payroll_relationships WHERE payroll_relationship_id='.(int)$d['payroll_relationship_id'].' LIMIT 2 FOR UPDATE');
    $per=hrm_pay_core_004_read_one('SELECT calendar_period_id FROM '.TB_PREF.'hrm_payroll_calendar_periods WHERE calendar_period_id='.(int)$d['calendar_period_id'].' LIMIT 2');
    if(!$rel||!$per){if($manage)cancel_transaction();$error='snapshot_scope_missing';return false;}
    if(!empty($rel['legal_entity_id'])&&(int)$rel['legal_entity_id']!==(int)$d['legal_entity_id']){if($manage)cancel_transaction();$error='legal_entity_scope_mismatch';return false;}
    if($d['assignment_id']!==null){$member=hrm_pay_core_004_read_one('SELECT relationship_member_id FROM '.TB_PREF.'hrm_payroll_relationship_members WHERE payroll_relationship_id='.(int)$d['payroll_relationship_id'].' AND assignment_id='.(int)$d['assignment_id'].' LIMIT 2');if(!$member){if($manage)cancel_transaction();$error='assignment_scope_mismatch';return false;}}
    $existing=hrm_pay_core_004_read_one('SELECT snapshot_id,snapshot_payload_json,sealed_by FROM '.TB_PREF.'hrm_pay_core_004_snapshots WHERE snapshot_sha256='.db_escape($material['snapshot_sha256']).' LIMIT 2 FOR UPDATE');
    if($existing){if(!hash_equals((string)$existing['snapshot_payload_json'],$material['payload_json'])){if($manage)cancel_transaction();$error='snapshot_hash_collision';return false;}$id=(int)$existing['snapshot_id'];if($manage)commit_transaction();return $id;}
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_core_004_snapshots (payroll_relationship_id,assignment_id,calendar_period_id,run_identity,legal_entity_id,currency,approved_inputs_sha256,element_versions_sha256,eligibility_sha256,rule_artifacts_sha256,source_events_sha256,configuration_sha256,snapshot_payload_json,snapshot_sha256,sealed_by) VALUES ('
        .(int)$d['payroll_relationship_id'].','.($d['assignment_id']===null?'NULL':(int)$d['assignment_id']).','.(int)$d['calendar_period_id'].','.db_escape($d['run_identity']).','.(int)$d['legal_entity_id'].','.db_escape($d['currency']).','
        .db_escape($d['approved_inputs_sha256']).','.db_escape($d['element_versions_sha256']).','.db_escape($d['eligibility_sha256']).','.db_escape($d['rule_artifacts_sha256']).','.db_escape($d['source_events_sha256']).','.db_escape($d['configuration_sha256']).','.db_escape($material['payload_json']).','.db_escape($material['snapshot_sha256']).','.$actor.')';
    if(!db_query($sql)){if($manage)cancel_transaction();$error='snapshot_insert_failed';return false;}$id=(int)db_insert_id();if($manage)commit_transaction();return $id;
}

function hrm_pay_core_004_result_component_hashes($lines,$traces,&$error=null)
{
    $error=null;if(!is_array($lines)||!is_array($traces)){$error='result_components_invalid';return false;}
    $line_hashes=array();$line_nos=array();foreach($lines as $line){$h=hrm_pay_core_004_result_line_sha256($line);if($h===false){$error='result_line_invalid';return false;}$n=(int)$line['line_no'];if(isset($line_nos[$n])){$error='duplicate_line_no';return false;}$line_nos[$n]=true;$line_hashes[]=$h;}
    $trace_hashes=array();$trace_seqs=array();foreach($traces as $trace){$h=hrm_pay_core_004_trace_sha256($trace);if($h===false){$error='trace_invalid';return false;}$n=(int)$trace['trace_seq'];if(isset($trace_seqs[$n])){$error='duplicate_trace_seq';return false;}$trace_seqs[$n]=true;$trace_hashes[]=$h;}
    return array('line_hashes'=>$line_hashes,'trace_hashes'=>$trace_hashes,'line_set_sha256'=>hrm_pay_core_004_hash_set($line_hashes),'trace_set_sha256'=>hrm_pay_core_004_hash_set($trace_hashes));
}

function hrm_pay_core_004_replay_matches($result_id,$result_sha,$line_set_sha,$trace_set_sha)
{
    $r=hrm_pay_core_004_read_one('SELECT result_sha256,line_set_sha256,trace_set_sha256 FROM '.TB_PREF.'hrm_pay_core_004_results WHERE result_id='.(int)$result_id.' LIMIT 2');
    if(!$r||!hash_equals((string)$r['result_sha256'],$result_sha)||!hash_equals((string)$r['line_set_sha256'],$line_set_sha)||!hash_equals((string)$r['trace_set_sha256'],$trace_set_sha))return false;
    $lh=array();$q=db_query('SELECT line_sha256 FROM '.TB_PREF.'hrm_pay_core_004_result_lines WHERE result_id='.(int)$result_id);if(!$q)return false;while($x=db_fetch_assoc($q))$lh[]=(string)$x['line_sha256'];
    $th=array();$q=db_query('SELECT trace_sha256 FROM '.TB_PREF.'hrm_pay_core_004_traces WHERE result_id='.(int)$result_id);if(!$q)return false;while($x=db_fetch_assoc($q))$th[]=(string)$x['trace_sha256'];
    return hash_equals($line_set_sha,(string)hrm_pay_core_004_hash_set($lh))&&hash_equals($trace_set_sha,(string)hrm_pay_core_004_hash_set($th));
}

function hrm_pay_core_004_append_result($snapshot_id,$totals,$lines,$traces,&$error=null)
{
    $error=null;$executor=hrm_pay_core_004_actor();$snapshot_id=(int)$snapshot_id;
    if($executor<=0||!hrm_pay_core_004_access('SA_HRM_WRITE_PAY_CORE_RESULT')){$error='access_denied';return false;}
    $components=hrm_pay_core_004_result_component_hashes($lines,$traces,$error);if($components===false)return false;
    $manage=hrm_pay_core_004_manage_transaction();if($manage)begin_transaction();
    $snapshot=hrm_pay_core_004_read_one('SELECT snapshot_sha256,currency FROM '.TB_PREF.'hrm_pay_core_004_snapshots WHERE snapshot_id='.$snapshot_id.' LIMIT 2 FOR UPDATE');if(!$snapshot){if($manage)cancel_transaction();$error='snapshot_missing';return false;}
    $descriptor=array('snapshot_sha256'=>$snapshot['snapshot_sha256'],'currency'=>isset($totals['currency'])?(string)$totals['currency']:'','gross_amount'=>isset($totals['gross_amount'])?$totals['gross_amount']:null,'deduction_amount'=>isset($totals['deduction_amount'])?$totals['deduction_amount']:null,'net_amount'=>isset($totals['net_amount'])?$totals['net_amount']:null,'line_set_sha256'=>$components['line_set_sha256'],'trace_set_sha256'=>$components['trace_set_sha256']);
    if(!hrm_pay_core_004_validate_result_descriptor($descriptor)||$descriptor['currency']!==$snapshot['currency']){if($manage)cancel_transaction();$error='result_descriptor_invalid';return false;}
    $result_sha=hrm_pay_core_004_result_sha256($descriptor);$existing=hrm_pay_core_004_read_one('SELECT result_id,executor_id FROM '.TB_PREF.'hrm_pay_core_004_results WHERE result_sha256='.db_escape($result_sha).' LIMIT 2 FOR UPDATE');
    if($existing){$id=(int)$existing['result_id'];if((int)$existing['executor_id']!==$executor||!hrm_pay_core_004_replay_matches($id,$result_sha,$components['line_set_sha256'],$components['trace_set_sha256'])){if($manage)cancel_transaction();$error='result_replay_mismatch';return false;}if($manage)commit_transaction();return $id;}
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_core_004_results (snapshot_id,snapshot_sha256,currency,gross_amount,deduction_amount,net_amount,line_set_sha256,trace_set_sha256,result_sha256,executor_id) VALUES ('.$snapshot_id.','.db_escape($descriptor['snapshot_sha256']).','.db_escape($descriptor['currency']).','.db_escape((string)$descriptor['gross_amount']).','.db_escape((string)$descriptor['deduction_amount']).','.db_escape((string)$descriptor['net_amount']).','.db_escape($descriptor['line_set_sha256']).','.db_escape($descriptor['trace_set_sha256']).','.db_escape($result_sha).','.$executor.')';
    if(!db_query($sql)){if($manage)cancel_transaction();$error='result_insert_failed';return false;}$result_id=(int)db_insert_id();
    foreach($lines as $i=>$line){$hash=$components['line_hashes'][$i];$sql='INSERT INTO '.TB_PREF.'hrm_pay_core_004_result_lines (result_id,line_no,element_version_id,line_kind,currency,amount,quantity,rate,source_sha256,line_sha256) VALUES ('.$result_id.','.(int)$line['line_no'].','.(int)$line['element_version_id'].','.db_escape($line['line_kind']).','.db_escape($line['currency']).','.db_escape((string)$line['amount']).','.db_escape((string)$line['quantity']).','.db_escape((string)$line['rate']).','.db_escape($line['source_sha256']).','.db_escape($hash).')';if(!db_query($sql)){if($manage)cancel_transaction();$error='result_line_insert_failed';return false;}}
    foreach($traces as $i=>$trace){$hash=$components['trace_hashes'][$i];$digest=isset($trace['value_digest'])&&$trace['value_digest']!==''?db_escape($trace['value_digest']):'NULL';$sql='INSERT INTO '.TB_PREF.'hrm_pay_core_004_traces (result_id,trace_seq,trace_kind,artifact_sha256,input_sha256,output_sha256,value_mode,value_digest,trace_sha256) VALUES ('.$result_id.','.(int)$trace['trace_seq'].','.db_escape($trace['trace_kind']).','.db_escape($trace['artifact_sha256']).','.db_escape($trace['input_sha256']).','.db_escape($trace['output_sha256']).','.db_escape($trace['value_mode']).','.$digest.','.db_escape($hash).')';if(!db_query($sql)){if($manage)cancel_transaction();$error='trace_insert_failed';return false;}}
    if($manage)commit_transaction();return $result_id;
}

function hrm_pay_core_004_recompute_integrity($result_id,&$error=null)
{
    $error=null;$result_id=(int)$result_id;
    $q=db_query('SELECT r.*,s.snapshot_payload_json,s.snapshot_sha256 stored_snapshot_sha FROM '.TB_PREF.'hrm_pay_core_004_results r INNER JOIN '.TB_PREF.'hrm_pay_core_004_snapshots s ON s.snapshot_id=r.snapshot_id WHERE r.result_id='.$result_id.' LIMIT 2');
    $r=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;if(!$r){$error='result_missing';return false;}
    $payload=json_decode($r['snapshot_payload_json'],true);if(!is_array($payload)||!isset($payload['descriptor'],$payload['identity_sets'])||!is_array($payload['descriptor'])||!is_array($payload['identity_sets'])){$error='snapshot_payload_invalid';return false;}
    $sets=$payload['identity_sets'];foreach(array('approved_inputs','element_versions','eligibility','rule_artifacts','source_events','configuration') as $name){if(!isset($sets[$name])||hrm_pay_core_004_hash_set($sets[$name])===false){$error='snapshot_set_invalid';return false;}}
    $d=$payload['descriptor'];$expected_map=array('approved_inputs_sha256'=>'approved_inputs','element_versions_sha256'=>'element_versions','eligibility_sha256'=>'eligibility','rule_artifacts_sha256'=>'rule_artifacts','source_events_sha256'=>'source_events','configuration_sha256'=>'configuration');foreach($expected_map as $field=>$set)if(!isset($d[$field])||!hash_equals((string)$d[$field],(string)hrm_pay_core_004_hash_set($sets[$set]))){$error='snapshot_set_hash_mismatch';return false;}
    $snapshot_sha=hrm_pay_core_004_snapshot_sha256($d);if($snapshot_sha===false||!hash_equals($snapshot_sha,(string)$r['stored_snapshot_sha'])||!hash_equals($snapshot_sha,(string)$r['snapshot_sha256'])){$error='snapshot_integrity_mismatch';return false;}
    $lh=array();$q=db_query('SELECT line_no,element_version_id,line_kind,currency,amount,quantity,rate,source_sha256,line_sha256 FROM '.TB_PREF.'hrm_pay_core_004_result_lines WHERE result_id='.$result_id.' ORDER BY line_no');if(!$q){$error='line_read_failed';return false;}while($x=db_fetch_assoc($q)){$calc=hrm_pay_core_004_result_line_sha256($x);if($calc===false||!hash_equals($calc,(string)$x['line_sha256'])){$error='line_integrity_mismatch';return false;}$lh[]=$calc;}
    $th=array();$q=db_query('SELECT trace_seq,trace_kind,artifact_sha256,input_sha256,output_sha256,value_mode,value_digest,trace_sha256 FROM '.TB_PREF.'hrm_pay_core_004_traces WHERE result_id='.$result_id.' ORDER BY trace_seq');if(!$q){$error='trace_read_failed';return false;}while($x=db_fetch_assoc($q)){$calc=hrm_pay_core_004_trace_sha256($x);if($calc===false||!hash_equals($calc,(string)$x['trace_sha256'])){$error='trace_integrity_mismatch';return false;}$th[]=$calc;}
    $line_set=hrm_pay_core_004_hash_set($lh);$trace_set=hrm_pay_core_004_hash_set($th);if(!hash_equals($line_set,(string)$r['line_set_sha256'])||!hash_equals($trace_set,(string)$r['trace_set_sha256'])){$error='component_set_mismatch';return false;}
    $rd=array('snapshot_sha256'=>$snapshot_sha,'currency'=>$r['currency'],'gross_amount'=>$r['gross_amount'],'deduction_amount'=>$r['deduction_amount'],'net_amount'=>$r['net_amount'],'line_set_sha256'=>$line_set,'trace_set_sha256'=>$trace_set);$result_sha=hrm_pay_core_004_result_sha256($rd);if($result_sha===false||!hash_equals($result_sha,(string)$r['result_sha256'])){$error='result_integrity_mismatch';return false;}
    return array('snapshot_sha256'=>$snapshot_sha,'result_sha256'=>$result_sha,'line_set_sha256'=>$line_set,'trace_set_sha256'=>$trace_set,'executor_id'=>(int)$r['executor_id'],'snapshot_id'=>(int)$r['snapshot_id']);
}

function hrm_pay_core_004_record_integrity($result_id,$verification_status,$evidence_sha256,&$error=null)
{
    $error=null;$reviewer=hrm_pay_core_004_actor();$result_id=(int)$result_id;
    if($reviewer<=0||!hrm_pay_core_004_access('SA_HRM_VERIFY_PAY_CORE_RESULT')||!in_array($verification_status,array('verified','failed'),true)||!hrm_pay_core_004_is_sha256($evidence_sha256)){$error='access_or_evidence_invalid';return false;}
    $manage=hrm_pay_core_004_manage_transaction();if($manage)begin_transaction();$integrity=hrm_pay_core_004_recompute_integrity($result_id,$error);if($integrity===false){if($manage)cancel_transaction();return false;}if($reviewer===$integrity['executor_id']){if($manage)cancel_transaction();$error='independent_verifier_required';return false;}
    $q=db_query('SELECT COUNT(*) n FROM '.TB_PREF.'hrm_pay_core_004_results WHERE snapshot_id='.(int)$integrity['snapshot_id'].' AND result_id<'.$result_id);$row=$q?db_fetch_assoc($q):false;if(!$row){if($manage)cancel_transaction();$error='result_history_read_failed';return false;}if((int)$row['n']>0){$lineage=hrm_pay_core_004_read_one('SELECT lineage_id FROM '.TB_PREF.'hrm_pay_core_004_correction_lineage WHERE successor_result_id='.$result_id.' LIMIT 2');if(!$lineage){if($manage)cancel_transaction();$error='correction_lineage_required';return false;}}
    $integrity_sha=hrm_pay_core_004_hash(array('result_id'=>$result_id,'snapshot_sha256'=>$integrity['snapshot_sha256'],'result_sha256'=>$integrity['result_sha256'],'line_set_sha256'=>$integrity['line_set_sha256'],'trace_set_sha256'=>$integrity['trace_set_sha256'],'verification_status'=>$verification_status,'evidence_sha256'=>$evidence_sha256,'verified_by'=>$reviewer));
    $existing=hrm_pay_core_004_read_one('SELECT integrity_evidence_id FROM '.TB_PREF.'hrm_pay_core_004_integrity_evidence WHERE integrity_sha256='.db_escape($integrity_sha).' LIMIT 2 FOR UPDATE');if($existing){$id=(int)$existing['integrity_evidence_id'];if($manage)commit_transaction();return $id;}
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_core_004_integrity_evidence (result_id,snapshot_sha256,result_sha256,line_set_sha256,trace_set_sha256,verification_status,evidence_sha256,integrity_sha256,verified_by) VALUES ('.$result_id.','.db_escape($integrity['snapshot_sha256']).','.db_escape($integrity['result_sha256']).','.db_escape($integrity['line_set_sha256']).','.db_escape($integrity['trace_set_sha256']).','.db_escape($verification_status).','.db_escape($evidence_sha256).','.db_escape($integrity_sha).','.$reviewer.')';if(!db_query($sql)){if($manage)cancel_transaction();$error='integrity_insert_failed';return false;}$id=(int)db_insert_id();if($manage)commit_transaction();return $id;
}

function hrm_pay_core_004_record_lineage($predecessor_result_id,$successor_result_id,$lineage_kind,$reason_code,$approval_evidence_sha256,&$error=null)
{
    $error=null;$checker=hrm_pay_core_004_actor();$predecessor_result_id=(int)$predecessor_result_id;$successor_result_id=(int)$successor_result_id;$reason=hrm_pay_core_004_token($reason_code,64);
    if($checker<=0||!hrm_pay_core_004_access('SA_HRM_REVIEW_PAY_CORE')||$predecessor_result_id<=0||$successor_result_id<=0||$predecessor_result_id===$successor_result_id||!in_array($lineage_kind,hrm_pay_core_004_lineage_kinds(),true)||$reason===false||!hrm_pay_core_004_is_sha256($approval_evidence_sha256)){$error='lineage_input_invalid';return false;}
    $manage=hrm_pay_core_004_manage_transaction();if($manage)begin_transaction();
    $sql='SELECT r.result_id,r.executor_id,r.result_sha256,s.payroll_relationship_id,s.calendar_period_id,s.legal_entity_id,s.currency FROM '.TB_PREF.'hrm_pay_core_004_results r INNER JOIN '.TB_PREF.'hrm_pay_core_004_snapshots s ON s.snapshot_id=r.snapshot_id WHERE r.result_id IN ('.$predecessor_result_id.','.$successor_result_id.') ORDER BY r.result_id FOR UPDATE';$q=db_query($sql);$rows=array();if($q)while($x=db_fetch_assoc($q))$rows[(int)$x['result_id']]=$x;if(count($rows)!==2){if($manage)cancel_transaction();$error='lineage_result_missing';return false;}$a=$rows[$predecessor_result_id];$b=$rows[$successor_result_id];foreach(array('payroll_relationship_id','calendar_period_id','legal_entity_id','currency') as $field)if((string)$a[$field]!== (string)$b[$field]){if($manage)cancel_transaction();$error='lineage_scope_mismatch';return false;}if($checker===(int)$b['executor_id']){if($manage)cancel_transaction();$error='checker_executor_separation_required';return false;}
    $sha=hrm_pay_core_004_hash(array('predecessor_result_sha256'=>$a['result_sha256'],'successor_result_sha256'=>$b['result_sha256'],'lineage_kind'=>$lineage_kind,'reason_code'=>$reason,'approval_evidence_sha256'=>$approval_evidence_sha256,'checker_id'=>$checker));
    $existing=hrm_pay_core_004_read_one('SELECT lineage_id,lineage_sha256 FROM '.TB_PREF.'hrm_pay_core_004_correction_lineage WHERE successor_result_id='.$successor_result_id.' LIMIT 2 FOR UPDATE');if($existing){if(!hash_equals((string)$existing['lineage_sha256'],$sha)){if($manage)cancel_transaction();$error='successor_lineage_already_bound';return false;}$id=(int)$existing['lineage_id'];if($manage)commit_transaction();return $id;}
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_core_004_correction_lineage (predecessor_result_id,successor_result_id,lineage_kind,reason_code,approval_evidence_sha256,lineage_sha256,checker_id) VALUES ('.$predecessor_result_id.','.$successor_result_id.','.db_escape($lineage_kind).','.db_escape($reason).','.db_escape($approval_evidence_sha256).','.db_escape($sha).','.$checker.')';if(!db_query($sql)){if($manage)cancel_transaction();$error='lineage_insert_failed';return false;}$id=(int)db_insert_id();if($manage)commit_transaction();return $id;
}

function hrm_pay_core_004_record_legacy_provenance($source_kind,$source_id,$source_sha256,$provenance_state,$gap_codes,$evidence_sha256,&$error=null)
{
    $error=null;$actor=hrm_pay_core_004_actor();$source_kind=hrm_pay_core_004_token($source_kind,32);$source_id=(int)$source_id;
    if($actor<=0||!hrm_pay_core_004_access('SA_HRM_REVIEW_PAY_CORE')||$source_kind===false||$source_id<=0||!hrm_pay_core_004_is_sha256($source_sha256)||!in_array($provenance_state,hrm_pay_core_004_legacy_provenance_states(),true)||!is_array($gap_codes)||!hrm_pay_core_004_is_sha256($evidence_sha256)){$error='legacy_provenance_invalid';return false;}
    $codes=array();foreach($gap_codes as $code){$t=hrm_pay_core_004_token($code,64);if($t===false){$error='legacy_gap_code_invalid';return false;}$codes[$t]=true;}$codes=array_keys($codes);sort($codes,SORT_STRING);if($provenance_state==='complete'&&count($codes)>0){$error='complete_provenance_cannot_have_gaps';return false;}if($provenance_state!=='complete'&&count($codes)===0){$error='provenance_gap_required';return false;}$gap_json=json_encode($codes,JSON_UNESCAPED_SLASHES);$sha=hrm_pay_core_004_hash(array('legacy_source_kind'=>$source_kind,'legacy_source_id'=>$source_id,'legacy_source_sha256'=>$source_sha256,'provenance_state'=>$provenance_state,'gap_codes'=>$codes,'evidence_sha256'=>$evidence_sha256,'recorded_by'=>$actor));
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_core_004_legacy_provenance_gaps (legacy_source_kind,legacy_source_id,legacy_source_sha256,provenance_state,gap_codes,evidence_sha256,provenance_sha256,recorded_by) VALUES ('.db_escape($source_kind).','.$source_id.','.db_escape($source_sha256).','.db_escape($provenance_state).','.db_escape($gap_json).','.db_escape($evidence_sha256).','.db_escape($sha).','.$actor.')';if(!db_query($sql)){ $existing=hrm_pay_core_004_read_one('SELECT legacy_provenance_id FROM '.TB_PREF.'hrm_pay_core_004_legacy_provenance_gaps WHERE provenance_sha256='.db_escape($sha).' LIMIT 2');if($existing)return (int)$existing['legacy_provenance_id'];$error='legacy_provenance_insert_failed';return false;}return (int)db_insert_id();
}

function hrm_pay_core_004_verified_integrity($integrity_evidence_id,$result_id)
{
    return hrm_pay_core_004_read_one('SELECT integrity_evidence_id,verified_by FROM '.TB_PREF.'hrm_pay_core_004_integrity_evidence WHERE integrity_evidence_id='.(int)$integrity_evidence_id.' AND result_id='.(int)$result_id." AND verification_status='verified' LIMIT 2");
}

function hrm_pay_core_004_record_recovery($result_id,$integrity_evidence_id,$recovery_state,$action_token,$evidence_sha256,&$error=null)
{
    $error=null;$reviewer=hrm_pay_core_004_actor();$result_id=(int)$result_id;$integrity_evidence_id=(int)$integrity_evidence_id;$action=hrm_pay_core_004_token($action_token,64);
    if($reviewer<=0||!hrm_pay_core_004_access('SA_HRM_RECOVER_PAY_CORE_RESULT')||!in_array($recovery_state,hrm_pay_core_004_recovery_states(),true)||$action===false||!hrm_pay_core_004_is_sha256($evidence_sha256)){$error='recovery_input_invalid';return false;}
    $manage=hrm_pay_core_004_manage_transaction();if($manage)begin_transaction();$integrity=hrm_pay_core_004_verified_integrity($integrity_evidence_id,$result_id);if(!$integrity){if($manage)cancel_transaction();$error='verified_integrity_required';return false;}
    $sha=hrm_pay_core_004_hash(array('result_id'=>$result_id,'integrity_evidence_id'=>$integrity_evidence_id,'recovery_state'=>$recovery_state,'action_token'=>$action,'evidence_sha256'=>$evidence_sha256,'reviewer_id'=>$reviewer));$existing=hrm_pay_core_004_read_one('SELECT recovery_evidence_id FROM '.TB_PREF.'hrm_pay_core_004_recovery_evidence WHERE recovery_sha256='.db_escape($sha).' LIMIT 2 FOR UPDATE');if($existing){$id=(int)$existing['recovery_evidence_id'];if($manage)commit_transaction();return $id;}
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_core_004_recovery_evidence (result_id,integrity_evidence_id,recovery_state,action_token,evidence_sha256,recovery_sha256,reviewer_id) VALUES ('.$result_id.','.$integrity_evidence_id.','.db_escape($recovery_state).','.db_escape($action).','.db_escape($evidence_sha256).','.db_escape($sha).','.$reviewer.')';if(!db_query($sql)){if($manage)cancel_transaction();$error='recovery_insert_failed';return false;}$id=(int)db_insert_id();if($manage)commit_transaction();return $id;
}

function hrm_pay_core_004_record_archive($result_id,$integrity_evidence_id,$archive_state,$archive_locator_sha256,$archive_payload_sha256,$evidence_sha256,&$error=null)
{
    $error=null;$reviewer=hrm_pay_core_004_actor();$result_id=(int)$result_id;$integrity_evidence_id=(int)$integrity_evidence_id;
    if($reviewer<=0||!hrm_pay_core_004_access('SA_HRM_ARCHIVE_PAY_CORE_RESULT')||!in_array($archive_state,hrm_pay_core_004_archive_states(),true)||!hrm_pay_core_004_is_sha256($archive_locator_sha256)||!hrm_pay_core_004_is_sha256($archive_payload_sha256)||!hrm_pay_core_004_is_sha256($evidence_sha256)){$error='archive_input_invalid';return false;}
    $manage=hrm_pay_core_004_manage_transaction();if($manage)begin_transaction();$integrity=hrm_pay_core_004_verified_integrity($integrity_evidence_id,$result_id);if(!$integrity){if($manage)cancel_transaction();$error='verified_integrity_required';return false;}
    $sha=hrm_pay_core_004_hash(array('result_id'=>$result_id,'integrity_evidence_id'=>$integrity_evidence_id,'archive_state'=>$archive_state,'archive_locator_sha256'=>$archive_locator_sha256,'archive_payload_sha256'=>$archive_payload_sha256,'evidence_sha256'=>$evidence_sha256,'reviewer_id'=>$reviewer));$existing=hrm_pay_core_004_read_one('SELECT archive_evidence_id FROM '.TB_PREF.'hrm_pay_core_004_archive_evidence WHERE archive_sha256='.db_escape($sha).' LIMIT 2 FOR UPDATE');if($existing){$id=(int)$existing['archive_evidence_id'];if($manage)commit_transaction();return $id;}
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_core_004_archive_evidence (result_id,integrity_evidence_id,archive_state,archive_locator_sha256,archive_payload_sha256,evidence_sha256,archive_sha256,reviewer_id) VALUES ('.$result_id.','.$integrity_evidence_id.','.db_escape($archive_state).','.db_escape($archive_locator_sha256).','.db_escape($archive_payload_sha256).','.db_escape($evidence_sha256).','.db_escape($sha).','.$reviewer.')';if(!db_query($sql)){if($manage)cancel_transaction();$error='archive_insert_failed';return false;}$id=(int)db_insert_id();if($manage)commit_transaction();return $id;
}

function hrm_pay_core_004_read_trace_projection($result_id,$payroll_relationship_id,&$error=null)
{
    $error=null;$result_id=(int)$result_id;$payroll_relationship_id=(int)$payroll_relationship_id;
    if(!hrm_pay_core_004_access('SA_HRM_VIEW_PAY_CORE')||$result_id<=0||$payroll_relationship_id<=0){$error='access_denied';return false;}
    $scope=hrm_pay_core_004_read_one('SELECT s.payroll_relationship_id FROM '.TB_PREF.'hrm_pay_core_004_results r INNER JOIN '.TB_PREF.'hrm_pay_core_004_snapshots s ON s.snapshot_id=r.snapshot_id WHERE r.result_id='.$result_id.' LIMIT 2');if(!$scope||(int)$scope['payroll_relationship_id']!==$payroll_relationship_id){$error='object_scope_denied';return false;}
    $can_digest=hrm_pay_core_004_access('SA_HRM_VIEW_PAY_CORE_TRACE_DIGEST')&&hrm_pay_core_004_access('SA_HRM_VIEW_SALARY');$q=db_query('SELECT trace_seq,trace_kind,artifact_sha256,input_sha256,output_sha256,value_mode,value_digest,trace_sha256 FROM '.TB_PREF.'hrm_pay_core_004_traces WHERE result_id='.$result_id.' ORDER BY trace_seq');if(!$q){$error='trace_read_failed';return false;}$out=array();while($x=db_fetch_assoc($q)){if(!$can_digest)$x['value_digest']=null;$out[]=$x;}return $out;
}
