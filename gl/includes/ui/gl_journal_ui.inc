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
include_once($path_to_root . "/includes/ui.inc");
include_once($path_to_root . "/includes/ui/items_cart.inc");

//--------------------------------------------------------------------------------

function display_order_header(&$Order)
{
	global $Ajax;

	$qes = has_quick_entries(QE_JOURNAL);
	$new = $Order->order_id==0;
	start_outer_table(TABLESTYLE2, "width='90%'");
	table_section(1);

    date_row(_("Journal Date:"), 'date_', '', $new, 0,0,0,null, true);
    if (input_changed('date_'))
    {
    	unset($_POST['ref']);
    	$Ajax->activate('ref');
    }

	currencies_list_row(_('Currency:'), 'currency', null, true);
 	if (list_updated('currency'))
		unset($_POST['_ex_rate']);
	exchange_rate_display(get_company_pref('curr_default'), get_post('currency'), get_post('date_'), true);

	if (list_updated('currency'))
		$Order->clear_items();

	table_section(2);

    date_row(_("Document Date:"), 'doc_date', '', $new);
    date_row(_("Event Date:"), 'event_date', '', $new);
    text_row(_("Source ref:"), 'source_ref', null, 30, 60);

 	if ($new) {
		table_section(3);
	}

	ref_row(_("Reference:"), 'ref', null, get_post('ref'), false, ST_JOURNAL, get_post('date_'));
	hidden('ref_original');

	if ($qes !== false)
	{
		table_section(3);
		quick_entries_list_row(_("Quick Entry").":", 'quick', null, QE_JOURNAL, true);

		start_row();
		$qid = get_quick_entry(get_post('quick'));
		if (list_updated('quick')) {
			unset($_POST['totamount']); // enable default
		$Ajax->activate('totamount');
		}
		end_row();
		if ($qid['bal_type'] == 1)
		{
			$accname = get_gl_account_name($qid['base_desc']);
			label_row(($qid['base_amount'] == 0 ? _("Yearly") : _("Monthly")) . " ". _("balance from account")." ".
				$qid['base_desc']." ".$accname."&nbsp;&nbsp;".submit('go', _("Go"), false, false, true),'', "colspan=2");
		}
		else
		{
			text_row(_('Additional info:'), 'aux_info', null, 35, 255);
			amount_row($qid['base_desc'].":", 'totamount', price_format($qid['base_amount']),
		 		null, "&nbsp;&nbsp;".submit('go', _("Go"), false, false, true));
		}

	}

	check_row(_('Include in tax register:'), 'taxable_trans', null, true);
        // Query the user to retain the reconciled status
        if (!$new) {
            $result = get_bank_trans(ST_JOURNAL, $Order->order_id);
            $row = db_fetch($result);
            if ($row
                && $row['reconciled']) {
                check_row(_('Reconciled:'), 'reconciled', 1, true);
                hidden('reconciled_date', $row['reconciled']);
            }
        }
	end_outer_table(1);
}

//---------------------------------------------------------------------------------

function display_gl_items($title, &$order)
{
	global $path_to_root;

	display_heading($title);

	$dim = get_company_pref('use_dimension');

	$id = find_submit('Edit');
	$sub_type = is_subledger_account(get_post('code_id'));
	$has_subaccounts = $order->has_sub_accounts();

    div_start('items_table');
	start_table(TABLESTYLE, "width='95%'");
	if ($dim == 2)
		$th = array(_("Account Code"), _("Account Description"), _("Dimension")." 1",
			_("Dimension")." 2", _("Debit"), _("Credit"), _("Memo"), "");
	elseif ($dim == 1)
		$th = array(_("Account Code"), _("Account Description"), _("Dimension"),
			_("Debit"), _("Credit"), _("Memo"), "");
	else
		$th = array(_("Account Code"), _("Account Description"),
			_("Debit"), _("Credit"), _("Memo"), "");

	if (count($order->gl_items)) $th[] = '';
	if ($sub_type || $has_subaccounts)
		array_insert($th, 2, _("Counterparty"));

	table_header($th);

	$k = 0;

	if ($id == -1) 
		$id = get_post('Index', -1);

	foreach ($order->gl_items as $line => $item) 
	{
		if ($id != $line)
		{
    		alt_table_row_color($k);

			$acc = $item->code_id . ($item->person_id ? sprintf(' %05d', $item->person_id) : '');
			label_cells($acc, $item->description);

		if ($sub_type || $has_subaccounts)
			label_cell($item->person_name);

    		if ($dim >= 1)
   				label_cell(get_dimension_string($item->dimension_id, true));
    		if ($dim > 1)
   				label_cell(get_dimension_string($item->dimension2_id, true));
    		if ($item->amount > 0)
    		{
    			amount_cell(abs($item->amount));
    			label_cell("");
    		}
    		else
    		{
    			label_cell("");
    			amount_cell(abs($item->amount));
    		}	
			label_cell($item->reference);

			edit_button_cell("Edit$line", _("Edit"),
				_('Edit journal line'));
			delete_button_cell("Delete$line", _("Delete"),
				_('Remove line from journal'));
    		end_row();
		} 
		else 
		{
			gl_edit_item_controls($order, $dim, $item->person_id, $line);
		}
	}

	if ($id == -1)
		gl_edit_item_controls($order, $dim, $sub_type);

	if ($order->count_gl_items()) 
	{
		$colspan = ($dim == 2 ? "4" : ($dim == 1 ? "3" : "2")) + ($sub_type || $has_subaccounts ? 1 : 0);
		start_row();
		label_cell(_("Total"), "align=right colspan=" . $colspan);
		amount_cell($order->gl_items_total_debit());
		amount_cell(abs($order->gl_items_total_credit()));
		label_cell('', "colspan=3");
		end_row();
	}

    end_table();
	div_end();
}

//---------------------------------------------------------------------------------

function gl_edit_item_controls(&$order, $dim, $sub_accounts, $Index=null)
{
	global $Ajax;
	start_row();

	$id = find_submit('Edit');
	if ($id == -1) 
		$continue = $id = get_post('Index', -1);

	if ($Index != -1 && $Index == $id)
	{
	    // Modifying an existing row
		$item = $order->gl_items[$Index];
		if (!isset($continue))
		{
			$_POST['code_id'] = $item->code_id;
			$_POST['dimension_id'] = $item->dimension_id;
			$_POST['dimension2_id'] = $item->dimension2_id;
			$_POST['person_id'] = $item->person_id;
			if ($item->amount > 0)
			{
				$_POST['AmountDebit'] = price_format($item->amount);
				$_POST['AmountCredit'] = "";
			}
			else
			{
				$_POST['AmountDebit'] = "";
				$_POST['AmountCredit'] = price_format(abs($item->amount));
			}
			$_POST['description'] = $item->description;
			$_POST['LineMemo'] = $item->reference;
		}
		hidden('Index', $id);
		$skip_bank = !$_SESSION["wa_current_user"]->can_access('SA_BANKJOURNAL');
		echo gl_all_accounts_list('code_id', null, $skip_bank, true, _('[Select account]'), true, false, false);

		if (is_subledger_account(get_post('code_id')))
		{	// for home currency postings show all customers
			$sel_cur = get_post('currency', $order->currency);
			subledger_list_cells(null, 'person_id', get_post('code_id'), null, false, false,
				get_company_currency() == $sel_cur  ? null : $sel_cur);
		} elseif ($order->has_sub_accounts())
			label_cell('');
		if ($dim >= 1) 
			dimensions_list_cells(null, 'dimension_id', null, true, " ", false, 1);
		if ($dim > 1) 
			dimensions_list_cells(null, 'dimension2_id', null, true, " ", false, 2);
	    $Ajax->activate('items_table');
	}
	else
	{
		$skip_bank = !$_SESSION["wa_current_user"]->can_access('SA_BANKJOURNAL');
		echo gl_all_accounts_list('code_id', null, $skip_bank, true, _('[Select account]'), true, false, false);
		if ($sub_accounts)
		{
			// for home currency postings show all customers
			$sel_cur = get_post('currency', $order->currency);
			subledger_list_cells(null, 'person_id', get_post('code_id'), null, false, false,
				get_company_currency() == $sel_cur  ? null : $sel_cur);
		}
		elseif ($order->has_sub_accounts())
			label_cell('');

		if ($dim >= 1)
			dimensions_list_cells(null, 'dimension_id', null, true, " ", false, 1);
		if ($dim > 1)
			dimensions_list_cells(null, 'dimension2_id', null, true, " ", false, 2);
		$_POST['LineMemo'] = "";			
	}

	if (get_post('_code_id_update'))
	{
	    $Ajax->activate('code_id');
		set_focus($sub_accounts ? 'person_id' : 'AmountDebit');
	}
	if ($dim < 1)
		hidden('dimension_id', 0);
	if ($dim < 2)
		hidden('dimension2_id', 0);

	small_amount_cells(null, 'AmountDebit');
	small_amount_cells(null, 'AmountCredit');
	text_cells_ex(null, 'LineMemo', 35, 255);

	if ($id != -1)
	{
		button_cell('UpdateItem', _("Update"),
				_('Confirm changes'), ICON_UPDATE);
		button_cell('CancelItemChanges', _("Cancel"),
				_('Cancel changes'), ICON_CANCEL);
 		set_focus('amount');
	} 
	else 
		submit_cells('AddItem', _("Add Item"), "colspan=2",
		    _('Add new line to journal'), true);

	end_row();
}

//---------------------------------------------------------------------------------

function gl_options_controls()
{
	  echo "<br><table align='center'>";

	  textarea_row(_("Memo"), 'memo_', null, 50, 3);

	  echo "</table>";
}
