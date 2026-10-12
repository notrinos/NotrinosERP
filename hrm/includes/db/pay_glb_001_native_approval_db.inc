<?php
/** Native v2 submission and terminal custody through the existing central payroll workflow. */
require_once dirname(__FILE__).'/pay_glb_001_native_projection_db.inc';

/** Require fresh source-purpose roles plus the independently default-disabled approval flag. */
function pay_glb_001_native_approval_principal($action,&$error=null)
{
    global $db_connections;
    $p=pay_glb_001_native_business_principal($action);
    if ($p===false || !isset($db_connections[$p['company_id']]['pay_glb_001_native_approval']['enabled'])
        || $db_connections[$p['company_id']]['pay_glb_001_native_approval']['enabled']!==true
        || $p['actor_user_id']>32767) { $error='native_approval_disabled_or_principal_denied';return false; }
    return $p;
}

/** Lock and hash the actual active workflow and every bounded active level, without assigning roles. */
function pay_glb_001_native_approval_policy(&$error=null)
{
    // SQL text casts preserve the existing mysqli evidence, including exact server-formatted DOUBLE values and NULL.
    $workflow_casts=array();foreach(array('id','trans_type','is_active','require_comments_on_reject','require_comments_on_approve',
        'allow_edit_on_approve','allow_self_approve','created_by','modified_by','version') as $field)
        $workflow_casts[]='CAST(w.'.$field.' AS CHAR) AS '.$field;
    $level_casts=array();foreach(array('id','workflow_id','level','role_id','min_approvers','amount_threshold','amount_upper',
        'escalation_days','escalation_to_level','is_active') as $field)$level_casts[]='CAST(l.'.$field.' AS CHAR) AS '.$field;
    $w=hrm_pay_core_004_read_one('SELECT w.*,'.implode(',',$workflow_casts).' FROM '.TB_PREF.'approval_workflows w WHERE trans_type='.(int)ST_PAYROLL_PERIOD.' AND is_active=1 LIMIT 2 FOR UPDATE');
    $q=$w?db_query('SELECT l.*,'.implode(',',$level_casts).' FROM '.TB_PREF.'approval_levels l WHERE workflow_id='.(int)$w['id'].' AND is_active=1 ORDER BY l.level,l.role_id LIMIT 129 FOR UPDATE'):false;
    if (!$w || !$q || db_num_rows($q)<1 || db_num_rows($q)>128) { $error='native_approval_actual_workflow_required';return false; }
    $rows=array();$minimum=array();
    while ($r=db_fetch_assoc($q)) {
        $level=(int)$r['level'];$min=(int)$r['min_approvers'];
        if ($level<1 || $level>32 || $min<1 || $min>32 || (int)$r['role_id']<=0
            || (isset($minimum[$level]) && $minimum[$level]!==$min)) { $error='native_approval_policy_ambiguous';return false; }
        $minimum[$level]=$min;$rows[]=$r;
    }
    ksort($minimum);
    if (array_keys($minimum)!==range(1,count($minimum))) { $error='native_approval_policy_level_gap';return false; }
    $body=array('workflow'=>$w,'levels'=>$rows);
    return array('body'=>$body,'minimum'=>$minimum,'sha256'=>pay_glb_001_native_producer_hash('approval-policy',$body));
}

/** Validate an actual typed command, all physical columns and its entire hashed request. */
function pay_glb_001_native_approval_command_valid(array $row,array $request,&$error=null)
{
    $d=pay_glb_001_native_receipt_decode_v1('command',$row['receipt_json'],$error);
    $body=$request;unset($body['request_sha256']);
    if ($d===false || pay_glb_001_native_receipt_sha256_v1('command',$d,$error)!==$row['command_sha256']
        || pay_glb_001_native_producer_hash('business-request',$body)!==$request['request_sha256']
        || $d['request_sha256']!==$request['request_sha256'] || $d['actor_id']!==$request['actor_id']
        || $d['scope_sha256']!==$request['scope_sha256'] || $d['company_id']!==$request['scope']['company_id']
        || $d['command_key_sha256']!==$request['command_key_sha256']) return false;
    foreach ($d as $field=>$value) if ($value!==($row[$field]===null?null:(string)$row[$field])) return false;
    return true;
}

/** Rebuild the actual complete current intent from immutable producer/projection bytes. */
function pay_glb_001_native_approval_context($selection_id,$intent_id,&$error=null)
{
    $intent=hrm_pay_core_004_read_one('SELECT * FROM '.TB_PREF.'hrm_pay_core_010_approval_intents WHERE approval_intent_id='.(int)$intent_id.' LIMIT 2 FOR UPDATE');
    $prepared=$intent?pay_glb_001_native_projection_prepare_locked($selection_id,$intent['command_key'],'read',$error):false;
    $projection=$prepared===false?false:pay_glb_001_native_projection_existing_locked($prepared,$error);
    if ($projection===false) return false;
    $r=$prepared['request'];
    $expected=pay_glb_001_native_projection_approval_intent_v2($prepared['operation']['descriptor'],$prepared['selection']['descriptor'],
        $prepared['result_rows'],$prepared['artifacts'],$r['cutover_transition_sha256'],$intent['command_key'],(string)$intent['maker_id'],$error);
    if ($expected===false || (int)$intent['maker_id']===$projection['projected_by']) return false;
    foreach ($expected as $field=>$value) if ((string)$intent[$field]!== (string)$value) { $error='native_approval_intent_descriptor_mismatch';return false; }
    $old=pay_glb_001_native_producer_command('native.intent',$r['scope_sha256'],$intent['command_key'],$error);
    $stored=is_array($old)?json_decode($old['request_context_json'],true):false;
    $original=$r;unset($original['request_sha256']);$original['action']='intent';$original['actor_id']=(string)$intent['maker_id'];
    $original['request_sha256']=pay_glb_001_native_producer_hash('business-request',$original);
    if (!is_array($stored) || !isset($stored['request']) || hrm_pay_core_004_canonical_json($stored['request'])!==hrm_pay_core_004_canonical_json($original)
        || !pay_glb_001_native_approval_command_valid($old,$original,$error)
        || $old['purpose_code']!=='PAY_GLB_001_NATIVE_INTENT' || $old['effect_kind']!=='core010_intent'
        || (string)$old['effect_id']!==$intent_id || $old['effect_sha256']!==$intent['intent_sha256']
        || pay_glb_001_native_selection_audit_proof($original,'intent_recorded',$intent['intent_sha256'],(int)$intent['maker_id'],$error)===false) {
        $error='native_approval_real_intent_custody_required';return false;
    }
    return array('prepared'=>$prepared,'intent'=>$intent,'projection'=>$projection);
}

/** Produce one exact native draft with string money; central DOUBLE amount is display/routing metadata only. */
function pay_glb_001_native_approval_payload(array $context,$key,array $policy,&$error=null)
{
    $r=$context['prepared']['request'];$i=$context['intent'];
    $d=array('pay_glb_001_native_approval_schema'=>'PAY_GLB_001_NATIVE_APPROVAL_DRAFT_V1',
        'pay_core_010_projection_contract_version'=>pay_glb_001_native_projection_version(),
        'period_id'=>(int)$r['scope']['payroll_period_id'],'selection_id'=>$r['selection_id'],
        'approval_intent_id'=>(string)$i['approval_intent_id'],'intent_sha256'=>$i['intent_sha256'],
        'selection_sha256'=>$r['selection_sha256'],'native_execution_sha256'=>$r['native_execution_sha256'],
        'source_manifest_sha256'=>$r['source_manifest_sha256'],'result_set_sha256'=>$i['result_set_sha256'],
        'projection_set_sha256'=>$i['projection_set_sha256'],'cutover_transition_sha256'=>$i['cutover_transition_sha256'],
        'intent_command_key_sha256'=>$i['command_key'],'command_key_sha256'=>$key,'maker_id'=>(string)$i['maker_id'],
        'producer_id'=>$r['producer_id'],'projector_id'=>(string)$context['projection']['projected_by'],
        'currency'=>$r['scope']['currency_code'],'total_gross'=>$i['gross_amount'],'total_deductions'=>$i['deduction_amount'],'total_net'=>$i['net_amount'],
        'workflow_id'=>(string)$policy['body']['workflow']['id'],'workflow_version'=>(string)$policy['body']['workflow']['version'],
        'workflow_sha256'=>$policy['sha256']);
    if (!pay_glb_001_sha256($key)) return false;
    $d['payload_sha256']=pay_glb_001_native_producer_hash('approval-draft',$d);return $d;
}

/** Decode only the central framework's established entity-encoded JSON representation. */
function pay_glb_001_native_approval_decode(array $draft,&$error=null)
{
    $d=json_decode(html_entity_decode((string)$draft['draft_data'],ENT_QUOTES,'UTF-8'),true);
    if (!is_array($d) || !isset($d['payload_sha256'])) { $error='native_approval_draft_payload_invalid';return false; }
    $body=$d;unset($body['payload_sha256']);
    if (pay_glb_001_native_producer_hash('approval-draft',$body)!==$d['payload_sha256']) return false;
    return $d;
}

/** Verify real submission custody; negative terminal actions may retain stale historical source. */
function pay_glb_001_native_approval_submission(array $draft,array $payload,&$error=null)
{
    $q=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_command_receipts WHERE company_id='.(int)user_company()
        .' AND command_domain=\'native.submit\' AND effect_kind=\'central_payroll_draft\' AND effect_id='.(int)$draft['id'].' LIMIT 2 FOR UPDATE');
    $row=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;$s=$row?json_decode($row['request_context_json'],true):false;
    if (!is_array($s) || !isset($s['request'],$s['payload'],$s['workflow'],$s['projection_actor_id'])
        || !pay_glb_001_native_approval_command_valid($row,$s['request'],$error)
        || $row['effect_sha256']!==$payload['payload_sha256'] || $row['purpose_code']!=='PAY_GLB_001_NATIVE_SUBMIT'
        || $row['expected_predecessor_sha256']!==$payload['selection_sha256']
        || hrm_pay_core_004_canonical_json($s['payload'])!==hrm_pay_core_004_canonical_json($payload)
        || pay_glb_001_native_producer_hash('approval-policy',$s['workflow'])!==$payload['workflow_sha256']
        || $s['request']['producer_id']!==$payload['producer_id'] || $s['projection_actor_id']!==$payload['projector_id']
        || (string)$draft['submitted_by']!==$payload['maker_id'] || (int)$draft['trans_type']!==(int)ST_PAYROLL_PERIOD
        || (string)$draft['workflow_id']!==$payload['workflow_id'] || (string)$draft['workflow_version']!==$payload['workflow_version']
        || pay_glb_001_native_selection_audit_proof($s['request'],'approval_submitted',$payload['payload_sha256'],(int)$payload['maker_id'],$error)===false) {
        $error='native_approval_actual_submission_required';return false;
    }
    return $s;
}

/** Build an actor/purpose/level/policy/comment-bound object request without granting a business decision. */
function pay_glb_001_native_approval_request($action,array $base,array $payload,array $policy,$draft=null,$comments='')
{
    $actor=(string)$_SESSION['wa_current_user']->user;$level=$draft===null?'0':(string)$draft['current_level'];$draft_id=$draft===null?'0':(string)$draft['id'];
    $key=$action==='submit'?$payload['command_key_sha256']:pay_glb_001_native_producer_hash('approval-action-key',
        array('action'=>$action,'draft_id'=>$draft_id,'actor_id'=>$actor,'level'=>$level,'payload_sha256'=>$payload['payload_sha256']));
    unset($base['request_sha256']);$base['action']=$action;$base['actor_id']=$actor;$base['command_key_sha256']=$key;
    $base['approval_intent_id']=$payload['approval_intent_id'];$base['intent_sha256']=$payload['intent_sha256'];
    $base['payload_sha256']=$payload['payload_sha256'];$base['draft_id']=$draft_id;$base['level']=$level;
    $base['workflow_sha256']=$policy['sha256'];$base['actor_role_id']=(string)$_SESSION['wa_current_user']->access;
    $base['expected_status']='pending';$base['comment_sha256']=hash('sha256',html_entity_decode((string)$comments,ENT_QUOTES,'UTF-8'));
    $base['request_sha256']=pay_glb_001_native_producer_hash('business-request',$base);return $base;
}

/** Revalidate a pending or replayed actual draft; negative actions never confer result/financial authority. */
function pay_glb_001_native_approval_action_context($draft_id,$action,$comments,&$error=null)
{
    $p=pay_glb_001_native_approval_principal($action,$error);
    $draft=$p===false?false:get_approval_draft_for_update((int)$draft_id);$d=$draft?pay_glb_001_native_approval_decode($draft,$error):false;
    $s=$d===false?false:pay_glb_001_native_approval_submission($draft,$d,$error);
    $policy=$s===false?false:pay_glb_001_native_approval_policy($error);if ($policy===false) return false;
    if ($action==='approve') {
        $context=pay_glb_001_native_approval_context($d['selection_id'],$d['approval_intent_id'],$error);
        $expected=$context===false?false:pay_glb_001_native_approval_payload($context,$d['command_key_sha256'],$policy,$error);
        if ($expected===false || hrm_pay_core_004_canonical_json($expected)!==hrm_pay_core_004_canonical_json($d)) {
            $error='native_approval_current_payload_required';return false;
        }
        $base=$context['prepared']['request'];
    } else {
        $context=null;$base=$s['request'];
        $latest=pay_glb_001_native_selection_latest($base['scope'],$error);
        if (!$latest) return false;
        $base['current_selection_sha256']=$latest['selection_sha256'];
    }
    if (($action==='cancel' && (string)$p['actor_user_id']!==$d['maker_id'])
        || (in_array($action,array('approve','reject'),true) && in_array((string)$p['actor_user_id'],
            array($d['maker_id'],$d['producer_id'],$d['projector_id']),true))) {
        $error='native_approval_independent_business_actor_required';return false;
    }
    $req=pay_glb_001_native_approval_request($action,$base,$d,$policy,$draft,$comments);
    return array('request'=>$req,'payload'=>$d,'draft'=>$draft,'policy'=>$policy,'context'=>$context,'submission'=>$s);
}

/** Append only existing CORE-010 approval/native binding/typed-command columns under fresh purpose rights. */
function pay_glb_001_native_approval_insert($table,array $values,$action,&$error=null)
{
    if (empty($GLOBALS['transaction_level']) || pay_glb_001_native_approval_principal($action,$error)===false) return false;
    if (!in_array($table,array('hrm_pay_core_010_approvals','hrm_pay_glb_001_native_approval_bindings','hrm_pay_glb_001_native_command_receipts'),true)) return false;
    if ($table!=='hrm_pay_glb_001_native_command_receipts' && $action!=='approve') return false;
    $allowed=array();$map=hrm_pay_core_010_columns();if (isset($map[$table])) $allowed=$map[$table];
    else foreach (pay_glb_001_native_schema_definitions() as $m) if ($m['name']===$table)
        foreach ($m['columns'] as $column) if ($column['extra']!=='auto_increment' && $column['name']!=='created_at') $allowed[]=$column['name'];
    $columns=array();$literals=array();
    foreach ($values as $field=>$value) {
        if (!in_array($field,$allowed,true) || in_array($field,array('approval_id','created_at'),true)) {
            // approval_id is the restrictive FK in native bindings, never a supplied CORE-010 primary key.
            if (!($field==='approval_id' && $table==='hrm_pay_glb_001_native_approval_bindings' && in_array($field,$allowed,true))) return false;
        }
        $literal=pay_glb_001_native_custody_sql_value($field,$value,$error);if ($literal===false) return false;
        $columns[]=$field;$literals[]=$literal;
    }
    if (!db_query('INSERT INTO '.TB_PREF.$table.' ('.implode(',',$columns).') VALUES ('.implode(',',$literals).')')) {
        $error='native_approval_atomic_insert_failed';return false;
    }
    return (string)db_insert_id();
}

/** Record an exact typed central workflow effect and real protected audit in the same transaction. */
function pay_glb_001_native_approval_record(array $request,$domain,$kind,$id,$sha,$audit,array $extra,&$error=null)
{
    $d=array('company_id'=>$request['scope']['company_id'],'command_domain'=>$domain,'scope_sha256'=>$request['scope_sha256'],
        'command_key_sha256'=>$request['command_key_sha256'],'expected_predecessor_sha256'=>$request['selection_sha256'],
        'request_sha256'=>$request['request_sha256'],'effect_kind'=>$kind,'effect_id'=>$id,'effect_sha256'=>$sha,
        'actor_id'=>$request['actor_id'],'purpose_code'=>'PAY_GLB_001_NATIVE_'.strtoupper($request['action']));
    $json=pay_glb_001_native_receipt_canonical_v1('command',$d,$error);if ($json===false) return false;
    $v=$d;$v['receipt_json']=$json;$v['command_sha256']=pay_glb_001_native_receipt_sha256_v1('command',$d,$error);
    $v['request_context_json']=hrm_pay_core_004_canonical_json(array('request'=>$request)+$extra);
    if ($v['request_context_json']===false || strlen($v['request_context_json'])>1048576) return false;
    return pay_glb_001_native_approval_insert('hrm_pay_glb_001_native_command_receipts',$v,$request['action'],$error)!==false
        && pay_glb_001_native_producer_audit($request,$audit,$sha,$error)!==false;
}

/** Require an actual native wrapper owner; the marker never replaces fresh role/source/business checks. */
function pay_glb_001_native_approval_owner($action,array $payload,$draft_id,&$error=null)
{
    $o=isset($GLOBALS['pay_glb_001_native_approval_owner'])?$GLOBALS['pay_glb_001_native_approval_owner']:false;
    if (!is_array($o) || (int)$GLOBALS['transaction_level']<2 || $o['action']!==$action
        || $o['payload']['payload_sha256']!==$payload['payload_sha256'] || $o['request']['draft_id']!==(string)$draft_id) {
        $error='native_approval_owned_workflow_required';return false;
    }
    return $o;
}

/** Validate every actual central action, including intermediate manual levels; native automatic decisions deny. */
function pay_glb_001_native_approval_validate($action,$data,$draft,array $service,$comments='')
{
    $error=null;
    if ($action==='auto_approve') return false;
    if (!is_array($data) || !in_array($action,array('submit','approve','reject','cancel'),true)) return false;
    $p=pay_glb_001_native_approval_principal($action,$error);
    if ($p===false || $service['user_id']!==$p['actor_user_id'] || $service['role_id']!==(int)$_SESSION['wa_current_user']->access) return false;
    $o=pay_glb_001_native_approval_owner($action,$data,$draft===null?'0':(string)$draft['id'],$error);
    if ($o===false || hrm_pay_core_004_canonical_json($o['payload'])!==hrm_pay_core_004_canonical_json($data)) return false;
    if ($action==='submit') {
        $context=pay_glb_001_native_approval_context($data['selection_id'],$data['approval_intent_id'],$error);
        $policy=$context===false?false:pay_glb_001_native_approval_policy($error);
        $expected=$policy===false?false:pay_glb_001_native_approval_payload($context,$data['command_key_sha256'],$policy,$error);
        if ($expected===false || (string)$p['actor_user_id']!==$data['maker_id']
            || hrm_pay_core_004_canonical_json($expected)!==hrm_pay_core_004_canonical_json($data)) return false;
        $req=pay_glb_001_native_approval_request('submit',$context['prepared']['request'],$data,$policy);
    } else {
        if (!is_array($draft) || (int)$draft['status']!==APPROVAL_STATUS_PENDING) return false;
        $c=pay_glb_001_native_approval_action_context((string)$draft['id'],$action,$comments,$error);
        if ($c===false) return false;$req=$c['request'];
    }
    $admission=pay_glb_001_native_business_admission($req,$error);
    if (hrm_pay_core_004_canonical_json($req)!==hrm_pay_core_004_canonical_json($o['request'])
        || $admission===false) return false;
    if ($action!=='submit' && !pay_glb_001_native_approval_record($req,'native.'.$action,'central_action_admission',
        (string)$draft['id'],$req['request_sha256'],'approval_'.$action.'_admitted',array('workflow'=>$o['policy']['body'],'admission'=>$admission),$error)) return false;
    return true;
}

/** Couple actual central submission with typed source custody and protected audit before notifications/commit. */
function pay_glb_001_native_approval_submitted(array $payload,$draft_id)
{
    $error=null;$o=pay_glb_001_native_approval_owner('submit',$payload,'0',$error);
    $row=get_approval_draft_for_update((int)$draft_id);
    if ($o===false || !$row || (int)$row['status']!==APPROVAL_STATUS_PENDING
        || (string)$row['submitted_by']!==$payload['maker_id'] || pay_glb_001_native_approval_decode($row,$error)!==$payload) return false;
    $admission=pay_glb_001_native_business_admission($o['request'],$error);if($admission===false)return false;
    return pay_glb_001_native_approval_record($o['request'],'native.submit','central_payroll_draft',(string)$draft_id,
        $payload['payload_sha256'],'approval_submitted',array('payload'=>$payload,'workflow'=>$o['policy']['body'],'admission'=>$admission,
            'projection_actor_id'=>(string)$o['context']['projection']['projected_by']),$error);
}

/** Require manual recorded approvals at every real active level and native actor/source receipts for each. */
function pay_glb_001_native_approval_manual_decisions(array $o,&$error=null)
{
    $q=db_query('SELECT * FROM '.TB_PREF.'approval_actions WHERE draft_id='.(int)$o['draft']['id'].' AND action_type=\'approve\' ORDER BY id LIMIT 1025 FOR UPDATE');
    if (!$q || db_num_rows($q)>1024) return false;$counts=array();
    while ($a=db_fetch_assoc($q)) {
        $level=(int)$a['level'];$actor=(string)$a['user_id'];
        if ($actor==='0' || !isset($o['policy']['minimum'][$level]) || in_array($actor,
            array($o['payload']['maker_id'],$o['payload']['producer_id'],$o['payload']['projector_id']),true)) return false;
        $key=pay_glb_001_native_producer_hash('approval-action-key',array('action'=>'approve','draft_id'=>(string)$o['draft']['id'],
            'actor_id'=>$actor,'level'=>(string)$level,'payload_sha256'=>$o['payload']['payload_sha256']));
        $c=pay_glb_001_native_producer_command('native.approve',$o['request']['scope_sha256'],$key,$error);
        $s=is_array($c)?json_decode($c['request_context_json'],true):false;
        if (!is_array($s) || !isset($s['request']) || !pay_glb_001_native_approval_command_valid($c,$s['request'],$error)
            || $s['request']['actor_id']!==$actor || $s['request']['level']!==(string)$level
            || $s['request']['draft_id']!==(string)$o['draft']['id'] || $s['request']['action']!=='approve'
            || $c['purpose_code']!=='PAY_GLB_001_NATIVE_APPROVE' || $c['effect_kind']!=='central_action_admission'
            || (string)$c['effect_id']!==(string)$o['draft']['id'] || $c['effect_sha256']!==$s['request']['request_sha256']
            || $s['request']['payload_sha256']!==$o['payload']['payload_sha256']
            || $s['request']['workflow_sha256']!==$o['policy']['sha256'] || $s['request']['comment_sha256']!==hash('sha256',html_entity_decode((string)$a['comments'],ENT_QUOTES,'UTF-8'))
            || pay_glb_001_native_selection_audit_proof($s['request'],'approval_approve_admitted',$s['request']['request_sha256'],(int)$actor,$error)===false
            || isset($counts[$level][$actor])) return false;
        $counts[$level][$actor]=true;
    }
    foreach ($o['policy']['minimum'] as $level=>$min) if (!isset($counts[$level]) || count($counts[$level])<$min) return false;
    return true;
}

/** Real registered executor: append CORE-010 decision, native binding/command/PAY-AUD; never mutate old payroll rows. */
function pay_glb_001_native_approval_terminal(array $payload,$reserved_trans_no,$draft_id)
{
    $error=null;$o=pay_glb_001_native_approval_owner('approve',$payload,(string)$draft_id,$error);
    $draft=get_approval_draft_for_update((int)$draft_id);
    if ($o===false || !$draft || (int)$draft['status']!==APPROVAL_STATUS_APPROVED
        || pay_glb_001_native_approval_principal('approve',$error)===false
        || pay_glb_001_native_business_admission($o['request'],$error)===false
        || !pay_glb_001_native_approval_manual_decisions($o,$error)) return false;
    $context=pay_glb_001_native_approval_context($payload['selection_id'],$payload['approval_intent_id'],$error);
    if ($context===false || hrm_pay_core_004_canonical_json(pay_glb_001_native_approval_payload($context,$payload['command_key_sha256'],$o['policy'],$error))
        !==hrm_pay_core_004_canonical_json($payload)) return false;
    $p=pay_glb_001_native_approval_principal('approve',$error);$i=$context['intent'];
    $a=hrm_pay_core_010_approval(array('intent_id'=>(int)$i['approval_intent_id'],'payroll_period_id'=>(int)$i['payroll_period_id'],
        'result_set_sha256'=>$i['result_set_sha256'],'projection_set_sha256'=>$i['projection_set_sha256'],'intent_sha256'=>$i['intent_sha256'],
        'approval_draft_id'=>(int)$draft_id,'decision_token'=>'approved','approver_id'=>$p['actor_user_id'],'maker_id'=>(int)$i['maker_id']),$error);
    if ($a===false) return false;
    $v=$a;unset($v['intent_id']);$v['approval_intent_id']=(string)$i['approval_intent_id'];
    $id=pay_glb_001_native_approval_insert('hrm_pay_core_010_approvals',$v,'approve',$error);if ($id===false) return false;
    $b=array('selection_sha256'=>$payload['selection_sha256'],'native_execution_sha256'=>$payload['native_execution_sha256'],
        'approval_intent_id'=>(string)$i['approval_intent_id'],'approval_id'=>$id,'intent_sha256'=>$i['intent_sha256'],
        'approval_sha256'=>$a['approval_sha256'],'result_set_sha256'=>$i['result_set_sha256'],'projection_set_sha256'=>$i['projection_set_sha256'],
        'cutover_transition_sha256'=>$i['cutover_transition_sha256'],'maker_id'=>(string)$i['maker_id'],
        'approver_id'=>(string)$p['actor_user_id'],'binder_id'=>(string)$p['actor_user_id'],'command_key_sha256'=>$o['request']['command_key_sha256']);
    $json=pay_glb_001_native_receipt_canonical_v1('approval_binding',$b,$error);if ($json===false) return false;
    $sha=pay_glb_001_native_receipt_sha256_v1('approval_binding',$b,$error);$v=$b;
    unset($v['selection_sha256'],$v['native_execution_sha256']);$v['selection_event_id']=$payload['selection_id'];
    $v['native_execution_id']=$context['prepared']['request']['native_execution_id'];$v['binding_sha256']=$sha;$v['receipt_json']=$json;
    $binding_id=pay_glb_001_native_approval_insert('hrm_pay_glb_001_native_approval_bindings',$v,'approve',$error);
    if ($binding_id===false || !pay_glb_001_native_approval_record($o['request'],'native.approval_binding','native_approval_binding',
        $binding_id,$sha,'approval_bound',array('approval_id'=>$id,'payload_sha256'=>$payload['payload_sha256']),$error)) return false;
    return (int)$payload['period_id'];
}

/** Real registered negative terminal; retains stale custody and writes no approved decision/binding. */
function pay_glb_001_native_approval_negative_terminal(array $payload,$draft_id,$action,$comments)
{
    $error=null;$o=pay_glb_001_native_approval_owner($action,$payload,(string)$draft_id,$error);
    $draft=get_approval_draft_for_update((int)$draft_id);$status=$action==='reject'?APPROVAL_STATUS_REJECTED:APPROVAL_STATUS_CANCELLED;
    if ($o===false || !$draft || (int)$draft['status']!==$status || pay_glb_001_native_approval_principal($action,$error)===false
        || pay_glb_001_native_business_admission($o['request'],$error)===false) return false;
    $last=get_last_approval_action((int)$draft_id,$action);
    if (!$last || (string)$last['user_id']!==$o['request']['actor_id']
        || hash('sha256',html_entity_decode((string)$last['comments'],ENT_QUOTES,'UTF-8'))!==$o['request']['comment_sha256']) return false;
    $sha=pay_glb_001_native_producer_hash('approval-terminal',array('action'=>$action,'draft_id'=>(string)$draft_id,
        'payload_sha256'=>$payload['payload_sha256'],'request_sha256'=>$o['request']['request_sha256']));
    return pay_glb_001_native_approval_record($o['request'],'native.'.$action.'_terminal','central_negative_terminal',
        (string)$draft_id,$sha,'approval_'.$action.'_terminal',array('payload_sha256'=>$payload['payload_sha256']),$error);
}

/** Return only a bounded source/object request to the grant reviewer, with no payroll detail or authority. */
function pay_glb_001_native_approval_prepare($selection_id,$intent_id,$key,&$error=null)
{
    if (!empty($GLOBALS['transaction_level']) || !pay_glb_001_native_selection_begin($error)) return false;
    $p=pay_glb_001_native_approval_principal('submit',$error);$c=$p===false?false:pay_glb_001_native_approval_context($selection_id,$intent_id,$error);
    $policy=$c===false?false:pay_glb_001_native_approval_policy($error);$d=$policy===false?false:pay_glb_001_native_approval_payload($c,$key,$policy,$error);
    $req=$d===false || (string)$p['actor_user_id']!==$d['maker_id']?false:pay_glb_001_native_approval_request('submit',$c['prepared']['request'],$d,$policy);
    if (!pay_glb_001_native_producer_finish($req!==false,$error)) return false;return $req===false?false:array('request'=>$req);
}

/** Prepare an actual draft action lease; no unauthenticated approver/role assignment is invented. */
function pay_glb_001_native_approval_prepare_action($draft_id,$action,$comments='',&$error=null)
{
    if (!in_array($action,array('approve','reject','cancel'),true) || !empty($GLOBALS['transaction_level']) || !pay_glb_001_native_selection_begin($error)) return false;
    $c=pay_glb_001_native_approval_action_context($draft_id,$action,$comments,$error);
    if (!pay_glb_001_native_producer_finish($c!==false,$error)) return false;return $c===false?false:array('request'=>$c['request']);
}

/** Submit through the real central service with outer resource/source ownership and atomic notifications. */
function pay_glb_001_native_approval_submit($selection_id,$intent_id,$key,&$error=null)
{
    if (!empty($GLOBALS['transaction_level']) || isset($GLOBALS['pay_glb_001_native_approval_owner']) || !pay_glb_001_native_selection_begin($error)) return false;
    try {
        $p=pay_glb_001_native_approval_principal('submit',$error);$c=$p===false?false:pay_glb_001_native_approval_context($selection_id,$intent_id,$error);
        $policy=$c===false?false:pay_glb_001_native_approval_policy($error);$d=$policy===false?false:pay_glb_001_native_approval_payload($c,$key,$policy,$error);
        $req=$d===false || (string)$p['actor_user_id']!==$d['maker_id']?false:pay_glb_001_native_approval_request('submit',$c['prepared']['request'],$d,$policy);
        if ($req===false || pay_glb_001_native_business_admission($req,$error)===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
        $old=pay_glb_001_native_producer_command('native.submit',$req['scope_sha256'],$key,$error);
        if ($old===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
        if ($old!==null) {
            $draft=get_approval_draft_for_update((int)$old['effect_id']);
            if (!$draft || $old['request_sha256']!==$req['request_sha256'] || pay_glb_001_native_approval_submission($draft,$d,$error)===false) {
                pay_glb_001_native_producer_finish(false,$error);return false;
            }
            $result=array('status'=>'replayed','draft_id'=>(int)$draft['id'],'draft_status'=>(int)$draft['status']);
        } else {
            $q=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_glb_001_native_command_receipts WHERE company_id='.(int)$p['company_id']
                .' AND command_domain=\'native.submit\' AND scope_sha256='.db_escape($req['scope_sha256']).' LIMIT 4097 FOR UPDATE');
            if (!$q || db_num_rows($q)>4096) { pay_glb_001_native_producer_finish(false,$error);return false; }
            while ($row=db_fetch_assoc($q)) {
                $stored=json_decode($row['request_context_json'],true);
                if (!is_array($stored) || !isset($stored['payload'])) { pay_glb_001_native_producer_finish(false,$error);return false; }
                $prior=get_approval_draft_for_update((int)$row['effect_id']);
                if ($stored['payload']['selection_id']===$selection_id && (!$prior || in_array((int)$prior['status'],array(APPROVAL_STATUS_PENDING,APPROVAL_STATUS_APPROVED),true))) {
                    $error='native_approval_selection_already_submitted';pay_glb_001_native_producer_finish(false,$error);return false;
                }
            }
            $GLOBALS['pay_glb_001_native_approval_owner']=array('action'=>'submit','request'=>$req,'payload'=>$d,'context'=>$c,'policy'=>$policy);
            $service=new ApprovalWorkflow((int)$_SESSION['wa_current_user']->access,$p['actor_user_id']);
            $result=$service->submit(ST_PAYROLL_PERIOD,$d,(float)$d['total_net'],array('currency'=>$d['currency'],'summary'=>'Governed native payroll revision','date'=>date('Y-m-d')));
            if (!is_array($result) || $result['status']!=='pending' || empty($result['draft_id'])) {
                $error='native_approval_central_submission_failed';pay_glb_001_native_producer_finish(false,$error);return false;
            }
        }
        if (pay_glb_001_native_business_admission($req,$error)===false || !pay_glb_001_native_producer_finish(true,$error)) {
            if (!empty($GLOBALS['transaction_level'])) pay_glb_001_native_producer_finish(false,$error);return false;
        }
        return $result;
    } catch (Exception $exception) { $error='native_approval_submission_failed';pay_glb_001_native_producer_finish(false,$error);return false;
    } catch (Throwable $exception) { $error='native_approval_submission_failed';pay_glb_001_native_producer_finish(false,$error);return false;
    } finally { unset($GLOBALS['pay_glb_001_native_approval_owner']); }
}

/** Execute real manual central approve/reject/cancel under current source and the actual outer transaction. */
function pay_glb_001_native_approval_act($draft_id,$action,$comments='',&$error=null)
{
    if (!in_array($action,array('approve','reject','cancel'),true) || !empty($GLOBALS['transaction_level'])
        || isset($GLOBALS['pay_glb_001_native_approval_owner']) || !pay_glb_001_native_selection_begin($error)) return false;
    try {
        $o=pay_glb_001_native_approval_action_context($draft_id,$action,$comments,$error);
        if ($o===false || (int)$o['draft']['status']!==APPROVAL_STATUS_PENDING
            || pay_glb_001_native_business_admission($o['request'],$error)===false) { pay_glb_001_native_producer_finish(false,$error);return false; }
        $o['action']=$action;$GLOBALS['pay_glb_001_native_approval_owner']=$o;
        $service=new ApprovalWorkflow((int)$_SESSION['wa_current_user']->access,(int)$_SESSION['wa_current_user']->user);
        $result=$service->$action((int)$draft_id,$comments);
        $allowed=$action==='approve'?array('approved','pending_next_level','pending_more_approvers'):array($action==='reject'?'rejected':'cancelled');
        if (!is_array($result) || !in_array($result['status'],$allowed,true)
            || pay_glb_001_native_approval_principal($action,$error)===false || pay_glb_001_native_business_admission($o['request'],$error)===false
            || !pay_glb_001_native_producer_finish(true,$error)) {
            if (!empty($GLOBALS['transaction_level'])) pay_glb_001_native_producer_finish(false,$error);
            if ($error===null) $error='native_approval_central_action_failed';return false;
        }
        return $result;
    } catch (Exception $exception) { $error='native_approval_action_failed';pay_glb_001_native_producer_finish(false,$error);return false;
    } catch (Throwable $exception) { $error='native_approval_action_failed';pay_glb_001_native_producer_finish(false,$error);return false;
    } finally { unset($GLOBALS['pay_glb_001_native_approval_owner']); }
}
