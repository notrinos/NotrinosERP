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
$page_security = 'SA_ITEMCATEGORY';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");

if (isset($_GET['FixedAsset'])) {
  $page_security = 'SA_ASSETCATEGORY';
  $help_context = "Fixed Assets Categories";
  $_POST['mb_flag'] = 'F';
}
else {
  $help_context = "Item Categories";
}

$js = "";
if ($SysPrefs->use_popup_windows && $SysPrefs->use_popup_search)
	$js .= get_js_open_window(900, 500);

page(_($help_context), false, false, "", $js);

include_once($path_to_root . "/includes/ui.inc");

include_once($path_to_root . "/inventory/includes/inventory_db.inc");

simple_page_mode(true);
//----------------------------------------------------------------------------------

if ($Mode=='ADD_ITEM' || $Mode=='UPDATE_ITEM') 
{

	//initialise no input errors assumed initially before we test
	$input_error = 0;

	if (strlen($_POST['description']) == 0) 
	{
		$input_error = 1;
		display_error(_("The item category description cannot be empty."));
		set_focus('description');
	}

	if ($input_error !=1)
	{
    	if ($selected_id != -1) 
    	{
		    update_item_category($selected_id, $_POST['description'],
				$_POST['tax_type_id'],	$_POST['sales_account'], 
				$_POST['cogs_account'], $_POST['inventory_account'], 
				$_POST['adjustment_account'], $_POST['wip_account'],
				$_POST['units'], $_POST['mb_flag'],	$_POST['dim1'],	$_POST['dim2'],
				check_value('no_sale'), check_value('no_purchase'));
			display_notification(_('Selected item category has been updated'));
    	} 
    	else 
    	{
		    add_item_category($_POST['description'],
				$_POST['tax_type_id'],	$_POST['sales_account'], 
				$_POST['cogs_account'], $_POST['inventory_account'], 
				$_POST['adjustment_account'], $_POST['wip_account'], 
				$_POST['units'], $_POST['mb_flag'],	$_POST['dim1'],	
				$_POST['dim2'],	check_value('no_sale'), check_value('no_purchase'));
			display_notification(_('New item category has been added'));
    	}
		$Mode = 'RESET';
	}
}

//---------------------------------------------------------------------------------- 

if ($Mode == 'Delete')
{

	// PREVENT DELETES IF DEPENDENT RECORDS IN 'stock_master'
	if (key_in_foreign_table($selected_id, 'stock_master', 'category_id'))
	{
		display_error(_("Cannot delete this item category because items have been created using this item category."));
	} 
	else 
	{
		delete_item_category($selected_id);
		display_notification(_('Selected item category has been deleted'));
	}
	$Mode = 'RESET';
}

if ($Mode == 'RESET')
{
	$selected_id = -1;
	$sav = get_post('show_inactive');
    $mb_flag = get_post('mb_flag');
	unset($_POST);
	$_POST['show_inactive'] = $sav;
	if (is_fixed_asset($mb_flag))
		$_POST['mb_flag'] = 'F';
}
if (list_updated('mb_flag')) {
	$Ajax->activate('details');
}

//----------------------------------------------------------------------------------
$fixed_asset = is_fixed_asset(get_post('mb_flag'));

$result = get_item_categories(check_value('show_inactive'), $fixed_asset);

start_form();
start_table(TABLESTYLE, "width='80%'");
if ($fixed_asset) {
	$th = array(_("Name"), _("Tax type"), _("Units"), _("Sales Act"),
		_("Asset Account"), _("Deprecation Cost Account"),
		_("Depreciation/Disposal Account"), "", "");
} else {
	$th = array(_("Name"), _("Tax type"), _("Units"), _("Type"), _("Sales Act"),
		_("Inventory Account"), _("COGS Account"), _("Adjustment Account"),
		_("Assembly Account"), "", "");
}
inactive_control_column($th);

table_header($th);
$k = 0; //row colour counter

while ($myrow = db_fetch($result)) 
{
	
	alt_table_row_color($k);

	label_cell($myrow["description"]);
	label_cell($myrow["tax_name"]);
	label_cell($myrow["dflt_units"], "align=center");
	if (!$fixed_asset)
		label_cell($stock_types[$myrow["dflt_mb_flag"]]);
	label_cell($myrow["dflt_sales_act"], "align=center");
	label_cell($myrow["dflt_inventory_act"], "align=center");
	label_cell($myrow["dflt_cogs_act"], "align=center");
	label_cell($myrow["dflt_adjustment_act"], "align=center");
	if (!$fixed_asset)
		label_cell($myrow["dflt_wip_act"], "align=center");
	inactive_control_cell($myrow["category_id"], $myrow["inactive"], 'stock_category', 'category_id');
 	edit_button_cell("Edit".$myrow["category_id"], _("Edit"));
 	delete_button_cell("Delete".$myrow["category_id"], _("Delete"));
	end_row();
}

inactive_control_row($th);
end_table();
echo '<br>';
//----------------------------------------------------------------------------------

div_start('details');
start_table(TABLESTYLE2);

if ($selected_id != -1) 
{
 	if ($Mode == 'Edit') {
		//editing an existing item category
		$myrow = get_item_category($selected_id);

		$_POST['category_id'] = $myrow["category_id"];
		$_POST['description']  = $myrow["description"];
		$_POST['tax_type_id']  = $myrow["dflt_tax_type"];
		$_POST['sales_account']  = $myrow["dflt_sales_act"];
		$_POST['cogs_account']  = $myrow["dflt_cogs_act"];
		$_POST['inventory_account']  = $myrow["dflt_inventory_act"];
		$_POST['adjustment_account']  = $myrow["dflt_adjustment_act"];
		$_POST['wip_account']  = $myrow["dflt_wip_act"];
		$_POST['units']  = $myrow["dflt_units"];
		$_POST['mb_flag']  = $myrow["dflt_mb_flag"];
		$_POST['dim1']  = $myrow["dflt_dim1"];
		$_POST['dim2']  = $myrow["dflt_dim2"];
		$_POST['no_sale']  = $myrow["dflt_no_sale"];
		$_POST['no_purchase']  = $myrow["dflt_no_purchase"];
	} 
	hidden('selected_id', $selected_id);
	hidden('category_id');
} else if ($Mode != 'CLONE') {
		$_POST['long_description'] = '';
		$_POST['description'] = '';
		$_POST['no_sale']  = 0;
		$_POST['no_purchase']  = 0;

		$company_record = get_company_prefs();

    if (get_post('inventory_account') == "")
    	$_POST['inventory_account'] = $company_record["default_inventory_act"];

    if (get_post('cogs_account') == "")
    	$_POST['cogs_account'] = $company_record["default_cogs_act"];

	if (get_post('sales_account') == "")
		$_POST['sales_account'] = $company_record["default_inv_sales_act"];

	if (get_post('adjustment_account') == "")
		$_POST['adjustment_account'] = $company_record["default_adj_act"];

	if (get_post('wip_account') == "")
		$_POST['wip_account'] = $company_record["default_wip_act"];

}

text_row(_("Category Name:"), 'description', null, 30, 30);  

table_section_title(_("Default values for new items"));

item_tax_types_list_row(_("Item Tax Type:"), 'tax_type_id', null);

if (is_fixed_asset(get_post('mb_flag')))
	hidden('mb_flag', 'F');
else
	stock_item_types_list_row(_("Item Type:"), 'mb_flag', null, true);

stock_units_list_row(_("Units of Measure:"), 'units', null);

if (is_fixed_asset($_POST['mb_flag'])) 
	hidden('no_sale', 0);
else
	check_row(_("Exclude from sales:"), 'no_sale');

check_row(_("Exclude from purchases:"), 'no_purchase');

gl_all_accounts_list_row(_("Sales Account:"), 'sales_account', $_POST['sales_account']);

if (is_service($_POST['mb_flag']))
{
	gl_all_accounts_list_row(_("C.O.G.S. Account:"), 'cogs_account', $_POST['cogs_account']);
	hidden('inventory_account', $_POST['inventory_account']);
	hidden('adjustment_account', $_POST['adjustment_account']);
}
elseif (is_fixed_asset($_POST['mb_flag'])) 
{
	gl_all_accounts_list_row(_("Asset account:"), 'inventory_account', $_POST['inventory_account']);
	gl_all_accounts_list_row(_("Depreciation cost account:"), 'cogs_account', $_POST['cogs_account']);
	gl_all_accounts_list_row(_("Depreciation/Disposal account:"), 'adjustment_account', $_POST['adjustment_account']);
}
else
{
	gl_all_accounts_list_row(_("Inventory Account:"), 'inventory_account', $_POST['inventory_account']);

	gl_all_accounts_list_row(_("C.O.G.S. Account:"), 'cogs_account', $_POST['cogs_account']);
	gl_all_accounts_list_row(_("Inventory Adjustments Account:"), 'adjustment_account', $_POST['adjustment_account']);
}

if (is_manufactured($_POST['mb_flag']))
	gl_all_accounts_list_row(_("Item Assembly Costs Account:"), 'wip_account', $_POST['wip_account']);
else
	hidden('wip_account', $_POST['wip_account']);

$dim = get_company_pref('use_dimension');
if ($dim >= 1)
{
	dimensions_list_row(_("Dimension")." 1", 'dim1', null, true, " ", false, 1);
	if ($dim > 1)
		dimensions_list_row(_("Dimension")." 2", 'dim2', null, true, " ", false, 2);
}
if ($dim < 1)
	hidden('dim1', 0);
if ($dim < 2)
	hidden('dim2', 0);

end_table(1);
div_end();
submit_add_or_update_center($selected_id == -1, '', 'both', true);

end_form();

end_page();

