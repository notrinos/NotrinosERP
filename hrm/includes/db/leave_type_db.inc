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
 * Check whether a column exists in leave_types table.
 *
 * @param string $column_name
 * @return bool
 */
function leave_type_has_column($column_name) {
    static $cache = array();

    if (isset($cache[$column_name]))
        return $cache[$column_name];

    if (!function_exists('employee_table_exists') || !employee_table_exists('leave_types'))
        return $cache[$column_name] = false;

    $sql = "SHOW COLUMNS FROM ".TB_PREF."leave_types LIKE ".db_escape($column_name);
    $result = db_query($sql, 'could not check leave_types column');
    $cache[$column_name] = ($result && db_num_rows($result) > 0);

    return $cache[$column_name];
}

/**
 * Get leave type with all available fields.
 *
 * @param int $leave_id
 * @return array|false
 */
function get_leave_type_extended($leave_id) {
    if (!function_exists('employee_table_exists') || !employee_table_exists('leave_types'))
        return false;

    $sql = "SELECT * FROM ".TB_PREF."leave_types WHERE leave_id = ".db_escape((int)$leave_id);
    $result = db_query($sql, 'could not get extended leave type');

    return db_fetch($result);
}

/**
 * Update optional/extended leave type attributes.
 *
 * @param int $leave_id
 * @param array $attributes
 * @return void
 */
function update_leave_type_attributes($leave_id, $attributes=array()) {
    $assignments = array();

    $field_map = array(
        'is_paid' => 1,
        'is_carry_forward' => 0,
        'max_carry_forward' => 0,
        'is_encashable' => 0,
        'requires_document' => 0,
        'max_consecutive_days' => null,
        'applicable_gender' => null,
        'deductable' => 0,
        'color_code' => '#3498db',
        'description' => ''
    );

    foreach ($field_map as $field => $default_value) {
        if (!leave_type_has_column($field))
            continue;

        $value = array_key_exists($field, $attributes) ? $attributes[$field] : $default_value;
        if ($value === null)
            $assignments[] = "$field = NULL";
        else
            $assignments[] = "$field = ".db_escape($value);
    }

    if (empty($assignments))
        return;

    $sql = "UPDATE ".TB_PREF."leave_types SET ".implode(', ', $assignments)."
        WHERE leave_id = ".db_escape((int)$leave_id);
    db_query($sql, 'could not update leave type attributes');
}

/**
 * Create leave type with optional extended attributes.
 *
 * Keeps backward compatibility by reusing legacy add_leave_type().
 *
 * @param string $leave_name
 * @param string $leave_code
 * @param float $pay_rate
 * @param array $attributes
 * @return int Inserted leave id.
 */
function add_leave_type_extended($leave_name, $leave_code, $pay_rate, $attributes=array()) {
    add_leave_type($leave_name, $leave_code, $pay_rate);
    $leave_id = db_insert_id();
    update_leave_type_attributes($leave_id, $attributes);

    return (int)$leave_id;
}

/**
 * Update leave type including optional extended attributes.
 *
 * Keeps backward compatibility by reusing legacy update_leave_type().
 *
 * @param int $leave_id
 * @param string $leave_name
 * @param string $leave_code
 * @param float $pay_rate
 * @param array $attributes
 * @return void
 */
function update_leave_type_extended($leave_id, $leave_name, $leave_code, $pay_rate, $attributes=array()) {
    update_leave_type($leave_id, $leave_name, $leave_code, $pay_rate);
    update_leave_type_attributes($leave_id, $attributes);
}

/**
 * Get leave types list including extra columns when available.
 *
 * @param bool $all Include inactive records.
 * @return resource
 */
function get_leave_types_extended($all=false) {
    $sql = "SELECT * FROM ".TB_PREF."leave_types";

    if (!$all && leave_type_has_column('inactive'))
        $sql .= " WHERE !inactive";

    $sql .= " ORDER BY leave_name";

    return db_query($sql, 'could not get extended leave types data');
}

