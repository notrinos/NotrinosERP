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
 * Report 320 — FEFO Compliance Report
 *
 * Cases where first-expiry-first-out was not followed.
 *
 * Parameters:
 *   PARAM_0: As at Date
 *   PARAM_1: Item
 *   PARAM_2: Location
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

print_fefo_compliance_report();

function print_fefo_compliance_report()
{
	global $path_to_root, $systypes_array;

	$as_at_date = $_POST['PARAM_0'];
	$stock_id  = $_POST['PARAM_1'];
	$location  = $_POST['PARAM_2'];
	$comments  = $_POST['PARAM_3'];
	$orientation = $_POST['PARAM_4'];
	$destination = $_POST['PARAM_5'];

	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');

	if ($stock_id == ALL_TEXT) $stock_id = null;
	if ($location == ALL_TEXT) $location = null;

	$item_name = $stock_id ? $stock_id : _('All');
	$loc_name = $location ? get_location_name($location) : _('All');

	$cols = array(0, 60, 160, 230, 310, 390, 450, 515);
	$headers = array(_('Date'), _('Item'), _('Location'), _('Batch Picked'), _('Picked Expiry'), _('Oldest Avail.'), _('Qty'));
	$aligns = array('left', 'left', 'left', 'left', 'left', 'left', 'right');

	$params = array(
		0 => $comments,
		1 => array('text' => _('As at Date'), 'from' => $as_at_date, 'to' => ''),
		2 => array('text' => _('Item'), 'from' => $item_name, 'to' => ''),
		3 => array('text' => _('Location'), 'from' => $loc_name, 'to' => ''),
	);

	$rep = new FrontReport(_('FEFO Compliance Report'), 'FEFOCompliance', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$result = get_fefo_compliance_data(null, $as_at_date, $stock_id, $location);

	$violation_count = 0;

	while ($row = db_fetch($result)) {
		$rep->TextCol(0, 1, sql2date($row['tran_date']));
		$rep->TextCol(1, 2, $row['stock_id'] . ' - ' . $row['item_description']);
		$rep->TextCol(2, 3, $row['location_name']);
		$rep->TextCol(3, 4, $row['picked_batch']);
		$rep->TextCol(4, 5, sql2date($row['picked_expiry']));
		$rep->TextCol(5, 6, sql2date($row['oldest_available_expiry']));
		$rep->AmountCol(6, 7, abs($row['qty_picked']), get_qty_dec());
		$rep->NewLine();
		$violation_count++;
	}

	$rep->Line($rep->row - 2);
	$rep->NewLine();
	$rep->Font('bold');
	$rep->TextCol(0, 7, sprintf(_('Total FEFO violations: %d'), $violation_count));
	$rep->Font();

	$rep->End();
}
