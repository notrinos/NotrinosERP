<?php
/** PAY-CTRY-004 predecessor, dependency and source-readiness checks. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
include_once(dirname(__FILE__).'/pay_ctry_004_schema_db.inc');
include_once($path_to_root.'/includes/pay_ctry_004_contract.inc');

function pay_ctry_004_require_source_tokens($patch,$relative,array $tokens,$label)
{
    global $path_to_root;$source=@file_get_contents($path_to_root.'/'.$relative);
    if(!is_string($source))return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,$label.' source is missing.');
    foreach($tokens as $token)if(strpos($source,$token)===false)return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,$label.' source/custody contract is incomplete.');
    return true;
}
function pay_ctry_004_require_predecessor_file($patch,$relative,$sha256)
{
    global $path_to_root;$path=$path_to_root.'/'.$relative;
    $actual=is_file($path)?@hash_file('sha256',$path):false;
    $matches=is_string($actual)&&(function_exists('hash_equals')?hash_equals($sha256,$actual):$actual===$sha256);
    if(!$matches)return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CTRY-004 immutable PAY-CTRY-003 predecessor changed: '.$relative);
    return true;
}
/** Require the independently accepted PAY-CTRY-003 closure without calling its historical current-head guard. */
function pay_ctry_004_require_predecessor($patch)
{
    global $path_to_root;
    $frozen=array(
        'includes/pay_ctry_003_contract.inc'=>'97021a703a87473e5d6281fcdd1f94ab7adea2c2c3892ce0215d83c55f793b23',
        'hrm/includes/db/pay_ctry_003_readiness_db.inc'=>'18521175c37812458316d368b6ff9dbee370c7b79ae962ade8e13a3e648c42bf',
        'hrm/includes/db/pay_ctry_003_schema_db.inc'=>'c3406671817a8540930dd286c36ddaf212c64f92ce89e592ba2995a15cba8724',
        'sql/upgrade_pay_ctry_003_schema.sql'=>'a0ad56c9216d089f47160bf3ca12249626a16a6d373142dab8674fa4f47add2d',
        'Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/EXECUTION_REPORT_PAY_CTRY_003_COMPLETION_2026-09-25.md'=>'53efee4f677f10e5fa11364bc605912db1dafba1926eca98923723c4d32db127'
    );
    foreach($frozen as $relative=>$sha)if(!pay_ctry_004_require_predecessor_file($patch,$relative,$sha))return false;
    $version=@file_get_contents($path_to_root.'/version.php');
    if(!is_string($version)||strpos($version,"\$db_version = '1.0.812';")===false)
        return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CTRY-004 requires target source head 1.0.812.');
    $registry=hrm_migration_head_registry();
    if((isset($registry['active_database_version'])?$registry['active_database_version']:'')!=='1.0.812'||(isset($registry['active_legacy_payroll_contract'])?$registry['active_legacy_payroll_contract']:'')!=='1.0.458'||(isset($registry['next_backlog_item'])?$registry['next_backlog_item']:'')!=='PAY-CTRY-005')
        return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CTRY-004 active registry/source custody mismatch.');
    return true;
}
/** Prove the bounded PAY-TST-001 capabilities PAY-CTRY-004 consumes already exist. */
function pay_ctry_004_require_pay_tst_dependency($patch)
{
    global $path_to_root;
    $required=array(
        'tests/php/test_pay_tst_001_runner.php'=>array('PAY-TST-001','fixture'),
        'sql/upgrade_pay_tst_001_disposable_fixture_pack.php'=>array('fixture'),
        'sql/upgrade_pay_tst_001_unified_php_runner.php'=>array('runner')
    );
    foreach($required as $relative=>$tokens){$source=@file_get_contents($path_to_root.'/'.$relative);if(!is_string($source))return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CTRY-004 PAY-TST-001 dependency source missing: '.$relative);foreach($tokens as $token)if(stripos($source,$token)===false)return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CTRY-004 PAY-TST-001 dependency capability incomplete: '.$relative);}
    return true;
}
function pay_ctry_004_require_permissions($patch)
{
    return pay_ctry_004_require_source_tokens($patch,'includes/access_levels.inc',array(
        PAY_CTRY_004_TECHNICAL_PERMISSION,PAY_CTRY_004_PAYROLL_EXPERT_PERMISSION,PAY_CTRY_004_LEGAL_PERMISSION,
        PAY_CTRY_004_CERTIFY_PERMISSION,PAY_CTRY_004_PUBLISH_PERMISSION,PAY_CTRY_004_EMERGENCY_PERMISSION,PAY_CTRY_004_RECOVERY_PERMISSION,
        'no automatic role grant'),'PAY-CTRY-004 least-privilege permissions');
}
?>
