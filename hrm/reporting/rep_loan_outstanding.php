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
*******************************************************************************/
$page_security = 'SA_HRMREPORTS';
$path_to_root  = '../..';
// NOTE: This file is included by reporting/rep891.php
// $path_to_root and session are already initialized when called via report framework.
// Direct access uses the above declarations.
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/hrm_db.inc');

/**
 * Print loan outstanding report.
 *
 * @return void
 */
function print_loan_outstanding_report() {
    global $path_to_root;

    $comments = isset($_POST['PARAM_0']) ? $_POST['PARAM_0'] : '';
    $destination = isset($_POST['PARAM_6']) ? (int)$_POST['PARAM_6'] : 0;

    if ($destination)
        include_once($path_to_root.'/reporting/includes/excel_report.inc');
    else
        include_once($path_to_root.'/reporting/includes/pdf_report.inc');

    $rep = new FrontReport(_('Loan Outstanding Report'), 'LoanOutstanding', user_pagesize(), 9, 'L');
    $cols = array(0, 70, 220, 330, 430, 520, 620);
    $headers = array(_('Loan ID'), _('Employee'), _('Loan Type'), _('Loan Amount'), _('Outstanding'), _('Status'));
    $aligns = array('left', 'left', 'left', 'right', 'right', 'left');
    recalculate_cols($cols);
    $rep->Info(array(0 => $comments), $cols, $headers, $aligns);
    $rep->NewPage();

    $status_labels = array(0 => _('Pending'), 1 => _('Active'), 2 => _('Completed'), 3 => _('Cancelled'));
    $res = get_employee_loans();
    $dec = user_price_dec();
    while ($row = db_fetch($res)) {
        $rep->TextCol(0, 1, $row['loan_id']);
        $rep->TextCol(1, 2, $row['employee_id'].' '.$row['employee_name']);
        $rep->TextCol(2, 3, $row['loan_type_name']);
        $rep->AmountCol(3, 4, $row['loan_amount'], $dec);
        $rep->AmountCol(4, 5, $row['outstanding_amount'], $dec);
        $rep->TextCol(5, 6, isset($status_labels[(int)$row['status']]) ? $status_labels[(int)$row['status']] : $row['status']);
        $rep->NewLine();
    }

    $rep->End();
}

print_loan_outstanding_report();
