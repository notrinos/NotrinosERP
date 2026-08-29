<?php
/**********************************************************************
    PAY-SEC-004 explicit first external-subject link creation command.

    Internal administrator command only. It consumes only an already accepted
    verifier decision and an explicit local user id. It never auto-links from
    email, login id, display name or any other profile claim, and it never
    activates a browser/PHP session.
***********************************************************************/

/** Resolve the authenticated administrator without trusting caller identity. */
function federation_first_link_actor_id()
{
    if (!isset($_SESSION['wa_current_user']) || !isset($_SESSION['wa_current_user']->user))
        return 0;
    return (int)$_SESSION['wa_current_user']->user;
}

/** Reuse the existing user-administration authority; no federation grant is added. */
function federation_first_link_authorized($company)
{
    if (!function_exists('user_check_access') || !user_check_access('SA_USERS'))
        return false;
    if (function_exists('user_company') && (int)user_company() !== (int)$company)
        return false;
    return federation_first_link_actor_id() > 0;
}

/** Normalize only explicit non-claim identifiers and an optional deterministic clock. */
function federation_first_link_request($company, $verifier_decision_id, $user_id, $now_epoch=null)
{
    $company=(int)$company;$verifier_decision_id=(int)$verifier_decision_id;$user_id=(int)$user_id;
    $now=$now_epoch===null?time():$now_epoch;
    if ($company<0 || $verifier_decision_id<=0 || $user_id<=0 || !is_int($now) || $now<=0)
        return false;
    if (!federation_first_link_authorized($company))
        return false;
    return array('company'=>$company,'verifier_decision_id'=>$verifier_decision_id,'user_id'=>$user_id,'now_epoch'=>$now);
}

/** Lock the accepted decision, transaction, active provider/configuration and explicit local user/role. */
function federation_first_link_custody_for_update($verifier_decision_id, $user_id)
{
    if (!defined('TB_PREF'))
        return false;
    $verifier_decision_id=(int)$verifier_decision_id;$user_id=(int)$user_id;
    if ($verifier_decision_id<=0 || $user_id<=0)
        return false;
    $sql="SELECT d.verifier_decision_id,d.verifier_transaction_id,d.external_subject_hash,d.verified_assurance_level,d.assurance_method,d.verified_at,d.valid_until,"
        ."t.verifier_config_id,t.consumed_at,c.provider_id,c.max_clock_skew_seconds,c.assurance_level_cap,c.config_status,p.provider_status,"
        ."u.id user_id,u.role_id,u.inactive user_inactive,r.inactive role_inactive "
        ."FROM ".TB_PREF."user_federation_verifier_decisions d "
        ."INNER JOIN ".TB_PREF."user_federation_verifier_transactions t ON t.verifier_transaction_id=d.verifier_transaction_id "
        ."INNER JOIN ".TB_PREF."user_federation_provider_verifier_configs c ON c.verifier_config_id=t.verifier_config_id "
        ."INNER JOIN ".TB_PREF."user_federation_providers p ON p.provider_id=c.provider_id "
        ."INNER JOIN ".TB_PREF."users u ON u.id=$user_id "
        ."INNER JOIN ".TB_PREF."security_roles r ON r.id=u.role_id "
        ."WHERE d.verifier_decision_id=$verifier_decision_id LIMIT 2 FOR UPDATE";
    $result=db_query($sql,'could not lock PAY-SEC-004 first-link custody');
    if (!$result)
        return false;
    $row=db_fetch_assoc($result);
    if (!$row || db_fetch_assoc($result)!==false)
        return false;
    return $row;
}

/** Lock every unique-key collision candidate before deciding create/idempotent/fail. */
function federation_first_link_collisions_for_update($row)
{
    $provider_id=(int)$row['provider_id'];$user_id=(int)$row['user_id'];$transaction_id=(int)$row['verifier_transaction_id'];
    $subject=(string)$row['external_subject_hash'];
    if ($provider_id<=0 || $user_id<=0 || $transaction_id<=0 || preg_match('/^[a-f0-9]{64}$/D',$subject)!==1)
        return false;
    $sql="SELECT external_subject_link_id,provider_id,user_id,external_subject_hash,verifier_transaction_id,link_status,deprovisioned_at "
        ."FROM ".TB_PREF."user_federation_external_subject_links WHERE "
        ."(provider_id=$provider_id AND (external_subject_hash=".db_escape($subject)." OR user_id=$user_id)) OR verifier_transaction_id=$transaction_id "
        ."ORDER BY external_subject_link_id FOR UPDATE";
    $result=db_query($sql,'could not lock PAY-SEC-004 first-link collision custody');
    if (!$result)
        return false;
    $rows=array();
    while ($candidate=db_fetch_assoc($result))
        $rows[]=$candidate;
    return $rows;
}

/** Accept only the exact still-active row as an idempotent retry; every other collision fails closed. */
function federation_first_link_existing_exact($collisions, $row)
{
    if (!is_array($collisions))
        return false;
    if (count($collisions)===0)
        return null;
    if (count($collisions)!==1)
        return false;
    $existing=$collisions[0];
    if ((int)$existing['provider_id']!==(int)$row['provider_id']
        || (int)$existing['user_id']!==(int)$row['user_id']
        || (string)$existing['external_subject_hash']!==(string)$row['external_subject_hash']
        || (int)$existing['verifier_transaction_id']!==(int)$row['verifier_transaction_id']
        || (string)$existing['link_status']!=='active' || $existing['deprovisioned_at']!==null)
        return false;
    return $existing;
}

/** Append restricted-value-free evidence before commit. */
function federation_first_link_audit($company, $row, $external_subject_link_id, $actor_id)
{
    return append_domain_audit_event(array('company_id'=>(int)$company,'legal_entity_id'=>0,
        'entity_type'=>'security_federation_subject_link','entity_key'=>array('external_subject_link_id'=>(int)$external_subject_link_id),
        'action'=>'security.federation.subject_link.created','reason_code'=>'pay_sec_004_explicit_first_link_creation',
        'before'=>null,'after'=>array('explicit_external_subject_link_created'=>true),
        'restricted_diff'=>array('provider_id'=>(int)$row['provider_id'],'verifier_config_id'=>(int)$row['verifier_config_id'],
            'verifier_decision_id'=>(int)$row['verifier_decision_id'],'verifier_transaction_id'=>(int)$row['verifier_transaction_id'],
            'user_id'=>(int)$row['user_id'],'role_id'=>(int)$row['role_id'],'actor_id'=>(int)$actor_id,
            'email_claim_used'=>false,'display_name_claim_used'=>false,'login_id_claim_used'=>false,'raw_external_subject_stored'=>false,
            'raw_token_or_assertion_stored'=>false,'session_issued'=>false,'browser_session_activated'=>false,'assurance_elevated'=>false,
            'role_granted'=>false,'provider_or_configuration_mutated'=>false,'payment_authority_changed'=>false,
            'payroll_authority_changed'=>false,'accounting_authority_changed'=>false)))!==false;
}

/**
 * Create exactly one first link from an accepted decision to one explicit local user.
 *
 * Link creation is committed before any separate existing-link session issuance call.
 * Exact retry returns the same link without a second row/audit event. Deprovisioned
 * or conflicting rows never auto-relink.
 */
function federation_create_first_external_subject_link($company, $verifier_decision_id, $user_id, $now_epoch=null)
{
    global $path_to_root;
    $request=federation_first_link_request($company,$verifier_decision_id,$user_id,$now_epoch);
    if ($request===false)
        return false;
    if (!function_exists('append_domain_audit_event')) {
        $root=isset($path_to_root)?realpath($path_to_root):false;
        if ($root===false)
            return false;
        $audit=$root.DIRECTORY_SEPARATOR.'includes'.DIRECTORY_SEPARATOR.'db'.DIRECTORY_SEPARATOR.'domain_audit_db.inc';
        if (!is_file($audit))
            return false;
        require_once($audit);
    }
    if (!function_exists('append_domain_audit_event') || !db_query('BEGIN','could not start PAY-SEC-004 first-link transaction'))
        return false;
    $committed=false;$result_value=false;
    do {
        $row=federation_first_link_custody_for_update($request['verifier_decision_id'],$request['user_id']);
        if (!is_array($row))
            break;
        $now=(int)$request['now_epoch'];$valid_until=strtotime((string)$row['valid_until'].' UTC');$verified_at=strtotime((string)$row['verified_at'].' UTC');
        if ((string)$row['provider_status']!=='enabled' || (string)$row['config_status']!=='enabled'
            || $row['consumed_at']===null || (int)$row['user_inactive']!==0 || (int)$row['role_inactive']!==0
            || (int)$row['verified_assurance_level']!==1 || (string)$row['assurance_method']!=='oidc_rs256'
            || (int)$row['assurance_level_cap']<1 || $valid_until===false || $verified_at===false
            || $now >= $valid_until || $verified_at > $now+(int)$row['max_clock_skew_seconds'])
            break;
        $collisions=federation_first_link_collisions_for_update($row);
        if (!is_array($collisions))
            break;
        $existing=federation_first_link_existing_exact($collisions,$row);
        if ($existing===false)
            break;
        if (is_array($existing)) {
            if (!db_query('COMMIT','could not commit PAY-SEC-004 idempotent first-link verification'))
                break;
            $committed=true;$result_value=array('external_subject_link_id'=>(int)$existing['external_subject_link_id'],
                'provider_id'=>(int)$row['provider_id'],'user_id'=>(int)$row['user_id'],'verifier_decision_id'=>(int)$row['verifier_decision_id'],
                'idempotent_retry'=>true,'session_issued'=>false,'browser_session_activated'=>false);break;
        }
        $sql="INSERT INTO ".TB_PREF."user_federation_external_subject_links(provider_id,user_id,external_subject_hash,verifier_transaction_id,link_status,linked_at,deprovisioned_at,deprovisioned_by,deprovision_reason) VALUES ("
            .(int)$row['provider_id'].",".(int)$row['user_id'].",".db_escape((string)$row['external_subject_hash']).",".(int)$row['verifier_transaction_id'].",'active',UTC_TIMESTAMP(),NULL,NULL,NULL)";
        if (!db_query($sql,'could not create PAY-SEC-004 explicit first link'))
            break;
        $link_id=function_exists('db_insert_id')?(int)db_insert_id():0;
        if ($link_id<=0 || !federation_first_link_audit($request['company'],$row,$link_id,federation_first_link_actor_id()))
            break;
        if (!db_query('COMMIT','could not commit PAY-SEC-004 explicit first link'))
            break;
        $committed=true;$result_value=array('external_subject_link_id'=>$link_id,'provider_id'=>(int)$row['provider_id'],
            'user_id'=>(int)$row['user_id'],'verifier_decision_id'=>(int)$row['verifier_decision_id'],
            'idempotent_retry'=>false,'session_issued'=>false,'browser_session_activated'=>false);
    } while(false);
    if (!$committed) {
        @db_query('ROLLBACK','could not roll back PAY-SEC-004 first-link transaction');
        return false;
    }
    return $result_value;
}
