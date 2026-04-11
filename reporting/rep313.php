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
 * Report 313 — Serial Number Ledger
 *
 * Complete movement history per serial number.
 *
 * Parameters:
 *   PARAM_0: Start Date
 *   PARAM_1: End Date
 *   PARAM_2: Item
 *   PARAM_3: Location
 *   PARAM_4: Comments
 *   PARAM_5: Orientation
 *   PARAM_6: Destination (0=PDF, 1=Excel)
 */
$page_security = 'SA_SERIALINQUIRY';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/includes/db/inventory_reports_db.inc');

//----------------------------------------------------------------------------------------------------

print_serial_ledger_report();

function print_serial_ledger_report()
{
	global $path_to_root, $systypes_array;

	$from_date = $_POST['PARAM_0'];
	$to_date   = $_POST['PARAM_1'];
	$stock_id  = $_POST['PARAM_2'];
	$location  = $_POST['PARAM_3'];
	$comments  = $_POST['PARAM_4'];
	$orientation = $_POST['PARAM_5'];
	$destination = $_POST['PARAM_6'];

	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');

	if ($stock_id == ALL_TEXT) $stock_id = null;
	if ($location == ALL_TEXT) $location = null;

	$item_name = $stock_id ? $stock_id : _('All');
	$loc_name = $location ? get_location_name($location) : _('All');

	$cols = array(0, 80, 160, 240, 300, 360, 420, 515);
	$headers = array(_('Serial #'), _('Reference'), _('Date'), _('Type'), _('From'), _('To'), _('Status Change'));
	$aligns = array('left', 'left', 'left', 'left', 'left', 'left', 'left');

	$params = array(
		0 => $comments,
		1 => array('text' => _('Period'), 'from' => $from_date, 'to' => $to_date),
		2 => array('text' => _('Item'), 'from' => $item_name, 'to' => ''),
		3 => array('text' => _('Location'), 'from' => $loc_name, 'to' => ''),
	);

	$rep = new FrontReport(_('Serial Number Ledger'), 'SerialLedger', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$result = get_serial_ledger_data($stock_id, null, $from_date, $to_date, $location);

	$current_serial = '';
	while ($row = db_fetch($result)) {
		if ($current_serial != $row['serial_no']) {
			if ($current_serial != '') {
				$rep->Line($rep->row - 2);
				$rep->NewLine();
			}
			$current_serial = $row['serial_no'];
			$rep->Font('bold');
			$rep->TextCol(0, 2, $row['serial_no']);
			$rep->TextCol(2, 7, $row['stock_id'] . ' - ' . $row['item_description']);
			$rep->Font();
			$rep->NewLine();
		}

		$rep->TextCol(0, 1, '');
		$rep->TextCol(1, 2, $row['reference'] ? $row['reference'] : ('#' . $row['trans_no']));
		$rep->TextCol(2, 3, sql2date($row['tran_date']));
		$type_label = isset($systypes_array[$row['trans_type']]) ? $systypes_array[$row['trans_type']] : $row['trans_type'];
		$rep->TextCol(3, 4, $type_label);
		$rep->TextCol(4, 5, $row['from_location'] ? $row['from_location'] : '-');
		$rep->TextCol(5, 6, $row['to_location'] ? $row['to_location'] : '-');
		$status_change = '';
		if ($row['from_status'] || $row['to_status'])
			$status_change = ($row['from_status'] ? $row['from_status'] : '?') . ' → ' . ($row['to_status'] ? $row['to_status'] : '?');
		$rep->TextCol(6, 7, $status_change);
		$rep->NewLine();
	}

	$rep->Line($rep->row - 4);
	$rep->End();
}
