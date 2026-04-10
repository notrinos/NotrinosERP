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
 * Report 352 — Bin-Level Stock Movement Ledger
 *
 * All stock moves at bin precision with serial/batch detail.
 *
 * Parameters:
 *   PARAM_0: Start Date
 *   PARAM_1: End Date
 *   PARAM_2: Location
 *   PARAM_3: Item
 *   PARAM_4: Comments
 *   PARAM_5: Orientation
 *   PARAM_6: Destination (0=PDF, 1=Excel)
 */
$page_security = 'SA_WAREHOUSE_DASHBOARD';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_reports_db.inc');

//----------------------------------------------------------------------------------------------------

print_bin_movement_ledger_report();

function print_bin_movement_ledger_report()
{
	global $path_to_root, $systypes_array;

	$from_date = $_POST['PARAM_0'];
	$to_date   = $_POST['PARAM_1'];
	$location  = $_POST['PARAM_2'];
	$stock_id  = $_POST['PARAM_3'];
	$comments  = $_POST['PARAM_4'];
	$orientation = $_POST['PARAM_5'];
	$destination = $_POST['PARAM_6'];

	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');
	$dec = user_price_dec();

	if ($location == ALL_TEXT) $location = null;
	if ($stock_id == ALL_TEXT) $stock_id = null;

	$loc_name = $location ? get_location_name($location) : _('All');
	$item_name = $stock_id ? $stock_id : _('All');

	$cols = array(0, 60, 140, 200, 260, 330, 395, 515);
	$headers = array(_('Date'), _('Item'), _('Type'), _('Qty'), _('From Bin'), _('To Bin'), _('Batch/Serial'));
	$aligns = array('left', 'left', 'left', 'right', 'left', 'left', 'left');

	$params = array(
		0 => $comments,
		1 => array('text' => _('Period'), 'from' => $from_date, 'to' => $to_date),
		2 => array('text' => _('Location'), 'from' => $loc_name, 'to' => ''),
		3 => array('text' => _('Item'), 'from' => $item_name, 'to' => ''),
	);

	$rep = new FrontReport(_('Bin-Level Movement Ledger'), 'BinMovementLedger', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$result = get_bin_movement_ledger_data($from_date, $to_date, $location, $stock_id);

	$move_count = 0;

	while ($row = db_fetch($result)) {
		$rep->TextCol(0, 1, sql2date($row['tran_date']));
		$rep->TextCol(1, 2, $row['stock_id']);
		$type_label = isset($systypes_array[$row['type']]) ? $systypes_array[$row['type']] : '#' . $row['type'];
		$rep->TextCol(2, 3, $type_label);
		$rep->AmountCol(3, 4, $row['qty'], get_qty_dec());
		$rep->TextCol(4, 5, $row['from_bin_code'] ? $row['from_bin_code'] : '-');
		$rep->TextCol(5, 6, $row['to_bin_code'] ? $row['to_bin_code'] : '-');

		$batch_serial = '';
		if ($row['batch_no']) $batch_serial .= $row['batch_no'];
		if ($row['serial_no']) $batch_serial .= ($batch_serial ? '/' : '') . $row['serial_no'];
		$rep->TextCol(6, 7, $batch_serial ? $batch_serial : '-');
		$rep->NewLine();

		$move_count++;
	}

	$rep->Line($rep->row - 2);
	$rep->NewLine();
	$rep->Font('bold');
	$rep->TextCol(0, 7, sprintf(_('Total movements: %d'), $move_count));
	$rep->Font();

	$rep->End();
}
