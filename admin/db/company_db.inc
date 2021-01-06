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
	Update main or gl company setup.
*/
function update_company_prefs($params) {
	$sql = "UPDATE ".TB_PREF."sys_prefs SET value = ";
	foreach($params as $name => $value) {
		if (!db_query($sql. db_escape($value). " WHERE name=".db_escape($name), 'The company prefferences could not be updated '))
			return false;
		// update cached value
		$_SESSION['SysPrefs']->prefs[$name] = $value;
	}
	return true;
}
/*
	Get company preferences. Returns cached values from global variable SysPrefs
	or retrieved from database if SysPrefs values are not set.
	$prefs can be preference name, array of names, or null for all preferences.
	
*/
function get_company_pref($prefs = null) {
	global $SysPrefs, $db_version;

	if (!isset($SysPrefs->prefs))	 // just after first login or reset
		$SysPrefs->refresh();

	$all = $SysPrefs->prefs;

	if ($prefs && is_string($prefs))
		return @$all[$prefs];

	if (!is_array($all))
		$all = array();

	return $all;
}

function get_company_prefs() {
	return get_company_pref(null);
}

function set_company_pref($pref, $category, $type, $length, $value) {
	$sql = "REPLACE `".TB_PREF."sys_prefs` SET `name`=".db_escape($pref).", `category`=".db_escape($category)
		.", `type`=".db_escape($type).", `length`=".db_escape($length).", `value`=".db_escape($value);
	return db_query($sql, 'cannot set company pref');
}

function get_base_sales_type() {
	return get_company_pref('base_sales');
}

function get_company_extensions($id = -1) {
	global $path_to_root;

	$file = $path_to_root.($id == -1 ? '' : '/company/'.(int)$id).'/installed_extensions.php';
	$installed_extensions = array();
	if (is_file($file))
		include($file);
	
	return $installed_extensions;
}

function add_payment_terms($daysOrFoll, $terms, $dayNumber) {
	if ($daysOrFoll) {
		$sql = "INSERT INTO ".TB_PREF."payment_terms (terms,
			days_before_due, day_in_following_month)
			VALUES (" .
			db_escape($terms) . ", " . db_escape($dayNumber) . ", 0)";
	} 
	else {
		$sql = "INSERT INTO ".TB_PREF."payment_terms (terms,
			days_before_due, day_in_following_month)
			VALUES (" . db_escape($terms) . ",
			0, " . db_escape($dayNumber) . ")";
	}
	db_query($sql, 'The payment term could not be added');
}

function update_payment_terms($selected_id, $daysOrFoll, $terms, $dayNumber) {
	if ($daysOrFoll) {
		$sql = "UPDATE ".TB_PREF."payment_terms SET terms=" . db_escape($terms) . ",
			day_in_following_month=0,
			days_before_due=" . db_escape($dayNumber) . "
			WHERE terms_indicator = " .db_escape($selected_id);
	} 
	else {
		$sql = "UPDATE ".TB_PREF."payment_terms SET terms=" . db_escape($terms) . ",
			day_in_following_month=" . db_escape($dayNumber) . ",
			days_before_due=0
			WHERE terms_indicator = " .db_escape($selected_id);
	}
	db_query($sql, 'The payment term could not be updated');
}

function delete_payment_terms($selected_id) {
	$sql="DELETE FROM ".TB_PREF."payment_terms WHERE terms_indicator=".db_escape($selected_id);
	db_query($sql, 'could not delete a payment terms');
}

function get_payment_terms($selected_id) {
	$sql = "SELECT *, (t.days_before_due=0) AND (t.day_in_following_month=0) as cash_sale
	 FROM ".TB_PREF."payment_terms t WHERE terms_indicator=".db_escape($selected_id);

	$result = db_query($sql, 'could not get payment term');

	return db_fetch($result);
}

function get_payment_terms_all($show_inactive) {
	$sql = "SELECT * FROM ".TB_PREF."payment_terms";
	if (!$show_inactive) $sql .= " WHERE !inactive";
	return db_query($sql, 'could not get payment terms');
}
/*
	Return number of records in tables, where some foreign key $id is used.
	$id - searched key value
	$tables - array of table names (without prefix); when table name is used as a key, then
		value is name of foreign key field. For numeric keys $stdkey field name is used.
	$stdkey - standard name of foreign key.
*/
function key_in_foreign_table($id, $tables, $stdkey) {

	if (!is_array($tables))
		$tables = array($tables);

	$sqls = array();
	foreach ($tables as $tbl => $key) {
		if (is_numeric($tbl)) {
			$tbl = $key;
			$key = $stdkey;
		}
		$sqls[] = "(SELECT COUNT(*) as cnt FROM `".TB_PREF."$tbl` WHERE `$key`=".db_escape($id).")\n";
	}

	$sql = "SELECT sum(cnt) FROM (". implode(' UNION ', $sqls).") as counts";

	$result = db_query($sql, 'check relations for '.implode(',',$tables).' failed');
	$count =  db_fetch($result);

	return $count[0];
}

//---------------------------------------------------------------------------------------------
//
// Resets $theme references in users records to 'default'.
//
function clean_user_themes($theme) {
	global $db_connections, $db;

	$comp = user_company();

	$connections = $db_connections;	// do not use db_connections directly here, or script will hang due to db_connections usage inside the loop
	foreach ($connections as $n => $conn) {
		$db = $_SESSION['wa_current_user']->set_db_connection($n);
		$sql = "UPDATE {$conn['tbpref']}users SET theme='default' WHERE theme='$theme'";
		if (!db_query($sql, 'Cannot update user theme settings'))
			return false;
	}
	$db = $_SESSION['wa_current_user']->set_db_connection($comp);
	$_SESSION['wa_current_user']->prefs->theme = 'default';
	return true;
}