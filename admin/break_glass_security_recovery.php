<?php
/** PAY-SEC-004 authenticated scoped break-glass activation and return-to-normal route. */
$page_security = 'SA_CHGPASSWD';
$path_to_root = '..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/includes/break_glass_browser_route.inc');

break_glass_browser_route_security_headers();
page(_($help_context = 'Break-glass security recovery'));
$company=function_exists('user_company')?(int)user_company():-1;
$user_id=isset($_SESSION['wa_current_user']->user)?(int)$_SESSION['wa_current_user']->user:0;
$route_ready=$company>=0 && $user_id>0 && break_glass_browser_route_activation_ready();
if($route_ready && !break_glass_browser_route_load_dependencies()) $route_ready=false;
$now=time();
$context=false;
if($route_ready){
    $context=break_glass_security_recovery_consumer_context('SA_CHGPASSWD',$now);
    if(!is_array($context)) break_glass_end_activation($company,$user_id,'lease_expired',$now);
}

if(!$route_ready){
    display_error(_('Break-glass security recovery is not active for this company. Complete the required Software Upgrade before using this page.'));
} elseif(isset($_POST['ACTIVATE_BREAK_GLASS'])) {
    $token=isset($_POST['break_glass_csrf'])?(string)$_POST['break_glass_csrf']:'';
    $csrf_ok=check_csrf_token() && break_glass_browser_route_consume_csrf($company,$user_id,'activate',$token,$now);
    if(!$csrf_ok) display_error(_('Break-glass activation was not accepted. Reload this page and try again.'));
    elseif($SysPrefs->allow_demo_mode) display_warning(_('Break-glass activation is unavailable in demo mode.'));
    else {
        $password=isset($_POST['current_password'])?(string)$_POST['current_password']:'';
        $reason=isset($_POST['reason_code'])?(string)$_POST['reason_code']:'';
        $ticket=isset($_POST['ticket_reference'])?(string)$_POST['ticket_reference']:'';
        $seconds=isset($_POST['activation_seconds'])?(int)$_POST['activation_seconds']:0;
        $confirmed=isset($_POST['confirm_activation']) && (string)$_POST['confirm_activation']==='1';
        if(!$confirmed) display_error(_('Confirm that this is a scoped security-recovery break-glass activation.'));
        else {
            $result=break_glass_browser_route_activate_current_user($company,$user_id,$password,$reason,$ticket,$seconds,$now);
            if(!is_array($result)) display_error(_('Break-glass activation failed. Verify designation, local password, reason, ticket reference and requested duration.'));
            else display_notification(sprintf(_('Break-glass security recovery is active until %s UTC. Existing role permissions were not widened.'),(string)$result['expires_at']));
        }
    }
} elseif(isset($_POST['RETURN_TO_NORMAL'])) {
    $token=isset($_POST['break_glass_csrf'])?(string)$_POST['break_glass_csrf']:'';
    $csrf_ok=check_csrf_token() && break_glass_browser_route_consume_csrf($company,$user_id,'return_to_normal',$token,$now);
    if(!$csrf_ok) display_error(_('Return-to-normal request was not accepted. Reload this page and try again.'));
    elseif(!is_array(break_glass_end_activation($company,$user_id,'return_to_normal',$now)))
        display_error(_('Break-glass session could not be terminalized securely.'));
    else {
        kill_login();
        header('Location: ../index.php?break_glass=returned_to_normal',true,303);
        exit;
    }
}

if($route_ready){
    $context=break_glass_security_recovery_consumer_context('SA_CHGPASSWD',time());
    if(is_array($context)){
        display_warning(_('Break-glass mode is active only for existing security-recovery permissions. It does not grant payroll, payment or accounting authority.'));
        start_table(TABLESTYLE2);
        label_row(_('Reason:'), htmlspecialchars((string)$context['reason_code'],ENT_QUOTES,'UTF-8'));
        label_row(_('Expires at:'), htmlspecialchars((string)$context['expires_at'],ENT_QUOTES,'UTF-8').' UTC');
        label_row(_('Remaining seconds:'), (string)(int)$context['remaining_seconds']);
        end_table(1);
        $return_token=break_glass_browser_route_issue_csrf($company,$user_id,'return_to_normal',time());
        if(is_string($return_token)){
            start_form(false,'','break_glass_return_form');
            hidden('break_glass_csrf',$return_token);
            submit_center('RETURN_TO_NORMAL',_('Return to normal and sign out'),true,'','nonajax');
            end_form();
        }
    } else {
        display_note(_('Use only a pre-designated local account. Activation requires the current local password, a bounded reason, a ticket/reference, and a duration of no more than 900 seconds.'),0,1);
        $activate_token=break_glass_browser_route_issue_csrf($company,$user_id,'activate',time());
        if(is_string($activate_token)){
            start_form(false,'','break_glass_activate_form');
            start_table(TABLESTYLE2);
            table_section_title(_('Activate scoped security recovery'));
            password_row(_('Current local password:'),'current_password','');
            text_row(_('Ticket/reference:'),'ticket_reference','',40,256);
            array_selector_row(_('Reason:'),'reason_code',null,array('provider_outage'=>_('Provider outage'),'provider_degraded'=>_('Provider degraded'),'security_incident'=>_('Security incident'),'recovery_drill'=>_('Recovery drill')));
            text_row(_('Activation seconds:'),'activation_seconds','900',6,3);
            check_row(_('I understand this does not grant new role permissions.'),'confirm_activation',0,false);
            hidden('break_glass_csrf',$activate_token);
            end_table(1);
            submit_center('ACTIVATE_BREAK_GLASS',_('Activate break-glass security recovery'),true,'','nonajax');
            end_form();
        } else display_error(_('Break-glass action token could not be created.'));
    }
}
end_page();
