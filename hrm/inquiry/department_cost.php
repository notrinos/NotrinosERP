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
include_once($path_to_root . '/hrm/includes/db/payslip_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_("Department Costs"), false, false, '', $js);

if (!isset($_POST['from_date']))
    $_POST['from_date'] = begin_month(Today());
if (!isset($_POST['to_date']))
    $_POST['to_date'] = end_month(Today());

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();
date_cells(_('From Date:'), 'from_date');
date_cells(_('To Date:'), 'to_date');
submit_cells('Search', _('Apply Filter'));
end_row();
end_table(1);

$table = payslip_header_table();

if (!$table) {
    display_note(_('No payslip records found.'), 0, 1);
    end_form();
    end_page();
    return;
}

$emp_col = payslip_has_column($table, 'employee_id') ? 'employee_id' : (payslip_has_column($table, 'emp_id') ? 'emp_id' : '');
$from_col = payslip_has_column($table, 'from_date') ? 'from_date' : (payslip_has_column($table, 'tran_date') ? 'tran_date' : '');
$gross_expr = payslip_has_column($table, 'gross_salary') ? 'p.gross_salary' : (payslip_has_column($table, 'salary_amount') ? 'p.salary_amount' : '0');
$ded_expr = payslip_has_column($table, 'total_deductions') ? 'p.total_deductions' : '0';
$net_expr = payslip_has_column($table, 'net_salary') ? 'p.net_salary' : (payslip_has_column($table, 'payable_amount') ? 'p.payable_amount' : '0');

if ($emp_col == '' || $from_col == '') {
    display_error(_('Department cost inquiry cannot run because required payslip columns are missing.'));
    end_form();
    end_page();
    return;
}

$sql = "SELECT d.department_id,
        COALESCE(d.department_name, ".db_escape(_('Unassigned')).") department_name,
        SUM(IFNULL($gross_expr,0)) gross_total,
        SUM(IFNULL($ded_expr,0)) deductions_total,
        SUM(IFNULL($net_expr,0)) net_total
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

