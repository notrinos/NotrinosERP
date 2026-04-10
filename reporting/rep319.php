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
 * Report 319 — Serial Warranty Status
 *
 * Active warranties by item/customer with days remaining.
 *
 * Parameters:
 *   PARAM_0: Item
 *   PARAM_1: Active Only
 *   PARAM_2: Comments
 *   PARAM_3: Orientation
 *   PARAM_4: Destination (0=PDF, 1=Excel)
 */
$page_security = 'SA_WARRANTY';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/includes/db/inventory_reports_db.inc');

//----------------------------------------------------------------------------------------------------

print_warranty_status_report();

function print_warranty_status_report()
{
	global $path_to_root;

	$stock_id    = $_POST['PARAM_0'];
	$active_only = $_POST['PARAM_1'];
	$comments    = $_POST['PARAM_2'];
	$orientation = $_POST['PARAM_3'];
	$destination = $_POST['PARAM_4'];

	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');
	$active_only = !$active_only; // YES_NO inverted

	if ($stock_id == ALL_TEXT) $stock_id = null;
	$item_name = $stock_id ? $stock_id : _('All');

	$cols = array(0, 80, 170, 260, 340, 410, 460, 515);
	$headers = array(_('Serial #'), _('Item'), _('Customer'), _('Warranty End'), _('Days Left'), _('Type'), _('Status'));
	$aligns = array('left', 'left', 'left', 'left', 'right', 'left', 'left');

	$params = array(
		0 => $comments,
		1 => array('text' => _('Item'), 'from' => $item_name, 'to' => ''),
		2 => array('text' => _('Active Only'), 'from' => $active_only ? _('Yes') : _('No'), 'to' => ''),
	);

	$rep = new FrontReport(_('Serial Warranty Status'), 'WarrantyStatus', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$result = get_warranty_status_data($stock_id, $active_only);

	$active_count = 0;
	$expiring_count = 0;
	$expired_count = 0;

	while ($row = db_fetch($result)) {
		$rep->TextCol(0, 1, $row['serial_no']);
		$rep->TextCol(1, 2, $row['stock_id']);
		$rep->TextCol(2, 3, $row['customer_name'] ? $row['customer_name'] : '-');
		$rep->TextCol(3, 4, $row['warranty_end'] ? sql2date($row['warranty_end']) : '-');
		$rep->TextCol(4, 5, $row['days_remaining'] !== null ? number_format2($row['days_remaining'], 0) : '-');
		$rep->TextCol(5, 6, $row['warranty_type'] ? ucfirst($row['warranty_type']) : '-');

		$ws = $row['warranty_status'];
		if ($ws == 'expired') $expired_count++;
		elseif ($ws == 'expiring_soon') $expiring_count++;
		else $active_count++;

		$rep->TextCol(6, 7, ucfirst(str_replace('_', ' ', $ws)));
		$rep->NewLine();
	}

	$rep->Line($rep->row - 2);
	$rep->NewLine();
	$rep->Font('bold');
	$rep->TextCol(0, 7, sprintf(_('Active: %d   Expiring Soon: %d   Expired: %d'), $active_count, $expiring_count, $expired_count));
	$rep->Font();

	$rep->End();
}
