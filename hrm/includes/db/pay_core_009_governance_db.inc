<?php
/**
 * PAY-CORE-009 authoritative restartable-run writers.
 *
 * Owns only orchestration state/evidence. It does not own payroll calculation,
 * GL, payment, statutory filing, approval or historical-result mutation.
 */
include_once(dirname(__FILE__).'/pay_core_009_schema_db.inc');
include_once(dirname(__FILE__).'/../payroll/pay_core_009_contract.inc');

function hrm_pay_core_009_actor(){return isset($_SESSION['wa_current_user']->user)?(int)$_SESSION['wa_current_user']->user:0;}
function hrm_pay_core_009_access($p){return function_exists('user_check_access')&&user_check_access($p);}
function hrm_pay_core_009_manage_transaction(){return empty($GLOBALS['transaction_level']);}
function hrm_pay_core_009_one($sql){$r=db_query($sql);if(!$r||db_num_rows($r)!==1)return false;return db_fetch_assoc($r);}
function hrm_pay_core_009_utc_sql($unix=null){if($unix===null)$unix=time();return gmdate('Y-m-d H:i:s',(int)$unix);}
function hrm_pay_core_009_utc_iso($unix=null){if($unix===null)$unix=time();return gmdate('Y-m-d\TH:i:s\Z',(int)$unix);}
function hrm_pay_core_009_nullable_sql_id($v){return $v===null?'NULL':(string)(int)$v;}

/** Resolve legacy employee -> normalized Person/Worker/Employment/Assignment/Relationship authority. */
function hrm_pay_core_009_resolve_employee_scope($employee_id,$from_date,$to_date,$for_update=false,&$error=null)
{
    $error=null;$employee_id=trim((string)$employee_id);
    if($employee_id===''||!hrm_pay_core_009_date($from_date)||!hrm_pay_core_009_date($to_date)||$from_date>$to_date){$error='scope_request_invalid';return false;}
    $sql='SELECT m.employee_number,m.worker_id,w.person_id,h.employment_id,h.legal_entity_id,a.assignment_id,'
        .'r.payroll_relationship_id,r.relationship_sha256,rm.member_sha256 '
        .'FROM '.TB_PREF.'hrm_employee_worker_map m '
        .'INNER JOIN '.TB_PREF.'hrm_workers w ON w.worker_id=m.worker_id '
        .'INNER JOIN '.TB_PREF.'hrm_employments h ON h.worker_id=m.worker_id AND h.legacy_employee_number=m.employee_number AND h.legacy_current=1 '
        .'INNER JOIN '.TB_PREF.'hrm_assignments a ON a.employment_id=h.employment_id AND a.legacy_employee_number=m.employee_number AND a.legacy_current=1 AND a.primary_flag=1 AND a.assignment_type_code=\'legacy_primary\' '
        .'INNER JOIN '.TB_PREF.'hrm_payroll_relationships r ON r.worker_id=m.worker_id AND r.employment_id=h.employment_id AND r.legal_entity_id=h.legal_entity_id '
        .'INNER JOIN '.TB_PREF.'hrm_payroll_relationship_members rm ON rm.payroll_relationship_id=r.payroll_relationship_id AND rm.assignment_id=a.assignment_id '
        .'WHERE m.employee_id='.db_escape($employee_id).' AND m.retired_at IS NULL'
        .' AND (h.start_date IS NULL OR h.start_date<='.db_escape($from_date).') AND (h.end_date IS NULL OR h.end_date>='.db_escape($to_date).')'
        .' AND (a.effective_from IS NULL OR a.effective_from<='.db_escape($from_date).') AND (a.effective_to IS NULL OR a.effective_to>='.db_escape($to_date).')'
        .' AND r.effective_from<='.db_escape($from_date).' AND (r.effective_to IS NULL OR r.effective_to>='.db_escape($to_date).')'
        .' AND rm.effective_from<='.db_escape($from_date).' AND (rm.effective_to IS NULL OR rm.effective_to>='.db_escape($to_date).')'
        .' ORDER BY r.payroll_relationship_id,a.assignment_id LIMIT 2'.($for_update?' FOR UPDATE':'');
    $q=db_query($sql,'could not resolve PAY-CORE-009 employee object scope');
    if(!$q||db_num_rows($q)!==1){$error=$q?'employee_scope_ambiguous_or_missing':'employee_scope_lookup_failed';return false;}
    $x=db_fetch_assoc($q);$scope=array('legal_entity_id'=>(int)$x['legal_entity_id'],'payroll_relationship_id'=>(int)$x['payroll_relationship_id'],'worker_id'=>(int)$x['worker_id'],'person_id'=>(int)$x['person_id'],'assignment_id'=>(int)$x['assignment_id'],'calendar_period_id'=>null);
    if(hrm_pay_core_009_scope($scope,$se)===false){$error=$se;return false;}

    // A legacy ad-hoc payroll range need not map to a governed calendar period.
    // If an exact normalized period exists, bind it; ambiguity fails closed.
    $cq=db_query('SELECT cp.calendar_period_id FROM '.TB_PREF.'hrm_payroll_group_enrollments ge '
        .'INNER JOIN '.TB_PREF.'hrm_payroll_group_versions gv ON gv.group_version_id=ge.group_version_id '
        .'INNER JOIN '.TB_PREF.'hrm_payroll_groups g ON g.payroll_group_id=gv.payroll_group_id '
        .'INNER JOIN '.TB_PREF.'hrm_payroll_calendar_periods cp ON cp.calendar_version_id=gv.calendar_version_id '
        .'WHERE ge.payroll_relationship_id='.(int)$x['payroll_relationship_id']
        .' AND g.legal_entity_id='.(int)$x['legal_entity_id']
        .' AND ge.effective_from<='.db_escape($from_date).' AND (ge.effective_to IS NULL OR ge.effective_to>='.db_escape($to_date).')'
        .' AND cp.period_start='.db_escape($from_date).' AND cp.period_end='.db_escape($to_date).' ORDER BY cp.calendar_period_id LIMIT 2',
        'could not resolve PAY-CORE-009 calendar scope');
    if(!$cq){$error='calendar_scope_lookup_failed';return false;}
    if(db_num_rows($cq)>1){$error='calendar_scope_ambiguous';return false;}
    if(db_num_rows($cq)===1){$c=db_fetch_assoc($cq);$scope['calendar_period_id']=(int)$c['calendar_period_id'];}
    $scope['employee_number']=(int)$x['employee_number'];
    $scope['relationship_sha256']=(string)$x['relationship_sha256'];
    $scope['member_sha256']=(string)$x['member_sha256'];
    return $scope;
}

function hrm_pay_core_009_item_input_sha($employee_id,array $scope,$from_date,$to_date)
{
    return hrm_pay_core_009_hash(array('employee_id_sha256'=>hash('sha256',(string)$employee_id),'scope'=>array_intersect_key($scope,array_flip(array('legal_entity_id','payroll_relationship_id','worker_id','person_id','assignment_id','calendar_period_id'))),'relationship_sha256'=>$scope['relationship_sha256']??null,'member_sha256'=>$scope['member_sha256']??null,'from_date'=>$from_date,'to_date'=>$to_date));
}

/** Start exactly one idempotent payroll run and its Draft payroll period. */
function hrm_pay_core_009_start_run($period_name,$from_date,$to_date,$department_id,array $employees,$chunk_size=HRM_PAY_CORE_009_DEFAULT_CHUNK_SIZE,&$error=null)
{
    $error=null;$actor=hrm_pay_core_009_actor();
    if($actor<=0||!hrm_pay_core_009_access('SA_HRM_MANAGE_PAY_CORE_RUN')){$error='access_denied';return false;}
    $from=function_exists('payroll_to_sql_date')?payroll_to_sql_date($from_date):(string)$from_date;$to=function_exists('payroll_to_sql_date')?payroll_to_sql_date($to_date):(string)$to_date;
    if(!hrm_pay_core_009_date($from)||!hrm_pay_core_009_date($to)||$from>$to||empty($employees)){$error='run_request_invalid';return false;}
    $manage=hrm_pay_core_009_manage_transaction();if($manage)begin_transaction();
    $items=array();
    foreach($employees as $employee){$employee_id=is_array($employee)?trim((string)($employee['employee_id']??'')):'';$scope=hrm_pay_core_009_resolve_employee_scope($employee_id,$from,$to,true,$se);if($scope===false){if($manage)cancel_transaction();$error=$se;return false;}$items[]=array('employee_id'=>$employee_id,'scope'=>$scope);}
    $request=hrm_pay_core_009_run_request($period_name,$from,$to,$items,$chunk_size,$re);if($request===false){if($manage)cancel_transaction();$error=$re;return false;}
    $same=hrm_pay_core_009_one('SELECT * FROM '.TB_PREF.'hrm_pay_core_009_runs WHERE request_sha256='.db_escape($request['request_sha256']).' LIMIT 2 FOR UPDATE');
    if($same){if((int)$same['created_by']!==$actor){if($manage)cancel_transaction();$error='request_owned_by_other_actor';return false;}if($manage)commit_transaction();return array('run_id'=>(int)$same['run_id'],'payroll_period_id'=>(int)$same['payroll_period_id'],'run_key'=>$same['run_key'],'replayed'=>true);}
    if(!function_exists('add_payroll_period')){if($manage)cancel_transaction();$error='payroll_period_writer_unavailable';return false;}
    $period_id=add_payroll_period($period_name,$from,$to,$department_id?$department_id:null);if(!$period_id){if($manage)cancel_transaction();$error='payroll_period_create_failed';return false;}
    $calendar=null;foreach($items as $i){$c=$i['scope']['calendar_period_id'];if($c===null){$calendar=null;break;}if($calendar===null)$calendar=$c;elseif($calendar!==$c){$calendar=null;break;}}
    $state=hrm_pay_core_009_state_descriptor($request['run_key'],'queued',1,$request['request_sha256'],0,null,$se);if($state===false){if($manage)cancel_transaction();$error=$se;return false;}
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_core_009_runs (run_key,run_type,payroll_period_id,legal_entity_id,calendar_period_id,period_name,from_date,to_date,chunk_size,request_sha256,state,state_version,state_sha256,checkpoint_no,created_by) VALUES ('
        .db_escape($request['run_key']).','.db_escape($request['run_type']).','.(int)$period_id.','.(int)$request['legal_entity_id'].','.hrm_pay_core_009_nullable_sql_id($calendar).','.db_escape(trim((string)$period_name)).','.db_escape($from).','.db_escape($to).','.(int)$chunk_size.','.db_escape($request['request_sha256']).',\'queued\',1,'.db_escape($state['state_sha256']).',0,'.$actor.')';
    if(!db_query($sql,'could not create PAY-CORE-009 run')){if($manage)cancel_transaction();$error='run_insert_failed';return false;}$run_id=(int)db_insert_id();if($run_id<=0){if($manage)cancel_transaction();$error='run_identity_missing';return false;}
    $ordinal=0;foreach($items as $i){$ordinal++;$s=$i['scope'];$input=hrm_pay_core_009_item_input_sha($i['employee_id'],$s,$from,$to);$desc=hrm_pay_core_009_item_descriptor($request['run_key'],$ordinal,$i['employee_id'],$s,$input,'pending',1,0,null,$ie);if($desc===false){if($manage)cancel_transaction();$error=$ie;return false;}$sql='INSERT INTO '.TB_PREF.'hrm_pay_core_009_items (run_id,ordinal_no,employee_id,employee_id_sha256,legal_entity_id,payroll_relationship_id,worker_id,person_id,assignment_id,calendar_period_id,object_scope_sha256,input_sha256,item_state,state_version,item_state_sha256,attempt_no) VALUES ('
        .$run_id.','.$ordinal.','.db_escape($i['employee_id']).','.db_escape(hash('sha256',$i['employee_id'])).','.$s['legal_entity_id'].','.$s['payroll_relationship_id'].','.$s['worker_id'].','.$s['person_id'].','.$s['assignment_id'].','.hrm_pay_core_009_nullable_sql_id($s['calendar_period_id']).','.db_escape(hrm_pay_core_009_hash(array_intersect_key($s,array_flip(array('legal_entity_id','payroll_relationship_id','worker_id','person_id','assignment_id','calendar_period_id'))))).','.db_escape($input).',\'pending\',1,'.db_escape($desc['item_state_sha256']).',0)';if(!db_query($sql,'could not create PAY-CORE-009 run item')){if($manage)cancel_transaction();$error='run_item_insert_failed';return false;}}
    if($manage)commit_transaction();return array('run_id'=>$run_id,'payroll_period_id'=>(int)$period_id,'run_key'=>$request['run_key'],'replayed'=>false);
}

function hrm_pay_core_009_get_run($run_id,$for_update=false){return hrm_pay_core_009_one('SELECT * FROM '.TB_PREF.'hrm_pay_core_009_runs WHERE run_id='.(int)$run_id.' LIMIT 2'.($for_update?' FOR UPDATE':''));}
function hrm_pay_core_009_run_state_from_row(array $r,&$error=null){return hrm_pay_core_009_state_descriptor($r['run_key'],$r['state'],$r['state_version'],$r['request_sha256'],$r['checkpoint_no'],$r['lease_token_sha256'],$error);}
function hrm_pay_core_009_run_owned_by_actor(array $run,$actor){return (int)$actor>0&&(int)$run['created_by']===(int)$actor;}

function hrm_pay_core_009_append_transition($run_id,array $from,array $to,$command_key,$actor)
{
    $tr=array('run_key'=>$from['run_key'],'from_state'=>$from['state'],'to_state'=>$to['state'],'expected_version'=>$from['state_version'],'resulting_version'=>$to['state_version'],'command_key'=>$command_key,'predecessor_state_sha256'=>$from['state_sha256'],'resulting_state_sha256'=>$to['state_sha256'],'actor_id'=>(int)$actor);$sha=hrm_pay_core_009_hash($tr);
    return db_query('INSERT INTO '.TB_PREF.'hrm_pay_core_009_transitions (run_id,from_state,to_state,expected_version,resulting_version,command_key,predecessor_state_sha256,resulting_state_sha256,transition_sha256,actor_id) VALUES ('.(int)$run_id.','.db_escape($from['state']).','.db_escape($to['state']).','.(int)$from['state_version'].','.(int)$to['state_version'].','.db_escape($command_key).','.db_escape($from['state_sha256']).','.db_escape($to['state_sha256']).','.db_escape($sha).','.(int)$actor.')','could not append PAY-CORE-009 transition');
}

/** CAS a run state and append immutable transition evidence in the same transaction. */
function hrm_pay_core_009_transition_locked(array $run,$target,$command_key,&$error=null,$checkpoint_override=null)
{
    $error=null;$actor=hrm_pay_core_009_actor();$from=hrm_pay_core_009_run_state_from_row($run,$se);if($from===false){$error=$se;return false;}$allowed=hrm_pay_core_009_allowed_transitions();if(!isset($allowed[$from['state']])||!in_array($target,$allowed[$from['state']],true)){$error='transition_denied';return false;}$cp=$checkpoint_override===null?(int)$from['checkpoint_no']:(int)$checkpoint_override;$to=hrm_pay_core_009_state_descriptor($from['run_key'],$target,(int)$from['state_version']+1,$from['request_sha256'],$cp,$from['lease_token_sha256'],$te);if($to===false){$error=$te;return false;}
    $sql='UPDATE '.TB_PREF.'hrm_pay_core_009_runs SET state='.db_escape($target).',state_version='.(int)$to['state_version'].',state_sha256='.db_escape($to['state_sha256']).',checkpoint_no='.$cp.' WHERE run_id='.(int)$run['run_id'].' AND state='.db_escape($from['state']).' AND state_version='.(int)$from['state_version'].' AND state_sha256='.db_escape($from['state_sha256']);
    if(!db_query($sql,'could not CAS PAY-CORE-009 run state')||db_num_affected_rows()!==1){$error='stale_state';return false;}if(!hrm_pay_core_009_append_transition((int)$run['run_id'],$from,$to,$command_key,$actor)){$error='transition_evidence_failed';return false;}$run['state']=$to['state'];$run['state_version']=$to['state_version'];$run['state_sha256']=$to['state_sha256'];$run['checkpoint_no']=$cp;return $run;
}

function hrm_pay_core_009_worker_token($channel)
{
    $actor=hrm_pay_core_009_actor();$channel=hrm_pay_core_009_token($channel,32);if($actor<=0||!$channel)return false;return $channel.'.actor-'.$actor;
}

function hrm_pay_core_009_recover_expired_items_locked(array $run,$actor,&$error=null)
{
    $error=null;$lease=(string)($run['lease_token_sha256']??'');$worker=(string)($run['lease_worker_sha256']??'');
    if($lease===''||!hrm_pay_core_009_is_sha256($lease))return array('count'=>0,'attempts'=>array());
    if(!hrm_pay_core_009_is_sha256($worker)){$error='expired_lease_worker_invalid';return false;}
    $q=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_core_009_items WHERE run_id='.(int)$run['run_id'].' AND item_state=\'running\' AND lease_token_sha256='.db_escape($lease).' ORDER BY ordinal_no FOR UPDATE','could not lock expired PAY-CORE-009 items');
    if(!$q){$error='expired_item_lookup_failed';return false;}$attempts=array();
    while($item=db_fetch_assoc($q)){
        $target=(int)$item['attempt_no']>=HRM_PAY_CORE_009_MAX_ATTEMPTS?'quarantined':'pending';
        $scope=array('legal_entity_id'=>(int)$item['legal_entity_id'],'payroll_relationship_id'=>(int)$item['payroll_relationship_id'],'worker_id'=>(int)$item['worker_id'],'person_id'=>(int)$item['person_id'],'assignment_id'=>(int)$item['assignment_id'],'calendar_period_id'=>$item['calendar_period_id']===null?null:(int)$item['calendar_period_id']);
        $desc=hrm_pay_core_009_item_descriptor($run['run_key'],$item['ordinal_no'],$item['employee_id'],$scope,$item['input_sha256'],$target,(int)$item['state_version']+1,(int)$item['attempt_no'],null,$de);
        if($desc===false){$error=$de;return false;}
        $code='lease_expired';$evidence=hrm_pay_core_009_hash(array('run_key'=>$run['run_key'],'ordinal'=>(int)$item['ordinal_no'],'attempt'=>(int)$item['attempt_no'],'expired_lease_sha256'=>$lease,'predecessor_state_sha256'=>$item['item_state_sha256']));
        $command='expired-lease-item-'.$item['ordinal_no'].'-attempt-'.$item['attempt_no'];
        $attempt=array('run_key'=>$run['run_key'],'ordinal'=>(int)$item['ordinal_no'],'attempt_no'=>(int)$item['attempt_no'],'command_key'=>$command,'worker_sha256'=>$worker,'predecessor_state_sha256'=>$item['item_state_sha256'],'outcome'=>'stale','result_sha256'=>null,'error_code'=>$code,'evidence_sha256'=>$evidence);$attempt['attempt_sha256']=hrm_pay_core_009_hash($attempt);
        $sql='UPDATE '.TB_PREF.'hrm_pay_core_009_items SET item_state='.db_escape($target).',state_version='.(int)$desc['state_version'].',item_state_sha256='.db_escape($desc['item_state_sha256']).',result_sha256=NULL,result_payload_json=NULL,lease_token_sha256=NULL,last_error_code='.db_escape($code).' WHERE run_item_id='.(int)$item['run_item_id'].' AND item_state=\'running\' AND state_version='.(int)$item['state_version'].' AND item_state_sha256='.db_escape($item['item_state_sha256']).' AND lease_token_sha256='.db_escape($lease);
        if(!db_query($sql,'could not recover expired PAY-CORE-009 item')||db_num_affected_rows()!==1){$error='expired_item_cas_failed';return false;}
        if(!db_query('INSERT INTO '.TB_PREF.'hrm_pay_core_009_attempts (run_id,run_item_id,attempt_no,command_key,worker_sha256,predecessor_state_sha256,outcome,result_sha256,error_code,evidence_sha256,attempt_sha256,executed_by) VALUES ('.(int)$run['run_id'].','.(int)$item['run_item_id'].','.(int)$item['attempt_no'].','.db_escape($command).','.db_escape($worker).','.db_escape($item['item_state_sha256']).',\'stale\',NULL,'.db_escape($code).','.db_escape($evidence).','.db_escape($attempt['attempt_sha256']).','.(int)$actor.')','could not append expired PAY-CORE-009 attempt evidence')){$error='expired_attempt_evidence_failed';return false;}
        if($target==='quarantined'){$qsha=hrm_pay_core_009_hash(array('attempt_sha256'=>$attempt['attempt_sha256'],'reason_code'=>$code,'evidence_sha256'=>$evidence));if(!db_query('INSERT INTO '.TB_PREF.'hrm_pay_core_009_quarantine (run_id,run_item_id,attempt_sha256,reason_code,evidence_sha256,quarantine_sha256,quarantined_by) VALUES ('.(int)$run['run_id'].','.(int)$item['run_item_id'].','.db_escape($attempt['attempt_sha256']).','.db_escape($code).','.db_escape($evidence).','.db_escape($qsha).','.(int)$actor.')')){$error='expired_quarantine_evidence_failed';return false;}}
        $attempts[]=$attempt['attempt_sha256'];
    }
    return array('count'=>count($attempts),'attempts'=>$attempts);
}

/** Acquire a bounded lease; expired leases are taken over with immutable evidence. */
function hrm_pay_core_009_acquire_lease($run_id,$channel='browser',$seconds=HRM_PAY_CORE_009_DEFAULT_LEASE_SECONDS,&$error=null)
{
    $error=null;$actor=hrm_pay_core_009_actor();$worker=hrm_pay_core_009_worker_token($channel);$seconds=(int)$seconds;
    if($actor<=0||!hrm_pay_core_009_access('SA_HRM_EXECUTE_PAY_CORE_RUN')||!$worker||$seconds<30||$seconds>900){$error='access_or_lease_invalid';return false;}
    begin_transaction();$run=hrm_pay_core_009_get_run($run_id,true);if(!$run){cancel_transaction();$error='run_missing';return false;}if(!hrm_pay_core_009_run_owned_by_actor($run,$actor)){cancel_transaction();$error='run_owner_mismatch';return false;}if(!in_array($run['state'],array('queued','running','finalizing'),true)){cancel_transaction();$error='run_not_resumable';return false;}
    $now=time();$takeover=false;if(!empty($run['lease_token_sha256'])&&!empty($run['lease_expires_at'])){$expiry=strtotime($run['lease_expires_at'].' UTC');if($expiry!==false&&$expiry>$now){cancel_transaction();$error='lease_busy';return false;}$takeover=true;}
    if($takeover){$pre=hrm_pay_core_009_run_state_from_row($run,$pe);$recovered=hrm_pay_core_009_recover_expired_items_locked($run,$actor,$xe);if($pre===false||$recovered===false){cancel_transaction();$error=$pe?:$xe;return false;}$evidence=hrm_pay_core_009_hash(array('run_key'=>$run['run_key'],'expired_lease_sha256'=>$run['lease_token_sha256'],'recovered_attempt_sha256s'=>$recovered['attempts']));$rec=hrm_pay_core_009_recovery($run['run_key'],'expire_lease',$pre['state_sha256'],$evidence,$actor,$re);if($rec===false||!db_query('INSERT INTO '.TB_PREF.'hrm_pay_core_009_recovery_evidence (run_id,action_token,predecessor_state_sha256,evidence_sha256,recovery_sha256,executed_by) VALUES ('.(int)$run_id.','.db_escape('expire_lease').','.db_escape($pre['state_sha256']).','.db_escape($evidence).','.db_escape($rec['recovery_sha256']).','.$actor.')')){cancel_transaction();$error=$re?:'expired_lease_recovery_evidence_failed';return false;}}
    if($run['state']==='queued'){$run=hrm_pay_core_009_transition_locked($run,'running','lease-start-'.$run['state_version'],$te);if($run===false){cancel_transaction();$error=$te;return false;}}
    $token=bin2hex(random_bytes(24));$token_sha=hash('sha256',$token);$worker_sha=hash('sha256',$worker);$expires=$now+$seconds;
    $state=hrm_pay_core_009_state_descriptor($run['run_key'],$run['state'],$run['state_version'],$run['request_sha256'],$run['checkpoint_no'],$token_sha,$se);if($state===false){cancel_transaction();$error=$se;return false;}
    if(!db_query('UPDATE '.TB_PREF.'hrm_pay_core_009_runs SET lease_worker_sha256='.db_escape($worker_sha).',lease_token_sha256='.db_escape($token_sha).',lease_expires_at='.db_escape(hrm_pay_core_009_utc_sql($expires)).',heartbeat_at='.db_escape(hrm_pay_core_009_utc_sql($now)).',state_sha256='.db_escape($state['state_sha256']).' WHERE run_id='.(int)$run_id.' AND state_version='.(int)$run['state_version'].' AND state_sha256='.db_escape($run['state_sha256']),'could not acquire PAY-CORE-009 lease')||db_num_affected_rows()!==1){cancel_transaction();$error='lease_cas_failed';return false;}
    $kind=$takeover?'expired_takeover':'acquired';$ev=hrm_pay_core_009_lease($run['run_key'],$worker,$token_sha,hrm_pay_core_009_utc_iso($now),hrm_pay_core_009_utc_iso($expires),$kind,$le);if($ev===false||!db_query('INSERT INTO '.TB_PREF.'hrm_pay_core_009_lease_events (run_id,event_kind,worker_sha256,lease_token_sha256,acquired_at,expires_at,lease_event_sha256,created_by) VALUES ('.(int)$run_id.','.db_escape($kind).','.db_escape($worker_sha).','.db_escape($token_sha).','.db_escape(hrm_pay_core_009_utc_sql($now)).','.db_escape(hrm_pay_core_009_utc_sql($expires)).','.db_escape($ev['lease_event_sha256']).','.$actor.')','could not append PAY-CORE-009 lease evidence')){cancel_transaction();$error=$le?:'lease_evidence_failed';return false;}
    commit_transaction();return array('run_id'=>(int)$run_id,'token'=>$token,'token_sha256'=>$token_sha,'worker_token'=>$worker,'expires_at'=>hrm_pay_core_009_utc_sql($expires));
}

function hrm_pay_core_009_assert_lease_locked(array $run,$token_sha,&$error=null)
{
    $error=null;if(!hrm_pay_core_009_is_sha256($token_sha)||empty($run['lease_token_sha256'])||!hash_equals((string)$run['lease_token_sha256'],$token_sha)){$error='lease_token_mismatch';return false;}if(empty($run['lease_expires_at'])||strtotime($run['lease_expires_at'].' UTC')<=time()){$error='lease_expired';return false;}return true;
}

function hrm_pay_core_009_heartbeat($run_id,$lease_token,$channel='browser',$seconds=HRM_PAY_CORE_009_DEFAULT_LEASE_SECONDS,&$error=null)
{
    $error=null;$actor=hrm_pay_core_009_actor();$worker=hrm_pay_core_009_worker_token($channel);$seconds=(int)$seconds;$sha=hash('sha256',(string)$lease_token);$worker_sha=$worker?hash('sha256',$worker):'';
    if($actor<=0||!hrm_pay_core_009_access('SA_HRM_EXECUTE_PAY_CORE_RUN')||!$worker||$seconds<30||$seconds>900){$error='access_or_lease_invalid';return false;}
    begin_transaction();$run=hrm_pay_core_009_get_run($run_id,true);
    if(!$run||!hrm_pay_core_009_assert_lease_locked($run,$sha,$le)){cancel_transaction();$error=$run?$le:'run_missing';return false;}
    if(empty($run['lease_worker_sha256'])||!hash_equals((string)$run['lease_worker_sha256'],$worker_sha)){cancel_transaction();$error='lease_worker_mismatch';return false;}
    $now=time();$exp=$now+$seconds;if(!db_query('UPDATE '.TB_PREF.'hrm_pay_core_009_runs SET heartbeat_at='.db_escape(hrm_pay_core_009_utc_sql($now)).',lease_expires_at='.db_escape(hrm_pay_core_009_utc_sql($exp)).' WHERE run_id='.(int)$run_id.' AND lease_token_sha256='.db_escape($sha).' AND lease_worker_sha256='.db_escape($worker_sha),'could not heartbeat PAY-CORE-009 lease')||db_num_affected_rows()!==1){cancel_transaction();$error='heartbeat_cas_failed';return false;}$ev=hrm_pay_core_009_lease($run['run_key'],$worker,$sha,hrm_pay_core_009_utc_iso($now),hrm_pay_core_009_utc_iso($exp),'heartbeat',$ee);if($ev===false||!db_query('INSERT INTO '.TB_PREF.'hrm_pay_core_009_lease_events (run_id,event_kind,worker_sha256,lease_token_sha256,acquired_at,expires_at,lease_event_sha256,created_by) VALUES ('.(int)$run_id.',\'heartbeat\','.db_escape($worker_sha).','.db_escape($sha).','.db_escape(hrm_pay_core_009_utc_sql($now)).','.db_escape(hrm_pay_core_009_utc_sql($exp)).','.db_escape($ev['lease_event_sha256']).','.$actor.')')){cancel_transaction();$error=$ee?:'heartbeat_evidence_failed';return false;}commit_transaction();return true;
}

function hrm_pay_core_009_release_lease($run_id,$lease_token,$channel='browser')
{
    $actor=hrm_pay_core_009_actor();$worker=hrm_pay_core_009_worker_token($channel);$sha=hash('sha256',(string)$lease_token);$worker_sha=$worker?hash('sha256',$worker):'';if($actor<=0||!$worker)return false;
    begin_transaction();$run=hrm_pay_core_009_get_run($run_id,true);
    if(!$run||empty($run['lease_token_sha256'])||!hash_equals((string)$run['lease_token_sha256'],$sha)||empty($run['lease_worker_sha256'])||!hash_equals((string)$run['lease_worker_sha256'],$worker_sha)){cancel_transaction();return false;}
    // Never detach an active lease while an item is still owned by it. If an
    // outcome write failed or the worker crashed between claim and outcome,
    // preserve the lease until expiry so the normal takeover path can append
    // immutable stale-attempt evidence and safely retry/quarantine the item.
    $active=db_query('SELECT run_item_id FROM '.TB_PREF.'hrm_pay_core_009_items WHERE run_id='.(int)$run_id.' AND item_state=\'running\' AND lease_token_sha256='.db_escape($sha).' ORDER BY run_item_id LIMIT 1 FOR UPDATE');if(!$active||db_num_rows($active)!==0){cancel_transaction();return false;}
    $now=time();$ev=hrm_pay_core_009_lease($run['run_key'],$worker,$sha,hrm_pay_core_009_utc_iso($now),hrm_pay_core_009_utc_iso($now+1),'released',$e);if($ev===false){cancel_transaction();return false;}$no_lease=hrm_pay_core_009_state_descriptor($run['run_key'],$run['state'],$run['state_version'],$run['request_sha256'],$run['checkpoint_no'],null,$ne);if($no_lease===false){cancel_transaction();return false;}if(!db_query('INSERT INTO '.TB_PREF.'hrm_pay_core_009_lease_events (run_id,event_kind,worker_sha256,lease_token_sha256,acquired_at,expires_at,lease_event_sha256,created_by) VALUES ('.(int)$run_id.',\'released\','.db_escape($worker_sha).','.db_escape($sha).','.db_escape(hrm_pay_core_009_utc_sql($now)).','.db_escape(hrm_pay_core_009_utc_sql($now+1)).','.db_escape($ev['lease_event_sha256']).','.$actor.')')||!db_query('UPDATE '.TB_PREF.'hrm_pay_core_009_runs SET lease_worker_sha256=NULL,lease_token_sha256=NULL,lease_expires_at=NULL,heartbeat_at=NULL,state_sha256='.db_escape($no_lease['state_sha256']).' WHERE run_id='.(int)$run_id.' AND lease_token_sha256='.db_escape($sha).' AND lease_worker_sha256='.db_escape($worker_sha).' AND state_sha256='.db_escape($run['state_sha256']))||db_num_affected_rows()!==1){cancel_transaction();return false;}commit_transaction();return true;
}

/** Claim one pending work item using row lock + state-version CAS. */
function hrm_pay_core_009_claim_item($run_id,$lease_token,&$error=null)
{
    $error=null;$sha=hash('sha256',(string)$lease_token);$actor=hrm_pay_core_009_actor();if($actor<=0||!hrm_pay_core_009_access('SA_HRM_EXECUTE_PAY_CORE_RUN')){$error='access_denied';return false;}begin_transaction();$run=hrm_pay_core_009_get_run($run_id,true);if(!$run||!hrm_pay_core_009_assert_lease_locked($run,$sha,$le)||$run['state']!=='running'){cancel_transaction();$error=$run?($le?:'run_not_running'):'run_missing';return false;}$q=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_core_009_items WHERE run_id='.(int)$run_id." AND item_state='pending' ORDER BY ordinal_no LIMIT 1 FOR UPDATE",'could not claim PAY-CORE-009 item');if(!$q){cancel_transaction();$error='item_lookup_failed';return false;}if(db_num_rows($q)===0){commit_transaction();return null;}$item=db_fetch_assoc($q);$scope=array('legal_entity_id'=>(int)$item['legal_entity_id'],'payroll_relationship_id'=>(int)$item['payroll_relationship_id'],'worker_id'=>(int)$item['worker_id'],'person_id'=>(int)$item['person_id'],'assignment_id'=>(int)$item['assignment_id'],'calendar_period_id'=>$item['calendar_period_id']===null?null:(int)$item['calendar_period_id']);$desc=hrm_pay_core_009_item_descriptor($run['run_key'],$item['ordinal_no'],$item['employee_id'],$scope,$item['input_sha256'],'running',(int)$item['state_version']+1,(int)$item['attempt_no']+1,null,$ie);if($desc===false){cancel_transaction();$error=$ie;return false;}$sql='UPDATE '.TB_PREF.'hrm_pay_core_009_items SET item_state=\'running\',state_version='.(int)$desc['state_version'].',item_state_sha256='.db_escape($desc['item_state_sha256']).',attempt_no='.(int)$desc['attempt_no'].',lease_token_sha256='.db_escape($sha).',last_error_code=NULL WHERE run_item_id='.(int)$item['run_item_id'].' AND item_state=\'pending\' AND state_version='.(int)$item['state_version'].' AND item_state_sha256='.db_escape($item['item_state_sha256']);if(!db_query($sql,'could not CAS PAY-CORE-009 item claim')||db_num_affected_rows()!==1){cancel_transaction();$error='item_claim_stale';return false;}$item['item_state']='running';$item['state_version']=$desc['state_version'];$item['item_state_sha256']=$desc['item_state_sha256'];$item['attempt_no']=$desc['attempt_no'];$item['lease_token_sha256']=$sha;commit_transaction();return $item;
}

function hrm_pay_core_009_record_item_outcome($run_id,$run_item_id,$lease_token,$outcome,$result_payload_json=null,$result_sha256=null,$error_code=null,&$error=null)
{
    $error=null;$actor=hrm_pay_core_009_actor();$lease_sha=hash('sha256',(string)$lease_token);if($actor<=0||!hrm_pay_core_009_access('SA_HRM_EXECUTE_PAY_CORE_RUN')){$error='access_denied';return false;}begin_transaction();$run=hrm_pay_core_009_get_run($run_id,true);if(!$run||!hrm_pay_core_009_assert_lease_locked($run,$lease_sha,$le)){cancel_transaction();$error=$run?$le:'run_missing';return false;}$item=hrm_pay_core_009_one('SELECT * FROM '.TB_PREF.'hrm_pay_core_009_items WHERE run_item_id='.(int)$run_item_id.' AND run_id='.(int)$run_id.' LIMIT 2 FOR UPDATE');if(!$item||$item['item_state']!=='running'||empty($item['lease_token_sha256'])||!hash_equals($item['lease_token_sha256'],$lease_sha)){cancel_transaction();$error='item_predecessor_stale';return false;}$target=$outcome==='completed'?'completed':(((int)$item['attempt_no']>=HRM_PAY_CORE_009_MAX_ATTEMPTS)?'quarantined':'failed');if($target==='completed'&&(!is_string($result_payload_json)||$result_payload_json===''||!hrm_pay_core_009_is_sha256($result_sha256)||!hash_equals($result_sha256,hash('sha256',$result_payload_json)))){cancel_transaction();$error='result_evidence_invalid';return false;}$code=$target==='completed'?null:(hrm_pay_core_009_token((string)$error_code,96)?:'worker_failed');$scope=array('legal_entity_id'=>(int)$item['legal_entity_id'],'payroll_relationship_id'=>(int)$item['payroll_relationship_id'],'worker_id'=>(int)$item['worker_id'],'person_id'=>(int)$item['person_id'],'assignment_id'=>(int)$item['assignment_id'],'calendar_period_id'=>$item['calendar_period_id']===null?null:(int)$item['calendar_period_id']);$desc=hrm_pay_core_009_item_descriptor($run['run_key'],$item['ordinal_no'],$item['employee_id'],$scope,$item['input_sha256'],$target,(int)$item['state_version']+1,(int)$item['attempt_no'],$target==='completed'?$result_sha256:null,$de);if($desc===false){cancel_transaction();$error=$de;return false;}$sql='UPDATE '.TB_PREF.'hrm_pay_core_009_items SET item_state='.db_escape($target).',state_version='.(int)$desc['state_version'].',item_state_sha256='.db_escape($desc['item_state_sha256']).',result_sha256='.($target==='completed'?db_escape($result_sha256):'NULL').',result_payload_json='.($target==='completed'?db_escape($result_payload_json):'NULL').',lease_token_sha256=NULL,last_error_code='.($code===null?'NULL':db_escape($code)).' WHERE run_item_id='.(int)$run_item_id.' AND state_version='.(int)$item['state_version'].' AND item_state_sha256='.db_escape($item['item_state_sha256']);if(!db_query($sql,'could not record PAY-CORE-009 item outcome')||db_num_affected_rows()!==1){cancel_transaction();$error='item_outcome_stale';return false;}$worker='actor-'.$actor;$evidence=$target==='completed'?$result_sha256:hrm_pay_core_009_hash(array('run_key'=>$run['run_key'],'ordinal'=>(int)$item['ordinal_no'],'attempt'=>(int)$item['attempt_no'],'error_code'=>$code));$attempt=hrm_pay_core_009_attempt($run['run_key'],$item['ordinal_no'],$item['attempt_no'],'item-'.$item['ordinal_no'].'-attempt-'.$item['attempt_no'],$worker,$item['item_state_sha256'],$target,$target==='completed'?$result_sha256:null,$code,$evidence,$ae);if($attempt===false||!db_query('INSERT INTO '.TB_PREF.'hrm_pay_core_009_attempts (run_id,run_item_id,attempt_no,command_key,worker_sha256,predecessor_state_sha256,outcome,result_sha256,error_code,evidence_sha256,attempt_sha256,executed_by) VALUES ('.(int)$run_id.','.(int)$run_item_id.','.(int)$item['attempt_no'].','.db_escape($attempt['command_key']).','.db_escape($attempt['worker_sha256']).','.db_escape($attempt['predecessor_state_sha256']).','.db_escape($target).','.($target==='completed'?db_escape($result_sha256):'NULL').','.($code===null?'NULL':db_escape($code)).','.db_escape($evidence).','.db_escape($attempt['attempt_sha256']).','.$actor.')','could not append PAY-CORE-009 attempt')){cancel_transaction();$error=$ae?:'attempt_evidence_failed';return false;}if($target==='quarantined'){$qsha=hrm_pay_core_009_hash(array('attempt_sha256'=>$attempt['attempt_sha256'],'reason_code'=>$code,'evidence_sha256'=>$evidence));if(!db_query('INSERT INTO '.TB_PREF.'hrm_pay_core_009_quarantine (run_id,run_item_id,attempt_sha256,reason_code,evidence_sha256,quarantine_sha256,quarantined_by) VALUES ('.(int)$run_id.','.(int)$run_item_id.','.db_escape($attempt['attempt_sha256']).','.db_escape($code).','.db_escape($evidence).','.db_escape($qsha).','.$actor.')')){cancel_transaction();$error='quarantine_evidence_failed';return false;}}commit_transaction();return $target;
}

/** Append checkpoint, then CAS run to running/finalizing/failed/quarantined. */
function hrm_pay_core_009_checkpoint_run($run_id,$lease_token,&$error=null)
{
    $error=null;$actor=hrm_pay_core_009_actor();$sha=hash('sha256',(string)$lease_token);begin_transaction();$run=hrm_pay_core_009_get_run($run_id,true);if(!$run||!hrm_pay_core_009_assert_lease_locked($run,$sha,$le)||$run['state']!=='running'){cancel_transaction();$error=$run?($le?:'checkpoint_state_invalid'):'run_missing';return false;}$q=db_query('SELECT item_state,item_state_sha256 FROM '.TB_PREF.'hrm_pay_core_009_items WHERE run_id='.(int)$run_id.' ORDER BY ordinal_no FOR UPDATE');if(!$q){cancel_transaction();$error='checkpoint_item_read_failed';return false;}$counts=array('pending'=>0,'running'=>0,'completed'=>0,'failed'=>0,'quarantined'=>0);$hashes=array();while($x=db_fetch_assoc($q)){if(isset($counts[$x['item_state']]))$counts[$x['item_state']]++;$hashes[]=$x['item_state_sha256'];}$item_set=hrm_pay_core_009_hash($hashes);$nextcp=(int)$run['checkpoint_no']+1;$evidence=hrm_pay_core_009_hash(array('run_key'=>$run['run_key'],'request_sha256'=>$run['request_sha256'],'checkpoint_no'=>$nextcp,'counts'=>$counts,'item_set_sha256'=>$item_set));$target=$counts['quarantined']>0?'quarantined':($counts['failed']>0?'failed':(($counts['pending']===0&&$counts['running']===0)?'finalizing':'running'));
    $from=hrm_pay_core_009_run_state_from_row($run,$se);if($from===false){cancel_transaction();$error=$se;return false;}$to=hrm_pay_core_009_state_descriptor($run['run_key'],$target,(int)$run['state_version']+1,$run['request_sha256'],$nextcp,$sha,$te);if($to===false){cancel_transaction();$error=$te;return false;}$cp=hrm_pay_core_009_checkpoint($run['run_key'],$nextcp,$counts['completed'],$counts['failed'],$counts['quarantined'],$item_set,$to['state_sha256'],$evidence,$ce);if($cp===false){cancel_transaction();$error=$ce;return false;}if(!db_query('INSERT INTO '.TB_PREF.'hrm_pay_core_009_checkpoints (run_id,checkpoint_no,completed_items,failed_items,quarantined_items,item_set_sha256,state_sha256,evidence_sha256,checkpoint_sha256,created_by) VALUES ('.(int)$run_id.','.$nextcp.','.$counts['completed'].','.$counts['failed'].','.$counts['quarantined'].','.db_escape($item_set).','.db_escape($to['state_sha256']).','.db_escape($evidence).','.db_escape($cp['checkpoint_sha256']).','.$actor.')')||!db_query('UPDATE '.TB_PREF.'hrm_pay_core_009_runs SET state='.db_escape($target).',state_version='.(int)$to['state_version'].',state_sha256='.db_escape($to['state_sha256']).',checkpoint_no='.$nextcp.' WHERE run_id='.(int)$run_id.' AND state_version='.(int)$run['state_version'].' AND state_sha256='.db_escape($run['state_sha256']))||db_num_affected_rows()!==1||!hrm_pay_core_009_append_transition((int)$run_id,$from,$to,'checkpoint-'.$nextcp,$actor)){cancel_transaction();$error='checkpoint_cas_failed';return false;}commit_transaction();return array('state'=>$target,'counts'=>$counts,'checkpoint_no'=>$nextcp);
}

/** Safe status projection: no employee identities, compensation, hashes or payloads. */
function hrm_pay_core_009_status($run_id,&$error=null)
{
    $error=null;$actor=hrm_pay_core_009_actor();if($actor<=0||(!hrm_pay_core_009_access('SA_HRM_VIEW_PAY_CORE_RUN')&&!hrm_pay_core_009_access('SA_HRM_EXECUTE_PAY_CORE_RUN'))){$error='access_denied';return false;}$run=hrm_pay_core_009_get_run($run_id,false);if(!$run||!hrm_pay_core_009_run_owned_by_actor($run,$actor)){$error='run_missing_or_denied';return false;}$q=db_query('SELECT item_state,COUNT(*) n FROM '.TB_PREF.'hrm_pay_core_009_items WHERE run_id='.(int)$run_id.' GROUP BY item_state');if(!$q){$error='status_read_failed';return false;}$counts=array('pending'=>0,'running'=>0,'completed'=>0,'failed'=>0,'quarantined'=>0);while($x=db_fetch_assoc($q))if(isset($counts[$x['item_state']]))$counts[$x['item_state']]=(int)$x['n'];return array('run_id'=>(int)$run_id,'payroll_period_id'=>(int)$run['payroll_period_id'],'state'=>$run['state'],'checkpoint_no'=>(int)$run['checkpoint_no'],'counts'=>$counts,'can_resume'=>in_array($run['state'],array('queued','running','finalizing'),true),'needs_recovery'=>in_array($run['state'],array('failed','quarantined'),true));
}

/** List only safe open-run summaries owned by the current actor. */
function hrm_pay_core_009_open_runs_for_actor(&$error=null)
{
    $error=null;$actor=hrm_pay_core_009_actor();if($actor<=0||(!hrm_pay_core_009_access('SA_HRM_VIEW_PAY_CORE_RUN')&&!hrm_pay_core_009_access('SA_HRM_EXECUTE_PAY_CORE_RUN'))){$error='access_denied';return false;}$q=db_query('SELECT run_id FROM '.TB_PREF.'hrm_pay_core_009_runs WHERE created_by='.$actor." AND state NOT IN ('completed','cancelled') ORDER BY run_id DESC LIMIT 20");if(!$q){$error='run_list_failed';return false;}$out=array();while($r=db_fetch_assoc($q)){$x=hrm_pay_core_009_status((int)$r['run_id'],$se);if($x!==false)$out[]=$x;}return $out;
}

/** Approval BOLA/state guard. Historical periods without a PAY-CORE-009 run remain backward compatible. */
function hrm_pay_core_009_period_ready_for_approval($payroll_period_id,&$error=null)
{
    $error=null;$q=db_query('SELECT run_id,state FROM '.TB_PREF.'hrm_pay_core_009_runs WHERE payroll_period_id='.(int)$payroll_period_id.' LIMIT 2');if(!$q){$error='runner_state_unavailable';return false;}if(db_num_rows($q)===0)return true;if(db_num_rows($q)!==1){$error='runner_state_ambiguous';return false;}$r=db_fetch_assoc($q);if($r['state']!=='completed'){$error='runner_not_completed';return false;}return true;
}

/** Explicit recovery creates successor evidence; historical evidence is never rewritten. */
function hrm_pay_core_009_recover_run($run_id,$action,&$error=null)
{
    $error=null;$actor=hrm_pay_core_009_actor();if($actor<=0||!hrm_pay_core_009_access('SA_HRM_RECOVER_PAY_CORE_RUN')){$error='access_denied';return false;}begin_transaction();$run=hrm_pay_core_009_get_run($run_id,true);if(!$run||!hrm_pay_core_009_run_owned_by_actor($run,$actor)){cancel_transaction();$error='run_missing_or_denied';return false;}$pre=hrm_pay_core_009_run_state_from_row($run,$se);if($pre===false){cancel_transaction();$error=$se;return false;}$action=hrm_pay_core_009_token($action,64);if(!$action){cancel_transaction();$error='recovery_action_invalid';return false;}$evidence=hrm_pay_core_009_hash(array('run_key'=>$run['run_key'],'action'=>$action,'predecessor_state_sha256'=>$pre['state_sha256'],'actor_id'=>$actor));$rec=hrm_pay_core_009_recovery($run['run_key'],$action,$pre['state_sha256'],$evidence,$actor,$re);if($rec===false){cancel_transaction();$error=$re;return false;}
    if($action==='expire_lease'){if(empty($run['lease_expires_at'])||strtotime($run['lease_expires_at'].' UTC')>time()){cancel_transaction();$error='lease_not_expired';return false;}$x=hrm_pay_core_009_recover_expired_items_locked($run,$actor,$xe);if($x===false){cancel_transaction();$error=$xe;return false;}$state=hrm_pay_core_009_state_descriptor($run['run_key'],$run['state'],$run['state_version'],$run['request_sha256'],$run['checkpoint_no'],null,$ne);if($state===false||!db_query('UPDATE '.TB_PREF.'hrm_pay_core_009_runs SET lease_worker_sha256=NULL,lease_token_sha256=NULL,lease_expires_at=NULL,heartbeat_at=NULL,state_sha256='.db_escape($state['state_sha256']).' WHERE run_id='.(int)$run_id.' AND state_sha256='.db_escape($run['state_sha256']))||db_num_affected_rows()!==1){cancel_transaction();$error=$ne?:'lease_expiry_recovery_failed';return false;}}
    elseif($action==='resume_failed'||$action==='resume_quarantined'){$want=$action==='resume_failed'?'failed':'quarantined';if($run['state']!==$want){cancel_transaction();$error='recovery_state_mismatch';return false;}$q=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_core_009_items WHERE run_id='.(int)$run_id.' AND item_state='.db_escape($want).' ORDER BY ordinal_no FOR UPDATE');if(!$q){cancel_transaction();$error='recovery_item_read_failed';return false;}while($item=db_fetch_assoc($q)){$scope=array('legal_entity_id'=>(int)$item['legal_entity_id'],'payroll_relationship_id'=>(int)$item['payroll_relationship_id'],'worker_id'=>(int)$item['worker_id'],'person_id'=>(int)$item['person_id'],'assignment_id'=>(int)$item['assignment_id'],'calendar_period_id'=>$item['calendar_period_id']===null?null:(int)$item['calendar_period_id']);$d=hrm_pay_core_009_item_descriptor($run['run_key'],$item['ordinal_no'],$item['employee_id'],$scope,$item['input_sha256'],'pending',(int)$item['state_version']+1,(int)$item['attempt_no'],null,$ie);if($d===false||!db_query('UPDATE '.TB_PREF.'hrm_pay_core_009_items SET item_state=\'pending\',state_version='.(int)$d['state_version'].',item_state_sha256='.db_escape($d['item_state_sha256']).',lease_token_sha256=NULL,last_error_code=NULL WHERE run_item_id='.(int)$item['run_item_id'].' AND state_version='.(int)$item['state_version'])){cancel_transaction();$error=$ie?:'recovery_item_cas_failed';return false;}}$run=hrm_pay_core_009_transition_locked($run,'running','recovery-'.$action.'-'.$run['state_version'],$te);if($run===false){cancel_transaction();$error=$te;return false;}}
    elseif($action==='cancel'){if(!in_array($run['state'],array('running','failed','quarantined'),true)){cancel_transaction();$error='cancel_state_invalid';return false;}$run=hrm_pay_core_009_transition_locked($run,'cancelled','recovery-cancel-'.$run['state_version'],$te);if($run===false){cancel_transaction();$error=$te;return false;}}
    elseif($action==='reconcile'){/* runner performs authoritative result reconciliation outside this transaction */}
    else {cancel_transaction();$error='recovery_action_unsupported';return false;}
    if(!db_query('INSERT INTO '.TB_PREF.'hrm_pay_core_009_recovery_evidence (run_id,action_token,predecessor_state_sha256,evidence_sha256,recovery_sha256,executed_by) VALUES ('.(int)$run_id.','.db_escape($action).','.db_escape($pre['state_sha256']).','.db_escape($evidence).','.db_escape($rec['recovery_sha256']).','.$actor.')')){cancel_transaction();$error='recovery_evidence_failed';return false;}commit_transaction();return true;
}
?>
