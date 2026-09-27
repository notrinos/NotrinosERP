<?php
/** PAY-FIL-001 predecessor, dependency and least-privilege readiness. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
include_once($path_to_root.'/includes/pay_fil_001_contract.inc');
include_once($path_to_root.'/includes/pay_fil_001_official_schema_contract.inc');
function pay_fil_001_fail_patch($patch,$m){return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,$m);}
function pay_fil_001_require_predecessor($patch){global $path_to_root;$files=array(
 'Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/EXECUTION_REPORT_PAY_PMT_001_COMPLETION_2026-09-27.md'=>'dc4df2b1cdeb967e4c6f0c0bbc1c136b14eb2bc97f77bda54c0b62fa9534d65d',
 'Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/EXECUTION_REPORT_PAY_CTRY_003_COMPLETION_2026-09-25.md'=>'53efee4f677f10e5fa11364bc605912db1dafba1926eca98923723c4d32db127',
 'Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/EXECUTION_REPORT_PAY_CTRY_004_COMPLETION_2026-09-25.md'=>'e41b88bfdc03f920928a0b9d24987df93818a4c042710b762b3317df174a1eb4',
 'tests/fixtures/payroll/legacy_v1/manifest_v1.0.461.json'=>'fe8aa2b7a30713dbe65d3ccee93034f6c8f0c9e206f7293c74656d6fbdbe86d9');
 foreach($files as $rel=>$sha)if(!is_file($path_to_root.'/'.$rel)||!hash_equals($sha,hash_file('sha256',$path_to_root.'/'.$rel)))return pay_fil_001_fail_patch($patch,'PAY-FIL-001 immutable predecessor custody mismatch: '.$rel.'.');return true;}
function pay_fil_001_require_dependencies($patch){global $path_to_root;$checks=array(
 'hrm/includes/payroll/pay_core_010_contract.inc'=>array('result_set_sha256','approval','projection'),
 'includes/pay_ctry_003_contract.inc'=>array('filings.declarative.v1','jurisdiction.filings','payment_execution'),
 'includes/pay_ctry_004_contract.inc'=>array("'Published'","'Suspended'","'Revoked'"),
 'includes/pay_pmt_001_contract.inc'=>array("'statutory_filing_authority'=>false","'network_authority'=>false"),
 'hrm/country_packs/vn_reference/1.0.0/artifacts/filing-boundaries.json'=>array('"generation_allowed": false','"transmission_allowed": false','PAY-FIL-001 or equivalent governed submission connector'));
 foreach($checks as $rel=>$tokens){$raw=@file_get_contents($path_to_root.'/'.$rel);if(!is_string($raw)||$raw==='')return pay_fil_001_fail_patch($patch,'PAY-FIL-001 dependency source missing: '.$rel.'.');foreach($tokens as $t)if(strpos($raw,$t)===false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 dependency source/custody contract incomplete: '.$rel.' missing '.$t.'.');}return true;}
function pay_fil_001_require_contract($patch){$a=pay_fil_001_authority_contract();foreach(array('legal_entity_period_jurisdiction_binding','many_to_many_exact_source_linkage','regular_offcycle_correction_aggregation','pinned_country_pack_schema_connector','immutable_obligation_and_source_custody','maker_checker_authorization_required','signed_payload_hash_required','submission_event_chain_required','ack_reject_correct_amend_chain_required','tax_data_restriction_required','retention_evidence_required') as $k)if(!isset($a[$k])||$a[$k]!==true)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 required contract capability missing: '.$k.'.');foreach(array('official_form_generation_without_named_schema','direct_government_submission','automatic_submission','unreviewed_correction_or_amendment','fabricated_historical_source_link','historical_payroll_or_filing_mutation','payment_authority','gl_authority','cross_company_filing','network_authority','connector_secret_custody','selective_downgrade') as $k)if(!array_key_exists($k,$a)||$a[$k]!==false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 authority ceiling violated: '.$k.'.');return true;}

function pay_fil_001_require_foundation_acceptance($patch){global $path_to_root;$files=array(
 'Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/EXECUTION_REPORT_PAY_FIL_001_FOUNDATION_BATCH_2026-09-27.md'=>'2c9b639f4fea26042805277cc2aa2c1aca1930bdb5e6c0ecf2b50ee2c010b8ef',
 'tests/fixtures/payroll/legacy_v1/manifest_v1.0.462.json'=>'edfafe6d1bcaf568b1b585eb07435e736fb7e12bdc095ae78fca8829ec8245cc');
 foreach($files as $rel=>$sha)if(!is_file($path_to_root.'/'.$rel)||!hash_equals($sha,hash_file('sha256',$path_to_root.'/'.$rel)))return pay_fil_001_fail_patch($patch,'PAY-FIL-001 accepted foundation custody mismatch: '.$rel.'.');return true;}

/** Require the accepted 1.0.853 pre-submission source/owner-machine custody before authorization foundations. */
function pay_fil_001_require_presubmission_acceptance($patch){global $path_to_root;$files=array(
 'Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/EXECUTION_REPORT_PAY_FIL_001_PRESUBMISSION_BATCH_2026-09-27.md'=>'317437810cc5f959bac1c3d48277029deb45a57cc2d739799e605095810efb67',
 'tests/fixtures/payroll/legacy_v1/manifest_v1.0.463.json'=>'64eee9925346ad321c6e1f3ae6e094fea8978c7656001cf04fd93561f097c01b');
 foreach($files as $rel=>$sha)if(!is_file($path_to_root.'/'.$rel)||!hash_equals($sha,hash_file('sha256',$path_to_root.'/'.$rel)))return pay_fil_001_fail_patch($patch,'PAY-FIL-001 accepted pre-submission custody mismatch: '.$rel.'.');return true;}

/** Keep the current VN reference boundary explicitly fail-closed until a named official schema is reviewed. */
function pay_fil_001_require_current_official_schema_boundary($patch){global $path_to_root;$rel='hrm/country_packs/vn_reference/1.0.0/artifacts/filing-boundaries.json';$raw=@file_get_contents($path_to_root.'/'.$rel);if(!is_string($raw)||hash('sha256',$raw)!=='388d8a6d459afc4d4ddab9515fda510dfa23d46a99fee8890be035fce19b50ee')return pay_fil_001_fail_patch($patch,'PAY-FIL-001 current filing boundary custody mismatch.');$b=json_decode($raw,true);if(!is_array($b)||!isset($b['official_supported_schemas'])||!is_array($b['official_supported_schemas'])||count($b['official_supported_schemas'])!==0)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 current filing boundary unexpectedly names an official schema.');foreach(array('tax_filing','social_insurance_filing') as $k)if(!isset($b[$k])||!array_key_exists('generation_allowed',$b[$k])||$b[$k]['generation_allowed']!==false||!array_key_exists('transmission_allowed',$b[$k])||$b[$k]['transmission_allowed']!==false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 current filing boundary no longer fails closed: '.$k.'.');return true;}

function pay_fil_001_require_permissions($patch){global $path_to_root;$raw=@file_get_contents($path_to_root.'/includes/access_levels.inc');if(!is_string($raw))return pay_fil_001_fail_patch($patch,'PAY-FIL-001 permission source missing.');foreach(array(PAY_FIL_001_PREPARE_PERMISSION,PAY_FIL_001_REVIEW_PERMISSION,PAY_FIL_001_AUTHORIZE_PERMISSION,PAY_FIL_001_SUBMIT_PERMISSION,PAY_FIL_001_RECOVER_PERMISSION,PAY_FIL_001_VIEW_PERMISSION,'no automatic role grant') as $t)if(strpos($raw,$t)===false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 least-privilege permission source incomplete: '.$t.'.');return true;}
?>
