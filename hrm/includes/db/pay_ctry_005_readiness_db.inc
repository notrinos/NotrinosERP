<?php
/** PAY-CTRY-005 predecessor and Vietnam pack source-custody checks. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');

function pay_ctry_005_fail($patch,$message)
{
    return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,$message);
}
function pay_ctry_005_require_predecessor_file($patch,$relative,$sha256)
{
    global $path_to_root;$path=$path_to_root.'/'.$relative;
    $actual=is_file($path)?@hash_file('sha256',$path):false;
    $ok=is_string($actual)&&(function_exists('hash_equals')?hash_equals($sha256,$actual):$actual===$sha256);
    return $ok?true:pay_ctry_005_fail($patch,'PAY-CTRY-005 immutable PAY-CTRY-004 predecessor changed: '.$relative);
}
/** Freeze the accepted PAY-CTRY-004 closure without invoking its historical 1.0.812 current-head guard. */
function pay_ctry_005_require_predecessor($patch)
{
    $frozen=array(
        'includes/pay_ctry_004_contract.inc'=>'319bd3d5ac841a87ac9b36f446f10d8f59a669ba114786d9fddcf81eca21688b',
        'hrm/includes/db/pay_ctry_004_readiness_db.inc'=>'9774eef23600f4ebcc3550f78b661a3671f085df612b48c9508f6a29616bab22',
        'hrm/includes/db/pay_ctry_004_schema_db.inc'=>'8b449e1cc5ce6f3974bbf014efbfdeda7e224662084a489873dd10c29f4da87f',
        'hrm/includes/db/pay_ctry_004_runtime_db.inc'=>'de26204c19a60d04c315de15bcdf836d07de98894ed451c66056063d2c41a80d',
        'sql/upgrade_pay_ctry_004_schema.sql'=>'bc01cfd189aabf662bb9fde2bdec4090a00cb9bfe40c4941d31c8cf602542d01',
        'Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/EXECUTION_REPORT_PAY_CTRY_004_COMPLETION_2026-09-25.md'=>'e41b88bfdc03f920928a0b9d24987df93818a4c042710b762b3317df174a1eb4'
    );
    foreach($frozen as $relative=>$sha)if(!pay_ctry_005_require_predecessor_file($patch,$relative,$sha))return false;
    return true;
}
function pay_ctry_005_load_json($patch,$relative,$label)
{
    global $path_to_root;$raw=@file_get_contents($path_to_root.'/'.$relative);
    if(!is_string($raw))return pay_ctry_005_fail($patch,$label.' source is missing.');
    $data=json_decode($raw,true);
    if(!is_array($data)||json_last_error()!==JSON_ERROR_NONE)return pay_ctry_005_fail($patch,$label.' JSON is invalid.');
    return $data;
}
/** Verify the complete declarative source candidate and exact package roots without executing pack content. */
function pay_ctry_005_require_pack_candidate($patch)
{
    global $path_to_root;
    $base='hrm/country_packs/vn_reference/1.0.0/';
    $candidate=pay_ctry_005_load_json($patch,$base.'package-candidate.json','PAY-CTRY-005 package candidate');if($candidate===false)return false;
    if(($candidate['schema']??'')!=='notrinos.vn.package-candidate.v1'||($candidate['capability_registry_version']??'')!=='pay-ctry-capabilities-v2'||($candidate['pack']['id']??'')!=='org.notrinos.payroll.vn.base'||($candidate['pack']['version']??'')!=='1.0.0')return pay_ctry_005_fail($patch,'PAY-CTRY-005 package identity/registry contract is incomplete.');
    if(($candidate['semantic_root']??'')!=='fc20d9550b20b915c4b4e34ef139caf133edb961a8c29661743da881249af7ec'||($candidate['evidence_root']??'')!=='ee0145671bc5119ccf8e219dcdd7fc92f14db463ee31b01ecedd4b37fed68611')return pay_ctry_005_fail($patch,'PAY-CTRY-005 semantic/evidence root mismatch.');
    if(($candidate['signing']['state']??'')!=='pending-authorized-publisher'||($candidate['signing']['signature_base64']??null)!==null)return pay_ctry_005_fail($patch,'PAY-CTRY-005 must not fabricate a publisher signature.');
    $artifacts=$candidate['artifacts']??null;if(!is_array($artifacts)||count($artifacts)<20)return pay_ctry_005_fail($patch,'PAY-CTRY-005 package member set is incomplete.');
    foreach($artifacts as $a){$rel=$a['path']??'';if(!is_string($rel)||(strpos($rel,'artifacts/')!==0&&strpos($rel,'evidence/')!==0)||preg_match('/\.(php|inc|sql|js|sh|bat|ps1)$/i',$rel))return pay_ctry_005_fail($patch,'PAY-CTRY-005 executable or invalid pack member denied.');$path=$path_to_root.'/'.$base.$rel;$hash=is_file($path)?@hash_file('sha256',$path):false;if(!is_string($hash)||!hash_equals((string)($a['sha256']??''),$hash)||(int)($a['bytes']??-1)!==(int)@filesize($path))return pay_ctry_005_fail($patch,'PAY-CTRY-005 package member custody mismatch: '.$rel);}
    return true;
}
function pay_ctry_005_require_certification_honesty($patch)
{
    $state=pay_ctry_005_load_json($patch,'hrm/country_packs/vn_reference/1.0.0/evidence/certification-state.json','PAY-CTRY-005 certification state');if($state===false)return false;
    if(($state['qualified_vietnamese_payroll_expert_review']['state']??'')!=='missing'||($state['legal_compliance_review']['state']??'')!=='missing'||($state['parallel_payroll_cycles']['state']??'')!=='missing'||($state['publisher_signature']['state']??'')!=='missing'||($state['publication']['state']??'')!=='not-published')return pay_ctry_005_fail($patch,'PAY-CTRY-005 certification evidence must remain honest and fail closed.');
    return true;
}
?>
