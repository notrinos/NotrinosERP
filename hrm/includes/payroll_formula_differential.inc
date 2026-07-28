<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

include_once(dirname(__FILE__).'/payroll_formula_policy.inc');

if (!defined('HRM_LEGACY_FORMULA_DIFFERENTIAL_ID'))
    define('HRM_LEGACY_FORMULA_DIFFERENTIAL_ID', 'notrinoserp.hrm.payroll.legacy_formula_differential');
if (!defined('HRM_LEGACY_FORMULA_DIFFERENTIAL_VERSION'))
    define('HRM_LEGACY_FORMULA_DIFFERENTIAL_VERSION', '1.0.0');
if (!defined('HRM_LEGACY_FORMULA_CORPUS_SCHEMA_VERSION'))
    define('HRM_LEGACY_FORMULA_CORPUS_SCHEMA_VERSION', 1);
if (!defined('HRM_LEGACY_FORMULA_EXCEPTION_SCHEMA_VERSION'))
    define('HRM_LEGACY_FORMULA_EXCEPTION_SCHEMA_VERSION', 1);
if (!defined('HRM_LEGACY_FORMULA_MAX_CORPUS_BYTES'))
    define('HRM_LEGACY_FORMULA_MAX_CORPUS_BYTES', 262144);
if (!defined('HRM_LEGACY_FORMULA_MAX_CORPUS_CONTEXTS'))
    define('HRM_LEGACY_FORMULA_MAX_CORPUS_CONTEXTS', 32);
if (!defined('HRM_LEGACY_FORMULA_MAX_CORPUS_CASES'))
    define('HRM_LEGACY_FORMULA_MAX_CORPUS_CASES', 128);
if (!defined('HRM_LEGACY_FORMULA_MAX_EXCEPTION_ENTRIES'))
    define('HRM_LEGACY_FORMULA_MAX_EXCEPTION_ENTRIES', 512);

/**
 * Load one bounded JSON governance artifact.
 *
 * @param string $path
 * @param string|null $error
 * @return array|false
 */
function hrm_legacy_formula_load_json_artifact($path, &$error = null) {
    $error = null;
    if (!is_string($path) || $path === '' || !is_file($path)) {
        $error = 'artifact_missing';
        return false;
    }
    $size = @filesize($path);
    if ($size === false || $size < 2 || $size > HRM_LEGACY_FORMULA_MAX_CORPUS_BYTES) {
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
 * Return true when an associative array has exactly the allowed keys.
 *
 * @param array $value
 * @param array $allowed
 * @return bool
 */
function hrm_legacy_formula_has_only_keys(array $value, array $allowed) {
    $lookup = array_fill_keys($allowed, true);
    foreach ($value as $key => $unused) {
        if (!isset($lookup[$key]))
            return false;
    }
    return true;
}

/**
 * Validate a bounded token used in governance metadata.
 *
 * @param mixed $value
 * @param int $max_length
 * @return bool
 */
function hrm_legacy_formula_is_safe_token($value, $max_length = 100) {
    return is_string($value)
        && $value !== ''
        && strlen($value) <= $max_length
        && preg_match('/^[A-Za-z0-9][A-Za-z0-9_.:@-]*$/D', $value);
}


/**
 * Validate an actual Gregorian calendar date in canonical YYYY-MM-DD form.
 *
 * @param mixed $value
 * @return bool
 */
function hrm_legacy_formula_is_iso_date($value) {
    if (!is_string($value) || !preg_match('/^(\d{4})-(\d{2})-(\d{2})$/D', $value, $parts))
        return false;
    return checkdate((int)$parts[2], (int)$parts[3], (int)$parts[1]);
}

/**
 * Validate the synthetic differential corpus and return normalized maps.
 *
 * @param array $corpus
 * @param string|null $error
 * @return array|false
 */
function hrm_legacy_formula_validate_corpus(array $corpus, &$error = null) {
    $error = null;
    if (!hrm_legacy_formula_has_only_keys($corpus, array(
        'schema_version', 'contract', 'data_policy', 'contexts',
        'migration_context_ids', 'cases'
    ))) {
        $error = 'corpus_root_fields_invalid';
        return false;
    }
    if (!isset($corpus['schema_version'])
        || (int)$corpus['schema_version'] !== HRM_LEGACY_FORMULA_CORPUS_SCHEMA_VERSION) {
        $error = 'corpus_schema_invalid';
        return false;
    }
    if (!isset($corpus['contract']) || !is_array($corpus['contract'])
        || !hrm_legacy_formula_has_only_keys($corpus['contract'], array('id', 'version'))
        || !isset($corpus['contract']['id'], $corpus['contract']['version'])
        || $corpus['contract']['id'] !== HRM_LEGACY_FORMULA_DIFFERENTIAL_ID
        || $corpus['contract']['version'] !== HRM_LEGACY_FORMULA_DIFFERENTIAL_VERSION) {
        $error = 'corpus_contract_invalid';
        return false;
    }
    if (!isset($corpus['data_policy']) || !is_array($corpus['data_policy'])
        || !hrm_legacy_formula_has_only_keys($corpus['data_policy'], array(
            'classification', 'contains_production_data', 'production_derived_profiles_permitted'
        ))
        || !isset($corpus['data_policy']['classification'])
        || $corpus['data_policy']['classification'] !== 'synthetic-no-production-pii'
        || !array_key_exists('contains_production_data', $corpus['data_policy'])
        || $corpus['data_policy']['contains_production_data'] !== false
        || !array_key_exists('production_derived_profiles_permitted', $corpus['data_policy'])
        || $corpus['data_policy']['production_derived_profiles_permitted'] !== false) {
        $error = 'corpus_data_policy_invalid';
        return false;
    }
    if (!isset($corpus['contexts']) || !is_array($corpus['contexts'])
        || count($corpus['contexts']) < 1
        || count($corpus['contexts']) > HRM_LEGACY_FORMULA_MAX_CORPUS_CONTEXTS) {
        $error = 'corpus_context_count_invalid';
        return false;
    }

    $allowed_variables = hrm_legacy_formula_variable_catalog();
    $contexts = array();
    foreach ($corpus['contexts'] as $context) {
        if (!is_array($context)
            || !hrm_legacy_formula_has_only_keys($context, array('id', 'variables'))
            || !isset($context['id'], $context['variables'])
            || !hrm_legacy_formula_is_safe_token($context['id'], 64)
            || isset($contexts[$context['id']])
            || !is_array($context['variables'])) {
            $error = 'corpus_context_invalid';
            return false;
        }
        $variables = array();
        foreach ($context['variables'] as $name => $value) {
            $name = strtoupper((string)$name);
            if (!isset($allowed_variables[$name])
                || isset($variables[$name])
                || !(is_int($value) || is_float($value))) {
                $error = 'corpus_context_variable_invalid';
                return false;
            }
            $number = (float)$value;
            if (is_nan($number) || is_infinite($number)) {
                $error = 'corpus_context_value_invalid';
                return false;
            }
            $variables[$name] = $number;
        }
        foreach ($allowed_variables as $name => $unused) {
            if (!array_key_exists($name, $variables)) {
                $error = 'corpus_context_variable_missing';
                return false;
            }
        }
        ksort($variables, SORT_STRING);
        $contexts[$context['id']] = array('id' => $context['id'], 'variables' => $variables);
    }

    if (!isset($corpus['migration_context_ids']) || !is_array($corpus['migration_context_ids'])
        || count($corpus['migration_context_ids']) < 1) {
        $error = 'corpus_migration_contexts_invalid';
        return false;
    }
    $migration_contexts = array();
    foreach ($corpus['migration_context_ids'] as $id) {
        if (!is_string($id) || !isset($contexts[$id]) || isset($migration_contexts[$id])) {
            $error = 'corpus_migration_context_invalid';
            return false;
        }
        $migration_contexts[$id] = $contexts[$id];
    }

    if (!isset($corpus['cases']) || !is_array($corpus['cases'])
        || count($corpus['cases']) < 1
        || count($corpus['cases']) > HRM_LEGACY_FORMULA_MAX_CORPUS_CASES) {
        $error = 'corpus_case_count_invalid';
        return false;
    }
    $source_catalog = hrm_legacy_formula_source_catalog();
    $allowed_classes = array('exact' => true, 'translated' => true, 'unsupported' => true);
    $cases = array();
    foreach ($corpus['cases'] as $case) {
        if (!is_array($case)
            || !hrm_legacy_formula_has_only_keys($case, array(
                'id', 'source', 'formula', 'context_ids', 'expected_classification', 'expected_reason'
            ))
            || !isset($case['id'], $case['source'], $case['formula'], $case['context_ids'], $case['expected_classification'])
            || !hrm_legacy_formula_is_safe_token($case['id'], 80)
            || isset($cases[$case['id']])
            || !isset($source_catalog[$case['source']])
            || !is_string($case['formula'])
            || strlen($case['formula']) > HRM_LEGACY_FORMULA_MAX_LENGTH
            || !is_array($case['context_ids'])
            || count($case['context_ids']) < 1
            || !isset($allowed_classes[$case['expected_classification']])) {
            $error = 'corpus_case_invalid';
            return false;
        }
        $case_contexts = array();
        foreach ($case['context_ids'] as $id) {
            if (!is_string($id) || !isset($contexts[$id]) || isset($case_contexts[$id])) {
                $error = 'corpus_case_context_invalid';
                return false;
            }
            $case_contexts[$id] = $contexts[$id];
        }
        $expected_reason = isset($case['expected_reason']) ? $case['expected_reason'] : '';
        if ($expected_reason !== '' && !hrm_legacy_formula_is_safe_token($expected_reason, 100)) {
            $error = 'corpus_case_reason_invalid';
            return false;
        }
        $cases[$case['id']] = array(
            'id' => $case['id'],
            'source' => $case['source'],
            'formula' => $case['formula'],
            'formula_hash' => hrm_legacy_formula_fingerprint($case['formula']),
            'contexts' => array_values($case_contexts),
            'expected_classification' => $case['expected_classification'],
            'expected_reason' => $expected_reason
        );
    }

    return array(
        'contexts' => $contexts,
        'migration_contexts' => array_values($migration_contexts),
        'cases' => $cases
    );
}

/**
 * Validate a redacted exception register. Formula text and payroll values have
 * no field in the accepted schema.
 *
 * @param array $register
 * @param string|null $error
 * @return array|false Hash-indexed normalized entries.
 */
function hrm_legacy_formula_validate_exception_register(array $register, &$error = null) {
    $error = null;
    if (!hrm_legacy_formula_has_only_keys($register, array(
        'schema_version', 'contract', 'data_policy', 'governance', 'corpus_sha256', 'entries'
    ))) {
        $error = 'exception_root_fields_invalid';
        return false;
    }
    if (!isset($register['schema_version'])
        || (int)$register['schema_version'] !== HRM_LEGACY_FORMULA_EXCEPTION_SCHEMA_VERSION) {
        $error = 'exception_schema_invalid';
        return false;
    }
    if (!isset($register['contract']) || !is_array($register['contract'])
        || !hrm_legacy_formula_has_only_keys($register['contract'], array('id', 'version'))
        || !isset($register['contract']['id'], $register['contract']['version'])
        || $register['contract']['id'] !== HRM_LEGACY_FORMULA_DIFFERENTIAL_ID
        || $register['contract']['version'] !== HRM_LEGACY_FORMULA_DIFFERENTIAL_VERSION) {
        $error = 'exception_contract_invalid';
        return false;
    }
    if (!isset($register['data_policy']) || !is_array($register['data_policy'])
        || !hrm_legacy_formula_has_only_keys($register['data_policy'], array(
            'classification', 'contains_formula_text', 'contains_payroll_context'
        ))
        || !isset($register['data_policy']['classification'])
        || $register['data_policy']['classification'] !== 'restricted-redacted-governance'
        || !array_key_exists('contains_formula_text', $register['data_policy'])
        || $register['data_policy']['contains_formula_text'] !== false
        || !array_key_exists('contains_payroll_context', $register['data_policy'])
        || $register['data_policy']['contains_payroll_context'] !== false) {
        $error = 'exception_data_policy_invalid';
        return false;
    }
    if (!isset($register['governance']) || !is_array($register['governance'])
        || !hrm_legacy_formula_has_only_keys($register['governance'], array(
            'owner_role', 'reviewer_role', 'approval_required',
            'formula_text_forbidden', 'default_disposition'
        ))
        || !isset($register['governance']['owner_role'], $register['governance']['reviewer_role'],
            $register['governance']['approval_required'], $register['governance']['formula_text_forbidden'],
            $register['governance']['default_disposition'])
        || !hrm_legacy_formula_is_safe_token($register['governance']['owner_role'])
        || !hrm_legacy_formula_is_safe_token($register['governance']['reviewer_role'])
        || $register['governance']['approval_required'] !== true
        || $register['governance']['formula_text_forbidden'] !== true
        || $register['governance']['default_disposition'] !== 'legacy_only_until_manual_rewrite') {
        $error = 'exception_governance_invalid';
        return false;
    }
    if (!isset($register['corpus_sha256'])
        || !is_string($register['corpus_sha256'])
        || !preg_match('/^[a-f0-9]{64}$/D', $register['corpus_sha256'])) {
        $error = 'exception_corpus_hash_invalid';
        return false;
    }
    if (!isset($register['entries']) || !is_array($register['entries'])
        || count($register['entries']) > HRM_LEGACY_FORMULA_MAX_EXCEPTION_ENTRIES) {
        $error = 'exception_entry_count_invalid';
        return false;
    }

    $allowed_reasons = array(
        'legacy_source_unsupported' => true,
        'facade_compile_or_execution_error' => true,
        'semantic_mismatch' => true,
        'translation_unavailable' => true
    );
    $source_catalog = hrm_legacy_formula_source_catalog();
    $entries = array();
    foreach ($register['entries'] as $entry) {
        if (!is_array($entry)
            || !hrm_legacy_formula_has_only_keys($entry, array(
                'formula_hash', 'source_scope', 'reason_code', 'disposition', 'status',
                'owner', 'approved_by', 'approved_on', 'review_due_on', 'evidence_id'
            ))
            || !isset($entry['formula_hash'], $entry['source_scope'], $entry['reason_code'],
                $entry['disposition'], $entry['status'], $entry['owner'], $entry['approved_by'],
                $entry['approved_on'], $entry['review_due_on'], $entry['evidence_id'])
            || !is_string($entry['formula_hash'])
            || !preg_match('/^[a-f0-9]{64}$/D', $entry['formula_hash'])
            || !is_array($entry['source_scope']) || count($entry['source_scope']) < 1
            || !isset($allowed_reasons[$entry['reason_code']])
            || $entry['disposition'] !== 'legacy_only_until_manual_rewrite'
            || $entry['status'] !== 'approved'
            || !hrm_legacy_formula_is_safe_token($entry['owner'])
            || !hrm_legacy_formula_is_safe_token($entry['approved_by'])
            || !hrm_legacy_formula_is_iso_date($entry['approved_on'])
            || !hrm_legacy_formula_is_iso_date($entry['review_due_on'])
            || strcmp($entry['review_due_on'], $entry['approved_on']) < 0
            || !hrm_legacy_formula_is_safe_token($entry['evidence_id'])) {
            $error = 'exception_entry_invalid';
            return false;
        }
        $scopes = array();
        foreach ($entry['source_scope'] as $source) {
            if (!is_string($source) || !isset($source_catalog[$source]) || isset($scopes[$source])) {
                $error = 'exception_source_scope_invalid';
                return false;
            }
            $scopes[$source] = true;
        }
        ksort($scopes, SORT_STRING);
        $normalized = $entry;
        $normalized['source_scope'] = array_keys($scopes);
        if (!isset($entries[$entry['formula_hash']]))
            $entries[$entry['formula_hash']] = array();
        foreach ($normalized['source_scope'] as $source) {
            if (isset($entries[$entry['formula_hash']][$source])) {
                $error = 'exception_entry_duplicate';
                return false;
            }
            $entries[$entry['formula_hash']][$source] = $normalized;
        }
    }
    return $entries;
}

/**
 * Apply the only approved syntax translation in this Phase-0 slice:
 * normalize leading/trailing decimal points without changing identifiers,
 * operators, precedence, or variable bindings.
 *
 * @param string $formula
 * @return string|false
 */
function hrm_legacy_formula_translate_for_facade($formula) {
    $formula = hrm_legacy_formula_normalize($formula);
    $classification = hrm_legacy_formula_classify($formula);
    if ($classification['classification'] !== 'legacy_compatible')
        return false;

    $translated = preg_replace_callback(
        '/(^|[^0-9.])\.(\d+)/',
        function($match) { return $match[1].'0.'.$match[2]; },
        $formula
    );
    $translated = preg_replace('/(\d+)\.(?!\d)/', '$1.0', $translated);
    return $translated;
}

/**
 * Compare finite numeric results without a tolerance that could conceal a
 * payroll difference.
 *
 * @param mixed $legacy
 * @param mixed $target
 * @return bool
 */
function hrm_legacy_formula_results_equal($legacy, $target) {
    if (!is_numeric($legacy) || !is_numeric($target))
        return false;
    $legacy = (float)$legacy;
    $target = (float)$target;
    if (is_nan($legacy) || is_infinite($legacy) || is_nan($target) || is_infinite($target))
        return false;
    return sprintf('%.17g', $legacy) === sprintf('%.17g', $target);
}

/**
 * Execute one candidate against a set of synthetic contexts.
 *
 * @param string $legacy_formula
 * @param string $target_formula
 * @param array $contexts
 * @param callable $legacy_evaluator
 * @param callable $target_evaluator
 * @return array
 */
function hrm_legacy_formula_compare_candidate(
    $legacy_formula,
    $target_formula,
    array $contexts,
    $legacy_evaluator,
    $target_evaluator
) {
    $result_parts = array();
    foreach ($contexts as $context) {
        try {
            $legacy = call_user_func($legacy_evaluator, $legacy_formula, $context['variables']);
        } catch (Exception $e) {
            return array('matched' => false, 'reason' => 'legacy_execution_error', 'result_set_hash' => hash('sha256', 'legacy_error'));
        } catch (Throwable $e) {
            return array('matched' => false, 'reason' => 'legacy_execution_error', 'result_set_hash' => hash('sha256', 'legacy_error'));
        }
        try {
            $target = call_user_func($target_evaluator, $target_formula, $context['variables']);
        } catch (Exception $e) {
            return array('matched' => false, 'reason' => 'facade_compile_or_execution_error', 'result_set_hash' => hash('sha256', 'target_error'));
        } catch (Throwable $e) {
            return array('matched' => false, 'reason' => 'facade_compile_or_execution_error', 'result_set_hash' => hash('sha256', 'target_error'));
        }
        $result_parts[] = $context['id'].':'.hash('sha256', sprintf('%.17g', (float)$legacy));
        if (!hrm_legacy_formula_results_equal($legacy, $target)) {
            return array(
                'matched' => false,
                'reason' => 'semantic_mismatch',
                'result_set_hash' => hash('sha256', implode('|', $result_parts))
            );
        }
    }
    return array(
        'matched' => true,
        'reason' => 'equivalent',
        'result_set_hash' => hash('sha256', implode('|', $result_parts))
    );
}

/**
 * Classify one formula as exact, deterministically translated, or unsupported.
 * Returned evidence contains only hashes, counts, and stable reason codes.
 *
 * @param string $formula
 * @param array $contexts
 * @param callable $legacy_evaluator
 * @param callable $target_evaluator
 * @return array
 */
function hrm_legacy_formula_differential_classify(
    $formula,
    array $contexts,
    $legacy_evaluator,
    $target_evaluator
) {
    $formula = hrm_legacy_formula_normalize($formula);
    $hash = hrm_legacy_formula_fingerprint($formula);
    $base = array(
        'formula_hash' => $hash,
        'classification' => 'unsupported',
        'reason' => 'translation_unavailable',
        'context_count' => count($contexts),
        'result_set_hash' => hash('sha256', ''),
        'translated_hash' => null
    );
    $legacy_classification = hrm_legacy_formula_classify($formula);
    if ($legacy_classification['classification'] !== 'legacy_compatible') {
        $base['reason'] = 'legacy_source_unsupported';
        return $base;
    }

    $exact = hrm_legacy_formula_compare_candidate(
        $formula,
        $formula,
        $contexts,
        $legacy_evaluator,
        $target_evaluator
    );
    if ($exact['matched']) {
        $base['classification'] = 'exact';
        $base['reason'] = 'equivalent_without_translation';
        $base['result_set_hash'] = $exact['result_set_hash'];
        return $base;
    }

    $translated = hrm_legacy_formula_translate_for_facade($formula);
    if ($translated !== false && !hash_equals(hash('sha256', $formula), hash('sha256', $translated))) {
        $translated_result = hrm_legacy_formula_compare_candidate(
            $formula,
            $translated,
            $contexts,
            $legacy_evaluator,
            $target_evaluator
        );
        $base['translated_hash'] = hash('sha256', $translated);
        if ($translated_result['matched']) {
            $base['classification'] = 'translated';
            $base['reason'] = 'decimal_literal_normalized';
            $base['result_set_hash'] = $translated_result['result_set_hash'];
            return $base;
        }
        $base['reason'] = $translated_result['reason'];
        $base['result_set_hash'] = $translated_result['result_set_hash'];
        return $base;
    }

    $base['reason'] = $exact['reason'];
    $base['result_set_hash'] = $exact['result_set_hash'];
    return $base;
}

/**
 * Run and verify the complete sanitized corpus.
 *
 * @param array $corpus
 * @param callable $legacy_evaluator
 * @param callable $target_evaluator
 * @param string|null $error
 * @return array|false
 */
function hrm_legacy_formula_run_corpus(
    array $corpus,
    $legacy_evaluator,
    $target_evaluator,
    &$error = null
) {
    $validated = hrm_legacy_formula_validate_corpus($corpus, $error);
    if ($validated === false)
        return false;

    $cases = array();
    $counts = array('exact' => 0, 'translated' => 0, 'unsupported' => 0);
    foreach ($validated['cases'] as $case) {
        $result = hrm_legacy_formula_differential_classify(
            $case['formula'],
            $case['contexts'],
            $legacy_evaluator,
            $target_evaluator
        );
        if ($result['classification'] !== $case['expected_classification']) {
            $error = 'corpus_classification_mismatch:'.$case['id'];
            return false;
        }
        if ($case['expected_reason'] !== '' && $result['reason'] !== $case['expected_reason']) {
            $error = 'corpus_reason_mismatch:'.$case['id'];
            return false;
        }
        $counts[$result['classification']]++;
        $cases[$case['id']] = array(
            'case_id' => $case['id'],
            'source' => $case['source'],
            'formula_hash' => $result['formula_hash'],
            'classification' => $result['classification'],
            'reason' => $result['reason'],
            'context_count' => $result['context_count'],
            'result_set_hash' => $result['result_set_hash'],
            'translated_hash' => $result['translated_hash']
        );
    }
    ksort($cases, SORT_STRING);
    return array(
        'contract' => HRM_LEGACY_FORMULA_DIFFERENTIAL_ID,
        'version' => HRM_LEGACY_FORMULA_DIFFERENTIAL_VERSION,
        'counts' => $counts,
        'cases' => $cases,
        'report_hash' => hash('sha256', json_encode(array($counts, $cases)))
    );
}

/**
 * Verify that every unsupported sanitized case is explicitly governed and no
 * exact/translated case is placed on the exception path.
 *
 * @param array $report
 * @param array $entry_index
 * @param string|null $error
 * @return bool
 */
function hrm_legacy_formula_verify_corpus_exception_coverage(
    array $report,
    array $entry_index,
    &$error = null
) {
    $error = null;
    $as_of = gmdate('Y-m-d');
    foreach ($report['cases'] as $case) {
        $registered = isset($entry_index[$case['formula_hash']][$case['source']]);
        if ($case['classification'] !== 'unsupported' && $registered) {
            $error = 'non_exception_case_registered:'.$case['case_id'];
            return false;
        }
        if ($case['classification'] !== 'unsupported')
            continue;
        if (!$registered) {
            $error = 'unsupported_case_unregistered:'.$case['case_id'];
            return false;
        }
        if ($entry_index[$case['formula_hash']][$case['source']]['reason_code'] !== $case['reason']) {
            $error = 'exception_reason_mismatch:'.$case['case_id'];
            return false;
        }
        if (!hrm_legacy_formula_exception_is_effective(
            $entry_index,
            $case['formula_hash'],
            $case['source'],
            $as_of,
            $case['reason']
        )) {
            $error = 'unsupported_case_exception_inactive:'.$case['case_id'];
            return false;
        }
    }
    return true;
}

/**
 * Create an isolated FormulaFacade-runtime evaluator without mutating the
 * process-wide FormulaFacade singleton or registries.
 *
 * @return callable|false
 */
function hrm_legacy_formula_create_isolated_target_evaluator() {
    global $path_to_root;
    $bootstrap = $path_to_root.'/includes/formula/formula_bootstrap.inc';
    if (!is_file($bootstrap))
        return false;
    require_once $bootstrap;

    $variables = array_keys(hrm_legacy_formula_variable_catalog());
    $runtime = new Formula_Runtime_FormulaRuntime(
        new Formula_Registry_FunctionRegistry(),
        new Formula_Registry_VariableRegistry(),
        $variables
    );
    return function($formula, $values) use ($runtime) {
        $context = Formula_Context_FormulaContextBuilder::create()
            ->withVariables($values)
            ->withCompatibilityMode(false)
            ->build();
        return $runtime->evaluate($formula, $context);
    };
}

/**
 * Return whether an exception entry is effective for this source and date.
 *
 * @param array $entry_index
 * @param string $hash
 * @param string $source
 * @param string $as_of YYYY-MM-DD
 * @param string|null $reason Expected differential reason code.
 * @return bool
 */
function hrm_legacy_formula_exception_is_effective(array $entry_index, $hash, $source, $as_of, $reason = null) {
    if (!hrm_legacy_formula_is_iso_date($as_of) || !isset($entry_index[$hash][$source]))
        return false;
    $entry = $entry_index[$hash][$source];
    return $entry['status'] === 'approved'
        && ($reason === null || $entry['reason_code'] === $reason)
        && $entry['approved_on'] <= $as_of
        && $entry['review_due_on'] >= $as_of;
}

/**
 * Differentially inventory the fixed production sources using only synthetic
 * contexts. Formula source and payroll context are never returned.
 *
 * @param array $contexts
 * @param array $entry_index
 * @param callable $legacy_evaluator
 * @param callable $target_evaluator
 * @param string|null $error
 * @return array|false
 */
function hrm_legacy_formula_differential_inventory(
    array $contexts,
    array $entry_index,
    $legacy_evaluator,
    $target_evaluator,
    &$error = null
) {
    $error = null;
    $inventory = array();
    $as_of = gmdate('Y-m-d');
    foreach (hrm_legacy_formula_source_catalog() as $source => $definition) {
        $table = $definition['table'];
        $column = $definition['column'];
        $result = db_query(
            "SELECT TRIM(".$column.") AS formula_source, COUNT(*) AS row_count"
            ." FROM ".TB_PREF.$table
            ." WHERE TRIM(COALESCE(".$column.", '')) <> ''"
            ." GROUP BY TRIM(".$column.") ORDER BY SHA2(TRIM(".$column."), 256)",
            'could not differentially inventory legacy payroll formulas'
        );
        if ($result === false) {
            $error = 'differential_inventory_query_failed';
            return false;
        }

        $summary = array(
            'formula_count' => 0,
            'distinct_formula_count' => 0,
            'exact_count' => 0,
            'translated_count' => 0,
            'unsupported_count' => 0,
            'registered_exception_count' => 0,
            'unregistered_exception_count' => 0,
            'classification_set_hash' => '',
            'unregistered_exception_hashes' => array()
        );
        $parts = array();
        while ($row = db_fetch_assoc($result)) {
            $row_count = (int)$row['row_count'];
            $classification = hrm_legacy_formula_differential_classify(
                $row['formula_source'],
                $contexts,
                $legacy_evaluator,
                $target_evaluator
            );
            $summary['formula_count'] += $row_count;
            $summary['distinct_formula_count']++;
            $summary[$classification['classification'].'_count'] += $row_count;
            if ($classification['classification'] === 'unsupported') {
                if (hrm_legacy_formula_exception_is_effective(
                    $entry_index,
                    $classification['formula_hash'],
                    $source,
                    $as_of,
                    $classification['reason']
                )) {
                    $summary['registered_exception_count'] += $row_count;
                } else {
                    $summary['unregistered_exception_count'] += $row_count;
                    $summary['unregistered_exception_hashes'][] = $classification['formula_hash'];
                }
            }
            $parts[] = $classification['formula_hash'].':'.$classification['classification'].':'.$row_count;
        }
        sort($summary['unregistered_exception_hashes'], SORT_STRING);
        $summary['unregistered_exception_hashes'] = array_values(array_unique($summary['unregistered_exception_hashes']));
        $summary['classification_set_hash'] = hash('sha256', implode('|', $parts));
        $inventory[$source] = $summary;
    }
    return $inventory;
}
