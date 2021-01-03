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
$page_security = 'SA_SALESMAN';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");

page(_($help_context = "Sales Persons"));

include($path_to_root . "/includes/ui.inc");

simple_page_mode(true);
//------------------------------------------------------------------------------------------------

if ($Mode=='ADD_ITEM' || $Mode=='UPDATE_ITEM') 
{

	//initialise no input errors assumed initially before we test
	$input_error = 0;

	if (strlen($_POST['salesman_name']) == 0)
	{
		$input_error = 1;
		display_error(_("The sales person name cannot be empty."));
		set_focus('salesman_name');
	}
	$pr1 = check_num('provision', 0,100);
	if (!$pr1 || !check_num('provision2', 0, 100)) {
		$input_error = 1;
		display_error( _("Salesman provision cannot be less than 0 or more than 100%."));
		set_focus(!$pr1 ? 'provision' : 'provision2');
	}
	if (!check_num('break_pt', 0)) {
		$input_error = 1;
		display_error( _("Salesman provision breakpoint must be numeric and not less than 0."));
		set_focus('break_pt');
	}
	if ($input_error != 1)
	{
    	if ($selected_id != -1) 
    	{
    		/*selected_id could also exist if submit had not been clicked this code would not run in this case cos submit is false of course  see the delete code below*/
			update_salesman($selected_id, $_POST['salesman_name'], $_POST['salesman_phone'], $_POST['salesman_fax'],
				$_POST['salesman_email'], input_num('provision'), input_num('break_pt'), input_num('provision2'));
    	}
    	else
    	{
    		/*Selected group is null cos no item selected on first time round so must be adding a record must be submitting new entries in the new Sales-person form */
			add_salesman($_POST['salesman_name'], $_POST['salesman_phone'], $_POST['salesman_fax'],
				$_POST['salesman_email'], input_num('provision'), input_num('break_pt'), input_num('provision2'));
    	}

    	if ($selected_id != -1) 
			display_notification(_('Selected sales person data have been updated'));
		else
			display_notification(_('New sales person data have been added'));
		$Mode = 'RESET';
	}
}
if ($Mode == 'Delete')
{
	//the link to delete a selected record was clicked instead of the submit button

	// PREVENT DELETES IF DEPENDENT RECORDS IN 'debtors_master'

	if (key_in_foreign_table($selected_id, 'cust_branch', 'salesman'))
	{
		display_error(_("Cannot delete this sales-person because branches are set up referring to this sales-person - first alter the branches concerned."));
	}
	else
	{
		delete_salesman($selected_id);
		display_notification(_('Selected sales person data have been deleted'));
	}
	$Mode = 'RESET';
}

if ($Mode == 'RESET')
{
	$selected_id = -1;
	$sav = get_post('show_inactive');
	unset($_POST);
	$_POST['show_inactive'] = $sav;
}
//------------------------------------------------------------------------------------------------

$result = get_salesmen(check_value('show_inactive'));

start_form();
start_table(TABLESTYLE, "width='60%'");
$th = array(_("Name"), _("Phone"), _("Fax"), _("Email"), _("Provision"), _("Break Pt."), _("Provision")." 2", "", "");
inactive_control_column($th);
table_header($th);

$k = 0;

while ($myrow = db_fetch($result))
{

	alt_table_row_color($k);

    label_cell($myrow["salesman_name"]);
   	label_cell($myrow["salesman_phone"]);
   	label_cell($myrow["salesman_fax"]);
	email_cell($myrow["salesman_email"]);
	label_cell(percent_format($myrow["provision"])." %", "nowrap align=right");
   	amount_cell($myrow["break_pt"]);
	label_cell(percent_format($myrow["provision2"])." %", "nowrap align=right");
	inactive_control_cell($myrow["salesman_code"], $myrow["inactive"],
		'salesman', 'salesman_code');
 	edit_button_cell("Edit".$myrow["salesman_code"], _("Edit"));
 	delete_button_cell("Delete".$myrow["salesman_code"], _("Delete"));
  	end_row();

} //END WHILE LIST LOOP

inactive_control_row($th);
end_table();
echo '<br>';

//------------------------------------------------------------------------------------------------

$_POST['salesman_email'] = "";
if ($selected_id != -1) 
{
 	if ($Mode == 'Edit') {
		//editing an existing Sales-person
		$myrow = get_salesman($selected_id);

		$_POST['salesman_name'] = $myrow["salesman_name"];
		$_POST['salesman_phone'] = $myrow["salesman_phone"];
		$_POST['salesman_fax'] = $myrow["salesman_fax"];
		$_POST['salesman_email'] = $myrow["salesman_email"];
		$_POST['provision'] = percent_format($myrow["provision"]);
		$_POST['break_pt'] = price_format($myrow["break_pt"]);
		$_POST['provision2'] = percent_format($myrow["provision2"]);
	}
	hidden('selected_id', $selected_id);
} elseif ($Mode != 'ADD_ITEM') {
	$_POST['provision'] = percent_format(0);
	$_POST['break_pt'] = price_format(0);
	$_POST['provision2'] = percent_format(0);	
}

start_table(TABLESTYLE2);

text_row_ex(_("Sales person name:"), 'salesman_name', 30);
text_row_ex(_("Telephone number:"), 'salesman_phone', 20);
text_row_ex(_("Fax number:"), 'salesman_fax', 20);
email_row_ex(_("E-mail:"), 'salesman_email', 40);
percent_row(_("Provision").':', 'provision');
amount_row(_("Turnover Break Pt Level:"), 'break_pt');
percent_row(_("Provision")." 2:", 'provision2');
end_table(1);

submit_add_or_update_center($selected_id == -1, '', 'both');

end_form();

end_page();

