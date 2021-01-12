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
$limit = 10;
$today = Today();

$result = supplier_top($today, $limit);

$title = sprintf(_("Top %s suppliers in fiscal year"), $limit);

$widget = new Widget();
$widget->setTitle($title);

$widget->Start();

if($widget->checkSecurity('SA_SUPPTRANSVIEW')) {
	$th = array(_('Supplier'), _('Amount'));
	start_table(TABLESTYLE, "width='$width%'");
	table_header($th);
	$k = 0;
	while ($myrow = db_fetch($result)) {
		alt_table_row_color($k);
		$name = $myrow['supplier_id'].' '.$myrow['supp_name'];
		label_cell($name);
		amount_cell($myrow['total']);
		end_row();
	}
	end_table();
}

$widget->End();