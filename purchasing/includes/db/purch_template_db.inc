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

include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/hooks.inc');
include_once($path_to_root . '/purchasing/includes/po_class.inc');
include_once($path_to_root . '/purchasing/includes/db/po_db.inc');
require_once __DIR__ . '/purch_policy_dimension_db.inc';
require_once __DIR__ . '/purchasing_classification_db.inc';

/**
 * Encode purchase template custom data for JSON storage.
 *
 * @param mixed $custom_data
 * @return string|null
 */
function encode_purch_template_custom_data($custom_data)
{
	if ($custom_data === null || $custom_data === '' || $custom_data === array())
		return null;

	if (is_string($custom_data))
		return $custom_data;

	$encoded = json_encode($custom_data);
	return $encoded === false ? null : $encoded;
}

/**
 * Build a SQL-safe JSON literal for purchase template custom data.
 *
 * @param mixed $custom_data
 * @return string
 */
function sql_purch_template_custom_data($custom_data)
{
	$encoded = encode_purch_template_custom_data($custom_data);
	return $encoded === null ? 'NULL' : "'" . db_escape_function($encoded) . "'";
}

/**
 * Decode purchase template custom data from storage.
 *
 * @param string|null $custom_data
 * @return array
 */
function decode_purch_template_custom_data($custom_data)
{
	if (!$custom_data)
		return array();

	$decoded = json_decode($custom_data, true);
	if (!is_array($decoded))
		$decoded = json_decode(html_entity_decode($custom_data, ENT_QUOTES, 'UTF-8'), true);

	return is_array($decoded) ? $decoded : array();
}

/**
 * Get one purchase order template header.
 *
 * @param int $template_id
 * @return array|false
 */
function get_purch_template($template_id)
{
	$sql = "SELECT template.*, supplier.supp_name, location.location_name,
		payment_terms.terms AS payment_terms_name,
		COUNT(template_line.id) AS line_count
		FROM " . TB_PREF . "purch_order_templates template
		LEFT JOIN " . TB_PREF . "suppliers supplier ON supplier.supplier_id = template.supplier_id
		LEFT JOIN " . TB_PREF . "locations location ON location.loc_code = template.delivery_location
		LEFT JOIN " . TB_PREF . "payment_terms payment_terms ON payment_terms.terms_indicator = template.default_payment_terms
		LEFT JOIN " . TB_PREF . "purch_order_template_lines template_line ON template_line.template_id = template.id
		WHERE template.id = " . (int)$template_id . "
		GROUP BY template.id";
	$result = db_query($sql, 'could not retrieve purchase template');
	$row = db_fetch($result);

	if ($row)
		$row['custom_data_array'] = decode_purch_template_custom_data($row['custom_data']);

	return $row;
}

/**
 * Get purchase order templates with optional supplier filtering.
 *
 * @param int  $supplier_id
 * @param bool $inactive
 * @return resource
 */
function get_purch_templates($supplier_id = 0, $inactive = false)
{
	$sql = "SELECT template.*, supplier.supp_name, location.location_name,
		COUNT(template_line.id) AS line_count
		FROM " . TB_PREF . "purch_order_templates template
		LEFT JOIN " . TB_PREF . "suppliers supplier ON supplier.supplier_id = template.supplier_id
		LEFT JOIN " . TB_PREF . "locations location ON location.loc_code = template.delivery_location
		LEFT JOIN " . TB_PREF . "purch_order_template_lines template_line ON template_line.template_id = template.id
		WHERE 1 = 1";

	if ((int)$supplier_id > 0)
		$sql .= " AND (template.supplier_id = 0 OR template.supplier_id = " . db_escape($supplier_id) . ")";
	if (!$inactive)
		$sql .= " AND template.inactive = 0";

	$sql .= " GROUP BY template.id ORDER BY template.name";

	return db_query($sql, 'could not retrieve purchase templates');
}

/**
 * Build template dropdown options.
 *
 * @param int $supplier_id
 * @return array
 */
function get_purch_template_options($supplier_id = 0)
{
	$options = array();
	$result = get_purch_templates($supplier_id, false);

	while ($row = db_fetch($result)) {
		$label = $row['name'];
		if (!empty($row['supp_name']))
			$label .= ' [' . $row['supp_name'] . ']';
		$options[$row['id']] = $label;
	}

	return $options;
}

/**
 * Add a purchase order template header.
 *
 * @param string $name
 * @param string $description
 * @param int    $supplier_id
 * @param string $location
 * @param int    $payment_terms
 * @param string $notes
 * @param int    $inactive
 * @param mixed  $custom_data
 * @param array|null $header_intents DIM-PUR-002 explicit header intent
 * @return int
 */
function add_purch_template($name, $description, $supplier_id, $location, $payment_terms, $notes = '', $inactive = 0, $custom_data = array(), $header_intents = null)
{
	$encoded = encode_purch_template_custom_data($custom_data);
	if ($encoded === null && $custom_data !== null && $custom_data !== '' && $custom_data !== array())
		return false;
	$sql = "INSERT INTO " . TB_PREF . "purch_order_templates
		(name, description, supplier_id, delivery_location, default_payment_terms, notes, inactive, custom_data)
		VALUES (
			" . db_escape($name) . ",
			" . db_escape($description) . ",
			" . db_escape((int)$supplier_id) . ",
			" . db_escape($location) . ",
			" . db_escape((int)$payment_terms) . ",
			" . db_escape($notes) . ",
			" . db_escape((int)$inactive) . ",
			" . sql_purch_template_custom_data($encoded) . ")";

	// A fresh reference must be open on its axis; the stored bytes must equal the admitted payload.
	return purch_policy_dimension_write('purch_order_templates', null, array('custom_data' => $encoded), function() use ($sql, $header_intents) {
		if (!db_query($sql, 'could not add purchase template'))
			return false;
		$template_id = (int)db_insert_id();
		// DIM-PUR-002: the durable source decision is part of this same writer transaction.
		if ($template_id > 0 && function_exists('purchasing_classification_schema_ready') && purchasing_classification_schema_ready())
			purchasing_classification_capture_template($template_id, $header_intents);
		return $template_id;
	});
}

/**
 * Update a purchase order template header.
 *
 * @param int    $template_id
 * @param string $name
 * @param string $description
 * @param int    $supplier_id
 * @param string $location
 * @param int    $payment_terms
 * @param string $notes
 * @param int    $inactive
 * @param mixed  $custom_data Null (default) keeps the stored metadata byte-exactly; any other value replaces it
 * @param array|null $header_intents DIM-PUR-002 explicit header intent
 * @return bool
 */
function update_purch_template($template_id, $name, $description, $supplier_id, $location, $payment_terms, $notes = '', $inactive = 0, $custom_data = null, $header_intents = null)
{
	$has_custom_data = $custom_data !== null;
	$encoded = $has_custom_data ? encode_purch_template_custom_data($custom_data) : null;
	if ($has_custom_data && $encoded === null && $custom_data !== '' && $custom_data !== array())
		return false;
	$sql = "UPDATE " . TB_PREF . "purch_order_templates SET
		name = " . db_escape($name) . ",
		description = " . db_escape($description) . ",
		supplier_id = " . db_escape((int)$supplier_id) . ",
		delivery_location = " . db_escape($location) . ",
		default_payment_terms = " . db_escape((int)$payment_terms) . ",
		notes = " . db_escape($notes) . ",
		inactive = " . db_escape((int)$inactive)
		. ($has_custom_data ? ",
		custom_data = " . sql_purch_template_custom_data($encoded) : "") . "
		WHERE id = " . (int)$template_id;

	return purch_policy_dimension_write('purch_order_templates', $template_id, $has_custom_data ? array('custom_data' => $encoded) : array(), function() use ($sql, $template_id, $header_intents) {
		if (!db_query($sql, 'could not update purchase template'))
			return false;
		// Capture only an explicit decision, or the first decision of a legacy template. An
		// unrelated edit never rewrites a previously captured decision.
		if (function_exists('purchasing_classification_schema_ready') && purchasing_classification_schema_ready()
			&& ($header_intents !== null
				|| !purchasing_classification_current_header_revision_id(ST_PURCHTEMPLATE, $template_id)))
			purchasing_classification_capture_template($template_id, $header_intents);
		return true;
	});
}

/**
 * Delete a purchase order template and its lines.
 *
 * @param int $template_id
 * @return bool
 */
function delete_purch_template($template_id)
{
	// Refused while the header or any line retains a Dimension reference (closed/orphan included).
	return purch_template_dimension_remove($template_id);
}

/**
 * Add a purchase order template line.
 *
 * @param int    $template_id
 * @param string $stock_id
 * @param string $description
 * @param float  $default_qty
 * @param int    $sort
 * @param mixed  $custom_data
 * @param array|null $line_intents DIM-PUR-002 explicit line intent
 * @return int
 */
function add_template_line($template_id, $stock_id, $description, $default_qty, $sort, $custom_data = array(), $line_intents = null)
{
	$encoded = encode_purch_template_custom_data($custom_data);
	if ($encoded === null && $custom_data !== null && $custom_data !== '' && $custom_data !== array())
		return false;
	$sql = "INSERT INTO " . TB_PREF . "purch_order_template_lines
		(template_id, stock_id, description, default_quantity, sort_order, custom_data)
		VALUES (
			" . db_escape((int)$template_id) . ",
			" . db_escape($stock_id) . ",
			" . db_escape($description) . ",
			" . db_escape((float)$default_qty) . ",
			" . db_escape((int)$sort) . ",
			" . sql_purch_template_custom_data($encoded) . ")";

	return purch_policy_dimension_write('purch_order_template_lines', null, array('custom_data' => $encoded), function() use ($sql, $template_id, $stock_id, $default_qty, $line_intents) {
		$rows = db_query('SELECT id FROM ' . TB_PREF . 'purch_order_templates WHERE id=' . (int)$template_id . ' FOR UPDATE');
		if (!$rows || db_num_rows($rows) !== 1)
			return false;
		if (!db_query($sql, 'could not add purchase template line'))
			return false;
		$line_id = (int)db_insert_id();
		// DIM-PUR-002: the line decision is appended in this same transaction.
		if ($line_id > 0 && function_exists('purchasing_classification_schema_ready') && purchasing_classification_schema_ready())
			purchasing_classification_capture_template_line($template_id,
				array('id' => $line_id, 'stock_id' => $stock_id, 'default_quantity' => $default_qty),
				$line_intents);
		return $line_id;
	});
}

/**
 * Update a purchase order template line.
 *
 * @param int    $line_id
 * @param string $stock_id
 * @param string $description
 * @param float  $default_qty
 * @param int    $sort
 * @param mixed  $custom_data Null (default) keeps the stored metadata byte-exactly; any other value replaces it
 * @param int|array|null $template_id_or_intents Parent template id (legacy 7th argument) or intent array
 * @param array|null $line_intents DIM-PUR-002 explicit line intent
 * @return bool
 */
function update_template_line($line_id, $stock_id, $description, $default_qty, $sort, $custom_data = null, $template_id_or_intents = null, $line_intents = null)
{
	$has_custom_data = $custom_data !== null;
	$encoded = $has_custom_data ? encode_purch_template_custom_data($custom_data) : null;
	if ($has_custom_data && $encoded === null && $custom_data !== '' && $custom_data !== array())
		return false;
	// The legacy 7th positional argument is the parent template identity. Intent carriers are
	// arrays, so both contracts remain unambiguous without changing an existing caller.
	$template_id = 0;
	if (is_array($template_id_or_intents))
		$line_intents = $template_id_or_intents;
	elseif ($template_id_or_intents !== null)
		$template_id = (int)$template_id_or_intents;
	$sql = "UPDATE " . TB_PREF . "purch_order_template_lines SET
		stock_id = " . db_escape($stock_id) . ",
		description = " . db_escape($description) . ",
		default_quantity = " . db_escape((float)$default_qty) . ",
		sort_order = " . db_escape((int)$sort)
		. ($has_custom_data ? ",
		custom_data = " . sql_purch_template_custom_data($encoded) : "") . "
		WHERE id = " . (int)$line_id;

	if ($template_id > 0)
		$sql .= " AND template_id = " . $template_id;

	$scoped = 'SELECT id FROM ' . TB_PREF . 'purch_order_template_lines WHERE id=' . (int)$line_id
		. ($template_id > 0 ? ' AND template_id=' . $template_id : '') . ' FOR UPDATE';

	return purch_policy_dimension_write('purch_order_template_lines', $line_id, $has_custom_data ? array('custom_data' => $encoded) : array(), function() use ($sql, $scoped, $line_id, $stock_id, $default_qty, $line_intents) {
		// A mismatched parent id is refused instead of silently changing nothing.
		$rows = db_query($scoped);
		if (!$rows || db_num_rows($rows) !== 1)
			return false;
		if (!db_query($sql, 'could not update purchase template line'))
			return false;
		$rows = db_query('SELECT template_id FROM ' . TB_PREF . 'purch_order_template_lines WHERE id=' . (int)$line_id);
		$row = $rows ? db_fetch_assoc($rows) : false;
		if ($row && function_exists('purchasing_classification_schema_ready') && purchasing_classification_schema_ready()
			&& ($line_intents !== null
				|| !purchasing_classification_current_line_revision_id(ST_PURCHTEMPLATE,
					(int)$row['template_id'], $line_id)))
			purchasing_classification_capture_template_line((int)$row['template_id'],
				array('id' => $line_id, 'stock_id' => $stock_id, 'default_quantity' => $default_qty),
				$line_intents);
		return true;
	});
}

/**
 * Delete one purchase order template line.
 *
 * @param int $line_id
 * @return bool
 */
function delete_template_line($line_id)
{
	$sql = "DELETE FROM " . TB_PREF . "purch_order_template_lines WHERE id = " . (int)$line_id;

	if ((int)func_num_args() >= 2) {
		$template_id = (int)func_get_arg(1);
		if ($template_id > 0)
			$sql .= " AND template_id = " . $template_id;
	}

	$scoped = 'SELECT id FROM ' . TB_PREF . 'purch_order_template_lines WHERE id=' . (int)$line_id
		. (isset($template_id) && $template_id > 0 ? ' AND template_id=' . $template_id : '') . ' FOR UPDATE';

	return purch_policy_dimension_write('purch_order_template_lines', $line_id, array(), function() use ($sql, $scoped) {
		$rows = db_query($scoped);
		if (!$rows || db_num_rows($rows) !== 1)
			return false;
		return db_query($sql, 'could not delete purchase template line') ? true : false;
	}, true);
}

/**
 * Get one template line row.
 *
 * @param int $line_id
 * @param int $template_id
 * @return array|false
 */
function get_template_line($line_id, $template_id = 0)
{
	$sql = "SELECT * FROM " . TB_PREF . "purch_order_template_lines WHERE id = " . (int)$line_id;
	if ((int)$template_id > 0)
		$sql .= " AND template_id = " . (int)$template_id;

	$result = db_query($sql, 'could not retrieve purchase template line');

	return db_fetch($result);
}

/**
 * Get template lines for one purchase order template.
 *
 * @param int $template_id
 * @return resource
 */
function get_template_lines($template_id)
{
	$sql = "SELECT template_line.*, stock.description AS stock_description, stock.units AS stock_units
		FROM " . TB_PREF . "purch_order_template_lines template_line
		LEFT JOIN " . TB_PREF . "stock_master stock ON stock.stock_id = template_line.stock_id
		WHERE template_line.template_id = " . (int)$template_id . "
		ORDER BY template_line.sort_order, template_line.id";

	return db_query($sql, 'could not retrieve purchase template lines');
}

/**
 * Get the delivery address configured for one stock location.
 *
 * @param string $location_code
 * @return string
 */
function get_purch_template_location_delivery_address($location_code)
{
	if ($location_code === '')
		return '';

	$sql = "SELECT delivery_address FROM " . TB_PREF . "locations WHERE loc_code = " . db_escape($location_code);
	$result = db_query($sql, 'could not retrieve template location delivery address');
	$row = db_fetch($result);

	return $row ? $row['delivery_address'] : '';
}

/**
 * Apply a purchase template to the current purchase order cart.
 *
 * @param purch_order $purchase_order
 * @param int         $template_id
 * @return bool
 */
function apply_template_to_po(&$purchase_order, $template_id)
{
	$template = get_purch_template($template_id);
	if (!$template)
		return false;

	if (!empty($template['inactive']))
		return false;

	$purchase_order->purch_template_id = (int)$template_id;

	if ((int)$template['supplier_id'] > 0 && (int)$purchase_order->supplier_id !== (int)$template['supplier_id'])
		get_supplier_details_to_order($purchase_order, (int)$template['supplier_id']);

	$purchase_order->line_items = array();
	$purchase_order->lines_on_order = 0;

	if ($template['delivery_location'] !== '') {
		$purchase_order->Location = $template['delivery_location'];
		$purchase_order->delivery_address = get_purch_template_location_delivery_address($template['delivery_location']);
	}

	if ((int)$template['default_payment_terms'] > 0)
		$purchase_order->terms = (int)$template['default_payment_terms'];

	if (trim($template['notes']) !== '')
		$purchase_order->Comments = $template['notes'];

	$template_lines = get_template_lines($template_id);
	$classification_lines = array();
	while ($template_line = db_fetch($template_lines)) {
		$default_quantity = (float)$template_line['default_quantity'];
		if ($default_quantity <= 0)
			continue;

		$default_description = trim($template_line['description']) !== '' ? $template_line['description'] : $template_line['stock_description'];
		$required_date = $purchase_order->orig_order_date ? $purchase_order->orig_order_date : Today();
		$unit_price = (int)$purchase_order->supplier_id > 0
			? get_purchase_price(
				$purchase_order->supplier_id,
				$template_line['stock_id'],
				$default_quantity,
				date2sql($required_date),
				isset($purchase_order->agreement_id) ? (int)$purchase_order->agreement_id : 0
			)
			: 0;

		$purchase_order->add_to_order(
			count($purchase_order->line_items),
			$template_line['stock_id'],
			$default_quantity,
			$default_description,
			$unit_price,
			$template_line['stock_units'],
			$required_date,
			0,
			0
		);
		$classification_lines[(int)$template_line['id']] = count($purchase_order->line_items) - 1;
	}

	// DIM-PUR-002: a saved template's exact header/line intent is copied into the order cart.
	function_exists('purchasing_classification_apply_template') && purchasing_classification_apply_template($purchase_order, (int)$template_id, $classification_lines);

	$hook_data = array(
		'template_id' => (int)$template_id,
		'purchase_order' => $purchase_order,
		'template' => $template,
	);
	hook_invoke_all('purch_template_applied', $hook_data);

	return $purchase_order->order_has_items();
}