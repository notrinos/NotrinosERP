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
/* Definition of the purch_order class to hold all the information for a purchase order and delivery
*/

class purch_order {

	var $trans_type; // order/grn/invoice (direct)
	var $line_items;
	var $curr_code;
	var $supp_ref;
	var $delivery_address;
	var $Comments;
	var $Location;
	var $supplier_id;
	var $supplier_name;
	var $orig_order_date;
	var $due_date;
	var $order_no; /*Only used for modification of existing orders otherwise only established when order committed */
	var $lines_on_order = 0;
	var $credit;
	var $tax_group_id;
	var $tax_group_array = null; // saves db queries
	var $tax_included; // type of prices
	var $terms;
	var $ex_rate;
	var $cash_account;
	var $dimension,
		$dimension2;

	var $reference;
	var $tax_overrides = array();		// array of taxes manually inserted during sales invoice entry (direct invoice)
	
	var $prep_amount = 0; // prepayment required
	var $alloc; // sum of payments allocated
	var $prepayments = array();

	var $fixed_asset = false;

	function __construct() {
		/*Constructor function initialises a new purchase order object */
		$this->line_items = array();
		$this->lines_on_order = $this->order_no = $this->supplier_id = 0;
	}
	
	function set_supplier($supplier_id, $supplier_name, $curr_code, $tax_group_id, $tax_included) {
		$this->supplier_id = $supplier_id;
		$this->supplier_name = $supplier_name;
		$this->curr_code = $curr_code;
		$this->tax_group_id = $tax_group_id;
		$this->tax_included = $tax_included;
		$this->tax_group_array = get_tax_group_items_as_array($tax_group_id);
	}
	
	function add_to_order($line_no, $stock_id, $qty, $item_descr, $price, $uom, $req_del_date, $qty_inv, $qty_recd) {
		if (isset($qty) && $qty != 0) {
			$this->line_items[$line_no] = new po_line_details($line_no, $stock_id, $item_descr, $qty, $price, $uom, 
				$req_del_date, $qty_inv, $qty_recd);
			$this->lines_on_order++;
			return 1;
		}
		return 0;
	}

	function update_order_item($line_no, $qty, $price, $req_del_date, $description="") {
		if ($description != "")
			$this->line_items[$line_no]->item_description = $description;
		$this->line_items[$line_no]->quantity = $qty;
		$this->line_items[$line_no]->price = $price;
		$this->line_items[$line_no]->req_del_date = $req_del_date;
		$this->line_items[$line_no]->item_description = $description;
	}

	function remove_from_order($line_no) {
		array_splice($this->line_items, $line_no, 1);
	}
	
	function order_has_items() {
		return count($this->line_items) != 0;
	}
	
	function clear_items() {
		unset($this->line_items);
		$this->line_items = array();
		
		$this->lines_on_order = 0;  
		$this->order_no = 0;
	}
	
	function any_already_received() {
		/* Checks if there have been deliveries or invoiced entered against any of the line items */
		if (count($this->line_items) > 0) {
			foreach ($this->line_items as $ordered_items) {
				if ($ordered_items->qty_received != 0 || $ordered_items->qty_inv != 0)
					return 1;
			}
		}
		return 0;
	}

	function some_already_received($line_no) {
		/* Checks if there have been deliveries or amounts invoiced against a specific line item */
		if (count($this->line_items) > 0) {
			if ($this->line_items[$line_no]->qty_received != 0 || $this->line_items[$line_no]->qty_inv != 0)
				return 1;
		}
		return 0;
	}
	
	//
	//	Returns taxes for PO/GRN.
	//	$receival=true in purchase receive context.
	//
	function get_taxes($shipping_cost=null, $receival=false) {
		$items = array();
		$prices = array();
		if($shipping_cost==null)
			$shipping_cost = 0;//$this->freight_cost;

		foreach ($this->line_items as $ln_itm) {
			$items[] = $ln_itm->stock_id;
			$prices[] = round($ln_itm->price * ($receival ? $ln_itm->receive_qty : $ln_itm->quantity),  user_price_dec());
		}
		$taxes = get_tax_for_items($items, $prices, $shipping_cost,
		  $this->tax_group_id, $this->tax_included,  $this->tax_group_array);

		// Adjustment for swiss franken, we always have 5 rappen = 1/20 franken
		if ($this->curr_code == 'CHF') {
			$val = $taxes['1']['Value'];
			$val1 = (floatval((intval(round(($val*20),0)))/20));
			$taxes['1']['Value'] = $val1;
		}
		foreach($this->tax_overrides as $id => $value) { // add values entered manually
			$taxes[$id]['Override'] = $value;
		}
		return $taxes;
	}

	/*
		Returns order value including all taxes
	*/
	function get_trans_total() {
		
		$total = 0;
		$dec = user_price_dec();

		foreach ($this->line_items as $ln_itm) {
			$items[] = $ln_itm->stock_id;
			$value = round($ln_itm->quantity * $ln_itm->price, $dec);
			$prices[] =$value;
			$total += $value;
		}

		if (!$this->tax_included ) {
			$taxes = get_tax_for_items($items, $prices, 0, $this->tax_group_id,
			$this->tax_included,  $this->tax_group_array);

			foreach($taxes as $tax)
				$total += round($tax['Value'], $dec);
		}
		return $total;
	}

} /* end of class defintion */

class po_line_details {

	var $line_no;
	var $po_detail_rec;
	var $grn_item_id;
	var $stock_id;
	var $item_description;
	var $price;
	var $units;
	var $req_del_date;
	var $tax_type;
	var $tax_type_name;

	var $quantity;		// current/entry quantity of PO line
	var $qty_inv;	// quantity already invoiced against this line
	var $receive_qty;	// current/entry GRN quantity
	var $qty_received;	// quantity already received against this line

	var $standard_cost;
	var $descr_editable;

	function __construct($line_no, $stock_item, $item_descr, $qty, $prc, $uom, $req_del_date, $qty_inv, $qty_recd, $grn_item_id=0) {

		/* Constructor function to add a new LineDetail object with passed params */
		$this->line_no = $line_no;
		$this->stock_id = $stock_item;
		$item_row = get_item($stock_item);
		if (!$item_row) 
			return;

		$this->descr_editable = $item_row["editable"];
		//if ($item_descr == null || !$this->descr_editable) this was overriding the prchase description!!
		if ($item_descr == null)
			$this->item_description = $item_row["description"];
		else
			$this->item_description = $item_descr;
		$this->quantity = $qty;
		$this->req_del_date = $req_del_date;
		$this->price = $prc;
//		$this->units = $uom;
		$this->tax_type = $item_row["tax_type_id"];
		$this->tax_type_name = $item_row["tax_type_name"];
		$this->units = $item_row["units"];
		$this->qty_received = $qty_recd;
		$this->qty_inv = $qty_inv;
		$this->receive_qty = 0;	/*initialise these last two only */
		$this->standard_cost =0;
		$this->grn_item_id = $grn_item_id;
	}
	
	// Deprecated, use with care. If you need line value as it is calculated in invoice, use taxfree_charge_value below.
	function taxfree_charge_price($po) {
		return get_tax_free_price_for_item($this->stock_id, $this->price, $po->tax_group_id, $po->tax_included, $po->tax_group_array);
	}

	//
	// GRN line tax free value.
	//
	function taxfree_charge_value($po) {
		return get_tax_free_price_for_item($this->stock_id, $this->receive_qty*$this->price, $po->tax_group_id, $po->tax_included, $po->tax_group_array);
	}
}