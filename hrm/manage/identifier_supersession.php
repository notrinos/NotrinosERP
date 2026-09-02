<?php
/** HRM-FND-004 governed identifier supersession browser route. */
$identifier_supersession_raw_method=isset($_SERVER['REQUEST_METHOD'])?(string)$_SERVER['REQUEST_METHOD']:'';
$identifier_supersession_raw_query=isset($_SERVER['QUERY_STRING'])?(string)$_SERVER['QUERY_STRING']:'';
$identifier_supersession_script_name=isset($_SERVER['SCRIPT_NAME'])?(string)$_SERVER['SCRIPT_NAME']:'';
$page_security='SA_EMPLOYEE';
$path_to_root='../..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/hrm_security.inc');
include_once($path_to_root.'/hrm/includes/db/person_identifier_supersession_db.inc');
include_once($path_to_root.'/includes/federation_oidc_identifier_supersession_step_up.inc');
include_once($path_to_root.'/includes/hrm_identifier_supersession_route.inc');

hrm_identifier_supersession_route_security_headers();
$now=time();
$company=function_exists('user_company')?(int)user_company():-1;
$callback_path=hrm_identifier_supersession_route_callback_path($identifier_supersession_script_name);

/* OIDC callback is processed before any page output. The selected employee is recovered only from server session custody. */
if($identifier_supersession_raw_method==='GET' && isset($_GET['state'])){
    if(!session_transport_is_https()||!hrm_identifier_supersession_route_authenticated()||!hrm_identifier_supersession_route_ready()
        ||!hrm_identifier_supersession_route_consume_attempt($now)||$company<0||$callback_path===false){
        http_response_code(403);exit;
    }
    $request=hrm_identifier_supersession_route_parse_callback($identifier_supersession_raw_method,$identifier_supersession_raw_query);
    if(!is_array($request)){http_response_code(400);exit;}
    $pending=federation_oidc_identifier_supersession_step_up_pending($company,(string)$request['state'],$now);
    $employee_ref=is_array($pending)&&isset($pending['employee_ref'])?(string)$pending['employee_ref']:false;
    if($employee_ref===false){http_response_code(403);exit;}
    if($request['kind']==='error'){
        hrm_identifier_supersession_route_terminalize_error($company,(string)$request['state'],(string)$request['error']);
        federation_oidc_identifier_supersession_step_up_forget_pending((string)$request['state']);
        hrm_identifier_supersession_route_redirect_local($identifier_supersession_script_name,$employee_ref,'failed');exit;
    }
    $result=federation_oidc_complete_identifier_supersession_step_up($company,(string)$request['state'],(string)$request['code'],$callback_path);
    $status=is_array($result)&&isset($result['status'])?(string)$result['status']:'denied';
    if($status==='assured' && hrm_identifier_supersession_route_bind_assured($result,$now)){
        hrm_identifier_supersession_route_reset_attempts();
        hrm_identifier_supersession_route_redirect_local($identifier_supersession_script_name,$employee_ref,'assured');exit;
    }
    if($status==='retry_later') federation_oidc_identifier_supersession_step_up_forget_pending((string)$request['state']);
    hrm_identifier_supersession_route_redirect_local($identifier_supersession_script_name,$employee_ref,$status==='retry_later'?'retry':'failed');exit;
}

$employee_ref=hrm_identifier_supersession_route_employee_ref(
    isset($_POST['employee_id'])?$_POST['employee_id']:(isset($_GET['employee_id'])?$_GET['employee_id']:'')
);
if(!hrm_identifier_supersession_route_authenticated()||$employee_ref===false){
    page(_($help_context='Governed Identifier Change'));display_error(_('Restricted identity verification authority is required.'));end_page();exit;
}
if(!hrm_identifier_supersession_route_ready()){
    page(_($help_context='Governed Identifier Change'));display_error(_('Software Upgrade must be completed before governed identifier changes are available.'));end_page();exit;
}

$action=isset($_POST['route_action'])?(string)$_POST['route_action']:'';
$generic_error='';
if($identifier_supersession_raw_method==='POST' && $action==='start_step_up'){
    if(!check_csrf_token()||!session_transport_is_https()||!hrm_identifier_supersession_route_consume_attempt($now)){
        $generic_error=_('The governed change could not be prepared.');
    } else {
        $candidate=isset($_POST['identifier_candidate'])?(string)$_POST['identifier_candidate']:'';
        $parts=explode(':',$candidate,2);
        $identifier_id=count($parts)===2?hrm_identifier_verification_request_positive_int($parts[0]):false;
        $row_version=count($parts)===2?hrm_identifier_verification_request_positive_int($parts[1]):false;
        $new_value=isset($_POST['new_identifier_value'])?(string)$_POST['new_identifier_value']:'';
        $jurisdiction=isset($_POST['issuing_jurisdiction'])?(string)$_POST['issuing_jurisdiction']:'';
        $valid_to=isset($_POST['valid_to'])?(string)$_POST['valid_to']:'';
        $prepared=($identifier_id===false||$row_version===false)?false:hrm_identifier_supersession_prepare_browser_action(
            $employee_ref,$identifier_id,$row_version,$new_value,$jurisdiction,$valid_to);
        /* $new_value is deliberately never copied into session, URL, audit output, or an error string. */
        $step_up_csrf=is_array($prepared)?federation_oidc_identifier_supersession_step_up_issue_csrf($prepared,$now):false;
        $result=$step_up_csrf===false?false:federation_oidc_start_identifier_supersession_step_up(
            $company,$employee_ref,(int)$prepared['identifier_id'],(int)$prepared['identifier_row_version'],
            (string)$prepared['action_hash'],$step_up_csrf,$callback_path,FEDERATION_OIDC_IDENTIFIER_SUPERSESSION_STEP_UP_TRANSACTION_TTL,$now);
        if(is_array($result)&&($result['status']??'')==='authorization_required'&&isset($result['authorization_url'])
            &&hrm_identifier_supersession_route_redirect_provider((string)$result['authorization_url']))exit;
        $generic_error=is_array($result)&&($result['status']??'')==='retry_later'
            ?_('Reauthentication is temporarily unavailable. No identifier change was made.')
            :_('The governed change could not be prepared. No identifier change was made.');
        unset($new_value,$prepared,$step_up_csrf,$result);
    }
}

$assured=hrm_identifier_supersession_route_assured($employee_ref,$now);
if($identifier_supersession_raw_method==='POST' && $action==='confirm_supersession'){
    if(!check_csrf_token()||!is_array($assured)
        ||!hrm_identifier_supersession_route_consume_confirm_csrf($assured,isset($_POST['confirm_csrf'])?(string)$_POST['confirm_csrf']:'',$now)){
        $generic_error=_('The governed change confirmation was rejected.');
    } else {
        $new_value=isset($_POST['new_identifier_value'])?(string)$_POST['new_identifier_value']:'';
        $jurisdiction=isset($_POST['issuing_jurisdiction'])?(string)$_POST['issuing_jurisdiction']:'';
        $valid_to=isset($_POST['valid_to'])?(string)$_POST['valid_to']:'';
        $prepared=hrm_identifier_supersession_prepare_browser_action($employee_ref,(int)$assured['identifier_id'],
            (int)$assured['identifier_row_version'],$new_value,$jurisdiction,$valid_to,true);
        if(!is_array($prepared)||!hash_equals((string)$assured['action_hash'],(string)$prepared['action_hash'])){
            $generic_error=_('The entered details do not match the action that was reauthenticated. Re-enter the exact prepared details.');
        } else {
            $result=supersede_hrm_person_identifier((int)$assured['identifier_id'],(int)$assured['identifier_row_version'],
                $new_value,$jurisdiction,$valid_to,$now);
            if(is_array($result)&&hrm_identifier_supersession_route_set_flash($employee_ref,$result)){
                hrm_identifier_supersession_route_clear_assured();
                hrm_identifier_supersession_route_redirect_local($identifier_supersession_script_name,$employee_ref,'success');exit;
            }
            $generic_error=_('The governed identifier change was not completed. No partial identifier change was retained.');
        }
        unset($new_value,$prepared,$result);
    }
    $assured=hrm_identifier_supersession_route_assured($employee_ref,$now);
}

page(_($help_context='Governed Identifier Change'));
echo '<div class="center" style="max-width:760px;margin:0 auto;">';
echo '<p>'._('This route replaces one current verified and independently approved identifier with a new unverified successor. It never reveals the stored identifier value.').'</p>';
if($generic_error!=='')display_error($generic_error);
$flash=hrm_identifier_supersession_route_take_flash($employee_ref);
if(is_array($flash)){
    display_notification(_('Governed identifier successor created. The successor remains unverified and unapproved.'));
    echo '<p>'._('Continue with the existing identifier verification request, independent approval, opaque evidence registration, and verification-promotion workflow before the successor can be treated as verified.').'</p>';
}
if(isset($_GET['step_up'])&&$_GET['step_up']==='failed')display_error(_('Reauthentication failed or was cancelled. No identifier change was made.'));
if(isset($_GET['step_up'])&&$_GET['step_up']==='retry')display_warning(_('Reauthentication must be restarted. No identifier change was made.'));

if(is_array($assured)){
    echo '<h3>'._('Confirm the reauthenticated change').'</h3>';
    echo '<p>'._('For privacy, the proposed identifier was not stored during reauthentication. Re-enter the exact same identifier, issuing jurisdiction, and valid-to date to consume the single-use assurance.').'</p>';
    $confirm_csrf=hrm_identifier_supersession_route_issue_confirm_csrf($assured,$now);
    start_form(false,$identifier_supersession_script_name);
    hidden('_token',ensure_csrf_token());hidden('route_action','confirm_supersession');hidden('employee_id',$employee_ref);hidden('confirm_csrf',$confirm_csrf);
    start_table(TABLESTYLE2);
    label_row(_('Predecessor identifier:'),sprintf(_('#%d, row version %d'),(int)$assured['identifier_id'],(int)$assured['identifier_row_version']));
    echo '<tr><td class="label">'._('New identifier:').'</td><td><input type="password" name="new_identifier_value" value="" maxlength="255" autocomplete="new-password" required></td></tr>';
    text_row(_('Issuing jurisdiction:'),'issuing_jurisdiction','',16,16);
    echo '<tr><td class="label">'._('Valid to (YYYY-MM-DD, optional):').'</td><td><input type="text" name="valid_to" value="" maxlength="10" autocomplete="off"></td></tr>';
    end_table(1);submit_center('confirm',_('Create Unverified Successor'));end_form();
} else {
    $candidates=get_hrm_identifier_supersession_candidates_masked($employee_ref);
    echo '<h3>'._('Prepare governed identifier change').'</h3>';
    if(!is_array($candidates)||count($candidates)===0){
        display_warning(_('No current verified and independently approved identifier is eligible for governed supersession.'));
    } else {
        start_form(false,$identifier_supersession_script_name);
        hidden('_token',ensure_csrf_token());hidden('route_action','start_step_up');hidden('employee_id',$employee_ref);
        start_table(TABLESTYLE2);
        echo '<tr><td class="label">'._('Current identifier:').'</td><td><select name="identifier_candidate">';
        foreach($candidates as $candidate){
            $value=(int)$candidate['identifier_id'].':'.(int)$candidate['row_version'];
            $label=(string)$candidate['identifier_type'].' — '.(string)$candidate['masked_value'].' — '.(string)$candidate['issuing_jurisdiction'];
            echo '<option value="'.htmlspecialchars($value,ENT_QUOTES,'UTF-8').'">'.htmlspecialchars($label,ENT_QUOTES,'UTF-8').'</option>';
        }
        echo '</select></td></tr>';
        echo '<tr><td class="label">'._('New identifier:').'</td><td><input type="password" name="new_identifier_value" value="" maxlength="255" autocomplete="new-password" required></td></tr>';
        text_row(_('Issuing jurisdiction:'),'issuing_jurisdiction','',16,16);
        echo '<tr><td class="label">'._('Valid to (YYYY-MM-DD, optional):').'</td><td><input type="text" name="valid_to" value="" maxlength="10" autocomplete="off"></td></tr>';
        end_table(1);submit_center('reauthenticate',_('Reauthenticate for Governed Change'));end_form();
    }
}
echo '<p><a href="employees.php?employee_id='.rawurlencode($employee_ref).'">'._('Back to Employee Maintenance').'</a></p></div>';
end_page();
