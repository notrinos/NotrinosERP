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
 * Check whether table exists in current company schema.
 *
 * @param string $table_name
 * @return bool
 */
function payroll_table_exists($table_name) {
    $sql = "SHOW TABLES LIKE ".db_escape(TB_PREF.$table_name);
    $result = db_query($sql);
    return $result && db_num_rows($result) > 0;
}

/**
 * Convert user/system date to SQL date format.
 *
 * @param string $date_value
 * @return string
 */
function payroll_to_sql_date($date_value) {
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
 * Resolve payslip header table for totals aggregation.
 *
 * @return string|false
 */
function payroll_payslip_header_table() {
    if (payroll_table_exists('payslips'))
        return 'payslips';
    if (payroll_table_exists('payslip'))
        return 'payslip';

    return false;
}

/**
 * Add payroll period.
 *
 * @param string $period_name
 * @param string $from_date
 * @param string $to_date
 * @param int|null $department_id
 * @return int|false
 */
function add_payroll_period($period_name, $from_date, $to_date, $department_id=null) {
    if (!payroll_table_exists('payroll_periods'))
        return false;

    $period_name = trim((string)$period_name);
    $from_sql = payroll_to_sql_date($from_date);
    $to_sql = payroll_to_sql_date($to_date);

    if ($period_name === '' || $from_sql === '' || $to_sql === '')
        return false;
    if ($from_sql > $to_sql)
        return false;

    $created_by = null;
    if (isset($_SESSION['wa_current_user'])) {
        $user = $_SESSION['wa_current_user'];
        if (isset($user->user) && is_numeric($user->user))
            $created_by = (int)$user->user;
        elseif (isset($user->id) && is_numeric($user->id))
            $created_by = (int)$user->id;
    }

    $fields = array('period_name', 'from_date', 'to_date');
    $values = array(db_escape($period_name), db_escape($from_sql), db_escape($to_sql));

    if ($department_id !== null && $department_id !== '') {
        $fields[] = 'department_id';
        $values[] = db_escape((int)$department_id);
    }

    if ($created_by !== null) {
        $fields[] = 'created_by';
        $values[] = db_escape($created_by);
    }

    $sql = "INSERT INTO ".TB_PREF."payroll_periods (".implode(', ', $fields).") VALUES (".implode(', ', $values).")";
    $db_ok = db_query($sql, 'could not add payroll period');
    if (!$db_ok)
        return false;

    return db_insert_id();
}

/**
 * Get one payroll period by id.
 *
 * @param int $period_id
 * @return array|false
 */
function get_payroll_period($period_id) {
    if (!payroll_table_exists('payroll_periods'))
        return false;

    $sql = "SELECT * FROM ".TB_PREF."payroll_periods WHERE period_id = ".db_escape((int)$period_id);
    $result = db_query($sql, 'could not get payroll period');

    return db_fetch($result);
}

/**
 * Count payroll periods optionally filtered by status.
 *
 * @param int|null $status
 * @return int
 */
function count_payroll_periods($status=null) {
    if (!payroll_table_exists('payroll_periods'))
        return 0;

    $sql = "SELECT COUNT(*) FROM ".TB_PREF."payroll_periods";
    if ($status !== null && $status !== '')
        $sql .= " WHERE status = ".db_escape((int)$status);

    $result = db_query($sql, 'could not count payroll periods');
    $row = db_fetch_row($result);
    return $row ? (int)$row[0] : 0;
}

/**
 * Get payroll periods optionally filtered by status.
 *
 * @param int|null $status
 * @param int|null $limit
 * @param int $offset
 * @return resource|false
 */
function get_payroll_periods($status=null, $limit=null, $offset=0) {
    if (!payroll_table_exists('payroll_periods'))
        return false;

    $sql = "SELECT * FROM ".TB_PREF."payroll_periods";
    if ($status !== null && $status !== '')
        $sql .= " WHERE status = ".db_escape((int)$status);
    $sql .= " ORDER BY from_date DESC, period_id DESC";

    $offset = max((int)$offset, 0);
    if ($limit !== null && (int)$limit > 0)
        $sql .= " LIMIT ".db_escape((int)$limit)." OFFSET ".db_escape($offset);

    return db_query($sql, 'could not get payroll periods');
}

/**
 * Update payroll period status.
 *
 * @param int $period_id
 * @param int $status
 * @return bool
 */
function update_payroll_period_status($period_id, $status) {
    if (!payroll_table_exists('payroll_periods'))
        return false;

    $status = (int)$status;
    $set = array("status = ".db_escape($status));

    if ($status === 2) {
        $approved_by = '';
        if (isset($_SESSION['wa_current_user'])) {
            $user = $_SESSION['wa_current_user'];
            if (isset($user->loginname))
                $approved_by = $user->loginname;
            elseif (isset($user->username))
                $approved_by = $user->username;
        }
        if ($approved_by !== '')
            $set[] = "approved_by = ".db_escape($approved_by);
        $set[] = "approval_date = NOW()";
    }

    $sql = "UPDATE ".TB_PREF."payroll_periods SET ".implode(', ', $set)." WHERE period_id = ".db_escape((int)$period_id);
    return (bool)db_query($sql, 'could not update payroll period status');
}

/**
 * Recalculate and update payroll period totals from payslips.
 *
 * @param int $period_id
 * @return bool
 */
function update_payroll_period_totals($period_id) {
    if (!payroll_table_exists('payroll_periods'))
        return false;

    $payslip_table = payroll_payslip_header_table();
    $gross = 0;
    $deductions = 0;
    $net = 0;
    $employer_cost = 0;

    if ($payslip_table) {
        $period_col = 'payroll_period_id';
        if (!check_empty_result("SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = ".db_escape(TB_PREF.$payslip_table)." AND COLUMN_NAME = 'payroll_period_id'")) {
            $period_col = 'period_id';
        }

        $sum_sql = "SELECT
            SUM(IFNULL(gross_salary, 0)) as total_gross,
            SUM(IFNULL(total_deductions, 0)) as total_deductions,
            SUM(IFNULL(net_salary, IFNULL(payable_amount, 0))) as total_net,
            SUM(IFNULL(total_cost, IFNULL(employer_contributions, 0) + IFNULL(net_salary, IFNULL(payable_amount, 0)))) as total_employer_cost
            FROM ".TB_PREF.$payslip_table." WHERE ".$period_col." = ".db_escape((int)$period_id);
        $sum_res = db_query($sum_sql, 'could not aggregate payroll period totals');
        $sum_row = db_fetch_assoc($sum_res);

        $gross = (float)$sum_row['total_gross'];
        $deductions = (float)$sum_row['total_deductions'];
        $net = (float)$sum_row['total_net'];
        $employer_cost = (float)$sum_row['total_employer_cost'];
    }

    $sql = "UPDATE ".TB_PREF."payroll_periods SET
        total_gross = ".db_escape($gross).",
        total_deductions = ".db_escape($deductions).",
        total_net = ".db_escape($net).",
        total_employer_cost = ".db_escape($employer_cost)."
        WHERE period_id = ".db_escape((int)$period_id);

    return (bool)db_query($sql, 'could not update payroll period totals');
}

/**
 * Void payroll period and mark linked payslips as void when possible.
 *
 * @param int $period_id
 * @return bool
 */
function void_payroll_period($period_id) {
    if (!payroll_table_exists('payroll_periods'))
        return false;

    begin_transaction();

    $ok = update_payroll_period_status((int)$period_id, 6);
    if (!$ok)
        return false;

    $payslip_table = payroll_payslip_header_table();
    if ($payslip_table) {
        $period_col = "payroll_period_id";
        if (!check_empty_result("SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = ".db_escape(TB_PREF.$payslip_table)." AND COLUMN_NAME = 'payroll_period_id'")) {
            $period_col = 'period_id';
        }

        $set_parts = array();
        if (check_empty_result("SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = ".db_escape(TB_PREF.$payslip_table)." AND COLUMN_NAME = 'status'"))
            $set_parts[] = "status = 6";
        if (check_empty_result("SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = ".db_escape(TB_PREF.$payslip_table)." AND COLUMN_NAME = 'voided_date'"))
            $set_parts[] = "voided_date = ".db_escape(date2sql(Today()));

        if (!empty($set_parts)) {
            $sql = "UPDATE ".TB_PREF.$payslip_table." SET ".implode(', ', $set_parts)." WHERE ".$period_col." = ".db_escape((int)$period_id);
            db_query($sql, 'could not void linked payslips');
        }
    }

    commit_transaction();
    return true;
}

