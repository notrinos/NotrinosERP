<?php
/**********************************************************************
    PAY-SEC-004 explicit first-link administration/public-login completion.

    This helper carries only a short-lived numeric verifier-decision reference
    in the anonymous PHP session and exposes an SA_USERS-only wrapper around
    the already-accepted first-link command. It never stores or accepts raw
    external subjects, tokens, email/display-name/login profile claims, and it
    never issues or adopts a browser session.
***********************************************************************/

if (!defined('FEDERATION_FIRST_LINK_PENDING_REQUEST_KEY'))
    define('FEDERATION_FIRST_LINK_PENDING_REQUEST_KEY', 'federation_first_link_pending_request');
if (!defined('FEDERATION_FIRST_LINK_PENDING_REQUEST_TTL'))
    define('FEDERATION_FIRST_LINK_PENDING_REQUEST_TTL', 300);

/** Return true only for the current anonymous browser session. */
function federation_first_link_pending_request_is_anonymous()
{
    return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
        && method_exists($_SESSION['wa_current_user'], 'logged_in')
        && !$_SESSION['wa_current_user']->logged_in();
}

/** Bind one numeric first-link request reference to this anonymous company session. */
function federation_first_link_pending_request_bind($company, $verifier_decision_id, $now_epoch=null)
{
    $company = (int)$company;
    $verifier_decision_id = (int)$verifier_decision_id;
    $now = $now_epoch === null ? time() : $now_epoch;
    if ($company < 0 || $verifier_decision_id <= 0 || !is_int($now) || $now <= 0
        || !federation_first_link_pending_request_is_anonymous()
        || !isset($_SESSION['wa_current_user']->company)
        || (int)$_SESSION['wa_current_user']->company !== $company)
        return false;
    $_SESSION[FEDERATION_FIRST_LINK_PENDING_REQUEST_KEY] = array(
        'company_id'=>$company,
        'verifier_decision_id'=>$verifier_decision_id,
        'bound_at_epoch'=>$now,
        'expires_at_epoch'=>$now + FEDERATION_FIRST_LINK_PENDING_REQUEST_TTL
    );
    return true;
}

/** Read the still-live session-only request reference without creating authority. */
function federation_first_link_pending_request($now_epoch=null)
{
    $now = $now_epoch === null ? time() : $now_epoch;
    if (!is_int($now) || $now <= 0 || !federation_first_link_pending_request_is_anonymous()
        || !isset($_SESSION[FEDERATION_FIRST_LINK_PENDING_REQUEST_KEY])
        || !is_array($_SESSION[FEDERATION_FIRST_LINK_PENDING_REQUEST_KEY]))
        return false;
    $request = $_SESSION[FEDERATION_FIRST_LINK_PENDING_REQUEST_KEY];
    if (!isset($request['company_id'], $request['verifier_decision_id'], $request['expires_at_epoch'])
        || (int)$request['company_id'] < 0 || (int)$request['verifier_decision_id'] <= 0
        || (int)$request['expires_at_epoch'] <= $now
        || !isset($_SESSION['wa_current_user']->company)
        || (int)$_SESSION['wa_current_user']->company !== (int)$request['company_id']) {
        unset($_SESSION[FEDERATION_FIRST_LINK_PENDING_REQUEST_KEY]);
        return false;
    }
    return array(
        'company_id'=>(int)$request['company_id'],
        'verifier_decision_id'=>(int)$request['verifier_decision_id'],
        'expires_at_epoch'=>(int)$request['expires_at_epoch']
    );
}

/** Clear any stale first-link request reference before a fresh login attempt or terminal result. */
function federation_first_link_pending_request_clear()
{
    unset($_SESSION[FEDERATION_FIRST_LINK_PENDING_REQUEST_KEY]);
    return true;
}

/** Return active local users/roles for explicit administrator selection only. */
function federation_first_link_administration_user_options()
{
    if (!defined('TB_PREF'))
        return false;
    $sql = "SELECT u.id,u.login_id,u.real_name FROM ".TB_PREF."users u "
        ."INNER JOIN ".TB_PREF."security_roles r ON r.id=u.role_id "
        ."WHERE u.inactive=0 AND r.inactive=0 ORDER BY u.login_id,u.id";
    $result = db_query($sql, 'could not read PAY-SEC-004 explicit first-link local-user options');
    if (!$result)
        return false;
    $options = array();
    while ($row = db_fetch_assoc($result)) {
        $id = (int)$row['id'];
        if ($id <= 0)
            continue;
        $login = (string)$row['login_id'];
        $real = trim((string)$row['real_name']);
        $options[$id] = $real === '' ? $login : $login.' - '.$real;
    }
    return $options;
}

/** Strictly normalize the two explicit administrator-selected identifiers. */
function federation_first_link_administration_request($verifier_decision_id, $user_id)
{
    if (is_array($verifier_decision_id) || is_array($user_id))
        return false;
    $decision = is_int($verifier_decision_id) ? (string)$verifier_decision_id : trim((string)$verifier_decision_id);
    $user = is_int($user_id) ? (string)$user_id : trim((string)$user_id);
    if ($decision === '' || $user === '' || strlen($decision) > 20 || strlen($user) > 10
        || preg_match('/^[1-9][0-9]*$/D', $decision) !== 1
        || preg_match('/^[1-9][0-9]*$/D', $user) !== 1)
        return false;
    return array('verifier_decision_id'=>(int)$decision, 'user_id'=>(int)$user);
}

/** Invoke only the accepted SA_USERS first-link command; no session continuation occurs here. */
function federation_first_link_administration_create($company, $verifier_decision_id, $user_id, $now_epoch=null)
{
    $request = federation_first_link_administration_request($verifier_decision_id, $user_id);
    if ($request === false)
        return false;
    if (!function_exists('federation_create_first_external_subject_link')) {
        $file = dirname(__FILE__).DIRECTORY_SEPARATOR.'federation_first_link.inc';
        if (!is_file($file))
            return false;
        require_once($file);
    }
    if (!function_exists('federation_create_first_external_subject_link'))
        return false;
    return federation_create_first_external_subject_link(
        (int)$company,
        (int)$request['verifier_decision_id'],
        (int)$request['user_id'],
        $now_epoch
    );
}
