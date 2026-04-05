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
 * CRM Dashboard Widget 1012 — Active Opportunities Count (stat card).
 */
$widget = new Widget();
$widget->setTitle(_('Opportunities'));
$widget->Start();

if ($widget->checkSecurity('SA_CRM_PIPELINE')) {
	$value = dashboard_count_crm_opportunities();
	render_dashboard_small_stat_card(_('Opportunities'), $value, 'target', 'info');
}

$widget->End();
