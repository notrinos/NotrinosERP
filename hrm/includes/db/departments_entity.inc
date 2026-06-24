<?php
/**
 * Entity class for departments.
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 *
 * Overrides get() and get_all() to LEFT JOIN on parent_department_id so that
 * the returned rows include a parent_department_name column (matching the old
 * get_department() / get_departments() convenience functions).
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class departments_entity extends Entity {
	function __construct() {
		parent::__construct(
			'departments',
			array(
				'department_id',
				'department_code',
				'department_name',
				'parent_department_id',
				'manager_employee_id',
				'cost_center_id',
				'payroll_expense_account',
				'payroll_liability_account',
				'description',
				'inactive'
			),
			'department_id'
		);
	}

	/** Override get() to include parent department name. */
	function get($key = null) {
		if ($key === null)
			return parent::get();

		$sql = "SELECT d.*, p.department_name AS parent_department_name
			FROM " . TB_PREF . "departments d
			LEFT JOIN " . TB_PREF . "departments p ON p.department_id = d.parent_department_id
			WHERE d.department_id = " . db_escape($key);
		$result = db_query($sql, 'could not get department record');
		return db_num_rows($result) ? db_fetch_assoc($result) : null;
	}

	/** Override get_all() to include parent department name & default ordering. */
	function get_all($where = null, $order_by = null) {
		$sql = "SELECT d.*, p.department_name AS parent_department_name
			FROM " . TB_PREF . "departments d
			LEFT JOIN " . TB_PREF . "departments p ON p.department_id = d.parent_department_id";
		if ($where) {
			// Prefix bare `inactive` / `!inactive` with `d.` to avoid ambiguous-column errors from self-join.
			// `!inactive` is MySQL shorthand for `NOT inactive`; rewrite to `NOT d.inactive`.
			// Use negative lookbehind so already-qualified columns are left alone.
			$where = preg_replace_callback('/(?<!\w\.)(!?)inactive\b/', function($m) {
				return $m[1] === '!' ? 'NOT d.inactive' : 'd.inactive';
			}, $where);
			$sql .= " WHERE " . ($this->subset ? '(' . $this->subset . ') AND ' : '') . $where;
		}
		if ($order_by !== null) {
			$order_by = (array)$order_by;
			$sql .= " ORDER BY " . implode(', ', $order_by);
		} else {
			$sql .= " ORDER BY COALESCE(d.parent_department_id, 0), d.department_name";
		}
		$result = db_query($sql, 'could not get departments data');
		if (!$result)
			return $this->error("Cannot get records from " . $this->name);
		return $result;
	}

	/** Check whether a department has child departments. */
	public static function has_children($department_id) {
		return check_empty_result(
			"SELECT COUNT(*) FROM " . TB_PREF . "departments
			WHERE parent_department_id = " . db_escape((int)$department_id)
		);
	}

	/** Get department name by id. */
	public static function name($id) {
		$row = static::find($id);
		return $row ? $row['department_name'] : '';
	}
}
