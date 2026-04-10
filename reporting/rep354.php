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
 * Report 354 — Dead Stock Report
 *
 * Items with no movement in N days.
 *
 * Parameters:
 *   PARAM_0: No Movement Days (threshold)
 *   PARAM_1: Location
 *   PARAM_2: Item Category
 *   PARAM_3: Comments
 *   PARAM_4: Orientation
 *   PARAM_5: Destination (0=PDF, 1=Excel)
 */
$page_security = 'SA_ITEMSVALREP';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_reports_db.inc');

//----------------------------------------------------------------------------------------------------

print_dead_stock_report();

function print_dead_stock_report()
{
	global $path_to_root;

	$no_move_days = $_POST['PARAM_0'];
	$location     = $_POST['PARAM_1'];
	$category     = $_POST['PARAM_2'];
	$comments     = $_POST['PARAM_3'];
	$orientation  = $_POST['PARAM_4'];
	$destination  = $_POST['PARAM_5'];

	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');
	$dec = user_price_dec();

	if (!$no_move_days || $no_move_days <= 0) $no_move_days = 180;
	if ($category == ALL_NUMERIC) $category = 0;
	if ($location == ALL_TEXT) $location = null;

	$loc_name = $location ? get_location_name($location) : _('All');
	$cat_name = $category ? get_category_name($category) : _('All');

	$cols = array(0, 80, 200, 270, 330, 395, 455, 515);
	$headers = array(_('Item'), _('Description'), _('Location'), _('Qty'), _('Last Move'), _('Days Idle'), _('Value'));
	$aligns = array('left', 'left', 'left', 'right', 'left', 'right', 'right');

	$params = array(
		0 => $comments,
		1 => array('text' => _('No Movement For'), 'from' => $no_move_days . ' ' . _('days'), 'to' => ''),
		2 => array('text' => _('Location'), 'from' => $loc_name, 'to' => ''),
		3 => array('text' => _('Category'), 'from' => $cat_name, 'to' => ''),
	);

	$rep = new FrontReport(_('Dead Stock Report'), 'DeadStock', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$result = get_dead_stock_data($no_move_days, $location, $category);

	$grand_total = 0;
	$item_count = 0;

	while ($row = db_fetch($result)) {
		$rep->TextCol(0, 1, $row['stock_id']);
		$rep->TextCol(1, 2, $row['item_description']);
		$rep->TextCol(2, 3, $row['location_name']);
		$rep->AmountCol(3, 4, $row['qty_on_hand'], get_qty_dec());
		$rep->TextCol(4, 5, $row['last_movement_date'] ? sql2date($row['last_movement_date']) : _('Never'));
		$rep->AmountCol(5, 6, $row['days_since_movement'] ? $row['days_since_movement'] : 999, 0);
		$rep->AmountCol(6, 7, $row['stock_value'], $dec);
		$rep->NewLine();

		$grand_total += $row['stock_value'];
		$item_count++;
	}

	$rep->Line($rep->row - 2);
	$rep->NewLine();
	$rep->Font('bold');
	$rep->TextCol(0, 3, sprintf(_('Total: %d items'), $item_count));
	$rep->AmountCol(6, 7, $grand_total, $dec);
	$rep->Font();

	$rep->End();
}
