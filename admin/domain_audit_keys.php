<?php
/**
 * CLI-only PAY-AUD-001 signing-key rotation and recovery ceremony.
 *
 * Commands:
 *   php admin/domain_audit_keys.php status --company=0
 *   php admin/domain_audit_keys.php verify --company=0
 *   php admin/domain_audit_keys.php rotate --company=0 --reason-code=scheduled_key_rotation
 *   php admin/domain_audit_keys.php recover --company=0
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
 * Emit one minimized result and terminate.
 *
 * @param bool $ok
 * @param string $code
 * @param array $details
 * @return void
 */
function domain_audit_keys_cli_exit($ok, $code, $details = array())
{
    $result = array_merge(array('ok' => (bool)$ok, 'code' => (string)$code), $details);
    fwrite($ok ? STDOUT : STDERR, json_encode($result)."\n");
    exit($ok ? 0 : 1);
}

/**
 * Parse fixed long options without positional aliases.
 *
 * @param array $arguments
 * @return array|false
 */
function domain_audit_keys_cli_options($arguments)
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
$options = domain_audit_keys_cli_options(array_slice($argv, 2));
if (!in_array($command, array('status', 'verify', 'rotate', 'recover'), true)
    || $options === false || !isset($options['company'])
    || preg_match('/^\d+$/', (string)$options['company']) !== 1
) {
    domain_audit_keys_cli_exit(false, 'invalid_arguments');
}
$allowed_options = $command === 'rotate'
    ? array('company', 'reason-code') : array('company');
foreach (array_keys($options) as $name) {
    if (!in_array($name, $allowed_options, true)) {
        domain_audit_keys_cli_exit(false, 'invalid_arguments');
    }
}
$company = (int)$options['company'];

require $path_to_root.'/config_db.php';
if (!isset($db_connections[$company])) {
    domain_audit_keys_cli_exit(false, 'unknown_company');
}
if (!defined('TB_PREF')) define('TB_PREF', '&TB_PREF&');
if (!defined('VARLIB_PATH')) define('VARLIB_PATH', $path_to_root.'/tmp');
if (!defined('VARLOG_PATH')) define('VARLOG_PATH', $path_to_root.'/tmp');

$_SESSION = array();
$_SESSION['wa_current_user'] = (object)array(
    'cur_con' => $company,
    'company' => $company,
    'name' => 'PAY-AUD-001 key administration CLI',
    'user' => 0,
    'access' => 0,
    'role_set' => array(),
    'auth_method' => 'key_rotation_cli',
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
include_once $path_to_root.'/includes/db/domain_audit_db.inc';

if (!isset($db_version) || $db_version !== '1.0.26') {
    domain_audit_keys_cli_exit(false, 'source_upgrade_required');
}
if (domain_audit_key_rotation_authorized($company) === false) {
    domain_audit_keys_cli_exit(false, 'key_admin_identity_rejected');
}

$paths = domain_audit_keyring_paths($company);
if ($paths === false) {
    domain_audit_keys_cli_exit(false, 'key_storage_unavailable');
}

if ($command === 'status') {
    $pending = is_file($paths['pending']);
    $manifest = $pending ? false : domain_audit_keyring_manifest($company);
    if ($pending) {
        domain_audit_keys_cli_exit(false, 'rotation_recovery_required');
    }
    if ($manifest === false) {
        domain_audit_keys_cli_exit(false, 'keyring_invalid');
    }
    domain_audit_keys_cli_exit(true, 'keyring_current', array(
        'company_id' => $company,
        'generation' => (int)$manifest['generation'],
        'active_key_id' => (string)$manifest['active_key_id'],
    ));
}

if ($command === 'verify') {
    $verification = verify_domain_audit_chain($company, 0);
    domain_audit_keys_cli_exit(!empty($verification['ok']),
        !empty($verification['ok']) ? 'keyring_chain_verified' : 'keyring_chain_verification_failed',
        $verification);
}

if ($command === 'recover') {
    $recovered = recover_domain_audit_key_rotation($company);
    domain_audit_keys_cli_exit(is_array($recovered), is_array($recovered)
        ? 'key_rotation_recovered' : 'key_rotation_recovery_failed',
        is_array($recovered) ? $recovered : array());
}

if (!isset($options['reason-code']) || !domain_audit_valid_token($options['reason-code'])) {
    domain_audit_keys_cli_exit(false, 'invalid_reason_code');
}
$rotated = rotate_domain_audit_key($company, $options['reason-code']);
domain_audit_keys_cli_exit(is_array($rotated), is_array($rotated)
    ? 'key_rotation_completed' : 'key_rotation_failed',
    is_array($rotated) ? $rotated : array());
