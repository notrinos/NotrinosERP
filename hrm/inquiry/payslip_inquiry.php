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
include_once($path_to_root . '/includes/db_pager.inc');
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

$table_name = payslip_header_table();
if (!$table_name) {
    display_note(_('No payslip records found.'), 0, 1);
    end_form();
    end_page();
    return;
}

$employee_filter = get_post('employee_id', '');
$employee_col = payslip_has_column($table_name, 'employee_id') ? 'employee_id' : (payslip_has_column($table_name, 'emp_id') ? 'emp_id' : '');
$id_col = payslip_has_column($table_name, 'payslip_id') ? 'payslip_id' : (payslip_has_column($table_name, 'payslip_no') ? 'payslip_no' : '');

if ($employee_col === '' || $id_col === '') {
    display_error(_('Payslip inquiry cannot run because required columns are missing.'));
    end_form();
    end_page();
    return;
}

$from_expr = payslip_has_column($table_name, 'from_date') ? 'p.from_date' : "''";
$to_expr = payslip_has_column($table_name, 'to_date') ? 'p.to_date' : "''";
$gross_expr = payslip_has_column($table_name, 'gross_salary') ? 'p.gross_salary' : (payslip_has_column($table_name, 'salary_amount') ? 'p.salary_amount' : '0');
$ded_expr = payslip_has_column($table_name, 'total_deductions') ? 'p.total_deductions' : '0';
$net_expr = payslip_has_column($table_name, 'net_salary') ? 'p.net_salary' : (payslip_has_column($table_name, 'payable_amount') ? 'p.payable_amount' : '0');
$ref_expr = payslip_has_column($table_name, 'reference') ? 'p.reference' : "''";

$where = array('1=1', payslip_non_voided_condition($table_name, 'p'));
if ($employee_filter != '' && $employee_filter != ALL_TEXT)
    $where[] = "p.$employee_col = ".db_escape($employee_filter);
if (payslip_has_column($table_name, 'from_date'))
    $where[] = "p.from_date >= ".db_escape(date2sql($_POST['from_date']));
if (payslip_has_column($table_name, 'to_date'))
    $where[] = "p.to_date <= ".db_escape(date2sql($_POST['to_date']));

$sql = "SELECT p.$id_col AS payslip_no,
        CONCAT(p.$employee_col, ' ', TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,'')))) employee_label,
        $from_expr AS from_date,
        $to_expr AS to_date,
        $gross_expr AS gross_amount,
        $ded_expr AS deduction_amount,
        $net_expr AS net_amount,
        $ref_expr AS reference
    FROM ".TB_PREF.$table_name." p
    LEFT JOIN ".TB_PREF."employees e ON e.employee_id = p.$employee_col
    WHERE ".implode(' AND ', $where);

if (payslip_has_column($table_name, 'from_date'))
    $sql .= " ORDER BY p.from_date DESC, p.$id_col DESC";
else
    $sql .= " ORDER BY p.$id_col DESC";

$cols = array(
    _('Payslip #') => array('name' => 'payslip_no', 'ord' => 'desc'),
    _('Employee') => array('name' => 'employee_label', 'ord' => ''),
    _('From') => array('name' => 'from_date', 'type' => 'date', 'ord' => ''),
    _('To') => array('name' => 'to_date', 'type' => 'date', 'ord' => ''),
    _('Gross') => array('name' => 'gross_amount', 'type' => 'amount'),
    _('Deductions') => array('name' => 'deduction_amount', 'type' => 'amount'),
    _('Net') => array('name' => 'net_amount', 'type' => 'amount'),
    _('Reference') => array('name' => 'reference', 'ord' => '')
);

$table =& new_db_pager('payslip_inquiry_tbl', $sql, $cols);
$table->width = '100%';
display_db_pager($table);
end_form();

end_page();

