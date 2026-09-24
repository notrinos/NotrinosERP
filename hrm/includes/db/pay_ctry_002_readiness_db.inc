<?php
/** PAY-CTRY-002 readiness bound to immutable accepted PAY-CTRY-001 custody. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
include_once($path_to_root.'/includes/pay_ctry_002_contract.inc');

function pay_ctry_002_require_source_tokens($patch,$relative,array $tokens,$label)
{
    global $path_to_root;$s=@file_get_contents($path_to_root.'/'.$relative);
    if(!is_string($s))return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,$label.' source is missing.');
    foreach($tokens as $t)if(strpos($s,$t)===false)return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,$label.' source/custody contract is incomplete.');
    return true;
}
function pay_ctry_002_require_predecessor($patch)
{
    global $path_to_root;
    $active=$path_to_root.'/tests/fixtures/payroll/legacy_v1/manifest.json';
    $accepted=$path_to_root.'/tests/fixtures/payroll/legacy_v1/manifest_v1.0.454.json';
    $expected='125c297f9bab66e21bfcd17900665623e8345bb9d4e67a6c559390692f89fedf';
    if(!is_file($accepted)||!is_string($h=@hash_file('sha256',$accepted))||!hash_equals($expected,$h))
        return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CTRY-002 immutable accepted PAY-CTRY-001 custody is missing or changed.');
    $f=json_decode((string)@file_get_contents($accepted),true);$fe=is_array($f)&&isset($f['pay_ctry_001_evidence'])?$f['pay_ctry_001_evidence']:array();$fc=is_array($f)&&isset($f['contract'])?$f['contract']:array();
    if((isset($fc['version'])?$fc['version']:'')!=='1.0.454'||(isset($fe['status'])?$fe['status']:'')!=='CLOSED / LOCAL ACCEPTANCE PASS'||(isset($fe['mandatory_local_gate_status'])?$fe['mandatory_local_gate_status']:'')!=='PASS'||(isset($fe['database_version'])?$fe['database_version']:'')!=='1.0.776'||(isset($fe['pay_ctry_002_started'])?$fe['pay_ctry_002_started']:true)!==false)
        return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CTRY-002 frozen accepted PAY-CTRY-001 predecessor contract is incomplete.');
    $m=json_decode((string)@file_get_contents($active),true);$e=is_array($m)&&isset($m['pay_ctry_002_evidence'])?$m['pay_ctry_002_evidence']:array();$c=is_array($m)&&isset($m['contract'])?$m['contract']:array();
    if((isset($c['version'])?$c['version']:'')!=='1.0.455'||(isset($e['status'])?$e['status']:'')!=='SOURCE-VERIFIED / LOCAL-GATE-PENDING'||(isset($e['source_custody_version'])?$e['source_custody_version']:'')!=='1.0.455'||(isset($e['accepted_predecessor_custody'])?$e['accepted_predecessor_custody']:'')!=='1.0.454'||(isset($e['accepted_predecessor_sha256'])?$e['accepted_predecessor_sha256']:'')!==$expected||(isset($e['migration_start'])?$e['migration_start']:'')!=='1.0.776'||(isset($e['migration_end'])?$e['migration_end']:'')!=='1.0.779')
        return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CTRY-002 active source/custody contract is incomplete.');
    return true;
}
?>
