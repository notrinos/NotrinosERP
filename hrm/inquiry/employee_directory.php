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
include_once($path_to_root . '/includes/db_pager.inc');
include_once($path_to_root . '/includes/ui.inc');

page(_("Employee Directory"));

/**
 * Format inactive flag for pager output.
 *
 * @param array $row
 * @param string $cell
 * @return string
 */
function employee_directory_inactive_label($row, $cell) {
    return ((int)$cell) ? _('Yes') : _('No');
}

if (!isset($_POST['show_inactive']))
    $_POST['show_inactive'] = 0;

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();
// text_cells(_('Search:'), 'search_text', get_post('search_text', ''), 30, 100);
ref_cells(_('Search:'), 'search_text', '', null, _('Enter fragment or leave empty'));
check_cells(_('Show Inactive:'), 'show_inactive');
submit_cells('Search', _('Apply Filter'));
end_row();
end_table(1);

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

$cols = array(
    _('Employee ID') => array('name' => 'employee_id', 'ord' => 'asc'),
    _('Employee Name') => array('name' => 'employee_name', 'ord' => ''),
    _('Department') => array('name' => 'department_name', 'ord' => ''),
    _('Position') => array('name' => 'position_name', 'ord' => ''),
    _('Grade') => array('name' => 'grade_name', 'ord' => ''),
    _('Email') => array('name' => 'email', 'ord' => ''),
    _('Mobile') => array('name' => 'mobile', 'ord' => ''),
    _('Inactive') => array('name' => 'inactive', 'fun' => 'employee_directory_inactive_label', 'ord' => '')
);

$table =& new_db_pager('employee_directory_tbl', $sql, $cols);
$table->width = '100%';
display_db_pager($table);
end_form();

end_page();

