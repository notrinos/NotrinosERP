<?php
/**
 * Manufacturing owner retained metadata custody (B02): work orders and work-order issue items.
 * Native SQL keeps its business authority; this only joins the existing ordinary
 * Dimension lifecycle protocol (lock, transition check, exact completion check).
 * Both tables declare custom_data NOT NULL DEFAULT '{}', so a native birth that writes no
 * metadata must pass an empty payload (the protocol then expects the stored '{}').
 */
require_once __DIR__.'/../../../dimensions/includes/dimension_master_metadata_writer.inc';

/**
 * Fixed Manufacturing owners admitted by this helper; no caller-selected table.
 *
 * @return array Owner table => native primary key
 */
function manufacturing_dimension_sources()
{
	return array(
		'workorders' => 'id',
		'wo_issue_items' => 'id',
	);
}

/**
 * Run one native Manufacturing SQL command under the ordinary writer fence.
 * Omitted custom_data must survive byte-exactly; admitted custom_data must keep
 * every retained reference path and may add only open references on their axis.
 *
 * @param string   $owner   One of manufacturing_dimension_sources()
 * @param int|null $key     Existing native id, or null for a native INSERT
 * @param callable $persist Native SQL; must return a truthy result
 * @param bool     $remove  Native DELETE of a proven-unused row
 * @return mixed Native result, or false when refused/rolled back
 */
function manufacturing_dimension_write($owner, $key, $persist, $remove = false)
{
	$sources = manufacturing_dimension_sources();
	if (!isset($sources[$owner]) || !is_callable($persist))
		return false;

	return dimension_master_metadata_write($owner, $key, array(), function($admitted) use ($persist) {
		return call_user_func($persist);
	}, $remove);
}

/**
 * Same as manufacturing_dimension_write() for internal writers whose callers are
 * void or ignore results: a refusal (the fence has already rolled the whole
 * transaction back) throws so the surrounding business operation cannot continue.
 *
 * @param string   $owner
 * @param int|null $key
 * @param callable $persist
 * @param bool     $remove
 * @return mixed Native result
 * @throws DimensionFinancialReferenceWriteException
 */
function manufacturing_dimension_guard($owner, $key, $persist, $remove = false)
{
	$result = manufacturing_dimension_write($owner, $key, $persist, $remove);
	if (!$result)
		dimension_financial_reference_failure();

	return $result;
}

/**
 * Apply one native UPDATE to every wo_issue_items row of an issue, each row under the
 * writer fence, so a row whose metadata would change is refused and the whole set unwinds.
 *
 * @param int      $issue_id
 * @param callable $sql_for function(int $id): string native UPDATE for one item
 * @param string   $message Native error message
 * @return void
 * @throws DimensionFinancialReferenceWriteException
 */
function manufacturing_issue_items_guard($issue_id, $sql_for, $message)
{
	$rows = db_query('SELECT id FROM '.TB_PREF.'wo_issue_items WHERE issue_id='.db_escape($issue_id).' ORDER BY id FOR UPDATE');
	if (!$rows)
		dimension_financial_reference_failure();
	$ids = array();
	while ($row = db_fetch_assoc($rows))
		$ids[] = (int)$row['id'];
	foreach ($ids as $id) {
		$sql = call_user_func($sql_for, $id);
		manufacturing_dimension_guard('wo_issue_items', $id, function() use ($sql, $message) {
			return db_query($sql, $message) ? true : false;
		});
	}
}
