<?php
/** Development/runtime evidence only; no production filing authority is granted. */

/** Return the bounded local acceptance policy separately from production authority. */
function pay_fil_001_local_development_acceptance_policy()
{
    return array(
        'development_gate'=>'OFFICIAL_LOCAL_VALIDATOR_COMPATIBILITY',
        'repository_xsd_and_identity_guards_required'=>true,
        'viewer_opening_alone_is_validation'=>false,
        'production_acceptance'=>'PRODUCTION-ENVIRONMENT-PENDING / OUTSIDE CURRENT DEVELOPMENT PLAN',
        'production_generation_allowed'=>false,
        'production_transmission_allowed'=>false,
        'production_signing_allowed'=>false,
        'production_credentials_allowed'=>false
    );
}

/** Validate a minimized, synthetic local-runtime observation without promoting statutory provenance. */
function pay_fil_001_validate_local_runtime_evidence(array $manifest, &$error = null)
{
    $error = null;
    $expectedIdentity = array('form'=>'05/KK-TNCN','form_id'=>'05_KK_TNCN_TT80','code'=>'864','xml_version'=>'2.9.3');
    if (!isset($manifest['schema'], $manifest['identity'], $manifest['policy'])
        || $manifest['schema'] !== 'notrinos.pay-fil-001.local-validator-runtime.v1'
        || $manifest['identity'] !== $expectedIdentity
        || $manifest['policy'] !== pay_fil_001_local_development_acceptance_policy()) {
        $error = 'local_runtime_identity_or_policy_invalid';
        return false;
    }
    if (!isset($manifest['positive'], $manifest['negative_cases'], $manifest['provenance'])
        || !is_array($manifest['positive']) || !is_array($manifest['negative_cases']) || !is_array($manifest['provenance'])
        || !isset($manifest['positive']['xml_sha256'], $manifest['positive']['period'], $manifest['positive']['rendered_period'], $manifest['positive']['pdf_sha256'])
        || !array_key_exists('independent_statutory_schema_publication_established', $manifest['provenance'])
        || !array_key_exists('independent_statutory_technical_specification_established', $manifest['provenance'])
        || !array_key_exists('production_tax_authority_acceptance_established', $manifest['provenance'])
        || $manifest['positive']['xml_sha256'] !== 'b40ff029d9fa3a0ba40aff2c93af5a76b167357e8bfc65e45460139dae14a526'
        || $manifest['positive']['period'] !== '3/2026'
        || $manifest['positive']['rendered_period'] !== 'Quý 3 năm 2026'
        || $manifest['positive']['pdf_sha256'] !== '63da3bba56d91a65acf9f0482c123d5ec76591c08c15444af7bed0ed214b75b7'
        || $manifest['provenance']['independent_statutory_schema_publication_established'] !== false
        || $manifest['provenance']['independent_statutory_technical_specification_established'] !== false
        || $manifest['provenance']['production_tax_authority_acceptance_established'] !== false) {
        $error = 'local_runtime_positive_or_provenance_invalid';
        return false;
    }
    $expected = array(
        'negative-version.xml'=>'REJECTED_WITH_VERSION_WARNING',
        'negative-required-element.xml'=>'RENDERED_DESPITE_XSD_VIOLATION',
        'negative-numeric.xml'=>'RENDERED_DESPITE_XSD_VIOLATION',
        'negative-unknown-element.xml'=>'RENDERED_DESPITE_XSD_VIOLATION',
        'negative-namespace.xml'=>'JSCRIPT_RUNTIME_ERROR',
        'negative-ct25_1.xml'=>'RENDERED_DESPITE_XSD_VIOLATION',
        'negative-location-required.xml'=>'RENDERED_DESPITE_XSD_VIOLATION',
        'negative-sequence.xml'=>'RENDERED_DESPITE_XSD_VIOLATION'
    );
    if (count($manifest['negative_cases']) !== count($expected)) {
        $error = 'local_runtime_negative_set_invalid';
        return false;
    }
    foreach ($manifest['negative_cases'] as $case) {
        if (!is_array($case) || !isset($case['fixture'], $case['outcome'], $case['fixture_sha256'], $case['repository_guard'])
            || !is_string($case['fixture']) || !is_string($case['fixture_sha256'])
            || !isset($expected[$case['fixture']])
            || $expected[$case['fixture']] !== $case['outcome']
            || preg_match('/^[a-f0-9]{64}$/D', $case['fixture_sha256']) !== 1
            || $case['repository_guard'] === 'pass') {
            $error = 'local_runtime_negative_observation_invalid';
            return false;
        }
        unset($expected[$case['fixture']]);
    }
    return $manifest;
}
?>
