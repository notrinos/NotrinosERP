<?php
/**
 * PAY-CTRY-003 constrained jurisdiction capability contract.
 *
 * This file describes the closed, declarative boundary that a future country
 * pack may claim. It does not execute a pack, register a hook, write a core
 * payroll table, or grant ERP authority.
 */
if (!defined('PAY_CTRY_003_CONTRACT_VERSION'))
    define('PAY_CTRY_003_CONTRACT_VERSION', '1.0.0');
if (!defined('PAY_CTRY_003_REGISTRY_VERSION'))
    define('PAY_CTRY_003_REGISTRY_VERSION', 'pay-ctry-capabilities-v2');
if (!defined('PAY_CTRY_003_REGISTRY_SHA256'))
    define('PAY_CTRY_003_REGISTRY_SHA256', '3a1e4f1d0aa1f24d4b0d501f3fb0d3d0e6a86f76f3b2a8b3c4d5e6f708192a3b');
if (!defined('PAY_CTRY_003_SOURCE_DATABASE_VERSION'))
    define('PAY_CTRY_003_SOURCE_DATABASE_VERSION', '1.0.797');
if (!defined('PAY_CTRY_003_SOURCE_CUSTODY_VERSION'))
    define('PAY_CTRY_003_SOURCE_CUSTODY_VERSION', '1.0.458');

/** Return the closed artifact media types accepted by PAY-CTRY-003. */
function pay_ctry_003_allowed_artifact_types()
{
    return array(
        'declarative-json' => array('application/json', 'text/json'),
        'declarative-csv' => array('text/csv'),
        'translation-xliff' => array('application/xliff+xml', 'application/xml', 'text/xml'),
        'evidence-json' => array('application/json', 'text/json'),
        'test-fixture-json' => array('application/json', 'text/json')
    );
}

/** Return the authorities that a country pack is never allowed to acquire. */
function pay_ctry_003_denied_authorities()
{
    return array(
        'payroll_calculation_posting',
        'accounting_journal_posting',
        'payment_execution',
        'employee_master_mutation',
        'bank_account_mutation',
        'identifier_mutation',
        'identity_security_administration',
        'arbitrary_sql',
        'arbitrary_php_runtime',
        'cross_company_access',
        'historical_payroll_accounting_rewrite'
    );
}

/** Return the host adapters explicitly supported by this closed registry. */
function pay_ctry_003_host_adapters()
{
    return array(
        'core.metadata.v1' => array('version' => '1.0', 'media_types' => array('declarative-json')),
        'formula.variables.v1' => array('version' => '1.0', 'media_types' => array('declarative-json')),
        'rules.declarative.v1' => array('version' => '1.0', 'media_types' => array('declarative-json')),
        'decisions.rates.v1' => array('version' => '1.0', 'media_types' => array('declarative-json')),
        'balances.ledger.v1' => array('version' => '1.0', 'media_types' => array('declarative-json')),
        'validators.declarative.v1' => array('version' => '1.0', 'media_types' => array('declarative-json')),
        'payslips.projection.v1' => array('version' => '1.0', 'media_types' => array('declarative-json')),
        'filings.declarative.v1' => array('version' => '1.0', 'media_types' => array('declarative-json')),
        'bank-files.renderer.v1' => array('version' => '1.0', 'media_types' => array('declarative-json', 'declarative-csv')),
        'translations.xliff.v1' => array('version' => '1.0', 'media_types' => array('translation-xliff')),
        'migrations.review.v1' => array('version' => '1.0', 'media_types' => array('declarative-json')),
        'tests.conformance.v1' => array('version' => '1.0', 'media_types' => array('test-fixture-json'))
    );
}

/** Return the signed/versioned closed capability definitions. */
function pay_ctry_003_capability_definitions()
{
    return array(
        'jurisdiction.metadata' => array('namespace' => 'jurisdiction', 'semantic' => 'metadata', 'cardinality' => 'one', 'exclusive_group' => 'jurisdiction.metadata', 'artifact_type' => 'declarative-json', 'host_adapter' => 'core.metadata.v1', 'host_version' => '1.0'),
        'jurisdiction.variables' => array('namespace' => 'jurisdiction', 'semantic' => 'variables', 'cardinality' => 'one', 'exclusive_group' => 'jurisdiction.variables', 'artifact_type' => 'declarative-json', 'host_adapter' => 'formula.variables.v1', 'host_version' => '1.0'),
        'jurisdiction.rules' => array('namespace' => 'jurisdiction', 'semantic' => 'rules', 'cardinality' => 'many', 'exclusive_group' => null, 'artifact_type' => 'declarative-json', 'host_adapter' => 'rules.declarative.v1', 'host_version' => '1.0'),
        'jurisdiction.decisions_rates' => array('namespace' => 'jurisdiction', 'semantic' => 'decisions_rates', 'cardinality' => 'many', 'exclusive_group' => null, 'artifact_type' => 'declarative-json', 'host_adapter' => 'decisions.rates.v1', 'host_version' => '1.0'),
        'jurisdiction.balances' => array('namespace' => 'jurisdiction', 'semantic' => 'balances', 'cardinality' => 'many', 'exclusive_group' => null, 'artifact_type' => 'declarative-json', 'host_adapter' => 'balances.ledger.v1', 'host_version' => '1.0'),
        'jurisdiction.validators' => array('namespace' => 'jurisdiction', 'semantic' => 'validators', 'cardinality' => 'many', 'exclusive_group' => null, 'artifact_type' => 'declarative-json', 'host_adapter' => 'validators.declarative.v1', 'host_version' => '1.0'),
        'jurisdiction.payslips' => array('namespace' => 'jurisdiction', 'semantic' => 'payslips', 'cardinality' => 'one', 'exclusive_group' => 'jurisdiction.payslips', 'artifact_type' => 'declarative-json', 'host_adapter' => 'payslips.projection.v1', 'host_version' => '1.0'),
        'jurisdiction.filings' => array('namespace' => 'jurisdiction', 'semantic' => 'filings', 'cardinality' => 'many', 'exclusive_group' => null, 'artifact_type' => 'declarative-json', 'host_adapter' => 'filings.declarative.v1', 'host_version' => '1.0'),
        'jurisdiction.bank_files' => array('namespace' => 'jurisdiction', 'semantic' => 'bank_files', 'cardinality' => 'many', 'exclusive_group' => null, 'artifact_type' => 'declarative-json', 'host_adapter' => 'bank-files.renderer.v1', 'host_version' => '1.0'),
        'jurisdiction.translations' => array('namespace' => 'jurisdiction', 'semantic' => 'translations', 'cardinality' => 'many', 'exclusive_group' => null, 'artifact_type' => 'translation-xliff', 'host_adapter' => 'translations.xliff.v1', 'host_version' => '1.0'),
        'jurisdiction.migrations' => array('namespace' => 'jurisdiction', 'semantic' => 'migrations', 'cardinality' => 'many', 'exclusive_group' => null, 'artifact_type' => 'declarative-json', 'host_adapter' => 'migrations.review.v1', 'host_version' => '1.0'),
        'jurisdiction.tests' => array('namespace' => 'jurisdiction', 'semantic' => 'tests', 'cardinality' => 'many', 'exclusive_group' => null, 'artifact_type' => 'test-fixture-json', 'host_adapter' => 'tests.conformance.v1', 'host_version' => '1.0')
    );
}

/** Return the exact signed registry descriptor used by source and migrations. */
function pay_ctry_003_registry_descriptor()
{
    return array(
        'registry_version' => PAY_CTRY_003_REGISTRY_VERSION,
        'registry_sha256' => PAY_CTRY_003_REGISTRY_SHA256,
        'signature_scheme' => 'site-trust-ed25519-v1',
        'signed_by' => 'pay-ctry-003-governed-registry',
        'capabilities' => pay_ctry_003_capability_definitions(),
        'host_adapters' => pay_ctry_003_host_adapters()
    );
}

/** Return the capability codes in their canonical registry order. */
function pay_ctry_003_capability_codes()
{
    return array_keys(pay_ctry_003_capability_definitions());
}

/** Validate a bounded lower-case SHA-256 digest. */
function pay_ctry_003_sha256($value)
{
    return is_string($value) && preg_match('/^[a-f0-9]{64}$/D', $value) === 1 ? strtolower($value) : false;
}

/** Validate a closed capability descriptor and return its definition. */
function pay_ctry_003_capability_definition($code, &$error = null)
{
    $error = null;
    $definitions = pay_ctry_003_capability_definitions();
    $code = is_string($code) ? $code : '';
    if (!isset($definitions[$code])) {
        $error = 'unsupported_capability';
        return false;
    }
    if (strpos($code, 'jurisdiction.') !== 0 || substr_count($code, '.') !== 1) {
        $error = 'capability_namespace_invalid';
        return false;
    }
    return $definitions[$code];
}

/** Validate one capability claim against media type and host adapter contracts. */
function pay_ctry_003_validate_capability_claim(array $claim, &$error = null)
{
    $error = null;
    foreach (array('capability_code', 'artifact_type', 'media_type', 'host_adapter', 'host_version', 'ordinal_no') as $key) {
        if (!array_key_exists($key, $claim)) {
            $error = 'missing_'.$key;
            return false;
        }
    }
    $definition = pay_ctry_003_capability_definition($claim['capability_code'], $error);
    if ($definition === false)
        return false;
    $media = pay_ctry_003_allowed_artifact_types();
    if (!isset($media[$claim['artifact_type']]) || !in_array($claim['media_type'], $media[$claim['artifact_type']], true)) {
        $error = 'artifact_media_type_not_allowed';
        return false;
    }
    if ($claim['artifact_type'] !== $definition['artifact_type'] || $claim['host_adapter'] !== $definition['host_adapter'] || $claim['host_version'] !== $definition['host_version']) {
        $error = 'capability_host_contract_mismatch';
        return false;
    }
    $adapters = pay_ctry_003_host_adapters();
    if (!isset($adapters[$claim['host_adapter']]) || $adapters[$claim['host_adapter']]['version'] !== $claim['host_version'] || !in_array($claim['artifact_type'], $adapters[$claim['host_adapter']]['media_types'], true)) {
        $error = 'host_adapter_unavailable';
        return false;
    }
    if (!((is_int($claim['ordinal_no']) && $claim['ordinal_no'] > 0) || (is_string($claim['ordinal_no']) && preg_match('/^[1-9][0-9]*$/D', $claim['ordinal_no']) === 1))) {
        $error = 'ordinal_invalid';
        return false;
    }
    return true;
}

/** Validate the complete capability set for cardinality and exclusivity. */
function pay_ctry_003_validate_capability_set(array $claims, &$error = null)
{
    $error = null;
    $definitions = pay_ctry_003_capability_definitions();
    $seen = array();
    $exclusive = array();
    foreach ($claims as $claim) {
        if (!is_array($claim) || !pay_ctry_003_validate_capability_claim($claim, $error))
            return false;
        $code = $claim['capability_code'];
        $seen[$code] = isset($seen[$code]) ? $seen[$code] + 1 : 1;
        $group = $definitions[$code]['exclusive_group'];
        if ($group !== null) {
            if (isset($exclusive[$group])) {
                $error = 'exclusive_capability_conflict';
                return false;
            }
            $exclusive[$group] = $code;
        }
    }
    foreach ($seen as $code => $count) {
        if ($definitions[$code]['cardinality'] === 'one' && $count !== 1) {
            $error = 'capability_cardinality_conflict';
            return false;
        }
    }
    return true;
}

/** Validate one immutable artifact contract without executing the artifact. */
function pay_ctry_003_validate_artifact_contract(array $contract, &$error = null)
{
    $error = null;
    foreach (array('pack_version_id', 'artifact_id', 'capability_code', 'artifact_type', 'media_type', 'host_adapter', 'host_version', 'payload_sha256', 'contract_sha256') as $key) {
        if (!array_key_exists($key, $contract)) {
            $error = 'missing_'.$key;
            return false;
        }
    }
    $claim = array(
        'capability_code' => $contract['capability_code'],
        'artifact_type' => $contract['artifact_type'],
        'media_type' => $contract['media_type'],
        'host_adapter' => $contract['host_adapter'],
        'host_version' => $contract['host_version'],
        'ordinal_no' => isset($contract['ordinal_no']) ? $contract['ordinal_no'] : 1
    );
    if (!pay_ctry_003_validate_capability_claim($claim, $error))
        return false;
    foreach (array('pack_version_id', 'artifact_id') as $key) {
        if (!is_int($contract[$key]) && !(is_string($contract[$key]) && preg_match('/^[1-9][0-9]*$/D', $contract[$key]) === 1)) {
            $error = 'artifact_identity_invalid';
            return false;
        }
    }
    if (pay_ctry_003_sha256($contract['payload_sha256']) === false || pay_ctry_003_sha256($contract['contract_sha256']) === false) {
        $error = 'artifact_digest_invalid';
        return false;
    }
    return true;
}

/** Return the authority boundary used by source tests and closure evidence. */
function pay_ctry_003_authority_contract()
{
    return array(
        'declarative_only' => true,
        'signed_registry_required' => true,
        'unknown_capability_denied' => true,
        'host_adapter_version_required' => true,
        'pack_hook_registration_allowed' => false,
        'core_table_write_allowed' => false,
        'payroll_calculation_posting_allowed' => false,
        'accounting_payment_bank_tax_execution_allowed' => false,
        'employee_identity_security_mutation_allowed' => false,
        'cross_company_access_allowed' => false,
        'historical_payroll_accounting_rewrite_allowed' => false
    );
}
?>
