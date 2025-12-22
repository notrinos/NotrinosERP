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
//----------------------------------------------------------------------------------------
// if ($writeoff_acc==0) return goods into $cart->Location
// if src_docs!=0 => credit invoice else credit note
//
function write_credit_note(&$credit_note, $write_off_acc) {
	global $Refs;

	if (is_array($credit_note->src_docs)) {
		$docs = array_keys($credit_note->src_docs);
		$credit_invoice = reset($docs);
	}
	else
		$credit_invoice = $credit_note->src_docs;

	$credit_date = $credit_note->document_date;
	$tax_group_id = $credit_note->tax_group_id;

	$trans_no = $credit_note->trans_no;
	if (is_array($trans_no))
		$trans_no = key($trans_no);
	
	$credit_type = $write_off_acc == 0 ? 'Return' : 'WriteOff';

	begin_transaction();
	hook_db_prewrite($credit_note, ST_CUSTCREDIT);

	$company_data = get_company_prefs();
	$branch_data = get_branch_accounts($credit_note->Branch);

	add_new_exchange_rate(get_customer_currency($credit_note->customer_id), $credit_date, $credit_note->ex_rate);

	$credit_note_total = $credit_note->get_items_total_dispatch();
	$freight_tax = $credit_note->get_shipping_tax();

	$taxes = $credit_note->get_taxes();

	$tax_total = 0;
	foreach ($taxes as $taxitem) {
		$taxitem['Value'] =  round2($taxitem['Value'], user_price_dec());
		$tax_total += $taxitem['Value'];
	}

	if ($credit_note->tax_included == 0) {
		$items_added_tax = $tax_total-$freight_tax;
		$freight_added_tax = $freight_tax;
	}
	else {
		$items_added_tax = 0;
		$freight_added_tax = 0;
	}
	// If the Customer Branch AR Account is set to a Bank Account,
	// the transaction will be settled at once.
	if (is_bank_account($branch_data['receivables_account']))
		$alloc = $credit_note_total + $items_added_tax + $credit_note->freight_cost + $freight_added_tax;
	else
		$alloc = 0;

	if (!isset($credit_note->order_no))
		$credit_note->order_no = 0;

	/*Now insert the Credit Note into the debtor_trans table with the allocations as calculated above*/
	// all amounts in debtor's currency
	$credit_no = write_customer_trans(ST_CUSTCREDIT, $trans_no, $credit_note->customer_id,
		$credit_note->Branch, $credit_date, $credit_note->reference,
		$credit_note_total, 0, $items_added_tax,
		$credit_note->freight_cost, $freight_added_tax,
		$credit_note->sales_type, $credit_note->order_no, $credit_note->ship_via,
		null, $alloc, 0, $credit_note->dimension_id, $credit_note->dimension2_id,
		$credit_note->payment, $credit_note->tax_included); 

	if ($trans_no==0)
		$credit_note->trans_no = array($credit_no=>0);
	else {
		delete_comments(ST_CUSTCREDIT, $credit_no);
		void_cust_allocations(ST_CUSTCREDIT, $credit_no, $credit_date);
		void_gl_trans(ST_CUSTCREDIT, $credit_no, true);
		void_stock_move(ST_CUSTCREDIT, $credit_no);
		void_trans_tax_details(ST_CUSTCREDIT, $credit_no);
	}
	if ($credit_invoice) {
		update_customer_trans_version(get_parent_type(ST_CUSTCREDIT), $credit_note->src_docs );

		$total = $credit_note_total + $credit_note->freight_cost + $items_added_tax 
			+ $freight_added_tax;

		if (!credit_sales_invoice_allocate($credit_invoice, $credit_no, $total, $credit_date))
			return false;
	}

	$total = 0;
	foreach ($credit_note->line_items as $credit_line) {

		if ($credit_invoice && $credit_line->qty_dispatched!=$credit_line->qty_old )
			update_parent_line(ST_CUSTCREDIT, $credit_line->src_id,($credit_line->qty_dispatched - $credit_line->qty_old));

		$line_taxfree_price = get_tax_free_price_for_item($credit_line->stock_id, $credit_line->price, 0, $credit_note->tax_included, $credit_note->tax_group_array);

		$line_tax = get_full_price_for_item($credit_line->stock_id, $credit_line->price, 0, $credit_note->tax_included, $credit_note->tax_group_array) - $line_taxfree_price;

		$credit_line->standard_cost = get_unit_cost($credit_line->stock_id); 
		
		write_customer_trans_detail_item(ST_CUSTCREDIT, $credit_no, $credit_line->stock_id,
			$credit_line->item_description, $credit_line->qty_dispatched,
			$credit_line->line_price(), $line_tax, $credit_line->discount_percent,
			$credit_line->standard_cost, $credit_line->src_id, $trans_no==0 ? 0:  $credit_line->id);

		if ($credit_type == 'Return')
			add_credit_movements_item($credit_note, $credit_line,
				$credit_type, ($line_taxfree_price+$line_tax)*(1-$credit_line->discount_percent), $credit_invoice);

		$total += add_gl_trans_credit_costs($credit_note, $credit_line, $credit_no,
			$credit_date, $credit_type, $write_off_acc, $branch_data);
	} /*end of credit_line loop */

	/*Post credit note transaction to GL credit debtors,
	debit freight re-charged and debit sales */

	if (($credit_note_total + $credit_note->freight_cost) != 0) {

		$total += add_gl_trans_customer(ST_CUSTCREDIT, $credit_no, $credit_date, $branch_data['receivables_account'], $credit_note->dimension_id, $credit_note->dimension2_id,
			-($credit_note_total + $credit_note->freight_cost + $items_added_tax + $freight_added_tax),
			$credit_note->customer_id,
			'The total debtor GL posting for the credit note could not be inserted');
	}

	if ($credit_note->freight_cost != 0) {
		$total += add_gl_trans_customer(ST_CUSTCREDIT, $credit_no, $credit_date, $company_data['freight_act'], $credit_note->dimension_id, $credit_note->dimension2_id,
			$credit_note->get_tax_free_shipping(), $credit_note->customer_id,
			'The freight GL posting for this credit note could not be inserted');
	}

	foreach ($taxes as $taxitem) {
		if ($taxitem['Net'] != 0) {

			$ex_rate = get_exchange_rate_from_home_currency(get_customer_currency($credit_note->customer_id), $credit_note->document_date);
			add_trans_tax_details(ST_CUSTCREDIT, $credit_no, $taxitem['tax_type_id'],
				$taxitem['rate'], $credit_note->tax_included, $taxitem['Value'],
				$taxitem['Net'], $ex_rate,
				$credit_note->document_date, $credit_note->reference, TR_OUTPUT);
			// sales_gl_code is not set for taxexempt
			if (isset($taxitem['sales_gl_code']))
				$total += add_gl_trans_customer(ST_CUSTCREDIT, $credit_no, $credit_date, $taxitem['sales_gl_code'], $credit_note->dimension_id, $credit_note->dimension2_id,
				$taxitem['Value'], $credit_note->customer_id,
				'A tax GL posting for this credit note could not be inserted');
		}
	}
	/*Post a balance post if $total != 0 */
	add_gl_balance(ST_CUSTCREDIT, $credit_no, $credit_date, -$total, PT_CUSTOMER, $credit_note->customer_id);	

	add_comments(ST_CUSTCREDIT, $credit_no, $credit_date, $credit_note->Comments);

	if ($trans_no == 0)
		$Refs->save(ST_CUSTCREDIT, $credit_no, $credit_note->reference);
	
	hook_db_postwrite($credit_note, ST_CUSTCREDIT);
	commit_transaction();

	return $credit_no;
}

//----------------------------------------------------------------------------------------
// Insert a stock movement coming back in to show the credit note and
// 	a reversing stock movement to show the write off
//
function add_credit_movements_item(&$credit_note, &$credit_line, $credit_type, $price, $credited_invoice=0) {
	//Chaitanya : Stamp current cost in stock moves $credit_line does not fetch cost
	$curr_std_cost = get_unit_cost($credit_line->stock_id);

	$reference = _('Return');
	if ($credited_invoice) 
		$reference .= ' '._('Ex Inv:').' ' . $credited_invoice;

	add_stock_move(ST_CUSTCREDIT,  $credit_line->stock_id,
		key($credit_note->trans_no), $credit_note->Location,
		$credit_note->document_date, $reference, $credit_line->qty_dispatched,
		$curr_std_cost, $price);
}

//----------------------------------------------------------------------------------------

function add_gl_trans_credit_costs($order, $order_line, $credit_no, $date_, $credit_type, $write_off_gl_code, &$branch_data) {
	$stock_gl_codes = get_stock_gl_code($order_line->stock_id);
	$customer = get_customer($order->customer_id);
	// If there is a Customer Dimension, then override with this,
	// else take the Item Dimension (if any)
	$dim = ($order->dimension_id != $customer['dimension_id'] ? $order->dimension_id : 
		($customer['dimension_id'] != 0 ? $customer['dimension_id'] : $stock_gl_codes['dimension_id']));
	$dim2 = ($order->dimension2_id != $customer['dimension2_id'] ? $order->dimension2_id : 
		($customer['dimension2_id'] != 0 ? $customer['dimension2_id'] : $stock_gl_codes['dimension2_id']));

	$total = 0;
	/* insert gl_trans to credit stock and debit cost of sales at standard cost*/
	$unit_cost = get_unit_cost($order_line->stock_id);

	if ($stock_gl_codes['mb_flag'] != 'D' && $unit_cost != 0) {
		/*first the cost of sales entry*/

		$total += add_gl_trans_std_cost(ST_CUSTCREDIT, $credit_no, $date_, $stock_gl_codes['cogs_account'],
			$dim, $dim2, '', -($unit_cost * $order_line->qty_dispatched),
			PT_CUSTOMER, $order->customer_id,
			'The cost of sales GL posting could not be inserted');

		/*now the stock entry*/
		if ($credit_type == 'WriteOff')
			$stock_entry_account = $write_off_gl_code;
		else {
			$stock_gl_code = get_stock_gl_code($order_line->stock_id);
			$stock_entry_account = $stock_gl_code['inventory_account'];
		}

		$total += add_gl_trans_std_cost(ST_CUSTCREDIT, $credit_no, $date_, $stock_entry_account, $dim, $dim2,
			'', ($unit_cost * $order_line->qty_dispatched),
			PT_CUSTOMER, $order->customer_id,
			'The stock side (or write off) of the cost of sales GL posting could not be inserted');

	} /* end of if GL and stock integrated and standard cost !=0 */

	if ($order_line->line_price() != 0) {

		$line_taxfree_price =
		get_tax_free_price_for_item($order_line->stock_id, $order_line->price,
		  0, $order->tax_included, $order->tax_group_array);

		$line_tax = get_full_price_for_item($order_line->stock_id, $order_line->price,
		  0, $order->tax_included, $order->tax_group_array) - $line_taxfree_price;

		//Post sales transaction to GL credit sales

		// If there is a Branch Sales Account, then override with this,
		// else take the Item Sales Account
		if ($branch_data['sales_account'] != '')
			$sales_account = $branch_data['sales_account'];
		else
			$sales_account = $stock_gl_codes['sales_account'];
		$total += add_gl_trans_customer(ST_CUSTCREDIT, $credit_no, $date_, $sales_account, $dim, $dim2,
			($line_taxfree_price * $order_line->qty_dispatched), $order->customer_id,
			'The credit note GL posting could not be inserted');

		if ($order_line->discount_percent != 0) {

			$total += add_gl_trans_customer(ST_CUSTCREDIT, $credit_no, $date_, $branch_data['sales_discount_account'],
				$dim, $dim2, -($line_taxfree_price * $order_line->qty_dispatched * $order_line->discount_percent),
				$order->customer_id,
				'The credit note discount GL posting could not be inserted');

		} /*end of if discount !=0 */
	} /*if line_price!=0 */
	return $total;
}