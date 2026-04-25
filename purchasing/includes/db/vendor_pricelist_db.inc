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
include_once($path_to_root . '/purchasing/includes/db/suppliers_db.inc');

/**
 * Normalize a vendor pricelist date into SQL format.
 *
 * @param string $date
 * @param string $fallback
 * @return string
 */
function normalize_vendor_pricelist_sql_date($date, $fallback = '')
{
	if ($date === null || $date === '')
		return $fallback;

	if (preg_match('/^\d{4}-\d{2}-\d{2}$/', $date))
		return $date;

	return is_date($date) ? date2sql($date) : $fallback;
}

/**
 * Encode custom data for vendor pricelist JSON columns.
 *
 * @param mixed $custom_data
 * @return string|null
 */
function encode_vendor_pricelist_custom_data($custom_data)
{
	if ($custom_data === null || $custom_data === '' || $custom_data === array())
		return null;

	if (is_string($custom_data))
		return $custom_data;

	$encoded = json_encode($custom_data);
	return $encoded === false ? null : $encoded;
}

/**
 * Build a SQL-safe JSON literal for vendor pricelist custom data.
 *
 * @param mixed $custom_data
 * @return string
 */
function sql_vendor_pricelist_custom_data($custom_data)
{
	$encoded = encode_vendor_pricelist_custom_data($custom_data);
	return $encoded === null ? 'NULL' : "'" . db_escape_function($encoded) . "'";
}

/**
 * Decode vendor pricelist custom data from storage.
 *
 * @param string|null $custom_data
 * @return array
 */
function decode_vendor_pricelist_custom_data($custom_data)
{
	if (!$custom_data)
		return array();

	$decoded = json_decode($custom_data, true);
	if (!is_array($decoded))
		$decoded = json_decode(html_entity_decode($custom_data, ENT_QUOTES, 'UTF-8'), true);

	return is_array($decoded) ? $decoded : array();
}

/**
 * Normalize the quantity break price bundle for one vendor pricelist row.
 *
 * @param array $prices
 * @return array
 */
function normalize_vendor_pricelist_prices($prices)
{
	$defaults = array(
		'price_break_qty_1' => 0,
		'price_1' => 0,
		'price_break_qty_2' => 0,
		'price_2' => 0,
		'price_break_qty_3' => 0,
		'price_3' => 0,
		'price_break_qty_4' => 0,
		'price_4' => 0,
	);

	if (!is_array($prices))
		return $defaults;

	foreach ($defaults as $key => $default_value) {
		if (isset($prices[$key]))
			$defaults[$key] = (float)$prices[$key];
	}

	return $defaults;
}

/**
 * Get the raw purch_data row for one supplier and item.
 *
 * @param int    $supplier_id
 * @param string $stock_id
 * @return array|false
 */
function get_purchase_data_row_direct($supplier_id, $stock_id)
{
	$sql = "SELECT *
		FROM " . TB_PREF . "purch_data
		WHERE supplier_id = " . db_escape($supplier_id) . "
			AND stock_id = " . db_escape($stock_id);
	$result = db_query($sql, 'could not retrieve supplier purchase data');

	return db_fetch($result);
}

/**
 * Get the stock defaults used by vendor pricelist and template helpers.
 *
 * @param string $stock_id
 * @return array|false
 */
function get_vendor_pricelist_stock_defaults($stock_id)
{
	$sql = "SELECT stock_id, description, units
		FROM " . TB_PREF . "stock_master
		WHERE stock_id = " . db_escape($stock_id);
	$result = db_query($sql, 'could not retrieve stock defaults for vendor pricelist');

	return db_fetch($result);
}

/**
 * Resolve the best raw vendor-unit price from one vendor pricelist row.
 *
 * @param array $pricelist_row
 * @param float $quantity
 * @return array
 */
function resolve_vendor_pricelist_break_price($pricelist_row, $quantity)
{
	$quantity = max(0, (float)$quantity);
	$resolved_price = 0;
	$matched_break_level = 0;
	$matched_break_qty = 0;

	for ($index = 4; $index >= 1; $index--) {
		$break_qty = isset($pricelist_row['price_break_qty_' . $index]) ? (float)$pricelist_row['price_break_qty_' . $index] : 0;
		$break_price = isset($pricelist_row['price_' . $index]) ? (float)$pricelist_row['price_' . $index] : 0;
		if ($break_price <= 0)
			continue;

		if ($break_qty <= 0 || $quantity >= $break_qty) {
			$resolved_price = $break_price;
			$matched_break_level = $index;
			$matched_break_qty = $break_qty;
			break;
		}
	}

	if ($resolved_price <= 0 && isset($pricelist_row['price_1']) && (float)$pricelist_row['price_1'] > 0) {
		$resolved_price = (float)$pricelist_row['price_1'];
		$matched_break_level = 1;
		$matched_break_qty = isset($pricelist_row['price_break_qty_1']) ? (float)$pricelist_row['price_break_qty_1'] : 0;
	}

	return array(
		'break_price' => $resolved_price,
		'break_level' => $matched_break_level,
		'break_qty' => $matched_break_qty,
	);
}

/**
 * Enrich one vendor pricelist row with computed pricing fields.
 *
 * @param array $pricelist_row
 * @param float $quantity
 * @return array
 */
function enrich_vendor_pricelist_row($pricelist_row, $quantity)
{
	if (!$pricelist_row)
		return $pricelist_row;

	$break_price = resolve_vendor_pricelist_break_price($pricelist_row, $quantity);
	$conversion_factor = isset($pricelist_row['conversion_factor']) && (float)$pricelist_row['conversion_factor'] > 0
		? (float)$pricelist_row['conversion_factor']
		: 1;
	$discount_percent = isset($pricelist_row['discount_percent']) ? (float)$pricelist_row['discount_percent'] : 0;
	$effective_unit_price = $break_price['break_price'] > 0
		? ($break_price['break_price'] / $conversion_factor) * (1 - ($discount_percent / 100))
		: 0;

	$pricelist_row['conversion_factor'] = $conversion_factor;
	$pricelist_row['custom_data_array'] = decode_vendor_pricelist_custom_data($pricelist_row['custom_data']);
	$pricelist_row['effective_source_price'] = round($break_price['break_price'], user_price_dec());
	$pricelist_row['effective_unit_price'] = round($effective_unit_price, user_price_dec());
	$pricelist_row['matched_break_level'] = $break_price['break_level'];
	$pricelist_row['matched_break_qty'] = $break_price['break_qty'];

	return $pricelist_row;
}

/**
 * Get one vendor pricelist entry by its identifier.
 *
 * @param int $id
 * @return array|false
 */
function get_vendor_pricelist_entry($id)
{
	$sql = "SELECT pricelist.*, supplier.supp_name, supplier.curr_code AS supplier_currency,
		stock.description AS stock_description, stock.units AS stock_units
		FROM " . TB_PREF . "vendor_pricelists pricelist
		INNER JOIN " . TB_PREF . "suppliers supplier ON supplier.supplier_id = pricelist.supplier_id
		LEFT JOIN " . TB_PREF . "stock_master stock ON stock.stock_id = pricelist.stock_id
		WHERE pricelist.id = " . (int)$id;
	$result = db_query($sql, 'could not retrieve vendor pricelist entry');
	$row = db_fetch($result);

	return $row ? enrich_vendor_pricelist_row($row, 0) : false;
}

/**
 * Get one vendor pricelist row by supplier, item, and currency key.
 *
 * @param int    $supplier_id
 * @param string $stock_id
 * @param string $currency
 * @return array|false
 */
function get_vendor_pricelist_entry_by_key($supplier_id, $stock_id, $currency = '')
{
	$sql = "SELECT *
		FROM " . TB_PREF . "vendor_pricelists
		WHERE supplier_id = " . db_escape($supplier_id) . "
			AND stock_id = " . db_escape($stock_id);
	if ($currency !== '')
		$sql .= " AND currency = " . db_escape($currency);
	$sql .= " ORDER BY id DESC";
	$result = db_query($sql, 'could not retrieve vendor pricelist row by key');

	return db_fetch($result);
}

/**
 * Get vendor pricelist rows for one supplier and optional item.
 *
 * @param int    $supplier_id
 * @param string $stock_id
 * @param bool   $active_only
 * @return resource
 */
function get_vendor_pricelist($supplier_id, $stock_id = '', $active_only = true)
{
	$effective_date = date('Y-m-d');
	$sql = "SELECT pricelist.*, supplier.supp_name, supplier.curr_code AS supplier_currency,
		stock.description AS stock_description, stock.units AS stock_units
		FROM " . TB_PREF . "vendor_pricelists pricelist
		INNER JOIN " . TB_PREF . "suppliers supplier ON supplier.supplier_id = pricelist.supplier_id
		LEFT JOIN " . TB_PREF . "stock_master stock ON stock.stock_id = pricelist.stock_id
		WHERE 1 = 1";

	if ((int)$supplier_id > 0)
		$sql .= " AND pricelist.supplier_id = " . db_escape($supplier_id);
	if ($stock_id !== '')
		$sql .= " AND pricelist.stock_id = " . db_escape($stock_id);
	if ($active_only) {
		$sql .= " AND pricelist.inactive = 0
			AND (pricelist.valid_from IS NULL OR pricelist.valid_from = '0000-00-00' OR pricelist.valid_from <= " . db_escape($effective_date) . ")
			AND (pricelist.valid_until IS NULL OR pricelist.valid_until = '0000-00-00' OR pricelist.valid_until >= " . db_escape($effective_date) . ")";
	}

	$sql .= " ORDER BY pricelist.stock_id, pricelist.is_preferred DESC, pricelist.currency, pricelist.id DESC";

	return db_query($sql, 'could not retrieve vendor pricelist rows');
}

/**
 * Get the best matching vendor pricelist row for one supplier and item.
 *
 * @param int    $supplier_id
 * @param string $stock_id
 * @param float  $quantity
 * @param string $currency
 * @param string $date
 * @param bool   $active_only
 * @return array|false
 */
function get_vendor_pricelist_match($supplier_id, $stock_id, $quantity = 0, $currency = '', $date = '', $active_only = true)
{
	$effective_date = normalize_vendor_pricelist_sql_date($date, date('Y-m-d'));
	$sql = "SELECT pricelist.*, supplier.supp_name, supplier.curr_code AS supplier_currency,
		stock.description AS stock_description, stock.units AS stock_units
		FROM " . TB_PREF . "vendor_pricelists pricelist
		INNER JOIN " . TB_PREF . "suppliers supplier ON supplier.supplier_id = pricelist.supplier_id
		LEFT JOIN " . TB_PREF . "stock_master stock ON stock.stock_id = pricelist.stock_id
		WHERE pricelist.supplier_id = " . db_escape($supplier_id) . "
			AND pricelist.stock_id = " . db_escape($stock_id);

	if ($active_only) {
		$sql .= " AND pricelist.inactive = 0
			AND (pricelist.valid_from IS NULL OR pricelist.valid_from = '0000-00-00' OR pricelist.valid_from <= " . db_escape($effective_date) . ")
			AND (pricelist.valid_until IS NULL OR pricelist.valid_until = '0000-00-00' OR pricelist.valid_until >= " . db_escape($effective_date) . ")";
	}

	if ($currency !== '') {
		$sql .= " AND (pricelist.currency = '' OR pricelist.currency = " . db_escape($currency) . ")";
		$sql .= " ORDER BY CASE WHEN pricelist.currency = " . db_escape($currency) . " THEN 0 ELSE 1 END,";
	} else {
		$sql .= " ORDER BY";
	}

	$sql .= " pricelist.is_preferred DESC, pricelist.valid_from DESC, pricelist.id DESC";
	$result = db_query($sql, 'could not retrieve matching vendor pricelist row');
	$row = db_fetch($result);

	return $row ? enrich_vendor_pricelist_row($row, $quantity) : false;
}

/**
 * Build a fallback pricing context from the legacy purch_data table.
 *
 * @param int    $supplier_id
 * @param string $stock_id
 * @return array
 */
function get_purchase_data_fallback_context($supplier_id, $stock_id)
{
	$data = get_purchase_data_row_direct($supplier_id, $stock_id);
	if (!$data)
		return array();

	$conversion_factor = !empty($data['conversion_factor']) ? (float)$data['conversion_factor'] : 1;
	$price = $conversion_factor > 0 ? (float)$data['price'] / $conversion_factor : (float)$data['price'];

	return array(
		'entry_id' => 0,
		'supplier_id' => (int)$supplier_id,
		'stock_id' => $stock_id,
		'price' => round($price, user_price_dec()),
		'source' => 'purch_data',
		'currency' => '',
		'conversion_factor' => $conversion_factor > 0 ? $conversion_factor : 1,
		'lead_time_days' => 0,
		'vendor_uom' => isset($data['suppliers_uom']) ? $data['suppliers_uom'] : '',
		'vendor_product_code' => '',
		'vendor_product_name' => isset($data['supplier_description']) ? $data['supplier_description'] : '',
		'discount_percent' => 0,
		'matched_break_level' => 0,
		'matched_break_qty' => 0,
		'effective_source_price' => round((float)$data['price'], user_price_dec()),
		'last_purchase_price' => round($price, user_price_dec()),
		'last_purchase_date' => '',
	);
}

/**
 * Resolve the complete pricing context for one supplier and item.
 *
 * @param int     $supplier_id
 * @param string  $stock_id
 * @param float   $quantity
 * @param string  $currency
 * @param string  $date
 * @param int     $agreement_id
 * @param boolean $use_vendor_pricelist
 * @return array
 */
function get_supplier_pricing_context($supplier_id, $stock_id, $quantity = 0, $currency = '', $date = '', $agreement_id = 0, $use_vendor_pricelist = true)
{
	$effective_date = normalize_vendor_pricelist_sql_date($date, date('Y-m-d'));
	$pricing_context = array(
		'entry_id' => 0,
		'supplier_id' => (int)$supplier_id,
		'stock_id' => $stock_id,
		'price' => 0,
		'source' => 'manual',
		'currency' => $currency,
		'conversion_factor' => 1,
		'lead_time_days' => 0,
		'vendor_uom' => '',
		'vendor_product_code' => '',
		'vendor_product_name' => '',
		'discount_percent' => 0,
		'matched_break_level' => 0,
		'matched_break_qty' => 0,
		'effective_source_price' => 0,
		'last_purchase_price' => 0,
		'last_purchase_date' => '',
	);

	if (function_exists('get_agreement_price')
		&& ((int)$agreement_id > 0 || get_company_pref('use_purchase_agreements'))) {
		$agreement_price = get_agreement_price($supplier_id, $stock_id, $quantity, $effective_date, (int)$agreement_id);
		if ($agreement_price > 0) {
			$pricing_context['price'] = round((float)$agreement_price, user_price_dec());
			$pricing_context['source'] = 'agreement';
			return $pricing_context;
		}
	}

	if ($use_vendor_pricelist) {
		$pricelist_row = get_vendor_pricelist_match($supplier_id, $stock_id, $quantity, $currency, $effective_date, true);
		if ($pricelist_row && $pricelist_row['effective_unit_price'] > 0) {
			$pricing_context['entry_id'] = (int)$pricelist_row['id'];
			$pricing_context['price'] = (float)$pricelist_row['effective_unit_price'];
			$pricing_context['source'] = 'vendor_pricelist';
			$pricing_context['currency'] = $pricelist_row['currency'] !== '' ? $pricelist_row['currency'] : $currency;
			$pricing_context['conversion_factor'] = (float)$pricelist_row['conversion_factor'];
			$pricing_context['lead_time_days'] = (int)$pricelist_row['lead_time_days'];
			$pricing_context['vendor_uom'] = $pricelist_row['vendor_uom'];
			$pricing_context['vendor_product_code'] = $pricelist_row['vendor_product_code'];
			$pricing_context['vendor_product_name'] = $pricelist_row['vendor_product_name'];
			$pricing_context['discount_percent'] = (float)$pricelist_row['discount_percent'];
			$pricing_context['matched_break_level'] = (int)$pricelist_row['matched_break_level'];
			$pricing_context['matched_break_qty'] = (float)$pricelist_row['matched_break_qty'];
			$pricing_context['effective_source_price'] = (float)$pricelist_row['effective_source_price'];
			$pricing_context['last_purchase_price'] = (float)$pricelist_row['last_purchase_price'];
			$pricing_context['last_purchase_date'] = $pricelist_row['last_purchase_date'];
			return $pricing_context;
		}
	}

	$fallback_context = get_purchase_data_fallback_context($supplier_id, $stock_id);
	if ($fallback_context)
		return $fallback_context;

	return $pricing_context;
}

/**
 * Get all known vendor price options for one item.
 *
 * @param string $stock_id
 * @param bool   $active_only
 * @param float  $quantity
 * @param string $currency
 * @param string $date
 * @return array
 */
function get_item_vendors($stock_id, $active_only = true, $quantity = 0, $currency = '', $date = '')
{
	$sql = "SELECT DISTINCT source_rows.supplier_id
		FROM (
			SELECT supplier_id
			FROM " . TB_PREF . "vendor_pricelists
			WHERE stock_id = " . db_escape($stock_id) .
			($active_only ? " AND inactive = 0" : '') . "
			UNION
			SELECT supplier_id
			FROM " . TB_PREF . "purch_data
			WHERE stock_id = " . db_escape($stock_id) . "
		) source_rows
		ORDER BY source_rows.supplier_id";
	$result = db_query($sql, 'could not retrieve item vendors');
	$vendors = array();

	while ($row = db_fetch($result)) {
		$pricing_context = get_supplier_pricing_context(
			(int)$row['supplier_id'],
			$stock_id,
			$quantity,
			$currency,
			$date,
			0,
			true
		);
		if ($pricing_context['price'] <= 0)
			continue;

		$supplier_name = get_supplier_name((int)$row['supplier_id']);
		$pricing_context['supplier_name'] = $supplier_name;
		$vendors[] = $pricing_context;
	}

	usort($vendors, 'sort_item_vendor_prices');

	return $vendors;
}

/**
 * Compare two vendor price rows for stable sorting.
 *
 * @param array $left
 * @param array $right
 * @return int
 */
function sort_item_vendor_prices($left, $right)
{
	if ((float)$left['price'] === (float)$right['price'])
		return strcmp($left['supplier_name'], $right['supplier_name']);

	return ((float)$left['price'] < (float)$right['price']) ? -1 : 1;
}

/**
 * Get the best vendor price context for one item.
 *
 * @param string $stock_id
 * @param float  $quantity
 * @param string $currency
 * @param string $date
 * @param int    $supplier_id
 * @param int    $agreement_id
 * @return array
 */
function get_best_vendor_price($stock_id, $quantity, $currency = '', $date = '', $supplier_id = 0, $agreement_id = 0)
{
	if ((int)$supplier_id > 0)
		return get_supplier_pricing_context($supplier_id, $stock_id, $quantity, $currency, $date, $agreement_id, true);

	$vendors = get_item_vendors($stock_id, true, $quantity, $currency, $date);
	return count($vendors) ? $vendors[0] : array('price' => 0, 'source' => 'manual');
}

/**
 * Add a vendor pricelist entry.
 *
 * @param int    $supplier_id
 * @param string $stock_id
 * @param string $vendor_product_code
 * @param string $currency
 * @param array  $prices
 * @param string $vendor_product_name
 * @param string $vendor_uom
 * @param float  $conversion_factor
 * @param float  $min_order_qty
 * @param int    $lead_time_days
 * @param string $valid_from
 * @param string $valid_until
 * @param float  $discount_percent
 * @param string $notes
 * @param int    $is_preferred
 * @param int    $inactive
 * @param mixed  $custom_data
 * @return int
 */
function add_vendor_pricelist_entry($supplier_id, $stock_id, $vendor_product_code, $currency, $prices, $vendor_product_name = '', $vendor_uom = '', $conversion_factor = 1, $min_order_qty = 0, $lead_time_days = 0, $valid_from = '', $valid_until = '', $discount_percent = 0, $notes = '', $is_preferred = 0, $inactive = 0, $custom_data = array())
{
	$prices = normalize_vendor_pricelist_prices($prices);
	$valid_from = normalize_vendor_pricelist_sql_date($valid_from, null);
	$valid_until = normalize_vendor_pricelist_sql_date($valid_until, null);

	$sql = "INSERT INTO " . TB_PREF . "vendor_pricelists
		(supplier_id, stock_id, vendor_product_code, vendor_product_name, vendor_uom,
		conversion_factor, currency, min_order_qty,
		price_break_qty_1, price_1, price_break_qty_2, price_2,
		price_break_qty_3, price_3, price_break_qty_4, price_4,
		lead_time_days, valid_from, valid_until, discount_percent,
		last_purchase_date, last_purchase_price, notes, is_preferred, inactive, custom_data)
		VALUES (
			" . db_escape($supplier_id) . ",
			" . db_escape($stock_id) . ",
			" . db_escape($vendor_product_code) . ",
			" . db_escape($vendor_product_name) . ",
			" . db_escape($vendor_uom) . ",
			" . db_escape(max(1, (float)$conversion_factor)) . ",
			" . db_escape($currency) . ",
			" . db_escape((float)$min_order_qty) . ",
			" . db_escape($prices['price_break_qty_1']) . ",
			" . db_escape($prices['price_1']) . ",
			" . db_escape($prices['price_break_qty_2']) . ",
			" . db_escape($prices['price_2']) . ",
			" . db_escape($prices['price_break_qty_3']) . ",
			" . db_escape($prices['price_3']) . ",
			" . db_escape($prices['price_break_qty_4']) . ",
			" . db_escape($prices['price_4']) . ",
			" . db_escape((int)$lead_time_days) . ",
			" . ($valid_from ? db_escape($valid_from) : 'NULL') . ",
			" . ($valid_until ? db_escape($valid_until) : 'NULL') . ",
			" . db_escape((float)$discount_percent) . ",
			NULL,
			0,
			" . db_escape($notes) . ",
			" . db_escape((int)$is_preferred) . ",
			" . db_escape((int)$inactive) . ",
			" . sql_vendor_pricelist_custom_data($custom_data) . ")";
	db_query($sql, 'could not add vendor pricelist entry');

	return db_insert_id();
}

/**
 * Update a vendor pricelist entry.
 *
 * @param int    $id
 * @param int    $supplier_id
 * @param string $stock_id
 * @param string $vendor_product_code
 * @param string $currency
 * @param array  $prices
 * @param string $vendor_product_name
 * @param string $vendor_uom
 * @param float  $conversion_factor
 * @param float  $min_order_qty
 * @param int    $lead_time_days
 * @param string $valid_from
 * @param string $valid_until
 * @param float  $discount_percent
 * @param string $notes
 * @param int    $is_preferred
 * @param int    $inactive
 * @param mixed  $custom_data
 * @return bool
 */
function update_vendor_pricelist_entry($id, $supplier_id, $stock_id, $vendor_product_code, $currency, $prices, $vendor_product_name = '', $vendor_uom = '', $conversion_factor = 1, $min_order_qty = 0, $lead_time_days = 0, $valid_from = '', $valid_until = '', $discount_percent = 0, $notes = '', $is_preferred = 0, $inactive = 0, $custom_data = array())
{
	$prices = normalize_vendor_pricelist_prices($prices);
	$valid_from = normalize_vendor_pricelist_sql_date($valid_from, null);
	$valid_until = normalize_vendor_pricelist_sql_date($valid_until, null);

	$sql = "UPDATE " . TB_PREF . "vendor_pricelists SET
		supplier_id = " . db_escape($supplier_id) . ",
		stock_id = " . db_escape($stock_id) . ",
		vendor_product_code = " . db_escape($vendor_product_code) . ",
		vendor_product_name = " . db_escape($vendor_product_name) . ",
		vendor_uom = " . db_escape($vendor_uom) . ",
		conversion_factor = " . db_escape(max(1, (float)$conversion_factor)) . ",
		currency = " . db_escape($currency) . ",
		min_order_qty = " . db_escape((float)$min_order_qty) . ",
		price_break_qty_1 = " . db_escape($prices['price_break_qty_1']) . ",
		price_1 = " . db_escape($prices['price_1']) . ",
		price_break_qty_2 = " . db_escape($prices['price_break_qty_2']) . ",
		price_2 = " . db_escape($prices['price_2']) . ",
		price_break_qty_3 = " . db_escape($prices['price_break_qty_3']) . ",
		price_3 = " . db_escape($prices['price_3']) . ",
		price_break_qty_4 = " . db_escape($prices['price_break_qty_4']) . ",
		price_4 = " . db_escape($prices['price_4']) . ",
		lead_time_days = " . db_escape((int)$lead_time_days) . ",
		valid_from = " . ($valid_from ? db_escape($valid_from) : 'NULL') . ",
		valid_until = " . ($valid_until ? db_escape($valid_until) : 'NULL') . ",
		discount_percent = " . db_escape((float)$discount_percent) . ",
		notes = " . db_escape($notes) . ",
		is_preferred = " . db_escape((int)$is_preferred) . ",
		inactive = " . db_escape((int)$inactive) . ",
		custom_data = " . sql_vendor_pricelist_custom_data($custom_data) . "
		WHERE id = " . (int)$id;
	db_query($sql, 'could not update vendor pricelist entry');

	return true;
}

/**
 * Delete one vendor pricelist entry.
 *
 * @param int $id
 * @return bool
 */
function delete_vendor_pricelist_entry($id)
{
	$sql = "DELETE FROM " . TB_PREF . "vendor_pricelists WHERE id = " . (int)$id;
	db_query($sql, 'could not delete vendor pricelist entry');

	return true;
}

/**
 * Update the last purchase tracking fields for one vendor/item combination.
 *
 * @param int    $supplier_id
 * @param string $stock_id
 * @param float  $price
 * @param string $date
 * @return int
 */
function update_last_purchase_price($supplier_id, $stock_id, $price, $date)
{
	$price = round((float)$price, user_price_dec());
	$effective_date = normalize_vendor_pricelist_sql_date($date, date('Y-m-d'));
	$sql = "UPDATE " . TB_PREF . "vendor_pricelists
		SET last_purchase_date = " . db_escape($effective_date) . ",
			last_purchase_price = " . db_escape($price) . "
		WHERE supplier_id = " . db_escape($supplier_id) . "
			AND stock_id = " . db_escape($stock_id);
	db_query($sql, 'could not update last purchase price in vendor pricelist');

	if (db_affected_rows() > 0)
		return db_affected_rows();

	$purchase_data = get_purchase_data_row_direct($supplier_id, $stock_id);
	$stock_defaults = get_vendor_pricelist_stock_defaults($stock_id);
	$currency = get_supplier_currency($supplier_id);
	$prices = array(
		'price_break_qty_1' => 0,
		'price_1' => $purchase_data ? (float)$purchase_data['price'] : $price,
		'price_break_qty_2' => 0,
		'price_2' => 0,
		'price_break_qty_3' => 0,
		'price_3' => 0,
		'price_break_qty_4' => 0,
		'price_4' => 0,
	);
	$entry_id = add_vendor_pricelist_entry(
		$supplier_id,
		$stock_id,
		'',
		$currency,
		$prices,
		$purchase_data && !empty($purchase_data['supplier_description']) ? $purchase_data['supplier_description'] : ($stock_defaults ? $stock_defaults['description'] : ''),
		$purchase_data && !empty($purchase_data['suppliers_uom']) ? $purchase_data['suppliers_uom'] : ($stock_defaults ? $stock_defaults['units'] : ''),
		$purchase_data && !empty($purchase_data['conversion_factor']) ? (float)$purchase_data['conversion_factor'] : 1,
		0,
		0,
		'',
		'',
		0,
		'',
		0,
		0,
		array('auto_created_from_last_purchase' => 1)
	);

	if ($entry_id > 0) {
		$sql = "UPDATE " . TB_PREF . "vendor_pricelists
			SET last_purchase_date = " . db_escape($effective_date) . ",
				last_purchase_price = " . db_escape($price) . "
			WHERE id = " . (int)$entry_id;
		db_query($sql, 'could not update newly created vendor pricelist last purchase fields');
	}

	return $entry_id;
}

/**
 * Sync one legacy purch_data row into the vendor pricelist table.
 *
 * @param int    $supplier_id
 * @param string $stock_id
 * @return int
 */
function sync_purchase_data_to_vendor_pricelist($supplier_id, $stock_id)
{
	$purchase_data = get_purchase_data_row_direct($supplier_id, $stock_id);
	if (!$purchase_data)
		return 0;

	$currency = get_supplier_currency($supplier_id);
	$existing = get_vendor_pricelist_entry_by_key($supplier_id, $stock_id, $currency);
	if ($existing) {
		$sql = "UPDATE " . TB_PREF . "vendor_pricelists SET
			vendor_product_name = CASE WHEN vendor_product_name = '' THEN " . db_escape($purchase_data['supplier_description']) . " ELSE vendor_product_name END,
			vendor_uom = CASE WHEN vendor_uom = '' THEN " . db_escape($purchase_data['suppliers_uom']) . " ELSE vendor_uom END,
			conversion_factor = CASE WHEN conversion_factor <= 0 THEN " . db_escape((float)$purchase_data['conversion_factor']) . " ELSE conversion_factor END,
			price_1 = CASE WHEN price_1 <= 0 THEN " . db_escape((float)$purchase_data['price']) . " ELSE price_1 END
			WHERE id = " . (int)$existing['id'];
		db_query($sql, 'could not sync purchase data into vendor pricelist');

		return (int)$existing['id'];
	}

	return add_vendor_pricelist_entry(
		$supplier_id,
		$stock_id,
		'',
		$currency,
		array(
			'price_break_qty_1' => 0,
			'price_1' => (float)$purchase_data['price'],
			'price_break_qty_2' => 0,
			'price_2' => 0,
			'price_break_qty_3' => 0,
			'price_3' => 0,
			'price_break_qty_4' => 0,
			'price_4' => 0,
		),
		$purchase_data['supplier_description'],
		$purchase_data['suppliers_uom'],
		(float)$purchase_data['conversion_factor'],
		0,
		0,
		'',
		'',
		0,
		'',
		0,
		0,
		array('synced_from_purch_data' => 1)
	);
}

/**
 * Import vendor pricelist rows from CSV content.
 *
 * @param int    $supplier_id
 * @param string $csv_data
 * @return array
 */
function import_vendor_pricelist($supplier_id, $csv_data)
{
	$stream = fopen('php://temp', 'r+');
	if (!$stream)
		return array('imported' => 0, 'errors' => array(_('Could not initialize CSV import stream.')));

	fwrite($stream, $csv_data);
	rewind($stream);

	$line_number = 0;
	$imported_count = 0;
	$errors = array();

	while (($row = fgetcsv($stream, 0, ',')) !== false) {
		$line_number++;
		if ($line_number === 1 && isset($row[0]) && strtolower(trim($row[0])) === 'stock_id')
			continue;
		if (count($row) < 10)
			continue;

		$stock_id = trim($row[0]);
		if ($stock_id === '') {
			$errors[] = sprintf(_('Line %s: stock_id is required.'), $line_number);
			continue;
		}

		$currency = isset($row[5]) ? trim($row[5]) : '';
		$prices = array(
			'price_break_qty_1' => isset($row[7]) ? (float)$row[7] : 0,
			'price_1' => isset($row[8]) ? (float)$row[8] : 0,
			'price_break_qty_2' => isset($row[9]) ? (float)$row[9] : 0,
			'price_2' => isset($row[10]) ? (float)$row[10] : 0,
			'price_break_qty_3' => isset($row[11]) ? (float)$row[11] : 0,
			'price_3' => isset($row[12]) ? (float)$row[12] : 0,
			'price_break_qty_4' => isset($row[13]) ? (float)$row[13] : 0,
			'price_4' => isset($row[14]) ? (float)$row[14] : 0,
		);

		$existing = get_vendor_pricelist_entry_by_key($supplier_id, $stock_id, $currency);
		if ($existing) {
			update_vendor_pricelist_entry(
				(int)$existing['id'],
				$supplier_id,
				$stock_id,
				isset($row[1]) ? trim($row[1]) : '',
				$currency,
				$prices,
				isset($row[2]) ? trim($row[2]) : '',
				isset($row[3]) ? trim($row[3]) : '',
				isset($row[4]) ? (float)$row[4] : 1,
				isset($row[6]) ? (float)$row[6] : 0,
				isset($row[15]) ? (int)$row[15] : 0,
				isset($row[16]) ? trim($row[16]) : '',
				isset($row[17]) ? trim($row[17]) : '',
				isset($row[18]) ? (float)$row[18] : 0,
				isset($row[19]) ? trim($row[19]) : '',
				isset($row[20]) ? (int)$row[20] : 0,
				isset($row[21]) ? (int)$row[21] : 0,
				array('imported' => 1)
			);
		} else {
			add_vendor_pricelist_entry(
				$supplier_id,
				$stock_id,
				isset($row[1]) ? trim($row[1]) : '',
				$currency,
				$prices,
				isset($row[2]) ? trim($row[2]) : '',
				isset($row[3]) ? trim($row[3]) : '',
				isset($row[4]) ? (float)$row[4] : 1,
				isset($row[6]) ? (float)$row[6] : 0,
				isset($row[15]) ? (int)$row[15] : 0,
				isset($row[16]) ? trim($row[16]) : '',
				isset($row[17]) ? trim($row[17]) : '',
				isset($row[18]) ? (float)$row[18] : 0,
				isset($row[19]) ? trim($row[19]) : '',
				isset($row[20]) ? (int)$row[20] : 0,
				isset($row[21]) ? (int)$row[21] : 0,
				array('imported' => 1)
			);
		}

		$imported_count++;
	}

	fclose($stream);

	$hook_data = array(
		'supplier_id' => (int)$supplier_id,
		'imported_count' => (int)$imported_count,
		'errors' => $errors,
	);
	hook_invoke_all('vendor_pricelist_imported', $hook_data);

	return array('imported' => $imported_count, 'errors' => $errors);
}

/**
 * Get historical purchase prices from GRN activity.
 *
 * @param int    $supplier_id
 * @param string $stock_id
 * @param int    $months
 * @return resource
 */
function get_price_history($supplier_id, $stock_id, $months = 24)
{
	$sql = "SELECT grn.delivery_date AS price_date,
		grn_item.qty_recd AS quantity,
		grn_item.unit_price AS unit_price,
		grn.reference,
		grn.id AS grn_batch_id
		FROM " . TB_PREF . "grn_items grn_item
		INNER JOIN " . TB_PREF . "grn_batch grn ON grn.id = grn_item.grn_batch_id
		WHERE grn.supplier_id = " . db_escape($supplier_id) . "
			AND grn_item.item_code = " . db_escape($stock_id) . "
			AND grn.delivery_date >= DATE_SUB(CURDATE(), INTERVAL " . (int)$months . " MONTH)
		ORDER BY grn.delivery_date DESC, grn.id DESC";

	return db_query($sql, 'could not retrieve vendor price history');
}