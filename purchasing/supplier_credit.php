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
$page_security = 'SA_SUPPLIERCREDIT';
$path_to_root = "..";

include_once($path_to_root . "/purchasing/includes/supp_trans_class.inc");

include_once($path_to_root . "/includes/session.inc");

include_once($path_to_root . "/includes/data_checks.inc");

include_once($path_to_root . "/purchasing/includes/purchasing_db.inc");
include_once($path_to_root . "/purchasing/includes/purchasing_ui.inc");
$js = "";
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 500);
if (user_use_date_picker())
	$js .= get_js_date_picker();

//----------------------------------------------------------------------------------------

if (isset($_GET['ModifyCredit'])) {
    check_is_editable(ST_SUPPCREDIT, $_GET['ModifyCredit']);
    $_SESSION['page_title'] = sprintf( _("Modifying Supplier Credit # %d"), $_GET['ModifyCredit']);
    $_SESSION['supp_trans'] = new supp_trans(ST_SUPPCREDIT, $_GET['ModifyCredit']);
}

//---------------------------------------------------------------------------------------------------

if (isset($_GET['New']))
{
	if (isset( $_SESSION['supp_trans']))
	{
		unset ($_SESSION['supp_trans']->grn_items);
		unset ($_SESSION['supp_trans']->gl_codes);
		unset ($_SESSION['supp_trans']);
	}

	if (isset($_GET['invoice_no']))
	{
		$_SESSION['supp_trans'] = new supp_trans(ST_SUPPINVOICE, $_GET['invoice_no']);
		$_SESSION['supp_trans']->src_docs = array( $_GET['invoice_no'] => $_SESSION['supp_trans']->supp_reference);


		$_SESSION['supp_trans']->trans_type = ST_SUPPCREDIT;
		$_SESSION['supp_trans']->trans_no = 0;
		$_SESSION['supp_trans']->supp_reference = '';
		$help_context = "Supplier Credit Note";
		$_SESSION['page_title'] = _("Supplier Credit Note");

	} else {
		$help_context = "Supplier Credit Note";
		$_SESSION['page_title'] = _("Supplier Credit Note");
		$_SESSION['supp_trans'] = new supp_trans(ST_SUPPCREDIT);
	}
}
page($_SESSION['page_title'], false, false, "", $js);

check_db_has_suppliers(_("There are no suppliers defined in the system."));

//---------------------------------------------------------------------------------------------------------------

if (isset($_GET['AddedID'])) 
{
	$invoice_no = $_GET['AddedID'];
	$trans_type = ST_SUPPCREDIT;


    echo "<center>";
    display_notification_centered(_("Supplier credit note has been processed."));
    display_note(get_trans_view_str($trans_type, $invoice_no, _("View this Credit Note")));

	display_note(get_gl_view_str($trans_type, $invoice_no, _("View the GL Journal Entries for this Credit Note")), 1);

    hyperlink_params($_SERVER['PHP_SELF'], _("Enter Another Credit Note"), "New=1");
	hyperlink_params("$path_to_root/admin/attachments.php", _("Add an Attachment"), "filterType=$trans_type&trans_no=$invoice_no");

	display_footer_exit();
}

function clear_fields()
{
	global $Ajax;
	
	unset($_POST['gl_code']);
	unset($_POST['dimension_id']);
	unset($_POST['dimension2_id']);
	unset($_POST['amount']);
	unset($_POST['memo_']);
	unset($_POST['AddGLCodeToTrans']);
	$Ajax->activate('gl_items');
	set_focus('gl_code');
}

function reset_tax_input()
{
	global $Ajax;

	unset($_POST['mantax']);
	$Ajax->activate('inv_tot');
}

//------------------------------------------------------------------------------------------------
//	GL postings are often entered in the same form to two accounts
//  so fileds are cleared only on user demand.
//
if (isset($_POST['ClearFields']))
{
	clear_fields();
}

if (isset($_POST['AddGLCodeToTrans'])) {

	$Ajax->activate('gl_items');
	$input_error = false;

	$result = get_gl_account_info($_POST['gl_code']);
	if (db_num_rows($result) == 0)
	{
		display_error(_("The account code entered is not a valid code, this line cannot be added to the transaction."));
		set_focus('gl_code');
		$input_error = true;
	}
	else
	{
		$myrow = db_fetch_row($result);
		$gl_act_name = $myrow[1];
		if (!check_num('amount'))
		{
			display_error(_("The amount entered is not numeric. This line cannot be added to the transaction."));
			set_focus('amount');
			$input_error = true;
		}
	}

	if (!is_tax_gl_unique(get_post('gl_code'))) {
   		display_error(_("Cannot post to GL account used by more than one tax type."));
		set_focus('gl_code');
   		$input_error = true;
	}

	if ($input_error == false)
	{
		$_SESSION['supp_trans']->add_gl_codes_to_trans($_POST['gl_code'], $gl_act_name,
			$_POST['dimension_id'], $_POST['dimension2_id'], 
			input_num('amount'), $_POST['memo_']);
		reset_tax_input();
		set_focus('gl_code');
	}
}


//---------------------------------------------------------------------------------------------------

function check_data()
{
	global $SysPrefs;

	if (!$_SESSION['supp_trans']->is_valid_trans_to_post())
	{
		display_error(_("The credit note cannot be processed because the there are no items or values on the invoice.  Credit notes are expected to have a charge."));
		set_focus('');
		return false;
	}

	if (!check_reference($_SESSION['supp_trans']->reference, ST_SUPPCREDIT, $_SESSION['supp_trans']->trans_no))
	{
		set_focus('reference');
		return false;
	}

	if (!is_date($_SESSION['supp_trans']->tran_date))
	{
		display_error(_("The credit note as entered cannot be processed because the date entered is not valid."));
		set_focus('tran_date');
		return false;
	} 
	elseif (!is_date_in_fiscalyear($_SESSION['supp_trans']->tran_date)) 
	{
		display_error(_("The entered date is out of fiscal year or is closed for further data entry."));
		set_focus('tran_date');
		return false;
	}
	if (!is_date( $_SESSION['supp_trans']->due_date))
	{
		display_error(_("The invoice as entered cannot be processed because the due date is in an incorrect format."));
		set_focus('due_date');
		return false;
	}

	if (trim(get_post('supp_reference')) == false)
	{
		display_error(_("You must enter a supplier's invoice reference."));
		set_focus('supp_reference');
		return false;
	}

	if (is_reference_already_there($_SESSION['supp_trans']->supplier_id, $_POST['supp_reference'], $_SESSION['supp_trans']->trans_no))
	{ 	/*Transaction reference already entered */
		display_error(_("This invoice number has already been entered. It cannot be entered again.") . " (" . $_POST['supp_reference'] . ")");
		set_focus('supp_reference');
		return false;
	}

	if (!$SysPrefs->allow_negative_stock()) {
		foreach ($_SESSION['supp_trans']->grn_items as $n => $item) {
			if (is_inventory_item($item->item_code))
			{
				if (check_negative_stock($item->item_code, -$item->this_quantity_inv, null, $_SESSION['supp_trans']->tran_date))
				{
					$stock = get_item($item->item_code);
					display_error(_("The return cannot be processed because there is an insufficient quantity for item:") .
						" " . $stock['stock_id'] . " - " . $stock['description'] . " - " .
						_("Quantity On Hand") . " = " . number_format2(get_qoh_on_date($stock['stock_id'], null, 
						$_SESSION['supp_trans']->tran_date), get_qty_dec($stock['stock_id'])));
					return false;
				}
			}
		}
	}
	return true;
}

//---------------------------------------------------------------------------------------------------

function handle_commit_credit_note()
{
	copy_to_trans($_SESSION['supp_trans']);

	if (!check_data())
		return;

	if (isset($_POST['invoice_no']))
		$invoice_no = add_supp_invoice($_SESSION['supp_trans'], $_POST['invoice_no']);
	else
		$invoice_no = add_supp_invoice($_SESSION['supp_trans']);

    $_SESSION['supp_trans']->clear_items();
    unset($_SESSION['supp_trans']);

	meta_forward($_SERVER['PHP_SELF'], "AddedID=$invoice_no");
}

//--------------------------------------------------------------------------------------------------

if (isset($_POST['PostCreditNote']))
{
	handle_commit_credit_note();
}

function check_item_data($n)
{

	if (!check_num('This_QuantityCredited'.$n, 0))
	{
		display_error(_("The quantity to credit must be numeric and greater than zero."));
		set_focus('This_QuantityCredited'.$n);
		return false;
	}

	if (!check_num('ChgPrice'.$n, 0))
	{
		display_error(_("The price is either not numeric or negative."));
		set_focus('ChgPrice'.$n);
		return false;
	}

	return true;
}

function commit_item_data($n)
{
	if (check_item_data($n))
	{
		$_SESSION['supp_trans']->add_grn_to_trans($n,
    		$_POST['po_detail_item'.$n], $_POST['item_code'.$n],
    		$_POST['item_description'.$n], $_POST['qty_recd'.$n],
    		$_POST['prev_quantity_inv'.$n], input_num('This_QuantityCredited'.$n),
    		$_POST['order_price'.$n], input_num('ChgPrice'.$n));
		reset_tax_input();
	}
}

//-----------------------------------------------------------------------------------------

$id = find_submit('grn_item_id');
if ($id != -1)
{
	commit_item_data($id);
}

if (isset($_POST['InvGRNAll']))
{
   	foreach($_POST as $postkey=>$postval )
    {
		if (strpos($postkey, "qty_recd") === 0)
		{
			$id = substr($postkey, strlen("qty_recd"));
			$id = (int)$id;
			commit_item_data($id);
		}
    }
}	


//--------------------------------------------------------------------------------------------------
$id3 = find_submit('Delete');
if ($id3 != -1)
{
	$_SESSION['supp_trans']->remove_grn_from_trans($id3);
	$Ajax->activate('grn_items');
	reset_tax_input();
}

$id4 = find_submit('Delete2');
if ($id4 != -1)
{
	$_SESSION['supp_trans']->remove_gl_codes_from_trans($id4);
	clear_fields();
	reset_tax_input();
	$Ajax->activate('gl_items');
}
if (isset($_POST['RefreshInquiry']))
{
	$Ajax->activate('grn_items');
	reset_tax_input();
}

if (isset($_POST['go']))
{
	$Ajax->activate('gl_items');
	display_quick_entries($_SESSION['supp_trans'], $_POST['qid'], input_num('totamount'), QE_SUPPINV);
	$_POST['totamount'] = price_format(0); $Ajax->activate('totamount');
	reset_tax_input();
}


//--------------------------------------------------------------------------------------------------

start_form();

invoice_header($_SESSION['supp_trans']);
if ($_POST['supplier_id']=='') 
	display_error('No supplier found for entered search text');
else {
	display_grn_items($_SESSION['supp_trans'], 1);

	display_gl_items($_SESSION['supp_trans'], 1);

	div_start('inv_tot');
	invoice_totals($_SESSION['supp_trans']);
	div_end();
}

if ($id != -1)
{
	$Ajax->activate('grn_items');
	$Ajax->activate('inv_tot');
}

if (get_post('AddGLCodeToTrans'))
	$Ajax->activate('inv_tot');

br();
submit_center('PostCreditNote', _("Enter Credit Note"), true, '', 'default');
br();

end_form();
end_page();
