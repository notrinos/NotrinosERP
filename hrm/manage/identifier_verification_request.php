<?php
/** HRM-FND-004 governed superseded-identifier verification-request browser continuation. */
$identifier_verification_request_raw_method=isset($_SERVER['REQUEST_METHOD'])?(string)$_SERVER['REQUEST_METHOD']:'';
$identifier_verification_request_raw_query=isset($_SERVER['QUERY_STRING'])?(string)$_SERVER['QUERY_STRING']:'';
$identifier_verification_request_script_name=isset($_SERVER['SCRIPT_NAME'])?(string)$_SERVER['SCRIPT_NAME']:'';
$page_security='SA_EMPLOYEE';
$path_to_root='../..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/hrm_security.inc');
include_once($path_to_root.'/hrm/includes/db/person_identifier_verification_request_db.inc');
include_once($path_to_root.'/hrm/includes/db/person_identifier_verification_request_browser_db.inc');
include_once($path_to_root.'/includes/federation_oidc_identifier_verification_request_step_up.inc');
include_once($path_to_root.'/includes/hrm_identifier_verification_request_route.inc');

hrm_identifier_verification_request_route_security_headers();
$now=time();
$company=function_exists('user_company')?(int)user_company():-1;
$callback_path=hrm_identifier_verification_request_route_callback_path($identifier_verification_request_script_name);

/* OIDC callback accepts only exact state/code or state/error. Employee and method remain server/user-bound. */
if($identifier_verification_request_raw_method==='GET' && isset($_GET['state'])){
    if(!session_transport_is_https()||!hrm_identifier_verification_request_route_authenticated()||!hrm_identifier_verification_request_route_ready()
        ||!hrm_identifier_verification_request_route_consume_attempt($now)||$company<0||$callback_path===false){
        http_response_code(403);exit;
    }
    $request=hrm_identifier_verification_request_route_parse_callback($identifier_verification_request_raw_method,$identifier_verification_request_raw_query);
    if(!is_array($request)){http_response_code(400);exit;}
    $pending=federation_oidc_identifier_verification_request_step_up_pending($company,(string)$request['state'],$now);
    $employee_ref=is_array($pending)&&isset($pending['employee_ref'])?(string)$pending['employee_ref']:false;
    if($employee_ref===false){http_response_code(403);exit;}
    if($request['kind']==='error'){
        hrm_identifier_verification_request_route_terminalize_error($company,(string)$request['state'],(string)$request['error']);
        federation_oidc_identifier_verification_request_step_up_forget_pending((string)$request['state']);
        hrm_identifier_verification_request_route_redirect_local($identifier_verification_request_script_name,$employee_ref,'failed');exit;
    }
    $result=federation_oidc_complete_identifier_verification_request_step_up($company,(string)$request['state'],(string)$request['code'],$callback_path);
    $status=is_array($result)&&isset($result['status'])?(string)$result['status']:'denied';
    if($status==='assured' && hrm_identifier_verification_request_route_bind_assured($result,$now)){
        hrm_identifier_verification_request_route_reset_attempts();
        hrm_identifier_verification_request_route_redirect_local($identifier_verification_request_script_name,$employee_ref,'assured');exit;
    }
    if($status==='retry_later') federation_oidc_identifier_verification_request_step_up_forget_pending((string)$request['state']);
    hrm_identifier_verification_request_route_redirect_local($identifier_verification_request_script_name,$employee_ref,$status==='retry_later'?'retry':'failed');exit;
}

$employee_ref=hrm_identifier_verification_request_route_employee_ref(
    isset($_POST['employee_id'])?$_POST['employee_id']:(isset($_GET['employee_id'])?$_GET['employee_id']:'')
);
if(!hrm_identifier_verification_request_route_authenticated()||$employee_ref===false){
    page(_($help_context='Identifier Verification Request'));display_error(_('Restricted identity verification authority is required.'));end_page();exit;
}
if(!hrm_identifier_verification_request_route_ready()){
    page(_($help_context='Identifier Verification Request'));display_error(_('Software Upgrade must be completed before governed identifier verification requests are available.'));end_page();exit;
}

$action=isset($_POST['route_action'])?(string)$_POST['route_action']:'';
$generic_error='';
if($identifier_verification_request_raw_method==='POST' && $action==='start_step_up'){
    if(!check_csrf_token()||!session_transport_is_https()||!hrm_identifier_verification_request_route_consume_attempt($now)){
        $generic_error=_('The verification request could not be prepared.');
    } else {
        $candidate=isset($_POST['identifier_candidate'])?(string)$_POST['identifier_candidate']:'';
        $parts=explode(':',$candidate,2);
        $identifier_id=count($parts)===2?hrm_identifier_verification_request_positive_int($parts[0]):false;
        $row_version=count($parts)===2?hrm_identifier_verification_request_positive_int($parts[1]):false;
        $verification_method=isset($_POST['verification_method'])?(string)$_POST['verification_method']:'';
        $prepared=($identifier_id===false||$row_version===false)?false:hrm_identifier_verification_request_prepare_browser_action(
            $employee_ref,$identifier_id,$row_version,$verification_method);
        $step_up_csrf=is_array($prepared)?federation_oidc_identifier_verification_request_step_up_issue_csrf($prepared,$now):false;
        $result=$step_up_csrf===false?false:federation_oidc_start_identifier_verification_request_step_up(
            $company,$employee_ref,(int)$prepared['identifier_id'],(int)$prepared['identifier_row_version'],
            (string)$prepared['action_hash'],$step_up_csrf,$callback_path,FEDERATION_OIDC_IDENTIFIER_VERIFICATION_REQUEST_STEP_UP_TRANSACTION_TTL,$now);
        if(is_array($result)&&($result['status']??'')==='authorization_required'&&isset($result['authorization_url'])
            &&hrm_identifier_verification_request_route_redirect_provider((string)$result['authorization_url']))exit;
        $generic_error=is_array($result)&&($result['status']??'')==='retry_later'
            ?_('Independent reauthentication is temporarily unavailable. No verification request was submitted.')
            :_('The verification request could not be prepared. No verification request was submitted.');
        unset($verification_method,$prepared,$step_up_csrf,$result);
    }
}

$assured=hrm_identifier_verification_request_route_assured($employee_ref,$now);
if($identifier_verification_request_raw_method==='POST' && $action==='confirm_request'){
    if(!check_csrf_token()||!is_array($assured)
        ||!hrm_identifier_verification_request_route_consume_confirm_csrf($assured,isset($_POST['confirm_csrf'])?(string)$_POST['confirm_csrf']:'',$now)){
        $generic_error=_('The verification request confirmation was rejected.');
    } else {
        $verification_method=isset($_POST['verification_method'])?(string)$_POST['verification_method']:'';
        $prepared=hrm_identifier_verification_request_prepare_browser_action($employee_ref,(int)$assured['identifier_id'],
            (int)$assured['identifier_row_version'],$verification_method);
        if(!is_array($prepared)||!hash_equals((string)$assured['action_hash'],(string)$prepared['action_hash'])){
            $generic_error=_('The verification method does not match the action that was reauthenticated. Re-enter the exact prepared method.');
        } else {
            $result=submit_hrm_person_identifier_verification_request((int)$assured['identifier_id'],
                (int)$assured['identifier_row_version'],(string)$prepared['verification_method'],$now);
            if($result!==false){
                $flash=is_array($result)?$result:array('status'=>'submitted');
                if(hrm_identifier_verification_request_route_set_flash($employee_ref,$flash)){
                    hrm_identifier_verification_request_route_clear_assured();
                    hrm_identifier_verification_request_route_redirect_local($identifier_verification_request_script_name,$employee_ref,'success');exit;
                }
            }
            $generic_error=_('The verification request was not submitted. No partial verification state was retained.');
        }
        unset($verification_method,$prepared,$result,$flash);
    }
    $assured=hrm_identifier_verification_request_route_assured($employee_ref,$now);
}

page(_($help_context='Identifier Verification Request'));
echo '<div class="center" style="max-width:760px;margin:0 auto;">';
echo '<p>'._('This route starts verification only for a current unverified successor created by governed identifier supersession. It shows masked metadata only and does not verify or approve the identifier.').'</p>';
if($generic_error!=='')display_error($generic_error);
$flash=hrm_identifier_verification_request_route_take_flash($employee_ref);
if(is_array($flash)){
    display_notification(_('Identifier verification request submitted for independent approval.'));
    echo '<p>'._('The identifier remains unverified and unapproved. An independent checker must complete the existing approval workflow before opaque verification evidence can be registered and the accepted promotion command can run.').'</p>';
}
if(isset($_GET['step_up'])&&$_GET['step_up']==='failed')display_error(_('Independent reauthentication failed or was cancelled. No verification request was submitted.'));
if(isset($_GET['step_up'])&&$_GET['step_up']==='retry')display_warning(_('Independent reauthentication must be restarted. No verification request was submitted.'));

if(is_array($assured)){
    $candidate=hrm_identifier_verification_request_browser_successor($employee_ref,(int)$assured['identifier_id'],(int)$assured['identifier_row_version']);
    echo '<h3>'._('Confirm the reauthenticated verification request').'</h3>';
    echo '<p>'._('Re-enter the exact verification method selected before reauthentication. The method is rebound to the action hash before the single-use assurance can be consumed.').'</p>';
    $confirm_csrf=hrm_identifier_verification_request_route_issue_confirm_csrf($assured,$now);
    start_form(false,$identifier_verification_request_script_name);
    hidden('_token',ensure_csrf_token());hidden('route_action','confirm_request');hidden('employee_id',$employee_ref);hidden('confirm_csrf',$confirm_csrf);
    start_table(TABLESTYLE2);
    $masked=is_array($candidate)?(string)$candidate['masked_value']:_('masked');
    label_row(_('Successor identifier:'),htmlspecialchars($masked,ENT_QUOTES,'UTF-8').' — '.sprintf(_('#%d, row version %d'),(int)$assured['identifier_id'],(int)$assured['identifier_row_version']));
    echo '<tr><td class="label">'._('Verification method:').'</td><td><input type="text" name="verification_method" value="" maxlength="32" autocomplete="off" required></td></tr>';
    end_table(1);submit_center('confirm',_('Submit Verification Request'));end_form();
} else {
    $candidates=get_hrm_identifier_verification_request_browser_candidates_masked($employee_ref);
    echo '<h3>'._('Prepare identifier verification request').'</h3>';
    if(!is_array($candidates)||count($candidates)===0){
        display_warning(_('No current unverified governed-supersession successor is eligible for a verification request.'));
    } else {
        start_form(false,$identifier_verification_request_script_name);
        hidden('_token',ensure_csrf_token());hidden('route_action','start_step_up');hidden('employee_id',$employee_ref);
        start_table(TABLESTYLE2);
        echo '<tr><td class="label">'._('Successor identifier:').'</td><td><select name="identifier_candidate">';
        foreach($candidates as $candidate){
            $value=(int)$candidate['identifier_id'].':'.(int)$candidate['row_version'];
            $label=(string)$candidate['identifier_type'].' — '.(string)$candidate['masked_value'].' — '.(string)$candidate['issuing_jurisdiction'];
            echo '<option value="'.htmlspecialchars($value,ENT_QUOTES,'UTF-8').'">'.htmlspecialchars($label,ENT_QUOTES,'UTF-8').'</option>';
        }
        echo '</select></td></tr>';
        echo '<tr><td class="label">'._('Verification method:').'</td><td><input type="text" name="verification_method" value="" maxlength="32" autocomplete="off" required></td></tr>';
        end_table(1);submit_center('reauthenticate',_('Reauthenticate for Verification Request'));end_form();
    }
}
echo '<p><a href="identifier_supersession.php?employee_id='.rawurlencode($employee_ref).'">'._('Back to Governed Identifier Change').'</a></p>';
echo '<p><a href="employees.php?employee_id='.rawurlencode($employee_ref).'">'._('Back to Employee Maintenance').'</a></p></div>';
end_page();
