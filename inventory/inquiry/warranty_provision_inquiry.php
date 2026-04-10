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
 * Warranty Provision Inquiry — View provision log, balances, and summary.
 *
 * Access: SA_WARRANTY_PROVISION
 */
$page_security = 'SA_WARRANTY_PROVISION';
$path_to_root = '../..';
include_once($path_to_root . '/includes/db_pager.inc');
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Warranty Provision Inquiry');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page($_SESSION['page_title'], false, false, '', $js);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/inventory/includes/db/warranty_provision_db.inc');

//----------------------------------------------------------------------
// Filter form
//----------------------------------------------------------------------
start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();

customer_list_cells(_('Customer:'), 'customer_id', null, true, true);

stock_items_list_cells(_('Item:'), 'stock_id', null, true, true);

$type_options = array(
	''        => _('All Types'),
	'accrual' => _('Accruals'),
	'release' => _('Releases'),
);
filter_cell_open(_('Type:'));
echo array_selector('provision_type', get_post('provision_type', ''),
	$type_options, array('select_submit' => true));
filter_cell_close();

date_cells(_('From:'), 'from_date', '', null, -365);
date_cells(_('To:'), 'to_date', '', null);

submit_cells('RefreshInquiry', _('Apply Filter'), '', _('Refresh provision log'), 'default');

end_row();
end_table();

if (get_post('RefreshInquiry') || list_updated('customer_id')
	|| list_updated('stock_id') || list_updated('provision_type'))
{
	$Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Summary section
//----------------------------------------------------------------------
$total_balance = get_warranty_provision_balance(
	get_post('customer_id', '') != ALL_TEXT ? get_post('customer_id', 0) : 0
);

echo "<div style='display:flex; flex-wrap:wrap; gap:10px; margin:10px 0;'>";

echo "<div style='background:#17a2b8;color:#fff;padding:12px 20px;border-radius:6px;min-width:200px;text-align:center;'>"
	. "<div style='font-size:24px;font-weight:bold;'>" . price_format($total_balance) . "</div>"
	. "<div style='font-size:12px;opacity:0.9;'>" . _('Outstanding Provision Balance') . "</div>"
	. "</div>";

echo "</div>";

//----------------------------------------------------------------------
// Summary by item
//----------------------------------------------------------------------
echo "<h3 style='margin:15px 0 5px 0; border-bottom:1px solid #ddd; padding-bottom:3px;'>"
	. _('Provision Balance by Item') . "</h3>";

$summary = get_warranty_provision_summary_by_item();
if (db_num_rows($summary) == 0) {
	display_note(_('No warranty provisions recorded yet.'));
} else {
	start_table(TABLESTYLE, "width='60%'");
	$th = array(_('Item Code'), _('Description'), _('Accrued'), _('Released'), _('Balance'));
	table_header($th);

	$k = 0;
	$total_accrued = $total_released = $total_bal = 0;
	while ($row = db_fetch($summary)) {
		alt_table_row_color($k);
		label_cell($row['stock_id']);
		label_cell($row['item_description']);
		amount_cell($row['accrued']);
		amount_cell($row['released']);
		amount_cell($row['balance']);
		end_row();
		$total_accrued += $row['accrued'];
		$total_released += $row['released'];
		$total_bal += $row['balance'];
	}

	// Totals
	start_row("class='inquirybg'");
	label_cell('<strong>' . _('Totals') . '</strong>', 'colspan=2');
	amount_cell($total_accrued);
	amount_cell($total_released);
	amount_cell($total_bal);
	end_row();

	end_table();
}

//----------------------------------------------------------------------
// Provision log detail
//----------------------------------------------------------------------
echo "<h3 style='margin:20px 0 5px 0; border-bottom:1px solid #ddd; padding-bottom:3px;'>"
	. _('Provision Log') . "</h3>";

$customer_filter = (get_post('customer_id', '') != '' && get_post('customer_id') != ALL_TEXT)
	? (int)get_post('customer_id') : 0;

$log = get_warranty_provision_log(
	$customer_filter,
	get_post('stock_id', ''),
	get_post('provision_type', ''),
	get_post('from_date', ''),
	get_post('to_date', '')
);

if (db_num_rows($log) == 0) {
	display_note(_('No provision log entries found for the selected filters.'));
} else {
	start_table(TABLESTYLE, "width='95%'");
	$th = array(_('Date'), _('Type'), _('Trans'), _('Item'), _('Serial/Batch'),
		_('Customer'), _('Amount'), _('Notes'));
	table_header($th);

	$k = 0;
	while ($entry = db_fetch($log)) {
		alt_table_row_color($k);

		label_cell(sql2date($entry['tran_date']));

		// Provision type badge
		if ($entry['provision_type'] == 'accrual') {
			label_cell("<span style='background:#dc3545;color:#fff;padding:2px 6px;border-radius:3px;font-size:11px;'>"
				. _('Accrual') . "</span>");
		} else {
			label_cell("<span style='background:#28a745;color:#fff;padding:2px 6px;border-radius:3px;font-size:11px;'>"
				. _('Release') . "</span>");
		}

		// Transaction reference
		label_cell(get_trans_view_str($entry['trans_type'], $entry['trans_no']));

		label_cell($entry['stock_id'] . ' - ' . $entry['item_description']);

		// Serial or Batch
		$serial_batch = '';
		if ($entry['serial_no'])
			$serial_batch = _('S/N: ') . $entry['serial_no'];
		if ($entry['batch_no'])
			$serial_batch .= ($serial_batch ? ' / ' : '') . _('Batch: ') . $entry['batch_no'];
		label_cell($serial_batch ? $serial_batch : '-');

		label_cell($entry['customer_name']);
		amount_cell($entry['provision_type'] == 'accrual' ? $entry['amount'] : -$entry['amount']);
		label_cell($entry['notes'] ? $entry['notes'] : '');
		end_row();
	}
	end_table();
}

end_form();
end_page();
