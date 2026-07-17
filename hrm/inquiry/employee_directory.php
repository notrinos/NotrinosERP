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
include_once($path_to_root . '/includes/db_pager.inc');
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_db.inc');
include_once($path_to_root . '/hrm/includes/hrm_security.inc');

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

$search = trim((string)get_post('search_text', ''));
$sql = get_employee_directory_projection_sql(check_value('show_inactive'), $search);
hrm_log_restricted_employee_projection('employee_directory');

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
