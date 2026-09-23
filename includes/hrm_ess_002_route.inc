<?php
/** Shared HRM-ESS-002 browser-route policy. */
include_once($path_to_root.'/hrm/includes/hrm_ess_002_contract.inc');
include_once($path_to_root.'/hrm/includes/db/hrm_ess_002_context_db.inc');
function hrm_ess_002_route_headers(){if(headers_sent())return;header('Cache-Control: private, no-store, max-age=0');header('Pragma: no-cache');header('Referrer-Policy: no-referrer');header('X-Content-Type-Options: nosniff');header('X-Frame-Options: DENY');}
function hrm_ess_002_route_post_valid(){return (isset($_SERVER['REQUEST_METHOD'])?$_SERVER['REQUEST_METHOD']:'')==='POST'&&function_exists('check_csrf_token')&&check_csrf_token();}
function hrm_ess_002_h($s){return htmlspecialchars((string)$s,ENT_QUOTES,'UTF-8');}
function hrm_ess_002_dir(){return isset($_SESSION['language']->dir)&&$_SESSION['language']->dir==='rtl'?'rtl':'ltr';}
function hrm_ess_002_require_self(){if(!hrm_ess_002_access('SA_HRM_ESS_VIEW_SELF'))return false;return get_hrm_ess_002_current_self_context();}
function hrm_ess_002_emit_shell_start($title,$active='dashboard'){hrm_ess_002_route_headers();echo '<a class="ess2-skip" href="#ess2-main">'._('Skip to main content').'</a><div class="ess2-shell" dir="'.hrm_ess_002_dir().'">';echo '<nav class="ess2-nav" aria-label="'._('Employee self service').'">';$links=array('dashboard'=>array(_('Home'),'dashboard.php','SA_HRM_ESS_VIEW_SELF'),'time'=>array(_('Time'),'time.php','SA_HRM_ESS_VIEW_SELF'),'leave'=>array(_('Leave'),'leave.php','SA_HRM_ESS_VIEW_SELF'),'benefits'=>array(_('Benefits'),'benefits.php','SA_HRM_ESS_VIEW_SELF'),'pay'=>array(_('Pay'),'pay.php','SA_HRM_ESS_VIEW_PAY'),'documents'=>array(_('Documents'),'documents.php','SA_HRM_ESS_VIEW_DOCUMENTS'),'inbox'=>array(_('Action inbox'),'action_inbox.php','SA_HRM_ESS_VIEW_SELF'),'team'=>array(_('My team'),'team.php','SA_HRM_ESS_VIEW_TEAM'),'proxy'=>array(_('Delegation'),'delegation.php','SA_HRM_ESS_MANAGE_PROXY'));foreach($links as $k=>$v){if(!hrm_ess_002_access($v[2]))continue;echo '<a '.($k===$active?'aria-current="page" ':'').'href="'.hrm_ess_002_h($v[1]).'">'.hrm_ess_002_h($v[0]).'</a>';}echo '</nav><main id="ess2-main" class="ess2-main" tabindex="-1"><h1>'.hrm_ess_002_h($title).'</h1>';}
function hrm_ess_002_emit_shell_end(){echo '</main></div>';}
?>
