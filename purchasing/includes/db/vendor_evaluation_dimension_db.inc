<?php
/**
 * Vendor evaluation and performance-log retained metadata custody (B02).
 * Native SQL keeps its business authority; this only joins the existing ordinary
 * Dimension lifecycle protocol (lock, transition check, exact completion check).
 */
require_once __DIR__.'/../../../dimensions/includes/dimension_master_metadata_writer.inc';

/**
 * Fixed vendor evaluation owners admitted by this helper; no caller-selected table.
 *
 * @return array Owner table => native primary key
 */
function vendor_evaluation_dimension_sources()
{
	return array('vendor_evaluations'=>'id', 'vendor_performance_log'=>'id');
}

/**
 * Run one native evaluation/log SQL command under the ordinary writer fence.
 * Omitted custom_data must survive byte-exactly; admitted custom_data must keep
 * every retained reference path and may add only open references on their axis.
 *
 * @param string   $owner   vendor_evaluations or vendor_performance_log
 * @param int|null $key     Existing native id, or null for a native INSERT
 * @param array    $next    Empty, or array('custom_data'=>encoded|null)
 * @param callable $persist Native SQL; must return a truthy result
 * @param bool     $remove  Native DELETE of a proven-unused row
 * @return mixed Native result, or false when refused/rolled back
 */
function vendor_evaluation_dimension_write($owner, $key, $next, $persist, $remove = false)
{
	$sources = vendor_evaluation_dimension_sources();
	if (!isset($sources[$owner]) || !is_array($next) || !is_callable($persist)
		|| array_diff(array_keys($next), array('custom_data')))
		return false;

	return dimension_master_metadata_write($owner, $key, $next, function($admitted) use ($persist) {
		return call_user_func($persist);
	}, $remove);
}

/**
 * Same as vendor_evaluation_dimension_write() for internal writers whose native
 * callers historically ignored results: a refusal unwinds the whole transaction.
 *
 * @param string   $owner
 * @param int|null $key
 * @param array    $next
 * @param callable $persist
 * @return mixed Native result
 * @throws DimensionFinancialReferenceWriteException
 */
function vendor_evaluation_dimension_guard($owner, $key, $next, $persist)
{
	$result = vendor_evaluation_dimension_write($owner, $key, $next, $persist);
	if (!$result)
		dimension_financial_reference_failure();

	return $result;
}

/**
 * Run the native supplier vendor-metric UPDATE as an ordinary supplier maintenance
 * write: the supplier row and its retained Dimension masters are locked, and its
 * scalar Dimension slots and custom_data must be byte-identical after the SQL.
 *
 * @param int    $supplier_id
 * @param string $sql Native UPDATE of metric columns only
 * @return bool
 * @throws DimensionFinancialReferenceWriteException
 */
function vendor_supplier_metrics_dimension_guard($supplier_id, $sql)
{
	$supplier_id = dimension_writer_reference_id($supplier_id);
	if (!$supplier_id || !is_string($sql))
		dimension_financial_reference_failure();

	$result = dimension_ordinary_master_write('suppliers', $supplier_id, array(), function($admitted) use ($sql, $supplier_id) {
		$read = 'SELECT dimension_id, dimension2_id, custom_data FROM '.TB_PREF.'suppliers WHERE supplier_id='.$supplier_id.' FOR UPDATE';
		$rows = db_query($read);
		$before = $rows ? db_fetch_assoc($rows) : false;
		if (!$before || !db_query($sql, 'could not update supplier vendor metrics'))
			return false;
		$rows = db_query($read);
		$after = $rows ? db_fetch_assoc($rows) : false;
		return $after === $before;
	}, true);
	if (!$result)
		dimension_financial_reference_failure();

	return true;
}

/**
 * Lock one evaluation and return its current status, or false when absent.
 * Used inside the fence so status checks and writes see the same locked row.
 *
 * @param int $evaluation_id
 * @return string|false
 */
function vendor_evaluation_locked_status($evaluation_id)
{
	$rows = db_query('SELECT status FROM '.TB_PREF.'vendor_evaluations WHERE id='.(int)$evaluation_id.' FOR UPDATE');
	if (!$rows || db_num_rows($rows) !== 1)
		return false;
	$row = db_fetch_assoc($rows);

	return (string)$row['status'];
}
