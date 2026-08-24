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
 * Resolve a request value to a configured company identifier.
 *
 * @param mixed $requested_id
 * @param array $connections
 * @return int|false
 */
function resolve_upgrade_log_company_id($requested_id, $connections) {
	if (!is_string($requested_id) || !preg_match('/\A[0-9]+\z/D', $requested_id))
		return false;

	return array_key_exists($requested_id, $connections) ? (int)$requested_id : false;
}
