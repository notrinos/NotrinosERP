<?php
/**
 * PAY-SEC-004 scoped local break-glass designation command foundation.
 *
 * Internal-only pre-designation/revocation over the accepted
 * user_break_glass_designations custody. Designation is policy metadata, not
 * live emergency authority: no route, login bypass, role grant, session lease,
 * recovery bypass, payroll-post, payment, GL or accounting authority is added.
 */

if (!defined('BREAK_GLASS_DESIGNATION_SCOPE'))
    define('BREAK_GLASS_DESIGNATION_SCOPE', 'security_recovery');
if (!defined('BREAK_GLASS_DESIGNATION_AUTH_METHOD'))
    define('BREAK_GLASS_DESIGNATION_AUTH_METHOD', 'local_password');
if (!defined('BREAK_GLASS_DESIGNATION_MAX_ACTIVATION_SECONDS'))
    define('BREAK_GLASS_DESIGNATION_MAX_ACTIVATION_SECONDS', 900);

function break_glass_designation_current_actor_id()
{
    if (!isset($_SESSION['wa_current_user']) || !is_object($_SESSION['wa_current_user'])
        || !method_exists($_SESSION['wa_current_user'], 'logged_in')
        || !$_SESSION['wa_current_user']->logged_in())
        return 0;
    return isset($_SESSION['wa_current_user']->user)
        ? (int)$_SESSION['wa_current_user']->user : 0;
}

function break_glass_designation_company_matches($company)
{
    $company = (int)$company;
    return $company >= 0 && function_exists('user_company')
        && (int)user_company() === $company;
}

/** Existing user-administration authority only; self-designation is denied. */
function break_glass_designation_authorized($company, $user_id)
{
    $actor_id = break_glass_designation_current_actor_id();
    $user_id = (int)$user_id;
    return $actor_id > 0 && $user_id > 0 && $actor_id !== $user_id
        && break_glass_designation_company_matches($company)
        && function_exists('user_check_access') && user_check_access('SA_USERS');
}

/** Verify that an account has a usable local-password credential representation. */
function break_glass_designation_local_password_hash_usable($hash)
{
    if (!is_string($hash) || $hash === '' || strlen($hash) > 255)
        return false;
    if (preg_match('/^[a-f0-9]{32}$/Di', $hash) === 1)
        return true; // Legacy local-password compatibility is verification-only.
    if (!function_exists('password_get_info'))
        return false;
    $info = password_get_info($hash);
    return is_array($info) && isset($info['algoName']) && $info['algoName'] !== 'unknown';
}

function break_glass_designation_target_for_update($user_id)
{
    $user_id = (int)$user_id;
    if ($user_id <= 0)
        return false;
    $result = db_query(
        "SELECT u.id user_id,u.login_id,u.password,u.role_id,u.inactive user_inactive,r.inactive role_inactive "
        ."FROM ".TB_PREF."users u JOIN ".TB_PREF."security_roles r ON r.id=u.role_id "
        ."WHERE u.id=".$user_id." FOR UPDATE",
        'could not lock PAY-SEC-004 break-glass designation target'
    );
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    if (!is_array($row) || db_fetch_assoc($result) !== null)
        return false;
    return $row;
}

/** Return one current policy row, null when absent, false on malformed query state. */
function break_glass_designation_row_for_update($user_id)
{
    $result = db_query(
        "SELECT break_glass_designation_id,user_id,scope_code,required_auth_method,max_activation_seconds,"
        ."designated_at,designated_by,revoked_at,revoked_by,revocation_reason,row_version "
        ."FROM ".TB_PREF."user_break_glass_designations "
        ."WHERE user_id=".(int)$user_id." AND scope_code=".db_escape(BREAK_GLASS_DESIGNATION_SCOPE)." FOR UPDATE",
        'could not lock PAY-SEC-004 break-glass designation custody'
    );
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    if ($row === null || $row === false)
        return null;
    if (!is_array($row) || db_fetch_assoc($result) !== null)
        return false;
    return $row;
}

function break_glass_designation_revocation_reason($reason)
{
    $allowed = array('administrator_revoked','account_inactive','role_inactive','policy_changed');
    return is_string($reason) && in_array($reason, $allowed, true) ? $reason : false;
}

function break_glass_designation_load_audit_gateway()
{
    global $path_to_root;
    if (function_exists('append_domain_audit_event'))
        return true;
    $root = isset($path_to_root) ? realpath($path_to_root) : false;
    if ($root === false)
        return false;
    $file = $root.DIRECTORY_SEPARATOR.'includes'.DIRECTORY_SEPARATOR.'db'.DIRECTORY_SEPARATOR.'domain_audit_db.inc';
    if (!is_file($file))
        return false;
    require_once($file);
    return function_exists('append_domain_audit_event');
}

/** Load the accepted activation cleanup command for ordered designation revocation. */
function break_glass_designation_load_activation_gateway()
{
    global $path_to_root;
    if (function_exists('break_glass_activation_active_lease_for_update')
        && function_exists('break_glass_activation_terminalize_locked_lease'))
        return true;
    $root = isset($path_to_root) ? realpath($path_to_root) : false;
    if ($root === false)
        return false;
    $file = $root.DIRECTORY_SEPARATOR.'includes'.DIRECTORY_SEPARATOR.'break_glass_activation.inc';
    if (!is_file($file))
        return false;
    require_once($file);
    return function_exists('break_glass_activation_active_lease_for_update')
        && function_exists('break_glass_activation_terminalize_locked_lease');
}

function break_glass_designation_audit($company, $designation_id, $user_id, $actor_id, $action, $reason_code, $before, $after, $activation_cleanup=false)
{
    return append_domain_audit_event(array(
        'company_id'=>(int)$company,'legal_entity_id'=>0,
        'entity_type'=>'security_break_glass_designation',
        'entity_key'=>array('break_glass_designation_id'=>(int)$designation_id,'user_id'=>(int)$user_id),
        'action'=>$action,'reason_code'=>$reason_code,
        'before'=>$before,'after'=>$after,
        'restricted_diff'=>array(
            'actor_id'=>(int)$actor_id,'user_id'=>(int)$user_id,'authorization'=>'SA_USERS',
            'self_designation_allowed'=>false,'fixed_scope'=>BREAK_GLASS_DESIGNATION_SCOPE,
            'required_auth_method'=>BREAK_GLASS_DESIGNATION_AUTH_METHOD,
            'max_activation_seconds'=>BREAK_GLASS_DESIGNATION_MAX_ACTIVATION_SECONDS,
            'local_password_credential_required'=>true,'active_user_required_for_designation'=>true,
            'active_role_required_for_designation'=>true,'activation_lease_created'=>false,
            'activation_challenge_performed'=>false,'provider_outage_bypass_added'=>false,
            'federation_bypass_added'=>false,'recovery_bypass_added'=>false,
            'durable_sessions_mutated'=>(bool)$activation_cleanup,'activation_lease_terminalized'=>(bool)$activation_cleanup,
            'role_granted'=>false,'security_area_granted'=>false,
            'payroll_post_authority_changed'=>false,'payment_authority_changed'=>false,
            'accounting_authority_changed'=>false,'historical_rows_rewritten'=>false))) !== false;
}

/**
 * Pre-designate one active local-password account for future security_recovery use.
 * Exact active retry is idempotent. A previously revoked row may be re-designated
 * in place under a new restricted audit event; row_version prevents stale writes.
 */
function break_glass_designate_user($company, $user_id, $now_epoch=null)
{
    $company = (int)$company;$user_id = (int)$user_id;
    $actor_id = break_glass_designation_current_actor_id();
    $now = $now_epoch === null ? time() : $now_epoch;
    if (!break_glass_designation_authorized($company, $user_id)
        || !is_int($now) || $now <= 0 || !break_glass_designation_load_audit_gateway()
        || !function_exists('begin_transaction') || !function_exists('commit_transaction')
        || !function_exists('cancel_transaction'))
        return false;

    begin_transaction();$committed=false;$result_value=false;
    do {
        $target = break_glass_designation_target_for_update($user_id);
        if (!is_array($target) || (int)$target['user_inactive'] !== 0 || (int)$target['role_inactive'] !== 0
            || !break_glass_designation_local_password_hash_usable((string)$target['password']))
            break;
        $row = break_glass_designation_row_for_update($user_id);
        if ($row === false)
            break;
        if (is_array($row) && $row['revoked_at'] === null) {
            if ((string)$row['scope_code'] !== BREAK_GLASS_DESIGNATION_SCOPE
                || (string)$row['required_auth_method'] !== BREAK_GLASS_DESIGNATION_AUTH_METHOD
                || (int)$row['max_activation_seconds'] !== BREAK_GLASS_DESIGNATION_MAX_ACTIVATION_SECONDS)
                break;
            commit_transaction();$committed=true;
            $result_value=array('break_glass_designation_id'=>(int)$row['break_glass_designation_id'],
                'user_id'=>$user_id,'idempotent_retry'=>true,'reactivated'=>false,
                'live_activation_added'=>false,'role_granted'=>false);break;
        }
        $designated_at = gmdate('Y-m-d H:i:s', $now);
        $reactivated = is_array($row);
        if ($reactivated) {
            $id=(int)$row['break_glass_designation_id'];$version=(int)$row['row_version'];
            $sql="UPDATE ".TB_PREF."user_break_glass_designations SET required_auth_method=".db_escape(BREAK_GLASS_DESIGNATION_AUTH_METHOD)
                .",max_activation_seconds=".(int)BREAK_GLASS_DESIGNATION_MAX_ACTIVATION_SECONDS
                .",designated_at=".db_escape($designated_at).",designated_by=".$actor_id
                .",revoked_at=NULL,revoked_by=NULL,revocation_reason=NULL,row_version=row_version+1 "
                ."WHERE break_glass_designation_id=".$id." AND row_version=".$version." AND revoked_at IS NOT NULL";
            if (!db_query($sql,'could not reactivate PAY-SEC-004 break-glass designation')
                || !function_exists('db_num_affected_rows') || (int)db_num_affected_rows() !== 1)
                break;
        } else {
            $sql="INSERT INTO ".TB_PREF."user_break_glass_designations(user_id,scope_code,required_auth_method,max_activation_seconds,designated_at,designated_by,revoked_at,revoked_by,revocation_reason,row_version) VALUES ("
                .$user_id.",".db_escape(BREAK_GLASS_DESIGNATION_SCOPE).",".db_escape(BREAK_GLASS_DESIGNATION_AUTH_METHOD).",".(int)BREAK_GLASS_DESIGNATION_MAX_ACTIVATION_SECONDS.",".db_escape($designated_at).",".$actor_id.",NULL,NULL,NULL,1)";
            if (!db_query($sql,'could not create PAY-SEC-004 break-glass designation'))
                break;
            $id=function_exists('db_insert_id')?(int)db_insert_id():0;
            if ($id <= 0)
                break;
        }
        if (!break_glass_designation_audit($company,$id,$user_id,$actor_id,
            'security.break_glass.designation.designated','pay_sec_004_break_glass_designation_command',
            array('designated'=>false,'previously_revoked'=>$reactivated),
            array('designated'=>true,'reactivated'=>$reactivated)))
            break;
        commit_transaction();$committed=true;
        $result_value=array('break_glass_designation_id'=>$id,'user_id'=>$user_id,
            'idempotent_retry'=>false,'reactivated'=>$reactivated,'live_activation_added'=>false,'role_granted'=>false);
    } while(false);
    if (!$committed){cancel_transaction();return false;}
    return $result_value;
}

/** Terminally revoke current designation policy; exact retry is idempotent. */
function break_glass_revoke_designation($company, $user_id, $reason='administrator_revoked', $now_epoch=null)
{
    $company=(int)$company;$user_id=(int)$user_id;$actor_id=break_glass_designation_current_actor_id();
    $now=$now_epoch===null?time():$now_epoch;$reason=break_glass_designation_revocation_reason($reason);
    if (!break_glass_designation_authorized($company,$user_id) || $reason===false
        || !is_int($now) || $now<=0 || !break_glass_designation_load_audit_gateway()
        || !function_exists('begin_transaction') || !function_exists('commit_transaction')
        || !function_exists('cancel_transaction'))
        return false;
    begin_transaction();$committed=false;$result_value=false;
    do {
        $row=break_glass_designation_row_for_update($user_id);
        if (!is_array($row))
            break;
        $id=(int)$row['break_glass_designation_id'];
        if ($row['revoked_at'] !== null) {
            commit_transaction();$committed=true;
            $result_value=array('break_glass_designation_id'=>$id,'user_id'=>$user_id,
                'revoked'=>true,'idempotent_retry'=>true,'live_activation_added'=>false);break;
        }
        if (!break_glass_designation_load_activation_gateway())
            break;
        $active_lease=break_glass_activation_active_lease_for_update($id);
        if ($active_lease===false)
            break;
        $activation_cleanup=is_array($active_lease);
        if ($activation_cleanup && !break_glass_activation_terminalize_locked_lease(
            $company,$active_lease,$actor_id,'designation_revoked',$now))
            break;
        $version=(int)$row['row_version'];$revoked_at=gmdate('Y-m-d H:i:s',$now);
        $sql="UPDATE ".TB_PREF."user_break_glass_designations SET revoked_at=".db_escape($revoked_at)
            .",revoked_by=".$actor_id.",revocation_reason=".db_escape($reason).",row_version=row_version+1 "
            ."WHERE break_glass_designation_id=".$id." AND row_version=".$version." AND revoked_at IS NULL";
        if (!db_query($sql,'could not revoke PAY-SEC-004 break-glass designation')
            || !function_exists('db_num_affected_rows') || (int)db_num_affected_rows() !== 1)
            break;
        if (!break_glass_designation_audit($company,$id,$user_id,$actor_id,
            'security.break_glass.designation.revoked',$reason,
            array('designated'=>true,'revoked'=>false),
            array('designated'=>true,'revoked'=>true,'active_lease_terminalized'=>$activation_cleanup),
            $activation_cleanup))
            break;
        commit_transaction();$committed=true;
        $result_value=array('break_glass_designation_id'=>$id,'user_id'=>$user_id,
            'revoked'=>true,'idempotent_retry'=>false,'live_activation_added'=>false,
            'active_lease_terminalized'=>$activation_cleanup);
    } while(false);
    if (!$committed){cancel_transaction();return false;}
    return $result_value;
}
