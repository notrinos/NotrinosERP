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

class fa2_4 extends fa_patch {
	var $previous = '2.3rc';		// applicable database version
	var $version = '2.4.1';	// version installed
	var $description;
	var $sql = 'alter2.4.sql';
	var $preconf = true;
	var	$max_upgrade_time = 900;	// table recoding is really long process
	
	function __construct() {
		parent::__construct();
		$this->description = _('Upgrade from version 2.3 to 2.4');
	}
	
    /*
	    Shows parameters to be selected before upgrade (if any)
	*/
    function show_params($comp)
	{
	  display_note(_('Set optimal parameters and start upgrade:'));
	  start_table(TABLESTYLE);
	  start_row();
		collations_list_row(_('Text collation optimization:'), 'collation', substr($_SESSION['language']->code, 0, 2));
	  end_row();
	  end_table();
	  br();
    }

	/*
	    Fetches selected upgrade parameters.
    */
	function prepare()
    {
		$this->collation = get_post('collation');
		return true;
	}

	//
	//	Install procedure. All additional changes 
	//	not included in sql file should go here.
	//
	function install($company, $force=false)
	{
		global $db_version, $db_connections;

		$pref = $db_connections[$company]['tbpref'];

		if (get_company_pref('grn_clearing_act') === null) { // available form 2.3.1, can be not defined on pre-2.4 installations
			set_company_pref('grn_clearing_act', 'glsetup.purchase', 'varchar', 15, 0);
		}
		if (get_company_pref('default_quote_valid_days') === null) { // new in 2.3.23 installations
			set_company_pref('default_quote_valid_days', 'glsetup.sales', 'smallint', 6, 30);
		}
		if (get_company_pref('bcc_email') === null) { // available from 2.3.14, can be not defined on pre-2.4 installations
			set_company_pref('bcc_email', 'setup.company', 'varchar', 100, '');
		}
		if (get_company_pref('alternative_tax_include_on_docs') === null) { // available from 2.3.14, can be not defined on pre-2.4 installations
			set_company_pref('alternative_tax_include_on_docs', 'setup.company', 'tinyint', 1, '0');
		}
		if (get_company_pref('suppress_tax_rates') === null) { // available from 2.3.14, can be not defined on pre-2.4 installations
			set_company_pref('suppress_tax_rates', 'setup.company', 'tinyint', 1, '0');
		}
		if (get_company_pref('company_logo_report') === null) { // available from 2.4.2, during updates
			set_company_pref('company_logo_report', 'setup.company', 'tinyint', 1, '0');
		}
		if (get_company_pref('print_dialog_direct') === null) { // available from 2.4.5, during updates
			set_company_pref('print_dialog_direct', 'setup.company', 'tinyint', 1, '0');
		}
		if (get_company_pref('barcodes_on_stock') === null) { // available from 2.4.3, during updates
			set_company_pref('barcodes_on_stock', 'setup.company', 'tinyint', 1, '0');
		}
		if (get_company_pref('ref_no_auto_increase') === null) { // available from 2.4.4, during updates
			set_company_pref('ref_no_auto_increase', 'setup.company', 'tinyint', 1, '0');
		}
		if (get_company_pref('shortname_name_in_list') === null) { // available from 2.4.2, during updates
			set_company_pref('shortname_name_in_list', 'setup.company', 'tinyint', 1, '0');
		}

		$result = $this->update_workorders()  && $this->update_grn_rates() && $this->switch_database_to_utf($pref);

		if ($result)
			$result = $this->do_cleanup();
		if ($result)
		{
			$db_connections[$company]['collation'] = $this->collation;
			if (write_config_db())
				return $this->log_error(_("Cannot update config_db.php file."));
		}

		$sec_updates = array(
			'SA_SETUPCOMPANY' => array(
				'SA_ASSET', 'SA_ASSETCATEGORY', 'SA_ASSETCLASS',
				'SA_ASSETSTRANSVIEW','SA_ASSETTRANSFER', 'SA_ASSETDISPOSAL',
				'SA_DEPRECIATION', 'SA_ASSETSANALYTIC'),
		);
		$result = $this->update_security_roles($sec_updates);

		return $result;
	}

	//
	// optional procedure done after upgrade fail, before backup is restored
	//
	function post_fail($company)
	{
		$pref = $this->companies[$company]['tbpref'];
		db_query("DROP TABLE IF EXISTS " . $pref . 'wo_costing');
		db_query("DROP TABLE IF EXISTS " . $pref . 'stock_fa_class');
		db_query("DELETE FROM ".$pref."sys_prefs
			WHERE `name` in (
				'gl_closing_date', 'deferred_income_act', 'no_zero_lines_amount', 'accounts_alpha',
				'tax_algorithm', 'grn_clearing_act', 'default_receival_required',
				'default_quote_valid_days',	'no_zero_lines_amount',	'show_po_item_codes', 'accounts_alpha',
				'loc_notification', 'print_invoice_no', 'allow_negative_prices', 'print_item_images_on_quote',
				'bcc_email', 'alternative_tax_include_on_docs', 'suppress_tax_rates', 'company_logo_report',
				'barcodes_on_stock', print_dialog_direct', 'ref_no_auto_increase')");
	}

	function update_workorders()
	{
		global $db;

		$sql = "SELECT DISTINCT type, type_no, tran_date, person_id FROM ".TB_PREF."gl_trans WHERE `type`=".ST_WORKORDER
			." AND person_type_id=1";
		$res = db_query($sql);
		if (!$res)
			return $this->log_error(sprintf(_("Cannot update work orders costs:\n%s"), db_error_msg($db)));

		while ($row = db_fetch($res))
		{
			$journal_id = get_next_trans_no(ST_JOURNAL);

			$sql1 = "UPDATE ".TB_PREF."gl_trans SET `type`=".ST_JOURNAL.", type_no={$journal_id},
				person_type_id=NULL, person_id=0
				WHERE `type`=".ST_WORKORDER." AND type_no={$row['type_no']} AND tran_date='{$row['tran_date']}'
				AND person_id='{$row['person_id']}'";
			if (!db_query($sql1)) return false;

			$sql2 = "INSERT INTO ".TB_PREF."wo_costing (workorder_id, cost_type, trans_no) 
				VALUES ({$row['type_no']}, {$row['person_id']}, {$journal_id})";
			if (!db_query($sql2)) return false;
		}
		return true;
	}

/*
	In previous versions FA ignored encoding settings on database/tables, so it depended on server settings,
	but data stored is encoded in user language encoding. Now we switch to utf8 internal database encoding, while
	user encoding can be selected independently.

	To perform safe FA database switch to utf-8 encoding we have to first ensure that all text/char columns 
	have properly set encoding (the same as its content), so the algorithm performed on every table is as follows:
 	. set default table encoding for the table to currently used on client side;
 	. for all text/char column:
	 - suppress autorecoding by change of the type to related binary/blob type
	 - change column to utf8 encodding and selected collation.
	. change default table encoding to utf8 and selected collation
*/
	function switch_database_to_utf($pref, $dbg = false) {

		global $installed_languages, $dflt_lang;

		$old_encoding = 'latin1'; // default client encoding

		// uncomment in case of 1071 errors (requires SUPER privileges)
		// db_query("SET @@global.innodb_large_prefix=1", "Cannot set large prefix");

		 // site default encoding is presumed as encoding for all databases!
		$lang = array_search_value($dflt_lang, $installed_languages, 'code');
		$new_encoding = get_mysql_encoding_name(strtoupper($lang['encoding']));

 		$this->log_error(sprintf('Switching database to utf8 encoding from %s', $old_encoding), 'Info');
		$collation = get_mysql_collation($this->collation);
		$tsql = "SHOW TABLES LIKE '".($pref=='' ? '' : substr($pref, 0, -1).'\\_')."%'";
		$tresult = db_query($tsql, "Cannot select all tables with prefix '$pref'");
		while($tbl = db_fetch($tresult)) {
			$table = $tbl[0];

			db_query("ALTER TABLE `$table` CONVERT TO CHARACTER SET $old_encoding"); // convert encoding on utf-8 tables

			// set proper default table encoding for current user language (used on binary->text conversion)
			db_query("ALTER TABLE `$table` CHARSET $new_encoding");
			$csql = "SHOW COLUMNS FROM $table";
			$cresult = db_query($csql, "Cannot select column names for table '$table'");
			$convert = false;

			$to_binary = $to_default = $to_utf = array();
			while($col = db_fetch($cresult)) {

				$bintype = strtr($col['Type'], array('varchar' => 'varbinary', 'char'=>'varbinary', 'text'=>'blob', 'tinytext'=>'tinyblob'));

				if ($bintype != $col['Type'])
				{ // this is char/text column, so change encoding to proper encoding
			 	 	if ($dbg)
 	 					$this->log_error(sprintf('%s switched to uft8.', $table.'.'.$col['Field']), 'Debug');

					$null = $col['Null'] === 'YES' ? ' NULL ' : ' NOT NULL ';
					$default = $col['Null'] !== 'YES' && isset($col['Default']) ? ' DEFAULT '.db_escape($col['Default']) : '';

					// to avoid column width multiplication x3 we old->binary->ui->utf column type change instead of column CONVERT

					$to_binary[] = "CHANGE `".$col['Field']."` `".$col['Field']."` ".$bintype;
					$to_default[] = "CHANGE `".$col['Field']."` `".$col['Field']."` ".$col['Type'].$null.$default;
					$to_utf[] = "MODIFY COLUMN `".$col['Field']."` ".$col['Type']." COLLATE ".$collation.$null.$default;
					$convert = true;
				}
			}
			if(count($to_binary))
			{
				$sql = "ALTER TABLE `$table` ".implode(',',$to_binary);
				db_query($sql);
				$sql = "ALTER TABLE `$table` ".implode(',',$to_default);
				db_query($sql);
				$sql = "ALTER TABLE `$table` ".implode(',',$to_utf);
				db_query($sql);
			}
			db_query("ALTER TABLE `$table` COLLATE $collation");
		}
		db_query("ALTER DATABASE COLLATE $collation");
 		$this->log_error(_('Convertion to utf8 done.'), 'Info');

		return true;
	}

	function update_grn_rates()
	{
		$sql = "SELECT grn.id, grn.delivery_date, supp.curr_code 
			FROM ".TB_PREF."grn_batch grn, ".TB_PREF."suppliers supp
			WHERE supp.supplier_id=grn.supplier_id AND supp.curr_code!='".get_company_pref('curr_default')."'";
		$result = db_query($sql);

		if (!$result)
			return false;

		$sql = "UPDATE ".TB_PREF."grn_batch SET rate=%s WHERE id=%d";
		while ($grn = db_fetch($result))
			db_query(sprintf($sql, get_exchange_rate_from_home_currency($grn['curr_code'], sql2date($grn['delivery_date'])), $grn['id']));

		return true;
	}

	function do_cleanup()
	{
		global $db;

		//remove obsolete and temporary columns.
		// this have to be done here as db_import rearranges alter query order
		$dropcol = array(
				'tax_groups' => array('tax_shipping'),
				'tax_group_items' => array('rate'),
				'budget_trans' => array('type', 'type_no', 'person_id', 'person_type_id', 'memo_'),
				'cust_branch' => array('contact_name', 'disable_trans'),
				'stock_moves' => array('discount_percent', 'visible', 'person_id'),
		);

		foreach($dropcol as $table => $columns)
			foreach($columns as $col) {
				if (db_query("ALTER TABLE `".TB_PREF."{$table}` DROP `$col`") == false) {
					return $this->log_error(sprintf(_("Cannot drop column in %s table: %s"), $table, db_error_msg($db)));
				}
			}
		return true;
  }
}

$install = new fa2_4;
