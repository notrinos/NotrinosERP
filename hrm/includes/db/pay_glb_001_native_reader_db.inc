<?php
/** A6 source-admitted current native observations; no grants, payroll decision or financial effect. */
require_once dirname(__FILE__).'/pay_glb_001_native_finance_db.inc';
require_once dirname(__FILE__).'/employee_person_worker_db.inc';
require_once dirname(__FILE__).'/pay_glb_001_native_reader_schema_db.inc';
require_once dirname(__FILE__).'/pay_glb_001_native_calendar_db.inc';
require_once dirname(dirname(dirname(dirname(__FILE__)))).'/includes/pay_glb_001_native_reader_view.inc';

/** Require a real current session, all existing read/review permissions and an explicit default-off reader switch. */
function pay_glb_001_native_reader_principal(&$error=null)
{
    global $db_connections;
    $p=pay_glb_001_current_principal('native_reader');
    if($p===false || !isset($db_connections[$p['company_id']]['pay_glb_001_native_reader']['enabled'])
        || $db_connections[$p['company_id']]['pay_glb_001_native_reader']['enabled']!==true) {
        $error='native_reader_disabled_or_principal_denied';return false;
    }
    if(!empty($GLOBALS['transaction_level']) && !pay_glb_001_native_execution_lock_principal($p,'native_reader')) {
        $error='native_reader_current_principal_denied';return false;
    }
    return $p;
}

/** Resolve current object grants before querying any payroll object, locking the permitted legal rows. */
function pay_glb_001_native_reader_access(array $p,&$error=null)
{
    $store=pay_glb_001_read_admission_store('acl');$codes=array();
    if($store===false)return false;
    foreach($store['records'] as $lease)if(isset($lease['actor_user_id'],$lease['entity_code'])
        && $lease['actor_user_id']===$p['actor_user_id']) {
        if(!pay_glb_001_code($lease['entity_code']))return false;$codes[$lease['entity_code']]=true;
    }
    $codes=array_keys($codes);sort($codes,SORT_STRING);
    if(count($codes)<1 || count($codes)>32){$error='native_reader_object_scope_required';return false;}
    $literals=array();foreach($codes as $code)$literals[]=db_escape($code);
    $q=db_query('SELECT legal_entity_id FROM '.TB_PREF.'hrm_legal_entities WHERE entity_code IN ('.implode(',',$literals).') ORDER BY legal_entity_id FOR UPDATE');
    if(!$q || db_num_rows($q)!==count($codes))return false;
    return pay_glb_001_resolve_server_access($codes);
}

/** Stream the permitted canonical identity graph in bounded SQL pages without exposing identifiers. */
function pay_glb_001_native_reader_identity_epoch(array $access,&$error=null)
{
    $ids=array();foreach($access['entities'] as $entity)$ids[]=(int)$entity['legal_entity_id'];
    if(count($ids)<1 || count($ids)>32){$error='native_reader_identity_scope_invalid';return false;}
    $hash=hash_init('sha256');$last='0';$started=microtime(true);
    do {
        $q=db_query('SELECT DISTINCT w.worker_id,w.person_id,m.employee_number,m.employee_id FROM '.TB_PREF.'hrm_workers w '
            .'INNER JOIN '.TB_PREF.'hrm_persons p ON p.person_id=w.person_id '
            .'INNER JOIN '.TB_PREF.'hrm_employee_worker_map m ON m.worker_id=w.worker_id '
            .'INNER JOIN '.TB_PREF.'hrm_payroll_relationships pr ON pr.worker_id=w.worker_id '
            .'WHERE m.retired_at IS NULL AND pr.legal_entity_id IN ('.implode(',',$ids).') AND w.worker_id>'.db_escape($last)
            .' ORDER BY w.worker_id LIMIT 512');
        if(!$q || microtime(true)-$started>60){$error='native_reader_identity_unavailable';return false;}
        $count=db_num_rows($q);
        while($row=db_fetch_assoc($q)){
            $row=pay_glb_001_native_producer_sql_strings($row,$error);
            if($row===false || pay_glb_001_native_helper_id($row['worker_id'],$error)===false
                || pay_glb_001_native_helper_id($row['person_id'],$error)===false){$error='native_reader_identity_invalid';return false;}
            $bytes=hrm_pay_core_004_canonical_json($row);if($bytes===false)return false;
            hash_update($hash,strlen($bytes).':'.$bytes."\n");$last=$row['worker_id'];
        }
    } while($count===512);
    return hash_final($hash);
}

/** Bind pagination to authorized scope, canonical identities, custody heads and non-read causal changes. */
function pay_glb_001_native_reader_epoch(array $p,array $access,&$error=null)
{
    $heads=array();
    foreach(array('hrm_pay_glb_001_native_selection_events'=>'selection_event_id',
        'hrm_pay_glb_001_native_approval_bindings'=>'approval_binding_id',
        'hrm_pay_glb_001_native_gl_bindings'=>'gl_binding_id') as $table=>$id) {
        $q=db_query('SELECT COUNT(*) n,COALESCE(MAX('.$id.'),0) head FROM '.TB_PREF.$table);
        if(!$q || db_num_rows($q)!==1)return false;$heads[$table]=db_fetch_assoc($q);
    }
    $ids=array();foreach($access['entities'] as $entity)$ids[]=(int)$entity['legal_entity_id'];
    $q=db_query('SELECT COUNT(*) n,COALESCE(MAX(e.execution_id),0) head FROM '.TB_PREF.'hrm_payroll_seed_executions e '
        .'JOIN '.TB_PREF.'hrm_payroll_seed_proposals p ON p.proposal_id=e.proposal_id WHERE p.legal_entity_id IN ('.implode(',',$ids).')');
    if(!$q || db_num_rows($q)!==1)return false;$heads['calendar_seed']=db_fetch_assoc($q);
    $q=db_query('SELECT COUNT(*) n,COALESCE(MAX(p.calendar_period_id),0) head FROM '.TB_PREF.'hrm_payroll_calendar_periods p '
        .'JOIN '.TB_PREF.'hrm_payroll_calendar_versions v ON v.calendar_version_id=p.calendar_version_id '
        .'JOIN '.TB_PREF.'hrm_payroll_calendars c ON c.calendar_id=v.calendar_id WHERE c.legal_entity_id IN ('.implode(',',$ids).')');
    if(!$q || db_num_rows($q)!==1)return false;$heads['calendar_periods']=db_fetch_assoc($q);
    $q=db_query('SELECT COUNT(*) n,COALESCE(MAX(event_id),0) head FROM '.TB_PREF.'domain_audit_events WHERE company_id='.(int)$p['company_id']
        ." AND action NOT IN ('pay_glb.admission.allowed','pay_glb.admission.denied','pay_glb.native.reader_observed')");
    if(!$q || db_num_rows($q)!==1)return false;$heads['causal_audit']=db_fetch_assoc($q);
    $business=pay_glb_001_read_admission_store('business');if($business===false)return false;
    $identity=pay_glb_001_native_reader_identity_epoch($access,$error);if($identity===false)return false;
    return pay_glb_001_native_producer_hash('reader-epoch',array('company_id'=>(string)$p['company_id'],
        'actor_id'=>(string)$p['actor_user_id'],'entities'=>$access['entities'],'acl_sha256'=>$access['store_sha256'],
        'business_sha256'=>$business['store_sha256'],'identity_sha256'=>$identity,'heads'=>$heads));
}

/** Decode only an actor/epoch/expiry-bound pagination position; this token never grants access. */
function pay_glb_001_native_reader_cursor($cursor,array $p,$epoch,$key,&$error=null)
{
    if($cursor==='')return 0;
    if(!is_string($cursor) || strlen($cursor)>2048 || preg_match('/^[A-Za-z0-9_-]+$/D',$cursor)!==1)return false;
    $raw=base64_decode(strtr($cursor,'-_','+/'),true);$c=$raw===false?false:json_decode($raw,true);
    if(!pay_glb_001_exact_keys($c,array('schema','company_id','actor_id','epoch_sha256','offset','expires_at_utc','mac'))
        || $c['schema']!=='PAY_GLB_001_NATIVE_PAGE_V1' || $c['company_id']!==$p['company_id'] || $c['actor_id']!==$p['actor_user_id']
        || $c['epoch_sha256']!==$epoch || !is_int($c['offset']) || $c['offset']<0 || $c['offset']>4096
        || !pay_glb_001_iso_utc($c['expires_at_utc']) || $c['expires_at_utc']<=gmdate('Y-m-d\TH:i:s\Z')
        || strtotime($c['expires_at_utc'])-time()>60 || !pay_glb_001_sha256($c['mac'])) {
        $error='native_reader_cursor_stale_or_denied';return false;
    }
    $body=$c;unset($body['mac']);$mac=domain_audit_sensitive_hash($body,$key,'pay-glb-native-page-v1');
    if(!is_string($mac) || !hash_equals($mac,$c['mac']))return false;return $c['offset'];
}

/** Encode a bounded next position using the existing current signing key without creating a key or access grant. */
function pay_glb_001_native_reader_next(array $p,$epoch,$offset,$key)
{
    $c=array('schema'=>'PAY_GLB_001_NATIVE_PAGE_V1','company_id'=>$p['company_id'],'actor_id'=>$p['actor_user_id'],
        'epoch_sha256'=>$epoch,'offset'=>$offset,'expires_at_utc'=>gmdate('Y-m-d\TH:i:s\Z',time()+60));
    $c['mac']=domain_audit_sensitive_hash($c,$key,'pay-glb-native-page-v1');
    return is_string($c['mac'])?rtrim(strtr(base64_encode(json_encode($c,JSON_UNESCAPED_SLASHES)),'+/','-_'),'='):false;
}

/** Verify actual journal/reconciliation/review/authority bodies and their protected native causal receipts. */
function pay_glb_001_native_reader_financial_proof(array $j,array $binding,array $leaf,&$error=null)
{
    $a=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_gl_001_posting_authorities WHERE authority_id='.(int)$leaf['authority_id'].' LIMIT 2 FOR UPDATE');
    $r=$a?hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_gl_001_reconciliations WHERE reconciliation_id='.(int)$a['reconciliation_id'].' LIMIT 2 FOR UPDATE'):false;
    $v=$a?hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_gl_001_reviews WHERE review_id='.(int)$a['review_id'].' LIMIT 2 FOR UPDATE'):false;
    if(!$a || !$r || !$v || (string)$a['journal_intent_id']!==(string)$j['journal_intent_id']
        || (string)$r['journal_intent_id']!==(string)$j['journal_intent_id'] || (string)$v['reconciliation_id']!==(string)$r['reconciliation_id']
        || $r['status']!=='reconciled' || $v['decision']!=='approved'
        || $r['journal_sha256']!==$j['journal_sha256'] || $r['line_set_sha256']!==$j['line_set_sha256'] || $r['allocation_set_sha256']!==$j['allocation_set_sha256']
        || pay_gl_001_hash(array('journal_sha256'=>$j['journal_sha256'],'line_set_sha256'=>$j['line_set_sha256'],
            'allocation_set_sha256'=>$j['allocation_set_sha256'],'native_projection_set_sha256'=>$r['native_projection_set_sha256'],
            'debit_minor'=>(int)$j['debit_minor'],'credit_minor'=>(int)$j['credit_minor'],'status'=>'reconciled'))!==$r['reconciliation_sha256']
        || pay_gl_001_hash(array('reconciliation_sha256'=>$r['reconciliation_sha256'],'decision'=>$v['decision'],
            'reviewer_id'=>(int)$v['reviewer_id'],'reviewed_at'=>$v['reviewed_at']))!==$v['review_sha256']
        || pay_gl_001_hash(array('journal_sha256'=>$j['journal_sha256'],'reconciliation_id'=>(int)$r['reconciliation_id'],
            'review_sha256'=>$v['review_sha256'],'command_key_sha256'=>$a['command_key_sha256'],
            'executor_id'=>(int)$a['executor_id'],'authorized_at'=>$a['authorized_at']))!==$a['authority_sha256']){$error='native_reader_financial_chain_body_invalid';return false;}
    foreach(array(array('gl_reconcile','pay_gl_reconciliation',$r['reconciliation_id'],$r['reconciliation_sha256']),
        array('gl_review','pay_gl_review',$v['review_id'],$v['review_sha256']),
        array('gl_authorize','pay_gl_authority',$a['authority_id'],$a['authority_sha256'])) as $effect)
        if(pay_glb_001_native_finance_effect_proof($effect[0],$effect[1],(string)$effect[2],$effect[3],$binding,$error)===false)return false;
    return true;
}

/** Resolve verified members through existing compatibility bindings to distinct current Persons. */
function pay_glb_001_native_reader_population(array $artifacts,array $period,&$error=null)
{
    $people=array();$bindings=array();
    foreach($artifacts as $artifact){
        $d=$artifact['descriptor'];
        $employee=hrm_pay_core_010_employee_for_result($d,$period,$error);
        $binding=$employee===false?false:get_hrm_person_worker_binding((int)$employee['employee_number'],false);
        if(!$employee || !$binding || $binding['retired_at']!==null
            || $binding['employee_id']!==$employee['employee_id']
            || hash('sha256',$employee['employee_id'])!==$d['employee_id_sha256']
            || pay_glb_001_native_helper_id((string)$binding['person_id'],$error)===false
            || pay_glb_001_native_helper_id((string)$binding['worker_id'],$error)===false){
            $error='native_reader_person_binding_unavailable';return false;
        }
        $person=hrm_pay_core_004_read_one('SELECT person_id FROM '.TB_PREF.'hrm_persons WHERE person_id='.(int)$binding['person_id'].' LIMIT 2 FOR UPDATE');
        if(!$person){$error='native_reader_person_missing';return false;}
        $locked=get_hrm_person_worker_binding((int)$employee['employee_number'],true);
        if(!$locked || $locked['retired_at']!==null || (string)$locked['person_id']!==(string)$person['person_id']
            || (string)$locked['worker_id']!==(string)$binding['worker_id'] || $locked['employee_id']!==$employee['employee_id']){
            $error='native_reader_person_binding_changed';return false;
        }
        $people[(string)$person['person_id']]=true;
        $bindings[]=array('relationship_id'=>(string)$d['payroll_relationship_id'],'employee_sha256'=>$d['employee_id_sha256'],
            'worker_id'=>(string)$binding['worker_id'],'person_id'=>(string)$person['person_id']);
    }
    $sha=pay_glb_001_native_producer_hash('reader-person-population',$bindings);
    if(count($people)<1 || $sha===false){$error='native_reader_person_population_unavailable';return false;}
    return array('count'=>count($people),'bindings_sha256'=>$sha);
}

/** Reconstruct one current selected scope from the genuine producer and existing current approval/GL chains. */
function pay_glb_001_native_reader_scope(array $scope,array $entity,&$error=null)
{
    $latest=pay_glb_001_native_selection_latest($scope,$error);if(!$latest){if($error===null)$error='native_reader_selection_unavailable';return false;}
    $period=hrm_pay_core_010_period_scope((int)$scope['payroll_period_id'],true,$error);
    $group=hrm_pay_core_004_read_one('SELECT group_code FROM '.TB_PREF.'hrm_payroll_groups WHERE payroll_group_id='.(int)$scope['payroll_group_id'].' LIMIT 2 FOR UPDATE');
    if(!$period || !$group || !pay_glb_001_native_reader_group_code($group['group_code'])){$error='native_reader_period_or_group_unavailable';return false;}
    $key=pay_glb_001_native_producer_hash('reader-key',array('actor_id'=>(string)$_SESSION['wa_current_user']->user,'selection_sha256'=>$latest['selection_sha256']));
    if($latest['event_kind']!=='select'){
        $error='native_reader_negative_selection_unavailable';return false;
    }
    $handoff=pay_glb_001_native_reader_schema_cached('finance');
    if($handoff)$GLOBALS['pay_glb_001_native_reader_schema_context']['projection_selection']=array('scope'=>$scope,'latest'=>$latest);
    try {
        $prepared=pay_glb_001_native_projection_prepare_locked((string)$latest['selection_event_id'],$key,'read',$error);
    } finally {
        if($handoff)unset($GLOBALS['pay_glb_001_native_reader_schema_context']['projection_selection']);
    }
    if($prepared===false || pay_glb_001_native_projection_existing_locked($prepared,$error)===false){if($error===null)$error='native_reader_projection_unavailable';return false;}
    $population=pay_glb_001_native_reader_population($prepared['artifacts'],$period,$error);if($population===false)return false;
    $calendar=pay_glb_001_native_calendar_collect($prepared['request']['scope'],$error);if($calendar===false)return false;
    $request=$prepared['request'];
    $request['command_key_sha256']=pay_glb_001_native_producer_hash('reader-person-calendar-key',array('key'=>$key,
        'bindings_sha256'=>$population['bindings_sha256'],'calendar_sha256'=>$calendar['proof_sha256']));
    unset($request['request_sha256']);$request['request_sha256']=pay_glb_001_native_producer_hash('business-request',$request);
    $row=array('entity_code'=>$entity['entity_code'],'country_code'=>$entity['country_code'],
        'payroll_group_code'=>$group['group_code'],'period_start'=>$period['from_date'],'period_end'=>$period['to_date'],
        'source_kind'=>'GOVERNED_NATIVE_V1','payroll_state'=>'SELECTED_CURRENT_UNAPPROVED','financial_state'=>'NO_CURRENT_APPROVAL',
        'population'=>$population['count'],'revision_sha256'=>$latest['selection_sha256'],'calendar'=>$calendar);
    $q=db_query('SELECT approval_id FROM '.TB_PREF.'hrm_pay_glb_001_native_approval_bindings WHERE selection_event_id='.(int)$latest['selection_event_id'].' LIMIT 2 FOR UPDATE');
    if(!$q || db_num_rows($q)>1){$error='native_reader_approval_history_unavailable';return false;}
    if(db_num_rows($q)===1) {
        $a=db_fetch_assoc($q);$binding=pay_glb_001_native_finance_approval_binding((string)$a['approval_id'],true,$error);if($binding===false){if($error===null)$error='native_reader_approval_binding_unavailable';return false;}
        $row['payroll_state']='APPROVED_CURRENT';$row['financial_state']='JOURNAL_UNPOSTED';$active=array();$pending=0;$reversed=0;
        $q=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_gl_001_journal_intents WHERE approval_sha256='.db_escape($binding['approval']['approval_sha256'])
            .' AND result_set_sha256='.db_escape($binding['intent']['result_set_sha256']).' ORDER BY journal_intent_id LIMIT 65 FOR UPDATE');
        if(!$q || db_num_rows($q)>64){$error='native_reader_journal_history_unavailable';return false;}
        while($j=db_fetch_assoc($q)) {
            if(pay_glb_001_native_finance_effect_proof('gl_intent','pay_gl_journal_intent',(string)$j['journal_intent_id'],$j['journal_sha256'],$binding,$error)===false){if($error===null)$error='native_reader_journal_effect_unavailable';return false;}
            $leaf=pay_glb_001_native_finance_posting_leaf($j,$error);if($leaf===false){if($error===null)$error='native_reader_posting_chain_unavailable';return false;}
            if($leaf===null){$pending++;continue;}
            if(!pay_glb_001_native_reader_financial_proof($j,$binding,$leaf,$error)){if($error===null)$error='native_reader_financial_effect_unavailable';return false;}
            if($leaf['posting_kind']==='reverse')$reversed++;else $active[]=$leaf['posting_kind'];
        }
        if(count($active)>1 || (count($active)===1 && $pending>0))$row['financial_state']='UNAVAILABLE_CONFLICT';
        elseif(count($active)===1)$row['financial_state']=$active[0]==='post'?'POSTED_RECONCILED':'REPOSTED_RECONCILED';
        elseif($pending===0 && $reversed>0)$row['financial_state']='FULLY_REVERSED';
    }
    return array('request'=>$request,'row'=>$row);
}

/** Collect only one coherent locked page; preparation returns requests only and never grants their use. */
function pay_glb_001_native_reader_window($cursor,$prepare,&$error=null)
{
    $error=null;$started=microtime(true);$stage='prerequisites';
    if(!is_bool($prepare) || !is_string($cursor) || !empty($GLOBALS['transaction_level'])
        || !pay_glb_001_native_selection_begin($error))return false;
    try {
        $p=pay_glb_001_native_reader_principal($error);
        $key=$p===false?false:domain_audit_signing_key($p['company_id'],false);
        if($key===false || !pay_glb_001_native_producer_hold_key_lock($p['company_id'],$error)
            || !pay_glb_001_native_reader_schema_begin($p,$started,$error))throw new RuntimeException('reader prerequisites');
        $stage='access';$key_id=domain_audit_key_id($key);$access=pay_glb_001_native_reader_access($p,$error);
        if($access===false)throw new RuntimeException('reader access');
        $stage='cursor';$epoch=pay_glb_001_native_reader_epoch($p,$access,$error);$offset=pay_glb_001_native_reader_cursor($cursor,$p,$epoch,$key,$error);
        if($offset===false)throw new RuntimeException('reader cursor');
        $entities=array();$ids=array();foreach($access['entities'] as $e){$entities[(string)$e['legal_entity_id']]=$e;$ids[]=(int)$e['legal_entity_id'];}
        $q=db_query('SELECT DISTINCT company_id,legal_entity_id,payroll_group_id,payroll_period_id FROM '.TB_PREF.'hrm_pay_glb_001_native_selection_events '
            .'WHERE company_id='.(int)$p['company_id'].' AND legal_entity_id IN ('.implode(',',$ids).') '
            .'ORDER BY payroll_period_id DESC,legal_entity_id,payroll_group_id LIMIT 4097');
        if(!$q || db_num_rows($q)>4096)throw new RuntimeException('reader scope bound');
        $scopes=array();while($s=db_fetch_assoc($q)){
            $s=pay_glb_001_native_producer_sql_strings($s,$error);if($s===false)throw new RuntimeException('reader scope scalar');
            $scopes[]=$s;
        }
        if($offset>count($scopes))throw new RuntimeException('reader position');
        $stage='source';$rows=array();$requests=array();$admissions=array();
        foreach(array_slice($scopes,$offset,25) as $s) {
            if(microtime(true)-$started>60)throw new RuntimeException('reader deadline');
            $item=pay_glb_001_native_reader_scope($s,$entities[$s['legal_entity_id']],$error);
            if($item===false || microtime(true)-$started>60)throw new RuntimeException('reader source');
            if(!$prepare){$a=pay_glb_001_native_business_admission($item['request'],$error);if($a===false)throw new RuntimeException('reader lease');$admissions[]=$a;}
            $requests[]=$item['request'];$rows[]=$item['row'];
        }
        $next=$offset+count($rows)<count($scopes)?pay_glb_001_native_reader_next($p,$epoch,$offset+count($rows),$key):'';
        $stage='completion';$view=count($rows)>0?pay_glb_001_native_calendar_minimize($rows,gmdate('Y-m-d\TH:i:s\Z'),$error)
            :array('schema'=>'PAY_GLB_001_NATIVE_MINIMUM_VIEW_V2','observed_at_utc'=>gmdate('Y-m-d\TH:i:s\Z'),'rows'=>array());
        if($view===false || $next===false || microtime(true)-$started>60
            || pay_glb_001_native_reader_principal($error)===false)throw new RuntimeException('reader completion');
        foreach($requests as $i=>$request)if(!$prepare && pay_glb_001_native_business_admission($request,$error)!==$admissions[$i])throw new RuntimeException('reader source fence');
        $stage='fence';$fence=pay_glb_001_read_admission_store('acl');$current_key=domain_audit_signing_key($p['company_id'],false);
        if($fence===false || $fence['store_sha256']!==$access['store_sha256'] || $current_key===false
            || domain_audit_key_id($current_key)!==$key_id || !pay_glb_001_native_reader_schema_cached('finance')
            || pay_glb_001_native_reader_epoch($p,$access,$error)!==$epoch)throw new RuntimeException('reader epoch fence');
        if(!pay_glb_001_native_producer_finish(true,$error))return false;
        return $prepare?array('requests'=>$requests,'next_cursor'=>$next):array('view'=>$view,'next_cursor'=>$next);
    } catch(Exception $e){if($error===null)$error='native_reader_unavailable_'.$stage;pay_glb_001_native_producer_finish(false,$error);return false;
    } catch(Throwable $e){error_log('S39 reader window '.get_class($e).' at '.basename($e->getFile()).':'.$e->getLine());if($error===null)$error='native_reader_unavailable';pay_glb_001_native_producer_finish(false,$error);return false;}
}

/** Trusted provisioning preparation only; no source grant or observed payroll payload is returned. */
function pay_glb_001_native_reader_prepare($cursor='',&$error=null){return pay_glb_001_native_reader_window($cursor,true,$error);}

/** Return one freshly admitted minimum-data page; caller cannot supply company, entity, group, period or observations. */
function pay_glb_001_native_read_current($cursor='',&$error=null){return pay_glb_001_native_reader_window($cursor,false,$error);}

/** Resolve a real pending native draft by its immutable submission, then require actual current workflow eligibility. */
function pay_glb_001_native_reader_review_locked(array $payload,&$error=null)
{
    $p=pay_glb_001_native_reader_principal($error);
    if($p===false || pay_glb_001_native_approval_principal('approve',$error)===false
        || !isset($payload['command_key_sha256'],$payload['payload_sha256'])
        || !pay_glb_001_sha256($payload['command_key_sha256']) || !pay_glb_001_sha256($payload['payload_sha256']))return false;
    $cmd=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_command_receipts WHERE company_id='.(int)$p['company_id']
        ." AND command_domain='native.submit' AND command_key_sha256=".db_escape($payload['command_key_sha256']).' LIMIT 2 FOR UPDATE');
    if(!$cmd || $cmd['effect_kind']!=='central_payroll_draft'){$error='native_reader_review_submission_unavailable';return false;}
    $draft=get_approval_draft_for_update((int)$cmd['effect_id']);$stored=$draft?pay_glb_001_native_approval_decode($draft,$error):false;
    if($stored===false || hrm_pay_core_004_canonical_json($stored)!==hrm_pay_core_004_canonical_json($payload)){$error='native_reader_review_payload_unavailable';return false;}
    $service=new ApprovalWorkflow((int)$_SESSION['wa_current_user']->access,(int)$_SESSION['wa_current_user']->user);
    if(!$service->canActOnDraft((int)$draft['id'])){$error='native_reader_actual_workflow_review_denied';return false;}
    $o=pay_glb_001_native_approval_action_context((string)$draft['id'],'approve','',$error);
    if($o===false){
        if($error==='native_approval_current_payload_required'){
            $detail_error=null;$detail=pay_glb_001_native_approval_context($stored['selection_id'],$stored['approval_intent_id'],$detail_error);
            if($detail===false && $detail_error!==null)$error=$detail_error;
        }
        return false;
    }
    $access=pay_glb_001_native_reader_access($p,$error);if($access===false){$error='native_reader_review_object_unavailable';return false;}$matched=false;
    foreach($access['entities'] as $entity)if((string)$entity['legal_entity_id']===$o['context']['prepared']['request']['scope']['legal_entity_id'])$matched=true;
    if(!$matched){$error='native_reader_review_object_denied';return false;}
    $request=pay_glb_001_native_approval_request('review',$o['context']['prepared']['request'],$stored,$o['policy'],$draft,'');
    return array('request'=>$request,'payload'=>$stored,'context'=>$o['context'],'access'=>$access);
}

/** Prepare only a real workflow/object/source-bound review request; this does not disclose or approve payroll. */
function pay_glb_001_native_reader_prepare_review(array $payload,&$error=null)
{
    $error=null;
    if(!empty($GLOBALS['transaction_level']) || !pay_glb_001_native_selection_begin($error))return false;
    try {
        $p=pay_glb_001_native_reader_principal($error);
        if($p===false || domain_audit_signing_key($p['company_id'],false)===false
            || !pay_glb_001_native_producer_hold_key_lock($p['company_id'],$error)){$error='native_reader_review_prerequisites_unavailable';throw new RuntimeException('review prerequisites');}
        $o=pay_glb_001_native_reader_review_locked($payload,$error);
        if(!pay_glb_001_native_producer_finish($o!==false,$error))return false;
        return $o===false?false:array('request'=>$o['request']);
    } catch(Exception $e){if($error===null){error_log('S39 reader review preparation '.get_class($e).' at '.basename($e->getFile()).':'.$e->getLine());$error='native_reader_review_prepare_unavailable';}pay_glb_001_native_producer_finish(false,$error);return false;
    } catch(Throwable $e){
        error_log('S39 reader review preparation '.get_class($e).' at '.basename($e->getFile()).':'.$e->getLine());
        if($error===null)$error='native_reader_review_prepare_unavailable';pay_glb_001_native_producer_finish(false,$error);return false;}
}

/** Disclose exact pending totals only to a current configured approver with a separate protected REVIEW lease. */
function pay_glb_001_native_reader_review(array $payload,&$error=null)
{
    $error=null;$started=microtime(true);
    if(!empty($GLOBALS['transaction_level']) || !pay_glb_001_native_selection_begin($error))return false;
    try {
        $p=pay_glb_001_native_reader_principal($error);
        if($p===false || domain_audit_signing_key($p['company_id'],false)===false
            || !pay_glb_001_native_producer_hold_key_lock($p['company_id'],$error))throw new RuntimeException('review prerequisites');
        $o=pay_glb_001_native_reader_review_locked($payload,$error);
        $admission=$o===false?false:pay_glb_001_native_business_admission($o['request'],$error);
        if($admission===false)throw new RuntimeException('review lease');
        $i=$o['context']['intent'];$scope=$o['context']['prepared']['request']['scope'];
        $period=hrm_pay_core_010_period_scope((int)$scope['payroll_period_id'],true,$error);if(!$period)throw new RuntimeException('review period');
        $view=array('schema'=>'PAY_GLB_001_NATIVE_PAYROLL_REVIEW_V1','period_start'=>$period['from_date'],'period_end'=>$period['to_date'],
            'currency'=>$scope['currency_code'],'gross'=>$i['gross_amount'],'deductions'=>$i['deduction_amount'],'net'=>$i['net_amount']);
        $acl=pay_glb_001_read_admission_store('acl');
        if($acl===false || $acl['store_sha256']!==$o['access']['store_sha256'] || microtime(true)-$started>60
            || pay_glb_001_native_reader_principal($error)===false
            || pay_glb_001_native_business_admission($o['request'],$error)!==$admission
            || !pay_glb_001_native_producer_finish(true,$error))throw new RuntimeException('review completion');
        return $view;
    } catch(Exception $e){if($error===null)$error='native_reader_review_unavailable';pay_glb_001_native_producer_finish(false,$error);return false;
    } catch(Throwable $e){if($error===null)$error='native_reader_review_unavailable';pay_glb_001_native_producer_finish(false,$error);return false;}
}
