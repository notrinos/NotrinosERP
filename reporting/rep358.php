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
 * Report 358 — Cycle Count Accuracy
 *
 * Cycle counting accuracy statistics by location.
 *
 * Parameters:
 *   PARAM_0: Start Date
 *   PARAM_1: End Date
 *   PARAM_2: Location
 *   PARAM_3: Comments
 *   PARAM_4: Orientation
 *   PARAM_5: Destination (0=PDF, 1=Excel)
 */
$page_security = 'SA_WAREHOUSE_CYCLE_COUNT';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_reports_db.inc');

//----------------------------------------------------------------------------------------------------

print_cycle_count_accuracy_report();

function print_cycle_count_accuracy_report()
{
	global $path_to_root;

	$from_date   = $_POST['PARAM_0'];
	$to_date     = $_POST['PARAM_1'];
	$location    = $_POST['PARAM_2'];
	$comments    = $_POST['PARAM_3'];
	$orientation = $_POST['PARAM_4'];
	$destination = $_POST['PARAM_5'];

	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');
	$dec = user_price_dec();

	if ($location == ALL_TEXT) $location = null;
	$loc_name = $location ? get_location_name($location) : _('All');

	$cols = array(0, 70, 130, 200, 260, 320, 390, 460, 515);
	$headers = array(_('Reference'), _('Date'), _('Location'), _('Total Lines'), _('Accurate'), _('Variance'), _('Accuracy %'), _('Var. Value'));
	$aligns = array('left', 'left', 'left', 'right', 'right', 'right', 'right', 'right');

	$params = array(
		0 => $comments,
		1 => array('text' => _('Period'), 'from' => $from_date, 'to' => $to_date),
		2 => array('text' => _('Location'), 'from' => $loc_name, 'to' => ''),
	);

	$rep = new FrontReport(_('Cycle Count Accuracy'), 'CycleAccuracy', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$result = get_cycle_count_accuracy_data($from_date, $to_date, $location);

	$grand_lines = 0;
	$grand_accurate = 0;
	$grand_variance = 0;
	$grand_var_value = 0;

	while ($row = db_fetch($result)) {
		$rep->TextCol(0, 1, $row['reference']);
		$rep->TextCol(1, 2, sql2date($row['count_date']));
		$rep->TextCol(2, 3, $row['location_name']);
		$rep->AmountCol(3, 4, $row['total_lines'], 0);
		$rep->AmountCol(4, 5, $row['accurate_lines'], 0);
		$rep->AmountCol(5, 6, $row['variance_lines'], 0);
		$rep->AmountCol(6, 7, $row['accuracy_pct'], 1);
		$rep->AmountCol(7, 8, $row['variance_value'], $dec);
		$rep->NewLine();

		$grand_lines += $row['total_lines'];
		$grand_accurate += $row['accurate_lines'];
		$grand_variance += $row['variance_lines'];
		$grand_var_value += $row['variance_value'];
	}

	$rep->Line($rep->row - 2);
	$rep->NewLine();
	$rep->Font('bold');
	$rep->TextCol(0, 3, _('Totals'));
	$rep->AmountCol(3, 4, $grand_lines, 0);
	$rep->AmountCol(4, 5, $grand_accurate, 0);
	$rep->AmountCol(5, 6, $grand_variance, 0);
	$overall_pct = $grand_lines > 0 ? round(($grand_accurate / $grand_lines) * 100, 1) : 0;
	$rep->AmountCol(6, 7, $overall_pct, 1);
	$rep->AmountCol(7, 8, $grand_var_value, $dec);
	$rep->Font();

	$rep->End();
}
