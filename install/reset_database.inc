<?php
/** Identify the exact database settings shown on the installation confirmation. */
function install_database_reset_target($connection) {
	$target = array();
	foreach (array('host', 'port', 'dbuser', 'dbname', 'tbpref', 'collation') as $key)
		$target[$key] = isset($connection[$key]) ? (string)$connection[$key] : '';
	return hash('sha256', serialize($target));
}

/** Require an explicit, CSRF-protected confirmation of the current install target. */
function install_database_reset_confirmed($connection) {
	if (!isset($_SERVER['REQUEST_METHOD']) || $_SERVER['REQUEST_METHOD'] !== 'POST'
		|| !check_csrf_token())
		return false;

	if (!isset($_POST['confirm_database_reset'], $_POST['database_reset_target'], $_POST['database_reset_token'],
		$_SESSION['install_database_reset_token'])
		|| $_POST['confirm_database_reset'] !== '1'
		|| $_POST['database_reset_token'] !== $_SESSION['install_database_reset_token']
		|| $_POST['database_reset_target'] !== install_database_reset_target($connection)) {
		display_error(_('Confirm that all contents of the selected database may be permanently deleted before installing.'));
		return false;
	}

	$database_name = isset($connection['dbname']) ? $connection['dbname'] : '';
	if (!is_string($database_name) || trim($database_name) === ''
		|| strpos($database_name, '`') !== false || strpos($database_name, "\0") !== false
		|| in_array(strtolower($database_name), array('mysql', 'information_schema', 'performance_schema', 'sys'))) {
		display_error(_('This database cannot be used as an installation target.'));
		return false;
	}
	return true;
}

/** Recreate only the confirmed target database, removing all old SQL objects and data. */
function install_reset_database($connection) {
	global $db;

	if (!install_database_reset_confirmed($connection))
		return false;

	$result = db_query('SELECT DATABASE()');
	$current_database = $result ? db_fetch($result) : false;
	if (!$current_database || $current_database[0] !== $connection['dbname']) {
		display_error(_('The active database does not match the confirmed installation target.'));
		return false;
	}

	// Consume approval before destructive work; every retry requires a new confirmation.
	unset($_SESSION['install_database_reset_token']);
	unset($_POST['confirm_database_reset'], $_POST['database_reset_target'], $_POST['database_reset_token']);
	$reset_time_limit = (int)ini_get('max_execution_time');
	// Large schemas need time for both the reset and the following full import.
	if ($reset_time_limit > 0 && $reset_time_limit < 600)
		ini_set('max_execution_time', 600);
	if (!db_query('DROP DATABASE `'.str_replace('`', '``', $connection['dbname']).'`')) {
		display_error(_('Cannot empty the selected database. Installation stopped. Check database permissions.'));
		return false;
	}
	db_close();
	$db = db_create_db($connection);
	if (!$db) {
		display_error(_('Cannot recreate the selected database. Installation stopped. Check database permissions.'));
		return false;
	}
	return true;
}
