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
include_once($path_to_root.'/gl/includes/db/gl_db_accounts.inc');
include_once($path_to_root.'/gl/includes/db/gl_db_trans.inc');

include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/hrm_hooks.inc');
include_once($path_to_root.'/hrm/includes/hrm_class.inc');
include_once($path_to_root.'/hrm/includes/db/employee_db.inc');
include_once($path_to_root.'/hrm/includes/db/job_positions_entity.inc');
include_once($path_to_root.'/hrm/includes/db/attendance_db.inc');
include_once($path_to_root.'/hrm/includes/db/payslip_db.inc');
include_once($path_to_root.'/hrm/includes/db/leave_balance_db.inc');
include_once($path_to_root.'/hrm/includes/db/leave_type_db.inc');
include_once($path_to_root.'/hrm/includes/db/overtime_type_db.inc');
include_once($path_to_root.'/hrm/includes/db/loan_db.inc');
include_once($path_to_root.'/hrm/includes/db/hr_settings_db.inc');

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
 * @param array  $context Optional preloaded runtime context.
 * @return payslip_doc|false
 */
function calculate_employee_payslip($employee, $from_date, $to_date, $period_id=0, $context=array()) {
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
    $doc->currency_code = isset($employee_full['contract_currency']) ? trim((string)$employee_full['contract_currency']) : '';
    $doc->exchange_rate = (isset($employee_full['contract_ex_rate']) && (float)$employee_full['contract_ex_rate'] > 0)
        ? (float)$employee_full['contract_ex_rate']
        : 1;

    $attendance = get_attendance_summary($doc->employee_id, $from_date, $to_date);
    $doc->working_days = $attendance['working_days'];
    $doc->worked_days = $attendance['worked_days'];
    $doc->leave_days = $attendance['leave_days'];
    $doc->absent_days = $attendance['absent_days'];
    $doc->extra_absent_days = isset($attendance['extra_absent_days']) ? (float)$attendance['extra_absent_days'] : 0.0;
    $doc->overtime_hours = $attendance['overtime_hours'];
    $doc->regular_hours = isset($attendance['regular_hours']) ? (float)$attendance['regular_hours'] : 0.0;
    $doc->paid_leave_days = isset($attendance['paid_leave_days']) ? (float)$attendance['paid_leave_days'] : 0.0;
    $doc->unpaid_leave_days = isset($attendance['unpaid_leave_days']) ? (float)$attendance['unpaid_leave_days'] : (float)$doc->absent_days;
    $doc->payable_days = isset($attendance['payable_days']) ? (float)$attendance['payable_days'] : (float)$doc->worked_days;
    $doc->leave_breakdown = isset($attendance['leave_breakdown']) && is_array($attendance['leave_breakdown'])
        ? $attendance['leave_breakdown']
        : array();
    $doc->overtime_breakdown = isset($attendance['overtime_breakdown']) && is_array($attendance['overtime_breakdown'])
        ? $attendance['overtime_breakdown']
        : array();

    $salary_components = array();
    if (isset($context['salary_components'][$doc->employee_id]) && is_array($context['salary_components'][$doc->employee_id]))
        $salary_components = $context['salary_components'][$doc->employee_id];
    elseif (function_exists('get_employee_salary_components'))
        $salary_components = get_employee_salary_components($doc->employee_id, date2sql($to_date));

    if (!is_array($salary_components))
        $salary_components = array();

    $formula = new payroll_formula_engine();
    $basic = _payroll_get_basic_salary($salary_components, $employee_full);
    $doc->basic_salary = $basic;

    $working_days = max((float)$doc->working_days, 1.0);
    $worked_days = min(max((float)$doc->payable_days, 0.0), $working_days);
    if ($worked_days <= 0 && (float)$doc->unpaid_leave_days <= 0)
        $worked_days = $working_days;

    $daily_rate = $basic / $working_days;
    $hourly_rate = $daily_rate / 8;

    $formula->set_context(array(
        'BASIC' => $basic,
        'GROSS' => $basic,
        'DAYS_WORKED' => $worked_days,
        'PAYABLE_DAYS' => $worked_days,
        'WORKING_DAYS' => $working_days,
        'DAYS_IN_MONTH' => $working_days,
        'LEAVE_DAYS' => (float)$doc->leave_days,
        'PAID_LEAVE_DAYS' => (float)$doc->paid_leave_days,
        'ABSENT_DAYS' => (float)$doc->absent_days,
        'OVERTIME_HOURS' => (float)$doc->overtime_hours,
        'UNPAID_LEAVE_DAYS' => (float)$doc->unpaid_leave_days,
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

    $attendance_deduction = calculate_attendance_deductions($doc->employee_id, $from_date, $to_date, $daily_rate, $attendance);
    if (floatcmp($attendance_deduction, 0) != 0) {
        $deduction_account = _payroll_resolve_synthetic_account('attendance_deduction', $salary_components);
        $doc->add_line(0, _('Attendance Deduction'), HRM_ELEM_DEDUCTION, 1, HRM_AMTTYPE_ATTENDANCE,
            $attendance_deduction, 0, $attendance_deduction, $attendance_deduction, $deduction_account, null, 0, 9000);
    }

    if (function_exists('hrm_has_hook') && hrm_has_hook('before_calculate_deductions'))
        hrm_fire_hook('before_calculate_deductions', $doc, $employee_full);

    $taxable_income = max(_payroll_doc_total_taxable_income($doc), 0);
    $income_tax = calculate_income_tax($taxable_income, $to_date);
    if (floatcmp($income_tax, 0) != 0) {
        $tax_account = _payroll_resolve_synthetic_account('income_tax', $salary_components);
        $doc->add_line(0, _('Income Tax'), HRM_ELEM_STATUTORY_DEDUCTION, 1, HRM_AMTTYPE_FIXED,
            $income_tax, 0, $income_tax, $income_tax, $tax_account, null, 0, 9100);
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

    $loan_deduction = 0.0;
    $loan_account = '';
    if (function_exists('get_total_due_loan_deduction')) {
        $loan_deduction = get_total_due_loan_deduction($doc->employee_id, $from_date, $to_date);
        if (function_exists('get_due_loan_deduction_account'))
            $loan_account = get_due_loan_deduction_account($doc->employee_id, $from_date, $to_date);
    }

    if (floatcmp($loan_deduction, 0) != 0) {
        $doc->loan_deduction = (float)$loan_deduction;
        $doc->add_line(0, _('Loan Repayment'), HRM_ELEM_DEDUCTION, 1, HRM_AMTTYPE_FIXED,
            $loan_deduction, 0, $loan_deduction, $loan_deduction, $loan_account, null, 0, 9300);
    }

    $doc->recalculate_totals();
    $doc->net_salary_home = $doc->exchange_rate > 0 ? ($doc->net_salary * $doc->exchange_rate) : $doc->net_salary;

    if (function_exists('hrm_has_hook') && hrm_has_hook('after_calculate_payslip'))
        hrm_fire_hook('after_calculate_payslip', $doc, $employee_full);

    return $doc;
}

/**
 * Get the next payslip transaction number, accounting for partial writes.
 *
 * Payslip posting writes GL, references and comments before the payslip header
 * exists, so relying on the header table alone can reuse a failed transaction
 * number on retry.
 *
 * @return int
 */
function payroll_get_next_payslip_trans_no() {
    $next_trans_no = (int)get_next_trans_no(ST_PAYSLIP);
    $queries = array(
        "SELECT MAX(type_no) AS last_no FROM ".TB_PREF."gl_trans WHERE type = ".db_escape(ST_PAYSLIP),
        "SELECT MAX(id) AS last_no FROM ".TB_PREF."comments WHERE type = ".db_escape(ST_PAYSLIP),
        "SELECT MAX(id) AS last_no FROM ".TB_PREF."refs WHERE type = ".db_escape(ST_PAYSLIP),
        "SELECT MAX(trans_no) AS last_no FROM ".TB_PREF."audit_trail WHERE type = ".db_escape(ST_PAYSLIP)
    );

    foreach ($queries as $sql) {
        $result = db_query($sql);
        if (!$result)
            continue;

        $row = db_fetch($result);
        $last_no = isset($row['last_no']) ? (int)$row['last_no'] : 0;
        if ($last_no >= $next_trans_no)
            $next_trans_no = $last_no + 1;
    }

    return $next_trans_no;
}

/**
 * Post payslip to GL using hrm_cart.
 *
 * @param payslip_doc $payslip_doc
 * @return int|false GL transaction number on success, false on failure
 */
function post_payslip_to_gl(&$payslip_doc) {
	global $Refs;

    if (!is_object($payslip_doc) || empty($payslip_doc->employee_id))
        return false;

    if (function_exists('hrm_has_hook') && hrm_has_hook('before_post_payslip'))
        hrm_fire_hook('before_post_payslip', $payslip_doc);

    $total = 0.0;
    $gl_totals = array();
    $invalid_accounts = array();
    foreach ($payslip_doc->lines as $line) {
        if (floatcmp($line->final_amount, 0) == 0)
            continue;

        if (empty($line->account_code)) {
            $invalid_accounts[] = $line->element_name;
            continue;
        }

        if (!_payroll_is_valid_gl_account($line->account_code)) {
            $invalid_accounts[] = $line->element_name . ' (' . $line->account_code . ')';
            continue;
        }

        $amount = (float)$line->final_amount;
        if (!empty($line->is_deduction))
            $amount = -abs($amount);
        else
            $amount = abs($amount);

        $total += $amount;
        if (!isset($gl_totals[$line->account_code]))
            $gl_totals[$line->account_code] = 0.0;
        $gl_totals[$line->account_code] += $amount;
    }

    // Reject posting if any pay elements have invalid GL accounts, rather
    // than silently omitting them from the GL entry. This prevents
    // unbalanced GL transactions (BUG-HRM-PAY-6-07, BUG-HRM-PAY-6-08).
    if (!empty($invalid_accounts)) {
        display_error(sprintf(
            _('Cannot post payslip. The following pay elements have missing or invalid GL accounts: %s. Please correct them in Pay Elements setup.'),
            implode(', ', $invalid_accounts)
        ));
        return false;
    }

    // Require at least one valid GL line before posting.
    if (empty($gl_totals) || floatcmp($total, 0) == 0) {
        display_error(_('Cannot post payslip. No valid GL account lines were found.'));
        return false;
    }

    $payable_account = get_company_pref('payroll_payable_act');
    if (empty($payable_account)) {
        display_error(_('Payroll Payable GL account is not configured. Please set it in System Preferences.'));
        return false;
    }

    if (!_payroll_is_valid_gl_account($payable_account)) {
        display_error(sprintf(
            _('Payroll Payable GL account (%s) is invalid. Please correct it in System Preferences.'),
            $payable_account
        ));
        return false;
    }

    // Save the payslip header and details FIRST, before writing GL entries.
    // This prevents orphaned GL transactions when the payslip save fails.
    // (Fixes BUG-HRM-PAY-6-05: GL posted before payslip saved)
	begin_transaction();
	// Transaction numbers and generated references are shared by every
	// employee. Lock an always-present company row before allocating them so
	// concurrent payroll runs for different employees cannot reuse a number.
	$allocation_lock_sql = "SELECT name FROM ".TB_PREF."sys_prefs"
		." WHERE name = 'coy_name' FOR UPDATE";
	if (!db_query($allocation_lock_sql, 'could not lock payslip transaction allocation')) {
		cancel_transaction();
		return false;
	}
	$employee_lock_sql = "SELECT employee_id FROM ".TB_PREF."employees"
		." WHERE employee_id = ".db_escape($payslip_doc->employee_id)." FOR UPDATE";
	if (!db_query($employee_lock_sql, 'could not lock employee for payslip posting')) {
		cancel_transaction();
		return false;
	}
	if (payslip_exists_for_period($payslip_doc->employee_id, $payslip_doc->from_date, $payslip_doc->to_date)) {
		cancel_transaction();
		display_error(_('A non-voided payslip already overlaps the selected employee and period.'));
		return false;
	}

	$trans_no = payroll_get_next_payslip_trans_no();
	$cart = new hrm_cart(ST_PAYSLIP, $trans_no);
	$cart->trans_no = $trans_no;
	$cart->order_id = $trans_no;
	$cart->tran_date = empty($payslip_doc->tran_date) ? Today() : $payslip_doc->tran_date;
	$cart->reference = empty($payslip_doc->reference)
		? _payroll_get_reference(ST_PAYSLIP, $cart->tran_date)
		: $payslip_doc->reference;
	$cart->set_employee($payslip_doc->employee_id, _payroll_get_employee_name($payslip_doc->employee));

    $payslip_doc->reference = $cart->reference;
    $payslip_doc->gl_trans_no = $trans_no;
    if (function_exists('add_payslip')) {
        $payslip_id = add_payslip($payslip_doc);
        if (!$payslip_id) {
			cancel_transaction();
            return false;
		}

        $payslip_doc->payslip_id = $payslip_id;
    }

    foreach ($gl_totals as $account_code => $amount) {
        if (floatcmp($amount, 0) == 0)
            continue;
		$cart->add_gl_item($account_code, 0, 0, $amount, _('Payroll Element'), null, null, $cart->tran_date);
    }

    if (floatcmp($total, 0) != 0)
        $cart->add_gl_item($payable_account, 0, 0, -$total, _('Payroll Payable'), null, null, $cart->tran_date);

    $cart->write_gl(true);
    if (isset($Refs) && is_object($Refs) && method_exists($Refs, 'save') && $cart->reference !== '')
        $Refs->save(ST_PAYSLIP, $trans_no, $cart->reference);

    $comment = trim(isset($payslip_doc->comments) ? $payslip_doc->comments : '');
    if ($comment === '')
        $comment = _('Payroll generated');

    if (function_exists('delete_comments'))
        delete_comments(ST_PAYSLIP, $trans_no);
    add_comments(ST_PAYSLIP, $trans_no, $cart->tran_date, $comment);
    add_audit_trail(ST_PAYSLIP, $trans_no, $cart->tran_date);

	if (function_exists('mark_loan_repayments_paid_by_payslip') && !empty($payslip_doc->payslip_id))
		mark_loan_repayments_paid_by_payslip($payslip_doc->employee_id, $payslip_doc->from_date, $payslip_doc->to_date, $payslip_doc->payslip_id);

	payroll_sync_leave_balances_for_period($payslip_doc->employee_id, $payslip_doc->from_date, $payslip_doc->to_date);

    if (function_exists('hrm_has_hook') && hrm_has_hook('after_post_payslip'))
        hrm_fire_hook('after_post_payslip', $payslip_doc->payslip_id, $trans_no);

	commit_transaction();
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
    $fiscal_year = get_leave_balance_fiscal_year_for_date($to_sql);
    list($year_start, $year_end) = _payroll_get_year_bounds($fiscal_year);

    $approved = array();
    $pending = array();

    $approved_sql = "SELECT leave_id, SUM(days) total_days
        FROM ".TB_PREF."leave_requests
        WHERE employee_id = ".db_escape($employee_id)."
            AND status = 1
            AND from_date >= ".db_escape($year_start)."
            AND from_date <= ".db_escape($year_end)."
        GROUP BY leave_id";
    $approved_res = db_query($approved_sql, 'Could not summarize approved leave requests');
    while ($row = db_fetch_assoc($approved_res))
        $approved[(int)$row['leave_id']] = (float)$row['total_days'];

    $pending_sql = "SELECT leave_id, SUM(days) total_days
        FROM ".TB_PREF."leave_requests
        WHERE employee_id = ".db_escape($employee_id)."
            AND status = 0
            AND from_date >= ".db_escape($year_start)."
            AND from_date <= ".db_escape($year_end)."
        GROUP BY leave_id";
    $pending_res = db_query($pending_sql, 'Could not summarize pending leave requests');
    while ($row = db_fetch_assoc($pending_res))
        $pending[(int)$row['leave_id']] = (float)$row['total_days'];

    $leave_ids = array_unique(array_merge(array_keys($approved), array_keys($pending)));
    foreach ($leave_ids as $leave_id) {
		ensure_leave_balance_entitlement($employee_id, $leave_id, $fiscal_year, $to_date);
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
    static $tax_bracket_cache = array();

    $taxable_income = max((float)$taxable_income, 0.0);
    if (floatcmp($taxable_income, 0) == 0)
        return 0.0;

    $sql_date = date2sql($as_of_date);
    if (!isset($tax_bracket_cache[$sql_date])) {
        $sql = "SELECT * FROM ".TB_PREF."tax_brackets
            WHERE effective_from <= ".db_escape($sql_date)."
            AND (effective_to IS NULL OR effective_to >= ".db_escape($sql_date).")
            ORDER BY from_amount ASC";
        $result = db_query($sql, 'Could not get tax brackets');
        $tax_bracket_cache[$sql_date] = array();
        while ($row = db_fetch_assoc($result))
            $tax_bracket_cache[$sql_date][] = $row;
    }

    $tax = 0.0;
    foreach ($tax_bracket_cache[$sql_date] as $row) {
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
    static $statutory_cache = array();

    $sql_date = date2sql($as_of_date);
    if (!isset($statutory_cache[$sql_date])) {
        $sql = "SELECT * FROM ".TB_PREF."statutory_deductions
            WHERE inactive = 0
            AND effective_from <= ".db_escape($sql_date)."
            AND (effective_to IS NULL OR effective_to >= ".db_escape($sql_date).")";
        $result = db_query($sql, 'Could not get statutory deductions');
        $statutory_cache[$sql_date] = array();
        while ($row = db_fetch_assoc($result))
            $statutory_cache[$sql_date][] = $row;
    }

    $rows = array();
    foreach ($statutory_cache[$sql_date] as $row) {
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
    static $attendance_cache = array();

    $cache_key = $employee_id.'|'.$from_date.'|'.$to_date;
    if (isset($attendance_cache[$cache_key]))
        return $attendance_cache[$cache_key];

    $from_sql = date2sql($from_date);
    $to_sql = date2sql($to_date);

    $sql = "SELECT
            COUNT(*) as records_count,
            SUM(CASE WHEN status = 1 THEN 1 WHEN status = 2 THEN 0.5 ELSE 0 END) as absent_days,
            SUM(CASE WHEN status = 0 THEN 1 WHEN status = 2 THEN 0.5 ELSE 0 END) as worked_days,
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

    $leave_summary = _payroll_get_leave_summary($employee_id, $from_sql, $to_sql);
    $leave_days = (float)$leave_summary['leave_days'];
    $paid_leave_days = (float)$leave_summary['paid_leave_days'];
    $leave_unpaid_days = (float)$leave_summary['unpaid_leave_days'];
    $overtime_breakdown = _payroll_get_overtime_breakdown($employee_id, $from_sql, $to_sql);

    if ($worked_days <= 0 && $absent_days <= 0)
        $worked_days = max($working_days - $leave_days, 0.0);

    // Calculate extra absent days: count non-working days that fall between
    // consecutive absent working days (e.g. absent Thu + Mon → Fri/Sat/Sun
    // also count as absent).
    $extra_absent_days = 0.0;
    if ($absent_days > 0 && !empty(get_hr_setting_value('calculate_extra_absent_days', 0))) {
        $extra_absent_days = _calculate_extra_absent_days($employee_id, $from_sql, $to_sql);
    }

    $payable_days = $worked_days + $paid_leave_days;
    if ($payable_days > $working_days)
        $payable_days = (float)$working_days;

    $unpaid_leave_days = $leave_unpaid_days + $absent_days + $extra_absent_days;

    $attendance_cache[$cache_key] = array(
        'working_days' => (float)$working_days,
        'worked_days' => (float)$worked_days,
        'leave_days' => (float)$leave_days,
        'absent_days' => (float)$absent_days + $extra_absent_days,
        'extra_absent_days' => $extra_absent_days,
        'overtime_hours' => (float)@$row['overtime_hours'],
        'regular_hours' => (float)@$row['regular_hours'],
        'late_minutes' => 0.0,
        'paid_leave_days' => (float)$paid_leave_days,
        'unpaid_leave_days' => (float)$unpaid_leave_days,
        'payable_days' => (float)$payable_days,
        'leave_breakdown' => isset($leave_summary['breakdown']) && is_array($leave_summary['breakdown'])
            ? $leave_summary['breakdown']
            : array(),
        'overtime_breakdown' => $overtime_breakdown
    );

    return $attendance_cache[$cache_key];
}

/**
 * Get effective leave day totals for the requested period.
 *
 * Clips overlapping leave rows to the payslip window, ignores non-approved
 * requests, and converts leave payability into paid/unpaid day equivalents.
 *
 * @param string $employee_id
 * @param string $from_sql SQL date Y-m-d
 * @param string $to_sql SQL date Y-m-d
 * @return array
 */
function _payroll_get_leave_summary($employee_id, $from_sql, $to_sql) {
    static $leave_cache = array();

    $cache_key = $employee_id.'|'.$from_sql.'|'.$to_sql;
    if (isset($leave_cache[$cache_key]))
        return $leave_cache[$cache_key];

    $summary = array(
        'leave_days' => 0.0,
        'paid_leave_days' => 0.0,
        'unpaid_leave_days' => 0.0,
        'breakdown' => array(),
    );

    if (!function_exists('employee_table_exists') || !employee_table_exists('leave_details')) {
        $leave_cache[$cache_key] = $summary;
        return $leave_cache[$cache_key];
    }

    $has_leave_types = employee_table_exists('leave_types');
    $select_pay_rate = ($has_leave_types && function_exists('leave_type_has_column') && leave_type_has_column('pay_rate'))
        ? 'lt.pay_rate'
        : 'NULL';
    $select_is_paid = ($has_leave_types && function_exists('leave_type_has_column') && leave_type_has_column('is_paid'))
        ? 'lt.is_paid'
        : 'NULL';
    $select_leave_name = $has_leave_types ? 'lt.leave_name' : 'NULL';
    $select_leave_code = $has_leave_types ? 'lt.leave_code' : 'NULL';
    $join_leave_types = $has_leave_types
        ? ' LEFT JOIN '.TB_PREF.'leave_types lt ON lt.leave_id = ld.leave_type_id'
        : '';

    $sql = "SELECT ld.leave_type_id, ld.from_date, ld.to_date, ld.days,
            ".$select_leave_name." AS leave_name, ".$select_leave_code." AS leave_code,
            ".$select_pay_rate." AS pay_rate, ".$select_is_paid." AS is_paid
        FROM ".TB_PREF."leave_details ld".$join_leave_types."
        WHERE ld.employee_id = ".db_escape($employee_id)."
        AND ld.from_date <= ".db_escape($to_sql)."
        AND ld.to_date >= ".db_escape($from_sql)."
        AND COALESCE(ld.approval_status, '') = ".db_escape(HRM_STATUS_APPROVED);
    $result = db_query($sql, 'Could not get leave summary');

    while ($row = db_fetch_assoc($result)) {
        $range_days = _payroll_count_days_inclusive($row['from_date'], $row['to_date']);
        if ($range_days <= 0)
            continue;

        $overlap_from = ($row['from_date'] > $from_sql) ? $row['from_date'] : $from_sql;
        $overlap_to = ($row['to_date'] < $to_sql) ? $row['to_date'] : $to_sql;
        $overlap_days = _payroll_count_days_inclusive($overlap_from, $overlap_to);
        if ($overlap_days <= 0)
            continue;

        $recorded_days = isset($row['days']) && is_numeric($row['days']) ? max((float)$row['days'], 0.0) : 0.0;
        $effective_days = $recorded_days > 0
            ? min($recorded_days, ($recorded_days * $overlap_days / $range_days))
            : (float)$overlap_days;

        $pay_ratio = _payroll_get_leave_pay_ratio($row);
        $paid_days = $effective_days * $pay_ratio;
        $leave_type_id = isset($row['leave_type_id']) ? (int)$row['leave_type_id'] : 0;
        $leave_name = isset($row['leave_name']) ? trim((string)$row['leave_name']) : '';
        $leave_code = isset($row['leave_code']) ? trim((string)$row['leave_code']) : '';
        $leave_label = $leave_name !== '' ? $leave_name : ($leave_code !== '' ? $leave_code : _('Unspecified Leave'));
        $breakdown_key = $leave_type_id.'|'.$leave_label;

        if (!isset($summary['breakdown'][$breakdown_key])) {
            $summary['breakdown'][$breakdown_key] = array(
                'leave_type_id' => $leave_type_id,
                'leave_name' => $leave_name,
                'leave_code' => $leave_code,
                'leave_label' => $leave_label,
                'leave_days' => 0.0,
                'paid_days' => 0.0,
                'unpaid_days' => 0.0,
            );
        }

        $summary['leave_days'] += $effective_days;
        $summary['paid_leave_days'] += $paid_days;
        $summary['unpaid_leave_days'] += max($effective_days - $paid_days, 0.0);
        $summary['breakdown'][$breakdown_key]['leave_days'] += $effective_days;
        $summary['breakdown'][$breakdown_key]['paid_days'] += $paid_days;
        $summary['breakdown'][$breakdown_key]['unpaid_days'] += max($effective_days - $paid_days, 0.0);
    }

    $summary['leave_days'] = _payroll_round($summary['leave_days']);
    $summary['paid_leave_days'] = _payroll_round($summary['paid_leave_days']);
    $summary['unpaid_leave_days'] = _payroll_round($summary['unpaid_leave_days']);
    foreach ($summary['breakdown'] as $breakdown_key => $breakdown_row) {
        $summary['breakdown'][$breakdown_key]['leave_days'] = _payroll_round($breakdown_row['leave_days']);
        $summary['breakdown'][$breakdown_key]['paid_days'] = _payroll_round($breakdown_row['paid_days']);
        $summary['breakdown'][$breakdown_key]['unpaid_days'] = _payroll_round($breakdown_row['unpaid_days']);
    }
    $summary['breakdown'] = _payroll_sort_breakdown_rows($summary['breakdown'], 'leave_label');

    $leave_cache[$cache_key] = $summary;
    return $leave_cache[$cache_key];
}

/**
 * Get overtime hour totals grouped by overtime type for a period.
 *
 * @param string $employee_id
 * @param string $from_sql SQL date Y-m-d
 * @param string $to_sql SQL date Y-m-d
 * @return array
 */
function _payroll_get_overtime_breakdown($employee_id, $from_sql, $to_sql) {
    static $overtime_cache = array();

    $cache_key = $employee_id.'|'.$from_sql.'|'.$to_sql;
    if (isset($overtime_cache[$cache_key]))
        return $overtime_cache[$cache_key];

    $rows = array();
    $overtime_table = function_exists('overtime_type_table') ? overtime_type_table() : 'overtime';
    $has_overtime_master = function_exists('check_empty_result')
        ? check_empty_result("SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = ".db_escape(TB_PREF.$overtime_table))
        : false;
    $overtime_join = $has_overtime_master
        ? ' LEFT JOIN '.TB_PREF.$overtime_table.' ot ON ot.overtime_id = a.overtime_type_id'
        : '';
    $overtime_name_select = $has_overtime_master ? 'ot.overtime_name' : 'NULL';
    $overtime_rate_select = $has_overtime_master ? 'ot.pay_rate' : 'NULL';
    $sql = "SELECT a.overtime_type_id,
            SUM(IFNULL(a.overtime_hours, 0)) AS overtime_hours,
            MAX(IFNULL(a.rate, 0)) AS applied_rate,
            ".$overtime_name_select." AS overtime_name,
            ".$overtime_rate_select." AS pay_rate
        FROM ".TB_PREF."attendance a
        ".$overtime_join."
        WHERE a.employee_id = ".db_escape($employee_id)."
        AND a.date >= ".db_escape($from_sql)."
        AND a.date <= ".db_escape($to_sql)."
        AND IFNULL(a.overtime_hours, 0) > 0
        GROUP BY a.overtime_type_id, ".$overtime_name_select.", ".$overtime_rate_select."
        ORDER BY ".$overtime_name_select.", a.overtime_type_id";
    $result = db_query($sql, 'Could not get overtime summary');

    while ($row = db_fetch_assoc($result)) {
        $overtime_name = isset($row['overtime_name']) ? trim((string)$row['overtime_name']) : '';
        $rows[] = array(
            'overtime_type_id' => isset($row['overtime_type_id']) ? (int)$row['overtime_type_id'] : 0,
            'overtime_name' => $overtime_name !== '' ? $overtime_name : _('Unassigned Overtime'),
            'overtime_hours' => _payroll_round(isset($row['overtime_hours']) ? (float)$row['overtime_hours'] : 0.0),
            'pay_rate' => isset($row['pay_rate']) && is_numeric($row['pay_rate']) ? (float)$row['pay_rate'] : 0.0,
            'applied_rate' => isset($row['applied_rate']) && is_numeric($row['applied_rate']) ? (float)$row['applied_rate'] : 0.0,
        );
    }

    $overtime_cache[$cache_key] = _payroll_sort_breakdown_rows($rows, 'overtime_name');
    return $overtime_cache[$cache_key];
}

/**
 * Sort breakdown rows by one label field.
 *
 * @param array $rows
 * @param string $label_key
 * @return array
 */
function _payroll_sort_breakdown_rows($rows, $label_key) {
    $rows = array_values((array)$rows);
    usort($rows, function($left, $right) use ($label_key) {
        $left_label = isset($left[$label_key]) ? (string)$left[$label_key] : '';
        $right_label = isset($right[$label_key]) ? (string)$right[$label_key] : '';
        return strnatcasecmp($left_label, $right_label);
    });

    return $rows;
}

/**
 * Count inclusive calendar days between two SQL dates.
 *
 * @param string $from_sql SQL date Y-m-d
 * @param string $to_sql SQL date Y-m-d
 * @return int
 */
function _payroll_count_days_inclusive($from_sql, $to_sql) {
    if ($from_sql === '' || $to_sql === '')
        return 0;

    $from = new DateTime($from_sql);
    $to = new DateTime($to_sql);
    if ($from > $to)
        return 0;

    return (int)$from->diff($to)->days + 1;
}

/**
 * Resolve the payable portion of one leave row as a 0-1 ratio.
 *
 * @param array $leave_row
 * @return float
 */
function _payroll_get_leave_pay_ratio($leave_row) {
    $has_pay_rate = isset($leave_row['pay_rate']) && $leave_row['pay_rate'] !== null && $leave_row['pay_rate'] !== '' && is_numeric($leave_row['pay_rate']);
    $has_is_paid = isset($leave_row['is_paid']) && $leave_row['is_paid'] !== null && $leave_row['is_paid'] !== '';

    if ($has_is_paid && !(int)$leave_row['is_paid'] && (!$has_pay_rate || (float)$leave_row['pay_rate'] >= 100))
        $pay_rate = 0.0;
    elseif ($has_pay_rate)
        $pay_rate = (float)$leave_row['pay_rate'];
    elseif ($has_is_paid)
        $pay_rate = !empty($leave_row['is_paid']) ? 100.0 : 0.0;
    else
        $pay_rate = 100.0;

    if ($pay_rate < 0)
        $pay_rate = 0.0;
    if ($pay_rate > 100)
        $pay_rate = 100.0;

    return $pay_rate / 100;
}

/**
 * Get company working days count in a period.
 *
 * @param string $from_date User date format
 * @param string $to_date User date format
 * @return int
 */
function get_working_days_in_period($from_date, $to_date) {
    static $working_day_count_cache = array();

    $cache_key = $from_date.'|'.$to_date;
    if (isset($working_day_count_cache[$cache_key]))
        return $working_day_count_cache[$cache_key];

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

    $working_day_count_cache[$cache_key] = $days;
    return $working_day_count_cache[$cache_key];
}

/**
 * Calculate attendance-based deductions using configured rules.
 *
 * @param string $employee_id
 * @param string $from_date User date format
 * @param string $to_date User date format
 * @param float $daily_rate
 * @param array|null $summary Optional preloaded attendance summary.
 * @return float
 */
function calculate_attendance_deductions($employee_id, $from_date, $to_date, $daily_rate, $summary=null) {
	static $rules_cache = null;

    $daily_rate = (float)$daily_rate;
    if (floatcmp($daily_rate, 0) <= 0)
        return 0.0;

    if (!is_array($summary))
        $summary = get_attendance_summary($employee_id, $from_date, $to_date);
    $absence_days = (float)$summary['absent_days'];

    if ($rules_cache === null) {
        $sql = "SELECT * FROM ".TB_PREF."attendance_deduction_rules
            WHERE inactive = 0
            ORDER BY rule_type, from_value";
        $result = db_query($sql, 'Could not get attendance deduction rules');
        $rules_cache = array();
        while ($row = db_fetch_assoc($result))
            $rules_cache[] = $row;
    }

    $deduction_days = 0.0;
    foreach ($rules_cache as $row) {
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

    if (!empty($employee['position_id']) && class_exists('job_positions_entity', false)) {
        $position = job_positions_entity::find($employee['position_id']);
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
            $worked = isset($doc->payable_days) ? (float)$doc->payable_days : (float)$doc->worked_days;
            $worked = min(max($worked, 0.0), $working);
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
	$basic_category = defined('HRM_ELEM_BASIC') ? HRM_ELEM_BASIC : 1;
    $total = 0.0;
    foreach ((array)$doc->lines as $line) {
		if (empty($line->is_deduction) && (int)$line->element_category !== (int)$basic_category)
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
    static $working_day_cache = array();

    $dow = (int)$dow;
    if (isset($working_day_cache[$dow]))
        return $working_day_cache[$dow];

    $sql = "SELECT is_working FROM ".TB_PREF."working_days WHERE day_of_week = ".db_escape($dow);
    $result = db_query($sql, 'Could not get working day');
    $row = db_fetch_assoc($result);

    if (!$row) {
        $working_day_cache[$dow] = ($dow >= 1 && $dow <= 5);
        return $working_day_cache[$dow];
    }

    $working_day_cache[$dow] = !empty($row['is_working']);
    return $working_day_cache[$dow];
}

/**
 * Check if date is holiday.
 *
 * @param string $sql_date SQL date Y-m-d
 * @return bool
 */
function _payroll_is_holiday($sql_date) {
    static $holiday_cache = array();

    if (isset($holiday_cache[$sql_date]))
        return $holiday_cache[$sql_date];

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

    $holiday_cache[$sql_date] = db_num_rows($result) > 0;
    return $holiday_cache[$sql_date];
}

/**
 * Calculate extra absent days: non-working days that fall between consecutive
 * absent working days. For example if employee is absent on Thursday and
 * Monday, then Friday/Saturday/Sunday are counted as extra absent days.
 *
 * @param string $employee_id
 * @param string $from_sql SQL date Y-m-d
 * @param string $to_sql SQL date Y-m-d
 * @return float
 */
function _calculate_extra_absent_days($employee_id, $from_sql, $to_sql) {
    // Get all absent dates for this employee in the period
    $sql = "SELECT date FROM ".TB_PREF."attendance
        WHERE employee_id = ".db_escape($employee_id)."
        AND date >= ".db_escape($from_sql)."
        AND date <= ".db_escape($to_sql)."
        AND status = 1
        ORDER BY date";
    $result = db_query($sql, 'Could not get absent dates');

    $absent_dates = array();
    while ($row = db_fetch_assoc($result))
        $absent_dates[] = $row['date'];

    if (count($absent_dates) < 2)
        return 0.0;

    $extra = 0.0;
    for ($i = 0; $i < count($absent_dates) - 1; $i++) {
        $current = new DateTime($absent_dates[$i]);
        $next = new DateTime($absent_dates[$i + 1]);
        $current->modify('+1 day');

        // Count non-working days between two consecutive absent dates
        $gap_non_working = 0;
        $all_non_working = true;
        for ($d = clone $current; $d < $next; $d->modify('+1 day')) {
            $dow = (int)$d->format('w');
            if (_payroll_is_working_day($dow) && !_payroll_is_holiday($d->format('Y-m-d'))) {
                $all_non_working = false;
                break;
            }
            $gap_non_working++;
        }

        // Only count if the gap contains only non-working days
        if ($all_non_working && $gap_non_working > 0)
            $extra += $gap_non_working;
    }

    return $extra;
}

/**
 * Get SQL year start/end bounds.
 *
 * @param int $year
 * @return array
 */
function _payroll_get_year_bounds($year) {
    $year = (int)$year;
    if (function_exists('get_leave_balance_fiscal_year_bounds'))
        return get_leave_balance_fiscal_year_bounds($year);

    return array($year.'-01-01', $year.'-12-31');
}

/**
 * Validate a GL account before payroll posting.
 *
 * @param string $account_code
 * @return bool
 */
function _payroll_is_valid_gl_account($account_code) {
    $account_code = trim((string)$account_code);
    if ($account_code === '')
        return false;

    if (function_exists('get_gl_account')) {
        $account = chart_master_entity::find($account_code);
        return is_array($account) && !empty($account);
    }

    return true;
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

/**
 * Resolve a GL account for a synthetic pay element.
 *
 * Synthetic pay elements (Attendance Deduction, Income Tax) are generated
 * programmatically and don't have a fixed pay_element_id. This function
 * resolves their GL account using a priority chain:
 *
 *   1. Company preference (payroll_deduct_act for deductions)
 *   2. First deduction element account from salary components
 *   3. Payroll payable account (last resort for balancing)
 *
 * @param string $type              Synthetic element type ('attendance_deduction', 'income_tax')
 * @param array  $salary_components Employee salary component rows
 * @return string A non-empty GL account code
 */
function _payroll_resolve_synthetic_account($type, $salary_components) {
    // Priority 1: Company preference
    $pref_key = ($type === 'income_tax')
        ? 'payroll_tax_act'
        : 'payroll_deduct_act';

    $pref_account = get_company_pref($pref_key);
    if (!empty($pref_account) && _payroll_is_valid_gl_account($pref_account)) {
        return $pref_account;
    }

    // Priority 2: First deduction element account from salary components
    if (is_array($salary_components)) {
        foreach ($salary_components as $component) {
            $cat = isset($component['element_category']) ? (int)$component['element_category'] : -1;
            $is_deduction = !empty($component['is_deduction']);
            if ($is_deduction && !empty($component['account_code'])
                && _payroll_is_valid_gl_account($component['account_code'])) {
                return $component['account_code'];
            }
            // Also match by category: HRM_ELEM_DEDUCTION or HRM_ELEM_STATUTORY_DEDUCTION
            if (($cat === HRM_ELEM_DEDUCTION || $cat === HRM_ELEM_STATUTORY_DEDUCTION)
                && !empty($component['account_code'])
                && _payroll_is_valid_gl_account($component['account_code'])) {
                return $component['account_code'];
            }
        }
    }

    // Priority 3: Fallback to payroll payable account
    $payable = get_company_pref('payroll_payable_act');
    if (!empty($payable) && _payroll_is_valid_gl_account($payable)) {
        return $payable;
    }

    // Absolute last resort: return empty string - will be caught by GL validation
    return '';
}
