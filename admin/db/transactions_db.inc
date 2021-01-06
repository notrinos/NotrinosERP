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

//--------------------------------------------------------------------------------------

function get_sql_for_view_transactions($filtertype, $from, $to, &$trans_ref) {
	$db_info = get_systype_db_info($filtertype);

	if ($db_info == null)
		return '';

	$table_name = $db_info[0];
	$type_name = $db_info[1];
	$trans_no_name = $db_info[2];
	$trans_ref = $db_info[3];
	$trans_date = $db_info[4];

	$sql = "SELECT t.$trans_no_name as trans_no";

	if ($trans_ref)
		$sql .= " ,t.$trans_ref as ref ";
	else
		$sql .= ", r.reference as ref";
	if ($trans_date)
		$sql .= ",t.$trans_date as trans_date";
	if ($type_name)
		$sql .= ", t.$type_name as type";
	$sql .= " FROM $table_name t LEFT JOIN ".TB_PREF."voided v ON"." t.$trans_no_name=v.id AND v.type=".db_escape($filtertype);

	$sql .= " WHERE ISNULL(v.`memo_`)";
	if ($from != null && $to != null) {
		$sql .= " AND t.$trans_no_name >= ".db_escape($from). "
			AND  t.$trans_no_name <= ".db_escape($to);
		if ($type_name != null)
			$sql .= " AND t.`$type_name` = ".db_escape($filtertype);
	}
	elseif ($type_name != null)
		$sql .= " AND t.`$type_name` = ".db_escape($filtertype);

	// the ugly hack below is necessary to exclude old gl_trans records lasting after edition,
	// otherwise old data transaction can be retrieved instead of current one.
	if ($table_name==TB_PREF.'gl_trans')
		$sql .= " AND t.`amount` <> 0";

	$sql .= " GROUP BY ".($type_name ? "t.$type_name," : '')." t.$trans_no_name";
	$sql .= " ORDER BY t.$trans_no_name DESC";
	return $sql;
}

function transaction_exists($trans_type, $trans_no) {
	return db_num_rows(db_query(get_sql_for_view_transactions($trans_type, $trans_no, $trans_no, $dummy)));
}

//
//	Returns counterparty (supplier/customer) name for selected transaction.
//
function get_counterparty_name($trans_type, $trans_no, $full=true) {
	switch($trans_type) {
		case ST_SALESORDER:
		case ST_SALESQUOTE:
			$sql = "SELECT order.customer_id as person_id, debtor.name as name FROM ".TB_PREF."sales_orders order, ".TB_PREF."debtors_master debtor WHERE order_no=".db_escape($trans_no)." AND trans_type=".db_escape($trans_type)." AND order.debtor_no=debtor.debtor_no";
			break;

		case ST_SALESINVOICE :
		case ST_CUSTCREDIT :
		case ST_CUSTPAYMENT :
		case ST_CUSTDELIVERY :
			$sql = "SELECT trans.debtor_no as person_id, debtor.name as name FROM ".TB_PREF."debtor_trans trans, ".TB_PREF."debtors_master debtor WHERE trans_no=".db_escape($trans_no)." AND type=".db_escape($trans_type)." AND trans.debtor_no=debtor.debtor_no";
			break;

		case ST_PURCHORDER :
			$sql = "SELECT order.supplier_id as person_id, supp.supp_name as name FROM ".TB_PREF."purch_orders order, ".TB_PREF."suppliers supp WHERE order_no=".db_escape($trans_no)." AND order.supplier_id=supp.supplier_id";
			break;

		case ST_SUPPINVOICE :
		case ST_SUPPCREDIT :
		case ST_SUPPAYMENT :
			$sql = "SELECT trans.supplier_id as person_id, supp.supp_name as name
			FROM ".TB_PREF."supp_trans trans, ".TB_PREF."suppliers supp
			WHERE trans_no=".db_escape($trans_no)." AND type=".db_escape($trans_type)
			." AND trans.supplier_id=supp.supplier_id";
			break;

		case ST_SUPPRECEIVE :
			$sql = "SELECT trans.supplier_id as person_id, supp.supp_name as name
			FROM ".TB_PREF."grn_batch trans, ".TB_PREF."suppliers supp
			WHERE id=".db_escape($trans_no)
			." AND trans.supplier_id=supp.supplier_id";
			break;

		case ST_BANKPAYMENT :
		case ST_BANKDEPOSIT :
			$sql = "SELECT trans.debtor_no as person_id, debtor.name as name
			FROM ".TB_PREF."debtor_trans trans, ".TB_PREF."debtors_master debtor
			WHERE trans_no=".db_escape($trans_no)." AND type=".db_escape($trans_type)
			." AND trans.debtor_no=debtor.debtor_no
			UNION
				SELECT trans.supplier_id as person_id, supp.supp_name as name
			FROM ".TB_PREF."supp_trans trans, ".TB_PREF."suppliers supp
			WHERE trans_no=".db_escape($trans_no)." AND type=".db_escape($trans_type)
			." AND trans.supplier_id=supp.supplier_id";
			break;

		case ST_JOURNAL:	// FIXME - this one can have multiply counterparties of various types depending on person_type_id

		default: 
		/*	// internal operations
		case ST_WORKORDER :
		case ST_INVADJUST : // GRN/DN returns ?
		case ST_BANKTRANSFER :
		case ST_LOCTRANSFER :
		case ST_MANUISSUE :
		case ST_MANURECEIVE :
		case ST_COSTUPDATE :
		*/
			return null;
	}

	$result = db_query($sql, 'cannot retrieve counterparty name');
	if (db_num_rows($result)) {
		$row = db_fetch($result);
		return sprintf("[%05s] %s", $row['person_id'], $row['name']);
	}

	return '';
}


//-----------------------------------------------------------------------------------------
//	Returns next transaction number.
//	Used only for transactions stored in tables without autoincremented key.
//

function get_next_trans_no ($trans_type) {

	$st = get_systype_db_info($trans_type);

	if (!($st && $st[0] && $st[2])) {
		// this is in fact internal error condition.
		display_error('Internal error: invalid type passed to get_next_trans_no()');
		return 0;
	}
	$sql1 = "SELECT MAX(`$st[2]`) as last_no FROM $st[0]";
	if ($st[1] != null)
		$sql1 .= " WHERE `$st[1]`=".db_escape($trans_type);

	// check also in voided transactions (some transactions like location transfer are removed completely)
	$sql2 = "SELECT MAX(`id`) as last_no FROM ".TB_PREF."voided WHERE `type`=".db_escape($trans_type);

	$sql = "SELECT max(last_no) last_no FROM ($sql1 UNION $sql2) a";
	$result = db_query($sql, "The next transaction number for $trans_type could not be retrieved");
	$myrow = db_fetch_row($result);

	return $myrow[0] + 1;
}

//-----------------------------------------------------------------------------

function get_systype_db_info($type) {
	switch ($type) {
		case    ST_JOURNAL      : return array(TB_PREF."journal", "type", "trans_no", "reference", "tran_date");
		case    ST_BANKPAYMENT  : return array(TB_PREF."bank_trans", "type", "trans_no", "ref", "trans_date");
		case    ST_BANKDEPOSIT  : return array(TB_PREF."bank_trans", "type", "trans_no", "ref", "trans_date");
		case    3               : return null;
		case    ST_BANKTRANSFER : return array(TB_PREF."bank_trans", "type", "trans_no", "ref", "trans_date");
		case    ST_SALESINVOICE : return array(TB_PREF."debtor_trans", "type", "trans_no", "reference", "tran_date");
		case    ST_CUSTCREDIT   : return array(TB_PREF."debtor_trans", "type", "trans_no", "reference", "tran_date");
		case    ST_CUSTPAYMENT  : return array(TB_PREF."debtor_trans", "type", "trans_no", "reference", "tran_date");
		case    ST_CUSTDELIVERY : return array(TB_PREF."debtor_trans", "type", "trans_no", "reference", "tran_date");
		case    ST_LOCTRANSFER  : return array(TB_PREF."stock_moves", "type", "trans_no", "reference", "tran_date");
		case    ST_INVADJUST    : return array(TB_PREF."stock_moves", "type", "trans_no", "reference", "tran_date");
		case    ST_PURCHORDER   : return array(TB_PREF."purch_orders", null, "order_no", "reference", "ord_date");
		case    ST_SUPPINVOICE  : return array(TB_PREF."supp_trans", "type", "trans_no", "reference", "tran_date");
		case    ST_SUPPCREDIT   : return array(TB_PREF."supp_trans", "type", "trans_no", "reference", "tran_date");
		case    ST_SUPPAYMENT   : return array(TB_PREF."supp_trans", "type", "trans_no", "reference", "tran_date");
		case    ST_SUPPRECEIVE  : return array(TB_PREF."grn_batch", null, "id", "reference", "delivery_date");
		case    ST_WORKORDER    : return array(TB_PREF."workorders", null, "id", "wo_ref", "released_date");
		case    ST_MANUISSUE    : return array(TB_PREF."wo_issues", null, "issue_no", "reference", "issue_date");
		case    ST_MANURECEIVE  : return array(TB_PREF."wo_manufacture", null, "id", "reference", "date_");
		case    ST_SALESORDER   : return array(TB_PREF."sales_orders", "trans_type", "order_no", "reference", "ord_date");
		case    31              : return array(TB_PREF."service_orders", null, "order_no", "cust_ref", "date");
		case    ST_SALESQUOTE   : return array(TB_PREF."sales_orders", "trans_type", "order_no", "reference", "ord_date");
		case    ST_DIMENSION    : return array(TB_PREF."dimensions", null, "id", "reference", "date_");
		case    ST_COSTUPDATE   : return array(TB_PREF."journal", "type", "trans_no", "reference", "tran_date");

		case    ST_CUSTOMER    : return array(TB_PREF."debtors_master", null, "debtor_no", "debtor_ref", null);
		case    ST_SUPPLIER    : return array(TB_PREF."suppliers", null, "supplier_id", "supp_ref", null);
	}

	display_db_error("invalid type ($type) sent to get_systype_db_info", "", true);
}