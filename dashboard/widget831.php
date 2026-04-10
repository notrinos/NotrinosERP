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
 * Widget 831 — Open Warranty Claims
 *
 * Shows count of open/pending warranty claims.
 */

$widget = new Widget();
$widget->setTitle(_('Open Warranty Claims'));
$widget->Start();

if ($widget->checkSecurity('SA_WARRANTY')) {
	$value = dashboard_count_open_warranty_claims();
	render_dashboard_small_stat_card(_('Open Claims'), $value, 'file-text', $value > 0 ? 'warning' : 'success');
}

$widget->End();
