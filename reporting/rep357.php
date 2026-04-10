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
 * Report 357 — Transfer Order Status
 *
 * Status of inter-location transfer orders.
 *
 * Parameters:
 *   PARAM_0: Start Date
 *   PARAM_1: End Date
 *   PARAM_2: Status filter (0=All, 1=Pending, 2=In Transit, 3=Completed)
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

print_transfer_order_status_report();

function print_transfer_order_status_report()
{
	global $path_to_root;

	$from_date   = $_POST['PARAM_0'];
	$to_date     = $_POST['PARAM_1'];
	$status      = $_POST['PARAM_2'];
	$comments    = $_POST['PARAM_3'];
	$orientation = $_POST['PARAM_4'];
	$destination = $_POST['PARAM_5'];

	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');

	$status_names = array(0 => _('All'), 1 => _('Pending'), 2 => _('In Transit'), 3 => _('Completed'));
	$status_name = isset($status_names[$status]) ? $status_names[$status] : _('All');

	$cols = array(0, 70, 130, 210, 290, 345, 395, 445, 515);
	$headers = array(_('Reference'), _('Date'), _('From'), _('To'), _('Status'), _('Lines'), _('Qty'), _('Received'));
	$aligns = array('left', 'left', 'left', 'left', 'left', 'right', 'right', 'right');

	$params = array(
		0 => $comments,
		1 => array('text' => _('Period'), 'from' => $from_date, 'to' => $to_date),
		2 => array('text' => _('Status'), 'from' => $status_name, 'to' => ''),
	);

	$rep = new FrontReport(_('Transfer Order Status'), 'TransferStatus', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$result = get_transfer_order_status_data($from_date, $to_date, $status);

	$total_lines = 0;
	$total_qty = 0;
	$total_received = 0;

	while ($row = db_fetch($result)) {
		$rep->TextCol(0, 1, $row['reference']);
		$rep->TextCol(1, 2, sql2date($row['tran_date']));
		$rep->TextCol(2, 3, $row['from_location']);
		$rep->TextCol(3, 4, $row['to_location']);
		$rep->TextCol(4, 5, $row['status_text']);
		$rep->AmountCol(5, 6, $row['total_lines'], 0);
		$rep->AmountCol(6, 7, $row['total_qty'], 0);
		$rep->AmountCol(7, 8, $row['received_qty'], 0);
		$rep->NewLine();

		$total_lines += $row['total_lines'];
		$total_qty += $row['total_qty'];
		$total_received += $row['received_qty'];
	}

	$rep->Line($rep->row - 2);
	$rep->NewLine();
	$rep->Font('bold');
	$rep->TextCol(0, 5, _('Totals'));
	$rep->AmountCol(5, 6, $total_lines, 0);
	$rep->AmountCol(6, 7, $total_qty, 0);
	$rep->AmountCol(7, 8, $total_received, 0);
	$rep->Font();

	$rep->End();
}
