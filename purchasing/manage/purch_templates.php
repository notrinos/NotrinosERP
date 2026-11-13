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

$page_security = 'SA_PURCHTEMPLATE';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/ui/ui_lists.inc');
include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');

page(_($help_context = 'Purchase Order Templates'));

/**
 * Validate the purchase template header inputs.
 *
 * @return bool
 */
function can_save_purchase_template_header()
{
	if (trim(get_post('template_name')) === '') {
		display_error(_('The template name is required.'));
		set_focus('template_name');
		return false;
	}

	return true;
}

/**
 * Validate the purchase template line inputs.
 *
 * @return bool
 */
function can_save_purchase_template_line($stock_field = 'line_stock_id', $quantity_field = 'line_default_quantity')
{
	if (trim(get_post($stock_field)) === '') {
		display_error(_('You must select an item.'));
		set_focus($stock_field);
		return false;
	}

    if (!check_num($quantity_field) || input_num($quantity_field) <= 0) {
		display_error(_('The default quantity must be greater than zero.'));
		set_focus($quantity_field);
		return false;
	}

	return true;
}

$selected_id = (int)get_post('selected_id', 0);
if (!$selected_id && isset($_GET['template_id']))
	$selected_id = (int)$_GET['template_id'];
if (isset($_GET['New']))
	$selected_id = 0;

if (isset($_POST['save_template']) && can_save_purchase_template_header()) {
	$template_header_intents = purchasing_classification_intents_from_values(
		get_post('template_dimension_id', 0), get_post('template_dimension2_id', 0),
		get_post('_template_unassigned_1', 0), get_post('_template_unassigned_2', 0),
		'template_header');
	if ($template_header_intents === false) {
		display_error(_('A header category cannot be both selected and explicitly unassigned.'));
		$selected_id = 0;
	} else {
	$selected_id = add_purch_template(
		trim(get_post('template_name')),
		trim(get_post('template_description')),
		get_post('template_supplier_id') == ALL_TEXT ? 0 : (int)get_post('template_supplier_id'),
		get_post('template_delivery_location') == ALL_TEXT ? '' : get_post('template_delivery_location'),
		get_post('template_payment_terms') == ALL_TEXT ? 0 : (int)get_post('template_payment_terms'),
		trim(get_post('template_notes')),
		check_value('template_inactive') ? 1 : 0,
		array('created_by_ui' => 1),
		$template_header_intents
	);
	if (!$selected_id) {
		display_error(_('The purchase order template could not be created.'));
		$selected_id = 0;
	} else {
		display_notification(_('Purchase order template has been created.'));
		meta_forward($_SERVER['PHP_SELF'], 'template_id=' . $selected_id . '&sel_app=AP');
	}
	}
}

if (isset($_POST['update_template']) && $selected_id > 0 && can_save_purchase_template_header()) {
	$template_header_intents = purchasing_classification_intents_from_values(
		get_post('template_dimension_id', 0), get_post('template_dimension2_id', 0),
		get_post('_template_unassigned_1', 0), get_post('_template_unassigned_2', 0),
		'template_header');
	// Metadata is omitted so retained Dimension references survive routine saves.
	$updated = $template_header_intents === false ? false : update_purch_template(
		$selected_id,
		trim(get_post('template_name')),
		trim(get_post('template_description')),
		get_post('template_supplier_id') == ALL_TEXT ? 0 : (int)get_post('template_supplier_id'),
		get_post('template_delivery_location') == ALL_TEXT ? '' : get_post('template_delivery_location'),
		get_post('template_payment_terms') == ALL_TEXT ? 0 : (int)get_post('template_payment_terms'),
		trim(get_post('template_notes')),
		check_value('template_inactive') ? 1 : 0,
		null,
		$template_header_intents
	);
	if ($template_header_intents === false)
		display_error(_('A header category cannot be both selected and explicitly unassigned.'));
	elseif ($updated)
		display_notification(_('Purchase order template has been updated.'));
	else
		display_error(_('The purchase order template could not be updated.'));
}

if (isset($_POST['delete_template']) && $selected_id > 0) {
	if (delete_purch_template($selected_id)) {
		display_notification(_('Purchase order template has been deleted.'));
		$selected_id = 0;
	} else {
		display_error(_('The purchase order template cannot be deleted while it retains a Dimension reference.'));
	}
}

if (isset($_POST['AddLine']) && $selected_id > 0 && can_save_purchase_template_line()) {
	$template_line_intents = purchasing_classification_intents_from_values(
		get_post('line_dimension_id', 0), get_post('line_dimension2_id', 0),
		get_post('_line_classification_unassigned_1', 0),
		get_post('_line_classification_unassigned_2', 0), 'template_line');
	if ($template_line_intents === false) {
		display_error(_('A line category cannot be both selected and explicitly unassigned.'));
	} elseif (add_template_line(
		$selected_id,
		trim(get_post('line_stock_id')),
		trim(get_post('line_description')),
		input_num('line_default_quantity'),
		(int)input_num('line_sort_order'),
		array('created_by_ui' => 1),
		$template_line_intents
	))
		display_notification(_('Template line has been added.'));
	else
		display_error(_('The template line could not be added.'));
}

$update_line_id = find_submit('UpdateLine');
if ($update_line_id > 0 && $selected_id > 0) {
	$line = get_template_line($update_line_id, $selected_id);
	if (!$line) {
		display_error(_('The selected template line no longer exists. Refresh and try again.'));
	} elseif (can_save_purchase_template_line('edit_stock_' . $update_line_id, 'edit_quantity_' . $update_line_id)) {
		$edit_line_intents = purchasing_classification_intents_from_values(
			get_post('edit_dimension_' . $update_line_id, 0),
			get_post('edit_dimension2_' . $update_line_id, 0),
			get_post('_edit_unassigned_1_' . $update_line_id, 0),
			get_post('_edit_unassigned_2_' . $update_line_id, 0), 'template_line');
		// Metadata is omitted (null) so retained Dimension references survive routine saves.
		if ($edit_line_intents !== false && update_template_line(
			$update_line_id,
			trim(get_post('edit_stock_' . $update_line_id)),
			trim(get_post('edit_description_' . $update_line_id)),
			input_num('edit_quantity_' . $update_line_id),
			(int)input_num('edit_sort_' . $update_line_id),
			null,
			$selected_id,
			$edit_line_intents
		))
			display_notification(_('Template line has been updated.'));
		else
			display_error(_('The template line could not be updated.'));
	}
}

$delete_line_id = find_submit('DeleteLine');
if ($delete_line_id > 0 && $selected_id > 0) {
	if (!get_template_line($delete_line_id, $selected_id)) {
		display_error(_('The selected template line no longer exists. Refresh and try again.'));
	} else {
		if (delete_template_line($delete_line_id, $selected_id))
			display_notification(_('Template line has been deleted.'));
		else
			display_error(_('The template line cannot be deleted while it retains a Dimension reference.'));
	}
}

$template = $selected_id > 0 ? get_purch_template($selected_id) : false;

if ($template) {
	$_POST['template_name'] = $template['name'];
	$_POST['template_description'] = $template['description'];
	$_POST['template_supplier_id'] = $template['supplier_id'];
	$_POST['template_delivery_location'] = $template['delivery_location'];
	$_POST['template_payment_terms'] = $template['default_payment_terms'];
	$_POST['template_notes'] = $template['notes'];
	$_POST['template_inactive'] = $template['inactive'];
	$template_header_values = purchasing_classification_carrier_values(
		array('classification_intents' => purchasing_classification_load_header_intents(ST_PURCHTEMPLATE,
			(int)$template['id'])));
	$_POST['template_dimension_id'] = $template_header_values['ids'][1];
	$_POST['template_dimension2_id'] = $template_header_values['ids'][2];
	$_POST['_template_unassigned_1'] = $template_header_values['unassigned'][1];
	$_POST['_template_unassigned_2'] = $template_header_values['unassigned'][2];
} else {
	if (!isset($_POST['template_supplier_id']))
		$_POST['template_supplier_id'] = 0;
	if (!isset($_POST['template_payment_terms']))
		$_POST['template_payment_terms'] = 0;
	if (!isset($_POST['template_inactive']))
		$_POST['template_inactive'] = 0;
	if (!isset($_POST['line_sort_order']))
		$_POST['line_sort_order'] = 10;
}

start_form();
hidden('selected_id', $selected_id);

echo '<div style="display:flex;justify-content:space-between;align-items:center;gap:12px;flex-wrap:wrap;">';
echo '<div><h2 style="margin:0;">' . ($template ? htmlspecialchars($template['name'], ENT_QUOTES, 'UTF-8') : _('New Purchase Template')) . '</h2></div>';
echo '<div>';
hyperlink_params($_SERVER['PHP_SELF'], _('Create New Template'), 'New=1&sel_app=AP');
echo '</div>';
echo '</div>';

br();

start_outer_table();

table_section(1);
text_row(_('Template Name:'), 'template_name', get_post('template_name'), 40, 100);
text_row(_('Description:'), 'template_description', get_post('template_description'), 50, 255);
echo '<tr><td class="label">' . _('Default Supplier:') . '</td>';
supplier_list_cells(null, 'template_supplier_id', get_post('template_supplier_id'), true, true);
echo '</tr>';
locations_list_row(_('Delivery Location:'), 'template_delivery_location', get_post('template_delivery_location'), true);

table_section(2);
payment_terms_list_row(_('Payment Terms:'), 'template_payment_terms', get_post('template_payment_terms'));
check_row(_('Inactive:'), 'template_inactive', get_post('template_inactive'));
textarea_row(_('Notes:'), 'template_notes', get_post('template_notes'), 40, 4);
if ((int)get_company_pref('use_dimension') > 0) {
	dimensions_list_row(dimension_category_policy_label(1).':', 'template_dimension_id',
		get_post('template_dimension_id', 0), true, _('Inherit supplier default'), false, 1, false);
	check_row(_('Leave this category explicitly unassigned'), '_template_unassigned_1',
		get_post('_template_unassigned_1', 0));
}
if ((int)get_company_pref('use_dimension') > 1) {
	dimensions_list_row(dimension_category_policy_label(2).':', 'template_dimension2_id',
		get_post('template_dimension2_id', 0), true, _('Inherit supplier default'), false, 2, false);
	check_row(_('Leave this category explicitly unassigned'), '_template_unassigned_2',
		get_post('_template_unassigned_2', 0));
}
end_outer_table(1);

if ($selected_id > 0)
	submit_center('update_template', _('Update Purchase Template'));
else
	submit_center('save_template', _('Create Purchase Template'));

if ($selected_id > 0) {
	br();
	display_heading(_('Template Lines'));
	start_table(TABLESTYLE, "width='100%'");
	table_header(array(_('Item'), _('Description'), _('Default Quantity'), _('Current Price'), _('Sort Order'), '', ''));
	$line_count = 0;
	$template_lines = get_template_lines($selected_id);
	while ($line = db_fetch($template_lines)) {
		$current_price = (int)$template['supplier_id'] > 0
			? get_purchase_price($template['supplier_id'], $line['stock_id'], $line['default_quantity'], date2sql(Today()), 0)
			: 0;
		alt_table_row_color($line_count);
		label_cell($line['stock_id']);
		echo '<td><input type="text" name="edit_description_' . $line['id'] . '" value="' . htmlspecialchars($line['description'], ENT_QUOTES, 'UTF-8') . '" size="35"></td>';
		echo '<td><input type="text" name="edit_quantity_' . $line['id'] . '" value="' . number_format2($line['default_quantity'], get_qty_dec($line['stock_id'])) . '" size="10"></td>';
		amount_cell($current_price);
		echo '<td><input type="text" name="edit_sort_' . $line['id'] . '" value="' . (int)$line['sort_order'] . '" size="6"></td>';
		echo '<td>';
		echo '<input type="hidden" name="edit_stock_' . $line['id'] . '" value="' . htmlspecialchars($line['stock_id'], ENT_QUOTES, 'UTF-8') . '">';
		echo '<input type="submit" class="inputsubmit" name="UpdateLine' . $line['id'] . '" value="' . _('Update') . '">';
		echo '</td>';
		echo '<td><input type="submit" class="inputsubmit" name="DeleteLine' . $line['id'] . '" value="' . _('Delete') . '"></td>';
		end_row();
		if ((int)get_company_pref('use_dimension') > 0) {
			$line_values = purchasing_classification_carrier_values(array('classification_intents' =>
				purchasing_classification_load_line_intents(ST_PURCHTEMPLATE, $selected_id, (int)$line['id'])));
			start_row();
			label_cell(_('Line tracking'), 'colspan=2');
			echo '<td colspan="5"><div style="display:flex;gap:18px;align-items:center;flex-wrap:wrap;">';
			echo '<label>'.htmlspecialchars(dimension_category_policy_label(1), ENT_QUOTES, 'UTF-8').' ';
			echo dimensions_list('edit_dimension_' . $line['id'], $line_values['ids'][1], true,
				_('Inherit header'), false, false, 1, false).' ';
			echo '<input type="checkbox" name="_edit_unassigned_1_' . $line['id'] . '" value="1"'
				.($line_values['unassigned'][1] ? ' checked' : '').'> '
				.htmlspecialchars(_('Explicitly unassigned'), ENT_QUOTES, 'UTF-8').'</label>';
			if ((int)get_company_pref('use_dimension') > 1) {
				echo '<label>'.htmlspecialchars(dimension_category_policy_label(2), ENT_QUOTES, 'UTF-8').' ';
				echo dimensions_list('edit_dimension2_' . $line['id'], $line_values['ids'][2], true,
					_('Inherit header'), false, false, 2, false).' ';
				echo '<input type="checkbox" name="_edit_unassigned_2_' . $line['id'] . '" value="1"'
					.($line_values['unassigned'][2] ? ' checked' : '').'> '
					.htmlspecialchars(_('Explicitly unassigned'), ENT_QUOTES, 'UTF-8').'</label>';
			}
			echo '</div></td>';
			end_row();
		}
		$line_count++;
	}
	if ($line_count == 0)
		label_row('', _('No template lines have been added yet.'), 'colspan=7 align=center');
	end_table(1);

	display_heading(_('Add Template Line'));
	start_table(TABLESTYLE2, "width='100%'");
	start_row();
	label_cell(_('Item:'));
	stock_costable_items_list_cells(null, 'line_stock_id', null, false, true);
	end_row();
	text_row(_('Description:'), 'line_description', null, 40, 255);
	qty_row(_('Default Quantity:'), 'line_default_quantity', null);
	text_row(_('Sort Order:'), 'line_sort_order', get_post('line_sort_order', 10), 8, 8);
	if ((int)get_company_pref('use_dimension') > 0) {
		dimensions_list_row(dimension_category_policy_label(1).':', 'line_dimension_id', 0,
			true, _('Inherit header'), false, 1, false);
		check_row(_('Leave this category explicitly unassigned'),
			'_line_classification_unassigned_1', 0);
	}
	if ((int)get_company_pref('use_dimension') > 1) {
		dimensions_list_row(dimension_category_policy_label(2).':', 'line_dimension2_id', 0,
			true, _('Inherit header'), false, 2, false);
		check_row(_('Leave this category explicitly unassigned'),
			'_line_classification_unassigned_2', 0);
	}
	end_table(1);
	submit_center('AddLine', _('Add Line'));

	br();
	submit_center('delete_template', _('Delete Purchase Template'));
	echo '<div style="margin-top:10px;text-align:center;">';
	hyperlink_params($path_to_root . '/purchasing/po_entry_items.php', _('Create Purchase Order From Template'), 'NewOrder=Yes&template_id=' . (int)$selected_id . '&sel_app=AP');
	echo '</div>';
}

br();
display_heading(_('Existing Templates'));
start_table(TABLESTYLE, "width='100%'");
table_header(array(_('Name'), _('Supplier'), _('Location'), _('Lines'), _('Status'), ''));

$template_rows = get_purch_templates(0, true);
$row_number = 0;
while ($row = db_fetch($template_rows)) {
	alt_table_row_color($row_number);
	label_cell('<a href="' . $_SERVER['PHP_SELF'] . '?template_id=' . (int)$row['id'] . '&amp;sel_app=AP">' . htmlspecialchars($row['name'], ENT_QUOTES, 'UTF-8') . '</a>');
	label_cell($row['supp_name'] ? $row['supp_name'] : '-');
	label_cell($row['location_name'] ? $row['location_name'] : '-');
	label_cell((int)$row['line_count'], 'align=right');
	label_cell($row['inactive'] ? _('Inactive') : _('Active'));
	label_cell('<a href="' . $_SERVER['PHP_SELF'] . '?template_id=' . (int)$row['id'] . '&amp;sel_app=AP">' . _('Open') . '</a>');
	end_row();
	$row_number++;
}

if ($row_number == 0)
	label_row('', _('No purchase templates have been created yet.'), 'colspan=6 align=center');

end_table(1);

end_form();
end_page();
