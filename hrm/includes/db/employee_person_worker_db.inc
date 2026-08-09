<?php
/**
 * HRM-FND-001 person/worker compatibility foundation.
 *
 * The legacy employees row remains authoritative for current application and
 * payroll behavior. These helpers maintain a company-scoped shadow identity
 * graph without merging duplicate candidates or exposing a new route.
 */

if (!function_exists('append_domain_audit_event'))
	include_once($path_to_root.'/includes/db/domain_audit_db.inc');

/**
 * Return the numeric actor identifier without exposing login information.
 *
 * @return int
 */
function hrm_person_worker_actor_id()
{
	if (function_exists('user_id'))
		return (int)user_id();
	if (isset($_SESSION['wa_current_user']->user))
		return (int)$_SESSION['wa_current_user']->user;
	return 0;
}

/**
 * Return whether one company-scoped foundation table exists.
 *
 * Kept local so migration seeding does not load the complete employee writer.
 *
 * @param string $table
 * @return bool
 */
function hrm_person_worker_table_exists($table)
{
	$result = db_query("SHOW TABLES LIKE ".db_escape(TB_PREF.$table));
	return $result && db_num_rows($result) === 1;
}

/**
 * Return true only when every foundation table exists for this company.
 *
 * @return bool
 */
function hrm_person_worker_tables_ready()
{
	foreach (array('hrm_persons', 'hrm_person_names', 'hrm_workers',
		'hrm_employee_worker_map') as $table) {
		if (!hrm_person_worker_table_exists($table))
			return false;
	}
	return true;
}

/**
 * Validate and normalize an as-of date/time without accepting SQL fragments.
 *
 * @param string $as_of
 * @return string|false
 */
function hrm_person_worker_normalize_as_of($as_of)
{
	$as_of = trim((string)$as_of);
	if (preg_match('/^\d{4}-\d{2}-\d{2}$/D', $as_of))
		$as_of .= ' 23:59:59';
	if (!preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/D', $as_of))
		return false;
	$parsed = DateTime::createFromFormat('!Y-m-d H:i:s', $as_of);
	return $parsed && $parsed->format('Y-m-d H:i:s') === $as_of
		? $as_of : false;
}

/**
 * Read one database UTC timestamp for a complete effective-date mutation.
 *
 * @return string|false
 */
function hrm_person_worker_utc_now()
{
	$result = db_query("SELECT UTC_TIMESTAMP() utc_now",
		'could not read HRM person/worker UTC timestamp');
	$row = $result ? db_fetch_assoc($result) : false;
	if (!$row || !isset($row['utc_now']))
		return false;
	return hrm_person_worker_normalize_as_of((string)$row['utc_now']);
}

/**
 * Restrict the new identity read helper to established HRM capabilities.
 *
 * @return bool
 */
function hrm_person_worker_user_can_read()
{
	if (!function_exists('user_check_access'))
		return false;
	foreach (array('SA_EMPLOYEE', 'SA_IMPORTEMP', 'SA_EMPLOYEEREP',
		'SA_EMPHISTORY') as $area) {
		if (user_check_access($area))
			return true;
	}
	return false;
}

/**
 * Append a PII-free person/worker binding event in the caller transaction.
 *
 * @param string $action
 * @param array $identity
 * @param array $changed_fields
 * @param string $reason_code
 * @return array|false
 */
function append_hrm_person_worker_domain_audit(
	$action,
	$identity,
	$changed_fields,
	$reason_code
) {
	if (!is_array($identity) || empty($identity['person_id'])
		|| empty($identity['worker_id']) || empty($identity['employee_number'])
		|| !is_array($changed_fields) || trim((string)$action) === ''
		|| trim((string)$reason_code) === '')
		return false;

	$changed_fields = array_values(array_unique($changed_fields));
	sort($changed_fields);
	return append_domain_audit_event(array(
		'company_id' => function_exists('user_company') ? (int)user_company() : 0,
		'legal_entity_id' => 0,
		'entity_type' => 'hrm_person_worker_binding',
		'entity_key' => array(
			'person_id' => (int)$identity['person_id'],
			'worker_id' => (int)$identity['worker_id'],
			'employee_number' => (int)$identity['employee_number'],
		),
		'action' => (string)$action,
		'reason_code' => (string)$reason_code,
		'before' => null,
		'after' => array('binding_state_changed' => true),
		'restricted_diff' => array(
			'changed_fields' => $changed_fields,
			'restricted_values_stored' => false,
		),
	));
}

/**
 * Load one current compatibility binding, optionally taking a row lock.
 *
 * @param int|string $employee_ref
 * @param bool $for_update
 * @return array|false
 */
function get_hrm_person_worker_binding($employee_ref, $for_update = false)
{
	if (!hrm_person_worker_tables_ready())
		return false;
	$column = is_numeric($employee_ref) ? 'employee_number' : 'employee_id';
	$value = is_numeric($employee_ref) ? (int)$employee_ref
		: trim((string)$employee_ref);
	if (($column === 'employee_number' && $value <= 0)
		|| ($column === 'employee_id' && $value === ''))
		return false;
	$sql = "SELECT m.employee_number,m.employee_id,m.worker_id,m.retired_at,"
		."w.person_id,w.worker_number,w.active_from,w.active_to,w.worker_status "
		."FROM ".TB_PREF."hrm_employee_worker_map m "
		."INNER JOIN ".TB_PREF."hrm_workers w ON w.worker_id=m.worker_id "
		."WHERE m.".$column."=".db_escape($value)." LIMIT 1";
	if ($for_update)
		$sql .= " FOR UPDATE";
	$result = db_query($sql, 'could not read person/worker compatibility binding');
	return $result ? db_fetch_assoc($result) : false;
}

/**
 * Create a one-person/one-worker shadow binding from one legacy employee.
 *
 * @param array $employee
 * @param bool $audit
 * @param string $source
 * @return array|false
 */
function create_hrm_person_worker_from_employee(
	$employee,
	$audit = true,
	$source = 'runtime_employee'
) {
	if (!is_array($employee) || !hrm_person_worker_tables_ready())
		return false;
	$employee_number = isset($employee['employee_number'])
		? (int)$employee['employee_number'] : 0;
	$employee_id = isset($employee['employee_id'])
		? trim((string)$employee['employee_id']) : '';
	$first_name = isset($employee['first_name'])
		? trim((string)$employee['first_name']) : '';
	$last_name = isset($employee['last_name'])
		? trim((string)$employee['last_name']) : '';
	$middle_name = isset($employee['middle_name'])
		? trim((string)$employee['middle_name']) : '';
	if ($employee_number <= 0 || $employee_id === ''
		|| $first_name === '' || $last_name === '')
		return false;
	if (get_hrm_person_worker_binding($employee_number, true)
		|| get_hrm_person_worker_binding($employee_id, true))
		return false;

	$actor_id = hrm_person_worker_actor_id();
	if (!db_query("INSERT INTO ".TB_PREF."hrm_persons "
		."(person_status,created_by) VALUES ('active',".(int)$actor_id.")",
		'could not create HRM person'))
		return false;
	$person_id = (int)db_insert_id();
	if ($person_id <= 0)
		return false;
	$name_effective_from = $source === 'legacy_employee_seed'
		? 'NULL' : 'UTC_TIMESTAMP()';
	if (!db_query("INSERT INTO ".TB_PREF."hrm_person_names "
		."(person_id,first_name,middle_name,last_name,effective_from,"
		."source,verification_status,created_by) VALUES ("
		.(int)$person_id.",".db_escape($first_name).",".db_escape($middle_name)
		.",".db_escape($last_name).",".$name_effective_from.","
		.db_escape($source).",'unverified',".(int)$actor_id.")",
		'could not create HRM person name'))
		return false;

	$active_from = !empty($employee['hire_date'])
		? db_escape((string)$employee['hire_date']) : 'NULL';
	$active_to = !empty($employee['released_date'])
		? db_escape((string)$employee['released_date']) : 'NULL';
	$status = !empty($employee['inactive']) ? 'inactive' : 'active';
	if (!db_query("INSERT INTO ".TB_PREF."hrm_workers "
		."(person_id,worker_number,active_from,active_to,worker_status,created_by) VALUES ("
		.(int)$person_id.",".db_escape($employee_id).",".$active_from.","
		.$active_to.",".db_escape($status).",".(int)$actor_id.")",
		'could not create HRM worker'))
		return false;
	$worker_id = (int)db_insert_id();
	if ($worker_id <= 0)
		return false;
	if (!db_query("INSERT INTO ".TB_PREF."hrm_employee_worker_map "
		."(employee_number,employee_id,worker_id,migration_source,created_by) VALUES ("
		.(int)$employee_number.",".db_escape($employee_id).",".(int)$worker_id
		.",".db_escape($source).",".(int)$actor_id.")",
		'could not create legacy employee compatibility map'))
		return false;

	$identity = array('person_id' => $person_id, 'worker_id' => $worker_id,
		'employee_number' => $employee_number);
	if ($audit && !append_hrm_person_worker_domain_audit(
		'hrm.person_worker.created', $identity,
		array('person', 'person_name', 'worker', 'legacy_map'),
		'hrm_person_worker_created'
	))
		return false;
	return $identity;
}

/**
 * Synchronize mutable legacy name and worker-state projection fields.
 *
 * @param array $employee
 * @return bool
 */
function synchronize_hrm_person_worker_from_employee($employee)
{
	if (!is_array($employee) || empty($employee['employee_number']))
		return false;
	$binding = get_hrm_person_worker_binding(
		(int)$employee['employee_number'], true
	);
	if (!$binding || $binding['retired_at'] !== null)
		return false;
	$changed = array();
	$status = !empty($employee['inactive']) ? 'inactive' : 'active';
	$active_from = !empty($employee['hire_date'])
		? (string)$employee['hire_date'] : null;
	$active_to = !empty($employee['released_date'])
		? (string)$employee['released_date'] : null;
	if ((string)$binding['worker_number'] !== (string)$employee['employee_id']
		|| $binding['active_from'] !== $active_from
		|| $binding['active_to'] !== $active_to
		|| (string)$binding['worker_status'] !== $status) {
		if (!db_query("UPDATE ".TB_PREF."hrm_workers SET "
			."worker_number=".db_escape((string)$employee['employee_id']).","
			."active_from=".($active_from === null ? 'NULL' : db_escape($active_from)).","
			."active_to=".($active_to === null ? 'NULL' : db_escape($active_to)).","
			."worker_status=".db_escape($status)
			." WHERE worker_id=".(int)$binding['worker_id'],
			'could not synchronize HRM worker'))
			return false;
		$changed[] = 'worker_projection';
	}

	$name_result = db_query("SELECT name_id,first_name,middle_name,last_name "
		."FROM ".TB_PREF."hrm_person_names WHERE person_id=".(int)$binding['person_id']
		." AND effective_to IS NULL ORDER BY name_id DESC LIMIT 1 FOR UPDATE",
		'could not read current HRM person name');
	$current_name = $name_result ? db_fetch_assoc($name_result) : false;
	$first_name = trim((string)$employee['first_name']);
	$middle_name = isset($employee['middle_name'])
		? trim((string)$employee['middle_name']) : '';
	$last_name = trim((string)$employee['last_name']);
	if (!$current_name || (string)$current_name['first_name'] !== $first_name
		|| (string)$current_name['middle_name'] !== $middle_name
		|| (string)$current_name['last_name'] !== $last_name) {
		if ($first_name === '' || $last_name === '')
			return false;
		$changed_at = hrm_person_worker_utc_now();
		if ($changed_at === false)
			return false;
		if ($current_name && !db_query("UPDATE ".TB_PREF."hrm_person_names "
			."SET effective_to=".db_escape($changed_at)
			." WHERE name_id=".(int)$current_name['name_id']
			." AND effective_to IS NULL", 'could not close HRM person name'))
			return false;
		if (!db_query("INSERT INTO ".TB_PREF."hrm_person_names "
			."(person_id,first_name,middle_name,last_name,effective_from,source,"
			."verification_status,created_by) VALUES ("
			.(int)$binding['person_id'].','.db_escape($first_name).','
			.db_escape($middle_name).','.db_escape($last_name).','
			.db_escape($changed_at).",'runtime_employee','unverified',"
			.hrm_person_worker_actor_id().')', 'could not version HRM person name'))
			return false;
		$changed[] = 'person_name_version';
	}
	if (empty($changed))
		return true;
	return append_hrm_person_worker_domain_audit(
		'hrm.person_worker.synchronized', $binding, $changed,
		'hrm_person_worker_synchronized'
	) !== false;
}

/**
 * Retire a legacy binding without deleting person/worker/name history.
 *
 * @param int|string $employee_ref
 * @return bool
 */
function retire_hrm_person_worker_binding($employee_ref)
{
	$binding = get_hrm_person_worker_binding($employee_ref, true);
	if (!$binding || $binding['retired_at'] !== null)
		return false;
	$now = hrm_person_worker_utc_now();
	if ($now === false)
		return false;
	if (!db_query("UPDATE ".TB_PREF."hrm_employee_worker_map SET retired_at="
		.db_escape($now)." WHERE employee_number=".(int)$binding['employee_number']
		." AND retired_at IS NULL", 'could not retire legacy employee map'))
		return false;
	if (!db_query("UPDATE ".TB_PREF."hrm_workers SET worker_status='inactive',"
		."active_to=COALESCE(active_to,".db_escape(substr($now, 0, 10)).")"
		." WHERE worker_id=".(int)$binding['worker_id'],
		'could not retire HRM worker'))
		return false;
	return append_hrm_person_worker_domain_audit(
		'hrm.person_worker.retired', $binding,
		array('legacy_map_retired_at', 'worker_status', 'worker_active_to'),
		'hrm_person_worker_retired'
	) !== false;
}

/**
 * Build the authorized as-of person/worker projection SQL.
 *
 * @param int|string $employee_ref
 * @param string $as_of
 * @return string|false
 */
function get_hrm_person_worker_as_of_sql($employee_ref, $as_of)
{
	$as_of = hrm_person_worker_normalize_as_of($as_of);
	if ($as_of === false)
		return false;
	$column = is_numeric($employee_ref) ? 'employee_number' : 'employee_id';
	$value = is_numeric($employee_ref) ? (int)$employee_ref
		: trim((string)$employee_ref);
	if (($column === 'employee_number' && $value <= 0)
		|| ($column === 'employee_id' && $value === ''))
		return false;
	$as_of_sql = db_escape($as_of);
	return "SELECT m.employee_number,m.employee_id,w.worker_id,w.person_id,"
		."w.worker_number,w.active_from,w.active_to,"
		."n.first_name,n.middle_name,n.last_name,n.verification_status "
		."FROM ".TB_PREF."hrm_employee_worker_map m "
		."INNER JOIN ".TB_PREF."hrm_workers w ON w.worker_id=m.worker_id "
		."INNER JOIN ".TB_PREF."hrm_person_names n ON n.person_id=w.person_id "
		."WHERE m.".$column."=".db_escape($value)
		." AND (m.retired_at IS NULL OR m.retired_at>".$as_of_sql.")"
		." AND (w.active_from IS NULL OR w.active_from<=DATE(".$as_of_sql."))"
		." AND (w.active_to IS NULL OR w.active_to>=DATE(".$as_of_sql."))"
		." AND (n.effective_from IS NULL OR n.effective_from<=".$as_of_sql.")"
		." AND (n.effective_to IS NULL OR n.effective_to>".$as_of_sql.")"
		." ORDER BY n.effective_from DESC,n.name_id DESC LIMIT 1";
}

/**
 * Return one authorized company-scoped person/worker state as of a date/time.
 *
 * @param int|string $employee_ref
 * @param string $as_of
 * @return array|false
 */
function get_hrm_person_worker_as_of($employee_ref, $as_of)
{
	if (!hrm_person_worker_user_can_read() || !hrm_person_worker_tables_ready())
		return false;
	$sql = get_hrm_person_worker_as_of_sql($employee_ref, $as_of);
	if ($sql === false)
		return false;
	$result = db_query($sql, 'could not read HRM person/worker as-of state');
	return $result ? db_fetch_assoc($result) : false;
}
