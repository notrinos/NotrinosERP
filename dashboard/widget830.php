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
 * Widget 830 — Quarantine Items
 *
 * Shows items currently held in quarantine from QC inspections.
 */

$width = 100;

$widget = new Widget();
$widget->setTitle(_('Quarantine Items'));
$widget->Start();

if ($widget->checkSecurity('SA_QC_INSPECTIONS')) {
	$result = dashboard_get_quarantine_items(10);
	$count = 0;

	$th = array(_('Item'), _('Batch/Serial'), _('Qty'), _('Location'), _('Hold Date'));
	start_table(TABLESTYLE, "width='$width%'");
	table_header($th);
	$k = 0;
	while ($row = db_fetch($result)) {
		alt_table_row_color($k);
		label_cell($row['stock_id'] . ' ' . $row['description']);
		label_cell($row['batch_no']);
		qty_cell($row['qty']);
		label_cell($row['location_name']);
		label_cell(sql2date($row['hold_date']));
		end_row();
		$count++;
	}
	if ($count == 0) {
		start_row();
		label_cell(_('No items in quarantine'), "colspan='5' class='centered'");
		end_row();
	}
	end_table();
}

$widget->End();
