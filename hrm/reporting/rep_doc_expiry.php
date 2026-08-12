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
include_once($path_to_root.'/hrm/reporting/report_controller_security.inc');
hrm_require_report_controller(888);
// NOTE: This file is included by reporting/rep888.php
// $path_to_root and session are already initialized when called via report framework.
// Direct access uses the above declarations.
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/hrm_db.inc');
include_once($path_to_root.'/hrm/includes/hrm_security.inc');
include_once($path_to_root.'/hrm/includes/db/employee_person_worker_db.inc');

/**
 * Resolve one Document Expiration Report display name at the report's fixed as-of instant.
 *
 * The document-expiry SQL remains authoritative for cohort, filters, document
 * fields and ordering and supplies the exact legacy employee_name fallback.
 * Only an accepted canonical link may replace that display value with the
 * canonical Person name.
 *
 * @param array $row
 * @param string|false $as_of
 * @return string
 */
function document_expiry_report_authoritative_name($row, $as_of) {
    $legacy_name = is_array($row) && isset($row['employee_name'])
        ? (string)$row['employee_name'] : '';
    if (!is_array($row) || !isset($row['employee_id'])
        || trim((string)$row['employee_id']) === '' || $as_of === false)
        return $legacy_name;

    $identity = get_hrm_person_worker_report_name_as_of($row['employee_id'], $as_of);
    if (!is_array($identity) || empty($identity['canonical_linked']))
        return $legacy_name;

    $first_name = isset($identity['first_name']) ? trim((string)$identity['first_name']) : '';
    $middle_name = isset($identity['middle_name']) ? trim((string)$identity['middle_name']) : '';
    $last_name = isset($identity['last_name']) ? trim((string)$identity['last_name']) : '';
    $canonical_name = trim($first_name.' '.($middle_name !== '' ? $middle_name.' ' : '').$last_name);

    return $canonical_name !== '' ? $canonical_name : $legacy_name;
}

/**
 * Print document expiry report.
 *
 * @return void
 */
function print_document_expiry_report() {
    global $path_to_root;

    $from_date = isset($_POST['PARAM_0']) ? $_POST['PARAM_0'] : Today();
    $to_date = isset($_POST['PARAM_1']) ? $_POST['PARAM_1'] : add_months(Today(), 1);
    $doc_type_id = isset($_POST['PARAM_2']) ? (int)$_POST['PARAM_2'] : 0;
    $department_id = isset($_POST['PARAM_3']) ? (int)$_POST['PARAM_3'] : 0;
    $comments = isset($_POST['PARAM_4']) ? $_POST['PARAM_4'] : '';
    $orientation = !empty($_POST['PARAM_5']) ? 1 : 0;
    $destination = isset($_POST['PARAM_6']) ? (int)$_POST['PARAM_6'] : 0;

    if ($destination)
        include_once($path_to_root.'/reporting/includes/excel_report.inc');
    else
        include_once($path_to_root.'/reporting/includes/pdf_report.inc');

    $rep = new FrontReport(_('Document Expiration Report'), 'DocumentExpiry', user_pagesize(), 9, $orientation ? 'L' : 'P');
    $cols = array(0, 90, 250, 350, 470, 520);
    $headers = array(_('Employee ID'), _('Employee'), _('Document Type'), _('Document Name'), _('Expiry Date'));
    $aligns = array('left', 'left', 'left', 'left', 'left');

    if ($orientation)
        recalculate_cols($cols);

    $rep->Info(array(0 => $comments), $cols, $headers, $aligns);
    $rep->NewPage();

    $where = array('d.expiry_date IS NOT NULL');

    if (is_date($from_date))
        $where[] = 'd.expiry_date >= '.db_escape(date2sql($from_date));

    if (is_date($to_date))
        $where[] = 'd.expiry_date <= '.db_escape(date2sql($to_date));

    if ($doc_type_id > 0)
        $where[] = 'd.doc_type_id = '.db_escape($doc_type_id);

    if ($department_id > 0)
        $where[] = 'e.department_id = '.db_escape($department_id);

    $sql = "SELECT d.*, t.type_name, e.first_name, e.last_name, e.employee_id,
        TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,''))) employee_name
        FROM ".TB_PREF."employee_documents d
        LEFT JOIN ".TB_PREF."document_types t ON t.doc_type_id = d.doc_type_id
        LEFT JOIN ".TB_PREF."employees e ON e.employee_id = d.employee_id
        WHERE ".implode(' AND ', $where)."
        ORDER BY d.expiry_date ASC, e.employee_id";

    $res = db_query($sql, 'could not get document expiry report rows');

    if (!$res || db_num_rows($res) == 0) {
        $rep->TextCol(0, 3, _('No expiring documents found for selected criteria.'));
        $rep->End();
        return;
    }

    hrm_log_restricted_employee_projection('document_expiry_report');
    $as_of = hrm_person_worker_utc_now();

    while ($row = db_fetch($res)) {
        $rep->TextCol(0, 1, $row['employee_id']);
        $rep->TextCol(1, 2, document_expiry_report_authoritative_name($row, $as_of));
        $rep->TextCol(2, 3, $row['type_name']);
        $rep->TextCol(3, 4, $row['doc_name']);
        $rep->TextCol(4, 5, empty($row['expiry_date']) ? '' : sql2date($row['expiry_date']));
        $rep->NewLine();
    }

    $rep->End();
}

print_document_expiry_report();
