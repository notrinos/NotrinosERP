<?php
/** PAY-FIL-001 reviewed public legal-authority evidence contract. Pure helpers only. */
if (!defined('PAY_FIL_001_LEGAL_AUTHORITY_REVIEW_CONTRACT_VERSION'))
    define('PAY_FIL_001_LEGAL_AUTHORITY_REVIEW_CONTRACT_VERSION', '1.0');

function pay_fil_001_legal_review_fail(&$error, $message)
{
    $error = $message;
    return false;
}

function pay_fil_001_legal_review_url_is_gdt($url)
{
    return preg_match('#^https://(?:www\.)?gdt\.gov\.vn/#D', (string)$url) === 1;
}

function pay_fil_001_validate_legal_authority_review_manifest(array $m, &$error = null)
{
    $error = null;
    $required = array('schema','reviewed_on','jurisdiction_code','obligation_type','form_code','form_name','legal_basis','electronic_filing_framework','review_scope','authority_boundary','remaining_official_evidence_gaps');
    foreach ($required as $k)
        if (!array_key_exists($k, $m))
            return pay_fil_001_legal_review_fail($error, 'legal_review_field_missing:'.$k);

    if ($m['schema'] !== 'notrinos.pay-fil-001.public-legal-authority-review.v1'
        || $m['reviewed_on'] !== '2026-10-05'
        || $m['jurisdiction_code'] !== 'VN'
        || $m['obligation_type'] !== 'personal-income-tax-withholding-declaration'
        || $m['form_code'] !== '05/KK-TNCN')
        return pay_fil_001_legal_review_fail($error, 'legal_review_identity_invalid');

    $legal = $m['legal_basis'];
    if (!is_array($legal)
        || (isset($legal['regulation'])?$legal['regulation']:'') !== '80/2021/TT-BTC'
        || (isset($legal['issued_on'])?$legal['issued_on']:'') !== '2021-09-29'
        || (isset($legal['effective_from'])?$legal['effective_from']:'') !== '2022-01-01'
        || (isset($legal['form_basis_reviewed'])?$legal['form_basis_reviewed']:false) !== true
        || (isset($legal['effective_date_reviewed'])?$legal['effective_date_reviewed']:false) !== true
        || !pay_fil_001_legal_review_url_is_gdt(isset($legal['official_document_url'])?$legal['official_document_url']:'')
        || !pay_fil_001_legal_review_url_is_gdt(isset($legal['official_procedure_url'])?$legal['official_procedure_url']:''))
        return pay_fil_001_legal_review_fail($error, 'legal_review_tt80_invalid');

    $electronic = $m['electronic_filing_framework'];
    if (!is_array($electronic)
        || (isset($electronic['base_regulation'])?$electronic['base_regulation']:'') !== '19/2021/TT-BTC'
        || (isset($electronic['base_effective_from'])?$electronic['base_effective_from']:'') !== '2021-05-03'
        || (isset($electronic['amending_regulation'])?$electronic['amending_regulation']:'') !== '46/2024/TT-BTC'
        || (isset($electronic['framework_reviewed'])?$electronic['framework_reviewed']:false) !== true
        || (isset($electronic['connector_or_network_authority_derived'])?$electronic['connector_or_network_authority_derived']:true) !== false
        || !pay_fil_001_legal_review_url_is_gdt(isset($electronic['official_document_url'])?$electronic['official_document_url']:'')
        || !pay_fil_001_legal_review_url_is_gdt(isset($electronic['official_procedure_url'])?$electronic['official_procedure_url']:''))
        return pay_fil_001_legal_review_fail($error, 'legal_review_electronic_filing_invalid');

    $scope = $m['review_scope'];
    if (!is_array($scope)
        || (isset($scope['legal_applicability_and_effective_date_reviewed'])?$scope['legal_applicability_and_effective_date_reviewed']:false) !== true
        || (isset($scope['exact_machine_schema_provenance_reviewed'])?$scope['exact_machine_schema_provenance_reviewed']:true) !== false
        || (isset($scope['authoritative_technical_specification_reviewed'])?$scope['authoritative_technical_specification_reviewed']:true) !== false
        || (isset($scope['generated_payload_acceptance_reviewed'])?$scope['generated_payload_acceptance_reviewed']:true) !== false)
        return pay_fil_001_legal_review_fail($error, 'legal_review_scope_invalid');

    $boundary = $m['authority_boundary'];
    foreach (array('official_schema_claimed','production_runtime_consumption_allowed','official_generation_allowed','filing_authorization_execution_allowed','transmission_allowed','network_authority_allowed','connector_secret_custody_allowed','historical_payroll_accounting_filing_mutation_allowed') as $k)
        if (!array_key_exists($k, $boundary) || $boundary[$k] !== false)
            return pay_fil_001_legal_review_fail($error, 'legal_review_authority_ceiling_invalid:'.$k);

    $expected = array(
        'independent-gdt-provenance-for-exact-schema',
        'independently-published-authoritative-technical-specification',
        'certification-or-etax-gdt-acceptance-of-independently-generated-xml'
    );
    if ($m['remaining_official_evidence_gaps'] !== $expected)
        return pay_fil_001_legal_review_fail($error, 'legal_review_gap_set_invalid');
    return $m;
}
?>
