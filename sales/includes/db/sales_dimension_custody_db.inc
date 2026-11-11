<?php
/**
 * Sales owner retained metadata custody (B02).
 * Native SQL keeps its business authority; this only joins the existing ordinary
 * Dimension lifecycle protocol (lock, transition check, exact completion check).
 */
require_once __DIR__.'/../../../dimensions/includes/dimension_master_metadata_writer.inc';

/**
 * Fixed Sales owners admitted by this helper; no caller-selected table.
 *
 * @return array Owner table => native primary key
 */
function sales_dimension_custody_sources()
{
	return array(
		'sales_quotation_templates' => 'id',
		'sales_quotation_template_lines' => 'id',
		'sales_discount_programs' => 'id',
		'sales_coupons' => 'id',
		'sales_commission_plans' => 'id',
		'sales_commission_entries' => 'id',
		'sales_pricelists' => 'id',
				'sales_pricelist_rules' => 'id',
		'sales_rma' => 'id',
		'sales_rma_lines' => 'id',
	);
}

/**
 * Run one native Sales SQL command under the ordinary writer fence.
 * Omitted custom_data must survive byte-exactly; admitted custom_data must keep
 * every retained reference path and may add only open references on their axis.
 *
 * @param string   $owner   One of sales_dimension_custody_sources()
 * @param int|null $key     Existing native id, or null for a native INSERT
 * @param array    $next    Empty, or array('custom_data'=>encoded|null); a birth that never
 *                          writes metadata must pass array('custom_data'=>null)
 * @param callable $persist Native SQL; must return a truthy result
 * @param bool     $remove  Native DELETE of a proven-unused row
 * @return mixed Native result, or false when refused/rolled back
 */
function sales_dimension_custody_write($owner, $key, $next, $persist, $remove = false)
{
	$sources = sales_dimension_custody_sources();
	if (!isset($sources[$owner]) || !is_array($next) || !is_callable($persist)
		|| array_diff(array_keys($next), array('custom_data')))
		return false;

	return dimension_master_metadata_write($owner, $key, $next, function($admitted) use ($persist) {
		return call_user_func($persist);
	}, $remove);
}

/**
 * Delete one quotation template and its lines. Refused when the header or any
 * line retains a Dimension reference (including closed or orphan history).
 *
 * @param int $template_id
 * @return bool
 */
function sales_quotation_template_dimension_remove($template_id)
{
	$template_id = (int)$template_id;
	if ($template_id <= 0)
		return false;

	return (bool)sales_dimension_custody_write('sales_quotation_templates', $template_id, array(), function() use ($template_id) {
		$rows = db_query('SELECT id, custom_data FROM '.TB_PREF.'sales_quotation_template_lines WHERE template_id='.$template_id.' ORDER BY id FOR UPDATE');
		if (!$rows)
			return false;
		while ($line = db_fetch_assoc($rows))
			if (dimension_master_payload_transition($line['custom_data'], null, true) === false)
				return false;
		return db_query('DELETE FROM '.TB_PREF.'sales_quotation_template_lines WHERE template_id='.$template_id, 'Could not delete quotation template lines')
			&& db_query('DELETE FROM '.TB_PREF.'sales_quotation_templates WHERE id='.$template_id, 'Could not delete quotation template');
	}, true);
}

/**
 * Delete one draft discount program and its coupons. Refused when the program or
 * any coupon retains a Dimension reference (including closed or orphan history).
 *
 * @param int $program_id
 * @return bool
 */
function sales_discount_program_dimension_remove($program_id)
{
	$program_id = (int)$program_id;
	if ($program_id <= 0)
		return false;

	return (bool)sales_dimension_custody_write('sales_discount_programs', $program_id, array(), function() use ($program_id) {
		$rows = db_query('SELECT id, custom_data FROM '.TB_PREF.'sales_coupons WHERE program_id='.$program_id.' ORDER BY id FOR UPDATE');
		if (!$rows)
			return false;
		while ($coupon = db_fetch_assoc($rows))
			if (dimension_master_payload_transition($coupon['custom_data'], null, true) === false)
				return false;
		return db_query('DELETE FROM '.TB_PREF.'sales_coupons WHERE program_id='.$program_id, 'Sales coupons cannot be deleted')
			&& db_query('DELETE FROM '.TB_PREF.'sales_discount_programs WHERE id='.$program_id, 'Sales discount program cannot be deleted');
	}, true);
}

/**
 * Run one native status-transition UPDATE of a single Sales row under the writer
 * fence. A status that is not eligible for the transition is a normal false result
 * (zero affected rows); a refused fence also returns false.
 *
 * @param string $owner   Registered Sales owner
 * @param int    $id      Native id
 * @param string $sql     Native status UPDATE
 * @param string $message Native error message
 * @return bool True only when the row was actually transitioned
 */
function sales_dimension_status_write($owner, $id, $sql, $message)
{
	$affected = 0;
	$saved = sales_dimension_custody_write($owner, $id, array(), function() use ($sql, $message, &$affected) {
		if (!db_query($sql, $message))
			return false;
		$affected = db_num_affected_rows();
		return true;
	});

	return $saved && $affected > 0;
}

/**
 * Apply one status transition to several existing Sales rows in a single transaction.
 * Ids that do not exist are skipped (the native bulk UPDATE ignored them); any refused
 * row unwinds the whole set.
 *
 * @param string   $owner     Registered Sales owner
 * @param array    $ids       Candidate native ids
 * @param callable $sql_for   function(int $id): string native status UPDATE for one id
 * @param string   $message   Native error message
 * @return bool True when every existing row was processed without refusal
 */
function sales_dimension_status_bulk($owner, $ids, $sql_for, $message)
{
	$sources = sales_dimension_custody_sources();
	if (!isset($sources[$owner]) || !is_callable($sql_for) || !begin_transaction())
		return false;
	$clean = array_values(array_unique(array_filter(array_map('intval', (array)$ids))));
	sort($clean, SORT_NUMERIC);
	$existing = array();
	if ($clean) {
		$rows = db_query('SELECT id FROM '.TB_PREF.$owner.' WHERE id IN ('.implode(',', $clean).') ORDER BY id FOR UPDATE');
		if (!$rows) {
			cancel_transaction();
			return false;
		}
		while ($row = db_fetch_assoc($rows))
			$existing[] = (int)$row['id'];
	}
	foreach ($existing as $id) {
		$sql = call_user_func($sql_for, $id);
		$ok = sales_dimension_custody_write($owner, $id, array(), function() use ($sql, $message) {
			return db_query($sql, $message) ? true : false;
		});
		if (!$ok) {
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
 * Delete one pricelist and all its rules. Refused when the pricelist or any rule
 * retains a Dimension reference (including closed or orphan history).
 *
 * @param int $pricelist_id
 * @return bool
 */
function sales_pricelist_dimension_remove($pricelist_id)
{
	$pricelist_id = (int)$pricelist_id;
	if ($pricelist_id <= 0)
		return false;

	return (bool)sales_dimension_custody_write('sales_pricelists', $pricelist_id, array(), function() use ($pricelist_id) {
		$rows = db_query('SELECT id, custom_data FROM '.TB_PREF.'sales_pricelist_rules WHERE pricelist_id='.$pricelist_id.' ORDER BY id FOR UPDATE');
		if (!$rows)
			return false;
		while ($rule = db_fetch_assoc($rows))
			if (dimension_master_payload_transition($rule['custom_data'], null, true) === false)
				return false;
		return db_query('DELETE FROM '.TB_PREF.'sales_pricelist_rules WHERE pricelist_id='.$pricelist_id, 'Pricelist rules cannot be deleted')
			&& db_query('DELETE FROM '.TB_PREF.'sales_pricelists WHERE id='.$pricelist_id, 'Pricelist cannot be deleted');
	}, true);
}
