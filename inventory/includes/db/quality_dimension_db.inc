<?php
/**
 * Quality inspection, recall campaign and warranty claim retained metadata custody (B02).
 * Native SQL keeps its business authority; this only joins the existing ordinary
 * Dimension lifecycle protocol (lock, transition check, exact completion check).
 */
require_once __DIR__.'/../../../dimensions/includes/dimension_master_metadata_writer.inc';

/**
 * Fixed owners admitted by this helper; no caller-selected table.
 *
 * @return array Owner table => native primary key
 */
function quality_dimension_sources()
{
	return array(
		'quality_inspections' => 'id',
		'recall_campaigns' => 'id',
		'warranty_claims' => 'id',
	);
}

/**
 * Run one native SQL command of an owner under the ordinary writer fence.
 * Omitted custom_data must survive byte-exactly; these native writers never write
 * metadata, so a birth passes array('custom_data' => null).
 *
 * @param string   $owner   One of quality_dimension_sources()
 * @param int|null $key     Existing native id, or null for a native INSERT
 * @param array    $next    Empty, or array('custom_data'=>null) for a birth
 * @param callable $persist Native SQL; must return a truthy result
 * @param bool     $remove  Native DELETE of a proven-unused row
 * @return mixed Native result, or false when refused/rolled back
 */
function quality_dimension_write($owner, $key, $next, $persist, $remove = false)
{
	$sources = quality_dimension_sources();
	if (!isset($sources[$owner]) || !is_array($next) || !is_callable($persist)
		|| array_diff(array_keys($next), array('custom_data')))
		return false;

	return dimension_master_metadata_write($owner, $key, $next, function($admitted) use ($persist) {
		return call_user_func($persist);
	}, $remove);
}

/**
 * Same as quality_dimension_write() for writers that run inside receipt/recall flows
 * and whose callers ignore results: a refusal (the fence has already rolled the whole
 * transaction back) throws so the surrounding operation cannot continue.
 *
 * @param string   $owner
 * @param int|null $key
 * @param array    $next
 * @param callable $persist
 * @param bool     $remove
 * @return mixed Native result
 * @throws DimensionFinancialReferenceWriteException
 */
function quality_dimension_guard($owner, $key, $next, $persist, $remove = false)
{
	$result = quality_dimension_write($owner, $key, $next, $persist, $remove);
	if (!$result)
		dimension_financial_reference_failure();

	return $result;
}

/**
 * Delete an owner row together with its dependent child rows (readings, parts or
 * items). Refused when the row retains a Dimension reference; the child removal is
 * rolled back with it.
 *
 * @param string $owner      quality_inspections, warranty_claims or recall_campaigns
 * @param int    $id         Parent id
 * @param string $child_sql  Native DELETE of the dependent rows
 * @param string $parent_sql Native DELETE of the parent row
 * @return bool
 */
function quality_dimension_remove_with_children($owner, $id, $child_sql, $parent_sql)
{
	$id = (int)$id;
	if ($id <= 0)
		return false;

	return (bool)quality_dimension_write($owner, $id, array(), function() use ($child_sql, $parent_sql) {
		return db_query($child_sql, 'could not delete dependent rows') && db_query($parent_sql, 'could not delete parent row');
	}, true);
}
