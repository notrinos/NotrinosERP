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

include_once($path_to_root.'/hrm/includes/db/employee_db.inc');
include_once($path_to_root.'/hrm/includes/db/leave_policy_db.inc');

/**
 * Get the configured fiscal-period bounds for a leave-balance year.
 *
 * Leave balances use the year in which the fiscal period ends as their
 * integer key. Calendar bounds are retained as a compatibility fallback for
 * historical rows whose year has no matching fiscal-year configuration.
 *
 * @param int $fiscal_year
 * @return array
 */
function get_leave_balance_fiscal_year_bounds($fiscal_year) {
    $fiscal_year = (int)$fiscal_year;
    $calendar_start = $fiscal_year.'-01-01';
    $calendar_end = $fiscal_year.'-12-31';

    if ($fiscal_year <= 0)
        return array($calendar_start, $calendar_end);

    $sql = "SELECT begin, end
        FROM ".TB_PREF."fiscal_year
        WHERE end >= ".db_escape($calendar_start)."
            AND end <= ".db_escape($calendar_end)."
        ORDER BY end DESC
        LIMIT 1";
    $result = db_query($sql, 'could not get leave balance fiscal year bounds');
    $row = db_fetch($result);

    if ($row
        && preg_match('/^\d{4}-\d{2}-\d{2}$/', $row['begin'])
        && preg_match('/^\d{4}-\d{2}-\d{2}$/', $row['end'])
        && $row['begin'] <= $row['end']) {
        return array($row['begin'], $row['end']);
    }

    return array($calendar_start, $calendar_end);
}

/**
 * Resolve a date to the ending-year key used by leave balances.
 *
 * @param string $date_value
 * @return int
 */
function get_leave_balance_fiscal_year_for_date($date_value) {
    $date_sql = leave_policy_to_sql_date($date_value);
    if (!preg_match('/^\d{4}-\d{2}-\d{2}$/', $date_sql))
        return 0;

    $sql = "SELECT end
        FROM ".TB_PREF."fiscal_year
        WHERE begin <= ".db_escape($date_sql)."
            AND end >= ".db_escape($date_sql)."
        ORDER BY begin DESC
        LIMIT 1";
    $result = db_query($sql, 'could not resolve leave balance fiscal year');
    $row = db_fetch($result);

    if ($row && preg_match('/^\d{4}-\d{2}-\d{2}$/', $row['end']))
        return (int)substr($row['end'], 0, 4);

    return (int)substr($date_sql, 0, 4);
}

/**
 * Get leave balance row by employee/leave/year.
 *
 * @param string $employee_id
 * @param int $leave_id
 * @param int $fiscal_year
 * @return array|false
 */
function get_leave_balance($employee_id, $leave_id, $fiscal_year) {
    $sql = "SELECT lb.*, lt.leave_name,
        TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,''))) employee_name
        FROM ".TB_PREF."leave_balances lb
        LEFT JOIN ".TB_PREF."leave_types lt ON lt.leave_id = lb.leave_id
        LEFT JOIN ".TB_PREF."employees e ON e.employee_id = lb.employee_id
        WHERE lb.employee_id = ".db_escape($employee_id)."
            AND lb.leave_id = ".db_escape((int)$leave_id)."
            AND lb.fiscal_year = ".db_escape((int)$fiscal_year);
    $result = db_query($sql, 'could not get leave balance');

    return db_fetch($result);
}

/**
 * Get leave balances by optional filters.
 *
 * @param int $fiscal_year
 * @param string $employee_id
 * @param int $leave_id
 * @return resource
 */
function get_leave_balances($fiscal_year=0, $employee_id='', $leave_id=0) {
    $where = array('1=1');

    if ((int)$fiscal_year > 0)
        $where[] = 'lb.fiscal_year = '.db_escape((int)$fiscal_year);

    if ($employee_id !== '' && $employee_id !== ALL_TEXT)
        $where[] = 'lb.employee_id = '.db_escape($employee_id);

    if ((int)$leave_id > 0)
        $where[] = 'lb.leave_id = '.db_escape((int)$leave_id);

    $sql = "SELECT lb.*, lt.leave_name,
        TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,''))) employee_name
        FROM ".TB_PREF."leave_balances lb
        LEFT JOIN ".TB_PREF."leave_types lt ON lt.leave_id = lb.leave_id
        LEFT JOIN ".TB_PREF."employees e ON e.employee_id = lb.employee_id
        WHERE ".implode(' AND ', $where)."
        ORDER BY lb.employee_id, lb.fiscal_year DESC, lb.leave_id";

    return db_query($sql, 'could not get leave balances');
}

/**
 * Backfill entitlement for matching leave balance rows before inquiry/reporting.
 *
 * @param int $fiscal_year
 * @param string $employee_id
 * @param int $leave_id
 * @return void
 */
function ensure_leave_balance_entitlements_for_filters($fiscal_year=0, $employee_id='', $leave_id=0) {
    $fiscal_year = (int)$fiscal_year;
    if ($fiscal_year <= 0)
        return;

    list(, $as_of_date) = get_leave_balance_fiscal_year_bounds($fiscal_year);
    $pairs = array();
    $where = array('fiscal_year = '.db_escape($fiscal_year));
    if ($employee_id !== '' && $employee_id !== ALL_TEXT)
        $where[] = 'employee_id = '.db_escape($employee_id);
    if ((int)$leave_id > 0)
        $where[] = 'leave_id = '.db_escape((int)$leave_id);

    $sql = "SELECT DISTINCT employee_id, leave_id FROM ".TB_PREF."leave_balances WHERE ".implode(' AND ', $where);
    $result = db_query($sql, 'could not get leave balance entitlement filters');

    while ($row = db_fetch($result))
        $pairs[$row['employee_id'].':'.(int)$row['leave_id']] = array($row['employee_id'], (int)$row['leave_id']);

    if (!function_exists('employee_table_exists')
        || !employee_table_exists('employees')
        || !employee_table_exists('leave_policies')) {
        foreach ($pairs as $pair)
            ensure_leave_balance_entitlement($pair[0], $pair[1], $fiscal_year, $as_of_date);
        return;
    }

    $policy_where = array('e.inactive = 0');
    if ($employee_id !== '' && $employee_id !== ALL_TEXT)
        $policy_where[] = 'e.employee_id = '.db_escape($employee_id);
    if ((int)$leave_id > 0)
        $policy_where[] = 'p.leave_id = '.db_escape((int)$leave_id);

    $policy_sql = "SELECT DISTINCT e.employee_id, p.leave_id
        FROM ".TB_PREF."employees e
        INNER JOIN ".TB_PREF."leave_policies p
            ON p.inactive = 0
            AND p.effective_from <= ".db_escape($as_of_date)."
            AND (p.effective_to IS NULL OR p.effective_to = '' OR p.effective_to >= ".db_escape($as_of_date).")
            AND (p.grade_id IS NULL OR p.grade_id = 0 OR p.grade_id = e.grade_id)
            AND (p.employment_type IS NULL OR p.employment_type = e.employment_type)
        WHERE ".implode(' AND ', $policy_where);
    $policy_result = db_query($policy_sql, 'could not get leave policy entitlement filters');

    while ($row = db_fetch($policy_result))
        $pairs[$row['employee_id'].':'.(int)$row['leave_id']] = array($row['employee_id'], (int)$row['leave_id']);

    foreach ($pairs as $pair)
        ensure_leave_balance_entitlement($pair[0], $pair[1], $fiscal_year, $as_of_date);
}

/**
 * Insert or update leave balance.
 *
 * @param string $employee_id
 * @param int $leave_id
 * @param int $fiscal_year
 * @param float $entitled
 * @param float $carried_forward
 * @param float $taken
 * @param float $pending
 * @param float $adjusted
 * @return void
 */
function save_leave_balance($employee_id, $leave_id, $fiscal_year, $entitled, $carried_forward=0, $taken=0, $pending=0, $adjusted=0) {
    $existing = get_leave_balance($employee_id, $leave_id, $fiscal_year);

    if ($existing) {
        $sql = "UPDATE ".TB_PREF."leave_balances SET
                entitled = ".db_escape((float)$entitled).",
                carried_forward = ".db_escape((float)$carried_forward).",
                taken = ".db_escape((float)$taken).",
                pending = ".db_escape((float)$pending).",
                adjusted = ".db_escape((float)$adjusted)."
            WHERE balance_id = ".db_escape((int)$existing['balance_id']);
        db_query($sql, 'could not update leave balance');
    } else {
        $sql = "INSERT INTO ".TB_PREF."leave_balances
            (employee_id, leave_id, fiscal_year, entitled, carried_forward, taken, pending, adjusted)
            VALUES (
                ".db_escape($employee_id).",
                ".db_escape((int)$leave_id).",
                ".db_escape((int)$fiscal_year).",
                ".db_escape((float)$entitled).",
                ".db_escape((float)$carried_forward).",
                ".db_escape((float)$taken).",
                ".db_escape((float)$pending).",
                ".db_escape((float)$adjusted)."
            )";
        db_query($sql, 'could not insert leave balance');
    }
}

/**
 * Ensure a leave balance row is initialized with policy entitlement.
 *
 * @param string $employee_id
 * @param int $leave_id
 * @param int $fiscal_year
 * @param string $as_of_date
 * @return array|false
 */
function ensure_leave_balance_entitlement($employee_id, $leave_id, $fiscal_year, $as_of_date='') {
    if (!function_exists('get_employee_by_code') || !function_exists('get_leave_policy_for_employee'))
        return get_leave_balance($employee_id, $leave_id, $fiscal_year);

    list($fiscal_year_start, $fiscal_year_end) = get_leave_balance_fiscal_year_bounds($fiscal_year);
    $as_of_sql = leave_policy_to_sql_date($as_of_date);
    if ($as_of_sql === '')
        $as_of_sql = $fiscal_year_end;

    $employee = get_employee_by_code($employee_id);
    if (!$employee)
        return get_leave_balance($employee_id, $leave_id, $fiscal_year);

    $policy = get_leave_policy_for_employee($employee, (int)$leave_id, $as_of_sql);
    if (!$policy)
        return get_leave_balance($employee_id, $leave_id, $fiscal_year);

    $entitled = calculate_leave_policy_entitlement($policy, $employee, $as_of_sql, $fiscal_year_start);
    $balance = get_leave_balance($employee_id, $leave_id, $fiscal_year);

    save_leave_balance(
        $employee_id,
        $leave_id,
        $fiscal_year,
        $entitled,
        $balance ? (float)$balance['carried_forward'] : 0,
        $balance ? (float)$balance['taken'] : 0,
        $balance ? (float)$balance['pending'] : 0,
        $balance ? (float)$balance['adjusted'] : 0
    );

    return get_leave_balance($employee_id, $leave_id, $fiscal_year);
}

/**
 * Apply leave movements to balance buckets.
 *
 * @param string $employee_id
 * @param int $leave_id
 * @param int $fiscal_year
 * @param float $taken_delta
 * @param float $pending_delta
 * @param float $adjusted_delta
 * @return void
 */
function apply_leave_balance_movement($employee_id, $leave_id, $fiscal_year, $taken_delta=0, $pending_delta=0, $adjusted_delta=0) {
    list(, $fiscal_year_end) = get_leave_balance_fiscal_year_bounds($fiscal_year);
    $balance = ensure_leave_balance_entitlement($employee_id, $leave_id, $fiscal_year, $fiscal_year_end);
    if (!$balance) {
        save_leave_balance($employee_id, $leave_id, $fiscal_year, 0, 0, $taken_delta, $pending_delta, $adjusted_delta);
        return;
    }

    $new_taken = (float)$balance['taken'] + (float)$taken_delta;
    $new_pending = (float)$balance['pending'] + (float)$pending_delta;
    $new_adjusted = (float)$balance['adjusted'] + (float)$adjusted_delta;

    save_leave_balance(
        $employee_id,
        $leave_id,
        $fiscal_year,
        (float)$balance['entitled'],
        (float)$balance['carried_forward'],
        $new_taken,
        $new_pending,
        $new_adjusted
    );
}

/**
 * Remaining leave days for employee/leave/year.
 *
 * @param string $employee_id
 * @param int $leave_id
 * @param int $fiscal_year
 * @return float
 */
function get_leave_balance_available_days($employee_id, $leave_id, $fiscal_year) {
    $balance = get_leave_balance($employee_id, $leave_id, $fiscal_year);
    if (!$balance)
        return 0;

    return (float)$balance['entitled']
        + (float)$balance['carried_forward']
        + (float)$balance['adjusted']
        - (float)$balance['taken']
        - (float)$balance['pending'];
}
