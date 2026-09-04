<?php
/** HRM-FND-004 governed Person bank-account supersession browser route. */
$person_bank_supersession_raw_method=isset($_SERVER['REQUEST_METHOD'])?(string)$_SERVER['REQUEST_METHOD']:'';
$person_bank_supersession_raw_query=isset($_SERVER['QUERY_STRING'])?(string)$_SERVER['QUERY_STRING']:'';
$person_bank_supersession_script_name=isset($_SERVER['SCRIPT_NAME'])?(string)$_SERVER['SCRIPT_NAME']:'';
$page_security='SA_EMPLOYEE';
$path_to_root='../..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/hrm_security.inc');
include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/db/person_bank_account_supersession_db.inc');
include_once($path_to_root.'/includes/federation_oidc_person_bank_account_supersession_step_up.inc');
include_once($path_to_root.'/includes/hrm_person_bank_account_supersession_route.inc');

hrm_person_bank_supersession_route_security_headers();
$now=time();
$company=function_exists('user_company')?(int)user_company():-1;
$callback_path=hrm_person_bank_supersession_route_callback_path($person_bank_supersession_script_name);

/* OIDC callback is processed before output. Employee/action custody comes only from the server session. */
if($person_bank_supersession_raw_method==='GET' && isset($_GET['state'])){
    if(!session_transport_is_https()||!hrm_person_bank_supersession_route_authenticated()||!hrm_person_bank_supersession_route_ready()
        ||!hrm_person_bank_supersession_route_consume_attempt($now)||$company<0||$callback_path===false){
        http_response_code(403);exit;
    }
    $request=hrm_person_bank_supersession_route_parse_callback($person_bank_supersession_raw_method,$person_bank_supersession_raw_query);
    if(!is_array($request)){http_response_code(400);exit;}
    $pending=federation_oidc_bank_account_supersession_step_up_pending($company,(string)$request['state'],$now);
    $employee_ref=is_array($pending)&&isset($pending['employee_ref'])?(string)$pending['employee_ref']:false;
    if($employee_ref===false){http_response_code(403);exit;}
    if($request['kind']==='error'){
        hrm_person_bank_supersession_route_terminalize_error($company,(string)$request['state'],(string)$request['error']);
        federation_oidc_bank_account_supersession_step_up_forget_pending((string)$request['state']);
        hrm_person_bank_supersession_route_redirect_local($person_bank_supersession_script_name,$employee_ref,'failed');exit;
    }
    $result=federation_oidc_complete_bank_account_supersession_step_up($company,(string)$request['state'],(string)$request['code'],$callback_path);
    $status=is_array($result)&&isset($result['status'])?(string)$result['status']:'denied';
    if($status==='assured' && hrm_person_bank_supersession_route_bind_assured($result,$now)){
        hrm_person_bank_supersession_route_reset_attempts();
        hrm_person_bank_supersession_route_redirect_local($person_bank_supersession_script_name,$employee_ref,'assured');exit;
    }
    if($status==='retry_later') federation_oidc_bank_account_supersession_step_up_forget_pending((string)$request['state']);
    hrm_person_bank_supersession_route_redirect_local($person_bank_supersession_script_name,$employee_ref,$status==='retry_later'?'retry':'failed');exit;
}

$employee_ref=hrm_person_bank_supersession_route_employee_ref(
    isset($_POST['employee_id'])?$_POST['employee_id']:(isset($_GET['employee_id'])?$_GET['employee_id']:'')
);
if(!hrm_person_bank_supersession_route_authenticated()||$employee_ref===false){
    page(_($help_context='Governed Bank Detail Change'));display_error(_('Restricted bank-detail change authority is required.'));end_page();exit;
}
if(!hrm_person_bank_supersession_route_ready()){
    page(_($help_context='Governed Bank Detail Change'));display_error(_('Software Upgrade must be completed before governed bank-detail changes are available.'));end_page();exit;
}

function hrm_person_bank_supersession_browser_payload_from_post()
{
    return array(
        'bank_name'=>isset($_POST['bank_name'])?(string)$_POST['bank_name']:'',
        'bank_branch'=>isset($_POST['bank_branch'])?(string)$_POST['bank_branch']:'',
        'bank_account'=>isset($_POST['bank_account'])?(string)$_POST['bank_account']:'',
        'bank_routing'=>isset($_POST['bank_routing'])?(string)$_POST['bank_routing']:'',
        'payment_method'=>isset($_POST['payment_method'])?(int)$_POST['payment_method']:0,
    );
}

function hrm_person_bank_supersession_browser_payment_method_options($selected=0)
{
    $methods=function_exists('hrm_get_payment_methods')?hrm_get_payment_methods():array(0=>_('Bank Transfer'),1=>_('Cash'),2=>_('Cheque'));
    $html='';
    foreach($methods as $value=>$label){
        $html.='<option value="'.(int)$value.'"'.((int)$selected===(int)$value?' selected':'').'>'.htmlspecialchars((string)$label,ENT_QUOTES,'UTF-8').'</option>';
    }
    return $html;
}

$action=isset($_POST['route_action'])?(string)$_POST['route_action']:'';
$generic_error='';
if($person_bank_supersession_raw_method==='POST' && $action==='start_step_up'){
    if(!check_csrf_token()||!session_transport_is_https()||!hrm_person_bank_supersession_route_consume_attempt($now)){
        $generic_error=_('The governed bank-detail change could not be prepared.');
    } else {
        $candidate=isset($_POST['bank_candidate'])?(string)$_POST['bank_candidate']:'';
        $parts=explode(':',$candidate,2);
        $bank_id=count($parts)===2?hrm_person_bank_supersession_positive_int($parts[0]):false;
        $row_version=count($parts)===2?hrm_person_bank_supersession_positive_int($parts[1]):false;
        $new_payload=hrm_person_bank_supersession_browser_payload_from_post();
        $prepared=($bank_id===false||$row_version===false)?false:hrm_person_bank_supersession_prepare_browser_action(
            $employee_ref,$bank_id,$row_version,$new_payload
        );
        /* Protected bank fields are deliberately never copied into session, URL, audit output, or an error string. */
        $step_up_csrf=is_array($prepared)?federation_oidc_bank_account_supersession_step_up_issue_csrf($prepared,$now):false;
        $result=$step_up_csrf===false?false:federation_oidc_start_bank_account_supersession_step_up(
            $company,$employee_ref,(int)$prepared['person_bank_account_id'],(int)$prepared['bank_account_row_version'],
            (string)$prepared['action_hash'],$step_up_csrf,$callback_path,FEDERATION_OIDC_BANK_ACCOUNT_SUPERSESSION_STEP_UP_TRANSACTION_TTL,$now
        );
        if(is_array($result)&&($result['status']??'')==='authorization_required'&&isset($result['authorization_url'])
            &&hrm_person_bank_supersession_route_redirect_provider((string)$result['authorization_url']))exit;
        $generic_error=is_array($result)&&($result['status']??'')==='retry_later'
            ?_('Reauthentication is temporarily unavailable. No bank-detail change was made.')
            :_('The governed bank-detail change could not be prepared. No bank-detail change was made.');
        unset($new_payload,$prepared,$step_up_csrf,$result);
    }
}

$assured=hrm_person_bank_supersession_route_assured($employee_ref,$now);
if($person_bank_supersession_raw_method==='POST' && $action==='confirm_supersession'){
    if(!check_csrf_token()||!is_array($assured)
        ||!hrm_person_bank_supersession_route_consume_confirm_csrf($assured,isset($_POST['confirm_csrf'])?(string)$_POST['confirm_csrf']:'',$now)){
        $generic_error=_('The governed bank-detail confirmation was rejected.');
    } else {
        $new_payload=hrm_person_bank_supersession_browser_payload_from_post();
        $prepared=hrm_person_bank_supersession_prepare_browser_action($employee_ref,(int)$assured['person_bank_account_id'],
            (int)$assured['bank_account_row_version'],$new_payload,true);
        if(!is_array($prepared)||!hash_equals((string)$assured['action_hash'],(string)$prepared['action_hash'])){
            $generic_error=_('The entered bank details do not match the action that was reauthenticated. Re-enter the exact prepared details.');
        } else {
            $result=supersede_hrm_person_bank_account((int)$assured['person_bank_account_id'],
                (int)$assured['bank_account_row_version'],$new_payload,$now);
            if(is_array($result)&&hrm_person_bank_supersession_route_set_flash($employee_ref,$result)){
                hrm_person_bank_supersession_route_clear_assured();
                hrm_person_bank_supersession_route_redirect_local($person_bank_supersession_script_name,$employee_ref,'success');exit;
            }
            $generic_error=_('The governed bank-detail change was not completed. No partial bank change was retained.');
        }
        unset($new_payload,$prepared,$result);
    }
    $assured=hrm_person_bank_supersession_route_assured($employee_ref,$now);
}

page(_($help_context='Governed Bank Detail Change'));
echo '<div class="center" style="max-width:760px;margin:0 auto;">';
echo '<p>'._('This route replaces one current verified and independently approved normalized bank account with a new unverified successor. Stored bank details are never revealed.').'</p>';
if($generic_error!=='')display_error($generic_error);
$flash=hrm_person_bank_supersession_route_take_flash($employee_ref);
if(is_array($flash)){
    display_notification(_('Governed bank-account successor created. The successor remains unverified and unapproved.'));
    echo '<p>'._('The successor must complete the accepted independent bank verification request, approval, opaque evidence registration, and verification-promotion workflow before it can be treated as verified. It is not a payment election or payment binding.').'</p>';
}
if(isset($_GET['step_up'])&&$_GET['step_up']==='failed')display_error(_('Reauthentication failed or was cancelled. No bank-detail change was made.'));
if(isset($_GET['step_up'])&&$_GET['step_up']==='retry')display_warning(_('Reauthentication must be restarted. No bank-detail change was made.'));

if(is_array($assured)){
    echo '<h3>'._('Confirm the reauthenticated bank-detail change').'</h3>';
    echo '<p>'._('For privacy, the proposed bank details were not stored during reauthentication. Re-enter the exact same bank name, branch, account, routing and payment method to consume the single-use assurance.').'</p>';
    $confirm_csrf=hrm_person_bank_supersession_route_issue_confirm_csrf($assured,$now);
    start_form(false,$person_bank_supersession_script_name);
    hidden('_token',ensure_csrf_token());hidden('route_action','confirm_supersession');hidden('employee_id',$employee_ref);hidden('confirm_csrf',$confirm_csrf);
    start_table(TABLESTYLE2);
    label_row(_('Predecessor bank account:'),htmlspecialchars((string)$assured['person_bank_account_id'],ENT_QUOTES,'UTF-8').' — '.sprintf(_('row version %d'),(int)$assured['bank_account_row_version']));
    echo '<tr><td class="label">'._('Bank name:').'</td><td><input type="text" name="bank_name" value="" maxlength="255" autocomplete="off"></td></tr>';
    echo '<tr><td class="label">'._('Bank branch:').'</td><td><input type="text" name="bank_branch" value="" maxlength="255" autocomplete="off"></td></tr>';
    echo '<tr><td class="label">'._('Account number / IBAN:').'</td><td><input type="password" name="bank_account" value="" maxlength="255" autocomplete="new-password"></td></tr>';
    echo '<tr><td class="label">'._('Routing / BIC:').'</td><td><input type="password" name="bank_routing" value="" maxlength="255" autocomplete="new-password"></td></tr>';
    echo '<tr><td class="label">'._('Payment method:').'</td><td><select name="payment_method">'.hrm_person_bank_supersession_browser_payment_method_options().'</select></td></tr>';
    end_table(1);submit_center('confirm',_('Create Unverified Bank Successor'));end_form();
} else {
    $candidates=get_hrm_person_bank_supersession_candidates_masked($employee_ref);
    echo '<h3>'._('Prepare governed bank-detail change').'</h3>';
    if(!is_array($candidates)||count($candidates)===0){
        display_warning(_('No current verified and independently approved normalized bank account is eligible for governed supersession.'));
    } else {
        start_form(false,$person_bank_supersession_script_name);
        hidden('_token',ensure_csrf_token());hidden('route_action','start_step_up');hidden('employee_id',$employee_ref);
        start_table(TABLESTYLE2);
        echo '<tr><td class="label">'._('Current bank account:').'</td><td><select name="bank_candidate">';
        foreach($candidates as $candidate){
            $value=(int)$candidate['person_bank_account_id'].':'.(int)$candidate['row_version'];
            $label=(string)$candidate['masked_account'].' — '.(string)$candidate['masked_routing'].' — '._('verified / approved');
            echo '<option value="'.htmlspecialchars($value,ENT_QUOTES,'UTF-8').'">'.htmlspecialchars($label,ENT_QUOTES,'UTF-8').'</option>';
        }
        echo '</select></td></tr>';
        echo '<tr><td class="label">'._('Bank name:').'</td><td><input type="text" name="bank_name" value="" maxlength="255" autocomplete="off"></td></tr>';
        echo '<tr><td class="label">'._('Bank branch:').'</td><td><input type="text" name="bank_branch" value="" maxlength="255" autocomplete="off"></td></tr>';
        echo '<tr><td class="label">'._('Account number / IBAN:').'</td><td><input type="password" name="bank_account" value="" maxlength="255" autocomplete="new-password"></td></tr>';
        echo '<tr><td class="label">'._('Routing / BIC:').'</td><td><input type="password" name="bank_routing" value="" maxlength="255" autocomplete="new-password"></td></tr>';
        echo '<tr><td class="label">'._('Payment method:').'</td><td><select name="payment_method">'.hrm_person_bank_supersession_browser_payment_method_options().'</select></td></tr>';
        end_table(1);submit_center('reauthenticate',_('Reauthenticate for Governed Bank Change'));end_form();
    }
}
echo '<p><a href="employees.php?employee_id='.rawurlencode($employee_ref).'">'._('Back to Employee Maintenance').'</a></p></div>';
end_page();
