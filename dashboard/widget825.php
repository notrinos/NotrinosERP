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
 * Widget 825 — Expiring Batches Alert
 *
 * Shows batches expiring within 30 days.
 */

$width = 100;

$widget = new Widget();
$widget->setTitle(_('Expiring Batches (30 days)'));
$widget->Start();

if ($widget->checkSecurity('SA_BATCHINQUIRY')) {
	$result = dashboard_get_expiring_batches(30, 10);
	$count = 0;

	$th = array(_('Item'), _('Batch'), _('Expiry'), _('Qty'), _('Location'));
	start_table(TABLESTYLE, "width='$width%'");
	table_header($th);
	$k = 0;
	while ($row = db_fetch($result)) {
		alt_table_row_color($k);
		label_cell($row['stock_id'] . ' ' . $row['description']);
		label_cell($row['batch_no']);
		label_cell(sql2date($row['expiry_date']));
		qty_cell($row['qty_on_hand']);
		label_cell($row['location_name']);
		end_row();
		$count++;
	}
	if ($count == 0) {
		start_row();
		label_cell(_('No batches expiring soon'), "colspan='5' class='centered'");
		end_row();
	}
	end_table();
}

$widget->End();
