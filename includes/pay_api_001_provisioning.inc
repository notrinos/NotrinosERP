<?php
/** PAY-API-001 shared value checks and the canonical SHA-256 hash of a payload. */
require_once(__DIR__.'/../includes/value_helpers.inc');
include_once(dirname(__FILE__).'/pay_api_001_authn.inc');

function pay_api_001_provisioning_token($value,$max,$pattern='/^[a-z0-9][a-z0-9._-]*$/D')
{
    return is_string($value)&&$value!==''&&strlen($value)<=(int)$max&&preg_match($pattern,$value)===1?$value:false;
}
function pay_api_001_provisioning_datetime($value)
{
    if(!is_string($value)||preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/D',$value)!==1)return false;
    $d=DateTime::createFromFormat('!Y-m-d H:i:s',$value,new DateTimeZone('UTC'));
    return $d&&$d->format('Y-m-d H:i:s')===$value?$value:false;
}
function pay_api_001_provisioning_canonicalize($value)
{
    if(!is_array($value))return $value;
    $keys=array_keys($value);$assoc=count($value)>0&&$keys!==range(0,count($value)-1);
    if($assoc)ksort($value,SORT_STRING);
    foreach($value as $k=>$v)$value[$k]=pay_api_001_provisioning_canonicalize($v);
    return $value;
}
function pay_api_001_provisioning_hash($value)
{
    $json=json_encode(pay_api_001_provisioning_canonicalize($value),JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE);
    return is_string($json)?hash('sha256',$json):false;
}
?>
