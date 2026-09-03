<?php
/** HRM-FND-004 governed identifier-verification-request browser boundary policy. */
if (!defined('HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_DB_VERSION')) define('HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_DB_VERSION','1.0.296');
if (!defined('HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_MAX_QUERY_BYTES')) define('HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_MAX_QUERY_BYTES',8192);
if (!defined('HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_MAX_CODE_BYTES')) define('HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_MAX_CODE_BYTES',4096);
if (!defined('HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_ATTEMPT_KEY')) define('HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_ATTEMPT_KEY','hrm_identifier_verification_request_route_attempts');
if (!defined('HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_MAX_ATTEMPTS')) define('HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_MAX_ATTEMPTS',8);
if (!defined('HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_ATTEMPT_WINDOW')) define('HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_ATTEMPT_WINDOW',60);
if (!defined('HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_ASSURED_TTL')) define('HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_ASSURED_TTL',300);

function hrm_identifier_verification_request_route_ready()
{
    if (!function_exists('get_company_pref')) return false;
    return version_compare((string)get_company_pref('version_id', true), HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_DB_VERSION, '>=');
}

function hrm_identifier_verification_request_route_authenticated()
{
    return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
        && method_exists($_SESSION['wa_current_user'],'logged_in') && $_SESSION['wa_current_user']->logged_in()
        && function_exists('hrm_identifier_verification_request_permission')
        && hrm_identifier_verification_request_permission();
}

function hrm_identifier_verification_request_route_employee_ref($value)
{
    if (!is_scalar($value)) return false;
    $value=trim((string)$value);
    return $value!=='' && strlen($value)<=64 && preg_match('/^[A-Za-z0-9_.:@-]+$/D',$value)===1 ? $value : false;
}

function hrm_identifier_verification_request_route_decode_component($value,$plus_as_space)
{
    if (!is_string($value)||preg_match('/%(?![0-9A-Fa-f]{2})/',$value)) return false;
    if ($plus_as_space) $value=str_replace('+',' ',$value);
    return rawurldecode($value);
}

/** Exact OIDC callback parser; employee/action/return parameters are forbidden. */
function hrm_identifier_verification_request_route_parse_callback($method,$raw_query)
{
    if ($method!=='GET'||!is_string($raw_query)||$raw_query===''||strlen($raw_query)>HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_MAX_QUERY_BYTES) return false;
    $values=array();
    foreach(explode('&',$raw_query) as $pair){
        if($pair==='') return false; $parts=explode('=',$pair,2); if(count($parts)!==2)return false;
        $key=hrm_identifier_verification_request_route_decode_component($parts[0],false);
        $value=hrm_identifier_verification_request_route_decode_component($parts[1],false);
        if($key===false||$value===false||!in_array($key,array('state','code','error','error_description'),true)||isset($values[$key]))return false;
        $values[$key]=$value;
    }
    if(!isset($values['state'])||preg_match('/^[A-Za-z0-9_-]{43}$/D',(string)$values['state'])!==1)return false;
    $has_code=isset($values['code']);$has_error=isset($values['error']);if($has_code===$has_error)return false;
    if($has_code){
        if(count($values)!==2||strlen((string)$values['code'])<1||strlen((string)$values['code'])>HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_MAX_CODE_BYTES
            ||preg_match('/[\x00-\x20\x7f]/',(string)$values['code']))return false;
        return array('kind'=>'code','state'=>(string)$values['state'],'code'=>(string)$values['code']);
    }
    if(count($values)>3||strlen((string)$values['error'])<1||strlen((string)$values['error'])>64
        ||preg_match('/^[A-Za-z0-9_.-]+$/D',(string)$values['error'])!==1)return false;
    if(isset($values['error_description'])&&(strlen((string)$values['error_description'])>256
        ||preg_match('/[\x00-\x1f\x7f]/',(string)$values['error_description'])))return false;
    return array('kind'=>'error','state'=>(string)$values['state'],'error'=>(string)$values['error']);
}

function hrm_identifier_verification_request_route_consume_attempt($now=null)
{
    $now=$now===null?time():(int)$now;if($now<=0)return false;
    $entry=isset($_SESSION[HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_ATTEMPT_KEY])&&is_array($_SESSION[HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_ATTEMPT_KEY])
        ?$_SESSION[HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_ATTEMPT_KEY]:array('window_start'=>$now,'count'=>0);
    if(!isset($entry['window_start'],$entry['count'])||(int)$entry['window_start']<=0
        ||$now-(int)$entry['window_start']>=HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_ATTEMPT_WINDOW)$entry=array('window_start'=>$now,'count'=>0);
    $entry['count']=(int)$entry['count']+1;$_SESSION[HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_ATTEMPT_KEY]=$entry;
    return $entry['count']<=HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_MAX_ATTEMPTS;
}

function hrm_identifier_verification_request_route_reset_attempts(){unset($_SESSION[HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_ATTEMPT_KEY]);}

function hrm_identifier_verification_request_route_callback_path($script_name)
{
    $suffix='/hrm/manage/identifier_verification_request.php';
    return is_string($script_name)&&substr($script_name,-strlen($suffix))===$suffix?$script_name:false;
}

function hrm_identifier_verification_request_route_local_path($script_name,$employee_ref,$result)
{
    $callback=hrm_identifier_verification_request_route_callback_path($script_name);$employee_ref=hrm_identifier_verification_request_route_employee_ref($employee_ref);
    if($callback===false||$employee_ref===false)return false;
    $fixed=array('assured'=>'assured','retry'=>'retry','failed'=>'failed','success'=>'success');
    $status=isset($fixed[$result])?$fixed[$result]:'failed';
    return $callback.'?employee_id='.rawurlencode($employee_ref).'&step_up='.$status;
}

function hrm_identifier_verification_request_route_redirect_local($script_name,$employee_ref,$result)
{
    $location=hrm_identifier_verification_request_route_local_path($script_name,$employee_ref,$result);
    if($location===false||headers_sent())return false;header('HTTP/1.1 303 See Other');header('Location: '.$location);return true;
}

function hrm_identifier_verification_request_route_redirect_provider($authorization_url)
{
    if(!function_exists('federation_oidc_valid_https_uri')||!federation_oidc_valid_https_uri($authorization_url,true)||headers_sent())return false;
    header('HTTP/1.1 303 See Other');header('Location: '.$authorization_url);return true;
}

function hrm_identifier_verification_request_route_terminalize_error($company,$state,$error)
{
    if(!function_exists('federation_oidc_acquire_callback_claim'))require_once(dirname(__FILE__).DIRECTORY_SEPARATOR.'federation_oidc_callback_claim.inc');
    $claim=federation_oidc_acquire_callback_claim((int)$company,$state,15);if(!is_array($claim)||!isset($claim['claim_token']))return false;
    $reason=$error==='access_denied'?'exchange_rejected':'exchange_uncertain';
    return federation_oidc_terminalize_callback_claim((int)$company,(string)$claim['claim_token'],$reason)!==false;
}

/** Store only non-secret proof-of-step-up context; never store target identifier material. */
function hrm_identifier_verification_request_route_bind_assured($result,$now=null)
{
    $now=$now===null?time():(int)$now;
    if(!is_array($result)||($result['status']??'')!=='assured'||!isset($result['session_id'],$result['employee_ref'],$result['identifier_id'],
        $result['identifier_row_version'],$result['action_class'],$result['action_hash'],$result['assurance_event_id'])||$now<=0)return false;
    if((string)$result['action_class']!==HRM_IDENTIFIER_VERIFICATION_REQUEST_ACTION_CLASS
        ||preg_match('/^[a-f0-9]{64}$/D',(string)$result['action_hash'])!==1)return false;
    $_SESSION['hrm_identifier_verification_request_assured']=array('session_id'=>(int)$result['session_id'],'employee_ref'=>(string)$result['employee_ref'],
        'identifier_id'=>(int)$result['identifier_id'],'identifier_row_version'=>(int)$result['identifier_row_version'],
        'action_class'=>(string)$result['action_class'],'action_hash'=>(string)$result['action_hash'],
        'assurance_event_id'=>(int)$result['assurance_event_id'],'expires_at_epoch'=>$now+HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_ASSURED_TTL);
    return true;
}

function hrm_identifier_verification_request_route_assured($employee_ref,$now=null)
{
    $now=$now===null?time():(int)$now;$employee_ref=hrm_identifier_verification_request_route_employee_ref($employee_ref);
    $row=isset($_SESSION['hrm_identifier_verification_request_assured'])?$_SESSION['hrm_identifier_verification_request_assured']:false;
    if(!is_array($row)||$employee_ref===false||$now<=0||(int)($row['expires_at_epoch']??0)<=$now
        ||(string)($row['employee_ref']??'')!==$employee_ref
        ||(int)($row['session_id']??0)!==federation_oidc_identifier_verification_request_step_up_current_session_id()
        ||(string)($row['action_class']??'')!==HRM_IDENTIFIER_VERIFICATION_REQUEST_ACTION_CLASS)return false;
    return $row;
}

function hrm_identifier_verification_request_route_clear_assured(){unset($_SESSION['hrm_identifier_verification_request_assured']);}

function hrm_identifier_verification_request_route_issue_confirm_csrf($assured,$now=null)
{
    $now=$now===null?time():(int)$now;if(!is_array($assured)||$now<=0)return false;
    try{$bytes=random_bytes(32);}catch(Exception $e){return false;}$token=rtrim(strtr(base64_encode($bytes),'+/','-_'),'=');
    if(preg_match('/^[A-Za-z0-9_-]{43}$/D',$token)!==1)return false;
    $_SESSION['hrm_identifier_verification_request_confirm_csrf']=array('hash'=>hash('sha256',$token),'action_hash'=>(string)$assured['action_hash'],
        'session_id'=>(int)$assured['session_id'],'expires_at_epoch'=>$now+HRM_IDENTIFIER_VERIFICATION_REQUEST_ROUTE_ASSURED_TTL);return $token;
}

function hrm_identifier_verification_request_route_consume_confirm_csrf($assured,$token,$now=null)
{
    $now=$now===null?time():(int)$now;$row=isset($_SESSION['hrm_identifier_verification_request_confirm_csrf'])?$_SESSION['hrm_identifier_verification_request_confirm_csrf']:false;
    unset($_SESSION['hrm_identifier_verification_request_confirm_csrf']);
    return is_array($assured)&&is_array($row)&&is_string($token)&&preg_match('/^[A-Za-z0-9_-]{43}$/D',$token)===1
        &&(int)$row['expires_at_epoch']>$now&&(int)$row['session_id']===(int)$assured['session_id']
        &&hash_equals((string)$row['action_hash'],(string)$assured['action_hash'])&&hash_equals((string)$row['hash'],hash('sha256',$token));
}

function hrm_identifier_verification_request_route_set_flash($employee_ref,$result)
{
    $employee_ref=hrm_identifier_verification_request_route_employee_ref($employee_ref);
    if($employee_ref===false||!is_array($result))return false;
    $request_id=isset($result['verification_request_id'])?(int)$result['verification_request_id']:(isset($result['request_id'])?(int)$result['request_id']:0);
    $draft_id=isset($result['approval_draft_id'])?(int)$result['approval_draft_id']:(isset($result['draft_id'])?(int)$result['draft_id']:0);
    $status=isset($result['request_status'])?(string)$result['request_status']:(isset($result['status'])?(string)$result['status']:'submitted');
    if(strlen($status)>32||preg_match('/^[A-Za-z0-9_.:-]+$/D',$status)!==1)return false;
    $_SESSION['hrm_identifier_verification_request_flash']=array('employee_ref'=>$employee_ref,'verification_request_id'=>$request_id,
        'approval_draft_id'=>$draft_id,'status'=>$status);return true;
}
function hrm_identifier_verification_request_route_take_flash($employee_ref)
{
    $employee_ref=hrm_identifier_verification_request_route_employee_ref($employee_ref);$row=isset($_SESSION['hrm_identifier_verification_request_flash'])?$_SESSION['hrm_identifier_verification_request_flash']:false;
    unset($_SESSION['hrm_identifier_verification_request_flash']);return is_array($row)&&$employee_ref!==false&&(string)$row['employee_ref']===$employee_ref?$row:false;
}

function hrm_identifier_verification_request_route_security_headers()
{
    if(headers_sent())return false;header('Cache-Control: no-store, max-age=0');header('Pragma: no-cache');header('Referrer-Policy: no-referrer');
    header('X-Frame-Options: DENY');header("Content-Security-Policy: default-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self' https:");
    header('Permissions-Policy: geolocation=(), microphone=(), camera=()');header('X-Content-Type-Options: nosniff');return true;
}
