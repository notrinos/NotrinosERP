<?php
/**
 * PAY-SEC-004 federated browser/PHP session adoption command.
 *
 * Internal command only. It adopts one already-issued provenance-bound
 * federated_oidc durable session into the current anonymous PHP session. The
 * command never authenticates by password/profile claims, never creates an
 * external-subject link, never issues a durable session and never elevates
 * assurance above the already-issued level-one OIDC session.
 */
require_once(dirname(__FILE__).DIRECTORY_SEPARATOR.'session_registry.inc');

/** Validate and normalize non-database browser-adoption inputs. */
function federation_browser_session_adoption_request($company,$session_id,$session_secret,$now_epoch=null)
{
    $company=(int)$company;$session_id=(int)$session_id;
    $now=$now_epoch===null?time():$now_epoch;
    if($company<0||$session_id<=0||!is_string($session_secret)||strlen($session_secret)<32
        ||!is_int($now)||$now<=0)return false;
    if(function_exists('user_company')&&(int)user_company()!==$company)return false;
    return array('company'=>$company,'session_id'=>$session_id,
        'session_key_hash'=>session_registry_hash($session_secret),
        'session_secret'=>$session_secret,'user_agent_hash'=>session_registry_user_agent_hash(),
        'now_epoch'=>$now);
}

/**
 * Lock the exact issued session plus immutable federation provenance and the
 * current provider/configuration/link/user/role custody needed for adoption.
 */
function federation_browser_session_adoption_custody_for_update($session_id)
{
    $session_id=(int)$session_id;if($session_id<=0||!defined('TB_PREF'))return false;
    $sql="SELECT s.id session_id,s.session_key_hash,s.user_id,s.role_id,s.auth_method,s.assurance_level,s.assurance_method,"
        ."s.assurance_issued_at,s.assurance_expires_at,s.created_at,s.last_seen_at,s.idle_expires_at,s.absolute_expires_at,s.revoked_at,s.user_agent_hash,"
        ."b.federation_session_binding_id,b.verifier_decision_id,b.external_subject_link_id,"
        ."d.verifier_transaction_id,d.external_subject_hash,d.verified_assurance_level,d.assurance_method decision_assurance_method,d.verified_at,d.valid_until,"
        ."t.verifier_config_id,t.consumed_at transaction_consumed_at,c.provider_id,c.max_clock_skew_seconds,c.assurance_level_cap,c.config_status,p.provider_status,"
        ."l.user_id linked_user_id,l.link_status,l.deprovisioned_at,"
        ."u.login_id,u.real_name,u.email,u.language,u.date_format,u.date_sep,u.tho_sep,u.dec_sep,u.theme,u.page_size,u.prices_dec,u.qty_dec,u.rates_dec,u.percent_dec,"
        ."u.show_gl,u.show_codes,u.show_hints,u.query_size,u.graphic_links,u.pos,u.print_profile,u.rep_popup,u.sticky_doc_date,u.startup_tab,u.transaction_days,"
        ."u.save_report_selections,u.use_date_picker,u.def_print_destination,u.def_print_orientation,u.inactive user_inactive,"
        ."r.sections role_sections,r.areas role_areas,r.inactive role_inactive "
        ."FROM ".TB_PREF."user_sessions s "
        ."INNER JOIN ".TB_PREF."user_federation_session_bindings b ON b.session_id=s.id "
        ."INNER JOIN ".TB_PREF."user_federation_verifier_decisions d ON d.verifier_decision_id=b.verifier_decision_id "
        ."INNER JOIN ".TB_PREF."user_federation_verifier_transactions t ON t.verifier_transaction_id=d.verifier_transaction_id "
        ."INNER JOIN ".TB_PREF."user_federation_provider_verifier_configs c ON c.verifier_config_id=t.verifier_config_id "
        ."INNER JOIN ".TB_PREF."user_federation_providers p ON p.provider_id=c.provider_id "
        ."INNER JOIN ".TB_PREF."user_federation_external_subject_links l ON l.external_subject_link_id=b.external_subject_link_id "
        ." AND l.provider_id=c.provider_id AND l.external_subject_hash=d.external_subject_hash "
        ."INNER JOIN ".TB_PREF."users u ON u.id=s.user_id AND u.id=l.user_id "
        ."INNER JOIN ".TB_PREF."security_roles r ON r.id=u.role_id "
        ."WHERE s.id=$session_id LIMIT 2 FOR UPDATE";
    $result=db_query($sql,'could not lock PAY-SEC-004 browser-session adoption custody');
    if(!$result)return false;$row=db_fetch_assoc($result);
    if(!$row||db_fetch_assoc($result)!==false)return false;
    return $row;
}

/** Return whether the locked durable session remains an adoptable level-one session. */
function federation_browser_session_adoption_custody_valid($row,$request)
{
    if(!is_array($row)||!is_array($request))return false;
    $now=(int)$request['now_epoch'];$skew=(int)$row['max_clock_skew_seconds'];
    $idle=strtotime((string)$row['idle_expires_at'].' UTC');
    $absolute=strtotime((string)$row['absolute_expires_at'].' UTC');
    $assurance_expires=strtotime((string)$row['assurance_expires_at'].' UTC');
    $assurance_issued=strtotime((string)$row['assurance_issued_at'].' UTC');
    $decision_valid=strtotime((string)$row['valid_until'].' UTC');
    $verified_at=strtotime((string)$row['verified_at'].' UTC');
    return (int)$row['session_id']===(int)$request['session_id']
        &&(int)$row['user_id']===(int)$row['linked_user_id']
        &&(int)$row['role_id']>0
        &&(string)$row['auth_method']==='federated_oidc'
        &&(int)$row['assurance_level']===1&&(string)$row['assurance_method']==='oidc_rs256'
        &&(int)$row['verified_assurance_level']===1&&(string)$row['decision_assurance_method']==='oidc_rs256'
        &&(int)$row['assurance_level_cap']>=1
        &&(string)$row['provider_status']==='enabled'&&(string)$row['config_status']==='enabled'
        &&(string)$row['link_status']==='active'&&$row['deprovisioned_at']===null
        &&$row['transaction_consumed_at']!==null&&$row['revoked_at']===null
        &&(int)$row['user_inactive']===0&&(int)$row['role_inactive']===0
        &&$idle!==false&&$absolute!==false&&$assurance_expires!==false&&$assurance_issued!==false
        &&$decision_valid!==false&&$verified_at!==false
        &&$now<$idle&&$now<$absolute&&$now<$assurance_expires&&$now<$decision_valid
        &&$assurance_issued<=$now+$skew&&$verified_at<=$now+$skew
        &&session_registry_hash_equals((string)$row['session_key_hash'],(string)$request['session_key_hash'])
        &&session_registry_hash_equals((string)$row['user_agent_hash'],(string)$request['user_agent_hash']);
}

/** Build the current user's approved modern-role area set without authentication hooks. */
function federation_browser_session_adoption_role_set($row)
{
    if(!is_array($row))return false;
    $sections=array();foreach(explode(';',(string)$row['role_sections']) as $section)
        if($section!=='')$sections[]=(int)$section;
    $areas=array();foreach(explode(';',(string)$row['role_areas']) as $area){
        if($area==='')continue;$code=(int)$area;
        if(in_array($code&~0xff,$sections,true))$areas[]=$code;
    }
    return $areas;
}

/**
 * Hard-rotate the anonymous PHP session and install only the already-bound
 * current_user/session-registry state. No credential/authentication hook runs.
 */
function federation_browser_session_adoption_apply_php_session($company,$row,$request)
{
    global $Session_manager;
    if(!isset($_SESSION['wa_current_user'])||!is_object($_SESSION['wa_current_user'])
        ||!method_exists($_SESSION['wa_current_user'],'logged_in')||$_SESSION['wa_current_user']->logged_in()
        ||!is_object($Session_manager)||!method_exists($Session_manager,'regenerateSession')
        ||!class_exists('user_prefs'))return false;
    $role_set=federation_browser_session_adoption_role_set($row);if($role_set===false)return false;
    if(!$Session_manager->regenerateSession(true))return false;
    if(isset($_SESSION['language'])&&is_object($_SESSION['language'])&&method_exists($_SESSION['language'],'set_language')){
        $_SESSION['language']->set_language((string)$row['language']);
        if(function_exists('db_set_encoding')&&isset($_SESSION['language']->encoding))db_set_encoding($_SESSION['language']->encoding);
    }
    $user=$_SESSION['wa_current_user'];
    $user->set_company((int)$company);$user->old_db=false;$user->role_set=$role_set;
    $user->access=(int)$row['role_id'];$user->name=(string)$row['real_name'];$user->pos=(int)$row['pos'];
    $user->loginname=(string)$row['login_id'];$user->username=$user->loginname;$user->prefs=new user_prefs($row);
    $user->user=(int)$row['user_id'];$user->email=(string)$row['email'];$user->logged=true;$user->login_attempt=0;
    $user->last_act=(int)$request['now_epoch'];$user->timeout=function_exists('session_timeout')?session_timeout():SESSION_REGISTRY_IDLE_LIFETIME;
    $user->auth_method='federated_oidc';
    $_SESSION['security_session_registry_id']=(int)$row['session_id'];
    $_SESSION['security_session_secret']=(string)$request['session_secret'];
    return true;
}

/** Append restricted browser-adoption evidence while federation custody is locked. */
function federation_browser_session_adoption_audit($company,$row)
{
    return append_domain_audit_event(array('company_id'=>(int)$company,'legal_entity_id'=>0,
        'entity_type'=>'security_federation_session','entity_key'=>array('session_id'=>(int)$row['session_id'],
            'verifier_decision_id'=>(int)$row['verifier_decision_id'],'external_subject_link_id'=>(int)$row['external_subject_link_id']),
        'action'=>'security.federation.session.browser_adopted','reason_code'=>'pay_sec_004_federated_browser_session_adoption',
        'before'=>array('browser_session_activated'=>false),'after'=>array('browser_session_activated'=>true),
        'restricted_diff'=>array('provider_id'=>(int)$row['provider_id'],'verifier_config_id'=>(int)$row['verifier_config_id'],
            'user_id'=>(int)$row['user_id'],'role_id'=>(int)$row['role_id'],'auth_method'=>'federated_oidc',
            'assurance_level'=>1,'assurance_method'=>'oidc_rs256','hard_php_session_rotation'=>true,
            'durable_session_issued'=>false,'session_provenance_reused'=>true,'raw_session_secret_stored'=>false,
            'raw_external_subject_stored'=>false,'raw_token_stored'=>false,'email_display_name_login_auto_linking'=>false,
            'first_link_written'=>false,'provider_or_config_mutated'=>false,'external_link_mutated'=>false,
            'targeted_revocation_written'=>false,'assurance_elevated'=>false,'recovery_factor_written'=>false,
            'role_granted'=>false,'payment_authority_changed'=>false,'payroll_authority_changed'=>false,
            'accounting_authority_changed'=>false)))!==false;
}

/** Return an idempotent success only for the exact already-adopted PHP session. */
function federation_browser_session_adoption_current_session_retry($request)
{
    if(!isset($_SESSION['wa_current_user'])||!is_object($_SESSION['wa_current_user'])
        ||!method_exists($_SESSION['wa_current_user'],'logged_in')||!$_SESSION['wa_current_user']->logged_in()
        ||!isset($_SESSION['security_session_registry_id'])||!isset($_SESSION['security_session_secret']))return false;
    if((int)$_SESSION['security_session_registry_id']!==(int)$request['session_id']
        ||!session_registry_hash_equals(session_registry_hash((string)$_SESSION['security_session_secret']),$request['session_key_hash'])
        ||!session_registry_hash_equals(session_registry_hash((string)$_SESSION['security_session_secret']),session_registry_hash((string)$request['session_secret'])))return false;
    $user=$_SESSION['wa_current_user'];
    if((int)$user->company!==(int)$request['company']||(string)$user->auth_method!=='federated_oidc')return false;
    $valid=session_registry_validate_current_session();if($valid!==true)return false;
    return array('session_id'=>(int)$request['session_id'],'user_id'=>(int)$user->user,'role_id'=>(int)$user->access,
        'browser_session_activated'=>true,'assurance_level'=>1,'idempotent_retry'=>true);
}

/**
 * Adopt an already-issued federated_oidc durable session into this browser.
 *
 * On failure before hard rotation the anonymous session is left untouched. On
 * any failure after hard rotation, the PHP login is destroyed; database/audit
 * work is rolled back and the caller must restart or exact-retry the controlled
 * federation flow while its server-confined session secret remains available.
 */
function federation_adopt_existing_browser_session($company,$session_id,$session_secret,$now_epoch=null)
{
    global $path_to_root;
    $request=federation_browser_session_adoption_request($company,$session_id,$session_secret,$now_epoch);
    if($request===false)return false;
    $retry=federation_browser_session_adoption_current_session_retry($request);if(is_array($retry))return $retry;
    if(isset($_SESSION['wa_current_user'])&&is_object($_SESSION['wa_current_user'])
        &&method_exists($_SESSION['wa_current_user'],'logged_in')&&$_SESSION['wa_current_user']->logged_in())return false;
    if(!function_exists('append_domain_audit_event')){
        $root=isset($path_to_root)?realpath($path_to_root):false;if($root===false)return false;
        $audit=$root.DIRECTORY_SEPARATOR.'includes'.DIRECTORY_SEPARATOR.'db'.DIRECTORY_SEPARATOR.'domain_audit_db.inc';
        if(!is_file($audit))return false;require_once($audit);
    }
    if(!function_exists('append_domain_audit_event')||!function_exists('begin_transaction')||!function_exists('commit_transaction')||!function_exists('cancel_transaction'))return false;
    begin_transaction();$rotated=false;$committed=false;$result_value=false;
    do {
        $row=federation_browser_session_adoption_custody_for_update($request['session_id']);if(!is_array($row))break;
        if(!federation_browser_session_adoption_custody_valid($row,$request))break;
        if(!federation_browser_session_adoption_apply_php_session($request['company'],$row,$request))break;
        $rotated=true;
        if(!federation_browser_session_adoption_audit($request['company'],$row))break;
        commit_transaction();$committed=true;
        if(function_exists('update_user_visitdate'))update_user_visitdate((string)$row['login_id']);
        if(function_exists('flush_dir')&&function_exists('user_js_cache'))flush_dir(user_js_cache());
        $result_value=array('session_id'=>(int)$row['session_id'],'user_id'=>(int)$row['user_id'],'role_id'=>(int)$row['role_id'],
            'verifier_decision_id'=>(int)$row['verifier_decision_id'],'external_subject_link_id'=>(int)$row['external_subject_link_id'],
            'browser_session_activated'=>true,'assurance_level'=>1,'idempotent_retry'=>false);
    } while(false);
    if(!$committed){cancel_transaction();if($rotated&&function_exists('kill_login'))kill_login();}
    return $committed?$result_value:false;
}
