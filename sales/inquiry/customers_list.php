<?php
/**********************************************************************
    Copyright (C) FrontAccounting, LLC.
	Released under the terms of the GNU General Public License, GPL, 
	as published by the Free Software Foundation, either version 3 
	of the License, or (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
    See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/
/**********************************************************************
  Page for searching customer list and select it to customer selection
  in pages that have the supplier dropdown lists.
  Author: bogeyman2007 from Discussion Forum. Modified by Joe Hunt
***********************************************************************/
$page_security = "SA_SALESORDER";
$path_to_root = "../..";
include_once($path_to_root . "/includes/session.inc");
include_once($path_to_root . "/includes/ui.inc");
include_once($path_to_root . "/sales/includes/db/customers_db.inc");

$mode = get_company_pref('no_customer_list');
if ($mode != 0)
	$js = get_js_set_combo_item();
else
	$js = get_js_select_combo_item();

page(_($help_context = "Customers"), true, false, "", $js);

if(get_post("search")) {
  $Ajax->activate("customer_tbl");
}

start_form(false, false, $_SERVER['PHP_SELF'] . "?" . $_SERVER['QUERY_STRING']);

start_table(TABLESTYLE_NOBORDER);

start_row();

text_cells(_("Customer"), "customer");
submit_cells("search", _("Search"), "", _("Search customers"), "default");

end_row();

end_table();

end_form();

div_start("customer_tbl");

start_table(TABLESTYLE);

$th = array("", _("Customer"), _("Short Name"), _("Address"), _("Tax ID"));

table_header($th);

$k = 0;
$name = $_GET["client_id"];
$result = get_customers_search(get_post("customer"));
while ($myrow = db_fetch_assoc($result)) {
	alt_table_row_color($k);
	$value = $myrow['debtor_no'];
	if ($mode != 0) {
		$text = $myrow['name'];
  		ahref_cell(_("Select"), 'javascript:void(0)', '', 'setComboItem(window.opener.document, "'.$name.'",  "'.$value.'", "'.$text.'")');
	}
	else {
  		ahref_cell(_("Select"), 'javascript:void(0)', '', 'selectComboItem(window.opener.document, "'.$name.'", "'.$value.'")');
	}
  	label_cell($myrow["name"]);
  	label_cell($myrow["debtor_ref"]);
  	label_cell($myrow["address"]);
  	label_cell($myrow["tax_id"]);
	end_row();
}

end_table(1);

div_end();

end_page(true);
