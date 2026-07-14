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
$page_security = 'SA_ITEMSVALREP';
$path_to_root='..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/ui/ui_input.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/gl/includes/gl_db.inc');
include_once($path_to_root . '/inventory/includes/inventory_db.inc');

//----------------------------------------------------------------------------------------------------

inventory_movements();

/**
 * Resolve a movement price in domestic currency without additional queries.
 *
 * @param array $movement Bulk-loaded stock movement row.
 * @return float Domestic movement price.
 */
function get_bulk_domestic_price($movement) {
	if ($movement['type'] == ST_SUPPRECEIVE || $movement['type'] == ST_SUPPCREDIT) {
		$price = $movement['price'];
		if ($movement['person_id'] > 0)
			$price *= $movement['ex_rate'];
		return $price;
	}

	return $movement['standard_cost'];
}

/**
 * Fetch report item metadata and unit precision in one query.
 *
 * @param int $category Inventory category, or zero for all categories.
 * @return resource Database result.
 */
function fetch_items($category=0) {
	$sql = "SELECT stock.stock_id, stock.description AS name,
			stock.category_id, stock.units, cat.description,
			units.decimals
		FROM ".TB_PREF."stock_master stock
		LEFT JOIN ".TB_PREF."stock_category cat ON stock.category_id=cat.category_id
		LEFT JOIN ".TB_PREF."item_units units ON units.abbr=stock.units
		WHERE stock.mb_flag <> 'D' AND stock.mb_flag <> 'F'";
	if ($category != 0)
		$sql .= " AND cat.category_id = ".db_escape($category);
	$sql .= " ORDER BY stock.category_id, stock.stock_id";

	return db_query($sql, 'No transactions were returned');
}

/**
 * Load the ordered movement history once and calculate every report metric by item.
 *
 * @param int $category Inventory category, or zero for all categories.
 * @param string $location Location code, or an empty string for all locations.
 * @param string $from_date Period start date.
 * @param string $to_date Period end date.
 * @return array Quantity and cost accumulators keyed by stock ID.
 */
function get_costed_movement_metrics($category, $location, $from_date, $to_date) {
	$from_sql = date2sql($from_date == null ? Today() : $from_date);
	$to_sql = date2sql($to_date == null ? Today() : $to_date);
	$sql = "SELECT move.stock_id, move.tran_date, move.trans_id, move.type,
			move.qty, move.price, move.standard_cost,
			supplier.supplier_id AS person_id,
			IF(ISNULL(grn.rate), credit.rate, grn.rate) AS ex_rate,
			voided.id AS voided_id
		FROM ".TB_PREF."stock_moves move
		INNER JOIN ".TB_PREF."stock_master stock ON stock.stock_id=move.stock_id
		LEFT JOIN ".TB_PREF."supp_trans credit
			ON credit.trans_no=move.trans_no AND credit.type=move.type
		LEFT JOIN ".TB_PREF."grn_batch grn
			ON grn.id=move.trans_no AND move.type=".ST_SUPPRECEIVE."
		LEFT JOIN ".TB_PREF."suppliers supplier
			ON IFNULL(grn.supplier_id, credit.supplier_id)=supplier.supplier_id
		LEFT JOIN ".TB_PREF."voided voided
			ON voided.type=move.type AND voided.id=move.trans_no
		WHERE move.tran_date <= '$to_sql'
			AND stock.mb_flag <> 'D' AND stock.mb_flag <> 'F'";
	if ($category != 0)
		$sql .= " AND stock.category_id=".db_escape($category);
	if ($location != '')
		$sql .= " AND move.loc_code=".db_escape($location);
	$sql .= " ORDER BY move.stock_id, move.tran_date, move.trans_id";

	$result = db_query($sql, 'No standard cost transactions were returned');
	$metrics = array();
	while ($movement = db_fetch($result)) {
		$stock_id = $movement['stock_id'];
		if (!isset($metrics[$stock_id])) {
			$metrics[$stock_id] = array(
				'opening_qty' => 0, 'inward_qty' => 0, 'outward_qty' => 0, 'closing_qty' => 0,
				'opening_cost_qty' => 0, 'opening_cost_total' => 0,
				'inward_cost_qty' => 0, 'inward_cost_total' => 0,
				'outward_cost_qty' => 0, 'outward_cost_total' => 0,
				'closing_cost_qty' => 0, 'closing_cost_total' => 0,
			);
		}

		$qty = $movement['qty'];
		$is_before_period = $movement['tran_date'] < $from_sql;
		if ($movement['voided_id'] === null) {
			if ($is_before_period)
				$metrics[$stock_id]['opening_qty'] += $qty;
			$metrics[$stock_id]['closing_qty'] += $qty;
		}

		if ($qty == 0 || $movement['type'] == ST_LOCTRANSFER)
			continue;

		$movement_cost = $qty * get_bulk_domestic_price($movement);
		$metrics[$stock_id]['closing_cost_qty'] += $qty;
		$metrics[$stock_id]['closing_cost_total'] += $movement_cost;
		if ($is_before_period) {
			$metrics[$stock_id]['opening_cost_qty'] += $qty;
			$metrics[$stock_id]['opening_cost_total'] += $movement_cost;
		}
		elseif ($qty > 0) {
			$metrics[$stock_id]['inward_qty'] += $qty;
			$metrics[$stock_id]['inward_cost_qty'] += $qty;
			$metrics[$stock_id]['inward_cost_total'] += $movement_cost;
		}
		else {
			$metrics[$stock_id]['outward_qty'] -= $qty;
			$metrics[$stock_id]['outward_cost_qty'] += $qty;
			$metrics[$stock_id]['outward_cost_total'] += $movement_cost;
		}
	}

	return $metrics;
}

/**
 * Calculate an average from quantity and total-cost accumulators.
 *
 * @param float $quantity Accumulated signed quantity.
 * @param float $total_cost Accumulated signed cost.
 * @return float Average unit cost, or zero for a zero quantity.
 */
function get_accumulated_average_cost($quantity, $total_cost) {
	return $quantity == 0 ? 0 : $total_cost / $quantity;
}

//----------------------------------------------------------------------------------------------------

function inventory_movements() {
	global $path_to_root;

	$from_date = $_POST['PARAM_0'];
	$to_date = $_POST['PARAM_1'];
	$category = $_POST['PARAM_2'];
	$location = $_POST['PARAM_3'];
	$comments = $_POST['PARAM_4'];
	$orientation = $_POST['PARAM_5'];
	$destination = $_POST['PARAM_6'];
	if ($destination)
		include_once($path_to_root . "/reporting/includes/excel_report.inc");
	else
		include_once($path_to_root . "/reporting/includes/pdf_report.inc");

	$orientation = ($orientation ? 'L' : 'P');
	if ($category == ALL_NUMERIC)
		$category = 0;
	if ($category == 0)
		$cat = _('All');
	else
		$cat = get_category_name($category);

	if ($location == '')
		$loc = _('All');
	else
		$loc = get_location_name($location);

	$cols = array(0, 60, 134, 160, 185, 215, 250, 275, 305, 340, 365, 395, 430, 455, 485, 520);

	$headers = array(_('Category'), _('Description'),	_('UOM'), '', '', _('OpeningStock'), '', '',_('StockIn'), '', '', _('Delivery'), '', '', _('ClosingStock'));
	$headers2 = array("", "", "", _("QTY"), _("Rate"), _("Value"), _("QTY"), _("Rate"), _("Value"), _("QTY"), _("Rate"), _("Value"), _("QTY"), _("Rate"), _("Value"));

	$aligns = array('left',	'left',	'left', 'right', 'right', 'right', 'right','right' ,'right', 'right', 'right','right', 'right', 'right', 'right');

	$params =   array( 	0 => $comments,
						1 => array('text' => _('Period'), 'from' => $from_date, 'to' => $to_date),
						2 => array('text' => _('Category'), 'from' => $cat, 'to' => ''),
						3 => array('text' => _('Location'), 'from' => $loc, 'to' => ''));

	$rep = new FrontReport(_('Costed Inventory Movements'), "CostedInventoryMovements", user_pagesize(), 8, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);

	$rep->Font();
	$rep->Info($params, $cols, $headers2, $aligns, $cols, $headers, $aligns);
	$rep->NewPage();

	$totval_open = $totval_in = $totval_out = $totval_close = 0; 
	$result = fetch_items($category);
	$movement_metrics = get_costed_movement_metrics($category, $location, $from_date, $to_date);

	$dec = user_price_dec();
	$catgor = '';
	while ($myrow=db_fetch($result)) {
		if ($catgor != $myrow['description']) {
			$rep->NewLine(2);
			$rep->fontSize += 2;
			$rep->TextCol(0, 3, $myrow['category_id'] . " - " . $myrow['description']);
			$catgor = $myrow['description'];
			$rep->fontSize -= 2;
			$rep->NewLine();
		}
		$metrics = isset($movement_metrics[$myrow['stock_id']]) ? $movement_metrics[$myrow['stock_id']] : array(
			'opening_qty' => 0, 'inward_qty' => 0, 'outward_qty' => 0, 'closing_qty' => 0,
			'opening_cost_qty' => 0, 'opening_cost_total' => 0,
			'inward_cost_qty' => 0, 'inward_cost_total' => 0,
			'outward_cost_qty' => 0, 'outward_cost_total' => 0,
			'closing_cost_qty' => 0, 'closing_cost_total' => 0,
		);
		$qoh_start = $metrics['opening_qty'];
		$qoh_end = $metrics['closing_qty'];
		$inward = $metrics['inward_qty'];
		$outward = $metrics['outward_qty'];
		$openCost = get_accumulated_average_cost($metrics['opening_cost_qty'], $metrics['opening_cost_total']);
		$unitCost = get_accumulated_average_cost($metrics['closing_cost_qty'], $metrics['closing_cost_total']);
		if ($qoh_start == 0 && $inward == 0 && $outward == 0 && $qoh_end == 0)
			continue;
		$qty_dec = $myrow['decimals'] == -1 || $myrow['decimals'] === null
			? user_qty_dec() : $myrow['decimals'];
		$rep->NewLine();
		$rep->TextCol(0, 1,	$myrow['stock_id']);
		$rep->TextCol(1, 2, substr($myrow['name'], 0, 24) . ' ');
		$rep->TextCol(2, 3, $myrow['units']);
		$rep->AmountCol(3, 4, $qoh_start, $qty_dec);
		$rep->AmountCol(4, 5, $openCost, $dec);
		$openCost *= $qoh_start;
		$totval_open += $openCost;
		$rep->AmountCol(5, 6, $openCost);
		
		if($inward>0){
			$rep->AmountCol(6, 7, $inward, $qty_dec);
			$unitCost_in = get_accumulated_average_cost($metrics['inward_cost_qty'], $metrics['inward_cost_total']);
			$rep->AmountCol(7, 8, $unitCost_in,$dec);
			$unitCost_in *= $inward;
			$totval_in += $unitCost_in;
			$rep->AmountCol(8, 9, $unitCost_in);
		}
		
		if($outward>0){
			$rep->AmountCol(9, 10, $outward, $qty_dec);
			$unitCost_out = get_accumulated_average_cost($metrics['outward_cost_qty'], $metrics['outward_cost_total']);
			$rep->AmountCol(10, 11, $unitCost_out,$dec);
			$unitCost_out *= $outward;
			$totval_out += $unitCost_out;
			$rep->AmountCol(11, 12, $unitCost_out);
		}
		
		$rep->AmountCol(12, 13, $qoh_end, $qty_dec);
		$rep->AmountCol(13, 14, $unitCost,$dec);
		$unitCost *= $qoh_end;
		$totval_close += $unitCost;
		$rep->AmountCol(14, 15, $unitCost);
		
		$rep->NewLine(0, 1);
	}
	$rep->Line($rep->row  - 4);
	$rep->NewLine(2);
	$rep->TextCol(0, 1,	_("Total Movement"));
	$rep->AmountCol(5, 6, $totval_open);
	$rep->AmountCol(8, 9, $totval_in);
	$rep->AmountCol(11, 12, $totval_out);
	$rep->AmountCol(14, 15, $totval_open + $totval_in - $totval_out);
	$rep->NewLine(1);
	$rep->TextCol(0, 1,	_("Total Out"));
	$rep->AmountCol(14, 15, $totval_close);
	$rep->Line($rep->row  - 4);

	$rep->End();
}
