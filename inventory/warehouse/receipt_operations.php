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

/**
 * Receipt (Inbound) Operations Dashboard.
 *
 * Shows pending receipt, quality-check, and putaway operations
 * for WMS-enabled warehouses. Allows starting, completing, and
 * cancelling operations.
 */
$page_security = 'SA_WAREHOUSE_OPERATIONS';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Receipt Operations');

page($_SESSION['page_title']);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_operations_db.inc');

/**
 * Read an operation id from a submitted action button payload.
 *
 * @param string $action_key POST key for the clicked action button.
 * @return int Operation id, or 0 when missing/invalid.
 */
function get_submitted_operation_id($action_key)
{
	if (!isset($_POST[$action_key]) || !is_scalar($_POST[$action_key]))
		return 0;

	$op_id = (int)$_POST[$action_key];
	return $op_id > 0 ? $op_id : 0;
}

/**
 * Validate and normalize receipt dashboard filter values.
 *
 * @return array Normalized filters with keys: loc_code, op_type, op_status.
 */
function get_receipt_operations_filters()
{
	$allowed_types = array('receipt', 'quality_check', 'putaway');
	$allowed_statuses = array('draft', 'ready', 'in_progress', 'done', 'cancelled');

	$loc_code = get_post('loc_code');
	if (!is_scalar($loc_code))
		$loc_code = '';
	$loc_code = trim((string)$loc_code);

	$op_type = get_post('op_type');
	if (!is_scalar($op_type))
		$op_type = '';
	$op_type = trim((string)$op_type);
	if ($op_type !== '' && !in_array($op_type, $allowed_types))
		$op_type = '';

	$op_status = get_post('op_status');
	if (!is_scalar($op_status))
		$op_status = '';
	$op_status = trim((string)$op_status);
	if ($op_status !== '' && !in_array($op_status, $allowed_statuses))
		$op_status = '';

	return array(
		'loc_code' => $loc_code,
		'op_type' => $op_type,
		'op_status' => $op_status,
	);
}

//-------------------------------------------------------------------------------------
// Process actions
//-------------------------------------------------------------------------------------

$submitted_filters = get_receipt_operations_filters();

if (isset($_POST['StartOp'])) {
	$op_id = get_submitted_operation_id('StartOp');
	$operation = $op_id ? get_wh_operation($op_id) : false;

	if (!$operation) {
		display_error(_('Invalid operation id for Start action.'));
	} elseif (!in_array($operation['op_status'], array('draft', 'ready'))) {
		display_error(sprintf(_('Operation #%d cannot be started from status %s.'), $op_id, $operation['op_status']));
	} else {
		update_wh_operation_status($op_id, 'in_progress');
		display_notification(sprintf(_('Operation #%d started.'), $op_id));
	}
	$Ajax->activate('_page_body');
}

if (isset($_POST['CompleteOp'])) {
	$op_id = get_submitted_operation_id('CompleteOp');
	$operation = $op_id ? get_wh_operation($op_id) : false;

	if (!$operation) {
		display_error(_('Invalid operation id for Complete action.'));
	} elseif ($operation['op_status'] !== 'in_progress') {
		display_error(sprintf(_('Operation #%d can only be completed from In Progress status.'), $op_id));
	} else {
		complete_wh_operation($op_id);
		display_notification(sprintf(_('Operation #%d completed.'), $op_id));
	}
	$Ajax->activate('_page_body');
}

if (isset($_POST['CancelOp'])) {
	$op_id = get_submitted_operation_id('CancelOp');
	$operation = $op_id ? get_wh_operation($op_id) : false;

	if (!$operation) {
		display_error(_('Invalid operation id for Cancel action.'));
	} elseif ($operation['op_status'] === 'done') {
		display_error(sprintf(_('Operation #%d is already completed and cannot be cancelled.'), $op_id));
	} else {
		cancel_wh_operation($op_id);
		display_notification(sprintf(_('Operation #%d cancelled.'), $op_id));
	}
	$Ajax->activate('_page_body');
}

//-------------------------------------------------------------------------------------
// Filter form
//-------------------------------------------------------------------------------------

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();

// Warehouse location filter
locations_list_cells(null, 'loc_code', $submitted_filters['loc_code'], true, _('All Locations'));

// Operation type filter
$op_types = array('' => _('All Operation Types'), 'receipt' => _('Receipt'), 'quality_check' => _('Quality Check'), 'putaway' => _('Putaway'));
echo "<div class = 'filter-field'>";
echo '<select name="op_type">';
foreach ($op_types as $val => $label) {
	$sel = ($submitted_filters['op_type'] == $val) ? ' selected' : '';
	echo '<option value="' . htmlspecialchars($val, ENT_QUOTES) . '"' . $sel . '>' . $label . '</option>';
}
echo '</select></div>';

// Status filter
$statuses = array('' => _('All Statuses'), 'draft' => _('Draft'), 'ready' => _('Ready'), 'in_progress' => _('In Progress'), 'done' => _('Done'), 'cancelled' => _('Cancelled'));
echo "<div class = 'filter-field'>";
echo '<select name="op_status">';
foreach ($statuses as $val => $label) {
	$sel = ($submitted_filters['op_status'] == $val) ? ' selected' : '';
	echo '<option value="' . htmlspecialchars($val, ENT_QUOTES) . '"' . $sel . '>' . $label . '</option>';
}
echo '</select></div>';

submit_cells('RefreshList', _('Search'), '', _('Refresh list'), 'default');

end_row();
end_table(1);

//-------------------------------------------------------------------------------------
// Summary cards
//-------------------------------------------------------------------------------------

display_heading(_('Inbound Operations Summary'));

// Build filter params for count queries
$filter_loc = $submitted_filters['loc_code'];

// Count by type+status  
$pending_receipts = count_wh_operations_filtered(array(
	'op_type' => 'receipt',
	'op_status' => array('draft', 'ready', 'in_progress'),
	'warehouse_loc_code' => $filter_loc,
));
$pending_qc = count_wh_operations_filtered(array(
	'op_type' => 'quality_check',
	'op_status' => array('draft', 'ready', 'in_progress'),
	'warehouse_loc_code' => $filter_loc,
));
$pending_putaway = count_wh_operations_filtered(array(
	'op_type' => 'putaway',
	'op_status' => array('draft', 'ready', 'in_progress'),
	'warehouse_loc_code' => $filter_loc,
));

echo '<div style="display:flex; gap:16px; margin:12px 0; flex-wrap:wrap;">';

// Receipt card
echo '<div style="flex:1; min-width:180px; padding:12px; border:1px solid #ddd; border-radius:6px; background:#fff3cd; text-align:center;">';
echo '<div style="font-size:24px; font-weight:bold; color:#856404;">' . $pending_receipts . '</div>';
echo '<div style="color:#856404;">' . _('Pending Receipts') . '</div>';
echo '</div>';

// QC card
echo '<div style="flex:1; min-width:180px; padding:12px; border:1px solid #ddd; border-radius:6px; background:#d1ecf1; text-align:center;">';
echo '<div style="font-size:24px; font-weight:bold; color:#0c5460;">' . $pending_qc . '</div>';
echo '<div style="color:#0c5460;">' . _('Pending QC') . '</div>';
echo '</div>';

// Putaway card
echo '<div style="flex:1; min-width:180px; padding:12px; border:1px solid #ddd; border-radius:6px; background:#d4edda; text-align:center;">';
echo '<div style="font-size:24px; font-weight:bold; color:#155724;">' . $pending_putaway . '</div>';
echo '<div style="color:#155724;">' . _('Pending Putaway') . '</div>';
echo '</div>';

echo '</div>';

//-------------------------------------------------------------------------------------
// Operations list
//-------------------------------------------------------------------------------------

display_heading(_('Operations'));

div_start('ops_table');

// Build filters
$filters = array();
if (!empty($submitted_filters['op_type']))
	$filters['op_type'] = $submitted_filters['op_type'];
if (!empty($submitted_filters['op_status']))
	$filters['op_status'] = $submitted_filters['op_status'];
else
	$filters['op_status'] = array('draft', 'ready', 'in_progress'); // default: active only
if (!empty($submitted_filters['loc_code']))
	$filters['warehouse_loc_code'] = $submitted_filters['loc_code'];

$result = get_wh_operations_filtered($filters, 100);

start_table(TABLESTYLE, "width='95%'");
$th = array(_('#'), _('Type'), _('Status'), _('Source Doc'), _('From'), _('To'),
	_('Priority'), _('Assigned'), _('Created'), _('Notes'), '');
table_header($th);

$k = 0;
while ($row = db_fetch($result)) {
	alt_table_row_color($k);

	label_cell($row['op_id']);

	// Type with color badge
	$type_color = get_wh_operation_type_color($row['op_type']);
	$type_label = ucwords(str_replace('_', ' ', $row['op_type']));
	echo '<td><span style="padding:2px 8px; border-radius:3px; background:' . $type_color . '; color:#fff; font-size:11px;">' . $type_label . '</span></td>';

	// Status with color badge
	$status_color = get_wh_operation_status_color($row['op_status']);
	$status_label = ucwords(str_replace('_', ' ', $row['op_status']));
	echo '<td><span style="padding:2px 8px; border-radius:3px; background:' . $status_color . '; color:#fff; font-size:11px;">' . $status_label . '</span></td>';

	// Source document
	$doc_type = $row['source_doc_type'];
	$doc_no = $row['source_doc_no'];
	if ($doc_type == ST_SUPPRECEIVE) {
		label_cell(get_trans_view_str($doc_type, $doc_no, sprintf(_('GRN #%d'), $doc_no)));
	} else {
		label_cell($doc_type . '-' . $doc_no);
	}

	// From/To locations
	label_cell($row['from_loc_name'] ? $row['from_loc_name'] : '-');
	label_cell($row['to_loc_name'] ? $row['to_loc_name'] : '-');

	// Priority
	label_cell($row['priority'], 'align=center');

	// Assigned user
	label_cell($row['assigned_to'] ? $row['assigned_to'] : _('Unassigned'));

	// Created date
	label_cell(sql2date($row['created_at']));

	// Notes
	label_cell($row['memo'] ? $row['memo'] : '');

	// Actions
	echo '<td nowrap>';
	$status = $row['op_status'];
	if ($status === 'ready' || $status === 'draft') {
		echo '<button type="submit" name="StartOp" value="' . (int)$row['op_id'] . '" class="ajaxsubmit" '
			. 'style="padding:2px 8px; background:#28a745; color:#fff; border:none; border-radius:3px; cursor:pointer; margin-right:4px;">'
			. _('Start') . '</button>';
		echo '<button type="submit" name="CancelOp" value="' . (int)$row['op_id'] . '" class="ajaxsubmit" '
			. 'style="padding:2px 8px; background:#dc3545; color:#fff; border:none; border-radius:3px; cursor:pointer;">'
			. _('Cancel') . '</button>';
	} elseif ($status === 'in_progress') {
		echo '<button type="submit" name="CompleteOp" value="' . (int)$row['op_id'] . '" class="ajaxsubmit" '
			. 'style="padding:2px 8px; background:#007bff; color:#fff; border:none; border-radius:3px; cursor:pointer; margin-right:4px;">'
			. _('Complete') . '</button>';
		echo '<button type="submit" name="CancelOp" value="' . (int)$row['op_id'] . '" class="ajaxsubmit" '
			. 'style="padding:2px 8px; background:#dc3545; color:#fff; border:none; border-radius:3px; cursor:pointer;">'
			. _('Cancel') . '</button>';
	} else {
		echo '-';
	}
	echo '</td>';

	end_row();

	// Show operation lines (expandable detail)
	$lines = get_wh_operation_lines($row['op_id']);
	if (db_num_rows($lines) > 0) {
		echo '<tr style="display:table-row;">';
		echo '<td></td>';
		echo '<td colspan="' . (count($th) - 1) . '" style="padding:4px 8px; background:#f8f9fa;">';
		echo '<table class="tablestyle" style="width:100%; font-size:11px;">';
		echo '<tr class="tableheader"><th>' . _('Item') . '</th><th>' . _('Qty') . '</th>'
			. '<th>' . _('Done') . '</th><th>' . _('From Bin') . '</th>'
			. '<th>' . _('To Bin') . '</th><th>' . _('Batch') . '</th>'
			. '<th>' . _('Serial') . '</th></tr>';
		while ($line = db_fetch($lines)) {
			$planned_qty = isset($line['qty_planned']) ? $line['qty_planned'] : (isset($line['qty']) ? $line['qty'] : 0);
			$done_qty = isset($line['qty_done']) ? $line['qty_done'] : 0;
			echo '<tr>';
			echo '<td>' . $line['stock_id'] . '</td>';
			echo '<td align="right">' . number_format2($planned_qty, get_qty_dec($line['stock_id'])) . '</td>';
			echo '<td align="right">' . number_format2($done_qty, get_qty_dec($line['stock_id'])) . '</td>';
			echo '<td>' . ($line['from_loc_id'] ? $line['from_loc_id'] : '-') . '</td>';
			echo '<td>' . ($line['to_loc_id'] ? $line['to_loc_id'] : '-') . '</td>';
			echo '<td>' . ($line['batch_id'] ? $line['batch_id'] : '-') . '</td>';
			echo '<td>' . ($line['serial_id'] ? $line['serial_id'] : '-') . '</td>';
			echo '</tr>';
		}
		echo '</table></td></tr>';
	}
}

end_table();
div_end();

end_form();
end_page();
