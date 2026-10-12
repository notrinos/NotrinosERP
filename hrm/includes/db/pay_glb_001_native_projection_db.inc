<?php
/** Actual native v2 projection and intent custody; central approval and GL remain separate. */
require_once dirname(__FILE__).'/pay_glb_001_native_replacement_db.inc';
require_once dirname(__FILE__).'/pay_core_010_governance_db.inc';
require_once dirname(dirname(__FILE__)).'/payroll/pay_glb_001_native_projection_contract.inc';
require_once dirname(dirname(dirname(dirname(__FILE__)))).'/includes/pay_glb_001_native_business_admission.inc';

/** Verify exact native custody and immutable existing CORE-010 prerequisites. */
function pay_glb_001_native_projection_require_schema(&$error=null)
{
    if(function_exists('pay_glb_001_native_reader_schema_cached') && pay_glb_001_native_reader_schema_cached('projection'))return true;
    $state=new PayGlb001NativeProducerSchemaState();
    if (!pay_glb_001_native_require_schema($state,false) || !hrm_pay_core_010_require_full_schema($state)) {
        $error='native_projection_custody_schema_invalid';return false;
    }
    $physical=hrm_migration_physical_table_name('hrm_pay_core_010_approval_intents');
    if($physical===false){$error='native_projection_intent_storage_scope_invalid';return false;}
    foreach (array('gross_amount','deduction_amount','net_amount') as $field) {
        $r=hrm_pay_core_004_read_one('SELECT COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='
            .db_escape($physical).' AND COLUMN_NAME='.db_escape($field));
        if (!$r || strtolower($r['COLUMN_TYPE'])!=='decimal(20,6)') { $error='native_projection_intent_storage_drift';return false; }
    }
    return true;
}

/** Recompute the actual immutable cutover chain and referenced parity without inventing an activation. */
function pay_glb_001_native_projection_cutover(array $scope,&$error=null)
{
    $q=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_core_010_cutover_transitions WHERE legal_entity_id='.(int)$scope['legal_entity_id']
        .' AND payroll_group_id='.(int)$scope['payroll_group_id'].' ORDER BY sequence_no LIMIT 4097 FOR UPDATE');
    if (!$q || db_num_rows($q)<1 || db_num_rows($q)>4096) { $error='native_projection_actual_cutover_required';return false; }
    $previous=null;$sequence=0;
    while ($row=db_fetch_assoc($q)) {
        $parity_sha=null;
        if ($row['parity_id']!==null) {
            $p=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_core_010_parity_evidence WHERE parity_id='.(int)$row['parity_id'].' LIMIT 2 FOR UPDATE');
            $pbody=$p; if ($pbody) unset($pbody['parity_id'],$pbody['created_at'],$pbody['parity_sha256']);
            $parity=$pbody?hrm_pay_core_010_parity($pbody,$error):false;
            if ($parity===false || $parity['parity_sha256']!==$p['parity_sha256'] || $p['parity_status']!=='match'
                || (string)$p['legal_entity_id']!==$scope['legal_entity_id'] || (string)$p['payroll_group_id']!==$scope['payroll_group_id']
                || (int)$p['reviewer_id']===(int)$row['actor_id']) { $error='native_projection_cutover_parity_invalid';return false; }
            $parity_sha=$p['parity_sha256'];
            $parity_scope=hrm_pay_core_010_period_scope((int)$p['payroll_period_id'],false,$error);
            if ($parity_scope===false || (string)$parity_scope['payroll_group_id']!==$scope['payroll_group_id']
                || (string)$parity_scope['legal_entity_id']!==$scope['legal_entity_id']
                || (string)$parity_scope['group_version_id']!==(string)$p['group_version_id']) {
                $error='native_projection_cutover_parity_scope_invalid';return false;
            }
        }
        $d=hrm_pay_core_010_transition(array('payroll_group_id'=>(int)$row['payroll_group_id'],'legal_entity_id'=>(int)$row['legal_entity_id'],
            'sequence_no'=>(int)$row['sequence_no'],'from_state'=>$row['from_state'],'to_state'=>$row['to_state'],
            'expected_sequence_no'=>(int)$row['expected_sequence_no'],'parity_sha256'=>$parity_sha,'command_key'=>$row['command_key'],
            'predecessor_transition_sha256'=>$row['predecessor_transition_sha256'],'actor_id'=>(int)$row['actor_id']),$error);
        if ($d===false || (int)$row['sequence_no']!==++$sequence || $row['from_state']!==($previous===null?'legacy':$previous['to_state'])
            || $row['predecessor_transition_sha256']!==($previous===null?null:$previous['transition_sha256'])
            || $row['command_sha256']!==$d['command_sha256'] || $row['transition_sha256']!==$d['transition_sha256']
            || $row['result_contract_version']!==HRM_PAY_CORE_010_CONTRACT_VERSION
            || ($row['to_state']==='active_new' && $previous!==null && (int)$previous['actor_id']===(int)$row['actor_id'])) {
            $error='native_projection_cutover_chain_invalid';return false;
        }
        $previous=$row;
    }
    if ($previous['to_state']!=='active_new') { $error='native_projection_active_cutover_required';return false; }
    return $previous;
}

/** Reconstruct current selected producer, actual mapped employees and complete forward artifacts under locks. */
function pay_glb_001_native_projection_prepare_locked($selection_id,$key,$action,&$error=null)
{
    $p=pay_glb_001_native_business_principal($action);
    if ($p===false) { $error='native_projection_principal_denied';return false; }
    if (pay_glb_001_native_helper_id($selection_id,$error)===false || !pay_glb_001_sha256($key)) {
        $error='native_projection_request_identity_invalid';return false;
    }
    if (!pay_glb_001_native_projection_require_schema($error)) return false;
    $selection=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_selection_events WHERE company_id='.(int)$p['company_id']
        .' AND selection_event_id='.(int)$selection_id.' LIMIT 2 FOR UPDATE');
    if (!$selection || $selection['event_kind']!=='select') { $error='native_projection_selected_event_required';return false; }
    $scope=array();foreach (array('company_id','legal_entity_id','payroll_group_id','payroll_period_id') as $field) $scope[$field]=(string)$selection[$field];
    $latest=$action==='read' && function_exists('pay_glb_001_native_reader_projection_selection')
        ?pay_glb_001_native_reader_projection_selection($scope):false;
    if($latest===false)$latest=pay_glb_001_native_selection_latest($scope,$error);
    if (!$latest || (string)$latest['selection_event_id']!==$selection_id || $latest['event_kind']!=='select') {
        $error='native_projection_stale_selection';return false;
    }
    $seal=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_command_receipts WHERE company_id='.(int)$p['company_id']
        .' AND command_domain=\'native.seal\' AND effect_kind=\'native_execution\' AND effect_id='.(int)$selection['native_execution_id'].' LIMIT 2 FOR UPDATE');
    $event=$seal?json_decode($seal['request_context_json'],true):false;
    $op=is_array($event)&&isset($event['operation_id'])?pay_glb_001_native_selection_operation($event['operation_id'],true,true,$error):false;
    if ($op===false || (string)$op['execution']['native_execution_id']!==(string)$selection['native_execution_id']
        || $latest['descriptor']['native_execution_sha256']!==$op['execution']['execution_sha256']) return false;
    $context=$op['context'];$cutover=pay_glb_001_native_projection_cutover($context['scope'],$error);if ($cutover===false) return false;
    $period=hrm_pay_core_010_period_scope((int)$context['scope']['payroll_period_id'],true,$error);if ($period===false) return false;
    $q=db_query('SELECT receipt_json FROM '.TB_PREF.'hrm_pay_glb_001_native_members WHERE native_execution_id='.(int)$selection['native_execution_id'].' ORDER BY ordinal_no FOR UPDATE');
    if (!$q || db_num_rows($q)!==count($context['sources'])) { $error='native_projection_complete_members_required';return false; }
    $artifacts=array();$rows=array();$i=0;
    while ($member_row=db_fetch_assoc($q)) {
        $member=pay_glb_001_native_receipt_decode_v1('member',$member_row['receipt_json'],$error);$source=$context['sources'][$i++];
        if ($member===false || $member['payroll_relationship_id']!==$source['payroll_relationship_id']) return false;
        $employee=hrm_pay_core_010_employee_for_result(array('assignment_id'=>$member['assignment_id'],'payroll_relationship_id'=>$member['payroll_relationship_id']),$period,$error);
        $bundle=$employee?pay_glb_001_native_producer_bundle($source,$context,$error):false;
        $candidate=$bundle===false?false:pay_glb_001_native_projection_candidate_v2($op['descriptor'],$latest['descriptor'],$member,
            $source['plan']['snapshot_material']['descriptor'],$bundle['result'],$bundle['lines'],$bundle['traces'],hash('sha256',(string)$employee['employee_id']),$error);
        if ($candidate===false) return false;
        $artifacts[]=$candidate;$r=$bundle['result'];
        $rows[]=array('result_id'=>$member['core004_result_id'],'result_sha256'=>$r['result_sha256'],'currency'=>$r['currency'],
            'gross_amount'=>$r['gross_amount'],'deduction_amount'=>$r['deduction_amount'],'net_amount'=>$r['net_amount']);
    }
    $intent=pay_glb_001_native_projection_approval_intent_v2($op['descriptor'],$latest['descriptor'],$rows,$artifacts,
        $cutover['transition_sha256'],$key,(string)$p['actor_user_id'],$error);if ($intent===false) return false;
    $projection_rows=array();foreach ($artifacts as $artifact) $projection_rows[]=$artifact['descriptor'];
    $set=hrm_pay_core_010_projection_set($projection_rows,$error);if ($set===false) return false;
    $request=array('schema'=>'PAY_GLB_001_NATIVE_BUSINESS_REQUEST_V1','action'=>$action,'selection_id'=>$selection_id,
        'selection_sha256'=>$latest['selection_sha256'],'native_execution_id'=>(string)$selection['native_execution_id'],
        'native_execution_sha256'=>$op['execution']['execution_sha256'],'scope'=>$context['scope'],
        'scope_sha256'=>pay_glb_001_native_producer_scope_hash($context),'source_manifest_sha256'=>$context['source_manifest_sha256'],
        'producer_id'=>(string)$op['start']['actor_id'],'actor_id'=>(string)$p['actor_user_id'],'projection_contract_version'=>pay_glb_001_native_projection_version(),
        'projection_set_sha256'=>$set['projection_set_sha256'],'result_set_sha256'=>$intent['result_set_sha256'],
        'cutover_transition_sha256'=>$cutover['transition_sha256'],'command_key_sha256'=>$key);
    $request['request_sha256']=pay_glb_001_native_producer_hash('business-request',$request);
    return array('request'=>$request,'operation'=>$op,'selection'=>$latest,'artifacts'=>$artifacts,'result_rows'=>$rows,'intent'=>$intent);
}

/** Prepare a current request without writing projection or intent authority. */
function pay_glb_001_native_projection_prepare($selection_id,$key,$action='project',&$error=null)
{
    if (!empty($GLOBALS['transaction_level']) || !pay_glb_001_native_selection_begin($error)) { $error='native_projection_top_level_required';return false; }
    $result=pay_glb_001_native_projection_prepare_locked($selection_id,$key,$action,$error);
    if (!pay_glb_001_native_producer_finish($result!==false,$error)) return false;return $result;
}

/** Insert existing projection/intent or native command columns with actual purpose-specific authorization. */
function pay_glb_001_native_projection_insert($table,array $values,$action,&$error=null)
{
    if (empty($GLOBALS['transaction_level']) || pay_glb_001_native_business_principal($action)===false) return false;
    if (($table==='hrm_pay_core_010_projections' && $action!=='project')
        || ($table==='hrm_pay_core_010_approval_intents' && $action!=='intent')) {
        $error='native_projection_insert_purpose_invalid';return false;
    }
    $map=hrm_pay_core_010_columns();
    if ($table==='hrm_pay_glb_001_native_command_receipts') foreach (pay_glb_001_native_schema_definitions() as $model)
        if ($model['name']===$table) { $map[$table]=array();foreach ($model['columns'] as $column) $map[$table][]=$column['name']; }
    if (!in_array($table,array('hrm_pay_core_010_projections','hrm_pay_core_010_approval_intents','hrm_pay_glb_001_native_command_receipts'),true)) return false;
    $columns=array();$literals=array();
    foreach ($values as $field=>$value) {
        if (!in_array($field,$map[$table],true) || in_array($field,array('projection_id','approval_intent_id','command_receipt_id','created_at','projected_at'),true)) {
            $error='native_projection_insert_column_invalid';return false;
        }
        $columns[]='`'.$field.'`';$literal=pay_glb_001_native_custody_sql_value($field,$value,$error);if($literal===false)return false;$literals[]=$literal;
    }
    if (!db_query('INSERT INTO '.TB_PREF.$table.' ('.implode(',',$columns).') VALUES ('.implode(',',$literals).')')) {
        $error='native_projection_insert_failed';return false;
    }
    return (string)db_insert_id();
}

/** Require exactly derived stored v2 rows for these selected members, retaining historical v1/v2 rows. */
function pay_glb_001_native_projection_existing_locked(array $prepared,&$error=null)
{
    $ids=array();$actor=null;$key=null;$projection_commands=array();
    foreach ($prepared['artifacts'] as $artifact) {
        $d=$artifact['descriptor'];
        $row=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_core_010_projections WHERE result_id='.(int)$d['result_id']
            .' AND projection_contract_version='.db_escape(pay_glb_001_native_projection_version()).' LIMIT 2 FOR UPDATE');
        if (!$row || $row['legacy_payslip_id']!==null) { $error='native_projection_complete_v2_rows_required';return false; }
        foreach ($d as $field=>$value) if ((string)$row[$field]!== (string)$value) { $error='native_projection_stored_descriptor_mismatch';return false; }
        if ($actor===null) { $actor=(int)$row['projected_by'];$key=$row['command_key']; }
        if ((int)$row['projected_by']!==$actor || $row['command_key']!==$key) { $error='native_projection_complete_owned_write_required';return false; }
        $ids[]=(string)$row['projection_id'];
        $projection_commands[]=array('command_sha256'=>$row['command_sha256'],'projection_sha256'=>$d['projection_sha256']);
    }
    $req=$prepared['request'];$command=pay_glb_001_native_producer_command('native.project',$req['scope_sha256'],$key,$error);
    $stored=is_array($command)?json_decode($command['request_context_json'],true):false;
    $cd=is_array($command)?pay_glb_001_native_receipt_decode_v1('command',$command['receipt_json'],$error):false;
    if ($cd===false || !is_array($stored) || !isset($stored['projection_ids'],$stored['request']) || !is_array($stored['request']) || $stored['projection_ids']!==$ids
        || $stored['request']['selection_sha256']!==$req['selection_sha256'] || $stored['request']['native_execution_sha256']!==$req['native_execution_sha256']
        || $stored['request']['projection_set_sha256']!==$req['projection_set_sha256'] || $stored['request']['source_manifest_sha256']!==$req['source_manifest_sha256']
        || $stored['request']['cutover_transition_sha256']!==$req['cutover_transition_sha256']
        || $cd['actor_id']!==(string)$actor || $cd['purpose_code']!=='PAY_GLB_001_NATIVE_PROJECT'
        || $cd['command_domain']!=='native.project' || $cd['effect_kind']!=='core010_projection_set'
        || $cd['scope_sha256']!==$req['scope_sha256'] || $cd['command_key_sha256']!==$key
        || $cd['expected_predecessor_sha256']!==$req['selection_sha256']
        || $cd['effect_sha256']!==$req['projection_set_sha256'] || $cd['effect_id']!==$req['native_execution_id']
        || pay_glb_001_native_receipt_sha256_v1('command',$cd,$error)!==$command['command_sha256']) {
        $error='native_projection_owned_command_required';return false;
    }
    $body=$stored['request'];unset($body['request_sha256']);
    if (pay_glb_001_native_producer_hash('business-request',$body)!==$cd['request_sha256']
        || $stored['request']['request_sha256']!==$cd['request_sha256'] || $stored['request']['actor_id']!==$cd['actor_id']) {
        $error='native_projection_request_custody_invalid';return false;
    }
    foreach ($projection_commands as $row) if ($row['command_sha256']!==pay_glb_001_native_producer_hash('projection-row-command',
        array('request_sha256'=>$cd['request_sha256'],'projection_sha256'=>$row['projection_sha256']))) {
        $error='native_projection_row_command_invalid';return false;
    }
    foreach ($cd as $field=>$value) if ($value!==($command[$field]===null?null:(string)$command[$field])) return false;
    if (pay_glb_001_native_selection_audit_proof(array('scope'=>$req['scope'],'request_sha256'=>$cd['request_sha256']),
        'projection_recorded',$req['projection_set_sha256'],$actor,$error)===false) return false;
    return array('projection_ids'=>$ids,'projected_by'=>$actor,'command'=>$command);
}

/** Append one typed projection/intent command, coupled with the caller's actual business transaction. */
function pay_glb_001_native_projection_record_command(array $request,$effect_kind,$effect_id,$effect_sha,array $context,&$error=null)
{
    $d=array('company_id'=>$request['scope']['company_id'],'command_domain'=>'native.'.$request['action'],
        'scope_sha256'=>$request['scope_sha256'],'command_key_sha256'=>$request['command_key_sha256'],
        'expected_predecessor_sha256'=>$request['selection_sha256'],'request_sha256'=>$request['request_sha256'],
        'effect_kind'=>$effect_kind,'effect_id'=>$effect_id,'effect_sha256'=>$effect_sha,'actor_id'=>$request['actor_id'],
        'purpose_code'=>'PAY_GLB_001_NATIVE_'.strtoupper($request['action']));
    $json=pay_glb_001_native_receipt_canonical_v1('command',$d,$error);if ($json===false) return false;
    $values=$d;$values['command_sha256']=pay_glb_001_native_receipt_sha256_v1('command',$d,$error);$values['receipt_json']=$json;
    $values['request_context_json']=hrm_pay_core_004_canonical_json(array('request'=>$request)+$context);
    if ($values['request_context_json']===false || strlen($values['request_context_json'])>1048576) return false;
    return pay_glb_001_native_projection_insert('hrm_pay_glb_001_native_command_receipts',$values,$request['action'],$error);
}

/** Atomically append a complete new v2 projection set; no legacy payslip, period-status or history mutation. */
function pay_glb_001_native_project($selection_id,$key,&$error=null)
{
    if (!empty($GLOBALS['transaction_level']) || !pay_glb_001_native_selection_begin($error)) return false;
    $prepared=pay_glb_001_native_projection_prepare_locked($selection_id,$key,'project',$error);
    $admission=$prepared===false?false:pay_glb_001_native_business_admission($prepared['request'],$error);
    if ($admission===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    $req=$prepared['request'];$old=pay_glb_001_native_producer_command('native.project',$req['scope_sha256'],$key,$error);
    if ($old===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    if ($old!==null) {
        $existing=pay_glb_001_native_projection_existing_locked($prepared,$error);
        $stored=json_decode($old['request_context_json'],true);
        if ($existing===false || $old['request_sha256']!==$req['request_sha256'] || $old['actor_id']!==$req['actor_id']
            || hrm_pay_core_004_canonical_json($stored['request'])!==hrm_pay_core_004_canonical_json($req)) {
            pay_glb_001_native_producer_finish(false,$error);$error='native_projection_conflicting_replay';return false;
        }
        $ids=$existing['projection_ids'];
    } else {
        $ids=array();
        foreach ($prepared['artifacts'] as $artifact) {
            $d=$artifact['descriptor'];$values=$d;$values['legacy_payslip_id']=null;$values['command_key']=$key;
            $values['command_sha256']=pay_glb_001_native_producer_hash('projection-row-command',array('request_sha256'=>$req['request_sha256'],'projection_sha256'=>$d['projection_sha256']));
            $values['projected_by']=$req['actor_id'];
            $id=pay_glb_001_native_projection_insert('hrm_pay_core_010_projections',$values,'project',$error);
            if ($id===false) { pay_glb_001_native_producer_finish(false,$error);return false; }$ids[]=$id;
        }
        if (pay_glb_001_native_projection_record_command($req,'core010_projection_set',$req['native_execution_id'],$req['projection_set_sha256'],
            array('projection_ids'=>$ids,'admission'=>$admission),$error)===false
            || pay_glb_001_native_producer_audit($req,'projection_recorded',$req['projection_set_sha256'],$error)===false) {
            pay_glb_001_native_producer_finish(false,$error);return false;
        }
    }
    if (pay_glb_001_native_business_admission($req,$error)===false || !pay_glb_001_native_producer_finish(true,$error)) {
        if (!empty($GLOBALS['transaction_level'])) pay_glb_001_native_producer_finish(false,$error);return false;
    }
    return array('projection_ids'=>$ids,'projection_set_sha256'=>$req['projection_set_sha256']);
}

/** Append a real v2 CORE-010 intent only for the complete current independently projected member set. */
function pay_glb_001_native_prepare_approval_intent($selection_id,$key,&$error=null)
{
    if (!empty($GLOBALS['transaction_level']) || !pay_glb_001_native_selection_begin($error)) return false;
    $prepared=pay_glb_001_native_projection_prepare_locked($selection_id,$key,'intent',$error);
    $admission=$prepared===false?false:pay_glb_001_native_business_admission($prepared['request'],$error);
    $projection=$admission===false?false:pay_glb_001_native_projection_existing_locked($prepared,$error);
    if ($projection===false || $projection['projected_by']===(int)$prepared['request']['actor_id']) {
        pay_glb_001_native_producer_finish(false,$error);$error='native_intent_independent_projection_required';return false;
    }
    $req=$prepared['request'];$intent=$prepared['intent'];$old=pay_glb_001_native_producer_command('native.intent',$req['scope_sha256'],$key,$error);
    if ($old===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
    if ($old!==null) {
        $row=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_core_010_approval_intents WHERE approval_intent_id='.(int)$old['effect_id'].' LIMIT 2 FOR UPDATE');
        $d=pay_glb_001_native_receipt_decode_v1('command',$old['receipt_json'],$error);
        if (!$row || $d===false || $d['request_sha256']!==$req['request_sha256'] || $d['actor_id']!==$req['actor_id']
            || $d['purpose_code']!=='PAY_GLB_001_NATIVE_INTENT' || $d['effect_kind']!=='core010_intent'
            || $d['effect_sha256']!==$intent['intent_sha256'] || pay_glb_001_native_receipt_sha256_v1('command',$d,$error)!==$old['command_sha256']) {
            pay_glb_001_native_producer_finish(false,$error);$error='native_intent_conflicting_replay';return false;
        }
        foreach ($intent as $field=>$value) if ((string)$row[$field]!== (string)$value) { pay_glb_001_native_producer_finish(false,$error);return false; }
        foreach ($d as $field=>$value) if ($value!==($old[$field]===null?null:(string)$old[$field])) { pay_glb_001_native_producer_finish(false,$error);return false; }
        if (pay_glb_001_native_selection_audit_proof($req,'intent_recorded',$intent['intent_sha256'],(int)$req['actor_id'],$error)===false) {
            pay_glb_001_native_producer_finish(false,$error);return false;
        }
        $id=(string)$row['approval_intent_id'];
    } else {
        $id=pay_glb_001_native_projection_insert('hrm_pay_core_010_approval_intents',$intent,'intent',$error);
        if ($id===false || pay_glb_001_native_projection_record_command($req,'core010_intent',$id,$intent['intent_sha256'],
            array('projection_command_sha256'=>$projection['command']['command_sha256'],'admission'=>$admission),$error)===false
            || pay_glb_001_native_producer_audit($req,'intent_recorded',$intent['intent_sha256'],$error)===false) {
            pay_glb_001_native_producer_finish(false,$error);return false;
        }
    }
    if (pay_glb_001_native_business_admission($req,$error)===false || !pay_glb_001_native_producer_finish(true,$error)) {
        if (!empty($GLOBALS['transaction_level'])) pay_glb_001_native_producer_finish(false,$error);return false;
    }
    $intent['approval_intent_id']=$id;return $intent;
}
