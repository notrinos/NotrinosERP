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
 * Report 353 — Stock Aging by Bin
 *
 * How long items have been sitting in each bin.
 *
 * Parameters:
 *   PARAM_0: Location
 *   PARAM_1: Item Category
 *   PARAM_2: Minimum Days
 *   PARAM_3: Comments
 *   PARAM_4: Orientation
 *   PARAM_5: Destination (0=PDF, 1=Excel)
 */
$page_security = 'SA_WAREHOUSE_DASHBOARD';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_reports_db.inc');

//----------------------------------------------------------------------------------------------------

print_bin_stock_aging_report();

function print_bin_stock_aging_report()
{
	global $path_to_root;

	$location   = $_POST['PARAM_0'];
	$category   = $_POST['PARAM_1'];
	$min_days   = $_POST['PARAM_2'];
	$comments   = $_POST['PARAM_3'];
	$orientation = $_POST['PARAM_4'];
	$destination = $_POST['PARAM_5'];

	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');
	$dec = user_price_dec();

	if ($category == ALL_NUMERIC) $category = 0;
	if ($location == ALL_TEXT) $location = null;
	if (!$min_days || $min_days < 0) $min_days = 0;

	$loc_name = $location ? get_location_name($location) : _('All');
	$cat_name = $category ? get_category_name($category) : _('All');

	$cols = array(0, 80, 180, 260, 310, 380, 440, 515);
	$headers = array(_('Bin'), _('Item'), _('Batch'), _('Qty'), _('Days Since Move'), _('Expiry'), _('Value'));
	$aligns = array('left', 'left', 'left', 'right', 'right', 'left', 'right');

	$params = array(
		0 => $comments,
		1 => array('text' => _('Location'), 'from' => $loc_name, 'to' => ''),
		2 => array('text' => _('Category'), 'from' => $cat_name, 'to' => ''),
		3 => array('text' => _('Min Days'), 'from' => $min_days, 'to' => ''),
	);

	$rep = new FrontReport(_('Stock Aging by Bin'), 'BinStockAging', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$result = get_bin_stock_aging_data($location, $category, $min_days);

	$grand_total = 0;

	while ($row = db_fetch($result)) {
		$rep->TextCol(0, 1, $row['location_code']);
		$rep->TextCol(1, 2, $row['stock_id'] . ' - ' . $row['item_description']);
		$rep->TextCol(2, 3, $row['batch_no'] ? $row['batch_no'] : '-');
		$rep->AmountCol(3, 4, $row['quantity'], get_qty_dec());
		$rep->AmountCol(4, 5, $row['days_since_movement'], 0);
		$rep->TextCol(5, 6, $row['expiry_date'] ? sql2date($row['expiry_date']) : '-');
		$rep->AmountCol(6, 7, $row['stock_value'], $dec);
		$rep->NewLine();

		$grand_total += $row['stock_value'];
	}

	$rep->Line($rep->row - 2);
	$rep->NewLine();
	$rep->Font('bold');
	$rep->TextCol(0, 5, _('Total'));
	$rep->AmountCol(6, 7, $grand_total, $dec);
	$rep->Font();

	$rep->End();
}
