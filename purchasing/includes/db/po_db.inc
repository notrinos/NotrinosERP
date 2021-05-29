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

function get_supplier_details_to_order(&$order, $supplier_id) {
	$sql = "SELECT curr_code, supp_name, tax_group_id, supp.tax_included, dimension_id, dimension2_id, supp.credit_limit - Sum((ov_amount + ov_gst + ov_discount)) as cur_credit, terms.terms, terms.days_before_due, terms.day_in_following_month
		FROM ".TB_PREF."suppliers supp
			LEFT JOIN ".TB_PREF."supp_trans trans ON supp.supplier_id = trans.supplier_id
			LEFT JOIN ".TB_PREF."payment_terms terms ON supp.payment_terms=terms.terms_indicator
		WHERE supp.supplier_id = ".db_escape($supplier_id)."
		GROUP BY supp.supp_name";

	$result = db_query($sql, 'The supplier details could not be retreived');
	$myrow = db_fetch($result);

	$order->credit = $myrow['cur_credit'];
	$order->terms = array( 
		'description' => $myrow['terms'],
		'days_before_due' => $myrow['days_before_due'], 
		'day_in_following_month' => $myrow['day_in_following_month']);

	$_POST['supplier_id'] = $supplier_id;
	$_POST['supplier_name'] = $myrow['supp_name'];
	$_POST['curr_code'] = $myrow['curr_code'];
	$_POST['dimension'] = $myrow['dimension_id'];
	$_POST['dimension2'] = $myrow['dimension2_id'];

	$order->set_supplier($supplier_id, $myrow['supp_name'], $myrow['curr_code'], $myrow['tax_group_id'], $myrow['tax_included']);
}

function delete_po($po) {
	global $Refs;

	begin_transaction();
	hook_db_prevoid(ST_PURCHORDER, $po);
	$sql = "DELETE FROM ".TB_PREF."purch_orders WHERE order_no=".db_escape($po);
	db_query($sql, 'The order header could not be deleted');

	$sql = "DELETE FROM ".TB_PREF."purch_order_details WHERE order_no =".db_escape($po);
	db_query($sql, 'The order detail lines could not be deleted');

	$Refs->restore_last(ST_PURCHORDER, $po);
	commit_transaction();
}

function add_po(&$po_obj) {
	global $Refs;

	begin_transaction();
	hook_db_prewrite($po_obj, ST_PURCHORDER);

	/*Insert to purchase order header record */
	$sql = "INSERT INTO ".TB_PREF."purch_orders (supplier_id, Comments, ord_date, reference, requisition_no, into_stock_location, delivery_address, total, tax_included, prep_amount) VALUES(";
	$sql .= db_escape($po_obj->supplier_id) . "," .
		db_escape($po_obj->Comments) . ",'" .
		date2sql($po_obj->orig_order_date) . "', " .
		db_escape($po_obj->reference) . ", " .
		db_escape($po_obj->supp_ref) . ", " .
		db_escape($po_obj->Location) . ", " .
		db_escape($po_obj->delivery_address) . ", " .
		db_escape($po_obj->get_trans_total()). ", " .
		db_escape($po_obj->tax_included). ", " .
		db_escape($po_obj->prep_amount). ")";

	db_query($sql, 'The purchase order header record could not be inserted');

	/*Get the auto increment value of the order number created from the sql above */
	$po_obj->order_no = db_insert_id();

	/*Insert the purchase order detail records */
	foreach ($po_obj->line_items as $line_no => $po_line) {
		$sql = "INSERT INTO ".TB_PREF."purch_order_details (order_no, item_code, description, delivery_date,	unit_price,	quantity_ordered) 
		VALUES (".
			db_escape($po_obj->order_no).", ".
			db_escape($po_line->stock_id).",".
			db_escape($po_line->item_description).",'".
			date2sql($po_line->req_del_date) . "'," .
			db_escape($po_line->price) . ", " .
			db_escape($po_line->quantity). ")";

		db_query($sql, 'One of the purchase order detail records could not be inserted');
		$po_obj->line_items[$line_no]->po_detail_rec = db_insert_id();
	}

	$Refs->save(ST_PURCHORDER, $po_obj->order_no, $po_obj->reference);

	add_audit_trail(ST_PURCHORDER, $po_obj->order_no, $po_obj->orig_order_date);
	hook_db_postwrite($po_obj, ST_PURCHORDER);
	commit_transaction();

	return $po_obj->order_no;
}

function update_po(&$po_obj) {
	begin_transaction();
	hook_db_prewrite($po_obj, ST_PURCHORDER);

	/*Update the purchase order header with any changes */
	$sql = "UPDATE ".TB_PREF."purch_orders SET Comments=".db_escape($po_obj->Comments).",
		requisition_no= ".db_escape( $po_obj->supp_ref).",
		into_stock_location=".db_escape($po_obj->Location).",
		ord_date='".date2sql($po_obj->orig_order_date)."',
		delivery_address=".db_escape($po_obj->delivery_address).",
		total=". db_escape($po_obj->get_trans_total()).",
		prep_amount=".db_escape($po_obj->prep_amount).",
		tax_included=".db_escape($po_obj->tax_included)."
	WHERE order_no = ".$po_obj->order_no;

	db_query($sql, 'The purchase order could not be updated');

	$sql = "DELETE FROM ".TB_PREF."purch_order_details WHERE order_no=".db_escape($po_obj->order_no);
	db_query($sql, 'could not delete old purch order details');

	/*Now Update the purchase order detail records */
	foreach ($po_obj->line_items as $po_line) {
		$sql = "INSERT INTO ".TB_PREF."purch_order_details (po_detail_item, order_no, item_code, 
			description, delivery_date, unit_price, quantity_ordered, quantity_received) VALUES ("
			.db_escape($po_line->po_detail_rec ? $po_line->po_detail_rec : 0).","
			.db_escape($po_obj->order_no).","
			.db_escape($po_line->stock_id).","
			.db_escape($po_line->item_description).",'"
			.date2sql($po_line->req_del_date)."',"
			.db_escape($po_line->price).", "
			.db_escape($po_line->quantity).", "
			.db_escape($po_line->qty_received).")";
		db_query($sql, 'One of the purchase order detail records could not be updated');
	}

	reallocate_payments($po_obj->order_no, ST_PURCHORDER, $po_obj->orig_order_date, $po_obj->get_trans_total(), $po_obj->prepayments, $po_obj->supplier_id);

	add_audit_trail($po_obj->trans_type, $po_obj->order_no, Today(), _('Updated.'));
	hook_db_postwrite($po_obj, ST_PURCHORDER);
	commit_transaction();

	return $po_obj->order_no;
}

function get_po($order_no) {
	$sql = "SELECT * FROM ".TB_PREF."purch_orders WHERE order_no = ".db_escape($order_no);
	$result = db_query($sql);

	return db_fetch($result);
}

function read_po_header($order_no, &$order) {
		$sql = "SELECT po.*, supplier.*, loc.location_name 
		FROM ".TB_PREF."purch_orders po,".TB_PREF."suppliers supplier,".TB_PREF."locations loc
		WHERE po.supplier_id = supplier.supplier_id
		AND loc.loc_code = into_stock_location
		AND po.order_no = ".db_escape($order_no);

	$result = db_query($sql, 'The order cannot be retrieved');

	if (db_num_rows($result) == 1) {

		$myrow = db_fetch($result);

		$order->trans_type = ST_PURCHORDER;
		$order->order_no = $order_no;

		$order->set_supplier($myrow['supplier_id'], $myrow['supp_name'], $myrow['curr_code'], $myrow['tax_group_id'], $myrow['tax_included']);

		$order->credit = get_current_supp_credit($order->supplier_id);

		$order->orig_order_date = sql2date($myrow['ord_date']);
		$order->Comments = $myrow['comments'];
		$order->Location = $myrow['into_stock_location'];
		$order->supp_ref = $myrow['requisition_no'];
		$order->reference = $myrow['reference'];
		$order->delivery_address = $myrow['delivery_address'];
		$order->alloc = $myrow['alloc'];
		$order->prep_amount = $myrow['prep_amount'];
		$order->prepayments = get_payments_for($order_no, ST_PURCHORDER, $myrow['supplier_id']);

		return true;
	}

	display_db_error('FATAL : duplicate purchase order found', '', true);
	return false;
}

function read_po_items($order_no, &$order, $open_items_only=false) {
	/*now populate the line po array with the purchase order details records */

	$sql = "SELECT poline.*, units
		FROM ".TB_PREF."purch_order_details poline
			LEFT JOIN ".TB_PREF."stock_master item	ON poline.item_code=item.stock_id
		WHERE order_no =".db_escape($order_no);

	if ($open_items_only)
		$sql .= " AND (poline.quantity_ordered > poline.quantity_received) ";

	$sql .= " ORDER BY po_detail_item";

	$result = db_query($sql, 'The lines on the purchase order cannot be retrieved');

	if (db_num_rows($result) > 0) {
		while ($myrow = db_fetch($result)) {
			$data = get_purchase_data($order->supplier_id, $myrow['item_code']);
			if ($data !== false) {
				if ($data['supplier_description'] != '')
					$myrow['description'] = $data['supplier_description'];
			}
			$units = is_null($myrow['units']) ? '' : $myrow['units'];

			if ($order->add_to_order($order->lines_on_order, $myrow['item_code'], $myrow['quantity_ordered'], $myrow['description'], $myrow['unit_price'], $units, sql2date($myrow['delivery_date']), $myrow['qty_invoiced'], $myrow['quantity_received']))
				$order->line_items[$order->lines_on_order-1]->po_detail_rec = $myrow['po_detail_item'];
		} /* line po from purchase order details */
	} //end of checks on returned data set
}

function read_po($order_no, &$order, $open_items_only=false) {

	if (read_po_header($order_no, $order))
		read_po_items($order_no, $order, $open_items_only);
}

function get_po_items($order_no) {
	$sql = "SELECT item_code, quantity_ordered, quantity_received, qty_invoiced FROM ".TB_PREF."purch_order_details WHERE order_no=".db_escape($order_no)." ORDER BY po_detail_item";

	$result = db_query($sql, 'could not query purch order details');
	check_db_error('Could not check that the details of the purchase order had not been changed by another user ', $sql);
	return $result;
}

function get_short_info($stock_id) {
	$sql = "SELECT description, units, mb_flag FROM ".TB_PREF."stock_master WHERE stock_id = ".db_escape($stock_id);

	return db_query($sql,'The stock details for '.$stock_id.' could not be retrieved');
}

function get_sql_for_po_search_completed($from, $to, $supplier_id=ALL_TEXT, $location=ALL_TEXT, $order_number='', $stock_id='', $also_closed=false) {
	$sql = "SELECT 
		porder.order_no, 
		porder.reference, 
		supplier.supp_name, 
		location.location_name,
		porder.requisition_no, 
		porder.ord_date, 
		supplier.curr_code, 
		Sum(line.unit_price*line.quantity_ordered) AS OrderValue,
		Sum(line.delivery_date < '". date2sql(Today()) ."'
		AND (line.quantity_ordered > line.quantity_received)) As OverDue,
		porder.into_stock_location,
		chk.isopen
	FROM ".TB_PREF."purch_orders as porder
		LEFT JOIN (
			SELECT order_no, SUM(quantity_ordered-quantity_received + quantity_ordered-qty_invoiced) isopen
			FROM ".TB_PREF."purch_order_details
			GROUP BY order_no) chk ON chk.order_no=porder.order_no,"
		.TB_PREF."purch_order_details as line, "
		.TB_PREF."suppliers as supplier, "
		.TB_PREF."locations as location
	WHERE porder.order_no = line.order_no
		AND porder.supplier_id = supplier.supplier_id
		AND location.loc_code = porder.into_stock_location ";

	if ($supplier_id != ALL_TEXT)
		$sql .= "AND supplier.supplier_id=".$supplier_id." ";
	if ($order_number != "")
		$sql .= "AND porder.reference LIKE ".db_escape('%'. $order_number . '%');
	else {
		$sql .= " AND porder.ord_date >= '".date2sql($from)."' AND porder.ord_date <= '".date2sql($to)."'";

		if ($location != ALL_TEXT)
			$sql .= " AND porder.into_stock_location = ".db_escape($location);
		if ($stock_id !== '')
			$sql .= " AND line.item_code=".db_escape($stock_id);
		if ($supplier_id != ALL_TEXT)
			$sql .= " AND supplier.supplier_id=".db_escape($supplier_id);
	}

	if (!$also_closed)
		$sql .= " AND isopen";
	$sql .= " GROUP BY porder.order_no";

	return $sql;
}

function get_sql_for_po_search($from, $to, $supplier_id=ALL_TEXT, $location=ALL_TEXT, $order_number='', $stock_id='') {
	$sql = "SELECT 
		porder.order_no, 
		porder.reference,
		supplier.supp_name, 
		location.location_name,
		porder.requisition_no, 
		porder.ord_date,
		supplier.curr_code,
		Sum(line.unit_price*line.quantity_ordered) AS OrderValue,
		Sum(line.delivery_date < '". date2sql(Today()) ."'
		AND (line.quantity_ordered > line.quantity_received)) As OverDue
		FROM ".TB_PREF."purch_orders as porder,"
			.TB_PREF."purch_order_details as line, "
			.TB_PREF."suppliers as supplier, "
			.TB_PREF."locations as location
		WHERE porder.order_no = line.order_no
		AND porder.supplier_id = supplier.supplier_id
		AND location.loc_code = porder.into_stock_location
		AND (line.quantity_ordered > line.quantity_received) ";

	if ($order_number != '')
		$sql .= "AND porder.reference LIKE ".db_escape('%'.$order_number.'%');
	else {
		$sql .= "  AND porder.ord_date >= '".date2sql($from)."' AND porder.ord_date <= '".date2sql($to)."'";

		if ($location != ALL_TEXT)
			$sql .= " AND porder.into_stock_location = ".db_escape($location);
		if ($stock_id != '')
			$sql .= " AND line.item_code=".db_escape($stock_id);
		if ($supplier_id != ALL_TEXT)
			$sql .= " AND supplier.supplier_id=".db_escape($supplier_id);
	} //end not order number selected

	$sql .= " GROUP BY porder.order_no";

	return $sql;
}
