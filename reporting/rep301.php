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
$path_to_root = '..';

include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/gl/includes/gl_db.inc');
include_once($path_to_root.'/inventory/includes/db/items_category_db.inc');

//----------------------------------------------------------------------------------------------------

print_inventory_valuation_report();

/**
 * Load average movement costs for every item selected by the report.
 *
 * @param int $category Inventory category, or zero for all categories.
 * @param string $location Location code, or 'all' for all locations.
 * @param string|null $to_date Report end date.
 * @return array Average costs keyed by stock ID.
 */
function get_inventory_average_costs($category, $location, $to_date) {
	if ($to_date == null)
		$to_date = Today();

	$to_date = date2sql($to_date);
	$sql = "SELECT move.stock_id,
			SUM(move.qty) AS total_qty,
			SUM(move.qty * IFNULL(CASE
				WHEN move.type IN (".ST_SUPPRECEIVE.", ".ST_SUPPCREDIT.") THEN
					move.price * IF(supplier.supplier_id > 0,
						IF(ISNULL(grn.rate), credit.rate, grn.rate), 1)
				ELSE move.standard_cost
			END, 0)) AS total_cost
		FROM ".TB_PREF."stock_moves move
		INNER JOIN ".TB_PREF."stock_master item ON item.stock_id=move.stock_id
		LEFT JOIN ".TB_PREF."supp_trans credit
			ON credit.trans_no=move.trans_no AND credit.type=move.type
		LEFT JOIN ".TB_PREF."grn_batch grn
			ON grn.id=move.trans_no AND move.type=".ST_SUPPRECEIVE."
		LEFT JOIN ".TB_PREF."suppliers supplier
			ON IFNULL(grn.supplier_id, credit.supplier_id)=supplier.supplier_id
		WHERE move.tran_date <= '$to_date'
			AND move.standard_cost > 0.001
			AND move.qty <> 0
			AND move.type <> ".ST_LOCTRANSFER."
			AND item.mb_flag <> 'D' AND item.mb_flag <> 'F'";

	if ($category != 0)
		$sql .= " AND item.category_id = ".db_escape($category);
	if ($location != 'all')
		$sql .= " AND move.loc_code = ".db_escape($location);

	$sql .= " GROUP BY move.stock_id";
	$result = db_query($sql, 'No standard cost transactions were returned');
	$costs = array();
	while ($row = db_fetch($result))
		$costs[$row['stock_id']] = $row['total_qty'] == 0 ? 0 : $row['total_cost'] / $row['total_qty'];

	return $costs;
}

/**
 * Fetch the inventory valuation rows and unit precision in one query.
 *
 * @param int $category Inventory category, or zero for all categories.
 * @param string $location Location code, or 'all' for all locations.
 * @param string $date Report end date.
 * @return resource Database result.
 */
function getTransactions($category, $location, $date) {

	$dec = user_qty_dec();
	$sql = "SELECT item.category_id,
			category.description AS cat_description,
			item.stock_id,
			item.units,
			item.description, item.inactive,
			move.loc_code,
			units.decimals,
			SUM(move.qty) AS QtyOnHand, 
			item.material_cost AS UnitCost,
			SUM(move.qty) * item.material_cost AS ItemTotal 
			FROM "
			.TB_PREF."stock_master item,"
			.TB_PREF."stock_category category,"
			.TB_PREF."stock_moves move,"
			.TB_PREF."item_units units
		WHERE item.stock_id=move.stock_id
		AND item.category_id=category.category_id
		AND item.mb_flag<>'D' AND mb_flag <> 'F' 
		AND move.tran_date <= '".date2sql($date)."'
		AND item.units = units.abbr";
		if ($category != 0)
			$sql .= " AND item.category_id = ".db_escape($category);
		if ($location != 'all')
			$sql .= " AND move.loc_code = ".db_escape($location);
		$sql .= "
		GROUP BY item.category_id,
			category.description, ";
		if ($location != 'all')
			$sql .= "move.loc_code, ";
		$sql .= "item.stock_id,
			item.units,
			item.description,
			item.inactive,
			item.material_cost,
			units.decimals
		HAVING ROUND(SUM(move.qty), IF(units.decimals <> -1, units.decimals, $dec)) != 0";
		$sql .= " ORDER BY item.category_id, item.stock_id";

	return db_query($sql, 'No transactions were returned');
}

//----------------------------------------------------------------------------------------------------

function print_inventory_valuation_report() {
	global $path_to_root, $SysPrefs;

	$date = $_POST['PARAM_0'];
	$category = $_POST['PARAM_1'];
	$location = $_POST['PARAM_2'];
	$detail = $_POST['PARAM_3'];
	$comments = $_POST['PARAM_4'];
	$orientation = $_POST['PARAM_5'];
	$destination = $_POST['PARAM_6'];
	if ($destination)
		include_once($path_to_root.'/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root.'/reporting/includes/pdf_report.inc');
	$detail = !$detail;
	$dec = user_price_dec();

	$orientation = ($orientation ? 'L' : 'P');
	if ($category == ALL_NUMERIC)
		$category = 0;
	$cat = $category == 0 ? _('All') : get_category_name($category);

	if ($location == ALL_TEXT)
		$location = 'all';
	$loc = $location == 'all' ? _('All') : get_location_name($location);

	$cols = array(0, 75, 225, 250, 350, 450,	515);

	$headers = array(_('Category'), '', _('UOM'), _('Quantity'), _('Unit Cost'), _('Value'));

	$aligns = array('left',	'left',	'left', 'right', 'right', 'right');

	$params = array(0 => $comments,
					1 => array('text' => _('End Date'), 'from' => $date, 'to' => ''),
					2 => array('text' => _('Category'), 'from' => $cat, 'to' => ''),
					3 => array('text' => _('Location'), 'from' => $loc, 'to' => ''));

	$rep = new FrontReport(_('Inventory Valuation Report'), 'InventoryValReport', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$res = getTransactions($category, $location, $date);
	$average_costs = array();
	if (isset($SysPrefs->use_costed_values) && $SysPrefs->use_costed_values == 1)
		$average_costs = get_inventory_average_costs($category, $location, $date);
	$total = $grandtotal = 0.0;
	$catt = '';
	while ($trans=db_fetch($res)) {
		if ($catt != $trans['cat_description']) {
			if ($catt != '') {
				if ($detail) {
					$rep->NewLine(2, 3);
					$rep->TextCol(0, 4, _('Total'));
				}
				$rep->AmountCol(5, 6, $total, $dec);
				if ($detail) {
					$rep->Line($rep->row - 2);
					$rep->NewLine();
				}
				$rep->NewLine();
				$total = 0.0;
			}
			$rep->TextCol(0, 1, $trans['category_id']);
			$rep->TextCol(1, 2, $trans['cat_description']);
			$catt = $trans['cat_description'];
			if ($detail)
				$rep->NewLine();
		}
		if (isset($SysPrefs->use_costed_values) && $SysPrefs->use_costed_values == 1) {
			$UnitCost = isset($average_costs[$trans['stock_id']]) ? $average_costs[$trans['stock_id']] : 0;
			$ItemTotal = $trans['QtyOnHand'] * $UnitCost;
		}	
		else {
			$UnitCost = $trans['UnitCost'];
			$ItemTotal = $trans['ItemTotal'];
		}	
		if ($detail) {
			$rep->NewLine();
			$rep->fontSize -= 2;
			$rep->TextCol(0, 1, $trans['stock_id']);
			$rep->TextCol(1, 2, $trans['description'].($trans['inactive'] == 1 ? ' ('._('Inactive').')' : ''), -1);
			$rep->TextCol(2, 3, $trans['units']);
			$qty_dec = $trans['decimals'] == -1 ? user_qty_dec() : $trans['decimals'];
			$rep->AmountCol(3, 4, $trans['QtyOnHand'], $qty_dec);
			$dec2 = 0;
			price_decimal_format($UnitCost, $dec2);
			$rep->AmountCol(4, 5, $UnitCost, $dec2);
			$rep->AmountCol(5, 6, $ItemTotal, $dec);
			$rep->fontSize += 2;
		}
		$total += $ItemTotal;
		$grandtotal += $ItemTotal;
	}
	if ($detail) {
		$rep->NewLine(2, 3);
		$rep->TextCol(0, 4, _('Total'));
	}
	$rep->Amountcol(5, 6, $total, $dec);
	if ($detail) {
		$rep->Line($rep->row - 2);
		$rep->NewLine();
	}
	$rep->NewLine(2, 1);
	$rep->TextCol(0, 4, _('Grand Total'));
	$rep->AmountCol(5, 6, $grandtotal, $dec);
	$rep->Line($rep->row  - 4);
	$rep->NewLine();
	$rep->End();
}
