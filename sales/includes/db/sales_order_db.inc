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
function add_sales_order(&$order) {
	global $SysPrefs, $path_to_root, $Refs;

	begin_transaction();
	hook_db_prewrite($order, $order->trans_type);
	$order_no = get_next_trans_no($order->trans_type);
	$del_date = date2sql($order->due_date);
	$order_type = 0; // this is default on new order
	$total = $order->get_trans_total();
	$sql = "INSERT INTO ".TB_PREF."sales_orders (order_no, type, debtor_no, trans_type, branch_code, customer_ref, reference, comments, ord_date,
		order_type, ship_via, deliver_to, delivery_address, contact_phone,
		freight_cost, from_stk_loc, delivery_date, payment_terms, total, prep_amount)
		VALUES (" .db_escape($order_no) . "," .db_escape($order_type) . "," . db_escape($order->customer_id) .
		 ", " .db_escape($order->trans_type) . "," .db_escape($order->Branch) . ", ".
			db_escape($order->cust_ref) .",". 
			db_escape($order->reference) .",". 
			db_escape($order->Comments) .",'" . 
			date2sql($order->document_date) . "', " .
			db_escape($order->sales_type) . ", " .
			db_escape($order->ship_via)."," . 
			db_escape($order->deliver_to) . "," .
			db_escape($order->delivery_address) . ", " .
			db_escape($order->phone) . ", " . 
			db_escape($order->freight_cost) .", " . 
			db_escape($order->Location) .", " .
			db_escape($del_date) . "," .
			db_escape($order->payment) . "," .
			db_escape($total) . "," .
			db_escape($order->prep_amount).")";

	db_query($sql, 'order Cannot be Added');

	$order->trans_no = array($order_no=>0);

	if ($SysPrefs->loc_notification() == 1) {
		include_once($path_to_root . '/inventory/includes/inventory_db.inc');
		$st_ids = array();
		$st_names = array();
		$st_num = array();
		$st_reorder = array();
	}
	foreach ($order->line_items as $line) {
		if ($SysPrefs->loc_notification() == 1 && is_inventory_item($line->stock_id))
			$loc = calculate_reorder_level($order->Location, $line, $st_ids, $st_names, $st_num, $st_reorder); 

		$sql = "INSERT INTO ".TB_PREF."sales_order_details (order_no, trans_type, stk_code, description, unit_price, quantity, discount_percent) VALUES (";
		$sql .= $order_no . ",".$order->trans_type .
				",".db_escape($line->stock_id).", "
				.db_escape($line->item_description).", $line->price,
				$line->quantity,
				$line->discount_percent)";
		db_query($sql, 'order Details Cannot be Added');

	// Now mark quotation line as processed
		if ($order->trans_type == ST_SALESORDER && $line->src_id)
			update_parent_line(ST_SALESORDER, $line->src_id, $line->qty_dispatched); // clear all the quote despite all or the part was ordered
	} /* inserted line items into sales order details */

	add_audit_trail($order->trans_type, $order_no, $order->document_date);
	$Refs->save($order->trans_type, $order_no, $order->reference, null, $order->fixed_asset);

	hook_db_postwrite($order, $order->trans_type);
	commit_transaction();

	if ($SysPrefs->loc_notification() == 1 && count($st_ids) > 0)
		send_reorder_email($loc, $st_ids, $st_names, $st_num, $st_reorder);
	return $order_no;
}

//----------------------------------------------------------------------------------------

function delete_sales_order($order_no, $trans_type) {
	begin_transaction();
	hook_db_prevoid($trans_type, $order_no);

	$sql = "DELETE FROM ".TB_PREF."sales_orders WHERE order_no=" . db_escape($order_no) 
		. " AND trans_type=".db_escape($trans_type);

	db_query($sql, "order Header Delete");

	$sql = "DELETE FROM ".TB_PREF."sales_order_details WHERE order_no =" 
		.db_escape($order_no) . " AND trans_type=".db_escape($trans_type);
	db_query($sql, 'order Detail Delete');

	add_audit_trail($trans_type, $order_no, Today(), _('Deleted.'));
	commit_transaction();
}

//----------------------------------------------------------------------------------------
// Mark changes in sales_order_details
//
function update_sales_order_version($order) {
	foreach ($order as $so_num => $so_ver) {
		$sql= 'UPDATE '.TB_PREF.'sales_orders SET version=version+1 WHERE order_no='. db_escape($so_num).
		' AND version='.$so_ver . " AND trans_type=".ST_SALESORDER;
		db_query($sql, 'Concurrent editing conflict while sales order update');
	}
}

//----------------------------------------------------------------------------------------

function update_sales_order($order) {
	global $SysPrefs, $path_to_root, $Refs;

	$del_date = date2sql($order->due_date);
	$ord_date = date2sql($order->document_date);
	$order_no =  key($order->trans_no);
	$version= current($order->trans_no);
	$total = $order->get_trans_total();

	begin_transaction();
	hook_db_prewrite($order, $order->trans_type);

	if ($order->trans_type == ST_SALESORDER)
		$allocs = get_payments_for($order_no, $order->trans_type, $order->customer_id);

	$sql = "UPDATE ".TB_PREF."sales_orders SET type =".db_escape($order->so_type)." ,
		debtor_no = " . db_escape($order->customer_id) . ",
		branch_code = " . db_escape($order->Branch) . ",
		customer_ref = ". db_escape($order->cust_ref) .",
		reference = ". db_escape($order->reference) .",
		comments = ". db_escape($order->Comments) .",
		ord_date = " . db_escape($ord_date) . ",
		order_type = " .db_escape($order->sales_type) . ",
		ship_via = " . db_escape($order->ship_via) .",
		deliver_to = " . db_escape($order->deliver_to) . ",
		delivery_address = " . db_escape($order->delivery_address) . ",
		contact_phone = " .db_escape($order->phone) . ",
		freight_cost = " .db_escape($order->freight_cost) .",
		from_stk_loc = " .db_escape($order->Location) .",
		delivery_date = " .db_escape($del_date). ",
		version = ".($version+1).",
		payment_terms = " .db_escape($order->payment). ",
		total = ". db_escape($total). ",
		prep_amount = ". db_escape($order->prep_amount) ."
	 WHERE order_no=" . db_escape($order_no) ."
	 AND trans_type=".$order->trans_type." AND version=".$version;
	db_query($sql, 'order Cannot be Updated, this can be concurrent edition conflict');

	$id_tokeep = array();
	foreach ($order->line_items as $line) {
		array_push($id_tokeep , $line->id);
	}
	$id_list = implode(', ', $id_tokeep);
	
	$sql = "DELETE FROM ".TB_PREF."sales_order_details WHERE order_no =" . db_escape($order_no) . " AND trans_type=".$order->trans_type;
	$sql .= " AND id NOT IN ($id_list)";

	db_query($sql, 'Old order Cannot be Deleted');

	if ($SysPrefs->loc_notification() == 1) {
		include_once($path_to_root . '/inventory/includes/inventory_db.inc');
		$st_ids = array();
		$st_names = array();
		$st_num = array();
		$st_reorder = array();
	}
	foreach ($order->line_items as $line) {
		if ($SysPrefs->loc_notification() == 1 && is_inventory_item($line->stock_id))
			$loc = calculate_reorder_level($order->Location, $line, $st_ids, $st_names, $st_num, $st_reorder); 

		if (!$line->id) //new line
			$sql = "INSERT INTO ".TB_PREF."sales_order_details
			 (order_no, trans_type, stk_code,  description, unit_price, quantity,
			  discount_percent, qty_sent)
			 VALUES (".$order_no . ",".$order->trans_type.","
				  .db_escape($line->stock_id) . ","
				  .db_escape($line->item_description) . ", "
				  .db_escape($line->price) . ", "
				  .db_escape($line->quantity) . ", "
				  .db_escape($line->discount_percent) . ", "
				  .db_escape($line->qty_done) ." )";
		else
		$sql = "UPDATE ".TB_PREF."sales_order_details
			SET id=".db_escape($line->id).",
				order_no=$order_no,
				trans_type=".$order->trans_type.",
				stk_code=".db_escape($line->stock_id).",
				description=".db_escape($line->item_description).",
				unit_price=".db_escape($line->price).",
				quantity=".db_escape($line->quantity).",
				discount_percent=".db_escape($line->discount_percent).",
				qty_sent=".db_escape($line->qty_done)."
			 WHERE id = ".db_escape($line->id);

		db_query($sql, 'Old order Cannot be updated');
	} /* inserted line items into sales order details */

	if ($order->trans_type == ST_SALESORDER)
		reallocate_payments($order_no, ST_SALESORDER, $ord_date, $total, $allocs, $order->customer_id);
	add_audit_trail($order->trans_type, $order_no, $order->document_date, _('Updated.'));
	$Refs->save($order->trans_type, $order_no, $order->reference, null, $order->fixed_asset);

	hook_db_postwrite($order, $order->trans_type);
	commit_transaction();
	if ($SysPrefs->loc_notification() == 1 && count($st_ids) > 0)
		send_reorder_email($loc, $st_ids, $st_names, $st_num, $st_reorder);
}

//----------------------------------------------------------------------------------------

function get_sales_order_header($order_no, $trans_type) {
	$sql = "SELECT sorder.*,
		cust.name,
		cust.curr_code,
		cust.address,
		loc.location_name,
		cust.discount,
		stype.sales_type,
		stype.id AS sales_type_id,
		stype.tax_included,
		stype.factor,
		ship.shipper_name,
		tax_group.name AS tax_group_name,
		tax_group.id AS tax_group_id,
		cust.tax_id,
		sorder.alloc,
		IFNULL(allocs.ord_allocs, 0)+IFNULL(inv.inv_allocs ,0) AS sum_paid,
		sorder.prep_amount>0 as prepaid
	FROM ".TB_PREF."sales_orders sorder
			LEFT JOIN (SELECT trans_no_to, sum(amt) ord_allocs FROM ".TB_PREF."cust_allocations
				WHERE trans_type_to=".ST_SALESORDER." AND trans_no_to=".db_escape($order_no)." GROUP BY trans_no_to)
				allocs ON sorder.trans_type=".ST_SALESORDER." AND allocs.trans_no_to=sorder.order_no
			LEFT JOIN (SELECT order_, sum(alloc) inv_allocs FROM ".TB_PREF."debtor_trans 
				WHERE type=".ST_SALESINVOICE." AND order_=".db_escape($order_no)."  GROUP BY order_)
				inv ON sorder.trans_type=".ST_SALESORDER." AND inv.order_=sorder.order_no
			LEFT JOIN ".TB_PREF."shippers ship ON  ship.shipper_id = sorder.ship_via,"
		.TB_PREF."debtors_master cust,"
		.TB_PREF."sales_types stype, "
		.TB_PREF."tax_groups tax_group, "
		.TB_PREF."cust_branch branch,"
		.TB_PREF."locations loc
	WHERE sorder.order_type=stype.id
		AND branch.branch_code = sorder.branch_code
		AND branch.tax_group_id = tax_group.id
		AND sorder.debtor_no = cust.debtor_no
		AND loc.loc_code = sorder.from_stk_loc
		AND sorder.trans_type = " . db_escape($trans_type) ."
		AND sorder.order_no = " . db_escape($order_no );

	$result = db_query($sql, 'order Retreival');

	$num = db_num_rows($result);
	if ($num > 1)
		display_warning("You have duplicate document in database: (type:$trans_type, number:$order_no).");
	else if ($num == 1)
		return db_fetch($result);
	else
		display_warning("You have missing or invalid sales document in database (type:$trans_type, number:$order_no).");
}

//----------------------------------------------------------------------------------------

function get_sales_order_details($order_no, $trans_type) {
	$sql = "SELECT id, stk_code, unit_price,
				line.description,
				line.quantity,
				discount_percent,
				qty_sent as qty_done,
				item.units,
				item.mb_flag,
				item.material_cost
			FROM ".TB_PREF."sales_order_details line,"
				.TB_PREF."stock_master item
			WHERE line.stk_code = item.stock_id
				AND order_no =".db_escape($order_no) 
				." AND trans_type = ".db_escape($trans_type) . " ORDER BY id";

	return db_query($sql, 'Retreive order Line Items');
}

//----------------------------------------------------------------------------------------

function read_sales_order($order_no, &$order, $trans_type) {

	$myrow = get_sales_order_header($order_no, $trans_type);

	$order->trans_type = $myrow['trans_type'];
	$order->so_type =  $myrow['type'];
	$order->trans_no = array($order_no=> $myrow['version']);

	$order->set_customer($myrow['debtor_no'], $myrow['name'], $myrow['curr_code'], $myrow['discount'], $myrow['payment_terms']);

	$order->set_branch($myrow['branch_code'], $myrow['tax_group_id'], $myrow['tax_group_name'], $myrow['contact_phone']);

	$order->set_sales_type($myrow['sales_type_id'], $myrow['sales_type'], $myrow['tax_included'], $myrow['factor']); // no default price calculations on edit

	$order->set_location($myrow['from_stk_loc'], $myrow['location_name']);

	$order->set_delivery($myrow['ship_via'], $myrow['deliver_to'], $myrow['delivery_address'], $myrow['freight_cost']);

	$order->cust_ref = $myrow['customer_ref'];
	$order->sales_type =$myrow['order_type'];
	$order->reference = $myrow['reference'];
	$order->Comments = $myrow['comments'];
	$order->due_date = sql2date($myrow['delivery_date']);
	$order->document_date = sql2date($myrow['ord_date']);

	$order->prepaid = $myrow['prepaid'];
	$order->alloc = $myrow['alloc'];
	$order->sum_paid = $myrow['sum_paid']; // sum of all prepayments to so (also invoiced)
	$order->prep_amount = $myrow['prep_amount'];
	$order->prepayments = get_payments_for($order_no, $myrow['trans_type'], $myrow['debtor_no']);

	$result = get_sales_order_details($order_no, $order->trans_type);
	if (db_num_rows($result) > 0) {
		$line_no=0;
		while ($myrow = db_fetch($result)) {
			$order->add_to_cart($line_no,$myrow['stk_code'],$myrow['quantity'],
				$myrow['unit_price'], $myrow['discount_percent'],
				$myrow['qty_done'], $myrow['material_cost'], $myrow['description'], $myrow['id'] );
			$line_no++;
		}
	}

	return true;
}

//----------------------------------------------------------------------------------------

function sales_order_has_deliveries($order_no) {
	$sql = "SELECT SUM(qty_sent) FROM ".TB_PREF.
	"sales_order_details WHERE order_no=".db_escape($order_no)
	." AND trans_type=".ST_SALESORDER;

	$result = db_query($sql, 'could not query for sales order usage');

	$row = db_fetch_row($result);

	if ($row[0] > 0)
		return true;  // 2010-04-21 added check for eventually voided deliveries, Joe Hunt
	$sql = "SELECT order_ FROM ".TB_PREF."debtor_trans WHERE type=".ST_CUSTDELIVERY." AND order_=".db_escape($order_no);
	$result = db_query($sql, 'The related delivery notes could not be retreived');
	return (db_num_rows($result) > 0);	
}

//----------------------------------------------------------------------------------------

function close_sales_order($order_no) {
	// set the quantity of each item to the already sent quantity. this will mark item as closed.
	$sql = "UPDATE ".TB_PREF."sales_order_details
		SET quantity = qty_sent WHERE order_no = ".db_escape($order_no)
		." AND trans_type=".ST_SALESORDER;

	db_query($sql, 'The sales order detail record could not be updated');
}

//---------------------------------------------------------------------------------------------------------------

function get_invoice_duedate($terms, $invdate) {
	if (!is_date($invdate))
		return new_doc_date();
	
	$myrow = get_payment_terms($terms);
	
	if (!$myrow)
		return $invdate;

	if ($myrow['day_in_following_month'] > 0)
		$duedate = add_days(end_month($invdate), $myrow['day_in_following_month']);
	elseif ($myrow['days_before_due'] > 0)
		$duedate = add_days($invdate, $myrow['days_before_due']);
	else
		$duedate = $invdate;
	return $duedate;
}

function get_customer_to_order($customer_id) {

	// Now check to ensure this account is not on hold */
	$sql = "SELECT cust.name, 
		cust.address,
		credit_status.dissallow_invoices, 
		cust.sales_type AS salestype,
		cust.dimension_id,
		cust.dimension2_id,
		stype.sales_type,
		stype.tax_included,
		stype.factor,
		cust.curr_code,
		cust.discount,
		cust.payment_terms,
		cust.pymt_discount,
		cust.credit_limit - Sum(IFNULL(IF(trans.type IN(".implode(',', array(ST_CUSTCREDIT, ST_CUSTPAYMENT, ST_BANKDEPOSIT))."),
			-1, 1) * (ov_amount + ov_gst + ov_freight + ov_freight_tax + ov_discount),0)) as cur_credit
		FROM ".TB_PREF."debtors_master cust
		LEFT JOIN ".TB_PREF."debtor_trans trans ON trans.type!=".ST_CUSTDELIVERY." AND trans.debtor_no = cust.debtor_no,"
		.TB_PREF."credit_status credit_status,"
		.TB_PREF."sales_types stype
		WHERE cust.sales_type=stype.id
			AND cust.credit_status=credit_status.id
			AND cust.debtor_no=".db_escape($customer_id)
		." GROUP by cust.debtor_no";

	$result =db_query($sql, 'Customer Record Retreive');
	return 	db_fetch($result);
}

function get_branch_to_order($customer_id, $branch_id) {

	// the branch was also selected from the customer selection so default the delivery details from the customer branches table cust_branch. The order process will ask for branch details later anyway
	$sql = "SELECT branch.br_name,
		branch.br_address,
		branch.br_post_address,
		default_location, location_name, default_ship_via,
		tax_group.name AS tax_group_name,
		tax_group.id AS tax_group_id
		FROM ".TB_PREF."cust_branch branch,"
		.TB_PREF."tax_groups tax_group,"
		.TB_PREF."locations location
		WHERE branch.tax_group_id = tax_group.id
		AND location.loc_code=default_location
		AND branch.branch_code=".db_escape($branch_id)."
		AND branch.debtor_no=".db_escape($customer_id);

		return db_query($sql, 'Customer Branch Record Retreive');
}

/*
	Supported filters:

	InvoiceTemplates
	DeliveryTemplates
	OutstandingOnly
	PrepaidOrders
*/
function get_sql_for_sales_orders_view($trans_type, $trans_no, $filter, $stock_item='', $from='', $to='', $ref='', $location=ALL_TEXT, $customer_id=ALL_TEXT) {
	if ($filter=='OutstandingOnly')
		$order_value = 'Sum(line.unit_price*(line.quantity-line.qty_sent)*(1-line.discount_percent))+freight_cost';
	else
		$order_value = 'Sum(line.unit_price*line.quantity*(1-line.discount_percent))+freight_cost';

	$sql = "SELECT 
			sorder.order_no,
			sorder.reference,
			debtor.name,
			branch.br_name,"
			.($filter=='InvoiceTemplates' 
				|| $filter=='DeliveryTemplates' ?
			 "sorder.comments, " : "sorder.customer_ref, ")
			."sorder.ord_date,
			sorder.delivery_date,
			sorder.deliver_to,
			$order_value AS OrderValue,
			sorder.type,
			debtor.curr_code,
			Sum(line.qty_sent) AS TotDelivered,
			Sum(line.quantity) AS TotQuantity,
			Sum(line.invoiced) AS TotInvoiced,
			alloc,
			prep_amount,
			allocs.ord_payments,
			inv.inv_payments,
			sorder.total,
			sorder.trans_type
		FROM ".TB_PREF."sales_orders as sorder
		LEFT JOIN (SELECT trans_no_to, sum(amt) ord_payments FROM ".TB_PREF."cust_allocations WHERE trans_type_to=".ST_SALESORDER." GROUP BY trans_no_to)
			 allocs ON sorder.trans_type=".ST_SALESORDER." AND allocs.trans_no_to=sorder.order_no
		LEFT JOIN (SELECT order_, sum(prep_amount) inv_payments	FROM ".TB_PREF."debtor_trans WHERE type=".ST_SALESINVOICE." GROUP BY order_)
				 inv ON sorder.trans_type=".ST_SALESORDER." AND inv.order_=sorder.order_no,"
			.TB_PREF."sales_order_details as line, "
			.TB_PREF."debtors_master as debtor, "
			.TB_PREF."cust_branch as branch
			WHERE sorder.order_no = line.order_no
			AND sorder.trans_type = line.trans_type
			AND sorder.trans_type = ".db_escape($trans_type)."
			AND sorder.debtor_no = debtor.debtor_no
			AND sorder.branch_code = branch.branch_code
			AND debtor.debtor_no = branch.debtor_no";

	if (isset($trans_no) && $trans_no != '') {
		// search orders with number like 
		$number_like = '%'.$trans_no;
		$sql .= " AND sorder.order_no LIKE ".db_escape($number_like);
				// ." GROUP BY sorder.order_no";
	}
	elseif ($ref != "") {
		// search orders with reference like 
		$sql .= " AND sorder.reference LIKE ".db_escape('%' . $ref . '%');
				// ." GROUP BY sorder.order_no";
	}
	else {	// ... or select inquiry constraints
		if ($filter!='DeliveryTemplates' && $filter!='InvoiceTemplates' && $filter!='OutstandingOnly') {
			$date_after = date2sql($from);
			$date_before = date2sql($to);

			$sql .=  " AND sorder.ord_date >= '$date_after'"
					." AND sorder.ord_date <= '$date_before'";
		}
	}
	if ($trans_type == ST_SALESQUOTE && !check_value('show_all'))
		$sql .= " AND sorder.delivery_date >= '".date2sql(Today())."' AND line.qty_sent=0"; // show only outstanding, not realized quotes

	//if ($selected_customer != -1)
	//	$sql .= " AND sorder.debtor_no=".db_escape($selected_customer);

	if ($stock_item != ALL_TEXT)
		$sql .= " AND line.stk_code=".db_escape($stock_item);

	if ($location != ALL_TEXT)
		$sql .= " AND sorder.from_stk_loc = ".db_escape($location);

	if ($filter=='OutstandingOnly')
		$sql .= " AND line.qty_sent < line.quantity";

	if ($filter=='PrepaidOrders')
		$sql .= " AND prep_amount>0";

	elseif ($filter=='InvoiceTemplates' || $filter=='DeliveryTemplates')
		$sql .= " AND sorder.type=1";

	//Chaiatanya : New Filter
	if ($customer_id != ALL_TEXT)
		$sql .= " AND sorder.debtor_no = ".db_escape($customer_id);		

	$sql .= " GROUP BY sorder.order_no,
		sorder.debtor_no,
		sorder.branch_code,
		sorder.customer_ref,
		sorder.ord_date,
		sorder.deliver_to
		ORDER BY sorder.order_no DESC";
	return $sql;
}

//--------------------------------------------------------------------------------------------------
function update_prepaid_so_line($line_id, $qty_invoiced) {
	$sql = "UPDATE ".TB_PREF."sales_order_details
		SET invoiced = invoiced + ".(float)$qty_invoiced."
		WHERE id=".db_escape($line_id);

	db_query($sql, 'The document detail record could not be updated with invoiced qty');
	return true;
}
/*
	Returns array of all issued invoices to sales order $order_no, optinally up to trans_no==$up_to
*/
function get_sales_order_invoices($order_no) {
	$sql = "SELECT trans_no, dt.type as type, tran_date, reference, prep_amount
		FROM ".TB_PREF."debtor_trans dt
		LEFT JOIN ".TB_PREF."voided v ON v.type=dt.type AND v.id=dt.trans_no
		WHERE ISNULL(v.id) AND dt.type=".ST_SALESINVOICE." AND dt.order_=".db_escape($order_no)
		." ORDER BY dt.tran_date, dt.reference, dt.trans_no";

	return db_query($sql, 'cannot retrieve sales invoices for sales order');
}

function is_sales_order_started($order_no) {
	$sql = "SELECT count(*) FROM ".TB_PREF."sales_order_details WHERE order_no=".db_escape($order_no)." AND trans_type=".ST_SALESORDER
		." AND (invoiced!=0 OR qty_sent!=0)";

	$result = db_fetch(db_query($sql, 'cannot retrieve sales invoices for sales order'));
	return $result[0];
}

//---------------------------------------------------------------------------------------------
//
// Mark/unmark sales order as template.
//
function sales_order_set_template($id, $status) {
	$sql = "UPDATE ".TB_PREF."sales_orders SET type = ".db_escape($status)." WHERE order_no=".db_escape($id);
	db_query($sql, "Can't change sales order type");
}

/*
	Check whether sales order is issued in prepaid mode and already opened
*/

function is_prepaid_order_open($order_no) {
	$sql = "SELECT count(*)
		FROM ".TB_PREF."sales_orders o,
		((SELECT trans_no_to FROM ".TB_PREF."cust_allocations
				WHERE trans_type_to=".ST_SALESORDER." AND trans_no_to=".db_escape($order_no).")
		UNION
		  (SELECT order_ FROM ".TB_PREF."debtor_trans 
			WHERE type=".ST_SALESINVOICE." AND order_=".db_escape($order_no).")) related
	WHERE
		o.prep_amount>0
		AND o.trans_type = ".ST_SALESORDER."
		AND o.order_no = " . db_escape($order_no);

	$result = db_fetch(db_query($sql, 'cannot check prepaid order open'));

	return $result[0];
}

function last_sales_order_detail($order, $field) {
	$sql = "SELECT $field
		FROM ".TB_PREF."sales_order_details d
		WHERE order_no =
		(SELECT order_no FROM ".TB_PREF."sales_orders o
		WHERE debtor_no=" . db_escape($order->customer_id) . "
		ORDER BY order_no DESC LIMIT 1)
		ORDER BY d.id DESC LIMIT 1";        

	$last_query=db_query($sql, 'Could not retrieve last order detail');
	$row = db_fetch_row($last_query);
	return $row == false ? false : $row[0];
}