<?php
/** Governed HRM-TIM-001 writers. Raw and approved evidence is append-only. */
include_once(dirname(dirname(__FILE__)).'/time/hrm_tim_001_contract.inc');
include_once(dirname(__FILE__).'/pay_core_003_governance_db.inc');

function hrm_tim_001_actor(){return isset($_SESSION['wa_current_user']->user)?(int)$_SESSION['wa_current_user']->user:0;}
function hrm_tim_001_access($area){return function_exists('user_check_access')&&user_check_access($area);}
function hrm_tim_001_insert_idempotent($table,$id_col,$hash_col,$hash,$sql,&$error){$q=db_query('SELECT '.$id_col.','.$hash_col.' FROM '.TB_PREF.$table.' WHERE '.$hash_col.'='.db_escape($hash).' LIMIT 2 FOR UPDATE');if(!$q||db_num_rows($q)>1){$error='idempotency_state_invalid';return false;}if(db_num_rows($q)===1){$r=db_fetch_assoc($q);return (int)$r[$id_col];}if(!db_query($sql,'could not append HRM-TIM-001 custody')){$error='insert_failed';return false;}return (int)db_insert_id();}
function hrm_tim_001_db_date($v){return hrm_tim_001_date($v)!==false?db_escape($v):false;}

/** Resolve Worker/Employment/Legal Entity only from governed Assignment state. */
function hrm_tim_001_resolve_assignment_scope($assignment_id,$as_of,$for_update=false,&$error=null)
{
    $error=null;$assignment_id=(int)$assignment_id;$as_of=hrm_tim_001_date($as_of);if($assignment_id<=0||$as_of===false){$error='assignment_scope_invalid';return false;}$d=db_escape($as_of);
    $sql='SELECT a.assignment_id,a.employment_id,a.manager_assignment_id,a.work_location_id,a.legacy_shift_id,h.worker_id,h.legal_entity_id '
        .'FROM '.TB_PREF.'hrm_assignments a INNER JOIN '.TB_PREF.'hrm_employments h ON h.employment_id=a.employment_id '
        .'WHERE a.assignment_id='.$assignment_id.' AND (a.effective_from IS NULL OR a.effective_from<='.$d.') AND (a.effective_to IS NULL OR a.effective_to>='.$d.') '
        .'AND (h.start_date IS NULL OR h.start_date<='.$d.') AND (h.end_date IS NULL OR h.end_date>='.$d.') LIMIT 2'.($for_update?' FOR UPDATE':'');
    $q=db_query($sql,'could not resolve HRM-TIM-001 assignment scope');if(!$q||db_num_rows($q)!==1){$error='assignment_scope_ambiguous_or_missing';return false;}$r=db_fetch_assoc($q);foreach(array('assignment_id','employment_id','worker_id','legal_entity_id') as $k)if((int)$r[$k]<=0){$error='assignment_scope_incomplete';return false;}return array('assignment_id'=>(int)$r['assignment_id'],'employment_id'=>(int)$r['employment_id'],'worker_id'=>(int)$r['worker_id'],'legal_entity_id'=>(int)$r['legal_entity_id'],'manager_assignment_id'=>empty($r['manager_assignment_id'])?null:(int)$r['manager_assignment_id'],'work_location_id'=>empty($r['work_location_id'])?null:(int)$r['work_location_id'],'legacy_shift_id'=>empty($r['legacy_shift_id'])?null:(int)$r['legacy_shift_id']);
}
function hrm_tim_001_resolve_payroll_relationship($assignment_id,$as_of,$for_update=false,&$error=null)
{
    $error=null;$scope=hrm_tim_001_resolve_assignment_scope($assignment_id,$as_of,$for_update,$error);if($scope===false)return false;$d=db_escape($as_of);
    $sql='SELECT r.payroll_relationship_id,r.worker_id,r.employment_id,r.legal_entity_id,m.assignment_id FROM '.TB_PREF.'hrm_payroll_relationship_members m INNER JOIN '.TB_PREF.'hrm_payroll_relationships r ON r.payroll_relationship_id=m.payroll_relationship_id '
        .'WHERE m.assignment_id='.(int)$assignment_id.' AND m.effective_from<='.$d.' AND (m.effective_to IS NULL OR m.effective_to>='.$d.') '
        .'AND r.effective_from<='.$d.' AND (r.effective_to IS NULL OR r.effective_to>='.$d.') LIMIT 2'.($for_update?' FOR UPDATE':'');
    $q=db_query($sql,'could not resolve HRM-TIM-001 payroll relationship');if(!$q||db_num_rows($q)!==1){$error='payroll_relationship_ambiguous_or_missing';return false;}$r=db_fetch_assoc($q);if((int)$r['worker_id']!==$scope['worker_id']||(int)$r['assignment_id']!==$scope['assignment_id']||(int)$r['legal_entity_id']!==$scope['legal_entity_id']||(!empty($r['employment_id'])&&(int)$r['employment_id']!==$scope['employment_id'])){$error='payroll_relationship_scope_mismatch';return false;}$scope['payroll_relationship_id']=(int)$r['payroll_relationship_id'];return $scope;
}
function hrm_tim_001_actor_worker_id(){global $path_to_root;if(!function_exists('get_hrm_ess_current_self_scope'))@include_once($path_to_root.'/hrm/includes/db/ess_self_scope_db.inc');if(!function_exists('get_hrm_ess_current_self_scope'))return false;$s=get_hrm_ess_current_self_scope();return is_array($s)&&!empty($s['worker_id'])?(int)$s['worker_id']:false;}
function hrm_tim_001_actor_can_review_scope(array $scope,$as_of)
{
    if(hrm_tim_001_access('SA_HRM_MANAGE_TIME'))return true;
    $actor_worker=hrm_tim_001_actor_worker_id();
    $as_of=hrm_tim_001_date($as_of);
    $manager_assignment_id=empty($scope['manager_assignment_id'])?0:(int)$scope['manager_assignment_id'];
    if($actor_worker===false||$as_of===false||$manager_assignment_id<=0)return false;
    $d=db_escape($as_of);
    $q=db_query('SELECT a.assignment_id,h.legal_entity_id FROM '.TB_PREF.'hrm_assignments a '
        .'INNER JOIN '.TB_PREF.'hrm_employments h ON h.employment_id=a.employment_id '
        .'WHERE h.worker_id='.(int)$actor_worker.' AND a.assignment_id='.$manager_assignment_id.' '
        .'AND (a.effective_from IS NULL OR a.effective_from<='.$d.') AND (a.effective_to IS NULL OR a.effective_to>='.$d.') '
        .'AND (h.start_date IS NULL OR h.start_date<='.$d.') AND (h.end_date IS NULL OR h.end_date>='.$d.') LIMIT 2');
    if(!$q||db_num_rows($q)!==1)return false;
    $m=db_fetch_assoc($q);
    return (int)$m['legal_entity_id']===(int)$scope['legal_entity_id'];
}
function hrm_tim_001_create_work_pattern($assignment_id,array $definition,&$error=null)
{
    $error=null;$actor=hrm_tim_001_actor();
    if($actor<=0||!hrm_tim_001_access('SA_HRM_MANAGE_TIME')){$error='access_denied';return false;}
    $from=$definition['effective_from']??null;
    begin_transaction();
    $scope=hrm_tim_001_resolve_assignment_scope($assignment_id,$from,true,$error);
    if($scope===false){cancel_transaction();return false;}
    foreach(array('worker_id','employment_id','legal_entity_id') as $k){
        if(isset($definition[$k])&&(int)$definition[$k]!==$scope[$k]){cancel_transaction();$error='caller_scope_mismatch';return false;}
    }
    $x=$definition+$scope;$x['created_by']=$actor;
    $desc=hrm_tim_001_work_pattern_descriptor($x,$error);
    if($desc===false){cancel_transaction();return false;}

    /* Exact replay is idempotent before moving-head predecessor checks. */
    $q=db_query('SELECT work_pattern_id FROM '.TB_PREF.'hrm_time_work_patterns WHERE pattern_sha256='.db_escape($desc['pattern_sha256']).' LIMIT 2 FOR UPDATE');
    if(!$q||db_num_rows($q)>1){cancel_transaction();$error='work_pattern_replay_state_invalid';return false;}
    if(db_num_rows($q)===1){$r=db_fetch_assoc($q);commit_transaction();return array('work_pattern_id'=>(int)$r['work_pattern_id'],'pattern_sha256'=>$desc['pattern_sha256'],'replay'=>true);}

    $q=db_query('SELECT work_pattern_id,effective_from,effective_to,pattern_sha256 FROM '.TB_PREF.'hrm_time_work_patterns WHERE assignment_id='.$scope['assignment_id'].' ORDER BY effective_from DESC,work_pattern_id DESC LIMIT 1 FOR UPDATE');
    $latest=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    $pred=empty($desc['predecessor_pattern_id'])?null:(int)$desc['predecessor_pattern_id'];
    if(($latest===false&&$pred!==null)||($latest!==false&&$pred!==(int)$latest['work_pattern_id'])){cancel_transaction();$error='work_pattern_predecessor_mismatch';return false;}
    if($latest!==false&&$desc['effective_from']<=(string)$latest['effective_from']){cancel_transaction();$error='work_pattern_effective_order_invalid';return false;}
    /* A null predecessor end is semantically closed by its linear successor. An explicit end may not be overlapped. */
    if($latest!==false&&!empty($latest['effective_to'])&&$desc['effective_from']<=(string)$latest['effective_to']){cancel_transaction();$error='work_pattern_effective_overlap';return false;}

    $sql='INSERT INTO '.TB_PREF.'hrm_time_work_patterns (worker_id,employment_id,assignment_id,legal_entity_id,effective_from,effective_to,timezone_name,cycle_days,pattern_code,source_kind,predecessor_pattern_id,pattern_sha256,created_by) VALUES ('
        .$scope['worker_id'].','.$scope['employment_id'].','.$scope['assignment_id'].','.$scope['legal_entity_id'].','.db_escape($desc['effective_from']).','
        .($desc['effective_to']===null?'NULL':db_escape($desc['effective_to'])).','.db_escape($desc['timezone_name']).','.(int)$desc['cycle_days'].','
        .db_escape($desc['pattern_code']).','.db_escape($desc['source_kind']).','.($pred===null?'NULL':$pred).','.db_escape($desc['pattern_sha256']).','.$actor.')';
    $id=hrm_tim_001_insert_idempotent('hrm_time_work_patterns','work_pattern_id','pattern_sha256',$desc['pattern_sha256'],$sql,$error);
    if($id===false){cancel_transaction();return false;}
    foreach($desc['segments'] as $seg){
        $linkhash=hrm_tim_001_hash(array('work_pattern_id'=>$id,'segment_sha256'=>$seg['segment_sha256']));
        $sql='INSERT INTO '.TB_PREF.'hrm_time_work_pattern_segments (work_pattern_id,cycle_day,segment_sequence,local_start_time,local_end_time,crosses_midnight,paid_minutes,break_minutes,rest_minutes,segment_sha256) VALUES ('
            .$id.','.(int)$seg['cycle_day'].','.(int)$seg['segment_sequence'].','.db_escape($seg['local_start_time']).','.db_escape($seg['local_end_time']).','.(int)$seg['crosses_midnight'].','
            .(int)$seg['paid_minutes'].','.(int)$seg['break_minutes'].','.(int)$seg['rest_minutes'].','.db_escape($linkhash).')';
        if(hrm_tim_001_insert_idempotent('hrm_time_work_pattern_segments','segment_id','segment_sha256',$linkhash,$sql,$error)===false){cancel_transaction();return false;}
    }
    commit_transaction();return array('work_pattern_id'=>$id,'pattern_sha256'=>$desc['pattern_sha256'],'replay'=>false);
}
function hrm_tim_001_ingest_raw_event($assignment_id,array $event,&$error=null)
{
    $error=null;$actor=hrm_tim_001_actor();
    if($actor<=0||!hrm_tim_001_access('SA_HRM_INGEST_TIME')){$error='access_denied';return false;}
    $local=$event['effective_local_at']??'';
    $work_date=is_string($local)&&strlen($local)>=10?substr($local,0,10):'';
    $resolve_error=null;
    $resolved=hrm_tim_001_local_to_utc($local,$event['timezone_name']??'',array_key_exists('utc_offset_minutes',$event)?$event['utc_offset_minutes']:null,$resolve_error);
    if($resolved===false){$error=$resolve_error?:'raw_event_time_resolution_invalid';return false;}
    $command=hrm_tim_001_token($event['ingest_command_key']??'',128);$channel=hrm_tim_001_token($event['source_channel']??'',32);$source_key=hrm_tim_001_token($event['source_event_key']??'',128);$recorded=hrm_tim_001_utc_datetime($event['recorded_at_utc']??'');$type=(string)($event['event_type']??'');$payload=(string)($event['source_payload_sha256']??'');$pred=empty($event['predecessor_event_id'])?null:(int)$event['predecessor_event_id'];
    if($command===false||$channel===false||$source_key===false||$recorded===false||!in_array($type,array('clock_in','clock_out','manual_anchor','legacy_attendance'),true)||!hrm_tim_001_is_sha256($payload)){$error='raw_event_identity_invalid';return false;}
    begin_transaction();
    /* Exact command replay is stable even after assignment/workflow state advances. */
    $q=db_query('SELECT raw_event_id,assignment_id,source_channel,source_event_key,recorded_at_utc,effective_local_at,timezone_name,effective_utc_at,utc_offset_minutes,event_type,source_payload_sha256,predecessor_event_id,ingest_command_key,event_sha256,recorded_by FROM '.TB_PREF.'hrm_time_raw_events WHERE ingest_command_key='.db_escape($command).' LIMIT 2 FOR UPDATE');
    if(!$q||db_num_rows($q)>1){cancel_transaction();$error='ingest_command_state_invalid';return false;}
    if(db_num_rows($q)===1){$old=db_fetch_assoc($q);$same=(int)$old['assignment_id']===(int)$assignment_id&&(string)$old['source_channel']===$channel&&(string)$old['source_event_key']===$source_key&&(string)$old['recorded_at_utc']===$recorded&&(string)$old['effective_local_at']===$local&&(string)$old['timezone_name']===(string)($event['timezone_name']??'')&&(string)$old['effective_utc_at']===$resolved['utc']&&(int)$old['utc_offset_minutes']===(int)$resolved['offset_minutes']&&(string)$old['event_type']===$type&&hash_equals((string)$old['source_payload_sha256'],$payload)&&((empty($old['predecessor_event_id'])?null:(int)$old['predecessor_event_id'])===$pred)&&(int)$old['recorded_by']===$actor;if($same){commit_transaction();return array('raw_event_id'=>(int)$old['raw_event_id'],'event_sha256'=>$old['event_sha256'],'effective_utc_at'=>$old['effective_utc_at'],'replay'=>true);}cancel_transaction();$error='ingest_command_replay_mismatch';return false;}
    $scope=hrm_tim_001_resolve_assignment_scope($assignment_id,$work_date,true,$error);
    if($scope===false){cancel_transaction();return false;}
    foreach(array('worker_id','employment_id','legal_entity_id') as $k){
        if(isset($event[$k])&&(int)$event[$k]!==$scope[$k]){cancel_transaction();$error='caller_scope_mismatch';return false;}
    }
    $x=$event+$scope;$x['recorded_by']=$actor;$x['effective_utc_at']=$resolved['utc'];$x['utc_offset_minutes']=$resolved['offset_minutes'];
    $desc=hrm_tim_001_raw_event_descriptor($x,$error);
    if($desc===false){cancel_transaction();return false;}

    /* One command identity cannot be reused for different content. */
    $q=db_query('SELECT raw_event_id,event_sha256 FROM '.TB_PREF.'hrm_time_raw_events WHERE ingest_command_key='.db_escape($desc['ingest_command_key']).' LIMIT 2 FOR UPDATE');
    if(!$q||db_num_rows($q)>1){cancel_transaction();$error='ingest_command_state_invalid';return false;}
    if(db_num_rows($q)===1){$r=db_fetch_assoc($q);if(hash_equals((string)$r['event_sha256'],$desc['event_sha256'])){commit_transaction();return array('raw_event_id'=>(int)$r['raw_event_id'],'event_sha256'=>$desc['event_sha256'],'effective_utc_at'=>$desc['effective_utc_at'],'replay'=>true);}cancel_transaction();$error='ingest_command_replay_mismatch';return false;}

    if($desc['predecessor_event_id']!==null){
        $pred=(int)$desc['predecessor_event_id'];
        $q=db_query('SELECT raw_event_id,worker_id,employment_id,assignment_id,legal_entity_id FROM '.TB_PREF.'hrm_time_raw_events WHERE raw_event_id='.$pred.' LIMIT 2 FOR UPDATE');
        $pr=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
        if(!$pr||(int)$pr['worker_id']!==$scope['worker_id']||(int)$pr['employment_id']!==$scope['employment_id']||(int)$pr['assignment_id']!==$scope['assignment_id']||(int)$pr['legal_entity_id']!==$scope['legal_entity_id']){cancel_transaction();$error='raw_event_predecessor_scope_mismatch';return false;}
        $q=db_query('SELECT raw_event_id FROM '.TB_PREF.'hrm_time_raw_events WHERE predecessor_event_id='.$pred.' LIMIT 2 FOR UPDATE');
        if(!$q||db_num_rows($q)>0){cancel_transaction();$error='raw_event_predecessor_already_superseded';return false;}
    }

    $q=db_query('SELECT raw_event_id,event_sha256,source_payload_sha256 FROM '.TB_PREF.'hrm_time_raw_events WHERE source_channel='.db_escape($desc['source_channel']).' AND source_event_key='.db_escape($desc['source_event_key']).' LIMIT 2 FOR UPDATE');
    if(!$q||db_num_rows($q)>1){cancel_transaction();$error='source_event_state_invalid';return false;}
    if(db_num_rows($q)===1){
        $r=db_fetch_assoc($q);
        if(hash_equals((string)$r['event_sha256'],$desc['event_sha256'])){commit_transaction();return array('raw_event_id'=>(int)$r['raw_event_id'],'event_sha256'=>$desc['event_sha256'],'effective_utc_at'=>$desc['effective_utc_at'],'replay'=>true);}
        $conflict=hrm_tim_001_hash(array('existing_raw_event_id'=>(int)$r['raw_event_id'],'existing_event_sha256'=>$r['event_sha256'],'attempted_event_sha256'=>$desc['event_sha256'],'attempted_payload_sha256'=>$desc['source_payload_sha256'],'ingest_command_key'=>$desc['ingest_command_key'],'recorded_by'=>$actor));
        $sql='INSERT INTO '.TB_PREF.'hrm_time_raw_event_conflicts (source_channel,source_event_key,existing_raw_event_id,existing_event_sha256,attempted_event_sha256,attempted_payload_sha256,ingest_command_key,conflict_sha256,recorded_by) VALUES ('
            .db_escape($desc['source_channel']).','.db_escape($desc['source_event_key']).','.(int)$r['raw_event_id'].','.db_escape($r['event_sha256']).','.db_escape($desc['event_sha256']).','.db_escape($desc['source_payload_sha256']).','.db_escape($desc['ingest_command_key']).','.db_escape($conflict).','.$actor.')';
        $dummy=null;$conflict_id=hrm_tim_001_insert_idempotent('hrm_time_raw_event_conflicts','conflict_id','conflict_sha256',$conflict,$sql,$dummy);
        if($conflict_id===false){cancel_transaction();$error='conflict_evidence_append_failed';return false;}
        commit_transaction();$error='conflicting_duplicate_raw_event';return false;
    }
    $sql='INSERT INTO '.TB_PREF.'hrm_time_raw_events (worker_id,employment_id,assignment_id,legal_entity_id,source_channel,source_event_key,recorded_at_utc,effective_local_at,timezone_name,effective_utc_at,utc_offset_minutes,event_type,source_payload_sha256,predecessor_event_id,ingest_command_key,event_sha256,recorded_by) VALUES ('
        .$scope['worker_id'].','.$scope['employment_id'].','.$scope['assignment_id'].','.$scope['legal_entity_id'].','.db_escape($desc['source_channel']).','.db_escape($desc['source_event_key']).','.db_escape($desc['recorded_at_utc']).','.db_escape($desc['effective_local_at']).','.db_escape($desc['timezone_name']).','.db_escape($desc['effective_utc_at']).','.(int)$desc['utc_offset_minutes'].','.db_escape($desc['event_type']).','.db_escape($desc['source_payload_sha256']).','.($desc['predecessor_event_id']===null?'NULL':(int)$desc['predecessor_event_id']).','.db_escape($desc['ingest_command_key']).','.db_escape($desc['event_sha256']).','.$actor.')';
    $id=hrm_tim_001_insert_idempotent('hrm_time_raw_events','raw_event_id','event_sha256',$desc['event_sha256'],$sql,$error);
    if($id===false){cancel_transaction();return false;}
    commit_transaction();return array('raw_event_id'=>$id,'event_sha256'=>$desc['event_sha256'],'effective_utc_at'=>$desc['effective_utc_at'],'replay'=>false);
}
function hrm_tim_001_review_time($assignment_id,array $review,array $raw_event_ids,array $allocations,&$error=null)
{
    $error=null;$actor=hrm_tim_001_actor();
    if($actor<=0||!hrm_tim_001_access('SA_HRM_REVIEW_TIME')){$error='access_denied';return false;}
    $date=$review['work_date']??'';$cmd=hrm_tim_001_token($review['review_command_key']??'',128);$start=hrm_tim_001_utc_datetime($review['period_start_utc']??'');$end=hrm_tim_001_utc_datetime($review['period_end_utc']??'');
    if(hrm_tim_001_date($date)===false||$cmd===false||$start===false||$end===false){$error='review_identity_invalid';return false;}
    $ids=array_values(array_unique(array_map('intval',$raw_event_ids)));sort($ids,SORT_NUMERIC);if(!$ids||in_array(0,$ids,true)){$error='raw_event_set_invalid';return false;}
    $requested_allocs=array();foreach($allocations as $a){$cat=(string)($a['allocation_category']??'');$qty=(int)($a['quantity_minutes']??0);if(!in_array($cat,array('worked_time','legacy_regular','legacy_overtime','paid_break'),true)||$qty<=0){$error='allocation_invalid';return false;}$requested_allocs[]=array($cat,$qty);}sort($requested_allocs);
    begin_transaction();
    /* Exact command replay returns immutable evidence before moving-state validation. */
    $q=db_query('SELECT reviewed_entry_id,assignment_id,work_date,period_start_utc,period_end_utc,reviewer_id,review_sha256 FROM '.TB_PREF.'hrm_time_reviewed_entries WHERE review_command_key='.db_escape($cmd).' LIMIT 2 FOR UPDATE');
    if(!$q||db_num_rows($q)>1){cancel_transaction();$error='review_command_state_invalid';return false;}
    if(db_num_rows($q)===1){$old=db_fetch_assoc($q);$oldid=(int)$old['reviewed_entry_id'];$lq=db_query('SELECT raw_event_id FROM '.TB_PREF.'hrm_time_reviewed_event_links WHERE reviewed_entry_id='.$oldid.' ORDER BY raw_event_id FOR UPDATE');$oldids=array();while($lq&&$z=db_fetch_assoc($lq))$oldids[]=(int)$z['raw_event_id'];$aq=db_query('SELECT allocation_id,allocation_sha256,quantity_minutes,allocation_category FROM '.TB_PREF.'hrm_time_reviewed_allocations WHERE reviewed_entry_id='.$oldid.' ORDER BY allocation_id FOR UPDATE');$oldallocs=array();$final_allocs=array();$allocation_ids=array();while($aq&&$a=db_fetch_assoc($aq)){$allocation_ids[]=(int)$a['allocation_id'];$oldallocs[]=array((string)$a['allocation_category'],(int)$a['quantity_minutes']);$final_allocs[]=array('allocation_id'=>(int)$a['allocation_id'],'allocation_sha256'=>$a['allocation_sha256'],'quantity_minutes'=>(int)$a['quantity_minutes'],'allocation_category'=>$a['allocation_category']);}sort($oldallocs);$same=$lq&&$aq&&(int)$old['assignment_id']===(int)$assignment_id&&(int)$old['reviewer_id']===$actor&&(string)$old['work_date']===$date&&(string)$old['period_start_utc']===$start&&(string)$old['period_end_utc']===$end&&$oldids===$ids&&$oldallocs===$requested_allocs;if($same&&$final_allocs){$allocation_set_sha=hrm_tim_001_hash($final_allocs);commit_transaction();return array('reviewed_entry_id'=>$oldid,'review_sha256'=>$old['review_sha256'],'allocation_ids'=>$allocation_ids,'allocation_set_sha256'=>$allocation_set_sha,'replay'=>true);}cancel_transaction();$error='review_command_replay_mismatch';return false;}
    $scope=hrm_tim_001_resolve_assignment_scope($assignment_id,$date,true,$error);
    if($scope===false||!hrm_tim_001_actor_can_review_scope($scope,$date)){cancel_transaction();$error=$error?:'review_scope_denied';return false;}
    $rel=hrm_tim_001_resolve_payroll_relationship($assignment_id,$date,true,$error);
    if($rel===false){cancel_transaction();return false;}

    $ids=array_values(array_unique(array_map('intval',$raw_event_ids)));sort($ids,SORT_NUMERIC);
    if(!$ids||in_array(0,$ids,true)){cancel_transaction();$error='raw_event_set_invalid';return false;}
    $events=array();
    $q=db_query('SELECT raw_event_id,event_sha256,effective_utc_at,worker_id,employment_id,assignment_id,legal_entity_id FROM '.TB_PREF.'hrm_time_raw_events WHERE raw_event_id IN ('.implode(',',$ids).') ORDER BY raw_event_id FOR UPDATE');
    while($q&&$r=db_fetch_assoc($q))$events[]=$r;
    if(!$q||count($events)!==count($ids)){cancel_transaction();$error='raw_event_set_missing';return false;}
    foreach($events as $r){
        if((int)$r['worker_id']!==$scope['worker_id']||(int)$r['employment_id']!==$scope['employment_id']||(int)$r['assignment_id']!==$scope['assignment_id']||(int)$r['legal_entity_id']!==$scope['legal_entity_id']){cancel_transaction();$error='raw_event_bola_scope_mismatch';return false;}
    }
    $q=db_query('SELECT raw_event_id FROM '.TB_PREF.'hrm_time_raw_events WHERE predecessor_event_id IN ('.implode(',',$ids).') LIMIT 1 FOR UPDATE');
    if(!$q||db_num_rows($q)>0){cancel_transaction();$error='raw_event_set_contains_superseded_event';return false;}
    $event_set_sha=hrm_tim_001_hash(array_map(function($r){return array('raw_event_id'=>(int)$r['raw_event_id'],'event_sha256'=>$r['event_sha256']);},$events));

    $q=db_query('SELECT work_pattern_id,pattern_sha256 FROM '.TB_PREF.'hrm_time_work_patterns WHERE assignment_id='.$scope['assignment_id'].' AND effective_from<='.db_escape($date).' AND (effective_to IS NULL OR effective_to>='.db_escape($date).') ORDER BY effective_from DESC,work_pattern_id DESC LIMIT 1 FOR UPDATE');
    $pat=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    if(!$pat){cancel_transaction();$error='governed_work_pattern_missing';return false;}

    $alloc_desc=array();
    foreach($allocations as $a){
        $a['reviewed_entry_id']=0;$a['assignment_id']=$scope['assignment_id'];$a['payroll_relationship_id']=$rel['payroll_relationship_id'];
        $d=hrm_tim_001_allocation_descriptor($a,$error);if($d===false){cancel_transaction();return false;}$alloc_desc[]=$d;
    }
    $request_set=hrm_tim_001_allocation_set_descriptor($alloc_desc,$error);
    if($request_set===false){cancel_transaction();return false;}
    $x=$review+$scope;$x['source_event_set_sha256']=$event_set_sha;$x['work_pattern_sha256']=$pat['pattern_sha256'];$x['allocation_set_sha256']=$request_set['allocation_set_sha256'];$x['reviewer_id']=$actor;
    $desc=hrm_tim_001_reviewed_entry_descriptor($x,$error);
    if($desc===false){cancel_transaction();return false;}
    if($request_set['total_minutes']>$desc['duration_minutes']){cancel_transaction();$error='allocation_total_invalid';return false;}
    foreach($events as $r){if($r['effective_utc_at']<$desc['period_start_utc']||$r['effective_utc_at']>$desc['period_end_utc']){cancel_transaction();$error='raw_event_outside_review_interval';return false;}}

    /* Command identity is the idempotency key for reviewed-time creation. */
    $q=db_query('SELECT reviewed_entry_id,review_sha256 FROM '.TB_PREF.'hrm_time_reviewed_entries WHERE review_command_key='.db_escape($desc['review_command_key']).' LIMIT 2 FOR UPDATE');
    if(!$q||db_num_rows($q)>1){cancel_transaction();$error='review_command_state_invalid';return false;}
    if(db_num_rows($q)===1){
        $existing=db_fetch_assoc($q);
        if(!hash_equals((string)$existing['review_sha256'],$desc['review_sha256'])){cancel_transaction();$error='review_command_replay_mismatch';return false;}
        $existing_id=(int)$existing['reviewed_entry_id'];$final_allocs=array();$allocation_ids=array();
        $aq=db_query('SELECT allocation_id,allocation_sha256,quantity_minutes,allocation_category FROM '.TB_PREF.'hrm_time_reviewed_allocations WHERE reviewed_entry_id='.$existing_id.' ORDER BY allocation_id FOR UPDATE');
        while($aq&&$a=db_fetch_assoc($aq)){$allocation_ids[]=(int)$a['allocation_id'];$final_allocs[]=array('allocation_id'=>(int)$a['allocation_id'],'allocation_sha256'=>$a['allocation_sha256'],'quantity_minutes'=>(int)$a['quantity_minutes'],'allocation_category'=>$a['allocation_category']);}
        if(!$aq||!$final_allocs){cancel_transaction();$error='review_replay_allocations_missing';return false;}
        $allocation_set_sha=hrm_tim_001_hash($final_allocs);commit_transaction();return array('reviewed_entry_id'=>$existing_id,'review_sha256'=>$desc['review_sha256'],'allocation_ids'=>$allocation_ids,'allocation_set_sha256'=>$allocation_set_sha,'replay'=>true);
    }

    $q=db_query('SELECT reviewed_entry_id,review_sha256 FROM '.TB_PREF.'hrm_time_reviewed_entries WHERE assignment_id='.$scope['assignment_id'].' AND work_date='.db_escape($date).' ORDER BY reviewed_entry_id DESC LIMIT 1 FOR UPDATE');
    $prev=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;$pred=empty($desc['predecessor_entry_id'])?null:(int)$desc['predecessor_entry_id'];
    if(($prev===false&&$pred!==null)||($prev!==false&&$pred!==(int)$prev['reviewed_entry_id'])){cancel_transaction();$error='stale_review_predecessor';return false;}

    $sql='INSERT INTO '.TB_PREF.'hrm_time_reviewed_entries (worker_id,employment_id,assignment_id,legal_entity_id,work_date,period_start_utc,period_end_utc,duration_minutes,source_event_set_sha256,source_event_count,work_pattern_sha256,review_command_key,predecessor_entry_id,reviewer_id,review_sha256) VALUES ('
        .$scope['worker_id'].','.$scope['employment_id'].','.$scope['assignment_id'].','.$scope['legal_entity_id'].','.db_escape($desc['work_date']).','.db_escape($desc['period_start_utc']).','.db_escape($desc['period_end_utc']).','.(int)$desc['duration_minutes'].','.db_escape($event_set_sha).','.count($events).','.db_escape($desc['work_pattern_sha256']).','.db_escape($desc['review_command_key']).','.($pred===null?'NULL':$pred).','.$actor.','.db_escape($desc['review_sha256']).')';
    $review_id=hrm_tim_001_insert_idempotent('hrm_time_reviewed_entries','reviewed_entry_id','review_sha256',$desc['review_sha256'],$sql,$error);
    if($review_id===false){cancel_transaction();return false;}
    foreach($events as $r){
        $h=hrm_tim_001_hash(array('reviewed_entry_id'=>$review_id,'raw_event_id'=>(int)$r['raw_event_id'],'event_sha256'=>$r['event_sha256']));
        $sql='INSERT INTO '.TB_PREF.'hrm_time_reviewed_event_links (reviewed_entry_id,raw_event_id,event_sha256,link_sha256) VALUES ('.$review_id.','.(int)$r['raw_event_id'].','.db_escape($r['event_sha256']).','.db_escape($h).')';
        if(hrm_tim_001_insert_idempotent('hrm_time_reviewed_event_links','reviewed_event_link_id','link_sha256',$h,$sql,$error)===false){cancel_transaction();return false;}
    }
    $allocation_ids=array();$final_allocs=array();
    foreach($alloc_desc as $a){
        $h=hrm_tim_001_hash(array('reviewed_entry_id'=>$review_id,'payroll_relationship_id'=>$rel['payroll_relationship_id'],'assignment_id'=>$scope['assignment_id'],'allocation_category'=>$a['allocation_category'],'quantity_minutes'=>$a['quantity_minutes']));
        $sql='INSERT INTO '.TB_PREF.'hrm_time_reviewed_allocations (reviewed_entry_id,payroll_relationship_id,assignment_id,allocation_category,quantity_minutes,allocation_sha256) VALUES ('.$review_id.','.$rel['payroll_relationship_id'].','.$scope['assignment_id'].','.db_escape($a['allocation_category']).','.(int)$a['quantity_minutes'].','.db_escape($h).')';
        $aid=hrm_tim_001_insert_idempotent('hrm_time_reviewed_allocations','allocation_id','allocation_sha256',$h,$sql,$error);
        if($aid===false){cancel_transaction();return false;}
        $allocation_ids[]=$aid;$final_allocs[]=array('allocation_id'=>$aid,'allocation_sha256'=>$h,'quantity_minutes'=>(int)$a['quantity_minutes'],'allocation_category'=>$a['allocation_category']);
    }
    $allocation_set_sha=hrm_tim_001_hash($final_allocs);
    commit_transaction();return array('reviewed_entry_id'=>$review_id,'review_sha256'=>$desc['review_sha256'],'allocation_ids'=>$allocation_ids,'allocation_set_sha256'=>$allocation_set_sha,'replay'=>false);
}
function hrm_tim_001_approve_time($reviewed_entry_id,$approval_evidence_sha256,$command_key,&$error=null)
{
    $error=null;$actor=hrm_tim_001_actor();$reviewed_entry_id=(int)$reviewed_entry_id;$cmd=hrm_tim_001_token($command_key,128);
    if($actor<=0||!hrm_tim_001_access('SA_HRM_APPROVE_TIME')||$reviewed_entry_id<=0||$cmd===false||!hrm_tim_001_is_sha256($approval_evidence_sha256)){$error='approval_access_or_evidence_invalid';return false;}
    begin_transaction();
    /* Return the original immutable result for an exact command replay, even if later state has advanced. */
    $q=db_query('SELECT approval_id,reviewed_entry_id,approval_evidence_sha256,approver_id,approval_sha256 FROM '.TB_PREF.'hrm_time_approvals WHERE approval_command_key='.db_escape($cmd).' LIMIT 2 FOR UPDATE');
    if(!$q||db_num_rows($q)>1){cancel_transaction();$error='approval_command_state_invalid';return false;}
    if(db_num_rows($q)===1){$old=db_fetch_assoc($q);if((int)$old['reviewed_entry_id']===$reviewed_entry_id&&(int)$old['approver_id']===$actor&&hash_equals((string)$old['approval_evidence_sha256'],$approval_evidence_sha256)){commit_transaction();return array('approval_id'=>(int)$old['approval_id'],'approval_sha256'=>$old['approval_sha256'],'replay'=>true);}cancel_transaction();$error='approval_command_replay_mismatch';return false;}

    $q=db_query('SELECT * FROM '.TB_PREF.'hrm_time_reviewed_entries WHERE reviewed_entry_id='.$reviewed_entry_id.' LIMIT 2 FOR UPDATE');
    $r=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    if(!$r||(int)$r['reviewer_id']===$actor){cancel_transaction();$error='distinct_approver_required';return false;}
    $live=hrm_tim_001_resolve_assignment_scope((int)$r['assignment_id'],$r['work_date'],true,$error);
    if($live===false||(int)$r['worker_id']!==$live['worker_id']||(int)$r['employment_id']!==$live['employment_id']||(int)$r['legal_entity_id']!==$live['legal_entity_id']||!hrm_tim_001_actor_can_review_scope($live,$r['work_date'])){cancel_transaction();$error=$error?:'approval_scope_denied';return false;}
    $q=db_query('SELECT reviewed_entry_id FROM '.TB_PREF.'hrm_time_reviewed_entries WHERE predecessor_entry_id='.$reviewed_entry_id.' LIMIT 1 FOR UPDATE');
    if(!$q||db_num_rows($q)>0){cancel_transaction();$error='stale_review_not_approvable';return false;}
    $q=db_query('SELECT newer.raw_event_id FROM '.TB_PREF.'hrm_time_reviewed_event_links l INNER JOIN '.TB_PREF.'hrm_time_raw_events newer ON newer.predecessor_event_id=l.raw_event_id WHERE l.reviewed_entry_id='.$reviewed_entry_id.' LIMIT 1 FOR UPDATE');
    if(!$q||db_num_rows($q)>0){cancel_transaction();$error='stale_review_raw_event_superseded';return false;}
    $q=db_query('SELECT pattern_sha256 FROM '.TB_PREF.'hrm_time_work_patterns WHERE assignment_id='.(int)$r['assignment_id'].' AND effective_from<='.db_escape($r['work_date']).' AND (effective_to IS NULL OR effective_to>='.db_escape($r['work_date']).') ORDER BY effective_from DESC,work_pattern_id DESC LIMIT 1 FOR UPDATE');
    $pat=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    if(!$pat||!hash_equals((string)$r['work_pattern_sha256'],(string)$pat['pattern_sha256'])){cancel_transaction();$error='stale_review_work_pattern_changed';return false;}

    $q=db_query('SELECT allocation_id,allocation_sha256,quantity_minutes,allocation_category FROM '.TB_PREF.'hrm_time_reviewed_allocations WHERE reviewed_entry_id='.$reviewed_entry_id.' ORDER BY allocation_id FOR UPDATE');
    $allocs=array();while($q&&$a=db_fetch_assoc($q))$allocs[]=array('allocation_id'=>(int)$a['allocation_id'],'allocation_sha256'=>$a['allocation_sha256'],'quantity_minutes'=>(int)$a['quantity_minutes'],'allocation_category'=>$a['allocation_category']);
    if(!$q||!$allocs){cancel_transaction();$error='approval_allocations_missing';return false;}
    $setsha=hrm_tim_001_hash($allocs);
    $desc=hrm_tim_001_approval_descriptor(array('reviewed_entry_id'=>$reviewed_entry_id,'review_sha256'=>$r['review_sha256'],'allocation_set_sha256'=>$setsha,'approval_evidence_sha256'=>$approval_evidence_sha256,'approval_command_key'=>$cmd,'reviewer_id'=>(int)$r['reviewer_id'],'approver_id'=>$actor),$error);
    if($desc===false){cancel_transaction();return false;}
    $sql='INSERT INTO '.TB_PREF.'hrm_time_approvals (reviewed_entry_id,review_sha256,allocation_set_sha256,approval_evidence_sha256,approval_command_key,approver_id,approval_sha256) VALUES ('.$reviewed_entry_id.','.db_escape($r['review_sha256']).','.db_escape($setsha).','.db_escape($approval_evidence_sha256).','.db_escape($cmd).','.$actor.','.db_escape($desc['approval_sha256']).')';
    $id=hrm_tim_001_insert_idempotent('hrm_time_approvals','approval_id','approval_sha256',$desc['approval_sha256'],$sql,$error);
    if($id===false){cancel_transaction();return false;}
    commit_transaction();return array('approval_id'=>$id,'approval_sha256'=>$desc['approval_sha256'],'replay'=>false);
}
function hrm_tim_001_create_payroll_mapping(array $x,&$error=null)
{
    $error=null;$actor=hrm_tim_001_actor();
    if($actor<=0||!hrm_tim_001_access('SA_HRM_MAP_TIME_PAYROLL')){$error='access_denied';return false;}
    $desc=hrm_tim_001_payroll_mapping_descriptor($x+array('created_by'=>$actor),$error);if($desc===false)return false;
    begin_transaction();
    $q=db_query('SELECT mapping_id FROM '.TB_PREF.'hrm_time_payroll_input_mappings WHERE mapping_sha256='.db_escape($desc['mapping_sha256']).' LIMIT 2 FOR UPDATE');
    if(!$q||db_num_rows($q)>1){cancel_transaction();$error='mapping_replay_state_invalid';return false;}
    if(db_num_rows($q)===1){$r=db_fetch_assoc($q);commit_transaction();return array('mapping_id'=>(int)$r['mapping_id'],'mapping_sha256'=>$desc['mapping_sha256'],'replay'=>true);}

    $q=db_query('SELECT d.input_definition_id,d.element_version_id,d.value_type,d.unit_token,v.effective_from,v.effective_to FROM '.TB_PREF.'hrm_pay_input_definitions d INNER JOIN '.TB_PREF.'hrm_pay_element_versions v ON v.element_version_id=d.element_version_id WHERE d.input_definition_id='.(int)$desc['input_definition_id'].' LIMIT 2 FOR UPDATE');
    $d=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    if(!$d||(int)$d['element_version_id']!==(int)$desc['element_version_id']||(string)$d['value_type']!=='decimal'||!in_array((string)$desc['unit_token'],array('minutes','hours'),true)||(string)$d['unit_token']!==(string)$desc['unit_token']||$desc['effective_from']<(string)$d['effective_from']||(!empty($d['effective_to'])&&($desc['effective_to']===null||$desc['effective_to']>(string)$d['effective_to']))){cancel_transaction();$error='pay_core_003_mapping_target_invalid';return false;}
    $q=db_query('SELECT mapping_id,effective_from,effective_to FROM '.TB_PREF.'hrm_time_payroll_input_mappings WHERE legal_entity_id='.(int)$desc['legal_entity_id'].' AND allocation_category='.db_escape($desc['allocation_category']).' ORDER BY effective_from DESC,mapping_id DESC LIMIT 1 FOR UPDATE');
    $prev=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;$pred=empty($desc['predecessor_mapping_id'])?null:(int)$desc['predecessor_mapping_id'];
    if(($prev===false&&$pred!==null)||($prev!==false&&$pred!==(int)$prev['mapping_id'])||($prev!==false&&$desc['effective_from']<=(string)$prev['effective_from'])){cancel_transaction();$error='mapping_predecessor_or_order_invalid';return false;}
    if($prev!==false&&!empty($prev['effective_to'])&&$desc['effective_from']<=(string)$prev['effective_to']){cancel_transaction();$error='mapping_effective_overlap';return false;}
    $sql='INSERT INTO '.TB_PREF.'hrm_time_payroll_input_mappings (legal_entity_id,allocation_category,effective_from,effective_to,element_version_id,input_definition_id,unit_token,predecessor_mapping_id,mapping_sha256,created_by) VALUES ('.(int)$desc['legal_entity_id'].','.db_escape($desc['allocation_category']).','.db_escape($desc['effective_from']).','.($desc['effective_to']===null?'NULL':db_escape($desc['effective_to'])).','.(int)$desc['element_version_id'].','.(int)$desc['input_definition_id'].','.db_escape($desc['unit_token']).','.($pred===null?'NULL':$pred).','.db_escape($desc['mapping_sha256']).','.$actor.')';
    $id=hrm_tim_001_insert_idempotent('hrm_time_payroll_input_mappings','mapping_id','mapping_sha256',$desc['mapping_sha256'],$sql,$error);
    if($id===false){cancel_transaction();return false;}
    commit_transaction();return array('mapping_id'=>$id,'mapping_sha256'=>$desc['mapping_sha256'],'replay'=>false);
}
function hrm_tim_001_send_approved_time_to_payroll($approval_id,$allocation_id,$payroll_period_id,$command_key,&$error=null)
{
    $error=null;$actor=hrm_tim_001_actor();$approval_id=(int)$approval_id;$allocation_id=(int)$allocation_id;$period=(int)$payroll_period_id;$cmd=hrm_tim_001_token($command_key,128);
    if($actor<=0||!hrm_tim_001_access('SA_HRM_EXECUTE_TIME_PAYROLL')||!hrm_tim_001_access('SA_HRM_EXECUTE_PAY_CORE_INPUT')||$approval_id<=0||$allocation_id<=0||$period<=0||$cmd===false){$error='adapter_access_or_identity_invalid';return false;}
    global $transaction_level;if(!empty($transaction_level)){$error='top_level_transaction_required';return false;}
    begin_transaction();

    /* Command replay is resolved before moving-state checks so a completed command is stable. */
    $q=db_query('SELECT receipt_id,approval_id,allocation_id,payroll_period_id,approved_input_id,approved_input_sha256,receipt_sha256,executed_by FROM '.TB_PREF.'hrm_time_payroll_input_receipts WHERE command_key='.db_escape($cmd).' LIMIT 2 FOR UPDATE');
    if(!$q||db_num_rows($q)>1){cancel_transaction();$error='adapter_command_state_invalid';return false;}
    if(db_num_rows($q)===1){$old=db_fetch_assoc($q);if((int)$old['approval_id']===$approval_id&&(int)$old['allocation_id']===$allocation_id&&(int)$old['payroll_period_id']===$period&&(int)$old['executed_by']===$actor){commit_transaction();return array('receipt_id'=>(int)$old['receipt_id'],'receipt_sha256'=>$old['receipt_sha256'],'approved_input_id'=>(int)$old['approved_input_id'],'approved_input_sha256'=>$old['approved_input_sha256'],'replay'=>true);}cancel_transaction();$error='adapter_command_replay_mismatch';return false;}
    $q=db_query('SELECT receipt_id FROM '.TB_PREF.'hrm_time_payroll_input_receipts WHERE approval_id='.$approval_id.' AND allocation_id='.$allocation_id.' LIMIT 2 FOR UPDATE');
    if(!$q||db_num_rows($q)>0){cancel_transaction();$error='approved_allocation_already_exported';return false;}

    $q=db_query('SELECT p.approval_id,p.approval_sha256,p.reviewed_entry_id,r.review_sha256,r.work_date,r.assignment_id,r.legal_entity_id,r.work_pattern_sha256,a.allocation_id,a.payroll_relationship_id,a.allocation_category,a.quantity_minutes,a.allocation_sha256 FROM '.TB_PREF.'hrm_time_approvals p INNER JOIN '.TB_PREF.'hrm_time_reviewed_entries r ON r.reviewed_entry_id=p.reviewed_entry_id AND r.review_sha256=p.review_sha256 INNER JOIN '.TB_PREF.'hrm_time_reviewed_allocations a ON a.reviewed_entry_id=r.reviewed_entry_id WHERE p.approval_id='.$approval_id.' AND a.allocation_id='.$allocation_id.' LIMIT 2 FOR UPDATE');
    $x=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    if(!$x){cancel_transaction();$error='approved_allocation_missing';return false;}
    $q=db_query('SELECT reviewed_entry_id FROM '.TB_PREF.'hrm_time_reviewed_entries WHERE predecessor_entry_id='.(int)$x['reviewed_entry_id'].' LIMIT 1 FOR UPDATE');
    if(!$q||db_num_rows($q)>0){cancel_transaction();$error='stale_approval';return false;}
    $q=db_query('SELECT newer.raw_event_id FROM '.TB_PREF.'hrm_time_reviewed_event_links l INNER JOIN '.TB_PREF.'hrm_time_raw_events newer ON newer.predecessor_event_id=l.raw_event_id WHERE l.reviewed_entry_id='.(int)$x['reviewed_entry_id'].' LIMIT 1 FOR UPDATE');
    if(!$q||db_num_rows($q)>0){cancel_transaction();$error='stale_approval_raw_event_superseded';return false;}
    $q=db_query('SELECT pattern_sha256 FROM '.TB_PREF.'hrm_time_work_patterns WHERE assignment_id='.(int)$x['assignment_id'].' AND effective_from<='.db_escape($x['work_date']).' AND (effective_to IS NULL OR effective_to>='.db_escape($x['work_date']).') ORDER BY effective_from DESC,work_pattern_id DESC LIMIT 1 FOR UPDATE');
    $pat=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    if(!$pat||!hash_equals((string)$x['work_pattern_sha256'],(string)$pat['pattern_sha256'])){cancel_transaction();$error='stale_approval_work_pattern_changed';return false;}
    $rel=hrm_tim_001_resolve_payroll_relationship((int)$x['assignment_id'],$x['work_date'],true,$error);
    if($rel===false||(int)$x['payroll_relationship_id']!==$rel['payroll_relationship_id']){cancel_transaction();$error=$error?:'payroll_relationship_changed';return false;}

    $q=db_query('SELECT lp.period_id,lp.status legacy_status,c.calendar_period_id,cp.calendar_version_id,cp.period_start,cp.period_end,g.legal_entity_id FROM '.TB_PREF.'payroll_periods lp INNER JOIN '.TB_PREF.'hrm_payroll_legacy_period_conversion_receipts c ON c.legacy_period_id=lp.period_id INNER JOIN '.TB_PREF.'hrm_payroll_calendar_periods cp ON cp.calendar_period_id=c.calendar_period_id INNER JOIN '.TB_PREF.'hrm_payroll_group_enrollments ge ON ge.payroll_relationship_id='.(int)$rel['payroll_relationship_id'].' AND ge.effective_from<='.db_escape($x['work_date']).' AND (ge.effective_to IS NULL OR ge.effective_to>='.db_escape($x['work_date']).') INNER JOIN '.TB_PREF.'hrm_payroll_group_versions gv ON gv.group_version_id=ge.group_version_id AND gv.calendar_version_id=cp.calendar_version_id AND gv.effective_from<='.db_escape($x['work_date']).' AND (gv.effective_to IS NULL OR gv.effective_to>='.db_escape($x['work_date']).') INNER JOIN '.TB_PREF.'hrm_payroll_groups g ON g.payroll_group_id=gv.payroll_group_id WHERE lp.period_id='.$period.' AND lp.status=0 AND lp.from_date<='.db_escape($x['work_date']).' AND lp.to_date>='.db_escape($x['work_date']).' AND cp.period_start<='.db_escape($x['work_date']).' AND cp.period_end>='.db_escape($x['work_date']).' AND g.legal_entity_id='.(int)$x['legal_entity_id'].' AND EXISTS (SELECT 1 FROM '.TB_PREF.'hrm_payroll_legacy_period_reconciliations rr WHERE rr.conversion_receipt_id=c.conversion_receipt_id AND rr.status_token=\'reconciled\' AND NOT EXISTS (SELECT 1 FROM '.TB_PREF.'hrm_payroll_legacy_period_reconciliations newer WHERE newer.conversion_receipt_id=rr.conversion_receipt_id AND newer.reconciliation_id>rr.reconciliation_id)) LIMIT 2 FOR UPDATE');
    $ps=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    if(!$ps){cancel_transaction();$error='payroll_period_stale_or_out_of_scope';return false;}
    $q=db_query('SELECT mapping_id,element_version_id,input_definition_id,unit_token,mapping_sha256 FROM '.TB_PREF.'hrm_time_payroll_input_mappings WHERE legal_entity_id='.(int)$x['legal_entity_id'].' AND allocation_category='.db_escape($x['allocation_category']).' AND effective_from<='.db_escape($x['work_date']).' AND (effective_to IS NULL OR effective_to>='.db_escape($x['work_date']).') ORDER BY effective_from DESC,mapping_id DESC LIMIT 1 FOR UPDATE');
    $m=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    if(!$m){cancel_transaction();$error='governed_mapping_missing_or_ambiguous';return false;}
    $value=$m['unit_token']==='minutes'?(string)(int)$x['quantity_minutes']:hrm_tim_001_minutes_to_decimal_hours((int)$x['quantity_minutes']);
    if($value===false){cancel_transaction();$error='adapter_quantity_invalid';return false;}

    $source_sha=hrm_tim_001_hash(array('approval_sha256'=>$x['approval_sha256'],'allocation_sha256'=>$x['allocation_sha256'],'mapping_sha256'=>$m['mapping_sha256'],'adapter_version'=>HRM_TIM_001_PAYROLL_ADAPTER_VERSION));
    $candidate=array('payroll_relationship_id'=>$rel['payroll_relationship_id'],'assignment_id'=>(int)$x['assignment_id'],'element_version_id'=>(int)$m['element_version_id'],'input_definition_id'=>(int)$m['input_definition_id'],'effective_date'=>$x['work_date'],'value_type'=>'decimal','value'=>$value,'source_kind'=>'hrm_tim_001_approved_time','source_event_id'=>$approval_id.':'.$allocation_id,'source_sha256'=>$source_sha,'approval_evidence_sha256'=>$x['approval_sha256']);
    $pc3=hrm_pay_core_003_write_approved_input($candidate,$error);
    if($pc3===false){cancel_transaction();return false;}
    $receipt_sha=hrm_tim_001_hash(array('approval_id'=>$approval_id,'allocation_id'=>$allocation_id,'mapping_id'=>(int)$m['mapping_id'],'payroll_period_id'=>$period,'calendar_period_id'=>(int)$ps['calendar_period_id'],'payroll_relationship_id'=>$rel['payroll_relationship_id'],'assignment_id'=>(int)$x['assignment_id'],'approved_input_id'=>(int)$pc3['approved_input_id'],'approved_input_sha256'=>$pc3['input_sha256'],'adapter_version'=>HRM_TIM_001_PAYROLL_ADAPTER_VERSION,'command_key'=>$cmd,'executed_by'=>$actor));
    $sql='INSERT INTO '.TB_PREF.'hrm_time_payroll_input_receipts (approval_id,allocation_id,mapping_id,payroll_period_id,calendar_period_id,payroll_relationship_id,assignment_id,approved_input_id,approved_input_sha256,adapter_version,command_key,receipt_sha256,executed_by) VALUES ('.$approval_id.','.$allocation_id.','.(int)$m['mapping_id'].','.$period.','.(int)$ps['calendar_period_id'].','.$rel['payroll_relationship_id'].','.(int)$x['assignment_id'].','.(int)$pc3['approved_input_id'].','.db_escape($pc3['input_sha256']).','.db_escape(HRM_TIM_001_PAYROLL_ADAPTER_VERSION).','.db_escape($cmd).','.db_escape($receipt_sha).','.$actor.')';
    $rid=hrm_tim_001_insert_idempotent('hrm_time_payroll_input_receipts','receipt_id','receipt_sha256',$receipt_sha,$sql,$error);
    if($rid===false){cancel_transaction();return false;}
    commit_transaction();return array('receipt_id'=>$rid,'receipt_sha256'=>$receipt_sha,'approved_input_id'=>(int)$pc3['approved_input_id'],'approved_input_sha256'=>$pc3['input_sha256'],'replay'=>false);
}
function hrm_tim_001_migrate_legacy_attendance($attendance_id,&$error=null)
{
    $error=null;$actor=hrm_tim_001_actor();$attendance_id=(int)$attendance_id;if($actor<=0||!hrm_tim_001_access('SA_HRM_MIGRATE_TIME')||$attendance_id<=0){$error='access_denied';return false;}
    begin_transaction();$q=db_query('SELECT * FROM '.TB_PREF.'attendance WHERE attendance_id='.$attendance_id.' LIMIT 2 FOR UPDATE');$a=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;if(!$a){cancel_transaction();$error='legacy_attendance_missing';return false;}$attendance_sha=hrm_tim_001_hash($a);
    $q=db_query('SELECT legacy_receipt_id,attendance_sha256,migration_status FROM '.TB_PREF.'hrm_time_legacy_attendance_receipts WHERE attendance_id='.$attendance_id.' LIMIT 2 FOR UPDATE');if($q&&db_num_rows($q)===1){$r=db_fetch_assoc($q);if(hash_equals($r['attendance_sha256'],$attendance_sha)){commit_transaction();return array('legacy_receipt_id'=>(int)$r['legacy_receipt_id'],'status'=>$r['migration_status']);}cancel_transaction();$error='legacy_attendance_changed_after_custody';return false;}
    $date=hrm_tim_001_date($a['date']??'');$employee_id=(string)($a['employee_id']??'');$q=db_query('SELECT m.worker_id,m.employee_number,m.employee_id,a.assignment_id,h.employment_id,h.legal_entity_id FROM '.TB_PREF.'hrm_employee_worker_map m INNER JOIN '.TB_PREF.'hrm_employments h ON h.worker_id=m.worker_id INNER JOIN '.TB_PREF.'hrm_assignments a ON a.employment_id=h.employment_id WHERE m.employee_id='.db_escape($employee_id).' AND m.retired_at IS NULL AND '.($date===false?'1=0':'(h.start_date IS NULL OR h.start_date<='.db_escape($date).') AND (h.end_date IS NULL OR h.end_date>='.db_escape($date).') AND (a.effective_from IS NULL OR a.effective_from<='.db_escape($date).') AND (a.effective_to IS NULL OR a.effective_to>='.db_escape($date).')').' LIMIT 2 FOR UPDATE');$map=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    $e1=null;$e2=null;$regular=hrm_tim_001_decimal_hours_to_minutes_exact($a['regular_hours']??'0',$e1);$overtime=hrm_tim_001_decimal_hours_to_minutes_exact($a['overtime_hours']??'0',$e2);$approved=(int)($a['approved']??0)===1?1:0;
    $status='quarantined';$reason='unresolved_identity';if($date!==false&&$map&&$regular!==false&&$overtime!==false){if($approved){$status='approved_legacy_fact';$reason='legacy_origin_no_raw_clock_provenance';}else{$status='unapproved_legacy_fact';$reason='legacy_origin_not_approved';}}elseif($regular===false||$overtime===false){$reason='legacy_quantity_not_exact_minute';}
    /* Legacy rows are custody facts only. We preserve approved status and exact minute totals but never fabricate raw clock, timezone, work-pattern, reviewer or payroll mapping history. */
    $receipt_sha=hrm_tim_001_hash(array('attendance_id'=>$attendance_id,'attendance_sha256'=>$attendance_sha,'migration_status'=>$status,'regular_minutes'=>$regular,'overtime_minutes'=>$overtime,'legacy_approved'=>$approved,'reason_token'=>$reason,'executed_by'=>$actor));
    $sql='INSERT INTO '.TB_PREF.'hrm_time_legacy_attendance_receipts (attendance_id,attendance_sha256,migration_status,regular_minutes,overtime_minutes,legacy_approved,raw_event_id,reviewed_entry_id,approval_id,reason_token,receipt_sha256,executed_by) VALUES ('.$attendance_id.','.db_escape($attendance_sha).','.db_escape($status).','.($regular===false?'NULL':(int)$regular).','.($overtime===false?'NULL':(int)$overtime).','.$approved.',NULL,NULL,NULL,'.db_escape($reason).','.db_escape($receipt_sha).','.$actor.')';
    $rid=hrm_tim_001_insert_idempotent('hrm_time_legacy_attendance_receipts','legacy_receipt_id','receipt_sha256',$receipt_sha,$sql,$error);if($rid===false){cancel_transaction();return false;}commit_transaction();return array('legacy_receipt_id'=>$rid,'status'=>$status,'reason'=>$reason,'attendance_sha256'=>$attendance_sha,'regular_minutes'=>$regular,'overtime_minutes'=>$overtime,'legacy_approved'=>$approved);
}

?>
