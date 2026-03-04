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

// Overtime type CRUD

/**
 * Resolve overtime type master table name.
 *
 * @return string
 */
function overtime_type_table() {
    if (check_empty_result("SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = ".db_escape(TB_PREF.'overtime_types')))
        return 'overtime_types';

    return 'overtime';
}

/**
 * Add overtime type.
 *
 * @param string $overtime_name
 * @param float $pay_rate
 * @param int $inactive
 * @return int
 */
function add_overtime_type($overtime_name, $pay_rate=1, $inactive=0) {
    $table = overtime_type_table();
    $sql = "INSERT INTO ".TB_PREF.$table." (overtime_name, pay_rate, inactive)
        VALUES (".db_escape($overtime_name).", ".db_escape((float)$pay_rate).", ".db_escape((int)$inactive).")";
    db_query($sql, 'could not add overtime type');

    return (int)db_insert_id();
}

/**
 * Update overtime type.
 *
 * @param int $overtime_id
 * @param string $overtime_name
 * @param float $pay_rate
 * @param int $inactive
 * @return bool
 */
function update_overtime_type($overtime_id, $overtime_name, $pay_rate=1, $inactive=0) {
    $table = overtime_type_table();
    $sql = "UPDATE ".TB_PREF.$table." SET
        overtime_name = ".db_escape($overtime_name).",
        pay_rate = ".db_escape((float)$pay_rate).",
        inactive = ".db_escape((int)$inactive)."
        WHERE overtime_id = ".db_escape((int)$overtime_id);

    return (bool)db_query($sql, 'could not update overtime type');
}

/**
 * Get one overtime type.
 *
 * @param int $overtime_id
 * @return array|false
 */
function get_overtime_type($overtime_id) {
    $table = overtime_type_table();
    $sql = "SELECT * FROM ".TB_PREF.$table." WHERE overtime_id = ".db_escape((int)$overtime_id);
    $result = db_query($sql, 'could not get overtime type');

    return db_fetch_assoc($result);
}

/**
 * Get overtime types.
 *
 * @param bool $all
 * @return resource
 */
function get_overtime_types($all=false) {
    $table = overtime_type_table();
    $sql = "SELECT * FROM ".TB_PREF.$table;
    if (!$all)
        $sql .= " WHERE inactive = 0";
    $sql .= " ORDER BY overtime_name";

    return db_query($sql, 'could not get overtime types');
}

/**
 * Delete overtime type.
 *
 * @param int $overtime_id
 * @return bool
 */
function delete_overtime_type($overtime_id) {
    $table = overtime_type_table();
    $sql = "DELETE FROM ".TB_PREF.$table." WHERE overtime_id = ".db_escape((int)$overtime_id);

    return (bool)db_query($sql, 'could not delete overtime type');
}

