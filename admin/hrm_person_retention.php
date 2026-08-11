<?php
/**
 * CLI-only HRM-FND-001 retired Person-shadow retention executor.
 *
 * Usage:
 *   php admin/hrm_person_retention.php anonymize --company=0 --person=123 --retained-until=2030-12-31 --policy-ref=approved_schedule_v1 --reason-code=approved_retention_schedule
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
function hrm_person_retention_cli_exit($result)
{
    $ok = is_array($result) && !empty($result['ok']);
    fwrite($ok ? STDOUT : STDERR, json_encode(is_array($result) ? $result
        : array('ok' => false, 'code' => 'retention_execution_failed'))."\n");
    exit($ok ? 0 : 1);
}
function hrm_person_retention_cli_options($arguments)
{
    $options = array();
    foreach ($arguments as $argument) {
        if (!preg_match('/^--([a-z-]+)=(.*)$/', (string)$argument, $matches)
            || isset($options[$matches[1]]))
            return false;
        $options[$matches[1]] = $matches[2];
    }
    return $options;
}
$command = isset($argv[1]) ? (string)$argv[1] : '';
$options = hrm_person_retention_cli_options(array_slice($argv, 2));
if ($command !== 'anonymize' || $options === false || count($options) !== 5
    || !isset($options['company'], $options['person'], $options['retained-until'],
        $options['policy-ref'], $options['reason-code'])
    || preg_match('/^(0|[1-9]\d*)$/D', (string)$options['company']) !== 1
    || preg_match('/^[1-9]\d*$/D', (string)$options['person']) !== 1
    || preg_match('/^\d{4}-\d{2}-\d{2}$/D', (string)$options['retained-until']) !== 1)
    hrm_person_retention_cli_exit(array('ok' => false, 'code' => 'invalid_arguments'));
$company = (int)$options['company'];
require $path_to_root.'/config_db.php';
if (!isset($db_connections[$company]))
    hrm_person_retention_cli_exit(array('ok' => false, 'code' => 'unknown_company'));
if (!defined('TB_PREF')) define('TB_PREF', '&TB_PREF&');
if (!defined('VARLIB_PATH')) define('VARLIB_PATH', $path_to_root.'/tmp');
if (!defined('VARLOG_PATH')) define('VARLOG_PATH', $path_to_root.'/tmp');
$_SESSION = array();
$_SESSION['wa_current_user'] = (object)array(
    'cur_con' => $company, 'company' => $company,
    'name' => 'HRM-FND-001 records governance CLI', 'user' => 0,
    'access' => 0, 'role_set' => array(), 'auth_method' => 'records_governance_cli'
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
include_once $path_to_root.'/hrm/includes/db/employee_person_worker_db.inc';
include_once $path_to_root.'/hrm/includes/db/employee_person_retention_db.inc';
if (!isset($db_version) || version_compare((string)$db_version, '1.0.143', '<'))
    hrm_person_retention_cli_exit(array('ok' => false, 'code' => 'source_upgrade_required'));
hrm_person_retention_cli_exit(execute_hrm_person_retention_anonymization(
    $company,
    (int)$options['person'],
    (string)$options['retained-until'],
    (string)$options['policy-ref'],
    (string)$options['reason-code']
));
