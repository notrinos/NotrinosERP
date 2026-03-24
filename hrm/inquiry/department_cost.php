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
$page_security = 'SA_DEPTCOST';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');

page(_("Department Costs"));

if (!isset($_POST['from_date']))
    $_POST['from_date'] = begin_month(Today());
if (!isset($_POST['to_date']))
    $_POST['to_date'] = end_month(Today());

start_form();
start_table(TABLESTYLE2);
date_row(_('From Date:'), 'from_date');
date_row(_('To Date:'), 'to_date');
end_table(1);
submit_center('Search', _('Apply Filter'));

$table = function_exists('payslip_header_table') ? payslip_header_table() : 'payslips';
$emp_col = ($table && function_exists('payslip_has_column') && payslip_has_column($table, 'employee_id')) ? 'employee_id' : 'emp_id';
$gross_col = ($table && function_exists('payslip_has_column') && payslip_has_column($table, 'gross_salary')) ? 'gross_salary' : 'salary_amount';
$ded_col = ($table && function_exists('payslip_has_column') && payslip_has_column($table, 'total_deductions')) ? 'total_deductions' : '0';
$net_col = ($table && function_exists('payslip_has_column') && payslip_has_column($table, 'net_salary')) ? 'net_salary' : 'payable_amount';
$from_col = ($table && function_exists('payslip_has_column') && payslip_has_column($table, 'from_date')) ? 'from_date' : 'tran_date';

$sql = "SELECT d.department_id, d.department_name,
        SUM(IFNULL(p.$gross_col,0)) gross_total,
        SUM(IFNULL(p.$ded_col,0)) deductions_total,
        SUM(IFNULL(p.$net_col,0)) net_total
    FROM ".TB_PREF.$table." p
    JOIN ".TB_PREF."employees e ON e.employee_id = p.$emp_col
    LEFT JOIN ".TB_PREF."departments d ON d.department_id = e.department_id
    WHERE p.$from_col >= ".db_escape(date2sql($_POST['from_date']))."
        AND p.$from_col <= ".db_escape(date2sql($_POST['to_date']))."
    GROUP BY d.department_id, d.department_name
    ORDER BY d.department_name";

start_table(TABLESTYLE, "width='95%'");
$th = array(_('Department'), _('Gross Total'), _('Deductions Total'), _('Net Total'));
table_header($th);

$res = db_query($sql, 'could not get department cost inquiry');
$k = 0;
while ($row = db_fetch($res)) {
    alt_table_row_color($k);
    label_cell($row['department_name']);
    amount_cell($row['gross_total']);
    amount_cell($row['deductions_total']);
    amount_cell($row['net_total']);
    end_row();
}
end_table(1);
end_form();

end_page();

