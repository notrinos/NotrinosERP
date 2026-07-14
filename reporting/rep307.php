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
 * Fetch item metadata and all movement quantities in one conditional aggregate.
 *
 * Opening and closing quantities retain the voided-transaction exclusion used by
 * get_qoh_on_date(), while period movements retain the report's existing behavior.
 *
 * @param int $category Inventory category, or zero for all categories.
 * @param string $location Location code, or an empty string for all locations.
 * @param string $from_date Period start date.
 * @param string $to_date Period end date.
 * @return resource Database result.
 */
function fetch_inventory_movements($category, $location, $from_date, $to_date) {
	$from_sql = date2sql($from_date == null ? Today() : $from_date);
	$to_sql = date2sql($to_date == null ? Today() : $to_date);
	$opening_sql = date2sql(add_days($from_date == null ? Today() : $from_date, -1));

	$sql = "SELECT stock.stock_id, stock.description AS name,
			stock.category_id, stock.units, cat.description,
			units.decimals,
			SUM(CASE WHEN voided.id IS NULL AND move.tran_date <= '$opening_sql'
				THEN move.qty ELSE 0 END) AS opening_qty,
			SUM(CASE WHEN move.tran_date >= '$from_sql' AND move.tran_date <= '$to_sql'
				AND move.qty > 0 THEN move.qty ELSE 0 END) AS inward_qty,
			-SUM(CASE WHEN move.tran_date >= '$from_sql' AND move.tran_date <= '$to_sql'
				AND move.qty < 0 THEN move.qty ELSE 0 END) AS outward_qty,
			SUM(CASE WHEN voided.id IS NULL AND move.tran_date <= '$to_sql'
				THEN move.qty ELSE 0 END) AS closing_qty
		FROM ".TB_PREF."stock_master stock
		LEFT JOIN ".TB_PREF."stock_category cat ON stock.category_id=cat.category_id
		LEFT JOIN ".TB_PREF."item_units units ON units.abbr=stock.units
		LEFT JOIN ".TB_PREF."stock_moves move ON move.stock_id=stock.stock_id";
	if ($location != '')
		$sql .= " AND move.loc_code=".db_escape($location);
	$sql .= " LEFT JOIN ".TB_PREF."voided voided
		ON voided.type=move.type AND voided.id=move.trans_no
		WHERE stock.mb_flag <> 'D' AND stock.mb_flag <> 'F'";
	if ($category != 0)
		$sql .= " AND cat.category_id = ".db_escape($category);
	$sql .= " GROUP BY stock.stock_id, stock.description, stock.category_id,
			stock.units, cat.description, units.decimals
		ORDER BY stock.category_id, stock.stock_id";

	return db_query($sql, 'No transactions were returned');
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
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');

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

	$cols = array(0, 60, 220, 240, 310, 380, 450, 520);

	$headers = array(_('Category'), _('Description'),	_('UOM'),	_('Opening'), _('Quantity In'), _('Quantity Out'), _('Balance'));

	$aligns = array('left',	'left',	'left', 'right', 'right', 'right','right');

	$params =   array( 	0 => $comments,
						1 => array('text' => _('Period'), 'from' => $from_date, 'to' => $to_date),
						2 => array('text' => _('Category'), 'from' => $cat, 'to' => ''),
						3 => array('text' => _('Location'), 'from' => $loc, 'to' => ''));

	$rep = new FrontReport(_('Inventory Movements'), 'InventoryMovements', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);

	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$result = fetch_inventory_movements($category, $location, $from_date, $to_date);

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
		$stock_qty_dec = $myrow['decimals'] == -1 || $myrow['decimals'] === null
			? user_qty_dec() : $myrow['decimals'];
		$rep->AmountCol(3, 4, $myrow['opening_qty'], $stock_qty_dec);
		$rep->AmountCol(4, 5, $myrow['inward_qty'], $stock_qty_dec);
		$rep->AmountCol(5, 6, $myrow['outward_qty'], $stock_qty_dec);
		$rep->AmountCol(6, 7, $myrow['closing_qty'], $stock_qty_dec);
		$rep->NewLine(0, 1);
	}
	$rep->Line($rep->row  - 4);

	$rep->NewLine();
	$rep->End();
}
