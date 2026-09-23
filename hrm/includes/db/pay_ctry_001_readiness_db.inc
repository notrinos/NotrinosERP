<?php
/** PAY-CTRY-001 migration readiness and immutable accepted HRM-ESS-002 predecessor custody. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
include_once(dirname(__FILE__).'/pay_ctry_001_schema_db.inc');

function pay_ctry_001_require_source_tokens($patch,$relative,array $tokens,$label)
{
    global $path_to_root;$s=@file_get_contents($path_to_root.'/'.$relative);
    if(!is_string($s))return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,$label.' source is missing.');
    foreach($tokens as $t)if(strpos($s,$t)===false)return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,$label.' source/custody contract is incomplete.');
    return true;
}
function pay_ctry_001_require_predecessor($patch)
{
    global $path_to_root;$file=$path_to_root.'/tests/fixtures/payroll/legacy_v1/manifest_v1.0.451.json';
    $expected='81e05a4fe0654b5bfb1722991b5561418f06a709ab781da02e364bece7d66c95';
    if(!is_file($file)||!is_string($h=@hash_file('sha256',$file))||!hash_equals($expected,$h))
        return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CTRY-001 immutable accepted HRM-ESS-002 custody is missing or changed.');
    $m=json_decode((string)@file_get_contents($file),true);$e=is_array($m)&&isset($m['hrm_ess_002_evidence'])?$m['hrm_ess_002_evidence']:null;$c=is_array($m)&&isset($m['contract'])?$m['contract']:array();
    if(!is_array($e)||!isset($c['version'])||$c['version']!=='1.0.451'||!isset($e['status'])||$e['status']!=='CLOSED / LOCAL ACCEPTANCE PASS'||!isset($e['mandatory_local_gate_status'])||$e['mandatory_local_gate_status']!=='PASS'||!isset($e['post_acceptance_database_version'])||$e['post_acceptance_database_version']!=='1.0.758'||!array_key_exists('pay_ctry_001_started',$e)||$e['pay_ctry_001_started']!==false)
        return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CTRY-001 semantic HRM-ESS-002 predecessor contract is incomplete.');
    return true;
}
function pay_ctry_001_require_active_source_custody($patch)
{
    global $path_to_root;$file=$path_to_root.'/tests/fixtures/payroll/legacy_v1/manifest.json';$m=json_decode((string)@file_get_contents($file),true);
    if(!is_array($m)||!isset($m['contract']['version'])||$m['contract']['version']!=='1.0.453'||!isset($m['pay_ctry_001_evidence']['status'])||$m['pay_ctry_001_evidence']['status']!=='SOURCE-CLOSED / LOCAL-GATE-PENDING')
        return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CTRY-001 active source custody is incomplete.');
    return true;
}
?>
