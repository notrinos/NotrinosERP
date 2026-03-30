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
$page_security = 'SA_EMPLOYEETRANSVIEW';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
include_once($path_to_root . '/hrm/includes/db/payslip_db.inc');
include_once($path_to_root . '/hrm/includes/db/loan_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_("Employee Transactions"), false, false, '', $js);

if (!isset($_POST['from_date']))
    $_POST['from_date'] = begin_month(Today());
if (!isset($_POST['to_date']))
    $_POST['to_date'] = end_month(Today());

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();
employees_list_cells(_('Employee:'), 'employee_id', null, false, false, false);
date_cells(_('From Date:'), 'from_date');
date_cells(_('To Date:'), 'to_date');
submit_cells('Search', _('Apply Filter'));
end_row();
end_table(1);

$employee_id = get_post('employee_id', '');
if ($employee_id != '' && $employee_id != ALL_TEXT) {
    display_heading(_('Payslip Transactions'));
    start_table(TABLESTYLE, "width='95%'");
    $th = array(_('Payslip'), _('From'), _('To'), _('Gross'), _('Deductions'), _('Net'));
    table_header($th);
    $payslips = get_payslips_for_employee($employee_id, $_POST['from_date'], $_POST['to_date']);
    $k = 0;
    if ($payslips) {
        while ($row = db_fetch($payslips)) {
            alt_table_row_color($k);
            label_cell(isset($row['payslip_id']) ? $row['payslip_id'] : (isset($row['payslip_no']) ? $row['payslip_no'] : ''));
            label_cell(isset($row['from_date']) ? sql2date($row['from_date']) : '');
            label_cell(isset($row['to_date']) ? sql2date($row['to_date']) : '');
            amount_cell(isset($row['gross_salary']) ? $row['gross_salary'] : 0);
            amount_cell(isset($row['total_deductions']) ? $row['total_deductions'] : 0);
            amount_cell(isset($row['net_salary']) ? $row['net_salary'] : 0);
            end_row();
        }
    }
    end_table(1);

    display_heading(_('Loan Transactions'));
    start_table(TABLESTYLE, "width='95%'");
    $th = array(_('Loan ID'), _('Loan Type'), _('Loan Date'), _('Amount'), _('Outstanding'), _('Status'));
    table_header($th);
    $loans = get_employee_loans($employee_id);
    $k = 0;
    $status_labels = array(0 => _('Pending'), 1 => _('Active'), 2 => _('Completed'), 3 => _('Cancelled'));
    while ($row = db_fetch($loans)) {
        alt_table_row_color($k);
        label_cell($row['loan_id']);
        label_cell($row['loan_type_name']);
        label_cell(sql2date($row['loan_date']));
        amount_cell($row['loan_amount']);
        amount_cell($row['outstanding_amount']);
        label_cell(isset($status_labels[(int)$row['status']]) ? $status_labels[(int)$row['status']] : $row['status']);
        end_row();
    }
    end_table(1);
}

end_form();

end_page();

