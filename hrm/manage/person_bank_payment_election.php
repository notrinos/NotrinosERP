<?php
/** HRM-FND-004 governed Person bank payment-election maker/final-checker browser continuation. */
$bank_payment_election_raw_method=isset($_SERVER['REQUEST_METHOD'])?(string)$_SERVER['REQUEST_METHOD']:'';
$bank_payment_election_raw_query=isset($_SERVER['QUERY_STRING'])?(string)$_SERVER['QUERY_STRING']:'';
$bank_payment_election_script_name=isset($_SERVER['SCRIPT_NAME'])?(string)$_SERVER['SCRIPT_NAME']:'';
$page_security='SA_EMPLOYEE';
$path_to_root='../..';
/* Reject and canonicalize malformed callback-shaped GETs before session/bootstrap output can commit headers.
 * This preflight performs no authentication, OIDC completion, DB access, or mutation. Valid callbacks
 * remain behind the authenticated session boundary below. */
include_once($path_to_root.'/includes/hrm_person_bank_payment_election_route.inc');
if($bank_payment_election_raw_method==='GET' && isset($_GET['state'])
    && !is_array(hrm_bank_payment_election_route_parse_callback($bank_payment_election_raw_method,$bank_payment_election_raw_query))){
    hrm_bank_payment_election_route_security_headers();
    $invalid_employee_ref=hrm_bank_payment_election_route_employee_ref(isset($_GET['employee_id'])?$_GET['employee_id']:'');
    if(hrm_bank_payment_election_route_redirect_invalid_callback($bank_payment_election_script_name,$invalid_employee_ref))exit;
    http_response_code(400);exit;
}
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/hrm_security.inc');
include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/db/person_bank_account_payment_election_db.inc');
include_once($path_to_root.'/hrm/includes/db/person_bank_account_payment_election_browser_db.inc');
include_once($path_to_root.'/includes/federation_oidc_person_bank_payment_election_step_up.inc');
include_once($path_to_root.'/includes/hrm_person_bank_payment_election_route.inc');

hrm_bank_payment_election_route_security_headers();
$now=time();
$company=function_exists('user_company')?(int)user_company():-1;
$callback_path=hrm_bank_payment_election_route_callback_path($bank_payment_election_script_name);

if($bank_payment_election_raw_method==='GET' && isset($_GET['state'])){
    if(!session_transport_is_https()||!hrm_bank_payment_election_route_authenticated()||!hrm_bank_payment_election_route_ready()
        ||$company<0||$callback_path===false){http_response_code(403);exit;}
    $request=hrm_bank_payment_election_route_parse_callback($bank_payment_election_raw_method,$bank_payment_election_raw_query);
    if(!is_array($request)){http_response_code(400);exit;}
    if(!hrm_bank_payment_election_route_consume_attempt($now)){http_response_code(403);exit;}
    $pending=federation_oidc_bank_payment_election_step_up_pending($company,(string)$request['state'],$now);
    $employee_ref=is_array($pending)&&isset($pending['employee_ref'])?(string)$pending['employee_ref']:false;
    if($employee_ref===false){http_response_code(403);exit;}
    if($request['kind']==='error'){
        hrm_bank_payment_election_route_terminalize_error($company,(string)$request['state'],(string)$request['error']);
        federation_oidc_bank_payment_election_step_up_forget_pending((string)$request['state']);
        hrm_bank_payment_election_route_redirect_local($bank_payment_election_script_name,$employee_ref,'failed');exit;
    }
    $result=federation_oidc_complete_bank_payment_election_step_up($company,(string)$request['state'],(string)$request['code'],$callback_path);
    $status=is_array($result)&&isset($result['status'])?(string)$result['status']:'denied';
    if($status==='assured'&&hrm_bank_payment_election_route_bind_assured($result,$now)){
        hrm_bank_payment_election_route_reset_attempts();
        hrm_bank_payment_election_route_redirect_local($bank_payment_election_script_name,$employee_ref,'assured');exit;
    }
    if($status==='retry_later')federation_oidc_bank_payment_election_step_up_forget_pending((string)$request['state']);
    hrm_bank_payment_election_route_redirect_local($bank_payment_election_script_name,$employee_ref,$status==='retry_later'?'retry':'failed');exit;
}

$employee_ref=hrm_bank_payment_election_route_employee_ref(isset($_POST['employee_id'])?$_POST['employee_id']:(isset($_GET['employee_id'])?$_GET['employee_id']:''));
if(!hrm_bank_payment_election_route_authenticated()||$employee_ref===false){
    page(_($help_context='Bank Payment Election'));display_error(_('Dedicated bank payment-election authority is required.'));end_page();exit;
}
if(!hrm_bank_payment_election_route_ready()){
    page(_($help_context='Bank Payment Election'));display_error(_('Software Upgrade must be completed before governed payment elections are available.'));end_page();exit;
}

$action=isset($_POST['route_action'])?(string)$_POST['route_action']:'';
$generic_error='';

if($bank_payment_election_raw_method==='POST'&&$action==='start_submit_step_up'){
    if(!check_csrf_token()||!session_transport_is_https()||!hrm_bank_payment_election_route_consume_attempt($now)){
        $generic_error=_('The payment-election action could not be prepared.');
    }else{
        $parts=explode(':',isset($_POST['bank_candidate'])?(string)$_POST['bank_candidate']:'',2);
        $bank_id=count($parts)===2?hrm_bank_payment_election_positive_int($parts[0]):false;
        $bank_version=count($parts)===2?hrm_bank_payment_election_positive_int($parts[1]):false;
        $currency=isset($_POST['currency_code'])?strtoupper(trim((string)$_POST['currency_code'])):'';
        $effective=isset($_POST['effective_from'])?trim((string)$_POST['effective_from']):'';
        $prepared=($bank_id===false||$bank_version===false)?false:hrm_bank_payment_election_browser_prepare_submit($employee_ref,$currency,$effective,$bank_id,$bank_version);
        $retry_probe=is_array($prepared)?hrm_bank_payment_election_browser_retry_probe($prepared):false;
        if(is_array($retry_probe)&&!empty($retry_probe['exact_retry'])){
            $result=submit_hrm_person_bank_payment_election((int)$prepared['person_id'],(string)$prepared['currency_code'],
                (int)$prepared['predecessor_payment_election_id'],(int)$prepared['predecessor_row_version'],(string)$prepared['effective_from'],
                $prepared['allocations'],$now);
            if(is_array($result)&&!empty($result['exact_retry'])&&isset($result['payment_election_id'],$result['approval_draft_id'])){
                $flash=array('payment_election_id'=>(int)$result['payment_election_id'],'approval_draft_id'=>(int)$result['approval_draft_id'],
                    'status'=>'pending_exact_retry');
                if(hrm_bank_payment_election_route_set_flash($employee_ref,$flash)){
                    hrm_bank_payment_election_route_redirect_local($bank_payment_election_script_name,$employee_ref,'success');exit;
                }
            }
            $generic_error=_('The exact pending payment-election retry could not be revalidated. No new assurance was issued.');
        }elseif(is_array($retry_probe)){
            $generic_error=_('A different payment election is already pending for this Person and currency. Resolve that draft before starting a new assurance.');
        }elseif($retry_probe===null){
            $step_csrf=federation_oidc_bank_payment_election_step_up_issue_csrf($prepared,$now);
            $result=$step_csrf===false?false:federation_oidc_start_bank_payment_election_step_up($company,$employee_ref,
                HRM_BANK_PAYMENT_ELECTION_SUBMIT_ACTION_CLASS,(int)$prepared['object_id'],(int)$prepared['object_row_version'],
                (string)$prepared['action_hash'],$step_csrf,$callback_path,FEDERATION_OIDC_BANK_PAYMENT_ELECTION_STEP_UP_TRANSACTION_TTL,$now);
            if(is_array($result)&&($result['status']??'')==='authorization_required'&&isset($result['authorization_url'])
                &&hrm_bank_payment_election_route_redirect_provider((string)$result['authorization_url']))exit;
            $generic_error=_('Fresh independent reauthentication could not be completed. No payment election was submitted.');
        }else{
            $generic_error=_('The payment-election recovery state could not be proven. No new assurance was issued.');
        }
    }
}

if($bank_payment_election_raw_method==='POST'&&$action==='start_checker_step_up'){
    if(!check_csrf_token()||!session_transport_is_https()||!hrm_bank_payment_election_route_consume_attempt($now)){
        $generic_error=_('The final checker action could not be prepared.');
    }else{
        $parts=explode(':',isset($_POST['election_candidate'])?(string)$_POST['election_candidate']:'',2);
        $election_id=count($parts)===2?hrm_bank_payment_election_positive_int($parts[0]):false;
        $row_version=count($parts)===2?hrm_bank_payment_election_positive_int($parts[1]):false;
        $prepared=($election_id===false||$row_version===false)?false:hrm_bank_payment_election_browser_checker_candidate($employee_ref,$election_id,$row_version);
        $step_csrf=is_array($prepared)?federation_oidc_bank_payment_election_step_up_issue_csrf($prepared,$now):false;
        $result=$step_csrf===false?false:federation_oidc_start_bank_payment_election_step_up($company,$employee_ref,
            HRM_BANK_PAYMENT_ELECTION_APPROVE_ACTION_CLASS,(int)$prepared['object_id'],(int)$prepared['object_row_version'],
            (string)$prepared['action_hash'],$step_csrf,$callback_path,FEDERATION_OIDC_BANK_PAYMENT_ELECTION_STEP_UP_TRANSACTION_TTL,$now);
        if(is_array($result)&&($result['status']??'')==='authorization_required'&&isset($result['authorization_url'])
            &&hrm_bank_payment_election_route_redirect_provider((string)$result['authorization_url']))exit;
        $generic_error=_('Fresh final-checker reauthentication could not be completed. No approval was recorded.');
    }
}

$assured=hrm_bank_payment_election_route_assured($employee_ref,$now);
if($bank_payment_election_raw_method==='POST'&&$action==='confirm_submit'){
    if(!check_csrf_token()||!is_array($assured)||(string)$assured['action_class']!==HRM_BANK_PAYMENT_ELECTION_SUBMIT_ACTION_CLASS
        ||!hrm_bank_payment_election_route_consume_confirm_csrf($assured,isset($_POST['confirm_csrf'])?(string)$_POST['confirm_csrf']:'',$now)){
        $generic_error=_('The payment-election confirmation was rejected.');
    }else{
        $parts=explode(':',isset($_POST['bank_candidate'])?(string)$_POST['bank_candidate']:'',2);
        $bank_id=count($parts)===2?hrm_bank_payment_election_positive_int($parts[0]):false;
        $bank_version=count($parts)===2?hrm_bank_payment_election_positive_int($parts[1]):false;
        $currency=isset($_POST['currency_code'])?strtoupper(trim((string)$_POST['currency_code'])):'';
        $effective=isset($_POST['effective_from'])?trim((string)$_POST['effective_from']):'';
        $prepared=($bank_id===false||$bank_version===false)?false:hrm_bank_payment_election_browser_prepare_submit($employee_ref,$currency,$effective,$bank_id,$bank_version);
        if(!is_array($prepared)||!hash_equals((string)$assured['action_hash'],(string)$prepared['action_hash'])){
            $generic_error=_('The re-entered election does not match the action that was reauthenticated.');
        }else{
            $result=submit_hrm_person_bank_payment_election((int)$prepared['person_id'],(string)$prepared['currency_code'],
                (int)$prepared['predecessor_payment_election_id'],(int)$prepared['predecessor_row_version'],(string)$prepared['effective_from'],
                $prepared['allocations'],$now);
            if(is_array($result)&&isset($result['payment_election_id'])){
                $flash=array('payment_election_id'=>(int)$result['payment_election_id'],'approval_draft_id'=>(int)$result['approval_draft_id'],
                    'status'=>(!empty($result['exact_retry'])?'pending_exact_retry':'pending'));
                if(hrm_bank_payment_election_route_set_flash($employee_ref,$flash)){
                    hrm_bank_payment_election_route_clear_assured();
                    hrm_bank_payment_election_route_redirect_local($bank_payment_election_script_name,$employee_ref,'success');exit;
                }
            }
            $generic_error=_('The payment election was not submitted. No partial election, allocation, or approval state was retained.');
        }
    }
    $assured=hrm_bank_payment_election_route_assured($employee_ref,$now);
}

if($bank_payment_election_raw_method==='POST'&&$action==='confirm_checker'){
    if(!check_csrf_token()||!is_array($assured)||(string)$assured['action_class']!==HRM_BANK_PAYMENT_ELECTION_APPROVE_ACTION_CLASS
        ||!hrm_bank_payment_election_route_consume_confirm_csrf($assured,isset($_POST['confirm_csrf'])?(string)$_POST['confirm_csrf']:'',$now)){
        $generic_error=_('The final-checker confirmation was rejected.');
    }else{
        $candidate=hrm_bank_payment_election_browser_checker_candidate($employee_ref,(int)$assured['object_id'],(int)$assured['object_row_version']);
        if(!is_array($candidate)||!hash_equals((string)$assured['action_hash'],(string)$candidate['action_hash'])){
            $generic_error=_('The payment-election approval custody changed before confirmation.');
        }else{
            $approval=get_approval_workflow_service();
            $result=is_object($approval)?$approval->approve((int)$candidate['approval_draft_id'],_('Approved through governed bank payment-election continuation.')):false;
            if(is_array($result)&&($result['status']??'')==='approved'){
                $flash=array('payment_election_id'=>(int)$candidate['payment_election_id'],'approval_draft_id'=>(int)$candidate['approval_draft_id'],'status'=>'approved');
                if(hrm_bank_payment_election_route_set_flash($employee_ref,$flash)){
                    hrm_bank_payment_election_route_clear_assured();
                    hrm_bank_payment_election_route_redirect_local($bank_payment_election_script_name,$employee_ref,'success');exit;
                }
            }
            $generic_error=_('Final approval was not committed. The approval action, assurance consumption, and election promotion were rolled back together.');
        }
    }
    $assured=hrm_bank_payment_election_route_assured($employee_ref,$now);
}

page(_($help_context='Bank Payment Election'));
echo '<div class="center" style="max-width:900px;margin:0 auto;">';
echo '<p>'._('This HRM continuation exposes only the accepted Person payment-election command. Bank details remain masked. A verified/approved normalized bank account is only an eligibility input; this route creates no payment instruction and performs no payment, payroll post, bank transaction, GL, or accounting action.').'</p>';
if($generic_error!=='')display_error($generic_error);
$flash=hrm_bank_payment_election_route_take_flash($employee_ref);
if(is_array($flash)){
    if($flash['status']==='approved')display_notification(_('Bank payment election approved by the final independent checker.'));
    else display_notification(_('Bank payment election submitted for central maker-checker approval.'));
    echo '<p>'.sprintf(_('Election #%d; approval draft #%d.'),(int)$flash['payment_election_id'],(int)$flash['approval_draft_id']).'</p>';
}
if(isset($_GET['step_up'])&&$_GET['step_up']==='failed')display_error(_('Independent reauthentication failed or was cancelled. No election state changed.'));
if(isset($_GET['step_up'])&&$_GET['step_up']==='retry')display_warning(_('Independent reauthentication must be restarted. No election state changed.'));

if(is_array($assured)){
    $confirm_csrf=hrm_bank_payment_election_route_issue_confirm_csrf($assured,$now);
    if((string)$assured['action_class']===HRM_BANK_PAYMENT_ELECTION_SUBMIT_ACTION_CLASS){
        $default_effective=gmdate('Y-m-d H:i:s',$now+3600);
        $banks=get_hrm_bank_payment_election_browser_bank_candidates_masked($employee_ref,$default_effective);
        echo '<h3>'._('Confirm reauthenticated payment election').'</h3>';
        echo '<p>'._('Re-enter the exact currency, effective time, and bank selected before reauthentication. This first browser slice supports one verified bank at exactly 100.000000%.').'</p>';
        start_form(false,$bank_payment_election_script_name);hidden('_token',ensure_csrf_token());hidden('route_action','confirm_submit');hidden('employee_id',$employee_ref);hidden('confirm_csrf',$confirm_csrf);
        start_table(TABLESTYLE2);
        echo '<tr><td class="label">'._('Currency:').'</td><td><input name="currency_code" maxlength="3" pattern="[A-Z]{3}" autocomplete="off" required></td></tr>';
        echo '<tr><td class="label">'._('Effective from (UTC):').'</td><td><input name="effective_from" value="'.htmlspecialchars($default_effective,ENT_QUOTES,'UTF-8').'" maxlength="19" required></td></tr>';
        echo '<tr><td class="label">'._('Verified bank:').'</td><td><select name="bank_candidate">';
        if(is_array($banks))foreach($banks as $bank){$value=(int)$bank['person_bank_account_id'].':'.(int)$bank['bank_account_row_version'];$label=(string)$bank['masked_account'].' / '.(string)$bank['masked_routing'];echo '<option value="'.htmlspecialchars($value,ENT_QUOTES,'UTF-8').'">'.htmlspecialchars($label,ENT_QUOTES,'UTF-8').'</option>';}
        echo '</select></td></tr>';label_row(_('Allocation:'),_('100.000000%'));end_table(1);submit_center('confirm',_('Submit Exact Payment Election'));end_form();
    }else{
        $candidate=hrm_bank_payment_election_browser_checker_candidate($employee_ref,(int)$assured['object_id'],(int)$assured['object_row_version']);
        echo '<h3>'._('Confirm final independent approval').'</h3>';
        if(!is_array($candidate))display_error(_('This payment election is no longer the exact final actionable draft.'));
        else{
            echo '<p>'.sprintf(_('Election #%d, draft #%d, %s, effective %s UTC, %d allocation(s).'),(int)$candidate['payment_election_id'],(int)$candidate['approval_draft_id'],
                htmlspecialchars((string)$candidate['currency_code'],ENT_QUOTES,'UTF-8'),htmlspecialchars((string)$candidate['effective_from'],ENT_QUOTES,'UTF-8'),(int)$candidate['allocation_count']).'</p>';
            start_form(false,$bank_payment_election_script_name);hidden('_token',ensure_csrf_token());hidden('route_action','confirm_checker');hidden('employee_id',$employee_ref);hidden('confirm_csrf',$confirm_csrf);
            submit_center('confirm',_('Approve Exact Payment Election'));end_form();
        }
    }
}else{
    $default_effective=gmdate('Y-m-d H:i:s',$now+3600);
    $banks=get_hrm_bank_payment_election_browser_bank_candidates_masked($employee_ref,$default_effective);
    echo '<h3>'._('Create payment election').'</h3>';
    if(!is_array($banks)||count($banks)===0)display_warning(_('No verified and approved governed bank account is currently eligible.'));
    else{
        start_form(false,$bank_payment_election_script_name);hidden('_token',ensure_csrf_token());hidden('route_action','start_submit_step_up');hidden('employee_id',$employee_ref);start_table(TABLESTYLE2);
        echo '<tr><td class="label">'._('Currency:').'</td><td><input name="currency_code" maxlength="3" pattern="[A-Z]{3}" autocomplete="off" required></td></tr>';
        echo '<tr><td class="label">'._('Effective from (UTC):').'</td><td><input name="effective_from" value="'.htmlspecialchars($default_effective,ENT_QUOTES,'UTF-8').'" maxlength="19" required></td></tr>';
        echo '<tr><td class="label">'._('Verified bank:').'</td><td><select name="bank_candidate">';
        foreach($banks as $bank){$value=(int)$bank['person_bank_account_id'].':'.(int)$bank['bank_account_row_version'];$label=(string)$bank['masked_account'].' / '.(string)$bank['masked_routing'];echo '<option value="'.htmlspecialchars($value,ENT_QUOTES,'UTF-8').'">'.htmlspecialchars($label,ENT_QUOTES,'UTF-8').'</option>';}
        echo '</select></td></tr>';label_row(_('Allocation:'),_('100.000000%'));end_table(1);submit_center('reauthenticate',_('Reauthenticate to Submit Election'));end_form();
    }
    $checker=get_hrm_bank_payment_election_browser_checker_candidates($employee_ref);
    echo '<h3>'._('Final checker approvals').'</h3>';
    if(!is_array($checker)||count($checker)===0)display_warning(_('No payment election is currently at the exact final approval action for this checker.'));
    else{
        start_form(false,$bank_payment_election_script_name);hidden('_token',ensure_csrf_token());hidden('route_action','start_checker_step_up');hidden('employee_id',$employee_ref);start_table(TABLESTYLE2);
        echo '<tr><td class="label">'._('Election:').'</td><td><select name="election_candidate">';
        foreach($checker as $candidate){$value=(int)$candidate['payment_election_id'].':'.(int)$candidate['row_version'];$label='#'.(int)$candidate['payment_election_id'].' / '.(string)$candidate['currency_code'].' / '.(string)$candidate['effective_from'].' UTC';echo '<option value="'.htmlspecialchars($value,ENT_QUOTES,'UTF-8').'">'.htmlspecialchars($label,ENT_QUOTES,'UTF-8').'</option>';}
        echo '</select></td></tr>';end_table(1);submit_center('reauthenticate',_('Reauthenticate for Final Approval'));end_form();
    }
}
echo '<p>'._('Maker and final-checker assurance events are single-use and action-bound. Exact retries remain delegated to the accepted command; no browser-supplied predecessor or return URL is trusted.').'</p>';
echo '<p><a href="employees.php?employee_id='.rawurlencode($employee_ref).'">'._('Back to Employee Maintenance').'</a></p></div>';
end_page();
