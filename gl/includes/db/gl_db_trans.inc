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
*	Base function for adding a GL transaction
*	$date_ is display date (non-sql)
*	$amount is in $currency currency
*	if $currency is not set, then defaults to no conversion
*/
function add_gl_trans($type, $trans_id, $date_, $account, $dimension, $dimension2, $memo_, $amount, $currency=null, $person_type_id=null, $person_id=null, $err_msg='', $rate=0) {
	global $SysPrefs;

	$date = date2sql($date_);
	if ($currency != null) {
		if ($rate == 0)
			$amount_in_home_currency = to_home_currency($amount, $currency, $date_);
		else
			$amount_in_home_currency = round2($amount * $rate,  user_price_dec());
	}		
	else
		$amount_in_home_currency = round2($amount, user_price_dec());
	if ($dimension == null || $dimension < 0)
		$dimension = 0;
	if ($dimension2 == null || $dimension2 < 0)
		$dimension2 = 0;
	if (isset($SysPrefs->use_audit_trail) && $SysPrefs->use_audit_trail) {
		if ($memo_ == '' || $memo_ == null)
			$memo_ = $_SESSION['wa_current_user']->username;
		else
			$memo_ = $_SESSION['wa_current_user']->username.' - '.$memo_;
	}
	if (!is_subledger_account($account) || $account==get_company_pref('grn_clearing_act'))
		$person_id = $person_type_id = null;
	
	$sql = "INSERT INTO ".TB_PREF."gl_trans ( type, type_no, tran_date, account, dimension_id, dimension2_id, memo_, amount";

	if ($person_type_id != null)
		$sql .= ", person_type_id, person_id";

	$sql .= ") ";

	$sql .= "VALUES (".db_escape($type).", ".db_escape($trans_id).", '$date',
		".db_escape($account).", ".db_escape($dimension).", "
		.db_escape($dimension2).", ".db_escape($memo_).", "
		.db_escape($amount_in_home_currency);

	if ($person_type_id != null)
		$sql .= ", ".db_escape($person_type_id).", ". db_escape($person_id);

	$sql .= ") ";

	if ($err_msg == '')
		$err_msg = 'The GL transaction could not be inserted';

	db_query($sql, $err_msg);
	return $amount_in_home_currency;
}

//--------------------------------------------------------------------------------

// GL Trans for standard costing, always home currency regardless of person
// $date_ is display date (non-sql)
// $amount is in HOME currency

function add_gl_trans_std_cost($type, $trans_id, $date_, $account, $dimension, $dimension2, $memo_,	$amount, $person_type_id=null, $person_id=null, $err_msg='') {

	if ($amount != 0)
		return add_gl_trans($type, $trans_id, $date_, $account, $dimension, $dimension2, $memo_, $amount, null, $person_type_id, $person_id, $err_msg);
	else
		return 0;
}

// Function for even out rounding problems
function add_gl_balance($type, $trans_id, $date_, $amount, $person_type_id=null, $person_id=null) {
	$amount = round2($amount, user_price_dec());

	if (floatcmp($amount, 0)) {
		error_log(sprintf( _('Rounding error %s encountered for trans_type:%s,trans_no:%s'), $amount, $type, $trans_id));
		return add_gl_trans($type, $trans_id, $date_, get_company_pref('exchange_diff_act'), 0, 0, '',
			$amount, null, $person_type_id, $person_id, 'The balanced GL transaction could not be inserted');
	}
	else
		return 0;
}

//--------------------------------------------------------------------------------

function get_gl_transactions($from_date, $to_date, $trans_no=0, $account=null, $dimension=0, $dimension2=0, $filter_type=null, $amount_min=null, $amount_max=null, $person_type=null, $person_id=null, $memo='') {
	global $SysPrefs;

	$from = date2sql($from_date);
	$to = date2sql($to_date);

	$sql = "SELECT gl.*, j.event_date, j.doc_date, a.gl_seq, u.user_id, st.supp_reference, gl.person_id subcode,
			IFNULL(IFNULL(sup.supp_name, debt.name), bt.person_id) as person_name, 
			IFNULL(gl.person_id, IFNULL(sup.supplier_id, IFNULL(debt.debtor_no, bt.person_id))) as person_id,
			IF(gl.person_id, gl.person_type_id, IF(sup.supplier_id,".  PT_SUPPLIER . "," .  "IF(debt.debtor_no," . PT_CUSTOMER . "," . 
			"IF(bt.person_id != '' AND !ISNULL(bt.person_id), bt.person_type_id, -1)))) as person_type_id,
			IFNULL(st.tran_date, IFNULL(dt.tran_date, IFNULL(bt.trans_date, IFNULL(grn.delivery_date, gl.tran_date)))) as doc_date,
			coa.account_name, ref.reference, IF(ISNULL(c.memo_), gl.memo_, CONCAT(gl.memo_,' ',c.memo_)) AS memo
			 FROM "
			.TB_PREF."gl_trans gl
			LEFT JOIN ".TB_PREF."voided v ON gl.type_no=v.id AND v.type=gl.type

			LEFT JOIN ".TB_PREF."supp_trans st ON gl.type_no=st.trans_no AND st.type=gl.type AND (gl.type!=".ST_JOURNAL." OR gl.person_id=st.supplier_id)
			LEFT JOIN ".TB_PREF."grn_batch grn ON grn.id=gl.type_no AND gl.type=".ST_SUPPRECEIVE."
			LEFT JOIN ".TB_PREF."debtor_trans dt ON gl.type_no=dt.trans_no AND dt.type=gl.type AND (gl.type!=".ST_JOURNAL." OR gl.person_id=dt.debtor_no)

			LEFT JOIN ".TB_PREF."suppliers sup ON st.supplier_id=sup.supplier_id
			LEFT JOIN ".TB_PREF."cust_branch branch ON dt.branch_code=branch.branch_code
			LEFT JOIN ".TB_PREF."debtors_master debt ON dt.debtor_no=debt.debtor_no

			LEFT JOIN ".TB_PREF."bank_trans bt ON bt.type=gl.type AND bt.trans_no=gl.type_no AND bt.amount!=0
				 AND (bt.person_id != '' AND !ISNULL(bt.person_id))

			LEFT JOIN ".TB_PREF."journal j ON j.type=gl.type AND j.trans_no=gl.type_no
			LEFT JOIN ".TB_PREF."audit_trail a ON a.type=gl.type AND a.trans_no=gl.type_no AND NOT ISNULL(gl_seq)
			LEFT JOIN ".TB_PREF."users u ON a.user=u.id
			LEFT JOIN ".TB_PREF."comments c ON c.id=gl.type_no AND c.type=gl.type

			LEFT JOIN ".TB_PREF."refs ref ON ref.type=gl.type AND ref.id=gl.type_no,"
		.TB_PREF."chart_master coa
		WHERE coa.account_code=gl.account
		AND ISNULL(v.date_)
		AND gl.tran_date >= '$from'
		AND gl.tran_date <= '$to'
		AND gl.amount <> 0"; 

	if ($trans_no > 0)
		$sql .= " AND gl.type_no LIKE ".db_escape('%'.$trans_no);
	if ($account != null)
		$sql .= " AND gl.account = ".db_escape($account);
	if ($dimension != 0)
		$sql .= " AND gl.dimension_id = ".($dimension<0 ? 0 : db_escape($dimension));
	if ($dimension2 != 0)
		$sql .= " AND gl.dimension2_id = ".($dimension2<0 ? 0 : db_escape($dimension2));
	if ($filter_type != null)
		$sql .= " AND gl.type IN (" . $filter_type .")";
	if ($amount_min != null)
		$sql .= " AND ABS(gl.amount) >= ABS(".db_escape($amount_min).")";
	if ($amount_max != null)
		$sql .= " AND ABS(gl.amount) <= ABS(".db_escape($amount_max).")";
	if ($memo)
		$sql .= " AND (gl.memo_ LIKE ". db_escape("%$memo%") . " OR c.memo_ LIKE " . db_escape("%$memo%") . ")";

	$sql .= " GROUP BY counter";

	$sql .= " HAVING TRUE";
	if ($person_type != 0)
			$sql .= " AND person_type_id=".db_escape($person_type); 
	if ($person_id != 0)
			$sql .= " AND person_id=".db_escape($person_id); 

	$sql .= " ORDER BY tran_date, counter";

	return db_query($sql, 'The transactions for could not be retrieved');
}

//--------------------------------------------------------------------------------

function get_gl_trans($type, $trans_id) {
	$sql = "SELECT gl.*, cm.account_name, IFNULL(refs.reference, '') AS reference, user.real_name, 
		COALESCE(st.tran_date, dt.tran_date, bt.trans_date, grn.delivery_date, gl.tran_date) as doc_date,
		IF(ISNULL(st.supp_reference), '', st.supp_reference) AS supp_reference
	FROM ".TB_PREF."gl_trans as gl
		LEFT JOIN ".TB_PREF."chart_master as cm ON gl.account = cm.account_code
		LEFT JOIN ".TB_PREF."refs as refs ON (gl.type=refs.type AND gl.type_no=refs.id)
		LEFT JOIN ".TB_PREF."audit_trail as audit ON (gl.type=audit.type AND gl.type_no=audit.trans_no AND NOT ISNULL(gl_seq))
		LEFT JOIN ".TB_PREF."users as user ON (audit.user=user.id)
	# all this below just to retrieve doc_date :>
		LEFT JOIN ".TB_PREF."supp_trans st ON gl.type_no=st.trans_no AND st.type=gl.type AND (gl.type!=".ST_JOURNAL." OR gl.person_id=st.supplier_id)
		LEFT JOIN ".TB_PREF."grn_batch grn ON grn.id=gl.type_no AND gl.type=".ST_SUPPRECEIVE." AND gl.person_id=grn.supplier_id
		LEFT JOIN ".TB_PREF."debtor_trans dt ON gl.type_no=dt.trans_no AND dt.type=gl.type AND (gl.type!=".ST_JOURNAL." OR gl.person_id=dt.debtor_no)
		LEFT JOIN ".TB_PREF."bank_trans bt ON bt.type=gl.type AND bt.trans_no=gl.type_no AND bt.amount!=0
			 AND bt.person_type_id=gl.person_type_id AND bt.person_id=gl.person_id
		LEFT JOIN ".TB_PREF."journal j ON j.type=gl.type AND j.trans_no=gl.type_no"

		." WHERE gl.type= ".db_escape($type) 
		." AND gl.type_no = ".db_escape($trans_id)
		." AND gl.amount <> 0"
		." ORDER BY tran_date, counter";

	return db_query($sql, 'The gl transactions could not be retrieved');
}

//--------------------------------------------------------------------------------

function get_gl_wo_cost_trans($trans_id, $cost_type=-1, $all_gl=false) {
	$sql = "SELECT costing.*, gl.*, chart.account_name, com.memo_
		FROM "
		.TB_PREF."wo_costing costing, "
		.TB_PREF."gl_trans gl LEFT JOIN ".TB_PREF."comments com ON gl.type=com.type	AND gl.type_no=com.id,"
		.TB_PREF."chart_master chart
		WHERE 
			costing.workorder_id=".db_escape($trans_id)
		."	AND chart.account_code=gl.account
			AND gl.type=costing.trans_type
			AND gl.type_no=costing.trans_no";
	if ($cost_type != -1)
		$sql .= " AND costing.cost_type=".db_escape($cost_type);
	$sql .= $all_gl ? " AND amount != 0" : " AND amount < 0";

	return db_query($sql, 'The gl transactions could not be retrieved');
}

function get_gl_wo_issue_trans($trans_id, $person_id=-1, $all_gl=false) {
	$sql = "SELECT issue.*, gl.*, chart.account_name, com.memo_
		FROM "
			.TB_PREF."wo_issues issue,"
			.TB_PREF."gl_trans gl LEFT JOIN ".TB_PREF."comments com ON gl.type=com.type	AND gl.type_no=com.id,"
			.TB_PREF."chart_master chart
		WHERE issue.workorder_id=".db_escape($trans_id)
		 ." AND chart.account_code=gl.account
			AND gl.type=".ST_MANUISSUE." AND gl.type_no=issue.issue_no";
			// ." AND gl.person_type_id=".PT_WORKORDER;
	if ($person_id != -1)
		$sql .= " AND gl.person_id=".db_escape($person_id);
	$sql .= $all_gl ? " AND amount != 0" : " AND amount < 0";
	$sql .= " ORDER BY type, type_no";
	return db_query($sql, 'The gl transactions could not be retrieved');
}

function get_gl_wo_productions($trans_id, $all_gl=false) {
	$sql = "SELECT rcv.*, gl.*, chart.account_name, com.memo_
		FROM "
		.TB_PREF."wo_manufacture rcv, "
		.TB_PREF."gl_trans gl LEFT JOIN ".TB_PREF."comments com ON gl.type=com.type	AND gl.type_no=com.id,"
		.TB_PREF."chart_master chart
		WHERE 
			rcv.workorder_id=".db_escape($trans_id)
		."	AND chart.account_code=gl.account
			AND gl.type=".ST_MANURECEIVE."
			AND gl.type_no=rcv.id
			AND amount != 0 "
		.($all_gl ? " AND amount != 0" : " AND amount < 0")
		." ORDER BY type, type_no";

	return db_query($sql, 'The gl transactions could not be retrieved');
}

function get_gl_balance_from_to($from_date, $to_date, $account, $dimension=0, $dimension2=0) {

	$sql = "SELECT SUM(amount) FROM ".TB_PREF."gl_trans
		WHERE account='$account'";
	if ($from_date != "")
		$sql .= "  AND tran_date > '".date2sql($from_date)."'";
	if ($to_date != "")
		$sql .= "  AND tran_date < '".date2sql($to_date)."'";
	if ($dimension != 0)
		$sql .= " AND dimension_id = ".($dimension<0 ? 0 : db_escape($dimension));
	if ($dimension2 != 0)
		$sql .= " AND dimension2_id = ".($dimension2<0 ? 0 : db_escape($dimension2));

	$result = db_query($sql, 'The starting balance for account $account could not be calculated');

	$row = db_fetch_row($result);
	return round2($row[0], user_price_dec());
}

//--------------------------------------------------------------------------------

function get_gl_trans_from_to($from_date, $to_date, $account, $dimension=0, $dimension2=0) {

	$sql = "SELECT SUM(amount) FROM ".TB_PREF."gl_trans
		WHERE account='$account'";
	if ($from_date != "")
		$sql .= " AND tran_date >= '".date2sql($from_date)."'";
	if ($to_date != "")
		$sql .= " AND tran_date <= '".date2sql($to_date)."'";
	if ($dimension != 0)
		$sql .= " AND dimension_id = ".($dimension<0 ? 0 : db_escape($dimension));
	if ($dimension2 != 0)
		$sql .= " AND dimension2_id = ".($dimension2<0 ? 0 : db_escape($dimension2));

	$result = db_query($sql, 'Transactions for account $account could not be calculated');

	$row = db_fetch_row($result);
	return round2($row[0], user_price_dec());
}

//----------------------------------------------------------------------------------------------------
function get_balance($account, $dimension, $dimension2, $from, $to, $from_incl=true, $to_incl=true) {

	$sql = "SELECT	SUM(IF(amount >= 0, amount, 0)) as debit, 
					SUM(IF(amount < 0, -amount, 0)) as credit,
					SUM(amount) as balance 
		FROM ".TB_PREF."gl_trans trans,"
			.TB_PREF."chart_master coa,"
			.TB_PREF."chart_types act_type, "
			.TB_PREF."chart_class act_class
		WHERE trans.account=coa.account_code
			AND coa.account_type=act_type.id 
		AND act_type.class_id=act_class.cid"
		." AND ".($from_incl ? "tran_date >= '".date2sql($from)."'" : "tran_date > IF(ctype>0 AND ctype<".CL_INCOME.", '0000-00-00', '".date2sql($from)."')")
		." AND ".($to_incl ? "tran_date <= '".date2sql($to)."'" : "tran_date < '".date2sql($to)."'")
		.($account == null ? '' : " AND account=".db_escape($account))
		.($dimension == 0 ? ''  : " AND dimension_id = ".($dimension<0 ? 0 : db_escape($dimension)))
		.($dimension2 == 0 ? '' : " AND dimension2_id = ".($dimension2<0 ? 0 : db_escape($dimension2)));

	$result = db_query($sql, 'No general ledger accounts were returned');

	return db_fetch($result);
}

//--------------------------------------------------------------------------------

function get_budget_trans_from_to($from_date, $to_date, $account, $dimension=0, $dimension2=0) {

	$sql = "SELECT SUM(amount)
		FROM ".TB_PREF."budget_trans
		WHERE account=".db_escape($account);
	if ($from_date != "")
		$sql .= " AND tran_date >= '".date2sql($from_date)."' ";
	if ($to_date != "")
		$sql .= " AND tran_date <= '".date2sql($to_date)."' ";
	if ($dimension != 0)
		$sql .= " AND dimension_id = ".($dimension<0?0:db_escape($dimension));
	if ($dimension2 != 0)
		$sql .= " AND dimension2_id = ".($dimension2<0?0:db_escape($dimension2));

	$result = db_query($sql, 'No budget accounts were returned');

	$row = db_fetch_row($result);
	return $row[0];
}

//-------------------------------------------------------------------------------------

function exists_gl_budget($date_, $account, $dimension, $dimension2) {
	$sql = "SELECT account FROM ".TB_PREF."budget_trans WHERE account=".db_escape($account)
	." AND tran_date='$date_' AND
		dimension_id=".db_escape($dimension)." AND dimension2_id=".db_escape($dimension2);
	$result = db_query($sql, 'Cannot retreive a gl transaction');

	return (db_num_rows($result) > 0);
}

function add_update_gl_budget_trans($date_, $account, $dimension, $dimension2, $amount) {
	$date = date2sql($date_);

	if (exists_gl_budget($date, $account, $dimension, $dimension2))
		$sql = "UPDATE ".TB_PREF."budget_trans SET amount=".db_escape($amount)
		." WHERE account=".db_escape($account)
		." AND dimension_id=".db_escape($dimension)
		." AND dimension2_id=".db_escape($dimension2)
		." AND tran_date='$date'";
	else
		$sql = "INSERT INTO ".TB_PREF."budget_trans (tran_date,
		account, dimension_id, dimension2_id, amount, memo_) VALUES ('$date',
		".db_escape($account).", ".db_escape($dimension).", "
		.db_escape($dimension2).", ".db_escape($amount).", '')";

	db_query($sql, 'The GL budget transaction could not be saved');
}

function delete_gl_budget_trans($date_, $account, $dimension, $dimension2) {

	$sql = "DELETE FROM ".TB_PREF."budget_trans WHERE account=".db_escape($account)
	." AND dimension_id=".db_escape($dimension)
	." AND dimension2_id=".db_escape($dimension2)
	." AND tran_date='".date2sql($date_)."'";
	db_query($sql, 'The GL budget transaction could not be deleted');
}

function get_only_budget_trans_from_to($from_date, $to_date, $account, $dimension=0, $dimension2=0) {

	$sql = "SELECT SUM(amount) FROM ".TB_PREF."budget_trans
		WHERE account=".db_escape($account)
		." AND tran_date >= '".date2sql($from_date)."' AND tran_date <= '".date2sql($to_date)."'
		AND dimension_id = ".db_escape($dimension)
		." AND dimension2_id = ".db_escape($dimension2);
	$result = db_query($sql, 'No budget accounts were returned');

	$row = db_fetch_row($result);
	return $row[0];
}

/*
*	Stores journal/bank transaction tax details if applicable
*/
function add_gl_tax_details($gl_code, $trans_type, $trans_no, $amount, $ex_rate, $date, $memo, $included=0, $net_amount = null) {
	$tax_type = is_tax_account($gl_code);
	if(!$tax_type) return;	// $gl_code is not tax account
	
	$tax = get_tax_type($tax_type);
	if ($trans_type == ST_SALESINVOICE || $trans_type == ST_CUSTDELIVERY || $trans_type == ST_CUSTCREDIT)
		$amount = -$amount;
	// we have to restore net amount as we cannot know the base amount
	if ($net_amount===null) {
		if ($tax['rate'] == 0)
			$net_amount = 0;
		else // calculate net amount
			$net_amount = $amount/$tax['rate']*100;
	}
	add_trans_tax_details($trans_type, $trans_no, $tax['id'], $tax['rate'], $included, $amount, $net_amount, $ex_rate, $date, $memo, null);
}

/*
*	Store transaction tax details for fiscal purposes with 'freezed' 
*	actual tax type rate.
*/
function add_trans_tax_details($trans_type, $trans_no, $tax_id, $rate, $included, $amount, $net_amount, $ex_rate, $tran_date, $memo, $reg_type=null) {
	// guess tax register if not set
	if (!isset($reg_type))
		$reg_type = in_array($trans_type, array(ST_SUPPINVOICE, ST_SUPPCREDIT)) ? TR_OUTPUT : (in_array($trans_type, array(ST_SALESINVOICE, ST_CUSTCREDIT)) ? TR_INPUT : null);

	$sql = "INSERT INTO ".TB_PREF."trans_tax_details 
		(trans_type, trans_no, tran_date, tax_type_id, rate, ex_rate, included_in_price, net_amount, amount, memo, reg_type)
		VALUES (".db_escape($trans_type)."," . db_escape($trans_no).",'"
			.date2sql($tran_date)."',"
			.(int)($tax_id).","
			.(float)($rate).","
			.(float)($ex_rate).",".($included ? 1:0).","
			.db_escape($net_amount).","
			.db_escape($amount).",".db_escape($memo).",".db_escape($reg_type, true).")";

	db_query($sql, 'Cannot save trans tax details');
}

//----------------------------------------------------------------------------------------

function get_trans_tax_details($trans_type, $trans_no) {
	$sql = "SELECT tax_details.*, tax_type.name AS tax_type_name, tax_type.rate AS rate
		FROM ".TB_PREF."trans_tax_details tax_details INNER JOIN 
		".TB_PREF."tax_types tax_type ON tax_type.id = tax_details.tax_type_id
		WHERE 
			trans_type = ".db_escape($trans_type)."
			AND trans_no = ".db_escape($trans_no)."
			AND (net_amount != 0 OR amount != 0)";

	return db_query($sql, 'The transaction tax details could not be retrieved');
}

//----------------------------------------------------------------------------------------

function void_trans_tax_details($type, $type_no) {
	$sql = "UPDATE ".TB_PREF."trans_tax_details SET amount=0, net_amount = 0 WHERE trans_no=".db_escape($type_no)." AND trans_type=".db_escape($type);

	db_query($sql, 'The transaction tax details could not be voided');
}

//----------------------------------------------------------------------------------------

function clear_trans_tax_details($type, $type_no) {
	$sql = "DELETE FROM ".TB_PREF."trans_tax_details WHERE trans_no=".db_escape($type_no)." AND trans_type=".db_escape($type);

	db_query($sql, 'The transaction tax details could not be cleared');
}

function get_tax_summary($from, $to, $also_zero_purchases=false) {

	$sql = "SELECT 
		SUM(IF(trans_type=".ST_CUSTCREDIT.",-1,1)*
		IF((reg_type=".TR_OUTPUT.")"
			." || ((trans_type IN(".ST_SALESINVOICE.",".ST_CUSTCREDIT.",".ST_BANKDEPOSIT."))
			), net_amount*ex_rate,0)
		) net_output,

		SUM(IF(trans_type=".ST_CUSTCREDIT.",-1,1)*
		IF((reg_type=".TR_OUTPUT.")"
			." || ((trans_type IN(".ST_SALESINVOICE.",".ST_CUSTCREDIT.",".ST_BANKDEPOSIT."))
			), amount*ex_rate,0)) payable,

		SUM(IF(trans_type IN(".ST_SUPPCREDIT.",".ST_BANKPAYMENT."),-1,1)*
		IF((reg_type=".TR_INPUT." OR trans_type IN (".ST_SUPPINVOICE.",".ST_BANKPAYMENT."))"
			. ($also_zero_purchases ? '': " AND tax_type_id AND taxrec.rate")
			.", net_amount*ex_rate, 0)) net_input,

		SUM(IF(trans_type IN(".ST_SUPPCREDIT.",".ST_BANKPAYMENT."),-1,1)*
		IF((reg_type=".TR_INPUT." OR trans_type IN (".ST_SUPPINVOICE.",".ST_BANKPAYMENT."))"
			. ($also_zero_purchases ? '': " AND tax_type_id AND taxrec.rate ") 
			.", amount*ex_rate, 0)) collectible,
		taxrec.rate,
		ttype.id,
		ttype.name
		FROM ".TB_PREF."trans_tax_details taxrec LEFT JOIN ".TB_PREF."tax_types ttype ON taxrec.tax_type_id=ttype.id
		WHERE taxrec.trans_type IN (".implode(',',
			array(ST_SALESINVOICE, ST_CUSTCREDIT, ST_SUPPINVOICE, ST_SUPPCREDIT, ST_JOURNAL)).")
			AND taxrec.tran_date >= '".date2sql($from)."'
			AND taxrec.tran_date <= '".date2sql($to)."'
		GROUP BY ttype.id";

		// display_error($sql);
	return db_query($sql, 'Cannot retrieve tax summary');
}

//--------------------------------------------------------------------------------------------------

function exists_gl_trans($type, $trans_id) {
	$sql = "SELECT type_no FROM ".TB_PREF."gl_trans WHERE type=".db_escape($type)
		." AND type_no=".db_escape($trans_id);
	$result = db_query($sql, 'Cannot retreive a gl transaction');

	return (db_num_rows($result) > 0);
}

//--------------------------------------------------------------------------------------------------

function void_gl_trans($type, $trans_id, $nested=false) {
	if (!$nested)
		begin_transaction();

	$sql = "UPDATE ".TB_PREF."gl_trans SET amount=0 WHERE type=".db_escape($type)
	." AND type_no=".db_escape($trans_id);

	db_query($sql, "could not void gl transactions for type=$type and trans_no=$trans_id");

	if (!$nested)
		commit_transaction();
}

//----------------------------------------------------------------------------------------

function clear_gl_trans($type, $trans_id, $nested=false) {
	if (!$nested)
		begin_transaction();

	$sql = "DELETE FROM ".TB_PREF."gl_trans WHERE type=".db_escape($type)
	." AND type_no=".db_escape($trans_id);

	db_query($sql, "could not clear gl transactions for type=$type and trans_no=$trans_id");

	if (!$nested)
		commit_transaction();
}

function get_sql_for_journal_inquiry($filter, $from, $to, $ref='', $memo='', $alsoclosed=false, $user_id=null) {
	$sql = "SELECT  IFNULL(a.gl_seq,0) as gl_seq,
		gl.tran_date,
		gl.type as trans_type,
		gl.type_no as trans_no,
		IF(MAX(gl.person_id), MAX(gl.person_id), IFNULL(st.supplier_id, IFNULL(grn.supplier_id, IFNULL(dt.debtor_no, bt.person_id)))) as person_id,
		IF(ISNULL(st.supp_reference), '', st.supp_reference) AS supp_reference,
		refs.reference,
		IF(gl.type=".ST_BANKTRANSFER.",MAX(gl.amount),SUM(IF(gl.amount>0, gl.amount,0))) as amount,
		com.memo_,
		IF(ISNULL(u.user_id),'',u.user_id) as user_id,
		IF(MAX(gl.person_id), MAX(gl.person_type_id), IF(!ISNULL(st.supplier_id) OR !ISNULL(grn.supplier_id),".  PT_SUPPLIER . "," .  "IF(dt.debtor_no," . PT_CUSTOMER . "," .
		"IF(bt.person_id != '' AND !ISNULL(bt.person_id), bt.person_type_id, -1)))) as person_type_id
		FROM ".TB_PREF."gl_trans as gl
		LEFT JOIN ".TB_PREF."audit_trail as a ON
			(gl.type=a.type AND gl.type_no=a.trans_no)
		LEFT JOIN ".TB_PREF."comments as com ON
			(gl.type=com.type AND gl.type_no=com.id)
		LEFT JOIN ".TB_PREF."refs as refs ON
			(gl.type=refs.type AND gl.type_no=refs.id)
		LEFT JOIN ".TB_PREF."users as u ON
			a.user=u.id
		LEFT JOIN ".TB_PREF."grn_batch grn ON grn.id=gl.type_no AND gl.type=".ST_SUPPRECEIVE."
		LEFT JOIN ".TB_PREF."bank_trans bt ON bt.type=gl.type AND bt.trans_no=gl.type_no AND bt.amount!=0
			AND (bt.person_id != '' AND !ISNULL(bt.person_id))
		LEFT JOIN ".TB_PREF."debtor_trans dt ON dt.type=gl.type AND gl.type_no=dt.trans_no AND gl.person_id=dt.debtor_no AND gl.person_type_id=2
        LEFT JOIN ".TB_PREF."supp_trans st ON st.type=gl.type AND gl.type_no=st.trans_no AND gl.person_id=st.supplier_id AND gl.person_type_id=3
		WHERE gl.tran_date >= '".date2sql($from)."'
		AND gl.tran_date <= '".date2sql($to)."'
		AND gl.amount!=0";
	if ($ref)
		$sql .= " AND refs.reference LIKE ". db_escape("%$ref%");
	if ($memo)
		$sql .= " AND com.memo_ LIKE ". db_escape("%$memo%");
	if ($filter != -1)
		$sql .= " AND gl.type=".db_escape($filter);
	if (!$alsoclosed)
		$sql .= " AND gl_seq=0";
	else
		$sql .= " AND NOT ISNULL(a.gl_seq)";

	if ($user_id != null)
		$sql .= " AND user_id = ".db_escape($user_id);

	$sql .= " GROUP BY gl.tran_date, a.gl_seq, gl.type, gl.type_no";
	return $sql;
}
