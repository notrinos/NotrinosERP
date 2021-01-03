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
function db_has_customers()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."debtors_master");
}

function check_db_has_customers($msg)
{
	global $path_to_root;
    if (!db_has_customers()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;	
    }	
}

function db_has_currencies()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."currencies");
}

function check_db_has_currencies($msg)
{
	global $path_to_root;
    if (!db_has_currencies()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;
    }
}

function db_has_currency_rates($currency, $date_, $msg=false)
{
	$date = date2sql($date_);

	if (is_company_currency($currency))
		return 1;
	$ret = check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."exchange_rates WHERE curr_code = '$currency' && date_ <= '$date'");
	if ($ret == 0 && $msg)
		display_error(sprintf(_("Cannot retrieve exchange rate for currency %s as of %s. Please add exchange rate manually on Exchange Rates page."),
					 $currency, $date_), true);
	return $ret;
}

function db_has_sales_types()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."sales_types");
}

function check_db_has_sales_types($msg)
{
	global $path_to_root;
    if (!db_has_sales_types()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;	
    }	
}

function db_has_item_tax_types()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."item_tax_types");
}

function check_db_has_item_tax_types($msg)
{
	global $path_to_root;
    if (!db_has_item_tax_types()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;	
    }	
}

function db_has_tax_types()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."tax_types");
}

function check_db_has_tax_types($msg)
{
	global $path_to_root;
    if (!db_has_tax_types()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;	
    }	
}

function db_has_tax_groups()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."tax_groups");
}

function check_db_has_tax_groups($msg)
{
	global $path_to_root;
    if (!db_has_tax_groups()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;	
    }	
}

function db_customer_has_branches($customer_id)
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."cust_branch "
		."WHERE debtor_no=".db_escape($customer_id));
}

function db_has_customer_branches()
{
	return check_empty_result("SELECT COUNT(*) FROM "
		.TB_PREF."cust_branch WHERE !inactive");
}

function check_db_has_customer_branches($msg)
{
	global $path_to_root;
    if (!db_has_customer_branches()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;	
    }	
}

function db_has_sales_people()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."salesman");
}

function check_db_has_sales_people($msg)
{
	global $path_to_root;
    if (!db_has_sales_people()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;
    }
}

function db_has_sales_areas()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."areas");
}

function check_db_has_sales_areas($msg)
{
	global $path_to_root;
    if (!db_has_sales_areas()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;	
    }	
}

function db_has_shippers()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."shippers");
}

function check_db_has_shippers($msg)
{
	global $path_to_root;
    if (!db_has_shippers()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;	
    }	
}

function db_has_open_workorders()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."workorders WHERE closed=0");
}

function db_has_workorders()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."workorders");
}

function check_db_has_workorders($msg)
{
	global $path_to_root;
    if (!db_has_workorders()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;	
    }	
}

function db_has_open_dimensions()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."dimensions WHERE closed=0");
}

function db_has_dimensions()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."dimensions");
}

function check_db_has_dimensions($msg)
{
	global $path_to_root;
    if (!db_has_dimensions()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;	
    }	
}


function db_has_suppliers()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."suppliers");
}

function check_db_has_suppliers($msg)
{
	global $path_to_root;
    if (!db_has_suppliers()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;	
    }	
}

function db_has_stock_items()
{

	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."stock_master WHERE mb_flag!='F'");
}

function check_db_has_stock_items($msg)
{
	global $path_to_root;
    if (!db_has_stock_items()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;	
    }	
}

function db_has_bom_stock_items()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."stock_master WHERE mb_flag='M'");
}

function check_db_has_bom_stock_items($msg)
{
	global $path_to_root;
    if (!db_has_bom_stock_items()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;	
    }	
}

function db_has_manufacturable_items()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."stock_master WHERE (mb_flag='M')");
}

function check_db_has_manufacturable_items($msg)
{
	global $path_to_root;
    if (!db_has_manufacturable_items()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;	
    }	
}

function db_has_purchasable_items()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."stock_master WHERE mb_flag!='M'");
}

function check_db_has_purchasable_items($msg)
{
	global $path_to_root;
    if (!db_has_purchasable_items()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;	
    }	
}

function db_has_costable_items()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."stock_master WHERE mb_flag!='D'");
}

function check_db_has_costable_items($msg)
{
	global $path_to_root;
    if (!db_has_costable_items()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;	
    }	
}

function check_db_has_fixed_asset_classes($msg)
{
	global $path_to_root;
    if (!db_has_fixed_asset_classes()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;
    }
}

function db_has_fixed_asset_classes()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."stock_fa_class");
}

function db_has_depreciable_fixed_assets()
{
	$year = get_current_fiscalyear();
	$begin = date2sql(add_months(sql2date($year['begin']), -1));
	$end = date2sql(add_months(sql2date($year['end']), -1));

	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."stock_master 
	    WHERE mb_flag='F'
		    AND material_cost > 0
    		AND stock_id IN ( SELECT stock_id FROM ".TB_PREF."stock_moves WHERE type=".ST_SUPPRECEIVE." AND qty!=0 )
		    AND stock_id NOT IN	( SELECT stock_id FROM ".TB_PREF."stock_moves WHERE (type=".ST_CUSTDELIVERY." OR type=".ST_INVADJUST.") AND qty!=0 )
		    AND depreciation_date <= '".$end."'
    		AND depreciation_date >='".$begin."'");
}

function check_db_has_depreciable_fixed_assets($msg)
{
	global $path_to_root;

	if (!db_has_depreciable_fixed_assets()) 
	{
		display_error($msg, true);
		end_page();
	exit;
	}
}

function db_has_fixed_assets()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."stock_master WHERE mb_flag='F'");
}

function check_db_has_fixed_assets($msg)
{
	global $path_to_root;

	if (!db_has_fixed_assets()) 
	{
		display_error($msg, true);
		end_page();
		exit;
	}
}

function db_has_purchasable_fixed_assets()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."stock_master 
    	WHERE mb_flag='F'
    		AND !inactive
			AND stock_id NOT IN
				( SELECT stock_id FROM ".TB_PREF."stock_moves WHERE type=".ST_SUPPRECEIVE." AND qty!=0 )");
}

function check_db_has_purchasable_fixed_assets($msg)
{
	global $path_to_root;

	if (!db_has_purchasable_fixed_assets()) 
	{
		display_error($msg, true);
		end_page();
	 	exit;
    }
}

function db_has_disposable_fixed_assets()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."stock_master 
    	WHERE mb_flag='F'
		    AND !inactive
		    AND stock_id IN
				( SELECT stock_id FROM ".TB_PREF."stock_moves WHERE type=".ST_SUPPRECEIVE." AND qty!=0 )
		    AND stock_id NOT IN
				( SELECT stock_id FROM ".TB_PREF."stock_moves WHERE (type=".ST_CUSTDELIVERY." OR type=".ST_INVADJUST.") AND qty!=0 )");
}

function check_db_has_disposable_fixed_assets($msg)
{
	global $path_to_root;

	if (!db_has_disposable_fixed_assets()) 
	{
		display_error($msg, true);
		end_page();
		exit;
	}
}

function db_has_stock_categories()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."stock_category WHERE dflt_mb_flag!='F'");
}

function check_db_has_fixed_asset_categories($msg)
{
	global $path_to_root;
    if (!db_has_fixed_asset_categories()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;
    }
}

function db_has_fixed_asset_categories()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."stock_category WHERE dflt_mb_flag='F'");
}

function check_db_has_stock_categories($msg)
{
	global $path_to_root;
    if (!db_has_stock_categories()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;
    }
}

function db_has_workcentres()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."workcentres");
}

function check_db_has_workcentres($msg)
{
	global $path_to_root;
    if (!db_has_workcentres()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;	
    }	
}

function db_has_locations()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."locations WHERE fixed_asset=0");
}

function check_db_has_locations($msg)
{
	global $path_to_root;
    if (!db_has_locations()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;	
    }	
}

function db_has_bank_accounts()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."bank_accounts");
}

function check_db_has_bank_accounts($msg)
{
	global $path_to_root;

    if (!db_has_bank_accounts()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;	
    }	
}

function db_has_cash_accounts()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."bank_accounts
		WHERE account_type=3");
}

function db_has_gl_accounts()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."chart_master");
}

function db_has_gl_account_groups()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."chart_types");
}

function check_db_has_gl_account_groups($msg)
{
	global $path_to_root;
    if (!db_has_gl_account_groups()) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;	
    }	
}

function db_has_quick_entries()
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."quick_entries");
}

function db_has_tags($type)
{
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."tags WHERE type=".db_escape($type));
}

function check_db_has_tags($type, $msg)
{
	global $path_to_root;
    if (!db_has_tags($type)) 
    {
    	display_error($msg, true);
    	end_page();
    	exit;	
    }	
}

function check_empty_result($sql)
{
	$result = db_query($sql, "could not do check empty query");	
	
	$myrow = db_fetch_row($result);
	return $myrow[0] > 0;
}
//
//	Integer input check 
//	Return 1 if number has proper form and is within <min, max> range
//
function check_int($postname, $min=null, $max=null) {
	if(!isset($_POST[$postname]))
	  return 0;
    $num = input_num($postname);
    if(!is_int($num)) 
	  return 0;
    if (isset($min) && ($num<$min)) 
	  return 0;
    if (isset($max) && ($num>$max)) 
	  return 0;
    return 1;
}
//
//	Numeric input check.
//	Return 1 if number has proper form and is within <min, max> range
//	Empty/not defined fields are defaulted to $dflt value.
//
function check_num($postname, $min=null, $max=null, $dflt=0) {
	if(!isset($_POST[$postname]))
	  return 0;
    $num = input_num($postname, $dflt);
    if ($num === false || $num === null) 
	  return 0;
    if (isset($min) && ($num<$min)) 
	  return 0;
    if (isset($max) && ($num>$max)) 
	  return 0;
    return 1;
}

function check_is_closed($type, $type_no, $msg=null)
{
	global $systypes_array;

    if (($type_no > 0) && is_closed_trans($type, $type_no))
    {
    	if (!$msg)
    		$msg = sprintf(_("%s #%s is closed for further edition."), $systypes_array[$type], $type_no);
	   	display_error($msg, true);
		display_footer_exit();
	}
}

function check_db_has_template_orders($msg)
{
 	$sql  = "SELECT sorder.order_no 
 		FROM ".TB_PREF."sales_orders as sorder,"
 			.TB_PREF."sales_order_details as line
		WHERE sorder.order_no = line.order_no AND sorder.type = 1
		GROUP BY line.order_no";

    if (!check_empty_result($sql))
    {
    	display_error($msg, true);
    	end_page();
    	exit;
    }
}

function check_deferred_income_act($msg)
{
	global $path_to_root;

    if (!get_company_pref('deferred_income_act')) 
    {
    	display_error($msg, true);
		display_footer_exit();
    }
}

function check_is_editable($trans_type, $trans_no, $msg=null)
{
	if (!$_SESSION['wa_current_user']->can_access('SA_EDITOTHERSTRANS'))
	{
		$audit = get_audit_trail_last($trans_type, $trans_no);

		if ($_SESSION['wa_current_user']->user != $audit['user'])
		{
	    	if (!$msg)
		    	$msg = '<b>'._("You have no edit access to transactions created by other users.").'</b>';
			display_note($msg);
			display_footer_exit();
		}
	}
	if (!in_array($trans_type, array(ST_SALESORDER, ST_SALESQUOTE, ST_PURCHORDER, ST_WORKORDER)))
		check_is_closed($trans_type, $trans_no, $msg);
}

function check_reference($reference, $trans_type, $trans_no=0, $context=null, $line=null)
{
	global $Refs;

	if (!$Refs->is_valid($reference, $trans_type, $context, $line))
	{
		display_error(_("The entered reference is invalid.")); return false;
	}
	elseif (!$Refs->is_new_reference($reference, $trans_type, $trans_no))
	{
		display_error( _("The entered reference is already in use.")); return false;
	}
	return true;
}

function check_sys_pref($name, $msg, $empty = '')
{
	global $path_to_root;

	if (get_company_pref($name) === $empty)
	{
		display_error(menu_link("/admin/gl_setup.php", $msg), true);
		display_footer_exit();
	}
}
