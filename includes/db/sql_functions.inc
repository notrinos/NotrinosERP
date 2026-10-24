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
//
//	General database functions common for all modules.
//
//-------------------------------------------------------------------
// Multilevel transaction control.
//
$transaction_level = 0;

/**
 * Preserve a caller-visible insert identity after a command commits internally.
 * COMMIT resets native driver insert-id APIs. The override is connection/company
 * bound and each driver's next query discards it, preserving normal query scope.
 */
function db_preserve_insert_id($identity) {
	global $db;
	if (!$db || (!is_int($identity) && !is_string($identity))
		|| !preg_match('/^[1-9][0-9]*$/D', (string)$identity)
		|| (int)$identity <= 0 || (string)(int)$identity !== (string)$identity)
		return false;
	$GLOBALS['db_preserved_insert_id'] = array('connection'=>$db,
		'company'=>isset($_SESSION['wa_current_user']->cur_con) ? $_SESSION['wa_current_user']->cur_con : 0,
		'identity'=>$identity);
	return true;
}

/** Begin or nest a transaction; unresolved resource cleanup prevents a new owner. */
function begin_transaction() {
	global $transaction_level;

	if (!empty($GLOBALS['transaction_resources_releasing']))
		return false;
	if (!$transaction_level && !empty($GLOBALS['transaction_resource_releases']))
		throw new RuntimeException('Transaction resource cleanup is pending.');
	if (!$transaction_level) {
		if (db_query("BEGIN", 'could not start a transaction') === false)
			return false;
	}
	$transaction_level++;
	return true;
}

/** Commit only the outer transaction, then release its registered resources. */
function commit_transaction() {
	global $transaction_level;

	if (!empty($GLOBALS['transaction_resources_releasing']) || $transaction_level <= 0)
		return false;
	if ($transaction_level > 1) {
		$transaction_level--;
		return true;
	}
	if (db_query("COMMIT", 'could not commit a transaction') === false)
		return false;
	$transaction_level = 0;
	return release_transaction_resources();
}

/*
	This function is called on end of script execution to cancel
	all aborted transactions (if any)
*/
/** Roll back the whole transaction before cleanup, including shutdown cancellation. */
function cancel_transaction() {
	global $transaction_level;

	if (!empty($GLOBALS['transaction_resources_releasing']))
		return false;
	if ($transaction_level && db_query("ROLLBACK", 'could not cancel a transaction') === false)
		return false;
	$transaction_level = 0;
	return release_transaction_resources();
}

/**
 * Register one named cleanup function for resources held until outer SQL completion.
 * Only trusted source callers register functions; this is not a business posting hook.
 * Exact duplicate registration is idempotent; conflicting ownership fails closed.
 * @return bool
 */
function register_transaction_resource_release($name, $callback) {
	global $transaction_level;
	if ($transaction_level <= 0 || !empty($GLOBALS['transaction_resources_releasing'])
		|| !is_string($name) || !preg_match('/^[a-z][a-z0-9_.:-]{0,95}$/D', $name)
		|| !is_string($callback) || !is_callable($callback))
		return false;
	if (!isset($GLOBALS['transaction_resource_releases']))
		$GLOBALS['transaction_resource_releases'] = array();
	if (isset($GLOBALS['transaction_resource_releases'][$name]))
		return $GLOBALS['transaction_resource_releases'][$name] === $callback;
	if (count($GLOBALS['transaction_resource_releases']) >= 32)
		return false;
	$GLOBALS['transaction_resource_releases'][$name] = $callback;
	return true;
}

/**
 * Run cleanup only after SQL COMMIT/ROLLBACK; retain uncertain releases for recovery.
 * A cleanup failure cannot undo a completed commit. It returns false, prevents a
 * new transaction and permits explicit cleanup retry through cancel_transaction().
 * @return bool
 */
function release_transaction_resources() {
	global $transaction_level;
	if ($transaction_level !== 0 || !empty($GLOBALS['transaction_resources_releasing']))
		return false;
	if (empty($GLOBALS['transaction_resource_releases']))
		return true;
	$GLOBALS['transaction_resources_releasing'] = true;
	$releases = $GLOBALS['transaction_resource_releases'];
	foreach ($releases as $name => $callback) {
		$released = false;
		try {
			$released = call_user_func($callback) === true;
		} catch (Exception $exception) {
			$released = false;
		} catch (Throwable $exception) {
			$released = false;
		}
		if ($released)
			unset($GLOBALS['transaction_resource_releases'][$name]);
	}
	$GLOBALS['transaction_resources_releasing'] = false;
	return empty($GLOBALS['transaction_resource_releases']);
}

//-----------------------------------------------------------------------------
//	Update record activity status.
//
function update_record_status($id, $status, $table, $key) {
	$sql = "UPDATE ".TB_PREF.$table." SET inactive = ".((int)$status)." WHERE $key=".db_escape($id);
		
	db_query($sql, "Can't update record status");
}
//-----------------------------------------------------------------------------
//
//	Helper for sql subquery returning running totals from delta tables like stock_moves or bank_trans
//
//	$table - table name with optional WHERE clause
//  $column - delta column
//	$index  - comma delimited list of columns for total grouping and order
//  Returns running totals with respective index column
//
function running_total_sql($table, $column, $index) {

	return "SELECT daily.$index, daily.$column, (@total:=@total+daily.$column) total 
		FROM
		(SELECT $index, sum($column) $column FROM $table GROUP BY $index ORDER BY $index) daily,
		(SELECT @total:=0) total_var";
}
