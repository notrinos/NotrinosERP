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
 * Recall Campaign Management.
 *
 * Features:
 *   - List recall campaigns with filters (item, status)
 *   - Summary cards by status
 *   - Create/edit campaigns with affected scope (serial range, batch IDs, date range)
 *   - Execute recall: auto-identify affected items, quarantine in-stock serials
 *   - Track individual affected items through recall lifecycle
 *   - Status transitions: Draft → Active → In Progress → Completed/Cancelled
 */
$page_security = 'SA_RECALL';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Recall Campaigns');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page($_SESSION['page_title'], false, false, '', $js);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/inventory/includes/db/recall_db.inc');
include_once($path_to_root . '/inventory/includes/db/serial_numbers_db.inc');
include_once($path_to_root . '/inventory/includes/serial_batch_ui.inc');
include_once($path_to_root . '/inventory/includes/inventory_db.inc');

simple_page_mode(true);

//----------------------------------------------------------------------
// Handle Delete
//----------------------------------------------------------------------
if ($Mode == 'Delete') {
	$can = can_delete_recall_campaign($selected_id);
	if ($can !== true) {
		display_error($can);
	} else {
		delete_recall_campaign($selected_id);
		display_notification(_('Recall campaign has been deleted.'));
	}
	$Mode = 'RESET';
}

//----------------------------------------------------------------------
// Handle Execute Recall
//----------------------------------------------------------------------
$execute_recall_id = find_submit('exec_recall_');
if ($execute_recall_id > 0) {
	$result_exec = execute_recall_campaign((int)$execute_recall_id);
	display_notification(sprintf(
		_('Recall executed: %d serials added, %d serials quarantined, %d batches added.'),
		$result_exec['serials_added'],
		$result_exec['serials_quarantined'],
		$result_exec['batches_added']
	));
	$Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Handle Campaign Status Change
//----------------------------------------------------------------------
$activate_campaign_id = find_submit('activate_campaign_');
if ($activate_campaign_id > 0) {
	update_recall_campaign_status((int)$activate_campaign_id, 'active');
	display_notification(sprintf(_('Campaign status changed to %s.'),
		get_recall_campaign_status_label('active')));
	$Ajax->activate('_page_body');
}

$progress_campaign_id = find_submit('progress_campaign_');
if ($progress_campaign_id > 0) {
	update_recall_campaign_status((int)$progress_campaign_id, 'in_progress');
	display_notification(sprintf(_('Campaign status changed to %s.'),
		get_recall_campaign_status_label('in_progress')));
	$Ajax->activate('_page_body');
}

$complete_campaign_id = find_submit('complete_campaign_');
if ($complete_campaign_id > 0) {
	update_recall_campaign_status((int)$complete_campaign_id, 'completed');
	display_notification(sprintf(_('Campaign status changed to %s.'),
		get_recall_campaign_status_label('completed')));
	$Ajax->activate('_page_body');
}

$cancel_campaign_id = find_submit('cancel_campaign_');
if ($cancel_campaign_id > 0) {
	update_recall_campaign_status((int)$cancel_campaign_id, 'cancelled');
	display_notification(sprintf(_('Campaign status changed to %s.'),
		get_recall_campaign_status_label('cancelled')));
	$Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Handle Recall Item Status Update
//----------------------------------------------------------------------
if (get_post('update_item_status')) {
	$item_id = (int)get_post('recall_item_id');
	$new_item_status = get_post('item_new_status');
	$item_notes = get_post('item_status_notes');
	$valid = array_keys(get_recall_item_statuses());
	if (in_array($new_item_status, $valid)) {
		update_recall_item_status($item_id, $new_item_status, $item_notes);
		// Update campaign totals
		$recall_id = (int)get_post('item_recall_id');
		update_recall_campaign_totals($recall_id);
		display_notification(sprintf(_('Recall item status updated to %s.'),
			get_recall_item_statuses()[$new_item_status]));
	}
	$Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Handle Add / Update Campaign
//----------------------------------------------------------------------
if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {

	$input_error = 0;

	if (strlen(trim(get_post('title'))) == 0) {
		$input_error = 1;
		display_error(_('Please enter a campaign title.'));
	}
	if (strlen(trim(get_post('description'))) == 0) {
		$input_error = 1;
		display_error(_('Please enter a description.'));
	}
	if (!get_post('stock_id')) {
		$input_error = 1;
		display_error(_('Please select an affected item.'));
	}

	if ($input_error != 1) {
		$affected_date_from = get_post('affected_date_from');
		$affected_date_to = get_post('affected_date_to');

		if ($Mode == 'ADD_ITEM') {
			$reference = get_next_recall_campaign_reference();
			$id = add_recall_campaign(
				$reference,
				get_post('title'),
				get_post('description'),
				get_post('stock_id'),
				get_post('recall_type'),
				get_post('severity'),
				get_post('start_date'),
				get_post('end_date'),
				get_post('affected_batch_ids'),
				get_post('affected_serial_from'),
				get_post('affected_serial_to'),
				$affected_date_from,
				$affected_date_to,
				get_post('regulatory_reference'),
				get_post('resolution'),
				get_post('notes')
			);
			display_notification(sprintf(_('Recall campaign %s has been created.'), $reference));
		} else {
			update_recall_campaign(
				$selected_id,
				get_post('title'),
				get_post('description'),
				get_post('stock_id'),
				get_post('recall_type'),
				get_post('severity'),
				get_post('start_date'),
				get_post('end_date'),
				get_post('affected_batch_ids'),
				get_post('affected_serial_from'),
				get_post('affected_serial_to'),
				$affected_date_from,
				$affected_date_to,
				get_post('regulatory_reference'),
				get_post('resolution'),
				get_post('notes')
			);
			display_notification(_('Recall campaign has been updated.'));
		}
		$Mode = 'RESET';
	}
}

if ($Mode == 'RESET') {
	$selected_id = -1;
	unset($_POST['title']);
	unset($_POST['description']);
	unset($_POST['stock_id']);
	unset($_POST['recall_type']);
	unset($_POST['severity']);
	unset($_POST['start_date']);
	unset($_POST['end_date']);
	unset($_POST['affected_batch_ids']);
	unset($_POST['affected_serial_from']);
	unset($_POST['affected_serial_to']);
	unset($_POST['affected_date_from']);
	unset($_POST['affected_date_to']);
	unset($_POST['regulatory_reference']);
	unset($_POST['resolution']);
	unset($_POST['notes']);
}

//----------------------------------------------------------------------
// View campaign detail mode — redirect to dedicated view page
//----------------------------------------------------------------------
if (get_post('view_campaign') || isset($_GET['id'])) {
	$view_id = get_post('view_campaign') ? (int)get_post('view_campaign_id') : (int)$_GET['id'];
	header('Location: ' . $path_to_root . '/inventory/view/view_recall.php?id=' . $view_id);
	exit;
}

start_form();

//----------------------------------------------------------------------
// Filter Section
//----------------------------------------------------------------------
echo '<div style="margin-bottom:10px;">';
start_table(TABLESTYLE_NOBORDER);
start_row();
stock_items_list_cells(null, 'filter_stock_id', null, _('All Items'), true, true);
$status_options = array_merge(array('' => _('All Statuses')), get_recall_campaign_statuses());
label_cell(_('Status:'));
echo '<td>';
echo array_selector('filter_status', get_post('filter_status'), $status_options,
	array('select_submit' => true));
echo '</td>';
submit_cells('search', _('Search'), '', _('Search recall campaigns'), 'default');
end_row();
end_table();
echo '</div>';

//----------------------------------------------------------------------
// Summary Cards
//----------------------------------------------------------------------
$summary = get_recall_campaign_summary();
$all_statuses = get_recall_campaign_statuses();

echo '<div style="display:flex;gap:10px;margin-bottom:15px;flex-wrap:wrap;">';
foreach ($all_statuses as $status_code => $status_label) {
	$cnt = isset($summary[$status_code]) ? $summary[$status_code] : 0;
	$color = get_recall_campaign_status_color($status_code);
	echo '<div style="flex:1;min-width:100px;padding:10px;border-radius:6px;text-align:center;'
		. 'background-color:' . $color . '15;border:1px solid ' . $color . '40;">';
	echo '<div style="font-size:20px;font-weight:bold;color:' . $color . ';">' . $cnt . '</div>';
	echo '<div style="font-size:11px;color:' . $color . ';">' . $status_label . '</div>';
	echo '</div>';
}
echo '</div>';

//----------------------------------------------------------------------
// Campaigns List
//----------------------------------------------------------------------
div_start('campaigns_list');

$filter_stock = get_post('filter_stock_id');
$filter_status = get_post('filter_status');

$result = get_recall_campaigns($filter_stock, $filter_status);

start_table(TABLESTYLE, "width='100%'");
$th = array(_('#'), _('Reference'), _('Title'), _('Item'), _('Type'), _('Severity'),
	_('Status'), _('Affected'), _('Recovered'), _('Progress'), '', '', '');
table_header($th);

$k = 0;
while ($row = db_fetch($result)) {
	alt_table_row_color($k);

	label_cell($row['id']);

	$link = '<a href="' . $path_to_root . '/inventory/view/view_recall.php?id=' . $row['id'] . '">' . $row['reference'] . '</a>';
	label_cell($link);

	label_cell(htmlspecialchars($row['title']));
	label_cell($row['item_description'] . ' (' . $row['stock_id'] . ')');

	$recall_types = get_recall_types();
	label_cell(isset($recall_types[$row['recall_type']]) ? $recall_types[$row['recall_type']] : $row['recall_type']);
	label_cell(recall_severity_badge($row['severity']));
	label_cell(recall_campaign_status_badge($row['status']));

	label_cell($row['total_affected_units'], "align='right'");
	label_cell($row['total_recovered_units'], "align='right'");

	// Progress
	$total = (int)$row['total_affected_units'];
	$recovered = (int)$row['total_recovered_units'];
	$pct = $total > 0 ? round(($recovered / $total) * 100) : 0;
	echo '<td>';
	if ($total > 0) {
		echo '<div style="background:#e9ecef;border-radius:3px;height:14px;width:80px;display:inline-block;overflow:hidden;">';
		echo '<div style="background:#28a745;height:100%;width:' . $pct . '%;"></div>';
		echo '</div> ' . $pct . '%';
	} else {
		echo '-';
	}
	echo '</td>';

	// Actions
	$edit_ok = ($row['status'] === 'draft');
	if ($edit_ok) {
		edit_button_cell("Edit" . $row['id'], _('Edit'));
	}
	$del_ok = can_delete_recall_campaign($row['id']);
	if ($del_ok === true) {
		delete_button_cell("Delete" . $row['id'], _('Delete'));
	}
	// Execute recall button and status change (in a combined action cell)
	echo '<td nowrap>';
	if (in_array($row['status'], array('draft', 'active'))) {
		submit("exec_recall_" . $row['id'], _('Execute'), true,
			_('Execute recall to identify affected items'), 'fa-bolt');
	}
	$status_transitions = array(
		'draft'       => array('activate_campaign_', _('Active'), 'fa-play'),
		'active'      => array('progress_campaign_', _('In Progress'), 'fa-arrow-right'),
		'in_progress' => array('complete_campaign_', _('Completed'), 'fa-check'),
	);
	if (isset($status_transitions[$row['status']])) {
		$tr = $status_transitions[$row['status']];
		submit($tr[0] . $row['id'], $tr[1], true, sprintf(_('Change status to %s'), $tr[1]), $tr[2]);
	}
	if (in_array($row['status'], array('active', 'in_progress'))) {
		submit("cancel_campaign_" . $row['id'], _('Cancel'), true,
			_('Cancel this campaign'), 'fa-times');
	}
	echo '</td>';
	end_row();
}
end_table(1);

div_end();

//----------------------------------------------------------------------
// Add / Edit Form
//----------------------------------------------------------------------
start_table(TABLESTYLE2);

$is_edit = ($selected_id != -1 && $Mode == 'Edit');
if ($is_edit) {
	$campaign = get_recall_campaign($selected_id);
	if ($campaign) {
		$_POST['title'] = $campaign['title'];
		$_POST['description'] = $campaign['description'];
		$_POST['stock_id'] = $campaign['stock_id'];
		$_POST['recall_type'] = $campaign['recall_type'];
		$_POST['severity'] = $campaign['severity'];
		$_POST['start_date'] = sql2date($campaign['start_date']);
		$_POST['end_date'] = $campaign['end_date'] ? sql2date($campaign['end_date']) : '';
		$_POST['affected_batch_ids'] = $campaign['affected_batch_ids'];
		$_POST['affected_serial_from'] = $campaign['affected_serial_from'];
		$_POST['affected_serial_to'] = $campaign['affected_serial_to'];
		$_POST['affected_date_from'] = $campaign['affected_date_from'] ? sql2date($campaign['affected_date_from']) : '';
		$_POST['affected_date_to'] = $campaign['affected_date_to'] ? sql2date($campaign['affected_date_to']) : '';
		$_POST['regulatory_reference'] = $campaign['regulatory_reference'];
		$_POST['resolution'] = $campaign['resolution'];
		$_POST['notes'] = $campaign['notes'];
	}
}

if (!isset($_POST['start_date']))
	$_POST['start_date'] = Today();

text_row(_('Campaign Title:'), 'title', get_post('title'), 60, 200);

textarea_row(_('Description:'), 'description', get_post('description'), 60, 4);

start_row();
label_cell(_('Affected Item:'));
stock_items_list_cells(null, 'stock_id', null, false, false, true);
end_row();

$recall_types = get_recall_types();
array_selector_row(_('Recall Type:'), 'recall_type', get_post('recall_type'), $recall_types);

$severity_levels = get_recall_severity_levels();
array_selector_row(_('Severity:'), 'severity', get_post('severity'), $severity_levels);

date_row(_('Start Date:'), 'start_date');
date_row(_('End Date:'), 'end_date', '', true, 0, 0, 1001);

// Scope definition
table_section_title(_('Affected Scope'));

text_row(_('Affected Batch IDs (CSV):'), 'affected_batch_ids', get_post('affected_batch_ids'), 40, 200);
text_row(_('Serial Range From:'), 'affected_serial_from', get_post('affected_serial_from'), 30, 100);
text_row(_('Serial Range To:'), 'affected_serial_to', get_post('affected_serial_to'), 30, 100);
date_row(_('Mfg Date From:'), 'affected_date_from', '', true, 0, 0, 1001);
date_row(_('Mfg Date To:'), 'affected_date_to', '', true, 0, 0, 1001);

table_section_title(_('Additional Information'));

text_row(_('Regulatory Reference:'), 'regulatory_reference', get_post('regulatory_reference'), 40, 200);
text_row(_('Resolution Approach:'), 'resolution', get_post('resolution'), 40, 200);
textarea_row(_('Notes:'), 'notes', get_post('notes'), 60, 3);

end_table(1);

submit_add_or_update_center($selected_id == -1, '', 'both');

end_form();
end_page();
