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

$width = 100;
$result = hrm_employees_by_department();
$title = _('Employees by Department');

$widget = new Widget();
$widget->setTitle($title);
$widget->Start();

if ($widget->checkSecurity('SA_EMPLOYEE')) {
	$th = array(_('Department'), _('Employees'));
	start_table(TABLESTYLE, "width='$width%'");
	table_header($th);
	$k = 0;
	while ($myrow = db_fetch($result)) {
		alt_table_row_color($k);
		label_cell($myrow['department_name'] != '' ? $myrow['department_name'] : _('Unassigned'));
		qty_cell($myrow['total'], false, 0);
		end_row();
	}
	end_table();
}

$widget->End();
