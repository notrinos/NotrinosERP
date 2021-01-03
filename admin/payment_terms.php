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
$page_security = 'SA_PAYTERMS';
$path_to_root="..";
include($path_to_root . "/includes/session.inc");

page(_($help_context = "Payment Terms"));

include($path_to_root . "/includes/ui.inc");

simple_page_mode(true);

//------------------------------
//	Helper to translate record content to more intuitive form
//
function term_days($myrow)
{
	return $myrow["day_in_following_month"] != 0 ? $myrow["day_in_following_month"] :
		$myrow["days_before_due"];
}

function term_type($myrow)
{
	if ($myrow["day_in_following_month"] != 0)
		return PTT_FOLLOWING;

	$days = $myrow["days_before_due"];

	return $days < 0 ? PTT_PRE : ($days ? PTT_DAYS : PTT_CASH);
}

//-------------------------------------------------------------------------------------------

if ($Mode=='ADD_ITEM' || $Mode=='UPDATE_ITEM') 
{

	$input_error = 0;

	if (!is_numeric($_POST['DayNumber']))
	{
		$input_error = 1;
		display_error( _("The number of days or the day in the following month must be numeric."));
		set_focus('DayNumber');
	} 
	elseif (strlen($_POST['terms']) == 0) 
	{
		$input_error = 1;
		display_error( _("The Terms description must be entered."));
		set_focus('terms');
	}

	if ($_POST['DayNumber'] == '')
		$_POST['DayNumber'] = 0;

	if ($input_error != 1)
	{
		$type = get_post('type');
		$days = input_num('DayNumber');
		$from_now = ($type != PTT_FOLLOWING);
		if ($type == PTT_CASH)
			$days = 0;
		if ($type == PTT_PRE)
			$days = -1;

    	if ($selected_id != -1) 
    	{
    		update_payment_terms($selected_id, $from_now, $_POST['terms'], $days); 
 			$note = _('Selected payment terms have been updated');
    	} 
    	else 
    	{
			add_payment_terms($from_now, $_POST['terms'], $days);
			$note = _('New payment terms have been added');
    	}
    	//run the sql from either of the above possibilites
		display_notification($note);
 		$Mode = 'RESET';
	}
}

if ($Mode == 'Delete')
{
	// PREVENT DELETES IF DEPENDENT RECORDS IN debtors_master
	if (key_in_foreign_table($selected_id, 'debtors_master', 'payment_terms'))
	{
		display_error(_("Cannot delete this payment term, because customer accounts have been created referring to this term."));
	} 
	else 
	{
		if (key_in_foreign_table($selected_id, 'suppliers', 'payment_terms'))
		{
			display_error(_("Cannot delete this payment term, because supplier accounts have been created referring to this term"));
		} 
		else 
		{
			//only delete if used in neither customer or supplier accounts
			delete_payment_terms($selected_id);
			display_notification(_('Selected payment terms have been deleted'));
		}
	}
	//end if payment terms used in customer or supplier accounts
	$Mode = 'RESET';
}

if ($Mode == 'RESET')
{
	$selected_id = -1;
	$sav = get_post('show_inactive');
	unset($_POST);
	$_POST['show_inactive'] = $sav;
}
//-------------------------------------------------------------------------------------------------

$result = get_payment_terms_all(check_value('show_inactive'));

start_form();
start_table(TABLESTYLE);
$th = array(_("Description"), _("Type"), _("Due After/Days"), "", "");
inactive_control_column($th);
table_header($th);

$k = 0; //row colour counter
while ($myrow = db_fetch($result)) 
{

	alt_table_row_color($k);
	$type = term_type($myrow);
	$days = term_days($myrow);
    label_cell($myrow["terms"]);
    label_cell($pterm_types[$type]);
    label_cell($type == PTT_DAYS ? "$days "._("days") : ($type == PTT_FOLLOWING ? $days : _("N/A")));
	inactive_control_cell($myrow["terms_indicator"], $myrow["inactive"], 'payment_terms', "terms_indicator");
 	edit_button_cell("Edit".$myrow["terms_indicator"], _("Edit"));
 	delete_button_cell("Delete".$myrow["terms_indicator"], _("Delete"));
    end_row();

}

inactive_control_row($th);
end_table(1);

//-------------------------------------------------------------------------------------------------
if (list_updated('type')) {
	$Ajax->activate('edits');
}

div_start('edits');

start_table(TABLESTYLE2);

$day_in_following_month = $days_before_due = 0;
if ($selected_id != -1) 
{
	if ($Mode == 'Edit') {
		//editing an existing payment terms
		$myrow = get_payment_terms($selected_id);

		$_POST['terms']  = $myrow["terms"];
		$_POST['DayNumber'] = term_days($myrow);
		$_POST['type'] = term_type($myrow);
	}
	hidden('selected_id', $selected_id);
}

text_row(_("Terms Description:"), 'terms', null, 40, 40);

payment_type_list_row(_("Payment type:"), 'type', null, true);

if ( in_array(get_post('type'), array(PTT_FOLLOWING, PTT_DAYS))) 
	text_row_ex(_("Days (Or Day In Following Month):"), 'DayNumber', 3);
else
	hidden('DayNumber', 0);

end_table(1);
div_end();

submit_add_or_update_center($selected_id == -1, '', 'both');

end_form();

end_page();

