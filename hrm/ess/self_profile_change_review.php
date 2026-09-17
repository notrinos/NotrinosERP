<?php
/** HRM-ESS-001 independent checker route for bounded self-profile requests. */
$ess_review_raw_method=isset($_SERVER['REQUEST_METHOD'])?(string)$_SERVER['REQUEST_METHOD']:'';
$ess_review_raw_query=isset($_SERVER['QUERY_STRING'])?(string)$_SERVER['QUERY_STRING']:'';
$ess_review_script=isset($_SERVER['SCRIPT_NAME'])?(string)$_SERVER['SCRIPT_NAME']:'';
$page_security='SA_HRM_APPROVE_ESS_SELF_PROFILE_CHANGE';$path_to_root='../..';
include_once($path_to_root.'/includes/session.inc');include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/ess_self_change_approval_db.inc');include_once($path_to_root.'/hrm/includes/db/ess_self_change_recovery_db.inc');
include_once($path_to_root.'/includes/hrm_ess_self_change_route.inc');include_once($path_to_root.'/includes/hrm_ess_self_change_review_route.inc');
hrm_ess_self_change_route_security_headers();
$now=time();$company=function_exists('user_company')?(int)user_company():-1;$suffix='/hrm/ess/self_profile_change_review.php';
$callback_path=hrm_ess_self_change_route_callback_path($ess_review_script,$suffix);$secure=function_exists('session_transport_is_https')&&session_transport_is_https();
$authorized=hrm_ess_self_change_review_checker_authorized();$ready=hrm_ess_self_change_route_ready_for('1.0.454');$message='';$ok_message='';

if($ess_review_raw_method==='GET'&&isset($_GET['state'])){
    if(!$secure||!$authorized||!$ready||!hrm_ess_self_change_route_consume_attempt($now)||$company<0||$callback_path===false){http_response_code(403);exit;}
    $request=hrm_ess_self_change_route_parse_callback($ess_review_raw_method,$ess_review_raw_query);if(!is_array($request)){http_response_code(400);exit;}
    if($request['kind']==='error'){hrm_ess_self_change_route_terminalize_error($company,(string)$request['state'],(string)$request['error']);hrm_ess_self_change_reauth_forget_pending((string)$request['state']);hrm_ess_self_change_route_redirect_local($ess_review_script,$suffix,'failed');exit;}
    $result=hrm_ess_self_change_reauth_complete_oidc($company,(string)$request['state'],(string)$request['code'],$callback_path);$status=is_array($result)?(string)($result['status']??'denied'):'denied';
    if($status==='assured'&&hrm_ess_self_change_route_bind_assured($result,$now)){hrm_ess_self_change_route_reset_attempts();hrm_ess_self_change_route_redirect_local($ess_review_script,$suffix,'assured');exit;}
    if($status==='retry_later')hrm_ess_self_change_reauth_forget_pending((string)$request['state']);hrm_ess_self_change_route_redirect_local($ess_review_script,$suffix,$status==='retry_later'?'retry':'failed');exit;
}

function ess_review_id($value){return hrm_ess_self_change_review_positive_id(is_scalar($value)?(string)$value:$value);}
function ess_review_local_or_federated($prepared,$password,$callable)
{
    if(!is_array($prepared))return false;
    if(hrm_ess_self_change_route_auth_method()==='local_password'){
        if(!hrm_ess_self_change_reauth_local_password((string)$password))return false;return call_user_func($callable);
    }
    return hrm_ess_self_change_route_run_governed($prepared,'',$callable);
}

$action=isset($_POST['route_action'])?(string)$_POST['route_action']:'';
if($ess_review_raw_method==='POST'){
    if(!$authorized||!$ready||!$secure||!check_csrf_token()||!hrm_ess_self_change_route_consume_attempt($now))$message=_('The review action was rejected. No request state was changed.');
    elseif(!hrm_ess_self_change_route_supported_auth_method())$message=_('This session does not support the required reauthentication method.');
    else{
        $request_id=ess_review_id($_POST['request_id']??'');
        if(in_array($action,array('start_approve_step_up','start_reject_step_up'),true)){
            $operation=$action==='start_approve_step_up'?'approve':'reject';$prepared=$request_id===false?false:hrm_ess_self_change_reauth_action_context($operation,$request_id,'');
            $token=is_array($prepared)?hrm_ess_self_change_reauth_issue_csrf($prepared,$now):false;$result=$token===false?false:hrm_ess_self_change_reauth_start_oidc($prepared,$token,$callback_path,HRM_ESS_SELF_CHANGE_REAUTH_TRANSACTION_TTL,$now);
            if(is_array($result)&&($result['status']??'')==='authorization_required'&&isset($result['authorization_url'])&&hrm_ess_self_change_route_redirect_provider((string)$result['authorization_url']))exit;
            $message=is_array($result)&&($result['status']??'')==='retry_later'?_('Independent reauthentication is temporarily unavailable. No review action was applied.'):_('Independent reauthentication could not be started. No review action was applied.');
        }elseif(in_array($action,array('approve_request','reject_request'),true)){
            $operation=$action==='approve_request'?'approve':'reject';$prepared=$request_id===false?false:hrm_ess_self_change_reauth_action_context($operation,$request_id,'');$password=(string)($_POST['current_password']??'');
            if($operation==='approve'){$digest=$request_id===false?false:hrm_ess_self_change_review_approval_digest($request_id);$ok=$digest!==false&&ess_review_local_or_federated($prepared,$password,function()use($request_id,$digest){return approve_hrm_ess_self_profile_change($request_id,$digest);})!==false;}
            else{$ok=ess_review_local_or_federated($prepared,$password,function()use($request_id){return reject_hrm_ess_self_profile_change($request_id,'checker_rejected_review');})!==false;}
            if($ok){hrm_ess_self_change_route_reset_attempts();$ok_message=$operation==='approve'?_('The request was approved for a distinct executor.'):_('The request was rejected without applying profile state.');}
            else $message=_('The independently reauthenticated review action failed closed. No partial request state was retained.');
            unset($password,$digest);
        }elseif(in_array($action,array('confirm_approve','confirm_reject'),true)){
            $operation=$action==='confirm_approve'?'approve':'reject';$assured=hrm_ess_self_change_route_assured($operation,$now);$confirm=(string)($_POST['confirm_csrf']??'');
            $prepared=is_array($assured)?hrm_ess_self_change_reauth_action_context($operation,(int)$assured['request_id'],''):false;
            $valid=is_array($assured)&&is_array($prepared)&&hash_equals((string)$assured['action_hash'],(string)$prepared['action_hash'])&&hrm_ess_self_change_route_consume_confirm_csrf($assured,$confirm,$now);
            if($operation==='approve'){$digest=$valid?hrm_ess_self_change_review_approval_digest((int)$prepared['request_id']):false;$ok=$digest!==false&&hrm_ess_self_change_route_run_governed($prepared,'',function()use($prepared,$digest){return approve_hrm_ess_self_profile_change((int)$prepared['request_id'],$digest);})!==false;}
            else{$ok=$valid&&hrm_ess_self_change_route_run_governed($prepared,'',function()use($prepared){return reject_hrm_ess_self_profile_change((int)$prepared['request_id'],'checker_rejected_review');})!==false;}
            if($ok){hrm_ess_self_change_route_reset_attempts();$ok_message=$operation==='approve'?_('The request was approved for a distinct executor.'):_('The request was rejected without applying profile state.');}
            else $message=_('The reauthenticated review confirmation failed closed. No partial request state was retained.');
        } else $message=_('The requested review operation is not allowed.');
    }
}

page(_($help_context='Review Profile Changes'));echo '<main class="center" style="max-width:900px;margin:0 auto;text-align:left" aria-labelledby="ess-review-title">';
if(!$authorized){display_error(_('Independent checker authority is required.'));echo '</main>';end_page();exit;}if(!$ready){display_error(_('Software Upgrade must be completed before this checker route is available.'));echo '</main>';end_page();exit;}if(!$secure){display_error(_('This checker route requires HTTPS.'));echo '</main>';end_page();exit;}if(!hrm_ess_self_change_route_supported_auth_method()){display_error(_('This session must use local-password or approved federated OIDC authentication.'));echo '</main>';end_page();exit;}
if($message!=='')display_error($message);if($ok_message!=='')display_notification($ok_message);if(($_GET['step_up']??'')==='failed')display_error(_('Independent reauthentication failed or was cancelled. No review action was applied.'));if(($_GET['step_up']??'')==='retry')display_warning(_('Independent reauthentication must be restarted. No review action was applied.'));
echo '<h2 id="ess-review-title">'._('Review Profile Changes').'</h2><p>'._('This route is checker-only. It may inspect only the bounded encrypted contact/private request after server-side object authorization. Approval never changes profile state. Bank, tax, identifier, compensation, payroll and accounting authority are excluded.').'</p>';
$assured=hrm_ess_self_change_route_assured(null,$now);$selected=is_array($assured)&&in_array($assured['operation'],array('approve','reject'),true)?(int)$assured['request_id']:ess_review_id($_GET['request_id']??'');
$candidates=hrm_ess_self_change_review_candidates('checker',50);if($candidates===false)display_error(_('Review candidates could not be resolved safely.'));else{
    echo '<section aria-labelledby="ess-review-queue"><h3 id="ess-review-queue">'._('Pending review queue').'</h3>';
    if(!$candidates)echo '<p>'._('No eligible bounded self-profile requests require checker review.').'</p>';else{echo '<ul>';foreach($candidates as $c)echo '<li><a href="?request_id='.(int)$c['request_id'].'">'.sprintf(_('Request #%d — %s'),(int)$c['request_id'],htmlspecialchars((string)$c['request_status'],ENT_QUOTES,'UTF-8')).'</a> — '.htmlspecialchars(implode(', ',$c['field_names']),ENT_QUOTES,'UTF-8').'</li>';echo '</ul>';}echo '</section>';
}
if($selected!==false&&$selected>0){$detail=hrm_ess_self_change_review_detail('checker',$selected);if(!is_array($detail))display_error(_('The selected request is not available to this checker.'));else{
    echo '<section aria-labelledby="ess-review-detail"><h3 id="ess-review-detail">'.sprintf(_('Review request #%d'),(int)$detail['request_id']).'</h3>';hrm_ess_self_change_review_render_values($detail);
    echo '<p aria-live="polite">'._('The maker cannot approve this request. Approval only releases it to a distinct executor; rejection is terminal and applies no profile values.').'</p>';
    $method=hrm_ess_self_change_route_auth_method();$confirming=is_array($assured)&&(int)$assured['request_id']===(int)$detail['request_id'];
    foreach(array('approve'=>_('Approve Request'),'reject'=>_('Reject Request')) as $op=>$label){if($op==='approve'&&$detail['request_status']!=='draft')continue;
        if($confirming&&$assured['operation']!==$op)continue;start_form();hidden('_token',ensure_csrf_token());hidden('request_id',(int)$detail['request_id']);
        if($confirming){hidden('route_action','confirm_'.$op);hidden('confirm_csrf',hrm_ess_self_change_route_issue_confirm_csrf($assured,$now));submit_center('confirm_'.$op,sprintf(_('Confirm %s'),$label));}
        elseif($method==='local_password'){hidden('route_action',$op.'_request');$pid='ess-review-password-'.$op;echo '<label for="'.$pid.'">'._('Current password:').'</label> <input id="'.$pid.'" type="password" name="current_password" maxlength="512" autocomplete="current-password" required>';submit_center($op.'_request',$label);}
        else{hidden('route_action','start_'.$op.'_step_up');submit_center('reauth_'.$op,sprintf(_('Reauthenticate to %s'),$label));}end_form();
    }echo '</section>';
}}
echo '</main>';end_page();
