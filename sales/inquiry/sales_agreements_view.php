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
 * Sales Agreements Inquiry
 *
 * Lists all sales agreements with filters for customer, status, type, and
 * date range.  Displays a status-summary strip above the results table.
 *
 * @package NotrinosERP
 * @subpackage Sales
 */
$page_security = 'SA_SALESAGREEMENT';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/ui/ui_lists.inc');
include_once($path_to_root . '/sales/includes/db/sales_agreement_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Sales Agreements Inquiry'), false, false, '', $js);

// ============================================================================
// FILTERS
// ============================================================================

$filter_customer = get_post('filter_customer', 0);
$filter_status   = get_post('filter_status', '');
$filter_type     = get_post('filter_type', '');
$filter_date_from = get_post('filter_date_from', '');
$filter_date_to   = get_post('filter_date_to', '');

$agreements = get_sales_agreements(
	$filter_customer == ALL_TEXT ? 0 : (int)$filter_customer,
	$filter_status,
	false,
	$filter_date_from !== '' ? date2sql($filter_date_from) : '',
	$filter_date_to   !== '' ? date2sql($filter_date_to)   : ''
);

// Collect rows and status summary
$rows           = array();
$status_summary = array();
$statuses       = array('' => _('All Statuses')) + get_sales_agreement_statuses();
$types          = array('' => _('All Types'))    + get_sales_agreement_types();

while ($row = db_fetch($agreements)) {
	// Manual type filter (agreement_type column)
	if ($filter_type !== '' && $row['agreement_type'] !== $filter_type)
		continue;

	$rows[] = $row;
	$status = $row['status'];

	if (!isset($status_summary[$status])) {
		$status_summary[$status] = array(
			'count'            => 0,
			'total_committed'  => 0,
			'total_ordered'    => 0,
		);
	}

	$status_summary[$status]['count']++;
	$status_summary[$status]['total_committed'] += (float)$row['total_committed'];
	$status_summary[$status]['total_ordered']   += (float)$row['total_ordered'];
}

// ============================================================================
// FILTER FORM
// ============================================================================

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();
customer_list_cells(_('Customer:'), 'filter_customer', $filter_customer, true, true);
echo '<td>' . _('Status:') . ' ';
echo array_selector('filter_status', $filter_status, $statuses, array('class' => array('nosearch')));
echo '</td>';
echo '<td>' . _('Type:') . ' ';
echo array_selector('filter_type', $filter_type, $types, array('class' => array('nosearch')));
echo '</td>';
end_row();

start_row();
date_cells(_('From:'), 'filter_date_from', $filter_date_from, null, -user_transaction_days());
date_cells(_('To:'),   'filter_date_to',   $filter_date_to);
submit_cells('SearchAgreements', _('Apply Filter'), '', _('Filter agreements'), 'default');
echo '<td>';
hyperlink_params(
	$path_to_root . '/sales/sales_agreement_entry.php',
	_('New Sales Agreement'),
	'New=1'
);
echo '</td>';
end_row();
end_table(1);

end_form();

// ============================================================================
// STATUS SUMMARY STRIP
// ============================================================================

if (!empty($status_summary)) {
	echo '<div style="display:flex;gap:10px;flex-wrap:wrap;margin:12px 0;">';
	foreach ($status_summary as $status => $summary) {
		$label = isset($statuses[$status]) ? $statuses[$status] : $status;
		$color = get_sales_agreement_status_color($status);

		echo '<div style="background:#fff;border:1px solid #ddd;border-left:4px solid '
			. $color . ';padding:8px 14px;border-radius:4px;min-width:200px;">';
		echo '<div style="font-size:13px;color:#666;">' . htmlspecialchars($label) . '</div>';
		echo '<div style="font-size:18px;font-weight:bold;">' . (int)$summary['count'] . '</div>';
		echo '<div style="font-size:12px;color:#666;">'
			. _('Committed: ') . price_format($summary['total_committed']) . '</div>';
		echo '<div style="font-size:12px;color:#666;">'
			. _('Ordered: ') . price_format($summary['total_ordered']) . '</div>';
		echo '</div>';
	}
	echo '</div>';
}

// ============================================================================
// RESULTS TABLE
// ============================================================================

start_table(TABLESTYLE, "width='100%'");
table_header(array(
	_('Reference'),
	_('Type'),
	_('Customer'),
	_('Branch'),
	_('Start Date'),
	_('End Date'),
	_('Currency'),
	_('Status'),
	_('Committed'),
	_('Ordered'),
	_(''),
));

$k = 0;
foreach ($rows as $row) {
	alt_table_row_color($k);

	label_cell(
		'<a href="' . $path_to_root . '/sales/sales_agreement_entry.php?agreement_id='
		. $row['id'] . '">' . htmlspecialchars($row['reference']) . '</a>'
	);
	label_cell(
		isset($types[$row['agreement_type']]) ? $types[$row['agreement_type']] : $row['agreement_type']
	);
	label_cell(htmlspecialchars($row['customer_name']));
	label_cell(htmlspecialchars($row['branch_name'] ? $row['branch_name'] : '-'));
	label_cell(sql2date($row['date_start']));
	label_cell($row['date_end'] ? sql2date($row['date_end']) : '-');
	label_cell($row['currency']);
	label_cell(sales_agreement_status_badge($row['status']));
	amount_cell($row['total_committed']);
	amount_cell($row['total_ordered']);

	echo '<td nowrap>';
	edit_link_params(
		$path_to_root . '/sales/sales_agreement_entry.php',
		_('Edit'),
		'agreement_id=' . $row['id']
	);
	echo '</td>';

	end_row();
	$k++;
}

if ($k == 0)
	label_row('', _('No sales agreements found for the selected criteria.'), "colspan='11' align='center'");

end_table(1);

end_page();
