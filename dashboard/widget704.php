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

$weeks = 5;
$today = Today();

$result = gl_performance($today, $weeks);

$title = sprintf(_("Last %s weeks Performance"), $weeks);

$i = 0;
while ($myrow = db_fetch($result)) {
	$pg->x[$i] = $myrow['week_name']; 
	$pg->y[$i] = $myrow['sales'];
	$pg->z[$i] = $myrow['costs'];
	$i++;
}

$widget = new Widget();
$widget->setTitle($title);
$widget->Start();

if($widget->checkSecurity('SA_GLANALYTIC'))
	source_graphic($title, _('Week'), $pg, _('Sales'), _('Costs'), 1);

$widget->End();