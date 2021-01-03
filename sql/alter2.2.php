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

class fa2_2 extends fa_patch  {
	var $previous = '2.1';		// applicable database version
	var $version = '2.2rc';	// version installed
	var $description;
	var $sql = 'alter2.2.sql';
	var $preconf = true;
	var $beta = false; // upgrade from 2.1 or 2.2beta; set in prepare()
	
	function __construct() {
		global $security_groups;
		$this->beta = !isset($security_groups);
		$this->description = _('Upgrade from version 2.1/2.2beta to 2.2');
		$this->preconf = fix_extensions();
	}
	
	//
	//	Install procedure. All additional changes 
	//	not included in sql file should go here.
	//
	function install($company, $force=false) 
	{
		global $db, $systypes_array, $db_connections;

		if (!$this->preconf)
			return false;

		$pref = $db_connections[$company]['tbpref'];
		// Until 2.2 sanitizing text input with db_escape was not
		// consequent enough. To avoid comparision problems we have to 
		// fix this now.
		sanitize_database($pref);

		if ($this->beta)	// nothing more to be done on upgrade from 2.2beta
			return true;

		// set item category dflt accounts to values from company GL setup
		$prefs = get_company_prefs();
		$sql = "UPDATE ".TB_PREF."stock_category SET "
			."dflt_sales_act = '" . $prefs['default_inv_sales_act'] . "',"
			."dflt_cogs_act = '". $prefs['default_cogs_act'] . "',"
			."dflt_inventory_act = '" . $prefs['default_inventory_act'] . "',"
			."dflt_adjustment_act = '" . $prefs['default_adj_act'] . "',"
			."dflt_assembly_act = '" . $prefs['default_assembly_act']."'";
		if (db_query($sql)==false) {
			display_error("Cannot update category default GL accounts"
			.':<br>'. db_error_msg($db));
			return false;
		}
		// add all references to refs table for easy searching via journal interface
		foreach($systypes_array as $typeno => $typename) {
			$info = get_systype_db_info($typeno);
			if ($info == null || $info[3] == null) continue;
 			$tbl = $info[0];
			$sql = "SELECT DISTINCT {$info[2]} as id,{$info[3]} as ref FROM $tbl";
			if ($info[1])
				$sql .= " WHERE {$info[1]}=$typeno";
			$result = db_query($sql);
			if (db_num_rows($result)) {
				while ($row = db_fetch($result)) {
					$res2 = db_query("INSERT INTO ".TB_PREF."refs VALUES("
						. $row['id'].",".$typeno.",'".$row['ref']."')");
					if (!$res2) {
						display_error(_("Cannot copy references from $tbl")
							.':<br>'. db_error_msg($db));
						return false;
					}
				}
			}
		}

		if (!($ret = db_query("SELECT MAX(`order_no`) FROM `".TB_PREF."sales_orders`")) ||
			!db_num_rows($ret))
		{
				display_error(_('Cannot query max sales order number.'));
				return false;
		} 
		$row = db_fetch($ret);
		$max_order = $row[0];
		$next_ref = $max_order+1;
		$sql = "UPDATE `".TB_PREF."sys_types` 
			SET `type_no`='$max_order',`next_reference`='$next_ref'
			WHERE `type_id`=30";
		if(!db_query($sql))
		{
			display_error(_('Cannot store next sales order reference.'));
			return false;
		}
		return convert_roles($pref);
	}
	//
	//	Checking before install
	//
	function prepare()
	{
		global $security_groups;

		if ($this->beta)
			$this->sql = 'alter2.2rc.sql';
		// return ok when security groups still defined (upgrade from 2.1)
		// or usersonline not defined (upgrade from 2.2 beta)
		$pref = $this->companies[$company]['tbpref'];

		return isset($security_groups) || (check_table($pref, 'usersonline')!=0);
	}
};

/*
	Conversion of old security roles stored into $security_groups table
*/
function convert_roles($pref) 
{
		global $security_groups, $security_headings, $security_areas, $path_to_root;
		include_once($path_to_root."/includes/access_levels.inc");

	$trans_sec = array(
		1 => array('SA_CHGPASSWD', 'SA_SETUPDISPLAY', 'SA_BANKTRANSVIEW',
			'SA_ITEMSTRANSVIEW','SA_SUPPTRANSVIEW', 'SA_SALESORDER',
			'SA_SALESALLOC', 'SA_SALESTRANSVIEW'),
		2 => array('SA_DIMTRANSVIEW', 'SA_STANDARDCOST', 'SA_ITEMSTRANSVIEW',
			'SA_ITEMSSTATVIEW', 'SA_SALESPRICE', 'SA_MANUFTRANSVIEW',
			'SA_WORKORDERANALYTIC', 'SA_WORKORDERCOST', 'SA_SUPPTRANSVIEW',
			'SA_SUPPLIERALLOC', 'SA_STEMPLATE', 'SA_SALESTRANSVIEW',
			'SA_SALESINVOICE', 'SA_SALESDELIVERY', 'SA_CUSTPAYMREP',
			'SA_CUSTBULKREP', 'SA_PRICEREP', 'SA_SALESBULKREP', 'SA_SALESMANREP',
			'SA_SALESBULKREP', 'SA_CUSTSTATREP', 'SA_SUPPLIERANALYTIC',
			'SA_SUPPPAYMREP', 'SA_SUPPBULKREP', 'SA_ITEMSVALREP', 'SA_ITEMSANALYTIC',
			'SA_BOMREP', 'SA_MANUFBULKREP', 'SA_DIMENSIONREP', 'SA_BANKREP', 'SA_GLREP',
			'SA_GLANALYTIC', 'SA_TAXREP', 'SA_SALESANALYTIC', 'SA_SALESQUOTE'),
		3 => array('SA_GLACCOUNTGROUP', 'SA_GLACCOUNTCLASS','SA_PAYMENT', 
			'SA_DEPOSIT', 'SA_JOURNALENTRY', 'SA_INVENTORYMOVETYPE',
			'SA_LOCATIONTRANSFER', 'SA_INVENTORYADJUSTMENT', 'SA_WORKCENTRES',
			'SA_MANUFISSUE', 'SA_SUPPLIERALLOC', 'SA_CUSTOMER', 'SA_CRSTATUS',
			'SA_SALESMAN', 'SA_SALESAREA', 'SA_SALESALLOC', 'SA_SALESCREDITINV',
			'SA_SALESPAYMNT', 'SA_SALESCREDIT', 'SA_SALESGROUP', 'SA_SRECURRENT',
			'SA_TAXRATES', 'SA_ITEMTAXTYPE', 'SA_TAXGROUPS', 'SA_QUICKENTRY'),
		4 => array('SA_REORDER', 'SA_PURCHASEPRICING', 'SA_PURCHASEORDER'),
		5 => array('SA_VIEWPRINTTRANSACTION', 'SA_BANKTRANSFER', 'SA_SUPPLIER',
			'SA_SUPPLIERINVOICE', 'SA_SUPPLIERPAYMNT', 'SA_SUPPLIERCREDIT'),
		8 => array('SA_ATTACHDOCUMENT', 'SA_RECONCILE',	'SA_GLANALYTIC',
			'SA_TAXREP', 'SA_BANKTRANSVIEW', 'SA_GLTRANSVIEW'),
		9 => array('SA_FISCALYEARS', 'SA_CURRENCY', 'SA_EXCHANGERATE', 
			'SA_BOM'),
		10 => array('SA_PAYTERMS', 'SA_GLSETUP', 'SA_SETUPCOMPANY',
			'SA_FORMSETUP', 'SA_DIMTRANSVIEW', 'SA_DIMENSION', 'SA_BANKACCOUNT',
			'SA_GLACCOUNT', 'SA_BUDGETENTRY', 'SA_MANUFRECEIVE',
			'SA_MANUFRELEASE', 'SA_WORKORDERENTRY', 'SA_MANUFTRANSVIEW',
			'SA_WORKORDERCOST'),
		11 => array('SA_ITEMCATEGORY', 'SA_ITEM', 'SA_UOM', 'SA_INVENTORYLOCATION',
			 'SA_GRN', 'SA_FORITEMCODE', 'SA_SALESKIT'),
		14 => array('SA_SHIPPING', 'SA_VOIDTRANSACTION', 'SA_SALESTYPES'),
		15 => array('SA_PRINTERS', 'SA_PRINTPROFILE', 'SA_BACKUP', 'SA_USERS',
			'SA_POSSETUP'),
		20 => array('SA_CREATECOMPANY', 'SA_CREATELANGUAGE', 'SA_CREATEMODULES',
			'SA_SOFTWAREUPGRADE', 'SA_SECROLES', 'SA_DIMTAGS', 'SA_GLACCOUNTTAGS')
		);
		$new_ids = array();
		foreach ($security_groups as $role_id => $areas) {
			$area_set = array();
			$sections = array();
			foreach ($areas as $a) {
			 if (isset($trans_sec[$a]))
				foreach ($trans_sec[$a] as $id) {
				 if ($security_areas[$id][0] != 0)
//				 	error_log('invalid area id: '.$a.':'.$id);
					$area_set[] = $security_areas[$id][0];
					$sections[$security_areas[$id][0]&~0xff] = 1;
				}
			}
			$sections  = array_keys($sections);
			sort($sections); sort($area_set);
			import_security_role($security_headings[$role_id], $sections, $area_set);
			$new_ids[$role_id] = db_insert_id();
		}
		$result = get_users(true);
		$users = array();
		while($row = db_fetch($result)) { // complete old user ids and roles
			$users[$row['role_id']][] = $row['id'];
		}
		foreach($users as $old_id => $uids)
			foreach( $uids as $id) {
				$sql = "UPDATE ".TB_PREF."users set role_id=".$new_ids[$old_id].
					" WHERE id=$id";
				$ret = db_query($sql, 'cannot update users roles');
				if(!$ret) return false;
			}
		return true;
}

function import_security_role($name, $sections, $areas)
{
	$sql = "INSERT INTO ".TB_PREF."security_roles (role, description, sections, areas)
	VALUES (".db_escape('FA 2.1 '.$name).",".db_escape($name).","
	.db_escape(implode(';',$sections)).",".db_escape(implode(';',$areas)).")";

	db_query($sql, "could not add new security role");
}

/*
	Changes in extensions system.
	This function is executed once on first Upgrade System display.
*/
function fix_extensions() {
	global $path_to_root, $db_connections;

	if (!file_exists($path_to_root.'/modules/installed_modules.php'))
		return true; // already converted
	
	if (!is_writable($path_to_root.'/modules/installed_modules.php')) {
		display_error(_('Cannot upgrade extensions system: file /modules/installed_modules.php is not writeable'));
		return false;
	}
	
	$exts = array();
	include($path_to_root.'/installed_extensions.php');
	foreach($installed_extensions as $ext) {
		$ext['filename'] = $ext['app_file']; unset($ext['app_file']);
		$ext['tab'] = $ext['name'];
		$ext['name'] = access_string($ext['title'], true); 
		$ext['path'] = $ext['folder']; unset($ext['folder']);
		$ext['type'] = 'extension';
		$ext['active'] = '1';
		$exts[] = $ext;
	}

	if (!write_extensions($exts))
		return false;
	
	$cnt = count($db_connections);
	for ($i = 0; $i < $cnt; $i++)
		write_extensions($exts, $i);

	unlink($path_to_root.'/modules/installed_modules.php');
	return true;
}

/*
	Find and update all database records with special chars in text fields 
	to ensure all of them are changed to html entites.
*/
function sanitize_database($pref, $test = false) {

 	 if ($test)
 	 	error_log('Sanitizing database ...');

	 $tsql = "SHOW TABLES LIKE '".($pref=='' ? '' : substr($pref,0,-1).'\\_')."%'";
	 $tresult = db_query($tsql, "Cannot select all tables with prefix '$pref'");
	 while($tbl = db_fetch($tresult)) {
		$table = $tbl[0];
		$csql = "SHOW COLUMNS FROM $table";
		$cresult = db_query($csql, "Cannot select column names for table '$table'");
		$textcols = $keys = array();
		while($col = db_fetch($cresult)) {
			if (strpos($col['Type'], 'char')!==false 
					|| strpos($col['Type'], 'text')!==false)
				$textcols[] = '`'.$col['Field'].'`';
			if ($col['Key'] == 'PRI') {
				$keys[] = '`'.$col['Field'].'`';
			}
		}

 		if (empty($keys)) { // comments table have no primary key, so give up
 			continue;
 		}
	 	if ($test)
		 	error_log("Table $table (".implode(',',$keys)."):(".implode(',',$textcols)."):");

		if (!count($textcols)) continue;

		// fetch all records containing special characters in text fields
		$sql = "SELECT ".implode(',', array_unique(array_merge($keys,$textcols)))
			." FROM {$table} WHERE 
			CONCAT(".implode(',', $textcols).") REGEXP '[\\'\"><&]'";
		$result = db_query($sql, "Cannot select all suspicious fields in $table");

		// and fix them
		while($rec= db_fetch($result)) {
			$sql = "UPDATE {$table} SET ";
			$val = $key = array();
			foreach ($textcols as $f) {
				$val[] = $f.'='.db_escape($rec[substr($f,1,-1)]);
			}
			$sql .= implode(',', $val). ' WHERE ';
			foreach ($keys as $k) {
				$key[] = $k.'=\''.$rec[substr($k,1,-1)].'\'';
			}
			$sql .= implode( ' AND ', $key);
		 	if ($test)
				error_log("\t(".implode(',',$val).") updated");
			else
				db_query($sql, 'cannot update record');
		}
	}
 	 if ($test)
 	 	error_log('Sanitizing done.');
}

$install = new fa2_2;
