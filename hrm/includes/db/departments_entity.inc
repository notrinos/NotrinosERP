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
if (!function_exists('append_domain_audit_event'))
	include_once($path_to_root . '/includes/db/domain_audit_db.inc');
include_once($path_to_root . '/hrm/includes/db/legal_entity_db.inc');
include_once($path_to_root . '/hrm/includes/db/organization_unit_db.inc');

/**
 * Reuse the existing Department maintenance authority inside the server writer.
 *
 * @return bool
 */
function hrm_department_writer_authorized() {
	return isset($_SESSION['wa_current_user'])
		&& is_object($_SESSION['wa_current_user'])
		&& method_exists($_SESSION['wa_current_user'], 'can_access')
		&& $_SESSION['wa_current_user']->can_access('SA_DEPARTMENT');
}

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

	/**
	 * Lock the complete Department hierarchy deterministically and validate it.
	 * Optionally verify that every existing row still has its exact normalized
	 * Organization Unit before any legacy mutation is allowed.
	 *
	 * @param bool $verify_projection
	 * @return array|false Rows keyed by Department id
	 */
	private static function lock_hierarchy($verify_projection) {
		$result = db_query("SELECT department_id,department_code,department_name,parent_department_id,"
			."manager_employee_id,cost_center_id,inactive FROM ".TB_PREF."departments "
			."ORDER BY department_id FOR UPDATE", 'could not lock Department hierarchy');
		if (!$result)
			return false;
		$rows = array();
		while ($row = db_fetch_assoc($result)) {
			$department_id = (int)$row['department_id'];
			if ($department_id <= 0 || isset($rows[$department_id]))
				return false;
			$rows[$department_id] = $row;
		}
		foreach ($rows as $department_id => $row) {
			$parent = ($row['parent_department_id'] === null || $row['parent_department_id'] === '')
				? 0 : (int)$row['parent_department_id'];
			if ($parent < 0 || $parent === $department_id || ($parent > 0 && !isset($rows[$parent])))
				return false;
			$seen = array();
			$cursor = $department_id;
			while (isset($rows[$cursor])) {
				if (isset($seen[$cursor]))
					return false;
				$seen[$cursor] = true;
				$next = ($rows[$cursor]['parent_department_id'] === null
					|| $rows[$cursor]['parent_department_id'] === '')
					? 0 : (int)$rows[$cursor]['parent_department_id'];
				if ($next <= 0)
					break;
				$cursor = $next;
			}
		}
		if (!$verify_projection)
			return $rows;
		$legal_entity_id = function_exists('get_hrm_default_legal_entity_binding_id')
			? get_hrm_default_legal_entity_binding_id(true) : false;
		if ($legal_entity_id === false)
			return false;
		$units = array();
		foreach ($rows as $department_id => $row) {
			$unit = get_hrm_organization_unit_by_legacy_department($department_id, true);
			if (!is_array($unit))
				return false;
			$units[$department_id] = $unit;
		}
		foreach ($rows as $department_id => $row) {
			$projection = hrm_organization_unit_projection_from_department($row, $legal_entity_id);
			$parent = ($row['parent_department_id'] === null || $row['parent_department_id'] === '')
				? 0 : (int)$row['parent_department_id'];
			$parent_unit_id = $parent > 0 ? (int)$units[$parent]['organization_unit_id'] : null;
			if (!$projection
				|| !hrm_organization_unit_row_matches_projection($units[$department_id], $projection, $parent_unit_id))
				return false;
		}
		return $rows;
	}

	/**
	 * Return normalized Organization Unit fields changed by a Department update.
	 * Values are never returned, so the list is safe for domain-audit metadata.
	 *
	 * @param array $before
	 * @param array $after
	 * @return array
	 */
	private static function projection_changed_fields($before, $after) {
		$fields = array('department_code','department_name','parent_department_id',
			'manager_employee_id','cost_center_id');
		$changed = array();
		foreach ($fields as $field) {
			$left = isset($before[$field]) ? (string)$before[$field] : '';
			$right = isset($after[$field]) ? (string)$after[$field] : '';
			if ($left !== $right)
				$changed[] = $field;
		}
		return $changed;
	}

	/**
	 * Create a Department and its exact Organization Unit atomically.
	 *
	 * @param array $data
	 * @return bool
	 */
	public static function create($data) {
		if (!hrm_department_writer_authorized())
			return false;
		begin_transaction();
		$e = new static();
		$existing = static::lock_hierarchy(true);
		$result = is_array($existing) && $e->insert($data);
		$department_id = $result ? (int)db_insert_id() : 0;
		if ($result)
			$result = is_array(static::lock_hierarchy(false))
				&& synchronize_hrm_organization_unit_from_department(
					$department_id,
					array('department_code','department_name','parent_department_id',
						'manager_employee_id','cost_center_id','inactive'),
					'create'
				);
		if ($result)
			commit_transaction();
		else
			cancel_transaction();
		return (bool)$result;
	}

	/**
	 * Update a Department and its Organization Unit atomically.
	 *
	 * @param mixed $key
	 * @param array $data
	 * @return bool
	 */
	public static function modify($key, $data) {
		if (!hrm_department_writer_authorized() || (int)$key <= 0)
			return false;
		begin_transaction();
		$e = new static();
		$rows = static::lock_hierarchy(true);
		$before = is_array($rows) && isset($rows[(int)$key]) ? $rows[(int)$key] : false;
		$changed = $before ? static::projection_changed_fields($before, $data) : array();
		$result = $before !== false && $e->update($key, $data);
		if ($result)
			$result = is_array(static::lock_hierarchy(false));
		if ($result && !empty($changed))
			$result = synchronize_hrm_organization_unit_from_department(
				(int)$key, $changed, 'update'
			);
		if ($result)
			commit_transaction();
		else
			cancel_transaction();
		return (bool)$result;
	}

	/**
	 * Synchronize the legacy inactive flag with Organization Unit status.
	 *
	 * @param int $department_id
	 * @param bool $inactive
	 * @return bool
	 */
	public static function set_inactive($department_id, $inactive) {
		if (!hrm_department_writer_authorized() || (int)$department_id <= 0)
			return false;
		begin_transaction();
		$rows = static::lock_hierarchy(true);
		$current = is_array($rows) && isset($rows[(int)$department_id])
			? (int)$rows[(int)$department_id]['inactive'] : -1;
		$inactive = $inactive ? 1 : 0;
		$result = $current !== -1;
		if ($result && $current !== $inactive)
			$result = db_query("UPDATE ".TB_PREF."departments SET inactive=".$inactive
				." WHERE department_id=".(int)$department_id,
				'could not update Department inactive status');
		if ($result && $current !== $inactive)
			$result = synchronize_hrm_organization_unit_from_department(
				(int)$department_id, array('inactive'), 'inactive'
			);
		if ($result)
			commit_transaction();
		else
			cancel_transaction();
		return (bool)$result;
	}

	/**
	 * Preserve normalized Assignment/audit history by denying physical deletion.
	 * Pre-foundation databases retain the legacy base-entity behavior for upgrade
	 * compatibility, but current normalized companies must inactivate the row.
	 *
	 * @param mixed $key
	 * @return bool
	 */
	public static function remove($key) {
		if (!hrm_department_writer_authorized())
			return false;
		if (hrm_organization_unit_table_ready())
			return false;
		$e = new static();
		return $e->delete($key);
	}

	/** Get department name by id. */
	public static function name($id) {
		$row = static::find($id);
		return $row ? $row['department_name'] : '';
	}
}

/**
 * Render the existing inactive checkbox while routing mutations through the
 * normalized Department writer instead of the generic one-table status helper.
 *
 * @param int $department_id
 * @param mixed $value
 * @return void
 */
function hrm_department_inactive_control_cell($department_id, $value) {
	$name = 'Inactive'.$department_id;
	$value = $value ? 1 : 0;
	if (check_value('show_inactive')) {
		if (isset($_POST['LInact'][$department_id])
			&& (get_post('_Inactive'.$department_id.'_update') || get_post('Update'))
			&& (check_value('Inactive'.$department_id) != $value)) {
			if (!departments_entity::set_inactive($department_id, !$value))
				display_error(_('The Department status could not be synchronized.'));
		}
		echo '<td align="center">'.checkbox(null, $name, $value, true, '')
			.hidden("LInact[$department_id]", $value, false).'</td>';
	}
}
