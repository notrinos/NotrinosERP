<?php
/**********************************************************************
    NotrinosERP-1.0 Approval Workflow System
    Approval escalation and durable notification queue runner.

    CLI usage (company and environment token are mandatory):
        NOTRINOS_APPROVAL_CRON_TOKEN='...' php admin/approval_cron.php --company=0

    Web usage requires the existing SA_APPROVALRULES session permission.
    PHP 5.6+ compatible.
***********************************************************************/
$page_security = 'SA_APPROVALRULES';
$path_to_root = '..';
$is_cli = (php_sapi_name() === 'cli');
$company = null;

if (!$is_cli) {
    include_once($path_to_root . '/includes/session.inc');
    page(_($help_context = 'Approval Escalation & Notification Processing'));
    include_once($path_to_root . '/includes/ui.inc');
} else {
    $_SERVER['HTTP_HOST'] = 'localhost';
    $_SERVER['REMOTE_ADDR'] = '127.0.0.1';

    if (!defined('FA_DIR'))
        define('FA_DIR', realpath(dirname(__FILE__) . '/..'));
    chdir(FA_DIR);
    $path_to_root = '.';

    include_once($path_to_root . '/includes/approval/approval_cron_auth.inc');
    include_once($path_to_root . '/config_db.php');

    $parsed_company = parse_approval_cron_company_argument($argv);
    if ($parsed_company['status'] !== 'parsed') {
        fwrite(STDERR, "Approval cron denied: company selection is required.\n");
        exit(2);
    }

    $company = (int)$parsed_company['company'];
    $provided_token = getenv('NOTRINOS_APPROVAL_CRON_TOKEN');
    $authorization = authorize_approval_cron_cli(
        $company,
        $provided_token === false ? '' : $provided_token,
        $db_connections
    );
    if ($authorization['status'] !== 'authorized') {
        fwrite(STDERR, "Approval cron denied: company-scoped credential rejected.\n");
        exit(2);
    }

    include_once($path_to_root . '/includes/current_user.inc');
    include_once($path_to_root . '/includes/main.inc');
    if (!set_global_connection($company)) {
        fwrite(STDERR, "Approval cron failed: company database unavailable.\n");
        exit(2);
    }
}

include_once($path_to_root . '/includes/approval/approval_escalation.inc');
include_once($path_to_root . '/includes/approval/approval_notify.inc');

$run_escalation = $is_cli || isset($_POST['RunEscalation']) || isset($_GET['auto']);
$run_emails = $is_cli || isset($_POST['RunEmails']) || isset($_GET['auto']);
$escalation_summary = null;
$email_summary = null;
$repair_result = null;

if (!$is_cli && isset($_POST['RepairNotification'])) {
    $repair_id = (int)get_post('RepairNotificationId');
    $operator_user_id = isset($_SESSION['wa_current_user'])
        ? (int)$_SESSION['wa_current_user']->user : 0;
    $repair_result = $repair_id > 0 && $operator_user_id > 0
        ? repair_approval_notification_delivery($repair_id, $operator_user_id) : false;
}

if ($run_escalation)
    $escalation_summary = process_approval_escalations();
if ($run_emails)
    $email_summary = process_approval_email_notifications_detailed(100);

if ($is_cli) {
    echo "=== Approval Workflow Cron ===\n";
    echo "Company: " . $company . "\n";
    echo date('Y-m-d H:i:s') . "\n\n";

    if ($escalation_summary) {
        echo "Escalation Results:\n";
        echo "  Processed: " . $escalation_summary['processed'] . "\n";
        echo "  Escalated: " . $escalation_summary['escalated'] . "\n";
        echo "  Reminded:  " . $escalation_summary['reminded'] . "\n";
        echo "  Skipped:   " . $escalation_summary['skipped'] . "\n";
        foreach ($escalation_summary['details'] as $detail)
            echo "  - " . $detail['message'] . "\n";
    }

    echo "\nEmail Results:\n";
    if ($email_summary !== null) {
        echo "  Claimed: " . $email_summary['claimed'] . "\n";
        echo "  Sent: " . $email_summary['sent'] . "\n";
        echo "  Retry Scheduled: " . $email_summary['retry_scheduled'] . "\n";
        echo "  Dead-Lettered: " . $email_summary['dead_lettered'] . "\n";
        echo "  Delivery Failed: " . $email_summary['delivery_failed'] . "\n";
        echo "  Missing Email: " . $email_summary['missing_email'] . "\n";
        echo "  Invalid Email: " . $email_summary['invalid_email'] . "\n";
        echo "  Missing Sender: " . $email_summary['missing_sender'] . "\n";
        echo "  Invalid Sender: " . $email_summary['invalid_sender'] . "\n";
        echo "  Invalid BCC: " . $email_summary['invalid_bcc'] . "\n";
        echo "  Transport Unavailable: " . $email_summary['transport_unavailable'] . "\n";
        echo "  Transport Executable Unavailable: " . $email_summary['transport_executable_unavailable'] . "\n";
        echo "  Transport Encoding Unavailable: " . $email_summary['transport_encoding_unavailable'] . "\n";
        echo "  Transport Argument Escape Unavailable: " . $email_summary['transport_arg_escape_unavailable'] . "\n";
        echo "  Transport Forced Params Override: " . $email_summary['transport_forced_params_override'] . "\n";
        echo "  Transport SMTP Config Invalid: " . $email_summary['transport_smtp_config_invalid'] . "\n";
        echo "  Legacy Transport SMTP Connect Failed: " . $email_summary['transport_smtp_connect_failed'] . "\n";
        echo "  Transport SMTP Session Start Failed: " . $email_summary['transport_smtp_start_failed'] . "\n";
        echo "  Invalid Notification Rows: " . $email_summary['invalid_notification'] . "\n";
        echo "  State Transition Failed: " . $email_summary['state_failed'] . "\n";
        echo "  Claim Failed: " . $email_summary['claim_failed'] . "\n";
    }

    $queue_counts = get_approval_notification_delivery_status_counts();
    echo "\nQueue State:\n";
    echo "  Ready: " . $queue_counts['ready'] . "\n";
    echo "  Leased: " . $queue_counts['leased'] . "\n";
    echo "  Deferred: " . $queue_counts['deferred'] . "\n";
    echo "  Dead-Lettered: " . $queue_counts['dead_lettered'] . "\n";
    echo "  Sent: " . $queue_counts['sent'] . "\n";

    exit($email_summary !== null
        && ($email_summary['state_failed'] > 0 || $email_summary['claim_failed'] > 0)
        ? 3 : 0);
}

start_form();
echo '<div style="max-width:800px; margin:0 auto;">';
display_heading(_('Approval Escalation & Notification Processing'));
echo '<br>';
echo '<p>' . _('This page processes overdue approval escalations and the durable approval email queue.') . '</p>';

echo '<div style="background:#f0f8ff; border:1px solid #b0c4de; padding:10px; margin:10px 0; border-radius:4px;">';
echo '<strong>' . _('Cron Setup') . ':</strong><br>';
echo '<code style="font-size:12px;">NOTRINOS_APPROVAL_CRON_TOKEN=... php '
    . realpath(__FILE__) . ' --company=&lt;id&gt;</code><br>';
echo '<em>' . _('Configure approval_cron_token_hash for that company in config_db.php. CLI execution has no default company and fails closed without the matching environment token.') . '</em>';
echo '</div>';

if ($repair_result !== null) {
    if ($repair_result)
        display_notification(_('Dead-lettered notification was requeued for a new bounded attempt window.'));
    else
        display_error(_('Notification could not be requeued. Confirm the ID is dead-lettered and still unsent.'));
}

echo '<table class="tablestyle_noborder" style="margin:10px 0;">';
echo '<tr><td>';
submit('RunEscalation', _('Process Escalations'), true, '', 'default');
echo '</td><td>';
submit('RunEmails', _('Send Pending Emails'), true, '', 'default');
echo '</td><td>';
submit('RunBoth', _('Process All'), true, '', 'process');
echo '</td></tr></table>';

if (isset($_POST['RunBoth'])) {
    if ($escalation_summary === null)
        $escalation_summary = process_approval_escalations();
    if ($email_summary === null)
        $email_summary = process_approval_email_notifications_detailed(100);
}

if ($escalation_summary !== null) {
    echo '<br>';
    display_heading2(_('Escalation Results'));
    start_table(TABLESTYLE, "width='100%'");
    table_header(array(_('Metric'), _('Count')));
    label_row(_('Drafts Processed'), $escalation_summary['processed']);
    label_row(_('Escalated to Next Level'), $escalation_summary['escalated']);
    label_row(_('Reminders Sent (At Max Level)'), $escalation_summary['reminded']);
    label_row(_('Skipped (Recently Escalated)'), $escalation_summary['skipped']);
    end_table();

    if (!empty($escalation_summary['details'])) {
        echo '<br>';
        display_heading2(_('Escalation Details'));
        start_table(TABLESTYLE, "width='100%'");
        table_header(array(_('Status'), _('Message')));
        foreach ($escalation_summary['details'] as $detail) {
            start_row();
            label_cell(htmlspecialchars($detail['status']));
            label_cell(htmlspecialchars($detail['message']));
            end_row();
        }
        end_table();
    }
}

if ($run_emails || isset($_POST['RunBoth'])) {
    echo '<br>';
    display_heading2(_('Email Notification Results'));
    start_table(TABLESTYLE, "width='100%'");
    table_header(array(_('Metric'), _('Count')));
    if ($email_summary !== null) {
        label_row(_('Notifications Claimed'), $email_summary['claimed']);
        label_row(_('Emails Sent'), $email_summary['sent']);
        label_row(_('Retries Scheduled'), $email_summary['retry_scheduled']);
        label_row(_('Dead-Lettered'), $email_summary['dead_lettered']);
        label_row(_('Delivery Failed'), $email_summary['delivery_failed']);
        label_row(_('Missing Email'), $email_summary['missing_email']);
        label_row(_('Invalid Email'), $email_summary['invalid_email']);
        label_row(_('Missing Sender'), $email_summary['missing_sender']);
        label_row(_('Invalid Sender'), $email_summary['invalid_sender']);
        label_row(_('Invalid BCC'), $email_summary['invalid_bcc']);
        label_row(_('Transport Unavailable'), $email_summary['transport_unavailable']);
        label_row(_('Transport Executable Unavailable'), $email_summary['transport_executable_unavailable']);
        label_row(_('Transport Encoding Unavailable'), $email_summary['transport_encoding_unavailable']);
        label_row(_('Transport Argument Escape Unavailable'), $email_summary['transport_arg_escape_unavailable']);
        label_row(_('Transport Forced Params Override'), $email_summary['transport_forced_params_override']);
        label_row(_('Transport Arguments Incompatible'), $email_summary['transport_args_incompatible']);
        label_row(_('Transport SMTP Config Invalid'), $email_summary['transport_smtp_config_invalid']);
        label_row(_('Legacy Transport SMTP Connect Failed'), $email_summary['transport_smtp_connect_failed']);
        label_row(_('Transport SMTP Session Start Failed'), $email_summary['transport_smtp_start_failed']);
        label_row(_('Invalid Notification Rows'), $email_summary['invalid_notification']);
        label_row(_('State Transition Failed'), $email_summary['state_failed']);
        label_row(_('Claim Failed'), $email_summary['claim_failed']);
    }
    end_table();
}

$queue_counts = get_approval_notification_delivery_status_counts();
echo '<br>';
display_heading2(_('Notification Queue State'));
start_table(TABLESTYLE, "width='100%'");
table_header(array(_('State'), _('Count')));
label_row(_('Ready'), $queue_counts['ready']);
label_row(_('Actively Leased'), $queue_counts['leased']);
label_row(_('Deferred by Backoff'), $queue_counts['deferred']);
label_row(_('Dead-Lettered'), $queue_counts['dead_lettered']);
label_row(_('Sent'), $queue_counts['sent']);
end_table();

$dead_letters = get_dead_lettered_approval_notifications(50);
if ($dead_letters && db_num_rows($dead_letters) > 0) {
    echo '<br>';
    display_heading2(_('Dead-Letter Repair'));
    start_table(TABLESTYLE, "width='100%'");
    table_header(array(_('ID'), _('Type'), _('Attempts'), _('Limit'), _('Repairs'), _('Error'), _('Last Attempt'), _('Dead-Lettered')));
    while ($dead = db_fetch($dead_letters)) {
        start_row();
        label_cell((int)$dead['id']);
        label_cell(htmlspecialchars($dead['notification_type']));
        label_cell((int)$dead['delivery_attempts']);
        label_cell((int)$dead['attempt_limit']);
        label_cell((int)$dead['repair_count']);
        label_cell(htmlspecialchars($dead['last_error_code']));
        label_cell(htmlspecialchars($dead['last_attempt_at']));
        label_cell(htmlspecialchars($dead['dead_lettered_at']));
        end_row();
    }
    end_table();
    echo '<br>';
    text_row(_('Notification ID to requeue'), 'RepairNotificationId', null, 12, 12);
    submit_center('RepairNotification', _('Requeue Dead-Letter'), true,
        _('Requeue only after the recipient or transport problem has been repaired.'), 'default');
}

echo '<br>';
display_heading2(_('Current Escalation Risk'));
$risk_items = get_escalation_risk_summary();
if (empty($risk_items)) {
    display_note(_('No pending drafts with escalation rules configured.'));
} else {
    start_table(TABLESTYLE, "width='100%'");
    table_header(array(_('Reference'), _('Workflow'), _('Level'), _('Days at Level'),
        _('Escalation After'), _('Days Remaining'), _('Status'), _('Amount')));
    foreach ($risk_items as $item) {
        start_row();
        label_cell(htmlspecialchars($item['reference']));
        label_cell(htmlspecialchars($item['workflow_name']));
        label_cell($item['current_level'], "align='center'");
        label_cell($item['days_at_level'], "align='center'");
        label_cell($item['escalation_days'] . ' ' . _('days'), "align='center'");
        if ($item['is_overdue']) {
            label_cell('<span style="color:#e74c3c;font-weight:bold;">' . _('OVERDUE') . '</span>', "align='center'");
            label_cell('<span style="color:#e74c3c;font-weight:bold;">' . _('Overdue') . '</span>', "align='center'");
        } else {
            label_cell($item['days_remaining'] . ' ' . _('days'), "align='center'");
            label_cell($item['days_remaining'] <= 1 ? _('At Risk') : _('Normal'), "align='center'");
        }
        amount_cell($item['amount']);
        end_row();
    }
    end_table();
}

echo '</div>';
end_form();
end_page();
