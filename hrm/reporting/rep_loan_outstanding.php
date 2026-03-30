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
if (!isset($path_to_root) || $path_to_root == '')
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

    $employee_id = isset($_POST['PARAM_0']) ? $_POST['PARAM_0'] : '';
    $loan_type_id = isset($_POST['PARAM_1']) ? (int)$_POST['PARAM_1'] : 0;
    $loan_status = isset($_POST['PARAM_2']) ? $_POST['PARAM_2'] : '';
    $comments = isset($_POST['PARAM_3']) ? $_POST['PARAM_3'] : '';
    $orientation = !empty($_POST['PARAM_4']) ? 1 : 0;
    $destination = isset($_POST['PARAM_5']) ? (int)$_POST['PARAM_5'] : 0;

    if ($destination)
        include_once($path_to_root.'/reporting/includes/excel_report.inc');
    else
        include_once($path_to_root.'/reporting/includes/pdf_report.inc');

    $rep = new FrontReport(_('Loan Outstanding Report'), 'LoanOutstanding', user_pagesize(), 9, $orientation ? 'L' : 'P');
    $cols = array(0, 70, 220, 330, 430, 520, 620);
    $headers = array(_('Loan ID'), _('Employee'), _('Loan Type'), _('Loan Amount'), _('Outstanding'), _('Status'));
    $aligns = array('left', 'left', 'left', 'right', 'right', 'left');

    if ($orientation)
        recalculate_cols($cols);

    $rep->Info(array(0 => $comments), $cols, $headers, $aligns);
    $rep->NewPage();

    $status_labels = array(0 => _('Pending'), 1 => _('Active'), 2 => _('Completed'), 3 => _('Cancelled'));
    $where = array('1=1');

    if ($employee_id !== '' && $employee_id !== ALL_TEXT)
        $where[] = 'l.employee_id = '.db_escape($employee_id);

    if ($loan_type_id > 0)
        $where[] = 'l.loan_type_id = '.db_escape($loan_type_id);

    if ($loan_status !== '' && $loan_status !== null)
        $where[] = 'l.status = '.db_escape((int)$loan_status);

    $sql = "SELECT l.*, lt.loan_type_name,
        TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,''))) employee_name
        FROM ".TB_PREF."employee_loans l
        LEFT JOIN ".TB_PREF."loan_types lt ON lt.loan_type_id = l.loan_type_id
        LEFT JOIN ".TB_PREF."employees e ON e.employee_id = l.employee_id
        WHERE ".implode(' AND ', $where)."
        ORDER BY l.loan_date DESC, l.loan_id DESC";

    $res = db_query($sql, 'could not get loan outstanding report rows');
    $dec = user_price_dec();

    if (!$res || db_num_rows($res) == 0) {
        $rep->TextCol(0, 3, _('No loan rows found for selected criteria.'));
        $rep->End();
        return;
    }

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
