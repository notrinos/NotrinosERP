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
 * Loan type definitions CRUD
 */

/**
 * Add loan type.
 *
 * @param string $name
 * @param string $code
 * @param float $interest_rate
 * @param float|null $max_amount
 * @param int|null $max_installments
 * @param int $max_active_loans
 * @param string $account_code
 * @param string $interest_account
 * @param int $inactive
 * @return int
 */
function add_loan_type($name, $code, $interest_rate=0, $max_amount=null, $max_installments=null, $max_active_loans=1, $account_code='', $interest_account='', $inactive=0) {
    $sql = "INSERT INTO ".TB_PREF."loan_types
        (loan_type_name, loan_type_code, interest_rate, max_amount, max_installments, max_active_loans, account_code, interest_account, inactive)
        VALUES (
            ".db_escape($name).",
            ".db_escape($code).",
            ".db_escape((float)$interest_rate).",
            ".($max_amount === null || $max_amount === '' ? 'NULL' : db_escape((float)$max_amount)).",
            ".($max_installments === null || $max_installments === '' ? 'NULL' : db_escape((int)$max_installments)).",
            ".db_escape((int)$max_active_loans).",
            ".db_escape($account_code).",
            ".(trim((string)$interest_account) === '' ? 'NULL' : db_escape($interest_account)).",
            ".db_escape((int)$inactive)."
        )";

    db_query($sql, 'could not add loan type');
    return (int)db_insert_id();
}

/**
 * Update loan type.
 *
 * @param int $loan_type_id
 * @param string $name
 * @param string $code
 * @param float $interest_rate
 * @param float|null $max_amount
 * @param int|null $max_installments
 * @param int $max_active_loans
 * @param string $account_code
 * @param string $interest_account
 * @param int $inactive
 * @return bool
 */
function update_loan_type($loan_type_id, $name, $code, $interest_rate=0, $max_amount=null, $max_installments=null, $max_active_loans=1, $account_code='', $interest_account='', $inactive=0) {
    $sql = "UPDATE ".TB_PREF."loan_types SET
        loan_type_name = ".db_escape($name).",
        loan_type_code = ".db_escape($code).",
        interest_rate = ".db_escape((float)$interest_rate).",
        max_amount = ".($max_amount === null || $max_amount === '' ? 'NULL' : db_escape((float)$max_amount)).",
        max_installments = ".($max_installments === null || $max_installments === '' ? 'NULL' : db_escape((int)$max_installments)).",
        max_active_loans = ".db_escape((int)$max_active_loans).",
        account_code = ".db_escape($account_code).",
        interest_account = ".(trim((string)$interest_account) === '' ? 'NULL' : db_escape($interest_account)).",
        inactive = ".db_escape((int)$inactive)."
        WHERE loan_type_id = ".db_escape((int)$loan_type_id);

    return (bool)db_query($sql, 'could not update loan type');
}

/**
 * Get one loan type.
 *
 * @param int $loan_type_id
 * @return array|false
 */
function get_loan_type($loan_type_id) {
    $sql = "SELECT * FROM ".TB_PREF."loan_types WHERE loan_type_id = ".db_escape((int)$loan_type_id);
    $result = db_query($sql, 'could not get loan type');
    return db_fetch_assoc($result);
}

/**
 * Get loan types.
 *
 * @param bool $all
 * @return resource
 */
function get_loan_types($all=false) {
    $sql = "SELECT * FROM ".TB_PREF."loan_types";
    if (!$all)
        $sql .= " WHERE inactive = 0";
    $sql .= " ORDER BY loan_type_name";

    return db_query($sql, 'could not get loan types');
}

/**
 * Delete loan type.
 *
 * @param int $loan_type_id
 * @return bool
 */
function delete_loan_type($loan_type_id) {
    if (check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."employee_loans WHERE loan_type_id = ".db_escape((int)$loan_type_id)))
        return false;

    $sql = "DELETE FROM ".TB_PREF."loan_types WHERE loan_type_id = ".db_escape((int)$loan_type_id);
    return (bool)db_query($sql, 'could not delete loan type');
}

