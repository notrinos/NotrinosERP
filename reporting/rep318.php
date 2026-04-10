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
 * Report 318 — Warranty Claims Summary
 *
 * Claims by product, status breakdown, cost summary.
 *
 * Parameters:
 *   PARAM_0: Start Date
 *   PARAM_1: End Date
 *   PARAM_2: Item
 *   PARAM_3: Comments
 *   PARAM_4: Orientation
 *   PARAM_5: Destination (0=PDF, 1=Excel)
 */
$page_security = 'SA_WARRANTY';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/includes/db/inventory_reports_db.inc');

//----------------------------------------------------------------------------------------------------

print_warranty_claims_report();

function print_warranty_claims_report()
{
	global $path_to_root;

	$from_date = $_POST['PARAM_0'];
	$to_date   = $_POST['PARAM_1'];
	$stock_id  = $_POST['PARAM_2'];
	$comments  = $_POST['PARAM_3'];
	$orientation = $_POST['PARAM_4'];
	$destination = $_POST['PARAM_5'];

	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');
	$dec = user_price_dec();

	if ($stock_id == ALL_TEXT) $stock_id = null;
	$item_name = $stock_id ? $stock_id : _('All');

	$cols = array(0, 80, 200, 250, 300, 345, 385, 450, 515);
	$headers = array(_('Item'), _('Description'), _('Total'), _('Open'), _('In Prog'), _('Resolved'), _('Repair Cost'), _('Repl. Cost'));
	$aligns = array('left', 'left', 'right', 'right', 'right', 'right', 'right', 'right');

	$params = array(
		0 => $comments,
		1 => array('text' => _('Period'), 'from' => $from_date, 'to' => $to_date),
		2 => array('text' => _('Item'), 'from' => $item_name, 'to' => ''),
	);

	$rep = new FrontReport(_('Warranty Claims Summary'), 'WarrantyClaims', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$result = get_warranty_claims_summary_data($from_date, $to_date, $stock_id);

	$grand_claims = 0;
	$grand_repair = 0;
	$grand_replace = 0;

	while ($row = db_fetch($result)) {
		$rep->TextCol(0, 1, $row['stock_id']);
		$rep->TextCol(1, 2, $row['item_description']);
		$rep->AmountCol(2, 3, $row['total_claims'], 0);
		$rep->AmountCol(3, 4, $row['open_claims'], 0);
		$rep->AmountCol(4, 5, $row['in_progress'], 0);
		$rep->AmountCol(5, 6, $row['resolved'], 0);
		$rep->AmountCol(6, 7, $row['total_repair_cost'], $dec);
		$rep->AmountCol(7, 8, $row['total_replacement_cost'], $dec);
		$rep->NewLine();

		$grand_claims += $row['total_claims'];
		$grand_repair += $row['total_repair_cost'];
		$grand_replace += $row['total_replacement_cost'];
	}

	$rep->Line($rep->row - 2);
	$rep->NewLine();
	$rep->Font('bold');
	$rep->TextCol(0, 2, _('Totals'));
	$rep->AmountCol(2, 3, $grand_claims, 0);
	$rep->AmountCol(6, 7, $grand_repair, $dec);
	$rep->AmountCol(7, 8, $grand_replace, $dec);
	$rep->Font();

	$rep->End();
}
