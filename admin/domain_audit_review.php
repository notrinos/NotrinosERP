<?php
/**
 * CLI-only, separately authorized, redacted PAY-AUD-001 review search.
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
 * Emit a minimized result and terminate.
 *
 * @param bool $ok
 * @param string $code
 * @param array $details
 * @return void
 */
function domain_audit_review_cli_exit($ok, $code, $details = array())
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
function domain_audit_review_cli_options($arguments)
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

$options = domain_audit_review_cli_options(array_slice($argv, 1));
$allowed = array(
    'company', 'legal-entity', 'sequence-start', 'sequence-end',
    'occurred-from', 'occurred-to', 'entity-type', 'action', 'reason-code',
    'event-reason-code', 'correlation-id', 'limit'
);
if ($options === false || !isset(
    $options['company'], $options['legal-entity'], $options['event-reason-code']
)) {
    domain_audit_review_cli_exit(false, 'invalid_arguments');
}
foreach (array_keys($options) as $name) {
    if (!in_array($name, $allowed, true)) {
        domain_audit_review_cli_exit(false, 'invalid_arguments');
    }
}
foreach (array('company', 'legal-entity') as $name) {
    if (preg_match('/^\d+$/', (string)$options[$name]) !== 1) {
        domain_audit_review_cli_exit(false, 'invalid_arguments');
    }
}
foreach (array('sequence-start', 'sequence-end', 'limit') as $name) {
    if (isset($options[$name])
        && preg_match('/^\d+$/', (string)$options[$name]) !== 1
    ) {
        domain_audit_review_cli_exit(false, 'invalid_arguments');
    }
}
$company = (int)$options['company'];
$legal_entity = (int)$options['legal-entity'];

require $path_to_root.'/config_db.php';
if (!isset($db_connections[$company])) {
    domain_audit_review_cli_exit(false, 'unknown_company');
}
if (!defined('TB_PREF')) define('TB_PREF', '&TB_PREF&');
if (!defined('VARLIB_PATH')) define('VARLIB_PATH', $path_to_root.'/tmp');
if (!defined('VARLOG_PATH')) define('VARLOG_PATH', $path_to_root.'/tmp');
$_SESSION = array();
$_SESSION['wa_current_user'] = (object)array(
    'cur_con' => $company,
    'company' => $company,
    'name' => 'PAY-AUD-001 audit review CLI',
    'user' => 0,
    'access' => 0,
    'role_set' => array(),
    'auth_method' => 'audit_review_cli',
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
include_once $path_to_root.'/includes/db/domain_audit_review.inc';

if (!isset($db_version) || $db_version !== '1.0.26') {
    domain_audit_review_cli_exit(false, 'source_upgrade_required');
}
$reviewer = domain_audit_reviewer_authorized($company);
if ($reviewer === false) {
    domain_audit_review_cli_exit(false, 'reviewer_identity_rejected');
}
$filter = domain_audit_review_filter(array(
    'company_id' => $company,
    'legal_entity_id' => $legal_entity,
    'sequence_start' => isset($options['sequence-start'])
        ? (int)$options['sequence-start'] : 1,
    'sequence_end' => isset($options['sequence-end'])
        ? (int)$options['sequence-end'] : 2147483647,
    'occurred_from' => isset($options['occurred-from'])
        ? $options['occurred-from'] : '',
    'occurred_to' => isset($options['occurred-to'])
        ? $options['occurred-to'] : '',
    'entity_type' => isset($options['entity-type']) ? $options['entity-type'] : '',
    'action' => isset($options['action']) ? $options['action'] : '',
    'reason_code' => isset($options['reason-code']) ? $options['reason-code'] : '',
    'correlation_id' => isset($options['correlation-id'])
        ? $options['correlation-id'] : '',
    'limit' => isset($options['limit']) ? (int)$options['limit'] : 100,
));
if ($filter === false
    || !domain_audit_valid_token($options['event-reason-code'])
) {
    domain_audit_review_cli_exit(false, 'invalid_review_request');
}

set_global_connection($company);
$verified = verify_domain_audit_chain($company, $legal_entity);
if (empty($verified['ok'])) {
    domain_audit_review_cli_exit(false, 'audit_chain_verification_failed');
}
$access = append_domain_audit_review_access(
    $filter, $options['event-reason-code'], $reviewer
);
if (!is_array($access)) {
    domain_audit_review_cli_exit(false, 'review_access_evidence_failed');
}
if (!domain_audit_activate_review_identity($company)) {
    domain_audit_review_cli_exit(false, 'review_identity_rejected');
}
$rows = domain_audit_review_search($filter);
if ($rows === false) {
    domain_audit_review_cli_exit(false, 'review_search_failed');
}
domain_audit_review_cli_exit(true, 'review_complete', array(
    'company_id' => $company,
    'legal_entity_id' => $legal_entity,
    'access_correlation_id' => (string)$access['correlation_id'],
    'result_count' => count($rows),
    'results' => $rows,
));
