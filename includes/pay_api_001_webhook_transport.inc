<?php
/**
 * PAY-API-001 bounded outbound webhook secret, signing and HTTPS transport helpers.
 *
 * This file contains no database writes, file/environment secret loading, retry
 * scheduling, response persistence, replay mutation, dead-letter mutation or
 * business-command dispatch. Secret and endpoint material are supplied through
 * explicit reviewed resolver boundaries and exist only in process memory.
 */
include_once(dirname(__FILE__).'/pay_api_001_webhook.inc');

if (!defined('PAY_API_001_WEBHOOK_MAX_SECRET_BYTES')) define('PAY_API_001_WEBHOOK_MAX_SECRET_BYTES', 4096);
if (!defined('PAY_API_001_WEBHOOK_MAX_PAYLOAD_BYTES')) define('PAY_API_001_WEBHOOK_MAX_PAYLOAD_BYTES', 262144);
if (!defined('PAY_API_001_WEBHOOK_MAX_RESPONSE_BYTES')) define('PAY_API_001_WEBHOOK_MAX_RESPONSE_BYTES', 8192);
if (!defined('PAY_API_001_WEBHOOK_MAX_RESPONSE_HEADER_BYTES')) define('PAY_API_001_WEBHOOK_MAX_RESPONSE_HEADER_BYTES', 16384);
if (!defined('PAY_API_001_WEBHOOK_MAX_DNS_ADDRESSES')) define('PAY_API_001_WEBHOOK_MAX_DNS_ADDRESSES', 32);
if (!defined('PAY_API_001_WEBHOOK_CONNECT_TIMEOUT_SECONDS')) define('PAY_API_001_WEBHOOK_CONNECT_TIMEOUT_SECONDS', 5);
if (!defined('PAY_API_001_WEBHOOK_TOTAL_TIMEOUT_SECONDS')) define('PAY_API_001_WEBHOOK_TOTAL_TIMEOUT_SECONDS', 15);

/** Strict externally governed endpoint path; query strings and fragments are denied. */
function pay_api_001_webhook_endpoint_path($value)
{
    if (!is_string($value) || strlen($value) < 1 || strlen($value) > 1024 || $value[0] !== '/') return false;
    if (strpos($value, '?') !== false || strpos($value, '#') !== false || strpos($value, "\\") !== false || preg_match('/[\x00-\x20\x7f]/', $value) === 1) return false;
    if (preg_match('#^/[A-Za-z0-9._~/-]*$#D', $value) !== 1 || preg_match('#//+#', $value) === 1 || preg_match('#(?:^|/)\.\.?(/|$)#', $value) === 1) return false;
    return $value;
}

/** Accept only canonical DNS hostnames, never URL authority syntax or IP literals. */
function pay_api_001_webhook_dns_hostname($value)
{
    if (!is_string($value) || strlen($value) < 1 || strlen($value) > 253) return false;
    $host=strtolower($value);
    if (filter_var($host,FILTER_VALIDATE_IP)!==false || in_array($host,array('localhost','localhost.localdomain'),true)) return false;
    if (preg_match('/^(?=.{1,253}$)(?:[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.)*[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$/D',$host)!==1) return false;
    $labels=explode('.',$host);
    if (count($labels)<2 || preg_match('/[a-z]/',$labels[count($labels)-1])!==1) return false;
    return $host;
}

/** Reject address ranges that are not ordinary public unicast IPv4 destinations. */
function pay_api_001_webhook_ipv4_is_public($ip)
{
    if (!is_string($ip)) return false;
    $flags=FILTER_FLAG_IPV4;
    if (defined('FILTER_FLAG_GLOBAL_RANGE')) $flags|=constant('FILTER_FLAG_GLOBAL_RANGE');
    else $flags|=FILTER_FLAG_NO_PRIV_RANGE|FILTER_FLAG_NO_RES_RANGE;
    if (filter_var($ip,FILTER_VALIDATE_IP,$flags)===false) return false;
    $packed=inet_pton($ip);
    if ($packed===false || strlen($packed)!==4) return false;

    /* Keep this fallback for PHP versions without FILTER_FLAG_GLOBAL_RANGE. */
    $non_public_ranges=array('0.0.0.0/8','10.0.0.0/8','100.64.0.0/10','127.0.0.0/8','169.254.0.0/16',
        '172.16.0.0/12','192.0.0.0/24','192.0.2.0/24','192.88.99.0/24','192.168.0.0/16',
        '198.18.0.0/15','198.51.100.0/24','203.0.113.0/24','224.0.0.0/4','240.0.0.0/4');
    foreach($non_public_ranges as $range){
        $parts=explode('/',$range,2);$network=inet_pton($parts[0]);$bits=(int)$parts[1];
        if($network===false||strlen($network)!==4)return false;
        $whole_bytes=(int)floor($bits/8);$matches=true;
        for($i=0;$i<$whole_bytes;$i++)if(ord($packed[$i])!==ord($network[$i])){$matches=false;break;}
        if($matches&&($bits%8)>0){$mask=(0xff<<(8-($bits%8)))&0xff;if((ord($packed[$whole_bytes])&$mask)!==(ord($network[$whole_bytes])&$mask))$matches=false;}
        if($matches)return false;
    }
    return true;
}

/** Resolve an opaque signing-key reference without reading files, env, globals or DB. */
function pay_api_001_webhook_resolve_signing_secret(array $secret_ref, $resolver, &$error=null)
{
    $error=null;
    foreach (array('company_id','ref_kind','provider_key','provider_reference_sha256','secret_ref_sha256') as $key) if (!array_key_exists($key,$secret_ref)) {$error='webhook_secret_reference_invalid';return false;}
    if (pay_api_001_uint_value($secret_ref['company_id'],true,4294967295)===false || (string)$secret_ref['ref_kind']!=='webhook_signing_key'
        || pay_api_001_provisioning_token($secret_ref['provider_key'],64)===false || !pay_api_001_is_sha256($secret_ref['provider_reference_sha256'])
        || !pay_api_001_is_sha256($secret_ref['secret_ref_sha256']) || !is_callable($resolver)) {$error='webhook_secret_reference_invalid';return false;}
    $request=array('company_id'=>(int)$secret_ref['company_id'],'ref_kind'=>'webhook_signing_key','provider_key'=>(string)$secret_ref['provider_key'],
        'provider_reference_sha256'=>(string)$secret_ref['provider_reference_sha256'],'secret_ref_sha256'=>(string)$secret_ref['secret_ref_sha256']);
    try {$resolved=call_user_func($resolver,$request);} catch (Throwable $t) {$error='webhook_secret_resolver_failed';return false;}
    if (!is_array($resolved) || array_keys($resolved)!==array('provider_reference_sha256','secret_ref_sha256','secret')) {$error='webhook_secret_resolver_invalid';return false;}
    if (!is_string($resolved['provider_reference_sha256']) || !hash_equals((string)$secret_ref['provider_reference_sha256'],$resolved['provider_reference_sha256'])
        || !is_string($resolved['secret_ref_sha256']) || !hash_equals((string)$secret_ref['secret_ref_sha256'],$resolved['secret_ref_sha256'])
        || !is_string($resolved['secret']) || strlen($resolved['secret'])<32 || strlen($resolved['secret'])>PAY_API_001_WEBHOOK_MAX_SECRET_BYTES) {$error='webhook_secret_resolver_mismatch';return false;}
    return $resolved['secret'];
}

/** Validate externally resolved endpoint material against the immutable target reference. */
function pay_api_001_webhook_endpoint_material(array $target, array $endpoint, &$error=null)
{
    $error=null;
    if (!isset($target['endpoint_config_ref_sha256']) || !pay_api_001_is_sha256($target['endpoint_config_ref_sha256'])) {$error='webhook_endpoint_target_reference_invalid';return false;}
    if (array_keys($endpoint)!==array('endpoint_config_ref_sha256','path') || !is_string($endpoint['endpoint_config_ref_sha256'])
        || !hash_equals((string)$target['endpoint_config_ref_sha256'],$endpoint['endpoint_config_ref_sha256'])) {$error='webhook_endpoint_reference_mismatch';return false;}
    $path=pay_api_001_webhook_endpoint_path($endpoint['path']);if($path===false){$error='webhook_endpoint_path_invalid';return false;}
    return array('endpoint_config_ref_sha256'=>$endpoint['endpoint_config_ref_sha256'],'path'=>$path);
}

/** Build the exact canonical signed POST request. It performs no network I/O. */
function pay_api_001_webhook_signed_request(array $delivery,array $tls_policy,array $signing_profile,array $target,array $endpoint,$signing_secret,$unix_time,&$error=null)
{
    $error=null;$t=pay_api_001_uint_value($unix_time,true,PHP_INT_MAX);
    if($t===false||!is_string($signing_secret)||strlen($signing_secret)<32||strlen($signing_secret)>PAY_API_001_WEBHOOK_MAX_SECRET_BYTES){$error='webhook_signing_material_invalid';return false;}
    foreach(array('webhook_outbox_id','event_type','payload_json','payload_sha256','queue_key_sha256') as $k)if(!array_key_exists($k,$delivery)){$error='webhook_delivery_material_invalid';return false;}
    $outbox=pay_api_001_positive_id($delivery['webhook_outbox_id']);$body=is_string($delivery['payload_json'])?$delivery['payload_json']:false;
    if($outbox===false||$delivery['event_type']!==PAY_API_001_WEBHOOK_EVENT_TYPE||$body===false||strlen($body)<2||strlen($body)>PAY_API_001_WEBHOOK_MAX_PAYLOAD_BYTES
        ||!pay_api_001_is_sha256($delivery['payload_sha256'])||!hash_equals($delivery['payload_sha256'],hash('sha256',$body))||!pay_api_001_is_sha256($delivery['queue_key_sha256'])){$error='webhook_delivery_material_invalid';return false;}
    $decoded=json_decode($body,true);if(!is_array($decoded)||json_encode($decoded,JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE)!==$body){$error='webhook_payload_not_canonical';return false;}
    $raw_host=isset($tls_policy['destination_host'])&&is_string($tls_policy['destination_host'])?strtolower($tls_policy['destination_host']):'';
    if(filter_var($raw_host,FILTER_VALIDATE_IP)!==false){$error='webhook_ip_literal_destination_denied';return false;}
    if(in_array($raw_host,array('localhost','localhost.localdomain'),true)){$error='webhook_loopback_destination_denied';return false;}
    $host=pay_api_001_webhook_dns_hostname($raw_host);
    $minimum_tls=isset($tls_policy['minimum_tls_version'])?(string)$tls_policy['minimum_tls_version']:'';
    if($host===false||($tls_policy['destination_port']??null)!==443||($tls_policy['scheme_code']??'')!=='https'||!in_array($minimum_tls,array('TLS1.2','TLS1.3'),true)
        ||($tls_policy['verify_peer_required']??null)!==1||($tls_policy['verify_hostname_required']??null)!==1){$error='webhook_tls_policy_invalid';return false;}
    if(($signing_profile['canonicalization_code']??'')!=='notrinos-json-v1'||($signing_profile['signature_algorithm']??'')!=='hmac-sha256'
        ||($signing_profile['signature_header_name']??'')!=='X-Notrinos-Signature'||($signing_profile['timestamp_header_name']??'')!=='X-Notrinos-Timestamp'){$error='webhook_signing_profile_invalid';return false;}
    $ep=pay_api_001_webhook_endpoint_material($target,$endpoint,$error);if($ep===false)return false;
    $timestamp=gmdate('Y-m-d\TH:i:s\Z',$t);
    $canonical="notrinos-webhook-v1\nPOST\nhttps\n443\n".$host."\n".$ep['path']."\n".$minimum_tls."\n".$timestamp."\n".$outbox."\n".$delivery['queue_key_sha256']."\n".$delivery['payload_sha256']."\n".$body;
    $signature='v1='.hash_hmac('sha256',$canonical,$signing_secret);
    $headers=array('Accept: application/json','Content-Type: application/json','X-Notrinos-Event: '.PAY_API_001_WEBHOOK_EVENT_TYPE,
        'X-Notrinos-Delivery: '.$outbox,'X-Notrinos-Timestamp: '.$timestamp,'X-Notrinos-Signature: '.$signature,'Expect:');
    $request_sha=hash('sha256',$canonical."\n".$signature);
    return array('scheme'=>'https','host'=>$host,'port'=>443,'path'=>$ep['path'],'url'=>'https://'.$host.$ep['path'],'minimum_tls_version'=>$minimum_tls,'headers'=>$headers,'body'=>$body,
        'payload_sha256'=>$delivery['payload_sha256'],'queue_key_sha256'=>$delivery['queue_key_sha256'],'webhook_outbox_id'=>$outbox,
        'timestamp'=>$timestamp,'request_sha256'=>$request_sha,'connect_timeout_seconds'=>PAY_API_001_WEBHOOK_CONNECT_TIMEOUT_SECONDS,
        'total_timeout_seconds'=>PAY_API_001_WEBHOOK_TOTAL_TIMEOUT_SECONDS,'max_response_bytes'=>PAY_API_001_WEBHOOK_MAX_RESPONSE_BYTES);
}

/** Rebuild the exact signing input shared by the signer and transport validator. */
function pay_api_001_webhook_signed_canonical_material($host,$path,$minimum_tls,$timestamp,$outbox,$queue_sha,$payload_sha,$body)
{
    return "notrinos-webhook-v1\nPOST\nhttps\n443\n".$host."\n".$path."\n".$minimum_tls."\n".$timestamp."\n".$outbox."\n".$queue_sha."\n".$payload_sha."\n".$body;
}

/** Validate the bounded signed-request envelope before any DNS or cURL operation. */
function pay_api_001_webhook_curl_request_valid(array $request)
{
    $expected_keys=array('scheme','host','port','path','url','minimum_tls_version','headers','body','payload_sha256','queue_key_sha256','webhook_outbox_id','timestamp','request_sha256','connect_timeout_seconds','total_timeout_seconds','max_response_bytes');
    if(array_keys($request)!==$expected_keys||$request['scheme']!=='https'||$request['port']!==443||!is_string($request['host'])
        ||pay_api_001_webhook_dns_hostname($request['host'])!==$request['host']||!in_array($request['minimum_tls_version'],array('TLS1.2','TLS1.3'),true)
        ||!is_string($request['path'])||pay_api_001_webhook_endpoint_path($request['path'])!==$request['path']
        ||$request['url']!=='https://'.$request['host'].$request['path']||!is_string($request['body'])||strlen($request['body'])<2
        ||strlen($request['body'])>PAY_API_001_WEBHOOK_MAX_PAYLOAD_BYTES||!pay_api_001_is_sha256($request['payload_sha256'])
        ||!hash_equals($request['payload_sha256'],hash('sha256',$request['body']))||!pay_api_001_is_sha256($request['queue_key_sha256'])
        ||!is_int($request['webhook_outbox_id'])||$request['webhook_outbox_id']<1||!is_string($request['timestamp'])
        ||preg_match('/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/D',$request['timestamp'])!==1
        ||!is_int($request['connect_timeout_seconds'])||$request['connect_timeout_seconds']<1||$request['connect_timeout_seconds']>PAY_API_001_WEBHOOK_CONNECT_TIMEOUT_SECONDS
        ||!is_int($request['total_timeout_seconds'])||$request['total_timeout_seconds']<$request['connect_timeout_seconds']
        ||$request['total_timeout_seconds']>PAY_API_001_WEBHOOK_TOTAL_TIMEOUT_SECONDS||!is_int($request['max_response_bytes'])
        ||$request['max_response_bytes']<1||$request['max_response_bytes']>PAY_API_001_WEBHOOK_MAX_RESPONSE_BYTES||!is_array($request['headers'])
        ||array_keys($request['headers'])!==array(0,1,2,3,4,5,6)||!pay_api_001_is_sha256($request['request_sha256']))return false;
    $signature_prefix='X-Notrinos-Signature: v1=';
    if(!is_string($request['headers'][5])||strpos($request['headers'][5],$signature_prefix)!==0)return false;
    $signature=substr($request['headers'][5],strlen($signature_prefix));
    if(preg_match('/^[a-f0-9]{64}$/D',$signature)!==1)return false;
    $expected_headers=array('Accept: application/json','Content-Type: application/json','X-Notrinos-Event: '.PAY_API_001_WEBHOOK_EVENT_TYPE,
        'X-Notrinos-Delivery: '.$request['webhook_outbox_id'],'X-Notrinos-Timestamp: '.$request['timestamp'],$signature_prefix.$signature,'Expect:');
    foreach($request['headers'] as $i=>$header)if(!is_string($header)||strlen($header)>256||preg_match('/[\x00\r\n]/',$header)===1||$header!==$expected_headers[$i])return false;
    $decoded=json_decode($request['body'],true);
    if(!is_array($decoded)||json_encode($decoded,JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE)!==$request['body'])return false;
    $canonical=pay_api_001_webhook_signed_canonical_material($request['host'],$request['path'],$request['minimum_tls_version'],$request['timestamp'],
        $request['webhook_outbox_id'],$request['queue_key_sha256'],$request['payload_sha256'],$request['body']);
    return hash_equals($request['request_sha256'],hash('sha256',$canonical."\nv1=".$signature));
}

/** Resolve and validate one public IPv4 destination before cURL; callers may inject a test resolver. */
function pay_api_001_webhook_resolve_public_ipv4($host,$resolver=null,&$error=null)
{
    $error=null;
    if(pay_api_001_webhook_dns_hostname($host)===false){$error='webhook_dns_host_invalid';return false;}
    if($resolver===null){if(!function_exists('gethostbynamel')){$error='webhook_dns_unavailable';return false;}$resolver='gethostbynamel';}
    if(!is_callable($resolver)){$error='webhook_dns_resolver_invalid';return false;}
    try{$ips=call_user_func($resolver,$host);}catch(Throwable $t){$error='webhook_dns_resolution_failed';return false;}
    if(!is_array($ips)||count($ips)<1){$error='webhook_dns_resolution_failed';return false;}
    if(count($ips)>PAY_API_001_WEBHOOK_MAX_DNS_ADDRESSES){$error='webhook_dns_answer_limit_exceeded';return false;}
    $public=array();
    foreach($ips as $ip){
        if(!pay_api_001_webhook_ipv4_is_public($ip)){$error='webhook_dns_private_or_reserved_destination_denied';return false;}
        $public[$ip]=true;
    }
    $public=array_keys($public);sort($public,SORT_STRING);
    return $public;
}

/** Reduce one bounded transport result to non-secret delivery evidence only. */
function pay_api_001_webhook_transport_classify($transport,$max_response_bytes=PAY_API_001_WEBHOOK_MAX_RESPONSE_BYTES)
{
    $keys=array('request_sent','transport_ok','http_status','content_type','body','response_headers_sha256','error_code');if(!is_array($transport)||array_keys($transport)!==$keys)return false;
    if(!is_bool($transport['request_sent'])||!is_bool($transport['transport_ok'])||!is_int($transport['http_status'])||!is_string($transport['content_type'])||!is_string($transport['body'])||!pay_api_001_is_sha256($transport['response_headers_sha256'])||!is_string($transport['error_code']))return false;
    $body=$transport['body'];$evidence=array('http_status'=>$transport['http_status'],'response_headers_sha256'=>$transport['response_headers_sha256'],'response_body_sha256'=>hash('sha256',$body),'response_body_bytes'=>strlen($body),'response_content_type_sha256'=>hash('sha256',strtolower(trim($transport['content_type']))));
    if(strlen($body)>(int)$max_response_bytes)return array_merge(array('classification'=>'delivery_uncertain','retry_safe'=>false),$evidence);
    if(!$transport['request_sent'])return array_merge(array('classification'=>'pre_send_failure','retry_safe'=>true),$evidence);
    if(!$transport['transport_ok'])return array_merge(array('classification'=>'delivery_uncertain','retry_safe'=>false),$evidence);
    $status=$transport['http_status'];if($status>=200&&$status<300)return array_merge(array('classification'=>'delivered','retry_safe'=>false),$evidence);
    if($status===408||$status===425||$status===429||$status>=500)return array_merge(array('classification'=>'provider_retryable','retry_safe'=>false),$evidence);
    if($status>=400&&$status<500)return array_merge(array('classification'=>'provider_rejected','retry_safe'=>false),$evidence);
    return array_merge(array('classification'=>'delivery_uncertain','retry_safe'=>false),$evidence);
}

/** Return stable minimized transport evidence without exposing provider body bytes. */
function pay_api_001_webhook_transport_minimized_result($transport,$max_response_bytes=PAY_API_001_WEBHOOK_MAX_RESPONSE_BYTES)
{
    $result=pay_api_001_webhook_transport_classify($transport,$max_response_bytes);
    if($result===false)return false;
    $result['error_code']=$transport['error_code'];
    return $result;
}

/** Build a pre-send failure result without exposing raw response material. */
function pay_api_001_webhook_transport_pre_send_failure($error_code)
{
    return array('classification'=>'pre_send_failure','retry_safe'=>true,'http_status'=>0,'response_headers_sha256'=>hash('sha256',''),'response_body_sha256'=>hash('sha256',''),
        'response_body_bytes'=>0,'response_content_type_sha256'=>hash('sha256',''),'error_code'=>(string)$error_code);
}

/** Execute one bounded HTTPS POST. No retry, persistence or reprocess action occurs here. */
function pay_api_001_webhook_curl_transport(array $request)
{
    if(!pay_api_001_webhook_curl_request_valid($request))return pay_api_001_webhook_transport_pre_send_failure('request_invalid');
    if(!function_exists('curl_init'))return pay_api_001_webhook_transport_pre_send_failure('curl_unavailable');
    if(!defined('CURLOPT_RESOLVE')||!defined('CURLOPT_PROTOCOLS')||!defined('CURLPROTO_HTTPS')||!defined('CURLOPT_SSLVERSION')||!defined('CURL_SSLVERSION_TLSv1_2'))return pay_api_001_webhook_transport_pre_send_failure('curl_transport_controls_unavailable');
    if($request['minimum_tls_version']==='TLS1.3'&&!defined('CURL_SSLVERSION_TLSv1_3'))return pay_api_001_webhook_transport_pre_send_failure('curl_tls13_control_unavailable');
    $dns_error=null;$public_ips=pay_api_001_webhook_resolve_public_ipv4($request['host'],null,$dns_error);if($public_ips===false)return pay_api_001_webhook_transport_pre_send_failure($dns_error);
    $response='';$response_headers='';$response_limit_exceeded=false;$response_header_limit_exceeded=false;$limit=$request['max_response_bytes'];$h=curl_init();if($h===false)return pay_api_001_webhook_transport_pre_send_failure('curl_init_failed');
    $ssl_minimum=$request['minimum_tls_version']==='TLS1.3'?constant('CURL_SSLVERSION_TLSv1_3'):constant('CURL_SSLVERSION_TLSv1_2');
    $options=array(CURLOPT_URL=>$request['url'],CURLOPT_POST=>true,CURLOPT_POSTFIELDS=>$request['body'],CURLOPT_HTTPHEADER=>$request['headers'],CURLOPT_FOLLOWLOCATION=>false,
        CURLOPT_MAXREDIRS=>0,CURLOPT_RETURNTRANSFER=>false,CURLOPT_HEADER=>false,CURLOPT_CONNECTTIMEOUT=>(int)$request['connect_timeout_seconds'],CURLOPT_TIMEOUT=>(int)$request['total_timeout_seconds'],
        CURLOPT_SSL_VERIFYPEER=>true,CURLOPT_SSL_VERIFYHOST=>2,CURLOPT_SSLVERSION=>$ssl_minimum,CURLOPT_PROXY=>'',CURLOPT_RESOLVE=>array($request['host'].':443:'.$public_ips[0]),
        CURLOPT_HEADERFUNCTION=>function($handle,$line)use(&$response_headers,&$response_header_limit_exceeded){if(strlen($response_headers)+strlen($line)>PAY_API_001_WEBHOOK_MAX_RESPONSE_HEADER_BYTES){$response_header_limit_exceeded=true;return 0;}$response_headers.=$line;return strlen($line);},
        CURLOPT_WRITEFUNCTION=>function($handle,$chunk)use(&$response,&$response_limit_exceeded,$limit){if(strlen($response)+strlen($chunk)>$limit){$response_limit_exceeded=true;return 0;}$response.=$chunk;return strlen($chunk);});
    $options[CURLOPT_PROTOCOLS]=CURLPROTO_HTTPS;if(defined('CURLOPT_REDIR_PROTOCOLS'))$options[CURLOPT_REDIR_PROTOCOLS]=CURLPROTO_HTTPS;
    if(!curl_setopt_array($h,$options)){curl_close($h);return pay_api_001_webhook_transport_pre_send_failure('curl_option_failed');}
    $ok=curl_exec($h);$errno=(int)curl_errno($h);$status=(int)curl_getinfo($h,CURLINFO_HTTP_CODE);$ctype=(string)curl_getinfo($h,CURLINFO_CONTENT_TYPE);$request_size=(int)curl_getinfo($h,CURLINFO_REQUEST_SIZE);curl_close($h);
    $transport=array('request_sent'=>$request_size>0,'transport_ok'=>$ok===true&&$errno===0&&!$response_limit_exceeded&&!$response_header_limit_exceeded,'http_status'=>$status,'content_type'=>$ctype,
        'body'=>$response,'response_headers_sha256'=>hash('sha256',$response_headers),'error_code'=>$response_header_limit_exceeded?'response_header_limit_exceeded':($response_limit_exceeded?'response_body_limit_exceeded':($errno===0?'':('curl_'.$errno))));
    return pay_api_001_webhook_transport_minimized_result($transport,$limit);
}
?>
