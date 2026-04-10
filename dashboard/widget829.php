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
 * Widget 829 — Active Recalls
 *
 * Shows count of active product recalls.
 */

$widget = new Widget();
$widget->setTitle(_('Active Recalls'));
$widget->Start();

if ($widget->checkSecurity('SA_SERIALINQUIRY')) {
	$value = dashboard_count_active_recalls();
	render_dashboard_small_stat_card(_('Active Recalls'), $value, 'alert-triangle', $value > 0 ? 'danger' : 'success');
}

$widget->End();
