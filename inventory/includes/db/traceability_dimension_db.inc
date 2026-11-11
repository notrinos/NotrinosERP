<?php
/**
 * Serial number and stock batch retained metadata custody (B02).
 * Native SQL keeps its business authority; this only joins the existing ordinary
 * Dimension lifecycle protocol (lock, transition check, exact completion check).
 * The two traceability masters are written from many stock-moving flows (receipts,
 * adjustments, transfers, putaway, production, mobile scanning), so a refusal throws
 * after the fence has rolled the whole surrounding operation back.
 */
require_once __DIR__.'/../../../dimensions/includes/dimension_master_metadata_writer.inc';

/**
 * Fixed traceability owners admitted by this helper; no caller-selected table.
 *
 * @return array Owner table => native primary key
 */
function traceability_dimension_sources()
{
	return array(
		'serial_numbers' => 'id',
		'stock_batches' => 'id',
	);
}

/**
 * Run one native serial/batch SQL command under the ordinary writer fence. These native
 * writers never write metadata, so a birth passes array('custom_data' => null).
 *
 * @param string   $owner   serial_numbers or stock_batches
 * @param int|null $key     Existing native id, or null for a native INSERT
 * @param array    $next    Empty, or array('custom_data'=>null) for a birth
 * @param callable $persist Native SQL; must return a truthy result
 * @param bool     $remove  Native DELETE of a proven-unused row
 * @return mixed Native result, or false when refused/rolled back
 */
function traceability_dimension_write($owner, $key, $next, $persist, $remove = false)
{
	$sources = traceability_dimension_sources();
	if (!isset($sources[$owner]) || !is_array($next) || !is_callable($persist)
		|| array_diff(array_keys($next), array('custom_data')))
		return false;

	return dimension_master_metadata_write($owner, $key, $next, function($admitted) use ($persist) {
		return call_user_func($persist);
	}, $remove);
}

/**
 * Same as traceability_dimension_write() for flow writers: a refusal throws.
 *
 * @param string   $owner
 * @param int|null $key
 * @param array    $next
 * @param callable $persist
 * @param bool     $remove
 * @return mixed Native result
 * @throws DimensionFinancialReferenceWriteException
 */
function traceability_dimension_guard($owner, $key, $next, $persist, $remove = false)
{
	$result = traceability_dimension_write($owner, $key, $next, $persist, $remove);
	if (!$result)
		dimension_financial_reference_failure();

	return $result;
}

/**
 * Apply one native `WHERE id=` UPDATE of a serial or batch. A row that does not exist
 * keeps the native no-op; an existing row is updated under the writer fence (throwing
 * on refusal).
 *
 * @param string $owner   serial_numbers or stock_batches
 * @param int    $id      Native id
 * @param string $sql     Native UPDATE of that one row
 * @param string $message Native error message
 * @return void
 * @throws DimensionFinancialReferenceWriteException
 */
function traceability_dimension_update($owner, $id, $sql, $message)
{
	$sources = traceability_dimension_sources();
	$id = (int)$id;
	if (!isset($sources[$owner]) || $id <= 0)
		return;
	$rows = db_query('SELECT id FROM '.TB_PREF.$owner.' WHERE id='.$id.' FOR UPDATE');
	if (!$rows || db_num_rows($rows) !== 1)
		return;
	traceability_dimension_guard($owner, $id, array(), function() use ($sql, $message) {
		return db_query($sql, $message) ? true : false;
	});
}

/**
 * Apply a per-row native UPDATE to a list of serials or batches, each existing row under
 * the writer fence (ascending id order); absent ids keep the native no-op.
 *
 * @param string   $owner   serial_numbers or stock_batches
 * @param array    $ids     Candidate native ids
 * @param callable $sql_for function(int $id): string native UPDATE of that one row
 * @param string   $message Native error message
 * @return void
 * @throws DimensionFinancialReferenceWriteException
 */
function traceability_dimension_update_group($owner, $ids, $sql_for, $message)
{
	$clean = array_values(array_unique(array_filter(array_map('intval', (array)$ids))));
	sort($clean, SORT_NUMERIC);
	foreach ($clean as $id)
		traceability_dimension_update($owner, $id, call_user_func($sql_for, $id), $message);
}
