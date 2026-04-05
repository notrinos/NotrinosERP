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
 * CRM Dashboard Widget 1013 — Pipeline Value (stat card).
 */
$widget = new Widget();
$widget->setTitle(_('Pipeline Value'));
$widget->Start();

if ($widget->checkSecurity('SA_CRM_PIPELINE')) {
	$value = dashboard_crm_pipeline_value();
	$formatted = number_format2($value, user_price_dec());
	render_dashboard_small_stat_card(_('Pipeline Value'), $formatted, 'dollar-sign', 'success');
}

$widget->End();
