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

$width = 100;
$limit = 10;
$today = Today();

$result = dimension_top($today, $limit);

$title = sprintf(_("Top %s Dimensions in fiscal year"), $limit);

$i = 0;
while ($myrow = db_fetch($result)) {

	$name = $myrow['reference'].' '.$myrow['name'];
	if ($pg != NULL) {
		$pg->x[$i] = $name; 
		$pg->y[$i] = abs($myrow['total']);
	}	
	$i++;
}

$widget = new Widget();
$widget->setTitle($title);
$widget->Start();

if($widget->checkSecurity('SA_DIMTRANSVIEW'))
	source_graphic($title, _('Dimension'), $pg, _('Performance'), null, 5);

$widget->End();