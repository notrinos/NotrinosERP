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
 * Sales RMA Inquiry
 *
 * Lists all RMAs with status/customer/date filters, analytics summary strip,
 * and action links to the entry form.
 *
 * @package NotrinosERP
 * @subpackage Sales
 */
$page_security = 'SA_SALESRETURN';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/ui/ui_lists.inc');
include_once($path_to_root . '/sales/includes/db/sales_rma_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Sales RMA Inquiry'), false, false, '', $js);

// ============================================================================
// FILTERS
// ============================================================================

$filter_customer  = get_post('filter_customer', 0);
$filter_status    = get_post('filter_status', '');
$filter_date_from = get_post('filter_date_from', '');
$filter_date_to   = get_post('filter_date_to', '');

$rmas_result = get_sales_rmas(
	$filter_customer == ALL_TEXT ? 0 : (int)$filter_customer,
	$filter_status,
	$filter_date_from !== '' ? date2sql($filter_date_from) : '',
	$filter_date_to   !== '' ? date2sql($filter_date_to)   : ''
);

// Collect rows and build status summary
$rows = array();
$status_counts = array();
$statuses = get_rma_statuses();

while ($row = db_fetch($rmas_result)) {
	$rows[] = $row;
	$s = $row['status'];
	$status_counts[$s] = isset($status_counts[$s]) ? $status_counts[$s] + 1 : 1;
}

// Analytics for current filter range
$analytics = get_rma_analytics(
	$filter_date_from !== '' ? date2sql($filter_date_from) : '',
	$filter_date_to   !== '' ? date2sql($filter_date_to)   : ''
);

// ============================================================================
// STATUS SUMMARY STRIP
// ============================================================================
echo '<div style="display:flex; gap:10px; flex-wrap:wrap; margin-bottom:14px; margin-top:4px;">';
foreach ($statuses as $code => $label) {
	$count = isset($status_counts[$code]) ? $status_counts[$code] : 0;
	$color = get_rma_status_color($code);
	echo '<div style="background:' . $color . '; color:#fff; padding:8px 16px; border-radius:5px; min-width:90px; text-align:center;">'
		. '<div style="font-size:22px; font-weight:bold;">' . $count . '</div>'
		. '<div style="font-size:11px;">' . $label . '</div>'
		. '</div>';
}
// Totals card
echo '<div style="background:#343a40; color:#fff; padding:8px 16px; border-radius:5px; min-width:120px; text-align:center;">'
	. '<div style="font-size:16px; font-weight:bold;">' . price_format((float)$analytics['total_refund_amount']) . '</div>'
	. '<div style="font-size:11px;">' . _('Total Refunds') . '</div>'
	. '</div>';
echo '</div>';

// ============================================================================
// FILTER FORM
// ============================================================================
start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();
echo '<td>'.customer_list('filter_customer', $filter_customer, true, true).'</td>';


$status_options = array('' => _('All Statuses')) + $statuses;
echo '<td>' .array_selector('filter_status', $filter_status, $status_options).'</td>';
date_cells(_('From:'), 'filter_date_from', $filter_date_from, null, -30);
date_cells(_('To:'), 'filter_date_to', $filter_date_to, null, 0);
submit_cells('filter_btn', _('Apply Filter'), '', _('Filter RMAs'), false);
end_row();
end_table(1);

end_form();

// ============================================================================
// RMA LIST TABLE
// ============================================================================
if (empty($rows)) {
	display_note(_('No RMAs found matching the current filter.'), 1);
} else {
	start_table(TABLESTYLE, "width='100%'");
	$th = array(
		_('RMA #'), _('Reference'), _('Customer'), _('Date'),
		_('Source'), _('Reason'), _('Method'), _('Refund Amount'),
		_('Status'), _('Actions')
	);
	table_header($th);

	$src_labels = array(
		ST_SALESINVOICE  => _('Invoice'),
		ST_CUSTDELIVERY  => _('Delivery'),
	);
	$methods = get_rma_return_methods();
	$k = 0;

	foreach ($rows as $row) {
		alt_table_row_color($k);
		$status_color = get_rma_status_color($row['status']);
		$status_label = isset($statuses[$row['status']]) ? $statuses[$row['status']] : $row['status'];

		label_cell('#' . $row['id']);
		label_cell($row['reference']);
		label_cell($row['customer_name']);
		label_cell(sql2date($row['request_date']));

		// Source document link
		if ($row['source_no'] > 0) {
			$src_lbl = isset($src_labels[$row['source_type']]) ? $src_labels[$row['source_type']] : _('Doc');
			if ($row['source_type'] == ST_SALESINVOICE)
				$src_url = $path_to_root . '/sales/view/view_invoice.php?trans_no=' . (int)$row['source_no'];
			else
				$src_url = $path_to_root . '/sales/view/view_dispatch.php?trans_no=' . (int)$row['source_no'];
			label_cell('<a href="' . $src_url . '">' . $src_lbl . ' #' . $row['source_no'] . '</a>');
		} else {
			label_cell('—');
		}

		label_cell($row['reason_description']);
		label_cell(isset($methods[$row['return_method']]) ? $methods[$row['return_method']] : $row['return_method']);
		amount_cell((float)$row['refund_amount']);

		// Status badge
		echo '<td><span style="background:' . $status_color
			. '; color:#fff; padding:2px 8px; border-radius:10px; font-size:11px; white-space:nowrap;">'
			. $status_label . '</span></td>';

		// Actions
		echo '<td style="white-space:nowrap;">';
		echo '<a href="' . $path_to_root . '/sales/sales_rma_entry.php?selected_id=' . (int)$row['id'] . '">'
			. _('View/Edit') . '</a>';
		if ($row['wh_return_order_id'] > 0) {
			echo ' &nbsp;|&nbsp; <a href="' . $path_to_root . '/inventory/warehouse/returns.php?selected_id=' . (int)$row['wh_return_order_id'] . '">'
				. _('WH Return') . '</a>';
		}
		if ($row['credit_note_no'] > 0) {
			echo ' &nbsp;|&nbsp; <a href="' . $path_to_root . '/sales/view/view_credit_note.php?trans_no=' . (int)$row['credit_note_no'] . '">'
				. _('Credit Note') . '</a>';
		}
		if ($row['replacement_order_no'] > 0) {
			echo ' &nbsp;|&nbsp; <a href="' . $path_to_root . '/sales/sales_order_entry.php?OrderNumber=' . (int)$row['replacement_order_no'] . '">'
				. _('Replacement SO') . '</a>';
		}
		echo '</td>';

		end_row();
	}
	end_table(1);
}

// ============================================================================
// NEW RMA BUTTON
// ============================================================================
echo '<div style="margin-top:10px;">';
echo '<a href="' . $path_to_root . '/sales/sales_rma_entry.php?New=1" class="button">' . _('New RMA') . '</a>';
echo '</div>';

end_page();
