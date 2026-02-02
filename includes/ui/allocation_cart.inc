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
/*
	Class for supplier/customer payment/credit allocations edition
	and related helpers.
*/
//-----------------------------------------------------------------------------------

class allocation {

	var $trans_no; 
	var $type;
	var $person_id = '';
	var $person_name = '';
	var $person_type;	// PT_SUPPLIER/PT_CUSTOMER
	var $person_curr;
	var $date_;
	var $amount = 0; /*Total amount of the transaction in FX */
	var $bank_amount = 0;
	var $currency;

	var $allocs; /*array of transactions allocated to */

	function __construct($type, $trans_no, $person_id = null, $person_type_id=null) {
		$this->allocs = array();
		
		$this->trans_no = $trans_no;
		$this->type = $type;
		if ($person_id)
			$this->set_person($person_id, $person_type_id);

		$this->read($type, $trans_no, $person_id, $person_type_id); // read payment or credit
	}

	function set_person($person_id, $person_type) {
		$this->person_id = $person_id;
		$this->person_type = $person_type;
		$this->person_curr = $person_type == PT_SUPPLIER ?
			get_supplier_currency($person_id) : get_customer_currency($person_id);
		return $this->person_curr;
	}

	function add_item($type, $type_no, $date_, $due_date, $amount, $amount_allocated, $current_allocated, $ref) {
		if (floatcmp($amount, 0)) {
			$this->allocs[count($this->allocs)] = new allocation_item($type, $type_no, $date_, $due_date, $amount, $amount_allocated, $current_allocated, $ref);
			return true;
		} 
		else 
			return false;
	}
	
	function update_item($index, $type, $type_no, $date_, $due_date, $amount, $amount_allocated, $current_allocated, $ref) {
		if (floatcmp($amount, 0)) {
			$this->allocs[$index] = new allocation_item($type, $type_no, $date_, $due_date, $amount, $amount_allocated, $current_allocated, $ref);
			return true;
		}
		else 
			return false;
	}
	
	function add_or_update_item($type, $type_no, $date_, $due_date, $amount, $amount_allocated, $current_allocated, $ref) {
		for ($i = 0; $i < count($this->allocs); $i++) {
			$item = $this->allocs[$i];
			if (($item->type == $type) && ($item->type_no == $type_no))
				return $this->update_item($i, $type, $type_no, $date_, $due_date, $amount, $amount_allocated, $current_allocated, $ref);
		}
		return $this->add_item($type, $type_no, $date_, $due_date, $amount, $amount_allocated, $current_allocated, $ref);
	}

	//
	//	Read payment or credit current/available allocations to cart.
	//
	// FIXME - read all transactions below twice seems to be suboptimal
	//
	function read($type = null, $trans_no = 0, $person_id=null, $person_type_id=null) {
		if ($type !== null) {	// otherwise re-read
			$type = $this->type;
			$trans_no = $this->trans_no;

			if (isset($person_type_id)) {
				$this->person_type = $person_type_id;
				$this->person_id = $person_id;
			}
			else { // guess person_type_id
				if (in_array($type, array(ST_BANKPAYMENT, ST_BANKDEPOSIT))) {
					$bank_trans = db_fetch(get_bank_trans($type, $trans_no));
					$this->person_type = $bank_trans['person_type_id'];
				}
				else
					$this->person_type = in_array($type, array(ST_SUPPCREDIT, ST_SUPPAYMENT)) ? PT_SUPPLIER : PT_CUSTOMER;
			}

			if ($trans_no) {
				$trans = $this->person_type == PT_SUPPLIER ? get_supp_trans($trans_no, $type, $person_id) : get_customer_trans($trans_no, $type, $person_id);

				$this->person_id = $trans[$this->person_type == PT_SUPPLIER ? 'supplier_id':'debtor_no'];
				$this->person_name = $trans[$this->person_type == PT_SUPPLIER ? 'supplier_name':'DebtorName'];
				$this->date_ = sql2date($trans['tran_date']);
				$this->person_curr = $trans['curr_code'];
				$this->currency = isset($trans['bank_curr_code']) ? $trans['bank_curr_code'] : $trans['curr_code'];
				$this->bank_amount = isset($trans['bank_amount']) ? $trans['bank_amount'] : $trans['Total']; // not set for journal entry
				$this->amount = $trans['Total'];
			}
		}
		/* Now populate the array of possible (and previous actual) allocations 
		for this customer/supplier. First get the transactions that have 
		outstanding balances ie Total-alloc >0 */

		$this->allocs = array();
		if ($this->person_id) {
			if ($this->person_type==PT_SUPPLIER)
				$trans_items = get_allocatable_to_supp_transactions($this->person_id);
			else
				$trans_items = get_allocatable_to_cust_transactions($this->person_id);
			while ($myrow = db_fetch($trans_items)) {
				$this->add_item($myrow['type'], $myrow['trans_no'],
					sql2date($myrow['tran_date']),
					sql2date($myrow['due_date']),
					$myrow['Total'], // trans total
					$myrow['alloc'], // trans total allocated
					0,
					$myrow['reference']); // this allocation
			}
		}
		if ($this->trans_no == 0) return; // this is new payment

		/* Now get trans that might have previously been allocated to by this trans
		NB existing entries where still some of the trans outstanding entered from
		above logic will be overwritten with the prev alloc detail below */

		if ($this->person_type==PT_SUPPLIER)
			$trans_items = get_allocatable_to_supp_transactions($this->person_id, $this->trans_no, $this->type);
		else
			$trans_items = get_allocatable_to_cust_transactions($this->person_id, $this->trans_no, $this->type);

		$amount = $this->amount;
		while ($myrow = db_fetch($trans_items)) {
			$amount -= $myrow['Total'];
			$this->add_or_update_item ($myrow['type'], $myrow['trans_no'], sql2date($myrow['tran_date']), sql2date($myrow['due_date']), $myrow['Total'], $myrow['alloc'] - $myrow['amt'], $myrow['amt'], $myrow['reference']);
		}

		/* Finally, if there any unallocated money remaining, assign the unallocated portion to
		the earliest transactions. This makes the javascript All/None keys consistent
		with the transaction amount. */

		if ($amount > 0) {
			foreach ($this->allocs as $alloc_item) {
				$allocatable = $alloc_item->amount - $alloc_item->amount_allocated;
				if ($allocatable > 0) {
					if ($amount >= $allocatable) {
						$alloc_item->current_allocated = $allocatable;
						$amount -= $allocatable;
					}
					else {
						$alloc_item->current_allocated += $amount;
						break;
					}
				}
			}
		}
	}
	//
	//	Update allocations in database.
	//
	function write() {
		global 	$no_exchange_variations;

		begin_transaction();

		if ($this->person_type == PT_SUPPLIER)
			clear_supp_alloctions($this->type, $this->trans_no, $this->person_id, $this->date_);
		else
			clear_cust_alloctions($this->type, $this->trans_no, $this->person_id, $this->date_);

		// now add the new allocations
		$total_allocated = 0;
		$dec = user_price_dec();
		foreach ($this->allocs as $alloc_item) {
			if ($alloc_item->current_allocated > 0) {
				$amount = round($alloc_item->current_allocated, $dec);

				if ($this->person_type == PT_SUPPLIER) {
					add_supp_allocation($amount, $this->type, $this->trans_no, $alloc_item->type, $alloc_item->type_no, $this->person_id, $this->date_);
					update_supp_trans_allocation($alloc_item->type, $alloc_item->type_no, $this->person_id);
				}
				else {
					add_cust_allocation($amount, $this->type, $this->trans_no, $alloc_item->type, $alloc_item->type_no, $this->person_id, $this->date_);
					update_debtor_trans_allocation($alloc_item->type, $alloc_item->type_no, $this->person_id);
				}
				// Exchange Variations Joe Hunt 2008-09-20 ////////////////////
				if ($alloc_item->type != ST_SALESORDER && !@$no_exchange_variations && $alloc_item->type != ST_PURCHORDER && $alloc_item->type != ST_JOURNAL && $this->type != ST_JOURNAL)
					exchange_variation($this->type, $this->trans_no, $alloc_item->type, $alloc_item->type_no, $this->date_, $amount, $this->person_type);

				//////////////////////////////////////////////////////////////
				$total_allocated += $alloc_item->current_allocated;
			}
		}  /*end of the loop through the array of allocations made */
		if ($this->person_type == PT_SUPPLIER)
			update_supp_trans_allocation($this->type, $this->trans_no, $this->person_id);
		else
			update_debtor_trans_allocation($this->type,	$this->trans_no, $this->person_id);

		commit_transaction();
	}
} 

//-----------------------------------------------------------------------------------

class allocation_item {

	var $type;
	var $type_no;
	
	var $date_;
	var $due_date;
	
	var $amount_allocated;
	var $amount;
	var $ref;
	
	var $current_allocated;
	
	function __construct($type, $type_no, $date_, $due_date, $amount, $amount_allocated, $current_allocated, $ref) {

		$this->type = $type;
		$this->type_no = $type_no;

		$this->ref = $ref;

		$this->date_ = $date_;
		$this->due_date = $due_date;
		
		$this->amount = $amount;
		$this->amount_allocated = $amount_allocated;
		$this->current_allocated = $current_allocated;
	}
}

//--------------------------------------------------------------------------------

function show_allocatable($show_totals) {

	global $systypes_array;
	
	$k = $total_allocated = 0;

	$cart = $_SESSION['alloc'];
	$supp_ref = in_array($cart->type, array(ST_SUPPCREDIT, ST_SUPPAYMENT, ST_BANKPAYMENT));

	if (count($cart->allocs))  {
		display_heading(sprintf(_('Allocated amounts in %s:'), $cart->person_curr));
		start_table(TABLESTYLE, "width='60%'");
		$th = array(_('Transaction Type'), _('#'), $supp_ref ? _('Supplier Ref'): _('Ref'), _('Date'), _('Due Date'), _('Amount'),
			_('Other Allocations'), _('Left to Allocate'), _('This Allocation'),'','');

		table_header($th);

		foreach ($cart->allocs as $id => $alloc_item) {
			if (floatcmp(abs($alloc_item->amount), $alloc_item->amount_allocated)) {
				alt_table_row_color($k);
				label_cell($systypes_array[$alloc_item->type]);
				label_cell(get_trans_view_str($alloc_item->type, $alloc_item->type_no), "nowrap align='right'");
				label_cell($alloc_item->ref);
				label_cell($alloc_item->date_, 'align=right');
				label_cell($alloc_item->due_date, 'align=right');
				amount_cell(abs($alloc_item->amount));
				amount_cell($alloc_item->amount_allocated);

				$_POST['amount' . $id] = price_format($alloc_item->current_allocated);

				$un_allocated = round((abs($alloc_item->amount) - $alloc_item->amount_allocated), 6);
				amount_cell($un_allocated, false,'', 'maxval'.$id);
				amount_cells(null, 'amount' . $id);//, input_num('amount' . $id));
				label_cell("<a href='javascript:void(0)' name=Alloc$id onclick='allocate_all(this.name.substr(5));return true;'>"
					 . _('All') . '</a>');
				label_cell("<a href='javascript:void(0)' name=DeAll$id onclick='allocate_none(this.name.substr(5));return true;'>"
					 . _('None') . '</a>'.hidden('un_allocated' . $id, 
					 price_format($un_allocated), false));
				end_row();

				$total_allocated += input_num('amount' . $id);
			}
		}
		if ($show_totals) {
			label_row(_('Total Allocated'), price_format($total_allocated),
				'colspan=8 align=right', "align=right id='total_allocated'", 3);

			$amount = abs($cart->amount);

			if (floatcmp($amount, $total_allocated) < 0) {
				$font1 = '<font color=red>';
				$font2 = '</font>';
			}
			else
				$font1 = $font2 = '';
			$left_to_allocate = price_format($amount - $total_allocated);
			label_row(_('Left to Allocate'), $font1 . $left_to_allocate . $font2, 'colspan=8 align=right', "nowrap align=right id='left_to_allocate'", 3);
		}
		end_table(1);
	}
	hidden('TotalNumberOfAllocs', count($cart->allocs));
}

//--------------------------------------------------------------------------------

function check_allocations() {
	global $SysPrefs;

	$total_allocated = 0;

	for ($counter = 0; $counter < get_post('TotalNumberOfAllocs'); $counter++) {
		if (!isset($_POST['amount'.$counter])) continue;
		if (!check_num('amount' . $counter, 0)) {
			display_error(_('The entry for one or more amounts is invalid or negative.'));
			set_focus('amount'.$counter);
			return false;
		}

		/* Now check to see that the AllocAmt is no greater than the
		amount left to be allocated against the transaction under review;
		skip check if no allocation is set to avoid deadlock on mistakenly overallocated transactions*/
		$allocated = input_num('amount' . $counter);
		if ($allocated && ($allocated > input_num('un_allocated' . $counter))) {
			display_error(_('At least one transaction is overallocated.'));
			set_focus('amount'.$counter);
			return false;
		}

		if ($_SESSION['alloc']->person_type == PT_CUSTOMER) {
			if ($_SESSION['alloc']->allocs[$counter]->type == ST_SALESORDER)
				$trans = get_sales_order_header($_SESSION['alloc']->allocs[$counter]->type_no, $_SESSION['alloc']->allocs[$counter]->type);
			else
				$trans = get_customer_trans($_SESSION['alloc']->allocs[$counter]->type_no, $_SESSION['alloc']->allocs[$counter]->type,$_SESSION['alloc']->person_id);

			if ($trans['debtor_no'] != $_SESSION['alloc']->person_id) {
				display_error(_('Allocated transaction allocated is not related to company selected.'));
				set_focus('amount'.$counter);
				return false;
			}
		}
		elseif ($_SESSION['alloc']->person_type == PT_SUPPLIER) {
			if ($_SESSION['alloc']->allocs[$counter]->type == ST_PURCHORDER)
				$trans = get_po($_SESSION['alloc']->allocs[$counter]->type_no);
			else
				$trans = get_supp_trans($_SESSION['alloc']->allocs[$counter]->type_no, $_SESSION['alloc']->allocs[$counter]->type,  $_SESSION['alloc']->person_id);
			if ($trans['supplier_id'] != $_SESSION['alloc']->person_id) {
				display_error(_('Allocated transaction allocated is not related to company selected.'));
				set_focus('amount'.$counter);
				return false;
			}
		}

		$_SESSION['alloc']->allocs[$counter]->current_allocated = input_num('amount' . $counter);
		$total_allocated += input_num('amount' . $counter);
	}

	$amount = abs($_SESSION['alloc']->amount);

	if ($total_allocated - ($amount + input_num('discount'))  > $SysPrefs->allocation_settled_allowance()) {
		display_error(_('These allocations cannot be processed because the amount allocated is more than the total amount left to allocate.'));
		return false;
	}

	return true;
}

//----------------------------------------------------------------------------------------
//
//	Returns sales or purchase invoice allocations to be reallocated after invoice edition.
//
function get_inv_allocations($trans_no, $trans_type, $person_id) {
	$allocs = array();
	if ($trans_type == ST_SUPPINVOICE || $trans_type == ST_SUPPCREDIT)
		$result = get_allocatable_from_supp_transactions($person_id, $trans_no, $trans_type);
	else
		$result = get_allocatable_from_cust_transactions($person_id, $trans_no, $trans_type);

	while($dat = db_fetch($result)) {
		$allocs[] = array('type'=> $dat['type'], 'trans_no'=> $dat['trans_no'], 'amount'=>$dat['alloc']);
	}
	return $allocs;
}