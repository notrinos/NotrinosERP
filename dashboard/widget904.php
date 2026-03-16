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
$result = hrm_employee_age_distribution();
$title = _('Employees by Age');

$widget = new Widget();
$widget->setTitle($title);
$widget->Start();

if ($widget->checkSecurity('SA_EMPLOYEE')) {
	$th = array(_('Age Range'), _('Employees'));
	start_table(TABLESTYLE, "width='$width%'");
	table_header($th);
	$k = 0;
	foreach ($result as $age => $val) {
		alt_table_row_color($k);
		label_cell($age);
		qty_cell($val, false, 0);
		end_row();
	}
	end_table();
}

$widget->End();