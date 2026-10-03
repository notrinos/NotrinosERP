<?php
/** PAY-ECO-001 immutable predecessor custody and source-readiness guards. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
include_once($path_to_root.'/includes/pay_eco_001_contract.inc');
function pay_eco_001_fail_patch($patch,$message){return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,$message);}
function pay_eco_001_predecessor_files(){return array(
 'hrm/country_packs/vn_reference/1.0.0/package-candidate.json'=>'71d7ae81625a17ad39e865ec4f1f09aeca27cf4a5418e41a03924240c0684412',
 'hrm/country_packs/vn_reference/1.0.0/custody.json'=>'c71b9ab7f2a5526b8c60b96b3f3e9b0d0cd1caa113d5d7776be91b47a4c8fc12',
 'hrm/country_packs/astervale_conformance/1.0.0/package-candidate.json'=>'dcef593f49706042f0ba171eb1d05a09520d869dd4738cb19f091a0f07dd8abb',
 'hrm/country_packs/astervale_conformance/1.0.0/custody.json'=>'bb0ee184f91508504196c65cc4eaef38a92c9723e1420a0d77e39d83a9d15072',
 'Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/EXECUTION_REPORT_PAY_CTRY_007_CLOSURE_ACCEPTANCE_2026-09-26.md'=>'ca93aeb7735c247ce50615c9041ef1c38e1a5125a89e199e2c7a1304775fc2d5',
 'includes/pay_ctry_004_contract.inc'=>'319bd3d5ac841a87ac9b36f446f10d8f59a669ba114786d9fddcf81eca21688b',
 'includes/pay_ctry_007_contract.inc'=>'3c8afc457ca76417e83a9cefdd63acc4c3879829e2ec5c0f0f4b419fef15a3ee');}
function pay_eco_001_require_predecessor($patch){global $path_to_root;foreach(pay_eco_001_predecessor_files() as $rel=>$sha){$file=$path_to_root.'/'.$rel;if(!is_file($file)||!hash_equals($sha,hash_file('sha256',$file)))return pay_eco_001_fail_patch($patch,'PAY-ECO-001 immutable predecessor custody mismatch: '.$rel.'.');}return true;}
function pay_eco_001_require_dependencies($patch){global $path_to_root;$checks=array(
 'includes/pay_ctry_001_verifier.inc'=>array('function pay_ctry_001_validate_trust_and_signature','function pay_ctry_001_canonical_json'),
 'includes/pay_ctry_004_contract.inc'=>array('function pay_ctry_004_verify_certification_signature','function pay_ctry_004_publication_guard'),
 'includes/pay_ctry_007_contract.inc'=>array('function pay_ctry_007_authority_contract','function pay_ctry_007_verify_catalog_signature'),
 'hrm/country_packs/vn_reference/1.0.0/package-candidate.json'=>array('pending-authorized-publisher','DENIED until a valid PAY-CTRY-001 manifest signature','pending PAY-CTRY-004 evidence'),
 'hrm/country_packs/astervale_conformance/1.0.0/package-candidate.json'=>array('not-signed-conformance-only','DENIED for production','Certified/Published are forbidden'));
 foreach($checks as $rel=>$tokens){$raw=@file_get_contents($path_to_root.'/'.$rel);if(!is_string($raw)||$raw==='')return pay_eco_001_fail_patch($patch,'PAY-ECO-001 dependency source missing: '.$rel.'.');foreach($tokens as $token)if(strpos($raw,$token)===false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 dependency source/custody contract incomplete: '.$rel.' missing '.$token.'.');}return true;}
function pay_eco_001_require_contract($patch){$authority=pay_eco_001_authority_contract();$allowed=array('immutable_publisher_profile_custody','immutable_pack_candidate_custody','immutable_catalog_decision_custody','publisher_profile_writer','pack_candidate_writer');foreach($authority as $name=>$value){$expected=in_array($name,$allowed,true);if($value!==$expected)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 authority ceiling violated: '.$name.'.');}return true;}

/** Require the accepted onboarding permissions and exact authority ceiling. */
function pay_eco_001_require_onboarding_contract($patch){
 $permissions=pay_eco_001_onboarding_permissions();
 if(!isset($permissions['publisher_profile'],$permissions['pack_candidate'])||$permissions['publisher_profile']!=='SA_PAY_CTRY_TRUST'||$permissions['pack_candidate']!=='SA_PAY_CTRY_INSTALL')return pay_eco_001_fail_patch($patch,'PAY-ECO-001 onboarding permission contract mismatch.');
 $authority=pay_eco_001_authority_contract();
 foreach(array('publisher_profile_writer','pack_candidate_writer') as $name)if(!isset($authority[$name])||$authority[$name]!==true)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 onboarding writer authority missing: '.$name.'.');
 foreach(array('catalog_decision_writer','sandbox_execution','dependency_vulnerability_scanning','signed_reproducible_artifact_admission','marketplace_publication','automatic_catalog_admission','pack_installation','company_activation','payroll_execution','payroll_posting','payment_authority','general_ledger_authority','statutory_filing_authority','external_network_delivery','permission_grant','publisher_self_verification','certification_self_claim','historical_payroll_rewrite','core_source_write') as $name)if(!array_key_exists($name,$authority)||$authority[$name]!==false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 successor authority ceiling violated: '.$name.'.');
 return true;
}
/** Require the selected onboarding writer implementation to match its boundary. */
function pay_eco_001_require_onboarding_writer_source($patch,$scope){global $path_to_root;$file=$path_to_root.'/hrm/includes/db/pay_eco_001_onboarding_db.inc';$raw=@file_get_contents($file);if(!is_string($raw)||$raw==='')return pay_eco_001_fail_patch($patch,'PAY-ECO-001 onboarding writer source missing.');$tokens=array(
 'publisher'=>array('function pay_eco_001_write_publisher_profile(','PAY_ECO_001_PUBLISHER_PROFILE_PERMISSION','pay.eco.publisher_profile.appended','publisher_profile_replay_conflict','pay_ctry_001_publisher_revoked'),
 'candidate'=>array('function pay_eco_001_write_pack_candidate(','PAY_ECO_001_PACK_CANDIDATE_PERMISSION',"final_status='accepted'",'pack_candidate_publisher_lineage_mismatch','pay.eco.pack_candidate.appended',"'marketplace_publication'=>false")
 );if(!isset($tokens[$scope]))return pay_eco_001_fail_patch($patch,'PAY-ECO-001 onboarding writer scope invalid.');foreach($tokens[$scope] as $token)if(strpos($raw,$token)===false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 '.$scope.' writer source contract incomplete: '.$token.'.');return true;}
?>
