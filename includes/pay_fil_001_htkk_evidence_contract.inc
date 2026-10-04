<?php
/** PAY-FIL-001 bounded HTKK observed-evidence contract. Pure helpers only; no filing authority. */
if (!defined('PAY_FIL_001_HTKK_EVIDENCE_CONTRACT_VERSION')) define('PAY_FIL_001_HTKK_EVIDENCE_CONTRACT_VERSION','1.0');

/** Exact non-sensitive identity of the reviewed user-supplied HTKK export. */
function pay_fil_001_htkk_observed_evidence_identity()
{
    return array(
        'evidence_id'=>'vn-htkk-05-kk-tncn-tt80-xml-2.9.3-observed-2026-10-03',
        'jurisdiction_code'=>'VN',
        'obligation_type'=>'personal-income-tax-withholding',
        'source_kind'=>'user-supplied-htkk-export',
        'source_sha256'=>'83aee7ef1a591f3fc105c3fff6ac7650d2ec295c7f1bbf8b51bc9a4c93ba5b9d',
        'source_size_bytes'=>2784,
        'service_id'=>'HTKK',
        'service_version'=>'5.7.8',
        'declaration_code'=>'864',
        'declaration_xml_version'=>'2.9.3',
        'period_kind'=>'Q',
        'xml_namespace'=>'http://kekhaithue.gdt.gov.vn/TKhaiThue',
        'xml_root'=>'HSoThueDTu',
        'ordered_local_tag_count'=>66,
        'ordered_local_tag_names_sha256'=>'00bab40d61c49a208a999b6dfd6ed0185b553ab28e0aab28aea848bc7cc63650',
        'main_indicator_tags_sha256'=>'c03bae621e70a579d6e2c78e34b6e9d973385c27b82fba3d79d4fbd1449d73a8'
    );
}

/** Exact indicator tag order observed in the supplied sample; values are intentionally excluded. */
function pay_fil_001_htkk_observed_indicator_tags()
{
    return array('mst_cu','ct15','ct16','ct17','ct18','ct19','ct20','ct21','ct22','ct23','ct24','ct25','ct25_1','ct26','ct27','ct28','ct29','ct30','ct31','ct32');
}

/** Authority remains fail-closed because an observed payload is not an authoritative schema/certification. */
function pay_fil_001_htkk_authority_boundary()
{
    return array(
        'official_schema_claimed'=>false,
        'official_form_generation_allowed'=>false,
        'filing_authorization_execution_allowed'=>false,
        'transmission_allowed'=>false,
        'network_authority_allowed'=>false,
        'connector_secret_custody_allowed'=>false,
        'raw_user_evidence_retained_in_repository'=>false
    );
}

/** Remaining human-reviewed evidence required before a named official schema can be enabled. */
function pay_fil_001_htkk_remaining_evidence_gaps()
{
    return array(
        'authoritative-machine-readable-schema-or-xsd-for-exact-form-version',
        'reviewed-legal-applicability-and-effective-date-evidence',
        'reviewed-authoritative-technical-specification-evidence',
        'reviewed-certification-or-acceptance-evidence-for-generated-payloads'
    );
}

/** Return a manifest value or its default using syntax supported by PHP 5.6. */
function pay_fil_001_htkk_manifest_value(array $data, $key, $default)
{
    return isset($data[$key]) ? $data[$key] : $default;
}

/** Validate the immutable redacted evidence manifest without parsing/storing taxpayer data. */
function pay_fil_001_validate_htkk_observed_evidence_manifest(array $m, &$e=null)
{
    $e=null;
    if (pay_fil_001_htkk_manifest_value($m, 'schema', '') !== 'notrinos.pay-fil-001.observed-filing-evidence.v1')
        return pay_fil_001_fail($e,'htkk_evidence_schema_invalid');
    $id=pay_fil_001_htkk_observed_evidence_identity();
    $checks=array(
        'evidence_id'=>$id['evidence_id'],
        'jurisdiction_code'=>$id['jurisdiction_code'],
        'obligation_type'=>$id['obligation_type'],
        'source_kind'=>$id['source_kind'],
        'source_sha256'=>$id['source_sha256'],
        'source_size_bytes'=>$id['source_size_bytes']
    );
    foreach($checks as $k=>$v) if(!array_key_exists($k,$m)||$m[$k]!==$v)
        return pay_fil_001_fail($e,'htkk_evidence_identity_invalid');
    if (pay_fil_001_htkk_manifest_value($m, 'source_retained_in_repository', true)!==false
        || pay_fil_001_htkk_manifest_value($m, 'source_contains_personal_or_taxpayer_data', false)!==true)
        return pay_fil_001_fail($e,'htkk_evidence_privacy_boundary_invalid');

    $svc=pay_fil_001_htkk_manifest_value($m, 'observed_service', array());
    $decl=pay_fil_001_htkk_manifest_value($m, 'observed_declaration', array());
    $xml=pay_fil_001_htkk_manifest_value($m, 'observed_xml', array());
    if (pay_fil_001_htkk_manifest_value($svc, 'id', '')!==$id['service_id'] || pay_fil_001_htkk_manifest_value($svc, 'version', '')!==$id['service_version']
        || pay_fil_001_htkk_manifest_value($decl, 'code', '')!==$id['declaration_code'] || pay_fil_001_htkk_manifest_value($decl, 'xml_version', '')!==$id['declaration_xml_version']
        || pay_fil_001_htkk_manifest_value($decl, 'period_kind', '')!==$id['period_kind']
        || pay_fil_001_htkk_manifest_value($xml, 'namespace', '')!==$id['xml_namespace'] || pay_fil_001_htkk_manifest_value($xml, 'root', '')!==$id['xml_root']
        || (int)pay_fil_001_htkk_manifest_value($xml, 'ordered_local_tag_count', -1)!==$id['ordered_local_tag_count']
        || pay_fil_001_htkk_manifest_value($xml, 'ordered_local_tag_names_sha256', '')!==$id['ordered_local_tag_names_sha256']
        || pay_fil_001_htkk_manifest_value($xml, 'main_indicator_tags_sha256', '')!==$id['main_indicator_tags_sha256'])
        return pay_fil_001_fail($e,'htkk_evidence_observed_profile_invalid');

    if (pay_fil_001_htkk_manifest_value($xml, 'main_indicator_tags', null)!==pay_fil_001_htkk_observed_indicator_tags())
        return pay_fil_001_fail($e,'htkk_evidence_indicator_profile_invalid');

    $privacy=pay_fil_001_htkk_manifest_value($m, 'privacy', array());
    foreach(array('raw_taxpayer_identity_retained','raw_address_retained','raw_phone_retained','raw_email_retained','raw_signature_name_retained','raw_filing_amounts_retained') as $k)
        if(pay_fil_001_htkk_manifest_value($privacy, $k, true)!==false) return pay_fil_001_fail($e,'htkk_evidence_pii_retention_invalid');

    if (pay_fil_001_htkk_manifest_value($m, 'authority', null)!==pay_fil_001_htkk_authority_boundary())
        return pay_fil_001_fail($e,'htkk_evidence_authority_boundary_invalid');
    if (pay_fil_001_htkk_manifest_value($m, 'remaining_required_evidence', null)!==pay_fil_001_htkk_remaining_evidence_gaps())
        return pay_fil_001_fail($e,'htkk_evidence_gap_contract_invalid');
    return $m;
}
?>
