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
$page_security = 'SA_PAYSLIPINQUIRY';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
include_once($path_to_root . '/hrm/includes/db/payslip_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_("Payslip History"), false, false, '', $js);

if (!isset($_POST['from_date']))
    $_POST['from_date'] = begin_month(Today());
if (!isset($_POST['to_date']))
    $_POST['to_date'] = end_month(Today());

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();
employees_list_cells(_('Employee:'), 'employee_id', null, true, false, false);
date_cells(_('From Date:'), 'from_date');
date_cells(_('To Date:'), 'to_date');
submit_cells('Search', _('Apply Filter'));
end_row();
end_table(1);

start_table(TABLESTYLE, "width='95%'");
$th = array(_('Payslip #'), _('Employee'), _('From'), _('To'), _('Gross'), _('Deductions'), _('Net'), _('Reference'));
table_header($th);

$employee_filter = get_post('employee_id', '');
$k = 0;

if ($employee_filter != '' && $employee_filter != ALL_TEXT) {
    $result = get_payslips_for_employee($employee_filter, $_POST['from_date'], $_POST['to_date']);
    $emp_name = function_exists('get_employee_name') ? get_employee_name($employee_filter) : '';
    while ($row = db_fetch($result)) {
        alt_table_row_color($k);
        label_cell(isset($row['payslip_id']) ? $row['payslip_id'] : (isset($row['payslip_no']) ? $row['payslip_no'] : ''));
        label_cell($employee_filter.' '.$emp_name);
        label_cell(isset($row['from_date']) ? sql2date($row['from_date']) : '');
        label_cell(isset($row['to_date']) ? sql2date($row['to_date']) : '');
        amount_cell(isset($row['gross_salary']) ? $row['gross_salary'] : (isset($row['salary_amount']) ? $row['salary_amount'] : 0));
        amount_cell(isset($row['total_deductions']) ? $row['total_deductions'] : 0);
        amount_cell(isset($row['net_salary']) ? $row['net_salary'] : (isset($row['payable_amount']) ? $row['payable_amount'] : 0));
        label_cell(isset($row['reference']) ? $row['reference'] : '');
        end_row();
    }
} else {
    $table_name = payslip_header_table();
    if ($table_name) {
        $emp_col = payslip_has_column($table_name, 'employee_id') ? 'employee_id' : 'emp_id';
        $sql = "SELECT p.*, p.$emp_col employee_ref,
            TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,''))) employee_name
            FROM ".TB_PREF.$table_name." p
            LEFT JOIN ".TB_PREF."employees e ON e.employee_id = p.$emp_col
            WHERE p.from_date >= ".db_escape(date2sql($_POST['from_date']))."
                AND p.to_date <= ".db_escape(date2sql($_POST['to_date']))."
            ORDER BY p.from_date DESC";
        $result = db_query($sql, 'could not get payslip inquiry rows');
        while ($row = db_fetch($result)) {
            alt_table_row_color($k);
            label_cell(isset($row['payslip_id']) ? $row['payslip_id'] : (isset($row['payslip_no']) ? $row['payslip_no'] : ''));
            label_cell($row['employee_ref'].' '.$row['employee_name']);
            label_cell(isset($row['from_date']) ? sql2date($row['from_date']) : '');
            label_cell(isset($row['to_date']) ? sql2date($row['to_date']) : '');
            amount_cell(isset($row['gross_salary']) ? $row['gross_salary'] : (isset($row['salary_amount']) ? $row['salary_amount'] : 0));
            amount_cell(isset($row['total_deductions']) ? $row['total_deductions'] : 0);
            amount_cell(isset($row['net_salary']) ? $row['net_salary'] : (isset($row['payable_amount']) ? $row['payable_amount'] : 0));
            label_cell(isset($row['reference']) ? $row['reference'] : '');
            end_row();
        }
    }
}

end_table(1);
end_form();

end_page();

