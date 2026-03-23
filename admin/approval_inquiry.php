<?php
/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	Admin: Approval History Inquiry

	Provides a searchable, filterable view of all approval drafts
	across the system. Supports filtering by status, transaction type,
	date range, submitter, and reference number.

	Phase 3 of the Approval System Development Plan.
***********************************************************************/
$page_security = 'SA_APPROVALINQUIRY';
$path_to_root = '..';

include($path_to_root . '/includes/db_pager.inc');
include_once($path_to_root . '/includes/session.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 600);

page(_($help_context = 'Approval History Inquiry'), false, false, '', $js);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/approval/db/approval_db.inc');
include_once($path_to_root . '/includes/approval/db/approval_rules_db.inc');
include_once($path_to_root . '/includes/approval/ui/approval_timeline.inc');
include_once($path_to_root . '/includes/approval/ui/approval_badges.inc');
include_once($path_to_root . '/admin/db/approval_rules_setup_db.inc');
include_once($path_to_root . '/admin/db/approval_dashboard_db.inc');

// =====================================================
// db_pager column formatters
// =====================================================

/**
 * Format transaction type for inquiry grid.
 *
 * @param array $row Grid row data
 * @return string    Formatted type label
 */
function inquiry_trans_type_cell($row)
{
	return get_trans_type_label((int)$row['trans_type']);
}

/**
 * Format status column for inquiry grid.
 *
 * @param array $row Grid row data
 * @return string    Status badge HTML
 */
function inquiry_status_cell($row)
{
	return approval_status_cell((int)$row['status']);
}

/**
 * Format amount for inquiry grid.
 *
 * @param array $row Grid row data
 * @return string    Formatted amount
 */
function inquiry_amount_cell($row)
{
	if ((float)$row['amount'] == 0)
		return '-';

	$formatted = number_format2($row['amount'], user_price_dec());

	return $formatted . ($row['currency'] ? ' ' . $row['currency'] : '');
}

/**
 * Generate view link for inquiry grid.
 *
 * @param array $row Grid row data
 * @return string    View link HTML
 */
function inquiry_view_link($row)
{
	return "<a href='approval_view_draft.php?draft_id=" . (int)$row['id'] . "' target='_blank' "
		. "onclick=\"javascript:openWindow(this.href,this.target); return false;\">"
		. "<i class='fas fa-search' style='margin-right:4px;'></i>"
		. _('View') . "</a>";
}

/**
 * Format reference as clickable link.
 *
 * @param array $row Grid row data
 * @return string    Reference link HTML
 */
function inquiry_reference_cell($row)
{
	$ref = $row['reference'] ? htmlspecialchars($row['reference'], ENT_QUOTES, 'UTF-8') : '#' . (int)$row['reserved_trans_no'];

	return "<a href='approval_view_draft.php?draft_id=" . (int)$row['id'] . "' target='_blank' "
		. "onclick=\"javascript:openWindow(this.href,this.target); return false;\">"
		. $ref . "</a>";
}

/**
 * Format submitted date for display.
 *
 * @param array $row Grid row data
 * @return string    Formatted date
 */
function inquiry_date_cell($row)
{
	return sql2date($row['submitted_date']);
}

/**
 * Format completed date for display.
 *
 * @param array $row Grid row data
 * @return string    Formatted date or dash
 */
function inquiry_completed_date_cell($row)
{
	return $row['completed_date'] ? sql2date($row['completed_date']) : '-';
}

/**
 * Format approved trans number.
 *
 * @param array $row Grid row data
 * @return string    Trans number or dash
 */
function inquiry_posted_trans_cell($row)
{
	return $row['approved_trans_no'] ? (int)$row['approved_trans_no'] : '-';
}

// =====================================================
// Display page
// =====================================================

start_form();

// --- Filter bar ---
start_table(TABLESTYLE_NOBORDER);
start_row();

// Transaction type filter
$approvable_types = get_approvable_transaction_types();
$type_options = array('' => _('All Types'));
foreach ($approvable_types as $type_id => $type_label) {
	$type_options[$type_id] = $type_label;
}
echo "<td>" . _('Type') . ":</td><td>";
echo array_selector('filter_trans_type', get_post('filter_trans_type'), $type_options,
	array('select_submit' => true));
echo "</td>";

// Status filter
$status_options = array(
	'-1' => _('All Statuses'),
	APPROVAL_STATUS_PENDING   => _('Pending'),
	APPROVAL_STATUS_APPROVED  => _('Approved'),
	APPROVAL_STATUS_REJECTED  => _('Rejected'),
	APPROVAL_STATUS_CANCELLED => _('Cancelled'),
	APPROVAL_STATUS_EXPIRED   => _('Expired'),
);
echo "<td>" . _('Status') . ":</td><td>";
echo array_selector('filter_status', get_post('filter_status', '-1'), $status_options,
	array('select_submit' => true));
echo "</td>";

end_row();
start_row();

// Date range filter
date_cells(_('From:'), 'filter_from_date', '', null, -30, 0, 0,
	array('select_submit' => true));
date_cells(_('To:'), 'filter_to_date', '', null, 0, 0, 0,
	array('select_submit' => true));

// Submitter filter
$users = get_users_for_delegation();
$user_options = array('' => _('All Users'));
foreach ($users as $uid => $uname) {
	$user_options[$uid] = $uname;
}
echo "<td>" . _('Submitted By') . ":</td><td>";
echo array_selector('filter_submitter', get_post('filter_submitter'), $user_options,
	array('select_submit' => true));
echo "</td>";

end_row();
start_row();

// Reference search
text_cells(_('Reference:'), 'filter_reference', get_post('filter_reference'), 20, 30);

submit_cells('SearchInquiry', _('Search'), '', _('Apply filters and search'), 'default');

end_row();
end_table();

// --- Build filters ---
$filters = array();

if (get_post('filter_trans_type') != '')
	$filters['trans_type'] = get_post('filter_trans_type');

if (get_post('filter_status') != '' && get_post('filter_status') != '-1')
	$filters['status'] = get_post('filter_status');

if (get_post('filter_from_date') != '')
	$filters['from_date'] = date2sql(get_post('filter_from_date'));

if (get_post('filter_to_date') != '')
	$filters['to_date'] = date2sql(get_post('filter_to_date'));

if (get_post('filter_submitter') != '')
	$filters['submitter'] = get_post('filter_submitter');

if (get_post('filter_reference') != '')
	$filters['reference'] = get_post('filter_reference');

// --- Results grid ---
$sql = get_sql_for_approval_inquiry($filters);

$cols = array(
	_('Reference') => array('fun' => 'inquiry_reference_cell'),
	_('Type') => array('insert' => true, 'fun' => 'inquiry_trans_type_cell'),
	_('Workflow') => 'workflow_name',
	_('Submitted By') => 'submitted_by_name',
	_('Date') => array('insert' => true, 'fun' => 'inquiry_date_cell', 'ord' => ''),
	_('Amount') => array('insert' => true, 'fun' => 'inquiry_amount_cell', 'align' => 'right'),
	_('Status') => array('insert' => true, 'fun' => 'inquiry_status_cell', 'align' => 'center'),
	_('Level') => array('name' => 'current_level', 'align' => 'center'),
	_('Completed') => array('insert' => true, 'fun' => 'inquiry_completed_date_cell'),
	_('Posted #') => array('insert' => true, 'fun' => 'inquiry_posted_trans_cell', 'align' => 'center'),
	_('Summary') => 'summary',
	array('insert' => true, 'fun' => 'inquiry_view_link', 'align' => 'center'),
);

$table =& new_db_pager('approval_inquiry', $sql, $cols);
$table->width = '100%';

display_db_pager($table);

// --- Summary counts ---
$counts = get_approval_status_counts($filters);
echo "<div style='margin:12px 0;padding:8px;background:#f8f9fa;border-radius:4px;font-size:12px;'>";
echo _('Results') . ": ";
echo "<strong>" . $counts['total'] . "</strong> " . _('total') . " | ";
echo "<span style='color:#ffc107;'>" . $counts['pending'] . " " . _('pending') . "</span> | ";
echo "<span style='color:#28a745;'>" . $counts['approved'] . " " . _('approved') . "</span> | ";
echo "<span style='color:#dc3545;'>" . $counts['rejected'] . " " . _('rejected') . "</span> | ";
echo "<span style='color:#6c757d;'>" . $counts['cancelled'] . " " . _('cancelled') . "</span>";
echo "</div>\n";

end_form();
end_page();
