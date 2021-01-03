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
$page_security = 'SA_SALESTYPES';
$path_to_root = "../..";
include_once($path_to_root . "/includes/session.inc");

page(_($help_context = "Sales Types"));

include_once($path_to_root . "/includes/ui.inc");
include_once($path_to_root . "/sales/includes/db/sales_types_db.inc");

simple_page_mode(true);
//----------------------------------------------------------------------------------------------------

function can_process()
{
	if (strlen($_POST['sales_type']) == 0)
	{
		display_error(_("The sales type description cannot be empty."));
		set_focus('sales_type');
		return false;
	}

	if (!check_num('factor', 0))
	{
		display_error(_("Calculation factor must be valid positive number."));
		set_focus('factor');
		return false;
	}
	return true;
}

//----------------------------------------------------------------------------------------------------

if ($Mode=='ADD_ITEM' && can_process())
{
	add_sales_type($_POST['sales_type'], check_value('tax_included'),
	    input_num('factor'));
	display_notification(_('New sales type has been added'));
	$Mode = 'RESET';
}

//----------------------------------------------------------------------------------------------------

if ($Mode=='UPDATE_ITEM' && can_process())
{

	update_sales_type($selected_id, $_POST['sales_type'], check_value('tax_included'),
	     input_num('factor'));
	display_notification(_('Selected sales type has been updated'));
	$Mode = 'RESET';
}

//----------------------------------------------------------------------------------------------------

if ($Mode == 'Delete')
{
	// PREVENT DELETES IF DEPENDENT RECORDS IN 'debtor_trans'
	
	if (key_in_foreign_table($selected_id, 'debtor_trans', 'tpe'))
	{
		display_error(_("Cannot delete this sale type because customer transactions have been created using this sales type."));

	}
	else
	{
		if (key_in_foreign_table($selected_id, 'debtors_master', 'sales_type'))
		{
			display_error(_("Cannot delete this sale type because customers are currently set up to use this sales type."));
		}
		else
		{
			delete_sales_type($selected_id);
			display_notification(_('Selected sales type has been deleted'));
		}
	} //end if sales type used in debtor transactions or in customers set up
	$Mode = 'RESET';
}

if ($Mode == 'RESET')
{
	$selected_id = -1;
	$sav = get_post('show_inactive');
	unset($_POST);
	$_POST['show_inactive'] = $sav;
}
//----------------------------------------------------------------------------------------------------

$result = get_all_sales_types(check_value('show_inactive'));

start_form();
start_table(TABLESTYLE, "width='30%'");

$th = array (_('Type Name'), _('Factor'), _('Tax Incl'), '','');
inactive_control_column($th);
table_header($th);
$k = 0;
$base_sales = get_base_sales_type();

while ($myrow = db_fetch($result))
{
	if ($myrow["id"] == $base_sales)
	    start_row("class='overduebg'");
	else
	    alt_table_row_color($k);
	label_cell($myrow["sales_type"]);
	$f = number_format2($myrow["factor"],4);
	if($myrow["id"] == $base_sales) $f = "<I>"._('Base')."</I>";
	label_cell($f);
	label_cell($myrow["tax_included"] ? _('Yes'):_('No'), 'align=center');
	inactive_control_cell($myrow["id"], $myrow["inactive"], 'sales_types', 'id');
 	edit_button_cell("Edit".$myrow['id'], _("Edit"));
 	delete_button_cell("Delete".$myrow['id'], _("Delete"));
	end_row();
}
inactive_control_row($th);
end_table();

display_note(_("Marked sales type is the company base pricelist for prices calculations."), 0, 0, "class='overduefg'");

//----------------------------------------------------------------------------------------------------

 if (!isset($_POST['tax_included']))
	$_POST['tax_included'] = 0;
 if (!isset($_POST['base']))
	$_POST['base'] = 0;

start_table(TABLESTYLE2);

if ($selected_id != -1)
{

 	if ($Mode == 'Edit') {
		$myrow = get_sales_type($selected_id);

		$_POST['sales_type']  = $myrow["sales_type"];
		$_POST['tax_included']  = $myrow["tax_included"];
		$_POST['factor']  = number_format2($myrow["factor"],4);
	}
	hidden('selected_id', $selected_id);
} else {
		$_POST['factor']  = number_format2(1,4);
}

text_row_ex(_("Sales Type Name").':', 'sales_type', 20);
amount_row(_("Calculation factor").':', 'factor', null, null, null, 4);
check_row(_("Tax included").':', 'tax_included', $_POST['tax_included']);

end_table(1);

submit_add_or_update_center($selected_id == -1, '', 'both');

end_form();

end_page();

