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

/*
 * The one copy of the JSON custom-data steps of the purchasing documents: procurement plans, agreements,
 * requisitions, RFQs, templates, vendor evaluations and vendor price lists. require_once this file from each
 * calling file; never copy a step under a document name (tests/php/test_purchasing_custom_data.php refuses copies).
 * Purchase matching keeps its own stricter steps in purch_matching_db.inc.
 */

/**
 * Encode purchasing custom data for a JSON column.
 *
 * An empty value (null, '' or an empty array) is stored as NULL. A string is taken as JSON that is already
 * encoded and is returned unchanged.
 *
 * @param mixed $custom_data
 * @return string|null JSON text, or null for an empty value or a value json_encode() refuses.
 */
function encode_purchasing_custom_data($custom_data)
{
	if ($custom_data === null || $custom_data === '' || $custom_data === array())
		return null;

	if (is_string($custom_data))
		return $custom_data;

	$encoded = json_encode($custom_data);
	return $encoded === false ? null : $encoded;
}

/**
 * Build the SQL literal of purchasing custom data: NULL or the quoted, escaped JSON text.
 *
 * @param mixed $custom_data
 * @return string
 */
function sql_purchasing_custom_data($custom_data)
{
	$encoded = encode_purchasing_custom_data($custom_data);
	return $encoded === null ? 'NULL' : "'" . db_escape_function($encoded) . "'";
}

/**
 * Decode purchasing custom data read from storage.
 *
 * Text that was stored HTML-encoded by an older form is decoded a second time after html_entity_decode().
 *
 * @param string|null $custom_data
 * @return array The decoded object, or an empty array for an empty or unreadable value.
 */
function decode_purchasing_custom_data($custom_data)
{
	if (!$custom_data)
		return array();

	$decoded = json_decode($custom_data, true);
	if (!is_array($decoded))
		$decoded = json_decode(html_entity_decode($custom_data, ENT_QUOTES, 'UTF-8'), true);

	return is_array($decoded) ? $decoded : array();
}
