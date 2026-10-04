<?php
/** PAY-FIL-001 hash-only receipt for the local HTKK TT80 05/KK-TNCN schema candidate. */
if (!defined('PAY_FIL_001_HTKK_SCHEMA_EVIDENCE_CONTRACT_VERSION'))
    define('PAY_FIL_001_HTKK_SCHEMA_EVIDENCE_CONTRACT_VERSION', '1.0');

/** Exact observed resources for the local HTKK 5.7.8 / XML 2.9.3 candidate stack. */
function pay_fil_001_htkk_schema_observed_resources()
{
    return array(
        array('role'=>'active-per-form-xsd-candidate','file'=>'05_KK_TNCN_TT80_293.xsd','source_path'=>'C:\\Program Files (x86)\\HTKK\\InterfaceTemplates\\Validate\\05_KK_TNCN_TT80_293.xsd','evidence_copy'=>'HTKK/InterfaceTemplates/Validate/05_KK_TNCN_TT80_293.xsd','bytes'=>17770,'sha256'=>'9cd4d367123ee0ac1f94fb043975738ac142420d558bb24969e4c0afe64ac846'),
        array('role'=>'per-form-xsd-redefine-dependency','file'=>'TKhaiThue_DBHC.xsd','source_path'=>'C:\\Program Files (x86)\\HTKK\\InterfaceTemplates\\TKhaiThue_DBHC.xsd','evidence_copy'=>'HTKK/InterfaceTemplates/TKhaiThue_DBHC.xsd','bytes'=>18036,'sha256'=>'aba7f2acbc9332caaf3eb2da9a081a3d876ddc7813d6230921da48c775460011'),
        array('role'=>'related-envelope-schema','file'=>'TKhaiThue.xsd','source_path'=>'C:\\Program Files (x86)\\HTKK\\InterfaceTemplates\\TKhaiThue.xsd','evidence_copy'=>'HTKK/InterfaceTemplates/TKhaiThue.xsd','bytes'=>17741,'sha256'=>'8f451719355b5a6247f65a34a37584b4a0c7f7cc81a568e1033d08b8fe9b8c1d'),
        array('role'=>'related-xml-signature-schema-dependency','file'=>'xmldsig-core-schema.xsd','source_path'=>'C:\\Program Files (x86)\\HTKK\\InterfaceTemplates\\xmldsig-core-schema.xsd','evidence_copy'=>'HTKK/InterfaceTemplates/xmldsig-core-schema.xsd','bytes'=>10264,'sha256'=>'c3b5160be1852606d10f6928e0b214d418f8dd418667cb2fb88fec31e4e6f56d'),
        array('role'=>'active-form-menu-registry','file'=>'Menu.xml','source_path'=>'C:\\Program Files (x86)\\HTKK\\Project\\Menu.xml','evidence_copy'=>'HTKK/Project/Menu.xml','bytes'=>334792,'sha256'=>'9a723950f17678edf983591d8a36ade266a823acc3a5d3d69af46c4c20d902a0'),
        array('role'=>'period-and-declaration-mapping','file'=>'Map.xml','source_path'=>'C:\\Program Files (x86)\\HTKK\\Project\\Map.xml','evidence_copy'=>'HTKK/Project/Map.xml','bytes'=>126425,'sha256'=>'3f5095bc33322722c628fbd4c6428dc0dffd3492e8f61558715954b44d399215'),
        array('role'=>'field-and-indicator-mapping','file'=>'MapMCT.xml','source_path'=>'C:\\Program Files (x86)\\HTKK\\Project\\MapMCT.xml','evidence_copy'=>'HTKK/Project/MapMCT.xml','bytes'=>695283,'sha256'=>'137b7752fce7ea42e142877dce9eb18cbf99189bf6cc27a418507608cd93e7b4'),
        array('role'=>'version-transition-map','file'=>'ConvertVersion.xml','source_path'=>'C:\\Program Files (x86)\\HTKK\\InterfaceIni\\ConvertVersion.xml','evidence_copy'=>'HTKK/InterfaceIni/ConvertVersion.xml','bytes'=>35500,'sha256'=>'eca3a24707fcbab8c420412f651e0a98775921915e25b79b073b433f090490e3'),
        array('role'=>'active-form-definition','file'=>'05_KK_TNCN_TT80_293.xml','source_path'=>'C:\\Program Files (x86)\\HTKK\\InterfaceTemplates\\xml\\05_KK_TNCN_TT80_293.xml','evidence_copy'=>'HTKK/InterfaceTemplates/xml/05_KK_TNCN_TT80_293.xml','bytes'=>10866,'sha256'=>'ac49d746988fa0e18fa4f41c839b02b87d2d612988a20fae93004c5a99ed1d6e'),
        array('role'=>'active-xml-serialization-template','file'=>'05_KK_TNCN_TT80_xml_293.xml','source_path'=>'C:\\Program Files (x86)\\HTKK\\InterfaceTemplates\\xml\\05_KK_TNCN_TT80_xml_293.xml','evidence_copy'=>'HTKK/InterfaceTemplates/xml/05_KK_TNCN_TT80_xml_293.xml','bytes'=>1721,'sha256'=>'ff1e3b4b03194c165d4ab44bd34472df526977a470b002b16853e812c8aeae3d')
    );
}

/** Validate the hash-only, observed-candidate receipt without promoting it to authority. */
function pay_fil_001_validate_htkk_xsd_observation_manifest(array $m, &$e=null)
{
    $e = null;
    if (!isset($m['schema']) || $m['schema'] !== 'notrinos.pay-fil-001.htkk-xsd-observation.v1'
        || !isset($m['evidence_id']) || $m['evidence_id'] !== 'vn-htkk-05-kk-tncn-tt80-xsd-2.9.3-observed-2026-10-05')
        return pay_fil_001_fail($e, 'htkk_xsd_evidence_identity_invalid');

    $scope = isset($m['scope']) && is_array($m['scope']) ? $m['scope'] : array();
    $menu = isset($scope['menu_entry']) && is_array($scope['menu_entry']) ? $scope['menu_entry'] : array();
    if (!isset($scope['jurisdiction_code']) || $scope['jurisdiction_code'] !== 'VN'
        || !isset($scope['regulation']) || $scope['regulation'] !== 'Thông tư 80/2021/TT-BTC'
        || !isset($scope['form']) || $scope['form'] !== '05/KK-TNCN'
        || !isset($scope['form_id']) || $scope['form_id'] !== '05_KK_TNCN_TT80'
        || !isset($scope['declaration_code']) || $scope['declaration_code'] !== '864'
        || !isset($scope['htkk_version']) || $scope['htkk_version'] !== '5.7.8'
        || !isset($scope['xml_version']) || $scope['xml_version'] !== '2.9.3'
        || $menu !== array('MaFile'=>'05_KK_TNCN_TT80','VersionXML'=>'2.9.3','VersionKHBS'=>'2.9.3','XsdSchema'=>'05_KK_TNCN_TT80','class'=>'TncnKk05TT80'))
        return pay_fil_001_fail($e, 'htkk_xsd_evidence_scope_invalid');

    $observations = isset($m['observations']) && is_array($m['observations']) ? $m['observations'] : array();
    $xmlIdentity = isset($observations['original_export_identity']) && is_array($observations['original_export_identity']) ? $observations['original_export_identity'] : array();
    $rootElement = isset($xmlIdentity['root_element']) ? $xmlIdentity['root_element'] : '';
    $namespaceUri = isset($xmlIdentity['namespace_uri']) ? $xmlIdentity['namespace_uri'] : '';
    $schemaLocationPresent = isset($xmlIdentity['schema_location_attribute_present']) ? $xmlIdentity['schema_location_attribute_present'] : true;
    $productCodeElement = isset($xmlIdentity['application_product_code_element']) ? $xmlIdentity['application_product_code_element'] : '';
    $productCode = isset($xmlIdentity['application_product_code']) ? $xmlIdentity['application_product_code'] : '';
    $applicationVersionElement = isset($xmlIdentity['application_version_element']) ? $xmlIdentity['application_version_element'] : '';
    $applicationVersion = isset($xmlIdentity['application_version']) ? $xmlIdentity['application_version'] : '';
    $declarationCodeElement = isset($xmlIdentity['declaration_code_element']) ? $xmlIdentity['declaration_code_element'] : '';
    $declarationCode = isset($xmlIdentity['declaration_code']) ? $xmlIdentity['declaration_code'] : '';
    $formVersionPresent = isset($xmlIdentity['form_version_element_present']) ? $xmlIdentity['form_version_element_present'] : true;
    $templateVersionPresent = isset($xmlIdentity['template_version_element_present']) ? $xmlIdentity['template_version_element_present'] : true;
    if (!isset($observations['original_export_has_schemaLocation']) || $observations['original_export_has_schemaLocation'] !== false
        || !isset($observations['original_export_embeds_form_version']) || $observations['original_export_embeds_form_version'] !== false
        || !isset($observations['original_export_embeds_template_version']) || $observations['original_export_embeds_template_version'] !== false
        || !isset($observations['original_export_embeds_application_version']) || $observations['original_export_embeds_application_version'] !== true
        || $rootElement !== 'HSoThueDTu'
        || $namespaceUri !== 'http://kekhaithue.gdt.gov.vn/TKhaiThue'
        || $schemaLocationPresent !== false
        || $productCodeElement !== 'maDVu'
        || $productCode !== 'HTKK'
        || $applicationVersionElement !== 'pbanDVu'
        || $applicationVersion !== '5.7.8'
        || $declarationCodeElement !== 'maTKhai'
        || $declarationCode !== '864'
        || $formVersionPresent !== false
        || $templateVersionPresent !== false)
        return pay_fil_001_fail($e, 'htkk_xsd_evidence_xml_identity_invalid');

    $custody = isset($m['custody']) && is_array($m['custody']) ? $m['custody'] : array();
    if (!isset($custody['mode']) || $custody['mode'] !== 'hash-and-observation-only'
        || !isset($custody['evidence_manifest_sha256']) || $custody['evidence_manifest_sha256'] !== 'b92fc8de9b6111e1364e44ba269501c3b520099c1dd202e1121653a414b56163'
        || !isset($custody['inventory_sha256']) || $custody['inventory_sha256'] !== '997fb5d4b24a89a6abed2211e5ec744bd76a41e13357c436daebda7e2c881fbd'
        || !isset($custody['original_taxpayer_xml_sha256']) || $custody['original_taxpayer_xml_sha256'] !== '83aee7ef1a591f3fc105c3fff6ac7650d2ec295c7f1bbf8b51bc9a4c93ba5b9d'
        || !isset($custody['original_taxpayer_xml_retained_in_repository']) || $custody['original_taxpayer_xml_retained_in_repository'] !== false
        || !isset($custody['htkk_schema_files_copied_into_repository']) || $custody['htkk_schema_files_copied_into_repository'] !== false)
        return pay_fil_001_fail($e, 'htkk_xsd_evidence_custody_invalid');

    $resources = isset($m['resources']) && is_array($m['resources']) ? $m['resources'] : array();
    if ($resources !== pay_fil_001_htkk_schema_observed_resources())
        return pay_fil_001_fail($e, 'htkk_xsd_evidence_resource_allowlist_invalid');

    $authority = isset($m['authority']) && is_array($m['authority']) ? $m['authority'] : array();
    foreach (array(
        'htkk_observed_local_match_only'=>true,
        'independent_gdt_provenance_established'=>false,
        'legal_applicability_reviewed'=>false,
        'independent_authoritative_technical_specification_reviewed'=>false,
        'generated_payload_certification_or_etax_acceptance_established'=>false,
        'production_runtime_consumption_allowed'=>false,
        'official_generation_allowed'=>false,
        'filing_authorization_execution_allowed'=>false,
        'transmission_allowed'=>false
    ) as $key=>$expected) {
        if (!array_key_exists($key, $authority) || $authority[$key] !== $expected)
            return pay_fil_001_fail($e, 'htkk_xsd_evidence_authority_boundary_invalid');
    }

    $gaps = isset($m['remaining_official_evidence_gaps']) && is_array($m['remaining_official_evidence_gaps']) ? $m['remaining_official_evidence_gaps'] : array();
    if ($gaps !== array(
        'independent-gdt-provenance-for-exact-schema',
        'reviewed-legal-applicability-and-effective-date-evidence',
        'independently-published-authoritative-technical-specification',
        'certification-or-etax-gdt-acceptance-of-independently-generated-xml'
    )) return pay_fil_001_fail($e, 'htkk_xsd_evidence_gap_set_invalid');

    return $m;
}
?>
