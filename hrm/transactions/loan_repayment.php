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
$page_security = 'SA_LOAN';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
include_once($path_to_root . '/hrm/includes/db/loan_db.inc');

page(_("Loan Repayment"));

if (!isset($_POST['employee_id']))
    $_POST['employee_id'] = '';
if (!isset($_POST['from_date']))
    $_POST['from_date'] = begin_month(Today());
if (!isset($_POST['to_date']))
    $_POST['to_date'] = end_month(Today());

foreach ($_POST as $name => $value) {
    if (strpos($name, 'Pay') === 0) {
        $repayment_id = (int)substr($name, 3);
        if ($repayment_id > 0) {
            $sql = "SELECT total_amount, paid_amount FROM ".TB_PREF."loan_repayments WHERE repayment_id = ".db_escape($repayment_id);
            $result = db_query($sql, 'could not get repayment amount');
            $row = db_fetch_assoc($result);
            if ($row) {
                $due = (float)$row['total_amount'] - (float)$row['paid_amount'];
                if (floatcmp($due, 0) > 0) {
                    apply_loan_repayment($repayment_id, $due, Today(), 0);
                    display_notification(_('Repayment has been marked as paid.'));
                }
            }
        }
    }
}

start_form();

start_table(TABLESTYLE2);
employees_list_row(_('Employee:'), 'employee_id', null, true, true, false);
date_row(_('From Date:'), 'from_date');
date_row(_('To Date:'), 'to_date');
end_table(1);
submit_center('Refresh', _('Refresh')); 

if ($_POST['employee_id'] != '' && $_POST['employee_id'] != ALL_TEXT) {
    start_table(TABLESTYLE, "width='95%'");
    $th = array(_('Repayment ID'), _('Loan ID'), _('Installment #'), _('Due Date'), _('Total Amount'), _('Paid Amount'), _('Outstanding'), _('Status'), '');
    table_header($th);

    $result = get_due_loan_repayments($_POST['employee_id'], $_POST['from_date'], $_POST['to_date']);
    $k = 0;
    while ($row = db_fetch($result)) {
        $due = (float)$row['total_amount'] - (float)$row['paid_amount'];
        alt_table_row_color($k);
        label_cell($row['repayment_id']);
        label_cell($row['loan_id']);
        label_cell($row['installment_no']);
        label_cell(sql2date($row['due_date']));
        amount_cell($row['total_amount']);
        amount_cell($row['paid_amount']);
        amount_cell($due);
        label_cell((int)$row['status'] == 1 ? _('Paid') : ((int)$row['status'] == 2 ? _('Overdue') : _('Scheduled')));
        if (floatcmp($due, 0) > 0)
            submit_cells('Pay'.$row['repayment_id'], _('Mark Paid'));
        else
            label_cell('');
        end_row();
    }

    end_table(1);

    $total_due = get_total_due_loan_deduction($_POST['employee_id'], $_POST['from_date'], $_POST['to_date']);
    display_note(_('Total due in selected period: ').price_format($total_due), 0, 1);
}

end_form();
end_page();

