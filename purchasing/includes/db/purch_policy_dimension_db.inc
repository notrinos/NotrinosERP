<?php
/**
 * Purchasing matching, bill-control and order-template retained metadata custody (B02).
 * Native SQL keeps its business authority; this only joins the existing ordinary
 * Dimension lifecycle protocol (lock, transition check, exact completion check).
 */
require_once __DIR__.'/../../../dimensions/includes/dimension_master_metadata_writer.inc';

/**
 * Fixed owners admitted by this helper; no caller-selected table.
 *
 * @return array Owner table => native primary key
 */
function purch_policy_dimension_sources()
{
	return array(
		'purch_matching_config' => 'id',
		'purch_matching_exceptions' => 'id',
		'purch_bill_control_policy' => 'id',
		'purch_order_templates' => 'id',
		'purch_order_template_lines' => 'id',
		'procurement_plan' => 'id',
		'procurement_plan_lines' => 'id',
	);
}

/**
 * Run one native policy/template SQL command under the ordinary writer fence.
 * Omitted custom_data must survive byte-exactly; admitted custom_data must keep
 * every retained reference path and may add only open references on their axis.
 *
 * @param string   $owner   One of purch_policy_dimension_sources()
 * @param int|null $key     Existing native id, or null for a native INSERT
 * @param array    $next    Empty, or array('custom_data'=>encoded|null)
 * @param callable $persist Native SQL; must return a truthy result
 * @param bool     $remove  Native DELETE of a proven-unused row
 * @return mixed Native result, or false when refused/rolled back
 */
function purch_policy_dimension_write($owner, $key, $next, $persist, $remove = false)
{
	$sources = purch_policy_dimension_sources();
	if (!isset($sources[$owner]) || !is_array($next) || !is_callable($persist)
		|| array_diff(array_keys($next), array('custom_data')))
		return false;

	return dimension_master_metadata_write($owner, $key, $next, function($admitted) use ($persist) {
		return call_user_func($persist);
	}, $remove);
}

/**
 * Clear the single-default flag on every bill-control policy while holding all
 * sibling rows and their retained Dimension masters. A sibling whose metadata
 * changes (for example through a trigger) makes the whole command fail.
 *
 * @return bool
 */
function purch_bill_control_policy_reset_default()
{
	$rows = db_query('SELECT * FROM '.TB_PREF.'purch_bill_control_policy ORDER BY id FOR UPDATE');
	if (!$rows)
		return false;
	$before = array();
	$all = array();
	while ($row = db_fetch_assoc($rows)) {
		$all[] = $row;
		$before[$row['id']] = $row['custom_data'];
	}
	if (!dimension_master_metadata_lock_rows($all))
		return false;
	if (!db_query('UPDATE '.TB_PREF.'purch_bill_control_policy SET is_default = 0', 'could not reset default policy'))
		return false;
	$rows = db_query('SELECT id, custom_data FROM '.TB_PREF.'purch_bill_control_policy ORDER BY id');
	if (!$rows)
		return false;
	$after = array();
	while ($row = db_fetch_assoc($rows))
		$after[$row['id']] = $row['custom_data'];

	return $after === $before;
}

/**
 * Delete one purchase template and its lines. Refused when the header or any
 * line retains a Dimension reference (including closed or orphan history).
 *
 * @param int $template_id
 * @return bool
 */
function purch_template_dimension_remove($template_id)
{
	$template_id = (int)$template_id;
	if ($template_id <= 0)
		return false;

	return (bool)purch_policy_dimension_write('purch_order_templates', $template_id, array(), function() use ($template_id) {
		$rows = db_query('SELECT id, custom_data FROM '.TB_PREF.'purch_order_template_lines WHERE template_id='.$template_id.' ORDER BY id FOR UPDATE');
		if (!$rows)
			return false;
		while ($line = db_fetch_assoc($rows))
			if (dimension_master_payload_transition($line['custom_data'], null, true) === false)
				return false;
		return db_query('DELETE FROM '.TB_PREF.'purch_order_template_lines WHERE template_id='.$template_id, 'could not delete purchase template lines')
			&& db_query('DELETE FROM '.TB_PREF.'purch_order_templates WHERE id='.$template_id, 'could not delete purchase template');
	}, true);
}

/**
 * Run one native status-transition UPDATE of a plan header or line under the writer
 * fence. A status that is not eligible for the transition is a normal false result
 * (zero affected rows); a refused fence also returns false.
 *
 * @param string $owner   procurement_plan or procurement_plan_lines
 * @param int    $id      Native id
 * @param string $sql     Native status UPDATE
 * @param string $message Native error message
 * @return bool True only when the row was actually transitioned
 */
function purch_plan_status_write($owner, $id, $sql, $message)
{
	$affected = 0;
	$saved = purch_policy_dimension_write($owner, $id, array(), function() use ($sql, $message, &$affected) {
		if (!db_query($sql, $message))
			return false;
		$affected = db_num_affected_rows();
		return true;
	});

	return $saved && $affected > 0;
}
