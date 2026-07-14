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
$page_security = 'SA_ITEMSANALYTIC';
$path_to_root = '..';

include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/gl/includes/gl_db.inc');
include_once($path_to_root.'/inventory/includes/db/items_category_db.inc');
include_once($path_to_root.'/includes/db/manufacturing_db.inc');

//----------------------------------------------------------------------------------------------------

print_inventory_planning();

function getTransactions($category, $location) {
	$sql = "SELECT item.category_id,
			category.description AS cat_description,
			item.stock_id,
			item.description, item.inactive,
			unit.decimals AS qty_dec,
			COALESCE(qoh.loc_code, '') AS loc_code,
			COALESCE(qoh.quantity, 0) AS qty_on_hand
		FROM ".TB_PREF."stock_master item
		INNER JOIN ".TB_PREF."stock_category category
			ON item.category_id=category.category_id
		LEFT JOIN ".TB_PREF."item_units unit ON unit.abbr=item.units
		LEFT JOIN (
			SELECT stock_id, MIN(loc_code) AS loc_code, SUM(qty) AS quantity
			FROM ".TB_PREF."stock_moves";
	if ($location != 'all')
		$sql .= " WHERE loc_code=".db_escape($location);
	$sql .= " GROUP BY stock_id
		) qoh ON qoh.stock_id=item.stock_id";
	if ($location != 'all')
		$sql .= " LEFT JOIN (
			SELECT DISTINCT stock_id FROM ".TB_PREF."stock_moves
		) moved ON moved.stock_id=item.stock_id";
	$sql .= " WHERE 1=1
		AND (item.mb_flag='B' OR item.mb_flag='M')";
	if ($category != 0)
		$sql .= " AND item.category_id = ".db_escape($category);
	if ($location != 'all')
		$sql .= " AND (moved.stock_id IS NULL OR qoh.stock_id IS NOT NULL)";
	$sql .= " ORDER BY item.category_id,
		item.stock_id";

	return db_query($sql, 'No transactions were returned');

}

/**
 * Load five sales periods for every selected item in one aggregate query.
 *
 * @param int $category Inventory category, or zero for all categories.
 * @param string $location Location code, or "all" for all locations.
 * @return array Period quantities keyed by stock ID.
 */
function getPeriods($category, $location) {
	$date5 = date('Y-m-d');
	$date4 = date('Y-m-d',mktime(0,0,0,date('m'),1,date('Y')));
	$date3 = date('Y-m-d',mktime(0,0,0,date('m')-1,1,date('Y')));
	$date2 = date('Y-m-d',mktime(0,0,0,date('m')-2,1,date('Y')));
	$date1 = date('Y-m-d',mktime(0,0,0,date('m')-3,1,date('Y')));
	$date0 = date('Y-m-d',mktime(0,0,0,date('m')-4,1,date('Y')));

	$sql = "SELECT move.stock_id, move.loc_code,
				SUM(CASE WHEN tran_date >= '$date0' AND tran_date < '$date1' THEN -qty ELSE 0 END) AS prd0,
				SUM(CASE WHEN tran_date >= '$date1' AND tran_date < '$date2' THEN -qty ELSE 0 END) AS prd1,
				SUM(CASE WHEN tran_date >= '$date2' AND tran_date < '$date3' THEN -qty ELSE 0 END) AS prd2,
				SUM(CASE WHEN tran_date >= '$date3' AND tran_date < '$date4' THEN -qty ELSE 0 END) AS prd3,
				SUM(CASE WHEN tran_date >= '$date4' AND tran_date <= '$date5' THEN -qty ELSE 0 END) AS prd4
			FROM ".TB_PREF."stock_moves move
			INNER JOIN ".TB_PREF."stock_master item ON item.stock_id=move.stock_id
			WHERE move.tran_date >= '$date0' AND move.tran_date <= '$date5'
			AND (move.type=13 OR move.type=11)
			AND (item.mb_flag='B' OR item.mb_flag='M')";
	if ($category != 0)
		$sql .= " AND item.category_id=".db_escape($category);
	if ($location != 'all')
		$sql .= " AND move.loc_code=".db_escape($location);
	$sql .= " GROUP BY move.stock_id, move.loc_code";

	$result = db_query($sql, 'No transactions were returned');
	$periods = array();
	while ($row = db_fetch($result))
		$periods[$row['stock_id']][$row['loc_code']] = $row;
	return $periods;
}

//----------------------------------------------------------------------------------------------------

function print_inventory_planning() {
	global $path_to_root, $tmonths;

	$category = $_POST['PARAM_0'];
	$location = $_POST['PARAM_1'];
	$comments = $_POST['PARAM_2'];
	$orientation = $_POST['PARAM_3'];
	$destination = $_POST['PARAM_4'];
	if ($destination)
		include_once($path_to_root.'/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root.'/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');
	if ($category == ALL_NUMERIC)
		$category = 0;
	if ($category == 0)
		$cat = _('All');
	else
		$cat = get_category_name($category);

	if ($location == ALL_TEXT)
		$location = 'all';
	if ($location == 'all')
		$loc = _('All');
	else
		$loc = get_location_name($location);

	$cols = array(0, 50, 150, 180, 210, 240, 270, 300, 330, 390, 435, 480, 525);

	$per0 = $tmonths[date('n',mktime(0,0,0,date('m'),1,date('Y')))];
	$per1 = $tmonths[date('n',mktime(0,0,0,date('m')-1,1,date('Y')))];
	$per2 = $tmonths[date('n',mktime(0,0,0,date('m')-2,1,date('Y')))];
	$per3 = $tmonths[date('n',mktime(0,0,0,date('m')-3,1,date('Y')))];
	$per4 = $tmonths[date('n',mktime(0,0,0,date('m')-4,1,date('Y')))];
	
	$headers = array(_('Category'), '', $per4, $per3, $per2, $per1, $per0, _('3*M'), _('QOH'), _('Cust Ord'), _('Supp Ord'), _('Sugg Ord'));

	$aligns = array('left',	'left',	'right', 'right', 'right', 'right', 'right', 'right', 'right', 'right', 'right', 'right');

	$params =   array( 	0 => $comments,
						1 => array('text' => _('Category'), 'from' => $cat, 'to' => ''),
						2 => array('text' => _('Location'), 'from' => $loc, 'to' => ''));

	$rep = new FrontReport(_('Inventory Planning Report'), 'InventoryPlanning', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);

	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$res = getTransactions($category, $location);
	$availability = get_inventory_availability_maps($category, $location);
	$periods = getPeriods($category, $location);
	$catt = '';
	while ($trans=db_fetch($res)) {
		if ($catt != $trans['cat_description']) {
			if ($catt != '') {
				$rep->Line($rep->row - 2);
				$rep->NewLine(2, 3);
			}
			$rep->TextCol(0, 1, $trans['category_id']);
			$rep->TextCol(1, 2, $trans['cat_description']);
			$catt = $trans['cat_description'];
			$rep->NewLine();
		}
		$stock_id = $trans['stock_id'];
		$custqty = isset($availability['direct_demand'][$stock_id])
			? $availability['direct_demand'][$stock_id] : 0;
		$custqty += isset($availability['assembly_demand'][$stock_id])
			? $availability['assembly_demand'][$stock_id] : 0;
		$suppqty = isset($availability['purchase_order'][$stock_id])
			? $availability['purchase_order'][$stock_id] : 0;
		$suppqty += isset($availability['work_order'][$stock_id])
			? $availability['work_order'][$stock_id] : 0;
		$period = isset($periods[$stock_id][$trans['loc_code']])
			? $periods[$stock_id][$trans['loc_code']]
			: array('prd0' => 0, 'prd1' => 0, 'prd2' => 0, 'prd3' => 0, 'prd4' => 0);
		$rep->NewLine();
		$dec = $trans['qty_dec'] === null || $trans['qty_dec'] == -1
			? user_qty_dec() : $trans['qty_dec'];
		$rep->TextCol(0, 1, $trans['stock_id']);
		$rep->TextCol(1, 2, $trans['description'].($trans['inactive']==1 ? ' ('._('Inactive').')' : ''), -1);
		$rep->AmountCol(2, 3, $period['prd0'], $dec);
		$rep->AmountCol(3, 4, $period['prd1'], $dec);
		$rep->AmountCol(4, 5, $period['prd2'], $dec);
		$rep->AmountCol(5, 6, $period['prd3'], $dec);
		$rep->AmountCol(6, 7, $period['prd4'], $dec);
		
		$MaxMthSales = Max($period['prd0'], $period['prd1'], $period['prd2'], $period['prd3']);
		$IdealStockHolding = $MaxMthSales * 3;
		$rep->AmountCol(7, 8, $IdealStockHolding, $dec);

		$rep->AmountCol(8, 9, $trans['qty_on_hand'], $dec);
		$rep->AmountCol(9, 10, $custqty, $dec);
		$rep->AmountCol(10, 11, $suppqty, $dec);

		$SuggestedTopUpOrder = $IdealStockHolding - $trans['qty_on_hand'] + $custqty - $suppqty;
		if ($SuggestedTopUpOrder < 0.0)
			$SuggestedTopUpOrder = 0.0;
		$rep->AmountCol(11, 12, $SuggestedTopUpOrder, $dec);
	}
	$rep->Line($rep->row - 4);
	$rep->NewLine();
	$rep->End();
}
