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
//include_once($path_to_root . "/includes/reserved.inc");

function set_global_supplier($supplier_id) 
{
	$_SESSION['wa_global_supplier_id'] = $supplier_id; 
}

function get_global_supplier($return_all=true) 
{
	if (!isset($_SESSION['wa_global_supplier_id']) || 
		($return_all == false && $_SESSION['wa_global_supplier_id'] == ALL_TEXT))
		return "";	
	return $_SESSION['wa_global_supplier_id'];
}

function set_global_stock_item($stock_id) 
{
	$_SESSION['wa_global_stock_id'] = $stock_id; 
}

function get_global_stock_item($return_all=true) 
{
	if (!isset($_SESSION['wa_global_stock_id']) || 
		($return_all == false && $_SESSION['wa_global_stock_id'] == ALL_TEXT))
		return "";		
	return $_SESSION['wa_global_stock_id'];
}

function set_global_customer($customer_id) 
{
	$_SESSION['wa_global_customer_id'] = $customer_id; 
}

function get_global_customer($return_all=true) 
{
	if (!isset($_SESSION['wa_global_customer_id']) || 
		($return_all == false && $_SESSION['wa_global_customer_id'] == ALL_TEXT))
		return "";
	return $_SESSION['wa_global_customer_id'];
}

function set_global_curr_code($curr_code) 
{
	$_SESSION['wa_global_curr_code'] = $curr_code;
}

function get_global_curr_code() 
{
	if (!isset($_SESSION['wa_global_curr_code']))
		return "";
	return $_SESSION['wa_global_curr_code'];
}

function count_array($array)
{
	return (is_array($array)) ? count($array) : (($array === NULL) ? 0 : 1);
}

//--------------------------------------------------------------------------------------

