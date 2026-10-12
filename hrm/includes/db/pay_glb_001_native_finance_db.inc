<?php
/** Native v2 financial custody through the actual PAY-GL engine; default-disabled, never a parallel ledger. */
require_once dirname(__FILE__).'/pay_glb_001_native_approval_db.inc';
require_once dirname(__FILE__).'/pay_glb_001_native_obligations_db.inc';
require_once dirname(__FILE__).'/pay_gl_001_completion_schema_db.inc';
require_once dirname(__FILE__).'/pay_gl_001_runtime_db.inc';

/** Require the actual fresh action role and separate native finance switch. */
function pay_glb_001_native_finance_principal($action,&$error=null)
{
    global $db_connections;
    if(!in_array($action,array('gl_intent','gl_reconcile','gl_review','gl_authorize','gl_post','gl_recovery_prepare','gl_recovery_review','gl_recover'),true))return false;
    $p=pay_glb_001_native_business_principal($action);
    if($p===false || !isset($db_connections[$p['company_id']]['pay_glb_001_native_finance']['enabled'])
        || $db_connections[$p['company_id']]['pay_glb_001_native_finance']['enabled']!==true) {
        $error='native_finance_disabled_or_principal_denied';return false;
    }
    return $p;
}

/** Revalidate existing immutable native/CORE-010/PAY-GL metadata before any financial effect. */
function pay_glb_001_native_finance_require_schema(&$error=null)
{
    if(function_exists('pay_glb_001_native_reader_schema_cached') && pay_glb_001_native_reader_schema_cached('finance'))return true;
    $state=new PayGlb001NativeProducerSchemaState();
    if(!pay_glb_001_native_projection_require_schema($error)
        || !pay_gl_001_completion_require_groups($state,array('mapping_governance','writer_reconciliation','posting_bridge','reversal_recovery','legacy_cutover'))) {
        $error='native_finance_actual_custody_schema_required';return false;
    }
    return true;
}

/** Reconstruct the exact v2 intent descriptor from physical immutable columns, without a v1 constructor. */
function pay_glb_001_native_finance_intent_proof(array $i,&$error=null)
{
    $x=array();
    foreach(array('payroll_period_id','calendar_period_id','payroll_group_id','group_version_id','legal_entity_id') as $field) {
        $v=pay_glb_001_native_helper_id((string)$i[$field],$error);if($v===false)return false;$x[$field]=$v;
    }
    $x['result_set_sha256']=$i['result_set_sha256'];$x['result_count']=(int)$i['result_count'];
    foreach(array('gross_amount','deduction_amount','net_amount') as $field) {
        if(!is_string($i[$field]) || !preg_match('/^(0|[1-9][0-9]{0,13})\.[0-9]{6}$/D',$i[$field]))return false;$x[$field]=$i[$field];
    }
    $x['projection_contract_version']=$i['projection_contract_version'];$x['projection_set_sha256']=$i['projection_set_sha256'];
    $x['cutover_transition_sha256']=$i['cutover_transition_sha256'];$x['command_key']=$i['command_key'];$x['maker_id']=(int)$i['maker_id'];
    if($x['projection_contract_version']!==pay_glb_001_native_projection_version() || $x['result_count']<1 || $x['result_count']>4096 || $x['maker_id']<=0)return false;
    foreach(array('result_set_sha256','projection_set_sha256','cutover_transition_sha256','command_key') as $field)if(!pay_glb_001_sha256($x[$field]))return false;
    $x['command_sha256']=pay_glb_001_native_projection_hash('intent',$x);
    $x['intent_sha256']=hrm_pay_core_010_hash(array('command_sha256'=>$x['command_sha256'],'result_set_sha256'=>$x['result_set_sha256'],
        'projection_set_sha256'=>$x['projection_set_sha256'],'maker_id'=>$x['maker_id']));
    return $x['command_sha256']===$i['command_sha256'] && $x['intent_sha256']===$i['intent_sha256'];
}

/** Verify real approved central history, original policy/manual actions and typed native binding/audit. */
function pay_glb_001_native_finance_approval_binding($approval_id,$current,&$error=null)
{
    if(empty($GLOBALS['transaction_level']) || pay_glb_001_native_helper_id($approval_id,$error)===false)return false;
    $a=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_core_010_approvals WHERE approval_id='.(int)$approval_id.' LIMIT 2 FOR UPDATE');
    $i=$a?hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_core_010_approval_intents WHERE approval_intent_id='.(int)$a['approval_intent_id'].' LIMIT 2 FOR UPDATE'):false;
    $b=$a?hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_approval_bindings WHERE approval_id='.(int)$approval_id.' LIMIT 2 FOR UPDATE'):false;
    $d=$b?pay_glb_001_native_receipt_decode_v1('approval_binding',$b['receipt_json'],$error):false;
    $draft=$a?get_approval_draft_for_update((int)$a['approval_draft_id']):false;
    $payload=$draft?pay_glb_001_native_approval_decode($draft,$error):false;
    $s=$payload===false?false:pay_glb_001_native_approval_submission($draft,$payload,$error);
    if(!$a || !$i || !$b || $d===false || $s===false || (int)$draft['status']!==APPROVAL_STATUS_APPROVED
        || (string)$draft['approved_trans_no']!==(string)$payload['period_id'] || $a['decision_token']!=='approved'
        || !pay_glb_001_native_finance_intent_proof($i,$error)
        || (string)$payload['approval_intent_id']!==(string)$i['approval_intent_id'] || $payload['intent_sha256']!==$i['intent_sha256']
        || $payload['maker_id']!==(string)$i['maker_id'] || $payload['selection_id']!==(string)$b['selection_event_id']
        || $payload['selection_sha256']!==$d['selection_sha256'] || $payload['native_execution_sha256']!==$d['native_execution_sha256']
        || $d['approval_id']!==$approval_id || $d['approval_intent_id']!==(string)$i['approval_intent_id']
        || $d['intent_sha256']!==$i['intent_sha256'] || $d['approval_sha256']!==$a['approval_sha256']
        || $d['maker_id']!==(string)$i['maker_id'] || $d['approver_id']!==(string)$a['approver_id'] || $d['binder_id']!==$d['approver_id']
        || pay_glb_001_native_receipt_sha256_v1('approval_binding',$d,$error)!==$b['binding_sha256']){if($error===null)$error='native_finance_approval_binding_body_invalid';return false;}
    foreach(array('result_set_sha256','projection_set_sha256','cutover_transition_sha256') as $field)
        if($d[$field]!==$i[$field] || $payload[$field]!==$i[$field]){$error='native_finance_approval_binding_scope_invalid';return false;}
    foreach($d as $field=>$value)if(!in_array($field,array('selection_sha256','native_execution_sha256'),true) && (string)$b[$field]!==$value){$error='native_finance_approval_binding_column_invalid';return false;}
    $proof=hrm_pay_core_010_approval(array('intent_id'=>(int)$i['approval_intent_id'],'payroll_period_id'=>(int)$i['payroll_period_id'],
        'result_set_sha256'=>$i['result_set_sha256'],'projection_set_sha256'=>$i['projection_set_sha256'],'intent_sha256'=>$i['intent_sha256'],
        'approval_draft_id'=>(int)$draft['id'],'decision_token'=>'approved','approver_id'=>(int)$a['approver_id'],'maker_id'=>(int)$i['maker_id']),$error);
    if($proof===false || $proof['approval_sha256']!==$a['approval_sha256']){if($error===null)$error='native_finance_core_approval_proof_invalid';return false;}
    foreach($proof as $field=>$value){if($field==='intent_id')$field='approval_intent_id';if((string)$a[$field]!== (string)$value){$error='native_finance_core_approval_column_invalid';return false;}}
    $minimum=array();foreach($s['workflow']['levels'] as $level){$n=(int)$level['level'];$min=(int)$level['min_approvers'];
        if($n<1 || $n>32 || $min<1 || $min>32 || (isset($minimum[$n])&&$minimum[$n]!==$min))return false;$minimum[$n]=$min;}
    ksort($minimum);if(array_keys($minimum)!==range(1,count($minimum)))return false;
    $o=array('draft'=>$draft,'payload'=>$payload,'policy'=>array('body'=>$s['workflow'],'minimum'=>$minimum,'sha256'=>$payload['workflow_sha256']),'request'=>$s['request']);
    if(!pay_glb_001_native_approval_manual_decisions($o,$error)){if($error===null)$error='native_finance_manual_decisions_invalid';return false;}
    $c=pay_glb_001_native_producer_command('native.approval_binding',$s['request']['scope_sha256'],$d['command_key_sha256'],$error);
    $stored=is_array($c)?json_decode($c['request_context_json'],true):false;
    if(!is_array($stored) || !isset($stored['request'],$stored['approval_id'],$stored['payload_sha256'])
        || !pay_glb_001_native_approval_command_valid($c,$stored['request'],$error) || $stored['approval_id']!==$approval_id
        || $stored['payload_sha256']!==$payload['payload_sha256'] || (string)$c['effect_id']!==(string)$b['approval_binding_id']
        || $c['effect_sha256']!==$b['binding_sha256'] || $c['effect_kind']!=='native_approval_binding'
        || $c['purpose_code']!=='PAY_GLB_001_NATIVE_APPROVE' || (string)$c['actor_id']!==$d['binder_id']
        || pay_glb_001_native_selection_audit_proof($stored['request'],'approval_bound',$b['binding_sha256'],(int)$d['binder_id'],$error)===false){if($error===null)$error='native_finance_approval_command_invalid';return false;}
    $context=null;
    if($current) {
        $context=pay_glb_001_native_approval_context($payload['selection_id'],(string)$i['approval_intent_id'],$error);
        $expected=$context===false?false:pay_glb_001_native_approval_payload($context,$payload['command_key_sha256'],$o['policy'],$error);
        if($expected===false || hrm_pay_core_004_canonical_json($expected)!==hrm_pay_core_004_canonical_json($payload)){if($error===null)$error='native_finance_current_approval_payload_invalid';return false;}
    }
    return array('approval'=>$a,'intent'=>$i,'binding'=>$b,'descriptor'=>$d,'draft'=>$draft,'payload'=>$payload,'submission'=>$s,'context'=>$context);
}

/** Classify the actual immutable approval version; unknown/missing authority fails closed. */
function pay_glb_001_native_finance_classify(array $p,&$error=null)
{
    if(!isset($p['approval_sha256'],$p['result_set_sha256']))return false;
    $q=db_query('SELECT i.projection_contract_version FROM '.TB_PREF.'hrm_pay_core_010_approvals a '
        .'JOIN '.TB_PREF.'hrm_pay_core_010_approval_intents i ON i.approval_intent_id=a.approval_intent_id '
        .'WHERE a.approval_sha256='.db_escape($p['approval_sha256']).' AND a.result_set_sha256='.db_escape($p['result_set_sha256']).' LIMIT 2');
    if(!$q || db_num_rows($q)!==1){$error='native_finance_actual_approval_version_required';return false;}
    $r=db_fetch_assoc($q);return $r['projection_contract_version'];
}

/** Require a native outer owner on every actual PAY-GL entry; retain established v1 calls. */
function pay_glb_001_native_finance_engine_guard($action,array $p,&$error=null)
{
    $version=pay_glb_001_native_finance_classify($p,$error);
    if($version===HRM_PAY_CORE_010_PROJECTION_CONTRACT_VERSION)return true;
    $o=isset($GLOBALS['pay_glb_001_native_finance_owner'])?$GLOBALS['pay_glb_001_native_finance_owner']:false;
    if($version!==pay_glb_001_native_projection_version() || !is_array($o) || empty($GLOBALS['transaction_level'])
        || $o['request']['action']!==$action || $o['binding']['approval']['approval_sha256']!==$p['approval_sha256']
        || $o['binding']['intent']['result_set_sha256']!==$p['result_set_sha256']
        || pay_glb_001_native_finance_principal($action,$error)===false || pay_glb_001_native_business_admission($o['request'],$error)===false) {
        $error='native_finance_owned_source_admission_required';return false;
    }
    return true;
}

/** Explicit v2 CORE-010 approval dispatch; period-wide v1 readers are never filtered or relabelled. */
function pay_glb_001_native_finance_core_approval(array $p,array $a,&$error=null)
{
    $o=isset($GLOBALS['pay_glb_001_native_finance_owner'])?$GLOBALS['pay_glb_001_native_finance_owner']:false;
    if(!is_array($o) || !pay_glb_001_native_finance_engine_guard($o['request']['action'],$p,$error)
        || $o['binding']['context']===null || (string)$a['approval_id']!==(string)$o['binding']['approval']['approval_id'])return false;
    foreach(array('company_id','legal_entity_id','payroll_group_id','payroll_period_id') as $field)
        if((string)$p[$field]!==$o['request']['scope'][$field])return false;
    return $a;
}

/** Supply only the complete selected v2 result set to the existing PAY-GL writer, with a genuine native owner. */
function pay_glb_001_native_finance_results(array $p,&$error=null)
{
    $version=pay_glb_001_native_finance_classify($p,$error);
    if($version===HRM_PAY_CORE_010_PROJECTION_CONTRACT_VERSION)return hrm_pay_core_010_result_set_for_period((int)$p['payroll_period_id'],false,$error);
    $o=isset($GLOBALS['pay_glb_001_native_finance_owner'])?$GLOBALS['pay_glb_001_native_finance_owner']:false;
    if(!is_array($o) || !pay_glb_001_native_finance_engine_guard('gl_intent',$p,$error) || $o['binding']['context']===null)return false;
    $prepared=$o['binding']['context']['prepared'];$set=pay_glb_001_native_projection_result_set_v2($prepared['result_rows'],$error);
    return $set===false?false:array('scope'=>$prepared['request']['scope'],'set'=>$set,'rows'=>$prepared['result_rows']);
}

/** Derive two explicit mapped accounting sides from each verified nonzero selected CORE-004 line. */
function pay_glb_001_native_finance_line_facts(array $p,array $results,&$error=null)
{
    if(!pay_glb_001_native_finance_engine_guard('gl_intent',$p,$error))return false;
    $o=$GLOBALS['pay_glb_001_native_finance_owner'];$prepared=$o['binding']['context']['prepared'];$facts=array();
    if($results['set']['result_set_sha256']!==$o['binding']['intent']['result_set_sha256'])return false;
    foreach($prepared['artifacts'] as $artifact) {
        $header=$artifact['header'];$rid=(int)$header['result_id'];$result_sha=$header['result_sha256'];
        foreach($artifact['details'] as $line) {
            if(trim(str_replace(array('.','0'),'',(string)$line['amount']))==='')continue;
            $minor=pay_gl_001_decimal_to_minor($line['amount'],$error);
            if($minor===false || $minor>10000000000000)return false;
            foreach(array('debit','credit') as $side) {
                $key='result:'.$rid.'-line:'.(int)$line['line_no'].'-'.$side;
                $facts[$result_sha.'|'.$key]=array('source_result_sha256'=>$result_sha,'source_line_key'=>$key,
                    'source_kind'=>'element','source_key'=>'element_version:'.(int)$line['element_version_id'],
                    'entry_side'=>$side,'amount_minor'=>$minor,'memo_code'=>'pc10-'.substr($result_sha,0,16).'-'.(int)$line['line_no'].'-'.$side);
            }
        }
    }
    return count($facts)>0 && count($facts)<=8192?$facts:false;
}

/** Admit an exact signed ledger range and company currency; unsupported native FX needs a versioned exact protocol. */
function pay_glb_001_native_finance_currency(array $scope,&$error=null)
{
    $q=db_query('SELECT value FROM '.TB_PREF.'sys_prefs WHERE name=\'curr_default\' AND category=\'setup.company\' LIMIT 2 FOR UPDATE');
    $row=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    if(!$row || $row['value']!==$scope['currency_code'] || !function_exists('user_price_dec') || user_price_dec()!==2
        || !function_exists('get_company_currency') || get_company_currency()!==$row['value']) {
        $error='native_finance_verified_home_currency_two_place_protocol_required';return false;
    }
    return $row['value'];
}

/** Lock and bind the actual current mapping/cutover and distinct original maker evidence. */
function pay_glb_001_native_finance_mapping(array $scope,$sha,$date,&$error=null)
{
    $m=pay_gl_001_load_mapping_snapshot($sha,(int)$scope['company_id'],(int)$scope['legal_entity_id'],$date,$error);
    $q=$m===false?false:db_query('SELECT * FROM '.TB_PREF.'hrm_pay_gl_001_cutovers WHERE company_id='.(int)$scope['company_id']
        .' AND legal_entity_id='.(int)$scope['legal_entity_id'].' AND (payroll_group_id=0 OR payroll_group_id='.(int)$scope['payroll_group_id'].') '
        .'AND effective_from<='.db_escape($date).' ORDER BY effective_from DESC,cutover_id DESC LIMIT 2 FOR UPDATE');
    $c=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    if(!$c || $c['mapping_snapshot_sha256']!==$sha || (int)$c['approved_by']<=0 || (int)$c['reviewer_id']<=0 || $c['approved_by']===$c['reviewer_id'])return false;
    $body=array('company_id'=>(int)$c['company_id'],'legal_entity_id'=>(int)$c['legal_entity_id'],'payroll_group_id'=>(int)$c['payroll_group_id'],
        'effective_from'=>$c['effective_from'],'mapping_snapshot_sha256'=>$sha,'approved_by'=>(int)$c['approved_by'],
        'reviewer_id'=>(int)$c['reviewer_id'],'approved_at'=>$c['approved_at']);
    if(pay_gl_001_hash($body)!==$c['cutover_sha256'])return false;
    return array('mapping'=>$m,'cutover'=>$c);
}

/** Insert only native command/GL-binding custody under the fresh financial action principal. */
function pay_glb_001_native_finance_insert($table,array $values,$action,&$error=null)
{
    if(empty($GLOBALS['transaction_level']) || pay_glb_001_native_finance_principal($action,$error)===false
        || !in_array($table,array('hrm_pay_glb_001_native_command_receipts','hrm_pay_glb_001_native_gl_bindings'),true))return false;
    $allowed=array();foreach(pay_glb_001_native_schema_definitions() as $m)if($m['name']===$table)
        foreach($m['columns'] as $column)if($column['extra']!=='auto_increment' && $column['name']!=='created_at')$allowed[]=$column['name'];
    $columns=array();$literals=array();foreach($values as $field=>$value) {
        if(!in_array($field,$allowed,true))return false;$literal=pay_glb_001_native_custody_sql_value($field,$value,$error);if($literal===false)return false;
        $columns[]=$field;$literals[]=$literal;
    }
    if(!db_query('INSERT INTO '.TB_PREF.$table.' ('.implode(',',$columns).') VALUES ('.implode(',',$literals).')')){$error='native_finance_atomic_insert_failed';return false;}
    return (string)db_insert_id();
}

/** Append typed native financial effect and actual protected audit in the existing outer transaction. */
function pay_glb_001_native_finance_record(array $request,$kind,$id,$sha,array $metadata,&$error=null)
{
    $d=array('company_id'=>$request['scope']['company_id'],'command_domain'=>'native.'.$request['action'],'scope_sha256'=>$request['scope_sha256'],
        'command_key_sha256'=>$request['command_key_sha256'],'expected_predecessor_sha256'=>$request['selection_sha256'],
        'request_sha256'=>$request['request_sha256'],'effect_kind'=>$kind,'effect_id'=>$id,'effect_sha256'=>$sha,
        'actor_id'=>$request['actor_id'],'purpose_code'=>'PAY_GLB_001_NATIVE_'.strtoupper($request['action']));
    $json=pay_glb_001_native_receipt_canonical_v1('command',$d,$error);if($json===false)return false;
    $v=$d;$v['receipt_json']=$json;$v['command_sha256']=pay_glb_001_native_receipt_sha256_v1('command',$d,$error);
    $v['request_context_json']=hrm_pay_core_004_canonical_json(array('request'=>$request)+$metadata);
    if($v['request_context_json']===false || strlen($v['request_context_json'])>1048576)return false;
    return pay_glb_001_native_finance_insert('hrm_pay_glb_001_native_command_receipts',$v,$request['action'],$error)!==false
        && pay_glb_001_native_producer_audit($request,$request['action'].'_recorded',$sha,$error)!==false;
}

/** Verify an actual prior financial phase by its typed source command and protected causal audit. */
function pay_glb_001_native_finance_effect_proof($action,$kind,$id,$sha,array $binding,&$error=null)
{
    $q=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_command_receipts WHERE company_id='.(int)user_company()
        .' AND command_domain='.db_escape('native.'.$action).' AND effect_kind='.db_escape($kind).' AND effect_id='.(int)$id.' LIMIT 2 FOR UPDATE');
    $c=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;$s=$c?json_decode($c['request_context_json'],true):false;
    if(!is_array($s) || !isset($s['request'],$s['admission'],$s['result']) || !pay_glb_001_native_approval_command_valid($c,$s['request'],$error)
        || $s['request']['action']!==$action || $s['request']['approval_binding_sha256']!==$binding['binding']['binding_sha256']
        || $s['request']['approval_sha256']!==$binding['approval']['approval_sha256'] || $c['effect_sha256']!==$sha
        || $c['purpose_code']!=='PAY_GLB_001_NATIVE_'.strtoupper($action)
        || pay_glb_001_native_selection_audit_proof($s['request'],$action.'_recorded',$sha,(int)$c['actor_id'],$error)===false)return false;
    return $s;
}

/** Resolve exact bounded inputs and lock their real financial rows, never caller-supplied approval/status facts. */
function pay_glb_001_native_finance_context($approval_id,$action,$key,array $input,&$error=null)
{
    $p=pay_glb_001_native_finance_principal($action,$error);
    if($p===false || !pay_glb_001_sha256($key) || !pay_glb_001_native_finance_require_schema($error))return false;
    $fields=array('gl_intent'=>array('journal_date','mapping_snapshot_sha256','source_lines'),'gl_reconcile'=>array('journal_intent_id'),
        'gl_review'=>array('reconciliation_id','decision'),'gl_authorize'=>array('journal_intent_id'),'gl_post'=>array('authority_id'),
        'gl_recovery_prepare'=>array('target_posting_id','command_kind','recovery_date','reason_code','recovery_key'),
        'gl_recovery_review'=>array('target_posting_id','command_kind','recovery_date','reason_code','recovery_key','recovery_preparation_sha256'),
        'gl_recover'=>array('recovery_command_id'));
    if(!pay_glb_001_exact_keys($input,$fields[$action])){$error='native_finance_exact_input_required';return false;}
    foreach(array('journal_intent_id','reconciliation_id','authority_id','target_posting_id','recovery_command_id') as $field)
        if(isset($input[$field]) && pay_glb_001_native_helper_id($input[$field],$error)===false)return false;
    $recovery=in_array($action,array('gl_recovery_prepare','gl_recovery_review','gl_recover'),true);
    $binding=pay_glb_001_native_finance_approval_binding($approval_id,!$recovery,$error);if($binding===false)return false;
    $actor=(string)$p['actor_user_id'];$payload=$binding['payload'];
    if(in_array($actor,array($payload['producer_id'],$payload['projector_id'],$payload['maker_id'],$binding['descriptor']['approver_id']),true)) {
        $error='native_finance_payroll_finance_separation_required';return false;
    }
    $scope=$binding['submission']['request']['scope'];
    if(pay_glb_001_native_finance_currency($scope,$error)===false)return false;
    $j=null;$r=null;$v=null;$a=null;$target=null;$command=null;$jid=null;
    if(isset($input['journal_intent_id']))$jid=$input['journal_intent_id'];
    if(isset($input['reconciliation_id'])) {
        $r=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_gl_001_reconciliations WHERE reconciliation_id='.(int)$input['reconciliation_id'].' LIMIT 2 FOR UPDATE');
        if(!$r)return false;$jid=(string)$r['journal_intent_id'];
    }
    if(isset($input['authority_id'])) {
        $a=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_gl_001_posting_authorities WHERE authority_id='.(int)$input['authority_id'].' LIMIT 2 FOR UPDATE');
        if(!$a || (string)$a['executor_id']!==$actor)return false;$jid=(string)$a['journal_intent_id'];
    }
    if(isset($input['recovery_command_id'])) {
        $command=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_gl_001_recovery_commands WHERE recovery_command_id='.(int)$input['recovery_command_id'].' LIMIT 2 FOR UPDATE');
        if(!$command || (string)$command['reviewer_id']!==$actor)return false;$jid=(string)$command['journal_intent_id'];
        $target=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_gl_001_gl_postings WHERE posting_id='.(int)$command['target_posting_id'].' LIMIT 2 FOR UPDATE');
    }
    if(isset($input['target_posting_id'])) {
        $target=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_gl_001_gl_postings WHERE posting_id='.(int)$input['target_posting_id'].' LIMIT 2 FOR UPDATE');
        if(!$target)return false;$jid=(string)$target['journal_intent_id'];
    }
    if($jid!==null) {
        $j=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_gl_001_journal_intents WHERE journal_intent_id='.(int)$jid.' LIMIT 2 FOR UPDATE');
        if(!$j || $j['approval_sha256']!==$binding['approval']['approval_sha256'] || $j['result_set_sha256']!==$binding['intent']['result_set_sha256']
            || pay_gl_001_journal_sha256($j)!==$j['journal_sha256']
            || pay_glb_001_native_finance_effect_proof('gl_intent','pay_gl_journal_intent',$jid,$j['journal_sha256'],$binding,$error)===false)return false;
        foreach(array('company_id','legal_entity_id','payroll_group_id','payroll_period_id') as $field)if((string)$j[$field]!==$scope[$field])return false;
    }
    $date=$action==='gl_intent'?$input['journal_date']:$j['journal_date'];$map_sha=$action==='gl_intent'?$input['mapping_snapshot_sha256']:$j['mapping_snapshot_sha256'];
    $mapping=pay_glb_001_native_finance_mapping($scope,$map_sha,$date,$error);if($mapping===false)return false;
    $forbidden=array((string)$mapping['mapping']['version']['maker_id'],(string)$mapping['mapping']['version']['checker_id']);
    if($j!==null && $action!=='gl_recovery_prepare' && $action!=='gl_recovery_review' && $action!=='gl_recover')$forbidden[]=(string)$j['prepared_by'];
    if(in_array($actor,$forbidden,true)){$error='native_finance_mapping_or_preparation_separation_required';return false;}
    if($action==='gl_intent') {
        if(!pay_gl_001_date($date) || !is_array($input['source_lines']) || count($input['source_lines'])<1 || count($input['source_lines'])>8192)return false;
        $debit=0;$credit=0;
        foreach($input['source_lines'] as $line) {
            if(!is_array($line) || !isset($line['amount_minor'],$line['entry_side']) || !is_int($line['amount_minor'])
                || $line['amount_minor']<=0 || $line['amount_minor']>10000000000000)return false;
            if($line['entry_side']==='debit')$debit+=$line['amount_minor'];elseif($line['entry_side']==='credit')$credit+=$line['amount_minor'];else return false;
            if($debit>10000000000000 || $credit>10000000000000){$error='native_finance_existing_ledger_exact_range_exceeded';return false;}
        }
        if($debit!==$credit)return false;
    }
    if(in_array($action,array('gl_review','gl_authorize','gl_post'),true)) {
        if($r===null)$r=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_gl_001_reconciliations WHERE journal_intent_id='.(int)$jid.' LIMIT 2 FOR UPDATE');
        if(!$r || (string)$r['created_by']===$actor || $r['journal_sha256']!==$j['journal_sha256']
            || $r['line_set_sha256']!==$j['line_set_sha256'] || $r['allocation_set_sha256']!==$j['allocation_set_sha256'] || $r['status']!=='reconciled'
            || pay_gl_001_hash(array('journal_sha256'=>$j['journal_sha256'],'line_set_sha256'=>$j['line_set_sha256'],
                'allocation_set_sha256'=>$j['allocation_set_sha256'],'native_projection_set_sha256'=>$r['native_projection_set_sha256'],
                'debit_minor'=>(int)$j['debit_minor'],'credit_minor'=>(int)$j['credit_minor'],'status'=>'reconciled'))!==$r['reconciliation_sha256']
            || pay_glb_001_native_finance_effect_proof('gl_reconcile','pay_gl_reconciliation',(string)$r['reconciliation_id'],$r['reconciliation_sha256'],$binding,$error)===false)return false;
    }
    if(in_array($action,array('gl_authorize','gl_post'),true)) {
        $v=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_gl_001_reviews WHERE reconciliation_id='.(int)$r['reconciliation_id'].' LIMIT 2 FOR UPDATE');
        if(!$v || $v['decision']!=='approved' || (string)$v['reviewer_id']===$actor
            || pay_gl_001_hash(array('reconciliation_sha256'=>$r['reconciliation_sha256'],'decision'=>$v['decision'],
                'reviewer_id'=>(int)$v['reviewer_id'],'reviewed_at'=>$v['reviewed_at']))!==$v['review_sha256']
            || pay_glb_001_native_finance_effect_proof('gl_review','pay_gl_review',(string)$v['review_id'],$v['review_sha256'],$binding,$error)===false)return false;
    }
    if($a!==null && ((string)$a['reconciliation_id']!==(string)$r['reconciliation_id'] || (string)$a['review_id']!==(string)$v['review_id']
        || pay_gl_001_hash(array('journal_sha256'=>$j['journal_sha256'],'reconciliation_id'=>(int)$r['reconciliation_id'],'review_sha256'=>$v['review_sha256'],
            'command_key_sha256'=>$a['command_key_sha256'],'executor_id'=>(int)$a['executor_id'],'authorized_at'=>$a['authorized_at']))!==$a['authority_sha256']
        || pay_glb_001_native_finance_effect_proof('gl_authorize','pay_gl_authority',(string)$a['authority_id'],$a['authority_sha256'],$binding,$error)===false))return false;
    if($recovery) {
        if(!$target || (string)$target['journal_intent_id']!==$jid || !pay_glb_001_native_finance_ledger_proof($target,$error)
            || pay_glb_001_native_finance_gl_binding_proof($target,$binding,$error)===false)return false;
        $kind=$command!==null?$command['command_kind']:$input['command_kind'];
        if($kind==='repost') {
            $binding=pay_glb_001_native_finance_approval_binding($approval_id,true,$error);if($binding===false)return false;
        } elseif($kind!=='reverse')return false;
        if(($kind==='reverse' && !in_array($target['posting_kind'],array('post','repost'),true)) || ($kind==='repost' && $target['posting_kind']!=='reverse'))return false;
        if($command!==null) {
            $body=array();foreach(array('company_id','journal_intent_id','target_posting_id') as $field)$body[$field]=(int)$command[$field];
            foreach(array('command_kind','recovery_date','command_key_sha256','reason_code') as $field)$body[$field]=$command[$field];
            $body['maker_id']=(int)$command['maker_id'];$body['reviewer_id']=(int)$command['reviewer_id'];$body['approved_at']=$command['approved_at'];
            if($command['maker_id']===$command['reviewer_id'] || pay_gl_001_hash($body)!==$command['command_sha256']
                || pay_glb_001_native_finance_effect_proof('gl_recovery_review','pay_gl_recovery_command',(string)$command['recovery_command_id'],$command['command_sha256'],$binding,$error)===false)return false;
        } else {
            if(!pay_gl_001_date($input['recovery_date']) || !pay_gl_001_token($input['reason_code'],64) || !pay_gl_001_token($input['recovery_key'],128))return false;
        }
    }
    if($action==='gl_review' && !in_array($input['decision'],array('approved','rejected'),true))return false;
    $excluded_journal=$jid;
    if($action==='gl_intent') {
        $existing=hrm_pay_core_004_read_one('SELECT journal_intent_id,prepared_by FROM '.TB_PREF.'hrm_pay_gl_001_journal_intents WHERE approval_sha256='
            .db_escape($binding['approval']['approval_sha256']).' AND result_set_sha256='.db_escape($binding['intent']['result_set_sha256']).' LIMIT 2 FOR UPDATE');
        if($existing){if((string)$existing['prepared_by']!==$actor)return false;$excluded_journal=(string)$existing['journal_intent_id'];}
    }
    if(!pay_glb_001_native_finance_obligations_clear($scope,$excluded_journal,$error))return false;
    $base=array();foreach(array('selection_id','selection_sha256','native_execution_id','native_execution_sha256','scope','scope_sha256',
        'source_manifest_sha256','producer_id','projection_contract_version','projection_set_sha256','result_set_sha256','cutover_transition_sha256') as $field)
        $base[$field]=$binding['submission']['request'][$field];
    $base['schema']='PAY_GLB_001_NATIVE_FINANCE_REQUEST_V1';$base['action']=$action;$base['actor_id']=$actor;$base['command_key_sha256']=$key;
    $base['actor_role_id']=(string)$_SESSION['wa_current_user']->access;$base['payroll_payload_sha256']=$payload['payload_sha256'];
    $base['approval_id']=$approval_id;$base['approval_sha256']=$binding['approval']['approval_sha256'];
    $base['approval_binding_id']=(string)$binding['binding']['approval_binding_id'];$base['approval_binding_sha256']=$binding['binding']['binding_sha256'];
    $base['finance_input_sha256']=pay_glb_001_native_producer_hash('finance-input',$input);
    $base['finance_cutover_sha256']=$mapping['cutover']['cutover_sha256'];$base['mapping_snapshot_sha256']=$map_sha;
    foreach(array('journal_sha256'=>$j===null?null:$j['journal_sha256'],'reconciliation_sha256'=>$r===null?null:$r['reconciliation_sha256'],
        'review_sha256'=>$v===null?null:$v['review_sha256'],'authority_sha256'=>$a===null?null:$a['authority_sha256'],
        'target_posting_sha256'=>$target===null?null:$target['posting_sha256'],'recovery_command_sha256'=>$command===null?null:$command['command_sha256']) as $field=>$value)$base[$field]=$value;
    $base['request_sha256']=pay_glb_001_native_producer_hash('business-request',$base);
    return array('request'=>$base,'binding'=>$binding,'input'=>$input,'journal'=>$j,'reconciliation'=>$r,'review'=>$v,'authority'=>$a,
        'target'=>$target,'command'=>$command,'mapping'=>$mapping);
}

/** Verify an immutable native GL binding, every FK/receipt column, typed command and protected audit. */
function pay_glb_001_native_finance_gl_binding_proof(array $posting,array $binding,&$error=null)
{
    $row=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_gl_bindings WHERE posting_id='.(int)$posting['posting_id'].' LIMIT 2 FOR UPDATE');
    $d=$row?pay_glb_001_native_receipt_decode_v1('gl_binding',$row['receipt_json'],$error):false;
    if($d===false || $d['approval_binding_sha256']!==$binding['binding']['binding_sha256']
        || (string)$row['approval_binding_id']!==(string)$binding['binding']['approval_binding_id']
        || $d['approval_sha256']!==$binding['approval']['approval_sha256'] || $d['posting_id']!==(string)$posting['posting_id']
        || $d['posting_sha256']!==$posting['posting_sha256'] || $d['posting_kind']!==$posting['posting_kind']
        || $d['predecessor_posting_id']!==($posting['predecessor_posting_id']===null?null:(string)$posting['predecessor_posting_id'])
        || pay_glb_001_native_receipt_sha256_v1('gl_binding',$d,$error)!==$row['gl_binding_sha256'])return false;
    foreach($d as $field=>$value)if($field!=='approval_binding_sha256' && $value!==($row[$field]===null?null:(string)$row[$field]))return false;
    $action=$posting['posting_kind']==='post'?'gl_post':'gl_recover';
    $proof=pay_glb_001_native_finance_effect_proof($action,'native_gl_binding',(string)$row['gl_binding_id'],$row['gl_binding_sha256'],$binding,$error);
    if($proof===false || $d['command_key_sha256']!==$proof['request']['command_key_sha256']
        || $d['bound_by']!==$proof['request']['actor_id']
        || $d['native_gl_effect_sha256']!==pay_glb_001_native_producer_hash('native-gl-effect',
            array('approval_binding_sha256'=>$binding['binding']['binding_sha256'],'effect_key'=>$posting['effect_key'],
                'posting_sha256'=>$posting['posting_sha256'],'request_sha256'=>$proof['request']['request_sha256'])))return false;
    return array('row'=>$row,'descriptor'=>$d,'command'=>$proof);
}

/** Append the real PAY-GL effect binding after exact balanced ledger read-back, under the same transaction. */
function pay_glb_001_native_finance_bind(array $o,array $result,&$error=null)
{
    $p=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_gl_001_gl_postings WHERE posting_id='.(int)$result['posting_id'].' LIMIT 2 FOR UPDATE');
    if(!$p || !pay_glb_001_native_finance_ledger_proof($p,$error))return false;
    $a=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_gl_001_posting_authorities WHERE authority_id='.(int)$p['authority_id'].' LIMIT 2 FOR UPDATE');
    $r=$a?hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_gl_001_reconciliations WHERE reconciliation_id='.(int)$a['reconciliation_id'].' LIMIT 2 FOR UPDATE'):false;
    $j=$a?hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_gl_001_journal_intents WHERE journal_intent_id='.(int)$a['journal_intent_id'].' LIMIT 2 FOR UPDATE'):false;
    if(!$r || !$j)return false;
    $effect=pay_glb_001_native_producer_hash('native-gl-effect',array('approval_binding_sha256'=>$o['binding']['binding']['binding_sha256'],
        'effect_key'=>$p['effect_key'],'posting_sha256'=>$p['posting_sha256'],'request_sha256'=>$o['request']['request_sha256']));
    $d=array('approval_binding_sha256'=>$o['binding']['binding']['binding_sha256'],'journal_intent_id'=>(string)$p['journal_intent_id'],
        'reconciliation_id'=>(string)$a['reconciliation_id'],'review_id'=>(string)$a['review_id'],'posting_authority_id'=>(string)$a['authority_id'],
        'posting_id'=>(string)$p['posting_id'],'journal_sha256'=>$j['journal_sha256'],'approval_sha256'=>$j['approval_sha256'],
        'posting_sha256'=>$p['posting_sha256'],'reconciliation_sha256'=>$r['reconciliation_sha256'],'posting_kind'=>$p['posting_kind'],
        'predecessor_posting_id'=>$p['predecessor_posting_id']===null?null:(string)$p['predecessor_posting_id'],'native_gl_effect_sha256'=>$effect,
        'command_key_sha256'=>$o['request']['command_key_sha256'],'bound_by'=>$o['request']['actor_id']);
    $json=pay_glb_001_native_receipt_canonical_v1('gl_binding',$d,$error);if($json===false)return false;
    $sha=pay_glb_001_native_receipt_sha256_v1('gl_binding',$d,$error);$values=$d;unset($values['approval_binding_sha256']);
    $values['approval_binding_id']=(string)$o['binding']['binding']['approval_binding_id'];$values['gl_binding_sha256']=$sha;$values['receipt_json']=$json;
    $id=pay_glb_001_native_finance_insert('hrm_pay_glb_001_native_gl_bindings',$values,$o['request']['action'],$error);
    return $id===false?false:array('id'=>$id,'sha'=>$sha,'posting'=>$p);
}

/** Prepare only a hashed financial object request; no ledger authority or payroll details are returned. */
function pay_glb_001_native_finance_prepare($approval_id,$action,$key,array $input,&$error=null)
{
    if(!empty($GLOBALS['transaction_level']) || !pay_glb_001_native_selection_begin($error))return false;
    try {
        $o=pay_glb_001_native_finance_context($approval_id,$action,$key,$input,$error);
        if(!pay_glb_001_native_producer_finish($o!==false,$error))return false;return $o===false?false:array('request'=>$o['request']);
    } catch(Exception $e){$error='native_finance_prepare_failed';pay_glb_001_native_producer_finish(false,$error);return false;
    } catch(Throwable $e){$error='native_finance_prepare_failed';pay_glb_001_native_producer_finish(false,$error);return false;}
}

/** Execute/replay the actual PAY-GL phase, custody and source revocation check in one owned transaction. */
function pay_glb_001_native_finance_execute($approval_id,$action,$key,array $input,&$error=null)
{
    if(!empty($GLOBALS['transaction_level']) || isset($GLOBALS['pay_glb_001_native_finance_owner']) || !pay_glb_001_native_selection_begin($error))return false;
    try {
        $o=pay_glb_001_native_finance_context($approval_id,$action,$key,$input,$error);
        $admission=$o===false?false:pay_glb_001_native_business_admission($o['request'],$error);
        if($admission===false){pay_glb_001_native_producer_finish(false,$error);return false;}
        $req=$o['request'];$GLOBALS['pay_glb_001_native_finance_owner']=$o;
        $old=pay_glb_001_native_producer_command('native.'.$action,$req['scope_sha256'],$key,$error);
        if(is_array($old)) {
            $stored=json_decode($old['request_context_json'],true);
            if(!is_array($stored) || !isset($stored['request'],$stored['result']) || !pay_glb_001_native_approval_command_valid($old,$stored['request'],$error)
                || $stored['request']['request_sha256']!==$req['request_sha256']
                || pay_glb_001_native_selection_audit_proof($req,$action.'_recorded',$old['effect_sha256'],(int)$req['actor_id'],$error)===false) {
                pay_glb_001_native_producer_finish(false,$error);return false;
            }
            $result=$stored['result'];
            if(in_array($action,array('gl_post','gl_recover'),true)) {
                $p=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_gl_001_gl_postings WHERE posting_id='.(int)$result['posting_id'].' LIMIT 2 FOR UPDATE');
                if(!$p || !pay_glb_001_native_finance_ledger_proof($p,$error) || pay_glb_001_native_finance_gl_binding_proof($p,$o['binding'],$error)===false){pay_glb_001_native_producer_finish(false,$error);return false;}
            }
        } else {
            if($old===false){pay_glb_001_native_producer_finish(false,$error);return false;}
            $kind=null;$id=null;$sha=null;
            if($action==='gl_intent') {
                $scope=$req['scope'];$p=array();foreach(array('company_id','legal_entity_id','payroll_group_id','payroll_period_id') as $field)$p[$field]=(int)$scope[$field];
                $p+=array('result_set_sha256'=>$o['binding']['intent']['result_set_sha256'],'approval_sha256'=>$o['binding']['approval']['approval_sha256'],
                    'journal_date'=>$input['journal_date'],'currency_code'=>$scope['currency_code'],'mapping_snapshot_sha256'=>$input['mapping_snapshot_sha256'],'source_lines'=>$input['source_lines']);
                $result=pay_gl_001_create_journal_intent($p,$error);$kind='pay_gl_journal_intent';$id_field='journal_intent_id';$sha_field='journal_sha256';
            } elseif($action==='gl_reconcile') {$result=pay_gl_001_reconcile_intent((int)$input['journal_intent_id'],$error);$kind='pay_gl_reconciliation';$id_field='reconciliation_id';$sha_field='reconciliation_sha256';
            } elseif($action==='gl_review') {$result=pay_gl_001_review_reconciliation((int)$input['reconciliation_id'],$input['decision'],$error);$kind='pay_gl_review';$id_field='review_id';$sha_field='review_sha256';
            } elseif($action==='gl_authorize') {$result=pay_gl_001_authorize_posting((int)$input['journal_intent_id'],$key,$error);$kind='pay_gl_authority';$id_field='authority_id';$sha_field='authority_sha256';
            } elseif($action==='gl_post') {$result=pay_gl_001_post_approved_intent((int)$input['authority_id'],$error);
            } elseif($action==='gl_recovery_prepare' || $action==='gl_recovery_review') {
                $spec=$input;unset($spec['recovery_key']);$spec['command_key']=$input['recovery_key'];
                if($action==='gl_recovery_prepare'){$result=pay_gl_001_prepare_recovery($spec,$error);$kind='pay_gl_recovery_preparation';$id_field=null;$sha_field='recovery_preparation_sha256';}
                else{$result=pay_gl_001_create_recovery_command($spec,$error);$kind='pay_gl_recovery_command';$id_field='recovery_command_id';$sha_field='command_sha256';}
            } else {
                $leaf=pay_glb_001_native_finance_posting_leaf($o['journal'],$error);
                if($leaf===false || !$leaf || (string)$leaf['posting_id']!==(string)$o['target']['posting_id']){$error='native_finance_recovery_target_not_current_leaf';pay_glb_001_native_producer_finish(false,$error);return false;}
                $result=pay_gl_001_execute_recovery((int)$input['recovery_command_id'],$error);
            }
            if(!is_array($result)){pay_glb_001_native_producer_finish(false,$error);return false;}
            $result=json_decode(hrm_pay_core_004_canonical_json($result),true);
            if(!is_array($result)){pay_glb_001_native_producer_finish(false,$error);return false;}
            if(in_array($action,array('gl_post','gl_recover'),true)) {
                $b=pay_glb_001_native_finance_bind($o,$result,$error);if($b===false){pay_glb_001_native_producer_finish(false,$error);return false;}
                $kind='native_gl_binding';$id=$b['id'];$sha=$b['sha'];
            } else {$id=$id_field===null?(string)$o['target']['posting_id']:(string)$result[$id_field];$sha=$result[$sha_field];}
            if(!pay_glb_001_native_finance_record($req,$kind,$id,$sha,array('admission'=>$admission,'input'=>$input,'result'=>$result),$error)) {
                pay_glb_001_native_producer_finish(false,$error);return false;
            }
        }
        if(pay_glb_001_native_finance_principal($action,$error)===false || pay_glb_001_native_business_admission($req,$error)===false
            || !pay_glb_001_native_producer_finish(true,$error)) {
            if(!empty($GLOBALS['transaction_level']))pay_glb_001_native_producer_finish(false,$error);return false;
        }
        return $result;
    } catch(Exception $e){$error='native_finance_execution_failed';pay_glb_001_native_producer_finish(false,$error);return false;
    } catch(Throwable $e){$error='native_finance_execution_failed';pay_glb_001_native_producer_finish(false,$error);return false;
    } finally {unset($GLOBALS['pay_glb_001_native_finance_owner']);}
}
