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
 * Widget 827 — Pending QC Inspections
 *
 * Shows count of pending quality control inspections.
 */

$widget = new Widget();
$widget->setTitle(_('Pending QC Inspections'));
$widget->Start();

if ($widget->checkSecurity('SA_QC_INSPECTIONS')) {
	$value = dashboard_count_pending_qc();
	render_dashboard_small_stat_card(_('Pending QC'), $value, 'clipboard-check', $value > 0 ? 'warning' : 'success');
}

$widget->End();
