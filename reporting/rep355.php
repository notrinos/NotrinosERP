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
 * Report 355 — Warehouse Utilization Report
 *
 * Capacity usage by location: weight, volume, item count.
 *
 * Parameters:
 *   PARAM_0: Location
 *   PARAM_1: Comments
 *   PARAM_2: Orientation
 *   PARAM_3: Destination (0=PDF, 1=Excel)
 */
$page_security = 'SA_WAREHOUSE_DASHBOARD';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_reports_db.inc');

//----------------------------------------------------------------------------------------------------

print_warehouse_utilization_report();

function print_warehouse_utilization_report()
{
	global $path_to_root;

	$location    = $_POST['PARAM_0'];
	$comments    = $_POST['PARAM_1'];
	$orientation = $_POST['PARAM_2'];
	$destination = $_POST['PARAM_3'];

	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');
	$dec = user_price_dec();

	if ($location == ALL_TEXT) $location = null;
	$loc_name = $location ? get_location_name($location) : _('All');

	$cols = array(0, 90, 150, 210, 270, 330, 390, 450, 515);
	$headers = array(_('Location'), _('Type'), _('Items'), _('Qty'), _('Wgt Used'), _('Vol Used'), _('Wgt %'), _('Vol %'));
	$aligns = array('left', 'left', 'right', 'right', 'right', 'right', 'right', 'right');

	$params = array(
		0 => $comments,
		1 => array('text' => _('Warehouse'), 'from' => $loc_name, 'to' => ''),
	);

	$rep = new FrontReport(_('Warehouse Utilization Report'), 'WHUtilization', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$result = get_warehouse_utilization_data($location);

	$grand_value = 0;

	while ($row = db_fetch($result)) {
		$rep->TextCol(0, 1, $row['location_code']);
		$rep->TextCol(1, 2, $row['location_type']);
		$rep->AmountCol(2, 3, $row['unique_items'], 0);
		$rep->AmountCol(3, 4, $row['items_stored'], get_qty_dec());
		$rep->AmountCol(4, 5, $row['weight_used'], 2);
		$rep->AmountCol(5, 6, $row['volume_used'], 2);
		$rep->TextCol(6, 7, $row['weight_pct'] > 0 ? number_format2($row['weight_pct'], 1) . '%' : '-');
		$rep->TextCol(7, 8, $row['volume_pct'] > 0 ? number_format2($row['volume_pct'], 1) . '%' : '-');
		$rep->NewLine();

		$grand_value += $row['stock_value'];
	}

	$rep->Line($rep->row - 2);
	$rep->NewLine();
	$rep->Font('bold');
	$rep->TextCol(0, 3, _('Total Stock Value'));
	$rep->AmountCol(3, 5, $grand_value, $dec);
	$rep->Font();

	$rep->End();
}
