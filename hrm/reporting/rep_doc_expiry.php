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
// NOTE: This file is included by reporting/rep888.php
// $path_to_root and session are already initialized when called via report framework.
// Direct access uses the above declarations.
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/hrm_db.inc');

/**
 * Print document expiry report.
 *
 * @return void
 */
function print_document_expiry_report() {
    global $path_to_root;

    $days_ahead = isset($_POST['PARAM_0']) ? (int)$_POST['PARAM_0'] : 30;
    $comments = isset($_POST['PARAM_1']) ? $_POST['PARAM_1'] : '';
    $destination = isset($_POST['PARAM_6']) ? (int)$_POST['PARAM_6'] : 0;

    if ($destination)
        include_once($path_to_root.'/reporting/includes/excel_report.inc');
    else
        include_once($path_to_root.'/reporting/includes/pdf_report.inc');

    $rep = new FrontReport(_('Document Expiration Report'), 'DocumentExpiry', user_pagesize(), 9, 'L');
    $cols = array(0, 90, 260, 380, 500, 620);
    $headers = array(_('Employee ID'), _('Employee'), _('Document Type'), _('Document Name'), _('Expiry Date'));
    $aligns = array('left', 'left', 'left', 'left', 'left');
    recalculate_cols($cols);
    $rep->Info(array(0 => $comments), $cols, $headers, $aligns);
    $rep->NewPage();

    $res = get_expiring_documents($days_ahead);
    while ($row = db_fetch($res)) {
        $rep->TextCol(0, 1, $row['employee_id']);
        $rep->TextCol(1, 2, trim($row['first_name'].' '.$row['last_name']));
        $rep->TextCol(2, 3, $row['type_name']);
        $rep->TextCol(3, 4, $row['doc_name']);
        $rep->TextCol(4, 5, empty($row['expiry_date']) ? '' : sql2date($row['expiry_date']));
        $rep->NewLine();
    }

    $rep->End();
}

print_document_expiry_report();
