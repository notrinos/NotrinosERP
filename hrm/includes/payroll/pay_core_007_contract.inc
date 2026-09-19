<?php
/**
 * PAY-CORE-007 pure retro/correction/reversal contract.
 *
 * This module has no database, request, session, filesystem, network, GL,
 * payment, bank-file, statutory-filing or production-payroll authority.
 * Every decision consumes explicit immutable evidence and uses PAY-CORE-005
 * fixed-point arithmetic. Missing historical evidence remains an explicit gap.
 */
include_once(dirname(__FILE__).'/pay_core_005_runtime.inc');
include_once(dirname(__FILE__).'/pay_core_006_contract.inc');
if (!defined('HRM_PAY_CORE_007_CONTRACT_VERSION')) define('HRM_PAY_CORE_007_CONTRACT_VERSION', '1.0');
if (!defined('HRM_PAY_CORE_007_PRODUCTION_RETRO_CUTOVER_ALLOWED')) define('HRM_PAY_CORE_007_PRODUCTION_RETRO_CUTOVER_ALLOWED', false);
if (!defined('HRM_PAY_CORE_007_HISTORICAL_MUTATION_ALLOWED')) define('HRM_PAY_CORE_007_HISTORICAL_MUTATION_ALLOWED', false);
if (!defined('HRM_PAY_CORE_007_PAYMENT_AUTHORITY_ALLOWED')) define('HRM_PAY_CORE_007_PAYMENT_AUTHORITY_ALLOWED', false);
if (!defined('HRM_PAY_CORE_007_ACCOUNTING_AUTHORITY_ALLOWED')) define('HRM_PAY_CORE_007_ACCOUNTING_AUTHORITY_ALLOWED', false);
if (!defined('HRM_PAY_CORE_007_STATUTORY_AUTHORITY_ALLOWED')) define('HRM_PAY_CORE_007_STATUTORY_AUTHORITY_ALLOWED', false);

function hrm_pay_core_007_hash($v){return hrm_pay_core_005_hash($v);}
function hrm_pay_core_007_is_sha256($v){return hrm_pay_core_005_is_sha256($v);}
function hrm_pay_core_007_token($v,$max=128){return hrm_pay_core_005_token($v,$max);}
function hrm_pay_core_007_date($v){return is_string($v)&&preg_match('/^\d{4}-\d{2}-\d{2}$/D',$v)&&date('Y-m-d',strtotime($v))===$v?$v:false;}
function hrm_pay_core_007_decimal($v){return hrm_pay_core_005_decimal_string($v);}
function hrm_pay_core_007_add($a,$b){return hrm_pay_core_005_decimal_add($a,$b);}
function hrm_pay_core_007_sub($a,$b){return hrm_pay_core_005_decimal_sub($a,$b);}
function hrm_pay_core_007_abs($a){return hrm_pay_core_005_decimal_abs($a);}
function hrm_pay_core_007_compare($a,$b){return hrm_pay_core_005_decimal_compare($a,$b);}
function hrm_pay_core_007_nullable_id($v){if($v===null||$v==='')return null;$v=(int)$v;return $v>0?$v:false;}
function hrm_pay_core_007_source_families(){return array('employment_assignment_effective_change','approved_payroll_input_correction','element_version_correction','rule_jurisdiction_retroactive_change','leave_time_overtime_correction','prior_payroll_correction_reversal');}
function hrm_pay_core_007_plan_types(){return array('correction','reversal');}
function hrm_pay_core_007_downstream_domains(){return array('payment','accounting','statutory');}
function hrm_pay_core_007_provenance_states(){return array('complete','gap');}

function hrm_pay_core_007_scope(array $x,&$error=null)
{
    $error=null;foreach(array('legal_entity_id','payroll_relationship_id') as $k)if(empty($x[$k])||(int)$x[$k]<=0){$error='scope_'.$k.'_invalid';return false;}
    $a=hrm_pay_core_007_nullable_id($x['assignment_id']??null);if($a===false){$error='scope_assignment_id_invalid';return false;}
    $w=hrm_pay_core_007_nullable_id($x['worker_id']??null);if($w===false){$error='scope_worker_id_invalid';return false;}
    $p=hrm_pay_core_007_nullable_id($x['person_id']??null);if($p===false){$error='scope_person_id_invalid';return false;}
    $out=array('legal_entity_id'=>(int)$x['legal_entity_id'],'payroll_relationship_id'=>(int)$x['payroll_relationship_id'],'assignment_id'=>$a,'worker_id'=>$w,'person_id'=>$p);
    $out['scope_sha256']=hrm_pay_core_007_hash($out);return $out;
}

/** A retro event is governed evidence, never an implicit mutation side effect. */
function hrm_pay_core_007_event(array $x,&$error=null)
{
    $error=null;$scope=hrm_pay_core_007_scope($x,$error);if($scope===false)return false;
    $family=(string)($x['source_family']??'');$system=hrm_pay_core_007_token($x['source_system']??'',64);$type=hrm_pay_core_007_token($x['source_type']??'',64);$key=hrm_pay_core_007_token($x['source_key']??'',160);
    $from=hrm_pay_core_007_date((string)($x['effective_from']??''));$to=array_key_exists('effective_to',$x)&&$x['effective_to']!==null?hrm_pay_core_007_date((string)$x['effective_to']):null;
    $source_sha=(string)($x['source_sha256']??'');$evidence_sha=(string)($x['evidence_sha256']??'');
    if(!in_array($family,hrm_pay_core_007_source_families(),true)||!$system||!$type||!$key||!$from||($to!==null&&!$to)||($to!==null&&$to<$from)||!hrm_pay_core_007_is_sha256($source_sha)||!hrm_pay_core_007_is_sha256($evidence_sha)){$error='retro_event_invalid';return false;}
    $out=array('scope'=>$scope,'source_family'=>$family,'source_system'=>$system,'source_type'=>$type,'source_key'=>$key,'source_sha256'=>$source_sha,'effective_from'=>$from,'effective_to'=>$to,'evidence_sha256'=>$evidence_sha);
    $out['event_sha256']=hrm_pay_core_007_hash($out);return $out;
}

/** Stable affected-result discovery over caller-supplied immutable result/period descriptors. */
function hrm_pay_core_007_discover_affected_results(array $event,array $candidates,&$error=null)
{
    $error=null;$e=hrm_pay_core_007_event($event,$error);if($e===false)return false;$out=array();$seen=array();
    foreach($candidates as $r){
        foreach(array('result_id','calendar_period_id','legal_entity_id','payroll_relationship_id','period_start','period_end','result_sha256','snapshot_sha256') as $k)if(!array_key_exists($k,$r)){$error='affected_result_descriptor_incomplete';return false;}
        $rid=(int)$r['result_id'];$pid=(int)$r['calendar_period_id'];$ps=hrm_pay_core_007_date((string)$r['period_start']);$pe=hrm_pay_core_007_date((string)$r['period_end']);$a=hrm_pay_core_007_nullable_id($r['assignment_id']??null);
        if($rid<=0||$pid<=0||!$ps||!$pe||$pe<$ps||!hrm_pay_core_007_is_sha256($r['result_sha256'])||!hrm_pay_core_007_is_sha256($r['snapshot_sha256'])||$a===false){$error='affected_result_descriptor_invalid';return false;}
        if(isset($seen[$rid])){$error='duplicate_candidate_result';return false;}$seen[$rid]=true;
        if((int)$r['legal_entity_id']!==$e['scope']['legal_entity_id']||(int)$r['payroll_relationship_id']!==$e['scope']['payroll_relationship_id'])continue;
        if($e['scope']['assignment_id']!==null&&$a!==null&&$a!==$e['scope']['assignment_id'])continue;
        $event_to=$e['effective_to']??'9999-12-31';if($pe<$e['effective_from']||$ps>$event_to)continue;
        $d=array('result_id'=>$rid,'calendar_period_id'=>$pid,'period_start'=>$ps,'period_end'=>$pe,'assignment_id'=>$a,'result_sha256'=>(string)$r['result_sha256'],'snapshot_sha256'=>(string)$r['snapshot_sha256']);
        $d['discovery_sha256']=hrm_pay_core_007_hash(array('event_sha256'=>$e['event_sha256'],'result'=>$d));$out[]=$d;
    }
    usort($out,function($a,$b){foreach(array('period_start','period_end') as $k){$c=strcmp($a[$k],$b[$k]);if($c!==0)return $c;}$c=$a['calendar_period_id']<=>$b['calendar_period_id'];return $c!==0?$c:strcmp($a['result_sha256'],$b['result_sha256']);});return $out;
}

/** Exact historical context; gap state is valid evidence but is non-executable. */
function hrm_pay_core_007_historical_context(array $x,&$error=null)
{
    $error=null;foreach(array('affected_result_id','original_result_id') as $k)if(empty($x[$k])||(int)$x[$k]<=0){$error='historical_context_identity_invalid';return false;}
    foreach(array('original_result_sha256','original_snapshot_sha256','approved_inputs_sha256','element_versions_sha256','eligibility_sha256','rule_artifacts_sha256','jurisdiction_resolution_sha256','source_events_sha256','configuration_sha256') as $k)if(!hrm_pay_core_007_is_sha256($x[$k]??'')){$error='historical_context_hash_invalid';return false;}
    $state=(string)($x['provenance_state']??'');$gaps=$x['gap_codes']??array();if(!in_array($state,hrm_pay_core_007_provenance_states(),true)||!is_array($gaps)){$error='historical_context_provenance_invalid';return false;}
    $norm=array();foreach($gaps as $g){$t=hrm_pay_core_007_token($g,64);if(!$t){$error='historical_context_gap_invalid';return false;}$norm[$t]=true;}$norm=array_keys($norm);sort($norm,SORT_STRING);
    if(($state==='complete'&&$norm)||($state==='gap'&&!$norm)){$error='historical_context_gap_state_invalid';return false;}
    $material=$x['recompute_snapshot_material']??null;if($state==='complete'){if(!is_array($material)||!hrm_pay_core_005_validate_snapshot_material($material,$me)){$error='historical_recompute_snapshot_invalid'.(isset($me)?':'.$me:'');return false;}}elseif($material!==null){$error='gap_context_cannot_claim_recompute_snapshot';return false;}
    $out=array('affected_result_id'=>(int)$x['affected_result_id'],'original_result_id'=>(int)$x['original_result_id'],'original_result_sha256'=>(string)$x['original_result_sha256'],'original_snapshot_sha256'=>(string)$x['original_snapshot_sha256'],'approved_inputs_sha256'=>(string)$x['approved_inputs_sha256'],'element_versions_sha256'=>(string)$x['element_versions_sha256'],'eligibility_sha256'=>(string)$x['eligibility_sha256'],'rule_artifacts_sha256'=>(string)$x['rule_artifacts_sha256'],'jurisdiction_resolution_sha256'=>(string)$x['jurisdiction_resolution_sha256'],'source_events_sha256'=>(string)$x['source_events_sha256'],'configuration_sha256'=>(string)$x['configuration_sha256'],'provenance_state'=>$state,'gap_codes'=>$norm,'recompute_snapshot_material'=>$material);
    $out['context_sha256']=hrm_pay_core_007_hash($out);return $out;
}

function hrm_pay_core_007_recalculate(array $context,array $plan,&$error=null)
{
    $error=null;$c=hrm_pay_core_007_historical_context($context,$error);if($c===false)return false;if($c['provenance_state']!=='complete'){$error='historical_provenance_gap';return false;}
    if(!isset($plan['snapshot_sha256'])||!hash_equals($c['recompute_snapshot_material']['snapshot_sha256'],(string)$plan['snapshot_sha256'])){$error='historical_snapshot_binding_mismatch';return false;}
    $bundle=hrm_pay_core_005_execute_plan($plan,$error);if($bundle===false)return false;$bundle['context_sha256']=$c['context_sha256'];$bundle['recalculation_sha256']=hrm_pay_core_007_hash(array('context_sha256'=>$c['context_sha256'],'plan_sha256'=>$bundle['plan_sha256'],'bundle_sha256'=>$bundle['bundle_sha256']));return $bundle;
}

function hrm_pay_core_007_line_map(array $lines,&$error=null)
{
    $error=null;$m=array();foreach($lines as $line){foreach(array('element_version_id','line_kind','currency','amount') as $k)if(!array_key_exists($k,$line)){$error='delta_line_incomplete';return false;}$id=(int)$line['element_version_id'];$kind=hrm_pay_core_007_token($line['line_kind'],32);$cur=strtoupper((string)$line['currency']);if($id<=0||!$kind||!preg_match('/^[A-Z]{3}$/D',$cur)){$error='delta_line_identity_invalid';return false;}try{$amount=hrm_pay_core_007_decimal($line['amount']);}catch(Throwable $t){$error='delta_line_amount_invalid';return false;}$k=sprintf('%020d|%s|%s',$id,$kind,$cur);$m[$k]=isset($m[$k])?hrm_pay_core_007_add($m[$k],$amount):$amount;}ksort($m,SORT_STRING);return $m;
}

/** Delta is recomputed minus accepted original; originals are never replaced. */
function hrm_pay_core_007_compute_deltas(array $original_lines,array $recomputed_lines,&$error=null)
{
    $error=null;$a=hrm_pay_core_007_line_map($original_lines,$error);if($a===false)return false;$b=hrm_pay_core_007_line_map($recomputed_lines,$error);if($b===false)return false;$keys=array_values(array_unique(array_merge(array_keys($a),array_keys($b))));sort($keys,SORT_STRING);$rows=array();$pos='0.000000';$neg='0.000000';
    foreach($keys as $k){list($id,$kind,$cur)=explode('|',$k,3);$oa=$a[$k]??'0.000000';$ra=$b[$k]??'0.000000';$raw=hrm_pay_core_007_sub($ra,$oa);$d=$kind==='deduction'?hrm_pay_core_007_sub('0',$raw):$raw;if(hrm_pay_core_007_compare($d,'0')>0)$pos=hrm_pay_core_007_add($pos,$d);elseif(hrm_pay_core_007_compare($d,'0')<0)$neg=hrm_pay_core_007_add($neg,$d);$r=array('delta_key'=>$k,'element_version_id'=>(int)$id,'line_kind'=>$kind,'currency'=>$cur,'original_amount'=>$oa,'recomputed_amount'=>$ra,'delta_amount'=>$d);$r['delta_sha256']=hrm_pay_core_007_hash($r);$rows[]=$r;}
    $set=hrm_pay_core_007_hash(array_map(function($r){return $r['delta_sha256'];},$rows));return array('rows'=>$rows,'positive_total'=>$pos,'negative_total'=>$neg,'net_delta'=>hrm_pay_core_007_add($pos,$neg),'delta_set_sha256'=>$set);
}

function hrm_pay_core_007_arrears_obligation(array $x,&$error=null)
{
    $error=null;foreach(array('legal_entity_id','payroll_relationship_id') as $k)if(empty($x[$k])||(int)$x[$k]<=0){$error='arrears_scope_invalid';return false;}$cur=strtoupper((string)($x['currency']??''));if(!preg_match('/^[A-Z]{3}$/D',$cur)||!hrm_pay_core_007_is_sha256($x['originating_delta_set_sha256']??'')||!hrm_pay_core_007_is_sha256($x['policy_evidence_sha256']??'')){$error='arrears_evidence_invalid';return false;}
    try{$amt=hrm_pay_core_007_decimal($x['amount']??'');}catch(Throwable $t){$error='arrears_amount_invalid';return false;}if(hrm_pay_core_007_compare($amt,'0')===0){$error='arrears_zero_invalid';return false;}$direction=hrm_pay_core_007_compare($amt,'0')>0?'payable_to_worker':'recoverable_from_worker';$out=array('legal_entity_id'=>(int)$x['legal_entity_id'],'payroll_relationship_id'=>(int)$x['payroll_relationship_id'],'currency'=>$cur,'direction'=>$direction,'originating_delta_set_sha256'=>(string)$x['originating_delta_set_sha256'],'policy_evidence_sha256'=>(string)$x['policy_evidence_sha256'],'initial_amount'=>hrm_pay_core_007_abs($amt));$out['arrears_sha256']=hrm_pay_core_007_hash($out);return $out;
}

function hrm_pay_core_007_arrears_movement(array $arrears,$kind,$amount,$prior_remaining,$source_sha256,$policy_evidence_sha256,&$error=null)
{
    $error=null;if(!in_array($kind,array('bring_forward','recovery','payment','writeoff_authorized','correction'),true)||!hrm_pay_core_007_is_sha256($source_sha256)||!hrm_pay_core_007_is_sha256($policy_evidence_sha256)){$error='arrears_movement_evidence_invalid';return false;}try{$a=hrm_pay_core_007_decimal($amount);$prior=hrm_pay_core_007_decimal($prior_remaining);}catch(Throwable $t){$error='arrears_movement_amount_invalid';return false;}if(hrm_pay_core_007_compare($a,'0')<0||hrm_pay_core_007_compare($a,$prior)>0){$error='arrears_movement_out_of_bounds';return false;}$remaining=hrm_pay_core_007_sub($prior,$a);$out=array('arrears_sha256'=>(string)($arrears['arrears_sha256']??''),'movement_kind'=>$kind,'amount'=>$a,'remaining_amount'=>$remaining,'source_sha256'=>$source_sha256,'policy_evidence_sha256'=>$policy_evidence_sha256);if(!hrm_pay_core_007_is_sha256($out['arrears_sha256'])){$error='arrears_identity_invalid';return false;}$out['movement_sha256']=hrm_pay_core_007_hash($out);return $out;
}

function hrm_pay_core_007_plan(array $x,&$error=null)
{
    $error=null;$type=(string)($x['plan_type']??'');$idem=hrm_pay_core_007_token($x['idempotency_key']??'',128);$reason=hrm_pay_core_007_token($x['reason_code']??'',64);$target=hrm_pay_core_007_nullable_id($x['target_calendar_period_id']??null);$bring=array_key_exists('bring_forward_date',$x)&&$x['bring_forward_date']!==null?hrm_pay_core_007_date((string)$x['bring_forward_date']):null;
    if(empty($x['affected_result_id'])||empty($x['context_id'])||!in_array($type,hrm_pay_core_007_plan_types(),true)||!$idem||!$reason||$target===false||($bring!==null&&!$bring)){$error='retro_plan_identity_invalid';return false;}
    foreach(array('reason_evidence_sha256','historical_plan_sha256','delta_set_sha256') as $k)if(!hrm_pay_core_007_is_sha256($x[$k]??'')){$error='retro_plan_evidence_invalid';return false;}
    if($type==='reversal'&&(!isset($x['predecessor_result_sha256'])||!hrm_pay_core_007_is_sha256($x['predecessor_result_sha256']))){$error='reversal_predecessor_required';return false;}
    $out=array('affected_result_id'=>(int)$x['affected_result_id'],'context_id'=>(int)$x['context_id'],'plan_type'=>$type,'idempotency_key'=>$idem,'reason_code'=>$reason,'reason_evidence_sha256'=>(string)$x['reason_evidence_sha256'],'historical_plan_sha256'=>(string)$x['historical_plan_sha256'],'delta_set_sha256'=>(string)$x['delta_set_sha256'],'bring_forward_date'=>$bring,'target_calendar_period_id'=>$target,'predecessor_result_sha256'=>$x['predecessor_result_sha256']??null);$out['plan_sha256']=hrm_pay_core_007_hash($out);return $out;
}

function hrm_pay_core_007_approval(array $plan,$maker_id,$checker_id,$approval_evidence_sha256,&$error=null)
{
    $error=null;$maker_id=(int)$maker_id;$checker_id=(int)$checker_id;if(empty($plan['plan_sha256'])||!hrm_pay_core_007_is_sha256($plan['plan_sha256'])||$maker_id<=0||$checker_id<=0||$maker_id===$checker_id||!hrm_pay_core_007_is_sha256($approval_evidence_sha256)){$error='approval_separation_or_evidence_invalid';return false;}$out=array('plan_sha256'=>$plan['plan_sha256'],'maker_id'=>$maker_id,'checker_id'=>$checker_id,'approval_evidence_sha256'=>$approval_evidence_sha256);$out['approval_sha256']=hrm_pay_core_007_hash($out);return $out;
}

function hrm_pay_core_007_execution_descriptor(array $plan,array $approval,$executor_id,$original_result_id,$successor_result_id,$successor_result_sha256,$balance_movement_set_sha256,&$error=null)
{
    $error=null;$executor_id=(int)$executor_id;if($executor_id<=0||$executor_id===(int)($approval['maker_id']??0)||$executor_id===(int)($approval['checker_id']??0)||!isset($plan['plan_sha256'],$approval['plan_sha256'])||!hash_equals((string)$plan['plan_sha256'],(string)$approval['plan_sha256'])){$error='execution_separation_invalid';return false;}
    if((int)$original_result_id<=0||(int)$successor_result_id<=0||(int)$original_result_id===(int)$successor_result_id||!hrm_pay_core_007_is_sha256($successor_result_sha256)||($balance_movement_set_sha256!==null&&!hrm_pay_core_007_is_sha256($balance_movement_set_sha256))){$error='execution_lineage_invalid';return false;}
    $out=array('plan_sha256'=>$plan['plan_sha256'],'approval_sha256'=>$approval['approval_sha256']??null,'execution_kind'=>$plan['plan_type'],'original_result_id'=>(int)$original_result_id,'successor_result_id'=>(int)$successor_result_id,'successor_result_sha256'=>$successor_result_sha256,'delta_set_sha256'=>$plan['delta_set_sha256'],'balance_movement_set_sha256'=>$balance_movement_set_sha256,'executor_id'=>$executor_id);if(!hrm_pay_core_007_is_sha256($out['approval_sha256'])){$error='execution_approval_invalid';return false;}$out['execution_sha256']=hrm_pay_core_007_hash($out);return $out;
}

/** Explicit obligations preserve paid/posted/filed history; they never execute downstream actions. */
function hrm_pay_core_007_downstream_obligations($execution_sha256,array $states,&$error=null)
{
    $error=null;if(!hrm_pay_core_007_is_sha256($execution_sha256)){$error='downstream_execution_invalid';return false;}$out=array();$map=array('payment'=>'paid','accounting'=>'posted','statutory'=>'filed');foreach($map as $domain=>$flag){$s=$states[$domain]??array();$done=!empty($s[$flag]);if(!$done)continue;$h=$s['evidence_sha256']??'';if(!hrm_pay_core_007_is_sha256($h)){$error='downstream_'.$domain.'_evidence_required';return false;}$r=array('execution_sha256'=>$execution_sha256,'domain'=>$domain,'obligation_kind'=>'correct_prior_'.$flag,'original_evidence_sha256'=>$h,'obligation_state'=>'open');$r['obligation_sha256']=hrm_pay_core_007_hash($r);$out[]=$r;}return $out;
}

function hrm_pay_core_007_reconciliation($execution_sha256,$source_sha256,$expected,$actual,$evidence_sha256,&$error=null)
{
    $error=null;if(!hrm_pay_core_007_is_sha256($execution_sha256)||!hrm_pay_core_007_is_sha256($source_sha256)||!hrm_pay_core_007_is_sha256($evidence_sha256)){$error='reconciliation_evidence_invalid';return false;}try{$e=hrm_pay_core_007_decimal($expected);$a=hrm_pay_core_007_decimal($actual);}catch(Throwable $t){$error='reconciliation_amount_invalid';return false;}$d=hrm_pay_core_007_sub($a,$e);$out=array('execution_sha256'=>$execution_sha256,'source_sha256'=>$source_sha256,'expected_delta_amount'=>$e,'actual_delta_amount'=>$a,'difference_amount'=>$d,'reconciliation_state'=>hrm_pay_core_007_compare($d,'0')===0?'matched':'mismatch','evidence_sha256'=>$evidence_sha256);$out['reconciliation_sha256']=hrm_pay_core_007_hash($out);return $out;
}

function hrm_pay_core_007_recovery($reconciliation_sha256,$action_token,$source_sha256,$evidence_sha256,&$error=null)
{
    $error=null;$action=hrm_pay_core_007_token($action_token,64);if(!hrm_pay_core_007_is_sha256($reconciliation_sha256)||!$action||!in_array($action,array('retry','forward_correction','reconcile_only','escalate_policy_gap'),true)||!hrm_pay_core_007_is_sha256($source_sha256)||!hrm_pay_core_007_is_sha256($evidence_sha256)){$error='recovery_evidence_invalid';return false;}$out=array('reconciliation_sha256'=>$reconciliation_sha256,'action_token'=>$action,'source_sha256'=>$source_sha256,'evidence_sha256'=>$evidence_sha256);$out['recovery_sha256']=hrm_pay_core_007_hash($out);return $out;
}
?>
