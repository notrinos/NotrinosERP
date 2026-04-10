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
 * Report 317 — Quality Inspection Summary
 *
 * Inspection pass/fail statistics by item.
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
$page_security = 'SA_QC_INSPECTIONS';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/includes/db/inventory_reports_db.inc');

//----------------------------------------------------------------------------------------------------

print_quality_summary_report();

function print_quality_summary_report()
{
	global $path_to_root;

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

	$cols = array(0, 80, 220, 280, 340, 400, 460, 515);
	$headers = array(_('Item'), _('Description'), _('Total'), _('Passed'), _('Failed'), _('Pending'), _('Pass Rate %'));
	$aligns = array('left', 'left', 'right', 'right', 'right', 'right', 'right');

	$params = array(
		0 => $comments,
		1 => array('text' => _('Period'), 'from' => $from_date, 'to' => $to_date),
		2 => array('text' => _('Item'), 'from' => $item_name, 'to' => ''),
		3 => array('text' => _('Location'), 'from' => $loc_name, 'to' => ''),
	);

	$rep = new FrontReport(_('Quality Inspection Summary'), 'QCSummary', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$result = get_quality_summary_data($from_date, $to_date, $stock_id, $location);

	$total_inspections = 0;
	$total_passed = 0;
	$total_failed = 0;
	$total_pending = 0;

	while ($row = db_fetch($result)) {
		$rep->TextCol(0, 1, $row['stock_id']);
		$rep->TextCol(1, 2, $row['item_description']);
		$rep->AmountCol(2, 3, $row['total_inspections'], 0);
		$rep->AmountCol(3, 4, $row['passed'], 0);
		$rep->AmountCol(4, 5, $row['failed'], 0);
		$rep->AmountCol(5, 6, $row['pending'], 0);
		$rep->TextCol(6, 7, number_format2($row['pass_rate'], 1) . '%');
		$rep->NewLine();

		$total_inspections += $row['total_inspections'];
		$total_passed += $row['passed'];
		$total_failed += $row['failed'];
		$total_pending += $row['pending'];
	}

	$rep->Line($rep->row - 2);
	$rep->NewLine();
	$rep->Font('bold');
	$rep->TextCol(0, 2, _('Totals'));
	$rep->AmountCol(2, 3, $total_inspections, 0);
	$rep->AmountCol(3, 4, $total_passed, 0);
	$rep->AmountCol(4, 5, $total_failed, 0);
	$rep->AmountCol(5, 6, $total_pending, 0);
	$overall_rate = $total_inspections > 0 ? round($total_passed * 100.0 / $total_inspections, 1) : 0;
	$rep->TextCol(6, 7, number_format2($overall_rate, 1) . '%');
	$rep->Font();

	$rep->End();
}
