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
 * DIM-INT-001: shared Dimension adapter for CSV import/export and other integration consumers.
 *
 * The customer and item CSV importers carry the master default Dimension pair. Before this adapter
 * each importer privately re-implemented a partial check (`id = '1abc'` matched row 1 through MySQL
 * numeric coercion and was then cast to 1, a closed value passed the dry run and only failed at the
 * writer, a file with no Dimension column silently cleared the stored default of an existing record
 * and an unrecognised or extra-axis column was routed by position). This adapter gives every
 * consumer one contract that matches the shared FND-004 identity rules and the fenced master writers:
 *
 *  - a cell is a canonical positive native identity, an explicit blank/`0` (unassigned) or an exact
 *    reference/name that resolves to exactly one value on that axis; numeric-looking text that is not
 *    canonical, an ambiguous name, a missing value and a wrong-axis value are refused with stable codes;
 *  - a column that is absent from the file is "unspecified": an update keeps the stored value and a
 *    new record stays unassigned. A column that is present with a blank cell is an explicit unassigned;
 *  - a closed or disabled-axis value is refused (so the dry run equals the fenced writer) unless it is
 *    exactly the value the record already stores, which keeps historical defaults re-importable;
 *  - a header that looks like a Dimension but is not a recognised column, a third or later axis, or a
 *    repeated axis column refuses the whole file instead of being dropped or positionally misrouted.
 *
 * The adapter only reads. It writes nothing, opens no transaction and never chooses a default; the
 * owning master writers (`add_customer()`/`add_item()` through the DIM-B02 fence) remain the only
 * persistence and re-check every value under lock.
 */
require_once __DIR__.'/dimension_integration_hook_guard.inc';
require_once __DIR__.'/dimension_validation_contract.inc';

/**
 * State what this adapter does and does not do, and where every integration surface stands.
 *
 * @return array Descriptor used by the contract suites and the operator documentation
 */
function dimension_integration_contract()
{
	return array(
		'payload_version' => dimension_integration_payload_version(),
		'axes' => array_keys(dimension_integration_axes()),
		'read_only' => true,
		'default_inheritance' => false,
		'silent_axis_drop' => false,
		'name_lookup' => 'exact_unique_per_axis',
		'absent_column' => 'unspecified_keeps_stored_value',
		'blank_cell' => 'explicit_unassigned',
		'surfaces' => array(
			'csv_customer' => 'adapted',
			'csv_item' => 'adapted',
			'extension_hooks' => 'sealed',
			'rest_endpoint' => 'NOT FOUND',
			'scheduled_consumer' => 'NOT FOUND',
			'trade_finance_extension' => 'inactive_deferred_to_DIM-EXT-001',
		),
	);
}

/**
 * Classify the headers of an import file against a schema's Dimension columns.
 *
 * @param string[] $headers Raw header cells of the file
 * @param array $labels Axis number => the schema's exact (upper-case) Dimension header; empty when the type has none
 * @return array ok flag, stable codes, display errors and the axis numbers present in the file
 */
function dimension_integration_header_audit($headers, $labels)
{
	$result = array('ok' => true, 'codes' => array(), 'errors' => array(), 'present' => array());
	foreach ($headers as $header) {
		$clean = strtoupper(trim(preg_replace('/^\xEF\xBB\xBF/', '', (string)$header)));
		if (!preg_match('/^(?:DIMENSION|DIM)[ _-]?([0-9]*)(?:[ _-]?ID)?$/D', $clean, $match))
			continue;
		$axis = $match[1] === '' ? 1 : (int)$match[1];
		$known = array_search($clean, $labels, true);
		if ($known === false) {
			$code = isset(dimension_integration_axes()[$axis]) ? 'unrecognised_dimension_header' : 'unsupported_axis';
			if (!count($labels))
				$code = 'dimension_not_supported_for_type';
			$result['codes'][] = $code;
			$result['errors'][] = sprintf(_('Column "%s" is not a supported Dimension column of this file type and was not imported. Remove it or use the documented DIMENSION and DIMENSION2 columns.'), $clean);
			continue;
		}
		if (isset($result['present'][$known])) {
			$result['codes'][] = 'duplicate_axis_column';
			$result['errors'][] = sprintf(_('Column "%s" appears more than once; the Dimension for that category must be stated once.'), $clean);
			continue;
		}
		$result['present'][$known] = true;
	}
	$result['present'] = array_keys($result['present']);
	$result['codes'] = array_values(array_unique($result['codes']));
	$result['ok'] = !count($result['codes']);
	return $result;
}

/**
 * Resolve one cell to a stored Dimension identity.
 *
 * @param mixed $raw Cell text
 * @param int $axis Native axis (1 or 2)
 * @return array ok, mode (unassigned|selected), id, closed, code and a display error
 */
function dimension_integration_resolve_token($raw, $axis)
{
	$out = array('ok' => false, 'mode' => 'unassigned', 'id' => 0, 'closed' => false, 'code' => '', 'error' => '');
	$fail = function ($code, $message) use (&$out) {
		$out['code'] = $code;
		$out['error'] = $message;
		return $out;
	};
	if (!isset(dimension_integration_axes()[$axis]))
		return $fail('unsupported_axis', _('This release posts Dimension categories 1 and 2 only.'));
	if (is_int($raw))
		$raw = (string)$raw;
	if ($raw !== null && !is_string($raw))
		return $fail('invalid_reference', _('A Dimension cell must be text.'));
	if ($raw !== null && preg_match('/[\x00-\x08\x0e-\x1f\x7f]/', $raw))
		return $fail('invalid_reference', _('The Dimension value is not a valid identity, reference or name.'));
	$token = $raw === null ? '' : trim($raw);
	if ($token === '' || $token === '0') {
		$out['ok'] = true;
		return $out;
	}
	if (strlen($token) > 60 || preg_match('/[\x00-\x1f\x7f]/', $token))
		return $fail('invalid_reference', _('The Dimension value is not a valid identity, reference or name.'));
	if (preg_match('/^[1-9][0-9]{0,9}$/D', $token) && (int)$token <= 2147483647) {
		$rows = dimension_integration_find('id = '.(int)$token);
		if ($rows === false)
			return $fail('unavailable', _('The Dimension master could not be read.'));
		if (!count($rows))
			return $fail('not_found', sprintf(_('Dimension #%s does not exist.'), $token));
		if ((int)$rows[0]['type_'] !== (int)$axis)
			return $fail('wrong_axis', sprintf(_('Dimension #%s belongs to a different category than this column.'), $token));
		return dimension_integration_selected($out, $rows[0]);
	}
	if (is_numeric($token) || preg_match('/^[+-]?(?:0x[0-9a-f]+|[0-9.]+(?:e[+-]?[0-9]+)?)$/Di', $token))
		return $fail('non_canonical_identity', sprintf(_('"%s" is not a canonical Dimension identity. Use the whole number without a sign, leading zero or fraction, or the exact reference.'), $token));
	$rows = dimension_integration_find('reference = '.db_escape($token).' OR name = '.db_escape($token));
	if ($rows === false)
		return $fail('unavailable', _('The Dimension master could not be read.'));
	$on_axis = array();
	foreach ($rows as $row)
		if ((int)$row['type_'] === (int)$axis)
			$on_axis[(int)$row['id']] = $row;
	if (count($on_axis) > 1)
		return $fail('ambiguous', sprintf(_('"%s" matches more than one Dimension of this category. Use the numeric identity.'), $token));
	if (!count($on_axis))
		return $fail(count($rows) ? 'wrong_axis' : 'not_found', count($rows)
			? sprintf(_('"%s" belongs to a different category than this column.'), $token)
			: sprintf(_('No Dimension has the reference or name "%s".'), $token));
	return dimension_integration_selected($out, reset($on_axis));
}

/**
 * Complete a resolution with the single selected master row.
 *
 * @param array $out Result under construction
 * @param array $row Dimension master row
 * @return array Completed result
 */
function dimension_integration_selected($out, $row)
{
	$out['ok'] = true;
	$out['mode'] = 'selected';
	$out['id'] = (int)$row['id'];
	$out['closed'] = (int)$row['closed'] !== 0;
	return $out;
}

/**
 * Read Dimension master rows by one prepared predicate.
 *
 * @param string $where Predicate built only from escaped literals or integers
 * @return array|false Rows with id, type_ and closed, or false when the read fails
 */
function dimension_integration_find($where)
{
	$rows = db_query('SELECT id, type_, closed FROM '.TB_PREF.'dimensions WHERE '.$where.' LIMIT 3');
	if ($rows === false)
		return false;
	$found = array();
	while ($row = db_fetch_assoc($rows))
		$found[] = $row;
	return $found;
}

/**
 * Resolve and admit the Dimension pair of one import row.
 *
 * @param array $cells Axis number => cell text, only for the axes present in the file
 * @param array|null $existing Stored pair of the record being updated (axis => id), or null for a new record
 * @param string[]|null $present Axis numbers present in the file; null means every supported axis (direct callers)
 * @return array ok, ids (axis => id), modes (axis => mode), codes and display errors (no line prefix)
 */
function dimension_integration_resolve_row($cells, $existing, $present = null)
{
	$result = array('ok' => true, 'ids' => array(1 => 0, 2 => 0), 'modes' => array(1 => 'unspecified', 2 => 'unspecified'),
		'codes' => array(), 'errors' => array());
	$enabled = function_exists('get_company_pref') ? (int)get_company_pref('use_dimension') : 2;
	foreach (dimension_integration_axes() as $axis => $column) {
		$stored = is_array($existing) && isset($existing[$axis]) ? (int)$existing[$axis] : 0;
		$in_file = ($present === null || in_array($axis, $present)) && array_key_exists($axis, $cells);
		if (!$in_file) {
			$result['ids'][$axis] = $stored;
			continue;
		}
		$resolved = dimension_integration_resolve_token($cells[$axis], $axis);
		if (!$resolved['ok']) {
			$result['codes'][] = $resolved['code'];
			$result['errors'][] = sprintf(_('Dimension %d: %s'), $axis, $resolved['error']);
			continue;
		}
		$result['modes'][$axis] = $resolved['mode'];
		$result['ids'][$axis] = $resolved['id'];
		if ($resolved['id'] === 0 || $resolved['id'] === $stored)
			continue;
		if ($axis > $enabled) {
			$result['codes'][] = 'disabled_axis';
			$result['errors'][] = sprintf(_('Dimension %d: this category is not enabled for the company.'), $axis);
		} elseif ($resolved['closed']) {
			$result['codes'][] = 'closed';
			$result['errors'][] = sprintf(_('Dimension %d: #%d is closed and cannot be assigned.'), $axis, $resolved['id']);
		}
	}
	$result['codes'] = array_values(array_unique($result['codes']));
	$result['ok'] = !count($result['errors']);
	return $result;
}

/**
 * Read the stored default pair of one master record.
 *
 * @param string $owner debtors_master (key debtor_no) or stock_master (key stock_id)
 * @param mixed $key Record key
 * @return array|null|false Axis => id, null when the record does not exist, false when the row cannot be read canonically
 */
function dimension_integration_stored_pair($owner, $key)
{
	$owners = array('debtors_master' => 'debtor_no', 'stock_master' => 'stock_id');
	if (!isset($owners[$owner]) || $key === null || $key === '')
		return false;
	$rows = db_query('SELECT dimension_id, dimension2_id FROM '.TB_PREF.$owner.' WHERE '.$owners[$owner].' = '.db_escape($key).' LIMIT 1');
	if ($rows === false)
		return false;
	$row = db_fetch_assoc($rows);
	if (!is_array($row))
		return null;
	$first = dimension_validation_legacy_id($row['dimension_id']);
	$second = dimension_validation_legacy_id($row['dimension2_id']);
	return $first === false || $second === false ? false : array(1 => $first, 2 => $second);
}

/**
 * Convert the presence marker the page loops add to a mapped row into an axis list.
 *
 * @param array $data Mapped row
 * @return string[]|null Axis numbers present in the file, or null when the row carries no marker
 */
function dimension_integration_present_axes($data)
{
	if (!isset($data['dimension_columns_present']) || !is_string($data['dimension_columns_present']))
		return null;
	$axes = array();
	foreach (explode(',', $data['dimension_columns_present']) as $axis)
		if ($axis !== '' && isset(dimension_integration_axes()[(int)$axis]))
			$axes[] = (int)$axis;
	return $axes;
}

/**
 * Record which Dimension columns the file itself supplied, before schema defaults fill the gaps.
 *
 * @param array $data Mapped row before defaults
 * @param array $keys Axis number => schema key of the Dimension column
 * @param string[] $file_axes Axis numbers present in the header audit
 * @return string Marker value for `dimension_columns_present`
 */
function dimension_integration_presence_marker($data, $keys, $file_axes)
{
	$present = array();
	foreach ($keys as $axis => $key)
		if (array_key_exists($key, $data) && in_array($axis, $file_axes))
			$present[] = $axis;
	return implode(',', $present);
}

/**
 * Validate one import row's Dimension cells against the stored record.
 *
 * @param array $data Mapped row
 * @param array $keys Axis number => data key (customer: dimension_id/dimension2_id; item: dim/dim2)
 * @param array|null $existing Stored pair of the record being updated, or null for a new record
 * @return array The dimension_integration_resolve_row() result
 */
function dimension_integration_resolve_import_row($data, $keys, $existing)
{
	$cells = array();
	foreach ($keys as $axis => $key)
		if (isset($data[$key]))
			$cells[$axis] = $data[$key];
	return dimension_integration_resolve_row($cells, $existing, dimension_integration_present_axes($data));
}
