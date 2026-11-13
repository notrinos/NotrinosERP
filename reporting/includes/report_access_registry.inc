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

/**
 * DIM-APR-002: central native report identity and declared security-area registry.
 *
 * A report is not authorized because a menu happened to hide it or because its
 * filename looks familiar. This registry is the reviewed map from every native
 * report identity to the exact security areas declared by that report source.
 * The redirector, the report center, the direct-script bootstrap and the tests
 * use the same map, so a report cannot acquire two different authorization
 * answers from two different entry points.
 *
 * Nine document print reports select their area from the requested document
 * range: an exact single document needs the transaction-view area, while a
 * bulk range needs the corresponding bulk-report area. `document_pair` keeps
 * that reviewed choice in the registry instead of re-parsing request input
 * with an ad-hoc conditional.
 *
 * PHP 5.6 compatible syntax.
 */

/**
 * Reviewed native report registry.
 *
 * @return array Report id => array('areas' => security area list, 'selector' => optional selector)
 */
function report_access_report_registry()
{
	static $registry = null;
	if ($registry !== null)
		return $registry;

	$registry = array(
		101 => array('areas' => array('SA_CUSTPAYMREP')),
		102 => array('areas' => array('SA_CUSTPAYMREP')),
		103 => array('areas' => array('SA_CUSTBULKREP')),
		104 => array('areas' => array('SA_PRICEREP')),
		105 => array('areas' => array('SA_SALESBULKREP')),
		106 => array('areas' => array('SA_SALESMANREP')),
		107 => array('areas' => array('SA_SALESTRANSVIEW', 'SA_SALESBULKREP'), 'selector' => 'document_pair'),
		108 => array('areas' => array('SA_CUSTSTATREP')),
		109 => array('areas' => array('SA_SALESTRANSVIEW', 'SA_SALESBULKREP'), 'selector' => 'document_pair'),
		110 => array('areas' => array('SA_SALESTRANSVIEW', 'SA_SALESBULKREP'), 'selector' => 'document_pair'),
		111 => array('areas' => array('SA_SALESTRANSVIEW', 'SA_SALESBULKREP'), 'selector' => 'document_pair'),
		112 => array('areas' => array('SA_SALESTRANSVIEW', 'SA_SALESBULKREP'), 'selector' => 'document_pair'),
		113 => array('areas' => array('SA_SALESTRANSVIEW', 'SA_SALESBULKREP'), 'selector' => 'document_pair'),
		114 => array('areas' => array('SA_TAXREP')),
		115 => array('areas' => array('SA_CUSTPAYMREP')),
		201 => array('areas' => array('SA_SUPPLIERANALYTIC')),
		202 => array('areas' => array('SA_SUPPLIERANALYTIC')),
		203 => array('areas' => array('SA_SUPPPAYMREP')),
		204 => array('areas' => array('SA_SUPPLIERANALYTIC')),
		205 => array('areas' => array('SA_SUPPLIERANALYTIC')),
		206 => array('areas' => array('SA_SUPPLIERANALYTIC')),
		209 => array('areas' => array('SA_SUPPTRANSVIEW', 'SA_SUPPBULKREP'), 'selector' => 'document_pair'),
		210 => array('areas' => array('SA_SUPPTRANSVIEW', 'SA_SUPPBULKREP'), 'selector' => 'document_pair'),
		301 => array('areas' => array('SA_ITEMSVALREP')),
		302 => array('areas' => array('SA_ITEMSANALYTIC')),
		303 => array('areas' => array('SA_ITEMSVALREP')),
		304 => array('areas' => array('SA_SALESANALYTIC')),
		305 => array('areas' => array('SA_SUPPLIERANALYTIC')),
		306 => array('areas' => array('SA_SUPPLIERANALYTIC')),
		307 => array('areas' => array('SA_ITEMSVALREP')),
		308 => array('areas' => array('SA_ITEMSVALREP')),
		309 => array('areas' => array('SA_SALESANALYTIC')),
		310 => array('areas' => array('SA_SUPPLIERANALYTIC')),
		311 => array('areas' => array('SA_TRACEABILITY')),
		312 => array('areas' => array('SA_RECALL')),
		313 => array('areas' => array('SA_SERIALINQUIRY')),
		314 => array('areas' => array('SA_BATCHINQUIRY')),
		315 => array('areas' => array('SA_BATCHINQUIRY')),
		316 => array('areas' => array('SA_BATCHINQUIRY')),
		317 => array('areas' => array('SA_QC_INSPECTIONS')),
		318 => array('areas' => array('SA_WARRANTY')),
		319 => array('areas' => array('SA_WARRANTY')),
		320 => array('areas' => array('SA_BATCHINQUIRY')),
		321 => array('areas' => array('SA_SERIALINQUIRY')),
		322 => array('areas' => array('SA_QC_INSPECTIONS')),
		350 => array('areas' => array('SA_WAREHOUSE_DASHBOARD')),
		351 => array('areas' => array('SA_WAREHOUSE_DASHBOARD')),
		352 => array('areas' => array('SA_WAREHOUSE_DASHBOARD')),
		353 => array('areas' => array('SA_WAREHOUSE_DASHBOARD')),
		354 => array('areas' => array('SA_ITEMSVALREP')),
		355 => array('areas' => array('SA_WAREHOUSE_DASHBOARD')),
		356 => array('areas' => array('SA_ITEMSVALREP')),
		357 => array('areas' => array('SA_WAREHOUSE_DASHBOARD')),
		358 => array('areas' => array('SA_WAREHOUSE_CYCLE_COUNT')),
		401 => array('areas' => array('SA_BOMREP')),
		402 => array('areas' => array('SA_BOMREP')),
		409 => array('areas' => array('SA_MANUFTRANSVIEW', 'SA_MANUFBULKREP'), 'selector' => 'document_pair'),
		451 => array('areas' => array('SA_ASSETSANALYTIC')),
		501 => array('areas' => array('SA_DIMENSIONREP')),
		601 => array('areas' => array('SA_BANKREP')),
		602 => array('areas' => array('SA_BANKREP')),
		701 => array('areas' => array('SA_GLREP')),
		702 => array('areas' => array('SA_GLANALYTIC')),
		704 => array('areas' => array('SA_GLREP')),
		705 => array('areas' => array('SA_GLANALYTIC')),
		706 => array('areas' => array('SA_GLANALYTIC')),
		707 => array('areas' => array('SA_GLANALYTIC')),
		708 => array('areas' => array('SA_GLANALYTIC')),
		709 => array('areas' => array('SA_TAXREP')),
		710 => array('areas' => array('SA_GLANALYTIC')),
		880 => array('areas' => array('SA_HRMREPORTS')),
		881 => array('areas' => array('SA_HRMREPORTS')),
		882 => array('areas' => array('SA_HRMREPORTS')),
		883 => array('areas' => array('SA_HRMREPORTS')),
		884 => array('areas' => array('SA_HRMREPORTS')),
		885 => array('areas' => array('SA_HRMREPORTS')),
		886 => array('areas' => array('SA_HRMREPORTS')),
		887 => array('areas' => array('SA_HRMREPORTS')),
		888 => array('areas' => array('SA_HRMREPORTS')),
		889 => array('areas' => array('SA_HRMREPORTS')),
		890 => array('areas' => array('SA_HRMREPORTS')),
		891 => array('areas' => array('SA_HRMREPORTS')),
		892 => array('areas' => array('SA_HRMREPORTS')),
		893 => array('areas' => array('SA_HRMREPORTS')),
		950 => array('areas' => array('SA_APPROVALINQUIRY')),
	);

	return $registry;
}

/**
 * Normalize a request or script report identity without accepting an array or a path.
 *
 * @param mixed $value Raw report identity
 * @return int|false Positive native report id, or false
 */
function report_access_report_id($value)
{
	if (is_int($value))
		$value = (string)$value;
	if (!is_string($value) || !preg_match('/^[1-9][0-9]{0,5}$/D', $value))
		return false;
	$id = (int)$value;
	return $id > 0 && $id <= 999999 ? $id : false;
}

/**
 * Return the reviewed areas for one native report identity.
 *
 * @param mixed $report_id Report identity
 * @return array Security area names; empty when the identity is not native
 */
function report_access_report_areas($report_id)
{
	$id = report_access_report_id($report_id);
	if ($id === false)
		return array();
	$registry = report_access_report_registry();
	return isset($registry[$id]) ? $registry[$id]['areas'] : array();
}

/**
 * Return the reviewed registry entry, optionally resolving a custom report header.
 *
 * Custom/extension report files are accepted only when their source declares an
 * exact `$page_security` area. A file that declares more than one area without a
 * reviewed selector is ambiguous and fails closed.
 *
 * @param mixed $report_id Report identity
 * @param string|null $source_file Exact report file when already resolved
 * @return array|false
 */
function report_access_report_entry($report_id, $source_file = null)
{
	$id = report_access_report_id($report_id);
	if ($id === false)
		return false;

	$registry = report_access_report_registry();
	if (isset($registry[$id]))
		return $registry[$id];

	$file = $source_file;
	if (($file === null || $file === '') && function_exists('find_custom_file')
		&& function_exists('company_path'))
		$file = find_custom_file('/reporting/rep'.$id.'.php');
	if (!is_string($file) || $file === '' || !is_file($file))
		return false;

	$areas = report_access_report_declared_areas($file);
	if (count($areas) === 0)
		return false;
	return array('areas' => $areas, 'selector' => null, 'custom' => true);
}

/**
 * Read the reviewed security areas declared by a report source header.
 *
 * Only the first 8 KiB is inspected, the declaration must precede report output,
 * and every token must be an exact registered security area name. This is a
 * source declaration, never request input.
 *
 * @param string $file Report source path
 * @return array
 */
function report_access_report_declared_areas($file)
{
	if (!is_string($file) || !is_file($file) || !is_readable($file))
		return array();
	$handle = @fopen($file, 'rb');
	if (!$handle)
		return array();
	$header = @fread($handle, 8192);
	@fclose($handle);
	if (!is_string($header))
		return array();

	if (!preg_match('/\$page_security\s*=\s*(.{0,600}?);/s', $header, $match))
		return array();
	preg_match_all("/'(SA_[A-Z0-9_]+)'/", $match[1], $areas);
	if (!isset($areas[1]))
		return array();
	return array_values(array_unique($areas[1]));
}

/**
 * Select the exact reviewed area for one report request.
 *
 * The `document_pair` selector reproduces the reviewed document print reports:
 * an exact single document uses the transaction-view area, any range uses the
 * bulk-report area. The values are string-normalized scalars; the registry is
 * the review record, not a caller-supplied area.
 *
 * @param mixed $report_id Report identity
 * @param array $parameters Normalized report parameters
 * @param string|null $source_file Exact custom report file
 * @return string|false
 */
function report_access_report_selected_area($report_id, $parameters, $source_file = null)
{
	$entry = report_access_report_entry($report_id, $source_file);
	if ($entry === false || !isset($entry['areas']) || !is_array($entry['areas']))
		return false;
	$areas = array_values($entry['areas']);
	if (count($areas) === 1)
		return $areas[0];
	if (count($areas) !== 2 || !isset($entry['selector']) || $entry['selector'] !== 'document_pair')
		return false;

	$parameters = is_array($parameters) ? $parameters : array();
	$from = isset($parameters['PARAM_0']) && is_scalar($parameters['PARAM_0'])
		? (string)$parameters['PARAM_0'] : '';
	$to = isset($parameters['PARAM_1']) && is_scalar($parameters['PARAM_1'])
		? (string)$parameters['PARAM_1'] : '';
	return $from === $to ? $areas[0] : $areas[1];
}

/**
 * Is the current script one of the native report scripts?
 *
 * @param string $script_filename Server script path
 * @return bool
 */
function report_access_request_is_report_script($script_filename)
{
	if (!is_string($script_filename) || $script_filename === '')
		return false;
	$normalized = str_replace('\\', '/', $script_filename);
	return (bool)preg_match('#/reporting/rep(?:[0-9]+|_[a-z0-9_]+)\.php$#i', $normalized);
}

/**
 * Derive a stable report identity from a running report script path.
 *
 * @param string $script_filename Server script path
 * @return int|string
 */
function report_access_report_script_id($script_filename)
{
	$normalized = str_replace('\\', '/', (string)$script_filename);
	$name = basename($normalized, '.php');
	if (preg_match('/^rep([1-9][0-9]{0,5})$/D', $name, $match))
		return (int)$match[1];
	return $name === '' ? 'unknown' : $name;
}
