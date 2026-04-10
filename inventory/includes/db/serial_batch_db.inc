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
	if (empty($format))
		$format = '{PREFIX}{YYYY}{MM}{SEQ:6}';

	$item = get_item($stock_id);
	$prefix = isset($item['serial_no_prefix']) && $item['serial_no_prefix'] !== '' 
		? $item['serial_no_prefix'] 
		: strtoupper(substr($stock_id, 0, 3));

	// Get next sequence number
	$seq = get_next_serial_sequence($stock_id);

	$result = $format;
	$result = str_replace('{PREFIX}', $prefix, $result);
	$result = str_replace('{YYYY}', date('Y'), $result);
	$result = str_replace('{YY}', date('y'), $result);
	$result = str_replace('{MM}', date('m'), $result);
	$result = str_replace('{DD}', date('d'), $result);

	// Handle {SEQ:n} token
	if (preg_match('/\{SEQ:(\d+)\}/', $result, $matches)) {
		$pad_length = (int)$matches[1];
		$result = str_replace($matches[0], str_pad($seq, $pad_length, '0', STR_PAD_LEFT), $result);
	}

	return $result;
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
	if (empty($format))
		$format = '{PREFIX}{YYYY}{MM}{SEQ:5}';

	$item = get_item($stock_id);
	$prefix = isset($item['batch_no_prefix']) && $item['batch_no_prefix'] !== '' 
		? $item['batch_no_prefix'] 
		: 'LOT';

	$seq = get_next_batch_sequence($stock_id);

	$result = $format;
	$result = str_replace('{PREFIX}', $prefix, $result);
	$result = str_replace('{YYYY}', date('Y'), $result);
	$result = str_replace('{YY}', date('y'), $result);
	$result = str_replace('{MM}', date('m'), $result);
	$result = str_replace('{DD}', date('d'), $result);

	if (preg_match('/\{SEQ:(\d+)\}/', $result, $matches)) {
		$pad_length = (int)$matches[1];
		$result = str_replace($matches[0], str_pad($seq, $pad_length, '0', STR_PAD_LEFT), $result);
	}

	return $result;
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
	$result = $format;
	$result = str_replace('{PREFIX}', $prefix, $result);
	$result = str_replace('{YYYY}', date('Y'), $result);
	$result = str_replace('{YY}', date('y'), $result);
	$result = str_replace('{MM}', date('m'), $result);
	$result = str_replace('{DD}', date('d'), $result);

	if (preg_match('/\{SEQ:(\d+)\}/', $result, $matches)) {
		$pad_length = (int)$matches[1];
		$result = str_replace($matches[0], str_pad($seq, $pad_length, '0', STR_PAD_LEFT), $result);
	}

	return $result;
}
