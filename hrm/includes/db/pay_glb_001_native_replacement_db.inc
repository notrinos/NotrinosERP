<?php
/** Full replacement native production with actual approved CORE-007 lineage; default-disabled. */
require_once dirname(__FILE__).'/pay_glb_001_native_selection_db.inc';
require_once dirname(__FILE__).'/pay_core_007_governance_db.inc';
require_once dirname(dirname(__FILE__)).'/payroll/pay_glb_001_native_retro_storage_adapter.inc';
require_once dirname(dirname(dirname(dirname(__FILE__)))).'/includes/pay_glb_001_native_correction_admission.inc';
require_once dirname(__FILE__).'/pay_glb_001_native_obligations_db.inc';

/** Versioned producer identity; all members are newly produced, never adopted. */
function pay_glb_001_native_replacement_version(){return 'PAY_GLB_001_STAGED_REPLACEMENT_V1';}

/** Authenticate actual calculation/write/seal/retro execution and strict correction flag. */
function pay_glb_001_native_replacement_principal()
{
    global $db_connections;
    $p=pay_glb_001_current_principal('native_replace');
    if($p===false || !isset($db_connections[$p['company_id']]['pay_glb_001_native_execution']['enabled'],$db_connections[$p['company_id']]['pay_glb_001_native_corrections']['enabled'])
        || $db_connections[$p['company_id']]['pay_glb_001_native_execution']['enabled']!==true || $db_connections[$p['company_id']]['pay_glb_001_native_corrections']['enabled']!==true)return false;
    if(!empty($GLOBALS['transaction_level']) && !pay_glb_001_native_execution_lock_principal($p,'native_replace'))return false;return $p;
}

/** Verify immutable native and existing CORE-007 custody plus exact storage precision before authority use. */
function pay_glb_001_native_replacement_require_schema(&$error=null)
{
    $state=new PayGlb001NativeProducerSchemaState();if(!pay_glb_001_native_require_schema($state,false)){$error='native_replacement_native_schema_invalid';return false;}
    $sets=array_merge(hrm_pay_core_007_event_columns(),hrm_pay_core_007_context_columns(),hrm_pay_core_007_delta_columns(),hrm_pay_core_007_plan_columns(),hrm_pay_core_007_execution_columns());
    $codes=array('hrm_pay_retro_events'=>'evt','hrm_pay_retro_affected_results'=>'aff','hrm_pay_retro_contexts'=>'ctx','hrm_pay_retro_deltas'=>'del',
        'hrm_pay_retro_plans'=>'pln','hrm_pay_retro_approvals'=>'app','hrm_pay_retro_executions'=>'exe');
    foreach($sets as $table=>$columns){if(hrm_pay_core_007_schema_columns($table)!==$columns || !hrm_pay_core_007_schema_engine_innodb($table)){$error='native_replacement_retro_schema_invalid';return false;}
        foreach(array('UPDATE','DELETE') as $op)if(!hrm_pay_core_007_trigger_exists($table,$codes[$table],$op)){$error='native_replacement_retro_immutability_missing';return false;}}
    foreach(array('hrm_pay_core_004_results'=>array('gross_amount','deduction_amount','net_amount'),
        'hrm_pay_core_004_result_lines'=>array('amount','quantity','rate'),'hrm_pay_retro_deltas'=>array('original_amount','recomputed_amount','delta_amount')) as $table=>$columns){
        $physical=hrm_migration_physical_table_name($table);if($physical===false){$error='native_replacement_storage_scope_invalid';return false;}
        foreach($columns as $column){$row=hrm_pay_core_004_read_one('SELECT COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($physical).' AND COLUMN_NAME='.db_escape($column));
            if(!$row || strtolower($row['COLUMN_TYPE'])!==($table==='hrm_pay_retro_deltas'?'decimal(30,8)':'decimal(24,8)')){$error='native_replacement_storage_precision_drift';return false;}}}
    return true;
}

/** Verify a genuinely sealed historical predecessor, retaining its old proof rather than claiming currentness. */
function pay_glb_001_native_replacement_predecessor($selection_id,$purpose,&$error=null)
{
    if(pay_glb_001_native_helper_id($selection_id,$error)===false)return false;
    $selection=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_selection_events WHERE company_id='.(int)user_company()
        .' AND selection_event_id='.(int)$selection_id.' LIMIT 2 FOR UPDATE');
    if(!$selection || $selection['event_kind']!=='select'){$error='native_replacement_selected_predecessor_required';return false;}
    $scope=array();foreach(array('company_id','legal_entity_id','payroll_group_id','payroll_period_id') as $field)$scope[$field]=(string)$selection[$field];
    $latest=pay_glb_001_native_selection_latest($scope,$error);if($latest===false)return false;
    $seal=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_command_receipts WHERE company_id='.(int)user_company()
        .' AND command_domain=\'native.seal\' AND effect_kind=\'native_execution\' AND effect_id='.(int)$selection['native_execution_id'].' LIMIT 2 FOR UPDATE');
    $event=$seal?json_decode($seal['request_context_json'],true):false;
    if(!is_array($event) || !isset($event['operation_id'],$event['event']['integrity_proofs'])){$error='native_replacement_terminal_source_required';return false;}
    $op=pay_glb_001_native_selection_operation($event['operation_id'],false,false,$error);if($op===false)return false;
    $owner=(int)$op['start']['actor_id'];$operation=array('id'=>$event['operation_id'],'row'=>$op['start'],'context'=>$op['context']);
    $material=pay_glb_001_native_producer_complete_material($operation,$error,$owner,$event['event']['integrity_proofs'],$purpose);
    if($material===false || pay_glb_001_native_producer_sealed_replay($operation,$material,$seal,$error,$owner,$purpose)!==(string)$selection['native_execution_id'])return false;
    if(pay_glb_001_native_selection_audit_proof($op['context'],'execution_sealed',$op['execution']['execution_sha256'],$owner,$error)===false)return false;
    return array('selection'=>$selection,'latest'=>$latest,'operation'=>$operation,'execution'=>$op['execution'],'descriptor'=>$op['descriptor'],'material'=>$material);
}

/** Bind an actual immutable context/plan to the selected original and independently sealed new snapshot. */
function pay_glb_001_native_retro_context_source($context_id,array $plan,array $predecessor,$actor,&$error=null)
{
    if(pay_glb_001_native_helper_id($context_id,$error)===false)return false;
    $row=hrm_pay_core_007_one('SELECT * FROM '.TB_PREF.'hrm_pay_retro_contexts WHERE context_id='.(int)$context_id.' AND OCTET_LENGTH(context_payload_json)<=1048576 LIMIT 2 FOR UPDATE');
    $context=$row?hrm_pay_core_007_context_from_row($row,$error):false;
    if($context===false || pay_glb_001_native_retro_context($context,$plan,$error)===false){$error='native_retro_actual_context_required';return false;}
    $original=null;foreach($predecessor['material']['members'] as $member)if($member['core004_result_id']===(string)$context['original_result_id'])$original=$member;
    if($original===null || $original['result_sha256']!==$context['original_result_sha256'] || $original['snapshot_sha256']!==$context['original_snapshot_sha256']){
        $error='native_retro_selected_original_required';return false;
    }
    $affected=hrm_pay_core_007_one('SELECT * FROM '.TB_PREF.'hrm_pay_retro_affected_results WHERE affected_result_id='.(int)$context['affected_result_id'].' LIMIT 2 FOR UPDATE');
    $event=$affected?hrm_pay_core_007_one('SELECT * FROM '.TB_PREF.'hrm_pay_retro_events WHERE retro_event_id='.(int)$affected['retro_event_id'].' LIMIT 2 FOR UPDATE'):false;
    $parsed=$event?hrm_pay_core_007_event_from_row($event,$error):false;
    if(!$affected || $parsed===false || hrm_pay_core_007_assert_object_scope($parsed['scope'],$error)===false
        || (string)$affected['original_result_id']!==$original['core004_result_id'] || $affected['original_result_sha256']!==$original['result_sha256']
        || $affected['original_snapshot_sha256']!==$original['snapshot_sha256'] || (string)$event['payroll_relationship_id']!==$original['payroll_relationship_id']){
        $error='native_retro_affected_source_invalid';return false;
    }
    $snap=hrm_pay_core_004_read_one('SELECT snapshot_id,sealed_by FROM '.TB_PREF.'hrm_pay_core_004_snapshots WHERE snapshot_sha256='.db_escape($plan['snapshot_sha256']).' LIMIT 2 FOR UPDATE');
    if(!$snap || (int)$snap['sealed_by']===$actor){$error='native_retro_independently_sealed_recompute_required';return false;}
    $scope=$plan['snapshot_material']['descriptor'];
    foreach(array('payroll_relationship_id','assignment_id','calendar_period_id','legal_entity_id','currency') as $field){
        $expected=$field==='assignment_id'?$original['assignment_id']:($field==='payroll_relationship_id'?$original[$field]:($field==='currency'?$predecessor['descriptor']['currency_code']:$predecessor['descriptor'][$field]));
        if(($scope[$field]===null?null:(string)$scope[$field])!==$expected){$error='native_retro_recompute_scope_invalid';return false;}
    }
    $q=db_query('SELECT line_no,element_version_id,line_kind,currency,amount,quantity,rate,source_sha256 FROM '.TB_PREF.'hrm_pay_core_004_result_lines WHERE result_id='.(int)$context['original_result_id'].' ORDER BY line_no LIMIT 4097');
    if(!$q || db_num_rows($q)>4096){$error='native_retro_original_lines_unavailable';return false;}$lines=array();while($line=db_fetch_assoc($q))$lines[]=$line;
    $d=pay_glb_001_native_retro_compute_deltas_v1($context,$plan,$lines,$error);if($d===false)return false;
    return array('context_id'=>$context_id,'context'=>$context,'snapshot_id'=>(string)$snap['snapshot_id'],'original'=>$original,'delta'=>$d,
        'affected'=>$affected,'event'=>$parsed);
}

/** Prepare an exact source-admitted forward delta request before an actual CORE-007 plan is approved. */
function pay_glb_001_native_retro_delta_prepare($context_id,array $plan,$selection_id,$key,&$error=null)
{
    $p=pay_glb_001_native_replacement_principal();if($p===false || !pay_glb_001_sha256($key)){$error='native_retro_delta_principal_denied';return false;}
    if(!pay_glb_001_native_replacement_require_schema($error))return false;
    $manage=empty($GLOBALS['transaction_level']);if($manage && !pay_glb_001_native_selection_begin($error))return false;
    $pred=pay_glb_001_native_replacement_predecessor($selection_id,'native_replace',$error);
    $source=$pred===false?false:pay_glb_001_native_retro_context_source($context_id,$plan,$pred,$p['actor_user_id'],$error);
    if($source===false){if($manage)pay_glb_001_native_producer_finish(false,$error);return false;}
    if((string)$pred['latest']['selection_event_id']!==$selection_id){if($manage)pay_glb_001_native_producer_finish(false,$error);$error='native_retro_delta_stale_selection';return false;}
    $c=$pred['operation']['context'];
    $request=array('schema'=>'PAY_GLB_001_NATIVE_RETRO_DELTA_V1','scope'=>$c['scope'],'population_sha256'=>$c['population_sha256'],
        'source_manifest_sha256'=>pay_glb_001_native_producer_hash('retro-delta-source',array('context_sha256'=>$source['context']['context_sha256'],
            'plan_sha256'=>$source['delta']['storage_binding']['plan_sha256'],'predecessor_execution_sha256'=>$pred['execution']['execution_sha256'],
            'storage_binding_sha256'=>$source['delta']['storage_binding_sha256'])),
        'context_id'=>$context_id,'selection_id'=>$selection_id,'predecessor_execution_sha256'=>$pred['execution']['execution_sha256'],'command_key_sha256'=>$key);
    $request['request_sha256']=pay_glb_001_native_producer_hash('request',$request);
    if($manage && !pay_glb_001_native_producer_finish(true,$error))return false;return array('request'=>$request,'source'=>$source);
}

/** Write actual old-domain deltas from lossless storage inputs, with atomic typed custody and protected audit. */
function pay_glb_001_native_retro_record_deltas($context_id,array $plan,$selection_id,$key,&$error=null)
{
    if(!empty($GLOBALS['transaction_level']) || pay_glb_001_native_replacement_principal()===false){$error='native_retro_delta_top_level_required';return false;}
    if(!pay_glb_001_native_selection_begin($error))return false;
    $prepared=pay_glb_001_native_retro_delta_prepare($context_id,$plan,$selection_id,$key,$error);
    if($prepared===false || pay_glb_001_native_producer_admit($prepared['request'],$error)===false){pay_glb_001_native_producer_finish(false,$error);return false;}
    $request=$prepared['request'];$source=$prepared['source'];$d=$source['delta']['core007_deltas'];$actor=hrm_pay_core_007_actor();
    $owned=pay_glb_001_native_producer_command('native.retro_delta',pay_glb_001_native_producer_scope_hash($request),$key,$error);
    if($owned===false){pay_glb_001_native_producer_finish(false,$error);return false;}
    $count=hrm_pay_core_004_read_one('SELECT COUNT(*) n FROM '.TB_PREF.'hrm_pay_retro_deltas WHERE context_id='.(int)$context_id.' FOR UPDATE');
    if(!$count || ($owned===null && (int)$count['n']!==0) || ($owned!==null && (int)$count['n']!==count($d['rows']))){pay_glb_001_native_producer_finish(false,$error);$error='native_retro_unowned_or_incomplete_deltas_denied';return false;}
    foreach($d['rows'] as $line){
        $existing=hrm_pay_core_007_one('SELECT * FROM '.TB_PREF.'hrm_pay_retro_deltas WHERE context_id='.(int)$context_id.' AND delta_key='.db_escape($line['delta_key']).' LIMIT 2 FOR UPDATE');
        if($existing){
            if($existing['delta_sha256']!==$line['delta_sha256'] || (int)$existing['computed_by']!==$actor){pay_glb_001_native_producer_finish(false,$error);$error='native_retro_delta_conflict';return false;}
            foreach(array('original_amount','recomputed_amount','delta_amount') as $field)if(pay_glb_001_native_retro_decimal_6($existing[$field],$error,true)!==$line[$field]){pay_glb_001_native_producer_finish(false,$error);return false;}
            continue;
        }
        $values=array('affected_result_id'=>(string)$source['context']['affected_result_id'],'context_id'=>$context_id,'delta_key'=>$line['delta_key'],
            'element_version_id'=>(string)$line['element_version_id'],'line_kind'=>$line['line_kind'],'currency'=>$line['currency'],
            'original_amount'=>$line['original_amount'],'recomputed_amount'=>$line['recomputed_amount'],'delta_amount'=>$line['delta_amount'],
            'delta_sha256'=>$line['delta_sha256'],'computed_by'=>(string)$actor);
        if(pay_glb_001_native_replacement_insert('hrm_pay_retro_deltas',$values,'execute',$error)===false){pay_glb_001_native_producer_finish(false,$error);return false;}
    }
    $old=pay_glb_001_native_producer_command('native.retro_delta',pay_glb_001_native_producer_scope_hash($request),$key,$error);
    if($old===false){pay_glb_001_native_producer_finish(false,$error);return false;}
    if($old!==null){$receipt=pay_glb_001_native_producer_command_valid($old,$request['request_sha256'],$actor,$error);
        if($receipt===false || $receipt['effect_sha256']!==$d['delta_set_sha256']){pay_glb_001_native_producer_finish(false,$error);return false;}
    }else{
        $id=pay_glb_001_native_producer_record_command('native.retro_delta',$request,$key,'core007_delta_set',$context_id,$d['delta_set_sha256'],
            array('context_id'=>$context_id,'selection_id'=>$selection_id,'storage_binding_sha256'=>$source['delta']['storage_binding_sha256']),$error);
        if($id===false || pay_glb_001_native_producer_audit($request,'retro_deltas_recorded',$d['delta_set_sha256'],$error)===false){pay_glb_001_native_producer_finish(false,$error);return false;}
    }
    if(pay_glb_001_native_producer_admit($request,$error)===false || pay_glb_001_native_replacement_principal()===false){pay_glb_001_native_producer_finish(false,$error);return false;}
    if(!pay_glb_001_native_producer_finish(true,$error))return false;return $source['delta'];
}

/** Insert only actual retro deltas or native phase commands with current purpose-specific authorization. */
function pay_glb_001_native_replacement_insert($table,array $values,$purpose,&$error=null)
{
    $p=$purpose==='execute'?pay_glb_001_native_replacement_principal():($purpose==='review'?pay_glb_001_native_correction_review_principal():false);
    if($p===false || empty($GLOBALS['transaction_level']) || !in_array($table,array('hrm_pay_retro_deltas','hrm_pay_glb_001_native_command_receipts'),true)){$error='native_replacement_insert_denied';return false;}
    $allowed=$table==='hrm_pay_retro_deltas'?hrm_pay_core_007_delta_columns()['hrm_pay_retro_deltas']:array();
    if($table!=='hrm_pay_retro_deltas')foreach(pay_glb_001_native_schema_definitions() as $model)if($model['name']===$table)foreach($model['columns'] as $column)$allowed[]=$column['name'];
    $keys=array();$literals=array();foreach($values as $column=>$value){if(!in_array($column,$allowed,true) || in_array($column,array('delta_id','command_receipt_id','created_at'),true)){$error='native_replacement_insert_column_invalid';return false;}$keys[]='`'.$column.'`';$literal=pay_glb_001_native_custody_sql_value($column,$value,$error);if($literal===false)return false;$literals[]=$literal;}
    if(!db_query('INSERT INTO '.TB_PREF.$table.' ('.implode(',',$keys).') VALUES ('.implode(',',$literals).')')){$error='native_replacement_insert_failed';return false;}return (string)db_insert_id();
}

/** Recompute the actual approved CORE-007 plan, approval, context, delta rows and causal delta custody. */
function pay_glb_001_native_replacement_approved_plan($plan_id,array $plan,array $predecessor,$executor,&$error=null)
{
    if(pay_glb_001_native_helper_id($plan_id,$error)===false)return false;
    $p=hrm_pay_core_007_one('SELECT * FROM '.TB_PREF.'hrm_pay_retro_plans WHERE retro_plan_id='.(int)$plan_id.' LIMIT 2 FOR UPDATE');
    $a=$p?hrm_pay_core_007_one('SELECT * FROM '.TB_PREF.'hrm_pay_retro_approvals WHERE retro_plan_id='.(int)$plan_id.' LIMIT 2 FOR UPDATE'):false;
    if(!$p || !$a || (int)$p['maker_id']===$executor || (int)$a['checker_id']===$executor){$error='native_retro_approved_separated_plan_required';return false;}
    $descriptor=hrm_pay_core_007_plan_from_row($p);$verified=hrm_pay_core_007_plan($descriptor,$error);
    $approval=$verified===false?false:hrm_pay_core_007_approval($verified,(int)$p['maker_id'],(int)$a['checker_id'],$a['approval_evidence_sha256'],$error);
    $source=pay_glb_001_native_retro_context_source((string)$p['context_id'],$plan,$predecessor,$executor,$error);
    if($verified===false || $approval===false || $source===false || $verified['plan_sha256']!==$p['plan_sha256']
        || $a['plan_sha256']!==$p['plan_sha256'] || $approval['approval_sha256']!==$a['approval_sha256']
        || (string)$p['affected_result_id']!==(string)$source['context']['affected_result_id']
        || $p['historical_plan_sha256']!==$source['delta']['storage_binding']['plan_sha256']
        || $p['delta_set_sha256']!==$source['delta']['core007_deltas']['delta_set_sha256']
        || $p['predecessor_result_sha256']!==$source['context']['original_result_sha256'] || $p['plan_type']!==$plan['scenario']
        || $p['bring_forward_date']!==null || ($p['target_calendar_period_id']!==null && (string)$p['target_calendar_period_id']!==$predecessor['descriptor']['calendar_period_id'])){
        $error='native_retro_approved_source_binding_invalid';return false;
    }
    $q=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_retro_deltas WHERE context_id='.(int)$p['context_id'].' ORDER BY delta_key LIMIT 4097 FOR UPDATE');
    $rows=$source['delta']['core007_deltas']['rows'];
    if(!$q || db_num_rows($q)!==count($rows)){$error='native_retro_complete_delta_rows_required';return false;}
    $computed_by=null;$index=0;
    while($row=db_fetch_assoc($q)){
        $expected=$rows[$index++];
        if($computed_by===null)$computed_by=(int)$row['computed_by'];
        if((int)$row['computed_by']!==$computed_by || (string)$row['affected_result_id']!==(string)$source['context']['affected_result_id']){$error='native_retro_delta_owner_invalid';return false;}
        foreach($expected as $field=>$value){$actual=$row[$field];if(in_array($field,array('original_amount','recomputed_amount','delta_amount'),true))$actual=pay_glb_001_native_retro_decimal_6($actual,$error,true);
            if((string)$value!==($actual===false?false:(string)$actual)){$error='native_retro_delta_row_identity_invalid';return false;}}
    }
    $command=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_command_receipts WHERE company_id='.(int)user_company()
        .' AND command_domain=\'native.retro_delta\' AND effect_kind=\'core007_delta_set\' AND effect_id='.(int)$p['context_id']
        .' AND effect_sha256='.db_escape($p['delta_set_sha256']).' LIMIT 2 FOR UPDATE');
    $cd=$command?pay_glb_001_native_producer_command_valid($command,$command['request_sha256'],$computed_by,$error):false;
    $data=$command?json_decode($command['request_context_json'],true):false;
    if($cd===false || !is_array($data) || !isset($data['context_id'],$data['selection_id'],$data['storage_binding_sha256'])
        || $data['context_id']!==(string)$p['context_id'] || $data['selection_id']!==(string)$predecessor['selection']['selection_event_id']
        || $data['storage_binding_sha256']!==$source['delta']['storage_binding_sha256']){$error='native_retro_delta_custody_required';return false;}
    $audit_context=array('scope'=>$predecessor['operation']['context']['scope'],'request_sha256'=>$command['request_sha256']);
    if(pay_glb_001_native_selection_audit_proof($audit_context,'retro_deltas_recorded',$p['delta_set_sha256'],$computed_by,$error)===false)return false;
    return array('retro_plan_id'=>$plan_id,'context_id'=>(string)$p['context_id'],'approval_id'=>(string)$a['approval_id'],
        'approved_plan_sha256'=>$p['plan_sha256'],'approval_sha256'=>$a['approval_sha256'],'approval_evidence_sha256'=>$a['approval_evidence_sha256'],
        'maker_id'=>(string)$p['maker_id'],'checker_id'=>(string)$a['checker_id'],'plan_type'=>$p['plan_type'],'reason_code'=>$p['reason_code'],
        'historical_context'=>$source['context'],'original_result_id'=>(string)$source['context']['original_result_id'],
        'original_result_sha256'=>$source['context']['original_result_sha256'],'delta_set_sha256'=>$p['delta_set_sha256'],
        'delta_command_receipt_id'=>(string)$command['command_receipt_id'],'delta_command_sha256'=>$command['command_sha256']);
}

/** Build a full replacement request with actual approved corrections and unchanged-member semantics. */
function pay_glb_001_native_replacement_prepare($period,$key,array $input,$selection_id,&$error=null,$owner=null,$purpose='native_replace')
{
    global $db_connections;
    $principal=$purpose==='native_replace'?pay_glb_001_native_replacement_principal():($purpose==='native_select'?pay_glb_001_native_selection_principal():
        ($purpose==='native_retro_review'?pay_glb_001_native_correction_review_principal():false));
    if($principal===false || !pay_glb_001_native_replacement_require_schema($error) || !isset($db_connections[$principal['company_id']]['pay_glb_001_native_corrections']['enabled'])
        || $db_connections[$principal['company_id']]['pay_glb_001_native_corrections']['enabled']!==true){$error='native_replacement_principal_denied';return false;}
    if($owner===null)$owner=$principal['actor_user_id'];
    if(!is_int($owner) || $owner<=0 || count($input)<1 || count($input)>4096){$error='native_replacement_owner_or_population_invalid';return false;}
    $manage=empty($GLOBALS['transaction_level']);if($manage && !pay_glb_001_native_selection_begin($error))return false;
    $result=pay_glb_001_native_replacement_build($period,$key,$input,$selection_id,$owner,$purpose,$error);
    if($manage && !pay_glb_001_native_producer_finish($result!==false,$error))return false;return $result;
}

/** Derive the immutable complete revision while allowing exact replay of its own terminal head only. */
function pay_glb_001_native_replacement_build($period,$key,array $input,$selection_id,$owner,$purpose,&$error=null)
{
    $base_input=array();$plans=array();
    foreach($input as $entry){if(!is_array($entry) || array_keys($entry)!==array('snapshot_id','plan','retro_plan_id')
        || ($entry['retro_plan_id']!==null && pay_glb_001_native_helper_id($entry['retro_plan_id'],$error)===false)){$error='native_replacement_source_entry_invalid';return false;}
        $base_input[]=array('snapshot_id'=>$entry['snapshot_id'],'plan'=>$entry['plan']);$plans[$entry['snapshot_id']]=$entry['retro_plan_id'];}
    $context=pay_glb_001_native_producer_source_context($period,$key,$base_input,$owner,$error,$purpose);if($context===false)return false;
    $pred=pay_glb_001_native_replacement_predecessor($selection_id,$purpose,$error);if($pred===false)return false;
    if(!pay_glb_001_native_finance_obligations_clear($pred['descriptor'],null,$error))return false;
    foreach(array('company_id','legal_entity_id','payroll_group_id','payroll_period_id','calendar_period_id','currency_code') as $field)
        if($context['scope'][$field]!==$pred['descriptor'][$field]){$error='native_replacement_scope_change_protocol_required';return false;}
    $old=array();foreach($pred['operation']['context']['sources'] as $source)$old[$source['payroll_relationship_id']]=$source;
    if(count($old)!==count($context['sources'])){$error='native_replacement_membership_change_protocol_required';return false;}
    $corrections=0;
    foreach($context['sources'] as &$source){
        if(!isset($old[$source['payroll_relationship_id']])){unset($source);$error='native_replacement_exact_relationship_set_required';return false;}
        $plan_id=$plans[$source['snapshot_id']];$previous=$old[$source['payroll_relationship_id']];
        if($source['snapshot_id']===$previous['snapshot_id']){unset($source);$error='native_replacement_new_snapshot_required';return false;}
        $source['retro']=null;
        if($plan_id!==null){
            $source['retro']=pay_glb_001_native_replacement_approved_plan($plan_id,$source['plan'],$pred,$owner,$error);
            if($source['retro']===false){unset($source);return false;}$corrections++;
        }else{
            $a=$source['plan'];$b=$previous['plan'];
            if(in_array($a['scenario'],array('correction','reversal'),true)){unset($source);$error='native_replacement_real_retro_plan_required';return false;}
            foreach(array(&$a,&$b) as &$plan){unset($plan['plan_sha256'],$plan['snapshot_sha256'],$plan['snapshot_material']['snapshot_sha256'],$plan['snapshot_material']['payload_json']);
                $plan['snapshot_material']['descriptor']['run_identity']='FULL_REPLACEMENT';}unset($plan);
            if(hrm_pay_core_004_canonical_json($a)!==hrm_pay_core_004_canonical_json($b)){unset($source);$error='native_replacement_unapproved_regular_change_denied';return false;}
        }
    }unset($source);
    if(!$corrections){$error='native_replacement_approved_correction_required';return false;}
    $revision=(int)$pred['descriptor']['revision_no'];if($revision>=PHP_INT_MAX){$error='native_replacement_revision_limit';return false;}
    $context['schema']='PAY_GLB_001_NATIVE_REPLACEMENT_OPERATION_V1';$context['producer_contract_version']=pay_glb_001_native_replacement_version();
    $context['replacement']=array('predecessor_selection_id'=>$selection_id,'predecessor_selection_sha256'=>$pred['selection']['selection_sha256'],
        'predecessor_execution_id'=>(string)$pred['execution']['native_execution_id'],'predecessor_execution_sha256'=>$pred['execution']['execution_sha256'],
        'revision_no'=>(string)($revision+1));
    $context['source_manifest_sha256']=pay_glb_001_native_producer_hash('source-manifest',$context['sources']);unset($context['request_sha256']);
    $json=hrm_pay_core_004_canonical_json($context);if($json===false || strlen($json)>16777216){$error='native_replacement_context_bound';return false;}
    $context['request_sha256']=pay_glb_001_native_producer_hash('request',$context);
    $head=hrm_pay_core_004_read_one('SELECT native_execution_id,revision_no,request_sha256,predecessor_execution_id FROM '.TB_PREF.'hrm_pay_glb_001_native_executions WHERE company_id='.(int)$context['scope']['company_id']
        .' AND legal_entity_id='.(int)$context['scope']['legal_entity_id'].' AND payroll_group_id='.(int)$context['scope']['payroll_group_id'].' AND payroll_period_id='.(int)$context['scope']['payroll_period_id'].' ORDER BY revision_no DESC LIMIT 1 FOR UPDATE');
    $own_head=$head && $head['request_sha256']===$context['request_sha256'] && (string)$head['revision_no']===$context['replacement']['revision_no']
        && (string)$head['predecessor_execution_id']===$context['replacement']['predecessor_execution_id'];
    if(!$head || (!$own_head && (string)$head['native_execution_id']!==$context['replacement']['predecessor_execution_id'])
        || ((string)$pred['latest']['selection_event_id']!==$selection_id && (!$own_head || $pred['latest']['event_kind']!=='select'
            || (string)$pred['latest']['native_execution_id']!==(string)$head['native_execution_id']))){$error='native_replacement_stale_selection_or_execution_head';return false;}
    return $context;
}

/** Start a genuinely source-admitted full revision through existing staged command/audit custody. */
function pay_glb_001_native_replacement_start($period,$key,array $input,$selection_id,&$error=null)
{
    return pay_glb_001_native_producer_start($period,$key,$input,$error,$selection_id);
}

/** Permit only a complete direct successor of the exact current selected execution. */
function pay_glb_001_native_replacement_selection_allowed(array $operation,array $latest,&$error=null)
{
    if(!isset($operation['context']['replacement']) || $operation['context']['producer_contract_version']!==pay_glb_001_native_replacement_version()
        || $latest['event_kind']!=='select' || (string)$latest['selection_event_id']!==$operation['context']['replacement']['predecessor_selection_id']
        || $latest['selection_sha256']!==$operation['context']['replacement']['predecessor_selection_sha256']
        || (string)$latest['native_execution_id']!==$operation['context']['replacement']['predecessor_execution_id']
        || $operation['descriptor']['predecessor_execution_sha256']!==$operation['context']['replacement']['predecessor_execution_sha256']){
        $error='native_replacement_exact_current_predecessor_required';return false;
    }
    return true;
}

/** Load an actual owned materialized member under independent retro-review source authority. */
function pay_glb_001_native_replacement_review_member($operation_id,$relationship,&$error=null)
{
    $p=pay_glb_001_native_correction_review_principal();
    if($p===false || pay_glb_001_native_helper_id($operation_id,$error)===false || pay_glb_001_native_helper_id($relationship,$error)===false)return false;
    $start=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_command_receipts WHERE company_id='.(int)$p['company_id']
        .' AND command_receipt_id='.(int)$operation_id.' AND command_domain=\'native.start\' AND OCTET_LENGTH(request_context_json)<=16785408 LIMIT 2 FOR UPDATE');
    $stored=$start?json_decode($start['request_context_json'],true):false;
    if(!is_array($stored) || !isset($stored['operation']['replacement'])){$error='native_retro_review_replacement_required';return false;}
    $context=$stored['operation'];$owner=(int)$start['actor_id'];
    if(pay_glb_001_native_producer_command_valid($start,$context['request_sha256'],$owner,$error)===false)return false;
    $aborted=pay_glb_001_native_producer_command('native.abort',pay_glb_001_native_producer_scope_hash($context),
        pay_glb_001_native_producer_hash('abort-key',array('operation_sha256'=>$start['command_sha256'])),$error);
    if($aborted!==null){$error='native_retro_review_cancelled_or_ambiguous_operation';return false;}
    $current=pay_glb_001_native_producer_rebuild($context,$owner,'native_retro_review',$error);
    if($current===false || hrm_pay_core_004_canonical_json($current)!==hrm_pay_core_004_canonical_json($context)){$error='native_retro_review_source_drift';return false;}
    $op=array('id'=>$operation_id,'row'=>$start,'context'=>$current);$source=null;
    foreach($context['sources'] as $item)if($item['payroll_relationship_id']===$relationship)$source=$item;
    if($source===null || $source['retro']===null || $source['retro']['checker_id']!==(string)$p['actor_user_id']){$error='native_retro_actual_approved_checker_required';return false;}
    $command=pay_glb_001_native_producer_command('native.member',pay_glb_001_native_producer_scope_hash($context),pay_glb_001_native_producer_member_key($op,$relationship),$error);
    $d=is_array($command)?pay_glb_001_native_producer_command_valid($command,$context['request_sha256'],$owner,$error):false;
    $bundle=pay_glb_001_native_producer_bundle($source,$context,$error);$integrity=$d===false?false:hrm_pay_core_004_recompute_integrity((int)$d['effect_id'],$error);
    if($d===false || $bundle===false || $integrity===false || $d['effect_kind']!=='core004_result' || $d['effect_sha256']!==$bundle['result']['result_sha256']
        || $integrity['result_sha256']!==$d['effect_sha256'] || $integrity['executor_id']!==$owner || (string)$integrity['snapshot_id']!==$source['snapshot_id']){$error='native_retro_owned_materialized_result_required';return false;}
    $request=array('scope_sha256'=>pay_glb_001_native_producer_scope_hash($context),'retro_plan_id'=>$source['retro']['retro_plan_id'],
        'approved_plan_sha256'=>$source['retro']['approved_plan_sha256'],'approval_sha256'=>$source['retro']['approval_sha256'],
        'original_result_id'=>$source['retro']['original_result_id'],'original_result_sha256'=>$source['retro']['original_result_sha256'],
        'successor_result_id'=>$d['effect_id'],'successor_result_sha256'=>$d['effect_sha256'],'source_manifest_sha256'=>$context['source_manifest_sha256'],
        'maker_id'=>$source['retro']['maker_id'],'executor_id'=>(string)$owner);
    return array('operation'=>$op,'source'=>$source,'member_command'=>$command,'result'=>$d['effect_id'],'request'=>$request);
}

/** Finalize only the real approved checker, preserving actual CORE-004 lineage and atomic source/audit custody. */
function pay_glb_001_native_replacement_finalize($operation_id,$relationship,&$error=null)
{
    if(!empty($GLOBALS['transaction_level']) || pay_glb_001_native_correction_review_principal()===false){$error='native_retro_review_top_level_required';return false;}
    if(!pay_glb_001_native_selection_begin($error))return false;
    $data=pay_glb_001_native_replacement_review_member($operation_id,$relationship,$error);
    $admission=$data===false?false:pay_glb_001_native_correction_admission($data['request'],$data['operation']['row']['command_sha256'],$error);
    if($admission===false){pay_glb_001_native_producer_finish(false,$error);return false;}
    $op=$data['operation'];$context=$op['context'];$retro=$data['source']['retro'];
    $key=pay_glb_001_native_producer_hash('retro-finalize-key',array('member_command_sha256'=>$data['member_command']['command_sha256'],'retro_plan_id'=>$retro['retro_plan_id']));
    $old=pay_glb_001_native_producer_command('native.retro_finalize',pay_glb_001_native_producer_scope_hash($context),$key,$error);
    if($old===false){pay_glb_001_native_producer_finish(false,$error);return false;}
    $lineage=hrm_pay_core_007_finalize_lineage((int)$retro['retro_plan_id'],(int)$data['result'],$error);
    $row=$lineage===false?false:hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_core_004_correction_lineage WHERE lineage_id='.(int)$lineage.' LIMIT 2 FOR UPDATE');
    if(!$row){pay_glb_001_native_producer_finish(false,$error);return false;}
    if($old!==null){
        $cd=pay_glb_001_native_receipt_decode_v1('command',$old['receipt_json'],$error);
        if($cd===false || $cd['actor_id']!==$retro['checker_id'] || $cd['purpose_code']!=='PAY_GLB_001_NATIVE_RETRO_FINALIZE'
            || $cd['request_sha256']!==$context['request_sha256'] || $cd['effect_id']!==(string)$lineage || $cd['effect_sha256']!==$row['lineage_sha256']
            || pay_glb_001_native_receipt_sha256_v1('command',$cd,$error)!==$old['command_sha256']){pay_glb_001_native_producer_finish(false,$error);$error='native_retro_finalization_replay_mismatch';return false;}
    }else{
        $cd=array('company_id'=>$context['scope']['company_id'],'command_domain'=>'native.retro_finalize','scope_sha256'=>pay_glb_001_native_producer_scope_hash($context),
            'command_key_sha256'=>$key,'expected_predecessor_sha256'=>$data['member_command']['command_sha256'],'request_sha256'=>$context['request_sha256'],
            'effect_kind'=>'core004_lineage','effect_id'=>(string)$lineage,'effect_sha256'=>$row['lineage_sha256'],'actor_id'=>$retro['checker_id'],'purpose_code'=>'PAY_GLB_001_NATIVE_RETRO_FINALIZE');
        $values=$cd;$values['command_sha256']=pay_glb_001_native_receipt_sha256_v1('command',$cd,$error);$values['receipt_json']=pay_glb_001_native_receipt_canonical_v1('command',$cd,$error);
        $values['request_context_json']=hrm_pay_core_004_canonical_json(array('operation_id'=>$operation_id,'operation_sha256'=>$op['row']['command_sha256'],
            'member_command_sha256'=>$data['member_command']['command_sha256'],'retro_plan_id'=>$retro['retro_plan_id'],'admission'=>$admission));
        if(pay_glb_001_native_replacement_insert('hrm_pay_glb_001_native_command_receipts',$values,'review',$error)===false
            || pay_glb_001_native_producer_audit($context,'retro_lineage_finalized',$row['lineage_sha256'],$error)===false){pay_glb_001_native_producer_finish(false,$error);return false;}
    }
    if(pay_glb_001_native_correction_admission($data['request'],$op['row']['command_sha256'],$error)===false){pay_glb_001_native_producer_finish(false,$error);return false;}
    if(!pay_glb_001_native_producer_finish(true,$error))return false;return (string)$lineage;
}

/** Validate actual approved lineage/checker custody and immutable execution identity without granting financial authority. */
function pay_glb_001_native_replacement_lineage_proof(array $op,array $source,$result,&$error=null)
{
    $retro=$source['retro'];$owner=(int)$op['row']['actor_id'];
    $row=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_core_004_correction_lineage WHERE predecessor_result_id='.(int)$retro['original_result_id'].' AND successor_result_id='.(int)$result.' LIMIT 2 FOR UPDATE');
    $r=hrm_pay_core_004_recompute_integrity((int)$result,$error);
    if(!$row || $r===false || $r['executor_id']!==$owner || $row['checker_id']!==$retro['checker_id'] || $row['reason_code']!==$retro['reason_code']
        || $row['approval_evidence_sha256']!==$retro['approval_evidence_sha256'] || $row['lineage_kind']!==$retro['plan_type']){$error='native_retro_actual_independent_lineage_required';return false;}
    $sha=hrm_pay_core_004_hash(array('predecessor_result_sha256'=>$retro['original_result_sha256'],'successor_result_sha256'=>$r['result_sha256'],
        'lineage_kind'=>$row['lineage_kind'],'reason_code'=>$row['reason_code'],'approval_evidence_sha256'=>$row['approval_evidence_sha256'],'checker_id'=>(int)$row['checker_id']));
    $member=pay_glb_001_native_producer_command('native.member',pay_glb_001_native_producer_scope_hash($op['context']),pay_glb_001_native_producer_member_key($op,$source['payroll_relationship_id']),$error);
    $key=$member?pay_glb_001_native_producer_hash('retro-finalize-key',array('member_command_sha256'=>$member['command_sha256'],'retro_plan_id'=>$retro['retro_plan_id'])):false;
    $command=$key===false?false:pay_glb_001_native_producer_command('native.retro_finalize',pay_glb_001_native_producer_scope_hash($op['context']),$key,$error);
    $cd=is_array($command)?pay_glb_001_native_receipt_decode_v1('command',$command['receipt_json'],$error):false;
    if($sha!==$row['lineage_sha256'] || $cd===false || $cd['request_sha256']!==$op['context']['request_sha256'] || $cd['actor_id']!==$retro['checker_id']
        || $cd['purpose_code']!=='PAY_GLB_001_NATIVE_RETRO_FINALIZE' || $cd['effect_id']!==(string)$row['lineage_id'] || $cd['effect_sha256']!==$sha
        || $cd['expected_predecessor_sha256']!==$member['command_sha256'] || pay_glb_001_native_receipt_sha256_v1('command',$cd,$error)!==$command['command_sha256']){
        $error='native_retro_lineage_command_required';return false;
    }
    foreach($cd as $field=>$value)if($value!==($command[$field]===null?null:(string)$command[$field])){$error='native_retro_lineage_command_column_invalid';return false;}
    if(pay_glb_001_native_selection_audit_proof($op['context'],'retro_lineage_finalized',$sha,(int)$retro['checker_id'],$error)===false)return false;
    return $row;
}

/** Original actual result executor completes the approved CORE-007 receipt atomically after real independent lineage. */
function pay_glb_001_native_replacement_complete($operation_id,$relationship,&$error=null)
{
    if(!empty($GLOBALS['transaction_level']) || pay_glb_001_native_replacement_principal()===false){$error='native_retro_completion_top_level_required';return false;}
    if(!pay_glb_001_native_selection_begin($error))return false;$op=pay_glb_001_native_producer_load($operation_id,$error);
    if($op===false || !isset($op['context']['replacement'])){pay_glb_001_native_producer_finish(false,$error);return false;}
    $source=null;foreach($op['context']['sources'] as $s)if($s['payroll_relationship_id']===$relationship)$source=$s;
    if($source===null || $source['retro']===null){pay_glb_001_native_producer_finish(false,$error);$error='native_retro_owned_correction_required';return false;}
    $member=pay_glb_001_native_producer_command('native.member',pay_glb_001_native_producer_scope_hash($op['context']),pay_glb_001_native_producer_member_key($op,$relationship),$error);
    $md=is_array($member)?pay_glb_001_native_producer_command_valid($member,$op['context']['request_sha256'],hrm_pay_core_007_actor(),$error):false;
    $lineage=$md===false?false:pay_glb_001_native_replacement_lineage_proof($op,$source,$md['effect_id'],$error);
    $id=$lineage===false?false:hrm_pay_core_007_record_execution((int)$source['retro']['retro_plan_id'],(int)$md['effect_id'],null,$error);
    $row=$id===false?false:hrm_pay_core_007_one('SELECT * FROM '.TB_PREF.'hrm_pay_retro_executions WHERE execution_id='.(int)$id.' LIMIT 2 FOR UPDATE');
    if(!$row){pay_glb_001_native_producer_finish(false,$error);return false;}
    $key=pay_glb_001_native_producer_hash('retro-complete-key',array('member_command_sha256'=>$member['command_sha256'],'retro_plan_id'=>$source['retro']['retro_plan_id']));
    $old=pay_glb_001_native_producer_command('native.retro_complete',pay_glb_001_native_producer_scope_hash($op['context']),$key,$error);
    if($old===false){pay_glb_001_native_producer_finish(false,$error);return false;}
    if($old!==null){$cd=pay_glb_001_native_producer_command_valid($old,$op['context']['request_sha256'],hrm_pay_core_007_actor(),$error);
        if($cd===false || $cd['effect_id']!==(string)$id || $cd['effect_sha256']!==$row['execution_sha256']){pay_glb_001_native_producer_finish(false,$error);return false;}
    }else{
        $receipt=pay_glb_001_native_producer_record_command('native.retro_complete',$op['context'],$key,'core007_execution',(string)$id,$row['execution_sha256'],
            array('operation_id'=>$operation_id,'retro_plan_id'=>$source['retro']['retro_plan_id'],'lineage_id'=>(string)$lineage['lineage_id'],'member_command_sha256'=>$member['command_sha256']),$error,$member['command_sha256']);
        if($receipt===false || pay_glb_001_native_producer_audit($op['context'],'retro_execution_completed',$row['execution_sha256'],$error)===false){pay_glb_001_native_producer_finish(false,$error);return false;}
    }
    if(pay_glb_001_native_producer_admit($op['context'],$error)===false || pay_glb_001_native_replacement_principal()===false){pay_glb_001_native_producer_finish(false,$error);return false;}
    if(!pay_glb_001_native_producer_finish(true,$error))return false;return (string)$id;
}

/** Require actual lineage, executor receipt, member binding and protected completion event before sealing any correction. */
function pay_glb_001_native_replacement_execution_proof(array $op,array $source,$result,&$error=null)
{
    $lineage=pay_glb_001_native_replacement_lineage_proof($op,$source,$result,$error);if($lineage===false)return false;
    $retro=$source['retro'];$row=hrm_pay_core_007_one('SELECT * FROM '.TB_PREF.'hrm_pay_retro_executions WHERE retro_plan_id='.(int)$retro['retro_plan_id'].' LIMIT 2 FOR UPDATE');
    $p=hrm_pay_core_007_one('SELECT * FROM '.TB_PREF.'hrm_pay_retro_plans WHERE retro_plan_id='.(int)$retro['retro_plan_id'].' LIMIT 2');
    $approval=array('plan_sha256'=>$retro['approved_plan_sha256'],'approval_sha256'=>$retro['approval_sha256'],'maker_id'=>(int)$retro['maker_id'],'checker_id'=>(int)$retro['checker_id']);
    $r=hrm_pay_core_004_recompute_integrity((int)$result,$error);
    $d=$row && $p && $r?hrm_pay_core_007_execution_descriptor(hrm_pay_core_007_plan_from_row($p),$approval,(int)$op['row']['actor_id'],
        (int)$retro['original_result_id'],(int)$result,$r['result_sha256'],null,$error):false;
    if($d===false || $d['execution_sha256']!==$row['execution_sha256'] || (string)$row['approval_id']!==$retro['approval_id']
        || (string)$row['core004_lineage_id']!==(string)$lineage['lineage_id'] || (string)$row['successor_snapshot_id']!==$source['snapshot_id']){$error='native_retro_final_executor_receipt_required';return false;}
    foreach($d as $field=>$value){if(in_array($field,array('plan_sha256','approval_sha256'),true))continue;
        if(($value===null?null:(string)$value)!==($row[$field]===null?null:(string)$row[$field])){$error='native_retro_execution_column_invalid';return false;}}
    $member=pay_glb_001_native_producer_command('native.member',pay_glb_001_native_producer_scope_hash($op['context']),pay_glb_001_native_producer_member_key($op,$source['payroll_relationship_id']),$error);
    $key=$member?pay_glb_001_native_producer_hash('retro-complete-key',array('member_command_sha256'=>$member['command_sha256'],'retro_plan_id'=>$retro['retro_plan_id'])):false;
    $command=$key===false?false:pay_glb_001_native_producer_command('native.retro_complete',pay_glb_001_native_producer_scope_hash($op['context']),$key,$error);
    $cd=is_array($command)?pay_glb_001_native_producer_command_valid($command,$op['context']['request_sha256'],(int)$op['row']['actor_id'],$error):false;
    if($cd===false || $cd['effect_kind']!=='core007_execution' || $cd['effect_id']!==(string)$row['execution_id'] || $cd['effect_sha256']!==$row['execution_sha256']
        || $cd['expected_predecessor_sha256']!==$member['command_sha256']){$error='native_retro_completion_command_required';return false;}
    if(pay_glb_001_native_selection_audit_proof($op['context'],'retro_execution_completed',$row['execution_sha256'],(int)$op['row']['actor_id'],$error)===false)return false;return $row;
}

/** Once lineage affects a historical predecessor, recovery must fix forward rather than abandon the operation. */
function pay_glb_001_native_replacement_abort_allowed(array $start,array $context,&$error=null)
{
    $q=db_query('SELECT l.lineage_id FROM '.TB_PREF.'hrm_pay_core_004_correction_lineage l INNER JOIN '.TB_PREF.'hrm_pay_glb_001_native_command_receipts c'
        .' ON c.effect_id=l.successor_result_id AND c.effect_kind=\'core004_result\' AND c.command_domain=\'native.member\''
        .' WHERE c.company_id='.(int)$context['scope']['company_id'].' AND c.request_sha256='.db_escape($context['request_sha256']).' LIMIT 1 FOR UPDATE');
    if(!$q || db_num_rows($q)>0){$error='native_replacement_fix_forward_after_lineage_required';return false;}return true;
}
