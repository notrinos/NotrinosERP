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
 * Employee asset allocation DB helpers.
 */

/**
 * Add asset allocation row.
 *
 * @param array $data
 * @return int
 */
function add_employee_asset_allocation($data) {
    $sql = "INSERT INTO ".TB_PREF."employee_asset_allocation
        (employee_id, asset_name, asset_code, serial_no, allocation_date, expected_return, return_date, status, notes)
        VALUES (
            ".db_escape($data['employee_id']).",
            ".db_escape($data['asset_name']).",
            ".db_escape($data['asset_code']).",
            ".db_escape($data['serial_no']).",
            ".db_escape(date2sql($data['allocation_date'])).",
            ".(trim($data['expected_return']) === '' ? 'NULL' : db_escape(date2sql($data['expected_return']))).",
            ".(trim($data['return_date']) === '' ? 'NULL' : db_escape(date2sql($data['return_date']))).",
            ".db_escape((int)$data['status']).",
            ".db_escape($data['notes'])."
        )";
    db_query($sql, 'could not add asset allocation');

    return (int)db_insert_id();
}

/**
 * Get asset allocation rows.
 *
 * @return resource
 */
function get_employee_asset_allocations() {
    $sql = "SELECT aa.*,
            CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,'')) employee_name
        FROM ".TB_PREF."employee_asset_allocation aa
        LEFT JOIN ".TB_PREF."employees e ON e.employee_id = aa.employee_id
        ORDER BY aa.allocation_date DESC, aa.allocation_id DESC";

    return db_query($sql, 'could not get asset allocations');
}
