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

$today = date2sql(Today());

$sql = "SELECT bank_act, bank_account_name, bank_curr_code, SUM(amount) balance FROM ".TB_PREF."bank_trans bt INNER JOIN ".TB_PREF."bank_accounts ba ON bt.bank_act = ba.id WHERE trans_date <= '$today' AND inactive <> 1 GROUP BY bank_act, bank_account_name ORDER BY bank_account_name";

$result = db_query($sql);

$title = _('Bank Account Balances');

$widget = new Widget();
$widget->setTitle($title);
$widget->Start();

$th = array(_('Account'), _('Currency'), _('Balance'));

if($widget->checkSecurity('SA_GLANALYTIC')) {

	start_table(TABLESTYLE, "width='100%'");
	table_header($th);
	$k = 0;
	while ($myrow = db_fetch($result)) {
		alt_table_row_color($k);
		label_cell(viewer_link($myrow['bank_account_name'], 'gl/inquiry/bank_inquiry.php?bank_account='.$myrow['bank_act']));
		label_cell($myrow['bank_curr_code']);
		amount_cell($myrow['balance']);
		end_row();
	}
	end_table();
}

$widget->End();