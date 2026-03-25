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

$pg = new graph();
$result = hrm_employee_age_distribution();
$title = _('Employees by Age');

if ($pg != null) {
	foreach ($result as $name => $val) {
		$pg->x[] = $name;
		$pg->y[] = abs($val);
	}
}

$widget = new Widget();
$widget->setTitle($title);
$widget->Start();

if ($widget->checkSecurity('SA_EMPLOYEE'))
	source_graphic($title, _('Age Range'), $pg, _('Employees'), null, 5);

$widget->End();
