<?php
/**
 * CLI-only PAY-AUD-001 retention-policy and legal-hold administration.
 *
 * Commands:
 *   php admin/domain_audit_governance.php status --company=0 --legal-entity=0
 *   php admin/domain_audit_governance.php policy --company=0 --legal-entity=0 --jurisdiction=VN --retention-days=3650 --effective-at="2026-07-23 00:00:00" --reason-code=approved_schedule
 *   php admin/domain_audit_governance.php hold --company=0 --legal-entity=0 --hold-id=<32-hex-token> --reason-code=legal_request
 *   php admin/domain_audit_governance.php release --company=0 --legal-entity=0 --hold-id=<32-hex-token> --reason-code=legal_release
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
function domain_audit_governance_cli_exit($ok, $code, $details = array())
{
    $result = array_merge(array('ok' => (bool)$ok, 'code' => (string)$code), $details);
    fwrite($ok ? STDOUT : STDERR, json_encode($result)."\n");
    exit($ok ? 0 : 1);
}

/**
 * Parse fixed long options without aliases.
 *
 * @param array $arguments
 * @return array|false
 */
function domain_audit_governance_cli_options($arguments)
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
$options = domain_audit_governance_cli_options(array_slice($argv, 2));
if (!in_array($command, array('status', 'policy', 'hold', 'release'), true)
    || $options === false || !isset($options['company'], $options['legal-entity'])
    || preg_match('/^\d+$/', (string)$options['company']) !== 1
    || preg_match('/^\d+$/', (string)$options['legal-entity']) !== 1
) {
    domain_audit_governance_cli_exit(false, 'invalid_arguments');
}
$allowed = array('company', 'legal-entity');
if ($command === 'policy') {
    $allowed = array_merge($allowed, array(
        'jurisdiction', 'retention-days', 'effective-at', 'reason-code'
    ));
} elseif ($command === 'hold' || $command === 'release') {
    $allowed = array_merge($allowed, array('hold-id', 'reason-code'));
}
foreach (array_keys($options) as $name) {
    if (!in_array($name, $allowed, true)) {
        domain_audit_governance_cli_exit(false, 'invalid_arguments');
    }
}
$company = (int)$options['company'];
$legal_entity = (int)$options['legal-entity'];

require $path_to_root.'/config_db.php';
if (!isset($db_connections[$company])) {
    domain_audit_governance_cli_exit(false, 'unknown_company');
}
if (!defined('TB_PREF')) define('TB_PREF', '&TB_PREF&');
if (!defined('VARLIB_PATH')) define('VARLIB_PATH', $path_to_root.'/tmp');
if (!defined('VARLOG_PATH')) define('VARLOG_PATH', $path_to_root.'/tmp');

$_SESSION = array();
$_SESSION['wa_current_user'] = (object)array(
    'cur_con' => $company,
    'company' => $company,
    'name' => 'PAY-AUD-001 records governance CLI',
    'user' => 0,
    'access' => 0,
    'role_set' => array(),
    'auth_method' => 'records_governance_cli',
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
include_once $path_to_root.'/includes/db/domain_audit_governance.inc';

if (!isset($db_version) || $db_version !== '1.0.26') {
    domain_audit_governance_cli_exit(false, 'source_upgrade_required');
}
if (domain_audit_records_admin_authorized($company) === false) {
    domain_audit_governance_cli_exit(false, 'records_admin_identity_rejected');
}

if ($command === 'status') {
    $state = domain_audit_governance_state($company, $legal_entity);
    if (empty($state['ok'])) {
        domain_audit_governance_cli_exit(false, 'governance_verification_failed');
    }
    $policy = empty($state['policy']) ? null : array(
        'jurisdiction_code' => (string)$state['policy']['jurisdiction_code'],
        'retention_days' => (int)$state['policy']['retention_days'],
        'effective_at' => (string)$state['policy']['effective_at'],
        'chain_sequence' => (int)$state['policy']['chain_sequence'],
    );
    domain_audit_governance_cli_exit($policy !== null,
        $policy === null ? 'retention_policy_missing' : 'governance_current',
        array(
            'company_id' => $company,
            'legal_entity_id' => $legal_entity,
            'policy' => $policy,
            'active_hold_count' => (int)$state['active_hold_count'],
        ));
}

if (!isset($options['reason-code']) || !domain_audit_valid_token($options['reason-code'])) {
    domain_audit_governance_cli_exit(false, 'invalid_reason_code');
}
if ($command === 'policy') {
    if (!isset($options['jurisdiction'], $options['retention-days'], $options['effective-at'])
        || !domain_audit_valid_jurisdiction_code($options['jurisdiction'])
        || preg_match('/^\d+$/', (string)$options['retention-days']) !== 1
        || !domain_audit_valid_utc_timestamp($options['effective-at'])
    ) {
        domain_audit_governance_cli_exit(false, 'invalid_policy');
    }
    $appended = append_domain_audit_governance_event(
        $company,
        $legal_entity,
        'audit.retention_policy.set',
        array(
            'jurisdiction_code' => $options['jurisdiction'],
            'retention_days' => (int)$options['retention-days'],
            'effective_at' => $options['effective-at'],
        ),
        $options['reason-code']
    );
} else {
    if (!isset($options['hold-id'])
        || preg_match('/^[a-f0-9]{32}$/', (string)$options['hold-id']) !== 1
    ) {
        domain_audit_governance_cli_exit(false, 'invalid_hold_id');
    }
    $appended = append_domain_audit_governance_event(
        $company,
        $legal_entity,
        $command === 'hold' ? 'audit.legal_hold.placed' : 'audit.legal_hold.released',
        array('hold_id' => $options['hold-id']),
        $options['reason-code']
    );
}
domain_audit_governance_cli_exit(is_array($appended),
    is_array($appended) ? 'governance_event_appended' : 'governance_event_failed',
    is_array($appended) ? array(
        'chain_sequence' => (int)$appended['chain_sequence'],
        'correlation_id' => (string)$appended['correlation_id'],
    ) : array());
