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
 * Convert date to SQL format.
 *
 * @param string $date_value
 * @return string
 */
function salary_structure_to_sql_date($date_value) {
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
 * Get salary_structure table columns map.
 *
 * @return array
 */
function salary_structure_columns() {
	static $columns = null;

	if ($columns !== null)
		return $columns;

	$columns = array();
	$sql = "SHOW COLUMNS FROM ".TB_PREF."salary_structure";
	$result = db_query($sql, 'could not get salary structure columns');
	while ($row = db_fetch_assoc($result))
		$columns[$row['Field']] = true;

	return $columns;
}

/**
 * Check whether salary_structure table has column.
 *
 * @param string $column_name
 * @return bool
 */
function salary_structure_has_column($column_name) {
	$columns = salary_structure_columns();
	return isset($columns[$column_name]);
}

/**
 * Get salary amount for one position/grade/element at date.
 *
 * @param int $position_id
 * @param int $grade_id
 * @param int $element_id
 * @param string|null $as_of_date
 * @return float
 */
function get_salary_structure_amount($position_id, $grade_id, $element_id, $as_of_date=null) {
	$as_of_sql = salary_structure_to_sql_date($as_of_date === null ? Today() : $as_of_date);

	$sql = "SELECT pay_amount FROM ".TB_PREF."salary_structure
		WHERE position_id = ".db_escape((int)$position_id)."
		AND grade_id = ".db_escape((int)$grade_id)."
		AND element_id = ".db_escape((int)$element_id);

	if (salary_structure_has_column('is_active'))
		$sql .= " AND is_active = 1";
	if (salary_structure_has_column('effective_from') && $as_of_sql !== '')
		$sql .= " AND effective_from <= ".db_escape($as_of_sql);
	if (salary_structure_has_column('effective_to') && $as_of_sql !== '')
		$sql .= " AND (effective_to IS NULL OR effective_to = '0000-00-00' OR effective_to >= ".db_escape($as_of_sql).")";

	if (salary_structure_has_column('effective_from'))
		$sql .= " ORDER BY effective_from DESC, structure_id DESC";

	$sql .= " LIMIT 1";
	$result = db_query($sql, 'could not get a salary structure record');
	$row = db_fetch($result);

	return $row ? (float)$row[0] : 0.0;
}

/**
 * Get salary structures optionally filtered by effective date.
 *
 * @param string|null $as_of_date
 * @return resource
 */
function get_salary_structures($as_of_date=null) {
	$sql = "SELECT * FROM ".TB_PREF."salary_structure WHERE 1=1";

	if ($as_of_date !== null) {
		$as_of_sql = salary_structure_to_sql_date($as_of_date);
		if (salary_structure_has_column('effective_from') && $as_of_sql !== '')
			$sql .= " AND effective_from <= ".db_escape($as_of_sql);
		if (salary_structure_has_column('effective_to') && $as_of_sql !== '')
			$sql .= " AND (effective_to IS NULL OR effective_to = '0000-00-00' OR effective_to >= ".db_escape($as_of_sql).")";
	}

	if (salary_structure_has_column('is_active'))
		$sql .= " AND is_active = 1";

	if (salary_structure_has_column('effective_from'))
		$sql .= " ORDER BY position_id, grade_id, element_id, effective_from DESC";
	else
		$sql .= " ORDER BY position_id, grade_id, element_id";

	return db_query($sql, 'could not get salary structure data');
}

/**
 * Add salary structure row.
 *
 * Backward compatible signature:
 * add_salary_structure_element($position_id, $grade_id, $element_id, $amount)
 *
 * @param int $position_id
 * @param int $grade_id
 * @param int $element_id
 * @param float $amount
 * @param array $data Optional enhanced fields.
 * @return int|false
 */
function add_salary_structure_element($position_id, $grade_id, $element_id, $amount, $data=array()) {
	if (!is_array($data))
		$data = array();

	$fields = array('position_id', 'grade_id', 'element_id', 'pay_amount');
	$values = array(
		db_escape((int)$position_id),
		db_escape((int)$grade_id),
		db_escape((int)$element_id),
		db_escape((float)$amount)
	);

	if (salary_structure_has_column('formula') && array_key_exists('formula', $data)) {
		$fields[] = 'formula';
		$values[] = db_escape($data['formula']);
	}

	if (salary_structure_has_column('effective_from')) {
		$effective_from = array_key_exists('effective_from', $data) ? salary_structure_to_sql_date($data['effective_from']) : date2sql(Today());
		if ($effective_from === '')
			$effective_from = date2sql(Today());
		$fields[] = 'effective_from';
		$values[] = db_escape($effective_from);
	}

	if (salary_structure_has_column('effective_to') && array_key_exists('effective_to', $data)) {
		$effective_to = salary_structure_to_sql_date($data['effective_to']);
		$fields[] = 'effective_to';
		$values[] = ($effective_to === '' ? 'NULL' : db_escape($effective_to));
	}

	if (salary_structure_has_column('is_active')) {
		$fields[] = 'is_active';
		$values[] = db_escape(array_key_exists('is_active', $data) ? (int)$data['is_active'] : 1);
	}

	$sql = "INSERT INTO ".TB_PREF."salary_structure (".implode(', ', $fields).") VALUES (".implode(', ', $values).")";
	$db_ok = db_query($sql, 'could not insert a salary structure element');
	if (!$db_ok)
		return false;

	return db_insert_id();
}

/**
 * Update salary structure row amount/effective data.
 *
 * Backward compatible signature:
 * update_salary_structure($position_id, $grade_id, $element_id, $pay_amount)
 *
 * @param int $position_id
 * @param int $grade_id
 * @param int $element_id
 * @param float $pay_amount
 * @param array $data Optional enhanced fields.
 * @return bool
 */
function update_salary_structure($position_id, $grade_id, $element_id, $pay_amount, $data=array()) {
	if (!is_array($data))
		$data = array();

	$set = array("pay_amount = ".db_escape((float)$pay_amount));
	if (salary_structure_has_column('formula') && array_key_exists('formula', $data))
		$set[] = "formula = ".db_escape($data['formula']);
	if (salary_structure_has_column('effective_to') && array_key_exists('effective_to', $data)) {
		$effective_to = salary_structure_to_sql_date($data['effective_to']);
		$set[] = "effective_to = ".($effective_to === '' ? 'NULL' : db_escape($effective_to));
	}
	if (salary_structure_has_column('is_active') && array_key_exists('is_active', $data))
		$set[] = "is_active = ".db_escape((int)$data['is_active']);

	$sql = "UPDATE ".TB_PREF."salary_structure SET ".implode(', ', $set)."
		WHERE position_id = ".db_escape((int)$position_id)."
		AND grade_id = ".db_escape((int)$grade_id)."
		AND element_id = ".db_escape((int)$element_id);

	if (salary_structure_has_column('effective_from') && array_key_exists('effective_from', $data)) {
		$effective_from = salary_structure_to_sql_date($data['effective_from']);
		if ($effective_from !== '')
			$sql .= " AND effective_from = ".db_escape($effective_from);
	}

	return (bool)db_query($sql, 'could not update salary structure record');
}

/**
 * Delete salary structure rows by element key.
 *
 * @param int $position_id
 * @param int $grade_id
 * @param int $element_id
 * @param string|null $effective_from
 * @return bool
 */
function delete_salary_structure($position_id, $grade_id, $element_id, $effective_from=null) {
	$sql = "DELETE FROM ".TB_PREF."salary_structure WHERE position_id = ".db_escape((int)$position_id)." AND grade_id = ".db_escape((int)$grade_id)." AND element_id = ".db_escape((int)$element_id);

	if (salary_structure_has_column('effective_from') && $effective_from !== null) {
		$effective_from_sql = salary_structure_to_sql_date($effective_from);
		if ($effective_from_sql !== '')
			$sql .= " AND effective_from = ".db_escape($effective_from_sql);
	}

	return (bool)db_query($sql, 'could not delete a salary structure record');
}

/**
 * Check if salary structure row exists for key and effective date.
 *
 * @param int $position_id
 * @param int $grade_id
 * @param int $element_id
 * @param string|null $as_of_date
 * @return bool
 */
function salary_structure_element_exist($position_id, $grade_id, $element_id, $as_of_date=null) {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."salary_structure
		WHERE position_id = ".db_escape((int)$position_id)."
		AND grade_id = ".db_escape((int)$grade_id)."
		AND element_id = ".db_escape((int)$element_id);

	if ($as_of_date !== null) {
		$as_of_sql = salary_structure_to_sql_date($as_of_date);
		if (salary_structure_has_column('effective_from') && $as_of_sql !== '')
			$sql .= " AND effective_from <= ".db_escape($as_of_sql);
		if (salary_structure_has_column('effective_to') && $as_of_sql !== '')
			$sql .= " AND (effective_to IS NULL OR effective_to = '0000-00-00' OR effective_to >= ".db_escape($as_of_sql).")";
	}

	if (salary_structure_has_column('is_active'))
		$sql .= " AND is_active = 1";

	return check_empty_result($sql);
}
