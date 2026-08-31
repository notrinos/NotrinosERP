<?php
/**
 * PAY-SEC-004 scoped local break-glass activation command foundation.
 *
 * Internal-only activation and terminalization over the accepted designation,
 * activation-lease and durable-session custody. The command proves the
 * pre-designated target's normal local password, creates one bounded level-one
 * durable session, and never grants a role/security area or payroll authority.
 */

if (!defined('BREAK_GLASS_ACTIVATION_SCOPE'))
    define('BREAK_GLASS_ACTIVATION_SCOPE', 'security_recovery');
if (!defined('BREAK_GLASS_ACTIVATION_AUTH_METHOD'))
    define('BREAK_GLASS_ACTIVATION_AUTH_METHOD', 'local_password');
if (!defined('BREAK_GLASS_ACTIVATION_MAX_SECONDS'))
    define('BREAK_GLASS_ACTIVATION_MAX_SECONDS', 900);

/** Load the accepted designation/session/audit dependencies without adding a route. */
function break_glass_activation_load_dependencies()
{
    global $path_to_root;
    $root = isset($path_to_root) ? realpath($path_to_root) : false;
    if ($root === false)
        return false;
    $required = array(
        'includes/break_glass_designation.inc',
        'includes/session_registry.inc'
    );
    foreach ($required as $relative) {
        $file = $root.DIRECTORY_SEPARATOR.str_replace('/', DIRECTORY_SEPARATOR, $relative);
        if (!is_file($file))
            return false;
        require_once($file);
    }
    if (!function_exists('append_domain_audit_event')) {
        $audit_file = $root.DIRECTORY_SEPARATOR.'includes'.DIRECTORY_SEPARATOR.'db'
            .DIRECTORY_SEPARATOR.'domain_audit_db.inc';
        if (!is_file($audit_file))
            return false;
        require_once($audit_file);
    }
    return function_exists('break_glass_designation_row_for_update')
        && function_exists('session_registry_hash')
        && function_exists('append_domain_audit_event');
}

/** Return the authenticated same-company target actor; activation is never delegated. */
function break_glass_activation_self_authorized($company, $user_id)
{
    $company = (int)$company;
    $user_id = (int)$user_id;
    $actor_id = function_exists('break_glass_designation_current_actor_id')
        ? break_glass_designation_current_actor_id() : 0;
    return $actor_id > 0 && $actor_id === $user_id
        && function_exists('break_glass_designation_company_matches')
        && break_glass_designation_company_matches($company);
}

/** Validate one finite activation reason. */
function break_glass_activation_reason($reason)
{
    $allowed = array('provider_outage','provider_degraded','security_incident','recovery_drill');
    return is_string($reason) && in_array($reason, $allowed, true) ? $reason : false;
}

/** Validate one finite terminal reason accepted by the internal command family. */
function break_glass_activation_terminal_reason($reason)
{
    $allowed = array('return_to_normal','administrator_revoked','designation_revoked',
        'lease_expired','account_inactive','role_inactive','policy_changed');
    return is_string($reason) && in_array($reason, $allowed, true) ? $reason : false;
}

/** Validate caller-held durable-session material; only its SHA-256 is stored. */
function break_glass_activation_session_secret_usable($secret)
{
    return is_string($secret) && strlen($secret) >= 32 && strlen($secret) <= 512
        && strpos($secret, "\0") === false;
}

/** Validate bounded ticket input before hashing; plaintext is never persisted or audited. */
function break_glass_activation_ticket_hash($ticket_reference)
{
    if (!is_string($ticket_reference) || $ticket_reference === '' || strlen($ticket_reference) > 256
        || strpos($ticket_reference, "\0") !== false)
        return false;
    return hash('sha256', $ticket_reference);
}

/** Verify a fresh normal local-password proof without mutating credential storage. */
function break_glass_activation_local_password_matches($password, $stored_hash)
{
    if (!is_string($password) || $password === '' || strlen($password) > 4096
        || !is_string($stored_hash) || $stored_hash === '')
        return false;
    if (function_exists('password_verify') && password_verify($password, $stored_hash))
        return true;
    return preg_match('/^[a-f0-9]{32}$/Di', $stored_hash) === 1
        && session_registry_hash_equals(strtolower($stored_hash), md5($password));
}

/** Lock the one active lease for a designation, or return null when absent. */
function break_glass_activation_active_lease_for_update($designation_id)
{
    $designation_id = (int)$designation_id;
    if ($designation_id <= 0)
        return false;
    $sql = "SELECT l.break_glass_activation_lease_id,l.break_glass_designation_id,l.user_id,l.scope_code,"
        ."l.required_auth_method,l.reason_code,l.ticket_ref_hash,l.activated_at,l.expires_at,l.session_id,"
        ."l.active_slot,l.terminalized_at,l.terminalized_by,l.terminal_reason,l.row_version,"
        ."s.session_key_hash,s.role_id session_role_id,s.auth_method session_auth_method,"
        ."s.assurance_level session_assurance_level,s.assurance_method session_assurance_method,"
        ."s.absolute_expires_at session_absolute_expires_at,s.revoked_at session_revoked_at "
        ."FROM ".TB_PREF."user_break_glass_activation_leases l "
        ."LEFT JOIN ".TB_PREF."user_sessions s ON s.id=l.session_id "
        ."WHERE l.break_glass_designation_id=".$designation_id." AND l.active_slot=1 FOR UPDATE";
    $result = db_query($sql, 'could not lock PAY-SEC-004 break-glass active lease');
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    if ($row === null || $row === false)
        return null;
    if (!is_array($row) || db_fetch_assoc($result) !== null)
        return false;
    return $row;
}

/** Append restricted activation/terminal evidence without plaintext password or ticket content. */
function break_glass_activation_audit($company, $lease_id, $designation_id, $user_id, $actor_id,
    $action, $reason_code, $ticket_ref_hash, $before, $after)
{
    $restricted = array(
        'actor_id'=>(int)$actor_id,'user_id'=>(int)$user_id,
        'break_glass_designation_id'=>(int)$designation_id,
        'fixed_scope'=>BREAK_GLASS_ACTIVATION_SCOPE,
        'required_auth_method'=>BREAK_GLASS_ACTIVATION_AUTH_METHOD,
        'max_activation_seconds'=>BREAK_GLASS_ACTIVATION_MAX_SECONDS,
        'local_password_proof_required'=>true,
        'ticket_ref_hash'=>$ticket_ref_hash === false ? null : (string)$ticket_ref_hash,
        'plaintext_ticket_stored'=>false,'plaintext_password_stored'=>false,
        'assurance_level'=>1,'assurance_elevated'=>false,
        'role_granted'=>false,'security_area_granted'=>false,
        'provider_bypass_added'=>false,'recovery_bypass_added'=>false,
        'payroll_post_authority_changed'=>false,'payment_authority_changed'=>false,
        'accounting_authority_changed'=>false,'historical_rows_rewritten'=>false
    );
    return append_domain_audit_event(array(
        'company_id'=>(int)$company,'legal_entity_id'=>0,
        'entity_type'=>'security_break_glass_activation_lease',
        'entity_key'=>array('break_glass_activation_lease_id'=>(int)$lease_id,
            'break_glass_designation_id'=>(int)$designation_id,'user_id'=>(int)$user_id),
        'action'=>$action,'reason_code'=>$reason_code,
        'before'=>$before,'after'=>$after,'restricted_diff'=>$restricted
    )) !== false;
}

/**
 * Revoke a bound durable session and then terminalize the locked active lease.
 * Caller owns the surrounding transaction and lock order.
 */
function break_glass_activation_terminalize_locked_lease($company, $lease, $actor_id, $reason, $now_epoch)
{
    $reason = break_glass_activation_terminal_reason($reason);
    if (!is_array($lease) || $reason === false || !is_int($now_epoch) || $now_epoch <= 0)
        return false;
    if ((int)$lease['active_slot'] !== 1 || $lease['terminalized_at'] !== null)
        return false;
    $lease_id = (int)$lease['break_glass_activation_lease_id'];
    $designation_id = (int)$lease['break_glass_designation_id'];
    $user_id = (int)$lease['user_id'];
    if ($lease_id <= 0 || $designation_id <= 0 || $user_id <= 0)
        return false;
    $terminalized_at = gmdate('Y-m-d H:i:s', $now_epoch);
    $session_id = $lease['session_id'] === null ? 0 : (int)$lease['session_id'];
    if ($session_id > 0) {
        $session_reason = 'break_glass_'.$reason;
        if (strlen($session_reason) > 40)
            return false;
        $sql = "UPDATE ".TB_PREF."user_sessions SET revoked_at=COALESCE(revoked_at,".db_escape($terminalized_at)."),"
            ."revoked_by=COALESCE(revoked_by,".(int)$actor_id."),revocation_reason=COALESCE(revocation_reason,"
            .db_escape($session_reason).") WHERE id=".$session_id." AND user_id=".$user_id;
        if (!db_query($sql, 'could not revoke PAY-SEC-004 break-glass durable session'))
            return false;
    }
    $version = (int)$lease['row_version'];
    $sql = "UPDATE ".TB_PREF."user_break_glass_activation_leases SET terminalized_at=".db_escape($terminalized_at)
        .",terminalized_by=".(int)$actor_id.",terminal_reason=".db_escape($reason)
        .",active_slot=NULL,row_version=row_version+1 WHERE break_glass_activation_lease_id=".$lease_id
        ." AND row_version=".$version." AND active_slot=1 AND terminalized_at IS NULL";
    if (!db_query($sql, 'could not terminalize PAY-SEC-004 break-glass activation lease')
        || !function_exists('db_num_affected_rows') || (int)db_num_affected_rows() !== 1)
        return false;
    return break_glass_activation_audit($company,$lease_id,$designation_id,$user_id,$actor_id,
        'security.break_glass.activation.terminalized',$reason,false,
        array('active'=>true,'session_id'=>$session_id > 0 ? $session_id : null),
        array('active'=>false,'session_revoked'=>$session_id > 0));
}

/** Return whether an existing active lease is the exact safe idempotent retry. */
function break_glass_activation_retry_matches($lease, $request, $role_id)
{
    if (!is_array($lease) || !is_array($request) || (int)$lease['session_id'] <= 0)
        return false;
    $activated_epoch = strtotime((string)$lease['activated_at'].' UTC');
    $expires_epoch = strtotime((string)$lease['expires_at'].' UTC');
    return (int)$lease['user_id'] === (int)$request['user_id']
        && (string)$lease['scope_code'] === BREAK_GLASS_ACTIVATION_SCOPE
        && (string)$lease['required_auth_method'] === BREAK_GLASS_ACTIVATION_AUTH_METHOD
        && (string)$lease['reason_code'] === (string)$request['reason_code']
        && session_registry_hash_equals((string)$lease['ticket_ref_hash'], (string)$request['ticket_ref_hash'])
        && $activated_epoch !== false && $expires_epoch !== false
        && ($expires_epoch - $activated_epoch) === (int)$request['requested_seconds']
        && $expires_epoch > (int)$request['now_epoch']
        && $lease['session_revoked_at'] === null
        && session_registry_hash_equals((string)$lease['session_key_hash'], (string)$request['session_key_hash'])
        && (int)$lease['session_role_id'] === (int)$role_id
        && (string)$lease['session_auth_method'] === BREAK_GLASS_ACTIVATION_AUTH_METHOD
        && (int)$lease['session_assurance_level'] === 1
        && (string)$lease['session_assurance_method'] === ''
        && (string)$lease['session_absolute_expires_at'] === (string)$lease['expires_at'];
}

/**
 * Activate one pre-designated target after a fresh local-password proof.
 * The caller supplies a high-entropy session secret and keeps it; only SHA-256
 * custody is persisted. Exact active retries are idempotent.
 */
function break_glass_activate_designated_user($company, $user_id, $password, $reason_code,
    $ticket_reference, $requested_seconds, $session_secret, $now_epoch=null)
{
    $company=(int)$company;$user_id=(int)$user_id;
    $now=$now_epoch===null?time():$now_epoch;
    $reason=break_glass_activation_reason($reason_code);
    $ticket_hash=break_glass_activation_ticket_hash($ticket_reference);
    $requested_seconds=(int)$requested_seconds;
    if (!is_int($now) || $now<=0 || $reason===false || $ticket_hash===false
        || $requested_seconds<1 || $requested_seconds>BREAK_GLASS_ACTIVATION_MAX_SECONDS
        || !break_glass_activation_session_secret_usable($session_secret)
        || !break_glass_activation_load_dependencies()
        || !break_glass_activation_self_authorized($company,$user_id)
        || !function_exists('begin_transaction') || !function_exists('commit_transaction')
        || !function_exists('cancel_transaction'))
        return false;
    $request=array('user_id'=>$user_id,'reason_code'=>$reason,'ticket_ref_hash'=>$ticket_hash,
        'requested_seconds'=>$requested_seconds,'session_key_hash'=>session_registry_hash($session_secret),
        'now_epoch'=>$now,'user_agent_hash'=>session_registry_user_agent_hash());

    begin_transaction();$committed=false;$result_value=false;
    do {
        $target=break_glass_designation_target_for_update($user_id);
        if (!is_array($target) || (int)$target['user_inactive']!==0 || (int)$target['role_inactive']!==0
            || !break_glass_designation_local_password_hash_usable((string)$target['password'])
            || !break_glass_activation_local_password_matches($password,(string)$target['password']))
            break;
        $designation=break_glass_designation_row_for_update($user_id);
        if (!is_array($designation) || $designation['revoked_at']!==null
            || (string)$designation['scope_code']!==BREAK_GLASS_ACTIVATION_SCOPE
            || (string)$designation['required_auth_method']!==BREAK_GLASS_ACTIVATION_AUTH_METHOD)
            break;
        $max_seconds=min(BREAK_GLASS_ACTIVATION_MAX_SECONDS,(int)$designation['max_activation_seconds']);
        if ($max_seconds<1 || $requested_seconds>$max_seconds)
            break;
        $lease=break_glass_activation_active_lease_for_update((int)$designation['break_glass_designation_id']);
        if ($lease===false)
            break;
        if (is_array($lease)) {
            $lease_expires=strtotime((string)$lease['expires_at'].' UTC');
            if ($lease_expires===false)
                break;
            if ($now >= $lease_expires) {
                if (!break_glass_activation_terminalize_locked_lease($company,$lease,$user_id,'lease_expired',$now))
                    break;
                $lease=null;
            } elseif (break_glass_activation_retry_matches($lease,$request,(int)$target['role_id'])) {
                commit_transaction();$committed=true;
                $result_value=array('break_glass_activation_lease_id'=>(int)$lease['break_glass_activation_lease_id'],
                    'session_id'=>(int)$lease['session_id'],'user_id'=>$user_id,
                    'expires_at'=>(string)$lease['expires_at'],'idempotent_retry'=>true,
                    'assurance_level'=>1,'role_granted'=>false);break;
            } else {
                break;
            }
        }
        $activated_at=gmdate('Y-m-d H:i:s',$now);$expires_at=gmdate('Y-m-d H:i:s',$now+$requested_seconds);
        $insert="INSERT INTO ".TB_PREF."user_break_glass_activation_leases(break_glass_designation_id,user_id,scope_code,required_auth_method,reason_code,ticket_ref_hash,activated_at,expires_at,session_id,active_slot,terminalized_at,terminalized_by,terminal_reason,row_version) VALUES ("
            .(int)$designation['break_glass_designation_id'].",".$user_id.",".db_escape(BREAK_GLASS_ACTIVATION_SCOPE).",".db_escape(BREAK_GLASS_ACTIVATION_AUTH_METHOD).",".db_escape($reason).",".db_escape($ticket_hash).",".db_escape($activated_at).",".db_escape($expires_at).",NULL,1,NULL,NULL,NULL,1)";
        if (!db_query($insert,'could not reserve PAY-SEC-004 break-glass activation lease'))
            break;
        $lease_id=function_exists('db_insert_id')?(int)db_insert_id():0;
        if ($lease_id<=0)
            break;
        $idle_epoch=min($now+$requested_seconds,$now+session_registry_idle_lifetime(SESSION_REGISTRY_IDLE_LIFETIME));
        $idle_expires_at=gmdate('Y-m-d H:i:s',$idle_epoch);
        $session="INSERT INTO ".TB_PREF."user_sessions(session_key_hash,user_id,role_id,auth_method,assurance_level,assurance_method,assurance_issued_at,assurance_expires_at,created_at,last_seen_at,idle_expires_at,absolute_expires_at,user_agent_hash) VALUES ("
            .db_escape($request['session_key_hash']).",".$user_id.",".(int)$target['role_id'].",".db_escape(BREAK_GLASS_ACTIVATION_AUTH_METHOD).",1,'',NULL,NULL,"
            .db_escape($activated_at).",".db_escape($activated_at).",".db_escape($idle_expires_at).",".db_escape($expires_at).",".db_escape($request['user_agent_hash']).")";
        if (!db_query($session,'could not issue PAY-SEC-004 break-glass durable session'))
            break;
        $session_id=(int)db_insert_id();
        if ($session_id<=0)
            break;
        $bind="UPDATE ".TB_PREF."user_break_glass_activation_leases SET session_id=".$session_id.",row_version=row_version+1 WHERE break_glass_activation_lease_id=".$lease_id." AND active_slot=1 AND terminalized_at IS NULL AND session_id IS NULL AND row_version=1";
        if (!db_query($bind,'could not bind PAY-SEC-004 break-glass session to lease')
            || !function_exists('db_num_affected_rows') || (int)db_num_affected_rows()!==1)
            break;
        if (!break_glass_activation_audit($company,$lease_id,(int)$designation['break_glass_designation_id'],$user_id,$user_id,
            'security.break_glass.activation.activated',$reason,$ticket_hash,
            array('active'=>false,'session_id'=>null),array('active'=>true,'session_id'=>$session_id,'expires_at'=>$expires_at)))
            break;
        commit_transaction();$committed=true;
        $result_value=array('break_glass_activation_lease_id'=>$lease_id,'session_id'=>$session_id,
            'user_id'=>$user_id,'expires_at'=>$expires_at,'idempotent_retry'=>false,
            'assurance_level'=>1,'role_granted'=>false);
    } while(false);
    if (!$committed){cancel_transaction();return false;}
    return $result_value;
}

/**
 * Explicitly terminate the target's active lease. Self-service is limited to
 * return-to-normal/expiry; SA_USERS may perform bounded administrative cleanup.
 */
function break_glass_end_activation($company, $user_id, $reason='return_to_normal', $now_epoch=null)
{
    $company=(int)$company;$user_id=(int)$user_id;$now=$now_epoch===null?time():$now_epoch;
    if (!break_glass_activation_load_dependencies() || !is_int($now) || $now<=0)
        return false;
    $reason=break_glass_activation_terminal_reason($reason);
    if ($reason===false)
        return false;
    $actor_id=break_glass_designation_current_actor_id();
    $self=$actor_id>0&&$actor_id===$user_id&&break_glass_designation_company_matches($company);
    $admin=$actor_id>0&&$actor_id!==$user_id&&break_glass_designation_company_matches($company)
        && function_exists('user_check_access')&&user_check_access('SA_USERS');
    if (($self && !in_array($reason,array('return_to_normal','lease_expired'),true))
        || ($admin && !in_array($reason,array('administrator_revoked','lease_expired','account_inactive','role_inactive','policy_changed'),true))
        || (!$self && !$admin))
        return false;
    if (!function_exists('begin_transaction') || !function_exists('commit_transaction') || !function_exists('cancel_transaction'))
        return false;
    begin_transaction();$committed=false;$result_value=false;
    do {
        $designation=break_glass_designation_row_for_update($user_id);
        if (!is_array($designation))
            break;
        $lease=break_glass_activation_active_lease_for_update((int)$designation['break_glass_designation_id']);
        if ($lease===false)
            break;
        if ($lease===null) {
            commit_transaction();$committed=true;
            $result_value=array('user_id'=>$user_id,'terminalized'=>true,'idempotent_retry'=>true);break;
        }
        if ($reason==='lease_expired') {
            $expires=strtotime((string)$lease['expires_at'].' UTC');
            if ($expires===false || $now<$expires)
                break;
        }
        if (!break_glass_activation_terminalize_locked_lease($company,$lease,$actor_id,$reason,$now))
            break;
        commit_transaction();$committed=true;
        $result_value=array('break_glass_activation_lease_id'=>(int)$lease['break_glass_activation_lease_id'],
            'session_id'=>$lease['session_id']===null?null:(int)$lease['session_id'],'user_id'=>$user_id,
            'terminalized'=>true,'idempotent_retry'=>false,'terminal_reason'=>$reason);
    } while(false);
    if (!$committed){cancel_transaction();return false;}
    return $result_value;
}
