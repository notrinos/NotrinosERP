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
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');

page(_("Attendance Report"));

if (!isset($_POST['from_date']))
    $_POST['from_date'] = begin_month(Today());
if (!isset($_POST['to_date']))
    $_POST['to_date'] = end_month(Today());

start_form();
start_table(TABLESTYLE2);
date_row(_('From Date:'), 'from_date');
date_row(_('To Date:'), 'to_date');
employees_list_row(_('Employee:'), 'employee_id', null, true, false, false);
end_table(1);
submit_center('Search', _('Search'));

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

start_table(TABLESTYLE, "width='95%'");
$th = array(_('Employee ID'), _('Employee Name'), _('Records'), _('Regular Hours'), _('Overtime Hours'), _('Absent Days'));
table_header($th);

$res = db_query($sql, 'could not get attendance inquiry');
$k = 0;
while ($row = db_fetch($res)) {
    alt_table_row_color($k);
    label_cell($row['employee_id']);
    label_cell($row['employee_name']);
    label_cell($row['records_count']);
    qty_cell($row['regular_hours']);
    qty_cell($row['overtime_hours']);
    label_cell($row['absent_days']);
    end_row();
}
end_table(1);
end_form();

end_page();

