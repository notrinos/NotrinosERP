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
	$result = db_query("SHOW TABLES LIKE '".TB_PREF.db_escape_function($table)."'");
	return $result && db_num_rows($result) === 1;
}

/**
 * Return true only when every foundation table exists for this company.
 *
 * @return bool
 */
function hrm_person_worker_tables_ready()
{
    if(function_exists('pay_glb_001_native_reader_schema_cached')
        && pay_glb_001_native_reader_schema_cached('person_worker'))return true;
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
 * Return the exact active company identifier for an identity operation.
 *
 * Person and Worker identifiers are database-local. Absence of an authenticated
 * company context therefore fails closed instead of falling back to a shared or
 * inferred identity namespace.
 *
 * @return int|false
 */
function hrm_person_worker_current_company_id()
{
	if (!function_exists('user_company'))
		return false;
	$company = user_company();
	if (is_int($company))
		return $company >= 0 ? $company : false;
	if (!is_string($company) || preg_match('/^(?:0|[1-9][0-9]*)$/D', $company) !== 1)
		return false;
	return (int)$company;
}

/**
 * Confirm that an explicit company scope is exactly the active company.
 *
 * No cross-company database switching, Person lookup, linking or global
 * identity write is performed by HRM-FND-001.
 *
 * @param int|string $company_id
 * @return bool
 */
function hrm_person_worker_company_scope_is_current($company_id)
{
	$current = hrm_person_worker_current_company_id();
	if ($current === false)
		return false;
	if (is_int($company_id))
		$requested = $company_id;
	elseif (is_string($company_id)
		&& preg_match('/^(?:0|[1-9][0-9]*)$/D', $company_id) === 1)
		$requested = (int)$company_id;
	else
		return false;
	return $requested === $current;
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
 * Build the canonical-link-aware Person expression for one as-of read.
 *
 * Before a reviewed canonical link becomes effective, the Worker resolves to
 * the immutable pre-link Person snapshot. At the exact link timestamp and
 * afterward it resolves to the current canonical Person. Unlinked Workers keep
 * their current Person binding, preserving the predecessor cohort semantics.
 *
 * @param string $as_of_sql Already escaped SQL datetime literal.
 * @return string
 */
function hrm_person_worker_resolved_person_as_of_sql($as_of_sql)
{
	return "CASE WHEN clw.link_worker_id IS NOT NULL AND cl.linked_at>".$as_of_sql
		." THEN clw.previous_person_id ELSE w.person_id END";
}

/**
 * Build the authorized as-of person/worker projection SQL.
 *
 * The source/database boundary that introduces this query requires the
 * canonical-link evidence tables. A snapshot must either be absent (legacy
 * cohort) or be singular and semantically consistent with the retained link,
 * current Worker ownership and unchanged employee/Worker mapping. Ambiguous or
 * tampered link evidence therefore returns no row instead of silently choosing
 * an identity.
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
	return hrm_person_worker_as_of_projection_sql(
		$as_of_sql, "m.".$column."=".db_escape($value)
	)." ORDER BY n.effective_from DESC,n.name_id DESC LIMIT 1";
}

/**
 * Share the exact identity evidence and interval predicates with batch reads.
 *
 * Arguments are internal SQL fragments, never request values.
 * @param string $as_of_sql
 * @param string $predicate
 * @param string $reference_join
 * @param string $reference_projection
 * @return string
 */
function hrm_person_worker_as_of_projection_sql($as_of_sql, $predicate,
	$reference_join = '', $reference_projection = '')
{
	$resolved_person_sql = hrm_person_worker_resolved_person_as_of_sql($as_of_sql);
	return "SELECT ".$reference_projection."m.employee_number,m.employee_id,w.worker_id,"
		.$resolved_person_sql." person_id,"
		."CASE WHEN clw.link_worker_id IS NULL THEN 0 ELSE 1 END canonical_linked,"
		."w.worker_number,w.active_from,w.active_to,"
		."n.first_name,n.middle_name,n.last_name,n.verification_status "
		."FROM ".TB_PREF."hrm_employee_worker_map m "
		.$reference_join
		."INNER JOIN ".TB_PREF."hrm_workers w ON w.worker_id=m.worker_id "
		."LEFT JOIN ".TB_PREF."hrm_person_canonical_link_workers clw "
		."ON clw.worker_id=w.worker_id AND clw.mapping_id=m.mapping_id "
		."LEFT JOIN ".TB_PREF."hrm_person_canonical_links cl ON cl.link_id=clw.link_id "
		."INNER JOIN ".TB_PREF."hrm_person_names n ON n.person_id=(".$resolved_person_sql.") "
		."WHERE ".$predicate
		." AND (m.retired_at IS NULL OR m.retired_at>".$as_of_sql.")"
		." AND (w.active_from IS NULL OR w.active_from<=DATE(".$as_of_sql."))"
		." AND (w.active_to IS NULL OR w.active_to>=DATE(".$as_of_sql."))"
		." AND (clw.link_worker_id IS NULL OR (cl.link_id IS NOT NULL"
		." AND clw.previous_person_id=cl.alias_person_id"
		." AND clw.canonical_person_id=cl.canonical_person_id"
		." AND w.person_id=cl.canonical_person_id))"
		." AND (SELECT COUNT(*) FROM ".TB_PREF."hrm_person_canonical_link_workers clw_count"
		." WHERE clw_count.worker_id=w.worker_id AND clw_count.mapping_id=m.mapping_id)<=1"
		." AND (n.effective_from IS NULL OR n.effective_from<=".$as_of_sql.")"
		." AND (n.effective_to IS NULL OR n.effective_to>".$as_of_sql.")";
}

/**
 * Resolve a presentation cohort with one readiness check and 200-ref chunks.
 *
 * This completed value is local to a post-command rendering operation. It is
 * not a schema proof, session cache or authority for subsequent reads/writes.
 * The database verifies its own connection/database against the authenticated
 * configuration. Each new operation repeats that verification and readiness.
 * Numeric references retain the single-reader's is_numeric/int semantics.
 *
 * @param array $employee_refs
 * @param string $as_of
 * @return array|false
 */
function get_hrm_person_worker_report_names_batch_as_of($employee_refs, $as_of)
{
	global $db, $db_connections;
	$company = hrm_person_worker_current_company_id();
	$as_of = hrm_person_worker_normalize_as_of($as_of);
	if (!is_array($employee_refs) || $company === false || $as_of === false
		|| !hrm_person_worker_user_can_read() || !isset($db)
		|| !isset($db_connections[$company]['dbname'], $db_connections[$company]['tbpref']))
		return false;
	$result = db_query('SELECT CONNECTION_ID() connection_id,DATABASE() database_name',
		'could not verify HRM name batch scope');
	$scope = $result ? db_fetch_assoc($result) : false;
	if (!$scope || $scope['database_name'] !== $db_connections[$company]['dbname'])
		return false;
	$batch = array('company' => $company, 'connection' => $db,
		'database' => $scope['database_name'], 'connection_id' => $scope['connection_id'],
		'prefix' => $db_connections[$company]['tbpref'], 'as_of' => $as_of, 'names' => array());
	$refs = array();
	foreach ($employee_refs as $ref) {
		if (!is_int($ref) && !is_string($ref))
			continue;
		$key = 'ref:'.(string)$ref;
		$batch['names'][$key] = false;
		$value = is_numeric($ref) ? (int)$ref : trim((string)$ref);
		if ((is_numeric($ref) && $value > 0) || (!is_numeric($ref) && $value !== ''))
			$refs[$key] = array('numeric' => is_numeric($ref), 'value' => $value);
	}
	if (empty($refs))
		return $batch;
	// Deliberately do not publish/reuse the locked native-reader schema proof.
	if (!hrm_person_worker_tables_ready())
		return false;
	foreach (array('hrm_person_canonical_links', 'hrm_person_canonical_link_workers') as $table) {
		if (!hrm_person_worker_table_exists($table))
			return false;
	}
	$groups = array('employee_number' => array(), 'employee_id' => array());
	foreach ($refs as $key => $ref)
		$groups[$ref['numeric'] ? 'employee_number' : 'employee_id'][$key] = $ref;
	foreach ($groups as $column => $group) {
		foreach (array_chunk($group, 200, true) as $chunk) {
			$reference_rows = array();
			$keys = array_keys($chunk);
			foreach (array_values($chunk) as $index => $ref) {
				$reference_rows[] = 'SELECT '.$index.' reference_index,'
					.db_escape($ref['value']).' employee_ref';
			}
			$join = 'INNER JOIN ('.implode(' UNION ALL ', $reference_rows).') batch_refs ON '
				.'m.'.$column.'=batch_refs.employee_ref ';
			$sql = hrm_person_worker_as_of_projection_sql(db_escape($as_of), '1=1',
				$join, 'batch_refs.reference_index,')
				.' AND n.name_id=(SELECT batch_name.name_id FROM '.TB_PREF.'hrm_person_names batch_name'
				.' WHERE batch_name.person_id=n.person_id'
				.' AND (batch_name.effective_from IS NULL OR batch_name.effective_from<='.db_escape($as_of).')'
				.' AND (batch_name.effective_to IS NULL OR batch_name.effective_to>'.db_escape($as_of).')'
				.' ORDER BY batch_name.effective_from DESC,batch_name.name_id DESC LIMIT 1)'
				.' ORDER BY batch_refs.reference_index,n.effective_from DESC,n.name_id DESC';
			$result = db_query($sql, 'could not read HRM report name batch');
			if (!$result)
				return false;
			$seen = array();
			while ($row = db_fetch_assoc($result)) {
				$index = (int)$row['reference_index'];
				if (isset($seen[$index]))
					continue;
				$seen[$index] = true;
				$batch['names'][$keys[$index]] = hrm_person_worker_report_name_from_row($row);
			}
			db_free_result($result);
		}
	}
	return $batch;
}

/**
 * Read only a completed batch value during its uninterrupted render operation.
 *
 * Scope/authorization changes fail closed; no lazy per-option query is issued.
 * Callers must create a new batch after commands or connection/database changes.
 * @param array|false $batch
 * @param int|string $employee_ref
 * @param string $as_of
 * @return array|false
 */
function hrm_person_worker_report_name_from_batch($batch, $employee_ref, $as_of)
{
	global $db, $db_connections;
	if (!is_array($batch) || !hrm_person_worker_user_can_read()
		|| !hrm_person_worker_company_scope_is_current($batch['company'])
		|| $batch['connection'] !== $db
		|| !isset($db_connections[$batch['company']]['dbname'], $db_connections[$batch['company']]['tbpref'])
		|| $batch['prefix'] !== $db_connections[$batch['company']]['tbpref']
		|| $batch['database'] !== $db_connections[$batch['company']]['dbname']
		|| $batch['as_of'] !== hrm_person_worker_normalize_as_of($as_of))
		return false;
	$key = 'ref:'.(string)$employee_ref;
	return isset($batch['names'][$key]) ? $batch['names'][$key] : false;
}

/**
 * Prepare names before formatting an explicitly opted-in employee control.
 *
 * Hidden inactive rows are excluded unless a search submission can select them.
 * Extra roster refs join the same operation. Overlapping controls may reuse
 * completed names after rechecking actual connection/database at their boundary;
 * missing refs get a new bounded operation, never a per-option lazy read.
 * @param array $rows
 * @param array $selected_ids
 * @param array $options
 * @param bool $searching
 * @return void
 */
function hrm_person_worker_prepare_employee_name_batch($rows, $selected_ids, $options, $searching)
{
	$settings = $options['employee_name_batch'];
	$refs = isset($settings['extra_refs']) ? $settings['extra_refs'] : array();
	foreach ($rows as $row) {
		if (!$searching && !$options['show_inactive'] && !empty($row['inactive'])
			&& !in_array((string)$row[0], $selected_ids, true))
			continue;
		$refs[] = $row[0];
	}
	$target = $settings['target'];
	$prior = isset($GLOBALS[$target]) ? $GLOBALS[$target] : false;
	$reuse = is_array($prior) && hrm_person_worker_user_can_read()
		&& hrm_person_worker_company_scope_is_current($prior['company'])
		&& $prior['connection'] === $GLOBALS['db']
		&& isset($GLOBALS['db_connections'][$prior['company']]['tbpref'])
		&& $prior['prefix'] === $GLOBALS['db_connections'][$prior['company']]['tbpref']
		&& $prior['as_of'] === $settings['as_of']
		&& isset($GLOBALS['db_connections'][$prior['company']]['dbname'])
		&& $prior['database'] === $GLOBALS['db_connections'][$prior['company']]['dbname'];
	if ($reuse) {
		$result = db_query('SELECT CONNECTION_ID() connection_id,DATABASE() database_name',
			'could not verify reused HRM names scope');
		$scope = $result ? db_fetch_assoc($result) : false;
		$reuse = $scope && $scope['database_name'] === $prior['database']
			&& (string)$scope['connection_id'] === (string)$prior['connection_id'];
	}
	$missing = array();
	foreach ($refs as $ref) {
		if (!$reuse || !array_key_exists('ref:'.(string)$ref, $prior['names']))
			$missing[] = $ref;
	}
	if ($reuse && empty($missing))
		return;
	$batch = get_hrm_person_worker_report_names_batch_as_of($missing, $settings['as_of']);
	if ($reuse && is_array($batch))
		$batch['names'] = array_merge($prior['names'], $batch['names']);
	$GLOBALS[$target] = $batch;
}

/**
 * Prepare completed names for an already selected visible row cohort.
 *
 * No detail reader or additional cohort query is used. Fields may include both
 * employee and reviewer references. The caller owns the post-command instant.
 * @param array $rows
 * @param array $fields
 * @param string|false $as_of
 * @param string $target
 * @return void
 */
function hrm_person_worker_prepare_row_name_batch($rows, $fields, $as_of, $target)
{
	$refs = array();
	foreach ($rows as $row)
		foreach ($fields as $field)
			if (isset($row[$field]))
				$refs[] = $row[$field];
	hrm_person_worker_prepare_employee_name_batch(array(), array(), array(
		'employee_name_batch' => array('as_of' => $as_of, 'target' => $target,
			'extra_refs' => $refs)), false);
}

/**
 * Stream presentation rows in bounded, independently verified render operations.
 *
 * Buffer at most 200 rows, preserving the original result order and row shape.
 * Each chunk gets fresh scope/readiness and completed names before yielding;
 * no readiness proof or growing name map survives a chunk. SQL result buffering
 * remains the driver's existing behavior. Rendering must not mutate identity.
 * @param resource|object|false $result
 * @param array $fields
 * @param string|false $as_of
 * @param string $target
 * @return Generator
 */
function hrm_person_worker_name_batch_rows($result, $fields, $as_of, $target)
{
	if (!$result)
		return;
	try {
		while (true) {
			$rows = array();
			while (count($rows) < 200 && ($row = db_fetch($result)))
				$rows[] = $row;
			if (empty($rows))
				break;
			$GLOBALS[$target] = false;
			hrm_person_worker_prepare_row_name_batch($rows, $fields, $as_of, $target);
			foreach ($rows as $row)
				yield $row;
		}
	} finally {
		$GLOBALS[$target] = false;
	}
}

/**
 * Return one authorized company-scoped person/worker state as of a date/time.
 *
 * @param int|string $company_id
 * @param int|string $employee_ref
 * @param string $as_of
 * @return array|false
 */
function get_hrm_person_worker_as_of_for_company($company_id, $employee_ref, $as_of)
{
	if (!hrm_person_worker_company_scope_is_current($company_id)
		|| !hrm_person_worker_user_can_read() || !hrm_person_worker_tables_ready())
		return false;
	$sql = get_hrm_person_worker_as_of_sql($employee_ref, $as_of);
	if ($sql === false)
		return false;
	$result = db_query($sql, 'could not read HRM person/worker as-of state');
	return $result ? db_fetch_assoc($result) : false;
}

/**
 * Return one authorized Person/Worker state in the authenticated company.
 *
 * This compatibility wrapper deliberately derives only the current company; it
 * never accepts or switches to a foreign company database. Callers that carry
 * an explicit company identifier should use get_hrm_person_worker_as_of_for_company().
 *
 * @param int|string $employee_ref
 * @param string $as_of
 * @return array|false
 */
function get_hrm_person_worker_as_of($employee_ref, $as_of)
{
	$company_id = hrm_person_worker_current_company_id();
	if ($company_id === false)
		return false;
	return get_hrm_person_worker_as_of_for_company(
		$company_id, $employee_ref, $as_of
	);
}

/**
 * Resolve the canonical-link-aware as-of display name for one employee.
 *
 * This is the bounded adoption helper used by authoritative read surfaces such
 * as the employee-profile report. It returns an array with exactly the
 * normalized name parts plus the bounded canonical-link state the report needs
 * to preserve byte-for-byte legacy-cohort parity, or false when no
 * Person/Worker binding is available so the caller keeps its legacy name
 * (legacy-cohort parity and fail-closed availability).
 *
 * The read is authorized and least-data. It performs no identity write, emits
 * no audit event, and never projects restricted identity/contact/private or
 * payroll values.
 *
 * @param int|string $employee_ref
 * @param string $as_of
 * @param array|false|null $batch Completed render value, or null for the single reader.
 * @return array|false Normalized name parts or false on missing binding.
 */
function get_hrm_person_worker_report_name_as_of($employee_ref, $as_of, $batch = null)
{
	if ($batch !== null)
		return hrm_person_worker_report_name_from_batch($batch, $employee_ref, $as_of);
	$row = get_hrm_person_worker_as_of($employee_ref, $as_of);
	return hrm_person_worker_report_name_from_row($row);
}

/** @param array|false $row @return array|false Normalized least-data name. */
function hrm_person_worker_report_name_from_row($row)
{
	if (!is_array($row))
		return false;
	$first = isset($row['first_name']) ? trim((string)$row['first_name']) : '';
	$middle = isset($row['middle_name']) ? trim((string)$row['middle_name']) : '';
	$last = isset($row['last_name']) ? trim((string)$row['last_name']) : '';
	if ($first === '' && $last === '')
		return false;
	return array(
		'first_name' => $first,
		'middle_name' => $middle,
		'last_name' => $last,
		'canonical_linked' => !empty($row['canonical_linked']) ? 1 : 0
	);
}
