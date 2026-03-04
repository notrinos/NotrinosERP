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
    $balance = get_leave_balance($employee_id, $leave_id, $fiscal_year);
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

