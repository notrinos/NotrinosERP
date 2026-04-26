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
 * Quotation Templates Management
 *
 * Full CRUD for sales quotation templates with inline line editor.
 * Templates allow pre-defining required and optional/upsell products
 * to load into a new quotation in one click.
 *
 * NOTE: This manages "Quotation Templates" â€” a distinct concept from
 * "Invoice Templates" (so_type=1 on recurrent invoices, managed via
 * SA_STEMPLATE). Labels and page title are intentionally different.
 *
 * @package NotrinosERP
 * @subpackage Sales
 */
$page_security = 'SA_SALESQUOTETPL';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

page(_($help_context = 'Quotation Templates'));

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/sales/includes/db/sales_quotation_template_db.inc');

simple_page_mode(true);

// ============================================================================
// VALIDATION
// ============================================================================

/**
 * Validate template header form data before save.
 *
 * @return bool True if valid.
 */
function can_process_template()
{
	if (strlen(trim(get_post('tpl_name'))) == 0) {
		display_error(_('Template name cannot be empty.'));
		set_focus('tpl_name');
		return false;
	}
	if (!check_num('tpl_validity_days', 1)) {
		display_error(_('Validity days must be a positive number.'));
		set_focus('tpl_validity_days');
		return false;
	}
	return true;
}

/**
 * Validate template line form data before save.
 *
 * @return bool True if valid.
 */
function can_process_line()
{
	$line_type = get_post('line_line_type', 'product');
	if (in_array($line_type, array('product', 'optional'))) {
		if (strlen(trim(get_post('line_stock_id'))) == 0) {
			display_error(_('Stock item is required for product lines.'));
			set_focus('line_stock_id');
			return false;
		}
	}
	if (!check_num('line_quantity', 0)) {
		display_error(_('Quantity must be a valid non-negative number.'));
		set_focus('line_quantity');
		return false;
	}
	if (!check_num('line_discount', 0, 100)) {
		display_error(_('Discount must be between 0 and 100.'));
		set_focus('line_discount');
		return false;
	}
	return true;
}

// ============================================================================
// TEMPLATE CRUD OPERATIONS
// ============================================================================

if ($Mode == 'ADD_ITEM' && can_process_template()) {
	$id = add_quotation_template(
		get_post('tpl_name'),
		get_post('tpl_description'),
		(int)get_post('tpl_validity_days', 30),
		get_post('tpl_terms'),
		get_post('tpl_notes'),
		(int)get_post('tpl_sales_type', 0),
		(int)get_post('tpl_payment_terms', 0),
		(int)get_post('tpl_ship_via', 0)
	);
	display_notification(_('New quotation template has been added.'));
	$selected_id = $id;
	$Mode = 'EDIT_ITEM';
}
elseif ($Mode == 'UPDATE_ITEM' && can_process_template()) {
	update_quotation_template(
		$selected_id,
		get_post('tpl_name'),
		get_post('tpl_description'),
		(int)get_post('tpl_validity_days', 30),
		get_post('tpl_terms'),
		get_post('tpl_notes'),
		(int)get_post('tpl_sales_type', 0),
		(int)get_post('tpl_payment_terms', 0),
		(int)get_post('tpl_ship_via', 0),
		check_value('tpl_inactive') ? 1 : 0
	);
	display_notification(_('Quotation template has been updated.'));
}
elseif ($Mode == 'DELETE_ITEM') {
	// Only delete if no orders reference this template
	$in_use = db_fetch(db_query(
		"SELECT COUNT(*) AS cnt FROM " . TB_PREF . "sales_orders WHERE template_id = " . db_escape((int)$selected_id),
		"check template in use"
	));
	if ($in_use && $in_use['cnt'] > 0) {
		display_error(_('This template cannot be deleted because it is referenced by existing quotations.'));
	} else {
		delete_quotation_template($selected_id);
		display_notification(_('Quotation template has been deleted.'));
	}
	$selected_id = -1;
}

// ============================================================================
// LINE CRUD OPERATIONS (only available when a template is selected)
// ============================================================================

if ($selected_id != -1) {
	$add_line_id = find_submit('AddLine');
	$edit_line_id = find_submit('EditLine');
	$del_line_id = find_submit('DeleteLine');

	if ($del_line_id != -1) {
		delete_quotation_template_line($del_line_id);
		display_notification(_('Template line has been removed.'));
	}
	elseif ($add_line_id != -1 || $edit_line_id != -1) {
		if (can_process_line()) {
			$line_type = get_post('line_line_type', 'product');
			$is_optional = ($line_type === 'optional') ? 1 : (int)check_value('line_is_optional');

			if ($edit_line_id != -1) {
				update_quotation_template_line(
					$edit_line_id,
					$line_type,
					get_post('line_stock_id'),
					get_post('line_description'),
					input_num('line_quantity', 1),
					input_num('line_discount', 0),
					$is_optional,
					(int)get_post('line_sort_order', 0)
				);
				display_notification(_('Template line has been updated.'));
			} else {
				add_quotation_template_line(
					$selected_id,
					$line_type,
					get_post('line_stock_id'),
					get_post('line_description'),
					input_num('line_quantity', 1),
					input_num('line_discount', 0),
					$is_optional,
					(int)get_post('line_sort_order', 0)
				);
				display_notification(_('Template line has been added.'));
			}
		}
	}
}

// ============================================================================
// TEMPLATES LIST TABLE
// ============================================================================

$result = get_quotation_templates(true); // include inactive

start_form();
start_table(TABLESTYLE);
$th = array(_('Name'), _('Validity Days'), _('Sales Type'), _('Inactive'), '', '');
table_header($th);

$k = 0;
while ($row = db_fetch($result)) {
	alt_table_row_color($k);

	label_cell($row['name']);
	label_cell($row['validity_days']);
	if ($row['sales_type'] > 0) {
		$st = db_fetch(db_query("SELECT sales_type FROM " . TB_PREF . "sales_types WHERE id = " . db_escape((int)$row['sales_type'])));
		label_cell($st ? $st['sales_type'] : $row['sales_type']);
	} else {
		label_cell(_('(Default)'));
	}
	label_cell($row['inactive'] ? _('Yes') : _('No'));
	edit_button_cell('Edit' . $row['id'], _('Edit'));
	delete_button_cell('Delete' . $row['id'], _('Delete'));
	end_row();
}

end_table(1);
end_form();

// ============================================================================
// TEMPLATE HEADER FORM
// ============================================================================

display_heading($selected_id == -1 ? _('New Quotation Template') : _('Edit Quotation Template'));
display_note(_('Quotation templates define sets of products (and optional/upsell products) that can be loaded into a new quotation in one click. These are separate from Invoice Templates used for recurrent invoicing.'), 0, 1);

start_form();
start_table(TABLESTYLE2);

if ($selected_id != -1) {
	$row = get_quotation_template($selected_id);
	$_POST['tpl_name']          = $row['name'];
	$_POST['tpl_description']   = $row['description'];
	$_POST['tpl_validity_days'] = $row['validity_days'];
	$_POST['tpl_terms']         = $row['terms_and_conditions'];
	$_POST['tpl_notes']         = $row['notes'];
	$_POST['tpl_sales_type']    = $row['sales_type'];
	$_POST['tpl_payment_terms'] = $row['default_payment_terms'];
	$_POST['tpl_ship_via']      = $row['default_ship_via'];
	$_POST['tpl_inactive']      = $row['inactive'];
}

text_row(_('Template Name') . ':', 'tpl_name', null, 60, 100);
text_row(_('Description') . ':', 'tpl_description', null, 60, 200);
amount_row(_('Default Validity (days)') . ':', 'tpl_validity_days', get_post('tpl_validity_days', 30), null, null, 0);
sales_types_list_row(_('Default Sales Type (0 = inherit from order)') . ':', 'tpl_sales_type', get_post('tpl_sales_type', 0), true);
payment_terms_list_row(_('Default Payment Terms (0 = inherit)') . ':', 'tpl_payment_terms', get_post('tpl_payment_terms', 0), true);
shippers_list_row(_('Default Shipper (0 = inherit)') . ':', 'tpl_ship_via', get_post('tpl_ship_via', 0), true);

textarea_row(_('Terms & Conditions') . ':', 'tpl_terms', get_post('tpl_terms'), 60, 4);
textarea_row(_('Internal Notes') . ':', 'tpl_notes', get_post('tpl_notes'), 60, 3);

if ($selected_id != -1)
	check_row(_('Inactive') . ':', 'tpl_inactive', get_post('tpl_inactive'));

end_table(1);
hidden('selected_id', $selected_id);
submit_add_or_update_center($selected_id == -1, '', 'both');
end_form();

// ============================================================================
// TEMPLATE LINES MANAGEMENT (only when editing an existing template)
// ============================================================================

if ($selected_id != -1) {

	display_heading(_('Template Lines'));

	// --- Lines table ---
	start_form();
	hidden('selected_id', $selected_id);
	start_table(TABLESTYLE, "width='100%'");
	$th = array(_('#'), _('Type'), _('Stock ID'), _('Description'), _('Qty'), _('Disc %'), _('Optional'), '', '');
	table_header($th);

	$lines_result = get_quotation_template_lines($selected_id);
	$k = 0;
	while ($line = db_fetch($lines_result)) {
		alt_table_row_color($k);

		label_cell($line['sort_order']);
		label_cell(ucfirst($line['line_type']));
		label_cell($line['stock_id']);
		label_cell($line['description']);
		qty_cell($line['quantity'], false, user_qty_dec());
		percent_cell($line['discount_percent']);
		label_cell($line['is_optional'] ? _('Yes') : _('No'));
		edit_button_cell('EditLine' . $line['id'], _('Edit'));
		delete_button_cell('DeleteLine' . $line['id'], _('Delete'));
		end_row();
	}

	if (db_num_rows($lines_result) === 0) {
		start_row();
		label_cell(_('No lines defined yet. Use the form below to add product lines.'), 'colspan=9 align=center');
		end_row();
	}

	end_table(1);
	end_form();

	// --- Add / Edit line form ---
	$line_types = array(
		'product'  => _('Product (required)'),
		'optional' => _('Optional / Upsell'),
		'section'  => _('Section Header'),
		'note'     => _('Note'),
	);

	display_heading(_('Add Template Line'));
	start_form();
	hidden('selected_id', $selected_id);
	start_table(TABLESTYLE2);

	// Detect if editing an existing line
	$editing_line_id = -1;
	$edit_line_id_chk = find_submit('EditLine');
	if ($edit_line_id_chk != -1) {
		$editing_line_id = $edit_line_id_chk;
		$edit_row = db_fetch(db_query(
			"SELECT * FROM " . TB_PREF . "sales_quotation_template_lines WHERE id = " . db_escape((int)$editing_line_id),
			"get template line for edit"
		));
		if ($edit_row) {
			$_POST['line_line_type']   = $edit_row['line_type'];
			$_POST['line_stock_id']    = $edit_row['stock_id'];
			$_POST['line_description'] = $edit_row['description'];
			$_POST['line_quantity']    = $edit_row['quantity'];
			$_POST['line_discount']    = $edit_row['discount_percent'];
			$_POST['line_is_optional'] = $edit_row['is_optional'];
			$_POST['line_sort_order']  = $edit_row['sort_order'];
		}
	}

	array_selector_row(_('Line Type') . ':', 'line_line_type', get_post('line_line_type', 'product'), $line_types);
	text_row(_('Stock ID') . ':', 'line_stock_id', get_post('line_stock_id'), 20, 20);
	text_row(_('Description (overrides item name)') . ':', 'line_description', get_post('line_description'), 60, 200);
	amount_row(_('Quantity') . ':', 'line_quantity', get_post('line_quantity', 1), null, null, user_qty_dec());
	amount_row(_('Discount %') . ':', 'line_discount', get_post('line_discount', 0), null, null, 2);
	amount_row(_('Sort Order') . ':', 'line_sort_order', get_post('line_sort_order', 0), null, null, 0);

	end_table(1);

	if ($editing_line_id != -1) {
		submit_center_first('EditLine' . $editing_line_id, _('Update Line'), '', 'default');
		submit_center_last('AddLine0', _('Add New Line Instead'), '');
	} else {
		submit_center('AddLine0', _('Add Line'), '', _('Add this line to the template'), 'default');
	}

	end_form();
}

end_page();

