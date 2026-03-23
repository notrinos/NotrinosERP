<?php
/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	Phase 6: Cron Runner — Escalation & Email Processing

	This page can be called from a cron job or manually by an admin.
	It processes:
	1. Overdue approval escalations (escalation_days thresholds)
	2. Unsent email notifications

	Cron usage:
		php /path/to/NotrinosERP-1.0/admin/approval_cron.php
	Or via HTTP (requires admin session):
		http://your-server/NotrinosERP-1.0/admin/approval_cron.php

	Manual admin usage:
		Navigate to the page via Approval Workflow > Process Escalations

	PHP 5.6+ compatible.
***********************************************************************/
$page_security = 'SA_APPROVALRULES';
$path_to_root = '..';

// Detect CLI mode
$is_cli = (php_sapi_name() === 'cli');

if (!$is_cli) {
	include_once($path_to_root . '/includes/session.inc');
	page(_($help_context = 'Approval Escalation & Notification Processing'));
	include_once($path_to_root . '/includes/ui.inc');
} else {
	// CLI bootstrap — minimal session setup
	$_SERVER['HTTP_HOST'] = 'localhost';
	$_SERVER['REMOTE_ADDR'] = '127.0.0.1';

	// Load required files
	if (!defined('FA_DIR')) {
		define('FA_DIR', realpath(dirname(__FILE__) . '/..'));
	}
	chdir(FA_DIR);
	$path_to_root = '.';

	include_once($path_to_root . '/includes/current_user.inc');
	include_once($path_to_root . '/config_db.php');
	include_once($path_to_root . '/includes/main.inc');

	// Use the first company (index 0)
	$company = 0;
	set_global_connection($company);
}

include_once($path_to_root . '/includes/approval/approval_escalation.inc');
include_once($path_to_root . '/includes/approval/approval_notify.inc');

// =====================================================
// Process Escalations
// =====================================================

$run_escalation = $is_cli || isset($_POST['RunEscalation']) || isset($_GET['auto']);
$run_emails = $is_cli || isset($_POST['RunEmails']) || isset($_GET['auto']);

$escalation_summary = null;
$email_count = 0;

if ($run_escalation) {
	$escalation_summary = process_approval_escalations();
}

if ($run_emails) {
	$email_count = process_approval_email_notifications(100);
}

// =====================================================
// CLI Output
// =====================================================

if ($is_cli) {
	echo "=== Approval Workflow Cron ===\n";
	echo date('Y-m-d H:i:s') . "\n\n";

	if ($escalation_summary) {
		echo "Escalation Results:\n";
		echo "  Processed: " . $escalation_summary['processed'] . "\n";
		echo "  Escalated: " . $escalation_summary['escalated'] . "\n";
		echo "  Reminded:  " . $escalation_summary['reminded'] . "\n";
		echo "  Skipped:   " . $escalation_summary['skipped'] . "\n";

		foreach ($escalation_summary['details'] as $detail) {
			echo "  - " . $detail['message'] . "\n";
		}
	}

	echo "\nEmail Results:\n";
	echo "  Sent: " . $email_count . "\n";

	exit(0);
}

// =====================================================
// Web UI Output
// =====================================================

start_form();

echo '<div style="max-width:800px; margin:0 auto;">';

// Page description
display_heading(_('Approval Escalation & Notification Processing'));
echo '<br>';
echo '<p>' . _('This page processes overdue approval escalations and sends pending email notifications. '
	. 'For automatic processing, configure a cron job to call this page regularly.') . '</p>';

// Cron setup instructions
echo '<div style="background:#f0f8ff; border:1px solid #b0c4de; padding:10px; margin:10px 0; border-radius:4px;">';
echo '<strong>' . _('Cron Setup') . ':</strong><br>';
echo '<code style="font-size:12px;">*/30 * * * * php ' . realpath(__FILE__)
	. '</code><br>';
echo '<em>' . _('Runs every 30 minutes (adjust as needed).') . '</em>';
echo '</div>';

// Action buttons
echo '<table class="tablestyle_noborder" style="margin:10px 0;">';
echo '<tr>';
echo '<td>';
submit('RunEscalation', _('Process Escalations'), true, '', 'default');
echo '</td>';
echo '<td>';
submit('RunEmails', _('Send Pending Emails'), true, '', 'default');
echo '</td>';
echo '<td>';
submit('RunBoth', _('Process All'), true, '', 'process');
echo '</td>';
echo '</tr>';
echo '</table>';

// Handle "Process All"
if (isset($_POST['RunBoth'])) {
	if ($escalation_summary === null) {
		$escalation_summary = process_approval_escalations();
	}
	if ($email_count === 0) {
		$email_count = process_approval_email_notifications(100);
	}
}

// Results display
if ($escalation_summary !== null) {
	echo '<br>';
	display_heading2(_('Escalation Results'));

	start_table(TABLESTYLE, "width='100%'");
	$th = array(_('Metric'), _('Count'));
	table_header($th);

	label_row(_('Drafts Processed'), $escalation_summary['processed']);
	label_row(_('Escalated to Next Level'), $escalation_summary['escalated']);
	label_row(_('Reminders Sent (At Max Level)'), $escalation_summary['reminded']);
	label_row(_('Skipped (Recently Escalated)'), $escalation_summary['skipped']);

	end_table();

	if (!empty($escalation_summary['details'])) {
		echo '<br>';
		display_heading2(_('Escalation Details'));

		start_table(TABLESTYLE, "width='100%'");
		$th = array(_('Status'), _('Message'));
		table_header($th);

		foreach ($escalation_summary['details'] as $detail) {
			$status_color = '';
			switch ($detail['status']) {
				case 'escalated':
					$status_color = 'color:#d35400;font-weight:bold;';
					break;
				case 'reminded':
					$status_color = 'color:#2980b9;';
					break;
				case 'skipped':
					$status_color = 'color:#7f8c8d;';
					break;
			}

			start_row();
			label_cell('<span style="' . $status_color . '">' . htmlspecialchars($detail['status']) . '</span>');
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
	$th = array(_('Metric'), _('Count'));
	table_header($th);

	label_row(_('Emails Sent'), $email_count);

	end_table();
}

// Escalation Risk Dashboard
echo '<br>';
display_heading2(_('Current Escalation Risk'));

$risk_items = get_escalation_risk_summary();

if (empty($risk_items)) {
	display_note(_('No pending drafts with escalation rules configured.'));
} else {
	start_table(TABLESTYLE, "width='100%'");
	$th = array(_('Reference'), _('Workflow'), _('Level'), _('Days at Level'),
		_('Escalation After'), _('Days Remaining'), _('Status'), _('Amount'));
	table_header($th);

	foreach ($risk_items as $item) {
		start_row();
		label_cell(htmlspecialchars($item['reference']));
		label_cell(htmlspecialchars($item['workflow_name']));
		label_cell($item['current_level'], "align='center'");
		label_cell($item['days_at_level'], "align='center'");
		label_cell($item['escalation_days'] . ' ' . _('days'), "align='center'");

		if ($item['is_overdue']) {
			label_cell('<span style="color:#e74c3c; font-weight:bold;">'
				. _('OVERDUE') . '</span>', "align='center'");
			label_cell('<span style="color:#e74c3c; font-weight:bold;">'
				. _('Overdue') . '</span>', "align='center'");
		} else {
			label_cell($item['days_remaining'] . ' ' . _('days'), "align='center'");
			if ($item['days_remaining'] <= 1) {
				label_cell('<span style="color:#f39c12; font-weight:bold;">'
					. _('At Risk') . '</span>', "align='center'");
			} else {
				label_cell('<span style="color:#27ae60;">'
					. _('Normal') . '</span>', "align='center'");
			}
		}

		amount_cell($item['amount']);
		end_row();
	}

	end_table();
}

echo '</div>';

end_form();

end_page();
