<?php
/**
 * CLI-only PAY-AUD-001 immutable-custody handoff and monitor.
 *
 * Commands:
 *   php admin/domain_audit_custody.php prepare --company=0 --output=/secure/staging/package.json [--max-events=1000]
 *   php admin/domain_audit_custody.php accept --company=0 --receipt=/secure/inbox/receipt.json
 *   php admin/domain_audit_custody.php verify --company=0
 *   php admin/domain_audit_custody.php monitor --company=0
 */

if (PHP_SAPI !== 'cli') {
    if (!headers_sent()) {
        http_response_code(404);
        header('Cache-Control: no-store');
        header('X-Content-Type-Options: nosniff');
    }
    exit;
}

$path_to_root = dirname(__DIR__);

/**
 * Emit one minimized CLI result and terminate.
 *
 * @param bool $ok
 * @param string $code
 * @param array $details
 * @return void
 */
function domain_audit_custody_cli_exit($ok, $code, $details = array())
{
    $result = array_merge(array('ok' => (bool)$ok, 'code' => (string)$code), $details);
    fwrite($ok ? STDOUT : STDERR, json_encode($result)."\n");
    exit($ok ? 0 : 1);
}

/**
 * Parse fixed long options without accepting positional aliases.
 *
 * @param array $arguments
 * @return array|false
 */
function domain_audit_custody_cli_options($arguments)
{
    $options = array();
    foreach ($arguments as $argument) {
        if (!preg_match('/^--([a-z-]+)=(.*)$/', (string)$argument, $matches)
            || isset($options[$matches[1]])
        ) {
            return false;
        }
        $options[$matches[1]] = $matches[2];
    }
    return $options;
}

$command = isset($argv[1]) ? (string)$argv[1] : '';
$options = domain_audit_custody_cli_options(array_slice($argv, 2));
if (!in_array($command, array('prepare', 'accept', 'verify', 'monitor'), true) || $options === false
    || !isset($options['company']) || preg_match('/^\d+$/', $options['company']) !== 1
) {
    domain_audit_custody_cli_exit(false, 'invalid_arguments');
}
$allowed_options = $command === 'prepare'
    ? array('company', 'output', 'max-events')
    : ($command === 'accept' ? array('company', 'receipt') : array('company'));
foreach (array_keys($options) as $name) {
    if (!in_array($name, $allowed_options, true)) {
        domain_audit_custody_cli_exit(false, 'invalid_arguments');
    }
}
$company = (int)$options['company'];

require $path_to_root.'/config_db.php';
if (!isset($db_connections[$company])) {
    domain_audit_custody_cli_exit(false, 'unknown_company');
}
if (!defined('TB_PREF')) define('TB_PREF', '&TB_PREF&');
if (!defined('VARLIB_PATH')) define('VARLIB_PATH', $path_to_root.'/tmp');
if (!defined('VARLOG_PATH')) define('VARLOG_PATH', $path_to_root.'/tmp');

$_SESSION = array();
$_SESSION['wa_current_user'] = (object)array(
    'cur_con' => $company,
    'company' => $company,
    'name' => 'PAY-AUD-001 custody CLI',
    'user' => 0,
    'access' => 0,
    'role_set' => array(),
    'auth_method' => 'custody_cli',
);
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
include_once $path_to_root.'/includes/db/domain_audit_export.inc';

if (!domain_audit_activate_export_identity($company)) {
    domain_audit_custody_cli_exit(false, 'export_identity_rejected');
}
if (!isset($db_version) || $db_version !== '1.0.19') {
    domain_audit_custody_cli_exit(false, 'source_upgrade_required');
}

if ($command === 'prepare') {
    if (!isset($options['output']) || $options['output'] === '') {
        domain_audit_custody_cli_exit(false, 'invalid_arguments');
    }
    $maximum = isset($options['max-events']) && preg_match('/^\d+$/', $options['max-events']) === 1
        ? (int)$options['max-events'] : DOMAIN_AUDIT_EXPORT_MAX_EVENTS;
    $package = domain_audit_prepare_next_export($company, 0, $maximum);
    if ($package === false) {
        domain_audit_custody_cli_exit(false, 'export_preparation_failed');
    }
    if (!empty($package['complete'])) {
        domain_audit_custody_cli_exit(true, 'no_pending_events', array(
            'last_sequence' => (int)$package['last_sequence'],
            'last_receipt_hash' => (string)$package['last_receipt_hash'],
        ));
    }
    if (!domain_audit_write_staging_package($options['output'], $package['json'], $company)) {
        domain_audit_custody_cli_exit(false, 'staging_write_failed');
    }
    domain_audit_custody_cli_exit(true, 'package_prepared', array(
        'sequence_start' => (int)$package['package']['sequence_start'],
        'sequence_end' => (int)$package['package']['sequence_end'],
        'event_count' => (int)$package['package']['event_count'],
        'package_hash' => (string)$package['package_hash'],
    ));
}

if ($command === 'accept') {
    if (!isset($options['receipt']) || $options['receipt'] === ''
        || !preg_match('/^(?:[A-Za-z]:[\\\\\/]|\/)/', $options['receipt'])
        || is_link($options['receipt']) || !is_file($options['receipt'])
        || filesize($options['receipt']) > 65536
    ) {
        domain_audit_custody_cli_exit(false, 'invalid_receipt_file');
    }
    $contents = @file_get_contents($options['receipt']);
    $envelope = is_string($contents) ? json_decode($contents, true) : null;
    $recorded = is_array($envelope) ? record_domain_audit_custody_receipt($envelope, $company) : false;
    if ($recorded === false) {
        domain_audit_custody_cli_exit(false, 'receipt_rejected');
    }
    domain_audit_custody_cli_exit(true, 'receipt_accepted', array(
        'receipt_id' => (int)$recorded['receipt_id'],
        'receipt_hash' => (string)$recorded['receipt_hash'],
        'idempotent' => !empty($recorded['idempotent']),
    ));
}

if ($command === 'monitor') {
    $status = domain_audit_custody_status($company, 0);
    domain_audit_custody_cli_exit(!empty($status['ok']),
        !empty($status['ok']) ? 'custody_monitor_ok' : 'custody_monitor_alert',
        $status);
}

$verification = verify_domain_audit_custody_receipts($company, 0);
domain_audit_custody_cli_exit(!empty($verification['ok']),
    !empty($verification['ok']) ? 'custody_verified' : 'custody_verification_failed',
    $verification);
