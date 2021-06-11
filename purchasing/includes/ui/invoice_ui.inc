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

function copy_from_trans(&$supp_trans) {
	$_POST['Comments'] = $supp_trans->Comments;
	$_POST['tran_date'] = $supp_trans->tran_date;
	$_POST['due_date'] = $supp_trans->due_date;
	$_POST['supp_reference'] = $supp_trans->supp_reference;
	$_POST['reference'] = $supp_trans->reference;
	$_POST['supplier_id'] = $supp_trans->supplier_id;
	$_POST['dimension'] = $supp_trans->dimension;
	$_POST['dimension2'] = $supp_trans->dimension2;

	$_POST['_ex_rate'] = $supp_trans->ex_rate;
	if (isset($supp_trans->tax_overrides))
		foreach($supp_trans->tax_overrides as $id => $value)
			$_POST['mantax'][$id] = price_format($value);
}

function copy_to_trans(&$supp_trans) {
	$supp_trans->Comments = $_POST['Comments'];
	$supp_trans->tran_date = $_POST['tran_date'];
	$supp_trans->due_date = $_POST['due_date'];
	$supp_trans->supp_reference = $_POST['supp_reference'];
	$supp_trans->reference = $_POST['reference'];
	$supp_trans->dimension = @$_POST['dimension'];
	$supp_trans->dimension2 =  @$_POST['dimension2'];

	$supp_trans->ex_rate = input_num('_ex_rate', null);

	$supp_trans->ov_amount = $supp_trans->ov_discount = 0; // for starters
	if (isset($_POST['mantax'])) {
		foreach($_POST['mantax'] as $id => $tax) {
			$supp_trans->tax_overrides[$id] = user_numeric($_POST['mantax'][$id]);
		}
	}
	else
		unset($supp_trans->tax_overrides);

	if (count($supp_trans->grn_items) > 0) {
		foreach ( $supp_trans->grn_items as $grn) {
			$supp_trans->ov_amount += round2(($grn->this_quantity_inv * $grn->chg_price), user_price_dec());
		}
	}
	if (count($supp_trans->gl_codes) > 0) {
		foreach ( $supp_trans->gl_codes as $gl_line) {
			if (!is_tax_account($gl_line->gl_code) || $supp_trans->tax_included)
				$supp_trans->ov_amount += $gl_line->amount;
		}
	}
}

function invoice_header(&$supp_trans) {
	global $Ajax, $Refs;
	
	// if vars have been lost, recopy
	if (!isset($_POST['tran_date']))
		copy_from_trans($supp_trans);

	start_outer_table(TABLESTYLE2, "width='95%'");

	table_section(1);

	if ($supp_trans->trans_type == ST_SUPPCREDIT && $supp_trans->src_docs) {
		$_POST['supplier_id'] = $supp_trans->supplier_id;
		$supp = $supp_trans->supplier_name." - ".$supp_trans->currency;
		label_row(_('Supplier:'), $supp.hidden('supplier_id', $_POST['supplier_id'], false));
	}
	else {
		if (!isset($_POST['supplier_id']) && (get_global_supplier() != ALL_TEXT))
			$_POST['supplier_id'] = get_global_supplier();

		if (!$supp_trans->trans_no)
			supplier_list_row(_('Supplier:'), 'supplier_id', $_POST['supplier_id'], false, true);
		else
			label_row(_('Supplier:'), $supp_trans->supplier_name.($supp_trans->currency ? ' - '.$supp_trans->currency : '').hidden('supplier_id', $_POST['supplier_id'], false));
	}
	if ($supp_trans->supplier_id != $_POST['supplier_id']) {
		copy_to_trans($supp_trans);
		// supplier has changed
		// delete all the order items - drastic but necessary because of
		// change of currency, etc
		$supp_trans->clear_items();
		read_supplier_details_to_trans($supp_trans, $_POST['supplier_id']);
		copy_from_trans($supp_trans);
	}

	date_row(_('Date').':', 'tran_date', '', true, 0, 0, 0, '', true);

	ref_row(_('Reference:'), 'reference', '', $Refs->get_next($supp_trans->trans_type, null, array('supplier' => get_post('supplier_id'), 'date' => get_post('tran_date'))), false, $supp_trans->trans_type);

	if ($supp_trans->trans_type == ST_SUPPCREDIT)
		label_row(_('Source Invoices:'), $supp_trans->src_docs ? implode(',' ,$supp_trans->src_docs) : '&nbsp;', '', '', 'src_docs');
	text_row(_("Supplier's Ref.:"), 'supp_reference', $_POST['supp_reference'], 20, 60);

	table_section(2, '33%');

	if (isset($_POST['_tran_date_changed'])) {
		$Ajax->activate('_ex_rate');
		$supp_trans->tran_date = $_POST['tran_date'];
		get_duedate_from_terms($supp_trans);
		$_POST['due_date'] = $supp_trans->due_date;
		$Ajax->activate('due_date');
	}

	date_row(_('Due Date').':', 'due_date');

	label_row(_('Terms:'), $supp_trans->terms['description']);

	if (get_company_pref('use_dimension'))
		dimensions_list_row(_('Dimension').':', 'dimension', null, true, _('Default'), false, 1);

	if (get_company_pref('use_dimension') == 2)
		dimensions_list_row(_('Dimension 2').':', 'dimension2', null, true, _('Default'), false, 2);

	table_section(3, '33%');

	set_global_supplier($_POST['supplier_id']);

	$company_currency = get_company_currency();

	if ($supp_trans->currency != $company_currency) {
		label_row(_("Supplier's Currency:"), '<b>'.$supp_trans->currency.'</b>');
		exchange_rate_display($company_currency, $supp_trans->currency, $_POST['tran_date']);
	}

	label_row(_('Tax Group:'), $supp_trans->tax_description);
	supplier_credit_row($supp_trans->supplier_id, $supp_trans->credit);

	end_outer_table(1);
}

function invoice_totals(&$supp_trans) {
	global $Ajax;

	copy_to_trans($supp_trans);

	$colspan = 2;
	div_start('tax_table');
	start_table(TABLESTYLE, "width='95%'");
	label_row(_('Sub-total:'), price_format( $supp_trans->ov_amount), 'width=80% colspan='.$colspan.' align=right', 'width=10% align=right', 2);

	$taxes = $supp_trans->get_taxes($supp_trans->tax_group_id);
	$tax_total = display_edit_tax_items($taxes, $colspan, $supp_trans->tax_included, 0, true);

	$display_total = price_format($supp_trans->ov_amount + $tax_total);
	start_row();
	if ($supp_trans->trans_type == ST_SUPPINVOICE)
		label_cells(_('Invoice Total:'), $display_total, "colspan=".$colspan." width=80% align=right style='font-weight:bold;'", "width=10% align=right style='font-weight:bold;'");
	else
		label_cells(_('Credit Note Total'), $display_total, "colspan=".$colspan." width=80% align=right style='font-weight:bold;color:red;'", "width=10% nowrap align=right style='font-weight:bold;color:red;'");
	submit_cells('update', _('Update'), "width=10% colspan=2 align='right'", _('Refresh'), true);
	end_row();
	end_table(1);
	start_table(TABLESTYLE2);
	textarea_row(_('Memo:'), 'Comments', null, 50, 3);
	end_table(1);
	div_end();
}

function display_gl_controls(&$supp_trans, $k) {
	$accs = get_supplier_accounts($supp_trans->supplier_id);
	if (!isset($_POST['gl_code']))
		$_POST['gl_code'] = $accs['purchase_account'] ? $accs['purchase_account'] : get_company_pref('default_cogs_act');

	alt_table_row_color($k);
	echo gl_all_accounts_list('gl_code', null, false, true);
	$dim = get_company_pref('use_dimension');
	if ($dim >= 1)
		dimensions_list_cells(null, 'dimension_id', null, true, ' ', false, 1);
	if ($dim > 1)
		dimensions_list_cells(null, 'dimension2_id', null, true, ' ', false, 2);
	amount_cells(null, 'amount');
	if ($dim < 1)
		text_cells_ex(null, 'memo_', 35, 50, null, null, null, hidden('dimension_id', 0, false).hidden('dimension2_id', 0, false));
	elseif ($dim < 2)
		text_cells_ex(null, 'memo_', 35, 50, null, null, null, hidden('dimension2_id', 0, false));
	else
		text_cells_ex(null, 'memo_', 35, 50, null, null, null);
	submit_cells('AddGLCodeToTrans', _('Add'), '', _('Add GL Line'), true);
	submit_cells('ClearFields', _('Reset'), '', _('Clear all GL entry fields'), true);
	end_row();	
}

/*
*  $mode = 0 none at the moment
*		 = 1 display on invoice/credit page
*		 = 2 display on view invoice
*		 = 3 display on view credit
*/

function display_gl_items(&$supp_trans, $mode=0) {
	global $path_to_root, $Ajax;

	// if displaying in form, and no items, exit
	if (($mode == 2 || $mode == 3) && count($supp_trans->gl_codes) == 0)
		return 0;

	if ($supp_trans->trans_type == ST_SUPPINVOICE)
		$heading = _('GL Items for this Invoice');
	else
		$heading = _('GL Items for this Credit Note');

	start_outer_table(TABLESTYLE, "width='95%'");

	if ($mode == 1) {
		$qes = has_quick_entries(QE_SUPPINV);
		if ($qes !== false) {
			echo "<div style='float:right;'>";
			echo _('Quick Entry:').'&nbsp;'; 
			echo quick_entries_list('qid', null, QE_SUPPINV, true);
			$qid = get_quick_entry(get_post('qid'));
			if (list_updated('qid')) {
				unset($_POST['totamount']); // enable default
				$Ajax->activate('totamount');
			}
			echo '&nbsp;'.$qid['base_desc'].':'.'&nbsp;';

			$amount = input_num('totamount', $qid['base_amount']);
			$dec = user_price_dec();
			echo "<input class='amount' type='text' name='totamount' size='7' maxlength='12' dec='$dec' value='$amount'>&nbsp;";
			submit('go', _('Go'), true, false, true);
			echo "</div>";
		}	
	}
	display_heading($heading);

	end_outer_table(0, false);

	div_start('gl_items');
	start_table(TABLESTYLE, "width='95%'");

	$dim = get_company_pref('use_dimension');
	if ($dim == 2)
		$th = array(_('Account'), _('Name'), _('Dimension').' 1', _('Dimension').' 2', _('Amount'), _('Memo'));
	elseif ($dim == 1)
		$th = array(_('Account'), _('Name'), _('Dimension'), _('Amount'), _('Memo'));
	else
		$th = array(_('Account'), _('Name'), _('Amount'), _('Memo'));

	if ($mode == 1) {
		$th[] = '';
		$th[] = '';
	}	
	table_header($th);
	$total_gl_value = 0;
	$total = 0;
	$i = 0;
	$k = 0;

	if (count($supp_trans->gl_codes) > 0) {

		foreach ($supp_trans->gl_codes as $entered_gl_code) {

			alt_table_row_color($k);

			if ($mode == 3)
				$entered_gl_code->amount = -$entered_gl_code->amount;

			label_cell($entered_gl_code->gl_code);
			label_cell($entered_gl_code->gl_act_name);

			if ($dim >= 1)
				label_cell(get_dimension_string($entered_gl_code->gl_dim, true));
			if ($dim > 1)
				label_cell(get_dimension_string($entered_gl_code->gl_dim2, true));

			amount_cell($entered_gl_code->amount, true);
			label_cell($entered_gl_code->memo_);

			if ($mode == 1) {
				delete_button_cell('Delete2'.$entered_gl_code->Counter, _('Delete'), _('Remove line from document'));
				edit_button_cell('Edit'.$entered_gl_code->Counter, _('Edit'), _('Edit line from document'));
			}	
			end_row();
			if ($mode > 1) {
				if ($supp_trans->tax_included || !is_tax_account($entered_gl_code->gl_code))
					$total_gl_value += $entered_gl_code->amount;
			}		
			else	
				$total_gl_value += $entered_gl_code->amount;
			$total += $entered_gl_code->amount;	
			$i++;
			if ($i > 15) {
				$i = 0;
				table_header($th);
			}
		}

	}
	if ($mode == 1)
		display_gl_controls($supp_trans, $k);
	$colspan = ($dim == 2 ? 4 : ($dim == 1 ? 3 : 2));
	label_row(_('Total'), price_format($total), "colspan=".$colspan." align=right", "nowrap align=right", ($mode == 1 ? 3 : 1));

	end_table(1);
	div_end();

	return $total_gl_value;
}

function display_grn_items_for_selection(&$supp_trans, $k) {
	if ($supp_trans->trans_type == ST_SUPPINVOICE)	// outstanding grns and eventually for selected invoice
		$result = get_grn_items(0, $supp_trans->supplier_id, true, false, $supp_trans->trans_no);
	else { // only invoiced
		if (isset($_POST['receive_begin']) && isset($_POST['receive_end']))
			$result = get_grn_items(0, $supp_trans->supplier_id, false, true, 0, $_POST['receive_begin'], $_POST['receive_end']);
		elseif ($supp_trans->src_docs)
			$result = get_grn_items(0, $supp_trans->supplier_id, false, true, array_keys($supp_trans->src_docs));
		else
			$result = get_grn_items(0, $supp_trans->supplier_id, false, true);
	}
	if (db_num_rows($result) == 0)
		return false;

	/*Set up a table to show the outstanding GRN items for selection */
	while ($myrow = db_fetch($result)) {
		$grn_already_on_invoice = false;

		foreach ($supp_trans->grn_items as $entered_grn) {
			if ($entered_grn->id == $myrow['id'])
				$grn_already_on_invoice = true;
		}

		if ($grn_already_on_invoice == false) {
			alt_table_row_color($k);

			$n = $myrow['id'];
			label_cell(get_trans_view_str(ST_SUPPRECEIVE, $myrow['grn_batch_id']), "nowrap align='right'");
			hidden('qty_recd'.$n, $myrow['qty_recd']);
			hidden('item_code'.$n, $myrow['item_code']);
			hidden('item_description'.$n, $myrow['description']);
			hidden('prev_quantity_inv'.$n, $myrow['quantity_inv'] ? $myrow['quantity_inv'] : 0);
			hidden('order_price'.$n, $myrow['unit_price']);
			hidden('po_detail_item'.$n, $myrow['po_detail_item']);
			label_cell(get_trans_view_str(ST_PURCHORDER, $myrow['purch_order_no']), "nowrap align='right'");
			label_cell($myrow['item_code']);
			label_cell($myrow['description']);
			label_cell(sql2date($myrow['delivery_date']));
			$dec = get_qty_dec($myrow['item_code']);
			qty_cell($myrow['qty_recd'], false, $dec);
			qty_cell($myrow['quantity_inv'], false, $dec);
			if ($supp_trans->trans_type == ST_SUPPINVOICE)
				qty_cells(null, 'this_quantity_inv'.$n, number_format2($myrow['qty_recd'] - $myrow['quantity_inv'], $dec), 
					null, null, $dec);
			else
				qty_cells(null, 'This_QuantityCredited'.$n, number_format2(max($myrow['quantity_inv'], 0), $dec), 
					null, null, $dec);
			$dec2 = 0;
			if ($supp_trans->trans_type == ST_SUPPINVOICE) {
				amount_cells(null, 'ChgPrice'.$n, price_decimal_format($myrow['unit_price'], $dec2), null, null, $dec2);
				amount_cell(round2($myrow['unit_price'] * ($myrow['qty_recd'] - $myrow['quantity_inv']), user_price_dec()));
			}
			else {
				amount_cells(null, 'ChgPrice'.$n, price_decimal_format($myrow['act_price'], $dec2), null, null, $dec2);
				amount_cell(round2($myrow['act_price'] * max($myrow['quantity_inv'], 0), user_price_dec()));
			}	
			if ($supp_trans->trans_type == ST_SUPPINVOICE)
				submit_cells('grn_item_id'.$n, _('Add'), '', _('Add to Invoice'), true);
			else	
				submit_cells('grn_item_id'.$n, _('Add'), '', _('Add to Credit Note'), true);
			if (($supp_trans->trans_type == ST_SUPPINVOICE) && $_SESSION['wa_current_user']->can_access('SA_GRNDELETE')) {	// Added 2008-10-18 by Joe Hunt. Special access rights needed.
				submit_cells('void_item_id'.$n, _('Remove'), '', _('WARNING! Be careful with removal. The operation is executed immediately and cannot be undone !!!'), true);
				submit_js_confirm('void_item_id'.$n, sprintf(_('You are about to remove all yet non-invoiced items from delivery line #%d. This operation also irreversibly changes related order line. Do you want to continue ?'), $n));
			}
			end_row();
		}
	}
	return true;
}

/* 
*  $mode = 0 none at the moment
*		 = 1 display on invoice/credit page
*		 = 2 display on view invoice
*		 = 3 display on view credit
*/
function display_grn_items(&$supp_trans, $mode=0) {
	global $path_to_root;

	$ret = true;
	// if displaying in form, and no items, exit
	if (($mode == 2  || $mode == 3) && count($supp_trans->grn_items) == 0)
		return 0;

	start_outer_table("style='border:1px solid #cccccc;' width='95%'");

	$heading2 = '';
	if ($mode == 1) {
		if ($supp_trans->trans_type == ST_SUPPINVOICE) {
			$heading = _('Items Received Yet to be Invoiced');
			if ($_SESSION['wa_current_user']->can_access('SA_GRNDELETE'))	// Added 2008-10-18 by Joe Hunt. Only admins can remove GRNs
				$heading2 = _('WARNING! Be careful with removal. The operation is executed immediately and cannot be undone !!!');
		}
		else
			$heading = _('Delivery Item Selected For Adding To A Supplier Credit Note');
	}
	else {
		if ($supp_trans->trans_type == ST_SUPPINVOICE)
			$heading = _('Received Items Charged on this Invoice');
		else
			$heading = _('Received Items Credited on this Note');
	}

	display_heading($heading);

	if ($mode == 1) {
		if ($supp_trans->trans_type == ST_SUPPCREDIT) {
			echo '</td>';
			date_cells(_('Received between'), 'receive_begin', '', null, -user_transaction_days(), 0, 0, 'valign=middle');
			date_cells(_('and'), 'receive_end', '', null, 1, 0, 0, 'valign=middle');
			submit_cells('RefreshInquiry', _('Search'), '', _('Refresh Inquiry'), true);
			echo '<td>';
		}

		if ($heading2 != '')
			display_note($heading2, 0, 0, "class='overduefg'");
		
		echo "</td><td width='10%' align='right'>";
		submit('InvGRNAll', _('Add All Items'), true, false, true);
	}

	end_outer_table(0, false);

	div_start('grn_items');
	start_table(TABLESTYLE, "width='95%'");
	if ($mode == 1) {
		$th = array(_('Delivery'), _('P.O.'), _('Item'), _('Description'), _('Received On'), _('Quantity Received'), _('Quantity Invoiced'), _('Qty Yet To Invoice'), $supp_trans->tax_included ? _('Price after Tax') : _('Price before Tax'), _('Total'), '');
		if (($supp_trans->trans_type == ST_SUPPINVOICE) && $_SESSION['wa_current_user']->can_access('SA_GRNDELETE'))
			$th[] = '';
		if ($supp_trans->trans_type == ST_SUPPCREDIT)
			$th[7] = _('Qty Yet To Credit');
	}
	else
		$th = array(_('Delivery'), _('Item'), _('Description'), _('Quantity'), _('Price'), _('Line Value'));

	table_header($th);
	$total_grn_value = 0;
	$i = $k = 0;

	if (count($supp_trans->grn_items) > 0) {

		foreach ($supp_trans->grn_items as $entered_grn) {

			alt_table_row_color($k);

			$grn_batch = get_grn_batch_from_item($entered_grn->id);
			label_cell(get_trans_view_str(ST_SUPPRECEIVE, $grn_batch), "nowrap align='right'");
			if ($mode == 1) {
				$row = get_grn_batch($grn_batch);
				label_cell(get_trans_view_str(ST_PURCHORDER, $row['purch_order_no']), "nowrap align='right'"); // PO
			}	
			label_cell($entered_grn->item_code);
			label_cell($entered_grn->item_description);
			$dec = get_qty_dec($entered_grn->item_code);
			if ($mode == 1) {
				label_cell(sql2date($row['delivery_date']));
				qty_cell($entered_grn->qty_recd, false, $dec);
				qty_cell($entered_grn->prev_quantity_inv, false, $dec);
			}
			qty_cell(abs($entered_grn->this_quantity_inv), true, $dec);
			amount_decimal_cell($entered_grn->chg_price);
			amount_cell( round2($entered_grn->chg_price * abs($entered_grn->this_quantity_inv), user_price_dec()), true);

			if ($mode == 1) {
				delete_button_cell('Delete' . $entered_grn->id, _('Edit'), _('Edit document line'));
				if (($supp_trans->trans_type == ST_SUPPINVOICE) && $_SESSION['wa_current_user']->can_access('SA_GRNDELETE'))	  
					label_cell('');
			}	
			end_row();

			$total_grn_value += round2($entered_grn->chg_price * abs($entered_grn->this_quantity_inv),
			   user_price_dec());

			$i++;
			if ($i > 15) {
				$i = 0;
				table_header($th);
			}
		}
	}
	if ($mode == 1) {
		$ret = display_grn_items_for_selection($supp_trans, $k);
		$colspan = 9;
	}
	else
		$colspan = 5;
	label_row(_('Total'), price_format($total_grn_value),
		'colspan='.$colspan.' align=right', 'nowrap align=right');
	if (!$ret) {
		start_row();
		echo '<td colspan='.($colspan + 1).'>';
		if ($supp_trans->trans_type == ST_SUPPINVOICE)
			display_note(_('There are no outstanding items received from this supplier that have not been invoiced by them.'), 0, 0);
		else {
			display_note(_('There are no received items for the selected supplier that have been invoiced.'));
			display_note(_('Credits can only be applied to invoiced items.'), 0, 0);
		}
		echo '</td>';
		end_row();
	}	
	end_table(1);
	div_end();

	return $total_grn_value;
}

function get_duedate_from_terms(&$trans) {
	$date = get_class($trans) == 'purch_order' ? $trans->orig_order_date : $trans->tran_date;

	if (!is_date($date))
		$date = Today();
	
	if ($trans->terms['day_in_following_month']) /*Its a day in the following month when due */
		$trans->due_date = add_days(end_month($date), $trans->terms['day_in_following_month']);
	else /*Use the Days Before Due to add to the invoice date */
		$trans->due_date = add_days($date, $trans->terms['days_before_due']);
}
