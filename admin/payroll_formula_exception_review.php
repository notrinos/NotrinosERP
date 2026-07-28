<?php
/**
 * CLI-only PAY-RULE-001 configured-company exception review workflow.
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
function payroll_formula_exception_review_cli_exit($ok, $code, $details = array()) {
    $result = array_merge(array('ok' => (bool)$ok, 'code' => (string)$code), $details);
    fwrite($ok ? STDOUT : STDERR, json_encode($result)."\n");
    exit($ok ? 0 : 1);
}

/**
 * Parse fixed long options without aliases or duplicate keys.
 *
 * @param array $arguments
 * @return array|false
 */
function payroll_formula_exception_review_cli_options($arguments) {
    $options = array();
    foreach ($arguments as $argument) {
        if (!preg_match('/^--([a-z-]+)=(.*)$/D', (string)$argument, $matches)
            || isset($options[$matches[1]])) {
            return false;
        }
        $options[$matches[1]] = $matches[2];
    }
    return $options;
}

$command = isset($argv[1]) ? (string)$argv[1] : '';
$options = payroll_formula_exception_review_cli_options(array_slice($argv, 2));
$allowed = $command === 'export'
    ? array('company', 'output', 'owner-id')
    : array(
        'company', 'report', 'output', 'receipt-output', 'approver-id',
        'evidence-id', 'review-due-on'
    );
if (!in_array($command, array('export', 'approve'), true)
    || $options === false || !isset($options['company'])
    || preg_match('/^\d+$/D', (string)$options['company']) !== 1
    || array_diff(array_keys($options), $allowed)) {
    payroll_formula_exception_review_cli_exit(false, 'invalid_arguments');
}
$company = (int)$options['company'];
if ($command === 'export'
    && !isset($options['output'], $options['owner-id'])) {
    payroll_formula_exception_review_cli_exit(false, 'invalid_arguments');
}
if ($command === 'approve'
    && !isset($options['report'], $options['output'], $options['receipt-output'],
        $options['approver-id'], $options['evidence-id'], $options['review-due-on'])) {
    payroll_formula_exception_review_cli_exit(false, 'invalid_arguments');
}
if ($command === 'approve'
    && ($options['output'] === $options['receipt-output']
        || $options['report'] === $options['output']
        || $options['report'] === $options['receipt-output'])) {
    payroll_formula_exception_review_cli_exit(false, 'artifact_paths_overlap');
}

require $path_to_root.'/config_db.php';
if (!isset($db_connections[$company])) {
    payroll_formula_exception_review_cli_exit(false, 'unknown_company');
}
if (!defined('TB_PREF')) define('TB_PREF', '&TB_PREF&');
if (!defined('VARLIB_PATH')) define('VARLIB_PATH', $path_to_root.'/tmp');
if (!defined('VARLOG_PATH')) define('VARLOG_PATH', $path_to_root.'/tmp');
$_SESSION = array();
$_SESSION['wa_current_user'] = (object)array(
    'cur_con' => $company,
    'company' => $company,
    'name' => 'PAY-RULE-001 formula exception review CLI',
    'user' => 0,
    'access' => 0,
    'role_set' => array(),
    'auth_method' => 'payroll_formula_exception_review_cli'
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
include_once $path_to_root.'/hrm/includes/payroll_formula_exception_review.inc';

if (!isset($db_version) || version_compare((string)$db_version, '1.0.47', '<')) {
    payroll_formula_exception_review_cli_exit(false, 'source_upgrade_required');
}
if (!hrm_legacy_formula_review_roles_separated($company)) {
    payroll_formula_exception_review_cli_exit(false, 'review_role_separation_rejected');
}
$connection = set_global_connection($company);
if (!$connection) {
    payroll_formula_exception_review_cli_exit(false, 'database_connection_failed');
}
$company_version = isset($SysPrefs->prefs['version_id'])
    ? (string)$SysPrefs->prefs['version_id'] : '';
if (!hrm_legacy_formula_review_company_version_is_reviewable($company_version)) {
    payroll_formula_exception_review_cli_exit(false, 'company_version_not_reviewable');
}
$corpus_path = $path_to_root.'/hrm/config/payroll_formula_differential_corpus.json';
$register_path = $path_to_root.'/hrm/config/payroll_formula_exception_register.json';
$error = null;

if ($command === 'export') {
    $owner_identity = hrm_legacy_formula_review_authorized_identity($company, 'data_owner');
    if ($owner_identity === false) {
        payroll_formula_exception_review_cli_exit(false, 'data_owner_identity_rejected');
    }
    $report = hrm_legacy_formula_exception_review_build_report(
        $company,
        $options['owner-id'],
        $owner_identity,
        $corpus_path,
        $register_path,
        null,
        $error
    );
    if ($report === false) {
        payroll_formula_exception_review_cli_exit(false, 'report_generation_failed', array(
            'reason' => (string)$error
        ));
    }
    $file_hash = hrm_legacy_formula_review_write_artifact(
        $options['output'], $report, $error
    );
    if ($file_hash === false) {
        payroll_formula_exception_review_cli_exit(false, 'report_write_failed', array(
            'reason' => (string)$error
        ));
    }
    payroll_formula_exception_review_cli_exit(true, 'report_exported', array(
        'company_id' => $company,
        'report_sha256' => $report['report_sha256'],
        'file_sha256' => $file_hash,
        'inventory_sha256' => $report['inventory']['inventory_sha256'],
        'unsupported_distinct_formula_count' => count($report['inventory']['exceptions'])
    ));
}

$reviewer_identity = hrm_legacy_formula_review_authorized_identity($company, 'security_reviewer');
if ($reviewer_identity === false) {
    payroll_formula_exception_review_cli_exit(false, 'security_reviewer_identity_rejected');
}
$report = hrm_legacy_formula_review_read_artifact($options['report'], $error);
if ($report === false
    || !hrm_legacy_formula_exception_review_validate_report($report, $error)) {
    payroll_formula_exception_review_cli_exit(false, 'report_rejected', array(
        'reason' => (string)$error
    ));
}
$reviewer_fingerprint = hrm_legacy_formula_review_identity_fingerprint($reviewer_identity);
if ($reviewer_fingerprint === false
    || domain_audit_hash_equals($reviewer_fingerprint, $report['generated_by_os_fingerprint'])) {
    payroll_formula_exception_review_cli_exit(false, 'reviewer_separation_rejected');
}
$current_state = null;
if (!hrm_legacy_formula_exception_review_report_is_current(
    $report, $company, $corpus_path, $register_path, $error, $current_state
)) {
    payroll_formula_exception_review_cli_exit(false, 'report_stale_or_invalid', array(
        'reason' => (string)$error
    ));
}
$approved_at = gmdate('Y-m-d\TH:i:s\Z');
$approved_on = substr($approved_at, 0, 10);
$register = hrm_legacy_formula_exception_review_build_register(
    $report,
    $current_state['register'],
    $current_state['register_sha256'],
    $options['approver-id'],
    $options['evidence-id'],
    $approved_on,
    $options['review-due-on'],
    $error
);
if ($register === false) {
    payroll_formula_exception_review_cli_exit(false, 'approval_rejected', array(
        'reason' => (string)$error
    ));
}
$register_hash = hrm_legacy_formula_review_write_artifact(
    $options['output'], $register, $error
);
if ($register_hash === false) {
    payroll_formula_exception_review_cli_exit(false, 'register_write_failed', array(
        'reason' => (string)$error
    ));
}
$receipt = hrm_legacy_formula_exception_review_build_receipt(
    $report,
    $register_hash,
    $options['approver-id'],
    $reviewer_identity,
    $options['evidence-id'],
    $approved_at,
    $options['review-due-on'],
    count($register['entries']),
    $error
);
if ($receipt === false) {
    $cleaned = is_file($options['output'])
        && hash_file('sha256', $options['output']) === $register_hash
        && @unlink($options['output']);
    payroll_formula_exception_review_cli_exit(false,
        $cleaned ? 'receipt_build_failed' : 'receipt_build_failed_recovery_required',
        array('reason' => (string)$error)
    );
}
$receipt_hash = hrm_legacy_formula_review_write_artifact(
    $options['receipt-output'], $receipt, $error
);
if ($receipt_hash === false) {
    $cleaned = is_file($options['output'])
        && hash_file('sha256', $options['output']) === $register_hash
        && @unlink($options['output']);
    payroll_formula_exception_review_cli_exit(false,
        $cleaned ? 'receipt_write_failed' : 'receipt_write_failed_recovery_required',
        array('reason' => (string)$error)
    );
}
payroll_formula_exception_review_cli_exit(true, 'register_approved', array(
    'company_id' => $company,
    'report_sha256' => $report['report_sha256'],
    'register_sha256' => $register_hash,
    'receipt_sha256' => $receipt['receipt_sha256'],
    'receipt_file_sha256' => $receipt_hash,
    'exception_entry_count' => count($register['entries']),
    'review_due_on' => $options['review-due-on']
));
