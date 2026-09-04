<?php
/** HRM-FND-004 final-checker opaque bank-account verification-evidence browser continuation. */
$bank_account_verification_evidence_raw_method=isset($_SERVER['REQUEST_METHOD'])?(string)$_SERVER['REQUEST_METHOD']:'';
$bank_account_verification_evidence_raw_query=isset($_SERVER['QUERY_STRING'])?(string)$_SERVER['QUERY_STRING']:'';
$bank_account_verification_evidence_script_name=isset($_SERVER['SCRIPT_NAME'])?(string)$_SERVER['SCRIPT_NAME']:'';
$page_security='SA_EMPLOYEE';
$path_to_root='../..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/hrm_security.inc');
include_once($path_to_root.'/hrm/includes/db/person_bank_account_verification_evidence_db.inc');
include_once($path_to_root.'/hrm/includes/db/person_bank_account_verification_evidence_browser_db.inc');
include_once($path_to_root.'/includes/federation_oidc_person_bank_account_verification_evidence_step_up.inc');
include_once($path_to_root.'/includes/hrm_person_bank_account_verification_evidence_route.inc');

hrm_bank_account_verification_evidence_route_security_headers();
$now=time();
$company=function_exists('user_company')?(int)user_company():-1;
$callback_path=hrm_bank_account_verification_evidence_route_callback_path($bank_account_verification_evidence_script_name);

/* OIDC callback accepts only exact state/code or state/error. Evidence provenance is never carried in the URL. */
if($bank_account_verification_evidence_raw_method==='GET' && isset($_GET['state'])){
    if(!session_transport_is_https()||!hrm_bank_account_verification_evidence_route_authenticated()||!hrm_bank_account_verification_evidence_route_ready()
        ||!hrm_bank_account_verification_evidence_route_consume_attempt($now)||$company<0||$callback_path===false){
        http_response_code(403);exit;
    }
    $request=hrm_bank_account_verification_evidence_route_parse_callback($bank_account_verification_evidence_raw_method,$bank_account_verification_evidence_raw_query);
    if(!is_array($request)){http_response_code(400);exit;}
    $pending=federation_oidc_bank_account_verification_evidence_step_up_pending($company,(string)$request['state'],$now);
    $employee_ref=is_array($pending)&&isset($pending['employee_ref'])?(string)$pending['employee_ref']:false;
    if($employee_ref===false){http_response_code(403);exit;}
    if($request['kind']==='error'){
        hrm_bank_account_verification_evidence_route_terminalize_error($company,(string)$request['state'],(string)$request['error']);
        federation_oidc_bank_account_verification_evidence_step_up_forget_pending((string)$request['state']);
        hrm_bank_account_verification_evidence_route_redirect_local($bank_account_verification_evidence_script_name,$employee_ref,'failed');exit;
    }
    $result=federation_oidc_complete_bank_account_verification_evidence_step_up(
        $company,(string)$request['state'],(string)$request['code'],$callback_path);
    $status=is_array($result)&&isset($result['status'])?(string)$result['status']:'denied';
    if($status==='assured'&&hrm_bank_account_verification_evidence_route_bind_assured($result,$now)){
        hrm_bank_account_verification_evidence_route_reset_attempts();
        hrm_bank_account_verification_evidence_route_redirect_local($bank_account_verification_evidence_script_name,$employee_ref,'assured');exit;
    }
    if($status==='retry_later')federation_oidc_bank_account_verification_evidence_step_up_forget_pending((string)$request['state']);
    hrm_bank_account_verification_evidence_route_redirect_local($bank_account_verification_evidence_script_name,$employee_ref,$status==='retry_later'?'retry':'failed');exit;
}

$employee_ref=hrm_bank_account_verification_evidence_route_employee_ref(
    isset($_POST['employee_id'])?$_POST['employee_id']:(isset($_GET['employee_id'])?$_GET['employee_id']:'')
);
if(!hrm_bank_account_verification_evidence_route_authenticated()||$employee_ref===false){
    page(_($help_context='Bank Account Verification Evidence'));display_error(_('Restricted financial bank-change authority is required.'));end_page();exit;
}
if(!hrm_bank_account_verification_evidence_route_ready()){
    page(_($help_context='Bank Account Verification Evidence'));display_error(_('Software Upgrade must be completed before bank-account verification evidence registration is available.'));end_page();exit;
}

$action=isset($_POST['route_action'])?(string)$_POST['route_action']:'';
$generic_error='';
if($bank_account_verification_evidence_raw_method==='POST'&&$action==='start_step_up'){
    if(!check_csrf_token()||!session_transport_is_https()||!hrm_bank_account_verification_evidence_route_consume_attempt($now)){
        $generic_error=_('The evidence registration could not be prepared.');
    }else{
        $candidate=isset($_POST['evidence_candidate'])?(string)$_POST['evidence_candidate']:'';
        $parts=explode(':',$candidate,3);
        $request_id=count($parts)===3?hrm_bank_account_verification_request_positive_int($parts[0]):false;
        $request_row_version=count($parts)===3?hrm_bank_account_verification_request_positive_int($parts[1]):false;
        $approval_draft_id=count($parts)===3?hrm_bank_account_verification_request_positive_int($parts[2]):false;
        $verified_at_epoch=hrm_bank_account_verification_evidence_browser_verified_at_epoch(isset($_POST['verified_at_utc'])?(string)$_POST['verified_at_utc']:'');
        $prepared=($request_id===false||$request_row_version===false||$approval_draft_id===false||$verified_at_epoch===false)?false:
            hrm_bank_account_verification_evidence_prepare_browser_action(
                $employee_ref,$request_id,$request_row_version,$approval_draft_id,
                isset($_POST['evidence_source'])?(string)$_POST['evidence_source']:'',
                isset($_POST['evidence_reference_hash'])?(string)$_POST['evidence_reference_hash']:'',
                isset($_POST['evidence_payload_hash'])?(string)$_POST['evidence_payload_hash']:'',
                $verified_at_epoch,$now);
        $step_up_csrf=is_array($prepared)?federation_oidc_bank_account_verification_evidence_step_up_issue_csrf($prepared,$now):false;
        $result=$step_up_csrf===false?false:federation_oidc_start_bank_account_verification_evidence_step_up(
            $company,$employee_ref,(int)$prepared['verification_request_id'],(int)$prepared['request_row_version'],
            (int)$prepared['approval_draft_id'],(int)$prepared['person_bank_account_id'],(int)$prepared['bank_account_row_version'],
            (string)$prepared['action_hash'],$step_up_csrf,$callback_path,
            FEDERATION_OIDC_BANK_ACCOUNT_VERIFICATION_EVIDENCE_STEP_UP_TRANSACTION_TTL,$now);
        if(is_array($result)&&($result['status']??'')==='authorization_required'&&isset($result['authorization_url'])
            &&hrm_bank_account_verification_evidence_route_redirect_provider((string)$result['authorization_url']))exit;
        $generic_error=is_array($result)&&($result['status']??'')==='retry_later'
            ?_('Independent checker reauthentication is temporarily unavailable. No evidence was registered.')
            :_('The evidence registration could not be prepared. No evidence was registered.');
        unset($prepared,$step_up_csrf,$result,$verified_at_epoch);
    }
}

$assured=hrm_bank_account_verification_evidence_route_assured($employee_ref,$now);
if($bank_account_verification_evidence_raw_method==='POST'&&$action==='confirm_evidence'){
    if(!check_csrf_token()||!is_array($assured)
        ||!hrm_bank_account_verification_evidence_route_consume_confirm_csrf($assured,isset($_POST['confirm_csrf'])?(string)$_POST['confirm_csrf']:'',$now)){
        $generic_error=_('The evidence registration confirmation was rejected.');
    }else{
        $verified_at_epoch=hrm_bank_account_verification_evidence_browser_verified_at_epoch(isset($_POST['verified_at_utc'])?(string)$_POST['verified_at_utc']:'');
        $prepared=$verified_at_epoch===false?false:hrm_bank_account_verification_evidence_prepare_browser_action(
            $employee_ref,(int)$assured['verification_request_id'],(int)$assured['request_row_version'],(int)$assured['approval_draft_id'],
            isset($_POST['evidence_source'])?(string)$_POST['evidence_source']:'',
            isset($_POST['evidence_reference_hash'])?(string)$_POST['evidence_reference_hash']:'',
            isset($_POST['evidence_payload_hash'])?(string)$_POST['evidence_payload_hash']:'',
            $verified_at_epoch,$now);
        if(!is_array($prepared)||!hash_equals((string)$assured['action_hash'],(string)$prepared['action_hash'])){
            $generic_error=_('The evidence provenance does not match the action that was reauthenticated. Re-enter the exact prepared source, hashes and UTC verification time.');
        }else{
            $result=register_hrm_person_bank_account_verification_evidence(
                (int)$prepared['verification_request_id'],(int)$prepared['request_row_version'],(int)$prepared['approval_draft_id'],
                (string)$prepared['verification_method'],(string)$prepared['evidence_source'],(string)$prepared['evidence_reference_hash'],
                (string)$prepared['evidence_payload_hash'],(int)$prepared['verified_at_epoch'],$now);
            if(is_array($result)&&hrm_bank_account_verification_evidence_route_set_flash($employee_ref,$result)){
                hrm_bank_account_verification_evidence_route_clear_assured();
                hrm_bank_account_verification_evidence_route_redirect_local($bank_account_verification_evidence_script_name,$employee_ref,'success');exit;
            }
            $generic_error=_('The evidence was not registered. No partial evidence or bank account promotion was retained.');
        }
        unset($verified_at_epoch,$prepared,$result);
    }
    $assured=hrm_bank_account_verification_evidence_route_assured($employee_ref,$now);
}

page(_($help_context='Bank Account Verification Evidence'));
echo '<div class="center" style="max-width:860px;margin:0 auto;">';
echo '<p>'._('This checker-only continuation registers opaque external verification provenance after the exact bank-account verification request has completed central approval. It never accepts raw evidence or reveals bank account plaintext.').'</p>';
if($generic_error!=='')display_error($generic_error);
$flash=hrm_bank_account_verification_evidence_route_take_flash($employee_ref);
if(is_array($flash)){
    display_notification(_('Opaque bank-account verification evidence registered.'));
    echo '<p>'.sprintf(_('Evidence #%d is now bound to the approved request and final checker.'),(int)$flash['verification_evidence_id']).'</p>';
    echo '<p>'._('The bank account remains unverified and unapproved. Verification promotion remains a separately governed downstream continuation and is not exposed by this page. No promotion ran from this page.').'</p>';
}
if(isset($_GET['step_up'])&&$_GET['step_up']==='failed')display_error(_('Independent checker reauthentication failed or was cancelled. No evidence was registered.'));
if(isset($_GET['step_up'])&&$_GET['step_up']==='retry')display_warning(_('Independent checker reauthentication must be restarted. No evidence was registered.'));

if(is_array($assured)){
    $candidate=hrm_bank_account_verification_evidence_browser_candidate($employee_ref,(int)$assured['verification_request_id'],
        (int)$assured['request_row_version'],(int)$assured['approval_draft_id']);
    echo '<h3>'._('Confirm the reauthenticated evidence registration').'</h3>';
    echo '<p>'._('Re-enter the exact opaque evidence provenance prepared before reauthentication. Only the SHA-256 hashes, bounded source token and UTC verification time are accepted; raw evidence must stay outside this route.').'</p>';
    $confirm_csrf=hrm_bank_account_verification_evidence_route_issue_confirm_csrf($assured,$now);
    start_form(false,$bank_account_verification_evidence_script_name);
    hidden('_token',ensure_csrf_token());hidden('route_action','confirm_evidence');hidden('employee_id',$employee_ref);hidden('confirm_csrf',$confirm_csrf);
    start_table(TABLESTYLE2);
    $masked_account=is_array($candidate)?(string)$candidate['masked_account']:_('masked');
    $masked_routing=is_array($candidate)?(string)$candidate['masked_routing']:_('masked');
    $payment_method=is_array($candidate)?(string)$candidate['payment_method']:_('unknown');
    $method=is_array($candidate)?(string)$candidate['verification_method']:_('unknown');
    label_row(_('Approved request:'),sprintf(_('#%d, request version %d, draft #%d'),(int)$assured['verification_request_id'],(int)$assured['request_row_version'],(int)$assured['approval_draft_id']));
    label_row(_('Bank account:'),htmlspecialchars($masked_account,ENT_QUOTES,'UTF-8').' / '.htmlspecialchars($masked_routing,ENT_QUOTES,'UTF-8').' — '.htmlspecialchars($payment_method,ENT_QUOTES,'UTF-8').' — '.sprintf(_('#%d, row version %d'),(int)$assured['person_bank_account_id'],(int)$assured['bank_account_row_version']));
    label_row(_('Verification method:'),htmlspecialchars($method,ENT_QUOTES,'UTF-8'));
    echo '<tr><td class="label">'._('Evidence source:').'</td><td><input type="text" name="evidence_source" maxlength="32" autocomplete="off" required></td></tr>';
    echo '<tr><td class="label">'._('Evidence reference SHA-256:').'</td><td><input type="text" name="evidence_reference_hash" maxlength="64" pattern="[0-9a-f]{64}" autocomplete="off" required></td></tr>';
    echo '<tr><td class="label">'._('Evidence payload SHA-256:').'</td><td><input type="text" name="evidence_payload_hash" maxlength="64" pattern="[0-9a-f]{64}" autocomplete="off" required></td></tr>';
    echo '<tr><td class="label">'._('Verified at UTC:').'</td><td><input type="text" name="verified_at_utc" value="" placeholder="YYYY-MM-DD HH:MM:SS" maxlength="19" autocomplete="off" required></td></tr>';
    end_table(1);submit_center('confirm',_('Register Opaque Verification Evidence'));end_form();
}else{
    $candidates=get_hrm_bank_account_verification_evidence_browser_candidates_masked($employee_ref);
    echo '<h3>'._('Prepare opaque verification evidence registration').'</h3>';
    if(!is_array($candidates)||count($candidates)===0){
        display_warning(_('No centrally approved bank-account verification request is eligible for evidence registration by this final checker.'));
    }else{
        start_form(false,$bank_account_verification_evidence_script_name);
        hidden('_token',ensure_csrf_token());hidden('route_action','start_step_up');hidden('employee_id',$employee_ref);
        start_table(TABLESTYLE2);
        echo '<tr><td class="label">'._('Approved request:').'</td><td><select name="evidence_candidate">';
        foreach($candidates as $candidate){
            $value=(int)$candidate['verification_request_id'].':'.(int)$candidate['request_row_version'].':'.(int)$candidate['approval_draft_id'];
            $label='#'.(int)$candidate['verification_request_id'].' — '.(string)$candidate['payment_method'].' — '.(string)$candidate['masked_account'].' / '.(string)$candidate['masked_routing'].' — '.(string)$candidate['verification_method'];
            echo '<option value="'.htmlspecialchars($value,ENT_QUOTES,'UTF-8').'">'.htmlspecialchars($label,ENT_QUOTES,'UTF-8').'</option>';
        }
        echo '</select></td></tr>';
        echo '<tr><td class="label">'._('Evidence source:').'</td><td><input type="text" name="evidence_source" maxlength="32" autocomplete="off" required></td></tr>';
        echo '<tr><td class="label">'._('Evidence reference SHA-256:').'</td><td><input type="text" name="evidence_reference_hash" maxlength="64" pattern="[0-9a-f]{64}" autocomplete="off" required></td></tr>';
        echo '<tr><td class="label">'._('Evidence payload SHA-256:').'</td><td><input type="text" name="evidence_payload_hash" maxlength="64" pattern="[0-9a-f]{64}" autocomplete="off" required></td></tr>';
        echo '<tr><td class="label">'._('Verified at UTC:').'</td><td><input type="text" name="verified_at_utc" value="'.htmlspecialchars(gmdate('Y-m-d H:i:s',$now),ENT_QUOTES,'UTF-8').'" placeholder="YYYY-MM-DD HH:MM:SS" maxlength="19" autocomplete="off" required></td></tr>';
        end_table(1);submit_center('reauthenticate',_('Reauthenticate to Register Evidence'));end_form();
    }
}
echo '<p><a href="person_bank_account_verification_request.php?employee_id='.rawurlencode($employee_ref).'">'._('Back to Bank Verification Request').'</a></p>';
echo '<p><a href="employees.php?employee_id='.rawurlencode($employee_ref).'">'._('Back to Employee Maintenance').'</a></p></div>';
end_page();
