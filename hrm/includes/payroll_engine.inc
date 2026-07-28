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
include_once($path_to_root.'/hrm/includes/db/payroll_db.inc');
include_once($path_to_root.'/hrm/includes/db/leave_balance_db.inc');
include_once($path_to_root.'/hrm/includes/db/leave_type_db.inc');
include_once($path_to_root.'/hrm/includes/db/overtime_type_db.inc');
include_once($path_to_root.'/hrm/includes/db/loan_db.inc');
include_once($path_to_root.'/hrm/includes/db/hr_settings_db.inc');

if (!defined('HRM_LEGACY_PAYROLL_CONTRACT_ID'))
    define('HRM_LEGACY_PAYROLL_CONTRACT_ID', 'notrinoserp.hrm.payroll.legacy');
if (!defined('HRM_LEGACY_PAYROLL_CONTRACT_VERSION'))
    define('HRM_LEGACY_PAYROLL_CONTRACT_VERSION', '1.0.5');

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
 * Return the active legacy payroll compatibility contract metadata.
 *
 * This identifier is intentionally available without database access so
 * calculation, posting, and regression evidence can name the exact legacy
 * behavior being exercised.
 *
 * @return array
 */
function hrm_legacy_payroll_contract_metadata() {
    return array(
        'contract_id' => HRM_LEGACY_PAYROLL_CONTRACT_ID,
        'version' => HRM_LEGACY_PAYROLL_CONTRACT_VERSION,
        'entrypoint' => 'calculate_employee_payslip',
        'implementation' => 'calculate_employee_payslip_legacy_v1'
    );
}

/**
 * Calculate an employee payslip through the frozen legacy v1 contract.
 *
 * @param array  $employee Employee row (or minimum with employee_id)
 * @param string $from_date User date format
 * @param string $to_date User date format
 * @param int    $period_id Payroll period id
 * @param array  $context Optional preloaded runtime context.
 * @return payslip_doc|false
 */
function calculate_employee_payslip($employee, $from_date, $to_date, $period_id=0, $context=array()) {
    return calculate_employee_payslip_legacy_v1($employee, $from_date, $to_date, $period_id, $context);
}


/**
 * Check whether the legacy calculator is inside its rollback-only boundary.
 *
 * The flag is intentionally process-local. It prevents the known posting
 * adapter from being invoked by a calculation hook while the calculation
 * transaction is active. Database writes attempted by legacy hooks are rolled
 * back when the boundary closes.
 *
 * @return bool
 */
function payroll_calculation_read_only_active() {
    return !empty($GLOBALS['hrm_payroll_calculation_read_only_depth']);
}

/**
 * Calculate one payslip inside a rollback-only database boundary.
 *
 * This is the production entrypoint for manual and batch payroll preparation.
 * It preserves the frozen calculator result and hook order, but always rolls
 * back the database transaction before returning the in-memory payslip result.
 * The caller may subsequently invoke the separately controlled legacy posting
 * adapter. An existing outer transaction is rejected because rollback would
 * otherwise cancel unrelated work.
 *
 * @param array  $employee Employee row (or minimum with employee_id)
 * @param string $from_date User date format
 * @param string $to_date User date format
 * @param int    $period_id Payroll period id
 * @param array  $context Optional preloaded runtime context
 * @return payslip_doc|false
 */
function calculate_employee_payslip_read_only($employee, $from_date, $to_date, $period_id=0, $context=array()) {
    global $transaction_level;

    if (!empty($transaction_level)) {
        if (function_exists('display_error'))
            display_error(_('Payroll calculation cannot run inside another database transaction.'));
        return false;
    }

    if (!isset($GLOBALS['hrm_payroll_calculation_read_only_depth']))
        $GLOBALS['hrm_payroll_calculation_read_only_depth'] = 0;

    $GLOBALS['hrm_payroll_calculation_read_only_depth']++;
    begin_transaction();

    try {
        $result = calculate_employee_payslip(
            $employee,
            $from_date,
            $to_date,
            $period_id,
            $context
        );
    } finally {
        cancel_transaction();
        $GLOBALS['hrm_payroll_calculation_read_only_depth']--;
    }

    return $result;
}

/**
 * Implement the frozen legacy v1 employee payslip calculation behavior.
 *
 * Keep the public `calculate_employee_payslip()` entrypoint for all existing
 * callers. Changes to this implementation require a new contract version and
 * updated golden evidence.
 *
 * @param array  $employee Employee row (or minimum with employee_id)
 * @param string $from_date User date format
 * @param string $to_date User date format
 * @param int    $period_id Payroll period id
 * @param array  $context Optional preloaded runtime context.
 * @return payslip_doc|false
 */
function calculate_employee_payslip_legacy_v1($employee, $from_date, $to_date, $period_id=0, $context=array()) {
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
 * Build the validated legacy GL posting plan without writing business rows.
 *
 * @param payslip_doc $payslip_doc
 * @return array
 */
function payroll_build_legacy_posting_plan(&$payslip_doc) {
    $plan = array(
        'ok' => false,
        'reason' => 'invalid_payslip',
        'message' => _('Cannot post an invalid payslip result.'),
        'gl_totals' => array(),
        'total' => 0.0,
        'payable_account' => '',
    );

    if (!is_object($payslip_doc) || empty($payslip_doc->employee_id))
        return $plan;

    if (!isset($payslip_doc->lines) || !is_array($payslip_doc->lines)) {
        $plan['reason'] = 'invalid_payslip_lines';
        $plan['message'] = _('Cannot post payslip. The calculated pay element result is invalid.');
        return $plan;
    }

    $total = 0.0;
    $gl_totals = array();
    $invalid_accounts = array();
    foreach ($payslip_doc->lines as $line) {
        if (!is_object($line)
            || !isset($line->final_amount)
            || !is_numeric($line->final_amount)
            || !property_exists($line, 'account_code')
            || (!is_string($line->account_code) && !is_numeric($line->account_code))
            || !isset($line->element_name)
            || (!is_string($line->element_name) && !is_numeric($line->element_name))
            || !property_exists($line, 'is_deduction')
        ) {
            $plan['reason'] = 'invalid_payslip_lines';
            $plan['message'] = _('Cannot post payslip. The calculated pay element result is invalid.');
            return $plan;
        }

        if (floatcmp($line->final_amount, 0) == 0)
            continue;

        if (empty($line->account_code)) {
            $invalid_accounts[] = $line->element_name;
            continue;
        }

        if (!_payroll_is_valid_gl_account($line->account_code)) {
            $invalid_accounts[] = $line->element_name.' ('.$line->account_code.')';
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

    if (!empty($invalid_accounts)) {
        $plan['reason'] = 'invalid_element_accounts';
        $plan['message'] = sprintf(
            _('Cannot post payslip. The following pay elements have missing or invalid GL accounts: %s. Please correct them in Pay Elements setup.'),
            implode(', ', $invalid_accounts)
        );
        return $plan;
    }

    if (empty($gl_totals) || floatcmp($total, 0) == 0) {
        $plan['reason'] = 'empty_gl_plan';
        $plan['message'] = _('Cannot post payslip. No valid GL account lines were found.');
        return $plan;
    }

    $payable_account = get_company_pref('payroll_payable_act');
    if (empty($payable_account)) {
        $plan['reason'] = 'missing_payable_account';
        $plan['message'] = _('Payroll Payable GL account is not configured. Please set it in System Preferences.');
        return $plan;
    }

    if (!_payroll_is_valid_gl_account($payable_account)) {
        $plan['reason'] = 'invalid_payable_account';
        $plan['message'] = sprintf(
            _('Payroll Payable GL account (%s) is invalid. Please correct it in System Preferences.'),
            $payable_account
        );
        return $plan;
    }

    $plan['ok'] = true;
    $plan['reason'] = '';
    $plan['message'] = '';
    $plan['gl_totals'] = $gl_totals;
    $plan['total'] = $total;
    $plan['payable_account'] = $payable_account;
    return $plan;
}

/**
 * Persist the legacy payslip result and allocate its posting identity.
 *
 * The caller must already own the surrounding transaction. This stage writes
 * only the payslip header/details and generated identity. The transaction
 * reference is reserved in the same transaction so concurrent preparation
 * cannot reuse approval-bound evidence; no GL, comment, core audit-trail,
 * loan, or leave row is written.
 *
 * @param payslip_doc $payslip_doc
 * @return array|false
 */
function payroll_persist_legacy_payslip_result(&$payslip_doc) {
    global $Refs, $transaction_level;

    if (empty($transaction_level)
        || !is_object($payslip_doc)
        || empty($payslip_doc->employee_id)
    ) {
        return false;
    }

    $allocation_lock_sql = "SELECT name FROM ".TB_PREF."sys_prefs"
        ." WHERE name = 'coy_name' FOR UPDATE";
    if (!db_query($allocation_lock_sql, 'could not lock payslip transaction allocation'))
        return false;

    $employee_lock_sql = "SELECT employee_id FROM ".TB_PREF."employees"
        ." WHERE employee_id = ".db_escape($payslip_doc->employee_id)." FOR UPDATE";
    if (!db_query($employee_lock_sql, 'could not lock employee for payslip posting'))
        return false;

    if (payslip_exists_for_period(
        $payslip_doc->employee_id,
        $payslip_doc->from_date,
        $payslip_doc->to_date
    )) {
        if (function_exists('display_error'))
            display_error(_('A non-voided payslip already overlaps the selected employee and period.'));
        return false;
    }

    $trans_no = payroll_get_next_payslip_trans_no();
    $tran_date = empty($payslip_doc->tran_date) ? Today() : $payslip_doc->tran_date;
    $reference = empty($payslip_doc->reference)
        ? _payroll_get_reference(ST_PAYSLIP, $tran_date)
        : $payslip_doc->reference;

    $payslip_doc->reference = $reference;
    $payslip_doc->gl_trans_no = $trans_no;
    if (!function_exists('add_payslip'))
        return false;

    $payslip_id = add_payslip($payslip_doc);
    if (!$payslip_id)
        return false;

    $payslip_doc->payslip_id = $payslip_id;
    if ($reference === ''
        || !isset($Refs)
        || !is_object($Refs)
        || !method_exists($Refs, 'save')
    ) {
        return false;
    }
    $Refs->save(ST_PAYSLIP, $trans_no, $reference);

    return array(
        'payslip_id' => (int)$payslip_id,
        'trans_no' => (int)$trans_no,
        'tran_date' => $tran_date,
        'reference' => $reference,
    );
}

/**
 * Apply the legacy GL/comment/audit-trail posting stage.
 *
 * The caller must already own the surrounding transaction and must have
 * persisted the result through payroll_persist_legacy_payslip_result().
 *
 * @param payslip_doc $payslip_doc
 * @param array $plan Validated posting plan
 * @param array $result_context Persisted result identity
 * @return bool
 */
function payroll_apply_legacy_gl_side_effects(&$payslip_doc, $plan, $result_context) {
    global $transaction_level;

    if (empty($transaction_level)
        || empty($plan['ok'])
        || empty($result_context['trans_no'])
    ) {
        return false;
    }

    $trans_no = (int)$result_context['trans_no'];
    $cart = new hrm_cart(ST_PAYSLIP, $trans_no);
    $cart->trans_no = $trans_no;
    $cart->order_id = $trans_no;
    $cart->tran_date = $result_context['tran_date'];
    $cart->reference = $result_context['reference'];
    $cart->set_employee(
        $payslip_doc->employee_id,
        _payroll_get_employee_name($payslip_doc->employee)
    );

    foreach ($plan['gl_totals'] as $account_code => $amount) {
        if (floatcmp($amount, 0) == 0)
            continue;
        $cart->add_gl_item(
            $account_code,
            0,
            0,
            $amount,
            _('Payroll Element'),
            null,
            null,
            $cart->tran_date
        );
    }

    if (floatcmp($plan['total'], 0) != 0) {
        $cart->add_gl_item(
            $plan['payable_account'],
            0,
            0,
            -$plan['total'],
            _('Payroll Payable'),
            null,
            null,
            $cart->tran_date
        );
    }

    $cart->write_gl(true);

    $comment = trim(isset($payslip_doc->comments) ? $payslip_doc->comments : '');
    if ($comment === '')
        $comment = _('Payroll generated');

    if (function_exists('delete_comments'))
        delete_comments(ST_PAYSLIP, $trans_no);
    add_comments(ST_PAYSLIP, $trans_no, $cart->tran_date, $comment);
    add_audit_trail(ST_PAYSLIP, $trans_no, $cart->tran_date);
    return true;
}

/**
 * Apply the legacy loan and leave balance side-effect stage.
 *
 * The caller must already own the surrounding transaction. The functions are
 * retained exactly as legacy compatibility effects. Batch payroll invokes
 * them only from the verified post-after-approval command; manual payslips
 * keep the established atomic adapter.
 *
 * @param payslip_doc $payslip_doc
 * @return bool
 */
function payroll_apply_legacy_balance_side_effects(&$payslip_doc) {
    global $transaction_level;

    if (empty($transaction_level)
        || !is_object($payslip_doc)
        || empty($payslip_doc->employee_id)
        || empty($payslip_doc->payslip_id)
    ) {
        return false;
    }

    if (function_exists('mark_loan_repayments_paid_by_payslip')
        && !empty($payslip_doc->payslip_id)
    ) {
        mark_loan_repayments_paid_by_payslip(
            $payslip_doc->employee_id,
            $payslip_doc->from_date,
            $payslip_doc->to_date,
            $payslip_doc->payslip_id
        );
    }

    payroll_sync_leave_balances_for_period(
        $payslip_doc->employee_id,
        $payslip_doc->from_date,
        $payslip_doc->to_date
    );
    return true;
}

/**
 * Persist a complete payroll-period result set for approval without effects.
 *
 * Every calculated document is validated before the transaction starts. The
 * transaction then locks the Draft period, persists every payslip/detail and
 * reserves its reference identity, reconciles totals, promotes the period to
 * Calculated, and appends minimized PAY-AUD evidence. No GL, comment, core
 * audit-trail, loan, or leave mutation is permitted in this command.
 *
 * @param int $period_id
 * @param array $payslip_documents
 * @return array
 */
function prepare_payroll_period_results_for_approval($period_id, $payslip_documents) {
    global $path_to_root;

    $result = array(
        'ok' => false,
        'code' => 'invalid_preparation_request',
        'message' => _('Payroll results could not be prepared for approval.'),
        'payslip_count' => 0,
    );
    $period_id = (int)$period_id;
    if ($period_id <= 0 || !is_array($payslip_documents) || empty($payslip_documents))
        return $result;
    if (function_exists('user_check_access') && !user_check_access('SA_PAYROLL')) {
        $result['code'] = 'unauthorized';
        $result['message'] = _('You do not have permission to prepare payroll results.');
        return $result;
    }
    if (payroll_calculation_read_only_active()) {
        $result['code'] = 'read_only_boundary_active';
        return $result;
    }

    // Validate the complete result set before any persistence begins.
    $expected_detail_count = 0;
    foreach ($payslip_documents as $payslip_doc) {
        if (!is_object($payslip_doc)
            || (int)$payslip_doc->payroll_period_id !== $period_id
            || empty($payslip_doc->employee_id)
        ) {
            $result['code'] = 'invalid_payslip_result';
            return $result;
        }
        $plan = payroll_build_legacy_posting_plan($payslip_doc);
        if (empty($plan['ok'])) {
            $result['code'] = isset($plan['reason']) ? $plan['reason'] : 'invalid_posting_plan';
            $result['message'] = isset($plan['message']) ? $plan['message'] : $result['message'];
            return $result;
        }
        $expected_detail_count += count($payslip_doc->lines);
    }

    include_once($path_to_root.'/includes/db/domain_audit_db.inc');
    begin_transaction();
    try {
        $period_before = get_payroll_period_for_update($period_id);
        if (!$period_before || (int)$period_before['status'] !== 0) {
            cancel_transaction();
            $result['code'] = 'period_not_draft';
            $result['message'] = _('Only a Draft payroll period can receive prepared results.');
            return $result;
        }

        $actor_id = function_exists('payroll_side_effect_control_actor_id')
            ? payroll_side_effect_control_actor_id() : 0;
        if ($actor_id <= 0
            || !isset($period_before['created_by'])
            || (int)$period_before['created_by'] !== $actor_id
        ) {
            cancel_transaction();
            $result['code'] = 'period_owner_mismatch';
            $result['message'] = _('The Draft payroll period is not owned by the current operator.');
            return $result;
        }

        $existing = payroll_approval_binding_fetch_rows(
            "SELECT payslip_id FROM ".TB_PREF."payslips"
                ." WHERE payroll_period_id = ".db_escape($period_id)
                ." ORDER BY payslip_id FOR UPDATE",
            'could not lock existing payroll-period results'
        );
        if ($existing === false) {
            cancel_transaction();
            $result['code'] = 'existing_result_check_failed';
            return $result;
        }
        if (!empty($existing)) {
            cancel_transaction();
            $result['code'] = 'period_results_already_exist';
            $result['message'] = _('The payroll period already contains persisted results.');
            return $result;
        }

        $persisted = array();
        foreach ($payslip_documents as $payslip_doc) {
            $identity = payroll_persist_legacy_payslip_result($payslip_doc);
            if ($identity === false) {
                cancel_transaction();
                $result['code'] = 'payslip_persistence_failed';
                return $result;
            }
            $persisted[] = $identity;
        }

        if (!update_payroll_period_totals($period_id)
            || !update_payroll_period_status($period_id, 1, 0)
        ) {
            cancel_transaction();
            $result['code'] = 'period_reconciliation_failed';
            return $result;
        }

        $period_after = get_payroll_period_for_update($period_id);
        if (!$period_after
            || (int)$period_after['status'] !== 1
            || count($persisted) !== count($payslip_documents)
        ) {
            cancel_transaction();
            $result['code'] = 'prepared_state_verification_failed';
            return $result;
        }

        $prepared_binding = build_payroll_period_approval_binding(
            $period_after,
            true,
            '1.1.0'
        );
        if (empty($prepared_binding['ok'])
            || (int)$prepared_binding['payslip_count'] !== count($persisted)
            || (int)$prepared_binding['detail_count'] !== $expected_detail_count
        ) {
            cancel_transaction();
            $result['code'] = empty($prepared_binding['code'])
                ? 'prepared_result_set_mismatch' : $prepared_binding['code'];
            $result['message'] = _(
                'The persisted payroll result set could not be verified. '
                .'No partial payroll result was committed.'
            );
            return $result;
        }

        $audit_result = function_exists('append_domain_audit_event')
            ? append_domain_audit_event(array(
                'legal_entity_id' => 0,
                'entity_type' => 'payroll_period',
                'entity_key' => array('period_id' => $period_id),
                'action' => 'payroll_period.calculated',
                'reason_code' => 'prepared_result_persistence',
                'before' => array(
                    'period_id' => $period_id,
                    'status' => isset($period_before['status']) ? (int)$period_before['status'] : null,
                ),
                'after' => array(
                    'period_id' => $period_id,
                    'status' => (int)$period_after['status'],
                ),
                'restricted_diff' => array(
                    'changed_fields' => array(
                        'status', 'total_gross', 'total_deductions',
                        'total_net', 'total_employer_cost', 'payslip_results',
                    ),
                    'mutation_kind' => 'calculated',
                    'payslip_count' => count($persisted),
                    'financial_side_effects_created' => false,
                    'restricted_values_stored' => false,
                ),
            )) : false;
        if ($audit_result === false) {
            cancel_transaction();
            $result['code'] = 'audit_append_failed';
            $result['message'] = _('Payroll preparation audit could not be recorded. No results were committed.');
            return $result;
        }

        commit_transaction();
        return array(
            'ok' => true,
            'code' => 'prepared',
            'message' => _('Payroll results were prepared for approval.'),
            'payslip_count' => count($persisted),
        );
    } catch (Exception $e) {
        cancel_transaction();
        $result['code'] = 'preparation_exception';
        return $result;
    }
}

/**
 * Rehydrate one immutable prepared payslip result for legacy effect posting.
 *
 * @param array $row Locked payslip header row
 * @return payslip_doc|false
 */
function payroll_rehydrate_prepared_payslip($row) {
    if (!is_array($row)
        || empty($row['payslip_id'])
        || empty($row['employee_id'])
        || empty($row['trans_no'])
        || (int)$row['status'] !== 0
    ) {
        return false;
    }

    $doc = new payslip_doc();
    $doc->payslip_id = (int)$row['payslip_id'];
    $doc->payroll_period_id = isset($row['payroll_period_id']) ? (int)$row['payroll_period_id'] : 0;
    $doc->employee_id = $row['employee_id'];
    $doc->employee = _payroll_get_employee_full_row($row['employee_id'], array(
        'employee_id' => $row['employee_id'],
        'department_id' => isset($row['department_id']) ? $row['department_id'] : null,
        'position_id' => isset($row['position_id']) ? $row['position_id'] : null,
        'grade_id' => isset($row['grade_id']) ? $row['grade_id'] : null,
    ));
    $doc->from_date = isset($row['from_date']) ? $row['from_date'] : '';
    $doc->to_date = isset($row['to_date']) ? $row['to_date'] : '';
    $doc->tran_date = isset($row['tran_date']) ? $row['tran_date'] : '';
    $doc->reference = isset($row['reference']) ? $row['reference'] : '';
    $doc->gl_trans_no = (int)$row['trans_no'];
    $doc->comments = isset($row['notes']) ? $row['notes'] : '';

    foreach (array(
        'basic_salary', 'total_earnings', 'gross_salary', 'total_deductions',
        'net_salary', 'employer_contributions', 'total_cost', 'working_days',
        'worked_days', 'overtime_hours', 'leave_days', 'absent_days',
        'loan_deduction', 'exchange_rate', 'net_salary_home',
    ) as $field) {
        if (array_key_exists($field, $row))
            $doc->$field = (float)$row[$field];
    }
    if (isset($row['currency_code']))
        $doc->currency_code = $row['currency_code'];

    $details = get_payslip_details($doc->payslip_id);
    if (!$details)
        return false;
    while ($line = db_fetch_assoc($details)) {
        $doc->add_line(
            isset($line['element_id']) ? (int)$line['element_id'] : 0,
            isset($line['element_name']) ? $line['element_name'] : '',
            isset($line['element_category']) ? (int)$line['element_category'] : 0,
            !empty($line['is_deduction']),
            isset($line['amount_type']) ? (int)$line['amount_type'] : 0,
            isset($line['base_amount']) ? (float)$line['base_amount'] : 0,
            isset($line['rate']) ? (float)$line['rate'] : 0,
            isset($line['calculated_amount']) ? (float)$line['calculated_amount'] : 0,
            isset($line['final_amount']) ? (float)$line['final_amount'] : 0,
            isset($line['account_code']) ? $line['account_code'] : '',
            isset($line['formula_used']) ? $line['formula_used'] : null,
            !isset($line['is_taxable']) || !empty($line['is_taxable']),
            isset($line['display_order']) ? (int)$line['display_order'] : 0
        );
    }

    return !empty($doc->lines) && trim((string)$doc->reference) !== '' ? $doc : false;
}

/**
 * Verify durable evidence for a Posted payroll period without writing rows.
 *
 * @param array $period
 * @param array $draft_data Approved binding metadata
 * @param bool $lock
 * @return array
 */
function verify_posted_payroll_period_evidence($period, $draft_data, $lock=true) {
    $failed = array('ok' => false, 'code' => 'invalid_posted_period');
    if (!is_array($period)
        || empty($period['period_id'])
        || (int)$period['status'] !== 3
        || !is_array($draft_data)
        || payroll_period_approval_binding_draft_state($draft_data) !== 'well_formed'
        || !isset($draft_data['period_id'])
        || (int)$draft_data['period_id'] !== (int)$period['period_id']
    ) {
        return $failed;
    }

    $period_id = (int)$period['period_id'];
    $suffix = $lock ? ' FOR UPDATE' : '';
    $rows = payroll_approval_binding_fetch_rows(
        "SELECT payslip_id,payroll_period_id,trans_no,reference,status,payment_trans_no"
            ." FROM ".TB_PREF."payslips WHERE payroll_period_id = ".db_escape($period_id)
            ." ORDER BY payslip_id".$suffix,
        'could not verify posted payroll-period payslips'
    );
    if ($rows === false
        || count($rows) !== (int)$draft_data['approval_payslip_count']
        || empty($rows)
    ) {
        $failed['code'] = 'posted_payslip_count_mismatch';
        return $failed;
    }

    $trans_nos = array();
    $references = array();
    foreach ($rows as $row) {
        $trans_no = isset($row['trans_no']) ? (int)$row['trans_no'] : 0;
        if ((int)$row['payroll_period_id'] !== $period_id
            || (int)$row['status'] !== 2
            || !empty($row['payment_trans_no'])
            || $trans_no <= 0
            || trim((string)$row['reference']) === ''
            || isset($references[$trans_no])
        ) {
            $failed['code'] = 'invalid_posted_payslip_state';
            return $failed;
        }
        $trans_nos[] = $trans_no;
        $references[$trans_no] = (string)$row['reference'];
    }

    $trans_sql = implode(',', array_map('intval', $trans_nos));
    $ref_rows = payroll_approval_binding_fetch_rows(
        "SELECT type,id,reference FROM ".TB_PREF."refs"
            ." WHERE type = ".db_escape(ST_PAYSLIP)
            ." AND id IN (".$trans_sql.") ORDER BY id".$suffix,
        'could not verify posted payroll-period references'
    );
    if ($ref_rows === false || count($ref_rows) !== count($trans_nos)) {
        $failed['code'] = 'posted_reference_count_mismatch';
        return $failed;
    }
    $seen_refs = array();
    foreach ($ref_rows as $row) {
        $id = isset($row['id']) ? (int)$row['id'] : 0;
        if (!isset($references[$id])
            || isset($seen_refs[$id])
            || !hash_equals($references[$id], (string)$row['reference'])
        ) {
            $failed['code'] = 'posted_reference_drift';
            return $failed;
        }
        $seen_refs[$id] = true;
    }

    $void_rows = payroll_approval_binding_fetch_rows(
        "SELECT type,id FROM ".TB_PREF."voided"
            ." WHERE type = ".db_escape(ST_PAYSLIP)
            ." AND id IN (".$trans_sql.") ORDER BY id".$suffix,
        'could not verify posted payroll-period void evidence'
    );
    if ($void_rows === false || !empty($void_rows)) {
        $failed['code'] = 'posted_result_voided';
        return $failed;
    }

    $gl_rows = payroll_approval_binding_fetch_rows(
        "SELECT type_no,amount FROM ".TB_PREF."gl_trans"
            ." WHERE type = ".db_escape(ST_PAYSLIP)
            ." AND type_no IN (".$trans_sql.") ORDER BY type_no,counter".$suffix,
        'could not verify posted payroll-period GL evidence'
    );
    if ($gl_rows === false) {
        $failed['code'] = 'posted_gl_read_failed';
        return $failed;
    }
    $counts = array();
    $balances = array();
    foreach ($gl_rows as $row) {
        $trans_no = isset($row['type_no']) ? (int)$row['type_no'] : 0;
        if (!isset($references[$trans_no]) || !isset($row['amount']) || !is_numeric($row['amount'])) {
            $failed['code'] = 'invalid_posted_gl_state';
            return $failed;
        }
        if (abs((float)$row['amount']) <= 0.000001)
            continue;
        if (!isset($counts[$trans_no])) {
            $counts[$trans_no] = 0;
            $balances[$trans_no] = 0.0;
        }
        $counts[$trans_no]++;
        $balances[$trans_no] += (float)$row['amount'];
    }
    foreach ($trans_nos as $trans_no) {
        if (empty($counts[$trans_no])
            || $counts[$trans_no] < 2
            || abs($balances[$trans_no]) > 0.000001
        ) {
            $failed['code'] = 'unreconciled_posted_gl';
            return $failed;
        }
    }

    return array(
        'ok' => true,
        'code' => 'posted_evidence_verified',
        'payslip_count' => count($rows),
        'reference_count' => count($ref_rows),
        'gl_line_count' => array_sum($counts),
    );
}

/**
 * Post one approved payroll period exactly once.
 *
 * Contract 1.1.0 consumes prepared, side-effect-free results. Historical
 * contract 1.0.0 is compatibility-only: its already committed GL/balance
 * effects are never duplicated. A successful retry verifies durable evidence
 * and performs no write or audit append.
 *
 * @param int $period_id
 * @return array
 */
function post_approved_payroll_period($period_id) {
    global $path_to_root;

    $result = array(
        'ok' => false,
        'code' => 'invalid_post_request',
        'message' => _('The approved payroll period could not be posted.'),
    );
    $period_id = (int)$period_id;
    if ($period_id <= 0)
        return $result;
    if (function_exists('user_check_access') && !user_check_access('SA_PAYROLLPOST')) {
        $result['code'] = 'unauthorized';
        $result['message'] = _('You do not have permission to post approved payroll.');
        return $result;
    }
    if (payroll_calculation_read_only_active()) {
        $result['code'] = 'read_only_boundary_active';
        return $result;
    }
    if ((function_exists('hrm_has_hook') && hrm_has_hook('before_post_payslip'))
        || (function_exists('hrm_has_hook') && hrm_has_hook('after_post_payslip'))
    ) {
        $result['code'] = 'unreviewed_posting_hook';
        $result['message'] = _('Payroll posting is blocked until registered posting extensions are recertified.');
        return $result;
    }

    include_once($path_to_root.'/includes/db/domain_audit_db.inc');
    begin_transaction();
    try {
        $period = get_payroll_period_for_update($period_id);
        if (!$period) {
            cancel_transaction();
            $result['code'] = 'period_not_found';
            return $result;
        }

        $approved_draft = get_payroll_period_approval_draft($period_id, 1, true);
        if (!$approved_draft || empty($approved_draft['decoded_data'])) {
            cancel_transaction();
            $result['code'] = 'approved_binding_missing';
            return $result;
        }
        $draft_data = $approved_draft['decoded_data'];

        // Idempotent replay: verification only, no mutation and no new audit.
        if ((int)$period['status'] === 3) {
            $verification = verify_posted_payroll_period_evidence($period, $draft_data, true);
            if (empty($verification['ok'])) {
                cancel_transaction();
                $result['code'] = $verification['code'];
                return $result;
            }
            commit_transaction();
            return array(
                'ok' => true,
                'code' => 'already_posted_verified',
                'message' => _('The payroll period was already posted and its evidence was verified.'),
                'payslip_count' => $verification['payslip_count'],
            );
        }

        if ((int)$period['status'] !== 2) {
            cancel_transaction();
            $result['code'] = 'period_not_approved';
            $result['message'] = _('Only an Approved payroll period can be posted.');
            return $result;
        }

        $binding_version = isset($draft_data['approval_binding_version'])
            ? (string)$draft_data['approval_binding_version'] : '';
        if (!payroll_period_approval_binding_version_supported($binding_version)) {
            cancel_transaction();
            $result['code'] = 'unsupported_binding_version';
            return $result;
        }
        $binding_period = $period;
        $binding_period['status'] = 1;
        $binding = build_payroll_period_approval_binding(
            $binding_period,
            true,
            $binding_version
        );
        if (!payroll_period_approval_binding_matches_draft($draft_data, $binding)) {
            cancel_transaction();
            $result['code'] = isset($binding['code']) ? $binding['code'] : 'approval_binding_drift';
            $result['message'] = _('The approved payroll evidence has changed. Nothing was posted.');
            return $result;
        }

        $payslip_rows = payroll_approval_binding_fetch_rows(
            "SELECT * FROM ".TB_PREF."payslips"
                ." WHERE payroll_period_id = ".db_escape($period_id)
                ." ORDER BY payslip_id FOR UPDATE",
            'could not lock approved payroll results for posting'
        );
        if ($payslip_rows === false
            || count($payslip_rows) !== (int)$draft_data['approval_payslip_count']
            || empty($payslip_rows)
        ) {
            cancel_transaction();
            $result['code'] = 'approved_result_count_mismatch';
            return $result;
        }

        foreach ($payslip_rows as $row) {
            if ((int)$row['status'] !== 0) {
                cancel_transaction();
                $result['code'] = 'prepared_payslip_status_drift';
                return $result;
            }

            if ($binding_version === '1.1.0') {
                $payslip_doc = payroll_rehydrate_prepared_payslip($row);
                if ($payslip_doc === false) {
                    cancel_transaction();
                    $result['code'] = 'prepared_payslip_rehydration_failed';
                    return $result;
                }
                $plan = payroll_build_legacy_posting_plan($payslip_doc);
                if (empty($plan['ok'])) {
                    cancel_transaction();
                    $result['code'] = isset($plan['reason']) ? $plan['reason'] : 'posting_plan_failed';
                    return $result;
                }
                $identity = array(
                    'payslip_id' => (int)$row['payslip_id'],
                    'trans_no' => (int)$row['trans_no'],
                    'tran_date' => $row['tran_date'],
                    'reference' => $row['reference'],
                );
                if (!payroll_apply_legacy_gl_side_effects($payslip_doc, $plan, $identity)
                    || !payroll_apply_legacy_balance_side_effects($payslip_doc)
                ) {
                    cancel_transaction();
                    $result['code'] = 'financial_effect_stage_failed';
                    return $result;
                }
            }

            if (!update_payslip_status_compare_and_set((int)$row['payslip_id'], 0, 2)) {
                cancel_transaction();
                $result['code'] = 'payslip_status_compare_and_set_failed';
                return $result;
            }
        }

        if (!update_payroll_period_status($period_id, 3, 2)) {
            cancel_transaction();
            $result['code'] = 'period_status_compare_and_set_failed';
            return $result;
        }
        $period_after = get_payroll_period_for_update($period_id);
        $verification = verify_posted_payroll_period_evidence($period_after, $draft_data, true);
        if (empty($verification['ok'])) {
            cancel_transaction();
            $result['code'] = $verification['code'];
            return $result;
        }

        $audit_result = function_exists('append_domain_audit_event')
            ? append_domain_audit_event(array(
                'legal_entity_id' => 0,
                'entity_type' => 'payroll_period',
                'entity_key' => array('period_id' => $period_id),
                'action' => 'payroll_period.posted',
                'reason_code' => 'approved_post_command',
                'legacy_source' => 'approval_drafts',
                'legacy_id' => isset($approved_draft['id']) ? (int)$approved_draft['id'] : null,
                'before' => array('period_id' => $period_id, 'status' => 2),
                'after' => array('period_id' => $period_id, 'status' => 3),
                'restricted_diff' => array(
                    'changed_fields' => array('status', 'payslip_status', 'financial_effects'),
                    'mutation_kind' => 'posted',
                    'approval_binding_contract' => $binding['contract'],
                    'approval_binding_version' => $binding['version'],
                    'approval_hash_verified' => true,
                    'payslip_count' => $verification['payslip_count'],
                    'reference_count' => $verification['reference_count'],
                    'gl_line_count' => $verification['gl_line_count'],
                    'financial_side_effects_created' => $binding_version === '1.1.0',
                    'restricted_values_stored' => false,
                ),
            )) : false;
        if ($audit_result === false) {
            cancel_transaction();
            $result['code'] = 'audit_append_failed';
            return $result;
        }

        commit_transaction();
        return array(
            'ok' => true,
            'code' => 'posted',
            'message' => _('The approved payroll period was posted successfully.'),
            'payslip_count' => $verification['payslip_count'],
        );
    } catch (Exception $e) {
        cancel_transaction();
        $result['code'] = 'posting_exception';
        return $result;
    }
}

/**
 * Post a calculated payslip through the separated legacy posting stages.
 *
 * The public adapter remains backward compatible and atomic. It intentionally
 * still executes result persistence, reference reservation, GL, loan, and
 * leave stages in one transaction for manual payslips outside payroll-period
 * approval sets. Batch payroll uses the separate approved post command.
 *
 * @param payslip_doc $payslip_doc
 * @return int|false GL transaction number on success, false on failure
 */
function post_payslip_to_gl(&$payslip_doc) {
    if (!is_object($payslip_doc) || empty($payslip_doc->employee_id))
        return false;

    if (payroll_calculation_read_only_active()) {
        if (function_exists('display_error'))
            display_error(_('Payroll posting is not permitted inside the read-only calculation boundary.'));
        return false;
    }

    begin_transaction();
    try {
        if (function_exists('hrm_has_hook') && hrm_has_hook('before_post_payslip'))
            hrm_fire_hook('before_post_payslip', $payslip_doc);

        $plan = payroll_build_legacy_posting_plan($payslip_doc);
        if (empty($plan['ok'])) {
            if (function_exists('display_error'))
                display_error($plan['message']);
            cancel_transaction();
            return false;
        }

        $result_context = payroll_persist_legacy_payslip_result($payslip_doc);
        if ($result_context === false) {
            cancel_transaction();
            return false;
        }

        if (!payroll_apply_legacy_gl_side_effects(
            $payslip_doc,
            $plan,
            $result_context
        )) {
            cancel_transaction();
            return false;
        }

        if (!payroll_apply_legacy_balance_side_effects($payslip_doc)) {
            cancel_transaction();
            return false;
        }

        if (function_exists('hrm_has_hook') && hrm_has_hook('after_post_payslip'))
            hrm_fire_hook(
                'after_post_payslip',
                $payslip_doc->payslip_id,
                $result_context['trans_no']
            );

        commit_transaction();
        return (int)$result_context['trans_no'];
    } catch (Exception $e) {
        cancel_transaction();
        throw $e;
    }
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
