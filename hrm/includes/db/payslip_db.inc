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

function get_employee_to_payslip($employee_id) {
	$sql = "SELECT employee_id, CONCAT(first_name, ' ', last_name) as name, position_id, hire_date, released_date FROM ".TB_PREF."employees WHERE employee_id = ".db_escape($employee_id);
	$result = db_query($sql, 'could not retreive employee record');

	return db_fetch($result);
}
