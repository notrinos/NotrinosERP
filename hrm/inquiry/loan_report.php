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
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
include_once($path_to_root . '/hrm/includes/db/loan_db.inc');

page(_("Loan Outstanding"));

start_form();
start_table(TABLESTYLE2);
employees_list_row(_('Employee:'), 'employee_id', null, true, false, false);
end_table(1);
submit_center('Search', _('Apply Filter'));

$employee_id = get_post('employee_id', '');

start_table(TABLESTYLE, "width='95%'");
$th = array(_('Loan ID'), _('Employee'), _('Loan Type'), _('Loan Date'), _('Loan Amount'), _('Outstanding'), _('Installments'), _('Status'));
table_header($th);

$status_labels = array(0 => _('Pending'), 1 => _('Active'), 2 => _('Completed'), 3 => _('Cancelled'));
$res = get_employee_loans($employee_id == ALL_TEXT ? '' : $employee_id);
$k = 0;
while ($row = db_fetch($res)) {
    alt_table_row_color($k);
    label_cell($row['loan_id']);
    label_cell($row['employee_id'].' '.$row['employee_name']);
    label_cell($row['loan_type_name']);
    label_cell(sql2date($row['loan_date']));
    amount_cell($row['loan_amount']);
    amount_cell($row['outstanding_amount']);
    label_cell($row['installments']);
    label_cell(isset($status_labels[(int)$row['status']]) ? $status_labels[(int)$row['status']] : $row['status']);
    end_row();
}
end_table(1);

end_form();

end_page();

