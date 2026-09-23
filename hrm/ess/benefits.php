<?php
$page_security = 'SA_HRM_ESS_VIEW_SELF';
$path_to_root = '../..';
include($path_to_root.'/includes/session.inc');
include_once(__DIR__.'/_common.inc');
page(_('My Benefits'));
$context = hrm_ess_002_require_self();
if (!$context) {
    display_error(_('Self scope unavailable.'));
    end_page();
    exit;
}
$message = null;
$is_error = false;
if (isset($_POST['ess2_benefit'])) {
    if (!hrm_ess_002_route_post_valid()) {
        $message = _('The election was rejected by CSRF protection.');
        $is_error = true;
    } else {
        $error = null;
        $plan_id = isset($_POST['plan_id']) ? (int)$_POST['plan_id'] : 0;
        $window_id = isset($_POST['window_id']) ? (int)$_POST['window_id'] : 0;
        $state = isset($_POST['election_state']) ? $_POST['election_state'] : '';
        $basis = isset($_POST['qualifying_basis_code']) ? $_POST['qualifying_basis_code'] : '';
        $evidence = isset($_POST['election_evidence_sha256']) ? $_POST['election_evidence_sha256'] : '';
        $nonce = isset($_POST['request_nonce']) ? $_POST['request_nonce'] : '';
        $result = hrm_ess_002_submit_benefit($plan_id, $window_id, $state, $basis, $evidence, $nonce, $error);
        $message = $result === false ? hrm_ess_002_safe_error($error) : _('Benefit election recorded by HRM-BEN-001 authority.');
        $is_error = $result === false;
    }
}
$rows = hrm_ess_002_self_benefits();
$available_plans = array();
$available_windows = array();
$seen_windows = array();
if (is_array($rows)) {
    foreach ($rows as $row) {
        $plan_id = (int)$row['plan_id'];
        $windows = hrm_ess_002_self_benefit_windows($plan_id, $context['as_of'], (int)$row['plan_version_id']);
        if (!is_array($windows))
            continue;
        $available_plans[$plan_id] = $row;
        foreach ($windows as $window) {
            $window_id = (int)$window['window_id'];
            if (isset($seen_windows[$window_id]))
                continue;
            $seen_windows[$window_id] = true;
            $window['plan_id'] = $plan_id;
            $window['plan_code'] = $row['plan_code'];
            $available_windows[] = $window;
        }
    }
}
hrm_ess_002_emit_shell_start(_('My Benefits'), 'benefits');
if ($message !== null)
    hrm_ess_002_ui_status($message, $is_error);
hrm_ess_002_ui_table(_('Eligible benefits'), array(_('Plan'), _('Version'), _('Coverage basis'), _('Election'), _('Effective from')), is_array($rows) ? $rows : array(), array('plan_code', 'version_token', 'coverage_basis_code', 'enrollment_state', 'effective_from'));
if (hrm_ess_002_access('SA_HRM_ESS_ENROLL_BENEFITS') && hrm_ess_002_access('SA_HRM_ENROLL_BENEFITS') && $available_windows) {
    $nonce = hrm_ess_002_issue_action_nonce('benefit');
    echo '<h2>'._('Record benefit election').'</h2><form class="ess2-form" method="post">';
    echo '<input type="hidden" name="_token" value="'.hrm_ess_002_h(ensure_csrf_token()).'">';
    echo '<input type="hidden" name="request_nonce" value="'.hrm_ess_002_h($nonce).'">';
    echo '<label>'._('Eligible plan').'<select id="ess2-plan" name="plan_id" required>';
    foreach ($available_plans as $plan_id => $plan)
        echo '<option value="'.(int)$plan_id.'">'.hrm_ess_002_h($plan['plan_code'].' — '.$plan['version_token']).'</option>';
    echo '</select></label><label>'._('Enrollment window').'<select id="ess2-window" name="window_id" required>';
    foreach ($available_windows as $window)
        echo '<option value="'.(int)$window['window_id'].'" data-plan="'.(int)$window['plan_id'].'" data-basis="'.hrm_ess_002_h($window['qualifying_basis_code']).'">'.hrm_ess_002_h($window['plan_code'].' — '.$window['window_code']).'</option>';
    echo '</select></label><label>'._('Election').'<select name="election_state"><option value="enrolled">'._('Enroll').'</option><option value="waived">'._('Waive').'</option></select></label>';
    echo '<label>'._('Qualifying basis code').'<select id="ess2-basis" name="qualifying_basis_code" required>';
    foreach ($available_windows as $window)
        echo '<option value="'.hrm_ess_002_h($window['qualifying_basis_code']).'" data-window="'.(int)$window['window_id'].'">'.hrm_ess_002_h($window['qualifying_basis_code']).'</option>';
    echo '</select></label><label>'._('Election evidence SHA-256').'<input name="election_evidence_sha256" maxlength="64" pattern="[a-fA-F0-9]{64}" required></label>';
    echo '<button name="ess2_benefit" value="1">'._('Record governed election').'</button></form>';
    echo '<script>(function(){var p=document.getElementById("ess2-plan"),w=document.getElementById("ess2-window"),b=document.getElementById("ess2-basis");function sync(){var first=null;for(var i=0;i<w.options.length;i++){var o=w.options[i],show=o.getAttribute("data-plan")===p.value;o.hidden=!show;if(show&&first===null)first=o;}if(first&&(!w.options[w.selectedIndex]||w.options[w.selectedIndex].hidden))first.selected=true;var wid=w.value,bfirst=null;for(var j=0;j<b.options.length;j++){var x=b.options[j],ok=x.getAttribute("data-window")===wid;x.hidden=!ok;if(ok&&bfirst===null)bfirst=x;}if(bfirst&&(!b.options[b.selectedIndex]||b.options[b.selectedIndex].hidden))bfirst.selected=true;}p.addEventListener("change",sync);w.addEventListener("change",sync);sync();}());</script>';
}
hrm_ess_002_emit_shell_end();
end_page();
?>
