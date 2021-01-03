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
function add_gl_account($account_code, $account_name, $account_type, $account_code2)
{
	$sql = "INSERT INTO ".TB_PREF."chart_master (account_code, account_code2, account_name, account_type)
		VALUES (".db_escape($account_code).", ".db_escape($account_code2).", "
			.db_escape($account_name).", ".db_escape($account_type).")";

	return db_query($sql);
}

function update_gl_account($account_code, $account_name, $account_type, $account_code2)
{
    $sql = "UPDATE ".TB_PREF."chart_master SET account_name=".db_escape($account_name)
    .",account_type=".db_escape($account_type).", account_code2=".db_escape($account_code2)
		." WHERE account_code = ".db_escape($account_code);

	return db_query($sql);
}

function delete_gl_account($code)
{
	$sql = "DELETE FROM ".TB_PREF."chart_master WHERE account_code=".db_escape($code);

	db_query($sql, "could not delete gl account");
}

function get_gl_accounts($from=null, $to=null, $type=null)
{
	$sql = "SELECT coa.*, act_type.name AS AccountTypeName
		FROM "
			.TB_PREF."chart_master coa,"
			.TB_PREF."chart_types act_type
		WHERE coa.account_type=act_type.id";
	if ($from != null)
		$sql .= " AND coa.account_code >= ".db_escape($from);
	if ($to != null)
		$sql .= " AND coa.account_code <= ".db_escape($to);
	if ($type != null)
		$sql .= " AND account_type=".db_escape($type);
	$sql .= " ORDER BY account_code";

	return db_query($sql, "could not get gl accounts");
}

function get_gl_account($code)
{
	$sql = "SELECT * FROM ".TB_PREF."chart_master WHERE account_code=".db_escape($code);

	$result = db_query($sql, "could not get gl account");
	return db_fetch($result);
}

function is_account_balancesheet($code)
{
	$sql = "SELECT act_class.ctype
		FROM "
			.TB_PREF."chart_class act_class, "
			.TB_PREF."chart_types act_type, "
			.TB_PREF."chart_master coa
		WHERE coa.account_type=act_type.id
			AND	act_type.class_id=act_class.cid
			AND coa.account_code=".db_escape($code);

	$result = db_query($sql,"could not retreive the account class for $code");
	$row = db_fetch_row($result);
	return $row[0] > 0 && $row[0] < CL_INCOME;
}

function get_gl_account_name($code)
{
	$sql = "SELECT account_name from ".TB_PREF."chart_master WHERE account_code=".db_escape($code);

	$result = db_query($sql,"could not retreive the account name for $code");

	if (db_num_rows($result) == 1)
	{
		$row = db_fetch_row($result);
		return $row[0];
	}

	display_db_error("could not retreive the account name for $code", $sql, true);
}

function gl_account_in_company_defaults($acc)
{
	$sql= "SELECT COUNT(*) 
	FROM ".TB_PREF."sys_prefs
	WHERE (name='debtors_act' 
		OR name='pyt_discount_act'
		OR name='creditors_act' 
		OR name='bank_charge_act' 
		OR name='exchange_diff_act'
		OR name='profit_loss_year_act'
		OR name='retained_earnings_act'
		OR name='freight_act'
		OR name='deferred_income_act'
		OR name='default_sales_act' 
		OR name='default_sales_discount_act'
		OR name='default_prompt_payment_act'
		OR name='default_inventory_act'
		OR name='default_cogs_act'
		OR name='default_adj_act'
		OR name='default_inv_sales_act'
		OR name='default_wip_act') AND value=".db_escape($acc);
	$result = db_query($sql,"Couldn't test for default company GL codes");

	$myrow = db_fetch_row($result);
	return ($myrow[0] > 0); 
}

function gl_account_in_stock_category($acc)
{
	$acc = db_escape($acc);

	$sql= "SELECT COUNT(*) 
		FROM ".TB_PREF."stock_category
		WHERE dflt_inventory_act=$acc 
			OR dflt_cogs_act=$acc
			OR dflt_adjustment_act=$acc 
			OR dflt_sales_act=$acc";
	$result = db_query($sql,"Couldn't test for existing stock category GL codes");

	$myrow = db_fetch_row($result);
	return ($myrow[0] > 0); 
}

function gl_account_in_stock_master($acc)
{
	$acc = db_escape($acc);

	$sql= "SELECT COUNT(*)
		FROM ".TB_PREF."stock_master
		WHERE inventory_account=$acc 
			OR cogs_account=$acc
			OR adjustment_account=$acc 
			OR sales_account=$acc";
	$result = db_query($sql,"Couldn't test for existing stock GL codes");

	$myrow = db_fetch_row($result);
	return ($myrow[0] > 0); 
}

function gl_account_in_tax_types($acc)
{
	$acc = db_escape($acc);

	$sql= "SELECT COUNT(*)
		FROM ".TB_PREF."tax_types
		WHERE sales_gl_code=$acc 
			OR purchasing_gl_code=$acc";
	$result = db_query($sql,"Couldn't test for existing tax GL codes");

	$myrow = db_fetch_row($result);
	return ($myrow[0] > 0); 
}

function gl_account_in_cust_branch($acc)
{
	$acc = db_escape($acc);

	$sql= "SELECT COUNT(*)
		FROM ".TB_PREF."cust_branch
		WHERE sales_account=$acc 
			OR sales_discount_account=$acc
			OR receivables_account=$acc
			OR payment_discount_account=$acc";
	$result = db_query($sql,"Couldn't test for existing cust branch GL codes");

	$myrow = db_fetch_row($result);
	return ($myrow[0] > 0); 
}

function gl_account_in_suppliers($acc)
{
	$acc = db_escape($acc);

	$sql= "SELECT COUNT(*)
		FROM ".TB_PREF."suppliers
		WHERE purchase_account=$acc
			OR payment_discount_account=$acc
			OR payable_account=$acc";
	$result = db_query($sql,"Couldn't test for existing suppliers GL codes");

	$myrow = db_fetch_row($result);
	return ($myrow[0] > 0); 
}

function gl_account_in_quick_entry_lines($acc)
{
	$acc = db_escape($acc);

	$sql=
	"SELECT COUNT(*)
		FROM ".TB_PREF."quick_entry_lines
		WHERE dest_id=$acc AND UPPER(LEFT(action, 1)) <> 'T'";
	$result = db_query($sql,"Couldn't test for existing Quick Entry Line GL codes");

	$myrow = db_fetch_row($result);
	return ($myrow[0] > 0); 
}
//
//	Returns n>0 when account is AR, n<0 when account is AP
//  (priority for AR accounts)
//
function is_subledger_account($account)
{
	$sql = "SELECT 1 
		FROM ".TB_PREF."cust_branch
		WHERE receivables_account=".db_escape($account)
	." UNION
		SELECT -1 
		FROM ".TB_PREF."suppliers 
		WHERE payable_account=".db_escape($account);

	$result = db_query($sql,"Couldn't test AR/AP account");
	$myrow = db_fetch_row($result);
	return $myrow == false ? 0 : $myrow[0];
}

function get_subaccount_data($code_id, $person_id)
{
	$sql = "SELECT debtor_ref as name, branch_code as id 
		FROM ".TB_PREF."cust_branch branch 
			LEFT JOIN ".TB_PREF."debtors_master d ON branch.debtor_no = d.debtor_no
		WHERE branch.receivables_account=".db_escape($code_id)
			." AND d.debtor_no=".db_escape($person_id)
		." UNION SELECT supp_ref as name, '' as id 
			FROM ".TB_PREF."suppliers supp
			WHERE supplier_id=".db_escape($person_id)." 
				AND payable_account=".db_escape($code_id);
	$result = db_query($sql, 'cannot retrieve counterparty name');

	return  db_fetch($result);
}

function get_subaccount_name($code_id, $person_id)
{
	$sql = "SELECT debtor_ref as ref
		FROM ".TB_PREF."cust_branch branch
			LEFT JOIN ".TB_PREF."debtors_master d ON branch.debtor_no = d.debtor_no
		WHERE branch.receivables_account=".db_escape($code_id)
			." AND d.debtor_no=".db_escape($person_id)
		." UNION SELECT supp_ref as ref
			FROM ".TB_PREF."suppliers supp
			WHERE payable_account=".db_escape($code_id)." 
				AND supplier_id=".db_escape($person_id);
	$result = db_query($sql, 'cannot retrieve counterparty name');
	$row = db_fetch($result);

	return  $row ? $row['ref'] : '';
}

function gl_account_in_bank_accounts($acc)
{
	$sql= "SELECT COUNT(*) FROM ".TB_PREF."bank_accounts WHERE 
		account_code=".db_escape($acc);
	$result = db_query($sql,"Couldn't test bank account GL codes");

	$myrow = db_fetch_row($result);
	return ($myrow[0] > 0); 
}

//----------------------------------------------------------------------------------
//	Check if given account is used by any bank_account. 
//	Returns id of first bank_account using account_code, null otherwise.
//
//	Keep in mind that direct posting to bank account is depreciated
//	because we have no way to select right bank account if 
//	there is more than one using given gl account.
//
function is_bank_account($account_code)
{
	$sql= "SELECT id FROM ".TB_PREF."bank_accounts WHERE account_code=".db_escape($account_code);
	$result = db_query($sql, "checking account is bank account");
	if (db_num_rows($result) > 0) {
		$acct = db_fetch($result);
		return $acct['id'];
	} else
		return false;
}

function get_chart_accounts_search($like, $skip=false)
{
	global $SysPrefs;

	if (isset($SysPrefs->max_rows_in_search))
		$limit = $SysPrefs->max_rows_in_search;
	else
		$limit = 10;

	if ($skip)
		$sql = "SELECT chart.account_code, chart.account_name, type.name, chart.inactive, type.id
			FROM (".TB_PREF."chart_master chart,".TB_PREF."chart_types type) "
			."LEFT JOIN ".TB_PREF."bank_accounts acc "
			."ON chart.account_code=acc.account_code
				WHERE acc.account_code  IS NULL
			AND chart.account_type=type.id ";
	else		
		$sql = "SELECT chart.account_code, chart.account_name, type.name, chart.inactive, type.id
			FROM ".TB_PREF."chart_master chart,".TB_PREF."chart_types type WHERE chart.account_type=type.id ";
	$sql .= "AND (
          	chart.account_name LIKE " . db_escape("%" . $like. "%") . " OR
          	chart.account_code LIKE " . db_escape("%" . $like. "%") . "
        	) 
    		ORDER BY chart.account_code LIMIT 0,".(int)($limit); // We only display 10 items.
	return db_query($sql, "Failed in retreiving GL account list.");
}
