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
$page_security = 'SA_PAYROLL';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/db/employee_db.inc');
include_once($path_to_root.'/hrm/includes/db/employee_projection_control_db.inc');
hrm_employee_projection_require_readable();
include_once($path_to_root.'/hrm/includes/db/payroll_db.inc');
include_once($path_to_root.'/hrm/includes/payroll_engine.inc');
include_once($path_to_root.'/hrm/includes/db/pay_core_009_governance_db.inc');
include_once($path_to_root.'/hrm/includes/payroll/pay_core_009_runner.inc');

$js = '';

if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = "Payroll Processing"), false, false, '', $js);

/**
 * Resolve one Payroll Processing selector label at the page-level instant.
 *
 * SA_PAYROLL remains payroll-processing authorization only. Canonical naming is
 * presentation-only and is available only when the same principal independently
 * holds an approved Person/Worker read capability. The shared selector cohort,
 * submitted employee_id, payroll eligibility, calculation, preparation and
 * approval-draft behavior remain exact legacy values.
 *
 * @param array $row
 * @return string
 */
function payroll_process_authoritative_employee_list($row) {
    global $payroll_process_selector_as_of;

    $legacy_label = _format_employee_list($row);
    if (!is_array($row) || !isset($row[0]) || trim((string)$row[0]) === ''
        || $payroll_process_selector_as_of === false)
        return $legacy_label;

    $identity = get_hrm_person_worker_report_name_as_of(
        $row[0], $payroll_process_selector_as_of
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

/**
 * Validate payroll process request.
 *
 * @return bool
 */
function validate_payroll_request() {
    if (!is_date(get_post('from_date'))) {
        display_error(_('From Date is invalid.'));
        set_focus('from_date');
        return false;
    }
    if (!is_date(get_post('to_date'))) {
        display_error(_('To Date is invalid.'));
        set_focus('to_date');
        return false;
    }
    if (date_comp(get_post('from_date'), get_post('to_date')) > 0) {
        display_error(_('From Date cannot be later than To Date.'));
        set_focus('from_date');
        return false;
    }
	if (date_comp(get_post('from_date'), Today()) > 0 || date_comp(get_post('to_date'), Today()) > 0) {
		display_error(_('Payroll cannot be processed for future dates.'));
		set_focus('to_date');
		return false;
	}
    if (empty(trim(get_post('period_name')))) {
        display_error(_('Payroll Period Name is required.'));
        set_focus('period_name');
        return false;
    }
    return true;
}

/**
 * Resolve the employee set targeted by the payroll run.
 *
 * @param int $department_id
 * @param string $employee_id
 * @param bool $invalid_employee_selection
 * @return array
 */
function resolve_payroll_employees($department_id, $employee_id, &$invalid_employee_selection) {
    $employees = array();
    $invalid_employee_selection = false;
    $employee_id = trim((string)$employee_id);

    if ($employee_id !== '') {
        $employee = get_employee_by_code($employee_id);
        if ($employee)
            $employees[] = $employee;
        else
            $invalid_employee_selection = true;

        return $employees;
    }

    $result = get_active_employees((int)$department_id);
    while ($row = db_fetch_assoc($result))
        $employees[] = $row;

    return $employees;
}

/**
 * Build the grouped skip-reason buckets used during payroll filtering.
 *
 * @return array
 */
function get_payroll_skip_reason_buckets() {
    return array(
        'existing_payslips' => array(),
		'not_hired' => array(),
        'missing_position' => array(),
        'missing_salary_components' => array()
    );
}

/**
 * Add an employee id to one payroll skip-reason bucket.
 *
 * @param array $skip_reasons
 * @param string $reason_key
 * @param string $employee_id
 * @return void
 */
function add_payroll_skip_reason(&$skip_reasons, $reason_key, $employee_id) {
    if (!isset($skip_reasons[$reason_key]) || !is_array($skip_reasons[$reason_key]))
        $skip_reasons[$reason_key] = array();

    $employee_id = trim((string)$employee_id);
    if ($employee_id === '' || in_array($employee_id, $skip_reasons[$reason_key], true))
        return;

    $skip_reasons[$reason_key][] = $employee_id;
}

/**
 * Check whether one employee is eligible for payroll generation.
 *
 * @param array $employee
 * @param string $from_date
 * @param string $to_date
 * @param array $salary_components_by_employee
 * @param array $skip_reasons
 * @return bool
 */
function is_employee_payroll_eligible($employee, $from_date, $to_date, $salary_components_by_employee, &$skip_reasons) {
    if (!is_array($employee) || empty($employee['employee_id']))
        return false;

    $employee_id = trim((string)$employee['employee_id']);
    if ($employee_id === '')
        return false;

	if (function_exists('check_employee_hired') && !check_employee_hired($employee_id, $from_date)) {
		add_payroll_skip_reason($skip_reasons, 'not_hired', $employee_id);
		return false;
	}

    if (function_exists('employee_has_position') && !employee_has_position($employee_id)) {
        add_payroll_skip_reason($skip_reasons, 'missing_position', $employee_id);
        return false;
    }

    if (function_exists('get_employee_salary_components')) {
        $salary_components = array();
        if (isset($salary_components_by_employee[$employee_id]) && is_array($salary_components_by_employee[$employee_id]))
            $salary_components = $salary_components_by_employee[$employee_id];
        else
            $salary_components = get_employee_salary_components($employee_id, date2sql($to_date));

        if (empty($salary_components)) {
            add_payroll_skip_reason($skip_reasons, 'missing_salary_components', $employee_id);
            return false;
        }
    }

    return true;
}

/**
 * Remove employees who already have a payslip overlapping the selected period.
 *
 * @param array $employees
 * @param string $from_date
 * @param string $to_date
 * @param array $skip_reasons
 * @return array
 */
function filter_payroll_eligible_employees($employees, $from_date, $to_date, &$skip_reasons) {
    $eligible_employees = array();
    $skip_reasons = get_payroll_skip_reason_buckets();
    $salary_components_by_employee = function_exists('get_salary_components_for_employees')
        ? get_salary_components_for_employees($employees, $to_date)
        : array();

    foreach ((array)$employees as $employee) {
        if (!is_array($employee) || empty($employee['employee_id']))
            continue;

        if (function_exists('payslip_exists_for_period')
            && payslip_exists_for_period($employee['employee_id'], $from_date, $to_date)) {
            add_payroll_skip_reason($skip_reasons, 'existing_payslips', $employee['employee_id']);
            continue;
        }

        if (!is_employee_payroll_eligible($employee, $from_date, $to_date, $salary_components_by_employee, $skip_reasons))
            continue;

        $eligible_employees[] = $employee;
    }

    return $eligible_employees;
}

/**
 * Display grouped payroll skip details for the current request.
 *
 * @param array $skip_reasons
 * @return void
 */
function display_payroll_skip_messages($skip_reasons) {
    if (!empty($skip_reasons['existing_payslips'])) {
        display_note(sprintf(
            _('Skipped employees with existing payslips for the selected period: %s'),
            implode(', ', $skip_reasons['existing_payslips'])
        ));
    }

    if (!empty($skip_reasons['missing_position'])) {
        display_note(sprintf(
            _('Skipped employees without job positions: %s'),
            implode(', ', $skip_reasons['missing_position'])
        ));
    }

	if (!empty($skip_reasons['not_hired'])) {
		display_note(sprintf(
			_('Skipped employees not yet hired for the requested period: %s'),
			implode(', ', $skip_reasons['not_hired'])
		));
	}

    if (!empty($skip_reasons['missing_salary_components'])) {
        display_note(sprintf(
            _('Skipped employees without salary components for the requested period: %s'),
            implode(', ', $skip_reasons['missing_salary_components'])
        ));
    }
}

/** Render one safe PAY-CORE-009 progress summary. */
function display_pay_core_009_progress($status) {
    if (!is_array($status))
        return;
    $counts = isset($status['counts']) && is_array($status['counts']) ? $status['counts'] : array();
    display_note(sprintf(
        _('Restartable payroll run #%d / Period #%d — state: %s; completed: %d; pending: %d; failed: %d; quarantined: %d; checkpoint: %d.'),
        (int)$status['run_id'],
        (int)$status['payroll_period_id'],
        (string)$status['state'],
        (int)($counts['completed'] ?? 0),
        (int)($counts['pending'] ?? 0),
        (int)($counts['failed'] ?? 0),
        (int)($counts['quarantined'] ?? 0),
        (int)$status['checkpoint_no']
    ));
}

$pay_core_009_last_status = null;
$resume_run_id = find_submit('ResumeRun');
if ($resume_run_id > 0) {
    $chunk = hrm_pay_core_009_run_chunk($resume_run_id, 'browser', null, $run_error);
    if ($chunk === false)
        display_error(sprintf(_('Payroll run could not resume (%s).'), $run_error));
    else
        display_notification($chunk['finalized']
            ? _('Payroll run completed and results were prepared for approval.')
            : _('Payroll run chunk completed. Use Resume to continue if work remains.'));
    $pay_core_009_last_status = hrm_pay_core_009_status($resume_run_id, $status_error);
}

$recover_run_id = find_submit('RecoverRun');
if ($recover_run_id > 0) {
    $before = hrm_pay_core_009_status($recover_run_id, $status_error);
    $action = is_array($before) && $before['state'] === 'quarantined'
        ? 'resume_quarantined' : 'resume_failed';
    if (!hrm_pay_core_009_recover_run($recover_run_id, $action, $recover_error))
        display_error(sprintf(_('Payroll run recovery was denied (%s).'), $recover_error));
    else {
        display_notification(_('Payroll run recovery evidence was recorded. The run can resume.'));
        $chunk = hrm_pay_core_009_run_chunk($recover_run_id, 'browser', null, $run_error);
        if ($chunk === false)
            display_error(sprintf(_('Recovered payroll run could not resume (%s).'), $run_error));
    }
    $pay_core_009_last_status = hrm_pay_core_009_status($recover_run_id, $status_error);
}

if (isset($_POST['process_payroll']) && validate_payroll_request()) {
    $period_name = trim((string)get_post('period_name'));
    $from_date = get_post('from_date');
    $to_date = get_post('to_date');
    $department_id = (int)get_post('department_id', 0);
    $employee_id = trim((string)get_post('employee_id', ''));

    $invalid_employee_selection = false;
    $employees = resolve_payroll_employees($department_id, $employee_id, $invalid_employee_selection);
    $skip_reasons = get_payroll_skip_reason_buckets();
    if (!$invalid_employee_selection && !empty($employees))
        $employees = filter_payroll_eligible_employees($employees, $from_date, $to_date, $skip_reasons);

    if ($invalid_employee_selection) {
        display_error(_('Selected employee was not found.'));
        set_focus('employee_id');
    } elseif (empty($employees)) {
        if (!empty($skip_reasons['existing_payslips'])
            && empty($skip_reasons['not_hired'])
            && empty($skip_reasons['missing_position'])
            && empty($skip_reasons['missing_salary_components']))
            display_error(_('Selected employees already have payslips for the requested period.'));
        elseif (!empty($skip_reasons['missing_position'])
            && empty($skip_reasons['not_hired'])
            && empty($skip_reasons['existing_payslips'])
            && empty($skip_reasons['missing_salary_components']))
            display_error(_('Selected employees are missing job positions.'));
        elseif (!empty($skip_reasons['missing_salary_components'])
            && empty($skip_reasons['not_hired'])
            && empty($skip_reasons['existing_payslips'])
            && empty($skip_reasons['missing_position']))
            display_error(_('Selected employees do not have salary components for the requested period.'));
        elseif (!empty($skip_reasons['not_hired'])
            && empty($skip_reasons['existing_payslips'])
            && empty($skip_reasons['missing_position'])
            && empty($skip_reasons['missing_salary_components']))
            display_error(_('Selected employees were not yet hired for the requested period.'));
        elseif (!empty($skip_reasons['existing_payslips'])
            || !empty($skip_reasons['not_hired'])
            || !empty($skip_reasons['missing_position'])
            || !empty($skip_reasons['missing_salary_components']))
            display_error(_('No eligible employees were found for payroll processing.'));
        else
            display_warning(_('No active employees found for selected filters.'));
        display_payroll_skip_messages($skip_reasons);
    } else {
        display_payroll_skip_messages($skip_reasons);
        $start = hrm_pay_core_009_start_run(
            $period_name, $from_date, $to_date, $department_id, $employees,
            HRM_PAY_CORE_009_DEFAULT_CHUNK_SIZE, $start_error
        );
        if ($start === false) {
            display_error(sprintf(
                _('Restartable payroll run could not be started (%s). No partial payroll result was prepared.'),
                $start_error
            ));
        } else {
            $chunk = hrm_pay_core_009_run_chunk($start['run_id'], 'browser', null, $run_error);
            if ($chunk === false)
                display_error(sprintf(_('Payroll run stopped safely (%s).'), $run_error));
            elseif ($chunk['finalized'])
                display_notification(sprintf(
                    _('Payroll results prepared for approval. Run #%d, Period #%d.'),
                    (int)$start['run_id'], (int)$start['payroll_period_id']
                ));
            else
                display_notification(sprintf(
                    _('Payroll run #%d saved a durable checkpoint. Use Resume to continue.'),
                    (int)$start['run_id']
                ));
            $pay_core_009_last_status = hrm_pay_core_009_status($start['run_id'], $status_error);
        }
    }
}

$payroll_process_selector_as_of = hrm_person_worker_utc_now();
hrm_log_restricted_employee_projection('payroll_process_selector');

start_form();

start_table(TABLESTYLE2);
text_row(_('Payroll Period Name:'), 'period_name', get_post('period_name', _('Payroll ').Today()), 42, 80);
date_row(_('From Date:'), 'from_date', get_post('from_date', begin_month(Today())));
date_row(_('To Date:'), 'to_date', get_post('to_date', end_month(Today())));
departments_list_row(_('Department (Optional):'), 'department_id', null, true, _('All departments'));

start_row();
label_cell(_('Employee (Optional):'));
employees_list_cells(null, 'employee_id', null, _('-- All Employees --'), true, false, true, array(
    'layout_class' => 'combo-layout-equal',
    'format' => 'payroll_process_authoritative_employee_list'
));
end_row();
end_table(1);

submit_center('process_payroll', _('Process Payroll'), true, '', 'default');

if (is_array($pay_core_009_last_status))
    display_pay_core_009_progress($pay_core_009_last_status);

$open_runs = hrm_pay_core_009_open_runs_for_actor($open_error);
if (is_array($open_runs) && !empty($open_runs)) {
    start_table(TABLESTYLE_DATA);
    table_header(array(_('Run'), _('Period'), _('State'), _('Checkpoint'), _('Progress'), ''));
    foreach ($open_runs as $run) {
        $counts = $run['counts'];
        start_row();
        label_cell((int)$run['run_id']);
        label_cell((int)$run['payroll_period_id']);
        label_cell((string)$run['state']);
        label_cell((int)$run['checkpoint_no']);
        label_cell(sprintf(_('Completed %d / Pending %d / Failed %d / Quarantined %d'),
            (int)$counts['completed'], (int)$counts['pending'], (int)$counts['failed'], (int)$counts['quarantined']));
        if (!empty($run['can_resume']))
            submit_cells('ResumeRun'.$run['run_id'], _('Resume'), _('Execute one bounded payroll chunk.'), true);
        elseif (!empty($run['needs_recovery']))
            submit_cells('RecoverRun'.$run['run_id'], _('Recover'), _('Record forward recovery evidence and resume the run.'), true);
        else
            label_cell('');
        end_row();
    }
    end_table(1);
}

end_form();

end_page();
