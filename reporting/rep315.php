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
 * Report 315 — Batch Aging Analysis
 *
 * Age distribution of batch stock (days in stock, days to expiry).
 *
 * Parameters:
 *   PARAM_0: As at Date
 *   PARAM_1: Item
 *   PARAM_2: Location
 *   PARAM_3: Item Category
 *   PARAM_4: Comments
 *   PARAM_5: Orientation
 *   PARAM_6: Destination (0=PDF, 1=Excel)
 */
$page_security = 'SA_BATCHINQUIRY';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/includes/db/inventory_reports_db.inc');

//----------------------------------------------------------------------------------------------------

print_batch_aging_report();

function print_batch_aging_report()
{
	global $path_to_root;

	$as_at_date = $_POST['PARAM_0'];
	$stock_id   = $_POST['PARAM_1'];
	$location   = $_POST['PARAM_2'];
	$category   = $_POST['PARAM_3'];
	$comments   = $_POST['PARAM_4'];
	$orientation = $_POST['PARAM_5'];
	$destination = $_POST['PARAM_6'];

	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');
	$dec = user_price_dec();

	if ($category == ALL_NUMERIC) $category = 0;
	if ($stock_id == ALL_TEXT) $stock_id = null;
	if ($location == ALL_TEXT) $location = null;

	$item_name = $stock_id ? $stock_id : _('All');
	$cat_name = $category ? get_category_name($category) : _('All');
	$loc_name = $location ? get_location_name($location) : _('All');

	$cols = array(0, 80, 180, 250, 310, 370, 430, 515);
	$headers = array(_('Batch #'), _('Item'), _('Location'), _('Qty'), _('Days In Stock'), _('Days to Expiry'), _('Value'));
	$aligns = array('left', 'left', 'left', 'right', 'right', 'right', 'right');

	$params = array(
		0 => $comments,
		1 => array('text' => _('As at Date'), 'from' => $as_at_date, 'to' => ''),
		2 => array('text' => _('Item'), 'from' => $item_name, 'to' => ''),
		3 => array('text' => _('Location'), 'from' => $loc_name, 'to' => ''),
		4 => array('text' => _('Category'), 'from' => $cat_name, 'to' => ''),
	);

	$rep = new FrontReport(_('Batch Aging Analysis'), 'BatchAging', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$result = get_batch_aging_data($stock_id, $location, $category, $as_at_date);

	$grand_total = 0;

	while ($row = db_fetch($result)) {
		$rep->TextCol(0, 1, $row['batch_no']);
		$rep->TextCol(1, 2, $row['stock_id'] . ' - ' . $row['item_description']);
		$rep->TextCol(2, 3, $row['location_name']);
		$rep->AmountCol(3, 4, $row['quantity_on_hand'], get_qty_dec());
		$rep->AmountCol(4, 5, $row['days_in_stock'], 0);
		$rep->TextCol(5, 6, $row['days_until_expiry'] !== null ? number_format2($row['days_until_expiry'], 0) : _('N/A'));
		$rep->AmountCol(6, 7, $row['stock_value'], $dec);
		$rep->NewLine();

		$grand_total += $row['stock_value'];
	}

	$rep->Line($rep->row - 2);
	$rep->NewLine();
	$rep->Font('bold');
	$rep->TextCol(0, 3, _('Total'));
	$rep->AmountCol(6, 7, $grand_total, $dec);
	$rep->Font();

	$rep->End();
}
