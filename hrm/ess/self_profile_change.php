<?php
/** HRM-ESS-001 current-user bounded self-profile request/cancel route. */
$page_security='SA_HRM_ESS_REQUEST_SELF_PROFILE_CHANGE';
$path_to_root='../..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/ess_self_change_request_db.inc');
include_once($path_to_root.'/hrm/includes/db/ess_self_change_recovery_db.inc');
include_once($path_to_root.'/includes/hrm_ess_self_change_route.inc');

hrm_ess_self_change_route_security_headers();
$method=isset($_SERVER['REQUEST_METHOD'])?(string)$_SERVER['REQUEST_METHOD']:'GET';
$secure=function_exists('session_transport_is_https')&&session_transport_is_https();
$authorized=hrm_ess_self_change_route_authenticated();
$ready=hrm_ess_self_change_route_ready();
$message=''; $ok_message='';

if ($method==='POST') {
    $action=isset($_POST['route_action'])?(string)$_POST['route_action']:'';
    if (!$authorized || !$ready || !$secure || !check_csrf_token() || !hrm_ess_self_change_route_consume_attempt()) {
        $message=_('The self-profile change request was rejected. No profile state was changed.');
    } else {
        $password=isset($_POST['current_password'])?(string)$_POST['current_password']:'';
        if (!hrm_ess_self_change_route_reauthenticate_local($password)) {
            $message=_('Reauthentication failed. Only an active local-password session may use this route. No profile state was changed.');
        } elseif ($action==='request_change') {
            $changes=array();
            foreach (hrm_ess_self_change_allowed_fields() as $field)
                if (array_key_exists($field,$_POST) && is_scalar($_POST[$field])) $changes[$field]=(string)$_POST[$field];
            $nonce=hrm_ess_self_change_route_nonce();
            $request_id=$nonce===false?false:request_hrm_ess_self_profile_change($changes,$nonce);
            if ($request_id===false) $message=_('The bounded self-profile change request was not created. No profile state was changed.');
            else { hrm_ess_self_change_route_reset_attempts(); $ok_message=sprintf(_('Self-profile change request #%d was submitted for independent approval.'),(int)$request_id); }
        } elseif ($action==='cancel_request') {
            $request_id=isset($_POST['request_id'])&&is_scalar($_POST['request_id'])&&preg_match('/^[1-9][0-9]{0,18}$/D',(string)$_POST['request_id'])
                ?(int)$_POST['request_id']:0;
            if ($request_id<=0 || !cancel_hrm_ess_self_profile_change($request_id,'owner_withdrawal'))
                $message=_('The self-profile change request could not be cancelled. No profile state was changed.');
            else { hrm_ess_self_change_route_reset_attempts(); $ok_message=_('The self-profile change request was cancelled.'); }
        } else {
            $message=_('The requested self-profile operation is not allowed.');
        }
        unset($password,$nonce,$changes);
    }
}

page(_($help_context='My Profile Change'));
echo '<div class="center" style="max-width:780px;margin:0 auto;text-align:left">';
if (!$authorized) {
    display_error(_('Your current account is not authorized for bounded self-profile change requests.'));
    echo '</div>';end_page();exit;
}
if (!$ready) {
    display_error(_('Software Upgrade must be completed before this self-service route is available.'));
    echo '</div>';end_page();exit;
}
if (!$secure) {
    display_error(_('This self-service route requires HTTPS. No profile change can be requested over this connection.'));
    echo '</div>';end_page();exit;
}
if ($message!=='') display_error($message);
if ($ok_message!=='') display_notification($ok_message);

echo '<h2>'._('My Profile Change').'</h2>';
echo '<p>'._('Request changes only to your own bounded contact/private profile. The request does not change your profile until an independent checker approves it and the separate governed executor applies it. Bank, tax, identifier and compensation data are not available here.').'</p>';
$open=hrm_ess_self_change_route_open_request();
if ($open===false) {
    display_error(_('Current self-profile request custody could not be resolved safely.'));
} elseif (is_array($open)) {
    echo '<p><strong>'.sprintf(_('Open request #%d'),(int)$open['request_id']).'</strong> — '.htmlspecialchars((string)$open['request_status'],ENT_QUOTES,'UTF-8').'</p>';
    echo '<p>'._('Submit your current password to withdraw this still-unapplied request. Ownership is revalidated server-side; the request identifier alone grants no authority.').'</p>';
    start_form(); hidden('_token',ensure_csrf_token()); hidden('route_action','cancel_request'); hidden('request_id',(int)$open['request_id']);
    start_table(TABLESTYLE2);
    echo '<tr><td class="label">'._('Current password:').'</td><td><input type="password" name="current_password" maxlength="512" autocomplete="current-password" required></td></tr>';
    end_table(1); submit_center('cancel_self_request',_('Cancel My Request')); end_form();
} else {
    $values=hrm_ess_self_change_route_current_values();
    if (!is_array($values)) {
        display_error(_('Your current self-profile projection could not be resolved safely.'));
    } else {
        $labels=array('address'=>_('Address'),'city'=>_('City'),'state'=>_('State/Province'),'country'=>_('Country'),
            'phone'=>_('Phone'),'mobile'=>_('Mobile'),'email'=>_('Work email'),'personal_email'=>_('Personal email'),
            'emergency_name'=>_('Emergency contact name'),'emergency_relation'=>_('Emergency contact relation'),'emergency_phone'=>_('Emergency contact phone'));
        start_form(); hidden('_token',ensure_csrf_token()); hidden('route_action','request_change'); start_table(TABLESTYLE2);
        foreach (hrm_ess_self_change_allowed_fields() as $field) {
            $maxlength=in_array($field,array('address'),true)?255:(in_array($field,array('emergency_name'),true)?100:100);
            echo '<tr><td class="label">'.htmlspecialchars($labels[$field],ENT_QUOTES,'UTF-8').':</td><td><input style="width:min(100%,420px)" type="text" name="'.htmlspecialchars($field,ENT_QUOTES,'UTF-8').'" value="'.htmlspecialchars((string)$values[$field],ENT_QUOTES,'UTF-8').'" maxlength="'.$maxlength.'"></td></tr>';
        }
        echo '<tr><td class="label">'._('Current password:').'</td><td><input type="password" name="current_password" maxlength="512" autocomplete="current-password" required></td></tr>';
        end_table(1); submit_center('request_self_change',_('Submit My Change Request')); end_form();
    }
}
echo '</div>'; end_page();
