<?php
/** HRM-ESS-001 reauthenticated, CSRF-protected current-user self-change browser boundary. */
if (!defined('HRM_ESS_SELF_CHANGE_ROUTE_DB_VERSION')) define('HRM_ESS_SELF_CHANGE_ROUTE_DB_VERSION','1.0.450');
if (!defined('HRM_ESS_SELF_CHANGE_ROUTE_ATTEMPT_KEY')) define('HRM_ESS_SELF_CHANGE_ROUTE_ATTEMPT_KEY','hrm_ess_self_change_route_attempts');
if (!defined('HRM_ESS_SELF_CHANGE_ROUTE_MAX_ATTEMPTS')) define('HRM_ESS_SELF_CHANGE_ROUTE_MAX_ATTEMPTS',5);
if (!defined('HRM_ESS_SELF_CHANGE_ROUTE_ATTEMPT_WINDOW')) define('HRM_ESS_SELF_CHANGE_ROUTE_ATTEMPT_WINDOW',300);

function hrm_ess_self_change_route_ready()
{
    return function_exists('get_company_pref')
        && version_compare((string)get_company_pref('version_id', true), HRM_ESS_SELF_CHANGE_ROUTE_DB_VERSION, '>=');
}

function hrm_ess_self_change_route_authenticated()
{
    return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
        && method_exists($_SESSION['wa_current_user'],'logged_in') && $_SESSION['wa_current_user']->logged_in()
        && function_exists('hrm_ess_self_change_user_can_request') && hrm_ess_self_change_user_can_request()
        && function_exists('get_hrm_ess_current_self_scope') && is_array(get_hrm_ess_current_self_scope());
}

function hrm_ess_self_change_route_security_headers()
{
    if (headers_sent()) return false;
    header('Cache-Control: no-store, no-cache, must-revalidate, max-age=0');
    header('Pragma: no-cache');
    header('Referrer-Policy: no-referrer');
    header('X-Content-Type-Options: nosniff');
    header('X-Frame-Options: DENY');
    /* 1.0.449 predecessor used frame-ancestors 'self'; Phase-1 completion tightens framing to none. */
    header("Content-Security-Policy: default-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self' https:");
    header('Permissions-Policy: geolocation=(), microphone=(), camera=()');
    return true;
}

function hrm_ess_self_change_route_consume_attempt($now = null)
{
    $now = $now === null ? time() : (int)$now;
    if ($now <= 0) return false;
    $entry = isset($_SESSION[HRM_ESS_SELF_CHANGE_ROUTE_ATTEMPT_KEY]) && is_array($_SESSION[HRM_ESS_SELF_CHANGE_ROUTE_ATTEMPT_KEY])
        ? $_SESSION[HRM_ESS_SELF_CHANGE_ROUTE_ATTEMPT_KEY] : array('window_start'=>$now,'count'=>0);
    if (!isset($entry['window_start'],$entry['count']) || (int)$entry['window_start'] <= 0
        || $now-(int)$entry['window_start'] >= HRM_ESS_SELF_CHANGE_ROUTE_ATTEMPT_WINDOW)
        $entry = array('window_start'=>$now,'count'=>0);
    $entry['count'] = (int)$entry['count'] + 1;
    $_SESSION[HRM_ESS_SELF_CHANGE_ROUTE_ATTEMPT_KEY] = $entry;
    return $entry['count'] <= HRM_ESS_SELF_CHANGE_ROUTE_MAX_ATTEMPTS;
}

function hrm_ess_self_change_route_reset_attempts()
{
    unset($_SESSION[HRM_ESS_SELF_CHANGE_ROUTE_ATTEMPT_KEY]);
}

/** Reauthenticate only the already-authenticated local-password session actor. */
function hrm_ess_self_change_route_reauthenticate_local($password)
{
    if (!hrm_ess_self_change_route_authenticated() || !is_string($password) || $password === '' || strlen($password) > 512)
        return false;
    $user = $_SESSION['wa_current_user'];
    if (!isset($user->auth_method) || (string)$user->auth_method !== 'local_password')
        return false;
    $actor = function_exists('hrm_ess_self_change_actor_id') ? (int)hrm_ess_self_change_actor_id() : 0;
    if ($actor <= 0) return false;
    $result = db_query('SELECT id,password,inactive FROM '.TB_PREF.'users WHERE id='.$actor.' LIMIT 2',
        'could not reauthenticate ESS self-change actor');
    if (!$result || db_num_rows($result) !== 1) return false;
    $row = db_fetch_assoc($result);
    if (!$row || (int)$row['id'] !== $actor || (int)$row['inactive'] !== 0 || !is_string($row['password']) || $row['password'] === '')
        return false;
    if (password_verify($password, (string)$row['password'])) return true;
    if (preg_match('/^[a-f0-9]{32}$/Di', (string)$row['password']) !== 1) return false;
    $legacy = md5($password);
    return function_exists('hash_equals') ? hash_equals(strtolower((string)$row['password']), $legacy)
        : strtolower((string)$row['password']) === $legacy;
}

function hrm_ess_self_change_route_nonce()
{
    try { $bytes = random_bytes(24); } catch (Exception $e) { return false; }
    return is_string($bytes) && strlen($bytes) === 24 ? bin2hex($bytes) : false;
}

/** Return only the current actor's bounded contact/private projection. */
function hrm_ess_self_change_route_current_values()
{
    $scope = get_hrm_ess_current_self_scope();
    if (!is_array($scope)) return false;
    $current = hrm_employee_projection_build((int)$scope['employee_number']);
    if (!is_array($current)) return false;
    $out = array();
    foreach (hrm_ess_self_change_allowed_fields() as $field)
        $out[$field] = array_key_exists($field,$current) && $current[$field] !== null ? (string)$current[$field] : '';
    return $out;
}

/** Return at most one open request owned by the current session actor; no encrypted payload is exposed. */
function hrm_ess_self_change_route_open_request()
{
    $actor = hrm_ess_self_change_actor_id();
    $scope = get_hrm_ess_current_self_scope();
    if ($actor <= 0 || !is_array($scope) || (int)$scope['user_id'] !== $actor) return false;
    $result = db_query('SELECT request_id,request_status,expires_at,row_version,created_at FROM '.TB_PREF.'hrm_ess_self_change_requests '
        .'WHERE user_id='.$actor.' AND worker_id='.(int)$scope['worker_id']." AND request_status IN ('draft','approved') "
        .'AND expires_at>UTC_TIMESTAMP() ORDER BY request_id LIMIT 2', 'could not read current ESS self-change request');
    if (!$result) return false;
    if (db_num_rows($result) === 0) return null;
    if (db_num_rows($result) !== 1) return false;
    $row = db_fetch_assoc($result);
    return $row ? array('request_id'=>(int)$row['request_id'],'request_status'=>(string)$row['request_status'],
        'expires_at'=>(string)$row['expires_at'],'row_version'=>(int)$row['row_version'],'created_at'=>(string)$row['created_at']) : false;
}

/* Phase-1 successor browser controls: local-password or action-bound federated reauthentication. */
if (!defined('HRM_ESS_SELF_CHANGE_ROUTE_PHASE1_DB_VERSION')) define('HRM_ESS_SELF_CHANGE_ROUTE_PHASE1_DB_VERSION','1.0.453');
if (!defined('HRM_ESS_SELF_CHANGE_ROUTE_ASSURED_TTL')) define('HRM_ESS_SELF_CHANGE_ROUTE_ASSURED_TTL',300);
if (!defined('HRM_ESS_SELF_CHANGE_ROUTE_MAX_CODE_BYTES')) define('HRM_ESS_SELF_CHANGE_ROUTE_MAX_CODE_BYTES',4096);
if (!function_exists('hrm_ess_self_change_reauth_action_context'))
    include_once(dirname(__FILE__).DIRECTORY_SEPARATOR.'hrm_ess_self_change_reauthentication.inc');

function hrm_ess_self_change_route_ready_for($version)
{
    return is_string($version) && preg_match('/^1\.0\.[0-9]+$/D',$version)===1
        && function_exists('get_company_pref')
        && version_compare((string)get_company_pref('version_id',true),$version,'>=');
}

function hrm_ess_self_change_route_auth_method()
{
    return function_exists('hrm_ess_self_change_reauth_current_auth_method')
        ? hrm_ess_self_change_reauth_current_auth_method() : '';
}

function hrm_ess_self_change_route_supported_auth_method()
{
    return in_array(hrm_ess_self_change_route_auth_method(),array('local_password','federated_oidc'),true);
}

function hrm_ess_self_change_route_decode_component($value,$plus_as_space=false)
{
    if(!is_string($value)||strlen($value)>HRM_ESS_SELF_CHANGE_ROUTE_MAX_CODE_BYTES*2)return false;
    if(!$plus_as_space && strpos($value,'+')!==false)return false;
    $decoded=rawurldecode($value);
    return preg_match('/[\x00\x0d\x0a]/',$decoded)?false:$decoded;
}

/** Parse only exact OIDC state/code or state/error callback parameters. */
function hrm_ess_self_change_route_parse_callback($method,$raw_query)
{
    if($method!=='GET'||!is_string($raw_query)||$raw_query===''||strlen($raw_query)>8192)return false;
    $values=array();
    foreach(explode('&',$raw_query) as $part){
        $pair=explode('=',$part,2);if(count($pair)!==2)return false;
        $key=rawurldecode($pair[0]);$value=hrm_ess_self_change_route_decode_component($pair[1],false);
        if($value===false||!in_array($key,array('state','code','error','error_description'),true)||array_key_exists($key,$values))return false;
        $values[$key]=$value;
    }
    if(!isset($values['state'])||preg_match('/^[A-Za-z0-9_-]{43}$/D',(string)$values['state'])!==1)return false;
    $has_code=isset($values['code']);$has_error=isset($values['error']);if($has_code===$has_error)return false;
    if($has_code){
        if(count($values)!==2||strlen((string)$values['code'])<1||strlen((string)$values['code'])>HRM_ESS_SELF_CHANGE_ROUTE_MAX_CODE_BYTES
            ||preg_match('/[\x00-\x20\x7f]/',(string)$values['code']))return false;
        return array('kind'=>'code','state'=>(string)$values['state'],'code'=>(string)$values['code']);
    }
    if(count($values)>3||strlen((string)$values['error'])<1||strlen((string)$values['error'])>64
        ||preg_match('/^[A-Za-z0-9_.-]+$/D',(string)$values['error'])!==1)return false;
    if(isset($values['error_description'])&&(strlen((string)$values['error_description'])>256
        ||preg_match('/[\x00-\x1f\x7f]/',(string)$values['error_description'])))return false;
    return array('kind'=>'error','state'=>(string)$values['state'],'error'=>(string)$values['error']);
}

function hrm_ess_self_change_route_callback_path($script_name,$suffix)
{
    if(!is_string($script_name)||!is_string($suffix)||$suffix===''||$suffix[0]!=='/')return false;
    return substr($script_name,-strlen($suffix))===$suffix?$script_name:false;
}

function hrm_ess_self_change_route_redirect_local($script_name,$suffix,$result)
{
    $path=hrm_ess_self_change_route_callback_path($script_name,$suffix);if($path===false||headers_sent())return false;
    $fixed=array('assured'=>'assured','retry'=>'retry','failed'=>'failed','success'=>'success');$status=isset($fixed[$result])?$fixed[$result]:'failed';
    header('HTTP/1.1 303 See Other');header('Location: '.$path.'?step_up='.$status);return true;
}

function hrm_ess_self_change_route_redirect_provider($authorization_url)
{
    if(!function_exists('federation_oidc_valid_https_uri')||!federation_oidc_valid_https_uri($authorization_url,true)||headers_sent())return false;
    header('HTTP/1.1 303 See Other');header('Location: '.$authorization_url);return true;
}

function hrm_ess_self_change_route_terminalize_error($company,$state,$error)
{
    if(!function_exists('federation_oidc_acquire_callback_claim'))require_once(dirname(__FILE__).DIRECTORY_SEPARATOR.'federation_oidc_callback_claim.inc');
    $claim=federation_oidc_acquire_callback_claim((int)$company,$state,15);if(!is_array($claim)||!isset($claim['claim_token']))return false;
    $reason=$error==='access_denied'?'exchange_rejected':'exchange_uncertain';
    return federation_oidc_terminalize_callback_claim((int)$company,(string)$claim['claim_token'],$reason)!==false;
}

/** Retain only proof metadata; profile values/passwords/tokens never enter this session record. */
function hrm_ess_self_change_route_bind_assured($result,$now=null)
{
    $now=$now===null?time():(int)$now;
    if(!is_array($result)||($result['status']??'')!=='assured'||$now<=0
        ||!isset($result['session_id'],$result['operation'],$result['request_id'],$result['intent_digest'],$result['action_class'],$result['action_hash'],$result['assurance_event_id']))return false;
    if(hrm_ess_self_change_reauth_operation((string)$result['operation'])===false||preg_match('/^[a-f0-9]{64}$/D',(string)$result['action_hash'])!==1)return false;
    $_SESSION['hrm_ess_self_change_route_assured']=array(
        'session_id'=>(int)$result['session_id'],'operation'=>(string)$result['operation'],'request_id'=>(int)$result['request_id'],
        'intent_digest'=>(string)$result['intent_digest'],'action_class'=>(string)$result['action_class'],'action_hash'=>(string)$result['action_hash'],
        'assurance_event_id'=>(int)$result['assurance_event_id'],'expires_at_epoch'=>$now+HRM_ESS_SELF_CHANGE_ROUTE_ASSURED_TTL);
    return true;
}

function hrm_ess_self_change_route_assured($operation=null,$now=null)
{
    $now=$now===null?time():(int)$now;$row=$_SESSION['hrm_ess_self_change_route_assured']??false;
    if(!is_array($row)||$now<=0||(int)($row['expires_at_epoch']??0)<=$now
        ||(int)($row['session_id']??0)!==(isset($_SESSION['security_session_registry_id'])?(int)$_SESSION['security_session_registry_id']:0))return false;
    if($operation!==null&&(string)($row['operation']??'')!==(string)$operation)return false;
    return $row;
}
function hrm_ess_self_change_route_clear_assured(){unset($_SESSION['hrm_ess_self_change_route_assured'],$_SESSION['hrm_ess_self_change_route_confirm_csrf']);}

function hrm_ess_self_change_route_issue_confirm_csrf($assured,$now=null)
{
    $now=$now===null?time():(int)$now;if(!is_array($assured)||$now<=0)return false;
    try{$bytes=random_bytes(32);}catch(Exception $e){return false;}$token=rtrim(strtr(base64_encode($bytes),'+/','-_'),'=');
    if(preg_match('/^[A-Za-z0-9_-]{43}$/D',$token)!==1)return false;
    $_SESSION['hrm_ess_self_change_route_confirm_csrf']=array('hash'=>hash('sha256',$token),'action_hash'=>(string)$assured['action_hash'],
        'session_id'=>(int)$assured['session_id'],'expires_at_epoch'=>$now+HRM_ESS_SELF_CHANGE_ROUTE_ASSURED_TTL);return $token;
}
function hrm_ess_self_change_route_consume_confirm_csrf($assured,$token,$now=null)
{
    $now=$now===null?time():(int)$now;$row=$_SESSION['hrm_ess_self_change_route_confirm_csrf']??false;unset($_SESSION['hrm_ess_self_change_route_confirm_csrf']);
    return is_array($assured)&&is_array($row)&&is_string($token)&&preg_match('/^[A-Za-z0-9_-]{43}$/D',$token)===1
        &&(int)$row['expires_at_epoch']>$now&&(int)$row['session_id']===(int)$assured['session_id']
        &&hash_equals((string)$row['action_hash'],(string)$assured['action_hash'])&&hash_equals((string)$row['hash'],hash('sha256',$token));
}

/** Stable server nonce for one exact action hash; never caller controlled. */
function hrm_ess_self_change_route_operation_nonce($action_hash,$consume=false)
{
    if(!is_string($action_hash)||preg_match('/^[a-f0-9]{64}$/D',$action_hash)!==1)return false;
    if(!isset($_SESSION['hrm_ess_self_change_operation_nonces'])||!is_array($_SESSION['hrm_ess_self_change_operation_nonces']))$_SESSION['hrm_ess_self_change_operation_nonces']=array();
    if(!isset($_SESSION['hrm_ess_self_change_operation_nonces'][$action_hash])){
        $nonce=hrm_ess_self_change_route_nonce();if($nonce===false)return false;
        $_SESSION['hrm_ess_self_change_operation_nonces'][$action_hash]=$nonce;
    }
    $nonce=(string)$_SESSION['hrm_ess_self_change_operation_nonces'][$action_hash];
    if($consume)unset($_SESSION['hrm_ess_self_change_operation_nonces'][$action_hash]);
    return $nonce;
}

/** Execute a governed command under one exact proof. Federated proof is consumed in the same outer transaction. */
function hrm_ess_self_change_route_run_governed($action,$local_password,$callable)
{
    if(!is_array($action)||!is_callable($callable))return false;
    $method=hrm_ess_self_change_route_auth_method();
    if($method==='local_password'){
        if(!hrm_ess_self_change_reauth_local_password((string)$local_password))return false;
        return call_user_func($callable);
    }
    if($method!=='federated_oidc')return false;
    $assured=hrm_ess_self_change_route_assured((string)$action['operation']);
    if(!is_array($assured)||!hash_equals((string)$assured['action_hash'],(string)$action['action_hash']))return false;
    begin_transaction();
    if(hrm_ess_self_change_reauth_consume_federated($action)===false){cancel_transaction();return false;}
    $result=call_user_func($callable);
    if($result===false){cancel_transaction();return false;}
    /* Command-level nested commits leave this outer transaction open. */
    global $transaction_level;if((int)$transaction_level!==1){cancel_transaction();return false;}
    commit_transaction();hrm_ess_self_change_route_clear_assured();return $result;
}
