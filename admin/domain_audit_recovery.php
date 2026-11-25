<?php
/**
 * CLI-only PAY-AUD-001 key-inclusive recovery manifest ceremony.
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
$allowed = $command === 'create'
    ? array('company', 'database-backup', 'output')
    : array('company', 'database-backup', 'manifest', 'restored-private-root');
if (!in_array($command, array('create', 'verify'), true)
    || $options === false || !isset($options['company'], $options['database-backup'])
    || preg_match('/^\d+$/', (string)$options['company']) !== 1
    || array_diff(array_keys($options), $allowed)
) {
    cli_command_exit(false, 'invalid_arguments');
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
    'cur_con' => $company, 'company' => $company,
    'name' => 'PAY-AUD recovery CLI', 'user' => 0,
    'access' => 0, 'role_set' => array(), 'auth_method' => 'recovery_cli'
);
$_SESSION['language'] = (object)array(
    'encoding' => 'UTF-8', 'code' => 'en_US', 'dir' => 'ltr'
);
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
include_once $path_to_root.'/includes/db/domain_audit_recovery.inc';

if (!isset($db_version) || version_compare((string)$db_version, '1.0.26', '<')) {
    cli_command_exit(false, 'source_upgrade_required');
}
if (domain_audit_key_rotation_authorized($company) === false) {
    cli_command_exit(false, 'recovery_identity_rejected');
}
if ($command === 'create') {
    if (!isset($options['output'])) {
        cli_command_exit(false, 'invalid_arguments');
    }
    $envelope = domain_audit_build_recovery_manifest(
        $company, $options['database-backup']
    );
    if (!is_array($envelope)
        || !domain_audit_write_recovery_manifest(
            $options['output'], $envelope, $company
        )
    ) {
        cli_command_exit(false, 'recovery_manifest_failed');
    }
    cli_command_exit(true, 'recovery_manifest_created', array(
        'company_id' => $company,
        'keyring_generation' => (int)$envelope['manifest']['keyring_generation'],
        'active_key_id' => (string)$envelope['manifest']['active_key_id'],
        'database_backup_sha256'
            => (string)$envelope['manifest']['database_backup']['sha256'],
    ));
}
if (!isset($options['manifest'], $options['restored-private-root'])
    || is_link($options['manifest']) || !is_file($options['manifest'])
    || filesize($options['manifest']) > DOMAIN_AUDIT_RECOVERY_MAX_MANIFEST_BYTES
) {
    cli_command_exit(false, 'invalid_arguments');
}
$json = @file_get_contents($options['manifest']);
$envelope = is_string($json) ? json_decode($json, true) : null;
$verified = domain_audit_verify_recovery_manifest(
    $envelope,
    $company,
    $options['database-backup'],
    $options['restored-private-root']
);
cli_command_exit(is_array($verified),
    is_array($verified) ? 'recovery_manifest_verified' : 'recovery_manifest_rejected',
    is_array($verified) ? $verified : array());
