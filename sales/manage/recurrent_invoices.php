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
$page_security = 'SA_SRECURRENT';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include($path_to_root . "/includes/ui.inc");
include_once($path_to_root . "/sales/includes/sales_db.inc");

$js = "";
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 600);
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = "Recurrent Invoices"), false, false, "", $js);

check_db_has_template_orders(_("There is no template order in database.
	You have to create at least one sales order marked as template to be able to define recurrent invoices."));

simple_page_mode(true);

if ($Mode=='ADD_ITEM' || $Mode=='UPDATE_ITEM') 
{

	$input_error = 0;

	if (!get_post('group_no'))
	{
		$input_error = 1;
		if (get_post('debtor_no'))
			display_error(_("This customer has no branches. Please define at least one branch for this customer first."));
		else
			display_error(_("There are no tax groups defined in the system. At least one tax group is required before proceeding."));
		set_focus('debtor_no');
	}
	if (strlen($_POST['description']) == 0) 
	{
		$input_error = 1;
		display_error(_("The invoice description cannot be empty."));
		set_focus('description');
	}
	if (!check_recurrent_invoice_description($_POST['description'], $selected_id))
	{
		$input_error = 1;
		display_error(_("This recurrent invoice description is already in use."));
		set_focus('description');
	}
	if (!is_date($_POST['begin']))
	{
		$input_error = 1;
		display_error(_("The entered date is invalid."));
		set_focus('begin');
	}
	if (!is_date($_POST['end']))
	{
		$input_error = 1;
		display_error(_("The entered date is invalid."));
		set_focus('end');
	}
	if (isset($_POST['last_sent']) && !is_date($_POST['last_sent'])) {
		$input_error = 1;
		display_error(_("The entered date is invalid."));
		set_focus('last_sent');
	}
	if (!$_POST['days'] && !$_POST['monthly'])
	{
		$input_error = 1;
		display_error(_("No recurence interval has been entered."));
		set_focus('days');
	}

	if ($input_error != 1)
	{
    	if ($selected_id != -1) 
    	{
    		update_recurrent_invoice($selected_id, $_POST['description'], $_POST['order_no'], input_num('debtor_no'), 
    			input_num('group_no'), input_num('days', 0), input_num('monthly', 0), $_POST['begin'], $_POST['end']);
    		if (isset($_POST['last_sent']))	
				update_last_sent_recurrent_invoice($selected_id, $_POST['last_sent']);
			$note = _('Selected recurrent invoice has been updated');
    	} 
    	else 
    	{
    		add_recurrent_invoice($_POST['description'], $_POST['order_no'], input_num('debtor_no'), input_num('group_no'),
    			input_num('days', 0), input_num('monthly', 0), $_POST['begin'], $_POST['end']);
			$note = _('New recurrent invoice has been added');
    	}
    
		display_notification($note);
		$Mode = 'RESET';
	}
} 

if ($Mode == 'Delete')
{

	$cancel_delete = 0;

	if ($cancel_delete == 0) 
	{
		delete_recurrent_invoice($selected_id);

		display_notification(_('Selected recurrent invoice has been deleted'));
	} //end if Delete area
	$Mode = 'RESET';
} 

if ($Mode == 'RESET')
{
	$selected_id = -1;
	unset($_POST);
}
//-------------------------------------------------------------------------------------------------

$result = get_recurrent_invoices();

start_form();
start_table(TABLESTYLE, "width=70%");
$th = array(_("Description"), _("Template No"),_("Customer"),_("Branch")."/"._("Group"),_("Days"),_("Monthly"),_("Begin"),_("End"),_("Last Created"),"", "");
table_header($th);
$k = 0;
while ($myrow = db_fetch($result)) 
{
	$begin = sql2date($myrow["begin"]);
	$end = sql2date($myrow["end"]);
	$last_sent = $myrow["last_sent"] == '0000-00-00' ? '' : sql2date($myrow["last_sent"]);
	
	alt_table_row_color($k);
		
	label_cell($myrow["description"]);
	label_cell(get_customer_trans_view_str(ST_SALESORDER, $myrow["order_no"]), "nowrap align='right'");
	if ($myrow["debtor_no"] == 0)
	{
		label_cell("");
		label_cell(get_sales_group_name($myrow["group_no"]));
	}	
	else
	{
		label_cell(get_customer_name($myrow["debtor_no"]));
		label_cell(get_branch_name($myrow['group_no']));
	}	
	label_cell($myrow["days"]);
	label_cell($myrow['monthly']);
	label_cell($begin);
	label_cell($end);
	label_cell($last_sent);
 	edit_button_cell("Edit".$myrow["id"], _("Edit"));
 	delete_button_cell("Delete".$myrow["id"], _("Delete"));
 	end_row();
}
end_table();

end_form();
echo '<br>';

//-------------------------------------------------------------------------------------------------

start_form();

start_table(TABLESTYLE2);

if ($selected_id != -1) 
{
 	if ($Mode == 'Edit') {
		//editing an existing area
		$myrow = get_recurrent_invoice($selected_id);

		$_POST['description']  = $myrow["description"];
		$_POST['order_no']  = $myrow["order_no"];
		$_POST['debtor_no']  = $myrow["debtor_no"];
		$_POST['group_no']  = $myrow["group_no"];
		$_POST['days']  = $myrow["days"];
		$_POST['monthly']  = $myrow["monthly"];
		$_POST['begin']  = sql2date($myrow["begin"]);
		$_POST['end']  = sql2date($myrow["end"]);
		$_POST['last_sent']  = ($myrow['last_sent']=="0000-00-00"?"":sql2date($myrow["last_sent"]));
	} 
	hidden("selected_id", $selected_id);
}


text_row_ex(_("Description:"), 'description', 50); 

templates_list_row(_("Template:"), 'order_no');

customer_list_row(_("Customer:"), 'debtor_no', null, " ", true);

if ($_POST['debtor_no'] > 0)
	customer_branches_list_row(_("Branch:"), $_POST['debtor_no'], 'group_no', null, false);
else	
	sales_groups_list_row(_("Sales Group:"), 'group_no', null);

small_amount_row(_("Days:"), 'days', 0, null, null, 0);

small_amount_row(_("Monthly:"), 'monthly', 0, null, null, 0);

date_row(_("Begin:"), 'begin');

date_row(_("End:"), 'end', null, null, 0, 0, 5);

if ($selected_id != -1 && @$_POST['last_sent'] != "")
	date_row(_("Last Created"), 'last_sent');

end_table(1);

submit_add_or_update_center($selected_id == -1, '', 'both');

end_form();

end_page();
?>
