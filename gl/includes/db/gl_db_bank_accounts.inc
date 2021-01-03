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
//---------------------------------------------------------------------------------------------
function clear_dflt_curr_account($curr_code) {
	  $sql = "UPDATE ".TB_PREF."bank_accounts SET dflt_curr_act=0 WHERE bank_curr_code="
	  .db_escape($curr_code);
	  db_query($sql, "could not update default currency account");
}

function add_bank_account($account_code, $account_type, $bank_account_name,
	$bank_name, $bank_account_number, $bank_address, $bank_curr_code, 
	$dflt_curr_act, $bank_charge_act)
{
	if($dflt_curr_act) 	// only one default account for any currency
	  clear_dflt_curr_account($bank_curr_code);

	$sql = "INSERT INTO ".TB_PREF."bank_accounts (account_code, account_type, 
		bank_account_name, bank_name, bank_account_number, bank_address, 
		bank_curr_code, dflt_curr_act, bank_charge_act)
		VALUES (".db_escape($account_code).", ".db_escape($account_type).", "
		.db_escape($bank_account_name).", ".db_escape($bank_name).", "
		.db_escape($bank_account_number).",".db_escape($bank_address).
		", ".db_escape($bank_curr_code).", ".db_escape($dflt_curr_act).", ".db_escape($bank_charge_act).")";

	db_query($sql, "could not add a bank account for $account_code");
}

//---------------------------------------------------------------------------------------------

function update_bank_account($id, $account_code, $account_type, $bank_account_name, 
	$bank_name, $bank_account_number, $bank_address, $bank_curr_code, $dflt_curr_act, $bank_charge_act)
{
	if($dflt_curr_act) 	// only one default account for any currency
	  clear_dflt_curr_account($bank_curr_code);

	$sql = "UPDATE ".TB_PREF."bank_accounts	SET account_type = ".db_escape($account_type).",
		account_code=".db_escape($account_code).",
		bank_account_name=".db_escape($bank_account_name).", bank_name=".db_escape($bank_name).",
		bank_account_number=".db_escape($bank_account_number).", bank_curr_code=".db_escape($bank_curr_code).",
		bank_address=".db_escape($bank_address).",
		dflt_curr_act=".db_escape($dflt_curr_act).", bank_charge_act=".db_escape($bank_charge_act)." 
		WHERE id = ".db_escape($id);

	db_query($sql, "could not update bank account for $account_code");
}

//---------------------------------------------------------------------------------------------

function delete_bank_account($id)
{
	$sql = "DELETE FROM ".TB_PREF."bank_accounts WHERE id=".db_escape($id);

	db_query($sql,"could not delete bank account for $id");
}


//---------------------------------------------------------------------------------------------

function get_bank_account($id)
{
	$sql = "SELECT * FROM ".TB_PREF."bank_accounts WHERE id=".db_escape($id);

	$result = db_query($sql, "could not retreive bank account for $id");

	return db_fetch($result);
}

//---------------------------------------------------------------------------------------------

function get_bank_accounts($show_inactive=false)
{
	$sql = "SELECT account.*, gl_account.account_name
		FROM ".TB_PREF."bank_accounts account, ".TB_PREF."chart_master gl_account
		WHERE account.account_code = gl_account.account_code";
	if (!$show_inactive) $sql .= " AND !account.inactive";
	$sql .= " ORDER BY account_code, bank_curr_code";

	return db_query($sql,"could not get bank accounts");
}

//---------------------------------------------------------------------------------------------

function get_bank_gl_account($id)
{
	$sql = "SELECT account_code FROM ".TB_PREF."bank_accounts WHERE id=".db_escape($id);

	$result = db_query($sql, "could not retreive bank account for $id");

	$bank_account = db_fetch($result);

	return $bank_account['account_code'];
}

//---------------------------------------------------------------------------------------------

function get_bank_charge_account($id)
{
	$sql = "SELECT bank_charge_act FROM ".TB_PREF."bank_accounts WHERE id=".db_escape($id);

	$result = db_query($sql, "could not retreive bank account for $id");

	$bank_account = db_fetch($result);

	return $bank_account['bank_charge_act'];
}

//---------------------------------------------------------------------------------------------

function add_quick_entry($description, $type, $base_amount, $base_desc, $bal_type, $usage)
{
	$sql = "INSERT INTO ".TB_PREF."quick_entries (description, type, base_amount, base_desc, bal_type, `usage`) 
	VALUES (".db_escape($description).", ".db_escape($type).", "
		.db_escape($base_amount).", ".db_escape($base_desc).", ".db_escape($bal_type).", ".db_escape($usage).")";

	db_query($sql, "could not insert quick entry for $description");
}

//---------------------------------------------------------------------------------------------

function update_quick_entry($selected_id, $description, $type, $base_amount, $base_desc, $bal_type, $usage)
{
	$sql = "UPDATE ".TB_PREF."quick_entries	SET description = ".db_escape($description).",
		type=".db_escape($type).", base_amount=".db_escape($base_amount)
		.", base_desc=".db_escape($base_desc).", bal_type=".db_escape($bal_type).", `usage`=".db_escape($usage)."
		WHERE id = ".db_escape($selected_id);

	db_query($sql, "could not update quick entry for $selected_id");
}

//---------------------------------------------------------------------------------------------

function delete_quick_entry($selected_id)
{
	$sql = "DELETE FROM ".TB_PREF."quick_entries WHERE id=".db_escape($selected_id);

	db_query($sql,"could not delete quick entry $selected_id");
}

//---------------------------------------------------------------------------------------------

function add_quick_entry_line($qid, $action, $dest_id, $amount, $dim, $dim2, $memo)
{
	$sql = "INSERT INTO ".TB_PREF."quick_entry_lines 
		(qid, action, dest_id, amount, dimension_id, dimension2_id, memo) 
	VALUES 
		(".db_escape($qid).", ".db_escape($action).",".db_escape($dest_id).",
			".db_escape($amount).", ".db_escape($dim).", ".db_escape($dim2).", ".db_escape($memo).")";

	db_query($sql, "could not insert quick entry line for $qid");
}

//---------------------------------------------------------------------------------------------

function update_quick_entry_line($selected_id, $qid, $action, $dest_id, $amount, $dim, $dim2, $memo)
{
	$sql = "UPDATE ".TB_PREF."quick_entry_lines SET qid = ".db_escape($qid)
		.", action=".db_escape($action).",
		dest_id=".db_escape($dest_id).", amount=".db_escape($amount)
		.", dimension_id=".db_escape($dim).", dimension2_id=".db_escape($dim2).", memo=".db_escape($memo)."
		WHERE id = ".db_escape($selected_id);

	db_query($sql, "could not update quick entry line for $selected_id");
}

//---------------------------------------------------------------------------------------------

function delete_quick_entry_line($selected_id)
{
	$sql = "DELETE FROM ".TB_PREF."quick_entry_lines WHERE id=".db_escape($selected_id);

	db_query($sql,"could not delete quick entry line $selected_id");
}
//---------------------------------------------------------------------------------------------


function has_quick_entries($type=null)
{
	$sql = "SELECT id FROM ".TB_PREF."quick_entries";
	if ($type != null)
		$sql .= " WHERE type=".db_escape($type);

	$result = db_query($sql, "could not retreive quick entries");
	return db_num_rows($result) > 0;
}

function get_quick_entries($type = null)
{
	$sql = "SELECT * FROM ".TB_PREF."quick_entries";
	if ($type != null)
		$sql .= " WHERE type=".db_escape($type);
	$sql .= " ORDER BY description";

	return db_query($sql, "could not retreive quick entries");
}

function get_quick_entry($selected_id)
{
	$sql = "SELECT * FROM ".TB_PREF."quick_entries WHERE id=".db_escape($selected_id);

	$result = db_query($sql, "could not retreive quick entry $selected_id");

	return db_fetch($result);
}

function get_quick_entry_lines($qid)
{
	$sql = "SELECT line.*, coa.account_name, taxtype.name as tax_name
		FROM ".TB_PREF."quick_entry_lines line
			LEFT JOIN ".TB_PREF."chart_master coa ON line.dest_id = coa.account_code
			LEFT JOIN ".TB_PREF."tax_types taxtype ON line.dest_id = taxtype.id
		WHERE 
			qid=".db_escape($qid)." ORDER by id";

	return db_query($sql, "could not retreive quick entries");
}

function has_quick_entry_lines($qid)
{
	$sql = "SELECT id FROM ".TB_PREF."quick_entry_lines WHERE qid=".db_escape($qid);

	$result = db_query($sql, "could not retreive quick entries");
	return db_num_rows($result) > 0;
}

//---------------------------------------------------------------------------------------------

function get_quick_entry_line($selected_id)
{
	$sql = "SELECT * FROM ".TB_PREF."quick_entry_lines WHERE id=".db_escape($selected_id);

	$result = db_query($sql, "could not retreive quick entry for $selected_id");

	return db_fetch($result);
}

//---------------------------------------------------------------------------------------------

function get_max_reconciled($date, $bank_account)
{
	$date = date2sql($date);
	 // temporary fix to enable fix of invalid entries made in 2.2RC
	if ($date == 0) $date = '0000-00-00';

	$sql = "SELECT MAX(reconciled) as last_date,
			 SUM(IF(reconciled<='$date', amount, 0)) as end_balance,
			 SUM(IF(reconciled<'$date', amount, 0)) as beg_balance,
			 SUM(amount) as total
		FROM ".TB_PREF."bank_trans trans
		WHERE bank_act=".db_escape($bank_account);
	//	." AND trans.reconciled IS NOT NULL";

	return db_query($sql,"Cannot retrieve reconciliation data");

}

//---------------------------------------------------------------------------------------------

function get_ending_reconciled($bank_account, $bank_date)
{
	$sql = "SELECT ending_reconcile_balance
		FROM ".TB_PREF."bank_accounts WHERE id=".db_escape($bank_account)
		. " AND last_reconciled_date=".db_escape($bank_date);
	$result = db_query($sql,"Cannot retrieve last reconciliation");
	return db_fetch($result);
}

//---------------------------------------------------------------------------------------------

function get_sql_for_bank_account_reconcile($bank_account, $date)
{
	$sql = "SELECT	bt.type, trans_no, ref, trans_date,
				amount,	person_id, person_type_id, reconciled, bt.id, c.memo_
		FROM ".TB_PREF."bank_trans bt
		LEFT JOIN ".TB_PREF."comments c ON c.type = bt.type AND c.id = bt.trans_no
		WHERE bank_act = ".db_escape($bank_account) . "
			AND (reconciled IS NULL OR reconciled='". date2sql($date) ."')
			AND amount != 0
		ORDER BY trans_date, bt.id";

	return $sql;
}

//---------------------------------------------------------------------------------------------

function update_reconciled_values($reconcile_id, $reconcile_value, $reconcile_date, $end_balance, $bank_account)
{
	$sql = "UPDATE ".TB_PREF."bank_trans SET reconciled=$reconcile_value"
		." WHERE id=".db_escape($reconcile_id);

  	db_query($sql, "Can't change reconciliation status");
	// save last reconcilation status (date, end balance)
    $sql2 = "UPDATE ".TB_PREF."bank_accounts SET last_reconciled_date='"
			.date2sql($reconcile_date)."',
    	    ending_reconcile_balance=$end_balance
			WHERE id=".db_escape($bank_account);

	db_query($sql2,"Error updating reconciliation information");
}
//---------------------------------------------------------------------------------------------

function get_default_bank_account($curr=null)
{
	/* default bank account is selected as first found account from:
		. default account in $curr if any
		. first defined account in $curr if any
		. default account in home currency
		. first defined account in home currency
	*/
	$home_curr = get_company_pref('curr_default');
	if (!isset($curr))
		$curr = $home_curr;

	$sql = "SELECT b.*, b.bank_curr_code='$home_curr' as fall_back FROM "
		.TB_PREF."bank_accounts b"
		." WHERE b.bank_curr_code=".db_escape($curr)
		." OR b.bank_curr_code='$home_curr'
		ORDER BY fall_back, dflt_curr_act desc";

	$result = db_query($sql, "could not retreive default bank account");

	return db_fetch($result);
}

function get_default_customer_bank_account($cust_id)
{
	$sql = "SELECT curr_code FROM ".TB_PREF."debtors_master WHERE debtor_no=".db_escape($cust_id);
	$result = db_query($sql, "could not retreive default customer currency code");
	$row = db_fetch_row($result);
	$ba = get_default_bank_account($row[0]);
	return $ba['id'];
}

function get_default_supplier_bank_account($supplier_id)
{
	$sql = "SELECT curr_code FROM ".TB_PREF."suppliers WHERE supplier_id=".db_escape($supplier_id);
	$result = db_query($sql, "could not retreive default supplier currency code");
	$row = db_fetch_row($result);
	$ba = get_default_bank_account($row[0]);
	return $ba['id'];
}
//---------------------------------------------------------------------------------------------
//
//	This function returns current allowed bank payment limit, or null if limit is not set.
//	As of FA 2.3 check is done only on cash account, for which limit is bank amount as of the date selected.
//
function get_bank_account_limit($account, $date, $user=null)
{
//	$user = current_user();

	$bankacc = get_bank_account($account);
	if ($bankacc['account_type'] != BT_CASH)
		return null;
	return max(0, get_balance_before_for_bank_account($account, add_days($date,1))); // in case of manco made before the check was implemented
}
 
function identify_bank_account($account_number)
{
	$number = db_escape($account_number);
	$sql = "(SELECT id AS id, ".BO_COMPANY." AS type FROM ".TB_PREF."bank_accounts WHERE REPLACE(bank_account_number,' ', '')=$number)";
	$sql .= " UNION (SELECT supplier_id AS id, ".BO_SUPPLIER." AS type FROM ".TB_PREF."suppliers WHERE REPLACE(bank_account,' ', '')=$number)";
	$sql .= " UNION (SELECT branch_code AS id, ".BO_CUSTBRANCH." AS type FROM ".TB_PREF."cust_branch WHERE REPLACE(bank_account,' ', '')=$number)";

	$result = db_query($sql, 'bank account identification failed');

	if (db_num_rows($result))
		return db_fetch($result);
	else
		return null;
}

//----------------------------------------------------------------------------------

function get_bank_account_currency($id)
{
	$sql= "SELECT bank_curr_code FROM ".TB_PREF."bank_accounts WHERE id=".db_escape($id);
	$result = db_query($sql, "retreive bank account currency");

	$myrow = db_fetch_row($result);
	return $myrow[0];
}

