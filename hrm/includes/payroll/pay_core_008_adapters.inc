<?php
/** Read-only adapters from immutable PAY-CORE predecessors into PAY-CORE-008. */
include_once(dirname(__FILE__).'/pay_core_008_contract.inc');

function hrm_pay_core_008_db_one($sql)
{
    $r=db_query($sql);if(!$r)return false;
    $x=db_fetch_assoc($r);if(!$x||db_fetch_assoc($r))return false;
    return $x;
}

/**
 * Load and independently verify an immutable PAY-CORE-004 result together with
 * the exact snapshot scope that produced it. No mutable payroll projection is read.
 */
function hrm_pay_core_008_load_result($result_id,&$error=null)
{
    $error=null;$id=(int)$result_id;
    if($id<=0){$error='result_id_invalid';return false;}
    $r=hrm_pay_core_008_db_one(
        'SELECT r.result_id,r.snapshot_id,r.snapshot_sha256,r.currency,r.gross_amount,r.deduction_amount,r.net_amount,r.line_set_sha256,r.trace_set_sha256,r.result_sha256,r.executor_id,' .
        's.payroll_relationship_id,s.assignment_id,s.calendar_period_id,s.run_identity,s.legal_entity_id,s.currency snapshot_currency,s.approved_inputs_sha256,s.element_versions_sha256,s.eligibility_sha256,s.rule_artifacts_sha256,s.source_events_sha256,s.configuration_sha256,s.snapshot_sha256 sealed_snapshot_sha256 ' .
        'FROM '.TB_PREF.'hrm_pay_core_004_results r INNER JOIN '.TB_PREF.'hrm_pay_core_004_snapshots s ON s.snapshot_id=r.snapshot_id WHERE r.result_id='.$id.' LIMIT 2'
    );
    if(!$r||!hrm_pay_core_008_is_sha256($r['result_sha256'])){$error='result_missing';return false;}
    $snapshot=array(
        'payroll_relationship_id'=>(int)$r['payroll_relationship_id'],
        'assignment_id'=>$r['assignment_id']===null?null:(int)$r['assignment_id'],
        'calendar_period_id'=>(int)$r['calendar_period_id'],
        'run_identity'=>$r['run_identity'],
        'legal_entity_id'=>(int)$r['legal_entity_id'],
        'currency'=>$r['snapshot_currency'],
        'approved_inputs_sha256'=>$r['approved_inputs_sha256'],
        'element_versions_sha256'=>$r['element_versions_sha256'],
        'eligibility_sha256'=>$r['eligibility_sha256'],
        'rule_artifacts_sha256'=>$r['rule_artifacts_sha256'],
        'source_events_sha256'=>$r['source_events_sha256'],
        'configuration_sha256'=>$r['configuration_sha256']
    );
    $snapshot_sha=hrm_pay_core_004_snapshot_sha256($snapshot);
    if($snapshot_sha===false||!hash_equals((string)$r['sealed_snapshot_sha256'],$snapshot_sha)||!hash_equals((string)$r['snapshot_sha256'],$snapshot_sha)){$error='snapshot_hash_drift';return false;}
    if((string)$r['currency']!==(string)$r['snapshot_currency']){$error='result_snapshot_currency_mismatch';return false;}
    $lines=array();
    $q=db_query('SELECT line_no,element_version_id,line_kind,currency,amount,quantity,rate,source_sha256,line_sha256 FROM '.TB_PREF.'hrm_pay_core_004_result_lines WHERE result_id='.$id.' ORDER BY line_no,result_line_id');
    if(!$q){$error='result_lines_read_failed';return false;}
    while($x=db_fetch_assoc($q)){
        $copy=$x;unset($copy['line_sha256']);
        $h=hrm_pay_core_004_result_line_sha256($copy);
        if($h===false||!hash_equals((string)$x['line_sha256'],$h)){$error='result_line_hash_drift';return false;}
        $lines[]=$copy;
    }
    $desc=array(
        'snapshot_sha256'=>$r['snapshot_sha256'],'currency'=>$r['currency'],
        'gross_amount'=>$r['gross_amount'],'deduction_amount'=>$r['deduction_amount'],'net_amount'=>$r['net_amount'],
        'line_set_sha256'=>$r['line_set_sha256'],'trace_set_sha256'=>$r['trace_set_sha256']
    );
    if(!hash_equals((string)$r['result_sha256'],(string)hrm_pay_core_004_result_sha256($desc))){$error='result_hash_drift';return false;}
    return array(
        'result_id'=>$id,'snapshot_id'=>(int)$r['snapshot_id'],'result_sha256'=>$r['result_sha256'],'executor_id'=>(int)$r['executor_id'],
        'scope'=>array('legal_entity_id'=>(int)$r['legal_entity_id'],'payroll_relationship_id'=>(int)$r['payroll_relationship_id'],'worker_id'=>null,'person_id'=>null,'assignment_id'=>$r['assignment_id']===null?null:(int)$r['assignment_id'],'calendar_period_id'=>(int)$r['calendar_period_id']),
        'descriptor'=>$desc,'lines'=>$lines
    );
}

function hrm_pay_core_008_load_result_lineage($result_id,&$error=null)
{
    $error=null;$id=(int)$result_id;if($id<=0){$error='result_id_invalid';return false;}
    $q=db_query('SELECT lineage_id,predecessor_result_id,successor_result_id,lineage_kind,reason_code,evidence_sha256,lineage_sha256 FROM '.TB_PREF.'hrm_pay_core_004_correction_lineage WHERE predecessor_result_id='.$id.' OR successor_result_id='.$id.' ORDER BY lineage_id');
    if(!$q){$error='result_lineage_read_failed';return false;}
    $o=array();while($x=db_fetch_assoc($q))$o[]=$x;return $o;
}

function hrm_pay_core_008_load_balance_movements($definition_sha256,$legal_entity_id,$payroll_relationship_id,&$error=null)
{
    $error=null;if(!hrm_pay_core_008_is_sha256($definition_sha256)||(int)$legal_entity_id<=0||(int)$payroll_relationship_id<=0){$error='balance_scope_invalid';return false;}
    $q=db_query('SELECT definition_sha256,legal_entity_id,payroll_relationship_id,person_id,worker_id,assignment_id,calendar_period_id,jurisdiction_id,leave_type_id,loan_id,source_system,source_type,source_event_key,source_event_sha256,movement_kind,effective_date,recognition_date,currency_code,unit_code,amount,predecessor_movement_sha256,movement_sha256 FROM '.TB_PREF.'hrm_pay_balance_movements WHERE definition_sha256='.db_escape($definition_sha256).' AND legal_entity_id='.(int)$legal_entity_id.' AND payroll_relationship_id='.(int)$payroll_relationship_id.' ORDER BY recognition_date,effective_date,movement_id');
    if(!$q){$error='balance_movement_read_failed';return false;}
    $o=array();while($x=db_fetch_assoc($q)){
        $scope=array('legal_entity_id'=>(int)$x['legal_entity_id'],'payroll_relationship_id'=>(int)$x['payroll_relationship_id'],'person_id'=>$x['person_id']===null?null:(int)$x['person_id'],'worker_id'=>$x['worker_id']===null?null:(int)$x['worker_id'],'assignment_id'=>$x['assignment_id']===null?null:(int)$x['assignment_id'],'calendar_period_id'=>$x['calendar_period_id']===null?null:(int)$x['calendar_period_id'],'jurisdiction_id'=>$x['jurisdiction_id']===null?null:(int)$x['jurisdiction_id'],'leave_type_id'=>$x['leave_type_id']===null?null:(int)$x['leave_type_id'],'loan_id'=>$x['loan_id']===null?null:(int)$x['loan_id']);
        $m=array('definition_sha256'=>$x['definition_sha256'],'scope'=>$scope,'source_system'=>$x['source_system'],'source_type'=>$x['source_type'],'source_event_key'=>$x['source_event_key'],'source_event_sha256'=>$x['source_event_sha256'],'movement_kind'=>$x['movement_kind'],'effective_date'=>$x['effective_date'],'recognition_date'=>$x['recognition_date'],'currency_code'=>$x['currency_code'],'unit_code'=>$x['unit_code'],'amount'=>$x['amount'],'predecessor_movement_sha256'=>$x['predecessor_movement_sha256']);
        $v=hrm_pay_core_006_movement($m,$me);if($v===false||!hash_equals((string)$x['movement_sha256'],$v['movement_sha256'])){$error='balance_movement_hash_drift';return false;}$o[]=$m;
    }
    return $o;
}
?>
