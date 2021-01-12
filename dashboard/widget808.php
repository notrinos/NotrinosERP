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
$today = Today();

$result = gl_top($today);

$title = _('Class Balances');

$widget = new Widget();
$widget->setTitle($title);
$widget->Start();

if($widget->checkSecurity('SA_GLANALYTIC')) {

	start_table(TABLESTYLE2, "width='$width%'");
	$total = 0;
	while ($myrow = db_fetch($result)) {
		if ($myrow['ctype'] > 3) {
			$total += $myrow['total'];
			$myrow['total'] = -$myrow['total'];
		}	
		label_row($myrow['class_name'], number_format2($myrow['total'], user_price_dec()), 
			"class='label' style='font-weight:bold;'", "style='font-weight:bold;' align=right");
	}
	$calculated = _('Calculated Return');
	label_row('&nbsp;', '');
	label_row($calculated, number_format2(-$total, user_price_dec()), "class='label' style='font-weight:bold;'", "style='font-weight:bold;' align=right");

	end_table();
}

$widget->End();