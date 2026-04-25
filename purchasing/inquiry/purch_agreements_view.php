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

$page_security = 'SA_PURCHAGREEMENT';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/ui/ui_lists.inc');
include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Purchase Agreements Inquiry'), false, false, '', $js);

$filter_supplier = get_post('filter_supplier', 0);
$filter_status = get_post('filter_status', '');
$filter_type = get_post('filter_type', '');
$filter_date_from = get_post('filter_date_from', '');
$filter_date_to = get_post('filter_date_to', '');

$agreements = get_purch_agreements(
	$filter_supplier == ALL_TEXT ? 0 : (int)$filter_supplier,
	$filter_status,
	false,
	$filter_type,
	$filter_date_from !== '' ? date2sql($filter_date_from) : '',
	$filter_date_to !== '' ? date2sql($filter_date_to) : ''
);

$rows = array();
$status_summary = array();
while ($row = db_fetch($agreements)) {
	$rows[] = $row;
	if (!isset($status_summary[$row['status']])) {
		$status_summary[$row['status']] = array(
			'count' => 0,
			'total_committed' => 0,
			'total_received' => 0,
		);
	}

	$status_summary[$row['status']]['count']++;
	$status_summary[$row['status']]['total_committed'] += (float)$row['total_committed'];
	$status_summary[$row['status']]['total_received'] += (float)$row['total_received'];
}

$statuses = array('' => _('All Statuses')) + get_purch_agreement_statuses();
$types = array('' => _('All Types')) + get_purch_agreement_types();

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();
supplier_list_cells(_('Supplier:'), 'filter_supplier', $filter_supplier, true, true);
echo '<td>' . _('Status:') . ' ';
echo array_selector('filter_status', $filter_status, $statuses, array('class' => array('nosearch')));
echo '</td>';
echo '<td>' . _('Type:') . ' ';
echo array_selector('filter_type', $filter_type, $types, array('class' => array('nosearch')));
echo '</td>';
end_row();

start_row();
date_cells(_('from:'), 'filter_date_from', $filter_date_from, null, -user_transaction_days());
date_cells(_('to:'), 'filter_date_to', $filter_date_to);
submit_cells('SearchAgreements', _('Apply Filter'), '', _('Filter agreements'), 'default');
echo '<td>';
	hyperlink_params($path_to_root . '/purchasing/purch_agreement_entry.php', _('New Purchase Agreement'), 'New=1&sel_app=AP');
echo '</td>';
end_row();
end_table(1);

if (!empty($status_summary)) {
	echo '<div style="display:flex;gap:10px;flex-wrap:wrap;margin:12px 0;">';
	foreach ($status_summary as $status => $summary) {
		echo '<div style="background:#fff;border:1px solid #ddd;border-left:4px solid ' . get_purch_agreement_status_color($status) . ';padding:8px 14px;border-radius:4px;min-width:200px;">';
		echo '<div style="font-size:13px;color:#666;">' . htmlspecialchars($statuses[$status]) . '</div>';
		echo '<div style="font-size:18px;font-weight:bold;">' . (int)$summary['count'] . '</div>';
		echo '<div style="font-size:12px;color:#666;">' . _('Committed: ') . price_format($summary['total_committed']) . '</div>';
		echo '<div style="font-size:12px;color:#666;">' . _('Received: ') . price_format($summary['total_received']) . '</div>';
		echo '</div>';
	}
	echo '</div>';
}

start_table(TABLESTYLE, "width='100%'");
table_header(array(
	_('Reference'),
	_('Type'),
	_('Status'),
	_('Supplier'),
	_('Start Date'),
	_('End Date'),
	_('Lines'),
	_('Remaining Qty'),
	_('Committed Total'),
	_('Ordered Total'),
	_('Received Total'),
	''
));

$k = 0;
foreach ($rows as $row) {
	alt_table_row_color($k);
	label_cell('<a href="' . $path_to_root . '/purchasing/purch_agreement_entry.php?agreement_id=' . (int)$row['id'] . '&amp;sel_app=AP">' . htmlspecialchars($row['reference']) . '</a>');
	label_cell($types[$row['agreement_type']]);
	label_cell(purch_agreement_status_badge($row['status']));
	label_cell($row['supp_name']);
	label_cell(sql2date($row['date_start']));
	label_cell($row['date_end'] ? sql2date($row['date_end']) : '-');
	label_cell((int)$row['line_count'], 'align=right');
	qty_cell($row['remaining_qty']);
	amount_cell($row['total_committed']);
	amount_cell($row['total_ordered']);
	amount_cell($row['total_received']);
	echo '<td nowrap>';
	echo '<a href="' . $path_to_root . '/purchasing/purch_agreement_entry.php?agreement_id=' . (int)$row['id'] . '&amp;sel_app=AP">' . _('Open') . '</a>';
	echo '</td>';
	end_row();
}

if ($k == 0)
	label_row('', _('No purchase agreements matched the selected filters.'), 'colspan=12 align=center');

end_table(1);

end_form();
end_page();