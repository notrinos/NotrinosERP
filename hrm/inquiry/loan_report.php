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
$page_security = 'SA_LOANREPORT';
$path_to_root = "../..";
include_once($path_to_root . '/includes/db_pager.inc');
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
include_once($path_to_root . '/hrm/includes/db/loan_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_("Loan Outstanding"), false, false, '', $js);

/**
 * Format loan status code for pager output.
 *
 * @param array $row
 * @param string $cell
 * @return string
 */
function loan_report_status_label($row, $cell) {
    $status_labels = array(0 => _('Pending'), 1 => _('Active'), 2 => _('Completed'), 3 => _('Cancelled'));
    $status = (int)$cell;

    return isset($status_labels[$status]) ? $status_labels[$status] : $cell;
}

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();
employees_list_cells(_('Employee:'), 'employee_id', null, true, false, false);
submit_cells('Search', _('Apply Filter'));
end_row();
end_table(1);

$employee_id = get_post('employee_id', '');

$sql = "SELECT l.loan_id,
        CONCAT(l.employee_id, ' ', TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,'')))) employee_label,
        lt.loan_type_name,
        l.loan_date,
        l.loan_amount,
        l.outstanding_amount,
        l.installments,
        l.status
    FROM ".TB_PREF."employee_loans l
    LEFT JOIN ".TB_PREF."loan_types lt ON lt.loan_type_id = l.loan_type_id
    LEFT JOIN ".TB_PREF."employees e ON e.employee_id = l.employee_id
    WHERE 1=1";

if ($employee_id !== '' && $employee_id !== ALL_TEXT)
    $sql .= " AND l.employee_id = ".db_escape($employee_id);

$sql .= " ORDER BY l.loan_date DESC, l.loan_id DESC";

$cols = array(
    _('Loan ID') => array('name' => 'loan_id', 'ord' => 'desc'),
    _('Employee') => array('name' => 'employee_label', 'ord' => ''),
    _('Loan Type') => array('name' => 'loan_type_name', 'ord' => ''),
    _('Loan Date') => array('name' => 'loan_date', 'type' => 'date', 'ord' => ''),
    _('Loan Amount') => array('name' => 'loan_amount', 'type' => 'amount'),
    _('Outstanding') => array('name' => 'outstanding_amount', 'type' => 'amount'),
    _('Installments') => array('name' => 'installments', 'ord' => ''),
    _('Status') => array('name' => 'status', 'fun' => 'loan_report_status_label', 'ord' => '')
);

$table =& new_db_pager('loan_report_tbl', $sql, $cols);
$table->width = '100%';
display_db_pager($table);

end_form();

end_page();

