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
 * Widget 826 — Warranty Expiry Alert
 *
 * Shows warranty items expiring within 30 days.
 */

$width = 100;

$widget = new Widget();
$widget->setTitle(_('Warranty Expiring (30 days)'));
$widget->Start();

if ($widget->checkSecurity('SA_WARRANTY')) {
	$result = dashboard_get_warranty_expiring(30, 10);
	$count = 0;

	$th = array(_('Item'), _('Serial No'), _('Expiry'), _('Customer'));
	start_table(TABLESTYLE, "width='$width%'");
	table_header($th);
	$k = 0;
	while ($row = db_fetch($result)) {
		alt_table_row_color($k);
		label_cell($row['stock_id'] . ' ' . $row['description']);
		label_cell($row['serial_no']);
		label_cell(sql2date($row['warranty_end']));
		label_cell($row['debtor_name']);
		end_row();
		$count++;
	}
	if ($count == 0) {
		start_row();
		label_cell(_('No warranties expiring soon'), "colspan='4' class='centered'");
		end_row();
	}
	end_table();
}

$widget->End();
