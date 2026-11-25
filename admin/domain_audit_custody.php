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
require_once(__DIR__.'/../includes/cli_command_helpers.inc');

if (PHP_SAPI !== 'cli') {
    if (!headers_sent()) {
        http_response_code(404);
        header('Cache-Control: no-store');
        header('X-Content-Type-Options: nosniff');
    }
    exit;
}

$path_to_root = dirname(__DIR__);

$command = isset($argv[1]) ? (string)$argv[1] : '';
$options = cli_command_options(array_slice($argv, 2));
if (!in_array($command, array('prepare', 'accept', 'verify', 'monitor'), true) || $options === false
    || !isset($options['company']) || preg_match('/^\d+$/', $options['company']) !== 1
) {
    cli_command_exit(false, 'invalid_arguments');
}
$allowed_options = $command === 'prepare'
    ? array('company', 'output', 'max-events')
    : ($command === 'accept' ? array('company', 'receipt') : array('company'));
foreach (array_keys($options) as $name) {
    if (!in_array($name, $allowed_options, true)) {
        cli_command_exit(false, 'invalid_arguments');
    }
}
$company = (int)$options['company'];

require $path_to_root.'/config_db.php';
if (!isset($db_connections[$company])) {
    cli_command_exit(false, 'unknown_company');
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
    cli_command_exit(false, 'export_identity_rejected');
}
if (!isset($db_version) || version_compare((string)$db_version, '1.0.26', '<')) {
    cli_command_exit(false, 'source_upgrade_required');
}

if ($command === 'prepare') {
    if (!isset($options['output']) || $options['output'] === '') {
        cli_command_exit(false, 'invalid_arguments');
    }
    $maximum = isset($options['max-events']) && preg_match('/^\d+$/', $options['max-events']) === 1
        ? (int)$options['max-events'] : DOMAIN_AUDIT_EXPORT_MAX_EVENTS;
    $package = domain_audit_prepare_next_export($company, 0, $maximum);
    if ($package === false) {
        cli_command_exit(false, 'export_preparation_failed');
    }
    if (!empty($package['complete'])) {
        cli_command_exit(true, 'no_pending_events', array(
            'last_sequence' => (int)$package['last_sequence'],
            'last_receipt_hash' => (string)$package['last_receipt_hash'],
        ));
    }
    if (!domain_audit_write_staging_package($options['output'], $package['json'], $company)) {
        cli_command_exit(false, 'staging_write_failed');
    }
    cli_command_exit(true, 'package_prepared', array(
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
        cli_command_exit(false, 'invalid_receipt_file');
    }
    $contents = @file_get_contents($options['receipt']);
    $envelope = is_string($contents) ? json_decode($contents, true) : null;
    $recorded = is_array($envelope) ? record_domain_audit_custody_receipt($envelope, $company) : false;
    if ($recorded === false) {
        cli_command_exit(false, 'receipt_rejected');
    }
    cli_command_exit(true, 'receipt_accepted', array(
        'receipt_id' => (int)$recorded['receipt_id'],
        'receipt_hash' => (string)$recorded['receipt_hash'],
        'idempotent' => !empty($recorded['idempotent']),
    ));
}

if ($command === 'monitor') {
    $status = domain_audit_custody_status($company, 0);
    cli_command_exit(!empty($status['ok']),
        !empty($status['ok']) ? 'custody_monitor_ok' : 'custody_monitor_alert',
        $status);
}

$verification = verify_domain_audit_custody_receipts($company, 0);
cli_command_exit(!empty($verification['ok']),
    !empty($verification['ok']) ? 'custody_verified' : 'custody_verification_failed',
    $verification);
