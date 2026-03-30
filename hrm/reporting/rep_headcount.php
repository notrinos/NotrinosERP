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
// NOTE: This file is included by reporting/rep892.php
// $path_to_root and session are already initialized when called via report framework.
// Direct access uses the above declarations.
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/hrm_db.inc');

/**
 * Print headcount analysis report.
 *
 * @return void
 */
function print_headcount_report() {
    global $path_to_root;

    $department_id = isset($_POST['PARAM_0']) ? (int)$_POST['PARAM_0'] : 0;
    $employment_type = isset($_POST['PARAM_1']) ? $_POST['PARAM_1'] : '';
    $as_at_date = isset($_POST['PARAM_2']) ? $_POST['PARAM_2'] : Today();
    $show_inactive = !empty($_POST['PARAM_3']) ? 1 : 0;
    $comments = isset($_POST['PARAM_4']) ? $_POST['PARAM_4'] : '';
    $orientation = !empty($_POST['PARAM_5']) ? 1 : 0;
    $destination = isset($_POST['PARAM_6']) ? (int)$_POST['PARAM_6'] : 0;

    if ($destination)
        include_once($path_to_root.'/reporting/includes/excel_report.inc');
    else
        include_once($path_to_root.'/reporting/includes/pdf_report.inc');

    $rep = new FrontReport(_('Headcount Analysis'), 'HeadcountAnalysis', user_pagesize(), 9, $orientation ? 'L' : 'P');
    $cols = array(0, 250, 430, 520);
    $headers = array(_('Department'), _('Active'), _('Inactive'));
    $aligns = array('left', 'right', 'right');

    if ($orientation)
        recalculate_cols($cols);

    $rep->Info(array(0 => $comments), $cols, $headers, $aligns);
    $rep->NewPage();

    $where = array('1=1');

    if ($department_id > 0)
        $where[] = 'e.department_id = '.db_escape($department_id);

    if ($employment_type !== '' && $employment_type !== ALL_TEXT)
        $where[] = 'e.employment_type = '.db_escape((int)$employment_type);

    if (is_date($as_at_date)) {
        $as_at_sql = date2sql($as_at_date);
        $where[] = "(e.hire_date IS NULL OR e.hire_date = '0000-00-00' OR e.hire_date <= ".db_escape($as_at_sql).")";
        $where[] = "(e.released_date IS NULL OR e.released_date = '0000-00-00' OR e.released_date = '' OR e.released_date > ".db_escape($as_at_sql).")";
    }

    if (!$show_inactive)
        $where[] = 'e.inactive = 0';

    $sql = "SELECT COALESCE(d.department_name, 'Unassigned') department_name,
        SUM(CASE WHEN e.inactive = 0 THEN 1 ELSE 0 END) active_count,
        SUM(CASE WHEN e.inactive = 1 THEN 1 ELSE 0 END) inactive_count
        FROM ".TB_PREF."employees e
        LEFT JOIN ".TB_PREF."departments d ON d.department_id = e.department_id
        WHERE ".implode(' AND ', $where)."
        GROUP BY d.department_name
        ORDER BY d.department_name";

    $res = db_query($sql, 'could not get headcount report rows');

    if (!$res || db_num_rows($res) == 0) {
        $rep->TextCol(0, 3, _('No headcount rows found for selected criteria.'));
        $rep->End();
        return;
    }

    while ($row = db_fetch($res)) {
        $rep->TextCol(0, 1, $row['department_name']);
        $rep->AmountCol(1, 2, $row['active_count'], 0);
        $rep->AmountCol(2, 3, $row['inactive_count'], 0);
        $rep->NewLine();
    }

    $rep->End();
}

print_headcount_report();
