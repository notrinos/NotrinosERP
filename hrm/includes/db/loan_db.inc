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

/**
 * Employee loan request and repayment CRUD
 */

/**
 * Convert user date to SQL date.
 *
 * @param string $date_value
 * @return string
 */
function loan_to_sql_date($date_value) {
    $date_value = trim((string)$date_value);
    if ($date_value === '')
        return '';

    if (preg_match('/^\d{4}-\d{2}-\d{2}$/', $date_value))
        return $date_value;

    if (is_date($date_value))
        return date2sql($date_value);

    return $date_value;
}

/**
 * Validate that the repayment schedule starts on or after the loan date.
 *
 * @param string $loan_date
 * @param string $first_repayment
 * @return bool
 */
function loan_has_valid_repayment_start_date($loan_date, $first_repayment) {
    $loan_date_sql = loan_to_sql_date($loan_date);
    $first_repayment_sql = loan_to_sql_date($first_repayment);

    if ($loan_date_sql === '' || $first_repayment_sql === '')
        return false;

    return $first_repayment_sql >= $loan_date_sql;
}

/**
 * Add employee loan request.
 *
 * @param string $employee_id
 * @param int $loan_type_id
 * @param float $loan_amount
 * @param float $interest_rate
 * @param int $installments
 * @param float $installment_amount
 * @param string $loan_date
 * @param string $first_repayment
 * @param string $notes
 * @return int|false
 */
function add_employee_loan($employee_id, $loan_type_id, $loan_amount, $interest_rate, $installments, $installment_amount, $loan_date, $first_repayment, $notes='') {
    $loan_date_sql = loan_to_sql_date($loan_date);
    $first_repayment_sql = loan_to_sql_date($first_repayment);
    if ($loan_date_sql === '' || $first_repayment_sql === '' || (int)$installments <= 0
        || !loan_has_valid_repayment_start_date($loan_date_sql, $first_repayment_sql))
        return false;

    $sql = "INSERT INTO ".TB_PREF."employee_loans
        (employee_id, loan_type_id, loan_amount, interest_rate, installments, installment_amount,
         outstanding_amount, loan_date, first_repayment, status, notes)
        VALUES (
            ".db_escape($employee_id).",
            ".db_escape((int)$loan_type_id).",
            ".db_escape((float)$loan_amount).",
            ".db_escape((float)$interest_rate).",
            ".db_escape((int)$installments).",
            ".db_escape((float)$installment_amount).",
            ".db_escape((float)$loan_amount).",
            ".db_escape($loan_date_sql).",
            ".db_escape($first_repayment_sql).",
            0,
            ".db_escape($notes)."
        )";

    db_query($sql, 'could not add employee loan');
    return (int)db_insert_id();
}

/**
 * Update employee loan request.
 *
 * @param int $loan_id
 * @param int $loan_type_id
 * @param float $loan_amount
 * @param float $interest_rate
 * @param int $installments
 * @param float $installment_amount
 * @param string $loan_date
 * @param string $first_repayment
 * @param string $notes
 * @return bool
 */
function update_employee_loan($loan_id, $loan_type_id, $loan_amount, $interest_rate, $installments, $installment_amount, $loan_date, $first_repayment, $notes='') {
    $loan_date_sql = loan_to_sql_date($loan_date);
    $first_repayment_sql = loan_to_sql_date($first_repayment);
    if ($loan_date_sql === '' || $first_repayment_sql === '' || (int)$installments <= 0
        || !loan_has_valid_repayment_start_date($loan_date_sql, $first_repayment_sql))
        return false;

    // For pending loans (status=0), keep outstanding_amount in sync with loan_amount.
    // Active loans (status=1) have their outstanding managed by apply_loan_repayment().
    $sql = "UPDATE ".TB_PREF."employee_loans SET
        loan_type_id = ".db_escape((int)$loan_type_id).",
        loan_amount = ".db_escape((float)$loan_amount).",
        outstanding_amount = CASE WHEN status = 0 THEN ".db_escape((float)$loan_amount)." ELSE outstanding_amount END,
        interest_rate = ".db_escape((float)$interest_rate).",
        installments = ".db_escape((int)$installments).",
        installment_amount = ".db_escape((float)$installment_amount).",
        loan_date = ".db_escape($loan_date_sql).",
        first_repayment = ".db_escape($first_repayment_sql).",
        notes = ".db_escape($notes)."
        WHERE loan_id = ".db_escape((int)$loan_id)." AND status IN (0, 1)";

    return (bool)db_query($sql, 'could not update employee loan');
}

/**
 * Get one employee loan.
 *
 * @param int $loan_id
 * @return array|false
 */
function get_employee_loan($loan_id) {
    $sql = "SELECT l.*, lt.loan_type_name, lt.loan_type_code, lt.account_code, lt.interest_account,
        TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,''))) employee_name
        FROM ".TB_PREF."employee_loans l
        LEFT JOIN ".TB_PREF."loan_types lt ON lt.loan_type_id = l.loan_type_id
        LEFT JOIN ".TB_PREF."employees e ON e.employee_id = l.employee_id
        WHERE l.loan_id = ".db_escape((int)$loan_id);
    $result = db_query($sql, 'could not get employee loan');
    return db_fetch_assoc($result);
}

/**
 * Get employee loans list.
 *
 * @param string $employee_id
 * @param int|null $status
 * @return resource
 */
function get_employee_loans($employee_id='', $status=null) {
    $sql = "SELECT l.*, lt.loan_type_name,
        TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,''))) employee_name
        FROM ".TB_PREF."employee_loans l
        LEFT JOIN ".TB_PREF."loan_types lt ON lt.loan_type_id = l.loan_type_id
        LEFT JOIN ".TB_PREF."employees e ON e.employee_id = l.employee_id
        WHERE 1=1";

    if ($employee_id !== '' && $employee_id !== ALL_TEXT)
        $sql .= " AND l.employee_id = ".db_escape($employee_id);

    if ($status !== null && $status !== '')
        $sql .= " AND l.status = ".db_escape((int)$status);

    $sql .= " ORDER BY l.loan_date DESC, l.loan_id DESC";
    return db_query($sql, 'could not get employee loans');
}

/**
 * Delete pending loan request.
 *
 * @param int $loan_id
 * @return bool
 */
function delete_employee_loan($loan_id) {
    // Verify the loan is actually pending before touching any data.
    $check = db_query("SELECT loan_id FROM ".TB_PREF."employee_loans WHERE loan_id = ".db_escape((int)$loan_id)." AND status = 0", 'could not check loan status');
    if (!db_fetch_assoc($check))
        return false;

    begin_transaction();
    db_query("DELETE FROM ".TB_PREF."loan_repayments WHERE loan_id = ".db_escape((int)$loan_id), 'could not delete loan repayments');
    db_query("DELETE FROM ".TB_PREF."employee_loans WHERE loan_id = ".db_escape((int)$loan_id)." AND status = 0", 'could not delete employee loan');
    $deleted = db_num_affected_rows() > 0;
    commit_transaction();

    return $deleted;
}

/**
 * Add one repayment schedule row.
 *
 * @param int $loan_id
 * @param int $installment_no
 * @param string $due_date_sql
 * @param float $principal_amount
 * @param float $interest_amount
 * @return int
 */
function add_loan_repayment_row($loan_id, $installment_no, $due_date_sql, $principal_amount, $interest_amount) {
    $total = (float)$principal_amount + (float)$interest_amount;
    $sql = "INSERT INTO ".TB_PREF."loan_repayments
        (loan_id, installment_no, due_date, principal_amount, interest_amount, total_amount, paid_amount, status)
        VALUES (
            ".db_escape((int)$loan_id).",
            ".db_escape((int)$installment_no).",
            ".db_escape($due_date_sql).",
            ".db_escape((float)$principal_amount).",
            ".db_escape((float)$interest_amount).",
            ".db_escape((float)$total).",
            0,
            0
        )";

    db_query($sql, 'could not add loan repayment row');
    return (int)db_insert_id();
}

/**
 * Generate repayment schedule for a loan.
 *
 * @param int $loan_id
 * @return bool
 */
function generate_loan_repayment_schedule($loan_id) {
    $loan = get_employee_loan($loan_id);
    if (!$loan)
        return false;

    $installments = max((int)$loan['installments'], 1);
    $loan_amount = (float)$loan['loan_amount'];
    $interest_rate = (float)$loan['interest_rate'];
    $first_date = loan_to_sql_date($loan['first_repayment']);
    if ($first_date === '')
        return false;

    $total_interest = $loan_amount * $interest_rate / 100;
    $principal_each = round2($loan_amount / $installments, user_price_dec());
    $interest_each = round2($total_interest / $installments, user_price_dec());

    begin_transaction();
    db_query("DELETE FROM ".TB_PREF."loan_repayments WHERE loan_id = ".db_escape((int)$loan_id)." AND status = 0", 'could not reset scheduled repayments');

    $current = new DateTime($first_date);
    for ($index = 1; $index <= $installments; $index++) {
        $principal = $principal_each;
        $interest = $interest_each;

        if ($index === $installments) {
            // Absorb cumulative rounding errors in both principal and interest on the last row.
            $scheduled_sql = "SELECT IFNULL(SUM(principal_amount),0) p, IFNULL(SUM(interest_amount),0) i FROM ".TB_PREF."loan_repayments WHERE loan_id = ".db_escape((int)$loan_id);
            $scheduled_result = db_query($scheduled_sql, 'could not aggregate scheduled amounts');
            $scheduled_row = db_fetch_assoc($scheduled_result);
            $principal = round2($loan_amount - (float)$scheduled_row['p'], user_price_dec());
            $interest  = round2($total_interest - (float)$scheduled_row['i'], user_price_dec());
        }

        add_loan_repayment_row((int)$loan_id, $index, $current->format('Y-m-d'), $principal, $interest);
        $current->modify('+1 month');
    }

    commit_transaction();
    return true;
}

/**
 * Approve loan and create repayment schedule.
 *
 * @param int $loan_id
 * @param string $approved_by
 * @return bool
 */
function approve_employee_loan($loan_id, $approved_by='') {
    begin_transaction();
    $sql = "UPDATE ".TB_PREF."employee_loans SET
        status = 1,
        approved_by = ".(trim((string)$approved_by) === '' ? 'NULL' : db_escape($approved_by)).",
        approval_date = CURDATE()
        WHERE loan_id = ".db_escape((int)$loan_id)." AND status = 0";
    db_query($sql, 'could not approve employee loan');

    $ok = generate_loan_repayment_schedule((int)$loan_id);

    // Sync outstanding_amount to the actual repayment schedule total (includes interest).
    if ($ok) {
        $total_sql = "SELECT IFNULL(SUM(total_amount),0) tot FROM ".TB_PREF."loan_repayments WHERE loan_id = ".db_escape((int)$loan_id);
        $total_result = db_query($total_sql, 'could not sum repayment schedule');
        $total_row = db_fetch_assoc($total_result);
        db_query("UPDATE ".TB_PREF."employee_loans SET outstanding_amount = ".db_escape((float)$total_row['tot'])."
            WHERE loan_id = ".db_escape((int)$loan_id), 'could not sync outstanding amount after approve');
    }

    commit_transaction();

    return $ok;
}

/**
 * Cancel loan request/loan.
 *
 * @param int $loan_id
 * @return bool
 */
function cancel_employee_loan($loan_id) {
    $sql = "UPDATE ".TB_PREF."employee_loans SET status = 3 WHERE loan_id = ".db_escape((int)$loan_id)." AND status IN (0,1)";
    return (bool)db_query($sql, 'could not cancel employee loan');
}

/**
 * Mark loan as cancelled by approval rejection/cancellation.
 *
 * @param int $loan_id
 * @return bool
 */
function mark_employee_loan_cancelled_by_approval($loan_id) {
    $sql = "UPDATE ".TB_PREF."employee_loans
        SET status = 3
        WHERE loan_id = ".db_escape((int)$loan_id)."
            AND status = 0";
    db_query($sql, 'could not set cancelled status for employee loan');

    return db_num_affected_rows() > 0;
}

/**
 * Check whether a loan already has a pending core approval draft.
 *
 * @param int $loan_id
 * @return bool
 */
function has_pending_loan_request_approval($loan_id) {
    $sql = "SELECT id FROM ".TB_PREF."approval_drafts
        WHERE trans_type = ".ST_LOAN_REQUEST."
            AND status = ".APPROVAL_STATUS_PENDING."
            AND draft_data LIKE ".db_escape('%\"loan_id\":'.(int)$loan_id.'%')."
        ORDER BY id DESC LIMIT 1";
    $result = db_query($sql, 'could not check pending loan request approval draft');

    return (bool)db_fetch($result);
}

/**
 * Build approval draft payload for a loan request.
 *
 * @param int $loan_id
 * @return array|false
 */
function get_loan_request_approval_data($loan_id) {
    $loan = get_employee_loan((int)$loan_id);
    if (!$loan)
        return false;

    return array(
        'loan_id' => (int)$loan['loan_id'],
        'employee_id' => $loan['employee_id'],
        'employee_name' => $loan['employee_name'],
        'loan_type_id' => (int)$loan['loan_type_id'],
        'loan_type_name' => isset($loan['loan_type_name']) ? $loan['loan_type_name'] : '',
        'loan_amount' => (float)$loan['loan_amount'],
        'interest_rate' => (float)$loan['interest_rate'],
        'installments' => (int)$loan['installments'],
        'installment_amount' => (float)$loan['installment_amount'],
        'loan_date' => $loan['loan_date'],
        'first_repayment' => $loan['first_repayment'],
        'notes' => isset($loan['notes']) ? $loan['notes'] : '',
    );
}

/**
 * Submit a loan request to core approval workflow.
 *
 * @param int $loan_id
 * @return array|bool
 */
function submit_employee_loan_for_core_approval($loan_id) {
    $loan = get_employee_loan((int)$loan_id);
    if (!$loan || (int)$loan['status'] !== 0)
        return false;

    if (has_pending_loan_request_approval((int)$loan_id))
        return false;

    $approval_registered = class_exists('ApprovalWorkflow') && ApprovalWorkflow::isRegistered(ST_LOAN_REQUEST);
    $approval_amount = (float)$loan['loan_amount'];

    if (!$approval_registered)
        return approve_employee_loan((int)$loan_id, 'approval_system') ? array('status' => 'auto_approved', 'loan_id' => (int)$loan_id) : false;

    $approval = get_approval_workflow_service();
    if (!$approval->isApprovalRequired(ST_LOAN_REQUEST, $approval_amount))
        return approve_employee_loan((int)$loan_id, 'approval_system') ? array('status' => 'auto_approved', 'loan_id' => (int)$loan_id) : false;

    $draft_data = get_loan_request_approval_data((int)$loan_id);
    if ($draft_data === false)
        return false;

    return $approval->submit(
        ST_LOAN_REQUEST,
        $draft_data,
        $approval_amount,
        array(
            'summary' => sprintf(_('Loan Request #%d'), (int)$loan_id),
            'currency' => get_company_pref('curr_default'),
            'person_type' => PT_EMPLOYEE,
            'person_id' => $loan['employee_id'],
            'loc_code' => '',
        )
    );
}

/**
 * Get repayment rows for a loan.
 *
 * @param int $loan_id
 * @param int|null $status
 * @return resource
 */
function get_loan_repayments($loan_id, $status=null) {
    $sql = "SELECT * FROM ".TB_PREF."loan_repayments WHERE loan_id = ".db_escape((int)$loan_id);
    if ($status !== null && $status !== '')
        $sql .= " AND status = ".db_escape((int)$status);
    $sql .= " ORDER BY installment_no";

    return db_query($sql, 'could not get loan repayments');
}

/**
 * Get due loan repayments for employee and period.
 *
 * @param string $employee_id
 * @param string $from_date
 * @param string $to_date
 * @return resource
 */
function get_due_loan_repayments($employee_id, $from_date, $to_date) {
    $from_sql = loan_to_sql_date($from_date);
    $to_sql = loan_to_sql_date($to_date);

    $sql = "SELECT r.*, l.loan_id, l.employee_id, lt.loan_type_name, lt.account_code
        FROM ".TB_PREF."loan_repayments r
        JOIN ".TB_PREF."employee_loans l ON l.loan_id = r.loan_id
        LEFT JOIN ".TB_PREF."loan_types lt ON lt.loan_type_id = l.loan_type_id
        WHERE l.status = 1
            AND l.employee_id = ".db_escape($employee_id)."
            AND r.status IN (0, 2)
            AND r.due_date >= ".db_escape($from_sql)."
            AND r.due_date <= ".db_escape($to_sql)."
        ORDER BY r.due_date, r.installment_no";

    return db_query($sql, 'could not get due loan repayments');
}

/**
 * Get GL account to use for due loan deductions in period.
 *
 * @param string $employee_id
 * @param string $from_date
 * @param string $to_date
 * @return string
 */
function get_due_loan_deduction_account($employee_id, $from_date, $to_date) {
    $rows = get_due_loan_repayments($employee_id, $from_date, $to_date);
    while ($row = db_fetch_assoc($rows)) {
        if (!empty($row['account_code']))
            return $row['account_code'];
    }

    return '';
}

/**
 * Get total due loan deduction for period.
 *
 * @param string $employee_id
 * @param string $from_date
 * @param string $to_date
 * @return float
 */
function get_total_due_loan_deduction($employee_id, $from_date, $to_date) {
    $from_sql = loan_to_sql_date($from_date);
    $to_sql = loan_to_sql_date($to_date);

    $sql = "SELECT IFNULL(SUM(r.total_amount - IFNULL(r.paid_amount, 0)), 0) amount
        FROM ".TB_PREF."loan_repayments r
        JOIN ".TB_PREF."employee_loans l ON l.loan_id = r.loan_id
        WHERE l.status = 1
            AND l.employee_id = ".db_escape($employee_id)."
            AND r.status IN (0, 2)
            AND r.due_date >= ".db_escape($from_sql)."
            AND r.due_date <= ".db_escape($to_sql);
    $result = db_query($sql, 'could not get due loan deduction');
    $row = db_fetch_assoc($result);

    return $row ? (float)$row['amount'] : 0;
}

/**
 * Apply payment for one repayment row.
 *
 * @param int $repayment_id
 * @param float $paid_amount
 * @param string $paid_date
 * @param int $payslip_id
 * @return bool
 */
function apply_loan_repayment($repayment_id, $paid_amount, $paid_date='', $payslip_id=0) {
    $paid_date_sql = loan_to_sql_date($paid_date);
    if ($paid_date_sql === '')
        $paid_date_sql = date2sql(Today());

    $sql = "SELECT * FROM ".TB_PREF."loan_repayments WHERE repayment_id = ".db_escape((int)$repayment_id);
    $result = db_query($sql, 'could not get repayment row');
    $row = db_fetch_assoc($result);
    if (!$row)
        return false;

    $existing_paid = (float)$row['paid_amount'];
    $total_amount = (float)$row['total_amount'];
    $new_paid = $existing_paid + (float)$paid_amount;
    if ($new_paid > $total_amount)
        $new_paid = $total_amount;

    $status = (floatcmp($new_paid, $total_amount) >= 0) ? 1 : 0;
    $loan_id = (int)$row['loan_id'];

    begin_transaction();
    $update = "UPDATE ".TB_PREF."loan_repayments SET
        paid_amount = ".db_escape($new_paid).",
        paid_date = ".db_escape($paid_date_sql).",
        payslip_id = ".((int)$payslip_id > 0 ? db_escape((int)$payslip_id) : 'NULL').",
        status = ".db_escape($status)."
        WHERE repayment_id = ".db_escape((int)$repayment_id);
    db_query($update, 'could not update repayment row');

    $remaining_sql = "SELECT IFNULL(SUM(total_amount - IFNULL(paid_amount, 0)), 0) rem
        FROM ".TB_PREF."loan_repayments WHERE loan_id = ".db_escape($loan_id)." AND status IN (0,2)";
    $remaining_result = db_query($remaining_sql, 'could not calculate loan outstanding');
    $remaining_row = db_fetch_assoc($remaining_result);
    $outstanding = max((float)$remaining_row['rem'], 0);

    $loan_status = (floatcmp($outstanding, 0) == 0) ? 2 : 1;
    $loan_update = "UPDATE ".TB_PREF."employee_loans SET
        outstanding_amount = ".db_escape($outstanding).",
        status = ".db_escape($loan_status)."
        WHERE loan_id = ".db_escape($loan_id);
    db_query($loan_update, 'could not update loan outstanding');

    commit_transaction();
    return true;
}

/**
 * Mark due repayments as paid by payslip deduction.
 *
 * @param string $employee_id
 * @param string $from_date
 * @param string $to_date
 * @param int $payslip_id
 * @return float
 */
function mark_loan_repayments_paid_by_payslip($employee_id, $from_date, $to_date, $payslip_id=0) {
    $total = 0.0;
    $rows = get_due_loan_repayments($employee_id, $from_date, $to_date);
    while ($row = db_fetch_assoc($rows)) {
        $due = (float)$row['total_amount'] - (float)$row['paid_amount'];
        if (floatcmp($due, 0) <= 0)
            continue;

        if (apply_loan_repayment((int)$row['repayment_id'], $due, $to_date, (int)$payslip_id))
            $total += $due;
    }

    return $total;
}

