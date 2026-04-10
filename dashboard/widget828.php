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
 * Widget 828 — Serial Number Status Summary
 *
 * Shows serial number counts by status (In Stock, Sold, Scrapped).
 */

$width = 100;

$widget = new Widget();
$widget->setTitle(_('Serial Status Summary'));
$widget->Start();

if ($widget->checkSecurity('SA_SERIALINQUIRY')) {
	$result = dashboard_get_serial_status_summary();
	$count = 0;

	$th = array(_('Status'), _('Count'));
	start_table(TABLESTYLE, "width='$width%'");
	table_header($th);
	$k = 0;
	while ($row = db_fetch($result)) {
		alt_table_row_color($k);
		label_cell($row['status']);
		qty_cell($row['total'], false, 0);
		end_row();
		$count++;
	}
	if ($count == 0) {
		start_row();
		label_cell(_('No serial tracking data'), "colspan='2' class='centered'");
		end_row();
	}
	end_table();
}

$widget->End();
