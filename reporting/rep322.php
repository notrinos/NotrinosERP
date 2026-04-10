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
 * Report 322 — Supplier Quality Report
 *
 * Rejection rate by supplier from QC inspections.
 *
 * Parameters:
 *   PARAM_0: Start Date
 *   PARAM_1: End Date
 *   PARAM_2: Supplier
 *   PARAM_3: Comments
 *   PARAM_4: Orientation
 *   PARAM_5: Destination (0=PDF, 1=Excel)
 */
$page_security = 'SA_QC_INSPECTIONS';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/includes/db/inventory_reports_db.inc');

//----------------------------------------------------------------------------------------------------

print_supplier_quality_report();

function print_supplier_quality_report()
{
	global $path_to_root;

	$from_date   = $_POST['PARAM_0'];
	$to_date     = $_POST['PARAM_1'];
	$supplier_id = $_POST['PARAM_2'];
	$comments    = $_POST['PARAM_3'];
	$orientation = $_POST['PARAM_4'];
	$destination = $_POST['PARAM_5'];

	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');

	if ($supplier_id == ALL_NUMERIC) $supplier_id = null;
	$supp_name = $supplier_id ? get_supplier_name($supplier_id) : _('All');

	$cols = array(0, 140, 200, 260, 320, 380, 450, 515);
	$headers = array(_('Supplier'), _('Total'), _('Passed'), _('Failed'), _('Conditional'), _('Reject %'), _('Items'));
	$aligns = array('left', 'right', 'right', 'right', 'right', 'right', 'right');

	$params = array(
		0 => $comments,
		1 => array('text' => _('Period'), 'from' => $from_date, 'to' => $to_date),
		2 => array('text' => _('Supplier'), 'from' => $supp_name, 'to' => ''),
	);

	$rep = new FrontReport(_('Supplier Quality Report'), 'SupplierQuality', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$result = get_supplier_quality_data($from_date, $to_date, $supplier_id);

	$grand_total = 0;
	$grand_failed = 0;

	while ($row = db_fetch($result)) {
		$rep->TextCol(0, 1, $row['supplier_name']);
		$rep->AmountCol(1, 2, $row['total_inspections'], 0);
		$rep->AmountCol(2, 3, $row['passed'], 0);
		$rep->AmountCol(3, 4, $row['failed'], 0);
		$rep->AmountCol(4, 5, $row['conditional'], 0);
		$rep->TextCol(5, 6, number_format2($row['rejection_rate'], 1) . '%');
		$rep->AmountCol(6, 7, $row['unique_items'], 0);
		$rep->NewLine();

		$grand_total += $row['total_inspections'];
		$grand_failed += $row['failed'];
	}

	$rep->Line($rep->row - 2);
	$rep->NewLine();
	$rep->Font('bold');
	$rep->TextCol(0, 1, _('Totals'));
	$rep->AmountCol(1, 2, $grand_total, 0);
	$rep->AmountCol(3, 4, $grand_failed, 0);
	$overall_rate = $grand_total > 0 ? round($grand_failed * 100.0 / $grand_total, 1) : 0;
	$rep->TextCol(5, 6, number_format2($overall_rate, 1) . '%');
	$rep->Font();

	$rep->End();
}
