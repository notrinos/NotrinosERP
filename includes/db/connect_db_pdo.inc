<?php
/**********************************************************************
	Copyright (C) NotrinosERP.
	Released under the terms of the GNU General Public License, GPL,
	as published by the Free Software Foundation, either version 3
	of the License, or (at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
	See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/

if (!defined('DB_DUPLICATE_ERROR'))
	define('DB_DUPLICATE_ERROR', 1062);

if (!defined('SQL_MODE'))
	define('SQL_MODE', 'STRICT_ALL_TABLES'); // prevents SQL injection with silent field content truncation

$db_last_inserted_id = 0;
$db_last_affected_rows = 0;
$db_last_error_no = 0;
$db_last_error_msg = '';
$db_last_sqlstate = '';

/**
 * Buffered PDO result wrapper used to emulate mysqli result semantics.
 */
class notrinos_pdo_buffered_result {
	/** @var array */
	var $rows = array();

	/** @var int */
	var $position = 0;

	/** @var array */
	var $column_meta = array();

	/**
	 * Create a new buffered PDO result wrapper.
	 *
	 * @param array $rows Result rows stored as numeric arrays.
	 * @param array $column_meta Result metadata in column order.
	 */
	function __construct($rows = array(), $column_meta = array()) {
		$this->rows = $rows;
		$this->column_meta = $column_meta;
	}
}

/**
 * Reset the last captured driver error details.
 */
function db_pdo_reset_error_state() {
	global $db_last_error_no, $db_last_error_msg, $db_last_sqlstate;

	$db_last_error_no = 0;
	$db_last_error_msg = '';
	$db_last_sqlstate = '';
}

/**
 * Persist the last captured driver error details.
 *
 * @param string $sqlstate SQLSTATE value reported by PDO.
 * @param int $driver_error_no Driver-specific error code.
 * @param string $driver_error_msg Driver-specific error message.
 */
function db_pdo_store_error_state($sqlstate, $driver_error_no, $driver_error_msg) {
	global $db_last_error_no, $db_last_error_msg, $db_last_sqlstate;

	$db_last_sqlstate = $sqlstate;
	$db_last_error_no = (int)$driver_error_no;
	$db_last_error_msg = $driver_error_msg;
}

/**
 * Extract driver error details from a PDO exception.
 *
 * @param PDOException $exception Driver exception.
 *
 * @return array Array containing sqlstate, driver error code and message.
 */
function db_pdo_extract_error_state($exception) {
	$sqlstate = $exception->getCode();
	$driver_error_no = 0;
	$driver_error_msg = $exception->getMessage();

	if (is_array($exception->errorInfo)) {
		if (isset($exception->errorInfo[0]))
			$sqlstate = $exception->errorInfo[0];
		if (isset($exception->errorInfo[1]))
			$driver_error_no = (int)$exception->errorInfo[1];
		if (isset($exception->errorInfo[2]))
			$driver_error_msg = $exception->errorInfo[2];
	}

	return array($sqlstate, $driver_error_no, $driver_error_msg);
}

/**
 * Return the active PDO connection or a provided override.
 *
 * @param PDO|null $dbase Optional connection override.
 *
 * @return PDO|null Active PDO connection.
 */
function db_pdo_get_connection($dbase = null) {
	global $db;

	if ($dbase instanceof PDO)
		return $dbase;

	return ($db instanceof PDO) ? $db : null;
}

/**
 * Build a PDO DSN string from a company connection definition.
 *
 * @param array $connection Company connection settings.
 * @param bool $include_database Whether dbname should be included in the DSN.
 *
 * @return string PDO DSN string.
 */
function db_pdo_build_dsn($connection, $include_database = true) {
	$dsn = 'mysql:host=' . $connection['host'];

	if (!empty($connection['port']))
		$dsn .= ';port=' . $connection['port'];

	if ($include_database && !empty($connection['dbname']))
		$dsn .= ';dbname=' . $connection['dbname'];

	return $dsn;
}

/**
 * Create a PDO connection for the given company settings.
 *
 * @param array $connection Company connection settings.
 * @param bool $include_database Whether to connect directly to the target database.
 *
 * @return PDO|int PDO instance on success, 0 on failure.
 */
function db_pdo_create_connection($connection, $include_database = true) {
	$db_pdo_options = array(
		PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
		PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
		PDO::ATTR_EMULATE_PREPARES => false,
	);

	if (defined('PDO::MYSQL_ATTR_USE_BUFFERED_QUERY'))
		$db_pdo_options[PDO::MYSQL_ATTR_USE_BUFFERED_QUERY] = true;

	try {
		$dbase = new PDO(
			db_pdo_build_dsn($connection, $include_database),
			$connection['dbuser'],
			$connection['dbpassword'],
			$db_pdo_options
		);
		db_pdo_reset_error_state();
		return $dbase;
	} catch (PDOException $exception) {
		$error_state = db_pdo_extract_error_state($exception);
		db_pdo_store_error_state($error_state[0], $error_state[1], $error_state[2]);
		return 0;
	}
}

/**
 * Determine whether the driver error represents a deadlock.
 *
 * @param int $driver_error_no Driver-specific error code.
 *
 * @return bool True if the error is a deadlock.
 */
function db_pdo_is_deadlock($driver_error_no) {
	return ((int)$driver_error_no === 1213);
}

/**
 * Determine whether the driver error represents an active unbuffered result.
 *
 * @param int $driver_error_no Driver-specific error code.
 *
 * @return bool True when MySQL reports active unbuffered queries.
 */
function db_pdo_has_active_result_error($driver_error_no) {
	return ((int)$driver_error_no === 2014);
}

/**
 * Execute transaction control statements using PDO native methods.
 *
 * @param PDO $db_connection Active PDO connection.
 * @param string $sql SQL statement.
 *
 * @return bool True on success, false on failure.
 */
function db_pdo_execute_transaction_sql($db_connection, $sql) {
	$normalized_sql = strtoupper(trim($sql));

	try {
		switch ($normalized_sql) {
			case 'BEGIN':
				return $db_connection->beginTransaction();
			case 'COMMIT':
				return $db_connection->commit();
			case 'ROLLBACK':
				return $db_connection->rollBack();
		}
	} catch (PDOException $exception) {
		$error_state = db_pdo_extract_error_state($exception);
		db_pdo_store_error_state($error_state[0], $error_state[1], $error_state[2]);
	}

	return false;
}

/**
 * Buffer a row-returning PDO statement into a mysqli-like result wrapper.
 *
 * @param PDOStatement $statement Statement to buffer.
 *
 * @return notrinos_pdo_buffered_result Buffered result object.
 */
function db_pdo_buffer_result($statement) {
	$column_count = $statement->columnCount();
	$column_meta = array();
	$column_index = 0;

	while ($column_index < $column_count) {
		$meta = $statement->getColumnMeta($column_index);
		if (!is_array($meta))
			$meta = array();
		if (!isset($meta['name']))
			$meta['name'] = $column_index;
		$column_meta[] = $meta;
		$column_index++;
	}

	$rows = $statement->fetchAll(PDO::FETCH_NUM);
	$statement->closeCursor();

	return new notrinos_pdo_buffered_result($rows, $column_meta);
}

/**
 * Fetch the next numeric row from a buffered PDO result.
 *
 * @param notrinos_pdo_buffered_result $result Buffered result object.
 *
 * @return array|bool Numeric row array or false if no rows remain.
 */
function db_pdo_next_numeric_row($result) {
	if (!($result instanceof notrinos_pdo_buffered_result))
		return false;

	if ($result->position >= count($result->rows))
		return false;

	$row = $result->rows[$result->position];
	$result->position++;

	return $row;
}

/**
 * Convert a numeric row into an associative row using buffered metadata.
 *
 * @param notrinos_pdo_buffered_result $result Buffered result object.
 * @param array $row Numeric row data.
 *
 * @return array Associative row data.
 */
function db_pdo_numeric_row_to_assoc($result, $row) {
	$assoc_row = array();
	$column_count = count($result->column_meta);
	$column_index = 0;

	while ($column_index < $column_count) {
		$column_name = $result->column_meta[$column_index]['name'];
		$assoc_row[$column_name] = isset($row[$column_index]) ? $row[$column_index] : null;
		$column_index++;
	}

	return $assoc_row;
}

/**
 * Convert a numeric row into the combined numeric and associative format returned by mysqli_fetch_array.
 *
 * @param notrinos_pdo_buffered_result $result Buffered result object.
 * @param array $row Numeric row data.
 *
 * @return array Combined numeric and associative row data.
 */
function db_pdo_numeric_row_to_both($result, $row) {
	$combined_row = array();
	$column_count = count($result->column_meta);
	$column_index = 0;

	while ($column_index < $column_count) {
		$column_name = $result->column_meta[$column_index]['name'];
		$column_value = isset($row[$column_index]) ? $row[$column_index] : null;
		$combined_row[$column_index] = $column_value;
		$combined_row[$column_name] = $column_value;
		$column_index++;
	}

	return $combined_row;
}

/**
 * Return the last insert id in a mysqli-compatible form.
 *
 * @param PDO|null $dbase Optional connection override.
 *
 * @return int|string Last insert id or 0 when not available.
 */
function db_pdo_last_insert_id($dbase = null) {
	$db_connection = db_pdo_get_connection($dbase);

	if (!$db_connection)
		return 0;

	try {
		$last_insert_id = $db_connection->lastInsertId();
		if ($last_insert_id === false || $last_insert_id === '')
			return 0;
		return $last_insert_id;
	} catch (PDOException $exception) {
		return 0;
	}
}

/**
 * Escape a string using PDO::quote and return it without surrounding quotes.
 *
 * @param string $string Raw string value.
 *
 * @return string Escaped string without surrounding quotes.
 */
function db_pdo_escape_string($string) {
	$db_connection = db_pdo_get_connection();

	if (!$db_connection)
		return addslashes($string);

	$quoted_string = $db_connection->quote($string);
	if ($quoted_string === false)
		return addslashes($string);

	return substr($quoted_string, 1, strlen($quoted_string) - 2);
}

/**
 * Set the SQL mode expected by the application.
 *
 * @param PDO|null $dbase Optional connection override.
 *
 * @return bool True on success, false on failure.
 */
function db_pdo_apply_sql_mode($dbase = null) {
	$db_connection = db_pdo_get_connection($dbase);

	if (!$db_connection)
		return false;

	try {
		$db_connection->exec("SET sql_mode = '" . SQL_MODE . "'");
		return true;
	} catch (PDOException $exception) {
		$error_state = db_pdo_extract_error_state($exception);
		db_pdo_store_error_state($error_state[0], $error_state[1], $error_state[2]);
		return false;
	}
}

/**
 * Set the current client character set using a MySQL-compatible encoding token.
 *
 * @param PDO|null $dbase Optional connection override.
 * @param string|null $ui_encoding Optional UI encoding to translate to MySQL charset.
 *
 * @return bool True on success, false on failure.
 */
function db_pdo_apply_charset($dbase = null, $ui_encoding = null) {
	$db_connection = db_pdo_get_connection($dbase);

	if (!$db_connection)
		return false;

	if (!isset($ui_encoding)) {
		global $dflt_lang, $installed_languages;
		$lang = array_search_value($dflt_lang, $installed_languages, 'code');
		$ui_encoding = strtoupper($lang['encoding']);
	}

	$mysql_encoding = get_mysql_encoding_name($ui_encoding);
	if (!$mysql_encoding)
		return false;

	try {
		$db_connection->exec('SET NAMES ' . $mysql_encoding);
		return true;
	} catch (PDOException $exception) {
		$error_state = db_pdo_extract_error_state($exception);
		db_pdo_store_error_state($error_state[0], $error_state[1], $error_state[2]);
		return false;
	}
}

/**
 * Open the active company database connection.
 *
 * @param int $company Company index, or -1 to use the current user company.
 *
 * @return PDO|int Active PDO connection on success, 0 on failure.
 */
function set_global_connection($company = -1) {
	global $db, $path_to_root, $db_connections, $SysPrefs;

	include($path_to_root . '/config_db.php');

	if ($company == -1)
		$company = user_company();

	cancel_transaction(); // cancel all aborted transactions (if any)

	$_SESSION['wa_current_user']->cur_con = $company;
	$connection = $db_connections[$company];
	$db = db_pdo_create_connection($connection, true);

	if (!$db)
		return 0;

	db_set_encoding();
	db_pdo_apply_sql_mode($db);
	$SysPrefs->refresh();

	return $db;
}

/**
 * Execute a SQL statement through PDO while preserving the legacy db_query contract.
 *
 * @param string $sql SQL statement.
 * @param string|null $err_msg Optional error context.
 *
 * @return notrinos_pdo_buffered_result|bool Buffered result or boolean status.
 */
function db_query($sql, $err_msg = null) {
	global $db, $SysPrefs, $sql_queries, $Ajax, $db_connections;
	global $db_last_inserted_id, $db_last_affected_rows;

	if (!($db instanceof PDO)) {
		db_pdo_store_error_state('HY000', 2000, 'No active PDO connection');
		return false;
	}

	$comp = isset($_SESSION['wa_current_user']->cur_con) ? $_SESSION['wa_current_user']->cur_con : 0;
	$cur_prefix = (string)@$db_connections[$comp]['tbpref'];
	$sql = str_replace(TB_PREF, $cur_prefix, $sql);

	if ($SysPrefs->show_sql) {
		$Ajax->activate('footer_debug');
		$sql_queries .= "<pre>$sql</pre>\n<hr>";
	}

	$normalized_sql = strtoupper(trim($sql));

	db_profile(); // mysql profiling
	db_pdo_reset_error_state();
	$db_last_affected_rows = 0;
	$result = false;
	$retry = MAX_DEADLOCK_RETRY;
	if ($normalized_sql === 'BEGIN' || $normalized_sql === 'COMMIT' || $normalized_sql === 'ROLLBACK') {
		$result = db_pdo_execute_transaction_sql($db, $normalized_sql);

		if (!$result && $normalized_sql === 'BEGIN' && db_pdo_has_active_result_error($db_last_error_no)) {
			$reconnected = set_global_connection($comp);
			if ($reconnected instanceof PDO)
				$result = db_pdo_execute_transaction_sql($reconnected, $normalized_sql);
		}
		$retry = 0;
	} else do {

		try {
			$statement = $db->query($sql);
			if ($statement instanceof PDOStatement && $statement->columnCount() > 0) {
				$result = db_pdo_buffer_result($statement);
				$db_last_affected_rows = 0;
			} else {
				$result = true;
				$db_last_affected_rows = ($statement instanceof PDOStatement) ? $statement->rowCount() : 0;
				if ($statement instanceof PDOStatement)
					$statement->closeCursor();
			}
			$statement = null;
			$retry = 0;
		} catch (PDOException $exception) {
			$error_state = db_pdo_extract_error_state($exception);
			db_pdo_store_error_state($error_state[0], $error_state[1], $error_state[2]);
			if (db_pdo_is_deadlock($error_state[1]) && $retry > 1) {
				sleep(1);
				$retry--;
			} else {
				$result = false;
				$retry = 0;
			}
		}
	} while ($retry);

	db_profile($sql);

	if ($SysPrefs->sql_trail) {
		$db_last_inserted_id = db_pdo_last_insert_id($db);
		if ($SysPrefs->select_trail || (strstr($sql, 'SELECT') === false)) {
			try {
				$db->exec("INSERT INTO " . $cur_prefix . "sql_trail
					(`sql`, `result`, `msg`)
					VALUES(" . db_escape($sql) . "," . ($result ? 1 : 0) . ",
					" . db_escape($err_msg) . ")");
			} catch (PDOException $trail_exception) {
				// keep legacy behavior: ignore sql_trail write failures to avoid masking original query results
			}
		}
	}

	if ($err_msg != null || $SysPrefs->go_debug) {
		$exit = $err_msg != null;
		if (function_exists('xdebug_call_file'))
			$err_msg = '<br>At file ' . xdebug_call_file() . ':' . xdebug_call_line() . ':<br>' . $err_msg;
		check_db_error($err_msg, $sql, $exit);
	}

	return $result;
}

/**
 * Fetch the next row as a numeric array.
 *
 * @param notrinos_pdo_buffered_result $result Buffered result object.
 *
 * @return array|bool Numeric row or false when no rows remain.
 */
function db_fetch_row($result) {
	return db_pdo_next_numeric_row($result);
}

/**
 * Fetch the next row as an associative array.
 *
 * @param notrinos_pdo_buffered_result $result Buffered result object.
 *
 * @return array|bool Associative row or false when no rows remain.
 */
function db_fetch_assoc($result) {
	$row = db_pdo_next_numeric_row($result);
	if ($row === false)
		return false;

	return db_pdo_numeric_row_to_assoc($result, $row);
}

/**
 * Fetch the next row with both numeric and associative indexes.
 *
 * @param notrinos_pdo_buffered_result $result Buffered result object.
 *
 * @return array|bool Combined row or false when no rows remain.
 */
function db_fetch($result) {
	$row = db_pdo_next_numeric_row($result);
	if ($row === false)
		return false;

	return db_pdo_numeric_row_to_both($result, $row);
}

/**
 * Seek to a buffered result row index.
 *
 * @param notrinos_pdo_buffered_result $result Buffered result object.
 * @param int $record Target row index.
 *
 * @return bool True on success, false on failure.
 */
function db_seek(&$result, $record) {
	if (!($result instanceof notrinos_pdo_buffered_result))
		return false;

	if ($record < 0 || $record >= count($result->rows))
		return false;

	$result->position = $record;
	return true;
}

/**
 * Release buffered result memory.
 *
 * @param notrinos_pdo_buffered_result $result Buffered result object.
 */
function db_free_result($result) {
	if ($result instanceof notrinos_pdo_buffered_result) {
		$result->rows = array();
		$result->column_meta = array();
		$result->position = 0;
	}
}

/**
 * Return the number of buffered rows.
 *
 * @param notrinos_pdo_buffered_result $result Buffered result object.
 *
 * @return int Number of rows in the result.
 */
function db_num_rows($result) {
	if (!($result instanceof notrinos_pdo_buffered_result))
		return 0;

	return count($result->rows);
}

/**
 * Return the number of columns in the buffered result.
 *
 * @param notrinos_pdo_buffered_result $result Buffered result object.
 *
 * @return int Number of columns in the result.
 */
function db_num_fields($result) {
	if (!($result instanceof notrinos_pdo_buffered_result))
		return 0;

	return count($result->column_meta);
}

/**
 * Escape a value for legacy SQL string construction.
 *
 * @param mixed $value Value to escape.
 * @param bool $nullify Whether empty values should become SQL NULL.
 *
 * @return string Escaped SQL literal.
 */
function db_escape($value = '', $nullify = false) {
	$value = @html_entity_decode(
		$value,
		ENT_QUOTES,
		$_SESSION['language']->encoding == 'iso-8859-2' ? 'ISO-8859-1' : $_SESSION['language']->encoding
	);
	$value = html_specials_encode($value);

	$nullify = ($nullify === null) ? false : $nullify;

	if ((!isset($value)) || is_null($value) || ($value === ''))
		$value = $nullify ? 'NULL' : "''";
	else {
		if (is_string($value))
			$value = "'" . db_pdo_escape_string($value) . "'";
		else if (!is_numeric($value)) {
			display_error('ERROR: incorrect data type send to sql query');
			echo '<br><br>';
			exit();
		}
	}

	return $value;
}

/**
 * Return the last driver error code.
 *
 * @return int Driver-specific error code.
 */
function db_error_no() {
	global $db_last_error_no;

	return $db_last_error_no;
}

/**
 * Return the last driver error message.
 *
 * @param PDO|null $conn Optional connection parameter kept for compatibility.
 *
 * @return string Driver error message.
 */
function db_error_msg($conn) {
	global $db_last_error_msg;

	return $db_last_error_msg;
}

/**
 * Return the last insert id while preserving sql_trail behavior.
 *
 * @return int|string Last insert id or 0 when not available.
 */
function db_insert_id() {
	global $db_last_inserted_id, $SysPrefs, $db;

	return $SysPrefs->sql_trail ? $db_last_inserted_id : db_pdo_last_insert_id($db);
}

/**
 * Return the number of affected rows from the last non-select statement.
 *
 * @return int Number of affected rows.
 */
function db_num_affected_rows() {
	global $db_last_affected_rows;

	return (int)$db_last_affected_rows;
}

/**
 * Return a result column name by zero-based index.
 *
 * @param notrinos_pdo_buffered_result $result Buffered result object.
 * @param int $n Column index.
 *
 * @return string|null Column name.
 */
function db_field_name($result, $n) {
	if (!($result instanceof notrinos_pdo_buffered_result))
		return null;

	if (!isset($result->column_meta[$n]))
		return null;

	return $result->column_meta[$n]['name'];
}

/**
 * Set the current database collation.
 *
 * @param PDO|null $dbase Optional connection override.
 * @param string $fa_collation FrontAccounting collation key.
 *
 * @return bool True on success, false on failure.
 */
function db_set_collation($dbase, $fa_collation) {
	$db_connection = db_pdo_get_connection($dbase);

	if (!$db_connection)
		return false;

	try {
		$db_connection->exec('ALTER DATABASE COLLATE ' . get_mysql_collation($fa_collation));
		return true;
	} catch (PDOException $exception) {
		$error_state = db_pdo_extract_error_state($exception);
		db_pdo_store_error_state($error_state[0], $error_state[1], $error_state[2]);
		return false;
	}
}

/*
	Create database for FA company. If database already exists,
	just set collation to be sure nothing weird will happen later.
*/
function db_create_db($connection) {
	global $db;

	$server_connection = db_pdo_create_connection($connection, false);
	if (!$server_connection)
		return 0;

	try {
		$server_connection->exec(
			"CREATE DATABASE IF NOT EXISTS `" . $connection['dbname'] . "` DEFAULT COLLATE '" . get_mysql_collation($connection['collation']) . "'"
		);
	} catch (PDOException $exception) {
		$error_state = db_pdo_extract_error_state($exception);
		db_pdo_store_error_state($error_state[0], $error_state[1], $error_state[2]);
		return 0;
	}

	$db = db_pdo_create_connection($connection, true);
	if (!$db)
		return 0;

	if (!db_set_collation($db, $connection['collation']))
		return 0;

	return $db;
}

/**
 * Drop the company database or prefixed company tables.
 *
 * @param array $connection Company connection settings.
 *
 * @return bool True on success, false on failure.
 */
function db_drop_db($connection) {
	if ($connection['tbpref'] == '') {
		$db_connection = db_pdo_get_connection();
		if (!$db_connection)
			return false;

		try {
			$db_connection->exec('DROP DATABASE IF EXISTS `' . $connection['dbname'] . '`');
			return true;
		} catch (PDOException $exception) {
			$error_state = db_pdo_extract_error_state($exception);
			db_pdo_store_error_state($error_state[0], $error_state[1], $error_state[2]);
			return false;
		}
	}

	$res = db_query('show table status');
	$all_tables = array();
	while ($row = db_fetch($res))
		$all_tables[] = $row;

	foreach ($all_tables as $table) {
		if (strpos($table['Name'], $connection['tbpref']) === 0)
			db_query("DROP TABLE `" . $table['Name'] . '`');
	}

	return true;
}

/**
 * Close the active PDO connection.
 *
 * @param PDO|null $dbase Optional connection override.
 *
 * @return bool Always true.
 */
function db_close($dbase = null) {
	global $db;

	if (!$dbase || $dbase === $db)
		$db = null;

	return true;
}

/**
 * Determine whether the PDO MySQL driver is available.
 *
 * @return bool True when PDO MySQL is installed.
 */
function db_extension_exists() {
	if (!class_exists('PDO'))
		return false;

	return in_array('mysql', PDO::getAvailableDrivers());
}

/**
 * Escape a string without surrounding quotes.
 *
 * @param string $string Raw string value.
 *
 * @return string Escaped string.
 */
function db_escape_function($string) {
	return db_pdo_escape_string($string);
}

/*
	Set mysql client encoding.
	Default is is encoding used by default language.
*/
function db_set_encoding($ui_encoding = null) {
	return db_pdo_apply_charset(null, $ui_encoding);
}

/**
 * Return the current connection character set.
 *
 * @param PDO|null $dbase Optional connection override.
 *
 * @return string Current connection character set, or empty string on failure.
 */
function db_get_charset($dbase) {
	$db_connection = db_pdo_get_connection($dbase);

	if (!$db_connection)
		return '';

	try {
		$statement = $db_connection->query('SELECT @@character_set_connection');
		$charset = $statement->fetchColumn();
		$statement->closeCursor();
		return $charset ? $charset : '';
	} catch (PDOException $exception) {
		$error_state = db_pdo_extract_error_state($exception);
		db_pdo_store_error_state($error_state[0], $error_state[1], $error_state[2]);
		return '';
	}
}

/**
 * Set the current connection character set.
 *
 * @param PDO|null $dbase Optional connection override.
 * @param string $charset Target MySQL character set.
 *
 * @return bool True on success, false on failure.
 */
function db_set_charset($dbase, $charset) {
	$db_connection = db_pdo_get_connection($dbase);

	if (!$db_connection)
		return false;

	if (!preg_match('/^[A-Za-z0-9_]+$/', $charset))
		return false;

	try {
		$db_connection->exec('SET NAMES ' . $charset);
		return true;
	} catch (PDOException $exception) {
		$error_state = db_pdo_extract_error_state($exception);
		db_pdo_store_error_state($error_state[0], $error_state[1], $error_state[2]);
		return false;
	}
}

/**
 * Execute a prepared SQL statement with bound parameters.
 *
 * @param string $sql SQL statement containing placeholders.
 * @param array $params Bound parameter values.
 * @param string|null $err_msg Optional error context.
 *
 * @return notrinos_pdo_buffered_result|bool Buffered result or boolean status.
 */
function db_query_params($sql, $params = array(), $err_msg = null) {
	global $db, $SysPrefs, $sql_queries, $Ajax, $db_connections;
	global $db_last_inserted_id, $db_last_affected_rows;

	if (!($db instanceof PDO)) {
		db_pdo_store_error_state('HY000', 2000, 'No active PDO connection');
		return false;
	}

	$comp = isset($_SESSION['wa_current_user']->cur_con) ? $_SESSION['wa_current_user']->cur_con : 0;
	$cur_prefix = (string)@$db_connections[$comp]['tbpref'];
	$sql = str_replace(TB_PREF, $cur_prefix, $sql);

	if ($SysPrefs->show_sql) {
		$Ajax->activate('footer_debug');
		$sql_queries .= "<pre>$sql</pre>\n<hr>";
	}

	db_profile();
	db_pdo_reset_error_state();
	$db_last_affected_rows = 0;
	$result = false;
	$retry = MAX_DEADLOCK_RETRY;

	do {
		try {
			$statement = $db->prepare($sql);
			$statement->execute($params);
			if ($statement->columnCount() > 0) {
				$result = db_pdo_buffer_result($statement);
				$db_last_affected_rows = 0;
			} else {
				$result = true;
				$db_last_affected_rows = $statement->rowCount();
				$statement->closeCursor();
			}
			$statement = null;
			$retry = 0;
		} catch (PDOException $exception) {
			$error_state = db_pdo_extract_error_state($exception);
			db_pdo_store_error_state($error_state[0], $error_state[1], $error_state[2]);
			if (db_pdo_is_deadlock($error_state[1]) && $retry > 1) {
				sleep(1);
				$retry--;
			} else {
				$result = false;
				$retry = 0;
			}
		}
	} while ($retry);

	db_profile($sql);

	if ($SysPrefs->sql_trail) {
		$db_last_inserted_id = db_pdo_last_insert_id($db);
		if ($SysPrefs->select_trail || (strstr($sql, 'SELECT') === false)) {
			try {
				$db->exec("INSERT INTO " . $cur_prefix . "sql_trail
					(`sql`, `result`, `msg`)
					VALUES(" . db_escape($sql) . "," . ($result ? 1 : 0) . ",
					" . db_escape($err_msg) . ")");
			} catch (PDOException $trail_exception) {
				// keep legacy behavior: ignore sql_trail write failures to avoid masking original query results
			}
		}
	}

	if ($err_msg != null || $SysPrefs->go_debug) {
		$exit = $err_msg != null;
		if (function_exists('xdebug_call_file'))
			$err_msg = '<br>At file ' . xdebug_call_file() . ':' . xdebug_call_line() . ':<br>' . $err_msg;
		check_db_error($err_msg, $sql, $exit);
	}

	return $result;
}

/**
 * Execute a prepared non-select statement.
 *
 * @param string $sql SQL statement containing placeholders.
 * @param array $params Bound parameter values.
 * @param string|null $err_msg Optional error context.
 *
 * @return bool True on success, false on failure.
 */
function db_execute($sql, $params = array(), $err_msg = null) {
	$result = db_query_params($sql, $params, $err_msg);

	if ($result instanceof notrinos_pdo_buffered_result)
		return true;

	return (bool)$result;
}

/**
 * Fetch all rows from a result set.
 *
 * @param notrinos_pdo_buffered_result $result Buffered result object.
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