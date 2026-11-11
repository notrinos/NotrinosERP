<?php
/**
 * Regulatory record retained metadata custody (B02): UDI registrations, FSMA 204 tracking
 * events and DSCSA transactions.
 * Native SQL keeps its business authority; this only joins the existing ordinary
 * Dimension lifecycle protocol (lock, transition check, exact completion check).
 */
require_once __DIR__.'/../../dimensions/includes/dimension_master_metadata_writer.inc';

/**
 * Fixed regulatory owners admitted by this helper; no caller-selected table.
 *
 * @return array Owner table => native primary key
 */
function regulatory_dimension_sources()
{
	return array(
		'udi_registrations' => 'id',
		'fsma_tracking_events' => 'id',
		'dscsa_transactions' => 'id',
	);
}

/**
 * Run one native regulatory SQL command under the ordinary writer fence.
 * Omitted custom_data must survive byte-exactly; admitted custom_data must keep
 * every retained reference path and may add only open references on their axis.
 *
 * @param string   $owner   One of regulatory_dimension_sources()
 * @param int|null $key     Existing native id, or null for a native INSERT
 * @param array    $next    Empty, or array('custom_data'=>stored text|null) for a birth
 * @param callable $persist Native SQL; must return a truthy result
 * @param bool     $remove  Native DELETE of a proven-unused row
 * @return mixed Native result, or false when refused/rolled back
 */
function regulatory_dimension_write($owner, $key, $next, $persist, $remove = false)
{
	$sources = regulatory_dimension_sources();
	if (!isset($sources[$owner]) || !is_array($next) || !is_callable($persist)
		|| array_diff(array_keys($next), array('custom_data')))
		return false;

	return dimension_master_metadata_write($owner, $key, $next, function($admitted) use ($persist) {
		return call_user_func($persist);
	}, $remove);
}

/**
 * Same as regulatory_dimension_write() for writers that run inside delivery/receipt
 * flows and whose callers ignore results: a refusal (the fence has already rolled the
 * whole transaction back) throws so the surrounding operation cannot continue.
 *
 * @param string   $owner
 * @param int|null $key
 * @param array    $next
 * @param callable $persist
 * @param bool     $remove
 * @return mixed Native result
 * @throws DimensionFinancialReferenceWriteException
 */
function regulatory_dimension_guard($owner, $key, $next, $persist, $remove = false)
{
	$result = regulatory_dimension_write($owner, $key, $next, $persist, $remove);
	if (!$result)
		dimension_financial_reference_failure();

	return $result;
}

/**
 * The text a native regulatory birth stores for its optional custom_data: the JSON the
 * caller supplied, or the writers' historical empty string when none was supplied.
 *
 * @param array  $data Writer input
 * @param string $key  Key holding the optional metadata
 * @return string|false Stored text, or false when the metadata cannot be encoded
 */
function regulatory_dimension_custom_text($data, $key = 'custom_data')
{
	if (!isset($data[$key]))
		return '';
	$json = json_encode($data[$key]);

	return is_string($json) ? $json : false;
}
