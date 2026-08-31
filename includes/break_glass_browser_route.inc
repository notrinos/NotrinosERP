<?php
/**
 * PAY-SEC-004 authenticated break-glass browser activation/return route policy.
 *
 * This adapter composes the accepted activation, durable-session and restricted
 * consumer foundations. It never grants a role/security area, adopts a
 * federated session, accepts an anonymous request or widens payroll authority.
 */
if (!defined('BREAK_GLASS_BROWSER_ROUTE_ACTIVE_DB_VERSION'))
    define('BREAK_GLASS_BROWSER_ROUTE_ACTIVE_DB_VERSION', '1.0.284');
if (!defined('BREAK_GLASS_BROWSER_ROUTE_CSRF_KEY'))
    define('BREAK_GLASS_BROWSER_ROUTE_CSRF_KEY', 'pay_sec_004_break_glass_browser_route_csrf');
if (!defined('BREAK_GLASS_BROWSER_ROUTE_CSRF_TTL'))
    define('BREAK_GLASS_BROWSER_ROUTE_CSRF_TTL', 300);

function break_glass_browser_route_random_bytes($length)
{
    $length=(int)$length;
    if($length<=0 || !function_exists('random_bytes')) return false;
    try { return random_bytes($length); } catch(Exception $e) { return false; }
}

function break_glass_browser_route_base64url($bytes)
{
    if(!is_string($bytes) || $bytes==='') return false;
    return rtrim(strtr(base64_encode($bytes),'+/','-_'),'=');
}

function break_glass_browser_route_action($action)
{
    return is_string($action) && in_array($action,array('activate','return_to_normal'),true) ? $action : false;
}

/** Issue one action-scoped token; session custody contains only its digest. */
function break_glass_browser_route_issue_csrf($company,$user_id,$action,$now_epoch=null)
{
    $company=(int)$company;$user_id=(int)$user_id;$action=break_glass_browser_route_action($action);
    $now=$now_epoch===null?time():$now_epoch;
    if($company<0 || $user_id<=0 || $action===false || !is_int($now) || $now<=0) return false;
    $raw=break_glass_browser_route_random_bytes(32);
    $token=$raw===false?false:break_glass_browser_route_base64url($raw);
    if(!is_string($token) || preg_match('/^[A-Za-z0-9_-]{43}$/D',$token)!==1) return false;
    if(!isset($_SESSION[BREAK_GLASS_BROWSER_ROUTE_CSRF_KEY]) || !is_array($_SESSION[BREAK_GLASS_BROWSER_ROUTE_CSRF_KEY]))
        $_SESSION[BREAK_GLASS_BROWSER_ROUTE_CSRF_KEY]=array();
    $_SESSION[BREAK_GLASS_BROWSER_ROUTE_CSRF_KEY][$action]=array(
        'digest'=>hash('sha256',$token),'company'=>$company,'user_id'=>$user_id,
        'issued_at_epoch'=>$now,'expires_at_epoch'=>$now+BREAK_GLASS_BROWSER_ROUTE_CSRF_TTL
    );
    return $token;
}

/** Consume the exact action token before any activation/terminalization mutation. */
function break_glass_browser_route_consume_csrf($company,$user_id,$action,$token,$now_epoch=null)
{
    $company=(int)$company;$user_id=(int)$user_id;$action=break_glass_browser_route_action($action);
    $now=$now_epoch===null?time():$now_epoch;
    $entry=false;
    if($action!==false && isset($_SESSION[BREAK_GLASS_BROWSER_ROUTE_CSRF_KEY][$action])
        && is_array($_SESSION[BREAK_GLASS_BROWSER_ROUTE_CSRF_KEY][$action]))
        $entry=$_SESSION[BREAK_GLASS_BROWSER_ROUTE_CSRF_KEY][$action];
    if($action!==false) unset($_SESSION[BREAK_GLASS_BROWSER_ROUTE_CSRF_KEY][$action]);
    if(isset($_SESSION[BREAK_GLASS_BROWSER_ROUTE_CSRF_KEY]) && empty($_SESSION[BREAK_GLASS_BROWSER_ROUTE_CSRF_KEY]))
        unset($_SESSION[BREAK_GLASS_BROWSER_ROUTE_CSRF_KEY]);
    if(!is_array($entry) || !is_string($token) || preg_match('/^[A-Za-z0-9_-]{43}$/D',$token)!==1
        || $company<0 || $user_id<=0 || !is_int($now) || $now<=0
        || !isset($entry['digest'],$entry['company'],$entry['user_id'],$entry['issued_at_epoch'],$entry['expires_at_epoch'])
        || (int)$entry['company']!==$company || (int)$entry['user_id']!==$user_id
        || (int)$entry['issued_at_epoch']<=0 || $now<(int)$entry['issued_at_epoch']
        || $now>(int)$entry['expires_at_epoch']
        || (int)$entry['expires_at_epoch']>(int)$entry['issued_at_epoch']+BREAK_GLASS_BROWSER_ROUTE_CSRF_TTL)
        return false;
    $expected=(string)$entry['digest'];$actual=hash('sha256',$token);
    return function_exists('hash_equals')?hash_equals($expected,$actual):$expected===$actual;
}

function break_glass_browser_route_activation_ready()
{
    if(!defined('TB_PREF') || !function_exists('db_query') || !function_exists('db_fetch_assoc')) return false;
    $version=function_exists('get_company_pref')?get_company_pref('version_id',true):false;
    if((string)$version!==BREAK_GLASS_BROWSER_ROUTE_ACTIVE_DB_VERSION) return false;
    $r=db_query("SELECT COUNT(*) event_count FROM ".TB_PREF."domain_audit_events WHERE entity_type='security_break_glass_browser_route_migration' AND action='security.break_glass.browser_route.activated' AND reason_code='pay_sec_004_break_glass_browser_route_foundation'",'could not verify PAY-SEC-004 break-glass browser route activation');
    $row=$r?db_fetch_assoc($r):false;
    return is_array($row) && (int)$row['event_count']===1;
}

function break_glass_browser_route_security_headers()
{
    if(headers_sent()) return;
    header('Cache-Control: no-store, no-cache, must-revalidate, max-age=0');
    header('Pragma: no-cache');
    header('Referrer-Policy: no-referrer');
    header('X-Frame-Options: DENY');
    header('X-Content-Type-Options: nosniff');
    header('Permissions-Policy: camera=(), microphone=(), geolocation=()');
}

function break_glass_browser_route_load_dependencies()
{
    global $path_to_root;
    $root=isset($path_to_root)?realpath($path_to_root):false;if($root===false)return false;
    foreach(array('includes/break_glass_activation.inc','includes/break_glass_security_recovery_consumer.inc','includes/session_registry.inc') as $relative){
        $file=$root.DIRECTORY_SEPARATOR.str_replace('/',DIRECTORY_SEPARATOR,$relative);if(!is_file($file))return false;require_once($file);
    }
    if(!function_exists('append_domain_audit_event')){
        $file=$root.DIRECTORY_SEPARATOR.'includes'.DIRECTORY_SEPARATOR.'db'.DIRECTORY_SEPARATOR.'domain_audit_db.inc';
        if(!is_file($file))return false;require_once($file);
    }
    return function_exists('break_glass_activate_designated_user') && function_exists('break_glass_end_activation')
        && function_exists('break_glass_security_recovery_consumer_custody_valid')
        && function_exists('session_registry_hash') && function_exists('append_domain_audit_event');
}

/** Lock the exact newly issued lease/session/user/role custody before browser binding. */
function break_glass_browser_route_custody_for_update($session_id)
{
    $session_id=(int)$session_id;if($session_id<=0 || !defined('TB_PREF'))return false;
    $sql="SELECT l.break_glass_activation_lease_id,l.break_glass_designation_id,l.user_id,l.scope_code,l.required_auth_method,l.reason_code,l.activated_at,l.expires_at,l.session_id,l.active_slot,l.terminalized_at,l.terminal_reason,l.row_version,d.scope_code designation_scope_code,d.required_auth_method designation_auth_method,d.max_activation_seconds,d.revoked_at designation_revoked_at,s.session_key_hash,s.user_id session_user_id,s.role_id,s.auth_method,s.assurance_level,s.assurance_method,s.created_at,s.last_seen_at,s.idle_expires_at,s.absolute_expires_at,s.user_agent_hash,s.revoked_at session_revoked_at,u.role_id current_role_id,u.inactive user_inactive,r.inactive role_inactive FROM ".TB_PREF."user_break_glass_activation_leases l INNER JOIN ".TB_PREF."user_break_glass_designations d ON d.break_glass_designation_id=l.break_glass_designation_id INNER JOIN ".TB_PREF."user_sessions s ON s.id=l.session_id AND s.user_id=l.user_id INNER JOIN ".TB_PREF."users u ON u.id=l.user_id AND u.id=d.user_id INNER JOIN ".TB_PREF."security_roles r ON r.id=u.role_id WHERE l.session_id=".$session_id." FOR UPDATE";
    $r=db_query($sql,'could not lock PAY-SEC-004 break-glass browser binding custody');if(!$r)return false;
    $row=db_fetch_assoc($r);if(!is_array($row) || db_fetch_assoc($r)!==false)return false;return $row;
}

function break_glass_browser_route_binding_audit($company,$row,$old_session_id)
{
    return append_domain_audit_event(array(
        'company_id'=>(int)$company,'legal_entity_id'=>0,'entity_type'=>'security_break_glass_activation_lease',
        'entity_key'=>array('break_glass_activation_lease_id'=>(int)$row['break_glass_activation_lease_id'],'session_id'=>(int)$row['session_id'],'user_id'=>(int)$row['user_id']),
        'action'=>'security.break_glass.browser_session.bound','reason_code'=>'pay_sec_004_break_glass_browser_route',
        'before'=>array('browser_session_bound'=>false,'previous_session_id'=>(int)$old_session_id),
        'after'=>array('browser_session_bound'=>true,'session_id'=>(int)$row['session_id'],'expires_at'=>(string)$row['expires_at']),
        'restricted_diff'=>array('fixed_scope'=>'security_recovery','required_auth_method'=>'local_password','hard_php_session_rotation'=>true,
            'previous_durable_session_revoked'=>true,'general_browser_adoption_added'=>false,'existing_role_preserved'=>(int)$row['role_id'],
            'role_granted'=>false,'security_area_granted'=>false,'assurance_level'=>1,'assurance_elevated'=>false,
            'raw_session_secret_stored'=>false,'plaintext_password_stored'=>false,'provider_bypass_added'=>false,
            'payroll_post_authority_changed'=>false,'payment_authority_changed'=>false,'accounting_authority_changed'=>false))
    )!==false;
}

/**
 * Hard-rotate the already authenticated PHP session and bind only the newly
 * issued break-glass durable session. The application role set is untouched.
 */
function break_glass_browser_route_bind_session($company,$user_id,$session_id,$session_secret,$now_epoch=null)
{
    global $Session_manager;
    $company=(int)$company;$user_id=(int)$user_id;$session_id=(int)$session_id;$now=$now_epoch===null?time():$now_epoch;
    if(!break_glass_browser_route_load_dependencies() || $company<0 || $user_id<=0 || $session_id<=0
        || !is_string($session_secret) || strlen($session_secret)<32 || !is_int($now) || $now<=0
        || !isset($_SESSION['wa_current_user']) || !is_object($_SESSION['wa_current_user'])
        || !method_exists($_SESSION['wa_current_user'],'logged_in') || !$_SESSION['wa_current_user']->logged_in()
        || (int)$_SESSION['wa_current_user']->user!==$user_id
        || (function_exists('user_company')?(int)user_company():(int)$_SESSION['wa_current_user']->company)!==$company
        || !isset($_SESSION['security_session_registry_id'],$_SESSION['security_session_secret'])
        || !is_object($Session_manager) || !method_exists($Session_manager,'regenerateSession')) return false;
    $old_session_id=(int)$_SESSION['security_session_registry_id'];$old_secret=(string)$_SESSION['security_session_secret'];
    if($old_session_id<=0 || $old_session_id===$session_id || strlen($old_secret)<32) return false;
    $request=array('company'=>$company,'user_id'=>$user_id,'role_id'=>(int)$_SESSION['wa_current_user']->access,
        'security_area'=>'SA_CHGPASSWD','session_id'=>$session_id,'session_key_hash'=>session_registry_hash($session_secret),
        'user_agent_hash'=>session_registry_user_agent_hash(),'now_epoch'=>$now);
    $preview=break_glass_security_recovery_consumer_custody($session_id);
    if(!break_glass_security_recovery_consumer_custody_valid($preview,$request)) return false;
    if(!$Session_manager->regenerateSession(true)) return false;
    if(!function_exists('begin_transaction') || !function_exists('commit_transaction') || !function_exists('cancel_transaction')) return false;
    begin_transaction();$committed=false;
    do {
        $row=break_glass_browser_route_custody_for_update($session_id);
        if(!break_glass_security_recovery_consumer_custody_valid($row,$request)) break;
        $sql="UPDATE ".TB_PREF."user_sessions SET revoked_at=COALESCE(revoked_at,UTC_TIMESTAMP()),revoked_by=COALESCE(revoked_by,".$user_id."),revocation_reason=COALESCE(revocation_reason,'break_glass_browser_bound') WHERE id=".$old_session_id." AND user_id=".$user_id." AND revoked_at IS NULL";
        if(!db_query($sql,'could not revoke prior durable session during PAY-SEC-004 break-glass browser binding')
            || !function_exists('db_num_affected_rows') || (int)db_num_affected_rows()!==1) break;
        if(!break_glass_browser_route_binding_audit($company,$row,$old_session_id)) break;
        commit_transaction();$committed=true;
    } while(false);
    if(!$committed){cancel_transaction();return false;}
    $_SESSION['security_session_registry_id']=$session_id;
    $_SESSION['security_session_secret']=$session_secret;
    $_SESSION['wa_current_user']->auth_method='local_password';
    return array('session_id'=>$session_id,'previous_session_id'=>$old_session_id,'user_id'=>$user_id,
        'browser_session_bound'=>true,'hard_php_session_rotation'=>true,'role_granted'=>false,'security_area_granted'=>false);
}

/** Compose activation + bounded browser binding; cleanup the new lease if binding fails. */
function break_glass_browser_route_activate_current_user($company,$user_id,$password,$reason_code,$ticket_reference,$requested_seconds,$now_epoch=null)
{
    $now=$now_epoch===null?time():$now_epoch;
    if(!break_glass_browser_route_load_dependencies() || !is_int($now) || $now<=0) return false;
    $raw=break_glass_browser_route_random_bytes(32);$secret=$raw===false?false:break_glass_browser_route_base64url($raw);
    if(!is_string($secret) || strlen($secret)<32) return false;
    $activation=break_glass_activate_designated_user((int)$company,(int)$user_id,$password,$reason_code,$ticket_reference,(int)$requested_seconds,$secret,$now);
    if(!is_array($activation) || !isset($activation['session_id'])) return false;
    $binding=break_glass_browser_route_bind_session((int)$company,(int)$user_id,(int)$activation['session_id'],$secret,$now);
    if(!is_array($binding)){
        break_glass_end_activation((int)$company,(int)$user_id,'return_to_normal',$now);
        return false;
    }
    return array('break_glass_activation_lease_id'=>(int)$activation['break_glass_activation_lease_id'],
        'session_id'=>(int)$activation['session_id'],'user_id'=>(int)$user_id,'expires_at'=>(string)$activation['expires_at'],
        'browser_session_bound'=>true,'role_granted'=>false,'security_area_granted'=>false);
}
