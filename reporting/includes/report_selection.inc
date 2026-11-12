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
 * DIM-LFC-002/AT-12: scope saved report selections to the authenticated principal and active company.
 *
 * Report parameters are often native Dimension IDs, and the same ID can name a different master in
 * another company. A browser-wide cookie therefore must not replay one company's saved filter into
 * another company (or another user's) report. The scope is derived only from the live session.
 *
 * @return string Opaque principal/company scope used inside the selection cookie
 */
function report_selection_scope() {
	$user = isset($_SESSION['wa_current_user']) ? $_SESSION['wa_current_user'] : null;
	$user_id = is_object($user) && isset($user->user) ? (int)$user->user : 0;
	$company = is_object($user) && isset($user->cur_con) ? (int)$user->cur_con : 0;
	return $user_id.'_'.$company;
}

/**
 * Stable cookie sub-key for one report parameter, isolated by principal and active company.
 *
 * @param int|string $report_id Native report ID
 * @param int|string $index Parameter position
 * @return string Cookie sub-key
 */
function report_selection_key($report_id, $index) {
	return report_selection_scope().'_'.(int)$index;
}
