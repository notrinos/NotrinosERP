<?php
/** CLI-only PAY-RULE-002 configured-company shadow acceptance workflow. */
if (PHP_SAPI !== 'cli') {
    if (!headers_sent()) {
        http_response_code(404);
        header('Cache-Control: no-store');
        header('X-Content-Type-Options: nosniff');
    }
    exit;
}

$path_to_root = dirname(__DIR__);

/** Emit one bounded JSON CLI result and terminate. */
function pr2_accept_cli_exit($ok, $code, $details=array())
{
    $payload = array_merge(array('ok'=>(bool)$ok, 'code'=>(string)$code), $details);
    fwrite($ok ? STDOUT : STDERR, json_encode($payload)."\n");
    exit($ok ? 0 : 1);
}

/** Parse unique --name=value CLI options. */
function pr2_accept_cli_options($arguments)
{
    $options = array();
    foreach ($arguments as $argument) {
        if (!preg_match('/^--([a-z-]+)=(.*)$/D', (string)$argument, $match)
            || isset($options[$match[1]]))
            return false;
        $options[$match[1]] = $match[2];
    }
    return $options;
}

/** Require an artifact path to differ from every protected input/output path. */
function pr2_accept_cli_paths_are_distinct($paths)
{
    return count($paths) === count(array_unique($paths, SORT_STRING));
}

$command = isset($argv[1]) ? (string)$argv[1] : '';
$options = pr2_accept_cli_options(array_slice($argv, 2));
$required = array(
    'export'=>array('company','output','author-id'),
    'review'=>array('company','report','output','reviewer-id','evidence-id','review-due-on'),
    'publish'=>array('company','report','review','output','receipt-output','publisher-id','executor-id','evidence-id'),
    'execute'=>array('company','report','review','registry','publication-receipt','output','executor-id')
);
if (!isset($required[$command]) || $options === false
    || !isset($options['company']) || preg_match('/^\d+$/D', (string)$options['company']) !== 1
    || array_diff(array_keys($options), $required[$command])
    || array_diff($required[$command], array_keys($options)))
    pr2_accept_cli_exit(false, 'invalid_arguments');

$artifact_paths = array();
foreach (array('report','review','registry','publication-receipt','output','receipt-output') as $field) {
    if (isset($options[$field]))
        $artifact_paths[] = (string)$options[$field];
}
if (!pr2_accept_cli_paths_are_distinct($artifact_paths))
    pr2_accept_cli_exit(false, 'artifact_paths_overlap');

$company = (int)$options['company'];
$db_connections = array();
require $path_to_root.'/config_db.php';
if (!isset($db_connections[$company]))
    pr2_accept_cli_exit(false, 'unknown_company');
if (!defined('TB_PREF')) define('TB_PREF', '&TB_PREF&');
if (!defined('VARLIB_PATH')) define('VARLIB_PATH', $path_to_root.'/tmp');
if (!defined('VARLOG_PATH')) define('VARLOG_PATH', $path_to_root.'/tmp');

$_SESSION = array();
$_SESSION['wa_current_user'] = (object)array(
    'cur_con'=>$company, 'company'=>$company, 'name'=>'PAY-RULE-002 safe acceptance CLI',
    'user'=>0, 'access'=>0, 'role_set'=>array(), 'auth_method'=>'payroll_formula_safe_acceptance_cli'
);
$_SESSION['language'] = (object)array('encoding'=>'UTF-8','code'=>'en_US','dir'=>'ltr');
$dflt_lang = 'en_US';
$installed_languages = array(array('code'=>'en_US','encoding'=>'UTF-8'));
include_once $path_to_root.'/version.php';
include_once $path_to_root.'/includes/errors.inc';
include_once $path_to_root.'/includes/current_user.inc';
include_once $path_to_root.'/admin/db/company_db.inc';
include_once $path_to_root.'/includes/prefs/sysprefs.inc';
$SysPrefs = new sys_prefs();
$_SESSION['SysPrefs'] =& $SysPrefs;
include_once $path_to_root.'/includes/main.inc';
include_once $path_to_root.'/hrm/includes/payroll_formula_safe_acceptance.inc';

if (!isset($db_version) || (string)$db_version !== '1.0.468')
    pr2_accept_cli_exit(false, 'source_version_not_1_0_468');
if (!hrm_payroll_formula_safe_acceptance_roles_separated($company))
    pr2_accept_cli_exit(false, 'four_role_separation_not_configured');
if (!set_global_connection($company))
    pr2_accept_cli_exit(false, 'database_connection_failed');
$company_version = isset($SysPrefs->prefs['version_id']) ? (string)$SysPrefs->prefs['version_id'] : '';
if ($company_version !== '1.0.468')
    pr2_accept_cli_exit(false, 'company_version_not_1_0_468', array('company_version'=>$company_version));

$corpus = json_decode(@file_get_contents($path_to_root.'/hrm/config/payroll_formula_differential_corpus.json'), true);
if (!is_array($corpus))
    pr2_accept_cli_exit(false, 'corpus_unavailable');
$error = null;
$rows = hrm_payroll_formula_safe_acceptance_collect_rows($error);
if ($rows === false)
    pr2_accept_cli_exit(false, 'inventory_failed', array('reason'=>(string)$error));

if ($command === 'export') {
    $identity = hrm_payroll_formula_safe_acceptance_authorized_identity($company, 'author');
    if ($identity === false)
        pr2_accept_cli_exit(false, 'author_identity_rejected');
    $report = hrm_payroll_formula_safe_acceptance_build_report(
        $company, $options['author-id'], $identity, $rows, $corpus, null, $error
    );
    if ($report === false)
        pr2_accept_cli_exit(false, 'report_generation_failed', array('reason'=>(string)$error));
    $file_hash = hrm_legacy_formula_review_write_artifact($options['output'], $report, $error);
    if ($file_hash === false)
        pr2_accept_cli_exit(false, 'report_write_failed', array('reason'=>(string)$error));
    pr2_accept_cli_exit(true, 'acceptance_exported', array(
        'company_id'=>$company, 'report_sha256'=>$report['report_sha256'],
        'file_sha256'=>$file_hash, 'inventory_sha256'=>$report['inventory']['inventory_sha256'],
        'acceptance_status'=>$report['acceptance_status'],
        'accepted_distinct_count'=>count($report['inventory']['accepted']),
        'blocked_distinct_count'=>count($report['inventory']['blocked'])
    ));
}

$report = hrm_legacy_formula_review_read_artifact($options['report'], $error);
if ($report === false || !hrm_payroll_formula_safe_acceptance_validate_report($report, $error))
    pr2_accept_cli_exit(false, 'report_rejected', array('reason'=>(string)$error));
if ((int)$report['company_id'] !== $company)
    pr2_accept_cli_exit(false, 'report_company_mismatch');
if (!hrm_payroll_formula_safe_acceptance_report_is_current($report, $rows, $corpus, $error))
    pr2_accept_cli_exit(false, 'report_stale_or_invalid', array('reason'=>(string)$error));

if ($command === 'review') {
    $identity = hrm_payroll_formula_safe_acceptance_authorized_identity($company, 'reviewer');
    if ($identity === false)
        pr2_accept_cli_exit(false, 'reviewer_identity_rejected');
    $review = hrm_payroll_formula_safe_acceptance_build_review(
        $report, $options['reviewer-id'], $identity, $options['evidence-id'],
        gmdate('Y-m-d\TH:i:s\Z'), $options['review-due-on'], $error
    );
    if ($review === false)
        pr2_accept_cli_exit(false, 'review_rejected', array('reason'=>(string)$error));
    $file_hash = hrm_legacy_formula_review_write_artifact($options['output'], $review, $error);
    if ($file_hash === false)
        pr2_accept_cli_exit(false, 'review_write_failed', array('reason'=>(string)$error));
    pr2_accept_cli_exit(true, 'acceptance_reviewed', array(
        'company_id'=>$company, 'report_sha256'=>$report['report_sha256'],
        'review_receipt_sha256'=>$review['receipt_sha256'], 'review_file_sha256'=>$file_hash,
        'review_due_on'=>$review['review_due_on']
    ));
}

$review = hrm_legacy_formula_review_read_artifact($options['review'], $error);
if ($review === false || !hrm_payroll_formula_safe_acceptance_validate_review($review, $report, $error))
    pr2_accept_cli_exit(false, 'review_receipt_rejected', array('reason'=>(string)$error));

if ($command === 'publish') {
    $publisher = hrm_payroll_formula_safe_acceptance_authorized_identity($company, 'publisher');
    if ($publisher === false)
        pr2_accept_cli_exit(false, 'publisher_identity_rejected');
    $executor_os = hrm_payroll_formula_safe_acceptance_expected_os_user($company, 'shadow_executor');
    if ($executor_os === '')
        pr2_accept_cli_exit(false, 'shadow_executor_identity_unconfigured');
    $published_at = gmdate('Y-m-d\TH:i:s\Z');
    $registry = hrm_payroll_formula_safe_acceptance_build_registry(
        $report, $review, $options['publisher-id'], $publisher, $options['executor-id'],
        $executor_os, $options['evidence-id'], $published_at, $error
    );
    if ($registry === false)
        pr2_accept_cli_exit(false, 'publication_rejected', array('reason'=>(string)$error));
    $registry_hash = hrm_legacy_formula_review_write_artifact($options['output'], $registry, $error);
    if ($registry_hash === false)
        pr2_accept_cli_exit(false, 'registry_write_failed', array('reason'=>(string)$error));
    $receipt = hrm_payroll_formula_safe_acceptance_build_publication_receipt(
        $report, $review, $registry, $registry_hash, $options['publisher-id'], $publisher,
        $options['executor-id'], $executor_os, $options['evidence-id'], $published_at, $error
    );
    if ($receipt === false) {
        $cleaned = is_file($options['output'])
            && hash_file('sha256', $options['output']) === $registry_hash
            && @unlink($options['output']);
        pr2_accept_cli_exit(false, $cleaned ? 'publication_receipt_build_failed' : 'publication_receipt_build_failed_recovery_required', array('reason'=>(string)$error));
    }
    $receipt_hash = hrm_legacy_formula_review_write_artifact($options['receipt-output'], $receipt, $error);
    if ($receipt_hash === false) {
        $cleaned = is_file($options['output'])
            && hash_file('sha256', $options['output']) === $registry_hash
            && @unlink($options['output']);
        pr2_accept_cli_exit(false, $cleaned ? 'publication_receipt_write_failed' : 'publication_receipt_write_failed_recovery_required', array('reason'=>(string)$error));
    }
    pr2_accept_cli_exit(true, 'publication_created', array(
        'company_id'=>$company, 'registry_sha256'=>$registry_hash,
        'publication_receipt_sha256'=>$receipt['receipt_sha256'],
        'receipt_file_sha256'=>$receipt_hash, 'entry_count'=>count($registry['entries']),
        'production_execution_forbidden'=>true
    ));
}

$executor = hrm_payroll_formula_safe_acceptance_authorized_identity($company, 'shadow_executor');
if ($executor === false)
    pr2_accept_cli_exit(false, 'shadow_executor_identity_rejected');
$registry = hrm_legacy_formula_review_read_artifact($options['registry'], $error);
$publication_receipt = hrm_legacy_formula_review_read_artifact($options['publication-receipt'], $error);
$registry_file_hash = is_file($options['registry']) && !is_link($options['registry'])
    ? @hash_file('sha256', $options['registry']) : false;
if (!is_array($registry) || !is_array($publication_receipt) || !is_string($registry_file_hash)
    || !hrm_payroll_formula_safe_acceptance_validate_publication_receipt(
        $publication_receipt, $report, $review, $registry, $registry_file_hash, $error
    ))
    pr2_accept_cli_exit(false, 'publication_custody_rejected', array('reason'=>(string)$error));

define('HRM_PAYROLL_FORMULA_SAFE_NONPRODUCTION_SHADOW', true);
$execution_receipt = null;
if (!hrm_payroll_formula_safe_acceptance_execute(
    $report, $review, $registry, $publication_receipt, $registry_file_hash,
    $options['executor-id'], $executor, $rows, $corpus, gmdate('Y-m-d\TH:i:s\Z'),
    $execution_receipt, $error
))
    pr2_accept_cli_exit(false, 'shadow_execution_rejected', array('reason'=>(string)$error));
$execution_file_hash = hrm_legacy_formula_review_write_artifact($options['output'], $execution_receipt, $error);
if ($execution_file_hash === false)
    pr2_accept_cli_exit(false, 'execution_receipt_write_failed', array('reason'=>(string)$error));
pr2_accept_cli_exit(true, 'shadow_execution_accepted', array(
    'company_id'=>$company, 'execution_receipt_sha256'=>$execution_receipt['receipt_sha256'],
    'execution_file_sha256'=>$execution_file_hash,
    'executed_distinct_formula_count'=>$execution_receipt['executed_distinct_formula_count'],
    'production_execution_authorized'=>false
));
