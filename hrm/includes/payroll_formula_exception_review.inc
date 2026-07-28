<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

include_once(dirname(__FILE__).'/payroll_formula_differential.inc');
include_once(dirname(dirname(dirname(__FILE__))).'/includes/db/domain_audit_db.inc');

if (!defined('HRM_LEGACY_FORMULA_EXCEPTION_REVIEW_ID'))
    define('HRM_LEGACY_FORMULA_EXCEPTION_REVIEW_ID', 'notrinoserp.hrm.payroll.legacy_formula_exception_review');
if (!defined('HRM_LEGACY_FORMULA_EXCEPTION_REVIEW_VERSION'))
    define('HRM_LEGACY_FORMULA_EXCEPTION_REVIEW_VERSION', '1.0.0');
if (!defined('HRM_LEGACY_FORMULA_EXCEPTION_REVIEW_SOURCE_VERSION'))
    define('HRM_LEGACY_FORMULA_EXCEPTION_REVIEW_SOURCE_VERSION', '1.0.47');
if (!defined('HRM_LEGACY_FORMULA_EXCEPTION_REPORT_SCHEMA_VERSION'))
    define('HRM_LEGACY_FORMULA_EXCEPTION_REPORT_SCHEMA_VERSION', 1);
if (!defined('HRM_LEGACY_FORMULA_EXCEPTION_RECEIPT_SCHEMA_VERSION'))
    define('HRM_LEGACY_FORMULA_EXCEPTION_RECEIPT_SCHEMA_VERSION', 1);
if (!defined('HRM_LEGACY_FORMULA_EXCEPTION_MAX_ARTIFACT_BYTES'))
    define('HRM_LEGACY_FORMULA_EXCEPTION_MAX_ARTIFACT_BYTES', 1048576);

/**
 * Return a canonical SHA-256 for one bounded governance payload.
 *
 * @param mixed $value
 * @return string|false
 */
function hrm_legacy_formula_review_payload_hash($value) {
    $json = domain_audit_canonical_json($value);
    return is_string($json) ? hash('sha256', $json) : false;
}

/**
 * Return a non-identifying fingerprint for an authorized operating-system
 * identity. Raw account names are never written to review artifacts.
 *
 * @param string $identity
 * @return string|false
 */
function hrm_legacy_formula_review_identity_fingerprint($identity) {
    if (!is_string($identity) || $identity === '' || strlen($identity) > 128)
        return false;
    return 'sha256:'.hash('sha256', strtolower($identity));
}

/**
 * Return one configured role identity without exposing it to artifact output.
 *
 * @param int $company
 * @param string $role data_owner or security_reviewer
 * @return string
 */
function hrm_legacy_formula_review_expected_os_user($company, $role) {
    global $db_connections;

    if ($role === 'data_owner') {
        if (defined('PAYROLL_FORMULA_DATA_OWNER_OS_USER'))
            return (string)PAYROLL_FORMULA_DATA_OWNER_OS_USER;
        return isset($db_connections[$company]['payroll_formula_data_owner_os_user'])
            ? (string)$db_connections[$company]['payroll_formula_data_owner_os_user'] : '';
    }
    if ($role === 'security_reviewer') {
        if (defined('PAYROLL_FORMULA_SECURITY_REVIEWER_OS_USER'))
            return (string)PAYROLL_FORMULA_SECURITY_REVIEWER_OS_USER;
        return isset($db_connections[$company]['payroll_formula_security_reviewer_os_user'])
            ? (string)$db_connections[$company]['payroll_formula_security_reviewer_os_user'] : '';
    }
    return '';
}

/**
 * Require the exact configured CLI operating-system identity for one role.
 *
 * @param int $company
 * @param string $role
 * @return string|false
 */
function hrm_legacy_formula_review_authorized_identity($company, $role) {
    if (PHP_SAPI !== 'cli')
        return false;
    $expected = hrm_legacy_formula_review_expected_os_user($company, $role);
    $actual = domain_audit_key_admin_os_identity();
    if ($expected === '' || strlen($expected) > 128
        || preg_match('/^[A-Za-z0-9_.@\\\\-]+$/D', $expected) !== 1
        || $actual === false
        || !domain_audit_hash_equals(strtolower($expected), strtolower($actual))) {
        return false;
    }
    return $actual;
}

/**
 * Enforce configured data-owner/security-reviewer separation.
 *
 * @param int $company
 * @return bool
 */
function hrm_legacy_formula_review_roles_separated($company) {
    $owner = hrm_legacy_formula_review_expected_os_user($company, 'data_owner');
    $reviewer = hrm_legacy_formula_review_expected_os_user($company, 'security_reviewer');
    return $owner !== '' && $reviewer !== ''
        && !domain_audit_hash_equals(strtolower($owner), strtolower($reviewer));
}

/**
 * Return whether a configured company is at a database version supported by
 * the exception-review workflow.
 *
 * The predecessor version must remain reviewable so a company with pending
 * exceptions can create governed evidence before the 1.0.47 activation gate.
 *
 * @param mixed $version
 * @return bool
 */
function hrm_legacy_formula_review_company_version_is_reviewable($version) {
    return is_string($version)
        && in_array($version, array('1.0.46', '1.0.47', '1.0.48'), true);
}

/**
 * Validate a canonical UTC timestamp.
 *
 * @param mixed $value
 * @return bool
 */
function hrm_legacy_formula_review_is_utc_timestamp($value) {
    if (!is_string($value)
        || preg_match('/^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})Z$/D', $value, $parts) !== 1)
        return false;
    return checkdate((int)$parts[2], (int)$parts[3], (int)$parts[1])
        && (int)$parts[4] <= 23 && (int)$parts[5] <= 59 && (int)$parts[6] <= 59;
}

/**
 * Read a bounded regular JSON artifact without following a symlink.
 *
 * @param string $path
 * @param string|null $error
 * @return array|false
 */
function hrm_legacy_formula_review_read_artifact($path, &$error = null) {
    $error = null;
    if (!is_string($path) || $path === '' || is_link($path) || !is_file($path)) {
        $error = 'artifact_missing';
        return false;
    }
    $size = @filesize($path);
    if ($size === false || $size < 2 || $size > HRM_LEGACY_FORMULA_EXCEPTION_MAX_ARTIFACT_BYTES) {
        $error = 'artifact_size_invalid';
        return false;
    }
    $bytes = @file_get_contents($path);
    if ($bytes === false) {
        $error = 'artifact_unreadable';
        return false;
    }
    $decoded = json_decode($bytes, true);
    if (!is_array($decoded) || json_last_error() !== JSON_ERROR_NONE) {
        $error = 'artifact_json_invalid';
        return false;
    }
    return $decoded;
}

/**
 * Write a new private JSON artifact. Existing paths and symlinks are denied.
 * A failed write removes only the file created by this attempt.
 *
 * @param string $path
 * @param array $artifact
 * @param string|null $error
 * @return string|false File SHA-256.
 */
function hrm_legacy_formula_review_write_artifact($path, array $artifact, &$error = null) {
    $error = null;
    if (!is_string($path) || $path === '' || strpos($path, "\0") !== false
        || file_exists($path) || is_link($path)) {
        $error = 'output_path_invalid';
        return false;
    }
    $directory = dirname($path);
    $real_directory = realpath($directory);
    if ($real_directory === false || !is_dir($real_directory) || !is_writable($real_directory)) {
        $error = 'output_directory_invalid';
        return false;
    }
    $bytes = json_encode($artifact, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES);
    if (!is_string($bytes) || strlen($bytes) > HRM_LEGACY_FORMULA_EXCEPTION_MAX_ARTIFACT_BYTES) {
        $error = 'output_payload_invalid';
        return false;
    }
    $bytes .= "\n";
    $previous_umask = umask(0077);
    $handle = @fopen($path, 'x+b');
    umask($previous_umask);
    if ($handle === false) {
        $error = 'output_create_failed';
        return false;
    }
    $stat = @fstat($handle);
    $private = @chmod($path, 0600);
    if (!is_array($stat)
        || (($stat['mode'] & 0170000) !== 0100000)
        || (isset($stat['nlink']) && (int)$stat['nlink'] !== 1)
        || (DIRECTORY_SEPARATOR === '/' && (!$private || ((@fileperms($path) & 0777) !== 0600)))) {
        @fclose($handle);
        @unlink($path);
        $error = 'output_permissions_invalid';
        return false;
    }
    $written = 0;
    $length = strlen($bytes);
    while ($written < $length) {
        $part = @fwrite($handle, substr($bytes, $written));
        if ($part === false || $part === 0)
            break;
        $written += $part;
    }
    $ok = $written === $length && @fflush($handle);
    if ($ok && function_exists('fsync'))
        $ok = @fsync($handle);
    @fclose($handle);
    if (!$ok) {
        @unlink($path);
        $error = 'output_write_failed';
        return false;
    }
    $hash = @hash_file('sha256', $path);
    if (!is_string($hash)) {
        @unlink($path);
        $error = 'output_hash_failed';
        return false;
    }
    return $hash;
}

/**
 * Load and validate the fixed corpus and current exception register.
 *
 * @param string $corpus_path
 * @param string $register_path
 * @param string|null $error
 * @return array|false
 */
function hrm_legacy_formula_review_load_governance($corpus_path, $register_path, &$error = null) {
    $error = null;
    $corpus = hrm_legacy_formula_load_json_artifact($corpus_path, $error);
    if ($corpus === false)
        return false;
    $validated = hrm_legacy_formula_validate_corpus($corpus, $error);
    if ($validated === false)
        return false;
    $register = hrm_legacy_formula_load_json_artifact($register_path, $error);
    if ($register === false)
        return false;
    $entries = hrm_legacy_formula_validate_exception_register($register, $error);
    if ($entries === false)
        return false;
    $corpus_hash = @hash_file('sha256', $corpus_path);
    $register_hash = @hash_file('sha256', $register_path);
    if (!is_string($corpus_hash) || !is_string($register_hash)
        || !isset($register['corpus_sha256'])
        || !domain_audit_hash_equals($register['corpus_sha256'], $corpus_hash)) {
        $error = 'governance_hash_mismatch';
        return false;
    }
    $target = hrm_legacy_formula_create_isolated_target_evaluator();
    if (!is_callable($target)) {
        $error = 'target_runtime_unavailable';
        return false;
    }
    $legacy = 'hrm_legacy_formula_evaluate_for_differential';
    $corpus_report = hrm_legacy_formula_run_corpus($corpus, $legacy, $target, $error);
    if ($corpus_report === false)
        return false;
    return array(
        'corpus' => $corpus,
        'validated_corpus' => $validated,
        'register' => $register,
        'entries' => $entries,
        'corpus_sha256' => $corpus_hash,
        'register_sha256' => $register_hash,
        'corpus_report' => $corpus_report,
        'legacy_evaluator' => $legacy,
        'target_evaluator' => $target
    );
}

/**
 * Build a formula-text-free live company inventory and unsupported candidate
 * list from the fixed source catalog.
 *
 * @param array $contexts
 * @param array $entry_index
 * @param callable $legacy_evaluator
 * @param callable $target_evaluator
 * @param string|null $error
 * @return array|false
 */
function hrm_legacy_formula_exception_review_inventory(
    array $contexts,
    array $entry_index,
    $legacy_evaluator,
    $target_evaluator,
    &$error = null
) {
    $error = null;
    $sources = array();
    $exceptions = array();
    $as_of = gmdate('Y-m-d');
    foreach (hrm_legacy_formula_source_catalog() as $source => $definition) {
        $result = db_query(
            'SELECT TRIM('.$definition['column'].') AS formula_source, COUNT(*) AS row_count'
            .' FROM '.TB_PREF.$definition['table']
            .' WHERE TRIM(COALESCE('.$definition['column'].", '')) <> ''"
            .' GROUP BY TRIM('.$definition['column'].')'
            .' ORDER BY SHA2(TRIM('.$definition['column'].'), 256)',
            'could not build the redacted legacy formula exception review'
        );
        if ($result === false) {
            $error = 'review_inventory_query_failed';
            return false;
        }
        $summary = array(
            'formula_count' => 0,
            'distinct_formula_count' => 0,
            'exact_count' => 0,
            'translated_count' => 0,
            'unsupported_count' => 0,
            'unsupported_distinct_count' => 0,
            'effective_exception_count' => 0,
            'pending_exception_count' => 0,
            'classification_set_sha256' => ''
        );
        $parts = array();
        while ($row = db_fetch_assoc($result)) {
            $row_count = (int)$row['row_count'];
            if ($row_count < 1) {
                $error = 'review_inventory_row_count_invalid';
                return false;
            }
            $classification = hrm_legacy_formula_differential_classify(
                $row['formula_source'], $contexts, $legacy_evaluator, $target_evaluator
            );
            $summary['formula_count'] += $row_count;
            $summary['distinct_formula_count']++;
            $summary[$classification['classification'].'_count'] += $row_count;
            $effective = false;
            if ($classification['classification'] === 'unsupported') {
                if (count($exceptions) >= HRM_LEGACY_FORMULA_MAX_EXCEPTION_ENTRIES) {
                    $error = 'review_exception_limit_exceeded';
                    return false;
                }
                $summary['unsupported_distinct_count']++;
                $effective = hrm_legacy_formula_exception_is_effective(
                    $entry_index,
                    $classification['formula_hash'],
                    $source,
                    $as_of,
                    $classification['reason']
                );
                if ($effective)
                    $summary['effective_exception_count'] += $row_count;
                else
                    $summary['pending_exception_count'] += $row_count;
                $exceptions[] = array(
                    'formula_hash' => $classification['formula_hash'],
                    'source' => $source,
                    'reason_code' => $classification['reason'],
                    'row_count' => $row_count,
                    'currently_effective' => $effective
                );
            }
            $parts[] = $classification['formula_hash'].':'.$classification['classification']
                .':'.$classification['reason'].':'.$row_count;
        }
        sort($parts, SORT_STRING);
        $summary['classification_set_sha256'] = hash('sha256', implode('|', $parts));
        $sources[$source] = $summary;
    }
    usort($exceptions, function($left, $right) {
        $a = $left['source'].'|'.$left['formula_hash'].'|'.$left['reason_code'];
        $b = $right['source'].'|'.$right['formula_hash'].'|'.$right['reason_code'];
        return strcmp($a, $b);
    });
    $inventory = array('sources' => $sources, 'exceptions' => $exceptions);
    $hash = hrm_legacy_formula_review_payload_hash($inventory);
    if ($hash === false) {
        $error = 'review_inventory_hash_failed';
        return false;
    }
    $inventory['inventory_sha256'] = $hash;
    return $inventory;
}

/**
 * Build one configured-company report without formula text or payroll context.
 *
 * @param int $company
 * @param string $owner_id
 * @param string $owner_os_identity
 * @param string $corpus_path
 * @param string $register_path
 * @param string|null $generated_at
 * @param string|null $error
 * @return array|false
 */
function hrm_legacy_formula_exception_review_build_report(
    $company,
    $owner_id,
    $owner_os_identity,
    $corpus_path,
    $register_path,
    $generated_at = null,
    &$error = null
) {
    $error = null;
    if ((int)$company < 0 || !hrm_legacy_formula_is_safe_token($owner_id)
        || !is_string($owner_os_identity) || $owner_os_identity === '') {
        $error = 'report_identity_invalid';
        return false;
    }
    $governance = hrm_legacy_formula_review_load_governance(
        $corpus_path, $register_path, $error
    );
    if ($governance === false)
        return false;
    $inventory = hrm_legacy_formula_exception_review_inventory(
        $governance['validated_corpus']['migration_contexts'],
        $governance['entries'],
        $governance['legacy_evaluator'],
        $governance['target_evaluator'],
        $error
    );
    if ($inventory === false)
        return false;
    $fingerprint = hrm_legacy_formula_review_identity_fingerprint($owner_os_identity);
    $generated_at = $generated_at === null ? gmdate('Y-m-d\TH:i:s\Z') : $generated_at;
    if ($fingerprint === false || !hrm_legacy_formula_review_is_utc_timestamp($generated_at)) {
        $error = 'report_metadata_invalid';
        return false;
    }
    $report = array(
        'schema_version' => HRM_LEGACY_FORMULA_EXCEPTION_REPORT_SCHEMA_VERSION,
        'contract' => array(
            'id' => HRM_LEGACY_FORMULA_EXCEPTION_REVIEW_ID,
            'version' => HRM_LEGACY_FORMULA_EXCEPTION_REVIEW_VERSION
        ),
        'artifact_type' => 'configured_company_exception_report',
        'data_policy' => array(
            'classification' => 'restricted-redacted-governance',
            'contains_formula_text' => false,
            'contains_payroll_context' => false
        ),
        'company_id' => (int)$company,
        'source_version' => HRM_LEGACY_FORMULA_EXCEPTION_REVIEW_SOURCE_VERSION,
        'generated_at_utc' => $generated_at,
        'generated_by_role' => 'payroll-data-owner',
        'generated_by' => $owner_id,
        'generated_by_os_fingerprint' => $fingerprint,
        'corpus_sha256' => $governance['corpus_sha256'],
        'current_register_sha256' => $governance['register_sha256'],
        'corpus_report_sha256' => $governance['corpus_report']['report_hash'],
        'inventory' => $inventory
    );
    $report_hash = hrm_legacy_formula_review_payload_hash($report);
    if ($report_hash === false) {
        $error = 'report_hash_failed';
        return false;
    }
    $report['report_sha256'] = $report_hash;
    return $report;
}

/**
 * Validate one redacted configured-company report.
 *
 * @param array $report
 * @param string|null $error
 * @return bool
 */
function hrm_legacy_formula_exception_review_validate_report(array $report, &$error = null) {
    $error = null;
    $root_fields = array(
        'schema_version', 'contract', 'artifact_type', 'data_policy', 'company_id',
        'source_version', 'generated_at_utc', 'generated_by_role', 'generated_by',
        'generated_by_os_fingerprint', 'corpus_sha256', 'current_register_sha256',
        'corpus_report_sha256', 'inventory', 'report_sha256'
    );
    if (!hrm_legacy_formula_has_only_keys($report, $root_fields)
        || !isset($report['schema_version'], $report['contract'], $report['artifact_type'],
            $report['data_policy'], $report['company_id'], $report['source_version'],
            $report['generated_at_utc'], $report['generated_by_role'], $report['generated_by'],
            $report['generated_by_os_fingerprint'], $report['corpus_sha256'],
            $report['current_register_sha256'], $report['corpus_report_sha256'],
            $report['inventory'], $report['report_sha256'])) {
        $error = 'report_fields_invalid';
        return false;
    }
    if ((int)$report['schema_version'] !== HRM_LEGACY_FORMULA_EXCEPTION_REPORT_SCHEMA_VERSION
        || !is_array($report['contract'])
        || !hrm_legacy_formula_has_only_keys($report['contract'], array('id', 'version'))
        || $report['contract']['id'] !== HRM_LEGACY_FORMULA_EXCEPTION_REVIEW_ID
        || $report['contract']['version'] !== HRM_LEGACY_FORMULA_EXCEPTION_REVIEW_VERSION
        || $report['artifact_type'] !== 'configured_company_exception_report'
        || !is_array($report['data_policy'])
        || !hrm_legacy_formula_has_only_keys($report['data_policy'], array(
            'classification', 'contains_formula_text', 'contains_payroll_context'
        ))
        || $report['data_policy']['classification'] !== 'restricted-redacted-governance'
        || $report['data_policy']['contains_formula_text'] !== false
        || $report['data_policy']['contains_payroll_context'] !== false
        || !is_int($report['company_id']) || $report['company_id'] < 0
        || $report['source_version'] !== HRM_LEGACY_FORMULA_EXCEPTION_REVIEW_SOURCE_VERSION
        || !hrm_legacy_formula_review_is_utc_timestamp($report['generated_at_utc'])
        || $report['generated_by_role'] !== 'payroll-data-owner'
        || !hrm_legacy_formula_is_safe_token($report['generated_by'])
        || !is_string($report['generated_by_os_fingerprint'])
        || preg_match('/^sha256:[a-f0-9]{64}$/D', $report['generated_by_os_fingerprint']) !== 1) {
        $error = 'report_metadata_invalid';
        return false;
    }
    foreach (array('corpus_sha256', 'current_register_sha256', 'corpus_report_sha256', 'report_sha256') as $hash_field) {
        if (!is_string($report[$hash_field])
            || preg_match('/^[a-f0-9]{64}$/D', $report[$hash_field]) !== 1) {
            $error = 'report_hash_invalid';
            return false;
        }
    }
    $inventory = $report['inventory'];
    if (!is_array($inventory)
        || !hrm_legacy_formula_has_only_keys($inventory, array('sources', 'exceptions', 'inventory_sha256'))
        || !isset($inventory['sources'], $inventory['exceptions'], $inventory['inventory_sha256'])
        || !is_array($inventory['sources']) || !is_array($inventory['exceptions'])
        || count($inventory['exceptions']) > HRM_LEGACY_FORMULA_MAX_EXCEPTION_ENTRIES
        || !is_string($inventory['inventory_sha256'])
        || preg_match('/^[a-f0-9]{64}$/D', $inventory['inventory_sha256']) !== 1) {
        $error = 'report_inventory_invalid';
        return false;
    }
    $catalog = hrm_legacy_formula_source_catalog();
    if (array_keys($inventory['sources']) !== array_keys($catalog)) {
        $error = 'report_sources_invalid';
        return false;
    }
    $summary_fields = array(
        'formula_count', 'distinct_formula_count', 'exact_count', 'translated_count',
        'unsupported_count', 'unsupported_distinct_count', 'effective_exception_count',
        'pending_exception_count', 'classification_set_sha256'
    );
    foreach ($inventory['sources'] as $source => $summary) {
        if (!is_array($summary) || !hrm_legacy_formula_has_only_keys($summary, $summary_fields)) {
            $error = 'report_source_summary_invalid';
            return false;
        }
        foreach (array_slice($summary_fields, 0, 8) as $field) {
            if (!isset($summary[$field]) || !is_int($summary[$field]) || $summary[$field] < 0) {
                $error = 'report_source_count_invalid';
                return false;
            }
        }
        if (!isset($summary['classification_set_sha256'])
            || preg_match('/^[a-f0-9]{64}$/D', $summary['classification_set_sha256']) !== 1
            || $summary['formula_count'] !== $summary['exact_count'] + $summary['translated_count'] + $summary['unsupported_count']
            || $summary['unsupported_count'] !== $summary['effective_exception_count'] + $summary['pending_exception_count']) {
            $error = 'report_source_summary_mismatch';
            return false;
        }
    }
    $seen = array();
    $exception_totals = array();
    foreach ($catalog as $source => $unused) {
        $exception_totals[$source] = array(
            'unsupported_count' => 0,
            'unsupported_distinct_count' => 0,
            'effective_exception_count' => 0,
            'pending_exception_count' => 0
        );
    }
    $last_sort_key = '';
    foreach ($inventory['exceptions'] as $exception) {
        if (!is_array($exception)
            || !hrm_legacy_formula_has_only_keys($exception, array(
                'formula_hash', 'source', 'reason_code', 'row_count', 'currently_effective'
            ))
            || !isset($exception['formula_hash'], $exception['source'], $exception['reason_code'],
                $exception['row_count'], $exception['currently_effective'])
            || preg_match('/^[a-f0-9]{64}$/D', $exception['formula_hash']) !== 1
            || !isset($catalog[$exception['source']])
            || !in_array($exception['reason_code'], array(
                'legacy_source_unsupported', 'facade_compile_or_execution_error',
                'semantic_mismatch', 'translation_unavailable'
            ), true)
            || !is_int($exception['row_count']) || $exception['row_count'] < 1
            || !is_bool($exception['currently_effective'])) {
            $error = 'report_exception_invalid';
            return false;
        }
        $key = $exception['source'].'|'.$exception['formula_hash'].'|'.$exception['reason_code'];
        if (isset($seen[$key]) || ($last_sort_key !== '' && strcmp($last_sort_key, $key) > 0)) {
            $error = 'report_exception_order_invalid';
            return false;
        }
        $seen[$key] = true;
        $last_sort_key = $key;
        $exception_totals[$exception['source']]['unsupported_count'] += $exception['row_count'];
        $exception_totals[$exception['source']]['unsupported_distinct_count']++;
        $effective_field = $exception['currently_effective']
            ? 'effective_exception_count' : 'pending_exception_count';
        $exception_totals[$exception['source']][$effective_field] += $exception['row_count'];
    }
    foreach ($inventory['sources'] as $source => $summary) {
        if ($summary['distinct_formula_count'] > $summary['formula_count']
            || $summary['unsupported_distinct_count'] > $summary['distinct_formula_count']
            || $summary['unsupported_count'] !== $exception_totals[$source]['unsupported_count']
            || $summary['unsupported_distinct_count'] !== $exception_totals[$source]['unsupported_distinct_count']
            || $summary['effective_exception_count'] !== $exception_totals[$source]['effective_exception_count']
            || $summary['pending_exception_count'] !== $exception_totals[$source]['pending_exception_count']) {
            $error = 'report_exception_summary_mismatch';
            return false;
        }
    }
    $inventory_payload = $inventory;
    unset($inventory_payload['inventory_sha256']);
    $inventory_hash = hrm_legacy_formula_review_payload_hash($inventory_payload);
    if ($inventory_hash === false
        || !domain_audit_hash_equals($inventory['inventory_sha256'], $inventory_hash)) {
        $error = 'report_inventory_hash_mismatch';
        return false;
    }
    $report_payload = $report;
    unset($report_payload['report_sha256']);
    $report_hash = hrm_legacy_formula_review_payload_hash($report_payload);
    if ($report_hash === false || !domain_audit_hash_equals($report['report_sha256'], $report_hash)) {
        $error = 'report_hash_mismatch';
        return false;
    }
    return true;
}

/**
 * Re-run the current company inventory and ensure a report is still current.
 *
 * @param array $report
 * @param int $company
 * @param string $corpus_path
 * @param string $register_path
 * @param string|null $error
 * @param array|null $current_state Validated governance state used by approval.
 * @return bool
 */
function hrm_legacy_formula_exception_review_report_is_current(
    array $report,
    $company,
    $corpus_path,
    $register_path,
    &$error = null,
    &$current_state = null
) {
    $current_state = null;
    if (!hrm_legacy_formula_exception_review_validate_report($report, $error))
        return false;
    if ($report['company_id'] !== (int)$company) {
        $error = 'report_company_mismatch';
        return false;
    }
    $governance = hrm_legacy_formula_review_load_governance(
        $corpus_path, $register_path, $error
    );
    if ($governance === false)
        return false;
    $inventory = hrm_legacy_formula_exception_review_inventory(
        $governance['validated_corpus']['migration_contexts'],
        $governance['entries'],
        $governance['legacy_evaluator'],
        $governance['target_evaluator'],
        $error
    );
    if ($inventory === false)
        return false;
    if (!domain_audit_hash_equals($report['corpus_sha256'], $governance['corpus_sha256'])
        || !domain_audit_hash_equals($report['current_register_sha256'], $governance['register_sha256'])
        || !domain_audit_hash_equals($report['corpus_report_sha256'], $governance['corpus_report']['report_hash'])
        || !domain_audit_hash_equals($report['inventory']['inventory_sha256'], $inventory['inventory_sha256'])) {
        $error = 'report_stale';
        return false;
    }
    $current_state = array(
        'register' => $governance['register'],
        'register_sha256' => $governance['register_sha256'],
        'inventory_sha256' => $inventory['inventory_sha256']
    );
    return true;
}

/**
 * Build the existing schema-v1 exception register from an approved report.
 *
 * @param array $report
 * @param array $current_register Current governed register to preserve/extend.
 * @param string $current_register_sha256 Exact file hash bound by the report.
 * @param string $approver_id
 * @param string $evidence_id
 * @param string $approved_on
 * @param string $review_due_on
 * @param string|null $error
 * @return array|false
 */
function hrm_legacy_formula_exception_review_build_register(
    array $report,
    array $current_register,
    $current_register_sha256,
    $approver_id,
    $evidence_id,
    $approved_on,
    $review_due_on,
    &$error = null
) {
    $error = null;
    $current_entries = hrm_legacy_formula_validate_exception_register($current_register, $error);
    if (!hrm_legacy_formula_exception_review_validate_report($report, $error)
        || $current_entries === false
        || !isset($current_register['corpus_sha256'])
        || !domain_audit_hash_equals($current_register['corpus_sha256'], $report['corpus_sha256'])
        || !is_string($current_register_sha256)
        || preg_match('/^[a-f0-9]{64}$/D', $current_register_sha256) !== 1
        || !domain_audit_hash_equals($current_register_sha256, $report['current_register_sha256'])
        || !hrm_legacy_formula_is_safe_token($approver_id)
        || !hrm_legacy_formula_is_safe_token($evidence_id, 64)
        || !hrm_legacy_formula_is_iso_date($approved_on)
        || !hrm_legacy_formula_is_iso_date($review_due_on)
        || strcmp($review_due_on, $approved_on) < 0
        || domain_audit_hash_equals(strtolower($report['generated_by']), strtolower($approver_id))) {
        $error = 'approval_metadata_invalid';
        return false;
    }
    $scoped = array();
    foreach ($current_register['entries'] as $entry) {
        foreach ($entry['source_scope'] as $source) {
            $scoped[$entry['formula_hash'].'|'.$source] = array(
                'formula_hash' => $entry['formula_hash'],
                'source' => $source,
                'reason_code' => $entry['reason_code'],
                'disposition' => $entry['disposition'],
                'status' => $entry['status'],
                'owner' => $entry['owner'],
                'approved_by' => $entry['approved_by'],
                'approved_on' => $entry['approved_on'],
                'review_due_on' => $entry['review_due_on'],
                'evidence_id' => $entry['evidence_id']
            );
        }
    }
    foreach ($report['inventory']['exceptions'] as $exception) {
        $entry_evidence = $evidence_id.'-'.substr($exception['formula_hash'], 0, 12);
        $scoped[$exception['formula_hash'].'|'.$exception['source']] = array(
            'formula_hash' => $exception['formula_hash'],
            'source' => $exception['source'],
            'reason_code' => $exception['reason_code'],
            'disposition' => 'legacy_only_until_manual_rewrite',
            'status' => 'approved',
            'owner' => $report['generated_by'],
            'approved_by' => $approver_id,
            'approved_on' => $approved_on,
            'review_due_on' => $review_due_on,
            'evidence_id' => $entry_evidence
        );
    }
    ksort($scoped, SORT_STRING);
    $grouped = array();
    foreach ($scoped as $item) {
        $key = implode('|', array(
            $item['formula_hash'], $item['reason_code'], $item['disposition'],
            $item['status'], $item['owner'], $item['approved_by'], $item['approved_on'],
            $item['review_due_on'], $item['evidence_id']
        ));
        if (!isset($grouped[$key])) {
            $grouped[$key] = $item;
            $grouped[$key]['source_scope'] = array();
            unset($grouped[$key]['source']);
        }
        $grouped[$key]['source_scope'][] = $item['source'];
    }
    ksort($grouped, SORT_STRING);
    $entries = array();
    foreach ($grouped as $item) {
        sort($item['source_scope'], SORT_STRING);
        $entries[] = array(
            'formula_hash' => $item['formula_hash'],
            'source_scope' => $item['source_scope'],
            'reason_code' => $item['reason_code'],
            'disposition' => $item['disposition'],
            'status' => $item['status'],
            'owner' => $item['owner'],
            'approved_by' => $item['approved_by'],
            'approved_on' => $item['approved_on'],
            'review_due_on' => $item['review_due_on'],
            'evidence_id' => $item['evidence_id']
        );
    }
    $register = array(
        'schema_version' => HRM_LEGACY_FORMULA_EXCEPTION_SCHEMA_VERSION,
        'contract' => array(
            'id' => HRM_LEGACY_FORMULA_DIFFERENTIAL_ID,
            'version' => HRM_LEGACY_FORMULA_DIFFERENTIAL_VERSION
        ),
        'data_policy' => array(
            'classification' => 'restricted-redacted-governance',
            'contains_formula_text' => false,
            'contains_payroll_context' => false
        ),
        'governance' => array(
            'owner_role' => 'payroll-data-owner',
            'reviewer_role' => 'payroll-security-reviewer',
            'approval_required' => true,
            'formula_text_forbidden' => true,
            'default_disposition' => 'legacy_only_until_manual_rewrite'
        ),
        'corpus_sha256' => $report['corpus_sha256'],
        'entries' => $entries
    );
    if (hrm_legacy_formula_validate_exception_register($register, $error) === false)
        return false;
    return $register;
}

/**
 * Build a receipt that binds the data-owner report to the approved register.
 *
 * @param array $report
 * @param string $register_sha256
 * @param string $approver_id
 * @param string $approver_os_identity
 * @param string $evidence_id
 * @param string $approved_at
 * @param string $review_due_on
 * @param int $entry_count
 * @param string|null $error
 * @return array|false
 */
function hrm_legacy_formula_exception_review_build_receipt(
    array $report,
    $register_sha256,
    $approver_id,
    $approver_os_identity,
    $evidence_id,
    $approved_at,
    $review_due_on,
    $entry_count,
    &$error = null
) {
    $error = null;
    $fingerprint = hrm_legacy_formula_review_identity_fingerprint($approver_os_identity);
    if (!hrm_legacy_formula_exception_review_validate_report($report, $error)
        || !is_string($register_sha256)
        || preg_match('/^[a-f0-9]{64}$/D', $register_sha256) !== 1
        || !hrm_legacy_formula_is_safe_token($approver_id)
        || !hrm_legacy_formula_is_safe_token($evidence_id, 64)
        || $fingerprint === false
        || domain_audit_hash_equals(strtolower($report['generated_by']), strtolower($approver_id))
        || domain_audit_hash_equals($fingerprint, $report['generated_by_os_fingerprint'])
        || !hrm_legacy_formula_review_is_utc_timestamp($approved_at)
        || !hrm_legacy_formula_is_iso_date($review_due_on)
        || strcmp($review_due_on, substr($approved_at, 0, 10)) < 0
        || !is_int($entry_count) || $entry_count < 0) {
        $error = 'receipt_metadata_invalid';
        return false;
    }
    $receipt = array(
        'schema_version' => HRM_LEGACY_FORMULA_EXCEPTION_RECEIPT_SCHEMA_VERSION,
        'contract' => array(
            'id' => HRM_LEGACY_FORMULA_EXCEPTION_REVIEW_ID,
            'version' => HRM_LEGACY_FORMULA_EXCEPTION_REVIEW_VERSION
        ),
        'artifact_type' => 'approved_exception_register_receipt',
        'data_policy' => array(
            'classification' => 'restricted-redacted-governance',
            'contains_formula_text' => false,
            'contains_payroll_context' => false
        ),
        'company_id' => $report['company_id'],
        'source_version' => HRM_LEGACY_FORMULA_EXCEPTION_REVIEW_SOURCE_VERSION,
        'report_sha256' => $report['report_sha256'],
        'inventory_sha256' => $report['inventory']['inventory_sha256'],
        'base_register_sha256' => $report['current_register_sha256'],
        'register_sha256' => $register_sha256,
        'owner' => $report['generated_by'],
        'approved_by' => $approver_id,
        'approved_by_os_fingerprint' => $fingerprint,
        'approved_at_utc' => $approved_at,
        'review_due_on' => $review_due_on,
        'evidence_id' => $evidence_id,
        'exception_entry_count' => $entry_count
    );
    $hash = hrm_legacy_formula_review_payload_hash($receipt);
    if ($hash === false) {
        $error = 'receipt_hash_failed';
        return false;
    }
    $receipt['receipt_sha256'] = $hash;
    return $receipt;
}

/**
 * Validate an approval receipt and its register binding.
 *
 * @param array $receipt
 * @param array $report
 * @param string $register_sha256
 * @param string|null $error
 * @return bool
 */
function hrm_legacy_formula_exception_review_validate_receipt(
    array $receipt,
    array $report,
    $register_sha256,
    &$error = null
) {
    $error = null;
    $fields = array(
        'schema_version', 'contract', 'artifact_type', 'data_policy', 'company_id',
        'source_version', 'report_sha256', 'inventory_sha256', 'register_sha256',
        'base_register_sha256',
        'owner', 'approved_by', 'approved_by_os_fingerprint', 'approved_at_utc',
        'review_due_on', 'evidence_id', 'exception_entry_count', 'receipt_sha256'
    );
    if (!hrm_legacy_formula_exception_review_validate_report($report, $error)
        || !hrm_legacy_formula_has_only_keys($receipt, $fields)
        || !isset($receipt['schema_version'], $receipt['contract'], $receipt['artifact_type'],
            $receipt['data_policy'], $receipt['company_id'], $receipt['source_version'],
            $receipt['report_sha256'], $receipt['inventory_sha256'], $receipt['register_sha256'],
            $receipt['base_register_sha256'],
            $receipt['owner'], $receipt['approved_by'], $receipt['approved_by_os_fingerprint'],
            $receipt['approved_at_utc'], $receipt['review_due_on'], $receipt['evidence_id'],
            $receipt['exception_entry_count'], $receipt['receipt_sha256'])) {
        $error = 'receipt_fields_invalid';
        return false;
    }
    if ((int)$receipt['schema_version'] !== HRM_LEGACY_FORMULA_EXCEPTION_RECEIPT_SCHEMA_VERSION
        || !is_array($receipt['contract'])
        || !hrm_legacy_formula_has_only_keys($receipt['contract'], array('id', 'version'))
        || $receipt['contract']['id'] !== HRM_LEGACY_FORMULA_EXCEPTION_REVIEW_ID
        || $receipt['contract']['version'] !== HRM_LEGACY_FORMULA_EXCEPTION_REVIEW_VERSION
        || $receipt['artifact_type'] !== 'approved_exception_register_receipt'
        || !is_array($receipt['data_policy'])
        || !hrm_legacy_formula_has_only_keys($receipt['data_policy'], array(
            'classification', 'contains_formula_text', 'contains_payroll_context'
        ))
        || $receipt['data_policy']['classification'] !== 'restricted-redacted-governance'
        || $receipt['data_policy']['contains_formula_text'] !== false
        || $receipt['data_policy']['contains_payroll_context'] !== false
        || $receipt['company_id'] !== $report['company_id']
        || $receipt['source_version'] !== HRM_LEGACY_FORMULA_EXCEPTION_REVIEW_SOURCE_VERSION
        || !domain_audit_hash_equals($receipt['report_sha256'], $report['report_sha256'])
        || !domain_audit_hash_equals($receipt['inventory_sha256'], $report['inventory']['inventory_sha256'])
        || !domain_audit_hash_equals($receipt['register_sha256'], $register_sha256)
        || !domain_audit_hash_equals($receipt['base_register_sha256'], $report['current_register_sha256'])
        || $receipt['owner'] !== $report['generated_by']
        || !hrm_legacy_formula_is_safe_token($receipt['approved_by'])
        || domain_audit_hash_equals(strtolower($receipt['owner']), strtolower($receipt['approved_by']))
        || preg_match('/^sha256:[a-f0-9]{64}$/D', $receipt['approved_by_os_fingerprint']) !== 1
        || domain_audit_hash_equals($receipt['approved_by_os_fingerprint'], $report['generated_by_os_fingerprint'])
        || !hrm_legacy_formula_review_is_utc_timestamp($receipt['approved_at_utc'])
        || !hrm_legacy_formula_is_iso_date($receipt['review_due_on'])
        || strcmp($receipt['review_due_on'], substr($receipt['approved_at_utc'], 0, 10)) < 0
        || !hrm_legacy_formula_is_safe_token($receipt['evidence_id'], 64)
        || !is_int($receipt['exception_entry_count']) || $receipt['exception_entry_count'] < 0
        || preg_match('/^[a-f0-9]{64}$/D', $receipt['receipt_sha256']) !== 1) {
        $error = 'receipt_metadata_invalid';
        return false;
    }
    $payload = $receipt;
    unset($payload['receipt_sha256']);
    $hash = hrm_legacy_formula_review_payload_hash($payload);
    if ($hash === false || !domain_audit_hash_equals($receipt['receipt_sha256'], $hash)) {
        $error = 'receipt_hash_mismatch';
        return false;
    }
    return true;
}
