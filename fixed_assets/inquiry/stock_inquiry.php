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
$page_security = 'SA_ASSETSANALYTIC';
$path_to_root = "../..";
include($path_to_root . "/includes/db_pager.inc");
include($path_to_root . "/includes/session.inc");

include($path_to_root . "/purchasing/includes/purchasing_ui.inc");
include($path_to_root . "/reporting/includes/reporting.inc");
include($path_to_root . "/fixed_assets/includes/fixed_assets_db.inc");

$js = "";
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 500);
if (user_use_date_picker())
	$js .= get_js_date_picker();
page(_($help_context = "Fixed Assets Inquiry"), false, false, "", $js);

if (isset($_GET['location'])) 
{
	$_POST['location'] = $_GET['location'];
}

//------------------------------------------------------------------------------------------------

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();
//locations_list_cells(_("From Location:"), 'location', null, false, false, true);
check_cells( _("Show inactive:"), 'show_inactive', null);
submit_cells('RefreshInquiry', _("Search"),'',_('Refresh Inquiry'), 'default');
end_row();

end_table();

//------------------------------------------------------------------------------------------------

if(get_post('RefreshInquiry'))
{
	$Ajax->activate('totals_tbl');
}

function gl_view($row)
{
  	$row = get_fixed_asset_move($row['stock_id'], ST_JOURNAL);

	return get_gl_view_str(ST_JOURNAL, $row["trans_no"]);
}

function fa_prepare_row($row) {
  	$purchase = get_fixed_asset_purchase($row['stock_id']);
  	if ($purchase !== false) {
    	$row['purchase_date'] = $purchase['tran_date'];
    	$row['purchase_no'] = $purchase['trans_no'];
  	}
  	else {
    	$row['purchase_date'] = NULL;
    	$row['purchase_no'] = NULL;
  	}

  	$disposal = get_fixed_asset_disposal($row['stock_id']);
  	if ($disposal !== false) {
    	$row['disposal_date'] = $disposal['tran_date'];
    	$row['disposal_no'] = $disposal['trans_no'];
    	$row['disposal_type'] = $disposal['type'];
  	}
  	else {
    	$row['disposal_date'] = NULL;
    	$row['disposal_no'] = NULL;
    	$row['disposal_type'] = NULL;
  	}	
  	return $row;
}

function fa_link($row)
{
  	$url = "inventory/manage/items.php?FixedAsset=1&stock_id=".$row['stock_id'];

  	return viewer_link($row['stock_id'], $url);
}

function depr_method_title($row) {
  	global $depreciation_methods;
  	return $depreciation_methods[$row['depreciation_method']];
}

function depr_par($row) {
	if ($row['depreciation_method'] == 'D')
		return $row['depreciation_rate']*$row['depreciation_factor'].'%';
	elseif ($row['depreciation_method'] == 'N')
		return $row['depreciation_rate'].' '._('years'
		);
	else
		return $row['depreciation_rate'].'%';
}

function status_title($row) {

   	if ($row['inactive'] || ($row['disposal_date'] !== NULL))
		return _("Disposed"); // disposed or saled
	elseif ($row['purchase_date'] === NULL)
		return _("Purchasable"); // not yet purchased
    else
    	return _("Active");  // purchased

}

function purchase_link($row)
{

  	if ($row['purchase_date'] === NULL)
    	return "";

  	return get_supplier_trans_view_str(ST_SUPPRECEIVE, $row["purchase_no"], sql2date($row["purchase_date"]));
}

function disposal_link($row)
{
  	if ($row['disposal_date'] === NULL)
    	return "";

  	switch ($row['disposal_type']) {
    	case ST_INVADJUST:
      		return get_inventory_trans_view_str(ST_INVADJUST, $row["disposal_no"], sql2date($row["disposal_date"]));
    	case ST_CUSTDELIVERY:
	    	return get_customer_trans_view_str(ST_CUSTDELIVERY, $row["disposal_no"], sql2date($row["disposal_date"]));
    	default:
      		return "";
  	}
}

function amount_link($row)
{
    return price_format($row['purchase_cost']);
}

function depr_link($row)
{
    return price_format($row['purchase_cost'] - $row['material_cost']);
}

function balance_link($row)
{
    return price_format($row['material_cost']);
}


//------------------------------------------------------------------------------------------------

$sql = get_sql_for_fixed_assets(get_post('show_inactive'));

$cols = array(
			//_("Type") => array('fun'=>'systype_name', 'ord'=>''), 
			//_("#") => array('fun'=>'trans_view', 'ord'=>''), 
			_("#") => array('fun' => 'fa_link'), 
			_("Class"), 
			_("UOM") => array('align' => 'center'), 
			_("Description"),
			_("Rate or Lifecycle") => array('fun' => 'depr_par'), 
			_("Method") => array('fun' => 'depr_method_title'), 
			_("Status") => array('fun' => 'status_title'), 
			_("Purchased") => array('fun' => 'purchase_link'),
			_("Initial") => array('align'=>'right', 'fun' => 'amount_link'),
			_("Depreciations") => array('align'=>'right', 'fun' => 'depr_link'),
			_("Current") => array('align'=>'right', 'fun' => 'balance_link'),
			_("Liquidation or Sale") => array('align' => 'center', 'fun' => 'disposal_link'), 
			//array('insert'=>true, 'fun'=>'gl_view'),
			//array('insert'=>true, 'fun'=>'rm_link'),
			//array('insert'=>true, 'fun'=>'edit_link'),
			//array('insert'=>true, 'fun'=>'prt_link'),
			);

//------------------------------------------------------------------------------------------------

/*show a table of the transactions returned by the sql */
$table =& new_db_pager('fixed_assets_tbl', $sql, $cols);

$table->width = "85%";
$table->row_fun = "fa_prepare_row";

display_db_pager($table);

end_form();
end_page();
