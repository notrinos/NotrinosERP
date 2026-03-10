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
define('DB_DUPLICATE_ERROR', 1062);
define('SQL_MODE', 'STRICT_ALL_TABLES'); // prevents SQL injection with silent field content truncation

function set_global_connection($company=-1) {
	global $db, $path_to_root, $db_connections, $SysPrefs;

	include ($path_to_root . '/config_db.php');
	if ($company == -1) 
		$company = user_company();

	cancel_transaction(); // cancel all aborted transactions if any

	$_SESSION['wa_current_user']->cur_con = $company;

	$connection = $db_connections[$company];
	
	$server = $connection['host'];
	if (!empty($connection['port']))
		$server .= ':'.$connection['port'];

	$db = mysql_connect($server, $connection['dbuser'], $connection['dbpassword']);
	mysql_select_db($connection['dbname'], $db);
	///// From MySql release 5.6.6 the sql_mode is no longer empty as it was prior to
	///// this release. Just for safety we make it empty for all 5.6 release and higher.
	///// This non empty sql_mode values can interphere with NotrinosERP, so all is set empty during
	///// our sessions.
	///// We are, however, investigating the existing code to be compatible in the future.
	db_query("SET sql_mode = '".SQL_MODE."'");
	/////
	$SysPrefs->refresh();
	return $db;
}

//DB wrapper functions to change only once for whole application

function db_query($sql, $err_msg=null) {
	global $db, $SysPrefs, $sql_queries, $Ajax, $db_connections, $db_last_inserted_id;

	// set current db prefix
	$comp = isset($_SESSION["wa_current_user"]->cur_con) ? $_SESSION["wa_current_user"]->cur_con : 0;
	$cur_prefix = @$db_connections[$comp]['tbpref'];
	$sql = str_replace(TB_PREF, $cur_prefix, $sql);

	if ($SysPrefs->show_sql) {
		$Ajax->activate('footer_debug');
		$sql_queries .= "<pre>$sql</pre>\n<hr>";
	}

	db_profile(); // mysql profiling

	$retry = MAX_DEADLOCK_RETRY;
	do {
		$result = mysql_query($db, $sql);
		if (mysql_errno($db) == 1213) { // deadlock detected
			sleep(1);
			$retry--;
		}
		else
			$retry = 0;
	}
	while ($retry);

	db_profile($sql);

	if($SysPrefs->sql_trail) {
		$db_last_inserted_id = mysql_insert_id($db);	// preserve in case trail insert is done
		if ($SysPrefs->select_trail || (strstr($sql, 'SELECT') === false)) {
			mysql_query(
			"INSERT INTO ".$cur_prefix."sql_trail
				(`sql`, `result`, `msg`)
				VALUES(".db_escape($sql).",".($result ? 1 : 0).",
				".db_escape($err_msg).")", $db);
		}
	}

	if ($err_msg != null || $SysPrefs->go_debug) {
		$exit = $err_msg != null;
		if (function_exists('xdebug_call_file'))
			$err_msg = '<br>At file '.xdebug_call_file().':'.xdebug_call_line().':<br>'.$err_msg;
		check_db_error($err_msg, $sql, $exit);
	}
	return $result;
}

function db_fetch_row($result) {

	return mysql_fetch_row($result);
}

function db_fetch_assoc($result) {

	return mysql_fetch_assoc($result);
}

function db_fetch($result) {

	return mysql_fetch_array($result);
}

function db_seek(&$result,$record) {
	return mysql_data_seek($result, $record);
}

function db_free_result($result) {
	if ($result)
		mysql_free_result($result);
}

function db_num_rows($result) {
	return mysql_num_rows($result);
}

function db_num_fields($result) {
	return mysql_num_fields($result);
}

function db_escape($value = '', $nullify = false) {
	$value = @html_entity_decode($value, ENT_QUOTES, $_SESSION['language']->encoding);
	$value = html_specials_encode($value);

	//reset default if second parameter is skipped
	$nullify = ($nullify === null) ? (false) : ($nullify);

	//check for null/unset/empty strings
	if ((!isset($value)) || (is_null($value)) || ($value === ""))
		$value = ($nullify) ? ("NULL") : ("''");
	else {
		if (is_string($value)) {
			//value is a string and should be quoted; determine best method based on available extensions
			if (function_exists('mysql_real_escape_string'))
				$value = "'" . mysql_real_escape_string($value) . "'";
			else
			  $value = "'" . mysql_escape_string($value) . "'";
			
		} else if (!is_numeric($value)) {
			//value is not a string nor numeric
			display_error("ERROR: incorrect data type send to sql query");
			echo '<br><br>';
			exit();
		}
	}
	return $value;
}

function db_error_no() {
	global $db;
	return mysql_errno($db);
}

function db_error_msg($conn) {
	return mysql_error($conn);
}

function db_insert_id() {
	global $db_last_inserted_id, $SysPrefs, $db;

	return $SysPrefs->sql_trail ? $db_last_inserted_id : mysql_insert_id($db);
}

function db_num_affected_rows() {
	global $db;
	return mysql_affected_rows($db);
}

function db_field_name($result, $n) {
	return mysql_field_name($result, $n);
}

function db_set_collation($db, $fa_collation) {
	return mysql_query("ALTER DATABASE COLLATE ".get_mysql_collation($fa_collation), $db);
}

/*
	Create database for FA company. If database already exists,
	just set collation to be sure nothing weird will happen later.
*/
function db_create_db($connection) {
	$server = $connection['host'];
	if (!empty($connection['port']))
		$server .= ':'.$connection['port'];
	$db = mysql_connect($server, $connection['dbuser'], $connection['dbpassword']);

	if (!mysql_select_db($connection['dbname'], $db)) {
		$sql = "CREATE DATABASE IF NOT EXISTS `" . $connection["dbname"] . "`"
			. " DEFAULT COLLATE '" . get_mysql_collation($connection["collation"]) . "'";

		if (!mysql_query($sql) || !mysql_select_db($connection["dbname"], $db))
				return 0;
	}
	else
		if (!db_set_collation($connection["collation"], $db))
			return 0;

	return $db;
}

function db_drop_db($connection) {

	if ($connection['tbpref'] == '') {
		$sql = "DROP DATABASE IF EXISTS " . $connection["dbname"] . "";
		return mysql_query($sql);
	}
	else {
		$res = db_query("show table status");
		$all_tables = array();
		while($row = db_fetch($res))
			$all_tables[] = $row;
		// get table structures
		foreach ($all_tables as $table) {
			if (strpos($table['Name'], $connection["tbpref"]) === 0)
				db_query("DROP TABLE `".$table['Name'] . "`");
		}
		//deleting the tables, how??
		return true;
	}
}

function db_close($dbase = null) {
	global $db;
	
	if (!$dbase)
		$dbase = $db;
	return mysql_close($dbase);
}

function db_extension_exists() {
	return function_exists('mysql_connect');
}

function db_escape_function($string) {
	return (function_exists('mysql_real_escape_string') ? mysql_real_escape_string($string) : mysql_escape_string($string));
}

/*
	Set mysql client encoding.
	Default is is encoding used by default language.
*/
function db_set_encoding($ui_encoding=null) {
	global $dflt_lang, $installed_languages;

	if (!isset($ui_encoding)) {
		$lang = array_search_value($dflt_lang, $installed_languages, 'code');
		$ui_encoding = strtoupper($lang['encoding']);
	}

	if ($mysql_enc = get_mysql_encoding_name($ui_encoding))
		mysql_set_charset($mysql_enc);
}

function db_get_charset($db) {
	return mysql_client_encoding();
}

function db_set_charset($db, $charset) {
	return mysql_set_charset($charset, $db);
}

/**
 * Execute a SQL statement with placeholder substitution for compatibility with PDO-style helpers.
 *
 * @param string $sql SQL statement containing '?' placeholders.
 * @param array $params Ordered parameter values.
 * @param string|null $err_msg Optional error context.
 *
 * @return mixed Query result from db_query.
 */
function db_query_params($sql, $params = array(), $err_msg = null) {
	if (!empty($params)) {
		foreach ($params as $param) {
			$escaped_param = db_escape($param);
			$sql = preg_replace('/\?/', $escaped_param, $sql, 1);
		}
	}

	return db_query($sql, $err_msg);
}

/**
 * Execute a parameterized statement intended for non-select operations.
 *
 * @param string $sql SQL statement containing '?' placeholders.
 * @param array $params Ordered parameter values.
 * @param string|null $err_msg Optional error context.
 *
 * @return bool True on success, false on failure.
 */
function db_execute($sql, $params = array(), $err_msg = null) {
	return (bool)db_query_params($sql, $params, $err_msg);
}

/**
 * Fetch all rows from a result set.
 *
 * @param mixed $result Database result resource.
 * @param string $mode Fetch mode: assoc, row, or both.
 *
 * @return array Result rows.
 */
function db_fetch_all($result, $mode = 'assoc') {
	$rows = array();

	if ($mode == 'row') {
		while ($row = db_fetch_row($result))
			$rows[] = $row;
	}
	else if ($mode == 'both') {
		while ($row = db_fetch($result))
			$rows[] = $row;
	}
	else {
		while ($row = db_fetch_assoc($result))
			$rows[] = $row;
	}

	return $rows;
}