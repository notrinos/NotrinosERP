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
class fa2_3 extends fa_patch {
	var $previous = '2.2rc';		// applicable database version
	var $version = '2.3rc';	// version installed
	var $description;
	var $sql = 'alter2.3.sql';
	var $preconf = true;
	var $beta = false; // upgrade from 2.2 or 2.3beta;
	
	function __construct() {
		$this->description = _('Upgrade from version 2.2 to 2.3');
	}
	
	//
	//	Install procedure. All additional changes 
	//	not included in sql file should go here.
	//
	function install($company, $force=false) 
	{
		global $db_version, $dflt_lang;

		$this->preconf = $this->fix_extensions();
		if (!$this->preconf)
			return false;

		if (!$this->beta) {
			// all specials below are already done on 2.3beta

			$sql = "SELECT debtor_no, payment_terms FROM ".TB_PREF."debtors_master";

			$result = db_query($sql);
			if (!$result) {
				display_error("Cannot read customers"
				.':<br>'. db_error_msg($db));
				return false;
			}
			// update all sales orders and transactions with customer std payment terms
			while($cust = db_fetch($result)) {
				$sql = "UPDATE ".TB_PREF."debtor_trans SET "
					."payment_terms = '" .$cust['payment_terms']
					."' WHERE debtor_no='".$cust['debtor_no']."'";
				if (db_query($sql)==false) {
					display_error("Cannot update cust trans payment"
					.':<br>'. db_error_msg($db));
					return false;
				}
				$sql = "UPDATE ".TB_PREF."sales_orders SET "
					."payment_terms = '" .$cust['payment_terms']
					."' WHERE debtor_no='".$cust['debtor_no']."'";
				if (db_query($sql)==false) {
					display_error("Cannot update sales order payment"
					.':<br>'. db_error_msg($db));
					return false;
				}
			}
			if (!$this->update_totals()) {
				display_error("Cannot update order totals");
				return false;
			}
			if (!$this->update_line_relations()) {
				display_error("Cannot update sales document links");
				return false;
			}
			//remove obsolete and temporary columns.
			// this have to be done here as db_import rearranges alter query order
			$dropcol = array(
				'crm_persons' => array('tmp_id','tmp_class'),
				'debtors_master' => array('email'),
				'cust_branch' => array('phone', 'phone2', 'fax', 'email'),
				'suppliers' => array('phone', 'phone2', 'fax', 'email'),
				'debtor_trans' => array('trans_link')
			);

			foreach($dropcol as $table => $columns)
				foreach($columns as $col) {
					if (db_query("ALTER TABLE `".TB_PREF."{$table}` DROP `$col`")==false) {
						display_error("Cannot drop {$table}.{$col} column:<br>".db_error_msg($db));
						return false;
					}
				}
			// remove old preferences table after upgrade script has been executed
			$sql = "DROP TABLE IF EXISTS `".TB_PREF."company`";
			if (!db_query($sql))
				return false;
		}
		$this->update_lang_cfg();
		return  update_company_prefs(array('version_id'=>$db_version));
	}
	//
	//	Checking before install
	//
	function prepare()
	{

		if ($this->beta)
			$this->sql = 'alter2.3rc.sql';

		return true;
	}

	//=========================================================================================
	//	2.3 specific update functions
	//

	/*
		Update order totals
	*/
	function update_totals()
	{
		global $path_to_root;

		include_once("$path_to_root/sales/includes/cart_class.inc");
		include_once("$path_to_root/purchasing/includes/po_class.inc");
		$cart = new cart(ST_SALESORDER);
		$sql = "SELECT order_no, trans_type FROM ".TB_PREF."sales_orders";
		$orders = db_query($sql);
		if (!$orders)
			return false;
		while ($order = db_fetch($orders)) {
			read_sales_order($order['order_no'], $cart, $order['trans_type']);
			$result = db_query("UPDATE ".TB_PREF."sales_orders 
				SET total=".$cart->get_trans_total()
				." WHERE order_no=".$order[0]);
			unset($cart->line_items);
		}
		unset($cart);
		$cart = new purch_order();
		$sql = "SELECT order_no FROM ".TB_PREF."purch_orders";
		$orders = db_query($sql);
		if (!$orders)
			 return false;
		while ($order_no = db_fetch($orders)) {
			read_po($order_no[0], $cart);
			$result = db_query("UPDATE ".TB_PREF."purch_orders SET total=".$cart->get_trans_total());
			unset($cart->line_items);
		}
		return true;
	}

	//------------------------------------------------------------------------------
	//	Retreive parent document number(s) for given transaction
	//
	function get_parent_trans_2_2($trans_type, $trans_no) {

		$sql = 'SELECT trans_link FROM
				'.TB_PREF.'debtor_trans WHERE trans_no='.db_escape($trans_no)
				.' AND type='.db_escape($trans_type).' AND trans_link!=0';

		$result = db_query($sql, 'Parent document numbers cannot be retrieved');

		if (db_num_rows($result)) {
			$link = db_fetch($result);
			return array($link['trans_link']);
		}
		if ($trans_type!=ST_SALESINVOICE) return 0;	// this is credit note with no parent invoice
		// invoice: find batch invoice parent trans.
		$sql = 'SELECT trans_no FROM
				'.TB_PREF.'debtor_trans WHERE
				(trans_link='.db_escape($trans_no).' AND type='. get_parent_type($trans_type) .')';

		$result = db_query($sql, 'Delivery links cannot be retrieved');

		$delivery = array();
		if(db_num_rows($result)>0) {
			while($link = db_fetch($result)) {
				$delivery[] = $link['trans_no'];
			}
		}
		return count($delivery) ? $delivery : 0;
	}

	/*
		Reorganizing document relations. Due to the design issue in pre 2.3 db structure
		there can be sales documents with lines not properly linked to parents. This rare 
		cases will be described in error log.
	*/
	function update_line_relations()
	{
		global $path_to_root, $systypes_array;

		require_once("$path_to_root/includes/sysnames.inc");
		
		$sql =	"SELECT d.type, trans_no, order_ FROM ".TB_PREF."debtor_trans d
			LEFT JOIN ".TB_PREF."voided v ON d.type=v.type AND d.trans_no=v.id
				WHERE ISNULL(v.type) AND 
				(d.type=".ST_CUSTDELIVERY
				." OR d.type=".ST_SALESINVOICE
				." OR d.type=".ST_CUSTCREDIT.")";
		$result = db_query($sql);
		if (!$result)
			return false;

		while ($trans = db_fetch($result)) {
			$type = $trans['type'];
			$trans_no = $trans['trans_no'];
			$invalid = 0;
			$msg ='';

			$lines = get_customer_trans_details($type, $trans_no);
			$n = db_num_rows($lines);

			if ($type==ST_CUSTDELIVERY)
				$src_lines = get_sales_order_details($trans['order_'], ST_SALESORDER);
			else
				$src_lines =  get_customer_trans_details(get_parent_type($type), 
					$this->get_parent_trans_2_2($type, $trans_no));

			$src_n = db_num_rows($src_lines);

			if (($type == ST_CUSTCREDIT) && ($src_n == 0))
				 continue;  // free credit note has no src lines 

			$max = $type == ST_CUSTDELIVERY ? $n : max($src_n, $n);

			for($i = 0, $j=0; $i < $max; $i++) {
				if (!($doc_line = @db_fetch($lines)))
					break;

				if(!($src_line = @db_fetch($src_lines)))
					break;

				if ($type == ST_CUSTDELIVERY)
					$src_line['stock_id'] = $src_line['stk_code']; // SO details has another field name 

				if ($src_line['stock_id'] == $doc_line['stock_id']
					&& ($src_line['quantity'] >= $doc_line['quantity'])) {

 					$sql = "UPDATE ".TB_PREF."debtor_trans_details SET src_id = {$src_line['id']}
						WHERE id = {$doc_line['id']}";
					if (!db_query($sql))
						return false;
					$j++;
				}
			}
			if ($j != $n) {
				error_log("Line level relations error for ".$systypes_array[$type]." #$trans_no.");
			}
		}
	return true;
	}

	function fix_extensions()
	{
		global $path_to_root, $next_extension_id, $installed_languages;

		$lang_chd = false;
		foreach($installed_languages as $i => $lang) {
			if (!isset($lang['path'])) {
				$code = $lang['code'];
				$installed_languages[$i]['path'] = 'lang/'.$code;
				$installed_languages[$i]['package'] = $code;
				$lang_chd = true;
			}
		}
		if ($lang_chd)
			write_lang();

		$installed_extensions= get_company_extensions();
		if (!isset($next_extension_id))
			$next_extension_id = 1;
		$new_exts = array();

/*	Old extension modules are uninstalled - they need manual porting after 
	heavy changes in extension system in FA2.3

		foreach($installed_extensions as $i => $ext)
		{
			if (isset($ext['title'])) // old type entry
			{
				if ($ext['type'] == 'module') {
					$new['type'] = 'extension';
					$new['tabs'][] = array(
						'url' => $ext['filename'],
						'access' => isset($ext['access']) ? $ext['access'] : 'SA_OPEN',
						'tab_id' => $ext['tab'],
						'title' => $ext['title']
					);
					$new['path'] = $ext['path'];
				}
				else // plugin
				{
					$new['type'] = 'extension';
					$new['tabs'] = array();
					$new['path'] = 'modules/'.$ext['path'];
					$new['entries'][] = array(
						'url' => $ext['filename'],
						'access' => isset($ext['access']) ? $ext['access'] : 'SA_OPEN',
						'tab_id' => $ext['tab'],
						'title' => $ext['title']
					);
				}
				if (isset($ext['acc_file']))
					$new['acc_file'] = $ext['acc_file'];
				$new['name'] = $ext['name'];
				$new['package'] = $new['package'] = '';
				$new['active'] = 1;

				$new_exts[$i] = $new;
			}
		}
*/		
		// Preserve non-standard themes
		$path = $path_to_root.'/themes/';
		$themes = array();
		$themedir = opendir($path);
		while (false !== ($fname = readdir($themedir)))
		{
			if ($fname!='.' && $fname!='..' && $fname!='CVS' && is_dir($path.$fname)
				&& !in_array($fname, array('aqua', 'cool', 'default')))
			{
				foreach($installed_extensions as $ext)  
					if ($ext['path'] == 'themes/'.$fname) // skip if theme is already listed
						continue 2;
				$new_exts[$next_extension_id++] = array(
					'name' => 'Theme '. ucwords($fname),
					'package' => $fname,
					'type' => 'theme',
					'active' => true,
					'path' => 'themes/'.$fname
				);
			}
		}
		closedir($themedir);

		if (count($new_exts)) {
			return update_extensions($new_exts);
		} else
			return true;
	}
	
	function update_lang_cfg()
	{
		global $dflt_lang, $installed_languages;

		foreach($installed_languages as $n => $lang) {
			if ($lang['code'] == 'en_GB') {
				$installed_languages[$n] = array('code'=>'C','name'=>'English',
					'encoding'=>'iso-8859-1', 'path' => '', 'package' => '');
				if ($dflt_lang == 'en_GB')
					$dflt_lang = 'C';
				write_lang();
			}
		}
	}

}

$install = new fa2_3;

