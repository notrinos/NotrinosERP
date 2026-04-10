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
 * Report 316 — Expiry Report
 *
 * Items approaching or past expiry date.
 *
 * Parameters:
 *   PARAM_0: Days Threshold (items expiring within N days)
 *   PARAM_1: Item Category
 *   PARAM_2: Location
 *   PARAM_3: Include Expired
 *   PARAM_4: Comments
 *   PARAM_5: Orientation
 *   PARAM_6: Destination (0=PDF, 1=Excel)
 */
$page_security = 'SA_BATCHINQUIRY';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/includes/db/inventory_reports_db.inc');

//----------------------------------------------------------------------------------------------------

print_expiry_report();

function print_expiry_report()
{
	global $path_to_root;

	$days       = $_POST['PARAM_0'];
	$category   = $_POST['PARAM_1'];
	$location   = $_POST['PARAM_2'];
	$inc_expired = $_POST['PARAM_3'];
	$comments   = $_POST['PARAM_4'];
	$orientation = $_POST['PARAM_5'];
	$destination = $_POST['PARAM_6'];

	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');
	$dec = user_price_dec();

	if (!$days || $days <= 0) $days = 90;
	if ($category == ALL_NUMERIC) $category = 0;
	if ($location == ALL_TEXT) $location = null;
	$inc_expired = !$inc_expired; // YES_NO: 0 = Yes, 1 = No in report system

	$cat_name = $category ? get_category_name($category) : _('All');
	$loc_name = $location ? get_location_name($location) : _('All');

	$cols = array(0, 80, 200, 280, 340, 400, 440, 515);
	$headers = array(_('Batch #'), _('Item'), _('Location'), _('Qty'), _('Expiry Date'), _('Days Left'), _('Value'));
	$aligns = array('left', 'left', 'left', 'right', 'left', 'right', 'right');

	$params = array(
		0 => $comments,
		1 => array('text' => _('Expiring Within'), 'from' => $days . ' ' . _('days'), 'to' => ''),
		2 => array('text' => _('Category'), 'from' => $cat_name, 'to' => ''),
		3 => array('text' => _('Location'), 'from' => $loc_name, 'to' => ''),
		4 => array('text' => _('Include Expired'), 'from' => $inc_expired ? _('Yes') : _('No'), 'to' => ''),
	);

	$rep = new FrontReport(_('Expiry Report'), 'ExpiryReport', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$result = get_expiry_report_data($days, null, $location, $category, $inc_expired);

	$grand_total = 0;
	$expired_count = 0;
	$expiring_count = 0;

	while ($row = db_fetch($result)) {
		$days_left = $row['days_until_expiry'];
		$is_expired = ($days_left < 0);

		if ($is_expired)
			$expired_count++;
		else
			$expiring_count++;

		$rep->TextCol(0, 1, $row['batch_no']);
		$rep->TextCol(1, 2, $row['stock_id'] . ' - ' . $row['item_description']);
		$rep->TextCol(2, 3, $row['location_name']);
		$rep->AmountCol(3, 4, $row['quantity_on_hand'], get_qty_dec());
		$rep->TextCol(4, 5, sql2date($row['expiry_date']));
		$rep->TextCol(5, 6, $is_expired ? _('EXPIRED') : number_format2($days_left, 0));
		$rep->AmountCol(6, 7, $row['stock_value'], $dec);
		$rep->NewLine();

		$grand_total += $row['stock_value'];
	}

	$rep->Line($rep->row - 2);
	$rep->NewLine();
	$rep->Font('bold');
	$rep->TextCol(0, 3, _('Total'));
	$rep->AmountCol(6, 7, $grand_total, $dec);
	$rep->NewLine();
	$rep->TextCol(0, 4, sprintf(_('Expired: %d   Expiring Soon: %d'), $expired_count, $expiring_count));
	$rep->Font();

	$rep->End();
}
