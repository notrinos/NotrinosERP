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
 * Report 351 — Stock by Location (Hierarchical)
 *
 * Hierarchical stock summary: Warehouse → Zone → Bin → Item.
 *
 * Parameters:
 *   PARAM_0: Location
 *   PARAM_1: Item Category
 *   PARAM_2: Comments
 *   PARAM_3: Orientation
 *   PARAM_4: Destination (0=PDF, 1=Excel)
 */
$page_security = 'SA_WAREHOUSE_DASHBOARD';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_reports_db.inc');

//----------------------------------------------------------------------------------------------------

print_stock_by_location_report();

function print_stock_by_location_report()
{
	global $path_to_root;

	$location  = $_POST['PARAM_0'];
	$category  = $_POST['PARAM_1'];
	$comments  = $_POST['PARAM_2'];
	$orientation = $_POST['PARAM_3'];
	$destination = $_POST['PARAM_4'];

	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');
	$dec = user_price_dec();

	if ($category == ALL_NUMERIC) $category = 0;
	if ($location == ALL_TEXT) $location = null;

	$loc_name = $location ? get_location_name($location) : _('All');
	$cat_name = $category ? get_category_name($category) : _('All');

	$cols = array(0, 100, 200, 280, 340, 400, 460, 515);
	$headers = array(_('Location'), _('Type'), _('Item'), _('Qty'), _('Reserved'), _('Available'), _('Value'));
	$aligns = array('left', 'left', 'left', 'right', 'right', 'right', 'right');

	$params = array(
		0 => $comments,
		1 => array('text' => _('Warehouse'), 'from' => $loc_name, 'to' => ''),
		2 => array('text' => _('Category'), 'from' => $cat_name, 'to' => ''),
	);

	$rep = new FrontReport(_('Stock by Location'), 'StockByLocation', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$result = get_stock_by_location_data($location, $category);

	$grand_total = 0;
	$current_loc = '';

	while ($row = db_fetch($result)) {
		if ($current_loc != $row['location_code']) {
			if ($current_loc != '')
				$rep->NewLine();
			$current_loc = $row['location_code'];
		}

		$indent = '';
		$level = $row['hierarchy_level'];
		if ($level > 0)
			$indent = str_repeat('  ', $level);

		$rep->TextCol(0, 1, $indent . $row['location_code']);
		$rep->TextCol(1, 2, $row['location_type']);
		$rep->TextCol(2, 3, $row['stock_id'] ? $row['stock_id'] : '-');
		if ($row['total_qty'] !== null) {
			$rep->AmountCol(3, 4, $row['total_qty'], get_qty_dec());
			$rep->AmountCol(4, 5, $row['total_reserved'], get_qty_dec());
			$rep->AmountCol(5, 6, $row['total_available'], get_qty_dec());
			$rep->AmountCol(6, 7, $row['total_value'], $dec);
			$grand_total += $row['total_value'];
		}
		$rep->NewLine();
	}

	$rep->Line($rep->row - 2);
	$rep->NewLine();
	$rep->Font('bold');
	$rep->TextCol(0, 5, _('Grand Total'));
	$rep->AmountCol(6, 7, $grand_total, $dec);
	$rep->Font();

	$rep->End();
}
