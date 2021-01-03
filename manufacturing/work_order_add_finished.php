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
$page_security = 'SA_MANUFRECEIVE';
$path_to_root = "..";
include_once($path_to_root . "/includes/session.inc");
include_once($path_to_root . "/includes/inventory.inc");

include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/gl/includes/db/gl_db_bank_trans.inc");

include_once($path_to_root . "/manufacturing/includes/manufacturing_db.inc");
include_once($path_to_root . "/manufacturing/includes/manufacturing_ui.inc");

$js = "";
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 500);
if (user_use_date_picker())
	$js .= get_js_date_picker();
page(_($help_context = "Produce or Unassemble Finished Items From Work Order"), false, false, "", $js);

if (isset($_GET['trans_no']) && $_GET['trans_no'] != "")
{
	$_POST['selected_id'] = $_GET['trans_no'];
}

//--------------------------------------------------------------------------------------------------

if (isset($_GET['AddedID']))
{
	include_once($path_to_root . "/reporting/includes/reporting.inc");
	$id = $_GET['AddedID'];
	$stype = ST_WORKORDER;

	display_notification(_("The manufacturing process has been entered."));
	
    display_note(get_trans_view_str($stype, $id, _("View this Work Order")));

   	display_note(get_gl_view_str($stype, $id, _("View the GL Journal Entries for this Work Order")), 1);
   	$ar = array('PARAM_0' => $_GET['date'], 'PARAM_1' => $_GET['date'], 'PARAM_2' => $stype); 
   	display_note(print_link(_("Print the GL Journal Entries for this Work Order"), 702, $ar), 1);

	hyperlink_no_params("search_work_orders.php", _("Select another &Work Order to Process"));
	br();

	end_page();
	exit;
}

//--------------------------------------------------------------------------------------------------

$wo_details = get_work_order($_POST['selected_id'], true);

if ($wo_details === false)
{
	display_error(_("The order number sent is not valid."));
	exit;
}

//--------------------------------------------------------------------------------------------------

function can_process($wo_details)
{
	global $SysPrefs;

	if (!check_reference($_POST['ref'], ST_MANURECEIVE))
	{
		set_focus('ref');
		return false;
	}

	if (!check_num('quantity', 0))
	{
		display_error(_("The quantity entered is not a valid number or less then zero."));
		set_focus('quantity');
		return false;
	}

	if (!is_date($_POST['date_']))
	{
		display_error(_("The entered date is invalid."));
		set_focus('date_');
		return false;
	}
	elseif (!is_date_in_fiscalyear($_POST['date_']))
	{
		display_error(_("The entered date is out of fiscal year or is closed for further data entry."));
		set_focus('date_');
		return false;
	}
	if (date_diff2(sql2date($wo_details["released_date"]), $_POST['date_'], "d") > 0)
	{
		display_error(_("The production date cannot be before the release date of the work order."));
		set_focus('date_');
		return false;
	}
	// don't produce more that required. Otherwise change the Work Order.
	if (input_num('quantity') > ($wo_details["units_reqd"] - $wo_details["units_issued"]))
	{
		display_error(_("The production exceeds the quantity needed. Please change the Work Order."));
		set_focus('quantity');
		return false;
	}
	// if unassembling we need to check the qoh
	if (($_POST['ProductionType'] == 0) && !$SysPrefs->allow_negative_stock())
	{
		if (check_negative_stock($wo_details["stock_id"], -input_num('quantity'), $wo_details["loc_code"], $_POST['date_']))
		{
			display_error(_("The unassembling cannot be processed because there is insufficient stock."));
			set_focus('quantity');
			return false;
		}
	}

	// if production we need to check the qoh of the wo requirements
	if (($_POST['ProductionType'] == 1) && !$SysPrefs->allow_negative_stock())
	{
    	$err = false;
    	$result = get_wo_requirements($_POST['selected_id']);
		while ($row = db_fetch($result))
		{
			if ($row['mb_flag'] == 'D') // service, non stock
				continue;

			if (check_negative_stock($row["stock_id"], -$row['units_req'] * input_num('quantity'), $row["loc_code"], $_POST['date_']))
			{
    			display_error( _("The production cannot be processed because a required item would cause a negative inventory balance :") .
    				" " . $row['stock_id'] . " - " .  $row['description']);
    			$err = true;
			}
		}
		if ($err)
		{
			set_focus('quantity');
			return false;
		}	
	}
	return true;
}

//--------------------------------------------------------------------------------------------------

if ((isset($_POST['Process']) || isset($_POST['ProcessAndClose'])) && can_process($wo_details) == true)
{

	$close_wo = 0;
	if (isset($_POST['ProcessAndClose']) && ($_POST['ProcessAndClose']!=""))
		$close_wo = 1;

	// if unassembling, negate quantity
	if ($_POST['ProductionType'] == 0)
		$_POST['quantity'] = -$_POST['quantity'];

	 $id = work_order_produce($_POST['selected_id'], $_POST['ref'], input_num('quantity'),
			$_POST['date_'], $_POST['memo_'], $close_wo);

	meta_forward($_SERVER['PHP_SELF'], "AddedID=".$_POST['selected_id']."&date=".$_POST['date_']);
}

//-------------------------------------------------------------------------------------

display_wo_details($_POST['selected_id']);

//-------------------------------------------------------------------------------------

start_form();

hidden('selected_id', $_POST['selected_id']);

$dec = get_qty_dec($wo_details["stock_id"]);
if (!isset($_POST['quantity']) || $_POST['quantity'] == '')
	$_POST['quantity'] = qty_format(max($wo_details["units_reqd"] - $wo_details["units_issued"], 0), $wo_details["stock_id"], $dec);

start_table(TABLESTYLE2);
br();

date_row(_("Date:"), 'date_');
ref_row(_("Reference:"), 'ref', '', $Refs->get_next(ST_MANURECEIVE, null, get_post('date_')), false, ST_MANURECEIVE);

if (!isset($_POST['ProductionType']))
	$_POST['ProductionType'] = 1;

yesno_list_row(_("Type:"), 'ProductionType', $_POST['ProductionType'],
	_("Produce Finished Items"), _("Return Items to Work Order"));

small_qty_row(_("Quantity:"), 'quantity', null, null, null, $dec);

textarea_row(_("Memo:"), 'memo_', null, 40, 3);

end_table(1);

submit_center_first('Process', _("Process"), '', 'default');
submit_center_last('ProcessAndClose', _("Process And Close Order"), '', true);

end_form();

end_page();

