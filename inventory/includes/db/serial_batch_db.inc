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
 * Get the supported item tracking modes.
 *
 * @return array
 */
function get_supported_tracking_modes() {
	return array('none', 'serial', 'batch', 'serial_batch');
}

/**
 * Normalize and validate an item tracking mode value.
 *
 * @param mixed  $value   Raw posted value
 * @param string $default Default mode when blank
 * @return string|false Normalized mode or false when invalid
 */
function normalize_tracking_mode($value, $default = 'none') {
	if (!is_scalar($value) && $value !== null)
		return false;

	$mode = trim((string)$value);
	if ($mode === '')
		$mode = $default;

	return in_array($mode, get_supported_tracking_modes(), true) ? $mode : false;
}

/**
 * Normalize an optional tracking prefix.
 *
 * Blank or whitespace-only values are converted to null so generation can
 * fall back to the stock-based default prefix.
 *
 * @param mixed  $value      Raw prefix value
 * @param int    $max_length Maximum allowed length
 * @param string $label      Field label for error text
 * @param string &$normalized Normalized output value
 * @return string|null Validation error or null when valid
 */
function normalize_tracking_prefix($value, $max_length, $label, &$normalized) {
	$normalized = null;

	if ($value === null || $value === '')
		return null;
	if (!is_scalar($value))
		return $label . ' must be a valid text value.';

	$trimmed = trim((string)$value);
	if ($trimmed === '')
		return null;
	if (strlen($trimmed) > $max_length)
		return $label . ' cannot be longer than ' . (int)$max_length . ' characters.';

	$normalized = $trimmed;
	return null;
}

/**
 * Normalize an optional non-negative integer tracking field.
 *
 * @param mixed  $value       Raw posted value
 * @param string $label       Field label for error text
 * @param int|null &$normalized Normalized integer output
 * @return string|null Validation error or null when valid
 */
function normalize_tracking_non_negative_int($value, $label, &$normalized) {
	$normalized = null;

	if ($value === null || $value === '')
		return null;
	if (!is_scalar($value))
		return $label . ' must be a whole number.';

	$trimmed = trim((string)$value);
	if ($trimmed === '')
		return $label . ' cannot contain only whitespace.';
	if (!preg_match('/^\d+$/', $trimmed))
		return $label . ' must be a non-negative whole number.';

	$normalized = (int)$trimmed;
	return null;
}

/**
 * Validate a generated serial or batch number format.
 *
 * Allowed tokens: {PREFIX} {YYYY} {YY} {MM} {DD} {SEQ:n}
 *
 * @param mixed  $format Raw format value
 * @param string $label  Field label for error text
 * @return string|null Validation error or null when valid
 */
function validate_tracking_number_format($format, $label) {
	if ($format === null || $format === '')
		return null;
	if (!is_scalar($format))
		return $label . ' must be a valid text value.';

	$trimmed = trim((string)$format);
	if ($trimmed === '')
		return null;
	if (!preg_match('/\{SEQ:\d+\}/', $trimmed))
		return $label . ' must contain a {SEQ:n} token for sequential numbering.';

	$remainder = preg_replace('/\{PREFIX\}|\{YYYY\}|\{YY\}|\{MM\}|\{DD\}|\{SEQ:\d+\}/', '', $trimmed);
	if (strpos($remainder, '{') !== false || strpos($remainder, '}') !== false)
		return $label . ' contains unsupported or malformed tokens. Allowed tokens: {PREFIX} {YYYY} {YY} {MM} {DD} {SEQ:n}.';

	return null;
}

/**
 * Resolve a stored/generated number format, falling back to a safe default
 * when the configured value is blank or malformed.
 *
 * @param mixed  $format         Raw format value
 * @param string $default_format Safe fallback format
 * @return string
 */
function resolve_tracking_number_format($format, $default_format) {
	if (validate_tracking_number_format($format, 'Tracking format') !== null)
		return $default_format;

	$trimmed = trim((string)$format);
	return $trimmed === '' ? $default_format : $trimmed;
}

/**
 * Render a serial or batch number from a validated format.
 *
 * @param string   $format    Format string
 * @param string   $prefix    Prefix value
 * @param int      $seq       Sequence number
 * @param int|null $timestamp Optional unix timestamp
 * @return string
 */
function render_tracking_number_format($format, $prefix, $seq, $timestamp = null) {
	$timestamp = $timestamp === null ? time() : $timestamp;

	$result = $format;
	$result = str_replace('{PREFIX}', $prefix, $result);
	$result = str_replace('{YYYY}', date('Y', $timestamp), $result);
	$result = str_replace('{YY}', date('y', $timestamp), $result);
	$result = str_replace('{MM}', date('m', $timestamp), $result);
	$result = str_replace('{DD}', date('d', $timestamp), $result);

	return preg_replace_callback('/\{SEQ:(\d+)\}/', function ($matches) use ($seq) {
		$pad_length = (int)$matches[1];
		return str_pad($seq, $pad_length, '0', STR_PAD_LEFT);
	}, $result);
}

/**
 * Build a unique generated serial or batch identifier for an item.
 *
 * @param string   $stock_id        Item code
 * @param mixed    $format          Stored format value
 * @param string   $default_format  Safe fallback format
 * @param string   $prefix_field    Item column holding the prefix
 * @param string   $default_prefix  Safe fallback prefix
 * @param callable $sequence_getter Callback returning the starting sequence
 * @param callable $duplicate_check Callback checking item-level duplicates
 * @return string
 */
function generate_tracking_identifier($stock_id, $format, $default_format, $prefix_field,
	$default_prefix, $sequence_getter, $duplicate_check) {
	$item = get_item($stock_id);
	$resolved_format = resolve_tracking_number_format($format, $default_format);
	$normalized_prefix = null;
	$prefix_value = isset($item[$prefix_field]) ? $item[$prefix_field] : null;
	normalize_tracking_prefix($prefix_value, 20, 'Tracking prefix', $normalized_prefix);
	if ($normalized_prefix === null)
		$normalized_prefix = $default_prefix;

	$start_seq = (int)call_user_func($sequence_getter, $stock_id);
	if ($start_seq < 1)
		$start_seq = 1;

	for ($offset = 0; $offset < 1000; $offset++) {
		$sequence = $start_seq + $offset;
		$candidate = render_tracking_number_format($resolved_format, $normalized_prefix, $sequence);
		if (!call_user_func($duplicate_check, $candidate, $stock_id))
			return $candidate;
	}

	return render_tracking_number_format($default_format, $normalized_prefix, $start_seq + 1000);
}

/**
 * Generate the next serial number for a given stock item.
 *
 * Supports format tokens:
 *   {PREFIX}  — replaced with item's serial_no_prefix
 *   {YYYY}    — 4-digit year
 *   {YY}      — 2-digit year
 *   {MM}      — 2-digit month
 *   {DD}      — 2-digit day
 *   {SEQ:n}   — zero-padded sequential number with n digits
 *
 * @param string $stock_id  The item code
 * @param string $format    Format pattern (null = use system default from sys_prefs)
 * @return string The generated serial number
 */
function generate_serial_number($stock_id, $format = null) {
	if ($format === null)
		$format = get_tracking_setting('serial_number_format');

	return generate_tracking_identifier(
		$stock_id,
		$format,
		'{PREFIX}{YYYY}{MM}{SEQ:6}',
		'serial_no_prefix',
		strtoupper(substr($stock_id, 0, 3)),
		'get_next_serial_sequence',
		'is_serial_number_duplicate'
	);
}

/**
 * Generate the next batch number for a given stock item.
 *
 * Supports same format tokens as generate_serial_number().
 *
 * @param string $stock_id  The item code
 * @param string $format    Format pattern (null = use system default from sys_prefs)
 * @return string The generated batch number
 */
function generate_batch_number($stock_id, $format = null) {
	if ($format === null)
		$format = get_tracking_setting('batch_number_format');

	return generate_tracking_identifier(
		$stock_id,
		$format,
		'{PREFIX}{YYYY}{MM}{SEQ:5}',
		'batch_no_prefix',
		'LOT',
		'get_next_batch_sequence',
		'is_batch_number_duplicate'
	);
}

/**
 * Get the next sequential number for serial number generation for an item.
 *
 * @param string $stock_id The item code
 * @return int The next sequence number
 */
function get_next_serial_sequence($stock_id) {
	$sql = "SELECT COUNT(*) + 1 AS next_seq FROM ".TB_PREF."serial_numbers 
		WHERE stock_id=".db_escape($stock_id);
	$result = db_query($sql, 'could not get serial sequence');
	$row = db_fetch_row($result);
	return (int)$row[0];
}

/**
 * Get the next sequential number for batch number generation for an item.
 *
 * @param string $stock_id  The item code
 * @return int The next sequence number
 */
function get_next_batch_sequence($stock_id) {
	$sql = "SELECT COUNT(*) + 1 AS next_seq FROM ".TB_PREF."stock_batches 
		WHERE stock_id=".db_escape($stock_id);
	$result = db_query($sql, 'could not get batch sequence');
	$row = db_fetch_row($result);
	return (int)$row[0];
}

/**
 * Check if an item has any stock movements (used to determine if track_by can be changed).
 *
 * @param string $stock_id  The item code
 * @return bool True if the item has stock movements
 */
function item_has_stock_movements($stock_id) {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."stock_moves WHERE stock_id=".db_escape($stock_id);
	$result = db_query($sql, 'could not check stock movements');
	$row = db_fetch_row($result);
	return $row[0] > 0;
}

/**
 * Get a tracking setting from sys_prefs.
 * Returns the value or the default if not set.
 *
 * @param string $name     Setting name
 * @param mixed  $default  Default value
 * @return mixed
 */
function get_tracking_setting($name, $default = null) {
	$val = get_company_pref($name);
	return ($val !== null && $val !== '') ? $val : $default;
}

/**
 * Save a tracking setting to sys_prefs.
 *
 * @param string $name   Setting name
 * @param string $value  Setting value
 */
function set_tracking_setting($name, $value) {
	set_company_pref($name, 'inventory.tracking', 'varchar', 60, $value);
}

/**
 * Decode GRN serial JSON into raw array entries.
 *
 * Accepts HTML-encoded JSON because older writes may have passed through
 * db_escape() before persistence.
 *
 * @param mixed $serial_numbers_json Stored JSON payload.
 * @return array
 */
function decode_grn_serial_numbers_json($serial_numbers_json) {
	if (!is_scalar($serial_numbers_json))
		return array();

	$raw_json = trim((string)$serial_numbers_json);
	if ($raw_json === '')
		return array();

	$decoded_json = html_entity_decode($raw_json, ENT_QUOTES, 'UTF-8');
	$entries = json_decode($decoded_json, true);

	return is_array($entries) ? $entries : array();
}

/**
 * Resolve stored GRN serial JSON to serial number strings.
 *
 * Supports current string payloads and legacy integer-id payloads.
 *
 * @param mixed $serial_numbers_json Stored JSON payload.
 * @return array
 */
function get_grn_serial_numbers_from_json($serial_numbers_json) {
	$entries = decode_grn_serial_numbers_json($serial_numbers_json);
	if (empty($entries))
		return array();

	$serial_numbers = array();
	$seen_serials = array();
	$serial_numbers_db_path = dirname(__FILE__) . '/serial_numbers_db.inc';
	if (file_exists($serial_numbers_db_path))
		include_once($serial_numbers_db_path);

	foreach ($entries as $entry) {
		$serial_number = '';

		if (is_int($entry) || is_float($entry)) {
			if (function_exists('get_serial_number')) {
				$serial_row = get_serial_number((int)$entry);
				if ($serial_row && !empty($serial_row['serial_no']))
					$serial_number = trim((string)$serial_row['serial_no']);
			}
		} elseif (is_scalar($entry)) {
			$serial_number = trim((string)$entry);
		}

		if ($serial_number === '')
			continue;

		$serial_key = strtoupper($serial_number);
		if (isset($seen_serials[$serial_key]))
			continue;

		$seen_serials[$serial_key] = true;
		$serial_numbers[] = $serial_number;
	}

	return $serial_numbers;
}

/**
 * Resolve stored GRN serial JSON to serial record ids.
 *
 * Supports current string payloads and legacy integer-id payloads.
 *
 * @param mixed  $serial_numbers_json Stored JSON payload.
 * @param string $stock_id            Item code for string lookups.
 * @return array
 */
function get_grn_serial_ids_from_json($serial_numbers_json, $stock_id = '') {
	$entries = decode_grn_serial_numbers_json($serial_numbers_json);
	if (empty($entries))
		return array();

	$serial_ids = array();
	$seen_ids = array();
	$serial_numbers_db_path = dirname(__FILE__) . '/serial_numbers_db.inc';
	if (file_exists($serial_numbers_db_path))
		include_once($serial_numbers_db_path);

	foreach ($entries as $entry) {
		$serial_id = 0;

		if (is_int($entry) || is_float($entry)) {
			$serial_id = (int)$entry;
		} elseif (is_scalar($entry) && function_exists('get_serial_number_by_code')) {
			$serial_number = trim((string)$entry);
			if ($serial_number !== '') {
				$serial_row = get_serial_number_by_code($serial_number, $stock_id);
				if ($serial_row)
					$serial_id = (int)$serial_row['id'];
			}
		}

		if ($serial_id <= 0 || isset($seen_ids[$serial_id]))
			continue;

		$seen_ids[$serial_id] = true;
		$serial_ids[] = $serial_id;
	}

	return $serial_ids;
}

/**
 * Get the tracking mode for an item.
 *
 * @param string $stock_id The item code
 * @return string 'none'|'serial'|'batch'|'serial_batch'
 */
function get_item_tracking_mode($stock_id) {
	$item = get_item($stock_id);
	return isset($item['track_by']) ? $item['track_by'] : 'none';
}

/**
 * Check if an item requires serial number tracking.
 *
 * @param string $stock_id The item code
 * @return bool
 */
function item_has_serial_tracking($stock_id) {
	$mode = get_item_tracking_mode($stock_id);
	return $mode === 'serial' || $mode === 'serial_batch';
}

/**
 * Check if an item requires batch/lot tracking.
 *
 * @param string $stock_id The item code
 * @return bool
 */
function item_has_batch_tracking($stock_id) {
	$mode = get_item_tracking_mode($stock_id);
	return $mode === 'batch' || $mode === 'serial_batch';
}

/**
 * Preview what a generated number would look like given a format and sample prefix.
 *
 * @param string $format    The format pattern
 * @param string $prefix    The prefix to use
 * @param int    $seq       The sequence number to use
 * @return string The preview string
 */
function preview_number_format($format, $prefix = 'ABC', $seq = 1) {
	return render_tracking_number_format(resolve_tracking_number_format($format, '{PREFIX}{YYYY}{MM}{SEQ:6}'), $prefix, $seq);
}
