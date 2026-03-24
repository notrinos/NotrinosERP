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
 * Income tax bracket CRUD
 */

/**
 * Convert user date to SQL date.
 *
 * @param string $date_value
 * @return string
 */
function tax_bracket_to_sql_date($date_value) {
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
 * Add income tax bracket.
 *
 * @param string $bracket_name
 * @param float $from_amount
 * @param float|null $to_amount
 * @param float $rate
 * @param float $fixed_amount
 * @param string $effective_from
 * @param string $effective_to
 * @return int|false
 */
function add_tax_bracket($bracket_name, $from_amount, $to_amount, $rate, $fixed_amount, $effective_from, $effective_to='') {
    $from_sql = tax_bracket_to_sql_date($effective_from);
    $to_sql = tax_bracket_to_sql_date($effective_to);
    if ($from_sql === '')
        return false;

    $sql = "INSERT INTO ".TB_PREF."tax_brackets
        (bracket_name, from_amount, to_amount, rate, fixed_amount, effective_from, effective_to)
        VALUES (
            ".db_escape($bracket_name).",
            ".db_escape((float)$from_amount).",
            ".($to_amount === null || $to_amount === '' ? 'NULL' : db_escape((float)$to_amount)).",
            ".db_escape((float)$rate).",
            ".db_escape((float)$fixed_amount).",
            ".db_escape($from_sql).",
            ".($to_sql === '' ? 'NULL' : db_escape($to_sql))."
        )";

    db_query($sql, 'could not add tax bracket');
    return db_insert_id();
}

/**
 * Update income tax bracket.
 *
 * @param int $bracket_id
 * @param string $bracket_name
 * @param float $from_amount
 * @param float|null $to_amount
 * @param float $rate
 * @param float $fixed_amount
 * @param string $effective_from
 * @param string $effective_to
 * @return bool
 */
function update_tax_bracket($bracket_id, $bracket_name, $from_amount, $to_amount, $rate, $fixed_amount, $effective_from, $effective_to='') {
    $from_sql = tax_bracket_to_sql_date($effective_from);
    $to_sql = tax_bracket_to_sql_date($effective_to);
    if ($from_sql === '')
        return false;

    $sql = "UPDATE ".TB_PREF."tax_brackets SET
        bracket_name = ".db_escape($bracket_name).",
        from_amount = ".db_escape((float)$from_amount).",
        to_amount = ".($to_amount === null || $to_amount === '' ? 'NULL' : db_escape((float)$to_amount)).",
        rate = ".db_escape((float)$rate).",
        fixed_amount = ".db_escape((float)$fixed_amount).",
        effective_from = ".db_escape($from_sql).",
        effective_to = ".($to_sql === '' ? 'NULL' : db_escape($to_sql))."
        WHERE bracket_id = ".db_escape((int)$bracket_id);

    return (bool)db_query($sql, 'could not update tax bracket');
}

/**
 * Get one tax bracket.
 *
 * @param int $bracket_id
 * @return array|false
 */
function get_tax_bracket($bracket_id) {
    $sql = "SELECT * FROM ".TB_PREF."tax_brackets WHERE bracket_id = ".db_escape((int)$bracket_id);
    $result = db_query($sql, 'could not get tax bracket');
    return db_fetch_assoc($result);
}

/**
 * Get tax brackets.
 *
 * @param string $as_of_date Optional user date.
 * @return resource
 */
function get_tax_brackets($as_of_date='') {
    $sql = "SELECT * FROM ".TB_PREF."tax_brackets WHERE 1=1";

    if (trim((string)$as_of_date) !== '') {
        $as_of_sql = tax_bracket_to_sql_date($as_of_date);
        if ($as_of_sql !== '') {
            $sql .= " AND effective_from <= ".db_escape($as_of_sql)
                ." AND (effective_to IS NULL OR effective_to >= ".db_escape($as_of_sql).")";
        }
    }

    $sql .= " ORDER BY effective_from DESC, from_amount ASC, bracket_id ASC";
    return db_query($sql, 'could not get tax brackets');
}

/**
 * Delete one tax bracket.
 *
 * @param int $bracket_id
 * @return bool
 */
function delete_tax_bracket($bracket_id) {
    $sql = "DELETE FROM ".TB_PREF."tax_brackets WHERE bracket_id = ".db_escape((int)$bracket_id);
    return (bool)db_query($sql, 'could not delete tax bracket');
}

