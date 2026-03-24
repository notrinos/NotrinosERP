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
$page_security = 'SA_IMPORTEMP';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/employee_db.inc');

/**
 * Parse CSV employee row into employee data array.
 *
 * @param array $row
 * @return array
 */
function parse_employee_csv_row($row) {
    return array(
        'employee_id' => trim($row[0]),
        'first_name' => trim($row[1]),
        'last_name' => trim($row[2]),
        'middle_name' => isset($row[3]) ? trim($row[3]) : '',
        'email' => isset($row[4]) ? trim($row[4]) : '',
        'mobile' => isset($row[5]) ? trim($row[5]) : '',
        'department_id' => isset($row[6]) ? (int)$row[6] : 0,
        'position_id' => isset($row[7]) ? (int)$row[7] : 0,
        'grade_id' => isset($row[8]) ? (int)$row[8] : 0,
        'hire_date' => isset($row[9]) ? trim($row[9]) : '',
        'inactive' => isset($row[10]) ? (int)$row[10] : 0
    );
}

page(_("Import/Export Employees"));

if (isset($_POST['download_template'])) {
    header('Content-Type: text/csv');
    header('Content-Disposition: attachment; filename="employee_import_template.csv"');
    echo "employee_id,first_name,last_name,middle_name,email,mobile,department_id,position_id,grade_id,hire_date,inactive\n";
    echo "EMP001,John,Doe,,john@example.com,555123,1,1,1,2026-01-01,0\n";
    return;
}

if (isset($_POST['export_employees'])) {
    header('Content-Type: text/csv');
    header('Content-Disposition: attachment; filename="employees_export_'.date('Ymd_His').'.csv"');
    echo "employee_id,first_name,last_name,middle_name,email,mobile,department_id,position_id,grade_id,hire_date,inactive\n";

    $sql = "SELECT employee_id, first_name, last_name, middle_name, email, mobile,
        department_id, position_id, grade_id, hire_date, inactive
        FROM ".TB_PREF."employees
        ORDER BY employee_id";
    $result = db_query($sql, 'could not export employees');
    while ($row = db_fetch($result)) {
        echo implode(',', array(
            $row['employee_id'],
            str_replace(',', ' ', $row['first_name']),
            str_replace(',', ' ', $row['last_name']),
            str_replace(',', ' ', $row['middle_name']),
            str_replace(',', ' ', $row['email']),
            str_replace(',', ' ', $row['mobile']),
            (int)$row['department_id'],
            (int)$row['position_id'],
            (int)$row['grade_id'],
            $row['hire_date'],
            (int)$row['inactive']
        ))."\n";
    }
    return;
}

if (isset($_POST['import_employees'])) {
    if (!isset($_FILES['csv_file']) || $_FILES['csv_file']['error'] != UPLOAD_ERR_OK) {
        display_error(_('Please choose a valid CSV file.'));
    } else {
        $handle = fopen($_FILES['csv_file']['tmp_name'], 'r');
        if (!$handle) {
            display_error(_('Unable to read uploaded file.'));
        } else {
            $line_no = 0;
            $inserted = 0;
            $updated = 0;
            $failed = 0;

            begin_transaction();
            while (($row = fgetcsv($handle, 0, ',')) !== false) {
                $line_no++;
                if ($line_no == 1 && isset($row[0]) && strtolower(trim($row[0])) == 'employee_id')
                    continue;
                if (count($row) < 3)
                    continue;

                $data = parse_employee_csv_row($row);
                if ($data['employee_id'] === '' || $data['first_name'] === '' || $data['last_name'] === '') {
                    $failed++;
                    continue;
                }

                $existing = get_employee_by_code($data['employee_id']);
                if ($existing) {
                    update_employee($data['employee_id'], $data);
                    $updated++;
                } else {
                    $new_data = $data;
                    unset($new_data['employee_id']);
                    $new_data['employee_id'] = $data['employee_id'];
                    $created = add_employee($new_data);
                    if ($created)
                        $inserted++;
                    else
                        $failed++;
                }
            }
            commit_transaction();
            fclose($handle);
            display_notification(sprintf(_('Import complete. Added: %s, Updated: %s, Failed: %s'), $inserted, $updated, $failed));
        }
    }
}

start_form(true);
start_table(TABLESTYLE2);
file_row(_('CSV File:'), 'csv_file', 'csv_file');
label_row(_('Expected columns:'), _('employee_id, first_name, last_name, middle_name, email, mobile, department_id, position_id, grade_id, hire_date, inactive'));
end_table(1);
submit_center('import_employees', _('Import Employees'));
submit_center('download_template', _('Download Template'));
submit_center('export_employees', _('Export Employees'));
end_form();

end_page();

