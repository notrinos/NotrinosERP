<?php
/**
 * PAY-SEC-004 existing-link federated durable-session issuance command.
 *
 * Internal command only. It accepts an already committed verifier decision and
 * an existing immutable subject link, creates exactly one durable session
 * family plus provenance and PAY-AUD evidence in one transaction, and never
 * mutates the browser/PHP session or exposes a public authentication route.
 */
require_once(dirname(__FILE__).DIRECTORY_SEPARATOR.'session_registry.inc');

/** Validate and normalize non-database issuance inputs. */
function federation_session_issue_request($company,$decision_id,$session_secret,$user_agent_hash,$now_epoch=null)
{
    $company=(int)$company;$decision_id=(int)$decision_id;
    $now=$now_epoch===null?time():$now_epoch;
    if($company<0||$decision_id<=0||!is_string($session_secret)||strlen($session_secret)<32
        ||!is_string($user_agent_hash)||preg_match('/^[a-f0-9]{64}$/D',$user_agent_hash)!==1
        ||!is_int($now)||$now<=0)return false;
    if(function_exists('user_company')&&(int)user_company()!==$company)return false;
    return array('company'=>$company,'verifier_decision_id'=>$decision_id,
        'session_key_hash'=>session_registry_hash($session_secret),
        'user_agent_hash'=>$user_agent_hash,'now_epoch'=>$now);
}

/** Lock the exact decision/provider/configuration/link/local-user/role custody. */
function federation_session_issue_custody_for_update($decision_id)
{
    if(!defined('TB_PREF'))return false;
    $decision_id=(int)$decision_id;if($decision_id<=0)return false;
    $sql="SELECT d.verifier_decision_id,d.verifier_transaction_id,d.external_subject_hash,d.verified_assurance_level,d.assurance_method,d.verified_at,d.valid_until,"
        ."t.verifier_config_id,c.provider_id,c.max_clock_skew_seconds,c.assurance_level_cap,c.config_status,p.provider_status,"
        ."l.external_subject_link_id,l.user_id,l.link_status,l.deprovisioned_at,u.role_id,u.inactive user_inactive,r.inactive role_inactive "
        ."FROM ".TB_PREF."user_federation_verifier_decisions d "
        ."INNER JOIN ".TB_PREF."user_federation_verifier_transactions t ON t.verifier_transaction_id=d.verifier_transaction_id "
        ."INNER JOIN ".TB_PREF."user_federation_provider_verifier_configs c ON c.verifier_config_id=t.verifier_config_id "
        ."INNER JOIN ".TB_PREF."user_federation_providers p ON p.provider_id=c.provider_id "
        ."INNER JOIN ".TB_PREF."user_federation_external_subject_links l ON l.provider_id=c.provider_id AND l.external_subject_hash=d.external_subject_hash "
        ."INNER JOIN ".TB_PREF."users u ON u.id=l.user_id "
        ."INNER JOIN ".TB_PREF."security_roles r ON r.id=u.role_id "
        ."WHERE d.verifier_decision_id=$decision_id LIMIT 2 FOR UPDATE";
    $result=db_query($sql,'could not lock PAY-SEC-004 federated session issuance custody');
    if(!$result)return false;$row=db_fetch_assoc($result);
    if(!$row||db_fetch_assoc($result)!==false)return false;
    return $row;
}

/** Load the one previously issued session family for idempotent retry. */
function federation_session_existing_binding_for_update($decision_id)
{
    $decision_id=(int)$decision_id;if($decision_id<=0||!defined('TB_PREF'))return false;
    $sql="SELECT b.federation_session_binding_id,b.session_id,b.verifier_decision_id,b.external_subject_link_id,b.bound_at,"
        ."s.session_key_hash,s.user_id,s.role_id,s.auth_method,s.assurance_level,s.assurance_method,s.assurance_issued_at,s.assurance_expires_at,"
        ."s.created_at,s.last_seen_at,s.idle_expires_at,s.absolute_expires_at,s.revoked_at,s.user_agent_hash "
        ."FROM ".TB_PREF."user_federation_session_bindings b INNER JOIN ".TB_PREF."user_sessions s ON s.id=b.session_id "
        ."WHERE b.verifier_decision_id=$decision_id LIMIT 2 FOR UPDATE";
    $result=db_query($sql,'could not verify existing PAY-SEC-004 federated session binding');
    if(!$result)return false;$row=db_fetch_assoc($result);
    if(!$row)return null;if(db_fetch_assoc($result)!==false)return false;
    return $row;
}

/** Append restricted-value-free session issuance evidence inside the transaction. */
function federation_session_issue_audit($company,$row,$session_id,$absolute_expires_at)
{
    return append_domain_audit_event(array('company_id'=>(int)$company,'legal_entity_id'=>0,
        'entity_type'=>'security_federation_session','entity_key'=>array('session_id'=>(int)$session_id,
            'verifier_decision_id'=>(int)$row['verifier_decision_id'],'external_subject_link_id'=>(int)$row['external_subject_link_id']),
        'action'=>'security.federation.session.issued','reason_code'=>'pay_sec_004_existing_link_session_issuance',
        'before'=>null,'after'=>array('durable_federated_session_issued'=>true),
        'restricted_diff'=>array('provider_id'=>(int)$row['provider_id'],'verifier_config_id'=>(int)$row['verifier_config_id'],
            'user_id'=>(int)$row['user_id'],'role_id'=>(int)$row['role_id'],'auth_method'=>'federated_oidc',
            'assurance_level'=>1,'assurance_method'=>'oidc_rs256','absolute_expires_at'=>(string)$absolute_expires_at,
            'provenance_binding_written'=>true,'raw_external_subject_stored'=>false,'raw_token_stored'=>false,
            'raw_session_secret_stored'=>false,'user_agent_stored'=>false,'public_callback_route_added'=>false,
            'browser_session_activated'=>false,'first_link_written'=>false,'provider_or_config_mutated'=>false,
            'external_link_mutated'=>false,'targeted_revocation_written'=>false,'assurance_elevated'=>false,
            'role_granted'=>false,'payment_authority_changed'=>false,'payroll_authority_changed'=>false,
            'accounting_authority_changed'=>false)) )!==false;
}

/**
 * Issue exactly one durable level-one federated session for an existing link.
 *
 * The caller owns the high-entropy session secret and may retry with the exact
 * same secret after an uncertain response. A different secret for an already
 * bound decision fails closed because the stored digest cannot be recovered.
 */
function federation_issue_existing_link_session($company,$decision_id,$session_secret,$user_agent_hash,$now_epoch=null)
{
    global $path_to_root;
    $request=federation_session_issue_request($company,$decision_id,$session_secret,$user_agent_hash,$now_epoch);
    if($request===false)return false;
    if(!function_exists('append_domain_audit_event')){
        $root=isset($path_to_root)?realpath($path_to_root):false;
        if($root===false)return false;$audit=$root.DIRECTORY_SEPARATOR.'includes'.DIRECTORY_SEPARATOR.'db'.DIRECTORY_SEPARATOR.'domain_audit_db.inc';
        if(!is_file($audit))return false;require_once($audit);
    }
    if(!function_exists('append_domain_audit_event')||!db_query('BEGIN','could not start PAY-SEC-004 federated session issuance transaction'))return false;
    $committed=false;$result_value=false;
    do {
        $row=federation_session_issue_custody_for_update($request['verifier_decision_id']);if(!is_array($row))break;
        $valid_until_epoch=strtotime((string)$row['valid_until'].' UTC');$verified_at_epoch=strtotime((string)$row['verified_at'].' UTC');
        $now=(int)$request['now_epoch'];$skew=(int)$row['max_clock_skew_seconds'];
        if((string)$row['provider_status']!=='enabled'||(string)$row['config_status']!=='enabled'
            ||(string)$row['link_status']!=='active'||$row['deprovisioned_at']!==null
            ||(int)$row['user_inactive']!==0||(int)$row['role_inactive']!==0
            ||(int)$row['verified_assurance_level']!==1||(string)$row['assurance_method']!=='oidc_rs256'
            ||(int)$row['assurance_level_cap']<1||$valid_until_epoch===false||$verified_at_epoch===false
            ||$now>=$valid_until_epoch||$verified_at_epoch>$now+$skew)break;
        $absolute_epoch=min($valid_until_epoch,$now+SESSION_REGISTRY_ABSOLUTE_LIFETIME);
        $idle_epoch=min($absolute_epoch,$now+session_registry_idle_lifetime(SESSION_REGISTRY_IDLE_LIFETIME));
        if($absolute_epoch<=$now||$idle_epoch<=$now||$verified_at_epoch>$absolute_epoch)break;
        $absolute=gmdate('Y-m-d H:i:s',$absolute_epoch);$idle=gmdate('Y-m-d H:i:s',$idle_epoch);$observed=gmdate('Y-m-d H:i:s',$now);
        $existing=federation_session_existing_binding_for_update($request['verifier_decision_id']);
        if($existing===false)break;
        if(is_array($existing)){
            $existing_absolute=strtotime((string)$existing['absolute_expires_at'].' UTC');
            if((int)$existing['external_subject_link_id']!==(int)$row['external_subject_link_id']
                ||(int)$existing['user_id']!==(int)$row['user_id']||(int)$existing['role_id']!==(int)$row['role_id']
                ||(string)$existing['auth_method']!=='federated_oidc'||(int)$existing['assurance_level']!==1
                ||(string)$existing['assurance_method']!=='oidc_rs256'||$existing['revoked_at']!==null
                ||$existing_absolute===false||$now>=$existing_absolute
                ||!session_registry_hash_equals((string)$existing['session_key_hash'],$request['session_key_hash'])
                ||!session_registry_hash_equals((string)$existing['user_agent_hash'],$request['user_agent_hash']))break;
            if(!db_query('COMMIT','could not commit PAY-SEC-004 idempotent federated session verification'))break;
            $committed=true;$result_value=array('session_id'=>(int)$existing['session_id'],'user_id'=>(int)$row['user_id'],
                'role_id'=>(int)$row['role_id'],'verifier_decision_id'=>(int)$row['verifier_decision_id'],
                'external_subject_link_id'=>(int)$row['external_subject_link_id'],'absolute_expires_at'=>(string)$existing['absolute_expires_at'],
                'idempotent_retry'=>true,'browser_session_activated'=>false,'assurance_elevated'=>false);break;
        }
        $sql="INSERT INTO ".TB_PREF."user_sessions(session_key_hash,user_id,role_id,auth_method,assurance_level,assurance_method,assurance_issued_at,assurance_expires_at,created_at,last_seen_at,idle_expires_at,absolute_expires_at,user_agent_hash) VALUES ("
            .db_escape($request['session_key_hash']).",".(int)$row['user_id'].",".(int)$row['role_id'].",'federated_oidc',1,'oidc_rs256',"
            .db_escape((string)$row['verified_at']).",".db_escape($absolute).",".db_escape($observed).",".db_escape($observed).",".db_escape($idle).",".db_escape($absolute).",".db_escape($request['user_agent_hash']).")";
        if(!db_query($sql,'could not create PAY-SEC-004 federated durable session'))break;
        $session_id=(int)db_insert_id();if($session_id<=0)break;
        $binding="INSERT INTO ".TB_PREF."user_federation_session_bindings(session_id,verifier_decision_id,external_subject_link_id,bound_at) VALUES ("
            .$session_id.",".(int)$row['verifier_decision_id'].",".(int)$row['external_subject_link_id'].",".db_escape($observed).")";
        if(!db_query($binding,'could not bind PAY-SEC-004 federated session provenance'))break;
        if(federation_session_issue_audit($request['company'],$row,$session_id,$absolute)===false)break;
        if(!db_query('COMMIT','could not commit PAY-SEC-004 federated session issuance transaction'))break;
        $committed=true;$result_value=array('session_id'=>$session_id,'user_id'=>(int)$row['user_id'],'role_id'=>(int)$row['role_id'],
            'verifier_decision_id'=>(int)$row['verifier_decision_id'],'external_subject_link_id'=>(int)$row['external_subject_link_id'],
            'absolute_expires_at'=>$absolute,'idempotent_retry'=>false,'browser_session_activated'=>false,'assurance_elevated'=>false);
    }while(false);
    if(!$committed){@db_query('ROLLBACK','could not roll back PAY-SEC-004 federated session issuance transaction');return false;}
    return $result_value;
}
