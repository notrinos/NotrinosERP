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
 * Sales Quotation Template Database Functions
 *
 * Provides CRUD and application functions for quotation templates.
 * Quotation templates allow pre-defining sets of products (required and
 * optional/upsell) that can be loaded into a new quotation in one click.
 *
 * NOTE: so_type=1 on 0_sales_orders means "Invoice Template" (for recurrent
 * invoicing). The quotation templates managed here are an entirely separate
 * concept stored in 0_sales_quotation_templates. Use distinct UI labels.
 *
 * @package NotrinosERP
 * @subpackage Sales
 */

// ============================================================================
// QUOTATION TEMPLATE CRUD
// ============================================================================

/**
 * Add a new quotation template.
 *
 * @param string $name             Template name.
 * @param string $description      Optional description.
 * @param int    $validity_days    Auto-set expiry on quotes using this template.
 * @param string $terms_and_conditions Full T&C text.
 * @param string $notes            Internal notes.
 * @param int    $sales_type       Default sales type (0 = use order default).
 * @param int    $default_payment_terms  Default payment terms.
 * @param int    $default_ship_via Default shipper.
 * @return int  The new template ID.
 */
function add_quotation_template($name, $description = '', $validity_days = 30,
	$terms_and_conditions = '', $notes = '', $sales_type = 0,
	$default_payment_terms = 0, $default_ship_via = 0)
{
	$sql = "INSERT INTO " . TB_PREF . "sales_quotation_templates
		(name, description, validity_days, terms_and_conditions, notes,
		 sales_type, default_payment_terms, default_ship_via, inactive)
		VALUES (" .
			db_escape($name) . ", " .
			db_escape($description) . ", " .
			db_escape((int)$validity_days) . ", " .
			db_escape($terms_and_conditions) . ", " .
			db_escape($notes) . ", " .
			db_escape((int)$sales_type) . ", " .
			db_escape((int)$default_payment_terms) . ", " .
			db_escape((int)$default_ship_via) . ", " .
			"0)";

	db_query($sql, "Could not add quotation template");
	return db_insert_id();
}

/**
 * Update an existing quotation template.
 *
 * @param int    $id               Template ID.
 * @param string $name             Template name.
 * @param string $description      Optional description.
 * @param int    $validity_days    Auto-set expiry on quotes using this template.
 * @param string $terms_and_conditions Full T&C text.
 * @param string $notes            Internal notes.
 * @param int    $sales_type       Default sales type.
 * @param int    $default_payment_terms Default payment terms.
 * @param int    $default_ship_via Default shipper.
 * @param int    $inactive         1 to deactivate, 0 to activate.
 * @return bool
 */
function update_quotation_template($id, $name, $description = '', $validity_days = 30,
	$terms_and_conditions = '', $notes = '', $sales_type = 0,
	$default_payment_terms = 0, $default_ship_via = 0, $inactive = 0)
{
	$sql = "UPDATE " . TB_PREF . "sales_quotation_templates SET
		name = " . db_escape($name) . ",
		description = " . db_escape($description) . ",
		validity_days = " . db_escape((int)$validity_days) . ",
		terms_and_conditions = " . db_escape($terms_and_conditions) . ",
		notes = " . db_escape($notes) . ",
		sales_type = " . db_escape((int)$sales_type) . ",
		default_payment_terms = " . db_escape((int)$default_payment_terms) . ",
		default_ship_via = " . db_escape((int)$default_ship_via) . ",
		inactive = " . db_escape((int)$inactive) . "
		WHERE id = " . db_escape((int)$id);

	return db_query($sql, "Could not update quotation template");
}

/**
 * Delete a quotation template and all its lines.
 *
 * @param int $id Template ID.
 * @return bool
 */
function delete_quotation_template($id)
{
	begin_transaction();
	db_query("DELETE FROM " . TB_PREF . "sales_quotation_template_lines WHERE template_id = " . db_escape((int)$id),
		"Could not delete quotation template lines");
	$ret = db_query("DELETE FROM " . TB_PREF . "sales_quotation_templates WHERE id = " . db_escape((int)$id),
		"Could not delete quotation template");
	commit_transaction();
	return $ret;
}

/**
 * Retrieve a single quotation template by ID.
 *
 * @param int $id Template ID.
 * @return array|false Associative array or false.
 */
function get_quotation_template($id)
{
	$sql = "SELECT * FROM " . TB_PREF . "sales_quotation_templates WHERE id = " . db_escape((int)$id);
	$result = db_query($sql, "Could not get quotation template");
	return db_fetch($result);
}

/**
 * Get all quotation templates, optionally including inactive ones.
 *
 * @param bool $include_inactive If true, returns inactive templates too.
 * @return resource Database result.
 */
function get_quotation_templates($include_inactive = false)
{
	$sql = "SELECT * FROM " . TB_PREF . "sales_quotation_templates";
	if (!$include_inactive)
		$sql .= " WHERE inactive = 0";
	$sql .= " ORDER BY name";
	return db_query($sql, "Could not get quotation templates");
}

// ============================================================================
// TEMPLATE LINE CRUD
// ============================================================================

/**
 * Add a line to a quotation template.
 *
 * @param int    $template_id     Parent template ID.
 * @param string $line_type       'product', 'section', 'note', or 'optional'.
 * @param string $stock_id        Stock item code (empty for section/note lines).
 * @param string $description     Line description.
 * @param double $quantity        Default quantity.
 * @param double $discount_percent Default discount %.
 * @param int    $is_optional     1 if this is an optional/upsell product.
 * @param int    $sort_order      Display order.
 * @return int New line ID.
 */
function add_quotation_template_line($template_id, $line_type = 'product', $stock_id = '',
	$description = '', $quantity = 1, $discount_percent = 0, $is_optional = 0, $sort_order = 0)
{
	$sql = "INSERT INTO " . TB_PREF . "sales_quotation_template_lines
		(template_id, line_type, stock_id, description, quantity, discount_percent,
		 is_optional, sort_order)
		VALUES (" .
			db_escape((int)$template_id) . ", " .
			db_escape($line_type) . ", " .
			db_escape($stock_id) . ", " .
			db_escape($description) . ", " .
			db_escape($quantity) . ", " .
			db_escape($discount_percent) . ", " .
			db_escape((int)$is_optional) . ", " .
			db_escape((int)$sort_order) . ")";

	db_query($sql, "Could not add template line");
	return db_insert_id();
}

/**
 * Update an existing template line.
 *
 * @param int    $id              Line ID.
 * @param string $line_type       Line type.
 * @param string $stock_id        Stock item code.
 * @param string $description     Line description.
 * @param double $quantity        Default quantity.
 * @param double $discount_percent Default discount %.
 * @param int    $is_optional     1 if optional.
 * @param int    $sort_order      Display order.
 * @return bool
 */
function update_quotation_template_line($id, $line_type, $stock_id, $description,
	$quantity, $discount_percent, $is_optional, $sort_order)
{
	$sql = "UPDATE " . TB_PREF . "sales_quotation_template_lines SET
		line_type = " . db_escape($line_type) . ",
		stock_id = " . db_escape($stock_id) . ",
		description = " . db_escape($description) . ",
		quantity = " . db_escape($quantity) . ",
		discount_percent = " . db_escape($discount_percent) . ",
		is_optional = " . db_escape((int)$is_optional) . ",
		sort_order = " . db_escape((int)$sort_order) . "
		WHERE id = " . db_escape((int)$id);

	return db_query($sql, "Could not update template line");
}

/**
 * Delete a single template line.
 *
 * @param int $id Line ID.
 * @return bool
 */
function delete_quotation_template_line($id)
{
	return db_query("DELETE FROM " . TB_PREF . "sales_quotation_template_lines WHERE id = " . db_escape((int)$id),
		"Could not delete template line");
}

/**
 * Get all lines for a quotation template, ordered by sort_order then id.
 *
 * @param int $template_id Template ID.
 * @return resource Database result.
 */
function get_quotation_template_lines($template_id)
{
	$sql = "SELECT tl.*, sm.long_description AS item_long_desc,
		sm.units, sm.material_cost
		FROM " . TB_PREF . "sales_quotation_template_lines tl
		LEFT JOIN " . TB_PREF . "stock_master sm ON sm.stock_id = tl.stock_id
		WHERE tl.template_id = " . db_escape((int)$template_id) . "
		ORDER BY tl.sort_order, tl.id";
	return db_query($sql, "Could not get template lines");
}

// ============================================================================
// APPLY TEMPLATE TO CART
// ============================================================================

/**
 * Populate a Cart with lines from a quotation template.
 *
 * Required product lines are added to $cart->line_items.
 * Optional/upsell lines are added to $cart->optional_items.
 * Pricing is resolved via the standard get_price() function which
 * respects the advanced pricelist engine if enabled.
 *
 * @param object $cart        Cart instance (by reference).
 * @param int    $template_id Template ID to apply.
 * @return bool  False if template not found.
 */
function apply_template_to_cart(&$cart, $template_id)
{
	global $path_to_root;

	$template = get_quotation_template($template_id);
	if (!$template)
		return false;

	hook_invoke_all('sales_template_apply', $cart, $template_id);

	// Set template-level defaults
	$cart->template_id = (int)$template_id;
	if (!empty($template['terms_and_conditions']))
		$cart->terms_and_conditions = $template['terms_and_conditions'];

	$lines = get_quotation_template_lines($template_id);
	while ($line = db_fetch($lines)) {
		// Skip section/note lines (no stock_id needed)
		if (in_array($line['line_type'], array('section', 'note')))
			continue;

		if (empty($line['stock_id']))
			continue;

		// Resolve price via standard pricing engine
		$price = get_price($line['stock_id'], $cart->customer_currency,
			$cart->sales_type, $cart->price_factor, $cart->document_date, $cart->customer_id);
		if ($price === false)
			$price = 0;

		$discount = $line['discount_percent'] / 100;

		// Apply customer default discount if line has no explicit discount
		if ($line['discount_percent'] == 0 && $cart->default_discount > 0)
			$discount = $cart->default_discount;

		if ($line['is_optional'] || $line['line_type'] === 'optional') {
			// Add to optional items list (separate from main line_items)
			$cart->optional_items[] = array(
				'stock_id'        => $line['stock_id'],
				'description'     => $line['description'] ? $line['description'] : $line['item_long_desc'],
				'quantity'        => $line['quantity'],
				'price'           => $price,
				'discount_percent'=> $discount,
				'units'           => $line['units'],
				'sort_order'      => $line['sort_order'],
				'template_line_id'=> $line['id'],
			);
		} else {
			// Add required product to cart line items
			add_to_order($cart, $line['stock_id'], $line['quantity'], $price,
				$discount,
				$line['description'] ? $line['description'] : '');
		}
	}

	return true;
}

