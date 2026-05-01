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

$page_security = 'SA_PURCHRFQ';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/ui/ui_lists.inc');
include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Purchase RFQ Inquiry'), false, false, '', $js);

$send_rfq_id = find_submit('SendNow');
if ($send_rfq_id > 0) {
	if (send_rfq_to_vendors($send_rfq_id))
		display_notification(_('RFQ has been marked as sent.'));
	else
		display_error(_('The RFQ could not be sent.'));
}

$filter_status = get_post('filter_status', '');
$filter_type = get_post('filter_type', '');
$filter_date_from = get_post('filter_date_from', '');
$filter_date_to = get_post('filter_date_to', '');

$rows = array();
$status_summary = array();

$rfq_result = get_purch_rfqs(
	$filter_status,
	$filter_date_from !== '' ? date2sql($filter_date_from) : '',
	$filter_date_to !== '' ? date2sql($filter_date_to) : '',
	$filter_type
);

while ($row = db_fetch($rfq_result)) {
	$rows[] = $row;
	if (!isset($status_summary[$row['status']]))
		$status_summary[$row['status']] = array('count' => 0, 'target_total' => 0);

	$status_summary[$row['status']]['count']++;
	$status_summary[$row['status']]['target_total'] += (float)$row['target_total'];
}

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();

$statuses = array('' => _('All Statuses')) + get_purch_rfq_statuses();
$types = array('' => _('All Types')) + get_purch_rfq_types();

echo '<td>'.array_selector('filter_status', $filter_status, $statuses, array('class' => array('nosearch'))).'</td>';
echo '<td>'.array_selector('filter_type', $filter_type, $types, array('class' => array('nosearch'))).'</td>';

end_row();
start_row();

date_cells(_('from:'), 'filter_date_from', $filter_date_from, null, -user_transaction_days());
date_cells(_('to:'), 'filter_date_to', $filter_date_to);
submit_cells('SearchRFQ', _('Apply Filter'), '', _('Filter RFQs'), 'default');
echo '<td>';
hyperlink_params($path_to_root . '/purchasing/purch_rfq_entry.php', _('New Purchase RFQ'), 'New=1');
echo '</td>';

end_row();
end_table(1);

if (!empty($status_summary)) {
	echo '<div style="display:flex;gap:10px;flex-wrap:wrap;margin:12px 0;">';
	foreach ($status_summary as $status => $summary) {
		echo '<div style="background:#fff;border:1px solid #ddd;border-left:4px solid ' . get_purch_rfq_status_color($status) . ';padding:8px 14px;border-radius:4px;min-width:180px;">';
		echo '<div style="font-size:13px;color:#666;">' . htmlspecialchars($statuses[$status]) . '</div>';
		echo '<div style="font-size:18px;font-weight:bold;">' . (int)$summary['count'] . '</div>';
		echo '<div style="font-size:12px;color:#666;">' . _('Target Total: ') . price_format($summary['target_total']) . '</div>';
		echo '</div>';
	}
	echo '</div>';
}

start_table(TABLESTYLE, "width='100%'");
table_header(array(
	_('Reference'),
	_('Type'),
	_('Status'),
	_('Description'),
	_('Created By'),
	_('Created Date'),
	_('Deadline'),
	_('Requisition'),
	_('Items'),
	_('Vendors'),
	_('Responses'),
	_('Target Total'),
	''
));

$k = 0;
$rfq_types = get_purch_rfq_types();
foreach ($rows as $row) {
	alt_table_row_color($k);
	label_cell('<a href="' . $path_to_root . '/purchasing/purch_rfq_entry.php?rfq_id=' . (int)$row['id'] . '">' . htmlspecialchars($row['reference']) . '</a>');
	label_cell($rfq_types[$row['rfq_type']]);
	label_cell(purch_rfq_status_badge($row['status']));
	label_cell($row['description'] ? $row['description'] : '-');
	label_cell($row['creator_name']);
	label_cell(sql2date($row['created_date']));
	label_cell($row['deadline_date'] ? sql2date($row['deadline_date']) : '-');
	label_cell($row['requisition_reference'] ? $row['requisition_reference'] : '-');
	label_cell((int)$row['item_count'], 'align=right');
	label_cell((int)$row['vendor_count'], 'align=right');
	label_cell((int)$row['responded_count'], 'align=right');
	amount_cell($row['target_total']);
	echo '<td nowrap>';
	echo '<a href="' . $path_to_root . '/purchasing/purch_rfq_entry.php?rfq_id=' . (int)$row['id'] . '">' . _('Open') . '</a>';
	if ((int)$row['responded_count'] > 0 || (int)$row['winner_count'] > 0) {
		echo '&nbsp;|&nbsp;<a href="' . $path_to_root . '/purchasing/purch_rfq_comparison.php?rfq_id=' . (int)$row['id'] . '">' . _('Compare') . '</a>';
	}
	if ($row['status'] === 'draft' && (int)$row['item_count'] > 0 && (int)$row['vendor_count'] > 0) {
		echo '&nbsp;';
		submit('SendNow' . $row['id'], _('Send'), true, _('Mark this RFQ as sent'));
	}
	echo '</td>';
	end_row();
}

if ($k == 0)
	label_row('', _('No purchase RFQs matched the selected filters.'), 'colspan=13 align=center');

end_table(1);

end_form();
end_page();