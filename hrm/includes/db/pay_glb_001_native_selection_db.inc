<?php
/** S39 initial independent selection and negative events; no correction or financial authority. */
require_once dirname(__FILE__).'/pay_glb_001_native_producer_db.inc';
require_once dirname(dirname(dirname(dirname(__FILE__)))).'/includes/pay_glb_001_native_selection_admission.inc';

/** Use current committed audit reads only for the next owned selection transaction. */
function pay_glb_001_native_selection_begin(&$error=null)
{
    if (!empty($GLOBALS['transaction_level']) || !db_query('SET TRANSACTION ISOLATION LEVEL READ COMMITTED')) {
        $error='native_selection_transaction_isolation_unavailable';return false;
    }
    // Source/auth/period/selection rows remain locked through commit. The
    // audit-key mutex stabilizes protected chain verification; this one-shot
    // setting does not change the application's session/default isolation.
    if (begin_transaction()===false) { $error='native_selection_transaction_begin_failed';return false; }return true;
}

/** Lock and authenticate a real sealed producer operation; never adopt an arbitrary execution. */
function pay_glb_001_native_selection_operation($operation_id,$positive,$checker,&$error=null)
{
    if (pay_glb_001_native_helper_id($operation_id,$error)===false) return false;
    $start=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_command_receipts WHERE command_receipt_id='.(int)$operation_id
        .' AND company_id='.(int)user_company().' AND command_domain=\'native.start\' LIMIT 2 FOR UPDATE');
    $stored=$start?json_decode($start['request_context_json'],true):false;
    if (!is_array($stored) || !isset($stored['operation']) || !is_array($stored['operation'])
        || !isset($stored['operation']['request_sha256'],$stored['operation']['scope'],$stored['operation']['sources'])) {
        $error='native_selection_owned_operation_required';return false;
    }
    $context=$stored['operation'];$maker=(int)$start['actor_id'];$body=$context;unset($body['request_sha256']);
    if (pay_glb_001_native_producer_command_valid($start,$context['request_sha256'],$maker,$error)===false
        || pay_glb_001_native_producer_hash('request',$body)!==$context['request_sha256']) return false;
    $seal=pay_glb_001_native_producer_command('native.seal',pay_glb_001_native_producer_scope_hash($context),$context['command_key_sha256'],$error);
    if (!is_array($seal) || pay_glb_001_native_producer_command_valid($seal,$context['request_sha256'],$maker,$error)===false
        || $seal['effect_kind']!=='native_execution') { $error='native_selection_actual_terminal_required';return false; }
    $execution=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_executions WHERE native_execution_id='.(int)$seal['effect_id'].' LIMIT 2 FOR UPDATE');
    $d=$execution?pay_glb_001_native_receipt_decode_v1('execution',$execution['receipt_json'],$error):false;
    if ($d===false || $d['maker_id']!==(string)$maker || $d['request_sha256']!==$context['request_sha256']
        || $d['revision_no']!==(isset($context['replacement'])?$context['replacement']['revision_no']:'1') || $d['source_kind']!=='GOVERNED_NATIVE_V1'
        || $d['producer_contract_version']!==$context['producer_contract_version']
        || pay_glb_001_native_receipt_sha256_v1('execution',$d,$error)!==$execution['execution_sha256']
        || $execution['execution_sha256']!==$seal['effect_sha256']) { $error='native_selection_execution_identity_invalid';return false; }
    foreach (array_slice(pay_glb_001_native_receipt_fields()['execution'],0,6) as $field)
        if ($d[$field]!==$context['scope'][$field]) { $error='native_selection_execution_scope_invalid';return false; }
    if ($positive) {
        $current=pay_glb_001_native_producer_rebuild($context,$maker,$checker?'native_select':'native_execute',$error);
        if ($current===false || hrm_pay_core_004_canonical_json($context)!==hrm_pay_core_004_canonical_json($current)) {
            $error='native_selection_current_source_drift';return false;
        }
        $op=array('id'=>$operation_id,'row'=>$start,'context'=>$current);
        $material=pay_glb_001_native_producer_complete_material($op,$error,$checker?$maker:null);
        if ($material===false || pay_glb_001_native_producer_sealed_replay($op,$material,$seal,$error,$checker?$maker:null)!==(string)$execution['native_execution_id']) return false;
    }
    return array('context'=>$context,'start'=>$start,'seal'=>$seal,'execution'=>$execution,'descriptor'=>$d);
}

/** Verify bounded actual protected audit chain and the exact HMAC-bound causal event. */
function pay_glb_001_native_selection_audit_proof(array $context,$action,$effect_sha,$actor,&$error=null,$verify_chain=true)
{
    $company=(int)$context['scope']['company_id'];$legal=(int)$context['scope']['legal_entity_id'];
    if (!pay_glb_001_native_producer_hold_key_lock($company,$error)) return false;
    $count=hrm_pay_core_004_read_one('SELECT COUNT(*) n FROM '.TB_PREF.'domain_audit_events WHERE company_id='.$company.' AND legal_entity_id IN (0,'.$legal.')');
    if (!$count || (int)$count['n']>65536) { $error='native_selection_audit_verification_bound';return false; }
    $chain=$verify_chain?verify_domain_audit_chain($company,$legal):array('ok'=>true);
    $manifest=domain_audit_keyring_manifest($company);
    if (empty($chain['ok']) || !is_array($manifest) || count($manifest['keys'])>256) { $error='native_selection_protected_chain_invalid';return false; }
    $hashes=array();
    foreach ($manifest['keys'] as $entry) {
        $key=domain_audit_key_by_id($company,$entry['key_id']);
        $hash=$key===false?false:domain_audit_sensitive_hash(array('request_sha256'=>$context['request_sha256'],'effect_sha256'=>$effect_sha),$key,'entity-key-v1');
        if ($hash===false) { $error='native_selection_historical_audit_key_missing';return false; }
        $hashes[]=db_escape($hash);
    }
    $row=hrm_pay_core_004_read_one('SELECT event_id,event_hash FROM '.TB_PREF.'domain_audit_events WHERE company_id='.$company.' AND legal_entity_id='.$legal
        .' AND entity_type=\'pay_glb_native_execution\' AND action='.db_escape('pay_glb.native.'.$action).' AND actor_user_id='.(int)$actor
        .' AND entity_key_hash IN ('.implode(',',$hashes).') LIMIT 2');
    if (!$row) { $error='native_selection_causal_audit_required';return false; }
    return array('event_id'=>(string)$row['event_id'],'event_hash'=>$row['event_hash']);
}

/** Lock and verify the complete bounded append-only selection chain for the SQL scope. */
function pay_glb_001_native_selection_latest(array $scope,&$error=null)
{
    $period=hrm_pay_core_004_read_one('SELECT period_id FROM '.TB_PREF.'payroll_periods WHERE period_id='.(int)$scope['payroll_period_id'].' LIMIT 2 FOR UPDATE');
    if (!$period) { $error='native_selection_scope_lock_failed';return false; }
    $where='company_id='.(int)$scope['company_id'].' AND legal_entity_id='.(int)$scope['legal_entity_id']
        .' AND payroll_group_id='.(int)$scope['payroll_group_id'].' AND payroll_period_id='.(int)$scope['payroll_period_id'];
    $q=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_selection_events WHERE '.$where.' ORDER BY sequence_no LIMIT 4097 FOR UPDATE');
    if (!$q || db_num_rows($q)>4096) { $error='native_selection_chain_unavailable_or_bound';return false; }
    $previous=null;$sequence=0;
    while ($row=db_fetch_assoc($q)) {
        $d=pay_glb_001_native_receipt_decode_v1('selection',$row['receipt_json'],$error);
        if ($d===false || $d['sequence_no']!==(string)(++$sequence)
            || $d['predecessor_selection_sha256']!==($previous===null?null:$previous['selection_sha256'])
            || ($row['predecessor_selection_event_id']===null?null:(string)$row['predecessor_selection_event_id'])!==($previous===null?null:(string)$previous['selection_event_id'])
            || pay_glb_001_native_receipt_sha256_v1('selection',$d,$error)!==$row['selection_sha256']) { $error='native_selection_chain_integrity_invalid';return false; }
        foreach ($d as $field=>$value) {
            if ($field==='native_execution_sha256') continue;
            if ($value!==($row[$field]===null?null:($field==='occurred_at_utc'?str_replace(' ','T',$row[$field]).'Z':(string)$row[$field]))) {
                $error='native_selection_chain_column_invalid';return false;
            }
        }
        if ($d['event_kind']==='select') {
            $execution=hrm_pay_core_004_read_one('SELECT execution_sha256 FROM '.TB_PREF.'hrm_pay_glb_001_native_executions WHERE native_execution_id='.(int)$row['native_execution_id'].' LIMIT 2');
            if (!$execution || $execution['execution_sha256']!==$d['native_execution_sha256']) { $error='native_selection_chain_execution_invalid';return false; }
        } elseif ($row['native_execution_id']!==null || $d['native_execution_sha256']!==null) { $error='native_selection_negative_execution_invalid';return false; }
        $command=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_command_receipts WHERE company_id='.(int)$scope['company_id']
            .' AND command_domain=\'native.selection_check\' AND effect_kind=\'native_selection\' AND effect_id='.(int)$row['selection_event_id'].' LIMIT 2 FOR UPDATE');
        $cd=$command?pay_glb_001_native_receipt_decode_v1('command',$command['receipt_json'],$error):false;
        if ($cd===false || $cd['company_id']!==$d['company_id'] || $cd['actor_id']!==$d['checker_id']
            || $cd['command_domain']!=='native.selection_check' || $cd['effect_kind']!=='native_selection'
            || $cd['effect_id']!==(string)$row['selection_event_id'] || $cd['effect_sha256']!==$row['selection_sha256']
            || $cd['command_key_sha256']!==$d['command_key_sha256'] || $cd['expected_predecessor_sha256']!==$d['predecessor_selection_sha256']
            || $cd['purpose_code']!=='PAY_GLB_001_NATIVE_SELECT'
            || pay_glb_001_native_receipt_sha256_v1('command',$cd,$error)!==$command['command_sha256']) { $error='native_selection_chain_command_invalid';return false; }
        foreach ($cd as $field=>$value) if ($value!==($command[$field]===null?null:(string)$command[$field])) { $error='native_selection_chain_command_column_invalid';return false; }
        // The key mutex spans the transaction, so verify the protected chain
        // once, then establish every prior event's exact causal HMAC binding.
        if (pay_glb_001_native_selection_audit_proof(array('scope'=>$scope,'request_sha256'=>$cd['request_sha256']),
            'selection_checked',$row['selection_sha256'],(int)$d['checker_id'],$error,$sequence===1)===false) return false;
        $row['descriptor']=$d;$previous=$row;
    }
    return $previous;
}

/** Build one exact immutable initial-selection or negative-event maker request. */
function pay_glb_001_native_selection_request($operation_id,$kind,$expected,$key,$reason,$checker,&$error=null)
{
    if (!pay_glb_001_native_field_valid('selection','event_kind',$kind) || !pay_glb_001_native_field_valid('selection','reason_code',$reason)
        || !pay_glb_001_native_field_valid('command','expected_predecessor_sha256',$expected) || !pay_glb_001_sha256($key)) {
        $error='native_selection_request_invalid';return false;
    }
    $op=pay_glb_001_native_selection_operation($operation_id,$kind==='select',$checker,$error);if ($op===false) return false;
    $c=$op['context'];$request=array('schema'=>'PAY_GLB_001_NATIVE_SELECTION_REQUEST_V1','operation_id'=>$operation_id,'scope'=>$c['scope'],
        'scope_sha256'=>pay_glb_001_native_producer_scope_hash($c),'maker_id'=>$op['descriptor']['maker_id'],
        'execution_id'=>(string)$op['execution']['native_execution_id'],'execution_sha256'=>$op['execution']['execution_sha256'],
        'population_sha256'=>$c['population_sha256'],'source_manifest_sha256'=>$c['source_manifest_sha256'],
        'expected_predecessor_sha256'=>$expected,'event_kind'=>$kind,'reason_code'=>$reason,'command_key_sha256'=>$key);
    return array('request'=>$request,'operation'=>$op,'request_sha256'=>pay_glb_001_native_producer_hash('selection-request',$request));
}

/** Append an actual maker proposal; this cannot create a selection or business approval. */
function pay_glb_001_native_selection_propose($operation_id,$kind,$expected,$key,$reason,&$error=null)
{
    global $db_connections;
    $p=pay_glb_001_native_producer_boundary($error);
    if ($p===false || !isset($db_connections[$p['company_id']]['pay_glb_001_native_selection']['enabled'])
        || $db_connections[$p['company_id']]['pay_glb_001_native_selection']['enabled']!==true) return false;
    if (!pay_glb_001_native_selection_begin($error)) return false;
    $data=pay_glb_001_native_selection_request($operation_id,$kind,$expected,$key,$reason,false,$error);
    if ($data===false || $data['request']['maker_id']!==(string)$p['actor_user_id']
        || pay_glb_001_native_producer_admit($data['operation']['context'],$error)===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    $request=$data['request'];$latest=pay_glb_001_native_selection_latest($request['scope'],$error);
    if ($kind!=='select' && (!user_check_access('SA_HRM_RECOVER_PAY_CORE_RESULT')
        || !pay_glb_001_native_execution_lock_principal($p,'native_recover'))) { pay_glb_001_native_producer_finish(false,$error);$error='native_selection_fresh_recovery_permission_required';return false; }
    if ($latest===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    $context=$data['operation']['context'];$context['request_sha256']=$data['request_sha256'];
    $existing=pay_glb_001_native_producer_command('native.selection_propose',$request['scope_sha256'],$key,$error);
    if ($existing!==null) {
        $d=is_array($existing)?pay_glb_001_native_producer_command_valid($existing,$data['request_sha256'],$p['actor_user_id'],$error):false;
        if ($d===false || $d['effect_kind']!=='native_selection_proposal' || $d['effect_id']!==$request['execution_id']
            || $d['effect_sha256']!==$data['request_sha256'] || $d['expected_predecessor_sha256']!==$expected
            || hrm_pay_core_004_canonical_json(json_decode($existing['request_context_json'],true))!==hrm_pay_core_004_canonical_json($request)) {
            pay_glb_001_native_producer_finish(false,$error);$error='native_selection_proposal_replay_mismatch';return false;
        }
        if (!pay_glb_001_native_producer_finish(true,$error)) return false;return (string)$existing['command_receipt_id'];
    }
    if ($expected!==($latest===null?null:$latest['selection_sha256']) || ($kind!=='select' && $latest===null)
        || ($kind==='select' && $latest!==null && (!function_exists('pay_glb_001_native_replacement_selection_allowed')
            || !pay_glb_001_native_replacement_selection_allowed($data['operation'],$latest,$error)))) { pay_glb_001_native_producer_finish(false,$error);$error='native_selection_stale_predecessor_or_correction_protocol_required';return false; }
    if (pay_glb_001_native_selection_audit_proof($data['operation']['context'],'execution_sealed',$request['execution_sha256'],$p['actor_user_id'],$error)===false) {
        pay_glb_001_native_producer_finish(false,$error);return false;
    }
    $id=pay_glb_001_native_producer_record_command('native.selection_propose',$context,$key,'native_selection_proposal',$request['execution_id'],
        $data['request_sha256'],$request,$error,$expected);
    if ($id===false || pay_glb_001_native_producer_audit($context,'selection_proposed',$data['request_sha256'],$error)===false
        || pay_glb_001_native_producer_admit($data['operation']['context'],$error)===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    if (!pay_glb_001_native_producer_finish(true,$error)) return false;return $id;
}

/** Insert only typed selection/checker-command custody under current checker authority. */
function pay_glb_001_native_selection_insert($table,array $values,&$error=null)
{
    if (!in_array($table,array('hrm_pay_glb_001_native_selection_events','hrm_pay_glb_001_native_command_receipts'),true)
        || empty($GLOBALS['transaction_level']) || pay_glb_001_native_selection_principal()===false) { $error='native_selection_insert_denied';return false; }
    $columns=array();foreach(pay_glb_001_native_schema_definitions() as $m) if($m['name']===$table)
        foreach($m['columns'] as $column) if($column['extra']!=='auto_increment' && $column['name']!=='created_at')$columns[]=$column['name'];
    foreach($values as $column=>$value) if(!in_array($column,$columns,true)){ $error='native_selection_insert_column_invalid';return false; }
    $keys=array();$literals=array();foreach($values as $column=>$value){$keys[]='`'.$column.'`';$literal=pay_glb_001_native_custody_sql_value($column,$value,$error);if($literal===false)return false;$literals[]=$literal;}
    if (!db_query('INSERT INTO '.TB_PREF.$table.' ('.implode(',',$keys).') VALUES ('.implode(',',$literals).')')) { $error='native_selection_insert_failed';return false; }
    return (string)db_insert_id();
}

/** Execute a fresh independently admitted proposal with linear CAS and atomic custody/audit. */
function pay_glb_001_native_selection_check($proposal_id,&$error=null)
{
    $error=null;$p=pay_glb_001_native_selection_principal();
    if ($p===false || !empty($GLOBALS['transaction_level']) || pay_glb_001_native_helper_id($proposal_id,$error)===false) { $error='native_selection_top_level_checker_required';return false; }
    $state=new PayGlb001NativeProducerSchemaState();
    if (!pay_glb_001_native_require_schema($state,false)) { $error='native_selection_schema_invalid';return false; }
    if (!pay_glb_001_native_selection_begin($error)) return false;
    if (pay_glb_001_native_selection_principal()===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    $proposal=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_command_receipts WHERE company_id='.(int)$p['company_id']
        .' AND command_receipt_id='.(int)$proposal_id.' AND command_domain=\'native.selection_propose\' LIMIT 2 FOR UPDATE');
    $request=$proposal?json_decode($proposal['request_context_json'],true):false;
    if (!is_array($request) || !isset($request['operation_id'],$request['event_kind'],$request['command_key_sha256'],$request['reason_code'],$request['maker_id'])
        || !array_key_exists('expected_predecessor_sha256',$request) || $request['maker_id']===(string)$p['actor_user_id']) {
        pay_glb_001_native_producer_finish(false,$error);$error='native_selection_independent_proposal_required';return false;
    }
    $data=pay_glb_001_native_selection_request($request['operation_id'],$request['event_kind'],$request['expected_predecessor_sha256'],
        $request['command_key_sha256'],$request['reason_code'],true,$error);
    $d=$data===false?false:pay_glb_001_native_producer_command_valid($proposal,$data['request_sha256'],(int)$request['maker_id'],$error);
    $admission=$data===false?false:pay_glb_001_native_selection_admission($data['request'],$proposal['command_sha256'],$error);
    if ($d===false || $admission===false || $d['effect_kind']!=='native_selection_proposal' || $d['effect_id']!==$data['request']['execution_id']
        || $d['effect_sha256']!==$data['request_sha256'] || $d['expected_predecessor_sha256']!==$request['expected_predecessor_sha256']
        || hrm_pay_core_004_canonical_json($request)!==hrm_pay_core_004_canonical_json($data['request'])) {
        pay_glb_001_native_producer_finish(false,$error);return false;
    }
    $latest=pay_glb_001_native_selection_latest($request['scope'],$error);
    $q=$latest===false?false:db_query('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_selection_events WHERE company_id='.(int)$p['company_id']
        .' AND legal_entity_id='.(int)$request['scope']['legal_entity_id'].' AND payroll_group_id='.(int)$request['scope']['payroll_group_id']
        .' AND payroll_period_id='.(int)$request['scope']['payroll_period_id'].' AND command_key_sha256='.db_escape($request['command_key_sha256']).' LIMIT 2 FOR UPDATE');
    if (!$q || db_num_rows($q)>1) { pay_glb_001_native_producer_finish(false,$error);$error='native_selection_replay_lookup_failed';return false; }
    $existing=db_num_rows($q)===1?db_fetch_assoc($q):null;
    $context=$data['operation']['context'];$context['request_sha256']=$data['request_sha256'];
    if (pay_glb_001_native_selection_audit_proof($context,'selection_proposed',$data['request_sha256'],(int)$request['maker_id'],$error)===false) {
        pay_glb_001_native_producer_finish(false,$error);return false;
    }
    if ($existing!==null) {
        $receipt=pay_glb_001_native_receipt_decode_v1('selection',$existing['receipt_json'],$error);
        $command=pay_glb_001_native_producer_command('native.selection_check',$request['scope_sha256'],$request['command_key_sha256'],$error);
        $cd=is_array($command)?pay_glb_001_native_receipt_decode_v1('command',$command['receipt_json'],$error):false;
        if ($receipt===false || $cd===false || $receipt['checker_id']!==(string)$p['actor_user_id']
            || $cd['actor_id']!==(string)$p['actor_user_id'] || $cd['purpose_code']!=='PAY_GLB_001_NATIVE_SELECT'
            || $cd['request_sha256']!==$data['request_sha256'] || $cd['effect_id']!==(string)$existing['selection_event_id']
            || $cd['command_domain']!=='native.selection_check' || $cd['scope_sha256']!==$request['scope_sha256']
            || $cd['command_key_sha256']!==$request['command_key_sha256'] || $cd['effect_kind']!=='native_selection'
            || $cd['expected_predecessor_sha256']!==$request['expected_predecessor_sha256']
            || $cd['effect_sha256']!==$existing['selection_sha256']
            || pay_glb_001_native_receipt_sha256_v1('command',$cd,$error)!==$command['command_sha256']
            || pay_glb_001_native_selection_audit_proof($context,'selection_checked',$existing['selection_sha256'],$p['actor_user_id'],$error)===false) {
            pay_glb_001_native_producer_finish(false,$error);if ($error===null) $error='native_selection_checker_replay_mismatch';return false;
        }
        foreach ($cd as $field=>$value) if ($value!==($command[$field]===null?null:(string)$command[$field])) {
            pay_glb_001_native_producer_finish(false,$error);$error='native_selection_checker_command_column_invalid';return false;
        }
        $stored=json_decode($command['request_context_json'],true);
        if (!is_array($stored) || !isset($stored['proposal_id'],$stored['proposal_sha256'],$stored['admission'])
            || $stored['proposal_id']!==$proposal_id || $stored['proposal_sha256']!==$proposal['command_sha256']) {
            pay_glb_001_native_producer_finish(false,$error);$error='native_selection_checker_command_context_invalid';return false;
        }
        if (pay_glb_001_native_selection_admission($data['request'],$proposal['command_sha256'],$error)===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
        if (!pay_glb_001_native_producer_finish(true,$error)) return false;return (string)$existing['selection_event_id'];
    }
    if ($request['event_kind']==='select' && $latest!==null && !pay_glb_001_native_finance_obligations_clear($context['scope'],null,$error)) {
        pay_glb_001_native_producer_finish(false,$error);return false;
    }
    if ($request['expected_predecessor_sha256']!==($latest===null?null:$latest['selection_sha256'])
        || ($request['event_kind']==='select' && $latest!==null && (!function_exists('pay_glb_001_native_replacement_selection_allowed')
            || !pay_glb_001_native_replacement_selection_allowed($data['operation'],$latest,$error))) || ($request['event_kind']!=='select' && $latest===null)) {
        pay_glb_001_native_producer_finish(false,$error);$error='native_selection_cas_conflict_or_correction_required';return false;
    }
    $receipt=array();$fields=pay_glb_001_native_receipt_fields();foreach(array_slice($fields['selection'],0,6) as $field)$receipt[$field]=$request['scope'][$field];
    $receipt+=array('sequence_no'=>$latest===null?'1':(string)((int)$latest['sequence_no']+1),'predecessor_selection_sha256'=>$request['expected_predecessor_sha256'],
        'event_kind'=>$request['event_kind'],'native_execution_sha256'=>$request['event_kind']==='select'?$request['execution_sha256']:null,
        'reason_code'=>$request['reason_code'],'maker_id'=>$request['maker_id'],'checker_id'=>(string)$p['actor_user_id'],
        'command_key_sha256'=>$request['command_key_sha256'],'occurred_at_utc'=>gmdate('Y-m-d\TH:i:s\Z'));
    $sha=pay_glb_001_native_receipt_sha256_v1('selection',$receipt,$error);$values=$receipt;unset($values['native_execution_sha256']);
    $values['native_execution_id']=$request['event_kind']==='select'?$request['execution_id']:null;
    $values['predecessor_selection_event_id']=$latest===null?null:(string)$latest['selection_event_id'];
    $values['selection_sha256']=$sha;$values['receipt_json']=pay_glb_001_native_receipt_canonical_v1('selection',$receipt,$error);
    $values['occurred_at_utc']=str_replace(array('T','Z'),array(' ',''),$receipt['occurred_at_utc']);
    $id=$sha===false?false:pay_glb_001_native_selection_insert('hrm_pay_glb_001_native_selection_events',$values,$error);
    $cmd=array('company_id'=>(string)$p['company_id'],'command_domain'=>'native.selection_check','scope_sha256'=>$request['scope_sha256'],
        'command_key_sha256'=>$request['command_key_sha256'],'expected_predecessor_sha256'=>$request['expected_predecessor_sha256'],
        'request_sha256'=>$data['request_sha256'],'effect_kind'=>'native_selection','effect_id'=>$id,'effect_sha256'=>$sha,
        'actor_id'=>(string)$p['actor_user_id'],'purpose_code'=>'PAY_GLB_001_NATIVE_SELECT');
    $cv=$cmd;$cv['command_sha256']=pay_glb_001_native_receipt_sha256_v1('command',$cmd,$error);
    $cv['receipt_json']=pay_glb_001_native_receipt_canonical_v1('command',$cmd,$error);
    $cv['request_context_json']=hrm_pay_core_004_canonical_json(array('proposal_id'=>$proposal_id,'proposal_sha256'=>$proposal['command_sha256'],'admission'=>$admission));
    if ($id===false || $cv['command_sha256']===false || pay_glb_001_native_selection_insert('hrm_pay_glb_001_native_command_receipts',$cv,$error)===false
        || pay_glb_001_native_producer_audit($context,'selection_checked',$sha,$error)===false
        || pay_glb_001_native_selection_admission($data['request'],$proposal['command_sha256'],$error)===false) {
        pay_glb_001_native_producer_finish(false,$error);return false;
    }
    if (!pay_glb_001_native_producer_finish(true,$error)) return false;return $id;
}
