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
 'includes/pay_ctry_002_contract.inc'=>array('PAY_CTRY_002_EXECUTOR_PERMISSION','activation_requires_independent_approval','automatic_pack_activation_allowed'),
 'hrm/includes/db/pay_ctry_002_runtime_db.inc'=>array('function pay_ctry_002_runtime_resolution_for_update','function pay_ctry_002_activation_lifecycle_clear_for_update'),
 'hrm/country_packs/vn_reference/1.0.0/package-candidate.json'=>array('pending-authorized-publisher','DENIED until a valid PAY-CTRY-001 manifest signature','pending PAY-CTRY-004 evidence'),
 'hrm/country_packs/astervale_conformance/1.0.0/package-candidate.json'=>array('not-signed-conformance-only','DENIED for production','Certified/Published are forbidden'));
 foreach($checks as $rel=>$tokens){$raw=@file_get_contents($path_to_root.'/'.$rel);if(!is_string($raw)||$raw==='')return pay_eco_001_fail_patch($patch,'PAY-ECO-001 dependency source missing: '.$rel.'.');foreach($tokens as $token)if(strpos($raw,$token)===false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 dependency source/custody contract incomplete: '.$rel.' missing '.$token.'.');}return true;}
function pay_eco_001_require_contract($patch){$authority=pay_eco_001_authority_contract();$allowed=array('immutable_publisher_profile_custody','immutable_pack_candidate_custody','immutable_catalog_decision_custody','immutable_sandbox_evaluation_custody','publisher_profile_writer','pack_candidate_writer','sandbox_evaluation_writer','immutable_dependency_scan_custody','dependency_integrity_scanner','immutable_vulnerability_snapshot_custody','immutable_vulnerability_advisory_custody','immutable_vulnerability_correlation_custody','vulnerability_intelligence_ingest_writer','dependency_vulnerability_scanning','immutable_artifact_admission_custody','signed_reproducible_artifact_admission','catalog_decision_writer','immutable_catalog_publication_custody','catalog_publication_writer','catalog_runtime_read','immutable_catalog_lifecycle_custody','catalog_lifecycle_propagation_writer','catalog_runtime_revocation_guard','immutable_marketplace_installation_custody','marketplace_installation_link_writer','immutable_marketplace_activation_custody','marketplace_activation_link_writer','marketplace_activation_runtime_guard','marketplace_publication');foreach($authority as $name=>$value){$expected=in_array($name,$allowed,true);if($value!==$expected)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 authority ceiling violated: '.$name.'.');}return true;}

/** Require the accepted onboarding permissions and exact authority ceiling. */
function pay_eco_001_require_onboarding_contract($patch){
 $permissions=pay_eco_001_onboarding_permissions();
 if(!isset($permissions['publisher_profile'],$permissions['pack_candidate'])||$permissions['publisher_profile']!=='SA_PAY_CTRY_TRUST'||$permissions['pack_candidate']!=='SA_PAY_CTRY_INSTALL')return pay_eco_001_fail_patch($patch,'PAY-ECO-001 onboarding permission contract mismatch.');
 $authority=pay_eco_001_authority_contract();
 foreach(array('publisher_profile_writer','pack_candidate_writer') as $name)if(!isset($authority[$name])||$authority[$name]!==true)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 onboarding writer authority missing: '.$name.'.');
 foreach(array('sandbox_execution','automatic_catalog_admission','pack_installation','company_activation','payroll_execution','payroll_posting','payment_authority','general_ledger_authority','statutory_filing_authority','external_network_delivery','permission_grant','publisher_self_verification','certification_self_claim','historical_payroll_rewrite','core_source_write') as $name)if(!array_key_exists($name,$authority)||$authority[$name]!==false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 successor authority ceiling violated: '.$name.'.');
 return true;
}
/** Require the selected onboarding writer implementation to match its boundary. */
function pay_eco_001_require_onboarding_writer_source($patch,$scope)
{
    global $path_to_root;
    $file=$path_to_root.'/hrm/includes/db/pay_eco_001_onboarding_db.inc';
    $raw=@file_get_contents($file);
    if(!is_string($raw)||$raw==='')return pay_eco_001_fail_patch($patch,'PAY-ECO-001 onboarding writer source missing.');
    $tokens=array(
        'publisher'=>array('function pay_eco_001_write_publisher_profile(','PAY_ECO_001_PUBLISHER_PROFILE_PERMISSION','pay.eco.publisher_profile.appended','publisher_profile_replay_conflict'),
        'candidate'=>array('function pay_eco_001_write_pack_candidate(','PAY_ECO_001_PACK_CANDIDATE_PERMISSION',"final_status='accepted'",'pack_candidate_publisher_lineage_mismatch','pay.eco.pack_candidate.appended',"'marketplace_publication'=>false")
    );
    if(!isset($tokens[$scope]))return pay_eco_001_fail_patch($patch,'PAY-ECO-001 onboarding writer scope invalid.');
    foreach($tokens[$scope] as $token)if(strpos($raw,$token)===false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 '.$scope.' writer source contract incomplete: '.$token.'.');
    if($scope==='publisher'){
        $writer_start=strpos($raw,'function pay_eco_001_write_publisher_profile(');
        $writer_end=strpos($raw,'function pay_eco_001_write_pack_candidate(',$writer_start);
        if($writer_start===false||$writer_end===false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 publisher writer source range missing.');
        $writer=substr($raw,$writer_start,$writer_end-$writer_start);
        foreach(array(
            "pay_eco_001_onboarding_exact_row('SELECT publisher_revocation_id",
            'pay_ctry_publisher_revocations WHERE publisher_id=',
            'LIMIT 2 FOR UPDATE',
            'if($revocation===false)break;',
            'if(is_array($revocation)){$error=\'publisher_profile_publisher_revoked\';break;}'
        ) as $token)if(strpos($writer,$token)===false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 publisher revocation writer source contract incomplete: '.$token.'.');

        $reader_start=strpos($raw,'function pay_eco_001_onboarding_exact_row(');
        $reader_end=strpos($raw,'function pay_eco_001_onboarding_now_iso(',$reader_start);
        if($reader_start===false||$reader_end===false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 bounded onboarding row reader source range missing.');
        $reader=substr($raw,$reader_start,$reader_end-$reader_start);
        foreach(array(
            'function pay_eco_001_onboarding_exact_row(',
            '$q=db_query($sql);',
            'if(!$q)return pay_eco_001_fail($error,\'onboarding_query_failed\');',
            'if($n>1)return pay_eco_001_fail($error,\'onboarding_state_ambiguous\');'
        ) as $token)if(strpos($reader,$token)===false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 publisher revocation row reader contract incomplete: '.$token.'.');
    }
    return true;
}

/** Require the bounded declarative sandbox/evaluation successor contract. */
function pay_eco_001_require_sandbox_contract($patch){
 $authority=pay_eco_001_authority_contract();
 foreach(array('immutable_sandbox_evaluation_custody','sandbox_evaluation_writer') as $name)if(!isset($authority[$name])||$authority[$name]!==true)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 sandbox authority missing: '.$name.'.');
 foreach(array('sandbox_execution','automatic_catalog_admission','pack_installation','company_activation','payroll_execution','payroll_posting','payment_authority','general_ledger_authority','statutory_filing_authority','external_network_delivery','permission_grant','publisher_self_verification','certification_self_claim','historical_payroll_rewrite','core_source_write') as $name)if(!array_key_exists($name,$authority)||$authority[$name]!==false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 sandbox successor authority ceiling violated: '.$name.'.');
 if(PAY_ECO_001_SANDBOX_EVALUATION_PERMISSION!=='SA_PAY_CTRY_TRUST')return pay_eco_001_fail_patch($patch,'PAY-ECO-001 sandbox evaluator permission mismatch.');
 $profile=pay_eco_001_sandbox_profile();foreach(array('declarative_only'=>true,'partner_executable_code'=>false,'network_access'=>false,'filesystem_write'=>false,'database_write'=>false,'process_spawn'=>false,'core_source_write'=>false,'production_data_access'=>false,'external_delivery'=>false) as $name=>$expected)if(!array_key_exists($name,$profile)||$profile[$name]!==$expected)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 sandbox profile boundary mismatch: '.$name.'.');
 return true;
}
/** Require the sandbox evaluator/writer source to preserve the declarative-only boundary. */
function pay_eco_001_require_sandbox_writer_source($patch){global $path_to_root;$file=$path_to_root.'/hrm/includes/db/pay_eco_001_sandbox_db.inc';$raw=@file_get_contents($file);if(!is_string($raw)||$raw==='')return pay_eco_001_fail_patch($patch,'PAY-ECO-001 sandbox evaluator source missing.');foreach(array('function pay_eco_001_write_sandbox_evaluation(','PAY_ECO_001_SANDBOX_EVALUATION_PERMISSION','pay_eco_001_sandbox_profile_sha256','pay_eco_001_sandbox_suite_sha256',"final_status='accepted'",'pay_eco_001_validate_pack_candidate','sandbox_candidate_integrity_invalid','PAY_CTRY_001_MAX_MEMBERS-1','PAY_CTRY_001_MAX_TOTAL_MEMBER_BYTES','sandbox_artifact_ordinal_invalid','sandbox_artifact_path_duplicate','sandbox_artifact_evidence_flag_invalid','artifact_inventory_sha256','sandbox_profile_mismatch','pay.eco.sandbox_evaluation.appended',"'sandbox_execution'=>false","'dependency_vulnerability_scanning'=>false","'marketplace_publication'=>false") as $token)if(strpos($raw,$token)===false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 sandbox evaluator source contract incomplete: '.$token.'.');return true;}


/** Require the bounded dependency-integrity scan successor contract without claiming vulnerability intelligence. */
function pay_eco_001_require_dependency_scan_contract($patch){
 $authority=pay_eco_001_authority_contract();
 foreach(array('immutable_dependency_scan_custody','dependency_integrity_scanner') as $name)if(!isset($authority[$name])||$authority[$name]!==true)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 dependency scan authority missing: '.$name.'.');
 foreach(array('automatic_catalog_admission','pack_installation','company_activation','payroll_execution','payroll_posting','payment_authority','general_ledger_authority','statutory_filing_authority','external_network_delivery','permission_grant','publisher_self_verification','certification_self_claim','historical_payroll_rewrite','core_source_write') as $name)if(!array_key_exists($name,$authority)||$authority[$name]!==false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 dependency scan successor authority ceiling violated: '.$name.'.');
 if(PAY_ECO_001_DEPENDENCY_SCAN_PERMISSION!=='SA_PAY_CTRY_TRUST')return pay_eco_001_fail_patch($patch,'PAY-ECO-001 dependency scan permission mismatch.');
 $profile=pay_eco_001_dependency_scan_profile();foreach(array('accepted_verification_required'=>true,'passed_sandbox_required'=>true,'dependency_lock_required'=>true,'resolved_dependency_verification_required'=>true,'resolved_dependency_revocation_check'=>true,'network_advisory_lookup'=>false,'publisher_vulnerability_claim_authoritative'=>false,'vulnerability_intelligence_configured'=>false,'artifact_admission_authority'=>false) as $name=>$expected)if(!array_key_exists($name,$profile)||$profile[$name]!==$expected)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 dependency scan profile boundary mismatch: '.$name.'.');
 return true;
}
function pay_eco_001_require_dependency_scan_writer_source($patch){global $path_to_root;$file=$path_to_root.'/hrm/includes/db/pay_eco_001_dependency_scan_db.inc';$raw=@file_get_contents($file);if(!is_string($raw)||$raw==='')return pay_eco_001_fail_patch($patch,'PAY-ECO-001 dependency scan writer source missing.');foreach(array('function pay_eco_001_write_dependency_scan(','PAY_ECO_001_DEPENDENCY_SCAN_PERMISSION','pay_eco_001_dependency_scan_profile_sha256','pay_eco_001_dependency_scan_suite_sha256',"final_status='accepted'",'pay_eco_001_sandbox_evaluations','pay_ctry_dependency_locks','pay_ctry_dependency_lock_members','resolved_dependency_verification_missing','resolved_dependency_publisher_revoked','vulnerability_intelligence_unconfigured','pay.eco.dependency_scan.appended',"'dependency_vulnerability_scanning'=>false","'signed_reproducible_artifact_admission'=>false") as $token)if(strpos($raw,$token)===false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 dependency scan writer source contract incomplete: '.$token.'.');return true;}


/** Require offline authoritative vulnerability-intelligence ingestion/correlation while later admission/runtime authority stays denied. */
function pay_eco_001_require_vulnerability_intelligence_contract($patch){
 $authority=pay_eco_001_authority_contract();
 foreach(array('immutable_vulnerability_snapshot_custody','immutable_vulnerability_advisory_custody','immutable_vulnerability_correlation_custody','vulnerability_intelligence_ingest_writer','dependency_vulnerability_scanning') as $name)if(!isset($authority[$name])||$authority[$name]!==true)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 vulnerability intelligence authority missing: '.$name.'.');
 foreach(array('automatic_catalog_admission','pack_installation','company_activation','payroll_execution','payroll_posting','payment_authority','general_ledger_authority','statutory_filing_authority','external_network_delivery','permission_grant','publisher_self_verification','certification_self_claim','historical_payroll_rewrite','core_source_write') as $name)if(!array_key_exists($name,$authority)||$authority[$name]!==false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 vulnerability successor authority ceiling violated: '.$name.'.');
 if(PAY_ECO_001_VULNERABILITY_INTELLIGENCE_PERMISSION!=='SA_PAY_CTRY_TRUST')return pay_eco_001_fail_patch($patch,'PAY-ECO-001 vulnerability intelligence permission mismatch.');
 $profile=pay_eco_001_vulnerability_intelligence_profile();foreach(array('trusted_operator_permission'=>'SA_PAY_CTRY_TRUST','offline_snapshot_ingestion'=>true,'network_advisory_lookup'=>false,'publisher_claim_authoritative'=>false,'exact_package_version_correlation'=>true,'max_advisories'=>256,'snapshot_expiry_required'=>true,'artifact_admission_authority'=>false,'publication_authority'=>false,'installation_authority'=>false) as $name=>$expected)if(!array_key_exists($name,$profile)||$profile[$name]!==$expected)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 vulnerability profile boundary mismatch: '.$name.'.');
 if(!pay_eco_001_sha($profile['accepted_source_authority_sha256']))return pay_eco_001_fail_patch($patch,'PAY-ECO-001 vulnerability source authority digest invalid.');
 return true;
}
function pay_eco_001_require_vulnerability_writer_source($patch){global $path_to_root;$file=$path_to_root.'/hrm/includes/db/pay_eco_001_vulnerability_db.inc';$raw=@file_get_contents($file);if(!is_string($raw)||$raw==='')return pay_eco_001_fail_patch($patch,'PAY-ECO-001 vulnerability writer source missing.');foreach(array('function pay_eco_001_write_vulnerability_snapshot(','function pay_eco_001_write_vulnerability_correlation(','PAY_ECO_001_VULNERABILITY_INTELLIGENCE_PERMISSION','pay_eco_001_vulnerability_intelligence_profile_sha256','accepted_source_authority_sha256','pay_eco_001_vulnerability_snapshots','pay_eco_001_vulnerability_advisories','pay_eco_001_vulnerability_correlations','snapshot_expired','exact_package_version_correlation','pay.eco.vulnerability_snapshot.appended','pay.eco.vulnerability_correlation.appended',"'network_advisory_lookup'=>false","'signed_reproducible_artifact_admission'=>false","'marketplace_publication'=>false") as $token)if(strpos($raw,$token)===false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 vulnerability writer source contract incomplete: '.$token.'.');return true;}


/** Require signed/reproducible artifact admission while catalog/publication/install/runtime authority stays denied. */
function pay_eco_001_require_artifact_admission_contract($patch){
 $authority=pay_eco_001_authority_contract();
 foreach(array('immutable_artifact_admission_custody','signed_reproducible_artifact_admission') as $name)if(!isset($authority[$name])||$authority[$name]!==true)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 artifact admission authority missing: '.$name.'.');
 foreach(array('automatic_catalog_admission','pack_installation','company_activation','payroll_execution','payroll_posting','payment_authority','general_ledger_authority','statutory_filing_authority','external_network_delivery','permission_grant','publisher_self_verification','certification_self_claim','historical_payroll_rewrite','core_source_write') as $name)if(!array_key_exists($name,$authority)||$authority[$name]!==false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 artifact admission successor authority ceiling violated: '.$name.'.');
 if(PAY_ECO_001_ARTIFACT_ADMISSION_PERMISSION!=='SA_PAY_CTRY_TRUST')return pay_eco_001_fail_patch($patch,'PAY-ECO-001 artifact admission permission mismatch.');
 $profile=pay_eco_001_artifact_admission_profile();foreach(array('trusted_operator_permission'=>'SA_PAY_CTRY_TRUST','accepted_verification_required'=>true,'signature_lineage_required'=>true,'passed_sandbox_required'=>true,'dependency_integrity_required'=>true,'passed_authoritative_vulnerability_correlation_required'=>true,'current_nonexpired_vulnerability_snapshot_required'=>true,'exact_manifest_roots_required'=>true,'exact_artifact_inventory_required'=>true,'reproducibility_digest_required'=>true,'effective_publisher_required'=>true,'publisher_not_revoked_required'=>true,'catalog_publication_authority'=>false,'installation_authority'=>false,'company_activation_authority'=>false,'network_authority'=>false) as $name=>$expected)if(!array_key_exists($name,$profile)||$profile[$name]!==$expected)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 artifact admission profile boundary mismatch: '.$name.'.');
 return true;
}
function pay_eco_001_require_artifact_admission_writer_source($patch){global $path_to_root;$file=$path_to_root.'/hrm/includes/db/pay_eco_001_artifact_admission_db.inc';$raw=@file_get_contents($file);if(!is_string($raw)||$raw==='')return pay_eco_001_fail_patch($patch,'PAY-ECO-001 artifact admission writer source missing.');foreach(array('function pay_eco_001_write_artifact_admission(','PAY_ECO_001_ARTIFACT_ADMISSION_PERMISSION',"final_status='accepted'",'pay_eco_001_sandbox_artifact_inventory','dependency_result','artifact_admission_stale_vulnerability_correlation','pay_ctry_trust_keys','pay_ctry_trust_key_revocations','reproducibility_sha256','signature_attestation_sha256','trust_snapshot_sha256','pay.eco.artifact_admission.appended',"'signed_reproducible_artifact_admission'=>true","'marketplace_publication'=>false","'pack_installation'=>false","'company_activation'=>false") as $token)if(strpos($raw,$token)===false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 artifact admission writer source contract incomplete: '.$token.'.');return true;}




/** Require bounded catalog-decision governance while publication/install/runtime authority stays denied. */
function pay_eco_001_require_catalog_decision_contract($patch){
 $authority=pay_eco_001_authority_contract();
 foreach(array('immutable_catalog_decision_custody','catalog_decision_writer') as $name)if(!isset($authority[$name])||$authority[$name]!==true)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 catalog decision authority missing: '.$name.'.');
 foreach(array('automatic_catalog_admission','pack_installation','company_activation','payroll_execution','payroll_posting','payment_authority','general_ledger_authority','statutory_filing_authority','external_network_delivery','permission_grant','publisher_self_verification','certification_self_claim','historical_payroll_rewrite','core_source_write') as $name)if(!array_key_exists($name,$authority)||$authority[$name]!==false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 catalog decision authority ceiling violated: '.$name.'.');
 if(PAY_ECO_001_CATALOG_DECISION_PERMISSION!=='SA_PAY_CTRY_TRUST')return pay_eco_001_fail_patch($patch,'PAY-ECO-001 catalog decision permission mismatch.');
 $profile=pay_eco_001_catalog_decision_profile();foreach(array('trusted_checker_permission'=>'SA_PAY_CTRY_TRUST','artifact_admission_required'=>true,'maker_from_candidate_submitter'=>true,'checker_is_current_actor'=>true,'checker_distinct_from_maker'=>true,'current_nonexpired_vulnerability_snapshot_required'=>true,'effective_nonrevoked_publisher_required'=>true,'admitted_trust_snapshot_revalidation_required'=>true,'signing_key_not_revoked_required'=>true,'single_terminal_decision_per_candidate'=>true,'publication_authority'=>false,'installation_authority'=>false,'company_activation_authority'=>false,'network_authority'=>false) as $name=>$expected)if(!array_key_exists($name,$profile)||$profile[$name]!==$expected)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 catalog decision profile boundary mismatch: '.$name.'.');
 return true;
}
function pay_eco_001_require_catalog_decision_writer_source($patch){global $path_to_root;$file=$path_to_root.'/hrm/includes/db/pay_eco_001_catalog_decision_db.inc';$raw=@file_get_contents($file);if(!is_string($raw)||$raw==='')return pay_eco_001_fail_patch($patch,'PAY-ECO-001 catalog decision writer source missing.');foreach(array('function pay_eco_001_write_catalog_decision(','PAY_ECO_001_CATALOG_DECISION_PERMISSION','pay_eco_001_catalog_decisions','pay_eco_001_artifact_admissions','pay_eco_001_vulnerability_correlations','pay_eco_001_vulnerability_snapshots','pay_ctry_trust_keys','pay_ctry_trust_key_revocations','catalog_decision_separation_required','catalog_decision_artifact_admission_required','catalog_decision_vulnerability_snapshot_not_current','catalog_decision_stale_vulnerability_correlation','catalog_decision_trust_snapshot_drift','catalog_decision_signing_key_revoked','pay.eco.catalog_decision.appended',"'marketplace_publication'=>false","'pack_installation'=>false","'company_activation'=>false") as $token)if(strpos($raw,$token)===false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 catalog decision writer source contract incomplete: '.$token.'.');return true;}


/** Require bounded explicit catalog publication plus read-only runtime metadata. */
function pay_eco_001_require_catalog_publication_contract($patch){
 $authority=pay_eco_001_authority_contract();
 foreach(array('immutable_catalog_publication_custody','catalog_publication_writer','catalog_runtime_read','immutable_catalog_lifecycle_custody','catalog_lifecycle_propagation_writer','catalog_runtime_revocation_guard','immutable_marketplace_installation_custody','marketplace_installation_link_writer','immutable_marketplace_activation_custody','marketplace_activation_link_writer','marketplace_activation_runtime_guard','marketplace_publication') as $name)if(!isset($authority[$name])||$authority[$name]!==true)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 catalog publication authority missing: '.$name.'.');
 foreach(array('automatic_catalog_admission','pack_installation','company_activation','payroll_execution','payroll_posting','payment_authority','general_ledger_authority','statutory_filing_authority','external_network_delivery','permission_grant','publisher_self_verification','certification_self_claim','historical_payroll_rewrite','core_source_write') as $name)if(!array_key_exists($name,$authority)||$authority[$name]!==false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 catalog publication authority ceiling violated: '.$name.'.');
 if(PAY_ECO_001_CATALOG_PUBLICATION_PERMISSION!=='SA_PAY_CTRY_TRUST'||PAY_ECO_001_CATALOG_RUNTIME_PERMISSION!=='SA_PAY_CTRY_INSTALL')return pay_eco_001_fail_patch($patch,'PAY-ECO-001 catalog publication/runtime permission mismatch.');
 $profile=pay_eco_001_catalog_publication_profile();foreach(array('trusted_publisher_permission'=>'SA_PAY_CTRY_TRUST','runtime_read_permission'=>'SA_PAY_CTRY_INSTALL','admit_for_review_decision_required'=>true,'accepted_artifact_admission_required'=>true,'current_nonexpired_vulnerability_snapshot_required'=>true,'effective_nonrevoked_pack_publisher_required'=>true,'signing_key_not_revoked_required'=>true,'current_certified_publication_required'=>true,'signed_catalog_release_required'=>true,'accepted_catalog_release_admission_required'=>true,'catalog_signature_reverification_required'=>true,'single_candidate_catalog_release'=>true,'publisher_distinct_from_candidate_maker'=>true,'immutable_publication_custody'=>true,'runtime_read_only'=>true,'automatic_catalog_admission'=>false,'installation_authority'=>false,'company_activation_authority'=>false,'network_authority'=>false,'payroll_authority'=>false,'historical_rewrite_authority'=>false) as $name=>$expected)if(!array_key_exists($name,$profile)||$profile[$name]!==$expected)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 catalog publication profile boundary mismatch: '.$name.'.');
 return true;
}
function pay_eco_001_require_catalog_publication_writer_source($patch){global $path_to_root;$file=$path_to_root.'/hrm/includes/db/pay_eco_001_catalog_publication_db.inc';$raw=@file_get_contents($file);if(!is_string($raw)||$raw==='')return pay_eco_001_fail_patch($patch,'PAY-ECO-001 catalog publication writer source missing.');foreach(array('function pay_eco_001_write_catalog_publication(','function pay_eco_001_get_catalog_publication(','PAY_ECO_001_CATALOG_PUBLICATION_PERMISSION','PAY_ECO_001_CATALOG_RUNTIME_PERMISSION','pay_eco_001_catalog_publication_profile_sha256','pay_eco_001_catalog_publication_entry_sha256','pay_eco_001_catalog_publications','pay_ctry_publication_records','pay_ctry_catalog_releases','pay_ctry_catalog_release_admissions','pay_ctry_007_verify_catalog_signature','catalog_publication_decision_not_admitted','catalog_publication_catalog_root_mismatch','catalog_publication_certification_not_current','catalog_publication_signing_authority_revoked','catalog_publication_replay_conflict','pay.eco.catalog_publication.appended',"'marketplace_publication'=>true","'automatic_catalog_admission'=>false","'pack_installation'=>false","'company_activation'=>false","'external_network_delivery'=>false","'historical_payroll_accounting_rows_rewritten'=>0") as $token)if(strpos($raw,$token)===false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 catalog publication writer source contract incomplete: '.$token.'.');return true;}


/** Require bounded propagation of existing PAY-CTRY suspension/revocation authority. */
function pay_eco_001_require_catalog_lifecycle_contract($patch){
 $authority=pay_eco_001_authority_contract();
 foreach(array('immutable_catalog_lifecycle_custody','catalog_lifecycle_propagation_writer','catalog_runtime_revocation_guard') as $name)if(!isset($authority[$name])||$authority[$name]!==true)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 catalog lifecycle authority missing: '.$name.'.');
 foreach(array('automatic_catalog_admission','pack_installation','company_activation','payroll_execution','payroll_posting','payment_authority','general_ledger_authority','statutory_filing_authority','external_network_delivery','permission_grant','historical_payroll_rewrite','core_source_write') as $name)if(!array_key_exists($name,$authority)||$authority[$name]!==false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 catalog lifecycle authority ceiling violated: '.$name.'.');
 if(PAY_ECO_001_CATALOG_LIFECYCLE_PERMISSION!=='SA_PAY_CTRY_TRUST')return pay_eco_001_fail_patch($patch,'PAY-ECO-001 catalog lifecycle permission mismatch.');
 $profile=pay_eco_001_catalog_lifecycle_profile();foreach(array('server_derived_source_only'=>true,'immutable_propagation_custody'=>true,'runtime_denies_before_propagation_record'=>true,'suspension_fails_closed'=>true,'revocation_fails_closed'=>true,'historical_publication_preserved'=>true,'publication_evidence_rewrite'=>false,'installation_authority'=>false,'company_activation_authority'=>false,'external_network_authority'=>false,'payroll_authority'=>false,'historical_rewrite_authority'=>false) as $name=>$expected)if(!array_key_exists($name,$profile)||$profile[$name]!==$expected)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 catalog lifecycle profile boundary mismatch: '.$name.'.');
 if(!isset($profile['authoritative_sources'])||$profile['authoritative_sources']!==array('certification_emergency_action','publisher_revocation','trust_key_revocation'))return pay_eco_001_fail_patch($patch,'PAY-ECO-001 catalog lifecycle authoritative source set mismatch.');return true;
}
function pay_eco_001_require_catalog_lifecycle_writer_source($patch){global $path_to_root;$file=$path_to_root.'/hrm/includes/db/pay_eco_001_catalog_lifecycle_db.inc';$raw=@file_get_contents($file);if(!is_string($raw)||$raw==='')return pay_eco_001_fail_patch($patch,'PAY-ECO-001 catalog lifecycle propagation source missing.');foreach(array('function pay_eco_001_propagate_catalog_lifecycle(','function pay_eco_001_catalog_lifecycle_guard(','PAY_ECO_001_CATALOG_LIFECYCLE_PERMISSION','pay_ctry_certification_emergency_actions','pay_ctry_publisher_revocations','pay_ctry_trust_key_revocations','runtime_denies_before_propagation_record','pay.eco.catalog_lifecycle.propagated',"'pack_installation'=>false","'company_activation'=>false","'historical_payroll_accounting_rows_rewritten'=>0") as $token)if(strpos($raw,$token)===false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 catalog lifecycle propagation source contract incomplete: '.$token.'.');return true;}


/** Require the marketplace bridge to consume existing PAY-CTRY-002 authority without becoming an installer/activation engine. */
function pay_eco_001_require_marketplace_contract($patch){
 $authority=pay_eco_001_authority_contract();
 foreach(array('immutable_marketplace_installation_custody','marketplace_installation_link_writer','immutable_marketplace_activation_custody','marketplace_activation_link_writer','marketplace_activation_runtime_guard') as $name)if(!isset($authority[$name])||$authority[$name]!==true)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 marketplace bridge authority missing: '.$name.'.');
 foreach(array('automatic_catalog_admission','pack_installation','company_activation','payroll_execution','payroll_posting','payment_authority','general_ledger_authority','statutory_filing_authority','external_network_delivery','permission_grant','historical_payroll_rewrite','core_source_write') as $name)if(!array_key_exists($name,$authority)||$authority[$name]!==false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 marketplace direct-authority ceiling violated: '.$name.'.');
 if(PAY_ECO_001_MARKETPLACE_INSTALL_PERMISSION!=='SA_PAY_CTRY_INSTALL'||PAY_ECO_001_MARKETPLACE_ACTIVATION_PERMISSION!=='SA_HRM_EXECUTE_PAY_CTRY_ACTIVATION')return pay_eco_001_fail_patch($patch,'PAY-ECO-001 marketplace permission binding mismatch.');
 $profile=pay_eco_001_marketplace_profile();foreach(array('site_install_permission'=>'SA_PAY_CTRY_INSTALL','company_activation_permission'=>'SA_HRM_EXECUTE_PAY_CTRY_ACTIVATION','published_catalog_required'=>true,'catalog_lifecycle_guard_required'=>true,'site_installation_pack_match_required'=>true,'exact_semantic_root_required'=>true,'exact_evidence_root_required'=>true,'pay_ctry_002_activation_binding_required'=>true,'pay_ctry_002_runtime_resolution_required'=>true,'installation_implies_activation'=>false,'automatic_installation'=>false,'automatic_activation'=>false,'direct_installation_authority'=>false,'direct_company_activation_authority'=>false,'new_permission_grant'=>false,'external_network_authority'=>false,'payroll_authority'=>false,'historical_rewrite_authority'=>false) as $name=>$expected)if(!array_key_exists($name,$profile)||$profile[$name]!==$expected)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 marketplace profile boundary mismatch: '.$name.'.');
 return true;
}
function pay_eco_001_require_marketplace_writer_source($patch){global $path_to_root;$file=$path_to_root.'/hrm/includes/db/pay_eco_001_marketplace_activation_db.inc';$raw=@file_get_contents($file);if(!is_string($raw)||$raw==='')return pay_eco_001_fail_patch($patch,'PAY-ECO-001 marketplace bridge source missing.');foreach(array('function pay_eco_001_record_marketplace_installation(','function pay_eco_001_record_marketplace_company_activation(','PAY_ECO_001_MARKETPLACE_INSTALL_PERMISSION','PAY_ECO_001_MARKETPLACE_ACTIVATION_PERMISSION','pay_eco_001_catalog_lifecycle_guard','pay_ctry_002_runtime_resolution_for_update','pay_eco_001_marketplace_installations','pay_eco_001_marketplace_activations','pay_ctry_site_installations','pay_ctry_activations','pay_ctry_activation_bindings','marketplace_installation_pack_mismatch','marketplace_activation_runtime_binding_not_current','pay.eco.marketplace_installation.linked','pay.eco.marketplace_activation.linked',"'direct_installation_authority'=>false","'direct_company_activation_authority'=>false","'historical_payroll_accounting_rows_rewritten'=>0") as $token)if(strpos($raw,$token)===false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 marketplace bridge source contract incomplete: '.$token.'.');foreach(array('exec(','shell_exec','system(','passthru(','proc_open','curl_','file_put_contents','fopen(','DROP TABLE','TRUNCATE') as $token)if(stripos($raw,$token)!==false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 marketplace bridge unsafe primitive present: '.$token.'.');return true;}


/** Final closure edge predicates; these are source-only acceptance markers. */
function pay_eco_001_ecosystem_lifecycle_acceptance_required_for_database_version($version){return version_compare((string)$version,'1.0.1014','>=');}
function pay_eco_001_migration_recovery_acceptance_required_for_database_version($version){return version_compare((string)$version,'1.0.1015','>=');}
function pay_eco_001_authority_isolation_completion_required_for_database_version($version){return version_compare((string)$version,'1.0.1016','>=');}

/** Bind all accepted PAY-ECO governance capabilities to the final default-deny lifecycle contract. */
function pay_eco_001_require_ecosystem_lifecycle_acceptance_source($patch){
 if(!pay_eco_001_require_onboarding_contract($patch)||!pay_eco_001_require_onboarding_writer_source($patch,'publisher')||!pay_eco_001_require_onboarding_writer_source($patch,'candidate')
  ||!pay_eco_001_require_sandbox_contract($patch)||!pay_eco_001_require_sandbox_writer_source($patch)
  ||!pay_eco_001_require_dependency_scan_contract($patch)||!pay_eco_001_require_dependency_scan_writer_source($patch)
  ||!pay_eco_001_require_vulnerability_intelligence_contract($patch)||!pay_eco_001_require_vulnerability_writer_source($patch)
  ||!pay_eco_001_require_artifact_admission_contract($patch)||!pay_eco_001_require_artifact_admission_writer_source($patch)
  ||!pay_eco_001_require_catalog_decision_contract($patch)||!pay_eco_001_require_catalog_decision_writer_source($patch)
  ||!pay_eco_001_require_catalog_publication_contract($patch)||!pay_eco_001_require_catalog_publication_writer_source($patch)
  ||!pay_eco_001_require_catalog_lifecycle_contract($patch)||!pay_eco_001_require_catalog_lifecycle_writer_source($patch)
  ||!pay_eco_001_require_marketplace_contract($patch)||!pay_eco_001_require_marketplace_writer_source($patch))return false;
 $c=pay_eco_001_completion_contract();
 foreach(array('ecosystem_lifecycle_acceptance_required','publisher_verification_required','signed_reproducible_artifacts_required','dependency_scanning_required','offline_vulnerability_intelligence_required','suspension_revocation_fail_closed_required','existing_pay_ctry_install_activation_authority_required','default_deny_required') as $key)
  if(!isset($c[$key])||$c[$key]!==true)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 lifecycle closure contract incomplete: '.$key.'.');
 foreach(array('automatic_catalog_admission_allowed','direct_pack_installation_authority_allowed','direct_company_activation_authority_allowed','external_network_delivery_allowed','permission_grant_allowed','payroll_execution_authority_allowed','payroll_posting_authority_allowed','payment_authority_allowed','general_ledger_authority_allowed','statutory_filing_authority_allowed','historical_payroll_accounting_mutation_allowed','selective_destructive_downgrade_allowed') as $key)
  if(!array_key_exists($key,$c)||$c[$key]!==false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 lifecycle closure default-deny boundary incomplete: '.$key.'.');
 return true;
}

/** Bind the complete reviewed migration chain and source-only closure recovery boundary. */
function pay_eco_001_require_migration_recovery_acceptance_source($patch){global $path_to_root;
 $edges=array(
  array('pay_eco_001_dependency_review','1.0.984','1.0.985'),
  array('pay_eco_001_schema_foundation','1.0.986','1.0.987'),
  array('pay_eco_001_sandbox_evaluator_writer','1.0.992','1.0.993'),
  array('pay_eco_001_dependency_scan_writer','1.0.995','1.0.996'),
  array('pay_eco_001_vulnerability_writer','1.0.998','1.0.999'),
  array('pay_eco_001_artifact_admission_writer','1.0.1001','1.0.1002'),
  array('pay_eco_001_catalog_decision_writer','1.0.1003','1.0.1004'),
  array('pay_eco_001_catalog_publication_runtime','1.0.1006','1.0.1007'),
  array('pay_eco_001_catalog_lifecycle_runtime','1.0.1009','1.0.1010'),
  array('pay_eco_001_marketplace_runtime','1.0.1012','1.0.1013'),
  array('pay_eco_001_ecosystem_lifecycle_acceptance','1.0.1013','1.0.1014'),
  array('pay_eco_001_migration_recovery_acceptance','1.0.1014','1.0.1015'));
 foreach($edges as $edge)if(!hrm_migration_registered_edge_matches($edge[0],$edge[1],$edge[2]))return hrm_migration_fail($patch,HRM_MIGRATION_PREDECESSOR_MIGRATION_ABSENT,'PAY-ECO-001 closure migration registry chain is incomplete: '.$edge[0].'.');
 foreach(array('sql/upgrade_pay_eco_001_ecosystem_lifecycle_acceptance.php','sql/upgrade_pay_eco_001_migration_recovery_acceptance.php','sql/upgrade_pay_eco_001_authority_isolation_completion.php') as $relative){$raw=@file_get_contents($path_to_root.'/'.$relative);if(!is_string($raw)||strpos($raw,'public $sql = \'\';')===false||strpos($raw,'Selective PAY-ECO-001 downgrade denied')===false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 source-only closure/recovery contract incomplete: '.$relative.'.');}
 return true;
}

/** Final authority ceiling: PAY-ECO remains governance/linkage only and cannot mutate payroll/accounting or deliver arbitrary code/network traffic. */
function pay_eco_001_require_authority_isolation_completion_source($patch){global $path_to_root;
 $authority=pay_eco_001_authority_contract();
 foreach(array('automatic_catalog_admission','pack_installation','company_activation','payroll_execution','payroll_posting','payment_authority','general_ledger_authority','statutory_filing_authority','external_network_delivery','permission_grant','publisher_self_verification','certification_self_claim','historical_payroll_rewrite','core_source_write') as $key)
  if(!array_key_exists($key,$authority)||$authority[$key]!==false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 final authority ceiling violated: '.$key.'.');
 $completion=pay_eco_001_completion_contract();if(($completion['authority_payroll_isolation_acceptance_required']??false)!==true||($completion['migration_schema_recovery_acceptance_required']??false)!==true)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 final completion acceptance contract incomplete.');
 $files=array('hrm/includes/db/pay_eco_001_onboarding_db.inc','hrm/includes/db/pay_eco_001_sandbox_db.inc','hrm/includes/db/pay_eco_001_dependency_scan_db.inc','hrm/includes/db/pay_eco_001_vulnerability_db.inc','hrm/includes/db/pay_eco_001_artifact_admission_db.inc','hrm/includes/db/pay_eco_001_catalog_decision_db.inc','hrm/includes/db/pay_eco_001_catalog_publication_db.inc','hrm/includes/db/pay_eco_001_catalog_lifecycle_db.inc','hrm/includes/db/pay_eco_001_marketplace_activation_db.inc');
 $denied=array('calculate_employee_payslip(','calculate_employee_payslip_read_only(','write_payroll_results(','add_gl_trans(','pay_core_010_','curl_exec(','shell_exec(','proc_open(','passthru(');
 foreach($files as $relative){$raw=@file_get_contents($path_to_root.'/'.$relative);if(!is_string($raw))return pay_eco_001_fail_patch($patch,'PAY-ECO-001 final authority source missing: '.$relative.'.');foreach($denied as $token)if(strpos($raw,$token)!==false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 final authority isolation breach in '.$relative.': '.$token);}
 $access=@file_get_contents($path_to_root.'/includes/access_levels.inc');if(!is_string($access)||stripos($access,'PAY_ECO')!==false||stripos($access,'PAY-ECO')!==false)return pay_eco_001_fail_patch($patch,'PAY-ECO-001 final closure must not introduce an access-level grant.');
 return true;
}

?>
