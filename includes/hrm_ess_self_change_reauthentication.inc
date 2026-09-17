<?php
/**
 * HRM-ESS-001 local-password and federated OIDC reauthentication boundary.
 *
 * This helper never grants HR authority. It binds one fresh authentication proof
 * to one already-authorized ESS self-change operation and consumes federated
 * assurance inside the caller's business transaction. Raw passwords, OIDC
 * tokens, profile values and recovery material are never persisted here.
 */
if (!defined('HRM_ESS_SELF_CHANGE_REAUTH_DB_VERSION')) define('HRM_ESS_SELF_CHANGE_REAUTH_DB_VERSION','1.0.452');
if (!defined('HRM_ESS_SELF_CHANGE_REAUTH_CSRF_TTL')) define('HRM_ESS_SELF_CHANGE_REAUTH_CSRF_TTL',600);
if (!defined('HRM_ESS_SELF_CHANGE_REAUTH_TRANSACTION_TTL')) define('HRM_ESS_SELF_CHANGE_REAUTH_TRANSACTION_TTL',300);
if (!defined('HRM_ESS_SELF_CHANGE_REAUTH_DEFAULT_LEASE')) define('HRM_ESS_SELF_CHANGE_REAUTH_DEFAULT_LEASE',45);
if (!defined('HRM_ESS_SELF_CHANGE_REAUTH_MAX_PENDING')) define('HRM_ESS_SELF_CHANGE_REAUTH_MAX_PENDING',4);
if (!defined('HRM_ESS_SELF_CHANGE_REAUTH_ACTION_CONTRACT')) define('HRM_ESS_SELF_CHANGE_REAUTH_ACTION_CONTRACT','notrinoserp.hrm-ess-001.self-change-reauth');

function hrm_ess_self_change_reauth_dependencies()
{
    global $path_to_root;
    $base=dirname(__FILE__);
    $required=array(
        'federation_assurance_consumer_current_custody_valid'=>'federation_assurance_consumer.inc',
        'federation_assurance_consumer_restore_level_one_sql'=>'federation_assurance_consumer.inc',
        'federation_oidc_issue_transaction'=>'federation_oidc_transaction.inc',
        'federation_oidc_load_proof'=>'federation_oidc_transaction.inc',
        'federation_oidc_client_config'=>'federation_oidc_transaction.inc',
        'federation_oidc_acquire_callback_claim'=>'federation_oidc_callback_claim.inc',
        'federation_oidc_terminalize_callback_claim'=>'federation_oidc_callback_claim.inc',
        'federation_oidc_exchange_authorization_code'=>'federation_oidc_token_exchange.inc',
        'federation_oidc_token_verifier_verify'=>'federation_oidc_token_verifier.inc',
        'federation_oidc_token_verifier_parse'=>'federation_oidc_token_verifier.inc',
        'federation_oidc_token_verifier_material_contract'=>'federation_oidc_token_material.inc',
        'federation_oidc_assurance_candidate_verify'=>'federation_oidc_assurance_candidate.inc',
        'federation_oidc_assurance_policy'=>'federation_oidc_assurance_candidate.inc',
        'federation_oidc_commit_verified_decision'=>'federation_oidc_decision.inc',
        'federation_issue_privileged_assurance'=>'federation_assurance_issuer.inc',
        'federation_control_load_bundle'=>'federation_control_material.inc',
        'federation_control_hash_equals'=>'federation_control_material.inc',
        'federation_control_valid_sha256'=>'federation_control_material.inc',
        'federation_verifier_dependency_configured_status'=>'federation_verifier_dependencies.inc'
    );
    foreach($required as $fn=>$file){if(!function_exists($fn)){ $p=$base.DIRECTORY_SEPARATOR.$file; if(!is_file($p))return false; require_once($p); }}
    foreach($required as $fn=>$file)if(!function_exists($fn))return false;
    if(!function_exists('get_hrm_ess_self_change_request')){
        $root=isset($path_to_root)?realpath($path_to_root):false;
        $p=$root===false?false:$root.'/hrm/includes/db/ess_self_change_request_db.inc';
        if($p===false||!is_file($p))return false; include_once($p);
    }
    return function_exists('get_hrm_ess_self_change_request') && function_exists('hrm_ess_self_change_actor_id');
}

function hrm_ess_self_change_reauth_operation($value)
{
    $allowed=array('request','cancel','approve','reject','execute');
    return is_string($value)&&in_array($value,$allowed,true)?$value:false;
}

function hrm_ess_self_change_reauth_action_class($operation)
{
    $op=hrm_ess_self_change_reauth_operation($operation); if($op===false)return false;
    return 'hrm.ess.self_change.'.$op;
}

function hrm_ess_self_change_reauth_current_auth_method()
{
    if(!isset($_SESSION['wa_current_user'])||!is_object($_SESSION['wa_current_user'])||!method_exists($_SESSION['wa_current_user'],'logged_in')||!$_SESSION['wa_current_user']->logged_in())return '';
    return isset($_SESSION['wa_current_user']->auth_method)?(string)$_SESSION['wa_current_user']->auth_method:'';
}

function hrm_ess_self_change_reauth_authorized($operation)
{
    $op=hrm_ess_self_change_reauth_operation($operation); if($op===false)return false;
    if($op==='request'||$op==='cancel')return function_exists('hrm_ess_self_change_user_can_request')&&hrm_ess_self_change_user_can_request();
    if($op==='approve'||$op==='reject')return function_exists('hrm_ess_self_change_user_can_approve')&&hrm_ess_self_change_user_can_approve();
    return function_exists('hrm_ess_self_change_user_can_execute')&&hrm_ess_self_change_user_can_execute();
}

function hrm_ess_self_change_reauth_intent_digest($changes)
{
    if(!is_array($changes)||!function_exists('get_hrm_ess_current_self_scope')||!function_exists('hrm_employee_projection_build')||!function_exists('hrm_ess_self_change_normalize_payload'))return false;
    $scope=get_hrm_ess_current_self_scope(); if(!is_array($scope))return false;
    $current=hrm_employee_projection_build((int)$scope['employee_number']); if(!is_array($current))return false;
    $normalized=hrm_ess_self_change_normalize_payload($changes,$current); if(!is_array($normalized)||count($normalized)===0)return false;
    ksort($normalized); $json=json_encode($normalized,JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE);
    return $json===false?false:hash('sha256',$json);
}

function hrm_ess_self_change_reauth_action_hash($company,$actor,$operation,$request_id,$request_digest,$row_version,$intent_digest)
{
    $op=hrm_ess_self_change_reauth_operation($operation); $company=(int)$company; $actor=(int)$actor; $request_id=(int)$request_id; $row_version=(int)$row_version;
    if($op===false||$company<0||$actor<=0||$request_id<0||$row_version<0)return false;
    foreach(array($request_digest,$intent_digest) as $d)if(!is_string($d)||preg_match('/^(?:|[a-f0-9]{64})$/D',$d)!==1)return false;
    return hash('sha256',implode("\0",array(HRM_ESS_SELF_CHANGE_REAUTH_ACTION_CONTRACT,'v1',$company,$actor,$op,$request_id,$request_digest,$row_version,$intent_digest)));
}

/** Build one exact server-bound operation context. */
function hrm_ess_self_change_reauth_action_context($operation,$request_id=0,$intent_digest='')
{
    $op=hrm_ess_self_change_reauth_operation($operation); $actor=hrm_ess_self_change_actor_id(); $company=function_exists('user_company')?(int)user_company():0;
    if($op===false||$actor<=0||!hrm_ess_self_change_reauth_authorized($op)||$company<0)return false;
    $request_id=(int)$request_id; $request_digest=''; $row_version=0;
    if($op==='request'){
        if($request_id!==0||!is_string($intent_digest)||preg_match('/^[a-f0-9]{64}$/D',$intent_digest)!==1)return false;
        $scope=get_hrm_ess_current_self_scope(); if(!is_array($scope)||(int)$scope['user_id']!==$actor)return false;
    }else{
        if($request_id<=0||$intent_digest!=='')return false;
        $request=get_hrm_ess_self_change_request($request_id,false); if(!is_array($request)||!isset($request['request_digest'],$request['request_status'],$request['row_version']))return false;
        $request_digest=(string)$request['request_digest']; $row_version=(int)$request['row_version'];
        if(!function_exists('hrm_ess_self_change_digest_is_valid')||!hrm_ess_self_change_digest_is_valid($request_digest))return false;
        if($op==='cancel'){
            $scope=get_hrm_ess_current_self_scope();
            if(!is_array($scope)||(int)$request['user_id']!==$actor||(int)$request['requested_by']!==$actor||(int)$scope['worker_id']!==(int)$request['worker_id']||!in_array((string)$request['request_status'],array('draft','approved'),true))return false;
        }elseif($op==='approve'||$op==='reject'){
            if((int)$request['requested_by']===$actor||!in_array((string)$request['request_status'],array('draft','approved'),true))return false;
            if($op==='approve'&&(string)$request['request_status']!=='draft')return false;
        }elseif($op==='execute'){
            if((int)$request['requested_by']===$actor||(string)$request['request_status']!=='approved'||(int)$request['approved_by']<=0||(int)$request['approved_by']===$actor)return false;
        }
    }
    $hash=hrm_ess_self_change_reauth_action_hash($company,$actor,$op,$request_id,$request_digest,$row_version,(string)$intent_digest);
    if($hash===false)return false;
    return array('company'=>$company,'actor_id'=>$actor,'operation'=>$op,'request_id'=>$request_id,'request_digest'=>$request_digest,
        'row_version'=>$row_version,'intent_digest'=>(string)$intent_digest,'action_class'=>hrm_ess_self_change_reauth_action_class($op),'action_hash'=>$hash);
}

function hrm_ess_self_change_reauth_revalidate_action($action)
{
    if(!is_array($action)||!isset($action['operation'],$action['request_id'],$action['intent_digest'],$action['action_hash']))return false;
    $fresh=hrm_ess_self_change_reauth_action_context((string)$action['operation'],(int)$action['request_id'],(string)$action['intent_digest']);
    return is_array($fresh)&&hash_equals((string)$fresh['action_hash'],(string)$action['action_hash'])?$fresh:false;
}

/** Local password reauthentication is limited to the current authenticated actor. */
function hrm_ess_self_change_reauth_local_password($password)
{
    if(hrm_ess_self_change_reauth_current_auth_method()!=='local_password'||!is_string($password)||$password===''||strlen($password)>512)return false;
    $actor=hrm_ess_self_change_actor_id(); if($actor<=0)return false;
    $q=db_query('SELECT id,password,inactive FROM '.TB_PREF.'users WHERE id='.$actor.' LIMIT 2','could not reauthenticate ESS self-change actor');
    if(!$q||db_num_rows($q)!==1)return false; $row=db_fetch_assoc($q);
    if(!$row||(int)$row['id']!==$actor||(int)$row['inactive']!==0||!is_string($row['password'])||$row['password']==='')return false;
    if(password_verify($password,(string)$row['password']))return true;
    if(preg_match('/^[a-f0-9]{32}$/Di',(string)$row['password'])!==1)return false;
    $legacy=md5($password); return function_exists('hash_equals')?hash_equals(strtolower((string)$row['password']),$legacy):strtolower((string)$row['password'])===$legacy;
}

/** Consume one action-bound level-two event inside the caller's already-open transaction. */
function hrm_ess_self_change_reauth_consume_federated($action,$now_epoch=null)
{
    global $transaction_level,$path_to_root;
    $now=$now_epoch===null?time():$now_epoch;
    if(!is_array($action)||!is_int($now)||$now<=0||empty($transaction_level)||hrm_ess_self_change_reauth_current_auth_method()!=='federated_oidc')return false;
    $fresh=hrm_ess_self_change_reauth_revalidate_action($action); if(!is_array($fresh))return false;
    if(!hrm_ess_self_change_reauth_dependencies())return false;
    if(!function_exists('append_domain_audit_event')){ $root=isset($path_to_root)?realpath($path_to_root):false; if($root===false)return false; require_once($root.'/includes/db/domain_audit_db.inc'); }
    $session_id=isset($_SESSION['security_session_registry_id'])?(int)$_SESSION['security_session_registry_id']:0; if($session_id<=0)return false;
    if(!db_query('SAVEPOINT hrm_ess_001_reauth','could not create HRM-ESS-001 reauthentication savepoint'))return false;
    $ok=false;$result=false;
    do{
        $row=federation_assurance_consumer_custody_for_update($session_id);
        if(!federation_assurance_consumer_current_custody_valid($row,$now,true))break;
        $q=db_query('SELECT assurance_event_id,session_id,assurance_level,assurance_method,action_class,action_hash,issued_at,expires_at,consumed_at FROM '.TB_PREF.'user_session_assurance_events WHERE session_id='.$session_id.' AND action_class='.db_escape($fresh['action_class']).' AND action_hash='.db_escape($fresh['action_hash']).' ORDER BY assurance_event_id DESC LIMIT 2 FOR UPDATE','could not lock HRM-ESS-001 assurance event');
        if(!$q)break; $event=db_fetch_assoc($q); if(!$event||db_fetch_assoc($q)!==false||(int)$event['session_id']!==$session_id||(int)$event['assurance_level']!==2||(string)$event['assurance_method']!=='oidc_acr_amr'||$event['consumed_at']!==null||!hash_equals((string)$event['action_hash'],(string)$fresh['action_hash']))break;
        $issued=strtotime((string)$event['issued_at'].' UTC');$expires=strtotime((string)$event['expires_at'].' UTC');$session_issued=strtotime((string)$row['assurance_issued_at'].' UTC');$session_expires=strtotime((string)$row['assurance_expires_at'].' UTC');
        if($issued===false||$expires===false||$session_issued===false||$session_expires===false||$issued!==$session_issued||$expires!==$session_expires||$now<$issued||$now>=$expires)break;
        $at=gmdate('Y-m-d H:i:s',$now);
        if(!db_query('UPDATE '.TB_PREF.'user_session_assurance_events SET consumed_at='.db_escape($at).' WHERE assurance_event_id='.(int)$event['assurance_event_id'].' AND consumed_at IS NULL AND expires_at>'.db_escape($at),'could not consume HRM-ESS-001 assurance event')||!function_exists('db_num_affected_rows')||(int)db_num_affected_rows()!==1)break;
        $restore=federation_assurance_consumer_restore_level_one_sql($row,$now); if($restore===false)break;
        if(!db_query('UPDATE '.TB_PREF.'user_sessions SET '.$restore.' WHERE id='.$session_id." AND revoked_at IS NULL AND assurance_level=2 AND assurance_method='oidc_acr_amr' AND assurance_issued_at=".db_escape((string)$row['assurance_issued_at']).' AND assurance_expires_at='.db_escape((string)$row['assurance_expires_at']),'could not reset HRM-ESS-001 federated session assurance')||(int)db_num_affected_rows()!==1)break;
        if(append_domain_audit_event(array('company_id'=>(int)$fresh['company'],'legal_entity_id'=>0,'entity_type'=>'security_session_assurance','entity_key'=>array('assurance_event_id'=>(int)$event['assurance_event_id'],'session_id'=>$session_id),'action'=>'security.session.assurance.consumed','reason_code'=>'hrm_ess_001_self_change_reauthentication','before'=>array('assurance_level'=>2),'after'=>array('assurance_level'=>1),'restricted_diff'=>array('operation'=>(string)$fresh['operation'],'request_id'=>(int)$fresh['request_id'],'action_class'=>(string)$fresh['action_class'],'action_hash'=>(string)$fresh['action_hash'],'values_logged'=>false,'single_use'=>true,'business_action_committed'=>false,'bank_tax_identifier_compensation_authority_added'=>false,'payroll_accounting_authority_changed'=>false)))===false)break;
        $ok=true;$result=array('assurance_event_id'=>(int)$event['assurance_event_id'],'session_id'=>$session_id,'action_class'=>$fresh['action_class'],'action_hash'=>$fresh['action_hash'],'consumed_at'=>$at);
    }while(false);
    if(!$ok){@db_query('ROLLBACK TO SAVEPOINT hrm_ess_001_reauth','could not roll back HRM-ESS-001 reauthentication savepoint');@db_query('RELEASE SAVEPOINT hrm_ess_001_reauth','could not release failed HRM-ESS-001 reauthentication savepoint');return false;}
    if(!db_query('RELEASE SAVEPOINT hrm_ess_001_reauth','could not release HRM-ESS-001 reauthentication savepoint'))return false;
    return $result;
}

function hrm_ess_self_change_reauth_session_context($session_id,$now)
{
    $session_id=(int)$session_id;if($session_id<=0||!is_int($now)||$now<=0||!defined('TB_PREF'))return false;
    $sql="SELECT s.id session_id,s.session_key_hash,s.user_id,s.role_id,s.auth_method,s.assurance_level,s.assurance_method,s.assurance_issued_at,s.assurance_expires_at,s.idle_expires_at,s.absolute_expires_at,s.revoked_at,s.user_agent_hash,b.verifier_decision_id,b.external_subject_link_id,d.external_subject_hash,d.verified_assurance_level decision_assurance_level,d.assurance_method decision_assurance_method,d.verified_at decision_verified_at,d.valid_until decision_valid_until,t.verifier_config_id,c.provider_id,c.audience_hash,c.trust_material_ref_hash,c.assurance_level_cap,c.config_status,p.issuer_hash,p.provider_status,l.user_id linked_user_id,l.link_status,l.deprovisioned_at,u.role_id current_role_id,u.inactive user_inactive,r.inactive role_inactive FROM ".TB_PREF."user_sessions s INNER JOIN ".TB_PREF."user_federation_session_bindings b ON b.session_id=s.id INNER JOIN ".TB_PREF."user_federation_verifier_decisions d ON d.verifier_decision_id=b.verifier_decision_id INNER JOIN ".TB_PREF."user_federation_verifier_transactions t ON t.verifier_transaction_id=d.verifier_transaction_id INNER JOIN ".TB_PREF."user_federation_provider_verifier_configs c ON c.verifier_config_id=t.verifier_config_id INNER JOIN ".TB_PREF."user_federation_providers p ON p.provider_id=c.provider_id INNER JOIN ".TB_PREF."user_federation_external_subject_links l ON l.external_subject_link_id=b.external_subject_link_id AND l.provider_id=c.provider_id AND l.external_subject_hash=d.external_subject_hash INNER JOIN ".TB_PREF."users u ON u.id=s.user_id AND u.id=l.user_id INNER JOIN ".TB_PREF."security_roles r ON r.id=u.role_id WHERE s.id=".$session_id." LIMIT 2";
    $q=db_query($sql,'could not read HRM-ESS-001 federated reauthentication session custody'); if(!$q)return false;$row=db_fetch_assoc($q);
    if(!$row||db_fetch_assoc($q)!==false||!federation_assurance_consumer_current_custody_valid($row,$now,false)||(int)$row['assurance_level']!==1||(string)$row['assurance_method']!=='oidc_rs256'||(int)$row['assurance_level_cap']<2)return false;
    return $row;
}

function hrm_ess_self_change_reauth_material_context($company,$session,$required_path)
{
    $company=(int)$company;if($company<0||!is_array($session)||!isset($session['trust_material_ref_hash'],$session['issuer_hash'],$session['audience_hash'])||!is_string($required_path)||strlen($required_path)<1||strlen($required_path)>1024||$required_path[0]!=='/'||strpos($required_path,'?')!==false||strpos($required_path,'#')!==false||preg_match('/[\x00-\x20\x7f]/',$required_path))return false;
    foreach(array('trust_material_ref_hash','issuer_hash','audience_hash') as $k)if(!federation_control_valid_sha256((string)$session[$k]))return false;
    $bundle=federation_control_load_bundle($company,(string)$session['trust_material_ref_hash'],'oidc','oidc_code_pkce_v1'); if(!is_array($bundle)||!isset($bundle['materials'])||!is_array($bundle['materials']))return false;$m=$bundle['materials'];
    if(!isset($m['oidc_client_config'],$m['oidc_assurance_policy']))return false;$client=federation_oidc_client_config($m['oidc_client_config']);$policy=federation_oidc_assurance_policy($m['oidc_assurance_policy']);
    if($client===false||$policy===false||!federation_control_hash_equals(hash('sha256',$client['issuer']),(string)$session['issuer_hash'])||!federation_control_hash_equals(hash('sha256',$client['client_id']),(string)$session['audience_hash']))return false;
    $matches=array();foreach($client['redirect_uris'] as $uri){$p=@parse_url($uri);$path=is_array($p)&&isset($p['path'])?(string)$p['path']:'/';if($path===$required_path&&!isset($p['query'])&&!isset($p['fragment']))$matches[]=$uri;} if(count($matches)!==1)return false;
    return array('redirect_uri'=>$matches[0],'client'=>$client,'policy'=>$policy,'materials'=>$m);
}

function hrm_ess_self_change_reauth_authorization_url($url,$policy)
{
    if(!is_string($url)||!is_array($policy)||!isset($policy['required_acr'],$policy['max_auth_age_seconds']))return false;$p=@parse_url($url);
    if(!is_array($p)||!isset($p['scheme'],$p['host'],$p['query'])||strtolower((string)$p['scheme'])!=='https'||isset($p['fragment']))return false;$q=array();parse_str((string)$p['query'],$q);foreach(array('prompt','max_age','acr_values') as $k)if(array_key_exists($k,$q))return false;
    $q['prompt']='login';$q['max_age']=(string)(int)$policy['max_auth_age_seconds'];$q['acr_values']=(string)$policy['required_acr'];$authority=$p['host'].(isset($p['port'])?':'.(int)$p['port']:'');$path=isset($p['path'])?$p['path']:'';return $p['scheme'].'://'.$authority.$path.'?'.http_build_query($q,'','&',PHP_QUERY_RFC3986);
}

function hrm_ess_self_change_reauth_issue_csrf($action,$now_epoch=null)
{
    $now=$now_epoch===null?time():$now_epoch;if(!is_array($action)||!is_int($now)||$now<=0)return false;$fresh=hrm_ess_self_change_reauth_revalidate_action($action);if(!is_array($fresh))return false;
    try{$b=random_bytes(32);}catch(Exception $e){return false;} $token=rtrim(strtr(base64_encode($b),'+/','-_'),'=');if(preg_match('/^[A-Za-z0-9_-]{43}$/D',$token)!==1)return false;
    $_SESSION['hrm_ess_self_change_reauth_csrf']=array('token_hash'=>hash('sha256',$token),'action'=>$fresh,'session_id'=>isset($_SESSION['security_session_registry_id'])?(int)$_SESSION['security_session_registry_id']:0,'expires_at_epoch'=>$now+HRM_ESS_SELF_CHANGE_REAUTH_CSRF_TTL);return $token;
}

function hrm_ess_self_change_reauth_consume_csrf($action,$token,$now)
{
    $row=isset($_SESSION['hrm_ess_self_change_reauth_csrf'])?$_SESSION['hrm_ess_self_change_reauth_csrf']:false;unset($_SESSION['hrm_ess_self_change_reauth_csrf']);
    if(!is_array($row)||!is_array($action)||!is_string($token)||preg_match('/^[A-Za-z0-9_-]{43}$/D',$token)!==1||!is_int($now)||$now<=0||!isset($row['action']['action_hash']))return false;
    return (int)$row['session_id']===(isset($_SESSION['security_session_registry_id'])?(int)$_SESSION['security_session_registry_id']:0)&&(int)$row['expires_at_epoch']>$now&&hash_equals((string)$row['token_hash'],hash('sha256',$token))&&hash_equals((string)$row['action']['action_hash'],(string)$action['action_hash']);
}

function hrm_ess_self_change_reauth_prune_pending($now)
{
    if(!isset($_SESSION['hrm_ess_self_change_reauth_pending'])||!is_array($_SESSION['hrm_ess_self_change_reauth_pending']))$_SESSION['hrm_ess_self_change_reauth_pending']=array();
    foreach($_SESSION['hrm_ess_self_change_reauth_pending'] as $h=>$row)if(!is_string($h)||preg_match('/^[a-f0-9]{64}$/D',$h)!==1||!is_array($row)||!isset($row['expires_at_epoch'])||(int)$row['expires_at_epoch']<=(int)$now)unset($_SESSION['hrm_ess_self_change_reauth_pending'][$h]);
}

function hrm_ess_self_change_reauth_bind_pending($state,$session,$action,$tx,$redirect,$now)
{
    if(!is_string($state)||preg_match('/^[A-Za-z0-9_-]{43}$/D',$state)!==1||!is_array($session)||!is_array($action)||!is_array($tx)||!isset($tx['verifier_transaction_id'],$tx['expires_at_epoch'])||!is_string($redirect))return false;
    hrm_ess_self_change_reauth_prune_pending($now);$pending=&$_SESSION['hrm_ess_self_change_reauth_pending'];foreach($pending as $r)if(is_array($r)&&(int)$r['session_id']===(int)$session['session_id']&&(string)$r['action_hash']===(string)$action['action_hash'])return false;if(count($pending)>=HRM_ESS_SELF_CHANGE_REAUTH_MAX_PENDING)return false;
    $pending[hash('sha256',$state)]=array('company'=>(int)$action['company'],'session_id'=>(int)$session['session_id'],'verifier_config_id'=>(int)$session['verifier_config_id'],'verifier_transaction_id'=>(int)$tx['verifier_transaction_id'],'operation'=>(string)$action['operation'],'request_id'=>(int)$action['request_id'],'intent_digest'=>(string)$action['intent_digest'],'action_class'=>(string)$action['action_class'],'action_hash'=>(string)$action['action_hash'],'redirect_uri_hash'=>hash('sha256',$redirect),'expires_at_epoch'=>(int)$tx['expires_at_epoch']);return true;
}

function hrm_ess_self_change_reauth_pending($company,$state,$now)
{
    if(!is_string($state)||preg_match('/^[A-Za-z0-9_-]{43}$/D',$state)!==1)return false;hrm_ess_self_change_reauth_prune_pending($now);$h=hash('sha256',$state);if(!isset($_SESSION['hrm_ess_self_change_reauth_pending'][$h])||!is_array($_SESSION['hrm_ess_self_change_reauth_pending'][$h]))return false;$r=$_SESSION['hrm_ess_self_change_reauth_pending'][$h];
    if((int)$r['company']!==(int)$company||(int)$r['session_id']!==(isset($_SESSION['security_session_registry_id'])?(int)$_SESSION['security_session_registry_id']:0))return false;return $r;
}
function hrm_ess_self_change_reauth_forget_pending($state){if(is_string($state)){ $h=hash('sha256',$state);unset($_SESSION['hrm_ess_self_change_reauth_pending'][$h]);}}
function hrm_ess_self_change_reauth_nonce($jwt,$expected){if(!is_string($expected)||preg_match('/^[a-f0-9]{64}$/D',$expected)!==1)return false;$p=federation_oidc_token_verifier_parse($jwt);if(!is_array($p)||!isset($p['claims']['nonce'])||!is_string($p['claims']['nonce'])||preg_match('/^[A-Za-z0-9_-]{32,128}$/D',$p['claims']['nonce'])!==1)return false;return federation_control_hash_equals($expected,hash('sha256',$p['claims']['nonce']))?$p['claims']['nonce']:false;}

function hrm_ess_self_change_reauth_result($status,$reason,$extra=array())
{
    if(!in_array($status,array('authorization_required','assured','retry_later','denied'),true)||!is_string($reason)||!is_array($extra))return false;$r=array('status'=>$status,'reason'=>$reason,'retry_safe'=>$status==='retry_later');
    foreach(array('authorization_url','expires_at_epoch','retry_after_epoch','assurance_event_id','session_id','operation','request_id','intent_digest','action_class','action_hash','verifier_transaction_id') as $k)if(array_key_exists($k,$extra))$r[$k]=$extra[$k];return $r;
}

function hrm_ess_self_change_reauth_start_oidc($action,$csrf_token,$required_callback_path,$ttl_seconds=HRM_ESS_SELF_CHANGE_REAUTH_TRANSACTION_TTL,$now_epoch=null)
{
    global $path_to_root;$now=$now_epoch===null?time():$now_epoch;$ttl_seconds=(int)$ttl_seconds;
    if(!is_array($action)||!is_int($now)||$now<=0||hrm_ess_self_change_reauth_current_auth_method()!=='federated_oidc')return hrm_ess_self_change_reauth_result('denied','federated_session_required');
    if(!hrm_ess_self_change_reauth_dependencies())return hrm_ess_self_change_reauth_result('retry_later','runtime_dependency_unavailable');$fresh=hrm_ess_self_change_reauth_revalidate_action($action);if(!is_array($fresh))return hrm_ess_self_change_reauth_result('denied','action_stale');
    if(!hrm_ess_self_change_reauth_consume_csrf($fresh,$csrf_token,$now))return hrm_ess_self_change_reauth_result('denied','csrf_rejected');$root=isset($path_to_root)?realpath($path_to_root):false;$dep=$root===false?false:federation_verifier_dependency_configured_status($root);if(!is_array($dep)||empty($dep['available']))return hrm_ess_self_change_reauth_result('retry_later','verifier_dependency_unavailable');
    $sid=isset($_SESSION['security_session_registry_id'])?(int)$_SESSION['security_session_registry_id']:0;$session=hrm_ess_self_change_reauth_session_context($sid,$now);if(!is_array($session))return hrm_ess_self_change_reauth_result('denied','federated_session_ineligible');$material=hrm_ess_self_change_reauth_material_context((int)$fresh['company'],$session,$required_callback_path);if(!is_array($material))return hrm_ess_self_change_reauth_result('retry_later','step_up_material_unavailable');
    $tx=federation_oidc_issue_transaction((int)$fresh['company'],(int)$session['verifier_config_id'],(string)$material['redirect_uri'],$ttl_seconds);if(!is_array($tx)||!isset($tx['state'],$tx['authorization_url'],$tx['expires_at_epoch'],$tx['verifier_transaction_id']))return hrm_ess_self_change_reauth_result('retry_later','transaction_issue_failed');$url=hrm_ess_self_change_reauth_authorization_url((string)$tx['authorization_url'],$material['policy']);if($url===false||!hrm_ess_self_change_reauth_bind_pending((string)$tx['state'],$session,$fresh,$tx,(string)$material['redirect_uri'],$now))return hrm_ess_self_change_reauth_result('denied','browser_correlation_failed');
    return hrm_ess_self_change_reauth_result('authorization_required','reauthentication_required',array('authorization_url'=>$url,'expires_at_epoch'=>(int)$tx['expires_at_epoch'],'verifier_transaction_id'=>(int)$tx['verifier_transaction_id'],'operation'=>$fresh['operation'],'request_id'=>$fresh['request_id'],'intent_digest'=>$fresh['intent_digest'],'action_class'=>$fresh['action_class'],'action_hash'=>$fresh['action_hash']));
}

function hrm_ess_self_change_reauth_complete_oidc($company,$state,$authorization_code,$required_callback_path,$lease_seconds=HRM_ESS_SELF_CHANGE_REAUTH_DEFAULT_LEASE,$now_epoch=null,$transport=null,$signature_verifier=null)
{
    global $path_to_root;$company=(int)$company;$now=$now_epoch===null?time():$now_epoch;$lease_seconds=(int)$lease_seconds;
    if($company<0||!is_int($now)||$now<=0||$lease_seconds<15||$lease_seconds>300||!is_string($state)||preg_match('/^[A-Za-z0-9_-]{43}$/D',$state)!==1||!is_string($authorization_code)||strlen($authorization_code)<1||strlen($authorization_code)>4096||hrm_ess_self_change_reauth_current_auth_method()!=='federated_oidc')return hrm_ess_self_change_reauth_result('denied','callback_invalid');
    if(!hrm_ess_self_change_reauth_dependencies())return hrm_ess_self_change_reauth_result('retry_later','runtime_dependency_unavailable');$pending=hrm_ess_self_change_reauth_pending($company,$state,$now);if(!is_array($pending))return hrm_ess_self_change_reauth_result('denied','browser_correlation_missing');
    $root=isset($path_to_root)?realpath($path_to_root):false;$dep=$root===false?false:federation_verifier_dependency_configured_status($root);if(!is_array($dep)||empty($dep['available']))return hrm_ess_self_change_reauth_result('retry_later','verifier_dependency_unavailable');$session=hrm_ess_self_change_reauth_session_context((int)$pending['session_id'],$now);if(!is_array($session)||(int)$session['verifier_config_id']!==(int)$pending['verifier_config_id']){hrm_ess_self_change_reauth_forget_pending($state);return hrm_ess_self_change_reauth_result('denied','federated_session_changed');}$material=hrm_ess_self_change_reauth_material_context($company,$session,$required_callback_path);if(!is_array($material)||!hash_equals((string)$pending['redirect_uri_hash'],hash('sha256',(string)$material['redirect_uri'])))return hrm_ess_self_change_reauth_result('retry_later','step_up_material_unavailable');
    $claim=federation_oidc_acquire_callback_claim($company,$state,$lease_seconds);if(!is_array($claim)){hrm_ess_self_change_reauth_forget_pending($state);return hrm_ess_self_change_reauth_result('denied','callback_claim_unavailable');}$claim_token=(string)$claim['claim_token'];$retry_after=(int)$claim['claim_expires_at_epoch'];if((int)$claim['verifier_transaction_id']!==(int)$pending['verifier_transaction_id']||(int)$claim['verifier_config_id']!==(int)$pending['verifier_config_id']){federation_oidc_terminalize_callback_claim($company,$claim_token,'callback_invalid');hrm_ess_self_change_reauth_forget_pending($state);return hrm_ess_self_change_reauth_result('denied','callback_context_invalid');}
    $proof=federation_oidc_load_proof($company,(string)$claim['proof_ref_hash'],$now);if(!is_array($proof)||(int)$proof['verifier_config_id']!==(int)$claim['verifier_config_id']||(string)$proof['transaction_key_hash']!==hash('sha256',$state)){federation_oidc_terminalize_callback_claim($company,$claim_token,'proof_unavailable');hrm_ess_self_change_reauth_forget_pending($state);return hrm_ess_self_change_reauth_result('denied','proof_unavailable');}
    $materials=$material['materials'];$contract=federation_oidc_token_verifier_material_contract($materials);if($contract===false)return hrm_ess_self_change_reauth_result('retry_later','verifier_material_unavailable',array('retry_after_epoch'=>$retry_after));$exchange=federation_oidc_exchange_authorization_code($authorization_code,(string)$material['redirect_uri'],(string)$proof['code_verifier'],$materials,$transport);if(!is_array($exchange)||!isset($exchange['classification'])){federation_oidc_terminalize_callback_claim($company,$claim_token,'exchange_uncertain');hrm_ess_self_change_reauth_forget_pending($state);return hrm_ess_self_change_reauth_result('denied','exchange_uncertain');}if((string)$exchange['classification']==='pre_send_failure')return hrm_ess_self_change_reauth_result('retry_later','exchange_pre_send_failure',array('retry_after_epoch'=>$retry_after));if((string)$exchange['classification']!=='exchange_succeeded'||!isset($exchange['id_token'])){$reason=(string)$exchange['classification']==='exchange_rejected'?'exchange_rejected':'exchange_uncertain';federation_oidc_terminalize_callback_claim($company,$claim_token,$reason);hrm_ess_self_change_reauth_forget_pending($state);return hrm_ess_self_change_reauth_result('denied',$reason);}
    $nonce=hrm_ess_self_change_reauth_nonce((string)$exchange['id_token'],(string)$claim['nonce_hash']);if($nonce===false){federation_oidc_terminalize_callback_claim($company,$claim_token,'verifier_rejected');hrm_ess_self_change_reauth_forget_pending($state);return hrm_ess_self_change_reauth_result('denied','nonce_rejected');}$expect=array('company_id'=>$company,'provider_id'=>(int)$session['provider_id'],'issuer'=>(string)$contract['issuer'],'client_id'=>(string)$contract['client_id'],'nonce'=>$nonce);$base=federation_oidc_token_verifier_verify((string)$exchange['id_token'],$expect,$materials,$now,$signature_verifier);$candidate=federation_oidc_assurance_candidate_verify((string)$exchange['id_token'],$expect,$materials,$now,$signature_verifier);if(!is_array($base)||empty($base['verified'])||!is_array($candidate)||empty($candidate['verified'])){federation_oidc_terminalize_callback_claim($company,$claim_token,'verifier_rejected');hrm_ess_self_change_reauth_forget_pending($state);return hrm_ess_self_change_reauth_result('denied','assurance_candidate_rejected');}$decision=federation_oidc_commit_verified_decision($company,$claim_token,$authorization_code,$base);if(!is_array($decision))$decision=federation_oidc_commit_verified_decision($company,$claim_token,$authorization_code,$base);if(!is_array($decision)){hrm_ess_self_change_reauth_forget_pending($state);return hrm_ess_self_change_reauth_result('denied','decision_commit_failed');}
    $action=hrm_ess_self_change_reauth_action_context((string)$pending['operation'],(int)$pending['request_id'],(string)$pending['intent_digest']);if(!is_array($action)||!hash_equals((string)$action['action_hash'],(string)$pending['action_hash'])){hrm_ess_self_change_reauth_forget_pending($state);return hrm_ess_self_change_reauth_result('denied','action_stale');}$issued=federation_issue_privileged_assurance($company,(int)$pending['session_id'],$candidate,(string)$pending['action_class'],(string)$pending['action_hash'],$now);if(!is_array($issued))$issued=federation_issue_privileged_assurance($company,(int)$pending['session_id'],$candidate,(string)$pending['action_class'],(string)$pending['action_hash'],$now);hrm_ess_self_change_reauth_forget_pending($state);if(!is_array($issued)||!isset($issued['assurance_event_id']))return hrm_ess_self_change_reauth_result('denied','assurance_issue_failed');
    return hrm_ess_self_change_reauth_result('assured','ok',array('assurance_event_id'=>(int)$issued['assurance_event_id'],'session_id'=>(int)$pending['session_id'],'operation'=>$pending['operation'],'request_id'=>(int)$pending['request_id'],'intent_digest'=>$pending['intent_digest'],'action_class'=>$pending['action_class'],'action_hash'=>$pending['action_hash']));
}
