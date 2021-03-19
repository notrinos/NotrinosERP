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
$result = get_recurrent_invoices($today);
$title = _('Overdue Recurrent Invoices');

$widget = new Widget();
$widget->setTitle($title);
$widget->Start();

$th = array(_('Description'), _('Template No'), _('Customer'), _('Branch').'/'._('Group'), _('Next invoice'));
start_table(TABLESTYLE, "width='100%'");
table_header($th);

$k = 0;
while ($myrow = db_fetch($result)) {
	if (!$myrow['overdue'])
		continue;
	alt_table_row_color($k);

	label_cell($myrow['description']);
	label_cell(get_customer_trans_view_str(ST_SALESORDER, $myrow['order_no']));
	if ($myrow['debtor_no'] == 0) {
		label_cell('');
		label_cell(get_sales_group_name($myrow['group_no']));
	}
	else {
		label_cell(get_customer_name($myrow['debtor_no']));
		label_cell(get_branch_name($myrow['group_no']));
	}
	label_cell(calculate_next_invoice($myrow), "align='center'");
	end_row();
}
end_table();

$widget->End();