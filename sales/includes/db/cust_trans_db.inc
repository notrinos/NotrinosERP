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
*	Mark changes in debtor_trans_details
*/
function update_customer_trans_version($type, $versions) {

	$sql= 'UPDATE '.TB_PREF.'debtor_trans SET version=version+1 WHERE type='.db_escape($type).' AND (';

	foreach ($versions as $trans_no => $version)
		$where[] = '(trans_no='.db_escape($trans_no).' AND version='.$version.')';

		$sql .= implode(' OR ', $where).')';

	return  db_query($sql, 'Concurrent editing conflict');
}

/*
*	Gets document header versions for transaction set of type $type
*	$trans_no = array(num1, num2,...);
*	returns array(num1=>ver1, num2=>ver2...)
*/
function get_customer_trans_version($type, $trans_no) {
	if (!is_array($trans_no))
		$trans_no = array( $trans_no );

	$sql= 'SELECT trans_no, version FROM '.TB_PREF.'debtor_trans WHERE type='.db_escape($type).' AND (';

	foreach ($trans_no as $key=>$trans)
		$trans_no[$key] = 'trans_no='.db_escape($trans_no[$key]);

	$sql .= implode(' OR ', $trans_no).')';

	$res = db_query($sql, 'document version retreival');

	$vers = array();
	while($mysql=db_fetch($res)) {
		$vers[$mysql['trans_no']] = $mysql['version'];
	}
	return $vers;
}

/*
*	$Total, $Tax, $Freight, $discount all in customer's currency
*	date_ is display date (non-sql)
*/
function write_customer_trans($trans_type, $trans_no, $debtor_no, $BranchNo, $date_, $reference, $Total, $discount=0, $Tax=0, $Freight=0, $FreightTax=0, $sales_type=0, $order_no=0, $ship_via=0, $due_date='', $AllocAmt=0, $rate=0, $dimension_id=0, $dimension2_id=0, $payment_terms=null, $tax_included=0, $prep_amount=0) {
	$new = $trans_no == 0;
	$curr = get_customer_currency($debtor_no);
	if ($rate == 0)
		$rate = get_exchange_rate_from_home_currency($curr, $date_);

	$SQLDate = date2sql($date_);
	$SQLDueDate = $due_date == '' ? '0000-00-00' : date2sql($due_date);
	
	if ($trans_type == ST_BANKPAYMENT)
		$Total = -$Total;

	if ($new || !exists_customer_trans($trans_type, $trans_no)) {
		if ($new)
			$trans_no = get_next_trans_no($trans_type);

		$sql = "INSERT INTO ".TB_PREF."debtor_trans (
			trans_no, type,
			debtor_no, branch_code,
			tran_date, due_date,
			reference, tpe,
			order_, ov_amount, ov_discount,
			ov_gst, ov_freight, ov_freight_tax,
			rate, ship_via, alloc,
			dimension_id, dimension2_id, payment_terms, tax_included, prep_amount
			) VALUES (".db_escape($trans_no).", ".db_escape($trans_type).",
			".db_escape($debtor_no).", ".db_escape($BranchNo).",
			'$SQLDate', '$SQLDueDate', ".db_escape($reference).",
			".db_escape($sales_type).", ".db_escape($order_no).", $Total, ".db_escape($discount).", $Tax,
			".db_escape($Freight).",
			$FreightTax, $rate, ".db_escape($ship_via).", $AllocAmt,
			".db_escape($dimension_id).", ".db_escape($dimension2_id).", "
			.db_escape($payment_terms, true).", "
			.db_escape($tax_included).", ".db_escape($prep_amount).")";
	}
	else {	// may be optional argument should stay unchanged ?
		$sql = "UPDATE ".TB_PREF."debtor_trans SET
		debtor_no=".db_escape($debtor_no)." , branch_code=".db_escape($BranchNo).",
		tran_date='$SQLDate', due_date='$SQLDueDate',
		reference=".db_escape($reference).", tpe=".db_escape($sales_type).", order_=".db_escape($order_no).",
		ov_amount=$Total, ov_discount=".db_escape($discount).", ov_gst=$Tax,
		ov_freight=".db_escape($Freight).", ov_freight_tax=$FreightTax, rate=$rate,
		ship_via=".db_escape($ship_via).", alloc=$AllocAmt,
		dimension_id=".db_escape($dimension_id).", dimension2_id=".db_escape($dimension2_id).",
		payment_terms=".db_escape($payment_terms, true).",
		tax_included=".db_escape($tax_included).",
		prep_amount =".db_escape($prep_amount)."
		WHERE trans_no=".db_escape($trans_no)." AND type=".db_escape($trans_type);
	}
	db_query($sql, 'The debtor transaction record could not be inserted');

	if ($trans_type != ST_JOURNAL && $trans_type != ST_BANKDEPOSIT && $trans_type != ST_BANKPAYMENT)
		add_audit_trail($trans_type, $trans_no, $date_, $new ? '' : _('Updated.'));

	return $trans_no;
}

function get_customer_trans($trans_id, $trans_type, $customer_id=null) {
	global $SysPrefs;

	$sql = "SELECT trans.*, 
		ov_amount+ov_gst+ov_freight+ov_freight_tax+ov_discount AS Total,
		cust.name AS DebtorName, cust.address,
		cust.debtor_ref,
		cust.curr_code,
		cust.tax_id,
		trans.prep_amount>0 as prepaid,
		com.memo_";

	if ($trans_type == ST_CUSTPAYMENT || $trans_type == ST_BANKDEPOSIT) {
		// it's a payment so also get the bank account
		$sql .= ",bank_act,".TB_PREF."bank_accounts.bank_name, ".TB_PREF."bank_accounts.bank_account_name,
			".TB_PREF."bank_accounts.account_type AS BankTransType,
			".TB_PREF."bank_accounts.bank_curr_code,
			".TB_PREF."bank_trans.amount as bank_amount";
	}

	if ($trans_type == ST_SALESINVOICE || $trans_type == ST_CUSTCREDIT || $trans_type == ST_CUSTDELIVERY) {
		// it's an invoice so also get the shipper and salestype
		$sql .= ", ".TB_PREF."shippers.shipper_name, "
		.TB_PREF."sales_types.sales_type, "
		.TB_PREF."sales_types.tax_included, "
		."branch.*, "
		."cust.discount, "
		.TB_PREF."tax_groups.name AS tax_group_name, "
		.TB_PREF."tax_groups.id AS tax_group_id ";
	}

	if ($trans_type == ST_JOURNAL)
		$sql .= ", branch.*";

	$sql .= " FROM ".TB_PREF."debtor_trans trans
			LEFT JOIN ".TB_PREF."comments com ON trans.type=com.type AND trans.trans_no=com.id
			LEFT JOIN ".TB_PREF."shippers ON ".TB_PREF."shippers.shipper_id=trans.ship_via, 
			".TB_PREF."debtors_master cust";

	if ($trans_type == ST_CUSTPAYMENT || $trans_type == ST_BANKDEPOSIT)
		// it's a payment so also get the bank account
		$sql .= ", ".TB_PREF."bank_trans, ".TB_PREF."bank_accounts";

	if ($trans_type == ST_SALESINVOICE || $trans_type == ST_CUSTCREDIT || $trans_type == ST_CUSTDELIVERY) {
		// it's an invoice so also get the shipper, salestypes
		$sql .= ", ".TB_PREF."sales_types, "
		.TB_PREF."cust_branch branch, "
		.TB_PREF."tax_groups ";
	}

	if ($trans_type == ST_JOURNAL)
		$sql .= ", ".TB_PREF."cust_branch branch ";

	$sql .= " WHERE trans.trans_no=".db_escape($trans_id)."
		AND trans.type=".db_escape($trans_type)."
		AND trans.debtor_no=cust.debtor_no";

	if (isset($customer_id))
		$sql .= " AND trans.debtor_no=".db_escape($customer_id);

	if ($trans_type == ST_CUSTPAYMENT || $trans_type == ST_BANKDEPOSIT) {
		// it's a payment so also get the bank account
		$sql .= " AND ".TB_PREF."bank_trans.trans_no =".db_escape($trans_id)."
			AND ".TB_PREF."bank_trans.type=$trans_type
			AND ".TB_PREF."bank_trans.amount != 0
			AND ".TB_PREF."bank_accounts.id=".TB_PREF."bank_trans.bank_act ";
	}
	if ($trans_type == ST_SALESINVOICE || $trans_type == ST_CUSTCREDIT || $trans_type == ST_CUSTDELIVERY) {
		// it's an invoice so also get the shipper
		$sql .= " AND ".TB_PREF."sales_types.id = trans.tpe
			AND branch.branch_code = trans.branch_code
			AND branch.tax_group_id = ".TB_PREF."tax_groups.id ";
	}
	if ($trans_type == ST_JOURNAL)
		$sql .= " AND branch.branch_code = trans.branch_code ";

	$result = db_query($sql, 'Cannot retreive a debtor transaction');

	if (db_num_rows($result) == 0) {
		// can't return nothing
		if ($SysPrefs->go_debug)
			display_backtrace();
		display_db_error('no debtor trans found for given params', $sql, true);
		exit;
	}
	if (db_num_rows($result) > 1) {
		// can't return multiple
		if($SysPrefs->go_debug)
			display_backtrace();
		display_db_error('duplicate debtor transactions found for given params', $sql, true);
		exit;
	}

	$row = db_fetch($result);
	return $row;
}

function exists_customer_trans($type, $type_no) {
	$sql = "SELECT trans_no FROM ".TB_PREF."debtor_trans WHERE type=".db_escape($type)." AND trans_no=".db_escape($type_no);

	$result = db_query($sql, 'Cannot retreive a debtor transaction');

	return (db_num_rows($result) > 0);
}

/*
*	retreives the related sales order for a given trans
*/
function get_customer_trans_order($type, $type_no) {
	$sql = "SELECT order_ FROM ".TB_PREF."debtor_trans WHERE type=".db_escape($type)." AND trans_no=".db_escape($type_no);

	$result = db_query($sql, 'The debtor transaction could not be queried');

	$row = db_fetch_row($result);

	return $row[0];
}

function get_customer_details_from_trans($type, $type_no) {
	$sql = "SELECT debtor.name, debtor.curr_code, branch.br_name
		FROM "
			.TB_PREF."debtors_master debtor,"
			.TB_PREF."cust_branch branch,"
			.TB_PREF."debtor_trans trans
		WHERE trans.type=".db_escape($type)." AND trans.trans_no=".db_escape($type_no)."
		AND debtor.debtor_no = trans.debtor_no
		AND	branch.branch_code = trans.branch_code";

	$result = db_query($sql, 'could not get customer details from trans');
	return db_fetch($result);
}

function void_customer_trans($type, $type_no) {
	// clear all values and mark as void
	$sql = "UPDATE ".TB_PREF."debtor_trans SET ov_amount=0, ov_discount=0, ov_gst=0, ov_freight=0,
		ov_freight_tax=0, alloc=0, prep_amount=0, version=version+1 WHERE type=".db_escape($type)." AND trans_no=".db_escape($type_no);

	db_query($sql, "could not void debtor transactions for type=$type and trans_no=$type_no");
}

function clear_customer_trans($type, $type_no) {
	// Delete
	$sql = "DELETE FROM ".TB_PREF."debtor_trans WHERE type=".db_escape($type)." AND trans_no=".db_escape($type_no);

	db_query($sql, "could not clear debtor transactions for type=$type and trans_no=$type_no");
}

function post_void_customer_trans($type, $type_no) {
	switch ($type) {
		case ST_SALESINVOICE :
		case ST_CUSTCREDIT   :
			void_sales_invoice($type, $type_no);
			break;
		case ST_CUSTDELIVERY :
			void_sales_delivery($type, $type_no);
			break;
		case ST_CUSTPAYMENT :
			void_customer_payment($type, $type_no);
			break;
	}
}

function get_sql_for_customer_inquiry($from, $to, $cust_id=ALL_TEXT, $filter=ALL_TEXT, $show_voided=0, $ref='') {

	$sql = "SELECT 
		trans.type, 
		trans.trans_no, 
		trans.order_, 
		trans.reference,
		trans.tran_date, 
		trans.due_date, 
		debtor.name, 
		branch.br_name,
		debtor.curr_code,
		IF(prep_amount, prep_amount, trans.ov_amount + trans.ov_gst + trans.ov_freight 
			+ trans.ov_freight_tax + trans.ov_discount)	AS TotalAmount,"
		. "IF(trans.type IN(".implode(',',  array(ST_CUSTCREDIT,ST_CUSTPAYMENT,ST_BANKDEPOSIT))."), -1, 1)
				*(IF(prep_amount, prep_amount, trans.ov_amount + trans.ov_gst + trans.ov_freight 
			+ trans.ov_freight_tax + trans.ov_discount)-trans.alloc) Balance, 
		debtor.debtor_no,";

		$sql .= "trans.alloc AS Allocated,
		((trans.type = ".ST_SALESINVOICE." || trans.type = ".ST_JOURNAL.")
			AND trans.due_date < '" . date2sql(Today()) . "') AS OverDue ,
		Sum(line.quantity-line.qty_done) AS Outstanding,
		Sum(line.qty_done) AS HasChild,
		prep_amount,
		(trans.ov_amount + trans.ov_gst + trans.ov_freight + trans.ov_freight_tax + trans.ov_discount) = 0 AND !ISNULL(v.date_) AS voided
		FROM "
			.TB_PREF."debtor_trans as trans
			LEFT JOIN ".TB_PREF."debtor_trans_details as line
				ON trans.trans_no=line.debtor_trans_no AND trans.type=line.debtor_trans_type
			LEFT JOIN ".TB_PREF."voided as v
				ON trans.trans_no=v.id AND trans.type=v.type
						LEFT JOIN ".TB_PREF."audit_trail as audit ON (trans.type=audit.type AND trans.trans_no=audit.trans_no)
						LEFT JOIN ".TB_PREF."users as user ON (audit.user=user.id)
			LEFT JOIN ".TB_PREF."cust_branch as branch ON trans.branch_code=branch.branch_code,"
			.TB_PREF."debtors_master as debtor
		WHERE debtor.debtor_no = trans.debtor_no"; // exclude voided transactions and self-balancing (journal) transactions:
	if (!$show_voided) 	
			$sql .= " AND ISNULL(v.date_) AND (trans.ov_amount + trans.ov_gst + trans.ov_freight + trans.ov_freight_tax + trans.ov_discount) != 0";
	if (!empty($ref))
    	$sql .= " AND trans.reference LIKE ". db_escape("%$ref%");

	if ($filter == '2')
		$sql .= " AND ABS(IF(prep_amount, prep_amount, ABS(trans.ov_amount) + trans.ov_gst + trans.ov_freight + trans.ov_freight_tax + trans.ov_discount)-trans.alloc)>".FLOAT_COMP_DELTA;
	else
		$sql .= " AND trans.tran_date >= '".date2sql($from)."' AND trans.tran_date <= '".date2sql($to)."'";

	if ($cust_id != ALL_TEXT)
		$sql .= " AND trans.debtor_no = ".db_escape($cust_id);

	if ($filter != ALL_TEXT) {
		if ($filter == '1')
			$sql .= " AND (trans.type = ".ST_SALESINVOICE.") ";
		elseif ($filter == '2')
			$sql .= " AND (trans.type <> ".ST_CUSTDELIVERY.") ";
		elseif ($filter == '3')
			$sql .= " AND (trans.type = ".ST_CUSTPAYMENT 
					." OR trans.type = ".ST_BANKDEPOSIT." OR trans.type = ".ST_BANKPAYMENT.") ";
		elseif ($filter == '4')
			$sql .= " AND trans.type = ".ST_CUSTCREDIT." ";
		elseif ($filter == '5')
			$sql .= " AND trans.type = ".ST_CUSTDELIVERY." ";
		elseif ($filter == '6')
			$sql .= " AND trans.type = ".ST_JOURNAL." ";

		if ($filter == '2')
			$sql .= " AND trans.due_date < '".date2sql(Today())."' AND (ABS(trans.ov_amount) + trans.ov_gst + trans.ov_freight_tax + trans.ov_freight + trans.ov_discount - trans.alloc > 0) ";
	}

	$sql .= " GROUP BY trans.trans_no, trans.type, trans.debtor_no";

	return $sql;
}

function get_sql_for_sales_deliveries_view($from, $to, $customer_id, $stock_item, $location, $delivery, $outstanding=false) {
	$sql = "SELECT trans.trans_no,
		debtor.name,
		branch.branch_code,
		branch.br_name,
		sorder.deliver_to,
		trans.reference,
		sorder.customer_ref,
		trans.tran_date,
		trans.due_date,
		(ov_amount+ov_gst+ov_freight+ov_freight_tax) AS DeliveryValue,
		debtor.curr_code,
		Sum(line.quantity-line.qty_done) AND sorder.prep_amount=0 AS Outstanding,
		Sum(line.qty_done) AS Done
	FROM "
		.TB_PREF."sales_orders as sorder, "
		.TB_PREF."debtor_trans as trans, "
		.TB_PREF."debtor_trans_details as line, "
		.TB_PREF."debtors_master as debtor, "
		.TB_PREF."cust_branch as branch
	WHERE
		sorder.order_no = trans.order_ AND
		trans.debtor_no = debtor.debtor_no
		AND trans.type = ".ST_CUSTDELIVERY."
		AND line.debtor_trans_no = trans.trans_no
		AND line.debtor_trans_type = trans.type
		AND trans.branch_code = branch.branch_code
		AND trans.debtor_no = branch.debtor_no ";

	if ($outstanding == true)
		$sql .= " AND line.qty_done < line.quantity ";

	//figure out the sql required from the inputs available
	if ($delivery) {
		$sql .= " AND trans.trans_no LIKE ".db_escape('%' . $delivery . '%');
		$sql .= " GROUP BY trans.trans_no";
	}
	else {
		$sql .= " AND trans.tran_date >= '".date2sql($from)."' AND trans.tran_date <= '".date2sql($to)."'";

		if ($stock_item != ALL_TEXT)
			$sql .= " AND line.stock_id=".db_escape($stock_item)." ";

		if ($location != ALL_TEXT)
			$sql .= " AND sorder.from_stk_loc = ".db_escape($location)." ";

		if ($customer_id != ALL_TEXT)
			$sql .= " AND trans.debtor_no = ".db_escape($customer_id);

		$sql .= " GROUP BY trans.trans_no ";

	} //end no delivery number selected
	return $sql;
}
