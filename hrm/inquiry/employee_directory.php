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
$page_security = 'SA_EMPLOYEEREP';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');

page(_("Employee Directory"));

if (!isset($_POST['show_inactive']))
    $_POST['show_inactive'] = 0;

start_form();
start_table(TABLESTYLE2);
text_row(_('Search:'), 'search_text', get_post('search_text', ''), 30, 100);
check_row(_('Show Inactive:'), 'show_inactive');
end_table(1);
submit_center('Search', _('Apply Filter'));

$sql = "SELECT e.employee_id,
        TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,''))) employee_name,
        e.email, e.mobile, d.department_name, p.position_name, g.grade_name, e.inactive
    FROM ".TB_PREF."employees e
    LEFT JOIN ".TB_PREF."departments d ON d.department_id = e.department_id
    LEFT JOIN ".TB_PREF."positions p ON p.position_id = e.position_id
    LEFT JOIN ".TB_PREF."pay_grades g ON g.grade_id = e.grade_id
    WHERE 1=1";

if (!check_value('show_inactive'))
    $sql .= " AND e.inactive = 0";

$search = trim((string)get_post('search_text', ''));
if ($search !== '') {
    $like = '%'.$search.'%';
    $sql .= " AND (
        e.employee_id LIKE ".db_escape($like)."
        OR e.first_name LIKE ".db_escape($like)."
        OR e.last_name LIKE ".db_escape($like)."
        OR e.email LIKE ".db_escape($like)."
    )";
}

$sql .= " ORDER BY e.employee_id";

start_table(TABLESTYLE, "width='95%'");
$th = array(_('Employee ID'), _('Employee Name'), _('Department'), _('Position'), _('Grade'), _('Email'), _('Mobile'), _('Inactive'));
table_header($th);

$res = db_query($sql, 'could not get employee directory');
$k = 0;
while ($row = db_fetch($res)) {
    alt_table_row_color($k);
    label_cell($row['employee_id']);
    label_cell($row['employee_name']);
    label_cell($row['department_name']);
    label_cell($row['position_name']);
    label_cell($row['grade_name']);
    label_cell($row['email']);
    label_cell($row['mobile']);
    label_cell($row['inactive'] ? _('Yes') : _('No'));
    end_row();
}

end_table(1);
end_form();

end_page();

