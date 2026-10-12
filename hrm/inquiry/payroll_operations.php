<?php
/** Authenticated minimum-data payroll operations page; no business write, drilldown or export. */
$page_security='SA_HRMREPORTS';$path_to_root='../..';
include($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/pay_glb_001_native_reader_db.inc');
header('Cache-Control: no-store, private, max-age=0');header('Pragma: no-cache');
header('X-Content-Type-Options: nosniff');header('Referrer-Policy: no-referrer');
page(_('Payroll Operations'));
echo '<section aria-labelledby="external-payroll-heading"><h2 id="external-payroll-heading">'
    .htmlspecialchars(_('External payroll'),ENT_QUOTES,'UTF-8').'</h2>';
echo '<p data-payroll-lane="PARTNER" data-lane-status="disabled">'
    .htmlspecialchars(_('Partner payroll: Disabled. Unconfigured.'),ENT_QUOTES,'UTF-8').'</p>';
echo '<p data-payroll-lane="MANAGED" data-lane-status="disabled">'
    .htmlspecialchars(_('Managed payroll: Disabled. Unconfigured.'),ENT_QUOTES,'UTF-8').'</p></section>';
$cursor=isset($_GET['cursor'])?$_GET['cursor']:'';$error=null;$result=false;
if(count(array_diff(array_keys($_GET),array('cursor')))===0 && isset($_SERVER['REQUEST_METHOD']) && $_SERVER['REQUEST_METHOD']==='GET'
    && is_string($cursor) && strlen($cursor)<=2048 && version_compare((string)get_company_pref('version_id',true),'1.0.1079','>='))
    $result=pay_glb_001_native_read_current($cursor,$error);
if($result===false){
    error_log('S39 native aggregate unavailable: '.substr(preg_replace('/[^A-Za-z0-9_.-]/','_',($error===null?'request_denied':$error)),0,160));
    display_notification(_('Payroll operations are unavailable for this request.'));
}
elseif(count($result['view']['rows'])===0)display_note(_('No admitted payroll observations are available.'));
else {
    $html=pay_glb_001_native_calendar_render($result['view']);
    if($html===false)display_notification(_('Payroll operations are unavailable for this request.'));
    else {echo $html;display_note(_('Observed at UTC: ').htmlspecialchars($result['view']['observed_at_utc'],ENT_QUOTES,'UTF-8'));
        display_note(_('Statutory deadlines and external provider status are unavailable.'));
        display_note(_('Operational calendar dates have no verified cutoff time or statutory authority.'));
        if($result['next_cursor']!=='')echo '<p><a href="payroll_operations.php?cursor='
            .htmlspecialchars(rawurlencode($result['next_cursor']),ENT_QUOTES,'UTF-8').'">'.htmlspecialchars(_('Next page'),ENT_QUOTES,'UTF-8').'</a></p>';
    }
}
end_page();
