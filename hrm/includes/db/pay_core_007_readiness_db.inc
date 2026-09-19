<?php
/** Source/schema readiness checks shared by PAY-CORE-007 migration edges. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
function hrm_pay_core_007_require_source_tokens($patch,$relative,array $tokens,$label){global $path_to_root;$s=@file_get_contents($path_to_root.'/'.$relative);if(!is_string($s))return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,$label.' source is missing.');foreach($tokens as $t)if(strpos($s,$t)===false)return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,$label.' source/custody contract is incomplete.');return true;}
function hrm_pay_core_007_require_predecessor($patch){
    global $path_to_root;
    $relative='tests/fixtures/payroll/legacy_v1/manifest_v1.0.423.json';
    $manifest_path=$path_to_root.'/'.$relative;
    if(!is_file($manifest_path))return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CORE-007 immutable predecessor custody source is missing.');
    $expected_hash='0230210020e4f4d33605b5391a2506ee9308f9d9dc4470ceb51c1620ca4be701';
    $actual_hash=@hash_file('sha256',$manifest_path);
    if(!is_string($actual_hash)||!hash_equals($expected_hash,$actual_hash))return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CORE-007 immutable predecessor custody bytes differ from accepted 1.0.423.');
    $manifest=json_decode((string)@file_get_contents($manifest_path),true);
    if(!is_array($manifest)||($manifest['contract']['version']??null)!=='1.0.423'||($manifest['pay_core_006_evidence']['status']??null)!=='CLOSED / LOCAL ACCEPTANCE PASS'||($manifest['pay_core_006_evidence']['mandatory_local_gate_status']??null)!=='PASS'||($manifest['pay_core_006_evidence']['final_operational_closed']??null)!==true)return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CORE-007 immutable predecessor custody semantic contract is incomplete.');
    if(!hrm_pay_core_007_require_source_tokens($patch,'hrm/includes/payroll_engine.inc',array('return calculate_employee_payslip_legacy_v1($employee, $from_date, $to_date, $period_id, $context);'),'PAY-CORE-007 legacy authority'))return false;
    $p=@file_get_contents($path_to_root.'/hrm/includes/payroll_engine.inc');if(strpos($p,'pay_core_007')!==false)return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CORE-007 must not be loaded by the production payroll engine.');return true;
}
?>
