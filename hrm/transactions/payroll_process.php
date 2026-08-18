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
include_once($path_to_root.'/hrm/includes/db/payroll_db.inc');
include_once($path_to_root.'/hrm/includes/payroll_engine.inc');

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
        $period_id = add_payroll_period($period_name, $from_date, $to_date, $department_id ? $department_id : null);
        if (!$period_id) {
            if (!payroll_table_exists('payroll_periods'))
                display_error(_('Could not create payroll period because payroll_periods table is missing in the current company database.'));
            else
                display_error(_('Could not create payroll period.'));
            return;
        }

        $success_count = 0;
        $failed_count = count($skip_reasons['existing_payslips'])
			+ count($skip_reasons['not_hired'])
            + count($skip_reasons['missing_position'])
            + count($skip_reasons['missing_salary_components']);

        display_payroll_skip_messages($skip_reasons);

        $runtime_context = array(
            'salary_components' => function_exists('get_salary_components_for_employees')
                ? get_salary_components_for_employees($employees, $to_date)
                : array()
        );

        $prepared_documents = array();
        $calculation_failed_count = 0;
        foreach ($employees as $employee) {
            $payslip_doc = calculate_employee_payslip_read_only(
                $employee,
                $from_date,
                $to_date,
                $period_id,
                $runtime_context
            );
            if (!$payslip_doc) {
                $failed_count++;
                $calculation_failed_count++;
                continue;
            }

            $prepared_documents[] = $payslip_doc;
        }

        if ($calculation_failed_count > 0) {
            $preparation = array(
                'ok' => false,
                'code' => 'incomplete_calculation_set',
                'message' => _(
                    'Payroll calculation failed for one or more eligible employees. '
                    .'No partial payroll result was prepared.'
                ),
            );
        } else {
            $preparation = !empty($prepared_documents)
                ? prepare_payroll_period_results_for_approval($period_id, $prepared_documents)
                : array('ok' => false, 'code' => 'empty_payroll_result');
        }

        if (!empty($preparation['ok'])) {
            $success_count = (int)$preparation['payslip_count'];
            display_notification(sprintf(
                _('Payroll results prepared for approval. Success: %d employee(s), Failed: %d employee(s), Period ID: %d'),
                $success_count,
                $failed_count,
                $period_id
            ));
        } else {
            $failed_count += count($prepared_documents);
            $discarded = discard_empty_payroll_period_draft($period_id);
            if (isset($preparation['message']) && trim((string)$preparation['message']) !== '')
                display_error($preparation['message']);
            else
                display_error(_('Payroll results could not be prepared for approval.'));

            if (!$discarded) {
                display_warning(sprintf(
                    _('Draft payroll period #%d was retained for controlled recovery because it was not safe to remove automatically.'),
                    $period_id
                ));
            }
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

end_form();

end_page();
