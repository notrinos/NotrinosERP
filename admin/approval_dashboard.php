<?php
/**********************************************************************
	Copyright (C) NotrinosERP.
	Released under the terms of the GNU General Public License, GPL, 
	as published by the Free Software Foundation, either version 3 
	of the License, or (at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
	See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/

/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	Admin: Unified Approval Dashboard

	Displays a dashboard of pending approvals for the current user,
	with summary statistics, filterable lists, and quick approve/reject
	actions. Also shows the user's own submitted drafts.
***********************************************************************/
$page_security = 'SA_APPROVALDASHBOARD';
$path_to_root = '..';

include($path_to_root . '/includes/db_pager.inc');
include_once($path_to_root . '/includes/session.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 600);

page(_($help_context = 'Approval Dashboard'), false, false, '', $js);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/approval/db/approval_db.inc');
include_once($path_to_root . '/includes/approval/db/approval_rules_db.inc');
include_once($path_to_root . '/includes/approval/db/approval_history_db.inc');
include_once($path_to_root . '/includes/approval/ui/approval_timeline.inc');
include_once($path_to_root . '/includes/approval/ui/approval_badges.inc');
include_once($path_to_root . '/admin/db/approval_rules_setup_db.inc');
include_once($path_to_root . '/admin/db/approval_dashboard_db.inc');
include_once($path_to_root . '/includes/approval/approval_delegation.inc');
include_once($path_to_root . '/includes/approval/approval_escalation.inc');

// =====================================================
// Current user context
// =====================================================

$current_user_id = $_SESSION['wa_current_user']->user;
$current_role_id = $_SESSION['wa_current_user']->access;

// =====================================================
// Handle approval/rejection actions
// =====================================================

if (isset($_POST['QuickApprove'])) {
	$draft_id = (int)get_post('action_draft_id');
	$comments = get_post('action_comments', '');

	if ($draft_id > 0) {
		$approval_service = get_approval_workflow_service();
		$result = $approval_service->approve($draft_id, $comments);

		if ($result['status'] === 'error') {
			display_error($result['message']);
		} else {
			display_notification($result['message']);
		}
		$Ajax->activate('_page_body');
	}
}

if (isset($_POST['QuickReject'])) {
	$draft_id = (int)get_post('action_draft_id');
	$comments = get_post('action_comments', '');

	if ($draft_id > 0) {
		$approval_service = get_approval_workflow_service();
		$result = $approval_service->reject($draft_id, $comments);

		if ($result['status'] === 'error') {
			display_error($result['message']);
		} else {
			display_notification($result['message']);
		}
		$Ajax->activate('_page_body');
	}
}

if (isset($_POST['CancelDraft'])) {
	$draft_id = (int)get_post('action_draft_id');
	$comments = get_post('action_comments', '');

	if ($draft_id > 0) {
		$approval_service = get_approval_workflow_service();
		$result = $approval_service->cancel($draft_id, $comments);

		if ($result['status'] === 'error') {
			display_error($result['message']);
		} else {
			display_notification($result['message']);
		}
		$Ajax->activate('_page_body');
	}
}

if (isset($_POST['DelegateDraft'])) {
	$draft_id = (int)get_post('action_draft_id');
	$delegate_to = (int)get_post('delegate_to_user');
	$reason = get_post('action_comments', '');

	if ($draft_id > 0 && $delegate_to > 0) {
		$result = perform_draft_delegation($draft_id, $delegate_to, $reason);

		if ($result['status'] === 'error') {
			display_error($result['message']);
		} else {
			display_notification($result['message']);
		}
		$Ajax->activate('_page_body');
	} else {
		display_error(_('Please select a user to delegate to.'));
	}
}

// =====================================================
// Tab management
// =====================================================

if (!isset($_POST['dashboard_tab']))
	$_POST['dashboard_tab'] = 'pending';

if (isset($_POST['TabPending']))
	$_POST['dashboard_tab'] = 'pending';

if (isset($_POST['TabMySubmissions']))
	$_POST['dashboard_tab'] = 'my_submissions';

if (isset($_POST['TabActivity']))
	$_POST['dashboard_tab'] = 'activity';

$active_tab = get_post('dashboard_tab', 'pending');

// =====================================================
// db_pager column formatters
// =====================================================

/**
 * Format transaction type column in dashboard grid.
 *
 * @param array $row Grid row data
 * @return string    Formatted transaction type label
 */
function dashboard_trans_type_cell($row)
{
	return get_trans_type_label((int)$row['trans_type']);
}

/**
 * Format status column in dashboard grid.
 *
 * @param array $row Grid row data
 * @return string    Status badge HTML
 */
function dashboard_status_cell($row)
{
	return approval_status_cell((int)$row['status']);
}

/**
 * Format amount column in dashboard grid.
 *
 * @param array $row Grid row data
 * @return string    Formatted amount
 */
function dashboard_amount_cell($row)
{
	if ((float)$row['amount'] == 0)
		return '-';

	$formatted = number_format2($row['amount'], user_price_dec());

	return $formatted . ($row['currency'] ? ' ' . $row['currency'] : '');
}

/**
 * Format urgency indicator for pending items.
 *
 * @param array $row Grid row data
 * @return string    Urgency indicator HTML
 */
function dashboard_urgency_cell($row)
{
	return approval_urgency_indicator($row['submitted_date']);
}

/**
 * Generate view draft link for dashboard grid.
 *
 * @param array $row Grid row data
 * @return string    View link HTML
 */
function dashboard_view_link($row)
{
	return "<a href='approval_view_draft.php?draft_id=" . (int)$row['id'] . "' target='_blank' "
		. "onclick=\"javascript:openWindow(this.href,this.target); return false;\">"
		. default_theme_icon('search')
		. _('View') . "</a>";
}

/**
 * Generate approve/reject action buttons for pending items.
 *
 * @param array $row Grid row data
 * @return string    Action buttons HTML
 */
function dashboard_action_buttons($row)
{
	if ((int)$row['status'] !== APPROVAL_STATUS_PENDING)
		return '-';

	return button('ApproveRow' . $row['id'], _('Approve'), _('Approve this draft'), ICON_OK, 'process')
		. ' '
		. button('RejectRow' . $row['id'], _('Reject'), _('Reject this draft'), ICON_CANCEL, 'cancel')
		. ' '
		. button('DelegateRow' . $row['id'], _('Delegate'), _('Delegate this draft to another approver'), ICON_ALLOC, '');
}

/**
 * Format escalation status indicator for pending items.
 *
 * @param array $row Grid row data
 * @return string    Escalation status badge HTML
 */
function dashboard_escalation_cell($row)
{
	$escalation = get_draft_escalation_status((int)$row['id']);
	if (!$escalation || $escalation['status'] === 'ok') {
		return "<span class='approval-escalation-badge is-ok'>" . _('OK') . "</span>";
	} elseif ($escalation['status'] === 'warning') {
		return "<span class='approval-escalation-badge is-warning'>"
			. sprintf(_('Due in %d days'), $escalation['days_remaining'])
			. "</span>";
	} elseif ($escalation['status'] === 'overdue') {
		return "<span class='approval-escalation-badge is-overdue'>"
			. sprintf(_('Overdue %d days'), abs($escalation['days_remaining']))
			. "</span>";
	}
	return '-';
}

/**
 * Generate cancel button for user's own pending submissions.
 *
 * @param array $row Grid row data
 * @return string    Cancel button HTML or '-'
 */
function dashboard_cancel_button($row)
{
	if ((int)$row['status'] !== APPROVAL_STATUS_PENDING)
		return '-';

	return button('CancelRow' . $row['id'], _('Cancel'), _('Cancel this submission'), ICON_CANCEL, 'cancel');
}

/**
 * Format submitted date for display.
 *
 * @param array $row Grid row data
 * @return string    Formatted date
 */
function dashboard_date_cell($row)
{
	return sql2date($row['submitted_date']);
}

/**
 * Format completed date for display.
 *
 * @param array $row Grid row data
 * @return string    Formatted date or dash
 */
function dashboard_completed_date_cell($row)
{
	return $row['completed_date'] ? sql2date($row['completed_date']) : '-';
}

/**
 * Format reference as a clickable link to view the draft.
 *
 * @param array $row Grid row data
 * @return string    Reference link HTML
 */
function dashboard_reference_cell($row)
{
	$ref = $row['reference'] ? htmlspecialchars($row['reference'], ENT_QUOTES, 'UTF-8') : '#' . (int)$row['reserved_trans_no'];

	return "<a href='approval_view_draft.php?draft_id=" . (int)$row['id'] . "' target='_blank' "
		. "onclick=\"javascript:openWindow(this.href,this.target); return false;\">"
		. $ref . "</a>";
}

// =====================================================
// Handle inline approve/reject from grid buttons
// =====================================================

$approve_id = find_submit('ApproveRow');
$reject_id = find_submit('RejectRow');
$cancel_id = find_submit('CancelRow');
$delegate_id = find_submit('DelegateRow');

if ($approve_id > 0 || $reject_id > 0 || $cancel_id > 0 || $delegate_id > 0) {
	if ($delegate_id > 0) {
		$action_draft_id = $delegate_id;
		$action_type = 'delegate';
	} else {
		$action_draft_id = $approve_id > 0 ? $approve_id : ($reject_id > 0 ? $reject_id : $cancel_id);
		$action_type = $approve_id > 0 ? 'approve' : ($reject_id > 0 ? 'reject' : 'cancel');
	}

	// Show confirmation dialog with comments field
	$_POST['action_draft_id'] = $action_draft_id;
	$_POST['action_type'] = $action_type;
}

// =====================================================
// Display page
// =====================================================

start_form();
hidden('dashboard_tab', $active_tab);

// --- Summary cards ---
$counts = get_approval_status_counts();
$my_pending_count = count_pending_approval_drafts($current_role_id);
$counts['my_pending'] = $my_pending_count;

display_approval_summary_cards($counts);

// --- Action confirmation dialog ---
if (get_post('action_draft_id') > 0 && get_post('action_type') != '') {
	$action_draft_id = (int)get_post('action_draft_id');
	$action_type = get_post('action_type');
	$action_draft = get_approval_draft($action_draft_id);

	if ($action_draft) {
		$action_label = $action_type === 'approve' ? _('Approve')
			: ($action_type === 'reject' ? _('Reject')
			: ($action_type === 'delegate' ? _('Delegate')
			: _('Cancel')));
		$action_theme_class = $action_type === 'approve' ? 'is-approve'
			: ($action_type === 'delegate' ? 'is-delegate' : 'is-danger');

		echo "<div class='approval-action-box " . $action_theme_class . "'>\n";
		echo "<h3 class='approval-action-title'>"
			. sprintf(_('%s Draft #%d — %s'), $action_label, $action_draft_id,
				htmlspecialchars($action_draft['workflow_name'], ENT_QUOTES, 'UTF-8'))
			. "</h3>\n";

		echo "<p>" . _('Reference') . ": <strong>" . htmlspecialchars($action_draft['reference'], ENT_QUOTES, 'UTF-8') . "</strong>"
			. " | " . _('Amount') . ": <strong>" . number_format2($action_draft['amount'], user_price_dec()) . "</strong>"
			. " | " . _('Type') . ": <strong>" . get_trans_type_label((int)$action_draft['trans_type']) . "</strong>"
			. "</p>\n";

		hidden('action_draft_id', $action_draft_id);

		start_table(TABLESTYLE2);

		if ($action_type === 'delegate') {
			// Show delegate-to user selector
			$eligible = get_eligible_delegates_for_draft($action_draft_id);
			$delegate_options = array();
			foreach ($eligible as $delegate) {
				$delegate_options[$delegate['id']] = $delegate['real_name'] . ' (' . $delegate['user_id'] . ')';
			}
			if (empty($delegate_options)) {
				label_row(_('Delegate To') . ':', '<em>' . _('No eligible delegates found.') . '</em>');
			} else {
				echo "<tr><td class='label'>" . _('Delegate To') . ":</td><td>";
				echo array_selector('delegate_to_user', '', $delegate_options);
				echo "</td></tr>\n";
			}
			textarea_row(_('Reason') . ':', 'action_comments', '', 50, 3);
		} else {
			textarea_row($action_label . ' ' . _('Comments') . ':', 'action_comments', '', 50, 3);
		}

		end_table(1);

		if ($action_type === 'approve') {
			submit_center_first('QuickApprove', _('Confirm Approve'), '', 'default');
			submit_center_last('CancelAction', _('Cancel'), '', 'cancel');
		} elseif ($action_type === 'reject') {
			submit_center_first('QuickReject', _('Confirm Reject'), '', 'default');
			submit_center_last('CancelAction', _('Cancel'), '', 'cancel');
		} elseif ($action_type === 'delegate') {
			if (!empty($delegate_options)) {
				submit_center_first('DelegateDraft', _('Confirm Delegate'), '', 'default');
				submit_center_last('CancelAction', _('Cancel'), '', 'cancel');
			} else {
				submit_center('CancelAction', _('Back'), '', 'cancel');
			}
		} else {
			submit_center_first('CancelDraft', _('Confirm Cancel'), '', 'default');
			submit_center_last('CancelAction', _('Cancel Action'), '', 'cancel');
		}

		echo "</div>\n";
	}
}

// --- Tab buttons ---
echo "<div class='approval-tabs'>\n";

$tabs = array(
	'pending'        => _('Pending Approvals'),
	'my_submissions' => _('My Submissions'),
	'activity'       => _('Recent Activity'),
);

foreach ($tabs as $tab_key => $tab_label) {
	$is_active = ($active_tab === $tab_key);
	$tab_class = $is_active ? 'approval-tab-button is-active' : 'approval-tab-button';

	$btn_name = 'Tab' . str_replace('_', '', ucwords($tab_key, '_'));
	echo "<input type='submit' class='{$tab_class}' name='{$btn_name}' value='{$tab_label}'>\n";
}

echo "</div>\n";
echo "<div class='approval-tab-content'>\n";

// --- Filter bar ---
start_table(TABLESTYLE_NOBORDER);
start_row();

$approvable_types = get_approvable_transaction_types();
$type_options = array('' => _('All Types'));
foreach ($approvable_types as $type_id => $type_label) {
	$type_options[$type_id] = $type_label;
}

filter_cell_open(_('Type:'));
echo array_selector('filter_trans_type', get_post('filter_trans_type'), $type_options,
	array('select_submit' => true));
filter_cell_close();

if ($active_tab !== 'activity') {
	date_cells(_('From:'), 'filter_from_date', '', null, 0, 0, 0,
		array('select_submit' => true));
	date_cells(_('To:'), 'filter_to_date', '', null, 0, 0, 0,
		array('select_submit' => true));
}

if ($active_tab === 'my_submissions') {
	$status_options = array(
		'-1' => _('All Statuses'),
		APPROVAL_STATUS_PENDING   => _('Pending'),
		APPROVAL_STATUS_APPROVED  => _('Approved'),
		APPROVAL_STATUS_REJECTED  => _('Rejected'),
		APPROVAL_STATUS_CANCELLED => _('Cancelled'),
	);

	filter_cell_open(_('Status:'));
	echo array_selector('filter_status', get_post('filter_status'), $status_options,
		array('select_submit' => true));
	filter_cell_close();
}

submit_cells('RefreshDashboard', _('Apply Filter'), '', _('Refresh the list'), 'default');

end_row();
end_table();

// --- Build filters array ---
$filters = array();
if (get_post('filter_trans_type') != '')
	$filters['trans_type'] = get_post('filter_trans_type');
if (get_post('filter_from_date') != '') {
	$filters['from_date'] = date2sql(get_post('filter_from_date'));
}
if (get_post('filter_to_date') != '') {
	$filters['to_date'] = date2sql(get_post('filter_to_date'));
}
if (get_post('filter_status') != '' && get_post('filter_status') != '-1') {
	$filters['status'] = get_post('filter_status');
}

// =====================================================
// Tab content
// =====================================================

if ($active_tab === 'pending') {
	// --- Pending Approvals tab ---
	$sql = get_sql_for_approval_dashboard($current_user_id, $current_role_id, $filters);

	$cols = array(
		_('Urgency') => array('insert' => true, 'fun' => 'dashboard_urgency_cell', 'align' => 'center'),
		_('Reference') => array('fun' => 'dashboard_reference_cell'),
		_('Type') => array('insert' => true, 'fun' => 'dashboard_trans_type_cell'),
		_('Workflow') => 'workflow_name',
		_('Submitted By') => 'submitted_by_name',
		_('Date') => array('insert' => true, 'fun' => 'dashboard_date_cell', 'ord' => ''),
		_('Amount') => array('insert' => true, 'fun' => 'dashboard_amount_cell', 'align' => 'right'),
		_('Level') => array('name' => 'current_level', 'align' => 'center'),
		_('Escalation') => array('insert' => true, 'fun' => 'dashboard_escalation_cell', 'align' => 'center'),
		_('Summary') => 'summary',
		_('Actions') => array('insert' => true, 'fun' => 'dashboard_action_buttons', 'align' => 'center'),
		_('View') => array('insert' => true, 'fun' => 'dashboard_view_link', 'align' => 'center'),
	);

	$table =& new_db_pager('pending_approvals', $sql, $cols);
	$table->width = '100%';

	display_db_pager($table);

	if ($my_pending_count == 0) {
		display_note(_('No pending approvals requiring your action.'), 1, 0);
	}

} elseif ($active_tab === 'my_submissions') {
	// --- My Submissions tab ---
	$sql = get_sql_for_my_submissions($current_user_id, $filters);

	$cols = array(
		_('Reference') => array('fun' => 'dashboard_reference_cell'),
		_('Type') => array('insert' => true, 'fun' => 'dashboard_trans_type_cell'),
		_('Workflow') => 'workflow_name',
		_('Date') => array('insert' => true, 'fun' => 'dashboard_date_cell', 'ord' => ''),
		_('Amount') => array('insert' => true, 'fun' => 'dashboard_amount_cell', 'align' => 'right'),
		_('Status') => array('insert' => true, 'fun' => 'dashboard_status_cell', 'align' => 'center'),
		_('Level') => array('name' => 'current_level', 'align' => 'center'),
		_('Completed') => array('insert' => true, 'fun' => 'dashboard_completed_date_cell'),
		_('Summary') => 'summary',
		_('Cancel') => array('insert' => true, 'fun' => 'dashboard_cancel_button', 'align' => 'center'),
		_('View') => array('insert' => true, 'fun' => 'dashboard_view_link', 'align' => 'center'),
	);

	$table =& new_db_pager('my_submissions', $sql, $cols);
	$table->width = '100%';

	display_db_pager($table);

} elseif ($active_tab === 'activity') {
	// --- Recent Activity tab ---
	$recent = get_recent_approval_activity(50);

	echo "<table class='tablestyle approval-activity-table'>\n";
	echo "<tr>";
	echo "<th>" . _('Date/Time') . "</th>";
	echo "<th>" . _('Action') . "</th>";
	echo "<th>" . _('User') . "</th>";
	echo "<th>" . _('Workflow') . "</th>";
	echo "<th>" . _('Reference') . "</th>";
	echo "<th>" . _('Type') . "</th>";
	echo "<th>" . _('Summary') . "</th>";
	echo "</tr>\n";

	$row_index = 0;
	while ($action = db_fetch($recent)) {
		$row_class = ($row_index % 2 == 0) ? 'oddrow' : 'evenrow';
		$action_badge = format_approval_action_badge($action['action_type']);
		$user_name = $action['user_name'] ? htmlspecialchars($action['user_name'], ENT_QUOTES, 'UTF-8') : _('System');
		$workflow_name = $action['workflow_name'] ? htmlspecialchars($action['workflow_name'], ENT_QUOTES, 'UTF-8') : '-';
		$ref_link = $action['draft_id']
			? "<a href='approval_view_draft.php?draft_id=" . (int)$action['draft_id'] . "' target='_blank' "
				. "onclick=\"javascript:openWindow(this.href,this.target); return false;\">"
				. htmlspecialchars($action['reference'], ENT_QUOTES, 'UTF-8') . "</a>"
			: '-';
		$trans_type_label = $action['trans_type'] ? get_trans_type_label((int)$action['trans_type']) : '-';
		$summary = $action['summary'] ? htmlspecialchars($action['summary'], ENT_QUOTES, 'UTF-8') : '-';
		$action_date = sql2date($action['action_date']);
		$action_time = date('H:i', strtotime($action['action_date']));

		echo "<tr class='$row_class'>";
		echo "<td>{$action_date} {$action_time}</td>";
		echo "<td>{$action_badge}</td>";
		echo "<td>{$user_name}</td>";
		echo "<td>{$workflow_name}</td>";
		echo "<td>{$ref_link}</td>";
		echo "<td>{$trans_type_label}</td>";
		echo "<td>{$summary}</td>";
		echo "</tr>\n";

		$row_index++;
	}

	if ($row_index == 0) {
		echo "<tr><td colspan='7' align='center'>" . _('No recent approval activity.') . "</td></tr>\n";
	}

	echo "</table>\n";
}

echo "</div>\n"; // close tab content wrapper

end_form();
end_page();
