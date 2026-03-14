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
include_once($path_to_root . '/hrm/includes/hrm_db.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
page(_("Employee Card"));

/**
 * Build employee full name.
 *
 * @param array $row
 * @return string
 */
function employee_card_full_name($row) {
    return trim((isset($row['first_name']) ? $row['first_name'] : '').' '.(isset($row['last_name']) ? $row['last_name'] : ''));
}

/**
 * Get a single display name from a master table.
 *
 * @param string $table
 * @param string $id_col
 * @param int $id_val
 * @param string $name_col
 * @return string
 */
function employee_card_lookup_name($table, $id_col, $id_val, $name_col) {
    if ((int)$id_val <= 0)
        return '';

    $sql = "SELECT $name_col AS label FROM ".TB_PREF.$table." WHERE $id_col = ".db_escape((int)$id_val);
    $result = db_query($sql, 'could not get display name');
    $row = db_fetch($result);

    return $row ? $row['label'] : '';
}

if (isset($_GET['employee_id']))
    $_POST['employee_id'] = $_GET['employee_id'];

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();
employees_list_cells(_('Employee:'), 'employee_id', get_post('employee_id', ''), false, false, false);
submit_cells('Show', _('Show'), '', _('Show employee card'), 'default');
end_row();
end_table(1);

$employee_id = trim((string)get_post('employee_id', ''));
if ($employee_id !== '' && $employee_id !== ALL_TEXT) {
    $employee = get_employee_by_code($employee_id);
    if (!$employee) {
        display_error(_('Employee was not found.'));
    } else {
        $department_name = employee_card_lookup_name('departments', 'department_id', (int)$employee['department_id'], 'department_name');
        $position_name = employee_card_lookup_name('positions', 'position_id', (int)$employee['position_id'], 'position_name');
        $grade_name = employee_card_lookup_name('pay_grades', 'grade_id', isset($employee['grade_id']) ? (int)$employee['grade_id'] : 0, 'grade_name');

        start_table(TABLESTYLE2, "width='65%'");
        label_row(_('Employee ID:'), $employee['employee_id']);
        label_row(_('Name:'), employee_card_full_name($employee));
        label_row(_('Department:'), $department_name);
        label_row(_('Position:'), $position_name);
        if (isset($employee['grade_id']))
            label_row(_('Grade:'), $grade_name);
        if (!empty($employee['hire_date']))
            label_row(_('Hire Date:'), sql2date($employee['hire_date']));
        if (!empty($employee['email']))
            label_row(_('Email:'), $employee['email']);
        if (!empty($employee['mobile']))
            label_row(_('Mobile:'), $employee['mobile']);
        label_row(_('Status:'), !empty($employee['inactive']) ? _('Inactive') : _('Active'));
        end_table(1);

        if (function_exists('display_employee_documents')) {
            display_heading(_('Documents'));
            display_employee_documents($employee_id);
        }

        $history = get_employee_history($employee_id);
        if ($history && db_num_rows($history) > 0) {
            display_heading(_('Recent History'));
            start_table(TABLESTYLE, "width='95%'");
            table_header(array(_('Date'), _('Change Type'), _('Department'), _('Position'), _('Reason')));
            $k = 0;
            while ($row = db_fetch($history)) {
                alt_table_row_color($k);
                label_cell(sql2date($row['effective_date']));
                label_cell($row['change_type']);
                label_cell($row['new_department_name']);
                label_cell($row['new_position_name']);
                label_cell($row['reason']);
                end_row();
            }
            end_table(1);
        }
    }
}

end_form();

end_page();

