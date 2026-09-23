<?php
/**
 * PAY-CTRY-001 country-pack trust and verification contract.
 *
 * Country packs are constrained signed artifacts. They never execute PHP/SQL,
 * register hooks, edit core files, or grant company/payroll activation.
 */
if (!defined('PAY_CTRY_001_CONTRACT_VERSION')) define('PAY_CTRY_001_CONTRACT_VERSION', '1.0.0');
if (!defined('PAY_CTRY_001_MANIFEST_VERSION')) define('PAY_CTRY_001_MANIFEST_VERSION', 'pay-ctry-manifest-v1');
if (!defined('PAY_CTRY_001_VERIFIER_VERSION')) define('PAY_CTRY_001_VERIFIER_VERSION', 'pay-ctry-verifier-v1');
if (!defined('PAY_CTRY_001_FINAL_DB_VERSION')) define('PAY_CTRY_001_FINAL_DB_VERSION', '1.0.776');
if (!defined('PAY_CTRY_001_SITE_COMPANY_ID')) define('PAY_CTRY_001_SITE_COMPANY_ID', 0);
if (!defined('PAY_CTRY_001_MAX_ARCHIVE_BYTES')) define('PAY_CTRY_001_MAX_ARCHIVE_BYTES', 33554432); // 32 MiB
if (!defined('PAY_CTRY_001_MAX_MEMBER_BYTES')) define('PAY_CTRY_001_MAX_MEMBER_BYTES', 4194304); // 4 MiB
if (!defined('PAY_CTRY_001_MAX_TOTAL_MEMBER_BYTES')) define('PAY_CTRY_001_MAX_TOTAL_MEMBER_BYTES', 16777216); // 16 MiB
if (!defined('PAY_CTRY_001_MAX_MEMBERS')) define('PAY_CTRY_001_MAX_MEMBERS', 256);
if (!defined('PAY_CTRY_001_MAX_JSON_DEPTH')) define('PAY_CTRY_001_MAX_JSON_DEPTH', 64);
if (!defined('PAY_CTRY_001_MAX_JSON_NODES')) define('PAY_CTRY_001_MAX_JSON_NODES', 25000);
if (!defined('PAY_CTRY_001_MAX_CAPABILITIES')) define('PAY_CTRY_001_MAX_CAPABILITIES', 128);
if (!defined('PAY_CTRY_001_MAX_DEPENDENCIES')) define('PAY_CTRY_001_MAX_DEPENDENCIES', 64);
if (!defined('PAY_CTRY_001_MAX_COMPRESSION_RATIO')) define('PAY_CTRY_001_MAX_COMPRESSION_RATIO', 100);
if (!defined('PAY_CTRY_001_CLOCK_SKEW_SECONDS')) define('PAY_CTRY_001_CLOCK_SKEW_SECONDS', 300);

/** Site-level authorities. These are intentionally separate from legacy SA_CREATEMODULES. */
if (!defined('PAY_CTRY_001_INSTALL_PERMISSION')) define('PAY_CTRY_001_INSTALL_PERMISSION', 'SA_PAY_CTRY_INSTALL');
if (!defined('PAY_CTRY_001_TRUST_PERMISSION')) define('PAY_CTRY_001_TRUST_PERMISSION', 'SA_PAY_CTRY_TRUST');

/** Core only knows constrained neutral artifact envelopes. PAY-CTRY-003 may add adapters/contracts later. */
function pay_ctry_001_allowed_artifact_types()
{
    return array(
        'declarative-json' => array('application/json', 'text/json'),
        'data-csv' => array('text/csv'),
        'translation-xliff' => array('application/xliff+xml', 'application/xml', 'text/xml'),
        'evidence-json' => array('application/json', 'text/json'),
        'test-fixture-json' => array('application/json', 'text/json')
    );
}

/** Only declarative payload suffixes are allowed at this trust-foundation stage. */
function pay_ctry_001_allowed_suffixes()
{
    return array('json', 'csv', 'xlf', 'xliff', 'xml');
}

/** Evidence types are excluded from semantic root and included in evidence root. */
function pay_ctry_001_is_evidence_type($type)
{
    return $type === 'evidence-json' || $type === 'test-fixture-json';
}

/** Exact v1 capability data is pinned in source as well as in the migration seed. */
function pay_ctry_001_expected_capability_registry()
{
    return array(
        'version'=>'pay-ctry-capabilities-v1',
        'registry_sha256'=>'95d41321a98412df4df289b0ac4df9dcabed417d618bbdb77086ea0fba7237bc',
        'definition_sha256'=>array(
            'core.country_pack.metadata'=>'0155864a5ee53ee39a2b33c6242c208ef66857e5dd224d314469d41563fb0305',
            'core.country_pack.evidence'=>'78965aaf820f59e47d2103be76206968d64efa27f0f4eefd640b1ab08ec6b150'
        ),
        'definitions'=>array(
            'core.country_pack.metadata'=>array('required_constraints'=>array(),'allowed_constraints'=>array('schema'),'exclusive_group'=>null,'allowed_artifact_types'=>array('declarative-json')),
            'core.country_pack.evidence'=>array('required_constraints'=>array(),'allowed_constraints'=>array('kind'),'exclusive_group'=>null,'allowed_artifact_types'=>array('evidence-json','test-fixture-json'))
        )
    );
}

/** Manifest keys are deliberately closed; unknown top-level keys fail closed. */
function pay_ctry_001_manifest_top_level_keys()
{
    return array('manifest_version','pack','host','capability_registry_version','capabilities','dependencies','artifacts','semantic_root','evidence_root','signing');
}

/** No PAY-CTRY-002 activation concepts are valid in a PAY-CTRY-001 manifest. */
function pay_ctry_001_forbidden_manifest_activation_keys()
{
    return array('company','company_id','legal_entity','legal_entity_id','payroll_scope','activation','activate','activation_proposal','impact_simulation','rollback','deactivation','activation_outbox');
}
?>
