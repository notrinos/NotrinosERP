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
class fa2_1 extends fa_patch {
	var $previous = '';		// applicable database version
	var $version = '2.1';	// version installed
	var $description;
	var $sql = 'alter2.1.sql';

	function __construct() {
		$this->description = _('Upgrade from version 2.0 to 2.1');
	}
	//
	//	Install procedure. All additional changes 
	//	not included in sql file should go here.
	//
	function install($company, $force=false) 
	{
		global $db;

	/*
	Statement below is allowed only for MySQL >=4.0.4:
	UPDATE `0_bank_trans`, `0_bank_accounts` 
		SET 0_bank_trans.bank_act=0_bank_accounts.id 
		WHERE 0_bank_trans.bank_act=0_bank_accounts.account_code;
	*/
		$sql = "SELECT id, account_code FROM ".TB_PREF."bank_accounts";
		if(!($res = db_query($sql))) {
			display_error(_("Cannot retrieve bank accounts codes")
				.':<br>'. db_error_msg($db));
			return false;
		}
		while ($acc = db_fetch($res)) {
			$sql = "UPDATE ".TB_PREF."bank_trans SET bank_act='"
				.$acc['id']."' WHERE bank_act=".$acc['account_code'];
			if (db_query($sql)==false) {
			display_error(_("Cannot update bank transactions")
				.':<br>'. db_error_msg($db));
				return false;
			}
		}
		// copy all item codes from stock_master into item_codes
		$sql = "SELECT `stock_id`,`description`,`category_id` FROM ".TB_PREF."stock_master";
		$result = db_query($sql);
		if (!$result) {
			display_error(_("Cannot select stock identificators")
				.':<br>'. db_error_msg($db));
			return false;
		} else {
			while ($row = db_fetch_assoc($result)) {
				$sql = "INSERT IGNORE "
					.TB_PREF."item_codes (`item_code`,`stock_id`,`description`,`category_id`)
					VALUES('".$row['stock_id']."','".$row['stock_id']."','"
					.$row['description']."','".$row['category_id']."')";
				$res2 = db_query($sql);
				if (!$res2) {
					display_error(_("Cannot insert stock id into item_codes")
						.':<br>'. db_error_msg($db));
					return false;
				}
			}
		}
		// remove obsolete bank_trans_types table 
		// (DROP queries are skipped during non-forced upgrade)
		$sql = "DROP TABLE IF EXISTS `".TB_PREF."bank_trans_types`";
		db_query($sql);
		//
		//	Move all debtor and supplier trans tax details to new table
		// (INSERT INTO t  SELECT ... FROM t ... available after 4.0.14)
		// No easy way to restore net amount for 0% tax rate for moved
		// FA 2.0 transactions, but who cares?
		//
	$move_sql =array( 
	"debtor_trans_tax_details" =>
		"SELECT tr.tran_date, tr.type, tr.trans_no, dt.tax_type_id, 
			dt.rate, dt.included_in_price, dt.amount, tr.reference as ref,
			tr.rate as ex_rate
		FROM ".TB_PREF."debtor_trans_tax_details dt	
			LEFT JOIN ".TB_PREF."trans_tax_details tt
				ON dt.debtor_trans_no=tt.trans_no 
				AND dt.debtor_trans_type=tt.trans_type,
			".TB_PREF."debtor_trans tr
		WHERE tt.trans_type is NULL
			AND dt.debtor_trans_no = tr.trans_no 
			AND dt.debtor_trans_type = tr.type",
	
	"supp_invoice_tax_items" =>
		"SELECT tr.tran_date, tr.type, tr.trans_no, st.tax_type_id, 
			st.rate, st.included_in_price, st.amount, tr.supp_reference as ref,
			tr.rate as ex_rate
			FROM ".TB_PREF."supp_invoice_tax_items st	
				LEFT JOIN ".TB_PREF."trans_tax_details tt
					ON st.supp_trans_no=tt.trans_no 
					AND st.supp_trans_type=tt.trans_type,
					".$pref."supp_trans tr
				WHERE tt.trans_type is NULL
					AND st.supp_trans_no = tr.trans_no 
					AND st.supp_trans_type = tr.type");

	foreach ($move_sql as $tbl => $sql) {
		if (!check_table(TB_PREF, $tbl)){
			$res = db_query($sql, "Cannot retrieve trans tax details from $tbl");
			while ($row = db_fetch($res)) {
				$net_amount = $row['rate'] == 0 ?
				 	0 : ($row['included_in_price'] ? 
							($row['amount']/$row['rate']*(100-$row['rate']))
							:($row['amount']/$row['rate']*100));
				$sql2 = "INSERT INTO ".TB_PREF."trans_tax_details 
				(trans_type,trans_no,tran_date,tax_type_id,rate,ex_rate,
					included_in_price, net_amount, amount, memo)
				VALUES ('".$row['type']."','".$row['trans_no']."','"
					.$row['tran_date']."','".$row['tax_type_id']."','"
					.$row['rate']."','".$row['ex_rate']."','"
					.$row['included_in_price']."','".$net_amount
					."','".$row['amount']."','".$row['ref']."')";
				db_query($sql2, "Cannot move trans tax details from $tbl");
			}
			db_query("DROP TABLE ".TB_PREF.$tbl, "cannot remove $tbl");
		}
	}
		
		return true;
	}

	//
	//	Checking before install
	//
	function prepare()
	{
	// We cannot perform successfull upgrade on system where the
	// trans tax details tables was deleted during previous try.
		$pref = $this->companies[$company]['tbpref'];

		if (check_table($pref, 'debtor_trans_tax_details') 
			|| check_table($pref, 'supp_invoice_tax_items')) {
			display_error(_("Seems that system upgrade to version 2.1 has 
			been performed for this company already.<br> If something has gone 
			wrong and you want to retry upgrade process you MUST perform 
			database restore from last backup file first."));

			return false;
		}

		return true; // true when ok, fail otherwise
	}
};

$install = new fa2_1;
