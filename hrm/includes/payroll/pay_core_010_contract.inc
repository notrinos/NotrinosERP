<?php
/**
 * PAY-CORE-010 immutable approval/projection cutover contract.
 *
 * Pure helpers only.  No database, approval, GL, payment, bank, tax or filing
 * authority is acquired by this contract.
 */
if (!defined('HRM_PAY_CORE_010_CONTRACT_VERSION')) define('HRM_PAY_CORE_010_CONTRACT_VERSION', '1.0');
if (!defined('HRM_PAY_CORE_010_PROJECTION_CONTRACT_VERSION')) define('HRM_PAY_CORE_010_PROJECTION_CONTRACT_VERSION', 'legacy-payslip-v1');
if (!defined('HRM_PAY_CORE_010_GL_AUTHORITY_ALLOWED')) define('HRM_PAY_CORE_010_GL_AUTHORITY_ALLOWED', false);
if (!defined('HRM_PAY_CORE_010_PAYMENT_AUTHORITY_ALLOWED')) define('HRM_PAY_CORE_010_PAYMENT_AUTHORITY_ALLOWED', false);
if (!defined('HRM_PAY_CORE_010_BANK_AUTHORITY_ALLOWED')) define('HRM_PAY_CORE_010_BANK_AUTHORITY_ALLOWED', false);
if (!defined('HRM_PAY_CORE_010_STATUTORY_AUTHORITY_ALLOWED')) define('HRM_PAY_CORE_010_STATUTORY_AUTHORITY_ALLOWED', false);
if (!defined('HRM_PAY_CORE_010_DUAL_AUTHORITY_ALLOWED')) define('HRM_PAY_CORE_010_DUAL_AUTHORITY_ALLOWED', false);

function hrm_pay_core_010_canonicalize($value)
{
    if (!is_array($value)) return $value;
    $keys=array_keys($value);$assoc=$keys!==range(0,count($value)-1);
    if ($assoc) ksort($value,SORT_STRING);
    foreach($value as $k=>$v)$value[$k]=hrm_pay_core_010_canonicalize($v);
    return $value;
}
function hrm_pay_core_010_hash($value)
{
    $json=json_encode(hrm_pay_core_010_canonicalize($value),JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE|JSON_PRESERVE_ZERO_FRACTION);
    return is_string($json)?hash('sha256',$json):false;
}
function hrm_pay_core_010_is_sha256($value){return is_string($value)&&preg_match('/^[a-f0-9]{64}$/D',$value)===1;}
function hrm_pay_core_010_token($value,$max=96){$v=trim((string)$value);return $v!==''&&strlen($v)<=$max&&preg_match('/^[A-Za-z0-9][A-Za-z0-9_.:\/-]*$/D',$v)?$v:false;}
function hrm_pay_core_010_decimal($value){$v=trim((string)$value);return preg_match('/^-?(?:0|[1-9][0-9]*)(?:\.[0-9]{1,6})?$/D',$v)?$v:false;}
function hrm_pay_core_010_states(){return array('legacy','shadow','cutover_ready','active_new','fallback');}
function hrm_pay_core_010_allowed_transitions(){return array('legacy'=>array('shadow'),'shadow'=>array('cutover_ready','legacy'),'cutover_ready'=>array('active_new','shadow'),'active_new'=>array('fallback'),'fallback'=>array('shadow'));}
function hrm_pay_core_010_transition_allowed($from,$to){$a=hrm_pay_core_010_allowed_transitions();return isset($a[$from])&&in_array($to,$a[$from],true);}
function hrm_pay_core_010_scope(array $x,&$error=null)
{
    $error=null;foreach(array('payroll_group_id','legal_entity_id') as $k)if(!isset($x[$k])||(int)$x[$k]<=0){$error='scope_invalid';return false;}
    return array('payroll_group_id'=>(int)$x['payroll_group_id'],'legal_entity_id'=>(int)$x['legal_entity_id']);
}
function hrm_pay_core_010_result_set(array $rows,&$error=null)
{
    $error=null;if(empty($rows)){$error='result_set_empty';return false;}$items=array();$gross='0';$ded='0';$net='0';$seen=array();
    foreach($rows as $r){
        if(!is_array($r)||!isset($r['result_id'],$r['result_sha256'],$r['gross_amount'],$r['deduction_amount'],$r['net_amount'])||(int)$r['result_id']<=0||!hrm_pay_core_010_is_sha256($r['result_sha256'])){$error='result_set_row_invalid';return false;}
        $id=(int)$r['result_id'];if(isset($seen[$id])){$error='result_set_duplicate';return false;}$seen[$id]=true;
        foreach(array('gross_amount','deduction_amount','net_amount') as $k)if(hrm_pay_core_010_decimal($r[$k])===false){$error='result_set_amount_invalid';return false;}
        $items[]=array('result_id'=>$id,'result_sha256'=>$r['result_sha256']);
        $gross=number_format((float)$gross+(float)$r['gross_amount'],6,'.','');$ded=number_format((float)$ded+(float)$r['deduction_amount'],6,'.','');$net=number_format((float)$net+(float)$r['net_amount'],6,'.','');
    }
    usort($items,function($a,$b){return $a['result_id']===$b['result_id']?0:($a['result_id']<$b['result_id']?-1:1);});
    $set_sha=hrm_pay_core_010_hash($items);if($set_sha===false){$error='result_set_hash_failed';return false;}
    return array('result_count'=>count($items),'result_set_sha256'=>$set_sha,'gross_amount'=>$gross,'deduction_amount'=>$ded,'net_amount'=>$net,'results'=>$items);
}
function hrm_pay_core_010_projection_descriptor(array $x,&$error=null)
{
    $error=null;$required=array('payroll_period_id','calendar_period_id','payroll_group_id','group_version_id','legal_entity_id','payroll_relationship_id','assignment_id','employee_id_sha256','result_id','result_sha256','header_sha256','detail_set_sha256');
    foreach($required as $k)if(!array_key_exists($k,$x)){$error='projection_field_missing';return false;}
    foreach(array('payroll_period_id','calendar_period_id','payroll_group_id','group_version_id','legal_entity_id','payroll_relationship_id','assignment_id','result_id') as $k)if((int)$x[$k]<=0){$error='projection_scope_invalid';return false;}
    foreach(array('employee_id_sha256','result_sha256','header_sha256','detail_set_sha256') as $k)if(!hrm_pay_core_010_is_sha256($x[$k])){$error='projection_hash_invalid';return false;}
    $p=array('payroll_period_id'=>(int)$x['payroll_period_id'],'calendar_period_id'=>(int)$x['calendar_period_id'],'payroll_group_id'=>(int)$x['payroll_group_id'],'group_version_id'=>(int)$x['group_version_id'],'legal_entity_id'=>(int)$x['legal_entity_id'],'payroll_relationship_id'=>(int)$x['payroll_relationship_id'],'assignment_id'=>(int)$x['assignment_id'],'employee_id_sha256'=>$x['employee_id_sha256'],'result_id'=>(int)$x['result_id'],'result_sha256'=>$x['result_sha256'],'projection_contract_version'=>HRM_PAY_CORE_010_PROJECTION_CONTRACT_VERSION,'header_sha256'=>$x['header_sha256'],'detail_set_sha256'=>$x['detail_set_sha256']);
    $p['projection_sha256']=hrm_pay_core_010_hash($p);return $p['projection_sha256']===false?false:$p;
}
function hrm_pay_core_010_projection_set(array $projection_rows,&$error=null)
{
    $error=null;if(empty($projection_rows)){$error='projection_set_empty';return false;}$items=array();
    foreach($projection_rows as $r){if(!is_array($r)||!isset($r['result_id'],$r['projection_sha256'])||(int)$r['result_id']<=0||!hrm_pay_core_010_is_sha256($r['projection_sha256'])){$error='projection_set_row_invalid';return false;}$items[]=array('result_id'=>(int)$r['result_id'],'projection_sha256'=>$r['projection_sha256']);}
    usort($items,function($a,$b){return $a['result_id']===$b['result_id']?0:($a['result_id']<$b['result_id']?-1:1);});$sha=hrm_pay_core_010_hash($items);return $sha===false?false:array('projection_count'=>count($items),'projection_set_sha256'=>$sha,'items'=>$items);
}
function hrm_pay_core_010_parity(array $x,&$error=null)
{
    $error=null;foreach(array('payroll_period_id','payroll_group_id','group_version_id','legal_entity_id','mismatch_count','reviewer_id') as $k)if(!isset($x[$k])||(int)$x[$k]<($k==='mismatch_count'?0:1)){$error='parity_scope_invalid';return false;}
    foreach(array('result_set_sha256','candidate_projection_set_sha256','legacy_projection_set_sha256','evidence_sha256') as $k)if(!isset($x[$k])||!hrm_pay_core_010_is_sha256($x[$k])){$error='parity_hash_invalid';return false;}
    $status=(string)($x['parity_status']??'');if(!in_array($status,array('match','mismatch'),true)||(($status==='match')!==((int)$x['mismatch_count']===0))){$error='parity_status_invalid';return false;}
    $p=array('payroll_period_id'=>(int)$x['payroll_period_id'],'payroll_group_id'=>(int)$x['payroll_group_id'],'group_version_id'=>(int)$x['group_version_id'],'legal_entity_id'=>(int)$x['legal_entity_id'],'result_set_sha256'=>$x['result_set_sha256'],'candidate_projection_set_sha256'=>$x['candidate_projection_set_sha256'],'legacy_projection_set_sha256'=>$x['legacy_projection_set_sha256'],'parity_status'=>$status,'mismatch_count'=>(int)$x['mismatch_count'],'evidence_sha256'=>$x['evidence_sha256'],'reviewer_id'=>(int)$x['reviewer_id']);
    $p['parity_sha256']=hrm_pay_core_010_hash($p);return $p['parity_sha256']===false?false:$p;
}
function hrm_pay_core_010_transition(array $x,&$error=null)
{
    $error=null;foreach(array('payroll_group_id','legal_entity_id','sequence_no','expected_sequence_no','actor_id') as $k)if(!isset($x[$k])||(int)$x[$k]<($k==='expected_sequence_no'?0:1)){$error='transition_scope_invalid';return false;}
    $from=(string)($x['from_state']??'');$to=(string)($x['to_state']??'');if(!in_array($from,hrm_pay_core_010_states(),true)||!hrm_pay_core_010_transition_allowed($from,$to)||((int)$x['sequence_no']!==(int)$x['expected_sequence_no']+1)){$error='transition_denied';return false;}
    $command=hrm_pay_core_010_token($x['command_key']??'',128);if($command===false){$error='command_key_invalid';return false;}
    $pred=$x['predecessor_transition_sha256']??null;if($pred!==null&&!hrm_pay_core_010_is_sha256($pred)){$error='predecessor_hash_invalid';return false;}
    if($from==='legacy'&&$pred!==null){$error='legacy_predecessor_must_be_null';return false;}if($from!=='legacy'&&$pred===null){$error='predecessor_required';return false;}
    $parity=$x['parity_sha256']??null;if(in_array($to,array('cutover_ready','active_new'),true)&&!hrm_pay_core_010_is_sha256($parity)){$error='fresh_parity_required';return false;}
    $t=array('payroll_group_id'=>(int)$x['payroll_group_id'],'legal_entity_id'=>(int)$x['legal_entity_id'],'sequence_no'=>(int)$x['sequence_no'],'from_state'=>$from,'to_state'=>$to,'expected_sequence_no'=>(int)$x['expected_sequence_no'],'result_contract_version'=>HRM_PAY_CORE_010_CONTRACT_VERSION,'parity_sha256'=>$parity,'command_key'=>$command,'predecessor_transition_sha256'=>$pred,'actor_id'=>(int)$x['actor_id']);
    $t['command_sha256']=hrm_pay_core_010_hash(array('scope'=>array($t['payroll_group_id'],$t['legal_entity_id']),'from'=>$from,'to'=>$to,'expected_sequence_no'=>$t['expected_sequence_no'],'parity_sha256'=>$parity,'command_key'=>$command,'actor_id'=>$t['actor_id']));$t['transition_sha256']=hrm_pay_core_010_hash($t);return ($t['command_sha256']===false||$t['transition_sha256']===false)?false:$t;
}
function hrm_pay_core_010_approval_intent(array $x,&$error=null)
{
    $error=null;$required=array('payroll_period_id','calendar_period_id','payroll_group_id','group_version_id','legal_entity_id','result_set_sha256','result_count','gross_amount','deduction_amount','net_amount','projection_set_sha256','cutover_transition_sha256','command_key','maker_id');foreach($required as $k)if(!array_key_exists($k,$x)){$error='approval_intent_field_missing';return false;}
    foreach(array('payroll_period_id','calendar_period_id','payroll_group_id','group_version_id','legal_entity_id','result_count','maker_id') as $k)if((int)$x[$k]<=0){$error='approval_intent_scope_invalid';return false;}
    foreach(array('result_set_sha256','projection_set_sha256','cutover_transition_sha256') as $k)if(!hrm_pay_core_010_is_sha256($x[$k])){$error='approval_intent_hash_invalid';return false;}
    foreach(array('gross_amount','deduction_amount','net_amount') as $k)if(hrm_pay_core_010_decimal($x[$k])===false){$error='approval_intent_amount_invalid';return false;}
    $cmd=hrm_pay_core_010_token($x['command_key'],128);if($cmd===false){$error='approval_command_invalid';return false;}
    $p=array('payroll_period_id'=>(int)$x['payroll_period_id'],'calendar_period_id'=>(int)$x['calendar_period_id'],'payroll_group_id'=>(int)$x['payroll_group_id'],'group_version_id'=>(int)$x['group_version_id'],'legal_entity_id'=>(int)$x['legal_entity_id'],'result_set_sha256'=>$x['result_set_sha256'],'result_count'=>(int)$x['result_count'],'gross_amount'=>$x['gross_amount'],'deduction_amount'=>$x['deduction_amount'],'net_amount'=>$x['net_amount'],'projection_contract_version'=>HRM_PAY_CORE_010_PROJECTION_CONTRACT_VERSION,'projection_set_sha256'=>$x['projection_set_sha256'],'cutover_transition_sha256'=>$x['cutover_transition_sha256'],'command_key'=>$cmd,'maker_id'=>(int)$x['maker_id']);$p['command_sha256']=hrm_pay_core_010_hash($p);$p['intent_sha256']=hrm_pay_core_010_hash(array('command_sha256'=>$p['command_sha256'],'result_set_sha256'=>$p['result_set_sha256'],'projection_set_sha256'=>$p['projection_set_sha256'],'maker_id'=>$p['maker_id']));return $p;
}
function hrm_pay_core_010_approval(array $x,&$error=null)
{
    $error=null;foreach(array('intent_id','payroll_period_id','approval_draft_id','approver_id') as $k)if(!isset($x[$k])||(int)$x[$k]<=0){$error='approval_scope_invalid';return false;}foreach(array('result_set_sha256','projection_set_sha256','intent_sha256') as $k)if(!isset($x[$k])||!hrm_pay_core_010_is_sha256($x[$k])){$error='approval_hash_invalid';return false;}if((int)$x['approver_id']===(int)($x['maker_id']??0)){$error='self_approval_denied';return false;}$decision=(string)($x['decision_token']??'');if($decision!=='approved'){$error='approval_decision_invalid';return false;}$p=array('intent_id'=>(int)$x['intent_id'],'payroll_period_id'=>(int)$x['payroll_period_id'],'result_set_sha256'=>$x['result_set_sha256'],'projection_set_sha256'=>$x['projection_set_sha256'],'intent_sha256'=>$x['intent_sha256'],'approval_draft_id'=>(int)$x['approval_draft_id'],'decision_token'=>'approved','approver_id'=>(int)$x['approver_id']);$p['approval_sha256']=hrm_pay_core_010_hash($p);return $p;
}
?>
