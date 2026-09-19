<?php
/**
 * PAY-CORE-006 pure balance/accumulator contract.
 *
 * No database, request, session, network, filesystem, GL, payment or mutable
 * production state is accessed here. Historical replay consumes only explicit
 * immutable movements and explicit period boundaries.
 */
include_once(dirname(__FILE__).'/pay_core_005_contract.inc');
if (!defined('HRM_PAY_CORE_006_CONTRACT_VERSION')) define('HRM_PAY_CORE_006_CONTRACT_VERSION', '1.0');
if (!defined('HRM_PAY_CORE_006_PRODUCTION_BALANCE_CUTOVER_ALLOWED')) define('HRM_PAY_CORE_006_PRODUCTION_BALANCE_CUTOVER_ALLOWED', false);
if (!defined('HRM_PAY_CORE_006_ACCOUNTING_AUTHORITY_ALLOWED')) define('HRM_PAY_CORE_006_ACCOUNTING_AUTHORITY_ALLOWED', false);
if (!defined('HRM_PAY_CORE_006_PAYMENT_AUTHORITY_ALLOWED')) define('HRM_PAY_CORE_006_PAYMENT_AUTHORITY_ALLOWED', false);
if (!defined('HRM_PAY_CORE_006_HISTORICAL_MUTATION_ALLOWED')) define('HRM_PAY_CORE_006_HISTORICAL_MUTATION_ALLOWED', false);

function hrm_pay_core_006_hash($v){return hrm_pay_core_005_hash($v);}
function hrm_pay_core_006_is_sha256($v){return hrm_pay_core_005_is_sha256($v);}
function hrm_pay_core_006_token($v,$max=128){return hrm_pay_core_005_token($v,$max);}
function hrm_pay_core_006_decimal($v){return hrm_pay_core_005_decimal_string($v);}
function hrm_pay_core_006_add($a,$b){return hrm_pay_core_005_decimal_add($a,$b);}
function hrm_pay_core_006_sub($a,$b){return hrm_pay_core_005_decimal_sub($a,$b);}
function hrm_pay_core_006_negate($a){return hrm_pay_core_005_decimal_sub('0',$a);}
function hrm_pay_core_006_compare($a,$b){return hrm_pay_core_005_decimal_compare($a,$b);}
function hrm_pay_core_006_date($v){return is_string($v)&&preg_match('/^\d{4}-\d{2}-\d{2}$/D',$v)&&date('Y-m-d',strtotime($v))===$v?$v:false;}
function hrm_pay_core_006_nullable_id($v){if($v===null||$v==='')return null;$v=(int)$v;return $v>0?$v:false;}

function hrm_pay_core_006_allowed_movement_kinds(){return array('opening','payroll','leave','loan','adjustment','reversal','correction','reset','expiry','carry_forward');}
function hrm_pay_core_006_allowed_units(){return array('money','hours','days','units');}
function hrm_pay_core_006_accumulator_kinds(){return array('ptd','mtd','qtd','ytd','lifetime');}

function hrm_pay_core_006_definition(array $d,&$error=null)
{
    $error=null;
    $code=isset($d['balance_code'])?hrm_pay_core_006_token($d['balance_code'],64):false;
    $version=(int)($d['version_no']??0);$unit=(string)($d['unit_code']??'');
    $currency=array_key_exists('currency_code',$d)&&$d['currency_code']!==null?strtoupper((string)$d['currency_code']):null;
    $dims=$d['dimensions']??array();$acc=$d['accumulators']??array();$reset=(string)($d['reset_policy']??'none');$expiry=(string)($d['expiry_policy']??'none');
    if(!$code||$version<=0||!in_array($unit,hrm_pay_core_006_allowed_units(),true)){$error='definition_identity_invalid';return false;}
    if($unit==='money'&&!is_string($currency)){$error='currency_required';return false;}
    if($unit!=='money'&&$currency!==null){$error='currency_not_allowed';return false;}
    if($currency!==null&&!preg_match('/^[A-Z]{3}$/D',$currency)){$error='currency_invalid';return false;}
    $allowed_dims=array('legal_entity','payroll_relationship','person','worker','assignment','calendar_period','jurisdiction','leave_type','loan');
    if(!is_array($dims)||array_diff($dims,$allowed_dims)||count($dims)!==count(array_unique($dims))){$error='dimensions_invalid';return false;}sort($dims,SORT_STRING);
    if(!in_array('legal_entity',$dims,true)||!in_array('payroll_relationship',$dims,true)){$error='mandatory_dimensions_missing';return false;}
    if(!is_array($acc)||array_diff($acc,hrm_pay_core_006_accumulator_kinds())||count($acc)!==count(array_unique($acc))){$error='accumulators_invalid';return false;}sort($acc,SORT_STRING);
    if(!in_array($reset,array('none','period','month','quarter','year','anniversary','explicit'),true)){$error='reset_policy_invalid';return false;}
    if(!in_array($expiry,array('none','fixed_date','age_days','explicit'),true)){$error='expiry_policy_invalid';return false;}
    $reset_month=array_key_exists('reset_month',$d)&&$d['reset_month']!==null?(int)$d['reset_month']:null;
    $reset_day=array_key_exists('reset_day',$d)&&$d['reset_day']!==null?(int)$d['reset_day']:null;
    if($reset==='anniversary'){
        if($reset_month===null||$reset_day===null||!checkdate($reset_month,$reset_day,2000)){$error='anniversary_boundary_invalid';return false;}
    } elseif($reset_month!==null||$reset_day!==null){$error='reset_boundary_not_allowed';return false;}
    $expiry_date=array_key_exists('expiry_date',$d)&&$d['expiry_date']!==null?hrm_pay_core_006_date((string)$d['expiry_date']):null;
    $expiry_days=array_key_exists('expiry_days',$d)&&$d['expiry_days']!==null?(int)$d['expiry_days']:null;
    if($expiry==='fixed_date'&&$expiry_date===null){$error='expiry_date_required';return false;}
    if($expiry==='age_days'&&($expiry_days===null||$expiry_days<=0||$expiry_days>36600)){$error='expiry_days_invalid';return false;}
    if($expiry!=='fixed_date'&&$expiry_date!==null){$error='expiry_date_not_allowed';return false;}
    if($expiry!=='age_days'&&$expiry_days!==null){$error='expiry_days_not_allowed';return false;}
    $from=hrm_pay_core_006_date((string)($d['effective_from']??''));
    $to=array_key_exists('effective_to',$d)&&$d['effective_to']!==null?hrm_pay_core_006_date((string)$d['effective_to']):null;
    if(!$from||($to!==null&&!$to)||($to!==null&&$to<$from)||($expiry_date!==null&&$expiry_date<$from)){$error='effective_dates_invalid';return false;}
    $out=array('balance_code'=>$code,'version_no'=>$version,'unit_code'=>$unit,'currency_code'=>$currency,'dimensions'=>$dims,'accumulators'=>$acc,'reset_policy'=>$reset,'reset_month'=>$reset_month,'reset_day'=>$reset_day,'expiry_policy'=>$expiry,'expiry_date'=>$expiry_date,'expiry_days'=>$expiry_days,'carry_forward_allowed'=>!empty($d['carry_forward_allowed']),'effective_from'=>$from,'effective_to'=>$to);
    $out['definition_sha256']=hrm_pay_core_006_hash($out);return $out;
}

function hrm_pay_core_006_scope(array $s,&$error=null)
{
    $error=null;
    foreach(array('legal_entity_id','payroll_relationship_id') as $k)if(empty($s[$k])||(int)$s[$k]<=0){$error='scope_'.$k.'_invalid';return false;}
    $out=array('legal_entity_id'=>(int)$s['legal_entity_id'],'payroll_relationship_id'=>(int)$s['payroll_relationship_id']);
    foreach(array('person_id','worker_id','assignment_id','calendar_period_id','leave_type_id','loan_id') as $k){$v=hrm_pay_core_006_nullable_id($s[$k]??null);if($v===false){$error='scope_'.$k.'_invalid';return false;}$out[$k]=$v;}
    $out['jurisdiction_code']=array_key_exists('jurisdiction_code',$s)&&$s['jurisdiction_code']!==null?(string)$s['jurisdiction_code']:null;
    if($out['jurisdiction_code']!==null&&!hrm_pay_core_006_token($out['jurisdiction_code'],64)){$error='scope_jurisdiction_invalid';return false;}
    $out['scope_sha256']=hrm_pay_core_006_hash($out);return $out;
}

function hrm_pay_core_006_movement(array $m,&$error=null)
{
    $error=null;$scope=hrm_pay_core_006_scope($m['scope']??array(),$se);if($scope===false){$error=$se;return false;}
    $kind=(string)($m['movement_kind']??'');$source_system=hrm_pay_core_006_token($m['source_system']??'',32);$source_type=hrm_pay_core_006_token($m['source_type']??'',32);$source_key=hrm_pay_core_006_token($m['source_event_key']??'',128);
    $source_sha=$m['source_event_sha256']??'';$def_sha=$m['definition_sha256']??'';$effective=hrm_pay_core_006_date((string)($m['effective_date']??''));$recognition=hrm_pay_core_006_date((string)($m['recognition_date']??''));
    if(!in_array($kind,hrm_pay_core_006_allowed_movement_kinds(),true)||!$source_system||!$source_type||!$source_key||!hrm_pay_core_006_is_sha256($source_sha)||!hrm_pay_core_006_is_sha256($def_sha)||!$effective||!$recognition){$error='movement_identity_invalid';return false;}
    try{$amount=hrm_pay_core_006_decimal((string)($m['amount']??''));}catch(Throwable $e){$error='movement_amount_invalid';return false;}
    $currency=array_key_exists('currency_code',$m)&&$m['currency_code']!==null?strtoupper((string)$m['currency_code']):null;$unit=(string)($m['unit_code']??'');
    if(!in_array($unit,hrm_pay_core_006_allowed_units(),true)||($currency!==null&&!preg_match('/^[A-Z]{3}$/D',$currency))||($unit==='money'&&$currency===null)||($unit!=='money'&&$currency!==null)){$error='movement_unit_invalid';return false;}
    $pred=array_key_exists('predecessor_movement_sha256',$m)&&$m['predecessor_movement_sha256']!==null?$m['predecessor_movement_sha256']:null;
    if($pred!==null&&!hrm_pay_core_006_is_sha256($pred)){$error='movement_predecessor_invalid';return false;}
    if(in_array($kind,array('reversal','correction'),true)&&$pred===null){$error='lineage_required';return false;}
    $out=array('definition_sha256'=>$def_sha,'scope'=>$scope,'source_system'=>$source_system,'source_type'=>$source_type,'source_event_key'=>$source_key,'source_event_sha256'=>$source_sha,'movement_kind'=>$kind,'effective_date'=>$effective,'recognition_date'=>$recognition,'currency_code'=>$currency,'unit_code'=>$unit,'amount'=>$amount,'predecessor_movement_sha256'=>$pred);
    $out['movement_sha256']=hrm_pay_core_006_hash($out);return $out;
}

function hrm_pay_core_006_period_boundaries($date,array $period,&$error=null)
{
    $error=null;$d=hrm_pay_core_006_date($date);$ps=hrm_pay_core_006_date((string)($period['period_start']??''));$pe=hrm_pay_core_006_date((string)($period['period_end']??''));
    if(!$d||!$ps||!$pe||$ps>$pe||$d<$ps||$d>$pe){$error='period_boundary_invalid';return false;}
    $y=(int)substr($d,0,4);$m=(int)substr($d,5,2);$q=(int)floor(($m-1)/3)*3+1;
    return array('ptd'=>array($ps,$pe),'mtd'=>array(sprintf('%04d-%02d-01',$y,$m),date('Y-m-t',strtotime($d))),'qtd'=>array(sprintf('%04d-%02d-01',$y,$q),date('Y-m-t',strtotime(sprintf('%04d-%02d-01',$y,$q+2)))),'ytd'=>array(sprintf('%04d-01-01',$y),sprintf('%04d-12-31',$y)),'lifetime'=>array(null,null));
}

function hrm_pay_core_006_replay(array $movements,$as_of,array $period,&$error=null)
{
    $error=null;$as=hrm_pay_core_006_date($as_of);$bounds=hrm_pay_core_006_period_boundaries($as,$period,$be);if(!$as||$bounds===false){$error=$be?:'as_of_invalid';return false;}
    $normalized=array();$seen=array();$events=array();
    foreach($movements as $m){$n=hrm_pay_core_006_movement($m,$me);if($n===false){$error=$me;return false;}if(isset($seen[$n['movement_sha256']])){$error='duplicate_movement';return false;}$event_key=$n['definition_sha256'].'|'.$n['scope']['payroll_relationship_id'].'|'.$n['source_system'].'|'.$n['source_event_key'];if(isset($events[$event_key])&&!hash_equals($events[$event_key],$n['movement_sha256'])){$error='duplicate_source_event_conflict';return false;}$events[$event_key]=$n['movement_sha256'];$seen[$n['movement_sha256']]=true;$normalized[]=$n;}
    usort($normalized,function($a,$b){foreach(array('effective_date','recognition_date','source_system','source_type','source_event_key','movement_sha256') as $k){$c=strcmp((string)$a[$k],(string)$b[$k]);if($c!==0)return $c;}return 0;});
    $tot=array('ptd'=>'0','mtd'=>'0','qtd'=>'0','ytd'=>'0','lifetime'=>'0');$included=array();
    foreach($normalized as $n){if($n['recognition_date']>$as)continue;$included[]=$n['movement_sha256'];$tot['lifetime']=hrm_pay_core_006_add($tot['lifetime'],$n['amount']);foreach(array('ptd','mtd','qtd','ytd') as $k)if($n['effective_date']>=$bounds[$k][0]&&$n['effective_date']<=$bounds[$k][1])$tot[$k]=hrm_pay_core_006_add($tot[$k],$n['amount']);}
    foreach($tot as $k=>$v)$tot[$k]=hrm_pay_core_006_decimal($v);
    return array('as_of_date'=>$as,'period_boundaries'=>$bounds,'balances'=>$tot,'movement_set_sha256'=>hrm_pay_core_006_hash($included),'ordered_movement_sha256'=>$included,'replay_sha256'=>hrm_pay_core_006_hash(array('as_of'=>$as,'bounds'=>$bounds,'balances'=>$tot,'movements'=>$included)));
}

function hrm_pay_core_006_reversal(array $original,$source_key,$source_sha,$recognition_date,&$error=null)
{
    $o=hrm_pay_core_006_movement($original,$oe);if($o===false){$error=$oe;return false;}
    $r=$original;$r['movement_kind']='reversal';$r['source_event_key']=$source_key;$r['source_event_sha256']=$source_sha;$r['recognition_date']=$recognition_date;$r['amount']=hrm_pay_core_006_negate($o['amount']);$r['predecessor_movement_sha256']=$o['movement_sha256'];return hrm_pay_core_006_movement($r,$error);
}

function hrm_pay_core_006_opening_from_inventory(array $inventory,&$error=null)
{
    $error=null;$state=(string)($inventory['provenance_state']??'');
    if(!in_array($state,array('exact','aggregate_only','provenance_gap'),true)){$error='opening_provenance_invalid';return false;}
    if(!hrm_pay_core_006_is_sha256($inventory['source_sha256']??'')){$error='opening_source_sha_invalid';return false;}
    $inventory_sha=$inventory['inventory_sha256']??null;if($inventory_sha!==null&&!hrm_pay_core_006_is_sha256($inventory_sha)){$error='opening_inventory_sha_invalid';return false;}
    $scope=$inventory['scope']??array();$as=(string)($inventory['as_of_date']??'');$identity=$inventory_sha?:hrm_pay_core_006_hash(array($inventory['source_type']??'aggregate',$inventory['source_sha256'],$scope,$as,$inventory['amount']??'0'));
    $m=array('definition_sha256'=>$inventory['definition_sha256']??'','scope'=>$scope,'source_system'=>'legacy','source_type'=>(string)($inventory['source_type']??'aggregate'),'source_event_key'=>'opening:'.substr($identity,0,48),'source_event_sha256'=>$inventory['source_sha256'],'movement_kind'=>'opening','effective_date'=>$as,'recognition_date'=>$as,'currency_code'=>$inventory['currency_code']??null,'unit_code'=>$inventory['unit_code']??'units','amount'=>$inventory['amount']??'0','predecessor_movement_sha256'=>null);
    return hrm_pay_core_006_movement($m,$error);
}

/** Return true only when an explicitly governed reset/expiry boundary is due. */
function hrm_pay_core_006_boundary_due(array $definition,$kind,$boundary_date,array $context=array(),&$error=null)
{
    $error=null;$d=hrm_pay_core_006_definition($definition,$de);if($d===false){$error=$de;return false;}$date=hrm_pay_core_006_date($boundary_date);if(!$date||!in_array($kind,array('reset','expiry'),true)){$error='boundary_identity_invalid';return false;}
    $policy=$kind==='reset'?$d['reset_policy']:$d['expiry_policy'];if($policy==='none'){$error='boundary_policy_none';return false;}
    if($kind==='reset'){
        $m=(int)substr($date,5,2);$day=(int)substr($date,8,2);
        if($policy==='month'&&$day!==1){$error='reset_boundary_not_due';return false;}
        if($policy==='quarter'&&($day!==1||!in_array($m,array(1,4,7,10),true))){$error='reset_boundary_not_due';return false;}
        if($policy==='year'&&substr($date,5)!=='01-01'){$error='reset_boundary_not_due';return false;}
        if($policy==='anniversary'&&($m!==$d['reset_month']||$day!==$d['reset_day'])){$error='reset_boundary_not_due';return false;}
        if($policy==='period'){$ps=hrm_pay_core_006_date((string)($context['period_start']??''));$pe=hrm_pay_core_006_date((string)($context['period_end']??''));if(!$ps||!$pe||$ps>$pe||$date!==$ps){$error='reset_period_boundary_invalid';return false;}}
    } else {
        if($policy==='fixed_date'&&$date!==$d['expiry_date']){$error='expiry_boundary_not_due';return false;}
        if($policy==='age_days'){$origin=hrm_pay_core_006_date((string)($context['origin_date']??''));if(!$origin){$error='expiry_origin_required';return false;}$expected=date('Y-m-d',strtotime($origin.' +'.$d['expiry_days'].' days'));if($date!==$expected){$error='expiry_boundary_not_due';return false;}}
    }
    if($policy==='explicit'&&!hrm_pay_core_006_is_sha256($context['boundary_evidence_sha256']??'')){$error='explicit_boundary_evidence_required';return false;}
    return true;
}

/** Materialize a deterministic zeroing movement; accumulator windows themselves remain read projections. */
function hrm_pay_core_006_boundary_movement(array $definition,array $scope,$kind,$boundary_date,$current_balance,$source_event_key,$source_event_sha256,array $context=array(),&$error=null)
{
    $error=null;$d=hrm_pay_core_006_definition($definition,$de);if($d===false){$error=$de;return false;}if(!hrm_pay_core_006_boundary_due($definition,$kind,$boundary_date,$context,$be)){$error=$be;return false;}
    try{$amount=hrm_pay_core_006_negate($current_balance);}catch(Throwable $e){$error='boundary_amount_invalid';return false;}
    return hrm_pay_core_006_movement(array('definition_sha256'=>$d['definition_sha256'],'scope'=>$scope,'source_system'=>'pay_core_006','source_type'=>$kind.'_boundary','source_event_key'=>$source_event_key,'source_event_sha256'=>$source_event_sha256,'movement_kind'=>$kind,'effective_date'=>$boundary_date,'recognition_date'=>$boundary_date,'currency_code'=>$d['currency_code'],'unit_code'=>$d['unit_code'],'amount'=>$amount,'predecessor_movement_sha256'=>null),$error);
}
?>
