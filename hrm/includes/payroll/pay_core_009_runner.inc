<?php
/** PAY-CORE-009 persisted synchronous chunk runner. */
include_once(dirname(__FILE__).'/pay_core_009_adapter.inc');
include_once(dirname(__FILE__).'/../db/pay_core_009_governance_db.inc');

/** Reverify normalized immutable scope before calculating one claimed employee. */
function hrm_pay_core_009_reverify_item_scope(array $run,array $item,&$error=null)
{
    $error=null;$scope=hrm_pay_core_009_resolve_employee_scope($item['employee_id'],$run['from_date'],$run['to_date'],false,$se);if($scope===false){$error=$se;return false;}
    foreach(array('legal_entity_id','payroll_relationship_id','worker_id','person_id','assignment_id') as $k)if((int)$scope[$k]!== (int)$item[$k]){$error='stale_object_scope';return false;}
    $stored_calendar=$item['calendar_period_id']===null?null:(int)$item['calendar_period_id'];if($scope['calendar_period_id']!==$stored_calendar){$error='stale_calendar_scope';return false;}
    $input=hrm_pay_core_009_item_input_sha($item['employee_id'],$scope,$run['from_date'],$run['to_date']);if(!hash_equals((string)$item['input_sha256'],$input)){$error='stale_input_predecessor';return false;}return $scope;
}

function hrm_pay_core_009_mark_run_failed($run_id,$lease_token,$code,&$error=null)
{
    $error=null;$lease_sha=hash('sha256',(string)$lease_token);begin_transaction();$run=hrm_pay_core_009_get_run($run_id,true);if(!$run||!hrm_pay_core_009_assert_lease_locked($run,$lease_sha,$le)){cancel_transaction();$error=$run?$le:'run_missing';return false;}if(!in_array($run['state'],array('running','finalizing'),true)){cancel_transaction();$error='run_failure_state_invalid';return false;}$next=hrm_pay_core_009_transition_locked($run,'failed','worker-failure-'.(hrm_pay_core_009_token((string)$code,64)?:'unknown').'-'.$run['state_version'],$te);if($next===false){cancel_transaction();$error=$te;return false;}commit_transaction();return true;
}

/** Complete/reconcile final atomic preparation after all staged items are complete. */
function hrm_pay_core_009_finalize($run_id,$lease_token,&$error=null)
{
    $error=null;$guard=pay_ctry_002_assert_run_binding_current($run_id,false,$ge);if($guard===false){$error=$ge?:'run_binding_guard_failed';return false;}$lease_sha=hash('sha256',(string)$lease_token);$run=hrm_pay_core_009_get_run($run_id,false);if(!$run||$run['state']!=='finalizing'){$error='run_not_finalizing';return false;}
    $q=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_core_009_items WHERE run_id='.(int)$run_id.' ORDER BY ordinal_no');if(!$q){$error='finalize_item_read_failed';return false;}$docs=array();$count=0;$expected_detail_count=0;$expected_employee_hashes=array();while($item=db_fetch_assoc($q)){$count++;if($item['item_state']!=='completed'||empty($item['result_payload_json'])||empty($item['result_sha256'])){$error='finalize_incomplete_item_set';return false;}$doc=hrm_pay_core_009_rehydrate_document($item['result_payload_json'],$item['result_sha256'],$de);if($doc===false||hash('sha256',(string)$doc->employee_id)!==$item['employee_id_sha256']||(int)$doc->payroll_period_id!==(int)$run['payroll_period_id']){$error=$de?:'finalize_result_scope_mismatch';return false;}$docs[]=$doc;$expected_detail_count+=count($doc->lines);$expected_employee_hashes[]=hash('sha256',(string)$doc->employee_id);}
    if($count===0){$error='finalize_empty_item_set';return false;}sort($expected_employee_hashes,SORT_STRING);$expected_employee_set_sha=hrm_pay_core_009_hash($expected_employee_hashes);
    $period=get_payroll_period((int)$run['payroll_period_id']);if(!$period){$error='payroll_period_missing';return false;}
    if((int)$period['status']===0){
        $preparation=prepare_payroll_period_results_for_approval((int)$run['payroll_period_id'],$docs);if(empty($preparation['ok'])){$error=hrm_pay_core_009_token((string)($preparation['code']??'preparation_failed'),96)?:'preparation_failed';hrm_pay_core_009_mark_run_failed($run_id,$lease_token,$error,$ignore);return false;}
    } elseif((int)$period['status']===1){
        // Crash-after-preparation recovery must bind to the exact persisted result
        // population, not merely to a caller-supplied count. Lock the Calculated
        // period and its result/control rows while reconciling the staged set.
        begin_transaction();$period_locked=get_payroll_period_for_update((int)$run['payroll_period_id']);
        if(!$period_locked||(int)$period_locked['status']!==1){cancel_transaction();$error='prepared_result_reconciliation_stale';return false;}
        $binding=build_payroll_period_approval_binding($period_locked,true,'1.1.0');$sql=get_payroll_period_approval_payslips_sql((int)$run['payroll_period_id'],true);$rows=$sql===false?false:payroll_approval_binding_fetch_rows($sql,'could not reconcile PAY-CORE-009 prepared employee set');
        $persisted_employee_hashes=array();if(is_array($rows))foreach($rows as $row)$persisted_employee_hashes[]=hash('sha256',(string)$row['employee_id']);sort($persisted_employee_hashes,SORT_STRING);$persisted_employee_set_sha=hrm_pay_core_009_hash($persisted_employee_hashes);
        if(empty($binding['ok'])||(int)($binding['payslip_count']??0)!==$count||(int)($binding['detail_count']??0)!==$expected_detail_count||$rows===false||!hash_equals($expected_employee_set_sha,$persisted_employee_set_sha)){cancel_transaction();$error='prepared_result_reconciliation_failed';return false;}
        $locked=hrm_pay_core_009_get_run($run_id,true);if(!$locked||!hrm_pay_core_009_assert_lease_locked($locked,$lease_sha,$le)||$locked['state']!=='finalizing'){cancel_transaction();$error=$locked?($le?:'finalize_state_stale'):'run_missing';return false;}$pre=hrm_pay_core_009_run_state_from_row($locked,$pe);$next=hrm_pay_core_009_transition_locked($locked,'completed','reconcile-prepared-'.$locked['state_version'],$te);if($pre===false||$next===false){cancel_transaction();$error=$pe?:$te;return false;}$actor=hrm_pay_core_009_actor();$evidence=hrm_pay_core_009_hash(array('run_key'=>$locked['run_key'],'payroll_period_id'=>(int)$locked['payroll_period_id'],'payslip_count'=>$count,'detail_count'=>$expected_detail_count,'employee_set_sha256'=>$expected_employee_set_sha,'prepared_result_sha256'=>$binding['result_hash']??null,'prepared_control_sha256'=>$binding['control_hash']??null));$rec=hrm_pay_core_009_recovery($locked['run_key'],'reconcile',$pre['state_sha256'],$evidence,$actor,$re);if($rec===false||!db_query('INSERT INTO '.TB_PREF.'hrm_pay_core_009_recovery_evidence (run_id,action_token,predecessor_state_sha256,evidence_sha256,recovery_sha256,executed_by) VALUES ('.(int)$run_id.',\'reconcile\','.db_escape($pre['state_sha256']).','.db_escape($evidence).','.db_escape($rec['recovery_sha256']).','.$actor.')')){cancel_transaction();$error=$re?:'reconcile_evidence_failed';return false;}commit_transaction();return array('ok'=>true,'reconciled_existing'=>true,'payslip_count'=>$count);
    } else {$error='payroll_period_state_conflict';return false;}

    begin_transaction();$locked=hrm_pay_core_009_get_run($run_id,true);if(!$locked||!hrm_pay_core_009_assert_lease_locked($locked,$lease_sha,$le)||$locked['state']!=='finalizing'){cancel_transaction();$error=$locked?($le?:'finalize_state_stale'):'run_missing';return false;}$next=hrm_pay_core_009_transition_locked($locked,'completed','finalize-'.$locked['state_version'],$te);if($next===false){cancel_transaction();$error=$te;return false;}commit_transaction();return array('ok'=>true,'reconciled_existing'=>false,'payslip_count'=>$count);
}

/** Execute one bounded synchronous chunk. Safe to invoke repeatedly from browser or CLI. */
function hrm_pay_core_009_run_chunk($run_id,$channel='browser',$chunk_size=null,&$error=null)
{
    $error=null;$lease=hrm_pay_core_009_acquire_lease($run_id,$channel,HRM_PAY_CORE_009_DEFAULT_LEASE_SECONDS,$le);if($lease===false){$error=$le;return false;}$token=$lease['token'];$processed=0;$failed=0;$final=false;
    try{
        $run=hrm_pay_core_009_get_run($run_id,false);if(!$run){$error='run_missing';return false;}
        if($run['state']==='finalizing'){$fin=hrm_pay_core_009_finalize($run_id,$token,$fe);if($fin===false){$error=$fe;return false;}$final=true;return array('ok'=>true,'processed'=>0,'failed'=>0,'finalized'=>true,'state'=>'completed');}
        $limit=$chunk_size===null?(int)$run['chunk_size']:(int)$chunk_size;$limit=max(1,min(HRM_PAY_CORE_009_MAX_CHUNK_SIZE,$limit));
        for($n=0;$n<$limit;$n++){
            $item=hrm_pay_core_009_claim_item($run_id,$token,$ce);if($item===false){$error=$ce;return false;}if($item===null)break;$processed++;
            $run=hrm_pay_core_009_get_run($run_id,false);$scope=hrm_pay_core_009_reverify_item_scope($run,$item,$se);if($scope===false){$failed++;hrm_pay_core_009_record_item_outcome($run_id,$item['run_item_id'],$token,'failed',null,null,$se,$oe);continue;}
            $employee=function_exists('get_employee_by_code')?get_employee_by_code($item['employee_id']):false;if(!$employee){$failed++;hrm_pay_core_009_record_item_outcome($run_id,$item['run_item_id'],$token,'failed',null,null,'employee_missing',$oe);continue;}
            $from_display=function_exists('sql2date')?sql2date($run['from_date']):$run['from_date'];$to_display=function_exists('sql2date')?sql2date($run['to_date']):$run['to_date'];
            $context=array('salary_components'=>function_exists('get_salary_components_for_employees')?get_salary_components_for_employees(array($employee),$to_display):array());
            $doc=calculate_employee_payslip_read_only($employee,$from_display,$to_display,(int)$run['payroll_period_id'],$context);
            if(!$doc){$failed++;$code='calculation_failed';if(function_exists('payroll_suspension_readiness_last_result')){$r=payroll_suspension_readiness_last_result();if(is_array($r)&&!empty($r['decision']))$code=$r['decision'];}hrm_pay_core_009_record_item_outcome($run_id,$item['run_item_id'],$token,'failed',null,null,$code,$oe);continue;}
            $stage=hrm_pay_core_009_stage_document($doc,$ae);if($stage===false||!hrm_pay_core_009_record_item_outcome($run_id,$item['run_item_id'],$token,'completed',$stage['payload_json'],$stage['result_sha256'],null,$oe)){$failed++;if($stage===false)hrm_pay_core_009_record_item_outcome($run_id,$item['run_item_id'],$token,'failed',null,null,$ae,$ignore);}
            if(!hrm_pay_core_009_heartbeat($run_id,$token,$channel,HRM_PAY_CORE_009_DEFAULT_LEASE_SECONDS,$he)){$error=$he;return false;}
        }
        $cp=hrm_pay_core_009_checkpoint_run($run_id,$token,$cpe);if($cp===false){$error=$cpe;return false;}
        if($cp['state']==='finalizing'){$fin=hrm_pay_core_009_finalize($run_id,$token,$fe);if($fin===false){$error=$fe;return false;}$final=true;return array('ok'=>true,'processed'=>$processed,'failed'=>$failed,'finalized'=>true,'state'=>'completed','counts'=>$cp['counts']);}
        return array('ok'=>true,'processed'=>$processed,'failed'=>$failed,'finalized'=>false,'state'=>$cp['state'],'counts'=>$cp['counts']);
    } finally {
        hrm_pay_core_009_release_lease($run_id,$token,$channel);
    }
}
?>
