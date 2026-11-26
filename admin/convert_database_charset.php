<?php
/**
 * Command line: move one company database to utf8mb4 with one collation, or report what is left.
 *
 *   php admin/convert_database_charset.php check --company=0
 *   php admin/convert_database_charset.php convert --company=0
 *
 * Take a backup first. The result is one JSON line (see includes/cli_command_helpers.inc): the tables that still hold
 * utf8 (3-byte) text or another collation, and for convert the number of converted tables. Exit 0 when nothing is left.
 */
require_once(__DIR__.'/../includes/cli_command_helpers.inc');

if (PHP_SAPI !== 'cli') {
	if (!headers_sent()) {
		http_response_code(404);
		header('Cache-Control: no-store');
	}
	exit;
}

$path_to_root = dirname(__DIR__);
$command = isset($argv[1]) ? (string)$argv[1] : '';
$options = cli_command_options(array_slice($argv, 2));
if (!in_array($command, array('check', 'convert'), true) || $options === false || array_keys($options) !== array('company')
	|| preg_match('/^\d+$/D', $options['company']) !== 1)
	cli_command_exit(false, 'invalid_arguments');
$company = (int)$options['company'];

require $path_to_root.'/config_db.php';
if (!isset($db_connections[$company]))
	cli_command_exit(false, 'unknown_company');
if (!defined('TB_PREF'))
	define('TB_PREF', '&TB_PREF&');
if (!defined('VARLIB_PATH'))
	define('VARLIB_PATH', $path_to_root.'/tmp');
if (!defined('VARLOG_PATH'))
	define('VARLOG_PATH', $path_to_root.'/tmp');
$_SESSION = array();
$_SESSION['wa_current_user'] = (object)array('cur_con' => $company, 'company' => $company, 'name' => 'database charset CLI',
	'user' => 0, 'access' => 0, 'role_set' => array());
$_SESSION['language'] = (object)array('encoding' => 'UTF-8', 'code' => 'en_US', 'dir' => 'ltr');
$dflt_lang = 'en_US';
$installed_languages = array(array('code' => 'en_US', 'encoding' => 'UTF-8'));

include_once $path_to_root.'/version.php';
include_once $path_to_root.'/includes/errors.inc';
include_once $path_to_root.'/includes/current_user.inc';
include_once $path_to_root.'/admin/db/company_db.inc';
include_once $path_to_root.'/includes/prefs/sysprefs.inc';
$SysPrefs = new sys_prefs();
$_SESSION['SysPrefs'] =& $SysPrefs;
include_once $path_to_root.'/includes/main.inc';
include_once $path_to_root.'/includes/db/database_charset.inc';

if (!set_global_connection($company))
	cli_command_exit(false, 'database_unavailable');

$prefix = (string)$db_connections[$company]['tbpref'];
$converted = 0;
$failed = array();
if ($command === 'convert')
	list($converted, $failed) = db_convert_tables_to_utf8mb4($prefix);
$left = array_keys(db_tables_needing_utf8mb4($prefix));
cli_command_exit(!$left && !$failed, $left || $failed ? 'utf8_tables_left' : 'utf8mb4', array(
	'converted' => $converted,
	'failed' => $failed,
	'left' => $left,
));
