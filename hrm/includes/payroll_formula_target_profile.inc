<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

include_once(dirname(__FILE__).'/payroll_formula_policy.inc');
include_once(dirname(__FILE__).'/payroll_formula_differential.inc');

if (!defined('HRM_LEGACY_FORMULA_TARGET_PROFILE_ID'))
    define('HRM_LEGACY_FORMULA_TARGET_PROFILE_ID', 'notrinoserp.hrm.payroll.legacy_formula_target_profile');
if (!defined('HRM_LEGACY_FORMULA_TARGET_PROFILE_VERSION'))
    define('HRM_LEGACY_FORMULA_TARGET_PROFILE_VERSION', '1.0.0');
if (!defined('HRM_LEGACY_FORMULA_TARGET_PUBLICATION_ID'))
    define('HRM_LEGACY_FORMULA_TARGET_PUBLICATION_ID', 'notrinoserp.hrm.payroll.legacy_formula_target_publication');
if (!defined('HRM_LEGACY_FORMULA_TARGET_PUBLICATION_VERSION'))
    define('HRM_LEGACY_FORMULA_TARGET_PUBLICATION_VERSION', '1.0.0');
if (!defined('HRM_LEGACY_FORMULA_TARGET_SOURCE_VERSION'))
    define('HRM_LEGACY_FORMULA_TARGET_SOURCE_VERSION', '1.0.48');

/**
 * Return the exact ordered variable names admitted by the migration profile.
 *
 * @return array
 */
function hrm_legacy_formula_target_variable_names() {
    return array_keys(hrm_legacy_formula_variable_catalog());
}

/**
 * Validate a fixed migration-only FormulaFacade target profile artifact.
 *
 * The profile intentionally covers only the frozen legacy arithmetic subset.
 * It is not the typed decimal/date/unit payroll profile owned by PAY-RULE-002.
 *
 * @param array $profile
 * @param string|null $error
 * @return array|false Normalized profile.
 */
function hrm_legacy_formula_target_validate_profile(array $profile, &$error = null) {
    $error = null;
    $top_keys = array('schema_version', 'contract', 'purpose', 'execution', 'context_policy', 'publication');
    if (!hrm_legacy_formula_has_only_keys($profile, $top_keys)
        || !isset($profile['schema_version'], $profile['contract'], $profile['purpose'],
            $profile['execution'], $profile['context_policy'], $profile['publication'])
        || $profile['schema_version'] !== 1
        || !is_array($profile['contract'])
        || !hrm_legacy_formula_has_only_keys($profile['contract'], array('id', 'version'))
        || !isset($profile['contract']['id'], $profile['contract']['version'])
        || $profile['contract']['id'] !== HRM_LEGACY_FORMULA_TARGET_PROFILE_ID
        || $profile['contract']['version'] !== HRM_LEGACY_FORMULA_TARGET_PROFILE_VERSION
        || $profile['purpose'] !== 'migration-only-non-production-shadow') {
        $error = 'target_profile_contract_invalid';
        return false;
    }

    $execution = $profile['execution'];
    if (!is_array($execution)
        || !hrm_legacy_formula_has_only_keys($execution, array(
            'engine', 'production_routing', 'compatibility_mode', 'function_allowlist',
            'namespace_allowlist', 'operator_allowlist', 'resource_limits', 'variable_allowlist'
        ))
        || !isset($execution['engine'], $execution['production_routing'], $execution['compatibility_mode'],
            $execution['function_allowlist'], $execution['namespace_allowlist'],
            $execution['operator_allowlist'], $execution['resource_limits'], $execution['variable_allowlist'])
        || $execution['engine'] !== 'FormulaFacade-isolated-runtime'
        || $execution['production_routing'] !== false
        || $execution['compatibility_mode'] !== false
        || $execution['function_allowlist'] !== array()
        || $execution['namespace_allowlist'] !== array()
        || $execution['operator_allowlist'] !== array('+', '-', '*', '/', '%', 'unary+', 'unary-')
        || $execution['variable_allowlist'] !== hrm_legacy_formula_target_variable_names()) {
        $error = 'target_profile_execution_invalid';
        return false;
    }

    $limits = $execution['resource_limits'];
    if (!is_array($limits)
        || !hrm_legacy_formula_has_only_keys($limits, array('max_source_length', 'max_tokens', 'max_parenthesis_depth'))
        || !isset($limits['max_source_length'], $limits['max_tokens'], $limits['max_parenthesis_depth'])
        || $limits['max_source_length'] !== HRM_LEGACY_FORMULA_MAX_LENGTH
        || $limits['max_tokens'] !== HRM_LEGACY_FORMULA_MAX_TOKENS
        || $limits['max_parenthesis_depth'] !== HRM_LEGACY_FORMULA_MAX_DEPTH) {
        $error = 'target_profile_resource_policy_invalid';
        return false;
    }

    $context = $profile['context_policy'];
    if (!is_array($context)
        || !hrm_legacy_formula_has_only_keys($context, array(
            'require_exact_variables', 'finite_numeric_only', 'numeric_strings_permitted',
            'company_context_permitted', 'security_context_permitted', 'business_data_permitted'
        ))
        || !isset($context['require_exact_variables'], $context['finite_numeric_only'],
            $context['numeric_strings_permitted'], $context['company_context_permitted'],
            $context['security_context_permitted'], $context['business_data_permitted'])
        || $context['require_exact_variables'] !== true
        || $context['finite_numeric_only'] !== true
        || $context['numeric_strings_permitted'] !== false
        || $context['company_context_permitted'] !== false
        || $context['security_context_permitted'] !== false
        || $context['business_data_permitted'] !== false) {
        $error = 'target_profile_context_policy_invalid';
        return false;
    }

    $publication = $profile['publication'];
    if (!is_array($publication)
        || !hrm_legacy_formula_has_only_keys($publication, array(
            'registry_contract_id', 'registry_contract_version', 'required_status',
            'formula_text_forbidden', 'payroll_context_forbidden', 'distinct_principals'
        ))
        || !isset($publication['registry_contract_id'], $publication['registry_contract_version'],
            $publication['required_status'], $publication['formula_text_forbidden'],
            $publication['payroll_context_forbidden'], $publication['distinct_principals'])
        || $publication['registry_contract_id'] !== HRM_LEGACY_FORMULA_TARGET_PUBLICATION_ID
        || $publication['registry_contract_version'] !== HRM_LEGACY_FORMULA_TARGET_PUBLICATION_VERSION
        || $publication['required_status'] !== 'approved_for_non_production_shadow'
        || $publication['formula_text_forbidden'] !== true
        || $publication['payroll_context_forbidden'] !== true
        || $publication['distinct_principals'] !== array('author', 'reviewer', 'publisher', 'test_executor')) {
        $error = 'target_profile_publication_policy_invalid';
        return false;
    }

    return $profile;
}

/**
 * Normalize and validate the exact immutable scalar context for one execution.
 *
 * @param array $values
 * @param string|null $error
 * @return array|false
 */
function hrm_legacy_formula_target_validate_context(array $values, &$error = null) {
    $error = null;
    $allowed = hrm_legacy_formula_target_variable_names();
    if (count($values) !== count($allowed)) {
        $error = 'target_context_variable_set_mismatch';
        return false;
    }

    $normalized_input = array();
    foreach ($values as $name => $value) {
        $upper = strtoupper((string)$name);
        if ($upper === '' || isset($normalized_input[$upper])) {
            $error = 'target_context_duplicate_variable';
            return false;
        }
        $normalized_input[$upper] = $value;
    }

    $normalized = array();
    foreach ($allowed as $name) {
        if (!array_key_exists($name, $normalized_input)) {
            $error = 'target_context_variable_set_mismatch';
            return false;
        }
        $value = $normalized_input[$name];
        if (!is_int($value) && !is_float($value)) {
            $error = 'target_context_non_numeric_value';
            return false;
        }
        $number = (float)$value;
        if (is_nan($number) || is_infinite($number)) {
            $error = 'target_context_non_finite_value';
            return false;
        }
        $normalized[$name] = $number;
    }
    return $normalized;
}

/**
 * Create an isolated, provider-free target evaluator for the fixed profile.
 *
 * @param array $profile
 * @param string|null $error
 * @return callable|false
 */
function hrm_legacy_formula_target_create_evaluator(array $profile, &$error = null) {
    global $path_to_root;
    if (hrm_legacy_formula_target_validate_profile($profile, $error) === false)
        return false;

    $bootstrap = $path_to_root.'/includes/formula/formula_bootstrap.inc';
    if (!is_file($bootstrap)) {
        $error = 'target_formula_bootstrap_missing';
        return false;
    }
    require_once $bootstrap;

    $function_registry = new Formula_Registry_FunctionRegistry();
    $variable_registry = new Formula_Registry_VariableRegistry();
    $function_registry->freeze();
    $variable_registry->freeze();
    $runtime = new Formula_Runtime_FormulaRuntime(
        $function_registry,
        $variable_registry,
        hrm_legacy_formula_target_variable_names()
    );

    return function($formula, $values) use ($runtime) {
        $classification = hrm_legacy_formula_classify($formula);
        if ($classification['classification'] !== 'legacy_compatible')
            throw new RuntimeException('target_formula_source_rejected');
        $context_error = null;
        $normalized = hrm_legacy_formula_target_validate_context($values, $context_error);
        if ($normalized === false)
            throw new RuntimeException($context_error);
        $context = Formula_Context_FormulaContextBuilder::create()
            ->withVariables($normalized)
            ->withCompatibilityMode(false)
            ->build();
        $result = $runtime->evaluate($formula, $context);
        if (!is_numeric($result))
            throw new RuntimeException('target_formula_non_numeric_result');
        $number = (float)$result;
        if (is_nan($number) || is_infinite($number))
            throw new RuntimeException('target_formula_non_finite_result');
        return $number;
    };
}

/**
 * Validate the default-deny redacted publication prerequisite registry.
 *
 * @param array $registry
 * @param string $profile_sha256
 * @param string $corpus_sha256
 * @param string $exception_register_sha256
 * @param string|null $error
 * @return array|false Indexed by source formula hash then source scope.
 */
function hrm_legacy_formula_target_validate_publication_registry(
    array $registry,
    $profile_sha256,
    $corpus_sha256,
    $exception_register_sha256,
    &$error = null
) {
    $error = null;
    if (!preg_match('/^[a-f0-9]{64}$/D', (string)$profile_sha256)
        || !preg_match('/^[a-f0-9]{64}$/D', (string)$corpus_sha256)
        || !preg_match('/^[a-f0-9]{64}$/D', (string)$exception_register_sha256)) {
        $error = 'target_publication_binding_invalid';
        return false;
    }

    if (!hrm_legacy_formula_has_only_keys($registry, array(
            'schema_version', 'contract', 'data_policy', 'governance',
            'profile_sha256', 'corpus_sha256', 'exception_register_sha256', 'entries'
        ))
        || !isset($registry['schema_version'], $registry['contract'], $registry['data_policy'],
            $registry['governance'], $registry['profile_sha256'], $registry['corpus_sha256'],
            $registry['exception_register_sha256'], $registry['entries'])
        || $registry['schema_version'] !== 1
        || !is_array($registry['contract'])
        || !hrm_legacy_formula_has_only_keys($registry['contract'], array('id', 'version'))
        || !isset($registry['contract']['id'], $registry['contract']['version'])
        || $registry['contract']['id'] !== HRM_LEGACY_FORMULA_TARGET_PUBLICATION_ID
        || $registry['contract']['version'] !== HRM_LEGACY_FORMULA_TARGET_PUBLICATION_VERSION
        || !is_array($registry['entries'])) {
        $error = 'target_publication_contract_invalid';
        return false;
    }

    $data = $registry['data_policy'];
    if (!is_array($data)
        || !hrm_legacy_formula_has_only_keys($data, array(
            'classification', 'contains_formula_text', 'contains_payroll_context', 'contains_production_data'
        ))
        || !isset($data['classification'], $data['contains_formula_text'],
            $data['contains_payroll_context'], $data['contains_production_data'])
        || $data['classification'] !== 'restricted-redacted-governance'
        || $data['contains_formula_text'] !== false
        || $data['contains_payroll_context'] !== false
        || $data['contains_production_data'] !== false) {
        $error = 'target_publication_data_policy_invalid';
        return false;
    }

    $governance = $registry['governance'];
    if (!is_array($governance)
        || !hrm_legacy_formula_has_only_keys($governance, array(
            'environment', 'default_deny', 'required_status', 'formula_text_forbidden',
            'payroll_context_forbidden', 'distinct_principals', 'production_execution_forbidden'
        ))
        || !isset($governance['environment'], $governance['default_deny'],
            $governance['required_status'], $governance['formula_text_forbidden'],
            $governance['payroll_context_forbidden'], $governance['distinct_principals'],
            $governance['production_execution_forbidden'])
        || $governance['environment'] !== 'non-production-shadow-only'
        || $governance['default_deny'] !== true
        || $governance['required_status'] !== 'approved_for_non_production_shadow'
        || $governance['formula_text_forbidden'] !== true
        || $governance['payroll_context_forbidden'] !== true
        || $governance['distinct_principals'] !== true
        || $governance['production_execution_forbidden'] !== true) {
        $error = 'target_publication_governance_invalid';
        return false;
    }

    if (!hash_equals($profile_sha256, (string)$registry['profile_sha256'])
        || !hash_equals($corpus_sha256, (string)$registry['corpus_sha256'])
        || !hash_equals($exception_register_sha256, (string)$registry['exception_register_sha256'])) {
        $error = 'target_publication_binding_mismatch';
        return false;
    }

    $catalog = hrm_legacy_formula_source_catalog();
    $index = array();
    foreach ($registry['entries'] as $entry) {
        $allowed = array(
            'source_formula_sha256', 'target_formula_sha256', 'source_scope',
            'classification', 'differential_reason', 'result_set_sha256',
            'status', 'author_id', 'reviewer_id', 'publisher_id', 'test_executor_id',
            'approved_on', 'review_due_on', 'evidence_id'
        );
        if (!is_array($entry)
            || !hrm_legacy_formula_has_only_keys($entry, $allowed)) {
            $error = 'target_publication_entry_schema_invalid';
            return false;
        }
        foreach ($allowed as $required) {
            if (!array_key_exists($required, $entry)) {
                $error = 'target_publication_entry_schema_invalid';
                return false;
            }
        }
        if (!preg_match('/^[a-f0-9]{64}$/D', (string)$entry['source_formula_sha256'])
            || !preg_match('/^[a-f0-9]{64}$/D', (string)$entry['target_formula_sha256'])
            || !preg_match('/^[a-f0-9]{64}$/D', (string)$entry['result_set_sha256'])
            || !isset($catalog[$entry['source_scope']])
            || ($entry['classification'] !== 'exact' && $entry['classification'] !== 'translated')
            || ($entry['differential_reason'] !== 'equivalent_without_translation'
                && $entry['differential_reason'] !== 'decimal_literal_normalized')
            || $entry['status'] !== 'approved_for_non_production_shadow'
            || !hrm_legacy_formula_is_iso_date($entry['approved_on'])
            || !hrm_legacy_formula_is_iso_date($entry['review_due_on'])
            || $entry['approved_on'] > $entry['review_due_on']) {
            $error = 'target_publication_entry_value_invalid';
            return false;
        }
        if (($entry['classification'] === 'exact'
                && $entry['differential_reason'] !== 'equivalent_without_translation')
            || ($entry['classification'] === 'translated'
                && $entry['differential_reason'] !== 'decimal_literal_normalized')) {
            $error = 'target_publication_classification_reason_mismatch';
            return false;
        }

        $principal_ids = array(
            $entry['author_id'], $entry['reviewer_id'],
            $entry['publisher_id'], $entry['test_executor_id']
        );
        foreach ($principal_ids as $principal_id) {
            if (!hrm_legacy_formula_is_safe_token($principal_id, 100)) {
                $error = 'target_publication_principal_invalid';
                return false;
            }
        }
        if (count(array_unique($principal_ids, SORT_STRING)) !== 4) {
            $error = 'target_publication_role_separation_invalid';
            return false;
        }
        if (!hrm_legacy_formula_is_safe_token($entry['evidence_id'], 120)) {
            $error = 'target_publication_evidence_invalid';
            return false;
        }

        $hash = $entry['source_formula_sha256'];
        $source = $entry['source_scope'];
        if (isset($index[$hash][$source])) {
            $error = 'target_publication_duplicate_scope';
            return false;
        }
        if (!isset($index[$hash]))
            $index[$hash] = array();
        $index[$hash][$source] = $entry;
    }
    return $index;
}

/**
 * Return whether a registry entry is active on the supplied date.
 *
 * @param array $entry
 * @param string $as_of
 * @return bool
 */
function hrm_legacy_formula_target_publication_is_effective(array $entry, $as_of) {
    return hrm_legacy_formula_is_iso_date($as_of)
        && $entry['status'] === 'approved_for_non_production_shadow'
        && $entry['approved_on'] <= $as_of
        && $entry['review_due_on'] >= $as_of;
}

/**
 * Fingerprint an actor token without retaining the token in execution evidence.
 *
 * @param string $actor_id
 * @return string
 */
function hrm_legacy_formula_target_actor_fingerprint($actor_id) {
    return hash('sha256', 'payroll-formula-target-actor-v1:'.(string)$actor_id);
}

/**
 * Emit a redacted operational event for target-profile execution decisions.
 *
 * @param string $outcome
 * @param string $reason
 * @param string $source_scope
 * @param string $formula_hash
 * @param string $actor_id
 * @return void
 */
function hrm_legacy_formula_target_log($outcome, $reason, $source_scope, $formula_hash, $actor_id) {
    error_log(json_encode(array(
        'contract' => HRM_LEGACY_FORMULA_TARGET_PROFILE_ID,
        'version' => HRM_LEGACY_FORMULA_TARGET_PROFILE_VERSION,
        'action' => 'legacy_formula_target_shadow_execution',
        'outcome' => $outcome === 'allowed' ? 'allowed' : 'denied',
        'reason' => hrm_legacy_formula_is_safe_token($reason, 100) ? $reason : 'invalid_reason',
        'source' => isset(hrm_legacy_formula_source_catalog()[$source_scope]) ? $source_scope : 'unknown',
        'formula_sha256' => preg_match('/^[a-f0-9]{64}$/D', $formula_hash) ? $formula_hash : hash('sha256', ''),
        'actor_fingerprint' => hrm_legacy_formula_target_actor_fingerprint($actor_id)
    )));
}

/**
 * Evaluate one reviewed candidate only in an explicit CLI non-production test.
 *
 * This function cannot be used by production payroll. It requires CLI plus an
 * explicit process constant, a current hash-bound registry entry, four distinct
 * principals, and a live re-run of the accepted synthetic differential corpus.
 * Formula text and context values are excluded from the returned receipt.
 *
 * @param string $source_formula
 * @param string $source_scope
 * @param array $values
 * @param string $executor_id
 * @param array $profile
 * @param array $registry
 * @param array $corpus
 * @param string $profile_sha256
 * @param string $corpus_sha256
 * @param string $exception_register_sha256
 * @param array|null $receipt
 * @param string|null $error
 * @param string|null $as_of
 * @return float|false
 */
function hrm_legacy_formula_target_shadow_evaluate(
    $source_formula,
    $source_scope,
    array $values,
    $executor_id,
    array $profile,
    array $registry,
    array $corpus,
    $profile_sha256,
    $corpus_sha256,
    $exception_register_sha256,
    &$receipt = null,
    &$error = null,
    $as_of = null
) {
    $receipt = null;
    $error = null;
    $source_hash = hrm_legacy_formula_fingerprint($source_formula);
    $deny = function($reason) use (&$error, $source_scope, $source_hash, $executor_id) {
        $error = $reason;
        hrm_legacy_formula_target_log('denied', $reason, $source_scope, $source_hash, $executor_id);
        return false;
    };

    if (PHP_SAPI !== 'cli'
        || !defined('HRM_LEGACY_FORMULA_TARGET_NONPRODUCTION')
        || HRM_LEGACY_FORMULA_TARGET_NONPRODUCTION !== true)
        return $deny('target_non_production_gate_required');

    if (!isset(hrm_legacy_formula_source_catalog()[$source_scope]))
        return $deny('target_source_scope_invalid');
    if (!hrm_legacy_formula_is_safe_token($executor_id, 100))
        return $deny('target_executor_invalid');
    if (hrm_legacy_formula_target_validate_profile($profile, $error) === false)
        return $deny($error);

    $entry_index = hrm_legacy_formula_target_validate_publication_registry(
        $registry,
        $profile_sha256,
        $corpus_sha256,
        $exception_register_sha256,
        $error
    );
    if ($entry_index === false)
        return $deny($error);
    if (!isset($entry_index[$source_hash][$source_scope]))
        return $deny('target_publication_entry_missing');
    $entry = $entry_index[$source_hash][$source_scope];

    $as_of = $as_of === null ? gmdate('Y-m-d') : $as_of;
    if (!hrm_legacy_formula_target_publication_is_effective($entry, $as_of))
        return $deny('target_publication_entry_inactive');
    if (!hash_equals($entry['test_executor_id'], $executor_id))
        return $deny('target_executor_not_authorized');

    $target_formula = $entry['classification'] === 'translated'
        ? hrm_legacy_formula_translate_for_facade($source_formula)
        : hrm_legacy_formula_normalize($source_formula);
    if ($target_formula === false
        || !hash_equals($entry['target_formula_sha256'], hash('sha256', $target_formula)))
        return $deny('target_formula_hash_mismatch');

    $target_evaluator = hrm_legacy_formula_target_create_evaluator($profile, $error);
    if (!is_callable($target_evaluator))
        return $deny($error === null ? 'target_evaluator_unavailable' : $error);
    $validated_corpus = hrm_legacy_formula_validate_corpus($corpus, $error);
    if ($validated_corpus === false)
        return $deny($error);
    $classification = hrm_legacy_formula_differential_classify(
        $source_formula,
        $validated_corpus['migration_contexts'],
        'hrm_legacy_formula_evaluate_for_differential',
        $target_evaluator
    );
    if ($classification['classification'] !== $entry['classification']
        || $classification['reason'] !== $entry['differential_reason']
        || !hash_equals($entry['result_set_sha256'], $classification['result_set_hash']))
        return $deny('target_differential_evidence_mismatch');

    $normalized_values = hrm_legacy_formula_target_validate_context($values, $error);
    if ($normalized_values === false)
        return $deny($error);

    try {
        $result = call_user_func($target_evaluator, $target_formula, $normalized_values);
    } catch (Exception $e) {
        return $deny('target_execution_failed');
    }

    $receipt = array(
        'contract' => array(
            'id' => HRM_LEGACY_FORMULA_TARGET_PROFILE_ID,
            'version' => HRM_LEGACY_FORMULA_TARGET_PROFILE_VERSION
        ),
        'mode' => 'non-production-shadow',
        'source_scope' => $source_scope,
        'source_formula_sha256' => $source_hash,
        'target_formula_sha256' => $entry['target_formula_sha256'],
        'classification' => $entry['classification'],
        'differential_reason' => $entry['differential_reason'],
        'result_set_sha256' => $entry['result_set_sha256'],
        'profile_sha256' => $profile_sha256,
        'corpus_sha256' => $corpus_sha256,
        'exception_register_sha256' => $exception_register_sha256,
        'evidence_id' => $entry['evidence_id'],
        'executor_fingerprint' => hrm_legacy_formula_target_actor_fingerprint($executor_id),
        'executed_on' => $as_of
    );
    hrm_legacy_formula_target_log('allowed', 'approved_non_production_shadow', $source_scope, $source_hash, $executor_id);
    return (float)$result;
}
