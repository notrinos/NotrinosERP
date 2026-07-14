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
$page_security = 'SA_PRICEREP';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/ui/ui_input.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/gl/includes/gl_db.inc');
include_once($path_to_root . '/inventory/includes/inventory_db.inc');

//----------------------------------------------------------------------------------------------------

print_price_listing();

function fetch_items($category=0) {
		$sql = "SELECT item.stock_id, item.description AS name,
				item.material_cost AS Standardcost,
				item.category_id,item.units,
				category.description
			FROM ".TB_PREF."stock_master item,
				".TB_PREF."stock_category category
			WHERE item.category_id=category.category_id AND NOT item.inactive";
		if ($category != 0)
			$sql .= " AND category.category_id = ".db_escape($category);
		$sql .= " AND item.mb_flag<> 'F' ORDER BY item.category_id,
				item.stock_id";

	return db_query($sql, 'No transactions were returned');
}

function get_kits($category=0) {
	$sql = "SELECT i.item_code AS kit_code, i.description AS kit_name, c.category_id AS cat_id, c.description AS cat_name, count(*)>1 AS kit
			FROM
				".TB_PREF."item_codes i
				LEFT JOIN ".TB_PREF."stock_category c ON i.category_id=c.category_id
			WHERE !i.is_foreign AND i.item_code!=i.stock_id";
	if ($category != 0)
		$sql .= " AND c.category_id = ".db_escape($category);
	$sql .= " GROUP BY i.item_code";
	return db_query($sql, 'No kits were returned');
}

/**
 * Load all pricing and sales-kit inputs needed by the price listing.
 *
 * @param string $currency Selected report currency.
 * @param int $sales_type_id Selected sales type ID.
 * @param string $home_currency Company currency.
 * @return array Pricing metadata and maps keyed by their natural IDs.
 */
function load_price_listing_context($currency, $sales_type_id, $home_currency) {
	$context = array(
		'currency_name' => '',
		'sales_types' => array(),
		'prices' => array(),
		'has_price_rows' => array(),
		'costs' => array(),
		'kit_graph' => array(),
		'currency' => $currency,
		'home_currency' => $home_currency,
		'sales_type_id' => $sales_type_id,
		'base_sales_type_id' => get_base_sales_type(),
		'add_pct' => get_company_pref('add_pct'),
		'round_to' => get_company_pref('round_to'),
		'rates' => array()
	);

	$sql = "SELECT curr_abrev, currency FROM ".TB_PREF."currencies
		WHERE curr_abrev=".db_escape($currency);
	$result = db_query($sql, 'Could not load price-listing currency');
	$row = db_fetch($result);
	if ($row)
		$context['currency_name'] = $row['currency'];

	$sql = "SELECT id, sales_type, factor FROM ".TB_PREF."sales_types";
	$result = db_query($sql, 'Could not load sales types for price listing');
	while ($row = db_fetch($result))
		$context['sales_types'][$row['id']] = $row;

	$sql = "SELECT stock_id, sales_type_id, curr_abrev, price
		FROM ".TB_PREF."prices
		WHERE curr_abrev=".db_escape($currency)."
			OR curr_abrev=".db_escape($home_currency);
	$result = db_query($sql, 'Could not load prices for price listing');
	while ($row = db_fetch($result)) {
		$context['prices'][$row['stock_id']][$row['sales_type_id']][$row['curr_abrev']]
			= $row['price'];
		$context['has_price_rows'][$row['stock_id']] = true;
	}

	$sql = "SELECT stock_id, material_cost FROM ".TB_PREF."stock_master";
	$result = db_query($sql, 'Could not load item costs for price listing');
	while ($row = db_fetch($result))
		$context['costs'][$row['stock_id']] = $row['material_cost'];

	$sql = "SELECT DISTINCT kit.item_code AS parent_code,
			kit.stock_id AS component_code, kit.quantity
		FROM ".TB_PREF."item_codes kit
		INNER JOIN ".TB_PREF."item_codes component
			ON component.item_code=kit.stock_id
		ORDER BY kit.id";
	$result = db_query($sql, 'Could not load sales-kit graph');
	while ($row = db_fetch($result))
		$context['kit_graph'][$row['parent_code']][] = array(
			'component_code' => $row['component_code'],
			'quantity' => $row['quantity']
		);

	$date = new_doc_date();
	$context['rates'][$home_currency] = 1.0;
	$context['rates'][$currency] = round2(
		get_exchange_rate_from_home_currency($currency, $date), user_exrate_dec());

	return $context;
}

/**
 * Reproduce standard item pricing from bulk-loaded price and metadata maps.
 *
 * @param string $stock_id Item or kit code.
 * @param string $currency Requested currency.
 * @param array $context Bulk pricing context.
 * @return float Resolved item price.
 */
function get_bulk_price_listing_item_price($stock_id, $currency, $context) {
	$sales_type_id = $context['sales_type_id'];
	$base_id = $context['base_sales_type_id'];
	$home_currency = $context['home_currency'];
	$factor = isset($context['sales_types'][$sales_type_id])
		? $context['sales_types'][$sales_type_id]['factor'] : null;
	$rate = isset($context['rates'][$currency]) ? $context['rates'][$currency] : 1.0;
	$prices = isset($context['prices'][$stock_id]) ? $context['prices'][$stock_id] : array();
	$price = false;

	if (isset($prices[$sales_type_id][$currency]))
		$price = $prices[$sales_type_id][$currency];
	elseif (isset($prices[$base_id][$currency]))
		$price = $prices[$base_id][$currency] * $factor;
	elseif (isset($prices[$sales_type_id][$home_currency]))
		$price = $prices[$sales_type_id][$home_currency] / $rate;
	elseif (isset($prices[$base_id][$home_currency]))
		$price = $prices[$base_id][$home_currency] * $factor / $rate;
	elseif (empty($context['has_price_rows'][$stock_id]) && $context['add_pct'] != -1) {
		$cost = isset($context['costs'][$stock_id]) ? $context['costs'][$stock_id] : 0;
		$price = $cost == 0 ? 0 : round2(
			$cost * (1 + $context['add_pct'] / 100), user_price_dec());
		if ($currency != $home_currency)
			$price /= $rate;
		if ($factor != 0)
			$price *= $factor;
	}

	if ($price === false)
		return 0;
	if ($context['round_to'] != 1)
		return round_to_nearest($price, $context['round_to']);
	return round2($price, user_price_dec());
}

/**
 * Resolve a kit price from the one-time graph preload without database access.
 *
 * Direct kit prices take precedence. Missing direct prices fall back to the
 * recursively accumulated component prices and are memoized per currency/code.
 *
 * @param string $item_code Item or kit code.
 * @param string $currency Requested currency.
 * @param array $context Bulk pricing context.
 * @param array $memo Previously resolved kit prices.
 * @param array $visiting Active recursion path used to guard invalid cycles.
 * @return float Resolved kit price.
 */
function get_bulk_price_listing_kit_price($item_code, $currency, $context,
	&$memo, &$visiting) {
	$key = $currency."\0".$item_code;
	if (isset($memo[$key]))
		return $memo[$key];

	$direct_price = get_bulk_price_listing_item_price($item_code, $currency, $context);
	if ($direct_price != 0) {
		$memo[$key] = $direct_price;
		return $direct_price;
	}
	if (!empty($visiting[$key]))
		return 0;

	$visiting[$key] = true;
	$kit_price = 0.0;
	if (!empty($context['kit_graph'][$item_code])) {
		foreach ($context['kit_graph'][$item_code] as $component) {
			if ($component['component_code'] != $item_code)
				$component_price = get_bulk_price_listing_kit_price(
					$component['component_code'], $currency, $context, $memo, $visiting);
			else
				$component_price = get_bulk_price_listing_item_price(
					$component['component_code'], $currency, $context);
			$kit_price += $component['quantity'] * $component_price;
		}
	}
	unset($visiting[$key]);
	$memo[$key] = $kit_price;
	return $kit_price;
}

//----------------------------------------------------------------------------------------------------

function print_price_listing() {
	global $path_to_root, $SysPrefs;

	$currency = $_POST['PARAM_0'];
	$category = $_POST['PARAM_1'];
	$salestype = $_POST['PARAM_2'];
	$pictures = $_POST['PARAM_3'];
	$showGP = $_POST['PARAM_4'];
	$comments = $_POST['PARAM_5'];
	$orientation = $_POST['PARAM_6'];
	$destination = $_POST['PARAM_7'];
	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');
	$dec = user_price_dec();

	$home_curr = get_company_pref('curr_default');
	if ($currency == ALL_TEXT)
		$currency = $home_curr;
	if ($category == ALL_NUMERIC)
		$category = 0;
	if ($salestype == ALL_NUMERIC)
		$salestype = 0;
	$pricing_context = load_price_listing_context($currency, $salestype, $home_curr);
	$curr_sel = $currency . ' - ' . $pricing_context['currency_name'];
	if ($category == 0)
		$cat = _('All');
	else
		$cat = get_category_name($category);
	if ($salestype == 0)
		$stype = _('All');
	else
		$stype = isset($pricing_context['sales_types'][$salestype])
			? $pricing_context['sales_types'][$salestype]['sales_type'] : '';
	if ($showGP == 0)
		$GP = _('No');
	else
		$GP = _('Yes');

	$cols = array(0, 100, 360, 385, 450, 515);

	$headers = array(_('Category/Items'), _('Description'),	_('UOM'), _('Price'),	_('GP %'));

	$aligns = array('left',	'left',	'left', 'right', 'right');

	$params =   array( 	0 => $comments,
						1 => array('text' => _('Currency'), 'from' => $curr_sel, 'to' => ''),
						2 => array('text' => _('Category'), 'from' => $cat, 'to' => ''),
						3 => array('text' => _('Sales Type'), 'from' => $stype, 'to' => ''),
						4 => array(  'text' => _('Show GP %'),'from' => $GP,'to' => ''));

	if ($pictures)
		$user_comp = user_company();
	else
		$user_comp = '';

	$rep = new FrontReport(_('Price Listing'), 'PriceListing', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);

	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$result = fetch_items($category);
	$kit_price_memo = array();
	$kit_price_visiting = array();

	$catgor = '';
	while ($myrow=db_fetch($result)) {
		if ($catgor != $myrow['description']) {
			$rep->Line($rep->row  - $rep->lineHeight);
			$rep->NewLine(2);
			$rep->fontSize += 2;
			$rep->TextCol(0, 3, $myrow['category_id'] . ' - ' . $myrow['description']);
			$catgor = $myrow['description'];
			$rep->fontSize -= 2;
			$rep->NewLine();
		}
		$rep->NewLine();
		$rep->TextCol(0, 1,	$myrow['stock_id']);
		$rep->TextCol(1, 2, $myrow['name']);
		$rep->TextCol(2, 3, $myrow['units']);
		$price = get_bulk_price_listing_item_price(
			$myrow['stock_id'], $currency, $pricing_context);
		$rep->AmountCol(3, 4, $price, $dec);
		if ($showGP) {
			$price2 = get_bulk_price_listing_item_price(
				$myrow['stock_id'], $home_curr, $pricing_context);
			if ($price2 != 0.0)
				$disp = ($price2 - $myrow['Standardcost']) * 100 / $price2;
			else
				$disp = 0.0;
			$rep->TextCol(4, 5,	number_format2($disp, user_percent_dec()) . ' %');
		}
		if ($pictures) {
			$image = company_path(). '/images/'
				. item_img_name($myrow['stock_id']) . '.jpg';
			if (file_exists($image)) {
				$rep->NewLine();
				if ($rep->row - $SysPrefs->pic_height < $rep->bottomMargin)
					$rep->NewPage();
				$rep->AddImage($image, $rep->cols[1], $rep->row - $SysPrefs->pic_height, 0, $SysPrefs->pic_height);
				$rep->row -= $SysPrefs->pic_height;
				$rep->NewLine();
			}
		}
		else
			$rep->NewLine(0, 1);
	}
	$rep->Line($rep->row  - 4);

	$result = get_kits($category);

	$catgor = '';
	while ($myrow=db_fetch($result)) {
		if ($catgor != $myrow['cat_name']) {
			if ($catgor == '') {
				$rep->NewLine(2);
				$rep->fontSize += 2;
				$rep->TextCol(0, 3, _('Sales Kits'));
				$rep->fontSize -= 2;
			}
			$rep->Line($rep->row  - $rep->lineHeight);
			$rep->NewLine(2);
			$rep->fontSize += 2;
			$rep->TextCol(0, 3, $myrow['cat_id'] . ' - ' . $myrow['cat_name']);
			$catgor = $myrow['cat_name'];
			$rep->fontSize -= 2;
			$rep->NewLine();
		}
		$rep->NewLine();
		$rep->TextCol(0, 1,	$myrow['kit_code']);
		$rep->TextCol(1, 3, $myrow['kit_name']);
		$price = get_bulk_price_listing_kit_price($myrow['kit_code'], $currency,
			$pricing_context, $kit_price_memo, $kit_price_visiting);
		$rep->AmountCol(3, 4, $price, $dec);
		$rep->NewLine(0, 1);
	}
	$rep->Line($rep->row  - 4);
	$rep->NewLine();
	$rep->End();
}
