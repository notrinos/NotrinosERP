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
    static $cache = array();

    if (isset($cache[$table_name]))
        return $cache[$table_name];

    $sql = "SHOW TABLES LIKE '".TB_PREF.$table_name."'";
    $result = db_query($sql);
    if ($result && db_num_rows($result) > 0) {
        $cache[$table_name] = true;
        return true;
    }

    // Payroll core tables are expected in this module; allow a safe fallback
    // when metadata lookups fail inside long HRM posting flows.
    if (in_array($table_name, array('payroll_periods', 'payslips'), true)) {
        $cache[$table_name] = true;
        return true;
    }

    $cache[$table_name] = false;
    return false;
}

/**
 * Ensure payroll_periods table exists for the active company schema.
 *
 * @return bool
 */
function ensure_payroll_periods_table() {
    if (payroll_table_exists('payroll_periods'))
        return true;

    $sql = "CREATE TABLE IF NOT EXISTS ".TB_PREF."payroll_periods (
        period_id int(11) NOT NULL AUTO_INCREMENT,
        period_name varchar(60) NOT NULL,
        from_date date NOT NULL,
        to_date date NOT NULL,
        pay_date date DEFAULT NULL,
        department_id int(11) DEFAULT NULL,
        status tinyint(1) NOT NULL DEFAULT 0,
        total_gross double NOT NULL DEFAULT 0,
        total_deductions double NOT NULL DEFAULT 0,
        total_net double NOT NULL DEFAULT 0,
        total_employer_cost double NOT NULL DEFAULT 0,
        approved_by varchar(20) DEFAULT NULL,
        approval_date datetime DEFAULT NULL,
        gl_trans_no int(11) DEFAULT NULL,
        created_by smallint(6) DEFAULT NULL,
        created_date timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
        notes text,
        PRIMARY KEY (period_id),
        KEY from_date (from_date),
        KEY status (status)
    ) ENGINE=InnoDB";

    $db_ok = db_query($sql, 'could not create payroll_periods table');
    return (bool)$db_ok;
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

    return false;
}

/**
 * Check whether a linked payslip column exists.
 *
 * @param string $table_name Table name without prefix.
 * @param string $column_name
 * @return bool
 */
function payroll_payslip_has_column($table_name, $column_name) {
    return check_empty_result(
        "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE()"
        ." AND TABLE_NAME = ".db_escape(TB_PREF.$table_name)
        ." AND COLUMN_NAME = ".db_escape($column_name)
    );
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
    if (!ensure_payroll_periods_table())
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

    $period_id = (int)db_insert_id();
    if ($period_id <= 0) {
        $lookup = "SELECT period_id FROM ".TB_PREF."payroll_periods WHERE "
            ."period_name = ".db_escape($period_name)
            ." AND from_date = ".db_escape($from_sql)
            ." AND to_date = ".db_escape($to_sql);

        if ($department_id !== null && $department_id !== '')
            $lookup .= " AND department_id = ".db_escape((int)$department_id);
        else
            $lookup .= " AND department_id IS NULL";

        $lookup .= " ORDER BY period_id DESC LIMIT 1";
        $lookup_result = db_query($lookup, 'could not resolve payroll period id');
        $lookup_row = $lookup_result ? db_fetch_row($lookup_result) : false;
        if ($lookup_row)
            $period_id = (int)$lookup_row[0];
    }

    return $period_id > 0 ? $period_id : false;
}

/**
 * Build the exact payroll-period locking read projection.
 *
 * The complete legacy row shape is retained because approval, posting,
 * reversal, reopen, and void commands consume different fields from the same
 * locked row. The caller must own the surrounding transaction.
 *
 * @param int $period_id
 * @return string
 */
function get_payroll_period_for_update_sql($period_id) {
    return "SELECT period_id, period_name, from_date, to_date, pay_date, "
        ."department_id, status, total_gross, total_deductions, total_net, "
        ."total_employer_cost, approved_by, approval_date, gl_trans_no, "
        ."created_by, created_date, notes "
        ."FROM ".TB_PREF."payroll_periods WHERE period_id = "
        .db_escape((int)$period_id)." FOR UPDATE";
}

/**
 * Get one payroll period by id FOR UPDATE (row lock).
 *
 * @param int $period_id
 * @return array|false
 */
function get_payroll_period_for_update($period_id) {
    if (!payroll_table_exists('payroll_periods'))
        return false;

    $sql = get_payroll_period_for_update_sql($period_id);
    $result = db_query($sql, 'could not get payroll period for update');

    return db_fetch($result);
}

/**
 * Build the exact payroll-period approval-status projection.
 *
 * The payment approval guard consumes only identity and status. Negative
 * identifiers remain available to rollback-only fixtures; persisted callers
 * continue to use the positive-id repository helper below. Lock semantics are
 * intentionally unchanged because command callers already own their existing
 * payroll-period locks and the payment page also performs a read-only preflight.
 *
 * @param mixed $period_id
 * @return string|false
 */
function get_payroll_period_approval_status_sql($period_id) {
    $period_id = (int)$period_id;
    if ($period_id === 0)
        return false;

    return "SELECT period_id, status FROM ".TB_PREF."payroll_periods"
        ." WHERE period_id = ".db_escape($period_id);
}

/**
 * Get the exact payroll-period identity and approval status.
 *
 * @param mixed $period_id
 * @return array|false
 */
function get_payroll_period_approval_status($period_id) {
    $period_id = (int)$period_id;
    if ($period_id <= 0 || !payroll_table_exists('payroll_periods'))
        return false;

    $sql = get_payroll_period_approval_status_sql($period_id);
    if ($sql === false)
        return false;
    $result = db_query($sql, 'could not get payroll period approval status');

    return db_fetch($result);
}

/**
 * Build the exact payroll-period read projection.
 *
 * Negative identifiers are reserved for transaction-scoped test fixtures; the
 * public repository helper below accepts only persisted positive identifiers.
 *
 * @param int $period_id
 * @return string|false
 */
function get_payroll_period_sql($period_id) {
    $period_id = (int)$period_id;
    if ($period_id === 0)
        return false;

    return "SELECT period_id, period_name, from_date, to_date, pay_date, "
        ."department_id, status, total_gross, total_deductions, total_net, "
        ."total_employer_cost, approved_by, approval_date, gl_trans_no, "
        ."created_by, created_date, notes "
        ."FROM ".TB_PREF."payroll_periods WHERE period_id = ".db_escape($period_id);
}

/**
 * Get one persisted payroll period by id.
 *
 * @param int $period_id
 * @return array|false
 */
function get_payroll_period($period_id) {
    $period_id = (int)$period_id;
    if ($period_id <= 0 || !payroll_table_exists('payroll_periods'))
        return false;

    $sql = get_payroll_period_sql($period_id);
    if ($sql === false)
        return false;
    $result = db_query($sql, 'could not get payroll period');

    return db_fetch($result);
}


/**
 * Canonical contract identifier for payroll-period approval binding.
 *
 * @return array
 */
function payroll_period_approval_binding_contract($version=null) {
    $supported = array('1.0.0', '1.1.0');
    if ($version === null || $version === '')
        $version = '1.1.0';
    if (!in_array((string)$version, $supported, true))
        return false;

    return array(
        'id' => 'notrinoserp.hrm.payroll.approval-binding',
        'version' => (string)$version,
    );
}

/**
 * Check whether an approval-binding version remains supported.
 *
 * @param string $version
 * @return bool
 */
function payroll_period_approval_binding_version_supported($version) {
    return payroll_period_approval_binding_contract($version) !== false;
}

/**
 * Normalize one decimal for stable cross-request hashing.
 *
 * Six fractional digits match the existing payroll-period drift tolerance and
 * avoid locale-dependent formatting or negative zero in the approval payload.
 *
 * @param mixed $value
 * @return string|false
 */
function payroll_approval_binding_decimal($value) {
    if (!is_numeric($value))
        return false;

    $number = (float)$value;
    if (is_nan($number) || is_infinite($number))
        return false;
    if (abs($number) < 0.0000005)
        $number = 0.0;

    return sprintf('%.6F', $number);
}

/**
 * Normalize one integer without accepting fractional or exponent notation.
 *
 * Database integer columns are returned as native integers or canonical base-10
 * strings. Rejecting broader is_numeric() forms prevents distinct malformed
 * source values from collapsing to the same approval evidence.
 *
 * @param mixed $value
 * @return int|false
 */
function payroll_approval_binding_integer($value) {
    if (is_int($value))
        return $value;
    if (!is_string($value)
        || preg_match('/^-?(0|[1-9][0-9]*)$/D', $value) !== 1
    ) {
        return false;
    }

    $integer = (int)$value;
    return (string)$integer === $value ? $integer : false;
}

/**
 * Recursively normalize associative keys before JSON hashing.
 *
 * @param mixed $value
 * @return mixed
 */
function payroll_approval_binding_canonicalize($value) {
    if (!is_array($value))
        return $value;

    $keys = array_keys($value);
    $is_list = $keys === range(0, count($value) - 1);
    if (!$is_list)
        ksort($value, SORT_STRING);

    foreach ($value as $key => $item)
        $value[$key] = payroll_approval_binding_canonicalize($item);

    return $value;
}

/**
 * Encode a deterministic JSON representation for one approval hash payload.
 *
 * @param array $payload
 * @return string|false
 */
function payroll_approval_binding_json($payload) {
    $json = json_encode(payroll_approval_binding_canonicalize($payload));
    return is_string($json) ? $json : false;
}

/**
 * Project one database row into explicitly typed canonical fields.
 *
 * @param array $row
 * @param array $types field => string|int|nullable_int|decimal|nullable_decimal
 * @return array|false
 */
function payroll_approval_binding_project_row($row, $types) {
    if (!is_array($row))
        return false;

    $projected = array();
    foreach ($types as $field => $type) {
        $value = array_key_exists($field, $row) ? $row[$field] : null;
        if ($type === 'string') {
            $projected[$field] = (string)$value;
        } elseif ($type === 'nullable_string') {
            $projected[$field] = $value === null ? null : (string)$value;
        } elseif ($type === 'int') {
            $integer = payroll_approval_binding_integer($value);
            if ($integer === false)
                return false;
            $projected[$field] = $integer;
        } elseif ($type === 'nullable_int') {
            if ($value === null || $value === '')
                $projected[$field] = null;
            else {
                $integer = payroll_approval_binding_integer($value);
                if ($integer === false)
                    return false;
                $projected[$field] = $integer;
            }
        } elseif ($type === 'decimal' || $type === 'nullable_decimal') {
            if ($type === 'nullable_decimal' && ($value === null || $value === '')) {
                $projected[$field] = null;
            } else {
                $decimal = payroll_approval_binding_decimal($value);
                if ($decimal === false)
                    return false;
                $projected[$field] = $decimal;
            }
        } else {
            return false;
        }
    }

    return $projected;
}

/**
 * Fetch all rows for one deterministic approval-binding query.
 *
 * @param string $sql
 * @param string $error
 * @return array|false
 */
function payroll_approval_binding_fetch_rows($sql, $error) {
    $result = db_query($sql, $error);
    if (!$result)
        return false;

    $rows = array();
    while ($row = db_fetch_assoc($result))
        $rows[] = $row;
    return $rows;
}

/**
 * Build a deterministic reconciled result/control hash for a locked period.
 *
 * The result hash covers the period envelope, every active payslip header and
 * every persisted line. The control hash covers exact payroll GL rows, absence
 * of core void/payment state, linked loan repayment state, and leave-balance
 * state for employees in the period. All mutable rows are read FOR UPDATE when
 * requested so the approval draft and terminal executor serialize with legacy
 * posting/payment/void and balance writers.
 *
 * No raw employee, formula, memo, amount, account, or balance value leaves this
 * helper; callers receive only opaque hashes, counts, and a stable failure code.
 *
 * @param array $period Locked payroll-period row
 * @param bool $lock
 * @return array
 */
function build_payroll_period_approval_binding($period, $lock=true, $contract_version='auto') {
    $failed = array(
        'ok' => false,
        'code' => 'invalid_period',
    );
    if (!is_array($period) || empty($period['period_id']))
        return $failed;

    $period_id = (int)$period['period_id'];
    if ($period_id <= 0 || !isset($period['status']) || (int)$period['status'] !== 1)
        return $failed;

    foreach (array('payslips', 'payslip_details', 'gl_trans', 'voided') as $table) {
        if (!payroll_table_exists($table)) {
            $failed['code'] = 'required_table_missing';
            return $failed;
        }
    }

    $suffix = $lock ? ' FOR UPDATE' : '';
    $payslip_sql = get_payroll_period_approval_payslips_sql($period_id, $lock);
    if ($payslip_sql === false) {
        $failed['code'] = 'payslip_read_failed';
        return $failed;
    }
    $payslip_rows = payroll_approval_binding_fetch_rows(
        $payslip_sql,
        'could not lock payroll-period payslips for approval binding'
    );
    if ($payslip_rows === false) {
        $failed['code'] = 'payslip_read_failed';
        return $failed;
    }
    if (empty($payslip_rows)) {
        $failed['code'] = 'empty_payroll_result';
        return $failed;
    }

    $payslip_types = array(
        'payslip_id' => 'int', 'payroll_period_id' => 'int',
        'trans_no' => 'int', 'reference' => 'string', 'employee_id' => 'string',
        'tran_date' => 'string', 'from_date' => 'string', 'to_date' => 'string',
        'department_id' => 'nullable_int', 'position_id' => 'nullable_int',
        'grade_id' => 'nullable_int', 'basic_salary' => 'decimal',
        'total_earnings' => 'decimal', 'gross_salary' => 'decimal',
        'total_deductions' => 'decimal', 'net_salary' => 'decimal',
        'employer_contributions' => 'decimal', 'total_cost' => 'decimal',
        'working_days' => 'decimal', 'worked_days' => 'decimal',
        'overtime_hours' => 'decimal', 'leave_days' => 'decimal',
        'absent_days' => 'decimal', 'leaves' => 'int',
        'deductable_leaves' => 'int', 'loan_deduction' => 'decimal',
        'payable_amount' => 'decimal', 'salary_amount' => 'decimal',
        'status' => 'int', 'payment_trans_no' => 'nullable_int',
        'notes' => 'nullable_string', 'custom_data' => 'nullable_string',
    );

    $payslips = array();
    $payslip_ids = array();
    $trans_nos = array();
    $seen_trans_nos = array();
    $references_by_trans_no = array();
    $employee_ids = array();
    $sum_gross = 0.0;
    $sum_deductions = 0.0;
    $sum_net = 0.0;
    $sum_employer_cost = 0.0;
    foreach ($payslip_rows as $row) {
        $projected = payroll_approval_binding_project_row($row, $payslip_types);
        if ($projected === false
            || $projected['payroll_period_id'] !== $period_id
            || $projected['payslip_id'] <= 0
            || $projected['trans_no'] <= 0
            || trim($projected['employee_id']) === ''
            || $projected['status'] === 6
            || $projected['status'] >= 3
            || !empty($projected['payment_trans_no'])
        ) {
            $failed['code'] = 'invalid_payslip_control_state';
            return $failed;
        }
        if (isset($seen_trans_nos[$projected['trans_no']])) {
            $failed['code'] = 'duplicate_payroll_transaction';
            return $failed;
        }
        $seen_trans_nos[$projected['trans_no']] = true;
        $references_by_trans_no[$projected['trans_no']] = $projected['reference'];
        $payslips[] = $projected;
        $payslip_ids[] = $projected['payslip_id'];
        $trans_nos[] = $projected['trans_no'];
        $employee_ids[$projected['employee_id']] = true;
        $sum_gross += (float)$row['gross_salary'];
        $sum_deductions += (float)$row['total_deductions'];
        $sum_net += (float)$row['net_salary'];
        $sum_employer_cost += (float)$row['total_cost'];
    }

    foreach (array(
        'total_gross' => $sum_gross,
        'total_deductions' => $sum_deductions,
        'total_net' => $sum_net,
        'total_employer_cost' => $sum_employer_cost,
    ) as $field => $calculated) {
        if (!isset($period[$field])
            || abs((float)$period[$field] - $calculated) > 0.000001
        ) {
            $failed['code'] = 'period_totals_drift';
            return $failed;
        }
    }

    $details = array();
    $detail_counts = array();
    if (!empty($payslip_ids)) {
        $detail_sql = get_payroll_period_approval_payslip_details_sql($payslip_ids, $lock);
        if ($detail_sql === false) {
            $failed['code'] = 'detail_read_failed';
            return $failed;
        }
        $detail_rows = payroll_approval_binding_fetch_rows(
            $detail_sql,
            'could not lock payroll-period details for approval binding'
        );
        if ($detail_rows === false) {
            $failed['code'] = 'detail_read_failed';
            return $failed;
        }
        $detail_types = array(
            'detail_id' => 'int', 'payslip_id' => 'int', 'element_id' => 'int',
            'element_name' => 'string', 'element_category' => 'int',
            'is_deduction' => 'int', 'amount_type' => 'int',
            'base_amount' => 'decimal', 'rate' => 'decimal',
            'calculated_amount' => 'decimal', 'adjusted_amount' => 'nullable_decimal',
            'final_amount' => 'decimal', 'account_code' => 'string',
            'formula_used' => 'nullable_string', 'is_taxable' => 'int',
            'display_order' => 'int', 'memo' => 'nullable_string',
        );
        foreach ($detail_rows as $row) {
            $projected = payroll_approval_binding_project_row($row, $detail_types);
            if ($projected === false || !in_array($projected['payslip_id'], $payslip_ids, true)) {
                $failed['code'] = 'invalid_detail_state';
                return $failed;
            }
            $details[] = $projected;
            if (!isset($detail_counts[$projected['payslip_id']]))
                $detail_counts[$projected['payslip_id']] = 0;
            $detail_counts[$projected['payslip_id']]++;
        }
        foreach ($payslip_ids as $payslip_id) {
            if (empty($detail_counts[$payslip_id])) {
                $failed['code'] = 'missing_payslip_details';
                return $failed;
            }
        }
    }

    $gl_rows_projected = array();
    $gl_row_count = 0;
    $void_rows_projected = array();
    if (!empty($trans_nos)) {
        $gl_sql = get_payroll_period_approval_gl_rows_sql($trans_nos, $lock);
        if ($gl_sql === false) {
            $failed['code'] = 'gl_read_failed';
            return $failed;
        }
        $gl_rows = payroll_approval_binding_fetch_rows(
            $gl_sql,
            'could not lock payroll-period GL rows for approval binding'
        );
        if ($gl_rows === false) {
            $failed['code'] = 'gl_read_failed';
            return $failed;
        }
        $gl_row_count = count($gl_rows);
        $gl_types = array(
            'counter' => 'int', 'type' => 'int', 'type_no' => 'int',
            'tran_date' => 'string', 'account' => 'string', 'memo_' => 'string',
            'amount' => 'decimal', 'dimension_id' => 'int',
            'dimension2_id' => 'int', 'person_type_id' => 'nullable_int',
            'person_id_hex' => 'nullable_string',
        );
        $gl_counts = array();
        $gl_balances = array();
        foreach ($gl_rows as $row) {
            $projected = payroll_approval_binding_project_row($row, $gl_types);
            if ($projected === false || !in_array($projected['type_no'], $trans_nos, true)) {
                $failed['code'] = 'invalid_gl_state';
                return $failed;
            }
            if (abs((float)$row['amount']) <= 0.000001)
                continue;
            $gl_rows_projected[] = $projected;
            if (!isset($gl_counts[$projected['type_no']])) {
                $gl_counts[$projected['type_no']] = 0;
                $gl_balances[$projected['type_no']] = 0.0;
            }
            $gl_counts[$projected['type_no']]++;
            $gl_balances[$projected['type_no']] += (float)$row['amount'];
        }
        $void_sql = get_payroll_period_approval_void_rows_sql($trans_nos, $lock);
        if ($void_sql === false) {
            $failed['code'] = 'void_read_failed';
            return $failed;
        }
        $void_rows = payroll_approval_binding_fetch_rows(
            $void_sql,
            'could not lock payroll-period void rows for approval binding'
        );
        if ($void_rows === false) {
            $failed['code'] = 'void_read_failed';
            return $failed;
        }
        foreach ($void_rows as $row) {
            $projected = payroll_approval_binding_project_row($row, array(
                'type' => 'int', 'id' => 'int', 'date_' => 'string', 'memo_' => 'string',
            ));
            if ($projected === false) {
                $failed['code'] = 'invalid_void_state';
                return $failed;
            }
            $void_rows_projected[] = $projected;
        }
        if (!empty($void_rows_projected)) {
            $failed['code'] = 'voided_payroll_result';
            return $failed;
        }
    }

    if ($contract_version === null || $contract_version === '' || $contract_version === 'auto')
        $contract_version = $gl_row_count === 0 ? '1.1.0' : '1.0.0';
    $contract = payroll_period_approval_binding_contract($contract_version);
    if ($contract === false) {
        $failed['code'] = 'unsupported_binding_version';
        return $failed;
    }

    if ($contract['version'] === '1.0.0') {
        foreach ($trans_nos as $trans_no) {
            if (empty($gl_counts[$trans_no])
                || $gl_counts[$trans_no] < 2
                || abs($gl_balances[$trans_no]) > 0.000001
            ) {
                $failed['code'] = 'unreconciled_gl_control';
                return $failed;
            }
        }
        foreach ($payslips as $payslip) {
            if ((int)$payslip['status'] > 2) {
                $failed['code'] = 'invalid_payslip_control_state';
                return $failed;
            }
        }
    } else {
        if ($gl_row_count !== 0) {
            $failed['code'] = 'unexpected_preapproval_gl';
            return $failed;
        }
        foreach ($payslips as $payslip) {
            if ((int)$payslip['status'] !== 0
                || trim((string)$payslip['reference']) === ''
            ) {
                $failed['code'] = 'invalid_prepared_payslip_state';
                return $failed;
            }
        }
    }

    $reference_rows_projected = array();
    if ($contract['version'] === '1.1.0') {
        if (!payroll_table_exists('refs')) {
            $failed['code'] = 'required_table_missing';
            return $failed;
        }
        $reference_sql = get_payroll_period_approval_reference_rows_sql($trans_nos, $lock);
        if ($reference_sql === false) {
            $failed['code'] = 'reference_read_failed';
            return $failed;
        }
        $reference_rows = payroll_approval_binding_fetch_rows(
            $reference_sql,
            'could not lock payroll-period reference identity for approval binding'
        );
        if ($reference_rows === false) {
            $failed['code'] = 'reference_read_failed';
            return $failed;
        }
        $seen_reference_ids = array();
        foreach ($reference_rows as $row) {
            $projected = payroll_approval_binding_project_row($row, array(
                'type' => 'int', 'id' => 'int', 'reference' => 'string',
            ));
            if ($projected === false
                || $projected['type'] !== (int)ST_PAYSLIP
                || !isset($references_by_trans_no[$projected['id']])
                || isset($seen_reference_ids[$projected['id']])
                || !hash_equals(
                    (string)$references_by_trans_no[$projected['id']],
                    (string)$projected['reference']
                )
            ) {
                $failed['code'] = 'reference_identity_drift';
                return $failed;
            }
            $seen_reference_ids[$projected['id']] = true;
            $reference_rows_projected[] = $projected;
        }
        if (count($seen_reference_ids) !== count($trans_nos)) {
            $failed['code'] = 'reference_identity_missing';
            return $failed;
        }
    }

    $posting_configuration = array();
    if ($contract['version'] === '1.1.0') {
        if (!payroll_table_exists('sys_prefs')) {
            $failed['code'] = 'required_table_missing';
            return $failed;
        }
        $posting_configuration_sql = get_payroll_period_approval_posting_configuration_sql($lock);
        $preference_rows = payroll_approval_binding_fetch_rows(
            $posting_configuration_sql,
            'could not lock payroll posting configuration for approval binding'
        );
        if ($preference_rows === false || count($preference_rows) !== 1) {
            $failed['code'] = 'posting_configuration_missing';
            return $failed;
        }
        $posting_configuration = payroll_approval_binding_project_row(
            $preference_rows[0],
            array('name' => 'string', 'value' => 'string')
        );
        if ($posting_configuration === false
            || $posting_configuration['name'] !== 'payroll_payable_act'
            || trim($posting_configuration['value']) === ''
        ) {
            $failed['code'] = 'posting_configuration_invalid';
            return $failed;
        }
    }

    $loan_rows_projected = array();
    if (!empty($payslip_ids) && payroll_table_exists('loan_repayments')) {
        $loan_types = array(
            'repayment_id' => 'int', 'loan_id' => 'int', 'installment_no' => 'int',
            'due_date' => 'string', 'principal_amount' => 'decimal',
            'interest_amount' => 'decimal', 'total_amount' => 'decimal',
            'paid_amount' => 'decimal', 'paid_date' => 'nullable_string',
            'payslip_id' => 'nullable_int', 'status' => 'int',
        );
        if ($contract['version'] !== '1.0.0') {
            if (!payroll_table_exists('employee_loans')) {
                $failed['code'] = 'required_table_missing';
                return $failed;
            }
            $loan_types['employee_id'] = 'string';
            $loan_types['loan_status'] = 'int';
        }
        $loan_sql = get_payroll_period_approval_loan_controls_sql(
            $contract['version'],
            $payslip_ids,
            array_keys($employee_ids),
            (string)$period['from_date'],
            (string)$period['to_date'],
            $lock
        );
        if ($loan_sql === false) {
            $failed['code'] = 'loan_read_failed';
            return $failed;
        }
        $loan_rows = payroll_approval_binding_fetch_rows(
            $loan_sql,
            'could not lock payroll-period loan controls for approval binding'
        );
        if ($loan_rows === false) {
            $failed['code'] = 'loan_read_failed';
            return $failed;
        }
        foreach ($loan_rows as $row) {
            $projected = payroll_approval_binding_project_row($row, $loan_types);
            if ($projected === false) {
                $failed['code'] = 'invalid_loan_state';
                return $failed;
            }
            if ($contract['version'] === '1.1.0') {
                if (!in_array($projected['employee_id'], array_keys($employee_ids), true)
                    || (int)$projected['loan_status'] !== 1
                    || $projected['payslip_id'] !== null
                ) {
                    $failed['code'] = 'unexpected_preapproval_loan_effect';
                    return $failed;
                }
            }
            $loan_rows_projected[] = $projected;
        }
    }

    $leave_rows_projected = array();
    if (!empty($employee_ids) && payroll_table_exists('leave_balances')) {
        $employee_sql = array();
        foreach (array_keys($employee_ids) as $employee_id)
            $employee_sql[] = db_escape($employee_id);
        $leave_sql = "SELECT balance_id,employee_id,leave_id,fiscal_year,entitled,"
            ."carried_forward,taken,pending,adjusted FROM ".TB_PREF."leave_balances"
            ." WHERE employee_id IN (".implode(',', $employee_sql).")"
            ." ORDER BY employee_id,fiscal_year,leave_id,balance_id".$suffix;
        $leave_rows = payroll_approval_binding_fetch_rows(
            $leave_sql,
            'could not lock payroll-period leave balances for approval binding'
        );
        if ($leave_rows === false) {
            $failed['code'] = 'leave_read_failed';
            return $failed;
        }
        $leave_types = array(
            'balance_id' => 'int', 'employee_id' => 'string', 'leave_id' => 'int',
            'fiscal_year' => 'int', 'entitled' => 'decimal',
            'carried_forward' => 'decimal', 'taken' => 'decimal',
            'pending' => 'decimal', 'adjusted' => 'decimal',
        );
        foreach ($leave_rows as $row) {
            $projected = payroll_approval_binding_project_row($row, $leave_types);
            if ($projected === false) {
                $failed['code'] = 'invalid_leave_state';
                return $failed;
            }
            $leave_rows_projected[] = $projected;
        }
    }

    $leave_request_rows_projected = array();
    if ($contract['version'] === '1.1.0'
        && !empty($employee_ids)
        && payroll_table_exists('leave_requests')
    ) {
        $employee_sql = array();
        foreach (array_keys($employee_ids) as $employee_id)
            $employee_sql[] = db_escape($employee_id);
        $leave_request_sql = "SELECT request_id,employee_id,leave_id,from_date,to_date,"
            ."days,half_day,status FROM ".TB_PREF."leave_requests"
            ." WHERE employee_id IN (".implode(',', $employee_sql).")"
            ." AND status IN (0,1)"
            ." ORDER BY employee_id,from_date,to_date,leave_id,request_id".$suffix;
        $leave_request_rows = payroll_approval_binding_fetch_rows(
            $leave_request_sql,
            'could not lock payroll-period leave request controls for approval binding'
        );
        if ($leave_request_rows === false) {
            $failed['code'] = 'leave_request_read_failed';
            return $failed;
        }
        $leave_request_types = array(
            'request_id' => 'int', 'employee_id' => 'string', 'leave_id' => 'int',
            'from_date' => 'string', 'to_date' => 'string', 'days' => 'decimal',
            'half_day' => 'int', 'status' => 'int',
        );
        foreach ($leave_request_rows as $row) {
            $projected = payroll_approval_binding_project_row($row, $leave_request_types);
            if ($projected === false
                || !in_array($projected['employee_id'], array_keys($employee_ids), true)
                || !in_array((int)$projected['status'], array(0, 1), true)
            ) {
                $failed['code'] = 'invalid_leave_request_state';
                return $failed;
            }
            $leave_request_rows_projected[] = $projected;
        }
    }

    $period_projection = payroll_approval_binding_project_row($period, array(
        'period_id' => 'int', 'period_name' => 'string', 'from_date' => 'string',
        'to_date' => 'string', 'pay_date' => 'nullable_string',
        'department_id' => 'nullable_int', 'status' => 'int',
        'total_gross' => 'decimal', 'total_deductions' => 'decimal',
        'total_net' => 'decimal', 'total_employer_cost' => 'decimal',
    ));
    if ($period_projection === false) {
        $failed['code'] = 'invalid_period_projection';
        return $failed;
    }

    $result_payload = array(
        'contract' => $contract,
        'period' => $period_projection,
        'payslips' => $payslips,
        'details' => $details,
    );
    $control_payload = array(
        'contract' => $contract,
        'period_id' => $period_id,
        'counts' => array(
            'payslips' => count($payslips),
            'details' => count($details),
            'gl_lines' => count($gl_rows_projected),
            'loan_repayments' => count($loan_rows_projected),
            'leave_balances' => count($leave_rows_projected),
        ),
        'reconciled_totals' => array(
            'gross' => payroll_approval_binding_decimal($sum_gross),
            'deductions' => payroll_approval_binding_decimal($sum_deductions),
            'net' => payroll_approval_binding_decimal($sum_net),
            'employer_cost' => payroll_approval_binding_decimal($sum_employer_cost),
        ),
        'gl_rows' => $gl_rows_projected,
        'void_rows' => $void_rows_projected,
        'loan_repayments' => $loan_rows_projected,
        'leave_balances' => $leave_rows_projected,
    );
    if ($contract['version'] === '1.1.0') {
        $control_payload['counts']['references'] = count($reference_rows_projected);
        $control_payload['counts']['leave_requests'] = count($leave_request_rows_projected);
        $control_payload['reference_rows'] = $reference_rows_projected;
        $control_payload['leave_requests'] = $leave_request_rows_projected;
        $control_payload['posting_configuration'] = $posting_configuration;
    }
    $result_json = payroll_approval_binding_json($result_payload);
    $control_json = payroll_approval_binding_json($control_payload);
    if ($result_json === false || $control_json === false) {
        $failed['code'] = 'hash_encoding_failed';
        return $failed;
    }

    $result_hash = hash('sha256', $contract['id'].'|'.$contract['version'].'|result|'.$result_json);
    $control_hash = hash('sha256', $contract['id'].'|'.$contract['version'].'|control|'.$control_json);
    $approval_hash = hash('sha256', $contract['id'].'|'.$contract['version'].'|'.$result_hash.'|'.$control_hash);

    return array(
        'ok' => true,
        'code' => 'verified',
        'contract' => $contract['id'],
        'version' => $contract['version'],
        'result_hash' => $result_hash,
        'control_hash' => $control_hash,
        'approval_hash' => $approval_hash,
        'payslip_count' => count($payslips),
        'detail_count' => count($details),
        'gl_line_count' => count($gl_rows_projected),
        'loan_repayment_count' => count($loan_rows_projected),
        'leave_balance_count' => count($leave_rows_projected),
    );
}

/**
 * Project one verified binding into the persisted approval draft.
 *
 * @param array $binding
 * @return array|false
 */
function payroll_period_approval_binding_draft_fields($binding) {
    if (empty($binding['ok']))
        return false;

    return array(
        'approval_binding_contract' => $binding['contract'],
        'approval_binding_version' => $binding['version'],
        'approval_result_hash' => $binding['result_hash'],
        'approval_control_hash' => $binding['control_hash'],
        'approval_hash' => $binding['approval_hash'],
        'approval_payslip_count' => (int)$binding['payslip_count'],
        'approval_detail_count' => (int)$binding['detail_count'],
        'approval_gl_line_count' => (int)$binding['gl_line_count'],
        'approval_loan_repayment_count' => (int)$binding['loan_repayment_count'],
        'approval_leave_balance_count' => (int)$binding['leave_balance_count'],
    );
}

/**
 * Verify that an approval draft is bound to the current locked result/control.
 *
 * @param array $draft_data
 * @param array $binding
 * @return bool
 */
function payroll_period_approval_binding_matches_draft($draft_data, $binding) {
    if (!is_array($draft_data)
        || empty($binding['ok'])
        || payroll_period_approval_binding_draft_state($draft_data) !== 'well_formed'
    ) {
        return false;
    }

    $expected = payroll_period_approval_binding_draft_fields($binding);
    if ($expected === false)
        return false;

    foreach ($expected as $field => $value) {
        if (!array_key_exists($field, $draft_data))
            return false;
        if (strpos($field, '_count') !== false) {
            if ((int)$draft_data[$field] !== (int)$value)
                return false;
        } elseif (!is_string($draft_data[$field])
            || !hash_equals((string)$value, $draft_data[$field])
        ) {
            return false;
        }
    }

    return true;
}

/**
 * Classify persisted approval-binding metadata without recomputing payroll.
 *
 * Rejection/cancellation may use this classification to close a stale or
 * malformed bound draft without ever approving it. Approval execution still
 * requires an exact recomputed match.
 *
 * @param array $draft_data
 * @return string absent|well_formed|malformed
 */
function payroll_period_approval_binding_draft_state($draft_data) {
    if (!is_array($draft_data))
        return 'absent';

    $fields = array(
        'approval_binding_contract', 'approval_binding_version',
        'approval_result_hash', 'approval_control_hash', 'approval_hash',
        'approval_payslip_count', 'approval_detail_count',
        'approval_gl_line_count', 'approval_loan_repayment_count',
        'approval_leave_balance_count',
    );
    $present = 0;
    foreach ($fields as $field) {
        if (array_key_exists($field, $draft_data))
            $present++;
    }
    if ($present === 0)
        return 'absent';
    if ($present !== count($fields))
        return 'malformed';

    $contract = payroll_period_approval_binding_contract(
        is_string($draft_data['approval_binding_version'])
            ? $draft_data['approval_binding_version'] : null
    );
    if ($contract === false
        || !is_string($draft_data['approval_binding_contract'])
        || !is_string($draft_data['approval_binding_version'])
        || !hash_equals($contract['id'], $draft_data['approval_binding_contract'])
        || !hash_equals($contract['version'], $draft_data['approval_binding_version'])
    ) {
        return 'malformed';
    }

    foreach (array(
        'approval_result_hash', 'approval_control_hash', 'approval_hash',
    ) as $field) {
        if (!is_string($draft_data[$field])
            || preg_match('/^[a-f0-9]{64}$/D', $draft_data[$field]) !== 1
        ) {
            return 'malformed';
        }
    }

    foreach (array(
        'approval_payslip_count', 'approval_detail_count',
        'approval_gl_line_count', 'approval_loan_repayment_count',
        'approval_leave_balance_count',
    ) as $field) {
        $value = $draft_data[$field];
        if ((!is_int($value)
                && (!is_string($value)
                    || preg_match('/^[0-9]+$/D', $value) !== 1))
            || (int)$value < 0
        ) {
            return 'malformed';
        }
    }

    return 'well_formed';
}

/**
 * Build the exact approval-draft projection for one payroll period and status.
 *
 * The complete legacy approval_drafts row is retained because posting and
 * reversal consume decoded binding evidence while duplicate matching rows must
 * still fail closed. Negative identifiers are reserved for rollback-only test
 * fixtures; the public repository helper accepts only persisted positive ids.
 *
 * @param int $period_id
 * @param int $status Approval status; approved is 1.
 * @param bool $lock
 * @return string|false
 */
function get_payroll_period_approval_draft_sql($period_id, $status=1, $lock=true) {
    $period_id = (int)$period_id;
    if ($period_id === 0)
        return false;

    return "SELECT id, workflow_id, trans_type, reserved_trans_no, draft_data, "
        ."amount, currency, person_type_id, person_id, current_level, status, "
        ."submitted_by, submitted_date, approved_trans_no, completed_date, "
        ."workflow_version, reference, summary, loc_code "
        ."FROM ".TB_PREF."approval_drafts"
        ." WHERE trans_type = ".db_escape(ST_PAYROLL_PERIOD)
        ." AND status = ".db_escape((int)$status)
        ." AND approved_trans_no = ".db_escape($period_id)
        ." ORDER BY id DESC".($lock ? ' FOR UPDATE' : '');
}

/**
 * Resolve the latest approval draft for one payroll period and status.
 *
 * @param int $period_id
 * @param int $status Approval status; approved is 1.
 * @param bool $lock
 * Missing or multiple matching approved drafts fail closed.
 *
 * @return array|false Unique row with decoded_data.
 */
function get_payroll_period_approval_draft($period_id, $status=1, $lock=true) {
    $period_id = (int)$period_id;
    if ($period_id <= 0 || !payroll_table_exists('approval_drafts'))
        return false;

    $sql = get_payroll_period_approval_draft_sql($period_id, $status, $lock);
    if ($sql === false)
        return false;
    $result = db_query($sql, 'could not get payroll-period approval evidence');
    if (!$result)
        return false;

    $matched = false;
    while ($row = db_fetch_assoc($result)) {
        $source = html_entity_decode(
            (string)$row['draft_data'],
            ENT_QUOTES,
            isset($_SESSION['language']->encoding)
                ? $_SESSION['language']->encoding : 'UTF-8'
        );
        $decoded = json_decode($source, true);
        if (is_array($decoded)
            && isset($decoded['period_id'])
            && (int)$decoded['period_id'] === $period_id
        ) {
            if ($matched !== false)
                return false;
            $row['decoded_data'] = $decoded;
            $matched = $row;
        }
    }

    return $matched;
}

/**
 * Compare-and-set one prepared payslip to Posted.
 *
 * @param int $payslip_id
 * @param int $expected_status
 * @param int $new_status
 * @return bool
 */
function update_payslip_status_compare_and_set($payslip_id, $expected_status, $new_status) {
    $table = payroll_payslip_header_table();
    if (!$table || (int)$payslip_id <= 0)
        return false;

    $sql = "UPDATE ".TB_PREF.$table
        ." SET status = ".db_escape((int)$new_status)
        ." WHERE payslip_id = ".db_escape((int)$payslip_id)
        ." AND status = ".db_escape((int)$expected_status);
    $ok = db_query($sql, 'could not update prepared payslip status');
    return $ok && db_num_affected_rows() === 1;
}

/**
 * Remove only a newly created empty Draft period after preparation rollback.
 *
 * @param int $period_id
 * @return bool
 */
function discard_empty_payroll_period_draft($period_id) {
    $period_id = (int)$period_id;
    $actor_id = payroll_side_effect_control_actor_id();
    if ($period_id <= 0
        || $actor_id <= 0
        || (function_exists('user_check_access') && !user_check_access('SA_PAYROLL'))
    ) {
        return false;
    }

    $sql = "DELETE p FROM ".TB_PREF."payroll_periods p"
        ." WHERE p.period_id = ".db_escape($period_id)
        ." AND p.status = 0"
        ." AND p.created_by = ".db_escape($actor_id)
        ." AND NOT EXISTS (SELECT 1 FROM ".TB_PREF."payslips s"
        ." WHERE s.payroll_period_id = p.period_id)";
    $ok = db_query($sql, 'could not discard empty payroll period draft');
    return $ok && db_num_affected_rows() === 1;
}

/**
 * Resolve the current numeric actor for PAY-CORE-011 containment logging.
 *
 * @return int
 */
function payroll_side_effect_control_actor_id() {
    if (!isset($_SESSION['wa_current_user']))
        return 0;

    $user = $_SESSION['wa_current_user'];
    if (isset($user->user) && is_numeric($user->user))
        return (int)$user->user;
    if (isset($user->id) && is_numeric($user->id))
        return (int)$user->id;

    return 0;
}

/**
 * Record a minimized denial event for an unsafe legacy void/reopen attempt.
 *
 * @param string $action
 * @param string $code
 * @param int $period_id
 * @param int $payslip_id
 * @param string $channel
 * @return void
 */
function payroll_side_effect_control_log_denial(
    $action,
    $code,
    $period_id=0,
    $payslip_id=0,
    $channel='hrm_db'
) {
    $payload = array(
        'contract' => 'notrinoserp.hrm.payroll.side-effect-control',
        'version' => '1.0.0',
        'action' => preg_replace('/[^a-z0-9_\-]/', '', strtolower((string)$action)),
        'code' => preg_replace('/[^a-z0-9_\-]/', '', strtolower((string)$code)),
        'company_id' => function_exists('user_company') ? (int)user_company() : 0,
        'actor_id' => payroll_side_effect_control_actor_id(),
        'payroll_period_id' => (int)$period_id,
        'payslip_id' => (int)$payslip_id,
        'channel' => preg_replace('/[^a-z0-9_\-]/', '', strtolower((string)$channel)),
    );

    error_log('PAY-CORE-011 '.json_encode($payload));
}

/**
 * Fetch one linked financial row and optionally lock it.
 *
 * @param string $sql
 * @param string $error
 * @return bool|null True when a row exists, false when none exists, null on query failure.
 */
function payroll_side_effect_row_exists($sql, $error) {
    $result = db_query($sql, $error);
    if (!$result)
        return null;

    return db_fetch_assoc($result) ? true : false;
}

/**
 * Build the exact persisted payslip projection for approval binding.
 *
 * The caller supplies a non-zero payroll-period identifier after locking the
 * authoritative period row. Fixed negative identifiers remain available only
 * to rollback-scoped fixtures. The complete legacy 32-field result shape and
 * deterministic payslip identity order are preserved. Identifier zero fails
 * closed before database access; callers choose whether to append the existing
 * row lock used by approval preparation and terminal execution.
 *
 * @param mixed $period_id
 * @param bool $lock
 * @return string|false
 */
function get_payroll_period_approval_payslips_sql($period_id, $lock=true) {
    $period_id = (int)$period_id;
    if ($period_id === 0)
        return false;

    return "SELECT payslip_id,payroll_period_id,trans_no,reference,employee_id,"
        ."tran_date,from_date,to_date,department_id,position_id,grade_id,basic_salary,"
        ."total_earnings,gross_salary,total_deductions,net_salary,employer_contributions,"
        ."total_cost,working_days,worked_days,overtime_hours,leave_days,absent_days,"
        ."leaves,deductable_leaves,loan_deduction,payable_amount,salary_amount,status,"
        ."payment_trans_no,notes,custom_data FROM ".TB_PREF."payslips"
        ." WHERE payroll_period_id = ".db_escape($period_id)
        ." ORDER BY payslip_id".($lock ? ' FOR UPDATE' : '');
}

/**
 * Build the exact persisted payslip-detail projection for approval binding.
 *
 * The caller supplies the non-empty payslip identity set obtained from the
 * locked persisted-header projection. Fixed negative identifiers remain
 * available only to rollback-scoped fixtures. The complete legacy 17-field
 * detail shape and deterministic payslip/display/detail order are preserved.
 * Empty, non-array, or zero-containing scopes fail closed before database
 * access; callers choose whether to append the existing approval row lock.
 *
 * @param array $payslip_ids
 * @param bool $lock
 * @return string|false
 */
function get_payroll_period_approval_payslip_details_sql($payslip_ids, $lock=true) {
    if (!is_array($payslip_ids) || empty($payslip_ids))
        return false;

    $ids = array();
    foreach ($payslip_ids as $payslip_id) {
        $payslip_id = (int)$payslip_id;
        if ($payslip_id === 0)
            return false;
        $ids[] = $payslip_id;
    }

    return "SELECT detail_id,payslip_id,element_id,element_name,element_category,"
        ."is_deduction,amount_type,base_amount,rate,calculated_amount,adjusted_amount,"
        ."final_amount,account_code,formula_used,is_taxable,display_order,memo FROM "
        .TB_PREF."payslip_details WHERE payslip_id IN (".implode(',', $ids).")"
        ." ORDER BY payslip_id,display_order,detail_id".($lock ? ' FOR UPDATE' : '');
}

/**
 * Build the exact persisted payroll GL projection for approval binding.
 *
 * The caller supplies the non-empty payroll transaction-number set obtained
 * from the locked persisted-payslip projection. Fixed negative identifiers
 * remain available only to rollback-scoped fixtures. The complete legacy
 * 11-field GL shape and deterministic transaction/counter order are preserved.
 * Empty, non-array, or zero-containing scopes fail closed before database
 * access; callers choose whether to append the existing approval row lock.
 *
 * @param array $trans_nos
 * @param bool $lock
 * @return string|false
 */
function get_payroll_period_approval_gl_rows_sql($trans_nos, $lock=true) {
    if (!is_array($trans_nos) || empty($trans_nos))
        return false;

    $ids = array();
    foreach ($trans_nos as $trans_no) {
        $trans_no = (int)$trans_no;
        if ($trans_no === 0)
            return false;
        $ids[] = $trans_no;
    }

    return "SELECT counter,type,type_no,tran_date,account,memo_,amount,"
        ."dimension_id,dimension2_id,person_type_id,HEX(person_id) person_id_hex"
        ." FROM ".TB_PREF."gl_trans WHERE type = ".db_escape(ST_PAYSLIP)
        ." AND type_no IN (".implode(',', $ids).")"
        ." ORDER BY type_no,counter".($lock ? ' FOR UPDATE' : '');
}

/**
 * Build the exact persisted payroll void projection for approval binding.
 *
 * The caller supplies the non-empty payroll transaction-number set obtained
 * from the locked persisted-payslip projection. Fixed negative identifiers
 * remain available only to rollback-scoped fixtures. The complete legacy
 * four-field void evidence shape and deterministic transaction identity order
 * are preserved. Empty, non-array, or zero-containing scopes fail closed
 * before database access; callers choose whether to append the existing
 * approval row lock.
 *
 * @param array $trans_nos
 * @param bool $lock
 * @return string|false
 */
function get_payroll_period_approval_void_rows_sql($trans_nos, $lock=true) {
    if (!is_array($trans_nos) || empty($trans_nos))
        return false;

    $ids = array();
    foreach ($trans_nos as $trans_no) {
        $trans_no = (int)$trans_no;
        if ($trans_no === 0)
            return false;
        $ids[] = $trans_no;
    }

    return "SELECT type,id,date_,memo_ FROM ".TB_PREF."voided"
        ." WHERE type = ".db_escape(ST_PAYSLIP)
        ." AND id IN (".implode(',', $ids).")"
        ." ORDER BY id".($lock ? ' FOR UPDATE' : '');
}

/**
 * Build the exact persisted payroll reference projection for approval binding.
 *
 * The caller supplies the non-empty payroll transaction-number set obtained
 * from the locked persisted-payslip projection. Fixed negative identifiers
 * remain available only to rollback-scoped fixtures. The exact three-field
 * reference identity shape and deterministic transaction identity order are
 * preserved. Empty, non-array, or zero-containing scopes fail closed before
 * database access; callers choose whether to append the existing approval row
 * lock.
 *
 * @param array $trans_nos
 * @param bool $lock
 * @return string|false
 */
function get_payroll_period_approval_reference_rows_sql($trans_nos, $lock=true) {
    if (!is_array($trans_nos) || empty($trans_nos))
        return false;

    $ids = array();
    foreach ($trans_nos as $trans_no) {
        $trans_no = (int)$trans_no;
        if ($trans_no === 0)
            return false;
        $ids[] = $trans_no;
    }

    return "SELECT type,id,reference FROM ".TB_PREF."refs"
        ." WHERE type = ".db_escape(ST_PAYSLIP)
        ." AND id IN (".implode(',', $ids).")"
        ." ORDER BY id".($lock ? ' FOR UPDATE' : '');
}

/**
 * Build the exact payroll posting-configuration projection for approval binding.
 *
 * The existing v1.1 approval contract binds the configured payroll payable
 * account by preference identity and value. The primary-key scoped read returns
 * only those two fields, excludes preference metadata and unrelated settings,
 * and preserves caller-selected row locking.
 *
 * @param bool $lock
 * @return string
 */
function get_payroll_period_approval_posting_configuration_sql($lock=true) {
    return "SELECT name,value FROM ".TB_PREF."sys_prefs"
        ." WHERE name = 'payroll_payable_act'".($lock ? ' FOR UPDATE' : '');
}

/**
 * Build the exact payroll approval loan-control projection.
 *
 * The legacy 1.0.0 approval contract binds only repayment rows already linked
 * to persisted payslips. The active 1.1.0 contract additionally binds active
 * employee-loan repayments due inside the payroll period, while retaining the
 * persisted-payslip scope used to detect unexpected pre-approval side effects.
 * Both paths preserve their approved exact fields, deterministic ordering, and
 * caller-selected terminal row lock. Unsupported versions, empty/non-array
 * identity scopes, zero payslip identifiers, blank employee identifiers, or
 * blank active-contract dates fail closed before database access.
 *
 * @param mixed $contract_version
 * @param mixed $payslip_ids
 * @param mixed $employee_ids
 * @param mixed $from_date
 * @param mixed $to_date
 * @param bool $lock
 * @return string|false
 */
function get_payroll_period_approval_loan_controls_sql($contract_version, $payslip_ids, $employee_ids, $from_date, $to_date, $lock=true) {
    $contract_version = (string)$contract_version;
    if (($contract_version !== '1.0.0' && $contract_version !== '1.1.0')
        || !is_array($payslip_ids) || empty($payslip_ids))
        return false;

    $ids = array();
    foreach ($payslip_ids as $payslip_id) {
        $payslip_id = (int)$payslip_id;
        if ($payslip_id === 0)
            return false;
        $ids[] = $payslip_id;
    }
    $id_sql = implode(',', $ids);
    if ($contract_version === '1.0.0') {
        return "SELECT repayment_id,loan_id,installment_no,due_date,"
            ."principal_amount,interest_amount,total_amount,paid_amount,paid_date,"
            ."payslip_id,status FROM ".TB_PREF."loan_repayments"
            ." WHERE payslip_id IN (".$id_sql.")"
            ." ORDER BY payslip_id,repayment_id".($lock ? ' FOR UPDATE' : '');
    }

    if (!is_array($employee_ids) || empty($employee_ids))
        return false;
    $employee_sql = array();
    foreach ($employee_ids as $employee_id) {
        $employee_id = (string)$employee_id;
        if (trim($employee_id) === '')
            return false;
        $employee_sql[] = db_escape($employee_id);
    }
    $from_date = (string)$from_date;
    $to_date = (string)$to_date;
    if (trim($from_date) === '' || trim($to_date) === '')
        return false;

    return "SELECT r.repayment_id,r.loan_id,r.installment_no,r.due_date,"
        ."r.principal_amount,r.interest_amount,r.total_amount,r.paid_amount,"
        ."r.paid_date,r.payslip_id,r.status,l.employee_id,l.status loan_status"
        ." FROM ".TB_PREF."loan_repayments r"
        ." JOIN ".TB_PREF."employee_loans l ON l.loan_id = r.loan_id"
        ." WHERE ((l.employee_id IN (".implode(',', $employee_sql).")"
        ." AND l.status = 1 AND r.status IN (0,2)"
        ." AND r.due_date >= ".db_escape($from_date)
        ." AND r.due_date <= ".db_escape($to_date).")"
        ." OR r.payslip_id IN (".$id_sql."))"
        ." ORDER BY l.employee_id,r.due_date,r.installment_no,r.repayment_id"
        .($lock ? ' FOR UPDATE' : '');
}

/**
 * Build the exact locked existing-payslip identity projection for preparation.
 *
 * The caller supplies a non-zero payroll-period identifier after locking and
 * verifying the Draft period and current operator ownership. Fixed negative
 * identifiers remain available only to rollback-scoped fixtures. The result is
 * deliberately limited to payslip identity, ordered deterministically, and
 * always locked so a concurrent result writer cannot bypass the empty-period
 * precondition. Identifier zero fails closed before database access.
 *
 * @param mixed $period_id
 * @return string|false
 */
function get_payroll_period_existing_payslip_ids_sql($period_id) {
    $period_id = (int)$period_id;
    if ($period_id === 0)
        return false;

    return "SELECT payslip_id FROM ".TB_PREF."payslips"
        ." WHERE payroll_period_id = ".db_escape($period_id)
        ." ORDER BY payslip_id FOR UPDATE";
}

/**
 * Build the exact locked loan-repayment posting-snapshot projection.
 *
 * Persisted callers supply a non-empty employee identifier and bounded payroll
 * dates. Fixed negative repayment and loan identifiers remain available only
 * to rollback-scoped fixtures. The exact joined eight-field result captures
 * repayment and active employee-loan pre-state before posting. Empty scope
 * values fail closed before database access, repayment identity order is
 * deterministic, and the joined row lock is mandatory.
 *
 * @param mixed $employee_id
 * @param mixed $from_date
 * @param mixed $to_date
 * @return string|false
 */
function get_payroll_reversal_loan_repayment_snapshot_sql($employee_id, $from_date, $to_date) {
    $employee_id = (string)$employee_id;
    $from_date = (string)$from_date;
    $to_date = (string)$to_date;
    if ($employee_id === '' || $from_date === '' || $to_date === '')
        return false;

    return "SELECT r.repayment_id,r.loan_id,r.paid_amount,r.paid_date,"
        ."r.payslip_id,r.status,l.outstanding_amount,l.status AS loan_status"
        ." FROM ".TB_PREF."loan_repayments r"
        ." JOIN ".TB_PREF."employee_loans l ON l.loan_id = r.loan_id"
        ." WHERE l.employee_id = ".db_escape($employee_id)
        ." AND l.status = 1 AND r.status IN (0,2)"
        ." AND r.due_date >= ".db_escape($from_date)
        ." AND r.due_date <= ".db_escape($to_date)
        ." ORDER BY r.repayment_id FOR UPDATE";
}

/**
 * Build the exact locked leave-request posting-snapshot projection.
 *
 * Persisted callers supply a non-empty employee identifier and bounded fiscal
 * year dates. Fixed negative request identifiers remain available only to
 * rollback-scoped fixtures. The exact two-field result is used only to derive
 * the distinct leave-balance identities captured before posting. Empty scope
 * values fail closed before database access, request identity order is
 * deterministic, and the row lock is mandatory.
 *
 * @param mixed $employee_id
 * @param mixed $year_start
 * @param mixed $year_end
 * @return string|false
 */
function get_payroll_reversal_leave_request_state_sql($employee_id, $year_start, $year_end) {
    $employee_id = (string)$employee_id;
    $year_start = (string)$year_start;
    $year_end = (string)$year_end;
    if ($employee_id === '' || $year_start === '' || $year_end === '')
        return false;

    return "SELECT request_id,leave_id"
        ." FROM ".TB_PREF."leave_requests"
        ." WHERE employee_id = ".db_escape($employee_id)
        ." AND status IN (0,1)"
        ." AND from_date >= ".db_escape($year_start)
        ." AND from_date <= ".db_escape($year_end)
        ." ORDER BY request_id FOR UPDATE";
}

/**
 * Build the exact locked leave-balance reversal-state projection.
 *
 * Persisted callers supply a non-empty employee identifier and positive leave
 * and fiscal-year identifiers. Fixed negative leave identifiers remain
 * available only to rollback-scoped fixtures. The exact nine-field row is
 * hashed before and after posting/reversal operations, so wildcard expansion
 * is prohibited. Empty employee identifiers and zero numeric identifiers fail
 * closed before database access, and the composite-key row lock is mandatory.
 *
 * @param mixed $employee_id
 * @param mixed $leave_id
 * @param mixed $fiscal_year
 * @return string|false
 */
function get_payroll_reversal_leave_balance_state_sql($employee_id, $leave_id, $fiscal_year) {
    $employee_id = (string)$employee_id;
    $leave_id = (int)$leave_id;
    $fiscal_year = (int)$fiscal_year;
    if ($employee_id === '' || $leave_id === 0 || $fiscal_year === 0)
        return false;

    return "SELECT balance_id,employee_id,leave_id,fiscal_year,entitled,"
        ."carried_forward,taken,pending,adjusted"
        ." FROM ".TB_PREF."leave_balances"
        ." WHERE employee_id = ".db_escape($employee_id)
        ." AND leave_id = ".db_escape($leave_id)
        ." AND fiscal_year = ".db_escape($fiscal_year)
        ." FOR UPDATE";
}

/**
 * Build the exact locked employee-loan reversal-state projection.
 *
 * Persisted callers supply positive loan identifiers from immutable reversal
 * evidence. Fixed negative identifiers remain available only to rollback-
 * scoped fixtures. The exact three-field row is hashed before and after
 * posting/reversal operations, so wildcard expansion is prohibited. Identifier
 * zero fails closed before database access and the row lock is mandatory.
 *
 * @param mixed $loan_id
 * @return string|false
 */
function get_payroll_reversal_employee_loan_state_sql($loan_id) {
    $loan_id = (int)$loan_id;
    if ($loan_id === 0)
        return false;

    return "SELECT loan_id,outstanding_amount,status"
        ." FROM ".TB_PREF."employee_loans"
        ." WHERE loan_id = ".db_escape($loan_id)
        ." FOR UPDATE";
}

/**
 * Build the exact locked loan-repayment reversal-state projection.
 *
 * Persisted callers supply positive repayment identifiers from immutable
 * reversal evidence. Fixed negative identifiers remain available only to
 * rollback-scoped fixtures. The exact six-field row is hashed before and after
 * posting/reversal operations, so wildcard expansion is prohibited. Identifier
 * zero fails closed before database access and the row lock is mandatory.
 *
 * @param mixed $repayment_id
 * @return string|false
 */
function get_payroll_reversal_repayment_state_sql($repayment_id) {
    $repayment_id = (int)$repayment_id;
    if ($repayment_id === 0)
        return false;

    return "SELECT repayment_id,loan_id,paid_amount,paid_date,payslip_id,status"
        ." FROM ".TB_PREF."loan_repayments"
        ." WHERE repayment_id = ".db_escape($repayment_id)
        ." FOR UPDATE";
}

/**
 * Build the canonical payroll reversal GL-line projection.
 *
 * Persisted callers supply positive ST_PAYSLIP transaction numbers. Fixed
 * negative identifiers remain available only to rollback-scoped fixtures.
 * The projection is intentionally exact because linked reversal creation and
 * verification hash these values. Identifier zero fails closed before any
 * database access, and locking remains caller-selectable.
 *
 * @param mixed $trans_no
 * @param bool $lock
 * @return string|false
 */
function get_payroll_reversal_gl_lines_sql($trans_no, $lock=true) {
    $trans_no = (int)$trans_no;
    if ($trans_no === 0)
        return false;

    return "SELECT counter,tran_date,account,memo_,amount,dimension_id,dimension2_id,"
        ."person_type_id,HEX(person_id) AS person_id_hex,"
        ."CAST(custom_data AS CHAR) AS custom_data"
        ." FROM ".TB_PREF."gl_trans"
        ." WHERE type = ".db_escape(ST_PAYSLIP)
        ." AND type_no = ".db_escape($trans_no)
        ." AND ABS(amount) > 0.000001"
        ." ORDER BY counter".($lock ? ' FOR UPDATE' : '');
}

/**
 * Build the exact payroll-GL side-effect existence projection.
 *
 * The void/reopen guard consumes only one immutable GL row identity. Persisted
 * callers supply positive transaction numbers; negative numbers remain
 * available to rollback-only fixtures. Locking remains caller-selectable so
 * read-only inspection and command-owned serialization preserve their current
 * behavior. Identifier zero fails closed before database access.
 *
 * @param mixed $gl_trans_no
 * @param bool $lock
 * @return string|false
 */
function get_payroll_gl_side_effect_exists_sql($gl_trans_no, $lock=true) {
    $gl_trans_no = (int)$gl_trans_no;
    if ($gl_trans_no === 0)
        return false;

    return "SELECT counter FROM ".TB_PREF."gl_trans"
        ." WHERE type = ".db_escape(ST_PAYSLIP)
        ." AND type_no = ".db_escape($gl_trans_no)
        .($lock ? ' FOR UPDATE' : '');
}

/**
 * Build the exact payroll void-evidence existence projection.
 *
 * The void/reopen guard needs only the immutable transaction identifier from
 * the core void register. Persisted callers supply positive transaction
 * numbers; fixed-negative identifiers remain available to rollback-only
 * fixtures. Locking remains caller-selectable so command-owned serialization
 * and read-only inspection retain their existing behavior. Identifier zero
 * fails closed before database access.
 *
 * @param mixed $gl_trans_no
 * @param bool $lock
 * @return string|false
 */
function get_payroll_void_side_effect_exists_sql($gl_trans_no, $lock=true) {
    $gl_trans_no = (int)$gl_trans_no;
    if ($gl_trans_no === 0)
        return false;

    return "SELECT id FROM ".TB_PREF."voided"
        ." WHERE type = ".db_escape(ST_PAYSLIP)
        ." AND id = ".db_escape($gl_trans_no)
        .($lock ? ' FOR UPDATE' : '');
}

/**
 * Build the exact payroll payment-bank side-effect existence projection.
 *
 * The void/reopen guard consumes only the immutable bank transaction identity
 * for an already-linked payment. Persisted callers supply positive transaction
 * numbers; fixed-negative identifiers remain available to rollback-only
 * fixtures. Locking remains caller-selectable so command-owned serialization
 * and read-only inspection retain their existing behavior. Identifier zero
 * fails closed before database access.
 *
 * @param mixed $payment_trans_no
 * @param bool $lock
 * @return string|false
 */
function get_payroll_payment_bank_side_effect_exists_sql($payment_trans_no, $lock=true) {
    $payment_trans_no = (int)$payment_trans_no;
    if ($payment_trans_no === 0)
        return false;

    return "SELECT id FROM ".TB_PREF."bank_trans"
        ." WHERE type = ".db_escape(ST_BANKPAYMENT)
        ." AND trans_no = ".db_escape($payment_trans_no)
        .($lock ? ' FOR UPDATE' : '');
}

/**
 * Build the exact payroll payment-GL side-effect existence projection.
 *
 * The void/reopen guard consumes only the immutable GL row identity for an
 * already-linked bank payment. Persisted callers supply positive transaction
 * numbers; fixed-negative identifiers remain available to rollback-only
 * fixtures. Locking remains caller-selectable so command-owned serialization
 * and read-only inspection retain their existing behavior. Identifier zero
 * fails closed before database access.
 *
 * @param mixed $payment_trans_no
 * @param bool $lock
 * @return string|false
 */
function get_payroll_payment_gl_side_effect_exists_sql($payment_trans_no, $lock=true) {
    $payment_trans_no = (int)$payment_trans_no;
    if ($payment_trans_no === 0)
        return false;

    return "SELECT counter FROM ".TB_PREF."gl_trans"
        ." WHERE type = ".db_escape(ST_BANKPAYMENT)
        ." AND type_no = ".db_escape($payment_trans_no)
        .($lock ? ' FOR UPDATE' : '');
}

/**
 * Build the exact payroll payment-void side-effect existence projection.
 *
 * The void/reopen guard consumes only the immutable transaction identifier
 * from the core void register for an already-linked bank payment. Persisted
 * callers supply positive transaction numbers; fixed-negative identifiers
 * remain available to rollback-only fixtures. Locking remains caller-selectable
 * so command-owned serialization and read-only inspection preserve their
 * current behavior. Identifier zero fails closed before database access.
 *
 * @param mixed $payment_trans_no
 * @param bool $lock
 * @return string|false
 */
function get_payroll_payment_void_side_effect_exists_sql($payment_trans_no, $lock=true) {
    $payment_trans_no = (int)$payment_trans_no;
    if ($payment_trans_no === 0)
        return false;

    return "SELECT id FROM ".TB_PREF."voided"
        ." WHERE type = ".db_escape(ST_BANKPAYMENT)
        ." AND id = ".db_escape($payment_trans_no)
        .($lock ? ' FOR UPDATE' : '');
}

/**
 * Inspect one locked payslip for any payroll-GL or payment side-effect link.
 *
 * Positive legacy transaction links are themselves treated as protected
 * evidence because partial legacy writes can leave references/comments/audit
 * or external bank effects even when a later row lookup is incomplete.
 * Exact GL, bank and core-void identities are also read (and locked when
 * requested) to serialize this guard with posting, payment and generic voids.
 *
 * @param array $payslip
 * @param bool $lock
 * @return array
 */
function payroll_payslip_financial_side_effects($payslip, $lock=true) {
    $result = array(
        'safe' => false,
        'code' => 'invalid_payslip',
        'payslip_id' => 0,
        'payroll_period_id' => 0,
        'evidence' => array(),
    );
    if (!is_array($payslip) || empty($payslip['payslip_id']))
        return $result;

    $result['payslip_id'] = (int)$payslip['payslip_id'];
    $result['payroll_period_id'] = isset($payslip['payroll_period_id'])
        ? (int)$payslip['payroll_period_id'] : 0;

    $payslip_status = isset($payslip['status']) ? (int)$payslip['status'] : 0;
    if ($payslip_status >= 2 && $payslip_status < 6)
        $result['evidence'][] = 'payslip_status_financial';

    $payroll_trans_no = isset($payslip['gl_trans_no'])
        ? (int)$payslip['gl_trans_no'] : 0;
    if ($payroll_trans_no <= 0 && isset($payslip['trans_no']))
        $payroll_trans_no = (int)$payslip['trans_no'];

    if ($payroll_trans_no > 0) {
        $result['evidence'][] = 'payroll_gl_link';
        $gl_exists_sql = get_payroll_gl_side_effect_exists_sql(
            $payroll_trans_no,
            $lock
        );
        if ($gl_exists_sql === false) {
            $result['code'] = 'payroll_gl_evidence_unavailable';
            return $result;
        }
        $void_exists_sql = get_payroll_void_side_effect_exists_sql(
            $payroll_trans_no,
            $lock
        );
        if ($void_exists_sql === false) {
            $result['code'] = 'payroll_gl_evidence_unavailable';
            return $result;
        }
        $queries = array(
            array(
                $gl_exists_sql,
                'could not inspect linked payroll GL rows'
            ),
            array(
                $void_exists_sql,
                'could not inspect linked payroll void evidence'
            ),
        );
        foreach ($queries as $query) {
            $exists = payroll_side_effect_row_exists($query[0], $query[1]);
            if ($exists === null) {
                $result['code'] = 'payroll_gl_evidence_unavailable';
                return $result;
            }
            if ($exists)
                $result['evidence'][] = strpos($query[0], 'gl_trans') !== false
                    ? 'payroll_gl_rows' : 'payroll_void_record';
        }
    }

    $payment_trans_no = isset($payslip['payment_trans_no'])
        ? (int)$payslip['payment_trans_no'] : 0;
    if ($payment_trans_no > 0) {
        $result['evidence'][] = 'payment_link';
        $payment_bank_exists_sql = get_payroll_payment_bank_side_effect_exists_sql(
            $payment_trans_no,
            $lock
        );
        if ($payment_bank_exists_sql === false) {
            $result['code'] = 'payment_evidence_unavailable';
            return $result;
        }
        $payment_gl_exists_sql = get_payroll_payment_gl_side_effect_exists_sql(
            $payment_trans_no,
            $lock
        );
        if ($payment_gl_exists_sql === false) {
            $result['code'] = 'payment_evidence_unavailable';
            return $result;
        }
        $payment_void_exists_sql = get_payroll_payment_void_side_effect_exists_sql(
            $payment_trans_no,
            $lock
        );
        if ($payment_void_exists_sql === false) {
            $result['code'] = 'payment_evidence_unavailable';
            return $result;
        }
        $queries = array(
            array(
                $payment_bank_exists_sql,
                'could not inspect linked payroll bank rows',
                'payment_bank_rows'
            ),
            array(
                $payment_gl_exists_sql,
                'could not inspect linked payroll payment GL rows',
                'payment_gl_rows'
            ),
            array(
                $payment_void_exists_sql,
                'could not inspect linked payroll payment void evidence',
                'payment_void_record'
            ),
        );
        foreach ($queries as $query) {
            $exists = payroll_side_effect_row_exists($query[0], $query[1]);
            if ($exists === null) {
                $result['code'] = 'payment_evidence_unavailable';
                return $result;
            }
            if ($exists)
                $result['evidence'][] = $query[2];
        }
    }

    if (!empty($result['evidence'])) {
        if (in_array('payment_link', $result['evidence'], true))
            $result['code'] = 'linked_payment_side_effect';
        elseif (in_array('payroll_gl_link', $result['evidence'], true))
            $result['code'] = 'linked_payroll_gl_side_effect';
        else
            $result['code'] = 'payslip_status_financial';
        return $result;
    }

    $result['safe'] = true;
    $result['code'] = 'side_effect_free';
    return $result;
}

/**
 * Build the exact linked-payslip projection used by period side-effect guards.
 *
 * Persisted callers provide positive period identifiers. Negative identifiers
 * remain reserved for rollback-only fixtures. The optional legacy
 * `gl_trans_no` field is included only when the deployed payslip schema exposes
 * it, preserving existing financial-evidence detection without wildcard reads.
 *
 * @param mixed $period_id
 * @param string $table_name
 * @param bool $lock
 * @param bool $has_gl_trans_no
 * @return string|false
 */
function get_payroll_period_side_effect_payslips_sql(
    $period_id,
    $table_name,
    $lock=true,
    $has_gl_trans_no=false
) {
    $period_id = (int)$period_id;
    $table_name = (string)$table_name;
    if ($period_id === 0 || $table_name !== 'payslips')
        return false;

    $fields = array(
        'payslip_id', 'payroll_period_id', 'trans_no', 'status',
        'payment_trans_no'
    );
    if ($has_gl_trans_no)
        $fields[] = 'gl_trans_no';

    return "SELECT ".implode(', ', $fields)." FROM ".TB_PREF.$table_name
        ." WHERE payroll_period_id = ".db_escape($period_id)
        .($lock ? ' FOR UPDATE' : '');
}

/**
 * Inspect one locked payroll period and every linked payslip for effects.
 *
 * @param array $period
 * @param bool $lock
 * @return array
 */
function payroll_period_financial_side_effects($period, $lock=true) {
    $result = array(
        'safe' => false,
        'code' => 'invalid_payroll_period',
        'payroll_period_id' => 0,
        'payslip_id' => 0,
        'payslip_count' => 0,
        'evidence' => array(),
    );
    if (!is_array($period) || empty($period['period_id']))
        return $result;

    $period_id = (int)$period['period_id'];
    $result['payroll_period_id'] = $period_id;
    $period_status = isset($period['status']) ? (int)$period['status'] : 0;
    if ($period_status >= 3 && $period_status < 6) {
        $result['code'] = 'period_status_financial';
        $result['evidence'][] = 'period_status_financial';
        return $result;
    }
    if (!empty($period['gl_trans_no'])) {
        $result['code'] = 'linked_period_gl_side_effect';
        $result['evidence'][] = 'period_gl_link';
        return $result;
    }

    $payslip_table = payroll_payslip_header_table();
    if (!$payslip_table) {
        $result['code'] = 'payslip_repository_unavailable';
        return $result;
    }

    $sql = get_payroll_period_side_effect_payslips_sql(
        $period_id,
        $payslip_table,
        $lock,
        payroll_payslip_has_column($payslip_table, 'gl_trans_no')
    );
    if ($sql === false) {
        $result['code'] = 'payslip_evidence_unavailable';
        return $result;
    }
    $payslips = db_query($sql, 'could not inspect linked payroll-period payslips');
    if (!$payslips) {
        $result['code'] = 'payslip_evidence_unavailable';
        return $result;
    }

    while ($payslip = db_fetch_assoc($payslips)) {
        $result['payslip_count']++;
        $payslip_result = payroll_payslip_financial_side_effects($payslip, $lock);
        if (!$payslip_result['safe']) {
            $result['code'] = $payslip_result['code'];
            $result['payslip_id'] = $payslip_result['payslip_id'];
            $result['evidence'] = $payslip_result['evidence'];
            return $result;
        }
    }

    $result['safe'] = true;
    $result['code'] = 'side_effect_free';
    return $result;
}

/**
 * Append immutable evidence for an allowed side-effect-free legacy transition.
 *
 * @param string $entity_type
 * @param array $entity_key
 * @param string $action
 * @param string $reason_code
 * @param array $before
 * @param array $after
 * @param array $changed_fields
 * @param string $mutation_kind
 * @param array $related_changes Minimized non-sensitive linked-row metadata.
 * @return bool
 */
function payroll_append_side_effect_free_transition_audit(
    $entity_type,
    $entity_key,
    $action,
    $reason_code,
    $before,
    $after,
    $changed_fields,
    $mutation_kind,
    $related_changes=array()
) {
    global $path_to_root;

    include_once($path_to_root.'/includes/db/domain_audit_db.inc');
    if (!function_exists('append_domain_audit_event'))
        return false;

    // This transition changes lifecycle flags only. Do not duplicate pay,
    // employee, bank, reference, note, or other restricted values into audit.
    $audit_fields = array_unique(array_merge(array_keys($entity_key), $changed_fields));
    $before_audit = array();
    $after_audit = array();
    foreach ($audit_fields as $field) {
        $before_audit[$field] = is_array($before) && array_key_exists($field, $before)
            ? $before[$field] : null;
        $after_audit[$field] = is_array($after) && array_key_exists($field, $after)
            ? $after[$field] : null;
    }

    $restricted_diff = array(
        'changed_fields' => array_values($changed_fields),
        'mutation_kind' => $mutation_kind,
        'financial_side_effects_present' => false,
        'restricted_values_stored' => false,
    );
    if (!empty($related_changes))
        $restricted_diff['related_changes'] = $related_changes;

    $audit = append_domain_audit_event(array(
        'legal_entity_id' => 0,
        'entity_type' => $entity_type,
        'entity_key' => $entity_key,
        'action' => $action,
        'reason_code' => $reason_code,
        'before' => $before_audit,
        'after' => $after_audit,
        'restricted_diff' => $restricted_diff,
    ));

    return $audit !== false;
}

/**
 * Reopen an Approved/Posted payroll period only when no linked effects exist.
 *
 * @param int $period_id
 * @return array
 */
function reopen_payroll_period_without_financial_side_effects($period_id) {
    $period_id = (int)$period_id;
    $result = array(
        'ok' => false,
        'code' => 'invalid_payroll_period',
        'message' => _('The payroll period could not be reopened.'),
    );
    if ($period_id <= 0)
        return $result;

    begin_transaction();
    $period = get_payroll_period_for_update($period_id);
    if (!$period) {
        cancel_transaction();
        $result['code'] = 'payroll_period_not_found';
        return $result;
    }

    $status = isset($period['status']) ? (int)$period['status'] : -1;
    if (!in_array($status, array(2, 3), true)) {
        cancel_transaction();
        $result['code'] = 'payroll_period_status_not_reopenable';
        $result['message'] = _('Only Approved or Posted payroll periods can use this guarded reopen action.');
        return $result;
    }

    $side_effects = payroll_period_financial_side_effects($period, true);
    if (!$side_effects['safe']) {
        payroll_side_effect_control_log_denial(
            'payroll_period_reopen',
            $side_effects['code'],
            $period_id,
            isset($side_effects['payslip_id']) ? $side_effects['payslip_id'] : 0,
            'payroll_approval'
        );
        cancel_transaction();
        $result['code'] = $side_effects['code'];
        $result['message'] = _(
            'This payroll period has linked payroll or payment effects and cannot be reopened. '
            .'Use a linked reversal after that workflow is implemented.'
        );
        return $result;
    }

    $sql = "UPDATE ".TB_PREF."payroll_periods SET status = 0,"
        ." approved_by = NULL, approval_date = NULL"
        ." WHERE period_id = ".db_escape($period_id)
        ." AND status = ".db_escape($status);
    if (!db_query($sql, 'could not reopen side-effect-free payroll period')
        || db_num_affected_rows() !== 1
    ) {
        cancel_transaction();
        $result['code'] = 'payroll_period_transition_conflict';
        return $result;
    }
    $after = get_payroll_period_for_update($period_id);
    if (!$after || (int)$after['status'] !== 0) {
        cancel_transaction();
        $result['code'] = 'payroll_period_transition_not_persisted';
        return $result;
    }

    if (!payroll_append_side_effect_free_transition_audit(
        'payroll_period',
        array('period_id' => $period_id),
        'payroll_period.reopened',
        'legacy_side_effect_free_reopen',
        $period,
        $after,
        array('status', 'approved_by', 'approval_date'),
        'reopened'
    )) {
        error_log('PAY-AUD-001: payroll-period side-effect-free reopen audit append failed');
        cancel_transaction();
        $result['code'] = 'audit_append_failed';
        return $result;
    }

    commit_transaction();
    return array(
        'ok' => true,
        'code' => 'payroll_period_reopened',
        'message' => _('Payroll period has been reopened as Draft.'),
    );
}

/**
 * Build the backward-compatible payroll-period count query.
 *
 * @param int|null $status
 * @return string
 */
function count_payroll_periods_sql($status=null) {
    $sql = "SELECT COUNT(*) FROM ".TB_PREF."payroll_periods";
    if ($status !== null && $status !== '')
        $sql .= " WHERE status = ".db_escape((int)$status);
    return $sql;
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

    $result = db_query(
        count_payroll_periods_sql($status),
        'could not count payroll periods'
    );
    $row = db_fetch_row($result);
    return $row ? (int)$row[0] : 0;
}

/**
 * Build the backward-compatible payroll-period list projection.
 *
 * The explicit column list preserves the complete legacy row shape while
 * preventing wildcard expansion if the table gains new fields. Filtering,
 * ordering, pagination and integer coercion intentionally retain the existing
 * public helper semantics.
 *
 * @param int|null $status
 * @param int|null $limit
 * @param int $offset
 * @return string
 */
function get_payroll_periods_sql($status=null, $limit=null, $offset=0) {
    $sql = "SELECT period_id, period_name, from_date, to_date, pay_date, "
        ."department_id, status, total_gross, total_deductions, total_net, "
        ."total_employer_cost, approved_by, approval_date, gl_trans_no, "
        ."created_by, created_date, notes FROM ".TB_PREF."payroll_periods";
    if ($status !== null && $status !== '')
        $sql .= " WHERE status = ".db_escape((int)$status);
    $sql .= " ORDER BY from_date DESC, period_id DESC";

    $offset = max((int)$offset, 0);
    if ($limit !== null && (int)$limit > 0)
        $sql .= " LIMIT ".(int)$limit." OFFSET ".$offset;

    return $sql;
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

    return db_query(
        get_payroll_periods_sql($status, $limit, $offset),
        'could not get payroll periods'
    );
}

/**
 * Build the exact payroll-period payment-status summary projection.
 *
 * Persisted callers provide positive period identifiers. Negative identifiers
 * remain reserved for rollback-only fixtures. The aggregate preserves the
 * existing paid/void status rules and returns only the two values consumed by
 * payment-status synchronization.
 *
 * @param mixed $period_id
 * @param string $table_name
 * @return string|false
 */
function get_payroll_period_payment_status_summary_sql($period_id, $table_name) {
    $period_id = (int)$period_id;
    $table_name = (string)$table_name;
    if ($period_id === 0 || $table_name !== 'payslips')
        return false;

    return "SELECT COUNT(*) AS total_count, "
        ."SUM(CASE WHEN COALESCE(status, 0) >= 3 AND COALESCE(status, 0) < 6 "
        ."THEN 1 ELSE 0 END) AS paid_count FROM ".TB_PREF.$table_name
        ." WHERE payroll_period_id = ".db_escape($period_id)
        ." AND COALESCE(status, 0) <> 6";
}

/**
 * Update payroll period status.
 *
 * @param int $period_id
 * @param int $status
 * @param int|null $expected_status Optional current status required for update
 * @return bool
 */
function update_payroll_period_status(
    $period_id,
    $status,
    $expected_status=null
) {
    if (!payroll_table_exists('payroll_periods'))
        return false;

    $status = (int)$status;

    // Reopen and void are controlled commands, not generic status updates.
    // This blocks extension/direct-helper flag-only bypasses. The guarded
    // commands below use locked compare-and-set SQL after side-effect checks.
    if ($status === 6)
        return false;

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
	} elseif ($status < 2) {
		$set[] = "approved_by = NULL";
		$set[] = "approval_date = NULL";
    }

    $sql = "UPDATE ".TB_PREF."payroll_periods SET ".implode(', ', $set).
        " WHERE period_id = ".db_escape((int)$period_id);
    if ($expected_status !== null)
        $sql .= " AND status = ".db_escape((int)$expected_status);
    if ($status < 2)
        $sql .= " AND status < 2";

    $result = db_query($sql, 'could not update payroll period status');
    if (!$result)
        return false;

    return $expected_status === null || db_num_affected_rows() === 1;
}

/**
 * Promote a payroll period to paid when all linked payslips are paid.
 *
 * @param int $period_id
 * @return bool
 */
function sync_payroll_period_payment_status($period_id) {
    $period_id = (int)$period_id;
    if ($period_id <= 0 || !payroll_table_exists('payroll_periods'))
        return false;

    $period = get_payroll_period($period_id);
    if (!$period)
        return false;

    $current_status = isset($period['status']) ? (int)$period['status'] : 0;
    if (!in_array($current_status, array(2, 3, 4), true))
        return true;

    $payslip_table = payroll_payslip_header_table();
    if (!$payslip_table)
        return true;

    $sql = get_payroll_period_payment_status_summary_sql(
        $period_id,
        $payslip_table
    );
    if ($sql === false)
        return false;
    $result = db_query($sql, 'could not summarize payroll period payment status');
    $row = db_fetch_assoc($result);

    $total_count = $row ? (int)$row['total_count'] : 0;
    $paid_count = $row ? (int)$row['paid_count'] : 0;

    if ($total_count > 0 && $paid_count >= $total_count && $current_status !== 4)
        return update_payroll_period_status($period_id, 4);

    return true;
}

/**
 * Mark a payroll period as paid and synchronize linked payslips.
 *
 * @param int $period_id
 * @return bool
 */
function mark_payroll_period_paid($period_id) {
    $period_id = (int)$period_id;
    if ($period_id <= 0)
        return false;

    $period = get_payroll_period($period_id);
    if (!$period)
        return false;

    $current_status = isset($period['status']) ? (int)$period['status'] : 0;
    if ($current_status === 4)
        return true;
    if (!in_array($current_status, array(2, 3), true))
        return false;

    begin_transaction();

    $ok = update_payroll_period_status($period_id, 4);
    $payslip_table = payroll_payslip_header_table();
    if ($ok && $payslip_table) {
        $sql = "UPDATE ".TB_PREF.$payslip_table." SET status = 3"
            ." WHERE payroll_period_id = ".db_escape($period_id)
            ." AND COALESCE(status, 0) <> 6";
        $ok = (bool)db_query($sql, 'could not mark linked payslips as paid');
    }

    if (!$ok) {
        cancel_transaction();
        return false;
    }

    commit_transaction();
    return true;
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
        $sum_sql = "SELECT
            SUM(IFNULL(gross_salary, 0)) as total_gross,
            SUM(IFNULL(total_deductions, 0)) as total_deductions,
            SUM(IFNULL(net_salary, IFNULL(payable_amount, 0))) as total_net,
            SUM(IFNULL(total_cost, IFNULL(employer_contributions, 0) + IFNULL(net_salary, IFNULL(payable_amount, 0)))) as total_employer_cost
            FROM ".TB_PREF.$payslip_table." WHERE payroll_period_id = ".db_escape((int)$period_id);
		if (payroll_payslip_has_column($payslip_table, 'status'))
			$sum_sql .= " AND COALESCE(status, 0) <> 6";
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
    $period_id = (int)$period_id;
    if ($period_id <= 0 || !payroll_table_exists('payroll_periods'))
        return false;

    begin_transaction();
    $period = get_payroll_period_for_update($period_id);
    if (!$period) {
        cancel_transaction();
        return false;
    }
    if ((int)$period['status'] === 6) {
        commit_transaction();
        return true;
    }

    $side_effects = payroll_period_financial_side_effects($period, true);
    if (!$side_effects['safe']) {
        payroll_side_effect_control_log_denial(
            'payroll_period_void',
            $side_effects['code'],
            $period_id,
            isset($side_effects['payslip_id']) ? $side_effects['payslip_id'] : 0,
            'payroll_db'
        );
        cancel_transaction();
        return false;
    }

    $current_status = isset($period['status']) ? (int)$period['status'] : 0;
    $sql = "UPDATE ".TB_PREF."payroll_periods SET status = 6"
        ." WHERE period_id = ".db_escape($period_id)
        ." AND status = ".db_escape($current_status);
    $ok = (bool)db_query($sql, 'could not void side-effect-free payroll period')
        && db_num_affected_rows() === 1;
    if (!$ok) {
        cancel_transaction();
        return false;
    }

    $payslip_table = payroll_payslip_header_table();
    $linked_payslip_changed_fields = array();
    if ($payslip_table) {
        $set_parts = array();
        if (payroll_payslip_has_column($payslip_table, 'status')) {
            $set_parts[] = "status = 6";
            $linked_payslip_changed_fields[] = 'status';
        }
        if (payroll_payslip_has_column($payslip_table, 'voided')) {
            $set_parts[] = "voided = 1";
            $linked_payslip_changed_fields[] = 'voided';
        }
        if (payroll_payslip_has_column($payslip_table, 'is_voided')) {
            $set_parts[] = "is_voided = 1";
            $linked_payslip_changed_fields[] = 'is_voided';
        }
        if (payroll_payslip_has_column($payslip_table, 'inactive')) {
            $set_parts[] = "inactive = 1";
            $linked_payslip_changed_fields[] = 'inactive';
        }
        if (payroll_payslip_has_column($payslip_table, 'voided_date')) {
            $set_parts[] = "voided_date = ".db_escape(date2sql(Today()));
            $linked_payslip_changed_fields[] = 'voided_date';
        }

        if ((int)$side_effects['payslip_count'] > 0 && empty($set_parts)) {
            cancel_transaction();
            return false;
        }
        if (!empty($set_parts) && (int)$side_effects['payslip_count'] > 0) {
            $sql = "UPDATE ".TB_PREF.$payslip_table." SET ".implode(', ', $set_parts)
                ." WHERE payroll_period_id = ".db_escape($period_id);
            $ok = (bool)db_query($sql, 'could not void side-effect-free linked payslips');
        }
    }

    $related_changes = array();
    if ((int)$side_effects['payslip_count'] > 0) {
        $related_changes = array(
            'entity_type' => 'payslip',
            'row_count' => (int)$side_effects['payslip_count'],
            'changed_fields' => $linked_payslip_changed_fields,
        );
    }

    $after = $ok ? get_payroll_period_for_update($period_id) : false;
    if (!$after || (int)$after['status'] !== 6
        || !payroll_append_side_effect_free_transition_audit(
            'payroll_period',
            array('period_id' => $period_id),
            'payroll_period.voided',
            'legacy_side_effect_free_void',
            $period,
            $after,
            array('status'),
            'voided',
            $related_changes
        )
    ) {
        error_log('PAY-AUD-001: side-effect-free payroll-period void audit append failed');
        cancel_transaction();
        return false;
    }

    commit_transaction();
    return true;
}

/**
 * Check whether the immutable linked-reversal evidence repository is present.
 *
 * @return bool
 */
function payroll_reversal_evidence_table_ready() {
    return payroll_table_exists('payroll_reversal_evidence');
}

/**
 * Insert one immutable posting/reversal evidence row.
 *
 * The database table denies UPDATE and DELETE through triggers. This helper
 * additionally validates the bounded evidence vocabulary and all identifiers
 * before writing. It must be called from the surrounding payroll transaction.
 *
 * @param array $evidence
 * @return bool
 */
function insert_payroll_reversal_evidence($evidence) {
    global $transaction_level;

    $types = array(
        'posting',
        'loan_repayment',
        'employee_loan',
        'leave_balance',
        'gl_reversal',
    );
    if (empty($transaction_level)
        || !payroll_reversal_evidence_table_ready()
        || !is_array($evidence)
    ) {
        return false;
    }

    $period_id = isset($evidence['payroll_period_id'])
        ? (int)$evidence['payroll_period_id'] : 0;
    $payslip_id = isset($evidence['payslip_id'])
        ? (int)$evidence['payslip_id'] : 0;
    $original_trans_no = isset($evidence['original_trans_no'])
        ? (int)$evidence['original_trans_no'] : 0;
    $type = isset($evidence['evidence_type'])
        ? (string)$evidence['evidence_type'] : '';
    $target_id = isset($evidence['target_id'])
        ? (int)$evidence['target_id'] : 0;
    $linked_trans_no = isset($evidence['linked_trans_no'])
        && $evidence['linked_trans_no'] !== null
        ? (int)$evidence['linked_trans_no'] : null;
    $before_state = isset($evidence['before_state'])
        && $evidence['before_state'] !== null
        ? (string)$evidence['before_state'] : null;
    $after_state_hash = isset($evidence['after_state_hash'])
        ? strtolower((string)$evidence['after_state_hash']) : '';
    $reason_code = isset($evidence['reason_code'])
        && $evidence['reason_code'] !== null
        ? strtoupper((string)$evidence['reason_code']) : null;
    $actor_id = isset($evidence['actor_id'])
        ? (int)$evidence['actor_id'] : payroll_side_effect_control_actor_id();
    $effective_date = isset($evidence['effective_date'])
        ? (string)$evidence['effective_date'] : '';
    if (!preg_match('/^\d{4}-\d{2}-\d{2}$/', $effective_date))
        $effective_date = date2sql($effective_date);

    $decoded_before = $before_state === null ? null : json_decode($before_state, true);
    $controlled_reasons = array(
        'POSTING_ERROR',
        'CONFIGURATION_ERROR',
        'APPROVAL_ERROR',
        'DUPLICATE_RUN',
        'OTHER_CONTROLLED',
    );
    $shape_valid = false;
    if ($type === 'posting') {
        $shape_valid = $target_id === 0
            && $linked_trans_no === null
            && $before_state === null
            && $reason_code === null;
    } elseif ($type === 'gl_reversal') {
        $shape_valid = $target_id > 0
            && $linked_trans_no !== null
            && is_array($decoded_before)
            && in_array($reason_code, $controlled_reasons, true);
    } elseif (in_array($type, array('loan_repayment', 'employee_loan', 'leave_balance'), true)) {
        $shape_valid = $target_id > 0
            && $linked_trans_no === null
            && is_array($decoded_before)
            && $reason_code === null;
    }

    if ($period_id <= 0
        || $payslip_id <= 0
        || $original_trans_no <= 0
        || !in_array($type, $types, true)
        || $target_id < 0
        || ($linked_trans_no !== null && $linked_trans_no <= 0)
        || !preg_match('/^[a-f0-9]{64}$/', $after_state_hash)
        || !preg_match('/^\d{4}-\d{2}-\d{2}$/', $effective_date)
        || ($reason_code !== null && !preg_match('/^[A-Z0-9_-]{3,40}$/', $reason_code))
        || !$shape_valid
    ) {
        return false;
    }

    $sql = "INSERT INTO ".TB_PREF."payroll_reversal_evidence ("
        ."payroll_period_id,payslip_id,original_trans_no,evidence_type,target_id,"
        ."linked_trans_no,before_state,after_state_hash,reason_code,actor_id,effective_date"
        .") VALUES ("
        .db_escape($period_id).","
        .db_escape($payslip_id).","
        .db_escape($original_trans_no).","
        .db_escape($type).","
        .db_escape($target_id).","
        .($linked_trans_no === null ? 'NULL' : db_escape($linked_trans_no)).","
        .($before_state === null ? 'NULL' : db_escape($before_state)).","
        .db_escape($after_state_hash).","
        .($reason_code === null ? 'NULL' : db_escape($reason_code)).","
        .($actor_id > 0 ? db_escape($actor_id) : 'NULL').","
        .db_escape($effective_date).")";
    $ok = db_query($sql, 'could not append immutable payroll reversal evidence');
    return $ok && db_num_affected_rows() === 1;
}

/**
 * Build the exact immutable payroll-reversal evidence projection.
 *
 * Persisted callers provide positive period identifiers. Fixed-negative
 * identifiers remain available to rollback-only fixtures through this builder,
 * while the public reader retains its positive-ID guard. The optional payslip
 * and evidence-type filters preserve existing semantics, deterministic ordering
 * is unchanged, and command-owned callers may request a terminal row lock.
 * Identifier zero fails closed before database access.
 *
 * @param mixed $period_id
 * @param mixed $payslip_id Optional payslip filter
 * @param string $evidence_type Optional type filter
 * @param bool $lock
 * @return string|false
 */
function get_payroll_reversal_evidence_rows_sql(
    $period_id,
    $payslip_id=0,
    $evidence_type='',
    $lock=false
) {
    $period_id = (int)$period_id;
    $payslip_id = (int)$payslip_id;
    $evidence_type = (string)$evidence_type;
    if ($period_id === 0)
        return false;

    $sql = "SELECT evidence_id,payroll_period_id,payslip_id,original_trans_no,"
        ."evidence_type,target_id,linked_trans_no,before_state,after_state_hash,"
        ."reason_code,actor_id,effective_date,created_at"
        ." FROM ".TB_PREF."payroll_reversal_evidence"
        ." WHERE payroll_period_id = ".db_escape($period_id);
    if ($payslip_id !== 0)
        $sql .= " AND payslip_id = ".db_escape($payslip_id);
    if ($evidence_type !== '')
        $sql .= " AND evidence_type = ".db_escape($evidence_type);
    $sql .= " ORDER BY payslip_id,evidence_type,target_id,evidence_id";
    if ($lock)
        $sql .= " FOR UPDATE";

    return $sql;
}

/**
 * Read immutable reversal evidence in deterministic order.
 *
 * @param int $period_id
 * @param int $payslip_id Optional payslip filter
 * @param string $evidence_type Optional type filter
 * @param bool $lock
 * @return array|false
 */
function get_payroll_reversal_evidence_rows(
    $period_id,
    $payslip_id=0,
    $evidence_type='',
    $lock=false
) {
    $period_id = (int)$period_id;
    $payslip_id = (int)$payslip_id;
    if ($period_id <= 0 || !payroll_reversal_evidence_table_ready())
        return false;

    $sql = get_payroll_reversal_evidence_rows_sql(
        $period_id,
        $payslip_id,
        $evidence_type,
        $lock
    );
    if ($sql === false)
        return false;

    return payroll_approval_binding_fetch_rows(
        $sql,
        'could not read immutable payroll reversal evidence'
    );
}

/**
 * Compare-and-set one Posted payslip to the terminal reversed state.
 *
 * @param int $payslip_id
 * @return bool
 */
function update_payslip_status_for_linked_reversal($payslip_id) {
    $table = payroll_payslip_header_table();
    if (!$table || (int)$payslip_id <= 0)
        return false;

    $sql = "UPDATE ".TB_PREF.$table." SET status = 6"
        ." WHERE payslip_id = ".db_escape((int)$payslip_id)
        ." AND status = 2 AND payment_trans_no IS NULL";
    $ok = db_query($sql, 'could not mark posted payslip as reversed');
    return $ok && db_num_affected_rows() === 1;
}

/**
 * Compare-and-set one Posted period to the terminal reversed state.
 *
 * Generic status helpers intentionally deny status 6. Only the linked
 * financial-reversal command may call this bounded transition.
 *
 * @param int $period_id
 * @return bool
 */
function update_payroll_period_status_for_linked_reversal($period_id) {
    if ((int)$period_id <= 0 || !payroll_table_exists('payroll_periods'))
        return false;

    $sql = "UPDATE ".TB_PREF."payroll_periods SET status = 6"
        ." WHERE period_id = ".db_escape((int)$period_id)
        ." AND status = 3";
    $ok = db_query($sql, 'could not mark posted payroll period as reversed');
    return $ok && db_num_affected_rows() === 1;
}
