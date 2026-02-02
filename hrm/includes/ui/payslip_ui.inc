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

	$employee_row = get_employee_to_payslip($employee_id);

	if($employee_row['position_id'] == 0)
		$error = _('The selected employee does not have a job position, please select another employee or set job position for the selected employee.');

	$order->set_employee($employee_id, $employee_row['name']);

	return $error;
}

function display_payslip_header($Order) {
	global $Ajax;

	$new = $Order->trans_no == 0;
	$emp_error = '';

	start_outer_table(TABLESTYLE2);
	table_section(1);

	if($new) {
		if (!isset($_POST['employee_id']) && (get_global_employee() != ALL_TEXT))
			$_POST['employee_id'] = get_global_employee();
		employees_list_row(_('Employee:'), 'employee_id', null, false, true, false, true);
	}
	else {
		hidden('employee_id', $order->employee_id);
		label_row(_('Employee:'), $order->employee_name);
	}
	
	label_row(_('Gross Salary:'), '');

	if($Order->employee_id != get_post('employee_id', -1) || list_updated('employee_id')) {
		$emp_error = get_employee_details_to_payslip($Order, get_post('employee_id'));
		$Ajax->activate('_page_body');
	}
	set_global_employee(get_post('employee_id'));

	table_section(2);

	date_row(_('Payslip Date:'), 'tran_date', '', $new, 0, 0, 0, null, true);
	ref_row(_('Reference:'), 'reference', null, get_post('reference'), false, ST_PAYSLIP, get_post('tran_date'));

	table_section(3);

	date_row(_('Payment Due:'), 'due_date', '', $new, 0, 0, 0, null, true);

	table_section(4);

	date_row(_('From Date:'), 'from_date');
	date_row(_('To Date:'), 'to_date');

	end_outer_table(1);

	return $emp_error;
}

function display_order_summary($title, $order, $editable_items=false) {

	display_heading($title);

	div_start('payslip_details');

	start_table(TABLESTYLE);

	$th = array(_('Pay Element'), _('Earnings'), _('Deduction'), '', '');

	table_header($th);

	$total = 0;
	$k = 0;

	foreach($order->get_elements() as $line_no=>$element) {

	}

	$display_gross = price_format(0);
	$display_net = price_format(0);

	start_row();
	label_cells(_('Gross Salary'), $display_gross, "colspan=3 align=right","align=right");
	end_row();

	start_row();
	label_cells(_('Net Salary'), $display_net, "colspan=3 align=right","align=right");
	end_row();

	end_table(1);
	div_end();
}
