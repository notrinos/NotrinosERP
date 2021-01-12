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

$today = Today();
$result = customer_trans($today);
$num = db_num_rows($result);
$title = sprintf(_("%s overdue Sales Invoices"), $num);

$widget = new Widget();
$widget->setTitle($title);
$widget->Start();

$th = array('#', _('Ref.'), _('Date'), _('Due Date'), _('Customer'), _('Branch'), _('Currency'), _('Total'), _('Remainder'),	_('Days'));

if($widget->checkSecurity('SA_SALESTRANSVIEW')) {

	start_table(TABLESTYLE, "width='100%'");
	table_header($th);
	$k = 0;
	while ($myrow = db_fetch($result)) {
		alt_table_row_color($k);
		label_cell(get_trans_view_str(ST_SALESINVOICE, $myrow["trans_no"]));
		label_cell($myrow['reference']);
		label_cell(sql2date($myrow['tran_date']));
		label_cell(sql2date($myrow['due_date']));
		$name = $myrow['debtor_no'].' '.$myrow['name'];
		label_cell($name);
		label_cell($myrow['br_name']);
		label_cell($myrow['curr_code']);
		amount_cell($myrow['total']);
		amount_cell($myrow['remainder']);
		label_cell($myrow['days'], "align='right'");
		end_row();
	}
	end_table();
}

$widget->End();