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

include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/includes/db/comments_db.inc');
include_once($path_to_root.'/includes/db/audit_trail_db.inc');
include_once($path_to_root.'/gl/includes/db/gl_db_trans.inc');

include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/hrm_hooks.inc');
include_once($path_to_root.'/hrm/includes/hrm_class.inc');
include_once($path_to_root.'/hrm/includes/db/employee_db.inc');
include_once($path_to_root.'/hrm/includes/db/attendance_db.inc');
include_once($path_to_root.'/hrm/includes/db/payslip_db.inc');
include_once($path_to_root.'/hrm/includes/db/leave_balance_db.inc');

/**
 * Payroll Formula Engine.
 *
 * Evaluates arithmetic formulas for pay elements using controlled variables.
 */
class payroll_formula_engine {
    /** @var array */
    private $variables = array();

    /**
     * Set full formula context.
     *
     * @param array $vars
     * @return void
     */
    function set_context($vars) {
        $this->variables = array();
        if (!is_array($vars))
            return;

        foreach ($vars as $name => $value)
            $this->set_variable($name, $value);
    }

    /**
     * Set one formula variable.
     *
     * @param string $name
     * @param float|int $value
     * @return void
     */
    function set_variable($name, $value) {
        $this->variables[strtoupper(trim($name))] = (float)$value;
    }

    /**
     * Evaluate a formula string.
     *
     * Supported operators: + - * / % and parentheses.
     *
     * @param string $formula
     * @return float
     */
    function evaluate($formula) {
        if (!is_string($formula) || trim($formula) === '')
            return 0.0;

        $expr = strtoupper($formula);

        foreach ($this->variables as $name => $value)
            $expr = preg_replace('/\b'.preg_quote($name, '/').'\b/', (string)((float)$value), $expr);

        return $this->safe_eval($expr);
    }

    /**
     * Safely evaluate numeric expression.
     *
     * @param string $expr
     * @return float
     */
    private function safe_eval($expr) {
        $expr = trim((string)$expr);
        if ($expr === '')
            return 0.0;

        if (!preg_match('/^[\d\s\+\-\*\/\%\(\)\.]+$/', $expr))
            return 0.0;

        $result = 0.0;
        set_error_handler(function() { return true; });
        try {
            $value = eval('return ('.$expr.');');
            $result = is_numeric($value) ? (float)$value : 0.0;
        } catch (Throwable $e) {
            $result = 0.0;
        }
        restore_error_handler();

        if (is_nan($result) || is_infinite($result))
            return 0.0;

        return $result;
    }
}

/**
 * Calculate employee payslip for a period.
 *
 * @param array  $employee Employee row (or minimum with employee_id)
 * @param string $from_date User date format
 * @param string $to_date User date format
 * @param int    $period_id Payroll period id
 * @return payslip_doc|false
 */
function calculate_employee_payslip($employee, $from_date, $to_date, $period_id=0) {
    if (!is_array($employee) || empty($employee['employee_id']))
        return false;

    if (!is_date($from_date) || !is_date($to_date))
        return false;

    if (date1_greater_date2($from_date, $to_date))
        return false;

    if (function_exists('hrm_has_hook') && hrm_has_hook('before_calculate_payslip'))
        hrm_fire_hook('before_calculate_payslip', $employee, $from_date, $to_date);

    $employee_full = _payroll_get_employee_full_row($employee['employee_id'], $employee);

    $doc = new payslip_doc();
    $doc->payroll_period_id = (int)$period_id;
    $doc->employee_id = $employee_full['employee_id'];
    $doc->employee = $employee_full;
    $doc->from_date = $from_date;
    $doc->to_date = $to_date;
    $doc->tran_date = $to_date;

    $attendance = get_attendance_summary($doc->employee_id, $from_date, $to_date);
    $doc->working_days = $attendance['working_days'];
    $doc->worked_days = $attendance['worked_days'];
    $doc->leave_days = $attendance['leave_days'];
    $doc->absent_days = $attendance['absent_days'];
    $doc->overtime_hours = $attendance['overtime_hours'];

    $salary_components = array();
    if (function_exists('get_employee_salary_components'))
        $salary_components = get_employee_salary_components($doc->employee_id, date2sql($to_date));

    if (!is_array($salary_components))
        $salary_components = array();

    $formula = new payroll_formula_engine();
    $basic = _payroll_get_basic_salary($salary_components, $employee_full);
    $doc->basic_salary = $basic;

    $working_days = max((float)$doc->working_days, 1.0);
    $worked_days = (float)$doc->worked_days;
    if ($worked_days <= 0 && $doc->absent_days <= 0)
        $worked_days = $working_days;

    $daily_rate = $basic / $working_days;
    $hourly_rate = $daily_rate / 8;

    $formula->set_context(array(
        'BASIC' => $basic,
        'GROSS' => $basic,
        'DAYS_WORKED' => $worked_days,
        'WORKING_DAYS' => $working_days,
        'DAYS_IN_MONTH' => $working_days,
        'OVERTIME_HOURS' => (float)$doc->overtime_hours,
        'UNPAID_LEAVE_DAYS' => (float)$doc->absent_days,
        'HOURLY_RATE' => $hourly_rate
    ));

    if (function_exists('hrm_has_hook') && hrm_has_hook('after_calculate_basic'))
        hrm_fire_hook('after_calculate_basic', $doc, $employee_full);

    foreach ($salary_components as $component) {
        $amount = _payroll_calculate_component_amount($component, $formula, $basic, $doc);
        $is_deduction = !empty($component['is_deduction']) ? 1 : 0;

        $doc->add_line(
            @$component['element_id'],
            @$component['element_name'],
            isset($component['element_category']) ? $component['element_category'] : 0,
            $is_deduction,
            isset($component['amount_type']) ? $component['amount_type'] : 0,
            isset($component['amount']) ? $component['amount'] : 0,
            0,
            $amount,
            $amount,
            isset($component['account_code']) ? $component['account_code'] : '',
            empty($component['formula']) ? null : $component['formula'],
            empty($component['is_taxable']) ? 0 : 1,
            empty($component['display_order']) ? 0 : $component['display_order']
        );

        $formula->set_variable('GROSS', $basic + _payroll_doc_total_earnings($doc));
    }

    if (function_exists('hrm_has_hook') && hrm_has_hook('after_calculate_earnings'))
        hrm_fire_hook('after_calculate_earnings', $doc, $employee_full);

    $attendance_deduction = calculate_attendance_deductions($doc->employee_id, $from_date, $to_date, $daily_rate);
    if (floatcmp($attendance_deduction, 0) != 0) {
        $doc->add_line(0, _('Attendance Deduction'), HRM_ELEM_DEDUCTION, 1, HRM_AMTTYPE_ATTENDANCE,
            $attendance_deduction, 0, $attendance_deduction, $attendance_deduction, '', null, 0, 9000);
    }

    if (function_exists('hrm_has_hook') && hrm_has_hook('before_calculate_deductions'))
        hrm_fire_hook('before_calculate_deductions', $doc, $employee_full);

    $taxable_income = max(_payroll_doc_total_taxable_income($doc), 0);
    $income_tax = calculate_income_tax($taxable_income, $to_date);
    if (floatcmp($income_tax, 0) != 0) {
        $doc->add_line(0, _('Income Tax'), HRM_ELEM_STATUTORY_DEDUCTION, 1, HRM_AMTTYPE_FIXED,
            $income_tax, 0, $income_tax, $income_tax, '', null, 0, 9100);
    }

    $statutory = calculate_statutory_deductions($doc->basic_salary, max(_payroll_doc_total_earnings($doc), $doc->basic_salary), $to_date);
    foreach ($statutory as $entry) {
        $emp_amt = (float)$entry['employee_amount'];
        $employer_amt = (float)$entry['employer_amount'];

        if (floatcmp($emp_amt, 0) != 0) {
            $doc->add_line(0, $entry['name'], HRM_ELEM_STATUTORY_DEDUCTION, 1, HRM_AMTTYPE_FIXED,
                $emp_amt, 0, $emp_amt, $emp_amt, $entry['employee_account'], null, 0, 9200);
        }

        if (floatcmp($employer_amt, 0) != 0)
            $doc->employer_contributions += $employer_amt;
    }

    $doc->recalculate_totals();

    if (function_exists('hrm_has_hook') && hrm_has_hook('after_calculate_payslip'))
        hrm_fire_hook('after_calculate_payslip', $doc, $employee_full);

    return $doc;
}

/**
 * Post payslip to GL using hrm_cart.
 *
 * @param payslip_doc $payslip_doc
 * @return int|false GL transaction number on success, false on failure
 */
function post_payslip_to_gl(&$payslip_doc) {
    if (!is_object($payslip_doc) || empty($payslip_doc->employee_id))
        return false;

    if (function_exists('hrm_has_hook') && hrm_has_hook('before_post_payslip'))
        hrm_fire_hook('before_post_payslip', $payslip_doc);

    $trans_no = get_next_trans_no(ST_PAYSLIP);
    $cart = new hrm_cart(ST_PAYSLIP, $trans_no);
    $cart->trans_no = $trans_no;
    $cart->order_id = $trans_no;
    $cart->tran_date = empty($payslip_doc->tran_date) ? Today() : $payslip_doc->tran_date;
    $cart->reference = _payroll_get_reference(ST_PAYSLIP, $cart->tran_date);
    $cart->set_employee($payslip_doc->employee_id, _payroll_get_employee_name($payslip_doc->employee));

    $total = 0.0;
    foreach ($payslip_doc->lines as $line) {
        if (empty($line->account_code) || floatcmp($line->final_amount, 0) == 0)
            continue;

        $amount = (float)$line->final_amount;
        if (!empty($line->is_deduction))
            $amount = -abs($amount);
        else
            $amount = abs($amount);

        $total += $amount;
        $cart->add_gl_item($line->account_code, 0, 0, $amount, _('Payroll Element'), null, $payslip_doc->employee_id, $cart->tran_date);
    }

    $payable_account = get_company_pref('payroll_payable_act');
    if (empty($payable_account))
        return false;

    if (floatcmp($total, 0) != 0)
        $cart->add_gl_item($payable_account, 0, 0, -$total, _('Payroll Payable'), null, $payslip_doc->employee_id, $cart->tran_date);

    $cart->write_gl(true);
    add_comments(ST_PAYSLIP, $trans_no, $cart->tran_date, _('Payroll generated'));
    add_audit_trail(ST_PAYSLIP, $trans_no, $cart->tran_date);

    if (function_exists('add_payslip')) {
        $payslip_doc->reference = $cart->reference;
        $payslip_doc->gl_trans_no = $trans_no;
        $payslip_id = add_payslip($payslip_doc);
        if ($payslip_id)
            $payslip_doc->payslip_id = $payslip_id;
    }

	payroll_sync_leave_balances_for_period($payslip_doc->employee_id, $payslip_doc->from_date, $payslip_doc->to_date);

    if (function_exists('hrm_has_hook') && hrm_has_hook('after_post_payslip'))
        hrm_fire_hook('after_post_payslip', $payslip_doc->payslip_id, $trans_no);

    return $trans_no;
}

/**
 * Synchronize leave balances from approved and pending leave requests.
 *
 * Called during payroll posting to keep leave balances aligned with latest requests.
 *
 * @param string $employee_id
 * @param string $from_date User date format
 * @param string $to_date User date format
 * @return void
 */
function payroll_sync_leave_balances_for_period($employee_id, $from_date, $to_date) {
    if (!function_exists('employee_table_exists'))
        return;

    if (!employee_table_exists('leave_balances') || !employee_table_exists('leave_requests'))
        return;

    $to_sql = date2sql($to_date);
    $fiscal_year = (int)date('Y', strtotime($to_sql));

    $approved = array();
    $pending = array();

    $approved_sql = "SELECT leave_id, SUM(days) total_days
        FROM ".TB_PREF."leave_requests
        WHERE employee_id = ".db_escape($employee_id)."
            AND status = 1
            AND YEAR(from_date) = ".db_escape($fiscal_year)."
        GROUP BY leave_id";
    $approved_res = db_query($approved_sql, 'Could not summarize approved leave requests');
    while ($row = db_fetch_assoc($approved_res))
        $approved[(int)$row['leave_id']] = (float)$row['total_days'];

    $pending_sql = "SELECT leave_id, SUM(days) total_days
        FROM ".TB_PREF."leave_requests
        WHERE employee_id = ".db_escape($employee_id)."
            AND status = 0
            AND YEAR(from_date) = ".db_escape($fiscal_year)."
        GROUP BY leave_id";
    $pending_res = db_query($pending_sql, 'Could not summarize pending leave requests');
    while ($row = db_fetch_assoc($pending_res))
        $pending[(int)$row['leave_id']] = (float)$row['total_days'];

    $leave_ids = array_unique(array_merge(array_keys($approved), array_keys($pending)));
    foreach ($leave_ids as $leave_id) {
        $existing = get_leave_balance($employee_id, $leave_id, $fiscal_year);
        $entitled = $existing ? (float)$existing['entitled'] : 0;
        $carried = $existing ? (float)$existing['carried_forward'] : 0;
        $adjusted = $existing ? (float)$existing['adjusted'] : 0;

        save_leave_balance(
            $employee_id,
            $leave_id,
            $fiscal_year,
            $entitled,
            $carried,
            isset($approved[$leave_id]) ? (float)$approved[$leave_id] : 0,
            isset($pending[$leave_id]) ? (float)$pending[$leave_id] : 0,
            $adjusted
        );
    }
}

/**
 * Calculate progressive income tax.
 *
 * @param float $taxable_income
 * @param string $as_of_date User date format
 * @return float
 */
function calculate_income_tax($taxable_income, $as_of_date) {
    $taxable_income = max((float)$taxable_income, 0.0);
    if (floatcmp($taxable_income, 0) == 0)
        return 0.0;

    $sql_date = date2sql($as_of_date);
    $sql = "SELECT * FROM ".TB_PREF."tax_brackets
        WHERE effective_from <= ".db_escape($sql_date)."
        AND (effective_to IS NULL OR effective_to >= ".db_escape($sql_date).")
        ORDER BY from_amount ASC";
    $result = db_query($sql, 'Could not get tax brackets');

    $tax = 0.0;
    while ($row = db_fetch_assoc($result)) {
        $from = (float)$row['from_amount'];
        $to = isset($row['to_amount']) ? (float)$row['to_amount'] : 0.0;
        $rate = (float)$row['rate'];
        $fixed = (float)$row['fixed_amount'];

        if ($taxable_income < $from)
            continue;

        if (empty($row['to_amount']) || $to <= 0 || $taxable_income <= $to) {
            $tax += $fixed + (($taxable_income - $from) * $rate / 100);
            break;
        }
    }

    return _payroll_round($tax);
}

/**
 * Calculate statutory deductions and employer contributions.
 *
 * @param float $base_salary
 * @param float $gross_salary
 * @param string $as_of_date User date format
 * @return array
 */
function calculate_statutory_deductions($base_salary, $gross_salary, $as_of_date) {
    $sql_date = date2sql($as_of_date);
    $sql = "SELECT * FROM ".TB_PREF."statutory_deductions
        WHERE inactive = 0
        AND effective_from <= ".db_escape($sql_date)."
        AND (effective_to IS NULL OR effective_to >= ".db_escape($sql_date).")";
    $result = db_query($sql, 'Could not get statutory deductions');

    $rows = array();
    while ($row = db_fetch_assoc($result)) {
        $base = ((int)$row['calculation_base'] === HRM_STAT_BASE_GROSS) ? (float)$gross_salary : (float)$base_salary;

        if (!empty($row['ceiling_amount']))
            $base = min($base, (float)$row['ceiling_amount']);
        if (!empty($row['floor_amount']))
            $base = max($base, (float)$row['floor_amount']);

        $employee_amount = ((float)$row['employee_fixed']) + ($base * (float)$row['employee_rate'] / 100);
        $employer_amount = ((float)$row['employer_fixed']) + ($base * (float)$row['employer_rate'] / 100);

        $rows[] = array(
            'statutory_id' => $row['statutory_id'],
            'name' => $row['statutory_name'],
            'employee_amount' => _payroll_round($employee_amount),
            'employer_amount' => _payroll_round($employer_amount),
            'employee_account' => $row['employee_account'],
            'employer_account' => $row['employer_account']
        );
    }

    return $rows;
}

/**
 * Get attendance summary for employee within period.
 *
 * @param string $employee_id
 * @param string $from_date User date format
 * @param string $to_date User date format
 * @return array
 */
function get_attendance_summary($employee_id, $from_date, $to_date) {
    $from_sql = date2sql($from_date);
    $to_sql = date2sql($to_date);

    $sql = "SELECT
            COUNT(*) as records_count,
            SUM(CASE WHEN status = 1 THEN 1 ELSE 0 END) as absent_days,
            SUM(CASE WHEN status = 0 THEN 1 ELSE 0 END) as worked_days,
            SUM(IFNULL(overtime_hours, 0)) as overtime_hours,
            SUM(IFNULL(regular_hours, 0)) as regular_hours
        FROM ".TB_PREF."attendance
        WHERE employee_id = ".db_escape($employee_id)."
        AND date >= ".db_escape($from_sql)."
        AND date <= ".db_escape($to_sql);
    $result = db_query($sql, 'Could not get attendance summary');
    $row = db_fetch_assoc($result);

    $working_days = get_working_days_in_period($from_date, $to_date);
    $worked_days = (float)@$row['worked_days'];
    $absent_days = (float)@$row['absent_days'];

    $leave_sql = "SELECT SUM(days) as leave_days
        FROM ".TB_PREF."leave_details
        WHERE employee_id = ".db_escape($employee_id)."
        AND from_date <= ".db_escape($to_sql)."
        AND to_date >= ".db_escape($from_sql)."
        AND approval_status <> 'rejected'";
    $leave_res = db_query($leave_sql, 'Could not get leave summary');
    $leave_row = db_fetch_assoc($leave_res);
    $leave_days = (float)@$leave_row['leave_days'];

    if ($worked_days <= 0 && $absent_days <= 0)
        $worked_days = $working_days;

    return array(
        'working_days' => (float)$working_days,
        'worked_days' => (float)$worked_days,
        'leave_days' => (float)$leave_days,
        'absent_days' => (float)$absent_days,
        'overtime_hours' => (float)@$row['overtime_hours'],
        'late_minutes' => 0.0,
        'paid_leave_days' => (float)$leave_days,
        'unpaid_leave_days' => (float)$absent_days
    );
}

/**
 * Get company working days count in a period.
 *
 * @param string $from_date User date format
 * @param string $to_date User date format
 * @return int
 */
function get_working_days_in_period($from_date, $to_date) {
    $from = date2sql($from_date);
    $to = date2sql($to_date);

    $start = new DateTime($from);
    $end = new DateTime($to);
    $end->modify('+1 day');

    $days = 0;
    for ($date = clone $start; $date < $end; $date->modify('+1 day')) {
        $dow = (int)$date->format('w');
        if (!_payroll_is_working_day($dow))
            continue;
        if (_payroll_is_holiday($date->format('Y-m-d')))
            continue;
        $days++;
    }

    return $days;
}

/**
 * Calculate attendance-based deductions using configured rules.
 *
 * @param string $employee_id
 * @param string $from_date User date format
 * @param string $to_date User date format
 * @param float $daily_rate
 * @return float
 */
function calculate_attendance_deductions($employee_id, $from_date, $to_date, $daily_rate) {
    $daily_rate = (float)$daily_rate;
    if (floatcmp($daily_rate, 0) <= 0)
        return 0.0;

    $summary = get_attendance_summary($employee_id, $from_date, $to_date);
    $absence_days = (float)$summary['absent_days'];

    $sql = "SELECT * FROM ".TB_PREF."attendance_deduction_rules
        WHERE inactive = 0
        ORDER BY rule_type, from_value";
    $result = db_query($sql, 'Could not get attendance deduction rules');

    $deduction_days = 0.0;
    while ($row = db_fetch_assoc($result)) {
        if ((int)$row['rule_type'] !== HRM_DEDTYPE_ABSENCE)
            continue;

        $from = (float)$row['from_value'];
        $to = (float)$row['to_value'];
        if ($absence_days >= $from && ($to <= 0 || $absence_days <= $to))
            $deduction_days += (float)$row['deduction_rate'];
    }

    if (floatcmp($deduction_days, 0) == 0)
        $deduction_days = $absence_days;

    return _payroll_round($deduction_days * $daily_rate);
}

/**
 * Get complete employee row with best available function.
 *
 * @param string $employee_id
 * @param array $fallback
 * @return array
 */
function _payroll_get_employee_full_row($employee_id, $fallback=array()) {
    if (function_exists('get_employee_by_code')) {
        $row = get_employee_by_code($employee_id);
        if (is_array($row) && !empty($row['employee_id']))
            return $row;
    }

    if (function_exists('get_employee_to_payslip')) {
        $row = get_employee_to_payslip($employee_id);
        if (is_array($row) && !empty($row['employee_id']))
            return array_merge($fallback, $row);
    }

    return $fallback;
}

/**
 * Determine basic salary from salary components or position setup.
 *
 * @param array $components
 * @param array $employee
 * @return float
 */
function _payroll_get_basic_salary($components, $employee) {
    foreach ((array)$components as $row) {
        $cat = isset($row['element_category']) ? (int)$row['element_category'] : -1;
        if ($cat === HRM_ELEM_BASIC)
            return _payroll_round(@$row['amount']);
    }

    if (!empty($employee['position_id']) && function_exists('get_job_position')) {
        $position = get_job_position($employee['position_id']);
        if (is_array($position) && isset($position['basic_amount']))
            return _payroll_round($position['basic_amount']);
    }

    return 0.0;
}

/**
 * Calculate a salary component amount.
 *
 * @param array $component
 * @param payroll_formula_engine $formula
 * @param float $basic
 * @param payslip_doc $doc
 * @return float
 */
function _payroll_calculate_component_amount($component, $formula, $basic, $doc) {
    $amount_type = isset($component['amount_type']) ? (int)$component['amount_type'] : HRM_AMTTYPE_FIXED;
    $base_amount = isset($component['amount']) ? (float)$component['amount'] : 0.0;
    $rate = isset($component['rate']) ? (float)$component['rate'] : 0.0;

    if (floatcmp($rate, 0) == 0 && floatcmp($base_amount, 0) != 0)
        $rate = $base_amount;

    $gross = $basic + _payroll_doc_total_earnings($doc);

    switch ($amount_type) {
        case HRM_AMTTYPE_PCT_BASIC:
            $amount = $basic * $rate / 100;
            break;
        case HRM_AMTTYPE_PCT_GROSS:
            $amount = $gross * $rate / 100;
            break;
        case HRM_AMTTYPE_FORMULA:
            $formula_text = empty($component['formula']) ? '' : $component['formula'];
            $amount = $formula->evaluate($formula_text);
            break;
        case HRM_AMTTYPE_ATTENDANCE:
            $working = max((float)$doc->working_days, 1.0);
            $worked = min((float)$doc->worked_days, $working);
            $amount = $base_amount * ($worked / $working);
            break;
        case HRM_AMTTYPE_FIXED:
        default:
            $amount = $base_amount;
            break;
    }

    return _payroll_round($amount);
}

/**
 * Sum non-deduction lines.
 *
 * @param payslip_doc $doc
 * @return float
 */
function _payroll_doc_total_earnings($doc) {
    $total = 0.0;
    foreach ((array)$doc->lines as $line) {
        if (empty($line->is_deduction))
            $total += (float)$line->final_amount;
    }
    return $total;
}

/**
 * Get taxable income from payslip lines.
 *
 * @param payslip_doc $doc
 * @return float
 */
function _payroll_doc_total_taxable_income($doc) {
    $total = 0.0;
    foreach ((array)$doc->lines as $line) {
        if (empty($line->is_taxable))
            continue;

        if (!empty($line->is_deduction))
            $total -= (float)$line->final_amount;
        else
            $total += (float)$line->final_amount;
    }
    return $total;
}

/**
 * Check if specific weekday is configured as working day.
 *
 * @param int $dow 0-6
 * @return bool
 */
function _payroll_is_working_day($dow) {
    $sql = "SELECT is_working FROM ".TB_PREF."working_days WHERE day_of_week = ".db_escape((int)$dow);
    $result = db_query($sql, 'Could not get working day');
    $row = db_fetch_assoc($result);

    if (!$row)
        return ($dow >= 1 && $dow <= 5);

    return !empty($row['is_working']);
}

/**
 * Check if date is holiday.
 *
 * @param string $sql_date SQL date Y-m-d
 * @return bool
 */
function _payroll_is_holiday($sql_date) {
    $year = substr($sql_date, 0, 4);
    $month_day = substr($sql_date, 5, 5);

    $sql = "SELECT holiday_id
        FROM ".TB_PREF."holidays
        WHERE (
            (holiday_date <= ".db_escape($sql_date)." AND (to_date IS NULL OR to_date >= ".db_escape($sql_date)."))
            OR (recurring = 1 AND DATE_FORMAT(holiday_date, '%m-%d') = ".db_escape($month_day).")
        )
        LIMIT 1";
    $result = db_query($sql, 'Could not check holiday');

    return db_num_rows($result) > 0;
}

/**
 * Get next transaction reference if reference manager is available.
 *
 * @param int $type
 * @param string $date
 * @return string
 */
function _payroll_get_reference($type, $date) {
    global $Refs;
    if (isset($Refs) && is_object($Refs) && method_exists($Refs, 'get_next'))
        return $Refs->get_next($type, null, $date);

    return '';
}

/**
 * Get employee display name.
 *
 * @param array $employee
 * @return string
 */
function _payroll_get_employee_name($employee) {
    if (!empty($employee['employee_name']))
        return $employee['employee_name'];

    $first = isset($employee['first_name']) ? $employee['first_name'] : '';
    $last = isset($employee['last_name']) ? $employee['last_name'] : '';
    $name = trim($first.' '.$last);

    if ($name === '' && !empty($employee['name']))
        $name = $employee['name'];

    return $name;
}

/**
 * Round payroll amounts by user price decimals.
 *
 * @param float|int $amount
 * @return float
 */
function _payroll_round($amount) {
    $dec = function_exists('user_price_dec') ? user_price_dec() : 2;
    return round2((float)$amount, $dec);
}

