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
include_once($path_to_root.'/includes/prefs/sysprefs.inc');
include_once($path_to_root.'/inventory/includes/inventory_db.inc');

class items_cart {
	var $trans_type;
	var $line_items;
	var $gl_items;

	var $order_id;

	var $from_loc;
	var $to_loc;
	var $tran_date;
	var $doc_date;
	var $event_date;
	var $transfer_type;
	var $increase;
	var $memo_;
	var $branch_id;
	var $reference;
	var $original_amount;
	var $currency;
	var $rate;
	var $source_ref;
	var $vat_category;

	var $tax_info;  // tax info for the GL transaction

	var $fixed_asset;

	function __construct($type, $trans_no=0) {
		$this->trans_type = $type;
		$this->order_id = $trans_no;
		$this->clear_items();
		if (in_array($type, array(ST_LOCTRANSFER, ST_INVADJUST, ST_COSTUPDATE, ST_MANUISSUE, ST_MANURECEIVE, ST_JOURNAL)))
			$this->currency = get_company_pref('curr_default');
		$this->rate = 1;
	}

	// --------------- line item functions

	function add_to_cart($line_no, $stock_id, $qty, $standard_cost, $description=null) {

		if (isset($stock_id) && $stock_id != '' && isset($qty)) {
			$this->line_items[$line_no] = new line_item($stock_id, $qty, $standard_cost, $description);
			return true;
		}
		else // shouldn't come here under normal circumstances
			display_error('unexpected - adding an invalid item or null quantity', '', true);

		return false;
	}

	function find_cart_item($stock_id) {
		foreach($this->line_items as $line_no=>$line) {
			if ($line->stock_id == $stock_id)
				return $this->line_items[$line_no];
		}
		return null;
	}

	function update_cart_item($line_no, $qty, $standard_cost) {
		$this->line_items[$line_no]->quantity = $qty;
		$this->line_items[$line_no]->standard_cost = $standard_cost;
	}

	function remove_from_cart($line_no) {
		array_splice($this->line_items, $line_no, 1);
	}

	function count_items() {
		return count($this->line_items);
	}

	function check_qoh($location, $date_, $reverse=false) {
		global $SysPrefs;

		$low_stock = array();

		if (!$SysPrefs->allow_negative_stock()) {
			foreach ($this->line_items as $line_no => $line_item)
				if (has_stock_holding($line_item->mb_flag) || is_fixed_asset($line_item->mb_flag)) {
					$quantity = $line_item->quantity;
					if ($reverse)
						$quantity = -$line_item->quantity;

					if ($quantity >= 0)
						continue;

					if (check_negative_stock($line_item->stock_id, $quantity, $location, $date_))
						$low_stock[] = $line_item->stock_id;
				}
		}
		return $low_stock;
	}

	// ----------- GL item functions

	function add_gl_item($code_id, $dimension_id, $dimension2_id, $amount, $memo='', $act_descr=null, $person_id=null, $date=null) {
		if (isset($code_id) && $code_id != '' && isset($amount) && isset($dimension_id) && isset($dimension2_id)) {
			$this->gl_items[] = new gl_item($code_id, $dimension_id, $dimension2_id, $amount, $memo, $act_descr, $person_id, $date);
			return true;
		}
		else // shouldn't come here under normal circumstances
			display_error("unexpected - invalid parameters in add_gl_item($code_id, $dimension_id, $dimension2_id, $amount,...)", '', true);

		return false;
	}

	function update_gl_item($index, $code_id, $dimension_id, $dimension2_id, $amount, $memo='', $act_descr=null, $person_id=null) {
		$this->gl_items[$index]->code_id = $code_id;
		$this->gl_items[$index]->person_id = $person_id;

		$gl_type = is_subledger_account($code_id);
		if ($person_id != null && $gl_type) {
			$this->gl_items[$index]->person_type_id = $gl_type > 0 ? PT_CUSTOMER : PT_SUPPLIER;
			$data = get_subaccount_data($code_id, $person_id);
			$this->gl_items[$index]->person_name = $data['name'];
			$this->gl_items[$index]->branch_id = $data['id'];
		}
		else
			$this->gl_items[$index]->person_type_id = $this->gl_items[$index]->person_name = '';
		
		$this->gl_items[$index]->dimension_id = $dimension_id;
		$this->gl_items[$index]->dimension2_id = $dimension2_id;
		$this->gl_items[$index]->amount = $amount;
		$this->gl_items[$index]->reference = $memo;

		if ($act_descr == null)
			$this->gl_items[$index]->description = get_gl_account_name($code_id);
		else
			$this->gl_items[$index]->description = $act_descr;
	}

	function remove_gl_item($index) {
		array_splice($this->gl_items, $index, 1);
	}

	function count_gl_items() {
		return count($this->gl_items);
	}

	function gl_items_total() {
		$total = 0;
		foreach ($this->gl_items as $gl_item)
			$total += $gl_item->amount;
		return $total;
	}

	function gl_items_total_debit() {
		$total = 0;
		foreach ($this->gl_items as $gl_item) {
			if ($gl_item->amount > 0)
				$total += $gl_item->amount;
		}
		return $total;
	}

	function gl_items_total_credit() {
		$total = 0;
		foreach ($this->gl_items as $gl_item) {
			if ($gl_item->amount < 0)
				$total += $gl_item->amount;
		}
		return $total;
	}

	// ------------ common functions

	function clear_items() {
		unset($this->line_items);
		$this->line_items = array();

		unset($this->gl_items);
		$this->gl_items = array();
	}
	//
	//  Check if cart contains virtual subaccount (AP/AR) postings
	//
	function has_sub_accounts() {
		foreach ($this->gl_items as $gl_item) {
			if (is_subledger_account($gl_item->code_id))
				return true;
		}
		return false;
	}

	//
	//  Check if cart contains postings to tax accounts
	//
	function has_taxes() {
		foreach ($this->gl_items as $gl_item) {
			if (is_tax_account($gl_item->code_id))
				return true;
		}
		return false;
	}

	/*
	Collect tax info from the GL transaction lines and return as array of values:
		'tax_date'      - tax date
		'tax_group'     - related counterparty tax group
		'tax_category'  - tax category (not set for now)
		'net_amount' - tax amounts array indexed by tax type id
		'tax_in', 'tax_out' - tax amounts array indexed by tax type id
		'tax_reg' - tax register used
	*/
	function collect_tax_info() {
		$tax_info = array();
		$subledger_sum = $net_sum = 0;

		$tax_info['tax_date'] = $this->tran_date;
		$vat_percent = get_company_pref('partial_vat_percent');
		$factor = $vat_percent && ($this->vat_category == VC_PARTIAL) ? $vat_percent/100: 1;

		foreach($this->gl_items as $gl) {
			// Todo:
			// More future person types will be added here
			// Function is_subledger_account() needs to be modified also to add person types
			if ($person_type = is_subledger_account($gl->code_id)) {
				$tax_info['person_type'] = $person_type < 0 ? PT_SUPPLIER : PT_CUSTOMER;
				$tax_info['person_id'] = $gl->person_id;

				if(!empty($gl->person_id)) { 
					if ($tax_info['person_type'] == PT_CUSTOMER) {
						$branch = get_default_branch($gl->person_id);
						$tax_info['tax_group'] = $branch['tax_group_id'];
					}
					else {
						$supplier = get_supplier($gl->person_id);
						$tax_info['tax_group'] = $supplier['tax_group_id'];
					}
				}
				
				$subledger_sum += $gl->amount;
			}
			elseif ($tax_id = is_tax_account($gl->code_id)) {
				$tax_type = get_tax_type($tax_id);
				if ($gl->code_id == $tax_type['purchasing_gl_code'] && $gl->amount > 0) {
					if (!isset($tax_info['tax_in'][$tax_id]))
						$tax_info['tax_in'][$tax_id] = 0;
					$tax_info['tax_in'][$tax_id] += $gl->amount;
					$tax_info['tax_reg'] = TR_INPUT;
				}
				else {
					if (!isset($tax_info['tax_out'][$tax_id]))
						$tax_info['tax_out'][$tax_id] = 0;
					$tax_info['tax_out'][$tax_id] -= $gl->amount;
					if (!isset($tax_info['tax_reg'])) // TR_INPUT has priority (EU are posted on both accounts)
						$tax_info['tax_reg'] = TR_OUTPUT;
				}
				if ($tax_type['rate']) {
					// assume transaction adjustment for negative tax in/out
					$sign = (@$tax_info['tax_in'][$tax_id] < 0 || @$tax_info['tax_out'][$tax_id] < 0) ? -1 : 1;
					// we can have both input and output tax postings in some cases like intra-EU trade.
					// so just calculate net_amount from the higher in/out tax
					$tax_info['net_amount'][$tax_id]
						= $sign*round2(max(abs(@$tax_info['tax_in'][$tax_id]), abs(@$tax_info['tax_out'][$tax_id]))/$tax_type['rate']*100, user_price_dec())/$factor;
				}
			}
			else
				$net_sum += $gl->amount;
		}
		// if no tax amount posted guess register type from person_type used (e.g. export invoice)
		if (!isset($tax_info['tax_reg']) && isset($tax_info['person_type']))
			$tax_info['tax_reg'] = $tax_info['person_type']==PT_CUSTOMER ? TR_OUTPUT : TR_INPUT;

		if (count_array(@$tax_info['net_amount'])) { // guess exempt sales/purchase if any tax has been found
			$ex_net = abs($net_sum) - @array_sum($tax_info['net_amount']);
			if ($ex_net != 0)
				$tax_info['net_amount_ex'] = $ex_net;
		}

		return $tax_info;
	}

	function set_currency($curr, $rate=0) {
		$this->currency = $curr;
		$this->rate = $rate;
	}

	/*
		Reduce number of necessary gl posting lines.
	*/
	function reduce_gl() {
		/* reduce additional postings */
		$codes = array();
		foreach($this->gl_items as $n => $gl) {
			$prev = @$codes[$gl->code_id][$gl->person_id][$gl->dimension_id][$gl->dimension2_id][$gl->reference];
			if (isset($prev)) { // add amount to previous line for the same gl_code dims and memo
				$this->gl_items[$prev]->amount += $gl->amount;
				if ($this->gl_items[$prev]->amount == 0) // discard if overall amount==0
					unset($this->gl_items[$prev], $codes[$gl->code_id][$gl->person_id][$gl->dimension_id][$gl->dimension2_id][$gl->reference]);
				
				unset($this->gl_items[$n]);
			}
			else
				$codes[$gl->code_id][$gl->person_id][$gl->dimension_id][$gl->dimension2_id][$gl->reference] = $n;
		}
	}
	/*
		Write transaction GL postings, creating tax records and updating AP/AR/bank ledger if needed.
	*/
	function write_gl($check_balance = true) {
		$delta = $this->gl_items_total();
		if ($check_balance && floatcmp($delta, 0) !=0) {
			$this->add_gl_item(get_company_pref($delta>0 ? 'rounding_db_act' : 'rounding_cr_act'), 0, 0, -$delta, '');
			error_log(sprintf( _('Rounding error %s encountered for trans_type:%s,trans_no:%s'), $delta, $this->trans_type, $this->order_id));
		}

		$bank_trans = $supp_trans = $cust_trans = array();
		$total_gl = 0;
		foreach($this->gl_items as $gl) {
			if (!isset($gl->date))
				$gl->date = $this->tran_date;

			$total_gl += add_gl_trans($this->trans_type, $this->order_id, $gl->date, $gl->code_id, $gl->dimension_id, $gl->dimension2_id, 
				$gl->reference, $gl->amount, $this->currency, $gl->person_type_id, $gl->person_id, '', $this->rate);

			// post to first found bank account using given gl acount code.
			$is_bank_to = is_bank_account($gl->code_id);
			if ($is_bank_to && (get_bank_account_currency($is_bank_to) == $this->currency)) { // do not register exchange variations in bank trans
				if (!isset($bank_trans[$is_bank_to]))
					$bank_trans[$is_bank_to] = 0;

				$bank_trans[$is_bank_to] += $gl->amount;
			}
			elseif ($gl->person_id) {
				$home_currency = get_company_currency();
				// collect per counterparty amounts (in case more than one posting was done to the account),
				// do not post exchange variations to AR/AP (journal in not customer/supplier currency)
				if ($gl->person_type_id==PT_SUPPLIER && (get_supplier_currency($gl->person_id) == $this->currency || $this->currency != $home_currency))
					$supp_trans[$gl->person_id] = @$supp_trans[$gl->person_id] + $gl->amount;
				elseif ($gl->person_type_id==PT_CUSTOMER && (get_customer_currency(null, $gl->branch_id) == $this->currency || $this->currency != $home_currency))
					$cust_trans[$gl->branch_id] = @$cust_trans[$gl->branch_id] + $gl->amount;
			}
		}
		// post currency roundings if any
		if ($check_balance && floatcmp($total_gl, 0))
			add_gl_trans($this->trans_type, $this->order_id, $this->tran_date, get_company_pref($total_gl>0 ? 'rounding_db_act' : 'rounding_cr_act'), 0, 0, _('Exchange rate roundings'), -$total_gl);

		// update bank ledger if used
		foreach($bank_trans as $bank_id => $amount)
			add_bank_trans($this->trans_type, $this->order_id, $bank_id, $this->reference, $this->tran_date, $amount, 0, '', $this->currency, 'Cannot insert a destination bank transaction');

		// add AP/AR for journal transaction
		if ($this->trans_type == ST_JOURNAL) {
			// update AR
			foreach($cust_trans as $branch_id => $amount)
				if (floatcmp($amount, 0))
					write_cust_journal($this->trans_type, $this->order_id, $branch_id, $this->tran_date, $this->reference, $amount, $this->rate);
			// update AP
			foreach($supp_trans as $supp_id => $amount)
				if (floatcmp($amount, 0))
					write_supp_journal($this->trans_type, $this->order_id, $supp_id, $this->tran_date, $this->reference, -$amount, $this->rate, $this->source_ref);
		}

		// generate tax records for journal transaction
		if ($this->trans_type == ST_JOURNAL && is_array($this->tax_info)) {
			foreach($this->tax_info['net_amount'] as $tax_id => $net) {
				if (!$net)
					continue;

				// in EU VAT system intra-community goods aquisition is posted to both purchasing and sales tax accounts,
				// but included only in purchase register. To avoid double registering ELSE is used below!
				if (isset($this->tax_info['tax_in'][$tax_id])) {
					$tax = $this->tax_info['tax_in'][$tax_id];
					$reg = TR_INPUT;
				}
				elseif (isset($this->tax_info['tax_out'][$tax_id])) {
					$tax = $this->tax_info['tax_out'][$tax_id];
					$reg = TR_OUTPUT;
				}
				elseif (isset($this->tax_info['tax_reg'])) {// e.g. export
					$tax = 0;
					$reg = $this->tax_info['tax_reg'];
				}
				else
					continue;

				$tax_nominal = $this->tax_info['rate'][$tax_id]/100*$net;
				add_trans_tax_details($this->trans_type, $this->order_id, $tax_id, $this->tax_info['rate'][$tax_id], 0, $tax_nominal, $net, $this->rate, $this->tran_date, $this->source_ref, $reg);
			}
		}
	}
}

//--------------------------------------------------------------------------------------------

class line_item {
	var $stock_id;
	var $item_description;
	var $units;
	var $mb_flag;

	var $quantity;
	var $price;
	var $standard_cost;

	function __construct($stock_id, $qty, $standard_cost=null, $description=null) {
		$item_row = get_item($stock_id);

		if ($item_row == null)
			display_error("invalid item added to order : $stock_id", '');

		$this->mb_flag = $item_row['mb_flag'];
		$this->units = $item_row['units'];

		if ($description == null)
			$this->item_description = $item_row['description'];
		else
			$this->item_description = $description;

		if ($standard_cost == null)
			$this->standard_cost = $item_row['purchase_cost'];
		else
			$this->standard_cost = $standard_cost;

		$this->stock_id = $stock_id;
		$this->quantity = $qty;
		//$this->price = $price;
		$this->price = 0;
	}
}

//---------------------------------------------------------------------------------------

class gl_item {

	var $code_id;
	var $dimension_id;
	var $dimension2_id;
	var $amount;
	var $reference;
	var $description;
	var $person_id;
	var $person_type_id;
	var $person_name;
	var $branch_id;
	var $date;

	function __construct($code_id=null, $dimension_id=0, $dimension2_id=0, $amount=0, $memo='', $act_descr=null, $person_id=null, $date=null) {
		//echo "adding $index, $code_id, $dimension_id, $amount, $reference<br>";

		if ($act_descr == null && $code_id)
			$this->description = get_gl_account_name($code_id);
		else
			$this->description = $act_descr;

		$this->code_id = $code_id;
		$this->person_id = $person_id;
		$gl_type = is_subledger_account($code_id);
		if ($person_id != null  && $gl_type) {
			$this->person_type_id = $gl_type > 0 ? PT_CUSTOMER : PT_SUPPLIER;
			$data = get_subaccount_data($code_id, $person_id);
			$this->person_name = $data['name'];
			$this->branch_id = $data['id'];
		}
		$this->dimension_id = $dimension_id;
		$this->dimension2_id = $dimension2_id;
		$this->amount = round2($amount, user_price_dec());
		$this->reference = $memo;
		$this->date = $date;
	}
}
