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
 * Report 321 — Serial Cost Report
 *
 * Actual cost tracking per serial number.
 *
 * Parameters:
 *   PARAM_0: Item
 *   PARAM_1: Location
 *   PARAM_2: Comments
 *   PARAM_3: Orientation
 *   PARAM_4: Destination (0=PDF, 1=Excel)
 */
$page_security = 'SA_SERIALINQUIRY';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/includes/db/inventory_reports_db.inc');

//----------------------------------------------------------------------------------------------------

print_serial_cost_report();

function print_serial_cost_report()
{
	global $path_to_root;

	$stock_id  = $_POST['PARAM_0'];
	$location  = $_POST['PARAM_1'];
	$comments  = $_POST['PARAM_2'];
	$orientation = $_POST['PARAM_3'];
	$destination = $_POST['PARAM_4'];

	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');
	$dec = user_price_dec();

	if ($stock_id == ALL_TEXT) $stock_id = null;
	if ($location == ALL_TEXT) $location = null;

	$item_name = $stock_id ? $stock_id : _('All');
	$loc_name = $location ? get_location_name($location) : _('All');

	$cols = array(0, 80, 180, 260, 330, 395, 455, 515);
	$headers = array(_('Serial #'), _('Item'), _('Status'), _('Location'), _('Purchase Cost'), _('Moves'), _('Supplier'));
	$aligns = array('left', 'left', 'left', 'left', 'right', 'right', 'left');

	$params = array(
		0 => $comments,
		1 => array('text' => _('Item'), 'from' => $item_name, 'to' => ''),
		2 => array('text' => _('Location'), 'from' => $loc_name, 'to' => ''),
	);

	$rep = new FrontReport(_('Serial Cost Report'), 'SerialCost', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$result = get_serial_cost_data($stock_id, $location);

	$grand_total = 0;
	$serial_count = 0;

	while ($row = db_fetch($result)) {
		$rep->TextCol(0, 1, $row['serial_no']);
		$rep->TextCol(1, 2, $row['stock_id']);
		$rep->TextCol(2, 3, ucfirst($row['status']));
		$rep->TextCol(3, 4, $row['location_name'] ? $row['location_name'] : '-');
		$rep->AmountCol(4, 5, $row['unit_cost'], $dec);
		$rep->AmountCol(5, 6, $row['movement_count'], 0);
		$rep->TextCol(6, 7, $row['supplier_name'] ? $row['supplier_name'] : '-');
		$rep->NewLine();

		$grand_total += $row['unit_cost'];
		$serial_count++;
	}

	$rep->Line($rep->row - 2);
	$rep->NewLine();
	$rep->Font('bold');
	$rep->TextCol(0, 3, sprintf(_('Total: %d serials'), $serial_count));
	$rep->AmountCol(4, 5, $grand_total, $dec);
	$rep->Font();

	$rep->End();
}
