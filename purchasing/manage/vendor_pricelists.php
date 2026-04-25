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

$page_security = 'SA_VENDORPRICELIST';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/ui/ui_lists.inc');
include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Vendor Pricelists'), false, false, '', $js);

/**
 * Validate the vendor pricelist form.
 *
 * @return bool
 */
function can_save_vendor_pricelist_form()
{
	if ((int)get_post('pricelist_supplier_id') <= 0 || get_post('pricelist_supplier_id') == ALL_TEXT) {
		display_error(_('You must select a supplier.'));
		set_focus('pricelist_supplier_id');
		return false;
	}

	if (trim(get_post('pricelist_stock_id')) === '') {
		display_error(_('You must select an item.'));
		set_focus('pricelist_stock_id');
		return false;
	}

	if (!check_num('pricelist_conversion_factor', 0)) {
		display_error(_('The conversion factor must be greater than zero.'));
		set_focus('pricelist_conversion_factor');
		return false;
	}

	if (get_post('pricelist_valid_from') !== '' && !is_date(get_post('pricelist_valid_from'))) {
		display_error(_('The valid-from date is invalid.'));
		set_focus('pricelist_valid_from');
		return false;
	}

	if (get_post('pricelist_valid_until') !== '' && !is_date(get_post('pricelist_valid_until'))) {
		display_error(_('The valid-until date is invalid.'));
		set_focus('pricelist_valid_until');
		return false;
	}

	if (get_post('pricelist_valid_from') !== '' && get_post('pricelist_valid_until') !== ''
		&& is_date(get_post('pricelist_valid_from')) && is_date(get_post('pricelist_valid_until'))
		&& date1_greater_date2(get_post('pricelist_valid_from'), get_post('pricelist_valid_until'))) {
		display_error(_('The valid-until date cannot be earlier than the valid-from date.'));
		set_focus('pricelist_valid_until');
		return false;
	}

	return true;
}

/**
 * Extract the pricelist quantity break inputs.
 *
 * @return array
 */
function get_vendor_pricelist_form_prices()
{
	return array(
		'price_break_qty_1' => input_num('pricelist_break_qty_1'),
		'price_1' => input_num('pricelist_price_1'),
		'price_break_qty_2' => input_num('pricelist_break_qty_2'),
		'price_2' => input_num('pricelist_price_2'),
		'price_break_qty_3' => input_num('pricelist_break_qty_3'),
		'price_3' => input_num('pricelist_price_3'),
		'price_break_qty_4' => input_num('pricelist_break_qty_4'),
		'price_4' => input_num('pricelist_price_4'),
	);
}

/**
 * Populate the edit form from one vendor pricelist row.
 *
 * @param array $entry
 * @return void
 */
function load_vendor_pricelist_form($entry)
{
	$_POST['selected_id'] = $entry['id'];
	$_POST['pricelist_supplier_id'] = $entry['supplier_id'];
	$_POST['pricelist_stock_id'] = $entry['stock_id'];
	$_POST['pricelist_vendor_product_code'] = $entry['vendor_product_code'];
	$_POST['pricelist_vendor_product_name'] = $entry['vendor_product_name'];
	$_POST['pricelist_vendor_uom'] = $entry['vendor_uom'];
	$_POST['pricelist_conversion_factor'] = $entry['conversion_factor'];
	$_POST['pricelist_currency'] = $entry['currency'];
	$_POST['pricelist_min_order_qty'] = $entry['min_order_qty'];
	$_POST['pricelist_break_qty_1'] = $entry['price_break_qty_1'];
	$_POST['pricelist_price_1'] = $entry['price_1'];
	$_POST['pricelist_break_qty_2'] = $entry['price_break_qty_2'];
	$_POST['pricelist_price_2'] = $entry['price_2'];
	$_POST['pricelist_break_qty_3'] = $entry['price_break_qty_3'];
	$_POST['pricelist_price_3'] = $entry['price_3'];
	$_POST['pricelist_break_qty_4'] = $entry['price_break_qty_4'];
	$_POST['pricelist_price_4'] = $entry['price_4'];
	$_POST['pricelist_lead_time_days'] = $entry['lead_time_days'];
	$_POST['pricelist_valid_from'] = $entry['valid_from'] ? sql2date($entry['valid_from']) : '';
	$_POST['pricelist_valid_until'] = $entry['valid_until'] ? sql2date($entry['valid_until']) : '';
	$_POST['pricelist_discount_percent'] = $entry['discount_percent'];
	$_POST['pricelist_notes'] = $entry['notes'];
	$_POST['pricelist_is_preferred'] = $entry['is_preferred'];
	$_POST['pricelist_inactive'] = $entry['inactive'];
}

/**
 * Reset the vendor pricelist form to default values.
 *
 * @return void
 */
function reset_vendor_pricelist_form()
{
	$_POST['selected_id'] = 0;
	$_POST['pricelist_supplier_id'] = 0;
	$_POST['pricelist_stock_id'] = '';
	$_POST['pricelist_vendor_product_code'] = '';
	$_POST['pricelist_vendor_product_name'] = '';
	$_POST['pricelist_vendor_uom'] = '';
	$_POST['pricelist_conversion_factor'] = 1;
	$_POST['pricelist_currency'] = '';
	$_POST['pricelist_min_order_qty'] = 0;
	$_POST['pricelist_break_qty_1'] = 0;
	$_POST['pricelist_price_1'] = 0;
	$_POST['pricelist_break_qty_2'] = 0;
	$_POST['pricelist_price_2'] = 0;
	$_POST['pricelist_break_qty_3'] = 0;
	$_POST['pricelist_price_3'] = 0;
	$_POST['pricelist_break_qty_4'] = 0;
	$_POST['pricelist_price_4'] = 0;
	$_POST['pricelist_lead_time_days'] = 0;
	$_POST['pricelist_valid_from'] = '';
	$_POST['pricelist_valid_until'] = '';
	$_POST['pricelist_discount_percent'] = 0;
	$_POST['pricelist_notes'] = '';
	$_POST['pricelist_is_preferred'] = 0;
	$_POST['pricelist_inactive'] = 0;
}

$selected_id = (int)get_post('selected_id', 0);
$edit_id = find_submit('Edit');
$delete_id = find_submit('Delete');

if (!empty($_POST))
	$Ajax->activate('_page_body');

if ($edit_id > 0)
	$selected_id = $edit_id;

if ($delete_id > 0) {
	delete_vendor_pricelist_entry($delete_id);
	display_notification(_('Vendor pricelist entry has been deleted.'));
	$selected_id = 0;
}

if ((isset($_POST['save_pricelist']) || isset($_POST['update_pricelist'])) && can_save_vendor_pricelist_form()) {
	$prices = get_vendor_pricelist_form_prices();
	if ((int)get_post('selected_id') > 0) {
		update_vendor_pricelist_entry(
			(int)get_post('selected_id'),
			(int)get_post('pricelist_supplier_id'),
			trim(get_post('pricelist_stock_id')),
			trim(get_post('pricelist_vendor_product_code')),
			trim(get_post('pricelist_currency')),
			$prices,
			trim(get_post('pricelist_vendor_product_name')),
			trim(get_post('pricelist_vendor_uom')),
			input_num('pricelist_conversion_factor', 1),
			input_num('pricelist_min_order_qty'),
			(int)input_num('pricelist_lead_time_days'),
			get_post('pricelist_valid_from') !== '' ? date2sql(get_post('pricelist_valid_from')) : '',
			get_post('pricelist_valid_until') !== '' ? date2sql(get_post('pricelist_valid_until')) : '',
			input_num('pricelist_discount_percent'),
			trim(get_post('pricelist_notes')),
			check_value('pricelist_is_preferred') ? 1 : 0,
			check_value('pricelist_inactive') ? 1 : 0,
			array('updated_by_ui' => 1)
		);
		display_notification(_('Vendor pricelist entry has been updated.'));
	} else {
		$selected_id = add_vendor_pricelist_entry(
			(int)get_post('pricelist_supplier_id'),
			trim(get_post('pricelist_stock_id')),
			trim(get_post('pricelist_vendor_product_code')),
			trim(get_post('pricelist_currency')),
			$prices,
			trim(get_post('pricelist_vendor_product_name')),
			trim(get_post('pricelist_vendor_uom')),
			input_num('pricelist_conversion_factor', 1),
			input_num('pricelist_min_order_qty'),
			(int)input_num('pricelist_lead_time_days'),
			get_post('pricelist_valid_from') !== '' ? date2sql(get_post('pricelist_valid_from')) : '',
			get_post('pricelist_valid_until') !== '' ? date2sql(get_post('pricelist_valid_until')) : '',
			input_num('pricelist_discount_percent'),
			trim(get_post('pricelist_notes')),
			check_value('pricelist_is_preferred') ? 1 : 0,
			check_value('pricelist_inactive') ? 1 : 0,
			array('created_by_ui' => 1)
		);
		display_notification(_('Vendor pricelist entry has been added.'));
	}
}

if (isset($_POST['reset_pricelist'])) {
	$selected_id = 0;
	reset_vendor_pricelist_form();
}

if (isset($_POST['import_pricelist'])) {
	$import_supplier_id = (int)get_post('import_supplier_id');
	if ($import_supplier_id <= 0 || get_post('import_supplier_id') == ALL_TEXT) {
		display_error(_('Select a supplier before importing a pricelist CSV.'));
	} elseif (!isset($_FILES['csv_file']) || $_FILES['csv_file']['error'] != UPLOAD_ERR_OK) {
		display_error(_('Please choose a valid CSV file.'));
	} else {
		$csv_data = file_get_contents($_FILES['csv_file']['tmp_name']);
		if ($csv_data === false) {
			display_error(_('The uploaded CSV file could not be read.'));
		} else {
			$import_result = import_vendor_pricelist($import_supplier_id, $csv_data);
			if (!empty($import_result['errors']))
				display_error(implode('<br>', $import_result['errors']));
			display_notification(sprintf(_('Vendor pricelist import complete. Imported: %s'), (int)$import_result['imported']));
		}
	}
}

if ($selected_id > 0) {
	$selected_entry = get_vendor_pricelist_entry($selected_id);
	if ($selected_entry)
		load_vendor_pricelist_form($selected_entry);
} elseif (!isset($_POST['selected_id'])) {
	reset_vendor_pricelist_form();
}

$filter_supplier_id = get_post('filter_supplier_id', 0);
$filter_stock_id = get_post('filter_stock_id', '');
$show_inactive = check_value('show_inactive');

start_form(true);

display_heading(_('Vendor Pricelist Filters'));
start_table(TABLESTYLE2, "width='100%'");
start_row();
	supplier_list_cells(_('Supplier:'), 'filter_supplier_id', $filter_supplier_id, true, true);
	stock_costable_items_list_cells(_('Item:'), 'filter_stock_id', $filter_stock_id, true, true);
	check_cells(_('Show Inactive:'), 'show_inactive', $show_inactive);
	submit_cells('filter_pricelists', _('Apply Filter'), '', _('Apply filter'), 'default');
end_row();
end_table(1);

display_heading(_('Vendor Pricelist Entries'));
start_table(TABLESTYLE, "width='100%'");
table_header(array(
	_('Supplier'),
	_('Item'),
	_('Vendor Code'),
	_('Vendor Name'),
	_('Currency'),
	_('Best Unit Price'),
	_('Lead Time'),
	_('Validity'),
	_('Flags'),
	'',
	''
));

$entries = get_vendor_pricelist($filter_supplier_id == ALL_TEXT ? 0 : (int)$filter_supplier_id, $filter_stock_id == ALL_TEXT ? '' : $filter_stock_id, !$show_inactive);
$k = 0;
while ($entry = db_fetch($entries)) {
	$entry = enrich_vendor_pricelist_row($entry, 1);
	alt_table_row_color($k);
	label_cell($entry['supp_name']);
	label_cell($entry['stock_id']);
	label_cell($entry['vendor_product_code']);
	label_cell($entry['vendor_product_name'] !== '' ? $entry['vendor_product_name'] : $entry['stock_description']);
	label_cell($entry['currency']);
	amount_cell($entry['effective_unit_price']);
	label_cell((int)$entry['lead_time_days'] . ' ' . _('days'));
	label_cell(($entry['valid_from'] ? sql2date($entry['valid_from']) : '-') . ' - ' . ($entry['valid_until'] ? sql2date($entry['valid_until']) : '-'));
	$flags = array();
	if ($entry['is_preferred'])
		$flags[] = _('Preferred');
	if ($entry['inactive'])
		$flags[] = _('Inactive');
	label_cell(count($flags) ? implode(', ', $flags) : '-');
	edit_button_cell('Edit' . $entry['id'], _('Edit'));
	delete_button_cell('Delete' . $entry['id'], _('Delete'));
	end_row();
	$k++;
}

if ($k == 0)
	label_row('', _('No vendor pricelist entries matched the selected filters.'), 'colspan=11 align=center');

end_table(2);

if ($filter_stock_id !== '' && $filter_stock_id != ALL_TEXT) {
	display_heading(_('Vendor Price Comparison'));
	$comparison_rows = get_item_vendors($filter_stock_id, true, 1, '', date2sql(Today()));
	start_table(TABLESTYLE, "width='75%'");
	table_header(array(_('Supplier'), _('Source'), _('Unit Price'), _('Lead Time')));
	$comparison_count = 0;
	foreach ($comparison_rows as $comparison_row) {
		alt_table_row_color($comparison_count);
		label_cell($comparison_row['supplier_name']);
		label_cell(get_purchase_price_source_label($comparison_row['source']));
		amount_cell($comparison_row['price']);
		label_cell((int)$comparison_row['lead_time_days'] . ' ' . _('days'));
		end_row();
		$comparison_count++;
	}
	if ($comparison_count == 0)
		label_row('', _('No vendor price comparison data is available for the selected item.'), 'colspan=4 align=center');
	end_table(2);
}

display_heading($selected_id > 0 ? _('Edit Vendor Pricelist Entry') : _('Add Vendor Pricelist Entry'));
start_table(TABLESTYLE2, "width='100%'");
hidden('selected_id', $selected_id);
echo '<tr><td class="label">' . _('Supplier:') . '</td>';
supplier_list_cells(null, 'pricelist_supplier_id', get_post('pricelist_supplier_id'), false, true);
echo '</tr>';
start_row();
	stock_costable_items_list_cells(_('Item:'), 'pricelist_stock_id', get_post('pricelist_stock_id'), false, true);
end_row();
text_row(_('Vendor Product Code:'), 'pricelist_vendor_product_code', get_post('pricelist_vendor_product_code'), 30, 50);
text_row(_('Vendor Product Name:'), 'pricelist_vendor_product_name', get_post('pricelist_vendor_product_name'), 40, 100);
text_row(_('Vendor UOM:'), 'pricelist_vendor_uom', get_post('pricelist_vendor_uom'), 20, 20);
text_row(_('Currency:'), 'pricelist_currency', get_post('pricelist_currency'), 8, 8);
amount_row(_('Conversion Factor:'), 'pricelist_conversion_factor', get_post('pricelist_conversion_factor', 1));
qty_row(_('Minimum Order Qty:'), 'pricelist_min_order_qty', get_post('pricelist_min_order_qty', 0));
start_row();
	label_cell(_('Break Qty 1:'));
	text_cells(null, 'pricelist_break_qty_1', get_post('pricelist_break_qty_1', 0), 10, 12);
	label_cell(_('Price 1:'));
	text_cells(null, 'pricelist_price_1', get_post('pricelist_price_1', 0), 10, 12);
end_row();
start_row();
	label_cell(_('Break Qty 2:'));
	text_cells(null, 'pricelist_break_qty_2', get_post('pricelist_break_qty_2', 0), 10, 12);
	label_cell(_('Price 2:'));
	text_cells(null, 'pricelist_price_2', get_post('pricelist_price_2', 0), 10, 12);
end_row();
start_row();
	label_cell(_('Break Qty 3:'));
	text_cells(null, 'pricelist_break_qty_3', get_post('pricelist_break_qty_3', 0), 10, 12);
	label_cell(_('Price 3:'));
	text_cells(null, 'pricelist_price_3', get_post('pricelist_price_3', 0), 10, 12);
end_row();
start_row();
	label_cell(_('Break Qty 4:'));
	text_cells(null, 'pricelist_break_qty_4', get_post('pricelist_break_qty_4', 0), 10, 12);
	label_cell(_('Price 4:'));
	text_cells(null, 'pricelist_price_4', get_post('pricelist_price_4', 0), 10, 12);
end_row();
text_row(_('Lead Time (Days):'), 'pricelist_lead_time_days', get_post('pricelist_lead_time_days', 0), 8, 8);
date_row(_('Valid From:'), 'pricelist_valid_from', get_post('pricelist_valid_from'));
date_row(_('Valid Until:'), 'pricelist_valid_until', get_post('pricelist_valid_until'));
amount_row(_('Discount Percent:'), 'pricelist_discount_percent', get_post('pricelist_discount_percent', 0));
check_row(_('Preferred Vendor for Item:'), 'pricelist_is_preferred', get_post('pricelist_is_preferred'));
check_row(_('Inactive:'), 'pricelist_inactive', get_post('pricelist_inactive'));
textarea_row(_('Notes:'), 'pricelist_notes', get_post('pricelist_notes'), 40, 4);
end_table(1);

if ($selected_id > 0)
	submit_center('update_pricelist', _('Update Vendor Pricelist Entry'));
else
	submit_center('save_pricelist', _('Add Vendor Pricelist Entry'));
submit_center('reset_pricelist', _('Reset Form'));

display_heading(_('Import Vendor Pricelist CSV'));
start_table(TABLESTYLE2, "width='100%'");
echo '<tr><td class="label">' . _('Supplier:') . '</td>';
supplier_list_cells(null, 'import_supplier_id', get_post('import_supplier_id'), false, false);
echo '</tr>';
file_row(_('CSV File:'), 'csv_file', 'csv_file');
label_row(_('Expected columns:'), _('stock_id, vendor_product_code, vendor_product_name, vendor_uom, conversion_factor, currency, min_order_qty, price_break_qty_1, price_1, price_break_qty_2, price_2, price_break_qty_3, price_3, price_break_qty_4, price_4, lead_time_days, valid_from, valid_until, discount_percent, notes, is_preferred, inactive'));
end_table(1);
submit_center('import_pricelist', _('Import Vendor Pricelist'));

end_form();
end_page();