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
include_once($path_to_root . '/hrm/includes/db/lifecycle_hire_csv_db.inc');

if (!defined('HRM_EMPLOYEE_CSV_MAX_UPLOAD_BYTES'))
    define('HRM_EMPLOYEE_CSV_MAX_UPLOAD_BYTES', 2 * 1024 * 1024);
if (!defined('HRM_EMPLOYEE_CSV_MAX_DATA_ROWS'))
    define('HRM_EMPLOYEE_CSV_MAX_DATA_ROWS', 500);

/**
 * Resolve export-only authoritative Person/Worker name fields while preserving
 * the exact legacy CSV row when canonical identity cannot be accepted.
 *
 * @param array $row
 * @param string|false $as_of
 * @return array
 */
function employee_csv_export_authoritative_name_fields($row, $as_of=false) {
    $legacy = array(
        'first_name' => isset($row['first_name']) ? (string)$row['first_name'] : '',
        'middle_name' => isset($row['middle_name']) ? (string)$row['middle_name'] : '',
        'last_name' => isset($row['last_name']) ? (string)$row['last_name'] : ''
    );

    if (!isset($row['employee_id']) || trim((string)$row['employee_id']) === '' || $as_of === false || $as_of === '')
        return $legacy;

    $identity = get_hrm_person_worker_report_name_as_of((string)$row['employee_id'], $as_of);
    if (!is_array($identity) || empty($identity['canonical_linked']))
        return $legacy;

    $first_name = isset($identity['first_name']) ? trim((string)$identity['first_name']) : '';
    $middle_name = isset($identity['middle_name']) ? trim((string)$identity['middle_name']) : '';
    $last_name = isset($identity['last_name']) ? trim((string)$identity['last_name']) : '';
    if ($first_name === '' && $last_name === '')
        return $legacy;

    return array(
        'first_name' => $first_name,
        'middle_name' => $middle_name,
        'last_name' => $last_name
    );
}

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

/**
 * Existing Employee CSV ownership is intentionally limited to profile fields.
 * Lifecycle/assignment/status fields remain governed by their accepted commands.
 *
 * @param array $data
 * @return array
 */
function employee_csv_existing_profile_update_payload($data) {
    return array(
        'first_name' => isset($data['first_name']) ? (string)$data['first_name'] : '',
        'last_name' => isset($data['last_name']) ? (string)$data['last_name'] : '',
        'middle_name' => isset($data['middle_name']) ? (string)$data['middle_name'] : '',
        'email' => isset($data['email']) ? (string)$data['email'] : '',
        'mobile' => isset($data['mobile']) ? (string)$data['mobile'] : ''
    );
}

/**
 * Read and classify the complete upload before the first database mutation.
 * Validation-skipped rows are counted and omitted. Duplicate Employee codes,
 * oversized files/batches, and unsafe new-Hire rows fail the whole preflight.
 *
 * @param resource $handle
 * @return array
 */
function employee_csv_import_preflight($handle) {
    $line_no = 0;
    $data_rows = 0;
    $skipped = 0;
    $seen = array();
    $existing_updates = array();
    $new_hires = array();

    while (($row = fgetcsv($handle, 0, ',', '"', '\\')) !== false) {
        $line_no++;
        if ($line_no == 1 && isset($row[0]) && strtolower(trim($row[0])) == 'employee_id')
            continue;
        $data_rows++;
        if ($data_rows > HRM_EMPLOYEE_CSV_MAX_DATA_ROWS)
            return array('ok'=>false, 'error'=>sprintf(_('CSV import exceeds the maximum of %s data rows.'), HRM_EMPLOYEE_CSV_MAX_DATA_ROWS));
        if (count($row) < 3) {
            $skipped++;
            continue;
        }

        $data = parse_employee_csv_row($row);
        if ($data['employee_id'] === '' || $data['first_name'] === '' || $data['last_name'] === '') {
            $skipped++;
            continue;
        }

        $slot = strtolower($data['employee_id']);
        if (isset($seen[$slot]))
            return array('ok'=>false, 'error'=>sprintf(_('Duplicate Employee code in uploaded file at line %s.'), $line_no));
        $seen[$slot] = true;

        if (employee_exists_by_code($data['employee_id'])) {
            $existing_updates[] = array(
                'employee_id'=>$data['employee_id'],
                'profile'=>employee_csv_existing_profile_update_payload($data)
            );
            continue;
        }

        // New Employee creation is approval-only. CSV may not seed an inactive
        // lifecycle state, and the complete Hire payload must canonicalize.
        if ((int)$data['inactive'] !== 0) {
            $skipped++;
            continue;
        }
        $new_payload = $data;
        unset($new_payload['inactive']);
        $canonical = hrm_lifecycle_hire_payload_canonicalize($new_payload);
        if ($canonical === false) {
            $skipped++;
            continue;
        }
        $new_hires[] = $canonical;
        if (count($new_hires) > HRM_LIFECYCLE_HIRE_CSV_MAX_NEW_HIRE_ROWS)
            return array('ok'=>false, 'error'=>sprintf(_('CSV import exceeds the maximum of %s new Employee Hire rows.'), HRM_LIFECYCLE_HIRE_CSV_MAX_NEW_HIRE_ROWS));
    }

    return array(
        'ok'=>true,
        'data_rows'=>$data_rows,
        'skipped'=>$skipped,
        'existing_updates'=>$existing_updates,
        'new_hires'=>$new_hires
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

    $employee_csv_export_as_of = hrm_person_worker_utc_now();
    hrm_log_restricted_employee_projection('employee_csv_export');

    $sql = "SELECT employee_id, first_name, last_name, middle_name, email, mobile,
        department_id, position_id, grade_id, hire_date, inactive
        FROM ".TB_PREF."employees
        ORDER BY employee_id";
    $result = db_query($sql, 'could not export employees');
    while ($row = db_fetch($result)) {
        $export_name = employee_csv_export_authoritative_name_fields($row, $employee_csv_export_as_of);
        echo implode(',', array(
            $row['employee_id'],
            str_replace(',', ' ', $export_name['first_name']),
            str_replace(',', ' ', $export_name['last_name']),
            str_replace(',', ' ', $export_name['middle_name']),
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
        $reported_size = isset($_FILES['csv_file']['size']) ? (int)$_FILES['csv_file']['size'] : 0;
        $actual_size = @filesize($_FILES['csv_file']['tmp_name']);
        $upload_size = $actual_size === false ? $reported_size : (int)$actual_size;
        if ($upload_size <= 0 || $upload_size > HRM_EMPLOYEE_CSV_MAX_UPLOAD_BYTES) {
            display_error(sprintf(_('CSV file must be between 1 byte and %s bytes.'), HRM_EMPLOYEE_CSV_MAX_UPLOAD_BYTES));
        } else {
            $handle = fopen($_FILES['csv_file']['tmp_name'], 'r');
            if (!$handle) {
                display_error(_('Unable to read uploaded file.'));
            } else {
                // HRM-FND-005: production CSV Employee Hire is two-pass. The
                // entire file is parsed/classified before the first mutation.
                $plan = employee_csv_import_preflight($handle);
                fclose($handle);

                if (empty($plan['ok'])) {
                    display_error(isset($plan['error']) ? $plan['error'] : _('CSV Employee import preflight failed.'));
                } elseif (empty($plan['existing_updates']) && empty($plan['new_hires'])) {
                    display_notification(sprintf(_('No valid Employee rows were accepted. Validation skipped: %s'), (int)$plan['skipped']));
                } else {
                    $updated = 0;
                    $submitted = 0;
                    $exact_retries = 0;
                    $write_failed = false;

                    begin_transaction();
                    foreach ($plan['existing_updates'] as $item) {
                        // Revalidate branch ownership inside the write transaction.
                        if (!employee_exists_by_code($item['employee_id'])
                            || !update_employee($item['employee_id'], $item['profile'])) {
                            $write_failed = true;
                            break;
                        }
                        $updated++;
                    }

                    if (!$write_failed && !empty($plan['new_hires'])) {
                        // Same-code rehire remains a separate lifecycle command.
                        foreach ($plan['new_hires'] as $hire_payload) {
                            if (employee_exists_by_code($hire_payload['employee_id'])) {
                                $write_failed = true;
                                break;
                            }
                        }
                        if (!$write_failed) {
                            $batch = submit_hrm_lifecycle_employee_hire_csv_batch($plan['new_hires']);
                            if (!is_array($batch) || !isset($batch['status']) || $batch['status'] !== 'pending') {
                                $write_failed = true;
                            } else {
                                $submitted = isset($batch['new_hire_rows']) ? (int)$batch['new_hire_rows'] : 0;
                                if (isset($batch['items']) && is_array($batch['items']))
                                    foreach ($batch['items'] as $result)
                                        if (!empty($result['exact_retry'])) $exact_retries++;
                            }
                        }
                    }

                    if ($write_failed) {
                        cancel_transaction();
                        display_error(_('Import was rolled back. No existing-profile update or new Employee Hire submission from this request was retained.'));
                    } else {
                        commit_transaction();
                        display_notification(sprintf(
                            _('Import complete. Hire approvals submitted: %s, Existing profiles updated: %s, Validation skipped: %s, Exact pending retries reused: %s'),
                            $submitted, $updated, (int)$plan['skipped'], $exact_retries
                        ));
                    }
                }
            }
        }
    }
}

start_form(true);
display_note(_('New Employee codes are staged for maker/checker Hire approval'));
display_note(_('2097152 bytes maximum; 500 data rows maximum; 200 new Hire rows maximum.'));
start_table(TABLESTYLE2);
file_row(_('CSV File:'), 'csv_file', 'csv_file');
label_row(_('Expected columns:'), _('employee_id, first_name, last_name, middle_name, email, mobile, department_id, position_id, grade_id, hire_date, inactive'));
end_table(1);
submit_center('import_employees', _('Import Employees'));
submit_center('download_template', _('Download Template'));
submit_center('export_employees', _('Export Employees'));
end_form();

end_page();
