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
 * Get one holiday.
 *
 * @param int $holiday_id
 * @return array|false
 */
function get_holiday($holiday_id) {
    $sql = "SELECT * FROM ".TB_PREF."holidays WHERE holiday_id = ".db_escape((int)$holiday_id);
    $result = db_query($sql, 'could not get holiday');

    return db_fetch($result);
}

/**
 * Get holiday list within optional date range.
 *
 * @param string $from_date
 * @param string $to_date
 * @return resource
 */
function get_holidays($from_date='', $to_date='') {
    $where = array('1=1');

    if (trim((string)$from_date) !== '')
        $where[] = 'holiday_date >= '.db_escape(date2sql($from_date));

    if (trim((string)$to_date) !== '')
        $where[] = 'holiday_date <= '.db_escape(date2sql($to_date));

    $sql = "SELECT * FROM ".TB_PREF."holidays
        WHERE ".implode(' AND ', $where)."
        ORDER BY holiday_date DESC, holiday_name";

    return db_query($sql, 'could not get holidays');
}

/**
 * Add holiday record.
 *
 * @param string $holiday_name
 * @param string $holiday_date
 * @param string|null $to_date
 * @param int $recurring
 * @param int $is_paid
 * @param string $description
 * @return int
 */
function add_holiday($holiday_name, $holiday_date, $to_date=null, $recurring=0, $is_paid=1, $description='') {
    $to_date_sql = ($to_date === null || trim((string)$to_date) === '') ? 'NULL' : db_escape(date2sql($to_date));

    $sql = "INSERT INTO ".TB_PREF."holidays
        (holiday_name, holiday_date, to_date, recurring, is_paid, description)
        VALUES (
            ".db_escape($holiday_name).",
            ".db_escape(date2sql($holiday_date)).",
            $to_date_sql,
            ".db_escape((int)$recurring).",
            ".db_escape((int)$is_paid).",
            ".db_escape($description)."
        )";

    db_query($sql, 'could not add holiday');
    return (int)db_insert_id();
}

/**
 * Update holiday record.
 *
 * @param int $holiday_id
 * @param string $holiday_name
 * @param string $holiday_date
 * @param string|null $to_date
 * @param int $recurring
 * @param int $is_paid
 * @param string $description
 * @return void
 */
function update_holiday($holiday_id, $holiday_name, $holiday_date, $to_date=null, $recurring=0, $is_paid=1, $description='') {
    $to_date_sql = ($to_date === null || trim((string)$to_date) === '') ? 'NULL' : db_escape(date2sql($to_date));

    $sql = "UPDATE ".TB_PREF."holidays SET
            holiday_name = ".db_escape($holiday_name).",
            holiday_date = ".db_escape(date2sql($holiday_date)).",
            to_date = $to_date_sql,
            recurring = ".db_escape((int)$recurring).",
            is_paid = ".db_escape((int)$is_paid).",
            description = ".db_escape($description)."
        WHERE holiday_id = ".db_escape((int)$holiday_id);

    db_query($sql, 'could not update holiday');
}

/**
 * Delete holiday record.
 *
 * @param int $holiday_id
 * @return void
 */
function delete_holiday($holiday_id) {
    $sql = "DELETE FROM ".TB_PREF."holidays WHERE holiday_id = ".db_escape((int)$holiday_id);
    db_query($sql, 'could not delete holiday');
}

