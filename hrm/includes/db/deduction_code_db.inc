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

// Deduction reason codes CRUD

/**
 * Add deduction code.
 *
 * @param string $deduction_name
 * @param string $account_code
 * @param int $inactive
 * @return int
 */
function add_deduction_code($deduction_name, $account_code='', $inactive=0) {
    $sql = "INSERT INTO ".TB_PREF."deduction_codes (deduction_name, account_code, inactive)
        VALUES (".db_escape($deduction_name).", ".db_escape($account_code).", ".db_escape((int)$inactive).")";
    db_query($sql, 'could not add deduction code');

    return (int)db_insert_id();
}

/**
 * Update deduction code.
 *
 * @param int $deduction_id
 * @param string $deduction_name
 * @param string $account_code
 * @param int $inactive
 * @return bool
 */
function update_deduction_code($deduction_id, $deduction_name, $account_code='', $inactive=0) {
    $sql = "UPDATE ".TB_PREF."deduction_codes SET
        deduction_name = ".db_escape($deduction_name).",
        account_code = ".db_escape($account_code).",
        inactive = ".db_escape((int)$inactive)."
        WHERE deduction_id = ".db_escape((int)$deduction_id);

    return (bool)db_query($sql, 'could not update deduction code');
}

/**
 * Get one deduction code.
 *
 * @param int $deduction_id
 * @return array|false
 */
function get_deduction_code($deduction_id) {
    $sql = "SELECT * FROM ".TB_PREF."deduction_codes WHERE deduction_id = ".db_escape((int)$deduction_id);
    $result = db_query($sql, 'could not get deduction code');

    return db_fetch_assoc($result);
}

/**
 * Get deduction codes.
 *
 * @param bool $all
 * @return resource
 */
function get_deduction_codes($all=false) {
    $sql = "SELECT * FROM ".TB_PREF."deduction_codes";
    if (!$all)
        $sql .= " WHERE inactive = 0";
    $sql .= " ORDER BY deduction_name";

    return db_query($sql, 'could not get deduction codes');
}

/**
 * Delete deduction code.
 *
 * @param int $deduction_id
 * @return bool
 */
function delete_deduction_code($deduction_id) {
    $sql = "DELETE FROM ".TB_PREF."deduction_codes WHERE deduction_id = ".db_escape((int)$deduction_id);

    return (bool)db_query($sql, 'could not delete deduction code');
}

