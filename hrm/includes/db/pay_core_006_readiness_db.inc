<?php
/** Source/schema readiness checks shared by PAY-CORE-006 migration edges. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
function hrm_pay_core_006_require_source_tokens($patch,$relative,array $tokens,$label){global $path_to_root;$s=@file_get_contents($path_to_root.'/'.$relative);if(!is_string($s))return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,$label.' source is missing.');foreach($tokens as $t)if(strpos($s,$t)===false)return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,$label.' source/custody contract is incomplete.');return true;}
function hrm_pay_core_006_require_predecessor($patch){
    if(!hrm_pay_core_006_require_source_tokens($patch,'hrm/includes/payroll_engine.inc',array("define('HRM_LEGACY_PAYROLL_CONTRACT_VERSION', '1.0.422')",'return calculate_employee_payslip_legacy_v1($employee, $from_date, $to_date, $period_id, $context);'),'PAY-CORE-006 legacy authority'))return false;
    global $path_to_root;$p=@file_get_contents($path_to_root.'/hrm/includes/payroll_engine.inc');if(strpos($p,'pay_core_006')!==false)return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CORE-006 must not be loaded by the production payroll engine.');return true;
}
?>