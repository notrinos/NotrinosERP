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
$page_security = 'SA_VOIDTRANSACTION';
$path_to_root = '..';

include($path_to_root.'/includes/db_pager.inc');
include_once($path_to_root.'/includes/session.inc');

include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/admin/db/transactions_db.inc');

include_once($path_to_root.'/admin/db/voiding_db.inc');

$js = '';

if (user_use_date_picker())
	$js .= get_js_date_picker();
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(800, 500);
	
page(_($help_context = 'Void a Transaction'), false, false, '', $js);
simple_page_mode(true);

//----------------------------------------------------------------------------------------

function exist_transaction($type, $type_no) {
	$void_entry = get_voided_entry($type, $type_no);

	if ($void_entry != null)
		return false;

	switch ($type) {
		case ST_JOURNAL : // it's a journal entry
			if (!exists_gl_trans($type, $type_no))
				return false;
			break;
			
		case ST_BANKPAYMENT : // it's a payment
		case ST_BANKDEPOSIT : // it's a deposit
		case ST_BANKTRANSFER : // it's a transfer
			if (!exists_bank_trans($type, $type_no))
				return false;
			break;

		case ST_SALESINVOICE : // it's a customer invoice
		case ST_CUSTCREDIT : // it's a customer credit note
		case ST_CUSTPAYMENT : // it's a customer payment
		case ST_CUSTDELIVERY : // it's a customer dispatch
			if (!exists_customer_trans($type, $type_no))
				return false;
			break;

		case ST_LOCTRANSFER : // it's a stock transfer
			if (get_stock_transfer_items($type_no) == null)
				return false;
			break;

		case ST_INVADJUST : // it's a stock adjustment
			if (get_stock_adjustment_items($type_no) == null)
				return false;
			break;

		case ST_PURCHORDER : // it's a PO
			return false;

		case ST_SUPPRECEIVE : // it's a GRN
			if (!exists_grn($type_no))
				return false;
			break;

		case ST_SUPPINVOICE : // it's a suppler invoice
		case ST_SUPPCREDIT : // it's a supplier credit note
		case ST_SUPPAYMENT : // it's a supplier payment
			if (!exists_supp_trans($type, $type_no))
				return false;
			break;

		case ST_WORKORDER : // it's a work order
			if (!get_work_order($type_no, true))
				return false;
			break;

		case ST_MANUISSUE : // it's a work order issue
			if (!exists_work_order_issue($type_no))
				return false;
			break;

		case ST_MANURECEIVE : // it's a work order production
			if (!exists_work_order_produce($type_no))
				return false;
			break;

		case ST_SALESORDER: // it's a sales order
		case ST_SALESQUOTE: // it's a sales quotation
			return false;
		case ST_COSTUPDATE : // it's a stock cost update
			return false;
	}

	return true;
}

function view_link($trans) {
	if (!isset($trans['type']))
		$trans['type'] = $_POST['filterType'];
	return get_trans_view_str($trans["type"], $trans["trans_no"]);
}

function select_link($row) {
	if (!isset($row['type']))
		$row['type'] = $_POST['filterType'];
	if (!is_date_in_fiscalyear($row['trans_date'], true))
		return _("N/A");
	return button('Edit'.$row["trans_no"], _("Select"), _("Select"), ICON_EDIT);
}

function gl_view($row) {
	if (!isset($row['type']))
		$row['type'] = $_POST['filterType'];
	return get_gl_view_str($row["type"], $row["trans_no"]);
}

function date_view($row) {
	return $row['trans_date'];
}

function ref_view($row) {
	return $row['ref'];
}

function is_selected($row) { // Function added by faisal
	global $selected_id;
	return $row['trans_no'] == $selected_id ? true : false;
}

function voiding_controls() {
	global $selected_id;

	$not_implemented =  array(ST_PURCHORDER, ST_SALESORDER, ST_SALESQUOTE, ST_COSTUPDATE, ST_CUSTOMER, ST_SUPPLIER);

	start_form();

	start_table(TABLESTYLE_NOBORDER);
	start_row();

	systypes_list_cells(_('Transaction Type:'), 'filterType', null, true, $not_implemented);
	if (list_updated('filterType'))
		$selected_id = -1;

	if (!isset($_POST['FromTransNo']))
		$_POST['FromTransNo'] = '1';
	if (!isset($_POST['ToTransNo']))
		$_POST['ToTransNo'] = '999999';

	ref_cells(_('from #:'), 'FromTransNo');
	ref_cells(_('to #:'), 'ToTransNo');

	submit_cells('ProcessSearch', _('Search'), '', '', 'default');

	end_row();
	end_table(1);
	
	$trans_ref = false;
	$sql = get_sql_for_view_transactions(get_post('filterType'), get_post('FromTransNo'), get_post('ToTransNo'), $trans_ref);
	if ($sql == '')
		return;

	$cols = array(
		_('#') => array('insert'=>true, 'fun'=>'view_link'),
		_('Reference') => array('fun'=>'ref_view'),
		_('Date') => array('type'=>'date', 'fun'=>'date_view'),
		_('GL') => array('insert'=>true, 'fun'=>'gl_view'),
		_('Select') => array('insert'=>true, 'fun'=>'select_link')
	);

	$table =& new_db_pager('transactions', $sql, $cols);
	$table->set_marker('is_selected', _('Marked transactions will be voided.')); //Added by Faisal

	$table->width = '40%';
	display_db_pager($table);

	start_table(TABLESTYLE2);

	if ($selected_id != -1) {
		hidden('trans_no', $selected_id);
		hidden('selected_id', $selected_id);
	}
	else {
		hidden('trans_no', '');
		$_POST['memo_'] = '';
	}	
	label_row(_('Transaction #:'), ($selected_id==-1 ? '' : $selected_id));

	date_row(_('Voiding Date:'), 'date_');

	textarea_row(_('Memo:'), 'memo_', null, 30, 4);

	end_table(1);

	if (!isset($_POST['ProcessVoiding']))
		submit_center('ProcessVoiding', _('Void Transaction'), true, '', 'default');
	else {
		if (!exist_transaction($_POST['filterType'],$_POST['trans_no'])) {
			display_error(_("The entered transaction does not exist or cannot be voided."));
			unset($_POST['trans_no']);
			unset($_POST['memo_']);
			unset($_POST['date_']);
			submit_center('ProcessVoiding', _("Void Transaction"), true, '', 'default');
		}	
		else {
			if ($_POST['filterType'] == ST_SUPPRECEIVE) { 
				$result = get_grn_items($_POST['trans_no']);
				if (db_num_rows($result) > 0) {
					while ($myrow = db_fetch($result)) {
						if (is_inventory_item($myrow['item_code'])) {
							if (check_negative_stock($myrow['item_code'], -$myrow['qty_recd'], null, $_POST['date_'])) {
								$stock = get_item($myrow['item_code']);
								display_error(_('The void cannot be processed because there is an insufficient quantity for item:') .' '.$stock['stock_id'].' - '.$stock['description'].' - '._('Quantity On Hand').' = '.number_format2(get_qoh_on_date($stock['stock_id'], null, $_POST['date_']), get_qty_dec($stock['stock_id'])));
								return false;
							}
						}
					}
				}
			}
			display_warning(_('Are you sure you want to void this transaction ? This action cannot be undone.'), 0, 1);
			br();
			submit_center_first('ConfirmVoiding', _('Proceed'), '', true);
			submit_center_last('CancelVoiding', _('Cancel'), '', 'cancel');
		}	
	}

	end_form();
}

//----------------------------------------------------------------------------------------

function check_valid_entries() {
	if (is_closed_trans($_POST['filterType'],$_POST['trans_no'])) {
		display_error(_('The selected transaction was closed for edition and cannot be voided.'));
		set_focus('trans_no');
		return false;
	}
	if (!is_date($_POST['date_'])) {
		display_error(_('The entered date is invalid.'));
		set_focus('date_');
		return false;
	}
	if (!is_date_in_fiscalyear($_POST['date_'])) {
		display_error(_('The entered date is out of fiscal year or is closed for further data entry.'));
		set_focus('date_');
		return false;
	}
	if (!is_numeric($_POST['trans_no']) OR $_POST['trans_no'] <= 0) {
		display_error(_('The transaction number is expected to be numeric and greater than zero.'));
		set_focus('trans_no');
		return false;
	}

	return true;
}

//----------------------------------------------------------------------------------------

function handle_void_transaction() {
	if (check_valid_entries()==true) {
		$void_entry = get_voided_entry($_POST['filterType'], $_POST['trans_no']);
		if ($void_entry != null) {
			display_error(_('The selected transaction has already been voided.'), true);
			unset($_POST['trans_no']);
			unset($_POST['memo_']);
			unset($_POST['date_']);
			set_focus('trans_no');
			return;
		}

		$msg = void_transaction($_POST['filterType'], $_POST['trans_no'], $_POST['date_'], $_POST['memo_']);

		if (!$msg) {
			display_notification_centered(_('Selected transaction has been voided.'));
			unset($_POST['trans_no']);
			unset($_POST['memo_']);
		}
		else {
			display_error($msg);
			set_focus('trans_no');

		}
	}
}

//----------------------------------------------------------------------------------------

if (!isset($_POST['date_'])) {
	$_POST['date_'] = Today();
	if (!is_date_in_fiscalyear($_POST['date_']))
		$_POST['date_'] = end_fiscalyear();
}		
if (isset($_POST['ProcessVoiding'])) {
	if (!check_valid_entries())
		unset($_POST['ProcessVoiding']);
	$Ajax->activate('_page_body');
}
if (isset($_POST['ConfirmVoiding'])) {
	handle_void_transaction();
	$selected_id = '';
	$Ajax->activate('_page_body');
}
if (isset($_POST['CancelVoiding'])) {
	$selected_id = -1;
	$Ajax->activate('_page_body');
}

voiding_controls();

end_page();