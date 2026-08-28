<?php
/**
 * PAY-SEC-004 bounded OIDC authorization-code token exchange transport.
 *
 * Internal transport only. It performs one no-redirect HTTPS token request,
 * classifies pre-send, uncertain, rejected and successful outcomes, and returns
 * only an already-received ID token for the separate pinned verifier adaptor.
 * It performs no callback parsing, durable replay/consume/decision mutation,
 * identity linking, login/session issuance or assurance elevation.
 */

if (!defined('FEDERATION_OIDC_EXCHANGE_MAX_CODE_BYTES'))
    define('FEDERATION_OIDC_EXCHANGE_MAX_CODE_BYTES', 4096);

/** Return a domain-separated, irreversible authorization-code replay marker. */
function federation_oidc_authorization_code_replay_hash($company, $provider_id, $verifier_config_id, $code)
{
    $company=(int)$company;$provider_id=(int)$provider_id;$verifier_config_id=(int)$verifier_config_id;
    if ($company < 0 || $provider_id <= 0 || $verifier_config_id <= 0 || !is_string($code)
        || strlen($code) < 1 || strlen($code) > FEDERATION_OIDC_EXCHANGE_MAX_CODE_BYTES
        || preg_match('/[\x00-\x20\x7f]/', $code) === 1)
        return false;
    return hash('sha256', "notrinoserp:oidc-authorization-code:v1\0".$company."\0".$provider_id."\0".$verifier_config_id."\0".$code);
}

/** Build the exact secret-bearing request passed only to the transport layer. */
function federation_oidc_token_exchange_request($code, $redirect_uri, $pkce_verifier, $materials)
{
    if (!function_exists('federation_oidc_token_verifier_material_contract')
        || !function_exists('federation_oidc_client_config'))
        return false;
    $contract=federation_oidc_token_verifier_material_contract($materials);
    $authorization=isset($materials['oidc_client_config'])
        ? federation_oidc_client_config($materials['oidc_client_config']) : false;
    if ($contract===false || $authorization===false || !is_string($code)
        || strlen($code)<1 || strlen($code)>FEDERATION_OIDC_EXCHANGE_MAX_CODE_BYTES
        || preg_match('/[\x00-\x20\x7f]/',$code)===1
        || !is_string($redirect_uri) || !in_array($redirect_uri,$authorization['redirect_uris'],true)
        || !is_string($pkce_verifier) || preg_match('/^[A-Za-z0-9._~-]{43,128}$/D',$pkce_verifier)!==1)
        return false;
    $client_secret=$materials['oidc_client_secret'];
    $credentials=base64_encode(rawurlencode($contract['client_id']).':'.rawurlencode($client_secret));
    $body=http_build_query(array('grant_type'=>'authorization_code','code'=>$code,
        'redirect_uri'=>$redirect_uri,'code_verifier'=>$pkce_verifier),'','&',PHP_QUERY_RFC3986);
    return array(
        'url'=>$contract['token_endpoint'],
        'headers'=>array('Accept: application/json','Content-Type: application/x-www-form-urlencoded',
            'Authorization: Basic '.$credentials),
        'body'=>$body,
        'connect_timeout_seconds'=>(int)$contract['connect_timeout_seconds'],
        'total_timeout_seconds'=>(int)$contract['total_timeout_seconds'],
        'max_response_bytes'=>(int)$contract['max_exchange_response_bytes']
    );
}

/** Execute one HTTPS request with redirects and non-HTTPS protocols denied. */
function federation_oidc_token_exchange_curl_transport($request)
{
    if (!function_exists('curl_init') || !is_array($request))
        return array('request_sent'=>false,'transport_ok'=>false,'http_status'=>0,'content_type'=>'','body'=>'','error_code'=>'curl_unavailable');
    $response='';$limit=(int)$request['max_response_bytes'];$handle=curl_init();
    if ($handle===false)
        return array('request_sent'=>false,'transport_ok'=>false,'http_status'=>0,'content_type'=>'','body'=>'','error_code'=>'curl_init_failed');
    $options=array(CURLOPT_URL=>$request['url'],CURLOPT_POST=>true,CURLOPT_POSTFIELDS=>$request['body'],
        CURLOPT_HTTPHEADER=>$request['headers'],CURLOPT_FOLLOWLOCATION=>false,CURLOPT_MAXREDIRS=>0,
        CURLOPT_RETURNTRANSFER=>false,CURLOPT_HEADER=>false,CURLOPT_CONNECTTIMEOUT=>(int)$request['connect_timeout_seconds'],
        CURLOPT_TIMEOUT=>(int)$request['total_timeout_seconds'],CURLOPT_SSL_VERIFYPEER=>true,CURLOPT_SSL_VERIFYHOST=>2,
        CURLOPT_WRITEFUNCTION=>function($handle,$chunk)use(&$response,$limit){if(strlen($response)+strlen($chunk)>$limit)return 0;$response.=$chunk;return strlen($chunk);});
    if (defined('CURLOPT_PROTOCOLS') && defined('CURLPROTO_HTTPS'))
        $options[CURLOPT_PROTOCOLS]=CURLPROTO_HTTPS;
    if (!curl_setopt_array($handle,$options)) {
        curl_close($handle);
        return array('request_sent'=>false,'transport_ok'=>false,'http_status'=>0,'content_type'=>'','body'=>'','error_code'=>'curl_option_failed');
    }
    $ok=curl_exec($handle);$errno=(int)curl_errno($handle);$status=(int)curl_getinfo($handle,CURLINFO_HTTP_CODE);
    $content_type=(string)curl_getinfo($handle,CURLINFO_CONTENT_TYPE);$request_size=(int)curl_getinfo($handle,CURLINFO_REQUEST_SIZE);
    curl_close($handle);
    return array('request_sent'=>$request_size>0,'transport_ok'=>$ok===true && $errno===0,
        'http_status'=>$status,'content_type'=>$content_type,'body'=>$response,'error_code'=>$errno===0?'':('curl_'.$errno));
}

/** Reduce a transport result to the accepted bounded exchange classifications. */
function federation_oidc_token_exchange_classify($transport, $max_response_bytes)
{
    $expected=array('request_sent','transport_ok','http_status','content_type','body','error_code');
    if (!is_array($transport)) return false;
    $keys=array_keys($transport);sort($keys);$sorted=$expected;sort($sorted);
    if ($keys!==$sorted || !is_bool($transport['request_sent']) || !is_bool($transport['transport_ok'])
        || !is_int($transport['http_status']) || !is_string($transport['content_type'])
        || !is_string($transport['body']) || !is_string($transport['error_code']))
        return false;
    if (!$transport['request_sent'])
        return array('classification'=>'pre_send_failure','retry_safe'=>true,'terminal_reason'=>null);
    if (!$transport['transport_ok'])
        return array('classification'=>'exchange_uncertain','retry_safe'=>false,'terminal_reason'=>'exchange_uncertain');
    $status=$transport['http_status'];$body=$transport['body'];
    if (strlen($body)>(int)$max_response_bytes || preg_match('#^application/json(?:\s*;|$)#i',$transport['content_type'])!==1)
        return array('classification'=>'exchange_uncertain','retry_safe'=>false,'terminal_reason'=>'exchange_uncertain');
    $json=json_decode($body,true);
    if (!is_array($json) || array_values($json)===$json)
        return array('classification'=>'exchange_uncertain','retry_safe'=>false,'terminal_reason'=>'exchange_uncertain');
    if ($status>=400 && $status<500 && isset($json['error']) && is_string($json['error'])
        && preg_match('/^[A-Za-z0-9._~-]{1,64}$/D',$json['error'])===1)
        return array('classification'=>'exchange_rejected','retry_safe'=>false,'terminal_reason'=>'exchange_rejected',
            'oauth_error'=>$json['error']);
    if ($status!==200 || !isset($json['id_token']) || !is_string($json['id_token'])
        || strlen($json['id_token'])<16 || strlen($json['id_token'])>FEDERATION_OIDC_TOKEN_VERIFIER_MAX_JWT_BYTES
        || isset($json['refresh_token']) || (isset($json['token_type']) && $json['token_type']!=='Bearer')
        || (isset($json['expires_in']) && (!is_int($json['expires_in']) || $json['expires_in']<1 || $json['expires_in']>86400)))
        return array('classification'=>'exchange_uncertain','retry_safe'=>false,'terminal_reason'=>'exchange_uncertain');
    return array('classification'=>'exchange_succeeded','retry_safe'=>false,'terminal_reason'=>null,
        'id_token'=>$json['id_token'],'response_sha256'=>hash('sha256',$body));
}

/** Perform one bounded exchange; the result alone grants no authentication authority. */
function federation_oidc_exchange_authorization_code($code,$redirect_uri,$pkce_verifier,$materials,$transport=null)
{
    $request=federation_oidc_token_exchange_request($code,$redirect_uri,$pkce_verifier,$materials);
    if ($request===false)
        return array('classification'=>'pre_send_failure','retry_safe'=>true,'terminal_reason'=>null);
    if ($transport===null)
        $result=federation_oidc_token_exchange_curl_transport($request);
    elseif (is_callable($transport)) {
        try {$result=call_user_func($transport,$request);}
        catch(Exception $e){return array('classification'=>'exchange_uncertain','retry_safe'=>false,'terminal_reason'=>'exchange_uncertain');}
        catch(Error $e){return array('classification'=>'exchange_uncertain','retry_safe'=>false,'terminal_reason'=>'exchange_uncertain');}
    } else $result=false;
    if ($result===false)
        return array('classification'=>'pre_send_failure','retry_safe'=>true,'terminal_reason'=>null);
    $classified=federation_oidc_token_exchange_classify($result,$request['max_response_bytes']);
    return $classified===false ? array('classification'=>'exchange_uncertain','retry_safe'=>false,'terminal_reason'=>'exchange_uncertain') : $classified;
}
