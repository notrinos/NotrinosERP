<?php
/** Governed HRM-TIM-002 overtime policy, request receipt and import writers. All TIM-002 evidence is append-only. */
require_once(__DIR__.'/../../../includes/value_helpers.inc');
include_once(dirname(dirname(__FILE__)).'/time/hrm_tim_002_contract.inc');
include_once(dirname(__FILE__).'/hrm_tim_001_governance_db.inc');
include_once(dirname(__FILE__).'/pay_core_003_governance_db.inc');
function hrm_tim_002_actor(){return current_user_id_or_zero();}
function hrm_tim_002_access($a){return function_exists('user_check_access')&&user_check_access($a);}
function hrm_tim_002_insert_idempotent($table,$id,$hashcol,$hash,$sql,&$error){return hrm_tim_001_insert_idempotent($table,$id,$hashcol,$hash,$sql,$error);}
function hrm_tim_002_sql_nullable($v){return $v===null?'NULL':db_escape($v);}
function hrm_tim_002_resolve_legacy_employee_assignment($employee_id,$date,$for_update=false,&$error=null)
{
    $error=null;$date=hrm_tim_002_date($date);$employee_id=trim((string)$employee_id);if($date===false||$employee_id===''){$error='legacy_employee_scope_invalid';return false;}$d=db_escape($date);
    $q=db_query('SELECT m.worker_id,h.employment_id,a.assignment_id,h.legal_entity_id FROM '.TB_PREF.'hrm_employee_worker_map m INNER JOIN '
      .TB_PREF.'hrm_employments h ON h.worker_id=m.worker_id INNER JOIN '.TB_PREF.'hrm_assignments a ON a.employment_id=h.employment_id WHERE m.employee_id='
      .db_escape($employee_id).' AND m.retired_at IS NULL AND (h.start_date IS NULL OR h.start_date<='.$d.') AND (h.end_date IS NULL OR h.end_date>='
      .$d.') AND (a.effective_from IS NULL OR a.effective_from<='.$d.') AND (a.effective_to IS NULL OR a.effective_to>='.$d
      .') LIMIT 2'.($for_update?' FOR UPDATE':'') );
    if(!$q||db_num_rows($q)!==1){$error='legacy_employee_assignment_ambiguous_or_missing';return false;}$r=db_fetch_assoc($q);return hrm_tim_001_resolve_payroll_relationship((int)$r['assignment_id'],$date,$for_update,$error);
}
function hrm_tim_002_create_overtime_policy(array $x,array $rules,&$error=null)
{
    $error=null;$actor=hrm_tim_002_actor();if($actor<=0||!hrm_tim_002_access('SA_HRM_MANAGE_OVERTIME_POLICY')){$error='access_denied';return false;}$desc=hrm_tim_002_policy_descriptor($x+array('created_by'=>$actor),$error);if($desc===false||!$rules){$error=$error?:'policy_rules_missing';return false;}
    begin_transaction();
    $q=db_query('SELECT policy_id FROM '.TB_PREF.'hrm_time_overtime_policies WHERE policy_sha256='.db_escape($desc['policy_sha256'])
      .' LIMIT 2 FOR UPDATE');
    if(!$q||db_num_rows($q)>1){
      cancel_transaction();
      $error='policy_replay_state_invalid';
      return false;
    }
    if(db_num_rows($q)===1){
      $p=db_fetch_assoc($q);
      commit_transaction();
      return array('policy_id'=>(int)$p['policy_id'],'policy_sha256'=>$desc['policy_sha256'],'replay'=>true);
    }
    $q=db_query('SELECT policy_id,effective_from,effective_to FROM '.TB_PREF.'hrm_time_overtime_policies WHERE legal_entity_id='
      .(int)$desc['legal_entity_id'].' AND policy_code='.db_escape($desc['policy_code']).' ORDER BY effective_from DESC,policy_id DESC LIMIT 1 FOR UPDATE');
    $prev=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    $pred=$desc['predecessor_policy_id'];
    if(($prev===false&&$pred!==null)||($prev!==false&&$pred!==(int)$prev['policy_id'])||($prev!==false&&$desc['effective_from']<=(string)$prev['effective_from'])){
      cancel_transaction();
      $error='policy_predecessor_or_order_invalid';
      return false;
    }
    if($prev!==false&&!empty($prev['effective_to'])&&$desc['effective_from']<=(string)$prev['effective_to']){
      cancel_transaction();
      $error='policy_effective_overlap';
      return false;
    }
    $sql='INSERT INTO '.TB_PREF.'hrm_time_overtime_policies (legal_entity_id,policy_code,policy_version,source_kind,source_ref,source_evidence_sha256,effective_from,effective_to,priority_rank,week_start_day,weekly_scope,weekend_days,predecessor_policy_id,policy_sha256,created_by) VALUES ('
      .(int)$desc['legal_entity_id'].','.db_escape($desc['policy_code']).','.db_escape($desc['policy_version']).','.db_escape($desc['source_kind'])
      .','.db_escape($desc['source_ref']).','.db_escape($desc['source_evidence_sha256']).','.db_escape($desc['effective_from'])
      .','.hrm_tim_002_sql_nullable($desc['effective_to']).','.(int)$desc['priority_rank'].','.($desc['week_start_day']===null?'NULL':(int)$desc['week_start_day'])
      .','.hrm_tim_002_sql_nullable($desc['weekly_scope']).','.($desc['weekend_days']?db_escape(implode(',',$desc['weekend_days'])):'NULL')
      .','.($pred===null?'NULL':(int)$pred).','.db_escape($desc['policy_sha256']).','.$actor.')';
    if(!db_query($sql,'could not append HRM-TIM-002 overtime policy')){
      cancel_transaction();
      $error='policy_insert_failed';
      return false;
    }
    $pid=(int)db_insert_id();
    $ids=array();
    foreach($rules as $rule){
      $rd=hrm_tim_002_rule_descriptor($rule+array('policy_id'=>$pid),$error);
      if($rd===false){
        cancel_transaction();
        return false;
      }
      $sql='INSERT INTO '.TB_PREF.'hrm_time_overtime_policy_rules (policy_id,rule_sequence,daily_threshold_minutes,weekly_threshold_minutes,weekend_required,holiday_required,rest_day_required,approved_request_required,schedule_deviation_min_minutes,qualification_mode,qualification_category,rule_sha256) VALUES ('
        .$pid.','.(int)$rd['rule_sequence'].','.($rd['daily_threshold_minutes']===null?'NULL':(int)$rd['daily_threshold_minutes'])
        .','.($rd['weekly_threshold_minutes']===null?'NULL':(int)$rd['weekly_threshold_minutes']).','.($rd['weekend_required']===null?'NULL':(int)$rd['weekend_required'])
        .','.($rd['holiday_required']===null?'NULL':(int)$rd['holiday_required']).','.($rd['rest_day_required']===null?'NULL':(int)$rd['rest_day_required'])
        .','.(int)$rd['approved_request_required'].','.($rd['schedule_deviation_min_minutes']===null?'NULL':(int)$rd['schedule_deviation_min_minutes'])
        .','.db_escape($rd['qualification_mode']).','.db_escape($rd['qualification_category']).','.db_escape($rd['rule_sha256'])
        .')';
      if(!db_query($sql,'could not append HRM-TIM-002 overtime rule')){
        cancel_transaction();
        $error='rule_insert_failed';
        return false;
      }
      $ids[]=(int)db_insert_id();
    }
    commit_transaction();
    return array('policy_id'=>$pid,'policy_sha256'=>$desc['policy_sha256'],'rule_ids'=>$ids,'replay'=>false);
}
/** Overtime request approval proves authorization only. It never proves worked time. */
function hrm_tim_002_record_overtime_request_approval($request_id,$approved_by_actor,$approval_draft_id=null,&$error=null)
{
    global $transaction_level;
    $error=null;
    $request_id=(int)$request_id;
    if(empty($transaction_level)||$request_id<=0){
      $error='outer_transaction_required';
      return false;
    }
    $actor=hrm_tim_002_actor();
    if($actor<=0){
      $error='actor_missing';
      return false;
    }
    $q=db_query('SELECT * FROM '.TB_PREF.'overtime_requests WHERE request_id='.$request_id.' LIMIT 2 FOR UPDATE');
    $r=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    if(!$r||(int)$r['status']!==0){
      $error='request_missing_or_stale';
      return false;
    }
    $minutes=hrm_tim_001_decimal_hours_to_minutes_exact($r['hours']??'',$qe);
    if($minutes===false||$minutes<=0){
      $error='request_quantity_not_exact_minute';
      return false;
    }
    $scope=hrm_tim_002_resolve_legacy_employee_assignment($r['employee_id'],$r['date'],true,$error);
    if($scope===false)return false;
    $draft=$approval_draft_id===null?null:(int)$approval_draft_id;
    $by=hrm_tim_002_token($approved_by_actor,64);
    if($by===false){
      $error='approver_identity_invalid';
      return false;
    }
    $snapshot=hrm_tim_002_hash(array('request_id'=>$request_id,'employee_id'=>(string)$r['employee_id'],'overtime_id'=>(int)$r['overtime_id'],
      'date'=>(string)$r['date'],'requested_minutes'=>$minutes,'reason_sha256'=>hash('sha256',(string)$r['reason'])));
    $cmd='overtime-request:'.$request_id.':'.($draft===null?'direct':$draft).':'.$snapshot;
    $receipt=hrm_tim_002_hash(array('request_snapshot_sha256'=>$snapshot,'assignment_id'=>$scope['assignment_id'],'payroll_relationship_id'=>$scope['payroll_relationship_id'],
      'approval_draft_id'=>$draft,'approved_by_actor'=>$by));
    $q=db_query('SELECT request_receipt_id,request_snapshot_sha256,approval_receipt_sha256 FROM '.TB_PREF.'hrm_time_overtime_request_receipts WHERE request_id='
      .$request_id.' LIMIT 2 FOR UPDATE');
    if(!$q||db_num_rows($q)>1){
      $error='request_receipt_state_invalid';
      return false;
    }
    if(db_num_rows($q)===1){
      $old=db_fetch_assoc($q);
      if(hash_equals($old['request_snapshot_sha256'],$snapshot)&&hash_equals($old['approval_receipt_sha256'],$receipt))return array('request_receipt_id'=>(int)$old['request_receipt_id'],
        'approval_receipt_sha256'=>$receipt,'replay'=>true);
      $error='request_approval_conflict';
      return false;
    }
    $sql='INSERT INTO '.TB_PREF.'hrm_time_overtime_request_receipts (request_id,worker_id,employment_id,assignment_id,legal_entity_id,payroll_relationship_id,overtime_type_id,work_date,requested_minutes,request_snapshot_sha256,approval_draft_id,approval_command_key,approved_by_actor,approval_receipt_sha256) VALUES ('
      .$request_id.','.$scope['worker_id'].','.$scope['employment_id'].','.$scope['assignment_id'].','.$scope['legal_entity_id']
      .','.$scope['payroll_relationship_id'].','.(int)$r['overtime_id'].','.db_escape($r['date']).','.$minutes.','.db_escape($snapshot)
      .','.($draft===null?'NULL':$draft).','.db_escape($cmd).','.db_escape($by).','.db_escape($receipt).')';
    if(!db_query($sql,'could not append overtime request approval receipt')){
      $error='request_receipt_insert_failed';
      return false;
    }
    return array('request_receipt_id'=>(int)db_insert_id(),'approval_receipt_sha256'=>$receipt,'replay'=>false);
}
function hrm_tim_002_get_overtime_request_receipt($request_id,$for_update=false){$q=db_query('SELECT * FROM '.TB_PREF.'hrm_time_overtime_request_receipts WHERE request_id='.(int)$request_id.' LIMIT 2'.($for_update?' FOR UPDATE':''));return $q&&db_num_rows($q)===1?db_fetch_assoc($q):false;}
function hrm_tim_002_create_import_connector(array $x,&$error=null)
{
    $error=null;
    $actor=hrm_tim_002_actor();
    if($actor<=0||!hrm_tim_002_access('SA_HRM_MANAGE_TIME_CONNECTOR')){
      $error='access_denied';
      return false;
    }
    foreach(array('connector_key','connector_version','legal_entity_id','source_system','channel_kind','auth_mode','max_bytes',
      'max_items','allowed_mime','effective_from') as $k)if(!array_key_exists($k,$x)){
      $error='connector_field_missing';
      return false;
    }
    $key=hrm_tim_002_source_key($x['connector_key'],64);
    $ver=hrm_tim_002_token($x['connector_version'],64);
    $src=hrm_tim_002_token($x['source_system'],64);
    $channel=hrm_tim_002_token($x['channel_kind'],24);
    $auth=hrm_tim_002_token($x['auth_mode'],32);
    $mime=hrm_tim_002_token($x['allowed_mime'],96);
    $from=hrm_tim_002_date($x['effective_from']);
    $to=empty($x['effective_to'])?null:hrm_tim_002_date($x['effective_to']);
    $le=(int)$x['legal_entity_id'];
    $mb=(int)$x['max_bytes'];
    $mi=(int)$x['max_items'];
    if($key===false||$ver===false||$src===false||!in_array($channel,array('csv','file','api'),true)||$auth===false||$mime===false||$from===false||($to!==null&&($to===false||$to<$from))||$le<=0||$mb<1||$mb>52428800||$mi<1||$mi>100000){
      $error='connector_definition_invalid';
      return false;
    }
    $pred=empty($x['predecessor_connector_id'])?null:(int)$x['predecessor_connector_id'];
    $desc=array('connector_key'=>$key,'connector_version'=>$ver,'legal_entity_id'=>$le,'source_system'=>$src,'channel_kind'=>$channel,
      'auth_mode'=>$auth,'max_bytes'=>$mb,'max_items'=>$mi,'allowed_mime'=>$mime,'effective_from'=>$from,'effective_to'=>$to,
      'predecessor_connector_id'=>$pred,'created_by'=>$actor);
    $sha=hrm_tim_002_hash($desc);
    begin_transaction();
    $q=db_query('SELECT connector_id FROM '.TB_PREF.'hrm_time_import_connectors WHERE connector_sha256='.db_escape($sha).' LIMIT 2 FOR UPDATE');
    if($q&&db_num_rows($q)===1){
      $r=db_fetch_assoc($q);
      commit_transaction();
      return array('connector_id'=>(int)$r['connector_id'],'connector_sha256'=>$sha,'replay'=>true);
    }
    $q=db_query('SELECT connector_id,effective_from,effective_to FROM '.TB_PREF.'hrm_time_import_connectors WHERE legal_entity_id='
      .$le.' AND connector_key='.db_escape($key).' ORDER BY effective_from DESC,connector_id DESC LIMIT 1 FOR UPDATE');
    $prev=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    if(($prev===false&&$pred!==null)||($prev!==false&&$pred!==(int)$prev['connector_id'])||($prev!==false&&$from<=(string)$prev['effective_from'])){
      cancel_transaction();
      $error='connector_predecessor_or_order_invalid';
      return false;
    }
    $sql='INSERT INTO '.TB_PREF.'hrm_time_import_connectors (connector_key,connector_version,legal_entity_id,source_system,channel_kind,auth_mode,max_bytes,max_items,allowed_mime,effective_from,effective_to,predecessor_connector_id,connector_sha256,created_by) VALUES ('
      .db_escape($key).','.db_escape($ver).','.$le.','.db_escape($src).','.db_escape($channel).','.db_escape($auth).','.$mb
      .','.$mi.','.db_escape($mime).','.db_escape($from).','.hrm_tim_002_sql_nullable($to).','.($pred===null?'NULL':$pred)
      .','.db_escape($sha).','.$actor.')';
    if(!db_query($sql)){
      cancel_transaction();
      $error='connector_insert_failed';
      return false;
    }
    $id=(int)db_insert_id();
    commit_transaction();
    return array('connector_id'=>$id,'connector_sha256'=>$sha,'replay'=>false);
}
function hrm_tim_002_append_import_error($batch_id,$item_id,$source_key,$code,array $context,&$error=null)
{
    $ctx=hrm_tim_002_hash($context);
    $sha=hrm_tim_002_hash(array('import_batch_id'=>(int)$batch_id,'import_item_id'=>$item_id===null?null:(int)$item_id,'source_item_key'=>$source_key,'error_code'=>$code,'error_context_sha256'=>$ctx));
    $sql='INSERT INTO '.TB_PREF.'hrm_time_import_errors (import_batch_id,import_item_id,source_item_key,error_code,error_context_sha256,error_sha256) VALUES ('.(int)$batch_id.','.($item_id===null?'NULL':(int)$item_id).','.($source_key===null?'NULL':db_escape($source_key)).','.db_escape($code).','.db_escape($ctx).','.db_escape($sha).')';
    return hrm_tim_002_insert_idempotent('hrm_time_import_errors','import_error_id','error_sha256',$sha,$sql,$error);
}
/** Derive final batch outcome from append-only item/error custody; the batch row itself remains an immutable receipt. */
function hrm_tim_002_import_batch_summary($batch_id)
{
    $batch_id=(int)$batch_id;
    if($batch_id<=0)return false;
    $q=db_query('SELECT status_token FROM '.TB_PREF.'hrm_time_import_batches WHERE import_batch_id='.$batch_id.' LIMIT 2');
    if(!$q||db_num_rows($q)!==1)return false;
    $b=db_fetch_assoc($q);
    if((string)$b['status_token']==='quarantined')return array('status'=>'quarantined','accepted'=>0,'quarantined'=>1);
    $q=db_query('SELECT status_token,COUNT(*) item_count FROM '.TB_PREF.'hrm_time_import_items WHERE import_batch_id='.$batch_id.' GROUP BY status_token');
    if(!$q)return false;
    $accepted=0;$quarantined=0;
    while($r=db_fetch_assoc($q)){
        if(in_array((string)$r['status_token'],array('accepted','replay'),true))$accepted+=(int)$r['item_count'];
        else $quarantined+=(int)$r['item_count'];
    }
    return array('status'=>$quarantined?($accepted?'partial':'quarantined'):'accepted','accepted'=>$accepted,'quarantined'=>$quarantined);
}
/** CSV adapter accepts only raw event facts. Totals, rates and approval fields are intentionally absent. */
function hrm_tim_002_process_csv_import($tmp_path,$original_name,$connector_key,$source_batch_key,$command_key,&$error=null,$retry_of_batch_id=null)
{
    $error=null;$actor=hrm_tim_002_actor();
    if($actor<=0||!hrm_tim_002_access('SA_HRM_INGEST_TIME_IMPORT')||!hrm_tim_002_access('SA_HRM_INGEST_TIME')){$error='access_denied';return false;}
    $ck=hrm_tim_002_source_key($connector_key,64);$bk=hrm_tim_002_source_key($source_batch_key,128);$cmd=hrm_tim_002_source_key($command_key,128);
    $retry=$retry_of_batch_id===null||$retry_of_batch_id===''?null:(int)$retry_of_batch_id;
    if($ck===false||$bk===false||$cmd===false||($retry!==null&&$retry<=0)||!is_file($tmp_path)||is_link($tmp_path)){$error='import_identity_or_file_invalid';return false;}
    $today=date('Y-m-d');
    $q=db_query('SELECT * FROM '.TB_PREF.'hrm_time_import_connectors WHERE connector_key='.db_escape($ck).' AND channel_kind=\'csv\' AND effective_from<='.db_escape($today).' AND (effective_to IS NULL OR effective_to>='.db_escape($today).') ORDER BY effective_from DESC,connector_id DESC LIMIT 2');
    if(!$q||db_num_rows($q)!==1){$error='connector_missing_or_ambiguous';return false;}
    $con=db_fetch_assoc($q);$size=filesize($tmp_path);
    if($size===false||$size<1||$size>(int)$con['max_bytes']){$error='import_size_invalid';return false;}
    $content=file_get_contents($tmp_path);
    if(!is_string($content)||(function_exists('mb_check_encoding')&&!mb_check_encoding($content,'UTF-8'))){$error='import_encoding_invalid';return false;}
    $content_sha=hash('sha256',$content);$filename_sha=hash('sha256',basename((string)$original_name));

    // Parse/normalize before reserving immutable batch custody. No application state is mutated here.
    $fh=fopen($tmp_path,'rb');if(!$fh){$error='import_open_failed';return false;}
    $header=fgetcsv($fh);$expected=array('source_item_id','assignment_id','event_kind','event_time_local','timezone_name','utc_offset_minutes');
    if($header!==$expected){fclose($fh);$error='import_header_invalid';return false;}
    $rows=array();$seen=array();$line=1;
    while(($r=fgetcsv($fh))!==false){
        $line++;
        if(count($r)!==6){$rows[]=array('line'=>$line,'error'=>'malformed_row','raw'=>$r);continue;}
        $key=hrm_tim_002_source_key(trim($r[0]),128);$payload=hash('sha256',json_encode($r,JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE));
        if($key===false){$rows[]=array('line'=>$line,'error'=>'source_item_key_invalid','raw'=>$r);continue;}
        if(isset($seen[$key])){$rows[]=array('line'=>$line,'error'=>$seen[$key]===$payload?'duplicate_source_item_key':'conflicting_duplicate_source_item_key','source_item_key'=>$key,'raw'=>$r);continue;}
        $seen[$key]=$payload;
        $rows[]=array('line'=>$line,'source_item_key'=>$key,'payload_sha256'=>$payload,'assignment_id'=>(int)$r[1],'event_kind'=>trim($r[2]),'event_time_local'=>trim($r[3]),'timezone_name'=>trim($r[4]),'utc_offset_minutes'=>trim($r[5]));
        if(count($rows)>(int)$con['max_items']){fclose($fh);$error='import_item_limit_exceeded';return false;}
    }
    fclose($fh);

    // Serialize source-batch reservation on the connector row so same-key concurrent arrivals cannot race.
    begin_transaction();
    $lq=db_query('SELECT connector_id FROM '.TB_PREF.'hrm_time_import_connectors WHERE connector_id='.(int)$con['connector_id'].' LIMIT 1 FOR UPDATE');
    if(!$lq||db_num_rows($lq)!==1){cancel_transaction();$error='connector_lock_failed';return false;}
    $q=db_query('SELECT import_batch_id,content_sha256,status_token,batch_sha256,retry_of_batch_id FROM '.TB_PREF.'hrm_time_import_batches WHERE connector_id='.(int)$con['connector_id'].' AND source_batch_key='.db_escape($bk).' AND retry_of_batch_id IS NULL ORDER BY import_batch_id ASC LIMIT 1 FOR UPDATE');
    $old=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;

    if($retry!==null){
        $rq=db_query('SELECT import_batch_id,connector_id,source_batch_key,content_sha256,status_token FROM '.TB_PREF.'hrm_time_import_batches WHERE import_batch_id='.$retry.' LIMIT 2 FOR UPDATE');
        $rb=$rq&&db_num_rows($rq)===1?db_fetch_assoc($rq):false;
        if(!$rb||(int)$rb['connector_id']!==(int)$con['connector_id']||(string)$rb['source_batch_key']!==$bk||!hash_equals((string)$rb['content_sha256'],$content_sha)){cancel_transaction();$error='retry_predecessor_mismatch';return false;}
        $cq=db_query('SELECT import_batch_id FROM '.TB_PREF.'hrm_time_import_batches WHERE retry_of_batch_id='.$retry.' LIMIT 1 FOR UPDATE');
        if(!$cq||db_num_rows($cq)>0){cancel_transaction();$error='retry_predecessor_already_consumed';return false;}
        $sq=db_query('SELECT SUM(CASE WHEN status_token IN (\'accepted\',\'replay\') THEN 1 ELSE 0 END) accepted_count,SUM(CASE WHEN status_token NOT IN (\'accepted\',\'replay\') THEN 1 ELSE 0 END) quarantined_count FROM '.TB_PREF.'hrm_time_import_items WHERE import_batch_id='.$retry.' FOR UPDATE');
        $sr=$sq?db_fetch_assoc($sq):false;
        if(!$sr||(int)$sr['quarantined_count']<1){cancel_transaction();$error='retry_not_required';return false;}
    } else if($old!==false){
        if(hash_equals((string)$old['content_sha256'],$content_sha)){
            $old_id=(int)$old['import_batch_id'];commit_transaction();$summary=hrm_tim_002_import_batch_summary($old_id);
            if($summary===false){$error='batch_replay_summary_failed';return false;}
            return array('import_batch_id'=>$old_id,'status'=>$summary['status'],'replay'=>true,'accepted'=>$summary['accepted'],'quarantined'=>$summary['quarantined']);
        }
        $batchsha=hrm_tim_002_hash(array('connector_id'=>(int)$con['connector_id'],'source_batch_key'=>$bk,'content_sha256'=>$content_sha,'conflict_with'=>(int)$old['import_batch_id'],'command_key'=>$cmd));
        $sql='INSERT INTO '.TB_PREF.'hrm_time_import_batches (connector_id,legal_entity_id,source_batch_key,content_sha256,file_name_sha256,file_size_bytes,normalization_version,received_at_utc,command_key,retry_of_batch_id,status_token,batch_sha256,created_by) VALUES ('
          .(int)$con['connector_id'].','.(int)$con['legal_entity_id'].','.db_escape($bk).','.db_escape($content_sha).','.db_escape($filename_sha)
          .','.(int)$size.','.db_escape(HRM_TIM_002_IMPORT_NORMALIZATION_VERSION).',UTC_TIMESTAMP(),'.db_escape($cmd).',NULL,\'quarantined\','
          .db_escape($batchsha).','.$actor.')';
        if(!db_query($sql)){cancel_transaction();$error='batch_conflict_insert_failed';return false;}
        $bid=(int)db_insert_id();$ee=null;hrm_tim_002_append_import_error($bid,null,null,'conflicting_source_batch_identity',array('existing_batch_id'=>(int)$old['import_batch_id'],'attempted_content_sha256'=>$content_sha),$ee);commit_transaction();
        $error='conflicting_source_batch_identity';return array('import_batch_id'=>$bid,'status'=>'quarantined','replay'=>false,'accepted'=>0,'quarantined'=>1);
    }

    $batchsha=hrm_tim_002_hash(array('connector_id'=>(int)$con['connector_id'],'legal_entity_id'=>(int)$con['legal_entity_id'],'source_batch_key'=>$bk,'content_sha256'=>$content_sha,'normalization_version'=>HRM_TIM_002_IMPORT_NORMALIZATION_VERSION,'command_key'=>$cmd,'retry_of_batch_id'=>$retry));
    $sql='INSERT INTO '.TB_PREF.'hrm_time_import_batches (connector_id,legal_entity_id,source_batch_key,content_sha256,file_name_sha256,file_size_bytes,normalization_version,received_at_utc,command_key,retry_of_batch_id,status_token,batch_sha256,created_by) VALUES ('
      .(int)$con['connector_id'].','.(int)$con['legal_entity_id'].','.db_escape($bk).','.db_escape($content_sha).','.db_escape($filename_sha)
      .','.(int)$size.','.db_escape(HRM_TIM_002_IMPORT_NORMALIZATION_VERSION).',UTC_TIMESTAMP(),'.db_escape($cmd).','.($retry===null?'NULL':$retry)
      .',\'received\','.db_escape($batchsha).','.$actor.')';
    if(!db_query($sql)){cancel_transaction();$error='batch_insert_failed';return false;}
    $bid=(int)db_insert_id();commit_transaction();

    $accepted=0;$quarantined=0;
    foreach($rows as $r){
        $status='quarantined';$rawid=null;$rawsha=null;$itemerr=$r['error']??null;$scope=null;
        if($itemerr===null){
            if((int)$r['assignment_id']<=0||!in_array($r['event_kind'],array('clock_in','clock_out','manual_anchor'),true)||preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/D',$r['event_time_local'])!==1||hrm_tim_001_timezone($r['timezone_name'])===false||preg_match('/^-?[0-9]+$/D',$r['utc_offset_minutes'])!==1){$itemerr='row_validation_failed';}
            else{$workdate=substr($r['event_time_local'],0,10);$scope=hrm_tim_001_resolve_assignment_scope($r['assignment_id'],$workdate,false,$se);if($scope===false||(int)$scope['legal_entity_id']!==(int)$con['legal_entity_id'])$itemerr='assignment_scope_or_entity_invalid';}
        }
        if($itemerr===null){
            $ev=array('source_channel'=>'tim2:'.$ck,'source_event_key'=>$bk.':'.$r['source_item_key'],'recorded_at_utc'=>gmdate('Y-m-d H:i:s'),
              'effective_local_at'=>$r['event_time_local'],'timezone_name'=>$r['timezone_name'],'utc_offset_minutes'=>(int)$r['utc_offset_minutes'],
              'event_type'=>$r['event_kind'],'source_payload_sha256'=>$r['payload_sha256'],'ingest_command_key'=>'tim2-import:'
              .$bid.':'.$r['source_item_key']);
            $res=hrm_tim_001_ingest_raw_event((int)$r['assignment_id'],$ev,$ie);
            if($res===false)$itemerr=$ie?:'raw_event_ingest_failed';else{$status=$res['replay']?'replay':'accepted';$rawid=(int)$res['raw_event_id'];$rawsha=$res['event_sha256'];$accepted++;}
        }
        if($itemerr!==null)$quarantined++;
        $source_key=$r['source_item_key']??('line-'.$r['line']);$payload_sha=$r['payload_sha256']??hash('sha256',json_encode($r['raw']??array()));
        $itemsha=hrm_tim_002_hash(array('import_batch_id'=>$bid,'source_item_key'=>$source_key,'item_index'=>$r['line'],'payload_sha256'=>$payload_sha,'status_token'=>$status,'raw_event_id'=>$rawid,'error_code'=>$itemerr));
        begin_transaction();
        $sql='INSERT INTO '.TB_PREF.'hrm_time_import_items (import_batch_id,source_item_key,item_index,payload_sha256,event_kind,effective_at_utc,worker_id,employment_id,assignment_id,legal_entity_id,raw_event_id,raw_event_sha256,predecessor_item_id,status_token,item_sha256) VALUES ('
          .$bid.','.db_escape($source_key).','.(int)$r['line'].','.db_escape($payload_sha).','.db_escape($r['event_kind']??'invalid')
          .','.($rawid===null?'NULL':'(SELECT effective_utc_at FROM '.TB_PREF.'hrm_time_raw_events WHERE raw_event_id='.$rawid
          .')').','.($scope?(int)$scope['worker_id']:'NULL').','.($scope?(int)$scope['employment_id']:'NULL').','.($scope?(int)$scope['assignment_id']:'NULL')
          .','.($scope?(int)$scope['legal_entity_id']:'NULL').','.($rawid===null?'NULL':$rawid).','.($rawsha===null?'NULL':db_escape($rawsha))
          .',NULL,'.db_escape($status).','.db_escape($itemsha).')';
        if(!db_query($sql)){cancel_transaction();$error='item_custody_insert_failed';return false;}
        $iid=(int)db_insert_id();
        if($itemerr!==null){$ee=null;if(hrm_tim_002_append_import_error($bid,$iid,$source_key,$itemerr,array('line'=>(int)$r['line'],'payload_sha256'=>$r['payload_sha256']??null),$ee)===false){cancel_transaction();$error=$ee?:'import_error_custody_failed';return false;}}
        commit_transaction();
    }
    return array('import_batch_id'=>$bid,'status'=>$quarantined?($accepted?'partial':'quarantined'):'accepted','replay'=>false,'accepted'=>$accepted,'quarantined'=>$quarantined,'retry_of_batch_id'=>$retry);
}
?>
