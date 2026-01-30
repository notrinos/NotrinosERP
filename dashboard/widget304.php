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

$title = _('Items Are Running Low');

$widget = new Widget();
$widget->setTitle($title);
$widget->Start();

if($widget->checkSecurity('SA_ITEMSTRANSVIEW')) {
	$today = Today();
	$result = get_low_stock($today);
	
	$th = array(_('Item Code'), _('Item Name'), _('Stock Location'), _('Reorder Level'), _('QTY On Hand'));
	start_table(TABLESTYLE, "width='100%'");
	table_header($th);
	$k = 0;
	while ($myrow = db_fetch($result)) {
		alt_table_row_color($k);
		label_cell($myrow['stock_id']);
		label_cell($myrow['description']);
		label_cell($myrow['location_name']);
		qty_cell($myrow['reorder_level']);
        qty_cell($myrow['QtyOnHand']);
		end_row();
	}
	end_table();
}

$widget->End();
