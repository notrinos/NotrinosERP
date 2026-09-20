<?php
/** Pure HRM-TIM-002 overtime-qualification/import contract. No jurisdiction defaults live here. */
include_once(dirname(__FILE__).'/hrm_tim_001_contract.inc');
if (!defined('HRM_TIM_002_CONTRACT_VERSION')) define('HRM_TIM_002_CONTRACT_VERSION','1.0.435');
if (!defined('HRM_TIM_002_IMPORT_NORMALIZATION_VERSION')) define('HRM_TIM_002_IMPORT_NORMALIZATION_VERSION','tim2-csv-v1');
if (!defined('HRM_TIM_002_PAYROLL_ADAPTER_VERSION')) define('HRM_TIM_002_PAYROLL_ADAPTER_VERSION','hrm-tim-002-pay-core-003-v1');

function hrm_tim_002_token($v,$max=128){return hrm_tim_001_token($v,$max);}
function hrm_tim_002_hash($v){return hrm_tim_001_hash($v);}
function hrm_tim_002_date($v){return hrm_tim_001_date($v);}
function hrm_tim_002_is_sha256($v){return hrm_tim_001_is_sha256($v);}
function hrm_tim_002_int_or_null($v,$min=0,$max=10000080){if($v===null||$v==='')return null;if(!is_int($v)&&(!is_string($v)||preg_match('/^[0-9]+$/D',$v)!==1))return false;$v=(int)$v;return $v<$min||$v>$max?false:$v;}
function hrm_tim_002_bool_or_null($v){if($v===null||$v==='')return null;if($v===true||$v===1||$v==='1')return 1;if($v===false||$v===0||$v==='0')return 0;return false;}
function hrm_tim_002_weekdays($v){if(!is_array($v))return false;$o=array();foreach($v as $d){if(!is_int($d)&&(!is_string($d)||preg_match('/^[0-6]$/D',$d)!==1))return false;$d=(int)$d;if($d<0||$d>6||isset($o[$d]))return false;$o[$d]=true;}ksort($o);return array_keys($o);}
function hrm_tim_002_csv_safe_cell($value){$s=(string)$value;if($s!==''&&(preg_match('/^[=+\-@]/u',$s)===1||preg_match('/^[\x00-\x08\x0B\x0C\x0E-\x1F]/',$s)===1))return "'".$s;return $s;}
function hrm_tim_002_source_key($v,$max=128){$t=hrm_tim_002_token($v,$max);return $t!==false&&preg_match('/^[A-Za-z0-9][A-Za-z0-9._:@\/-]*$/D',$t)===1?$t:false;}

/** A policy is explicit evidence. Weekend/week boundaries are nullable unless rules require them. */
function hrm_tim_002_policy_descriptor(array $x,&$error=null)
{
    $error=null;foreach(array('legal_entity_id','policy_code','policy_version','source_kind','source_ref','source_evidence_sha256','effective_from','priority_rank','created_by') as $k)if(!array_key_exists($k,$x)){$error='policy_field_missing';return false;}
    $le=(int)$x['legal_entity_id'];$code=hrm_tim_002_token($x['policy_code'],64);$ver=hrm_tim_002_token($x['policy_version'],64);$kind=hrm_tim_002_token($x['source_kind'],24);$ref=hrm_tim_002_token($x['source_ref'],160);$from=hrm_tim_002_date($x['effective_from']);$to=empty($x['effective_to'])?null:hrm_tim_002_date($x['effective_to']);$priority=(int)$x['priority_rank'];$week_start=hrm_tim_002_int_or_null($x['week_start_day']??null,0,6);$scope=(string)($x['weekly_scope']??'');$weekend=isset($x['weekend_days'])?hrm_tim_002_weekdays($x['weekend_days']):array();
    if($le<=0||$code===false||$ver===false||!in_array($kind,array('employer','pack','collective'),true)||$ref===false||!hrm_tim_002_is_sha256($x['source_evidence_sha256'])||$from===false||($to!==null&&($to===false||$to<$from))||$priority<0||$priority>100000||$week_start===false||($scope!==''&&!in_array($scope,array('assignment','payroll_relationship'),true))||$weekend===false||(int)$x['created_by']<=0){$error='policy_definition_invalid';return false;}
    $p=array('legal_entity_id'=>$le,'policy_code'=>$code,'policy_version'=>$ver,'source_kind'=>$kind,'source_ref'=>$ref,'source_evidence_sha256'=>strtolower($x['source_evidence_sha256']),'effective_from'=>$from,'effective_to'=>$to,'priority_rank'=>$priority,'week_start_day'=>$week_start,'weekly_scope'=>$scope===''?null:$scope,'weekend_days'=>$weekend,'predecessor_policy_id'=>empty($x['predecessor_policy_id'])?null:(int)$x['predecessor_policy_id'],'created_by'=>(int)$x['created_by']);$p['policy_sha256']=hrm_tim_002_hash($p);return $p;
}
function hrm_tim_002_rule_descriptor(array $x,&$error=null)
{
    $error=null;foreach(array('policy_id','rule_sequence','qualification_mode','qualification_category') as $k)if(!array_key_exists($k,$x)){$error='rule_field_missing';return false;}
    $pid=(int)$x['policy_id'];$seq=(int)$x['rule_sequence'];$mode=(string)$x['qualification_mode'];$cat=hrm_tim_002_token($x['qualification_category'],64);$daily=hrm_tim_002_int_or_null($x['daily_threshold_minutes']??null,0);$weekly=hrm_tim_002_int_or_null($x['weekly_threshold_minutes']??null,0);$weekend=hrm_tim_002_bool_or_null($x['weekend_required']??null);$holiday=hrm_tim_002_bool_or_null($x['holiday_required']??null);$rest=hrm_tim_002_bool_or_null($x['rest_day_required']??null);$req=hrm_tim_002_bool_or_null($x['approved_request_required']??0);$dev=hrm_tim_002_int_or_null($x['schedule_deviation_min_minutes']??null,0);
    if($pid<=0||$seq<=0||$cat===false||!in_array($mode,array('all_approved','daily_excess','weekly_excess','request_cap','schedule_deviation'),true)||$daily===false||$weekly===false||$weekend===false||$holiday===false||$rest===false||$req===false||$dev===false){$error='rule_definition_invalid';return false;}
    if($mode==='daily_excess'&&$daily===null){$error='daily_threshold_missing';return false;}if($mode==='weekly_excess'&&$weekly===null){$error='weekly_threshold_missing';return false;}if($mode==='request_cap'&&$req!==1){$error='request_requirement_missing';return false;}if($mode==='schedule_deviation'&&$dev===null){$error='schedule_deviation_missing';return false;}
    $p=array('policy_id'=>$pid,'rule_sequence'=>$seq,'daily_threshold_minutes'=>$daily,'weekly_threshold_minutes'=>$weekly,'weekend_required'=>$weekend,'holiday_required'=>$holiday,'rest_day_required'=>$rest,'approved_request_required'=>$req,'schedule_deviation_min_minutes'=>$dev,'qualification_mode'=>$mode,'qualification_category'=>$cat);$p['rule_sha256']=hrm_tim_002_hash($p);return $p;
}
/** Deterministic rule evaluation. Rules are ordered explicitly; first matching rule wins. */
function hrm_tim_002_evaluate_rule(array $rule,array $ctx,&$error=null)
{
    $error=null;foreach(array('subject_minutes','daily_approved_minutes','weekly_approved_minutes','scheduled_minutes','is_weekend','is_holiday','is_rest_day','approved_request_minutes') as $k)if(!array_key_exists($k,$ctx)){$error='qualification_context_missing';return false;}
    foreach(array('subject_minutes','daily_approved_minutes','weekly_approved_minutes','scheduled_minutes') as $k)if(!is_int($ctx[$k])||$ctx[$k]<0){$error='qualification_minutes_invalid';return false;}$request=$ctx['approved_request_minutes'];if($request!==null&&(!is_int($request)||$request<0)){$error='qualification_request_invalid';return false;}
    foreach(array('is_weekend','is_holiday','is_rest_day') as $k)if($ctx[$k]!==0&&$ctx[$k]!==1){$error='qualification_flag_unknown';return false;}
    foreach(array('weekend_required'=>'is_weekend','holiday_required'=>'is_holiday','rest_day_required'=>'is_rest_day') as $rk=>$ck)if($rule[$rk]!==null&&(int)$rule[$rk]!==$ctx[$ck])return array('matched'=>false,'qualified_minutes'=>0,'reason'=>'context_not_matched');
    if((int)$rule['approved_request_required']===1&&$request===null)return array('matched'=>false,'qualified_minutes'=>0,'reason'=>'approved_request_required');
    $subject=$ctx['subject_minutes'];$qualified=0;$reason='';
    switch($rule['qualification_mode']){
        case 'all_approved':$qualified=$subject;$reason='explicit_all_approved';break;
        case 'daily_excess':$excess=max(0,$ctx['daily_approved_minutes']-(int)$rule['daily_threshold_minutes']);$qualified=min($subject,$excess);$reason='explicit_daily_threshold';break;
        case 'weekly_excess':$excess=max(0,$ctx['weekly_approved_minutes']-(int)$rule['weekly_threshold_minutes']);$qualified=min($subject,$excess);$reason='explicit_weekly_threshold';break;
        case 'request_cap':$qualified=min($subject,(int)$request);$reason='explicit_approved_request_cap';break;
        case 'schedule_deviation':$excess=max(0,$ctx['daily_approved_minutes']-$ctx['scheduled_minutes']);if($excess<(int)$rule['schedule_deviation_min_minutes'])$excess=0;$qualified=min($subject,$excess);$reason='explicit_schedule_deviation';break;
        default:$error='qualification_mode_invalid';return false;
    }
    return array('matched'=>true,'qualified_minutes'=>$qualified,'reason'=>$reason,'category'=>$rule['qualification_category']);
}
function hrm_tim_002_evaluate_policy(array $rules,array $ctx,&$error=null)
{
    $error=null;if(!$rules){$error='qualification_rules_missing';return false;}usort($rules,function($a,$b){return (int)$a['rule_sequence']<=>(int)$b['rule_sequence'];});$seen=array();foreach($rules as $r){$seq=(int)$r['rule_sequence'];if(isset($seen[$seq])){$error='qualification_rule_sequence_ambiguous';return false;}$seen[$seq]=true;$x=hrm_tim_002_evaluate_rule($r,$ctx,$error);if($x===false)return false;if($x['matched'])return $x;}return array('matched'=>false,'qualified_minutes'=>0,'reason'=>'no_explicit_rule_matched','category'=>null);
}
?>
