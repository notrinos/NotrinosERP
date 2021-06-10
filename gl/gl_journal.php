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
$page_security = 'SA_JOURNALENTRY';
$path_to_root = '..';
include_once($path_to_root.'/includes/ui/items_cart.inc');

include_once($path_to_root.'/includes/session.inc');

include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');

include_once($path_to_root.'/gl/includes/ui/gl_journal_ui.inc');
include_once($path_to_root.'/gl/includes/gl_db.inc');
include_once($path_to_root.'/gl/includes/gl_ui.inc');

$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(800, 500);
if (user_use_date_picker())
	$js .= get_js_date_picker();

if (isset($_GET['ModifyGL'])) {
	$_SESSION['page_title'] = sprintf(_('Modifying Journal Transaction # %d.'), 
		$_GET['trans_no']);
	$help_context = 'Modifying Journal Entry';
}
else
	$_SESSION['page_title'] = _($help_context = 'Journal Entry');

page($_SESSION['page_title'], false, false, '', $js);

//--------------------------------------------------------------------------------------------------

function line_start_focus() {
	global 	$Ajax;

	unset($_POST['Index']);
	$Ajax->activate('tabs');
	unset($_POST['_code_id_edit'], $_POST['code_id'], $_POST['AmountDebit'], $_POST['AmountCredit'], $_POST['dimension_id'], $_POST['dimension2_id']);
	set_focus('_code_id_edit');
}

//-----------------------------------------------------------------------------------------------

if (isset($_GET['AddedID'])) {
	$trans_no = $_GET['AddedID'];
	$trans_type = ST_JOURNAL;

	display_notification_centered( _('Journal entry has been entered').' #'.$trans_no);

	display_note(get_gl_view_str($trans_type, $trans_no, _('&View this Journal Entry')));

	reset_focus();
	hyperlink_params($_SERVER['PHP_SELF'], _('Enter &New Journal Entry'), 'NewJournal=Yes');

	hyperlink_params($path_to_root.'/admin/attachments.php', _('Add an Attachment'), 'filterType='.$trans_type.'&trans_no='.$trans_no);

	display_footer_exit();
}
elseif (isset($_GET['UpdatedID'])) {
	$trans_no = $_GET['UpdatedID'];
	$trans_type = ST_JOURNAL;

	display_notification_centered( _('Journal entry has been updated').' #'.$trans_no);

	display_note(get_gl_view_str($trans_type, $trans_no, _('&View this Journal Entry')));

	hyperlink_no_params($path_to_root.'/gl/inquiry/journal_inquiry.php', _('Return to Journal &Inquiry'));

	display_footer_exit();
}

//--------------------------------------------------------------------------------------------------

if (isset($_GET['NewJournal']))
	create_cart(0,0);
elseif (isset($_GET['ModifyGL'])) {
	check_is_editable($_GET['trans_type'], $_GET['trans_no']);

	if (!isset($_GET['trans_type']) || $_GET['trans_type']!= 0) {
		display_error(_('You can edit directly only journal entries created via Journal Entry page.'));
		hyperlink_params($path_to_root.'/gl/gl_journal.php', _('Entry &New Journal Entry'), 'NewJournal=Yes');
		display_footer_exit();
	}
	create_cart($_GET['trans_type'], $_GET['trans_no']);
}

function create_cart($type=0, $trans_no=0) {
	global $Refs;

	if (isset($_SESSION['journal_items']))
		unset ($_SESSION['journal_items']);

	check_is_closed($type, $trans_no);
	$cart = new items_cart($type);
	$cart->order_id = $trans_no;

	if ($trans_no) {
		$header = get_journal($type, $trans_no);
		$cart->event_date = sql2date($header['event_date']);
		$cart->doc_date = sql2date($header['doc_date']);
		$cart->tran_date = sql2date($header['tran_date']);
		$cart->currency = $header['currency'];
		$cart->rate = $header['rate'];
		$cart->source_ref = $header['source_ref'];

		$result = get_gl_trans($type, $trans_no);

		if ($result) {
			while ($row = db_fetch($result)) {
				$curr_amount = $cart->rate ? round($row['amount']/$cart->rate, $_SESSION['wa_current_user']->prefs->price_dec()) : $row['amount'];
				if ($curr_amount)
					$cart->add_gl_item($row['account'], $row['dimension_id'], $row['dimension2_id'], $curr_amount, $row['memo_'], '', $row['person_id']);
			}
		}
		$cart->memo_ = get_comments_string($type, $trans_no);
		$cart->reference = $header['reference'];
		// update net_amounts from tax register

		// retrieve tax details
		$tax_info = $cart->collect_tax_info(); // tax amounts in reg are always consistent with GL, so we can read them from GL lines

		$taxes = get_trans_tax_details($type, $trans_no);
		while ($detail = db_fetch($taxes)) {
			$tax_id = $detail['tax_type_id'];
			$tax_info['net_amount'][$tax_id] = $detail['net_amount']; // we can two records for the same tax_id, but in this case net_amount is the same
			$tax_info['tax_date'] = sql2date($detail['tran_date']);
			//$tax_info['tax_group'] = $detail['tax_group_id'];

		}
		if (isset($tax_info['net_amount']))	{// guess exempt sales/purchase if any tax has been found
			$net_sum = 0;
			foreach($cart->gl_items as $gl)
				if (!is_tax_account($gl->code_id) && !is_subledger_account($gl->code_id))
					$net_sum += $gl->amount;

			$ex_net = abs($net_sum) - array_sum($tax_info['net_amount']);
			if ($ex_net > 0)
				$tax_info['net_amount_ex'] = $ex_net;
		}
		$cart->tax_info = $tax_info;
	}
	else {
		$cart->tran_date = $cart->doc_date = $cart->event_date = new_doc_date();
		if (!is_date_in_fiscalyear($cart->tran_date))
			$cart->tran_date = end_fiscalyear();
		$cart->reference = $Refs->get_next(ST_JOURNAL, null, $cart->tran_date);
	}

	$_POST['memo_'] = $cart->memo_;
	$_POST['ref'] = $cart->reference;
	$_POST['date_'] = $cart->tran_date;
	$_POST['event_date'] = $cart->event_date;
	$_POST['doc_date'] = $cart->doc_date;
	$_POST['currency'] = $cart->currency;
	$_POST['_ex_rate'] = exrate_format($cart->rate);
	$_POST['source_ref'] = $cart->source_ref;
	if (isset($cart->tax_info['net_amount']) || (!$trans_no && get_company_pref('default_gl_vat')))
		$_POST['taxable_trans'] = true;
	$_SESSION['journal_items'] = &$cart;
}

function update_tax_info() {

	if (!isset($_SESSION['journal_items']->tax_info) || list_updated('tax_category'))
		$_SESSION['journal_items']->tax_info = $_SESSION['journal_items']->collect_tax_info();

	foreach ($_SESSION['journal_items']->tax_info as $name => $value)
		if (is_array($value)) {
			foreach ($value as $id => $amount) {
				$_POST[$name.'_'.$id] = price_format($amount);
			}
		}
		else
			$_POST[$name] = $value;
	$_POST['tax_date'] = $_SESSION['journal_items']->order_id ? $_SESSION['journal_items']->tax_info['tax_date'] : $_POST['date_'];
}

//-----------------------------------------------------------------------------------------------

if (isset($_POST['Process'])) {
	$input_error = 0;

	if ($_SESSION['journal_items']->count_gl_items() < 1) {
		display_error(_('You must enter at least one journal line.'));
		set_focus('code_id');
		$input_error = 1;
	}
	if (abs($_SESSION['journal_items']->gl_items_total()) > 0.001) {
		display_error(_('The journal must balance (debits equal to credits) before it can be processed.'));
		set_focus('code_id');
		$input_error = 1;
	}
	if (!is_date($_POST['date_'])) {
		display_error(_('The entered date is invalid.'));
		set_focus('date_');
		$input_error = 1;
	} 
	elseif (!is_date_in_fiscalyear($_POST['date_'])) {
		display_error(_('The entered date is out of fiscal year or is closed for further data entry.'));
		set_focus('date_');
		$input_error = 1;
	} 
	if (!is_date($_POST['event_date'])) {
		display_error(_('The entered date is invalid.'));
		set_focus('event_date');
		$input_error = 1;
	}
	if (!is_date($_POST['doc_date'])) {
		display_error(_('The entered date is invalid.'));
		set_focus('doc_date');
		$input_error = 1;
	}
	if (!check_reference($_POST['ref'], ST_JOURNAL, $_SESSION['journal_items']->order_id)) {
		set_focus('ref');
		$input_error = 1;
	}
	if (get_post('currency') != get_company_pref('curr_default'))
		if (isset($_POST['_ex_rate']) && !check_num('_ex_rate', 0.000001)) {
			display_error(_('The exchange rate must be numeric and greater than zero.'));
			set_focus('_ex_rate');
			$input_error = 1;
		}
	if (get_post('_tabs_sel') == 'tax') {
		if (!is_date($_POST['tax_date'])) {
			display_error(_('The entered date is invalid.'));
			set_focus('tax_date');
			$input_error = 1;
		} 
		elseif (!is_date_in_fiscalyear($_POST['tax_date'])) {
			display_error(_('The entered date is out of fiscal year or is closed for further data entry.'));
			set_focus('tax_date');
			$input_error = 1;
		}
		// FIXME: check proper tax net input values, check sum of net values against total GL an issue warning
	}

	if (check_value('taxable_trans')) {
		if (!tab_visible('tabs', 'tax')) {
			display_warning(_("Check tax register records before processing transaction or switch off 'Include in tax register' option."));
			$_POST['tabs_tax'] = true; // force tax tab select
			$input_error = 1;
		}
		else {
			$taxes = get_all_tax_types();
			$net_amount = 0;
			while ($tax = db_fetch($taxes)) {
				$tax_id = $tax['id'];
				$net_amount += input_num('net_amount_'.$tax_id);
			}
			// in case no tax account used we have to guss tax register on customer/supplier used.
			if ($net_amount && !$_SESSION['journal_items']->has_taxes() && !$_SESSION['journal_items']->has_sub_accounts()) {
				display_error(_('Cannot determine tax register to be used. You have to make at least one posting either to tax or customer/supplier account to use tax register.'));
				$_POST['tabs_gl'] = true; // force gl tab select
				$input_error = 1;
			}
		}
	}

	if ($input_error == 1)
		unset($_POST['Process']);
}

if (isset($_POST['Process'])) {
	$cart = &$_SESSION['journal_items'];
	$new = $cart->order_id == 0;

	$cart->reference = $_POST['ref'];
	$cart->tran_date = $_POST['date_'];
	$cart->doc_date = $_POST['doc_date'];
	$cart->event_date = $_POST['event_date'];
	$cart->source_ref = $_POST['source_ref'];
	if (isset($_POST['memo_']))
		$cart->memo_ = $_POST['memo_'];

	$cart->currency = $_POST['currency'];
	if ($cart->currency != get_company_pref('curr_default'))
		$cart->rate = input_num('_ex_rate');

	if (check_value('taxable_trans')) {
		// complete tax register data
		$cart->tax_info['tax_date'] = $_POST['tax_date'];
		//$cart->tax_info['tax_group'] = $_POST['tax_group'];
		$taxes = get_all_tax_types();
		while ($tax = db_fetch($taxes)) {
			$tax_id = $tax['id'];
			$cart->tax_info['net_amount'][$tax_id] = input_num('net_amount_'.$tax_id);
			$cart->tax_info['rate'][$tax_id] = $tax['rate'];
		}
	}
	else
		$cart->tax_info = false;
	$trans_no = write_journal_entries($cart);

	// retain the reconciled status if desired by user
	if (isset($_POST['reconciled']) && $_POST['reconciled'] == 1) {
		$sql = "UPDATE ".TB_PREF."bank_trans SET reconciled=".db_escape($_POST['reconciled_date'])." WHERE type=" . ST_JOURNAL . " AND trans_no=".db_escape($trans_no);

		db_query($sql, "Can't change reconciliation status");
	}

	$cart->clear_items();
	new_doc_date($_POST['date_']);
	unset($_SESSION['journal_items']);
	if($new)
		meta_forward($_SERVER['PHP_SELF'], 'AddedID='.$trans_no);
	else
		meta_forward($_SERVER['PHP_SELF'], 'UpdatedID='.$trans_no);
}

//-----------------------------------------------------------------------------------------------

function check_item_data() {
	global $Ajax;

	if (!get_post('code_id')) {
		display_error(_('You must select GL account.'));
		set_focus('code_id');
		return false;
	}
	if (is_subledger_account(get_post('code_id'))) {
		if(!get_post('person_id')) {
			display_error(_('You must select subledger account.'));
			$Ajax->activate('items_table');
			set_focus('person_id');
			return false;
		}
	}
	if (isset($_POST['dimension_id']) && $_POST['dimension_id'] != 0 && dimension_is_closed($_POST['dimension_id'])) {
		display_error(_('Dimension is closed.'));
		set_focus('dimension_id');
		return false;
	}
	if (isset($_POST['dimension2_id']) && $_POST['dimension2_id'] != 0 && dimension_is_closed($_POST['dimension2_id'])) {
		display_error(_('Dimension is closed.'));
		set_focus('dimension2_id');
		return false;
	}
	if (!(input_num('AmountDebit')!=0 ^ input_num('AmountCredit')!=0) ) {
		display_error(_('You must enter either a debit amount or a credit amount.'));
		set_focus('AmountDebit');
		return false;
	}
	if (strlen($_POST['AmountDebit']) && !check_num('AmountDebit', 0)) {
		display_error(_('The debit amount entered is not a valid number or is less than zero.'));
		set_focus('AmountDebit');
		return false;
	}
	elseif (strlen($_POST['AmountCredit']) && !check_num('AmountCredit', 0)) {
		display_error(_('The credit amount entered is not a valid number or is less than zero.'));
		set_focus('AmountCredit');
		return false;
	}
	if (!is_tax_gl_unique(get_post('code_id'))) {
		display_error(_('Cannot post to GL account used by more than one tax type.'));
		set_focus('code_id');
		return false;
	}
	if (!$_SESSION['wa_current_user']->can_access('SA_BANKJOURNAL') && is_bank_account($_POST['code_id'])) {
		display_error(_('You cannot make a journal entry for a bank account. Please use one of the banking functions for bank transactions.'));
		set_focus('code_id');
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------------------------

function handle_update_item() {
	if($_POST['UpdateItem'] != '' && check_item_data()) {

		$amount = input_num('AmountDebit') > 0 ? input_num('AmountDebit') : -input_num('AmountCredit');

		$_SESSION['journal_items']->update_gl_item($_POST['Index'], $_POST['code_id'], $_POST['dimension_id'], $_POST['dimension2_id'], $amount, $_POST['LineMemo'], '', get_post('person_id'));
		unset($_SESSION['journal_items']->tax_info);
		line_start_focus();
	}
}

//-----------------------------------------------------------------------------------------------

function handle_delete_item($id) {
	$_SESSION['journal_items']->remove_gl_item($id);
	unset($_SESSION['journal_items']->tax_info);
	line_start_focus();
}

//-----------------------------------------------------------------------------------------------

function handle_new_item() {
	if (!check_item_data())
		return;

	$amount = input_num('AmountDebit') > 0 ? input_num('AmountDebit') : -input_num('AmountCredit');
	
	$_SESSION['journal_items']->add_gl_item($_POST['code_id'], $_POST['dimension_id'], $_POST['dimension2_id'], $amount, $_POST['LineMemo'], '', get_post('person_id'));
	unset($_SESSION['journal_items']->tax_info);
	line_start_focus();
}

//-----------------------------------------------------------------------------------------------

if (isset($_POST['_taxable_trans_update'])) {
	if (!check_value('taxable_trans'))
		$_POST['tabs_gl'] = true; // force tax tab select
	else
		set_focus('taxable_trans');
	$Ajax->activate('tabs');
}

if (tab_closed('tabs', 'gl'))
	$_SESSION['journal_items']->memo_ = $_POST['memo_'];
elseif (tab_closed('tabs', 'tax')) {
	$cart = &$_SESSION['journal_items'];
	$cart->tax_info['tax_date'] = $_POST['tax_date'];
	//$cart->tax_info['tax_group'] = $_POST['tax_group'];
	$taxes = get_all_tax_types();
	while ($tax = db_fetch($taxes)) {
		$tax_id = $tax['id'];
		$cart->tax_info['net_amount'][$tax_id] = input_num('net_amount_'.$tax_id);
		$cart->tax_info['rate'][$tax_id] = $tax['rate'];
	}
}
if (tab_opened('tabs', 'gl'))
	$_POST['memo_'] = $_SESSION['journal_items']->memo_;
elseif (tab_opened('tabs', 'tax'))
	set_focus('tax_date');

$id = find_submit('Delete');
if ($id != -1)
	handle_delete_item($id);

if (isset($_POST['AddItem'])) 
	handle_new_item();

if (isset($_POST['UpdateItem'])) 
	handle_update_item();
	
if (isset($_POST['CancelItemChanges']))
	line_start_focus();

if (isset($_POST['go'])) {
	display_quick_entries($_SESSION['journal_items'], $_POST['quick'], input_num('totamount'), QE_JOURNAL, get_post('aux_info'));
	$_POST['totamount'] = price_format(0); $Ajax->activate('totamount');
	line_start_focus();
}

if (list_updated('tax_category'))
	$Ajax->activate('tabs');

//-----------------------------------------------------------------------------------------------

start_form();

display_order_header($_SESSION['journal_items']);

tabbed_content_start('tabs', array(
	'gl' => array(_('&GL postings'), true),
	'tax' => array(_('&Tax register'), check_value('taxable_trans')),
));
	
switch (get_post('_tabs_sel')) {
	default:
	case 'gl':
		start_table(TABLESTYLE, "width='90%'", 10);
		start_row();
		echo '<td>';
		display_gl_items(_('Rows'), $_SESSION['journal_items']);
		gl_options_controls();
		echo '</td>';
		end_row();
		end_table(1);
		break;
	case 'tax':
		update_tax_info();
		br();
		display_heading(_('Tax register record'));
		br();
		start_table(TABLESTYLE2);
		date_row(_('VAT date:'), 'tax_date', '', "colspan='3'");
		//tax_groups_list_row(_('Tax group:'), 'tax_group');
		end_table(1);

		start_table(TABLESTYLE2, 'width=60%');
		table_header(array(_('Name'), _('Input Tax'), _('Output Tax'), _('Net amount')));
		$taxes = get_all_tax_types();
		while ($tax = db_fetch($taxes)) {
			start_row();
			label_cell($tax['name'].' '.$tax['rate'].'%');
			amount_cell(input_num('tax_in_'.$tax['id']));
			amount_cell(input_num('tax_out_'.$tax['id']));

			amount_cells(null, 'net_amount_'.$tax['id']);
			end_row();
		}
		end_table(1);
		break;
};
submit_center('Process', _('Process Journal Entry'), true , _('Process journal entry only if debits equal to credits'), 'default');
br();
tabbed_content_end();

end_form();

end_page();
