<?php
/**
 * Warehouse configuration master retained metadata custody (B02): storage categories,
 * putaway rules, routes and route rules.
 * Native SQL keeps its business authority; this only joins the existing ordinary
 * Dimension lifecycle protocol (lock, transition check, exact completion check).
 */
require_once __DIR__.'/../../../../dimensions/includes/dimension_master_metadata_writer.inc';

/**
 * Fixed warehouse configuration owners admitted by this helper; no caller-selected table.
 *
 * @return array Owner table => native primary key
 */
function warehouse_config_dimension_sources()
{
	return array(
		'wh_storage_categories' => 'id',
		'wh_putaway_rules' => 'rule_id',
		'wh_routes' => 'route_id',
				'wh_route_rules' => 'rule_id',
		'wh_replenishment_rules' => 'rule_id',
				'wh_cycle_count_plans' => 'plan_id',
				'wh_scrap' => 'scrap_id',
		'wh_picking_waves' => 'wave_id',
				'wh_packages' => 'package_id',
		'wh_return_orders' => 'return_id',
						'wh_return_order_lines' => 'line_id',
		'wh_operations' => 'op_id',
				'wh_operation_lines' => 'line_id',
		'wh_locations' => 'loc_id',
		'wh_bin_stock' => 'id',
		'locations' => 'loc_code',
		'wh_consignment_stock' => 'id',
		'wh_cycle_counts' => 'count_id',
	);
}

/**
 * Run one native warehouse configuration SQL command under the ordinary writer fence.
 * Omitted custom_data must survive byte-exactly; admitted custom_data must keep
 * every retained reference path and may add only open references on their axis.
 *
 * @param string   $owner   One of warehouse_config_dimension_sources()
 * @param int|null $key     Existing native id, or null for a native INSERT
 * @param array    $next    Empty, or array('custom_data'=>encoded|null); a birth that never
 *                          writes metadata must pass array('custom_data'=>null)
 * @param callable $persist Native SQL; must return a truthy result
  * @param bool     $remove  Native DELETE of a proven-unused row
 * @param string|null $final_key Native key of a string-keyed birth (locations); null otherwise
 * @return mixed Native result, or false when refused/rolled back
 */
function warehouse_config_dimension_write($owner, $key, $next, $persist, $remove = false, $final_key = null)
{
	$sources = warehouse_config_dimension_sources();
	if (!isset($sources[$owner]) || !is_array($next) || !is_callable($persist)
		|| array_diff(array_keys($next), array('custom_data')))
		return false;

		return dimension_master_metadata_write($owner, $key, $next, function($admitted) use ($persist) {
		return call_user_func($persist);
	}, $remove, $final_key);
}

/**
 * Clear the default flag of every sibling route of a type and warehouse while holding
 * all sibling rows and their retained Dimension masters. A sibling whose metadata
 * changes (for example through a trigger) makes the whole command fail.
 *
 * @param string      $route_type
 * @param string|null $warehouse_loc_code
 * @param int|null    $exclude_route_id
 * @return bool
 */
function warehouse_route_reset_default($route_type, $warehouse_loc_code = null, $exclude_route_id = null)
{
	$where = 'route_type='.db_escape($route_type)
		.($warehouse_loc_code ? ' AND warehouse_loc_code='.db_escape($warehouse_loc_code) : ' AND warehouse_loc_code IS NULL')
		.($exclude_route_id ? ' AND route_id != '.(int)$exclude_route_id : '');
	$rows = db_query('SELECT * FROM '.TB_PREF.'wh_routes WHERE '.$where.' ORDER BY route_id FOR UPDATE');
	if (!$rows)
		return false;
	$before = array();
	$all = array();
	while ($row = db_fetch_assoc($rows)) {
		$all[] = $row;
		$before[$row['route_id']] = $row['custom_data'];
	}
	if (!dimension_master_metadata_lock_rows($all))
		return false;
	if (!db_query('UPDATE '.TB_PREF.'wh_routes SET is_default=0 WHERE '.$where, 'could not unset default route'))
		return false;
	$rows = db_query('SELECT route_id, custom_data FROM '.TB_PREF.'wh_routes WHERE '.$where.' ORDER BY route_id');
	if (!$rows)
		return false;
	$after = array();
	while ($row = db_fetch_assoc($rows))
		$after[$row['route_id']] = $row['custom_data'];

	return $after === $before;
}

/**
 * Delete one route and all its rules. Refused when the route or any rule retains a
 * Dimension reference (including closed or orphan history).
 *
 * @param int $route_id
 * @return bool
 */
function warehouse_route_dimension_remove($route_id)
{
	$route_id = (int)$route_id;
	if ($route_id <= 0)
		return false;

	return (bool)warehouse_config_dimension_write('wh_routes', $route_id, array(), function() use ($route_id) {
		$rows = db_query('SELECT rule_id, custom_data FROM '.TB_PREF.'wh_route_rules WHERE route_id='.$route_id.' ORDER BY rule_id FOR UPDATE');
		if (!$rows)
			return false;
		while ($rule = db_fetch_assoc($rows))
			if (dimension_master_payload_transition($rule['custom_data'], null, true) === false)
				return false;
		return db_query('DELETE FROM '.TB_PREF.'wh_route_rules WHERE route_id='.$route_id, 'could not delete route rules')
			&& db_query('DELETE FROM '.TB_PREF.'wh_routes WHERE route_id='.$route_id, 'could not delete route');
	}, true);
}

/**
 * Delete every rule of a route, each under the writer fence, so a rule that retains a
 * Dimension reference is never erased; any refusal unwinds the whole set.
 *
 * @param int $route_id
 * @return bool
 */
function warehouse_route_rules_dimension_remove($route_id)
{
	$route_id = (int)$route_id;
	if ($route_id <= 0 || !begin_transaction())
		return false;
	$rows = db_query('SELECT rule_id FROM '.TB_PREF.'wh_route_rules WHERE route_id='.$route_id.' ORDER BY rule_id FOR UPDATE');
	if (!$rows) {
		cancel_transaction();
		return false;
	}
	$ids = array();
	while ($row = db_fetch_assoc($rows))
		$ids[] = (int)$row['rule_id'];
	foreach ($ids as $rule_id) {
		$sql = 'DELETE FROM '.TB_PREF.'wh_route_rules WHERE rule_id='.$rule_id;
		if (!warehouse_config_dimension_write('wh_route_rules', $rule_id, array(), function() use ($sql) {
			return db_query($sql, 'could not delete route rules') ? true : false;
		}, true)) {
			cancel_transaction();
			return false;
		}
	}
	if (!commit_transaction()) {
		cancel_transaction();
		return false;
	}

	return true;
}

/**
 * Run one native status-transition UPDATE of a single warehouse configuration row under
 * the writer fence. Zero affected rows is a normal false result; a refused fence also
 * returns false.
 *
 * @param string $owner   Registered owner
 * @param int    $id      Native id
 * @param string $sql     Native UPDATE
 * @param string $message Native error message
 * @return bool True only when the row was actually changed
 */
function warehouse_config_dimension_status_write($owner, $id, $sql, $message)
{
	$affected = 0;
	$saved = warehouse_config_dimension_write($owner, $id, array(), function() use ($sql, $message, &$affected) {
		if (!db_query($sql, $message))
			return false;
		$affected = db_num_affected_rows();
		return true;
	});

	return $saved && $affected > 0;
}

/**
 * Delete one draft return order and its lines. Refused when the order or any line retains
 * a Dimension reference (including closed or orphan history).
 *
 * @param int $return_id
 * @return bool
 */
function warehouse_return_order_dimension_remove($return_id)
{
	$return_id = (int)$return_id;
	if ($return_id <= 0)
		return false;

	return (bool)warehouse_config_dimension_write('wh_return_orders', $return_id, array(), function() use ($return_id) {
		$rows = db_query('SELECT line_id, custom_data FROM '.TB_PREF.'wh_return_order_lines WHERE return_id='.$return_id.' ORDER BY line_id FOR UPDATE');
		if (!$rows)
			return false;
		while ($line = db_fetch_assoc($rows))
			if (dimension_master_payload_transition($line['custom_data'], null, true) === false)
				return false;
		return db_query('DELETE FROM '.TB_PREF.'wh_return_order_lines WHERE return_id='.$return_id, 'Could not delete return order lines')
			&& db_query('DELETE FROM '.TB_PREF.'wh_return_orders WHERE return_id='.$return_id, 'Could not delete return order');
	}, true);
}

/**
 * Same as warehouse_config_dimension_write() for operation-engine writers whose callers
 * (picking, packing, shipping, putaway, scrap, returns) are void or ignore results: a
 * refusal throws after the fence has rolled the whole surrounding operation back.
 *
 * @param string   $owner
 * @param int|null $key
 * @param array    $next
 * @param callable $persist
 * @param bool     $remove
 * @return mixed Native result
 * @throws DimensionFinancialReferenceWriteException
 */
function warehouse_config_dimension_guard($owner, $key, $next, $persist, $remove = false, $final_key = null)
{
	$result = warehouse_config_dimension_write($owner, $key, $next, $persist, $remove, $final_key);
	if (!$result)
		dimension_financial_reference_failure();

	return $result;
}

/**
 * Apply one native per-line command to every operation line of an operation (optionally
 * narrowed by an extra native predicate), each line under the writer fence in id order.
 * Any refusal throws and unwinds the whole set.
 *
 * @param int      $op_id       Operation id
 * @param callable $sql_for     function(int $line_id): string native command for one line
 * @param string   $message     Native error message
 * @param string   $extra_where Optional extra native predicate (with leading AND)
 * @param bool     $remove      True for a DELETE of each line
 * @return void
 * @throws DimensionFinancialReferenceWriteException
 */
function warehouse_operation_lines_each($op_id, $sql_for, $message, $extra_where = '', $remove = false)
{
	// The set is one unit even when the caller holds no transaction of its own.
	if (!begin_transaction())
		dimension_financial_reference_failure();
	$rows = db_query('SELECT line_id FROM '.TB_PREF.'wh_operation_lines WHERE op_id='.(int)$op_id.' '.$extra_where.' ORDER BY line_id FOR UPDATE');
	if (!$rows)
		dimension_financial_reference_failure();
	$ids = array();
	while ($row = db_fetch_assoc($rows))
		$ids[] = (int)$row['line_id'];
	foreach ($ids as $line_id) {
		$sql = call_user_func($sql_for, $line_id);
				warehouse_config_dimension_guard('wh_operation_lines', $line_id, array(), function() use ($sql, $message) {
			return db_query($sql, $message) ? true : false;
		}, $remove);
	}
	if (!commit_transaction()) {
		cancel_transaction();
		dimension_financial_reference_failure();
	}
}

/**
 * Run one native UPDATE of one existing row (by its native key) under the writer fence.
 * An absent row keeps the native no-op and returns false; a refusal throws.
 *
 * @param string     $owner   Registered owner
 * @param int|string $id      Native key
 * @param string     $sql     Native UPDATE of exactly that key
 * @param string     $message Native error message
 * @return bool True when the row existed and the command ran
 * @throws DimensionFinancialReferenceWriteException
 */
function warehouse_config_dimension_update($owner, $id, $sql, $message)
{
	$sources = warehouse_config_dimension_sources();
	if (!isset($sources[$owner]))
		dimension_financial_reference_failure();
	$exists = db_query('SELECT '.$sources[$owner].' FROM '.TB_PREF.$owner.' WHERE '.$sources[$owner].'='.db_escape($id).' LIMIT 1', 'could not check '.$owner);
	if (!$exists || !db_num_rows($exists))
		return false;
	warehouse_config_dimension_guard($owner, $id, array(), function() use ($sql, $message) {
		return db_query($sql, $message) ? true : false;
	});

	return true;
}

/**
 * Apply one native SET list to every row of an owner matching a native predicate, each row
 * under the writer fence in key order and all of them in one transaction. Any refusal
 * throws and unwinds the whole set.
 *
 * @param string $owner   Registered owner with a numeric key
 * @param string $where   Native predicate
 * @param string $set_sql Native SET list
 * @param string $message Native error message
 * @return int Number of rows processed
 * @throws DimensionFinancialReferenceWriteException
 */
function warehouse_config_dimension_update_where($owner, $where, $set_sql, $message)
{
	$sources = warehouse_config_dimension_sources();
	if (!isset($sources[$owner]) || !begin_transaction())
		dimension_financial_reference_failure();
	$key = $sources[$owner];
	$rows = db_query('SELECT '.$key.' FROM '.TB_PREF.$owner.' WHERE '.$where.' ORDER BY '.$key.' FOR UPDATE', $message);
	if (!$rows)
		dimension_financial_reference_failure();
	$ids = array();
	while ($row = db_fetch_assoc($rows))
		$ids[] = (int)$row[$key];
	foreach ($ids as $id) {
		$sql = 'UPDATE '.TB_PREF.$owner.' SET '.$set_sql.' WHERE '.$key.'='.$id;
		warehouse_config_dimension_guard($owner, $id, array(), function() use ($sql, $message) {
			return db_query($sql, $message) ? true : false;
		});
	}
	if (!commit_transaction()) {
		cancel_transaction();
		dimension_financial_reference_failure();
	}

	return count($ids);
}

/**
 * Run a whole native "UPDATE <table> SET ... WHERE ..." statement of one owner row set under
 * the writer fence (see warehouse_config_dimension_update_where()).
 *
 * @param string $owner      Registered owner
 * @param string $update_sql Native statement (one SET list, one WHERE predicate)
 * @param string $message    Native error message
 * @return int Number of rows processed
 * @throws DimensionFinancialReferenceWriteException
 */
function warehouse_config_dimension_update_statement($owner, $update_sql, $message)
{
	if (!preg_match('/^\s*UPDATE\s+'.preg_quote(TB_PREF.$owner, '/').'\s+SET\s+(.+?)\s+WHERE\s+(.+)$/is', $update_sql, $parts))
		dimension_financial_reference_failure();

	return warehouse_config_dimension_update_where($owner, $parts[2], $parts[1], $message);
}
