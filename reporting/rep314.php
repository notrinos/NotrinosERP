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
 * Report 314 — Batch-wise Stock Balance
 *
 * Stock balance by batch/lot per location with value.
 *
 * Parameters:
 *   PARAM_0: Item Category
 *   PARAM_1: Location
 *   PARAM_2: Item
 *   PARAM_3: Comments
 *   PARAM_4: Orientation
 *   PARAM_5: Destination (0=PDF, 1=Excel)
 */
$page_security = 'SA_BATCHINQUIRY';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/includes/db/inventory_reports_db.inc');

//----------------------------------------------------------------------------------------------------

print_batch_balance_report();

function print_batch_balance_report()
{
	global $path_to_root;

	$stock_id  = $_POST['PARAM_0'];
	$location  = $_POST['PARAM_1'];
	$category  = $_POST['PARAM_2'];
	$comments  = $_POST['PARAM_3'];
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
	if ($stock_id == ALL_TEXT) $stock_id = null;

	$cat_name = $category ? get_category_name($category) : _('All');
	$loc_name = $location ? get_location_name($location) : _('All');
	$item_name = $stock_id ? $stock_id : _('All');

	$cols = array(0, 80, 180, 240, 310, 370, 430, 515);
	$headers = array(_('Batch #'), _('Item'), _('Location'), _('Qty'), _('Expiry'), _('Status'), _('Value'));
	$aligns = array('left', 'left', 'left', 'right', 'left', 'left', 'right');

	$params = array(
		0 => $comments,
		1 => array('text' => _('Category'), 'from' => $cat_name, 'to' => ''),
		2 => array('text' => _('Location'), 'from' => $loc_name, 'to' => ''),
		3 => array('text' => _('Item'), 'from' => $item_name, 'to' => ''),
	);

	$rep = new FrontReport(_('Batch-wise Stock Balance'), 'BatchBalance', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$result = get_batch_balance_data($stock_id, $location, $category);

	$current_item = '';
	$item_total = 0;
	$grand_total = 0;

	while ($row = db_fetch($result)) {
		if ($current_item != $row['stock_id']) {
			if ($current_item != '') {
				$rep->NewLine();
				$rep->Font('bold');
				$rep->TextCol(0, 3, _('Subtotal'));
				$rep->AmountCol(6, 7, $item_total, $dec);
				$rep->Font();
				$rep->Line($rep->row - 2);
				$rep->NewLine();
				$item_total = 0;
			}
			$current_item = $row['stock_id'];
			$rep->Font('bold');
			$rep->TextCol(0, 7, $row['stock_id'] . ' - ' . $row['item_description']);
			$rep->Font();
			$rep->NewLine();
		}

		$rep->TextCol(0, 1, $row['batch_no']);
		$rep->TextCol(1, 2, '');
		$rep->TextCol(2, 3, $row['location_name']);
		$rep->AmountCol(3, 4, $row['quantity_on_hand'], get_qty_dec());
		$rep->TextCol(4, 5, $row['expiry_date'] ? sql2date($row['expiry_date']) : '-');
		$rep->TextCol(5, 6, ucfirst($row['status']));
		$rep->AmountCol(6, 7, $row['stock_value'], $dec);
		$rep->NewLine();

		$item_total += $row['stock_value'];
		$grand_total += $row['stock_value'];
	}

	if ($current_item != '') {
		$rep->NewLine();
		$rep->Font('bold');
		$rep->TextCol(0, 3, _('Subtotal'));
		$rep->AmountCol(6, 7, $item_total, $dec);
		$rep->Font();
	}

	$rep->Line($rep->row - 2);
	$rep->NewLine(2);
	$rep->Font('bold');
	$rep->TextCol(0, 3, _('Grand Total'));
	$rep->AmountCol(6, 7, $grand_total, $dec);
	$rep->Font();

	$rep->End();
}
