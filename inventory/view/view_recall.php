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
 * Recall Campaign Detail View / Print page.
 *
 * Displays a printer-friendly, comprehensive view of a recall campaign including:
 *   - Campaign header details (reference, item, type, severity, dates)
 *   - Progress dashboard (metrics, progress bar)
 *   - Customer notification list with notification status
 *   - Complete list of affected items with per-item status tracking
 *   - Recall timeline of key events
 *   - Batch action buttons for status updates (mark all notified, etc.)
 *
 * Opened via recall_campaigns.php or directly via ?id=N
 */
$page_security = 'SA_RECALL';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'View Recall Campaign');

page($_SESSION['page_title'], true, false, '', '');

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/inventory/includes/db/recall_db.inc');
include_once($path_to_root . '/inventory/includes/db/serial_numbers_db.inc');
include_once($path_to_root . '/inventory/includes/inventory_db.inc');

$campaign_id = (int)(isset($_GET['id']) ? $_GET['id'] : get_post('id'));

if (!$campaign_id) {
	display_error(_('No recall campaign ID specified.'));
	end_page();
	exit;
}

$campaign = get_recall_campaign($campaign_id);
if (!$campaign) {
	display_error(_('Recall campaign not found.'));
	end_page();
	exit;
}

// =====================================================================
// Handle batch actions via POST
// =====================================================================
if (isset($_POST['batch_mark_notified'])) {
	$count = batch_update_recall_items_by_status($campaign_id, 'identified', 'notified',
		_('Batch notified via recall view'));
	display_notification(sprintf(_('%d items marked as notified.'), $count));
	$campaign = get_recall_campaign($campaign_id);
}

if (isset($_POST['mark_customer_notified'])) {
	$cust_id = (int)$_POST['notify_customer_id'];
	if ($cust_id) {
		$count = mark_customer_notified($campaign_id, $cust_id,
			_('Marked notified from recall view'));
		display_notification(sprintf(_('%d items for customer marked as notified.'), $count));
		$campaign = get_recall_campaign($campaign_id);
	}
}

// Handle Execute Recall from view page
if (isset($_POST['execute_recall_view'])) {
	$result_exec = execute_recall_campaign($campaign_id);
	display_notification(sprintf(
		_('Recall executed: %d serials added, %d serials quarantined, %d batches added.'),
		$result_exec['serials_added'],
		$result_exec['serials_quarantined'],
		$result_exec['batches_added']
	));
	$campaign = get_recall_campaign($campaign_id);
	$Ajax->activate('_page_body');
}

// Handle Status Change from view page using find_submit pattern
$view_status_map = array(
	'view_activate_'  => 'active',
	'view_progress_'  => 'in_progress',
	'view_complete_'  => 'completed',
	'view_cancel_'    => 'cancelled',
);
foreach ($view_status_map as $prefix => $target_status) {
	if (find_submit($prefix) != -1) {
		update_recall_campaign_status($campaign_id, $target_status);
		display_notification(sprintf(_('Campaign status changed to %s.'),
			get_recall_campaign_status_label($target_status)));
		$campaign = get_recall_campaign($campaign_id);
		$Ajax->activate('_page_body');
		break;
	}
}

// Get comprehensive progress data
$progress = get_recall_campaign_progress($campaign_id);

// Print CSS
echo '<style>
@media print {
	.noprint { display:none !important; }
	body { font-size:11pt; }
}
.recall-header { margin:10px 0; }
.recall-header h2 { margin:0 0 5px 0; }
.recall-detail-table { border-collapse:collapse; margin:10px 0; }
.recall-detail-table td { padding:4px 15px 4px 0; vertical-align:top; }
.recall-detail-table .label { font-weight:600; white-space:nowrap; }
.recall-separator { border:0; border-top:2px solid #333; margin:15px 0; }
.recall-cards { display:flex; gap:10px; margin:15px 0; flex-wrap:wrap; }
.recall-card { flex:1; min-width:110px; padding:12px; border-radius:6px; text-align:center; }
.recall-card .value { font-size:22px; font-weight:bold; }
.recall-card .label { font-size:11px; margin-top:2px; }
.recall-progress-bar { background:#e9ecef; border-radius:4px; height:24px; overflow:hidden; margin:5px 0; }
.recall-progress-fill { height:100%; transition:width 0.3s; border-radius:4px; }
.recall-timeline { margin:15px 0; }
.recall-timeline-item { display:flex; gap:10px; padding:6px 0; border-bottom:1px solid #eee; }
.recall-timeline-item .date { min-width:100px; color:#666; font-size:12px; }
.recall-timeline-item .action { font-weight:600; }
.recall-timeline-item .detail { color:#666; }
</style>';

// Action buttons
echo '<div class="noprint" style="text-align:center;margin:10px 0;">';
echo '<button onclick="window.print();" style="padding:6px 15px;cursor:pointer;">'
	. '<i class="fa fa-print"></i> ' . _('Print') . '</button>';
echo '&nbsp;&nbsp;';
echo '<a href="' . $path_to_root . '/inventory/manage/recall_campaigns.php" '
	. 'style="padding:6px 15px;text-decoration:none;background:#6c757d;color:#fff;border-radius:4px;">'
	. '<i class="fa fa-arrow-left"></i> ' . _('Back to Campaigns') . '</a>';
echo '&nbsp;&nbsp;';
echo '<a href="' . $path_to_root . '/reporting/rep312.php?PARAM_0=&PARAM_1=' . urlencode($campaign['status'])
	. '&PARAM_2=&PARAM_3=&PARAM_4=&PARAM_5=1&PARAM_6=0" target="_blank" '
	. 'style="padding:6px 15px;text-decoration:none;background:#007bff;color:#fff;border-radius:4px;">'
	. '<i class="fa fa-file-pdf-o"></i> ' . _('PDF Report') . '</a>';
echo '</div>';

// Campaign Action buttons (Execute Recall, Change Status)
if (!in_array($campaign['status'], array('completed', 'cancelled'))) {
	echo '<div class="noprint" style="text-align:center;margin:10px 0;padding:10px;background:#f8f9fa;border-radius:6px;">';

	start_form(false, $_SERVER['PHP_SELF'] . '?id=' . $campaign_id);
	hidden('id', $campaign_id);

	// Execute Recall button
	if (in_array($campaign['status'], array('draft', 'active'))) {
		submit('execute_recall_view', _('Execute Recall'), true,
			_('Execute recall to identify affected items from serial/batch records'), 'fa-bolt');
		echo '&nbsp;';
	}

	// Status change buttons
	$status_transitions = array(
		'draft'       => array(array('view_activate_', _('Active'), 'fa-play')),
		'active'      => array(
			array('view_progress_', _('In Progress'), 'fa-arrow-right'),
			array('view_cancel_', _('Cancel'), 'fa-times'),
		),
		'in_progress' => array(
			array('view_complete_', _('Completed'), 'fa-check'),
			array('view_cancel_', _('Cancel'), 'fa-times'),
		),
	);
	if (isset($status_transitions[$campaign['status']])) {
		foreach ($status_transitions[$campaign['status']] as $tr) {
			submit($tr[0] . $campaign_id, $tr[1], true,
				sprintf(_('Change status to %s'), $tr[1]), $tr[2]);
			echo '&nbsp;';
		}
	}

	end_form();
	echo '</div>';
}

// Company header
$company = get_company_prefs();

echo '<div class="recall-header" style="text-align:center;">';
echo '<h2>' . htmlspecialchars($company['coy_name']) . '</h2>';
echo '<h3>' . _('Recall Campaign Report') . '</h3>';
echo '<p style="font-size:14px;font-weight:600;">'
	. htmlspecialchars($campaign['reference']) . ' &mdash; '
	. recall_campaign_status_badge($campaign['status'])
	. ' ' . recall_severity_badge($campaign['severity']) . '</p>';
echo '</div>';

echo '<hr class="recall-separator">';

// =====================================================================
// Section 1: Campaign Details
// =====================================================================
echo '<h3>' . _('Campaign Details') . '</h3>';

echo '<table class="recall-detail-table">';
echo '<tr><td class="label">' . _('Reference') . ':</td><td>' . htmlspecialchars($campaign['reference']) . '</td>';
echo '<td class="label">' . _('Title') . ':</td><td>' . htmlspecialchars($campaign['title']) . '</td></tr>';

echo '<tr><td class="label">' . _('Item') . ':</td><td>' . htmlspecialchars($campaign['item_description']) . ' (' . htmlspecialchars($campaign['stock_id']) . ')</td>';
$recall_types = get_recall_types();
echo '<td class="label">' . _('Recall Type') . ':</td><td>' . (isset($recall_types[$campaign['recall_type']]) ? $recall_types[$campaign['recall_type']] : $campaign['recall_type']) . '</td></tr>';

echo '<tr><td class="label">' . _('Severity') . ':</td><td>' . recall_severity_badge($campaign['severity']) . '</td>';
echo '<td class="label">' . _('Status') . ':</td><td>' . recall_campaign_status_badge($campaign['status']) . '</td></tr>';

echo '<tr><td class="label">' . _('Start Date') . ':</td><td>' . sql2date($campaign['start_date']) . '</td>';
echo '<td class="label">' . _('End Date') . ':</td><td>' . ($campaign['end_date'] ? sql2date($campaign['end_date']) : '&mdash;') . '</td></tr>';

if ($campaign['affected_batch_ids']) {
	echo '<tr><td class="label">' . _('Affected Batch IDs') . ':</td><td colspan="3">' . htmlspecialchars($campaign['affected_batch_ids']) . '</td></tr>';
}
if ($campaign['affected_serial_from']) {
	echo '<tr><td class="label">' . _('Serial Range') . ':</td><td colspan="3">' . htmlspecialchars($campaign['affected_serial_from']) . ' &mdash; ' . htmlspecialchars($campaign['affected_serial_to']) . '</td></tr>';
}
if ($campaign['affected_date_from']) {
	echo '<tr><td class="label">' . _('Mfg Date Range') . ':</td><td colspan="3">'
		. sql2date($campaign['affected_date_from'])
		. ($campaign['affected_date_to'] ? ' &mdash; ' . sql2date($campaign['affected_date_to']) : '')
		. '</td></tr>';
}
if ($campaign['regulatory_reference']) {
	echo '<tr><td class="label">' . _('Regulatory Reference') . ':</td><td colspan="3">' . htmlspecialchars($campaign['regulatory_reference']) . '</td></tr>';
}
if ($campaign['resolution']) {
	echo '<tr><td class="label">' . _('Resolution Approach') . ':</td><td colspan="3">' . htmlspecialchars($campaign['resolution']) . '</td></tr>';
}
if ($campaign['description']) {
	echo '<tr><td class="label">' . _('Description') . ':</td><td colspan="3">' . nl2br(htmlspecialchars($campaign['description'])) . '</td></tr>';
}
if ($campaign['notes']) {
	echo '<tr><td class="label">' . _('Notes') . ':</td><td colspan="3">' . nl2br(htmlspecialchars($campaign['notes'])) . '</td></tr>';
}

echo '<tr><td class="label">' . _('Created') . ':</td><td colspan="3">'
	. sql2date($campaign['created_at']) . ' ' . _('by') . ' ' . htmlspecialchars($campaign['created_by_name'])
	. '</td></tr>';
echo '</table>';

echo '<hr class="recall-separator">';

// =====================================================================
// Section 2: Progress Dashboard
// =====================================================================
echo '<h3>' . _('Progress Summary') . '</h3>';

// Summary cards
echo '<div class="recall-cards">';

$cards = array(
	array('label' => _('Total Affected'),     'value' => $progress['total'],          'color' => '#6c757d'),
	array('label' => _('Identified'),          'value' => $progress['identified'],     'color' => '#6c757d'),
	array('label' => _('Notified'),            'value' => $progress['notified'],       'color' => '#17a2b8'),
	array('label' => _('Returned'),            'value' => $progress['returned'],       'color' => '#ffc107'),
	array('label' => _('Resolved'),            'value' => $progress['resolved'],       'color' => '#28a745'),
	array('label' => _('Unreachable'),         'value' => $progress['unreachable'],    'color' => '#dc3545'),
);

foreach ($cards as $card) {
	echo '<div class="recall-card" style="background-color:' . $card['color'] . '15;border:1px solid ' . $card['color'] . '40;">';
	echo '<div class="value" style="color:' . $card['color'] . ';">' . $card['value'] . '</div>';
	echo '<div class="label" style="color:' . $card['color'] . ';">' . $card['label'] . '</div>';
	echo '</div>';
}

echo '</div>';

// Recovery progress bar
echo '<div style="margin:15px 0;">';
echo '<div style="font-size:13px;font-weight:600;margin-bottom:5px;">'
	. sprintf(_('Recovery Progress: %s%%'), $progress['pct_recovered']) . '</div>';
echo '<div class="recall-progress-bar">';
echo '<div class="recall-progress-fill" style="background:#28a745;width:' . $progress['pct_recovered'] . '%;"></div>';
echo '</div>';
echo '</div>';

// Notification progress bar
echo '<div style="margin:15px 0;">';
echo '<div style="font-size:13px;font-weight:600;margin-bottom:5px;">'
	. sprintf(_('Notification Progress: %s%%'), $progress['pct_notified']) . '</div>';
echo '<div class="recall-progress-bar">';
echo '<div class="recall-progress-fill" style="background:#17a2b8;width:' . $progress['pct_notified'] . '%;"></div>';
echo '</div>';
echo '</div>';

// Customer stats
if ($progress['customers_total'] > 0) {
	echo '<div style="margin:10px 0;padding:10px;background:#f8f9fa;border-radius:6px;">';
	echo '<strong>' . _('Customer Impact') . ':</strong> ';
	echo sprintf(_('%d total customers, %d notified, %d pending notification'),
		$progress['customers_total'], $progress['customers_notified'], $progress['customers_pending']);
	echo '</div>';
}

echo '<hr class="recall-separator">';

// =====================================================================
// Section 3: Batch Actions (noprint)
// =====================================================================
if (!in_array($campaign['status'], array('completed', 'cancelled')) && $progress['identified'] > 0) {
	echo '<div class="noprint">';
	echo '<h3>' . _('Batch Actions') . '</h3>';

	start_form(false, $_SERVER['PHP_SELF'] . '?id=' . $campaign_id);
	hidden('id', $campaign_id);

	echo '<div style="display:flex;gap:10px;flex-wrap:wrap;margin-bottom:15px;">';

	if ($progress['identified'] > 0) {
		echo '<div>';
		submit('batch_mark_notified', sprintf(_('Mark All Identified as Notified (%d items)'),
			$progress['identified']), true, '', 'fa-bell');
		echo '</div>';
	}

	echo '</div>';

	end_form();
	echo '</div>';
}

// =====================================================================
// Section 4: Customer Notification List
// =====================================================================
$customers = get_recall_customer_notification_list($campaign_id);

if (!empty($customers)) {
	echo '<h3>' . _('Customer Notification List') . '</h3>';

	start_table(TABLESTYLE, "width='100%'");
	$th = array(_('#'), _('Customer'), _('Ref'), _('Serials'), _('Batches'),
		_('Total Items'), _('First Notified'), _('Status'), '');
	table_header($th);

	$k = 0;
	foreach ($customers as $cust) {
		alt_table_row_color($k);

		label_cell($cust['customer_id']);
		label_cell(htmlspecialchars($cust['customer_name']));
		label_cell(htmlspecialchars($cust['debtor_ref']));

		// Truncate long serial/batch lists
		$serial_display = $cust['serial_numbers'] ? $cust['serial_numbers'] : '&mdash;';
		if (strlen($serial_display) > 60)
			$serial_display = substr($serial_display, 0, 57) . '...';
		label_cell(htmlspecialchars($serial_display));

		$batch_display = $cust['batch_numbers'] ? $cust['batch_numbers'] : '&mdash;';
		if (strlen($batch_display) > 40)
			$batch_display = substr($batch_display, 0, 37) . '...';
		label_cell(htmlspecialchars($batch_display));

		label_cell($cust['total_items'], "align='right'");
		label_cell($cust['first_notified'] ? sql2date($cust['first_notified']) : '&mdash;');

		// Status
		if ($cust['has_resolved'])
			label_cell('<span style="color:#28a745;font-weight:600;">' . _('Resolved') . '</span>');
		elseif ($cust['first_notified'])
			label_cell('<span style="color:#17a2b8;font-weight:600;">' . _('Notified') . '</span>');
		else
			label_cell('<span style="color:#dc3545;font-weight:600;">' . _('Pending') . '</span>');

		// Action: mark as notified
		echo '<td class="noprint">';
		if (!$cust['first_notified'] && !in_array($campaign['status'], array('completed', 'cancelled'))) {
			echo '<form method="POST" action="' . htmlspecialchars($_SERVER['PHP_SELF']) . '?id=' . $campaign_id . '" style="display:inline;">';
			echo '<input type="hidden" name="id" value="' . $campaign_id . '">';
			echo '<input type="hidden" name="notify_customer_id" value="' . $cust['customer_id'] . '">';
			echo '<button type="submit" name="mark_customer_notified" value="1" '
				. 'style="font-size:11px;padding:2px 8px;" title="' . _('Mark as notified') . '">'
				. '<i class="fa fa-bell"></i> ' . _('Notified') . '</button>';
			echo '</form>';
		}
		echo '</td>';

		end_row();
	}

	end_table(1);
}

echo '<hr class="recall-separator">';

// =====================================================================
// Section 5: All Affected Items
// =====================================================================
echo '<h3>' . _('Affected Items Detail') . '</h3>';

$items_result = get_recall_items($campaign_id);

start_table(TABLESTYLE, "width='100%'");
$th = array(_('#'), _('Serial No'), _('Batch No'), _('Customer'), _('Status'),
	_('Notified'), _('Returned'), _('Resolved'), _('Notes'));
table_header($th);

$k = 0;
$has_items = false;
while ($item = db_fetch($items_result)) {
	$has_items = true;
	alt_table_row_color($k);

	label_cell($item['id']);
	label_cell($item['serial_no'] ? htmlspecialchars($item['serial_no']) : '&mdash;');
	label_cell($item['batch_no'] ? htmlspecialchars($item['batch_no']) : '&mdash;');
	label_cell($item['customer_name'] ? htmlspecialchars($item['customer_name']) : '&mdash;');
	label_cell(recall_item_status_badge($item['status']));
	label_cell($item['notification_date'] ? sql2date($item['notification_date']) : '&mdash;');
	label_cell($item['return_date'] ? sql2date($item['return_date']) : '&mdash;');
	label_cell($item['resolution_date'] ? sql2date($item['resolution_date']) : '&mdash;');
	label_cell($item['notes'] ? htmlspecialchars($item['notes']) : '&mdash;');

	end_row();
}

if (!$has_items) {
	echo '<tr><td colspan="9" align="center">' . _('No affected items identified yet. Execute recall to identify affected items.') . '</td></tr>';
}

end_table(1);

echo '<hr class="recall-separator">';

// =====================================================================
// Section 6: Recall Timeline
// =====================================================================
$timeline = get_recall_campaign_timeline($campaign_id);

if (!empty($timeline)) {
	echo '<h3>' . _('Recall Timeline') . '</h3>';
	echo '<div class="recall-timeline">';

	foreach ($timeline as $event) {
		$event_color = '#6c757d';
		if ($event['type'] === 'notified') $event_color = '#17a2b8';
		elseif ($event['type'] === 'returned') $event_color = '#ffc107';
		elseif ($event['type'] === 'resolved') $event_color = '#28a745';
		elseif ($event['type'] === 'create') $event_color = '#007bff';

		echo '<div class="recall-timeline-item">';
		echo '<span class="date">' . ($event['date'] ? sql2date($event['date']) : '&mdash;') . '</span>';
		echo '<span class="action" style="color:' . $event_color . ';">' . htmlspecialchars($event['action']) . '</span>';
		echo '<span class="detail">' . htmlspecialchars($event['detail']) . '</span>';
		echo '</div>';
	}

	echo '</div>';
}

end_page();
