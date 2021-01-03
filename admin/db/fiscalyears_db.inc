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
require_once($path_to_root. "/includes/ui/items_cart.inc");

// fiscal year routines
function add_fiscalyear($from_date, $to_date, $closed)
{
	$from = date2sql($from_date);
	$to = date2sql($to_date);

	$sql = "INSERT INTO ".TB_PREF."fiscal_year (begin, end, closed)
		VALUES (".db_escape($from).",".db_escape($to).", ".db_escape($closed).")";

	db_query($sql, "could not add fiscal year");
}

function update_fiscalyear($id, $closed)
{
	$sql = "UPDATE ".TB_PREF."fiscal_year SET closed=".db_escape($closed)."
		WHERE id=".db_escape($id);

	db_query($sql, "could not update fiscal year");
}

function get_all_fiscalyears()
{
	$sql = "SELECT * FROM ".TB_PREF."fiscal_year ORDER BY begin";

	return db_query($sql, "could not get all fiscal years");
}

function get_fiscalyear($id)
{
	$sql = "SELECT * FROM ".TB_PREF."fiscal_year WHERE id=".db_escape($id);

	$result = db_query($sql, "could not get fiscal year");

	return db_fetch($result);
}

function get_current_fiscalyear()
{
	$year = get_company_pref('f_year');

	$sql = "SELECT * FROM ".TB_PREF."fiscal_year WHERE id=".db_escape($year);

	$result = db_query($sql, "could not get current fiscal year");

	return db_fetch($result);
}


function delete_fiscalyear($id)
{
	begin_transaction();

	$sql="DELETE FROM ".TB_PREF."fiscal_year WHERE id=".db_escape($id);

	db_query($sql, "could not delete fiscal year");

	commit_transaction();
}

function is_date_in_fiscalyears($date, $closed=true)
{
	$date = date2sql($date);
	$sql = "SELECT * FROM ".TB_PREF."fiscal_year WHERE '$date' >= begin AND '$date' <= end";
	if (!$closed)
		$sql .= " AND closed=0";
	$result = db_query($sql, "could not get all fiscal years");
	return db_fetch($result) !== false;
}

function get_fiscalyear_begin_for_date($date)
{
 	$date = date2sql($date);
	$sql = "SELECT begin FROM ".TB_PREF."fiscal_year WHERE '$date' >= begin AND '$date' <= end";
	$result = db_query($sql, "could not get begin date of the fiscal year");
	$row = db_fetch_row($result);
	if ($row != false)
		return sql2date($row[0]);
	else
		return begin_fiscalyear();
}

function check_begin_end_date($date1, $date2)
{
	$sql = "SELECT MAX(end), MIN(begin) FROM ".TB_PREF."fiscal_year";
	$result = db_query($sql, "could not retrieve last fiscal years");
	$row = db_fetch_row($result);
	if ($row[0] === null)
		return true;
	$max = add_days(sql2date($row[0]), 1);
	$min = add_days(sql2date($row[1]), -1);
	return ($max === $date1 || $min === $date2);
}

function next_begin_date()
{
	$sql = "SELECT MAX(end) FROM ".TB_PREF."fiscal_year";
	$result = db_query($sql, "could not retrieve last fiscal years");
	$row = db_fetch_row($result);
	if ($row[0] === null)
		return false;
	return add_days(sql2date($row[0]), 1);
}

function check_years_before($date, $closed=false)
{
	$date = date2sql($date);
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."fiscal_year WHERE begin < '$date'";
	if (!$closed)
		$sql .= " AND closed=0";

	$result = db_query($sql, "could not check fiscal years before");
	$row = db_fetch_row($result);
	return ($row[0] > 0);
}

//---------------------------------------------------------------------------------------------
function close_year($year)
{
	global $Refs;

	$co = get_company_prefs();
	if (get_gl_account($co['retained_earnings_act']) == false || get_gl_account($co['profit_loss_year_act']) == false)
	{
		display_error(_("The Retained Earnings Account or the Profit and Loss Year Account has not been set in System and General GL Setup"));
		return false;
	}
	if (!is_account_balancesheet($co['retained_earnings_act']) || is_account_balancesheet($co['profit_loss_year_act']))
	{
		display_error(_("The Retained Earnings Account should be a Balance Account or the Profit and Loss Year Account should be an Expense Account (preferred the last one in the Expense Class)"));
		return false;
	}

	begin_transaction();

	$myrow = get_fiscalyear($year);
	$to = $myrow['end'];
	// retrieve total balances from balance sheet accounts
    $sql = "SELECT SUM(amount) FROM ".TB_PREF."gl_trans INNER JOIN ".TB_PREF."chart_master ON account=account_code
    	INNER JOIN ".TB_PREF."chart_types ON account_type=id INNER JOIN ".TB_PREF."chart_class ON class_id=cid 
		WHERE ctype>=".CL_ASSETS." AND ctype <=".CL_EQUITY." AND tran_date <= '$to'";
	$result = db_query($sql, "The total balance could not be calculated");

	$row = db_fetch_row($result);
	$balance = round2($row[0], user_price_dec());

	$to = sql2date($to);

	if ($balance != 0.0)
	{
		$cart = new items_cart(ST_JOURNAL);
		$cart->reference = $Refs->get_next(ST_JOURNAL, null, $to);
		$cart->tran_date = $cart->doc_date = $cart->event_date = $to;

		$cart->add_gl_item($co['retained_earnings_act'], 0, 0, -$balance, _("Closing Year"), '', 0);
		$cart->add_gl_item($co['profit_loss_year_act'], 0, 0, $balance, _("Closing Year"), '', 0);
		$cart->memo_ = _("Closing Year");
		write_journal_entries($cart);
	}
	close_transactions($to);

	commit_transaction();
	return true;
}

function open_year($year)
{
	$myrow = get_fiscalyear($year);
	$from = sql2date($myrow['begin']);

	begin_transaction();
	close_transactions(add_days($from, -1));
	commit_transaction();
}

//---------------------------------------------------------------------------------------------
function delete_attachments_and_comments($type_no, $trans_no)
{
	
	$sql = "SELECT * FROM ".TB_PREF."attachments WHERE type_no = $type_no AND trans_no = $trans_no";
	$result = db_query($sql, "Could not retrieve attachments");
	$delflag = false;
	while ($row = db_fetch($result))
	{
		$delflag = true;
		$dir =  company_path(). "/attachments";
		if (file_exists($dir."/".$row['unique_name']))
			unlink($dir."/".$row['unique_name']);
	}
	if ($delflag)
	{
		$sql = "DELETE FROM ".TB_PREF."attachments WHERE  type_no = $type_no AND trans_no = $trans_no";
		db_query($sql, "Could not delete attachment");
	}	
	$sql = "DELETE FROM ".TB_PREF."comments WHERE  type = $type_no AND id = $trans_no";
	db_query($sql, "Could not delete comments");
	$sql = "DELETE FROM ".TB_PREF."refs WHERE  type = $type_no AND id = $trans_no";
	db_query($sql, "Could not delete refs");
}	

//---------------------------------------------------------------------------------------------
function delete_this_fiscalyear($selected_id)
{
	global $db_connections;
	
	db_backup($db_connections[user_company()], 'Security backup before Fiscal Year Removal');
	begin_transaction();
	$ref = _("Open Balance");
	$myrow = get_fiscalyear($selected_id);
	$to = $myrow['end'];
	$sql = "SELECT order_no, trans_type FROM ".TB_PREF."sales_orders WHERE ord_date <= '$to' AND type <> 1"; // don't take the templates
	$result = db_query($sql, "Could not retrieve sales orders");
	while ($row = db_fetch($result))
	{
		$sql = "SELECT SUM(qty_sent), SUM(quantity) FROM ".TB_PREF."sales_order_details WHERE order_no = {$row['order_no']} AND trans_type = {$row['trans_type']}";
		$res = db_query($sql, "Could not retrieve sales order details");
		$row2 = db_fetch_row($res);
		if ($row2[0] == $row2[1])
		{
			$sql = "DELETE FROM ".TB_PREF."sales_order_details WHERE order_no = {$row['order_no']} AND trans_type = {$row['trans_type']}";
			db_query($sql, "Could not delete sales order details");
			$sql = "DELETE FROM ".TB_PREF."sales_orders WHERE order_no = {$row['order_no']} AND trans_type = {$row['trans_type']}";
			db_query($sql, "Could not delete sales order");
			delete_attachments_and_comments($row['trans_type'], $row['order_no']);
		}
	}
	$sql = "SELECT order_no FROM ".TB_PREF."purch_orders WHERE ord_date <= '$to'";
	$result = db_query($sql, "Could not retrieve purchase orders");
	while ($row = db_fetch($result))
	{
		$sql = "SELECT SUM(quantity_ordered), SUM(quantity_received) FROM ".TB_PREF."purch_order_details WHERE order_no = {$row['order_no']}";
		$res = db_query($sql, "Could not retrieve purchase order details");
		$row2 = db_fetch_row($res);
		if ($row2[0] == $row2[1])
		{
			$sql = "DELETE FROM ".TB_PREF."purch_order_details WHERE order_no = {$row['order_no']}";
			db_query($sql, "Could not delete purchase order details");
			$sql = "DELETE FROM ".TB_PREF."purch_orders WHERE order_no = {$row['order_no']}";
			db_query($sql, "Could not delete purchase order");
			delete_attachments_and_comments(ST_PURCHORDER, $row['order_no']);
		}
	}
	$sql = "SELECT id FROM ".TB_PREF."grn_batch WHERE delivery_date <= '$to'";
	$result = db_query($sql, "Could not retrieve grn batch");
	while ($row = db_fetch($result))
	{
		$sql = "DELETE FROM ".TB_PREF."grn_items WHERE grn_batch_id = {$row['id']}";
		db_query($sql, "Could not delete grn items");
		$sql = "DELETE FROM ".TB_PREF."grn_batch WHERE id = {$row['id']}";
		db_query($sql, "Could not delete grn batch");
		delete_attachments_and_comments(25, $row['id']);
	}
	$sql = "SELECT trans_no, type FROM ".TB_PREF."debtor_trans WHERE tran_date <= '$to' AND 
		(ov_amount + ov_gst + ov_freight + ov_freight_tax + ov_discount) = 
		(SELECT SUM(amt) FROM ".TB_PREF."cust_allocations WHERE date_alloc <= '$to' AND trans_no = trans_no_to AND
		type = trans_type_to)";
	$result = db_query($sql, "Could not retrieve debtor trans");
	while ($row = db_fetch($result))
	{
		if ($row['type'] == ST_SALESINVOICE)
		{
			$deliveries = get_sales_parent_numbers($row['type'], $row['trans_no']);
			foreach ($deliveries as $delivery)
			{
				$sql = "DELETE FROM ".TB_PREF."debtor_trans_details WHERE debtor_trans_no = $delivery AND debtor_trans_type = ".ST_CUSTDELIVERY;
				db_query($sql, "Could not delete debtor trans details");
				$sql = "DELETE FROM ".TB_PREF."debtor_trans WHERE trans_no = $delivery AND type = ".ST_CUSTDELIVERY;
				db_query($sql, "Could not delete debtor trans");
				delete_attachments_and_comments(ST_CUSTDELIVERY, $delivery);
			}		
		}	
		$sql = "DELETE FROM ".TB_PREF."cust_allocations WHERE trans_no_from = {$row['trans_no']} AND trans_type_from = {$row['type']}";
		db_query($sql, "Could not delete cust allocations");
		$sql = "DELETE FROM ".TB_PREF."debtor_trans_details WHERE debtor_trans_no = {$row['trans_no']} AND debtor_trans_type = {$row['type']}";
		db_query($sql, "Could not delete debtor trans details");
		$sql = "DELETE FROM ".TB_PREF."debtor_trans WHERE trans_no = {$row['trans_no']} AND type = {$row['type']}";
		db_query($sql, "Could not delete debtor trans");
		delete_attachments_and_comments($row['type'], $row['trans_no']);
	}
	$sql = "SELECT trans_no, type FROM ".TB_PREF."supp_trans WHERE tran_date <= '$to' AND 
		ABS(ov_amount + ov_gst + ov_discount) = (SELECT ABS(SUM(amt)) FROM ".TB_PREF."supp_allocations WHERE date_alloc <= '$to' AND trans_no = trans_no_to AND type = trans_type_to)";
	$result = db_query($sql, "Could not retrieve supp trans");
	while ($row = db_fetch($result))
	{
		$sql = "DELETE FROM ".TB_PREF."supp_allocations WHERE trans_no_from = {$row['trans_no']} AND trans_type_from = {$row['type']}";
		db_query($sql, "Could not delete supp allocations");
		$sql = "DELETE FROM ".TB_PREF."supp_invoice_items WHERE supp_trans_no = {$row['trans_no']} AND supp_trans_type = {$row['type']}";
		db_query($sql, "Could not delete supp invoice items");
		$sql = "DELETE FROM ".TB_PREF."supp_trans WHERE trans_no = {$row['trans_no']} AND type = {$row['type']}";
		db_query($sql, "Could not delete supp trans");
		delete_attachments_and_comments($row['type'], $row['trans_no']);
	}
	$sql = "SELECT id FROM ".TB_PREF."workorders WHERE released_date <= '$to' AND closed=1";
	$result = db_query($sql, "Could not retrieve workorders");
	while ($row = db_fetch($result))
	{
		$sql = "SELECT issue_no FROM ".TB_PREF."wo_issues WHERE workorder_id = {$row['id']}"; 
		$res = db_query($sql, "Could not retrieve wo issues");
        while ($row2 = db_fetch_row($res))
        {
            $sql = "DELETE FROM ".TB_PREF."wo_issue_items WHERE issue_id = {$row2[0]}";
            db_query($sql, "Could not delete wo issue items");
            $sql = "DELETE FROM ".TB_PREF."wo_issues WHERE workorder_id = {$row2[0]}";
            db_query($sql, "Could not delete wo issues");
            delete_attachments_and_comments(ST_MANUISSUE, $row2[0]);
        }		
		$sql = "DELETE FROM ".TB_PREF."wo_manufacture WHERE workorder_id = {$row['id']}";
		db_query($sql, "Could not delete wo manufacture");
		$sql = "DELETE FROM ".TB_PREF."wo_requirements WHERE workorder_id = {$row['id']}";
		db_query($sql, "Could not delete wo requirements");
		$sql = "DELETE FROM ".TB_PREF."workorders WHERE id = {$row['id']}";
		db_query($sql, "Could not delete workorders");
		delete_attachments_and_comments(ST_WORKORDER, $row['id']);
	}
	$sql = "SELECT loc_code, stock_id, SUM(qty) AS qty,"
	    ." SUM(-qty*IF(type=".ST_SUPPRECEIVE." OR type=".ST_SUPPCREDIT.", price, standard_cost)) AS std_cost"
	    ." FROM ".TB_PREF."stock_moves WHERE tran_date <= '$to' GROUP by loc_code, stock_id";
	$result = db_query($sql, "Could not retrieve stock moves");
	while ($row = db_fetch($result))
	{
		$sql = "DELETE FROM ".TB_PREF."stock_moves WHERE tran_date <= '$to' AND loc_code = '{$row['loc_code']}' AND stock_id = '{$row['stock_id']}'";
		db_query($sql, "Could not delete stock moves");
		$qty = $row['qty'];
		$std_cost = ($qty == 0 ? 0 : round2($row['std_cost'] / $qty, user_price_dec()));
		$sql = "INSERT INTO ".TB_PREF."stock_moves (stock_id, loc_code, tran_date, reference, qty, standard_cost) VALUES
			('{$row['stock_id']}', '{$row['loc_code']}', '$to', '$ref', $qty, $std_cost)";   
		db_query($sql, "Could not insert stock move");
	}		
	$sql = "DELETE FROM ".TB_PREF."voided WHERE date_ <= '$to'";
	db_query($sql, "Could not delete voided items");
	$sql = "DELETE FROM ".TB_PREF."trans_tax_details WHERE tran_date <= '$to'";
	db_query($sql, "Could not delete trans tax details");
	$sql = "DELETE FROM ".TB_PREF."exchange_rates WHERE date_ <= '$to'";
	db_query($sql, "Could not delete exchange rates");
	$sql = "DELETE FROM ".TB_PREF."budget_trans WHERE tran_date <= '$to'";
	db_query($sql, "Could not delete budget trans");
	$sql = "SELECT account, SUM(amount) AS amount, person_type_id, person_id FROM "
		.TB_PREF."gl_trans WHERE tran_date <= '$to' GROUP by account, person_type_id, person_id";
	$result = db_query($sql, "Could not retrieve gl trans");
	$trans_no = get_next_trans_no(ST_JOURNAL);

	$last_account='';
	$new = false;
	$total = 0.0;
	while ($row = db_fetch($result))
	{
		if ($last_account != $row['account']) // deletes all subledgers postings, so do it once for account
		{
			$sql = "DELETE FROM ".TB_PREF."gl_trans WHERE tran_date <= '$to' AND account = '{$row['account']}'";
			db_query($sql, "Could not delete gl trans");
			$last_account = $row['account'];
		}
		if (is_account_balancesheet($row['account']) && $row['amount'])
		{
			$sql = "INSERT INTO ".TB_PREF."gl_trans (type, type_no, tran_date, account, memo_, amount, person_type_id, person_id) VALUES
				(".ST_JOURNAL.", $trans_no, '$to', '{$row['account']}', '$ref', {$row['amount']}, "
				.db_escape($row['person_type_id'], true).", ".db_escape($row['person_id'], true).")";
			db_query($sql, "Could not insert gl trans");
			if ($row['amount'] > 0.0)
				$total += $row['amount'];
			$new = true;
		}
	}
	if ($new)
	{
		global $Refs;
		$trans_type = ST_JOURNAL;
		$date_ = sql2date($to);
		$reference = $Refs->get_next($trans_type, null, $date_);
		$Refs->save($trans_type, $trans_no, $reference);
		add_journal($trans_type, $trans_no, $total, $date_, get_company_currency(), $reference);
		$Refs->save($trans_type, $trans_no, $reference);
		add_audit_trail($trans_type, $trans_no, $date_);
	}
	$sql = "SELECT bank_act, SUM(amount) AS amount FROM ".TB_PREF."bank_trans WHERE trans_date <= '$to' GROUP BY bank_act";
	$result = db_query($sql, "Could not retrieve bank trans");
	while ($row = db_fetch($result))
	{
		$sql = "DELETE FROM ".TB_PREF."bank_trans WHERE trans_date <= '$to' AND bank_act = '{$row['bank_act']}'";
		db_query($sql, "Could not delete bank trans");
		$sql = "INSERT INTO ".TB_PREF."bank_trans (type, trans_no, trans_date, bank_act, ref, amount) VALUES
			(0, 0, '$to', '{$row['bank_act']}', '$ref', {$row['amount']})";
		db_query($sql, "Could not insert bank trans");
	}	
	
	$sql = "DELETE FROM ".TB_PREF."audit_trail WHERE gl_date <= '$to'";
	db_query($sql, "Could not delete audit trail");
	
	$sql = "SELECT type, id FROM ".TB_PREF."comments WHERE type != ".ST_SALESQUOTE." AND type != ".ST_SALESORDER." AND type != ".ST_PURCHORDER;
	$result = db_query($sql, "Could not retrieve comments");
	while ($row = db_fetch($result))
	{
		$sql = "SELECT count(*) FROM ".TB_PREF."gl_trans WHERE type = {$row['type']} AND type_no = {$row['id']}";
		$res = db_query($sql, "Could not retrieve gl_trans");
		$row2 = db_fetch_row($res);
		if ($row2[0] == 0) // if no link, then delete comments
		{
			$sql = "DELETE FROM ".TB_PREF."comments WHERE type = {$row['type']} AND id = {$row['id']}";
			db_query($sql, "Could not delete comments");
		}
	}	
	$sql = "SELECT type, id FROM ".TB_PREF."refs WHERE type != ".ST_SALESQUOTE." AND type != ".ST_SALESORDER." AND type != ".ST_PURCHORDER;
	$result = db_query($sql, "Could not retrieve refs");
	while ($row = db_fetch($result))
	{
		$sql = "SELECT count(*) FROM ".TB_PREF."gl_trans WHERE type = {$row['type']} AND type_no = {$row['id']}";
		$res = db_query($sql, "Could not retrieve gl_trans");
		$row2 = db_fetch_row($res);
		if ($row2[0] == 0) // if no link, then delete refs
		{
			$sql = "DELETE FROM ".TB_PREF."refs WHERE type = {$row['type']} AND id = {$row['id']}";
			db_query($sql, "Could not delete refs");
		}
	}	
		
	delete_fiscalyear($selected_id);
	commit_transaction();	
}


