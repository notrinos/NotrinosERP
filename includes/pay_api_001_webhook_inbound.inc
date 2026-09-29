<?php
/**
 * PAY-API-001 bounded inbound webhook parsing/signature verification helpers.
 *
 * Pure source contract: no database, file, environment, network or business-command
 * access occurs here. The signed body is bounded, verified byte-for-byte and reduced
 * to hashes before database custody.
 */
include_once(dirname(__FILE__).'/pay_api_001_webhook_transport.inc');
if (!defined('PAY_API_001_WEBHOOK_INBOUND_PATH')) define('PAY_API_001_WEBHOOK_INBOUND_PATH', '/payroll_webhook.php');
if (!defined('PAY_API_001_WEBHOOK_INBOUND_CANONICALIZATION')) define('PAY_API_001_WEBHOOK_INBOUND_CANONICALIZATION', 'notrinos-inbound-v1');
if (!defined('PAY_API_001_WEBHOOK_INBOUND_MAX_QUERY_BYTES')) define('PAY_API_001_WEBHOOK_INBOUND_MAX_QUERY_BYTES', 256);

function pay_api_001_webhook_inbound_source_key($value)
{
    return pay_api_001_provisioning_token($value,96,'/^[a-z0-9][a-z0-9._-]{0,95}$/D');
}
function pay_api_001_webhook_inbound_event_type($value)
{
    return pay_api_001_provisioning_token($value,128,'/^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$/D');
}
function pay_api_001_webhook_inbound_delivery_key($value)
{
    return pay_api_001_provisioning_token($value,128,'/^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$/D');
}
function pay_api_001_webhook_inbound_timestamp($value)
{
    if(!is_string($value)||preg_match('/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/D',$value)!==1)return false;
    $d=DateTime::createFromFormat('!Y-m-d\TH:i:s\Z',$value,new DateTimeZone('UTC'));
    return $d&&$d->format('Y-m-d\TH:i:s\Z')===$value?$d->getTimestamp():false;
}
function pay_api_001_webhook_inbound_signature($value)
{
    return is_string($value)&&preg_match('/^v1=[a-f0-9]{64}$/D',$value)===1?$value:false;
}
function pay_api_001_webhook_inbound_parse_query($raw)
{
    if(!is_string($raw)||strlen($raw)>PAY_API_001_WEBHOOK_INBOUND_MAX_QUERY_BYTES)return false;
    $pairs=$raw===''?array():explode('&',$raw);$out=array();
    foreach($pairs as $pair){
        if($pair==='')return false;$bits=explode('=',$pair,2);if(count($bits)!==2)return false;
        $k=rawurldecode($bits[0]);$v=rawurldecode($bits[1]);if(!in_array($k,array('company','source'),true)||array_key_exists($k,$out)||is_array($v))return false;$out[$k]=$v;
    }
    if(array_keys($out)!==array('company','source')&&array_keys($out)!==array('source','company'))return false;
    $company=isset($out['company'])&&preg_match('/^(0|[1-9][0-9]{0,9})$/D',$out['company'])===1?(int)$out['company']:false;
    $source=isset($out['source'])?pay_api_001_webhook_inbound_source_key($out['source']):false;
    return $company===false||$source===false?false:array('company_id'=>$company,'source_key'=>$source);
}
function pay_api_001_webhook_inbound_headers_from_server(array $server)
{
    $map=array('event'=>'HTTP_X_NOTRINOS_EVENT','delivery'=>'HTTP_X_NOTRINOS_DELIVERY','timestamp'=>'HTTP_X_NOTRINOS_TIMESTAMP','signature'=>'HTTP_X_NOTRINOS_SIGNATURE');$out=array();
    foreach($map as $k=>$sk){if(!isset($server[$sk])||!is_string($server[$sk])||strlen($server[$sk])>256||preg_match('/[\x00\r\n]/',$server[$sk])===1)return false;$out[$k]=$server[$sk];}
    $ct=isset($server['CONTENT_TYPE'])?(string)$server['CONTENT_TYPE']:'';if(strtolower(trim(explode(';',$ct,2)[0]))!=='application/json')return false;
    return $out;
}
function pay_api_001_webhook_inbound_canonical_material($company_id,$source_key,$event_type,$delivery_key,$timestamp,$payload_sha256,$body)
{
    return "notrinos-inbound-webhook-v1\nPOST\n".PAY_API_001_WEBHOOK_INBOUND_PATH."\n".(int)$company_id."\n".$source_key."\n".$event_type."\n".$delivery_key."\n".$timestamp."\n".$payload_sha256."\n".$body;
}
function pay_api_001_webhook_inbound_verify(array $source,array $headers,$body,$secret,$unix_time=null,&$error=null)
{
    $error=null;$required=array('webhook_inbound_source_id','company_id','source_key','event_type','canonicalization_code','signature_algorithm','signature_header_name','timestamp_header_name','max_clock_skew_seconds','replay_ttl_seconds','source_sha256');
    foreach($required as $k)if(!array_key_exists($k,$source)){$error='inbound_source_invalid';return false;}
    $sid=pay_api_001_positive_id($source['webhook_inbound_source_id']);$company=pay_api_001_uint_value($source['company_id'],true,4294967295);$source_key=pay_api_001_webhook_inbound_source_key($source['source_key']);$event=pay_api_001_webhook_inbound_event_type($source['event_type']);
    $skew=pay_api_001_uint_value($source['max_clock_skew_seconds'],false,900);$ttl=pay_api_001_uint_value($source['replay_ttl_seconds'],false,86400);
    if($sid===false||$company===false||$source_key===false||$event===false||$skew===false||$skew<30||$ttl===false||$ttl<$skew||!pay_api_001_is_sha256($source['source_sha256'])||$source['canonicalization_code']!==PAY_API_001_WEBHOOK_INBOUND_CANONICALIZATION||$source['signature_algorithm']!=='hmac-sha256'||$source['signature_header_name']!=='X-Notrinos-Signature'||$source['timestamp_header_name']!=='X-Notrinos-Timestamp'){$error='inbound_source_invalid';return false;}
    if(array_keys($headers)!==array('event','delivery','timestamp','signature')){$error='inbound_headers_invalid';return false;}
    $got_event=pay_api_001_webhook_inbound_event_type($headers['event']);$delivery=pay_api_001_webhook_inbound_delivery_key($headers['delivery']);$stamp=(string)$headers['timestamp'];$signature=pay_api_001_webhook_inbound_signature($headers['signature']);$stamp_unix=pay_api_001_webhook_inbound_timestamp($stamp);
    $now=$unix_time===null?time():pay_api_001_uint_value($unix_time,true,PHP_INT_MAX);
    if($got_event===false||$delivery===false||$signature===false||$stamp_unix===false||$now===false){$error='inbound_headers_invalid';return false;}
    if(!hash_equals($event,$got_event)){$error='inbound_event_denied';return false;}
    if(abs($now-$stamp_unix)>$skew){$error='inbound_timestamp_outside_window';return false;}
    if(!is_string($body)||strlen($body)<2||strlen($body)>PAY_API_001_WEBHOOK_MAX_PAYLOAD_BYTES){$error='inbound_payload_invalid';return false;}
    $decoded=json_decode($body,true);if(!is_array($decoded)||json_last_error()!==JSON_ERROR_NONE){$error='inbound_payload_invalid';return false;}
    if(!is_string($secret)||strlen($secret)<32||strlen($secret)>PAY_API_001_WEBHOOK_MAX_SECRET_BYTES){$error='inbound_secret_invalid';return false;}
    $payload_sha=hash('sha256',$body);$canonical=pay_api_001_webhook_inbound_canonical_material((int)$source['company_id'],$source_key,$event,$delivery,$stamp,$payload_sha,$body);$expected='v1='.hash_hmac('sha256',$canonical,$secret);
    if(!hash_equals($expected,$signature)){$error='inbound_signature_invalid';return false;}
    $delivery_sha=hash('sha256',$delivery);$signature_sha=hash('sha256',$signature);
    $replay=pay_api_001_provisioning_hash(array('source_sha256'=>$source['source_sha256'],'delivery_key_sha256'=>$delivery_sha,'event_type'=>$event,'timestamp'=>$stamp,'payload_sha256'=>$payload_sha));
    $correlation=pay_api_001_provisioning_hash(array('source_sha256'=>$source['source_sha256'],'replay_key_sha256'=>$replay,'signature_sha256'=>$signature_sha));
    return array('company_id'=>$company,'webhook_inbound_source_id'=>$sid,'source_key'=>$source_key,'event_type'=>$event,'delivery_key_sha256'=>$delivery_sha,'request_target_sha256'=>hash('sha256',PAY_API_001_WEBHOOK_INBOUND_PATH),'event_type_sha256'=>hash('sha256',$event),'payload_sha256'=>$payload_sha,'signature_sha256'=>$signature_sha,'timestamp_value'=>$stamp,'replay_key_sha256'=>$replay,'correlation_sha256'=>$correlation,'valid_until'=>gmdate('Y-m-d H:i:s',$stamp_unix+$ttl));
}
function pay_api_001_webhook_inbound_security_headers(){if(!headers_sent()){header('Cache-Control: no-store');header('Pragma: no-cache');header('X-Content-Type-Options: nosniff');header('Content-Type: application/json; charset=UTF-8');}}
function pay_api_001_webhook_inbound_respond($status,$body=array())
{
    pay_api_001_webhook_inbound_security_headers();http_response_code((int)$status);if(!empty($body))echo json_encode($body,JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE);exit;
}
?>
