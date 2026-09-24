<?php
/** PAY-CTRY-002 readiness bound to immutable accepted PAY-CTRY-001 and immediate PAY-CTRY-002 predecessor custody. */
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
    $prior=$path_to_root.'/tests/fixtures/payroll/legacy_v1/manifest_v1.0.455.json';
    $accepted_hash='125c297f9bab66e21bfcd17900665623e8345bb9d4e67a6c559390692f89fedf';
    $prior_hash='c6e5df7b98c30efa4c8eeb6eb0188be599aee0edfc37afa86f51cb0dd1edd489';
    if(!is_file($accepted)||!is_string($h=@hash_file('sha256',$accepted))||!hash_equals($accepted_hash,$h))
        return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CTRY-002 immutable accepted PAY-CTRY-001 custody is missing or changed.');
    $f=json_decode((string)@file_get_contents($accepted),true);$fe=is_array($f)&&isset($f['pay_ctry_001_evidence'])?$f['pay_ctry_001_evidence']:array();$fc=is_array($f)&&isset($f['contract'])?$f['contract']:array();
    if((isset($fc['version'])?$fc['version']:'')!=='1.0.454'||(isset($fe['status'])?$fe['status']:'')!=='CLOSED / LOCAL ACCEPTANCE PASS'||(isset($fe['mandatory_local_gate_status'])?$fe['mandatory_local_gate_status']:'')!=='PASS'||(isset($fe['database_version'])?$fe['database_version']:'')!=='1.0.776'||(isset($fe['pay_ctry_002_started'])?$fe['pay_ctry_002_started']:true)!==false)
        return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CTRY-002 frozen accepted PAY-CTRY-001 predecessor contract is incomplete.');
    if(!is_file($prior)||!is_string($ph=@hash_file('sha256',$prior))||!hash_equals($prior_hash,$ph))
        return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CTRY-002 immutable source-custody predecessor 1.0.455 is missing or changed.');
    $pm=json_decode((string)@file_get_contents($prior),true);$pe=is_array($pm)&&isset($pm['pay_ctry_002_evidence'])?$pm['pay_ctry_002_evidence']:array();$pc=is_array($pm)&&isset($pm['contract'])?$pm['contract']:array();
    if((isset($pc['version'])?$pc['version']:'')!=='1.0.455'||(isset($pe['source_custody_version'])?$pe['source_custody_version']:'')!=='1.0.455'||(isset($pe['migration_end'])?$pe['migration_end']:'')!=='1.0.779'||(isset($pe['activation_execution_started'])?$pe['activation_execution_started']:true)!==false)
        return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CTRY-002 frozen source-custody predecessor 1.0.455 contract is incomplete.');
    $m=json_decode((string)@file_get_contents($active),true);$e=is_array($m)&&isset($m['pay_ctry_002_evidence'])?$m['pay_ctry_002_evidence']:array();$c=is_array($m)&&isset($m['contract'])?$m['contract']:array();$prev=is_array($m)&&isset($m['previous_contract_manifest'])?$m['previous_contract_manifest']:array();
    if((isset($c['version'])?$c['version']:'')!=='1.0.456'||!in_array((isset($e['status'])?$e['status']:''),array('SOURCE-VERIFIED / LOCAL-GATE-PENDING','SOURCE-VERIFIED / LOCAL ACCEPTANCE PASS'),true)||(isset($e['source_custody_version'])?$e['source_custody_version']:'')!=='1.0.456'||(isset($e['accepted_predecessor_custody'])?$e['accepted_predecessor_custody']:'')!=='1.0.454'||(isset($e['accepted_predecessor_sha256'])?$e['accepted_predecessor_sha256']:'')!==$accepted_hash||(isset($e['migration_start'])?$e['migration_start']:'')!=='1.0.776'||(isset($e['migration_end'])?$e['migration_end']:'')!=='1.0.782'||(isset($prev['version'])?$prev['version']:'')!=='1.0.455'||(isset($prev['sha256'])?$prev['sha256']:'')!==$prior_hash)
        return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CTRY-002 active source/custody contract is incomplete.');
    return true;
}
?>
