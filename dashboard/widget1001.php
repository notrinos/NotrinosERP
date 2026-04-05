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
 * CRM Dashboard Widget 1001 — Pipeline by Stage (table).
 */
$width = 100;
$result = dashboard_crm_pipeline_by_stage();
$title = _('Pipeline by Stage');

$widget = new Widget();
$widget->setTitle($title);
$widget->Start();

if ($widget->checkSecurity('SA_CRM_PIPELINE')) {
	if ($result) {
		$th = array(_('Stage'), _('Probability'), _('Deals'), _('Value'));
		start_table(TABLESTYLE, "width='$width%'");
		table_header($th);
		$k = 0;
		while ($myrow = db_fetch($result)) {
			alt_table_row_color($k);
			label_cell($myrow['stage_name']);
			label_cell($myrow['probability'].'%', "align='right'");
			qty_cell($myrow['total'], false, 0);
			amount_cell($myrow['revenue']);
			end_row();
		}
		end_table();
	} else {
		display_note(_('CRM tables not available.'));
	}
}

$widget->End();
