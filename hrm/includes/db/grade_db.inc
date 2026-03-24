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
 * Get pay_grades table columns map.
 *
 * @return array
 */
function pay_grade_columns() {
	static $columns = null;

	if ($columns !== null)
		return $columns;

	$columns = array();
	$sql = "SHOW COLUMNS FROM ".TB_PREF."pay_grades";
	$result = db_query($sql, 'could not get pay grade columns');
	while ($row = db_fetch_assoc($result))
		$columns[$row['Field']] = true;

	return $columns;
}

/**
 * Check whether pay_grades table has column.
 *
 * @param string $column_name
 * @return bool
 */
function pay_grade_has_column($column_name) {
	$columns = pay_grade_columns();
	return isset($columns[$column_name]);
}

/**
 * Update pay grade.
 *
 * Backward compatible signature:
 * update_pay_grade($grade_id, $grade_name, $position_id, $pay_amount)
 *
 * @param int $grade_id
 * @param string $grade_name
 * @param int $position_id
 * @param float $pay_amount
 * @param array $extra
 * @return bool
 */
function update_pay_grade($grade_id, $grade_name, $position_id=0, $pay_amount=0, $extra=array()) {
	if (!is_array($extra))
		$extra = array();

	$set = array("grade_name = ".db_escape($grade_name));

	if (pay_grade_has_column('position_id'))
		$set[] = "position_id = ".db_escape((int)$position_id);
	if (pay_grade_has_column('pay_amount'))
		$set[] = "pay_amount = ".db_escape((float)$pay_amount);

	if (pay_grade_has_column('grade_code') && array_key_exists('grade_code', $extra))
		$set[] = "grade_code = ".db_escape($extra['grade_code']);
	if (pay_grade_has_column('grade_level') && array_key_exists('grade_level', $extra))
		$set[] = "grade_level = ".db_escape((int)$extra['grade_level']);
	if (pay_grade_has_column('min_salary') && array_key_exists('min_salary', $extra))
		$set[] = "min_salary = ".db_escape((float)$extra['min_salary']);
	if (pay_grade_has_column('mid_salary') && array_key_exists('mid_salary', $extra))
		$set[] = "mid_salary = ".db_escape((float)$extra['mid_salary']);
	if (pay_grade_has_column('max_salary') && array_key_exists('max_salary', $extra))
		$set[] = "max_salary = ".db_escape((float)$extra['max_salary']);
	if (pay_grade_has_column('description') && array_key_exists('description', $extra))
		$set[] = "description = ".db_escape($extra['description']);

	$sql = "UPDATE ".TB_PREF."pay_grades SET ".implode(', ', $set)." WHERE grade_id = ".db_escape((int)$grade_id);
	return (bool)db_query($sql, 'could not update grade record');
}

/**
 * Add pay grade.
 *
 * Backward compatible signature:
 * add_pay_grade($grade_name, $position_id, $pay_amount=0)
 *
 * @param string $grade_name
 * @param int $position_id
 * @param float $pay_amount
 * @param array $extra
 * @return int|false
 */
function add_pay_grade($grade_name, $position_id=0, $pay_amount=0, $extra=array()) {
	if (!is_array($extra))
		$extra = array();

	$fields = array('grade_name');
	$values = array(db_escape($grade_name));

	if (pay_grade_has_column('position_id')) {
		$fields[] = 'position_id';
		$values[] = db_escape((int)$position_id);
	}
	if (pay_grade_has_column('pay_amount')) {
		$fields[] = 'pay_amount';
		$values[] = db_escape((float)$pay_amount);
	}

	if (pay_grade_has_column('grade_code') && array_key_exists('grade_code', $extra)) {
		$fields[] = 'grade_code';
		$values[] = db_escape($extra['grade_code']);
	}
	if (pay_grade_has_column('grade_level') && array_key_exists('grade_level', $extra)) {
		$fields[] = 'grade_level';
		$values[] = db_escape((int)$extra['grade_level']);
	}
	if (pay_grade_has_column('min_salary') && array_key_exists('min_salary', $extra)) {
		$fields[] = 'min_salary';
		$values[] = db_escape((float)$extra['min_salary']);
	}
	if (pay_grade_has_column('mid_salary') && array_key_exists('mid_salary', $extra)) {
		$fields[] = 'mid_salary';
		$values[] = db_escape((float)$extra['mid_salary']);
	}
	if (pay_grade_has_column('max_salary') && array_key_exists('max_salary', $extra)) {
		$fields[] = 'max_salary';
		$values[] = db_escape((float)$extra['max_salary']);
	}
	if (pay_grade_has_column('description') && array_key_exists('description', $extra)) {
		$fields[] = 'description';
		$values[] = db_escape($extra['description']);
	}

	$sql = "INSERT INTO ".TB_PREF."pay_grades (".implode(', ', $fields).") VALUES (".implode(', ', $values).")";
	$db_ok = db_query($sql, 'could not insert grade record');
	if (!$db_ok)
		return false;

	return db_insert_id();
}

/**
 * Get pay grade by id.
 *
 * @param int $grade_id
 * @return array|false
 */
function get_pay_grade($grade_id) {
	$sql = "SELECT * FROM ".TB_PREF."pay_grades WHERE grade_id = ".db_escape((int)$grade_id);
	$result = db_query($sql, 'could not get grade record');

	return db_fetch($result);
}

/**
 * Get all pay grades.
 *
 * @param bool $all
 * @return resource
 */
function get_pay_grades($all=false) {
	$sql = "SELECT * FROM ".TB_PREF."pay_grades";

	if (!$all)
		$sql .= " WHERE !inactive";

	if (pay_grade_has_column('grade_level'))
		$sql .= " ORDER BY grade_level, grade_name";
	else
		$sql .= " ORDER BY grade_name";

	return db_query($sql, 'could not get grades data');
}

/**
 * Delete pay grade.
 *
 * @param int $grade_id
 * @return bool
 */
function delete_pay_grade($grade_id) {
	$sql = "DELETE FROM ".TB_PREF."pay_grades WHERE grade_id = ".db_escape((int)$grade_id);
	return (bool)db_query($sql, 'could not delete a pay grade record');
}

/**
 * Check whether pay grades exist.
 *
 * @return bool
 */
function db_has_pay_grades() {
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."pay_grades");
}
