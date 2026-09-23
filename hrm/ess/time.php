<?php
$page_security = 'SA_HRM_ESS_VIEW_SELF';
$path_to_root = '../..';
include($path_to_root.'/includes/session.inc');
include_once(__DIR__.'/_common.inc');
page(_('My Time'));
$context = hrm_ess_002_require_self();
if (!$context) {
    display_error(_('Self scope unavailable.'));
    end_page();
    exit;
}
$message = null;
$is_error = false;
if (isset($_POST['ess2_overtime'])) {
    if (!hrm_ess_002_route_post_valid()) {
        $message = _('The request was rejected by CSRF protection.');
        $is_error = true;
    } else {
        $error = null;
        $request_id = isset($_POST['overtime_id']) ? (int)$_POST['overtime_id'] : 0;
        $work_date = isset($_POST['work_date']) ? $_POST['work_date'] : '';
        $hours = input_num('hours');
        $reason = isset($_POST['reason']) ? $_POST['reason'] : '';
        $nonce = isset($_POST['request_nonce']) ? $_POST['request_nonce'] : '';
        $result = hrm_ess_002_submit_overtime($request_id, $work_date, $hours, $reason, $nonce, $error);
        $message = $result === false ? hrm_ess_002_safe_error($error) : _('Overtime request submitted through the governed workflow.');
        $is_error = $result === false;
    }
}
$time_rows = hrm_ess_002_self_time();
$request_rows = hrm_ess_002_self_overtime_requests();
hrm_ess_002_emit_shell_start(_('My Time'), 'time');
if ($message !== null)
    hrm_ess_002_ui_status($message, $is_error);
hrm_ess_002_ui_table(_('Reviewed and approved time'), array(_('Date'), _('Minutes'), _('Status')), is_array($time_rows) ? $time_rows : array(), array('work_date', 'duration_minutes', 'status_token'));
hrm_ess_002_ui_table(_('Overtime requests'), array(_('Date'), _('Hours'), _('Status')), is_array($request_rows) ? $request_rows : array(), array('date', 'hours', 'status'));
if (hrm_ess_002_access('SA_HRM_ESS_REQUEST_TIME')) {
    $nonce = hrm_ess_002_issue_action_nonce('overtime');
    $types = get_overtime_types(false);
    echo '<h2>'._('Request overtime').'</h2><form class="ess2-form" method="post">';
    echo '<input type="hidden" name="_token" value="'.hrm_ess_002_h(ensure_csrf_token()).'">';
    echo '<input type="hidden" name="request_nonce" value="'.hrm_ess_002_h($nonce).'">';
    echo '<label>'._('Overtime type').'<select name="overtime_id" required><option value="">'._('Choose a type').'</option>';
    if ($types)
        while ($type = db_fetch_assoc($types))
            echo '<option value="'.(int)$type['overtime_id'].'">'.hrm_ess_002_h($type['overtime_name']).'</option>';
    echo '</select></label><label>'._('Work date').'<input type="date" name="work_date" required></label>';
    echo '<label>'._('Hours').'<input name="hours" inputmode="decimal" required></label>';
    echo '<label>'._('Reason').'<textarea name="reason" maxlength="255" required></textarea></label>';
    echo '<button name="ess2_overtime" value="1">'._('Submit overtime request').'</button></form>';
}
hrm_ess_002_emit_shell_end();
end_page();
?>
