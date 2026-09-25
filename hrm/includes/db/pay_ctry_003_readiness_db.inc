<?php
/** PAY-CTRY-003 predecessor and source-readiness checks. */
include_once(dirname(__FILE__).'/pay_ctry_002_readiness_db.inc');
include_once(dirname(__FILE__).'/pay_ctry_003_schema_db.inc');

/** Require exact source markers before a PAY-CTRY-003 edge can run. */
function pay_ctry_003_require_source_tokens($patch, $relative, array $tokens, $label)
{
    global $path_to_root;
    $source = @file_get_contents($path_to_root.'/'.$relative);
    if (!is_string($source)) return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, $label.' source is missing.');
    foreach ($tokens as $token) if (strpos($source, $token) === false) return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, $label.' source contract is incomplete.');
    return true;
}

/** Require the accepted PAY-CTRY-002 closure before any PAY-CTRY-003 edge. */
function pay_ctry_003_require_predecessor($patch)
{
    global $path_to_root;
    if (!pay_ctry_002_require_predecessor($patch)) return false;
    $version = @file_get_contents($path_to_root.'/version.php');
    if (!is_string($version) || strpos($version, "\$db_version = '1.0.797';") === false)
        return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'PAY-CTRY-003 requires target source head 1.0.797.');
    $registry = hrm_migration_head_registry();
    $active_database = isset($registry['active_database_version']) ? $registry['active_database_version'] : '';
    $active_custody = isset($registry['active_legacy_payroll_contract']) ? $registry['active_legacy_payroll_contract'] : '';
    if ($active_database !== '1.0.797' || $active_custody !== '1.0.458')
        return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'PAY-CTRY-003 requires active PAY-CTRY-002 registry custody.');
    return true;
}

/** Return a stable digest for a canonical JSON seed descriptor. */
function pay_ctry_003_seed_digest($value)
{
    return hash('sha256', json_encode($value, JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE));
}

/** Seed or verify the immutable signed registry, definitions and adapters. */
function pay_ctry_003_seed_registry($patch)
{
    $registry = pay_ctry_003_registry_descriptor();
    $version = db_escape(PAY_CTRY_003_REGISTRY_VERSION);
    $count_query = db_query('SELECT COUNT(*) c FROM '.TB_PREF.'pay_ctry_capability_contract_versions');
    $count_row = $count_query ? db_fetch_assoc($count_query) : false;
    if (!$count_row) return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-CTRY-003 registry seed count query failed.');
    $count = (int)$count_row['c'];
    if ($count === 0) {
        $signature = hash('sha256', PAY_CTRY_003_REGISTRY_VERSION.'|'.PAY_CTRY_003_REGISTRY_SHA256.'|site-trust-ed25519-v1');
        $sql = 'INSERT INTO '.TB_PREF.'pay_ctry_capability_contract_versions (registry_version,registry_sha256,signature_scheme,signed_by,signature_sha256,host_min_version,host_max_version,status,created_by,created_at) VALUES ('.$version.','.db_escape(PAY_CTRY_003_REGISTRY_SHA256).','.db_escape('site-trust-ed25519-v1').','.db_escape('pay-ctry-003-governed-registry').','.db_escape($signature).','.db_escape('1.0').','.db_escape('1.0').','.db_escape('active').',0,NOW())';
        if (!db_query($sql)) return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-CTRY-003 registry seed insert failed.');
        foreach (pay_ctry_003_capability_definitions() as $code => $definition) {
            $definition_hash = hash('sha256', json_encode(array('capability_code'=>$code,'definition'=>$definition), JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE));
            $sql = 'INSERT INTO '.TB_PREF.'pay_ctry_capability_contract_definitions (registry_version_id,capability_code,namespace,semantic,cardinality,exclusive_group,artifact_type,host_adapter,host_version,definition_sha256,created_at) SELECT registry_version_id,'.db_escape($code).','.db_escape($definition['namespace']).','.db_escape($definition['semantic']).','.db_escape($definition['cardinality']).','.($definition['exclusive_group'] === null ? 'NULL' : db_escape($definition['exclusive_group'])).','.db_escape($definition['artifact_type']).','.db_escape($definition['host_adapter']).','.db_escape($definition['host_version']).','.db_escape($definition_hash).',NOW() FROM '.TB_PREF.'pay_ctry_capability_contract_versions WHERE registry_version='.$version;
            if (!db_query($sql)) return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-CTRY-003 capability definition seed failed: '.$code);
        }
        foreach (pay_ctry_003_host_adapters() as $code => $adapter) {
            $adapter_hash = hash('sha256', json_encode(array('adapter_code'=>$code,'adapter'=>$adapter), JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE));
            $sql = 'INSERT INTO '.TB_PREF.'pay_ctry_host_adapters (adapter_code,adapter_version,media_types_json,adapter_sha256,status,created_at) VALUES ('.db_escape($code).','.db_escape($adapter['version']).','.db_escape(json_encode($adapter['media_types'], JSON_UNESCAPED_SLASHES)).','.db_escape($adapter_hash).','.db_escape('active').',NOW())';
            if (!db_query($sql)) return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-CTRY-003 host adapter seed failed: '.$code);
        }
    } elseif ($count !== 1) {
        return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'PAY-CTRY-003 registry seed is partial or duplicated.');
    }
    $registry_query = db_query('SELECT registry_sha256,status FROM '.TB_PREF.'pay_ctry_capability_contract_versions WHERE registry_version='.$version.' LIMIT 2');
    $registry_row = $registry_query ? db_fetch_assoc($registry_query) : false;
    if (!$registry_row || $registry_row['registry_sha256'] !== PAY_CTRY_003_REGISTRY_SHA256 || $registry_row['status'] !== 'active')
        return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'PAY-CTRY-003 signed registry seed does not match source custody.');
    $definitions = pay_ctry_003_capability_definitions();
    $definition_count = db_query('SELECT COUNT(*) c FROM '.TB_PREF.'pay_ctry_capability_contract_definitions');
    $definition_row = $definition_count ? db_fetch_assoc($definition_count) : false;
    if (!$definition_row || (int)$definition_row['c'] !== count($definitions)) return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'PAY-CTRY-003 capability registry definition count mismatch.');
    $adapter_count = db_query('SELECT COUNT(*) c FROM '.TB_PREF.'pay_ctry_host_adapters');
    $adapter_row = $adapter_count ? db_fetch_assoc($adapter_count) : false;
    if (!$adapter_row || (int)$adapter_row['c'] !== count(pay_ctry_003_host_adapters())) return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'PAY-CTRY-003 host adapter registry count mismatch.');
    return true;
}
?>
