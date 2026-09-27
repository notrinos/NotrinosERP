<?php
/** PAY-API-001 pure rate-limit policy/decision gate. Durable atomic enforcement is implemented by the governed DB runtime service. */
include_once(dirname(__FILE__).'/pay_api_001_authn.inc');
if (!defined('PAY_API_001_RATE_LIMIT_CONTRACT_VERSION')) define('PAY_API_001_RATE_LIMIT_CONTRACT_VERSION', '1.0.867');
function pay_api_001_rate_limit_positive_int($value,$max){$n=pay_api_001_uint_value($value,false,$max);return $n===false?false:$n;}
function pay_api_001_rate_limit_policy_valid($policy)
{
    if(!is_array($policy))return false;foreach(array('api_rate_limit_policy_id','api_resource_id','reviewed_by','approved_by') as $k)if(!isset($policy[$k])||pay_api_001_rate_limit_positive_int($policy[$k],PHP_INT_MAX)===false)return false;
    if(pay_api_001_positive_id($policy['reviewed_by'])===pay_api_001_positive_id($policy['approved_by'])||!isset($policy['action_code'])||(string)$policy['action_code']!=='read'||!isset($policy['window_seconds'])||pay_api_001_rate_limit_positive_int($policy['window_seconds'],86400)===false||!isset($policy['max_requests'])||pay_api_001_rate_limit_positive_int($policy['max_requests'],100000)===false||!isset($policy['policy_sha256'])||!pay_api_001_is_sha256($policy['policy_sha256']))return false;return true;
}
function pay_api_001_rate_limit_window_start($unix_time,$window_seconds)
{
    $t=pay_api_001_uint_value($unix_time,true,PHP_INT_MAX);$w=pay_api_001_rate_limit_positive_int($window_seconds,86400);if($t===false||$w===false)return false;return $t-($t%$w);
}
function pay_api_001_rate_limit_gate($policy,$accepted_in_window,$unix_time)
{
    if(!pay_api_001_rate_limit_policy_valid($policy))return array('allowed'=>false,'code'=>'rate_policy_invalid');if(!is_int($accepted_in_window)||$accepted_in_window<0)return array('allowed'=>false,'code'=>'rate_state_invalid');$window=pay_api_001_rate_limit_window_start($unix_time,$policy['window_seconds']);if($window===false)return array('allowed'=>false,'code'=>'rate_clock_invalid');$max=(int)$policy['max_requests'];if($accepted_in_window>=$max)return array('allowed'=>false,'code'=>'rate_limit_exceeded','window_start'=>$window,'remaining'=>0);return array('allowed'=>true,'code'=>'rate_limit_allowed','window_start'=>$window,'remaining'=>$max-$accepted_in_window-1);
}
?>
