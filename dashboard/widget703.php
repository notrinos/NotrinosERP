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
$today = Today();

$result = gl_top($today);

$title = _('Class Balances');

$i = 0;
while ($myrow = db_fetch($result)) {

	if ($myrow['ctype'] > 3) {
		$total += $myrow['total'];
		$myrow['total'] = -$myrow['total'];
		if ($pg != null) {
			$pg->x[$i] = $myrow['class_name']; 
			$pg->y[$i] = abs($myrow['total']);
		}	
		$i++;
	}
}
if ($pg != null) {
	$pg->x[$i] = $calculated; 
	$pg->y[$i] = -$total;
}

$widget = new Widget();
$widget->setTitle($title);
$widget->Start();

if($widget->checkSecurity('SA_GLANALYTIC'))
	source_graphic($title, _('Class'), $pg, _('Amount'), null, 5);

$widget->End();