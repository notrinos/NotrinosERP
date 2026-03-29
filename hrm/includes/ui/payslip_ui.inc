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

function get_employee_details_to_payslip($order, $employee_id) {

	$error = '';
	if ($employee_id === '' || $employee_id === null || $employee_id === ALL_TEXT)
		return _('The selected employee is invalid.');

	$employee_row = get_employee_to_payslip($employee_id);
	if (!$employee_row)
		return _('The selected employee was not found.');

	if($employee_row['position_id'] == 0)
		$error = _('The selected employee does not have a job position, please select another employee or set job position for the selected employee.');

	$order->set_employee($employee_id, $employee_row['name']);

	return $error;
}

/**
 * Build a preview payslip document for the current employee and date range.
 *
 * @param hrm_cart $order
 * @param string $employee_id
 * @param string $from_date
 * @param string $to_date
 * @return array Array(doc, error_message)
 */
function build_payslip_preview($order, $employee_id, $from_date, $to_date) {
	$employee_id = trim((string)$employee_id);
	if ($employee_id === '' || $employee_id === ALL_TEXT)
		return array(null, '');

	if (!is_date($from_date) || !is_date($to_date))
		return array(null, '');

	if (date_comp($from_date, $to_date) > 0)
		return array(null, _('End date cannot be before the start date.'));

	$employee = get_employee_by_code($employee_id);
	if (!$employee)
		return array(null, _('The selected employee was not found.'));

	if (!employee_has_position($employee_id))
		return array(null, _('The selected employee does not have a job position, please select another employee or set job position for the selected employee.'));

	$salary_components = function_exists('get_employee_salary_components')
		? get_employee_salary_components($employee_id, date2sql($to_date))
		: array();

	if (empty($salary_components))
		return array(null, _('No salary components were found for the selected employee and period. Configure a salary structure or personal salary first.'));

	$preview_doc = calculate_employee_payslip(
		$employee,
		$from_date,
		$to_date,
		0,
		array('salary_components' => array($employee_id => $salary_components))
	);

	if (!$preview_doc)
		return array(null, _('Could not calculate payslip preview for the selected employee.'));

	$preview_doc->reference = get_post('reference');
	$preview_doc->tran_date = get_post('tran_date', $to_date);
	$preview_doc->comments = get_post('Comments');

	return array($preview_doc, '');
}

/**
 * Get the active preview payslip document from the order/cart context.
 *
 * @param mixed $order
 * @return payslip_doc|false
 */
function get_payslip_display_doc($order) {
	if (is_object($order) && $order instanceof payslip_doc)
		return $order;

	if (is_object($order) && isset($order->payslip_preview) && is_object($order->payslip_preview) && $order->payslip_preview instanceof payslip_doc)
		return $order->payslip_preview;

	return false;
}

function display_payslip_header($Order) {
	global $Ajax;

	$new = $Order->trans_no == 0;
	$emp_error = '';
	$preview_doc = false;

	start_outer_table(TABLESTYLE2, "data-order-header='1'");
	table_section(1);

	if($new) {
		if (!isset($_POST['employee_id']) && (get_global_employee() != ALL_TEXT))
			$_POST['employee_id'] = get_global_employee();
		employees_list_row(_('Employee:'), 'employee_id', null, false, true, false, true);
	}
	else {
		hidden('employee_id', $Order->employee_id);
		label_row(_('Employee:'), $Order->employee_name);
	}

	if($Order->employee_id != get_post('employee_id', -1) || list_updated('employee_id')) {
		$emp_error = get_employee_details_to_payslip($Order, get_post('employee_id'));
		$Ajax->activate('_page_body');
	}
	set_global_employee(get_post('employee_id'));
	$selected_employee_id = get_post('employee_id', $Order->employee_id);

	date_row(_('From Date:'), 'from_date');
	date_row(_('To Date:'), 'to_date');

	if ($emp_error == '') {
		list($preview_doc, $preview_error) = build_payslip_preview($Order, $selected_employee_id, get_post('from_date'), get_post('to_date'));
		if ($preview_error !== '')
			$emp_error = $preview_error;
	}

	$Order->payslip_preview = $preview_doc ? $preview_doc : null;
	label_row(_('Gross Salary:'), $preview_doc ? price_format($preview_doc->gross_salary) : price_format(0));

	table_section(2);

	date_row(_('Payslip Date:'), 'tran_date', '', $new, 0, 0, 0, null, true);
	ref_row(_('Reference:'), 'reference', null, get_post('reference'), false, ST_PAYSLIP, get_post('tran_date'));

	date_row(_('Payment Due:'), 'due_date', '', $new, 0, 0, 0, null, true);

	end_outer_table(1);

	return $emp_error;
}

function display_order_summary($title, $order, $editable_items=false) {
	$doc = get_payslip_display_doc($order);

	display_heading($title);

	div_start('payslip_details');

	if (!$doc) {
		display_note(_('No payslip calculation is available for the selected employee and period.'), 0, 1);
		div_end();
		return;
	}

	start_table(TABLESTYLE);

	$th = array(_('Pay Element'), _('Earnings'), _('Deduction'));

	table_header($th);

	$k = 0;

	foreach($doc->lines as $line_no=>$element) {
		alt_table_row_color($k);
		label_cell($element->element_name);
		if ($element->is_deduction) {
			amount_cell('');
			amount_cell($element->final_amount);
		} else {
			amount_cell($element->final_amount);
			amount_cell('');
		}
		end_row();
	}

	start_row();
	label_cells(_('Gross Salary'), price_format($doc->gross_salary), "colspan=2 align=right","align=right");
	end_row();

	start_row();
	label_cells(_('Total Deductions'), price_format($doc->total_deductions), "colspan=2 align=right","align=right");
	end_row();

	start_row();
	label_cells(_('Net Salary'), price_format($doc->net_salary), "colspan=2 align=right","align=right");
	end_row();

	end_table(1);
	div_end();
}
