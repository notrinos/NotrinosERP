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
include_once($path_to_root . '/includes/db_pager.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_("Attendance Report"), false, false, '', $js);

if (!isset($_POST['from_date']))
    $_POST['from_date'] = begin_month(Today());
if (!isset($_POST['to_date']))
    $_POST['to_date'] = end_month(Today());

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();
date_cells(_('From Date:'), 'from_date');
date_cells(_('To Date:'), 'to_date');
employees_list_cells(null, 'employee_id', null, true, false, false);
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

$cols = array(
    _('Employee ID') => array('name' => 'employee_id', 'ord' => 'asc'),
    _('Employee Name') => array('name' => 'employee_name', 'ord' => ''),
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

