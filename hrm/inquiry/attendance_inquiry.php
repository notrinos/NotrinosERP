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
$page_security = 'SA_ATTINQUIRY';
$path_to_root = "../..";
include_once($path_to_root . '/includes/db_pager.inc');
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_security.inc');
include_once($path_to_root . '/hrm/includes/db/employee_person_worker_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_("Attendance Report"), false, false, '', $js);

/**
 * Resolve one inquiry row's authoritative display name at the page-level instant.
 *
 * Missing, denied, unavailable, ambiguous, tampered, or unlinked Person/Worker
 * evidence returns the exact legacy name cell. Only accepted canonical-link
 * evidence may replace that cell with the canonical Person name.
 *
 * @param array $row
 * @param string $cell
 * @return string
 */
function attendance_inquiry_authoritative_name($row, $cell) {
    global $attendance_inquiry_as_of;

    $legacy_name = (string)$cell;
    if (!is_array($row) || !isset($row['employee_id'])
        || trim((string)$row['employee_id']) === '')
        return $legacy_name;

    $identity = $attendance_inquiry_as_of === false
        ? false
        : get_hrm_person_worker_report_name_as_of(
            $row['employee_id'], $attendance_inquiry_as_of
        );
    if (!is_array($identity) || empty($identity['canonical_linked']))
        return $legacy_name;

    $first_name = isset($identity['first_name']) ? trim((string)$identity['first_name']) : '';
    $middle_name = isset($identity['middle_name']) ? trim((string)$identity['middle_name']) : '';
    $last_name = isset($identity['last_name']) ? trim((string)$identity['last_name']) : '';
    $canonical_name = trim($first_name.' '.($middle_name !== '' ? $middle_name.' ' : '').$last_name);

    return $canonical_name !== '' ? $canonical_name : $legacy_name;
}

/**
 * Resolve one Attendance Inquiry Employee filter label at the page instant.
 *
 * SA_ATTINQUIRY remains intentionally outside Person/Worker identity-read
 * authority. The exact shared employee-list label is the fail-closed fallback;
 * accepted canonical identity may change presentation only when the same
 * principal independently holds an approved identity-read capability. Shared
 * selector cohort/search/order and submitted employee_id remain unchanged.
 *
 * @param array $row
 * @return string
 */
function attendance_inquiry_authoritative_employee_list($row) {
    global $attendance_inquiry_as_of;

    $legacy_label = _format_employee_list($row);
    if (!is_array($row) || !isset($row[0]) || trim((string)$row[0]) === ''
        || $attendance_inquiry_as_of === false)
        return $legacy_label;

    $identity = get_hrm_person_worker_report_name_as_of(
        $row[0], $attendance_inquiry_as_of
    );
    if (!is_array($identity) || empty($identity['canonical_linked']))
        return $legacy_label;

    $first_name = isset($identity['first_name']) ? trim((string)$identity['first_name']) : '';
    $middle_name = isset($identity['middle_name']) ? trim((string)$identity['middle_name']) : '';
    $last_name = isset($identity['last_name']) ? trim((string)$identity['last_name']) : '';
    $canonical_name = trim($first_name.' '.($middle_name !== '' ? $middle_name.' ' : '').$last_name);
    if ($canonical_name === '')
        return $legacy_label;

    return (user_show_codes() ? ((string)$row[0].' - ') : '').$canonical_name;
}

if (!isset($_POST['from_date']))
    $_POST['from_date'] = begin_month(Today());
if (!isset($_POST['to_date']))
    $_POST['to_date'] = end_month(Today());

$attendance_inquiry_as_of = hrm_person_worker_utc_now();
hrm_log_restricted_employee_projection('attendance_inquiry_selector');

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();
date_cells(_('From Date:'), 'from_date');
date_cells(_('To Date:'), 'to_date');
employees_list_cells(null, 'employee_id', null, true, false, false, false, array(
    'format' => 'attendance_inquiry_authoritative_employee_list'
));
submit_cells('Search', _('Apply Filter'));
end_row();
end_table(1);

$sql = "SELECT a.employee_id,
        TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,''))) employee_name,
        SUM(IFNULL(a.regular_hours, 0)) regular_hours,
        SUM(IFNULL(a.overtime_hours, 0)) overtime_hours,
        SUM(CASE WHEN a.status = 1 THEN 1 ELSE 0 END) absent_days,
        COUNT(*) records_count
    FROM ".TB_PREF."attendance a
    LEFT JOIN ".TB_PREF."employees e ON e.employee_id = a.employee_id
    WHERE a.date >= ".db_escape(date2sql($_POST['from_date']))."
        AND a.date <= ".db_escape(date2sql($_POST['to_date']));

if (get_post('employee_id') != '' && get_post('employee_id') != ALL_TEXT)
    $sql .= " AND a.employee_id = ".db_escape(get_post('employee_id'));

$sql .= " GROUP BY a.employee_id, employee_name ORDER BY a.employee_id";
hrm_log_restricted_employee_projection('attendance_inquiry');

$cols = array(
    _('Employee ID') => array('name' => 'employee_id', 'ord' => 'asc'),
    _('Employee Name') => array('name' => 'employee_name', 'fun' => 'attendance_inquiry_authoritative_name', 'ord' => ''),
    _('Records') => array('name' => 'records_count', 'ord' => ''),
    _('Regular Hours') => array('name' => 'regular_hours', 'type' => 'qty'),
    _('Overtime Hours') => array('name' => 'overtime_hours', 'type' => 'qty'),
    _('Absent Days') => array('name' => 'absent_days', 'ord' => '')
);

$table =& new_db_pager('attendance_inquiry_tbl', $sql, $cols);
$table->width = '100%';
display_db_pager($table);
end_form();

end_page();
