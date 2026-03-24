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
$page_security = 'SA_IMPORTATT';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/attendance_db.inc');

page(_("Import/Export Attendance"));

if (isset($_POST['download_template'])) {
    header('Content-Type: text/csv');
    header('Content-Disposition: attachment; filename="attendance_import_template.csv"');
    echo "employee_id,date,regular_hours,overtime_hours,overtime_type_id,rate,status\n";
    echo "EMP001,2026-03-01,8,0,0,1,0\n";
    return;
}

if (isset($_POST['export_attendance'])) {
    $from = get_post('from_date', begin_month(Today()));
    $to = get_post('to_date', end_month(Today()));
    header('Content-Type: text/csv');
    header('Content-Disposition: attachment; filename="attendance_export_'.date('Ymd_His').'.csv"');
    echo "employee_id,date,regular_hours,overtime_hours,overtime_type_id,rate,status\n";

    $sql = "SELECT employee_id, date, regular_hours, overtime_hours, overtime_type_id, rate, status
        FROM ".TB_PREF."attendance
        WHERE date >= ".db_escape(date2sql($from))."
        AND date <= ".db_escape(date2sql($to))."
        ORDER BY employee_id, date";
    $result = db_query($sql, 'could not export attendance');
    while ($row = db_fetch($result)) {
        echo implode(',', array(
            $row['employee_id'],
            $row['date'],
            (float)$row['regular_hours'],
            (float)$row['overtime_hours'],
            (int)$row['overtime_type_id'],
            (float)$row['rate'],
            (int)$row['status']
        ))."\n";
    }
    return;
}

if (isset($_POST['import_attendance'])) {
    if (!isset($_FILES['csv_file']) || $_FILES['csv_file']['error'] != UPLOAD_ERR_OK) {
        display_error(_('Please choose a valid CSV file.'));
    } else {
        $handle = fopen($_FILES['csv_file']['tmp_name'], 'r');
        if (!$handle) {
            display_error(_('Unable to read uploaded file.'));
        } else {
            $line_no = 0;
            $imported = 0;
            $failed = 0;
            begin_transaction();

            while (($row = fgetcsv($handle, 0, ',')) !== false) {
                $line_no++;
                if ($line_no == 1 && isset($row[0]) && strtolower(trim($row[0])) == 'employee_id')
                    continue;
                if (count($row) < 3)
                    continue;

                $employee_id = trim($row[0]);
                $date = trim($row[1]);
                $regular_hours = isset($row[2]) ? (float)$row[2] : 0;
                $overtime_hours = isset($row[3]) ? (float)$row[3] : 0;
                $overtime_type_id = isset($row[4]) ? (int)$row[4] : 0;
                $rate = isset($row[5]) ? (float)$row[5] : 1;

                if ($employee_id == '' || $date == '' || !preg_match('/^\d{4}-\d{2}-\d{2}$/', $date)) {
                    $failed++;
                    continue;
                }

                $display_date = sql2date($date);
                if ($regular_hours >= 0)
                    write_attendance($employee_id, 0, $regular_hours, $rate, $display_date);
                if ($overtime_hours > 0)
                    write_attendance($employee_id, $overtime_type_id, $overtime_hours, $rate, $display_date);

                $imported++;
            }

            commit_transaction();
            fclose($handle);
            display_notification(sprintf(_('Import complete. Imported: %s, Failed: %s'), $imported, $failed));
        }
    }
}

if (!isset($_POST['from_date']))
    $_POST['from_date'] = begin_month(Today());
if (!isset($_POST['to_date']))
    $_POST['to_date'] = end_month(Today());

start_form(true);
start_table(TABLESTYLE2);
file_row(_('CSV File:'), 'csv_file', 'csv_file');
label_row(_('Expected columns:'), _('employee_id, date(YYYY-MM-DD), regular_hours, overtime_hours, overtime_type_id, rate, status'));
date_row(_('Export From Date:'), 'from_date');
date_row(_('Export To Date:'), 'to_date');
end_table(1);
submit_center('import_attendance', _('Import Attendance'));
submit_center('download_template', _('Download Template'));
submit_center('export_attendance', _('Export Attendance'));
end_form();

end_page();

