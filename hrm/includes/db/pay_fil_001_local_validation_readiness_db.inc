<?php
/** Source-only local development compatibility readiness; production authority remains denied. */
include_once(dirname(__FILE__).'/pay_fil_001_readiness_db.inc');
include_once($path_to_root.'/includes/pay_fil_001_local_validation_evidence_contract.inc');

/** Require exact synthetic runtime evidence and corrected regression while preserving predecessor custody. */
function pay_fil_001_require_local_validator_compatibility_source($patch)
{
    global $path_to_root;
    if (!pay_fil_001_require_electronic_filing_legal_boundary_review_source($patch)) return false;
    $files = array(
        'tests/fixtures/payroll/legacy_v1/manifest_v1.0.526.json'=>'92844195f339c13d7d692cb0cf322a5cc482b936a636244cfdfd43a5e4787c03',
        'tests/fixtures/pay_fil_001/itaxviewer-tt80-05-kk-tncn-2.9.3-synthetic.xml'=>'b40ff029d9fa3a0ba40aff2c93af5a76b167357e8bfc65e45460139dae14a526',
        'includes/pay_fil_001_local_validation_evidence_contract.inc'=>'450fbdbadd2a2e0bb2153f7ebe756a7454f5d9b8f7716f8330544ae30a1d794f',
        'tests/php/test_pay_fil_001_itaxviewer_local_compatibility.php'=>'a098d1e162d76168494dfce9fdd6d4767dcf3ae8af664813e59412cfdd213b95',
        'hrm/country_packs/vn_reference/1.0.0/artifacts/filing-evidence/itaxviewer-05-kk-tncn-tt80-local-runtime-2026-10-06.json'=>'1f2e6d9727f6f9738ca7aeebfb20cbdc4dd48eb97b6aaad6bbba63dc340c2443'
    );
    foreach ($files as $relative=>$sha256) {
        if (!is_file($path_to_root.'/'.$relative) || hash_file('sha256', $path_to_root.'/'.$relative) !== $sha256)
            return pay_fil_001_fail_patch($patch, 'PAY-FIL-001 local-runtime custody mismatch: '.$relative.'.');
    }
    $relative = 'hrm/country_packs/vn_reference/1.0.0/artifacts/filing-evidence/itaxviewer-05-kk-tncn-tt80-local-runtime-2026-10-06.json';
    $manifest = json_decode(file_get_contents($path_to_root.'/'.$relative), true);
    if (!is_array($manifest) || pay_fil_001_validate_local_runtime_evidence($manifest, $error) === false)
        return pay_fil_001_fail_patch($patch, 'PAY-FIL-001 local-runtime evidence invalid: '.(string)$error.'.');
    return pay_fil_001_require_current_official_schema_boundary($patch);
}

/** Return only the exact source-only compatibility edge activation boundary. */
function pay_fil_001_local_validator_compatibility_required_for_database_version($version)
{
    return version_compare((string)$version, '1.0.1024', '>=');
}
?>
