<?php
/** HRM-ESS-001 current-user bounded self-profile request/cancel route. */
$ess_self_raw_method=isset($_SERVER['REQUEST_METHOD'])?(string)$_SERVER['REQUEST_METHOD']:'';
$ess_self_raw_query=isset($_SERVER['QUERY_STRING'])?(string)$_SERVER['QUERY_STRING']:'';
$ess_self_script=isset($_SERVER['SCRIPT_NAME'])?(string)$_SERVER['SCRIPT_NAME']:'';
$page_security='SA_HRM_ESS_REQUEST_SELF_PROFILE_CHANGE';
$path_to_root='../..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/ess_self_change_request_db.inc');
include_once($path_to_root.'/hrm/includes/db/ess_self_change_recovery_db.inc');
include_once($path_to_root.'/includes/hrm_ess_self_change_route.inc');

hrm_ess_self_change_route_security_headers();
$now=time();$company=function_exists('user_company')?(int)user_company():-1;
$suffix='/hrm/ess/self_profile_change.php';$callback_path=hrm_ess_self_change_route_callback_path($ess_self_script,$suffix);
$secure=function_exists('session_transport_is_https')&&session_transport_is_https();
$authorized=hrm_ess_self_change_route_authenticated();
$ready=hrm_ess_self_change_route_ready_for('1.0.453');
$message='';$ok_message='';

/* Federated callback: exact state/code or state/error only; target principal is never accepted. */
if($ess_self_raw_method==='GET'&&isset($_GET['state'])){
    if(!$secure||!$authorized||!$ready||!hrm_ess_self_change_route_consume_attempt($now)||$company<0||$callback_path===false){http_response_code(403);exit;}
    $request=hrm_ess_self_change_route_parse_callback($ess_self_raw_method,$ess_self_raw_query);if(!is_array($request)){http_response_code(400);exit;}
    if($request['kind']==='error'){
        hrm_ess_self_change_route_terminalize_error($company,(string)$request['state'],(string)$request['error']);
        hrm_ess_self_change_reauth_forget_pending((string)$request['state']);
        hrm_ess_self_change_route_redirect_local($ess_self_script,$suffix,'failed');exit;
    }
    $result=hrm_ess_self_change_reauth_complete_oidc($company,(string)$request['state'],(string)$request['code'],$callback_path);
    $status=is_array($result)&&isset($result['status'])?(string)$result['status']:'denied';
    if($status==='assured'&&hrm_ess_self_change_route_bind_assured($result,$now)){
        hrm_ess_self_change_route_reset_attempts();hrm_ess_self_change_route_redirect_local($ess_self_script,$suffix,'assured');exit;
    }
    if($status==='retry_later')hrm_ess_self_change_reauth_forget_pending((string)$request['state']);
    hrm_ess_self_change_route_redirect_local($ess_self_script,$suffix,$status==='retry_later'?'retry':'failed');exit;
}

function ess_self_collect_changes()
{
    $changes=array();foreach(hrm_ess_self_change_allowed_fields() as $field)
        if(array_key_exists($field,$_POST)&&is_scalar($_POST[$field]))$changes[$field]=(string)$_POST[$field];
    return $changes;
}
function ess_self_valid_request_id($value)
{
    return is_scalar($value)&&preg_match('/^[1-9][0-9]{0,18}$/D',(string)$value)===1?(int)$value:0;
}

$action=isset($_POST['route_action'])?(string)$_POST['route_action']:'';
if($ess_self_raw_method==='POST'){
    if(!$authorized||!$ready||!$secure||!check_csrf_token()||!hrm_ess_self_change_route_consume_attempt($now)){
        $message=_('The self-profile change request was rejected. No profile state was changed.');
    } elseif(!hrm_ess_self_change_route_supported_auth_method()) {
        $message=_('This authenticated session does not support the required reauthentication method. No profile state was changed.');
    } elseif($action==='start_request_step_up') {
        $changes=ess_self_collect_changes();$intent=hrm_ess_self_change_reauth_intent_digest($changes);
        $prepared=$intent===false?false:hrm_ess_self_change_reauth_action_context('request',0,$intent);
        $step_csrf=is_array($prepared)?hrm_ess_self_change_reauth_issue_csrf($prepared,$now):false;
        $result=$step_csrf===false?false:hrm_ess_self_change_reauth_start_oidc($prepared,$step_csrf,$callback_path,HRM_ESS_SELF_CHANGE_REAUTH_TRANSACTION_TTL,$now);
        if(is_array($result)&&($result['status']??'')==='authorization_required'&&isset($result['authorization_url'])
            &&hrm_ess_self_change_route_redirect_provider((string)$result['authorization_url']))exit;
        $message=is_array($result)&&($result['status']??'')==='retry_later'
            ?_('Independent reauthentication is temporarily unavailable. No request was submitted.')
            :_('Independent reauthentication could not be started. No request was submitted.');
        unset($changes,$intent,$prepared,$step_csrf,$result);
    } elseif($action==='start_cancel_step_up') {
        $request_id=ess_self_valid_request_id($_POST['request_id']??'');$prepared=$request_id>0?hrm_ess_self_change_reauth_action_context('cancel',$request_id,''):false;
        $step_csrf=is_array($prepared)?hrm_ess_self_change_reauth_issue_csrf($prepared,$now):false;
        $result=$step_csrf===false?false:hrm_ess_self_change_reauth_start_oidc($prepared,$step_csrf,$callback_path,HRM_ESS_SELF_CHANGE_REAUTH_TRANSACTION_TTL,$now);
        if(is_array($result)&&($result['status']??'')==='authorization_required'&&isset($result['authorization_url'])
            &&hrm_ess_self_change_route_redirect_provider((string)$result['authorization_url']))exit;
        $message=is_array($result)&&($result['status']??'')==='retry_later'
            ?_('Independent reauthentication is temporarily unavailable. No request was cancelled.')
            :_('Independent reauthentication could not be started. No request was cancelled.');
        unset($prepared,$step_csrf,$result);
    } elseif($action==='request_change') {
        $changes=ess_self_collect_changes();$intent=hrm_ess_self_change_reauth_intent_digest($changes);$prepared=$intent===false?false:hrm_ess_self_change_reauth_action_context('request',0,$intent);
        $password=isset($_POST['current_password'])?(string)$_POST['current_password']:'';
        /* Historical 1.0.450 local boundary remains present while 1.0.453 adds federated assurance. */
        if(!is_array($prepared)||!hrm_ess_self_change_route_reauthenticate_local($password))$request_id=false;
        else{$nonce=hrm_ess_self_change_route_operation_nonce((string)$prepared['action_hash']);$request_id=$nonce===false?false:request_hrm_ess_self_profile_change($changes,$nonce);}
        if($request_id===false)$message=_('The bounded self-profile change request was not created. No profile state was changed.');
        else{hrm_ess_self_change_route_operation_nonce((string)$prepared['action_hash'],true);hrm_ess_self_change_route_reset_attempts();$ok_message=sprintf(_('Self-profile change request #%d was submitted for independent approval.'),(int)$request_id);}
        unset($password,$nonce,$changes,$intent,$prepared);
    } elseif($action==='cancel_request') {
        $request_id=ess_self_valid_request_id($_POST['request_id']??'');$password=isset($_POST['current_password'])?(string)$_POST['current_password']:'';
        if($request_id<=0||!hrm_ess_self_change_route_reauthenticate_local($password)||!cancel_hrm_ess_self_profile_change($request_id,'owner_withdrawal'))
            $message=_('The self-profile change request could not be cancelled. No profile state was changed.');
        else{hrm_ess_self_change_route_reset_attempts();$ok_message=_('The self-profile change request was cancelled.');}
        unset($password);
    } elseif($action==='confirm_request') {
        $assured=hrm_ess_self_change_route_assured('request',$now);$changes=ess_self_collect_changes();$intent=hrm_ess_self_change_reauth_intent_digest($changes);
        $prepared=$intent===false?false:hrm_ess_self_change_reauth_action_context('request',0,$intent);
        $confirm=isset($_POST['confirm_csrf'])?(string)$_POST['confirm_csrf']:'';
        if(!is_array($assured)||!is_array($prepared)||!hash_equals((string)$assured['action_hash'],(string)$prepared['action_hash'])
            ||!hrm_ess_self_change_route_consume_confirm_csrf($assured,$confirm,$now))$request_id=false;
        else{
            $nonce=hrm_ess_self_change_route_operation_nonce((string)$prepared['action_hash']);
            $request_id=$nonce===false?false:hrm_ess_self_change_route_run_governed($prepared,'',function()use($changes,$nonce){return request_hrm_ess_self_profile_change($changes,$nonce);});
        }
        if($request_id===false)$message=_('The reauthenticated request was not submitted. Reauthenticate again; no partial profile state was retained.');
        else{hrm_ess_self_change_route_operation_nonce((string)$prepared['action_hash'],true);hrm_ess_self_change_route_reset_attempts();$ok_message=sprintf(_('Self-profile change request #%d was submitted for independent approval.'),(int)$request_id);}
        unset($assured,$changes,$intent,$prepared,$confirm,$nonce);
    } elseif($action==='confirm_cancel') {
        $assured=hrm_ess_self_change_route_assured('cancel',$now);$confirm=isset($_POST['confirm_csrf'])?(string)$_POST['confirm_csrf']:'';
        $prepared=is_array($assured)?hrm_ess_self_change_reauth_action_context('cancel',(int)$assured['request_id'],''):false;
        $ok=is_array($assured)&&is_array($prepared)&&hash_equals((string)$assured['action_hash'],(string)$prepared['action_hash'])
            &&hrm_ess_self_change_route_consume_confirm_csrf($assured,$confirm,$now)
            &&hrm_ess_self_change_route_run_governed($prepared,'',function()use($prepared){return cancel_hrm_ess_self_profile_change((int)$prepared['request_id'],'owner_withdrawal');})!==false;
        if(!$ok)$message=_('The reauthenticated cancellation was not completed. No profile state was changed.');
        else{hrm_ess_self_change_route_reset_attempts();$ok_message=_('The self-profile change request was cancelled.');}
        unset($assured,$confirm,$prepared);
    } else $message=_('The requested self-profile operation is not allowed.');
}

page(_($help_context='My Profile Change'));
echo '<main class="center" style="max-width:780px;margin:0 auto;text-align:left" aria-labelledby="ess-self-title">';
if(!$authorized){display_error(_('Your current account is not authorized for bounded self-profile change requests.'));echo '</main>';end_page();exit;}
if(!$ready){display_error(_('Software Upgrade must be completed before this self-service route is available.'));echo '</main>';end_page();exit;}
if(!$secure){display_error(_('This self-service route requires HTTPS. No profile change can be requested over this connection.'));echo '</main>';end_page();exit;}
if(!hrm_ess_self_change_route_supported_auth_method()){display_error(_('This session must use either local-password or approved federated OIDC authentication before this route can be used.'));echo '</main>';end_page();exit;}
if($message!=='')display_error($message);if($ok_message!=='')display_notification($ok_message);
if(isset($_GET['step_up'])&&$_GET['step_up']==='failed')display_error(_('Independent reauthentication failed or was cancelled. No profile state was changed.'));
if(isset($_GET['step_up'])&&$_GET['step_up']==='retry')display_warning(_('Independent reauthentication must be restarted. No profile state was changed.'));

echo '<h2 id="ess-self-title">'._('My Profile Change').'</h2>';
echo '<p>'._('Request changes only to your own bounded contact/private profile. The request does not change your profile until an independent checker approves it and a distinct governed executor applies it. Bank, tax, identifier and compensation data are not available here.').'</p>';
$auth_method=hrm_ess_self_change_route_auth_method();$assured=hrm_ess_self_change_route_assured(null,$now);
$open=hrm_ess_self_change_route_open_request();
if($open===false){display_error(_('Current self-profile request custody could not be resolved safely.'));}
elseif(is_array($assured)&&$assured['operation']==='cancel'){
    echo '<section aria-labelledby="ess-confirm-cancel"><h3 id="ess-confirm-cancel">'._('Confirm reauthenticated cancellation').'</h3><p>'._('The cancellation target is bound to your authenticated session and will be revalidated before any state transition.').'</p>';
    $confirm=hrm_ess_self_change_route_issue_confirm_csrf($assured,$now);start_form();hidden('_token',ensure_csrf_token());hidden('route_action','confirm_cancel');hidden('confirm_csrf',$confirm);submit_center('confirm_cancel',_('Confirm Cancellation'));end_form();echo '</section>';
}elseif(is_array($open)){
    echo '<section aria-labelledby="ess-open-request"><h3 id="ess-open-request">'.sprintf(_('Open request #%d'),(int)$open['request_id']).'</h3><p>'.htmlspecialchars((string)$open['request_status'],ENT_QUOTES,'UTF-8').'</p>';
    echo '<p>'._('Withdrawal is reauthenticated and ownership is revalidated server-side; the request identifier alone grants no authority.').'</p>';
    start_form();hidden('_token',ensure_csrf_token());hidden('request_id',(int)$open['request_id']);
    if($auth_method==='local_password'){
        hidden('route_action','cancel_request');echo '<label for="ess-current-password-cancel">'._('Current password:').'</label> <input id="ess-current-password-cancel" type="password" name="current_password" maxlength="512" autocomplete="current-password" required>';
        submit_center('cancel_self_request',_('Cancel My Request'));
    }else{hidden('route_action','start_cancel_step_up');submit_center('reauth_cancel_self_request',_('Reauthenticate to Cancel My Request'));}
    end_form();echo '</section>';
}else{
    $values=hrm_ess_self_change_route_current_values();if(!is_array($values))display_error(_('Your current self-profile projection could not be resolved safely.'));
    else{
        $labels=array('address'=>_('Address'),'city'=>_('City'),'state'=>_('State/Province'),'country'=>_('Country'),'phone'=>_('Phone'),'mobile'=>_('Mobile'),'email'=>_('Work email'),'personal_email'=>_('Personal email'),'emergency_name'=>_('Emergency contact name'),'emergency_relation'=>_('Emergency contact relation'),'emergency_phone'=>_('Emergency contact phone'));
        $confirming=is_array($assured)&&$assured['operation']==='request';
        echo '<section aria-labelledby="ess-change-form"><h3 id="ess-change-form">'.($confirming?_('Confirm reauthenticated request'):_('Prepare bounded profile request')).'</h3>';
        if($confirming)echo '<p id="ess-confirm-help">'._('Re-enter the exact requested values used before federated reauthentication. The action digest must match before submission.').'</p>';
        start_form();hidden('_token',ensure_csrf_token());
        if($confirming){hidden('route_action','confirm_request');hidden('confirm_csrf',hrm_ess_self_change_route_issue_confirm_csrf($assured,$now));}
        elseif($auth_method==='local_password')hidden('route_action','request_change');else hidden('route_action','start_request_step_up');
        echo '<table class="tablestyle2" role="presentation">';
        foreach(hrm_ess_self_change_allowed_fields() as $field){$maxlength=$field==='address'?255:100;$id='ess-field-'.$field;
            echo '<tr><td class="label"><label for="'.htmlspecialchars($id,ENT_QUOTES,'UTF-8').'">'.htmlspecialchars($labels[$field],ENT_QUOTES,'UTF-8').':</label></td><td><input id="'.htmlspecialchars($id,ENT_QUOTES,'UTF-8').'" style="width:min(100%,420px)" type="text" name="'.htmlspecialchars($field,ENT_QUOTES,'UTF-8').'" value="'.htmlspecialchars((string)$values[$field],ENT_QUOTES,'UTF-8').'" maxlength="'.$maxlength.'" autocomplete="off"></td></tr>';}
        if(!$confirming&&$auth_method==='local_password')echo '<tr><td class="label"><label for="ess-current-password-request">'._('Current password:').'</label></td><td><input id="ess-current-password-request" type="password" name="current_password" maxlength="512" autocomplete="current-password" required></td></tr>';
        echo '</table>';
        if($confirming)submit_center('confirm_self_change',_('Confirm My Change Request'));
        elseif($auth_method==='local_password')submit_center('request_self_change',_('Submit My Change Request'));
        else submit_center('reauth_self_change',_('Reauthenticate and Continue'));
        end_form();echo '<p aria-live="polite">'._('Requested values remain pending until independent approval and execution.').'</p></section>';
    }
}
echo '</main>';end_page();
