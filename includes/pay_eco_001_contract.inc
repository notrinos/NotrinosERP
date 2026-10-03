<?php
/** PAY-ECO-001 third-party pack ecosystem governance foundation contract. */
include_once(dirname(__FILE__).'/pay_ctry_001_verifier.inc');

if (!defined('PAY_ECO_001_CONTRACT_VERSION')) define('PAY_ECO_001_CONTRACT_VERSION','1.0.0');
if (!defined('PAY_ECO_001_SOURCE_DATABASE_VERSION')) define('PAY_ECO_001_SOURCE_DATABASE_VERSION','1.0.999');
if (!defined('PAY_ECO_001_LEGACY_PAYROLL_CONTRACT_VERSION')) define('PAY_ECO_001_LEGACY_PAYROLL_CONTRACT_VERSION','1.0.517');
if (!defined('PAY_ECO_001_PUBLISHER_PROFILE_PERMISSION')) define('PAY_ECO_001_PUBLISHER_PROFILE_PERMISSION','SA_PAY_CTRY_TRUST');
if (!defined('PAY_ECO_001_PACK_CANDIDATE_PERMISSION')) define('PAY_ECO_001_PACK_CANDIDATE_PERMISSION','SA_PAY_CTRY_INSTALL');
if (!defined('PAY_ECO_001_SANDBOX_EVALUATION_PERMISSION')) define('PAY_ECO_001_SANDBOX_EVALUATION_PERMISSION','SA_PAY_CTRY_TRUST');
if (!defined('PAY_ECO_001_DEPENDENCY_SCAN_PERMISSION')) define('PAY_ECO_001_DEPENDENCY_SCAN_PERMISSION','SA_PAY_CTRY_TRUST');
if (!defined('PAY_ECO_001_VULNERABILITY_INTELLIGENCE_PERMISSION')) define('PAY_ECO_001_VULNERABILITY_INTELLIGENCE_PERMISSION','SA_PAY_CTRY_TRUST');

function pay_eco_001_fail(&$error,$code){$error=$code;return false;}
function pay_eco_001_canonical_json($value){return pay_ctry_001_canonical_json($value);}
function pay_eco_001_sha($value){return pay_ctry_001_sha($value);}
function pay_eco_001_hash($value){return hash('sha256',pay_eco_001_canonical_json($value));}
/** Return the publisher validity state at a server-derived scan time. */
function pay_eco_001_dependency_publisher_validity_state($valid_from,$valid_to,$now_epoch){
 if(!is_string($valid_from)||$valid_from===''||($valid_to!==null&&!is_string($valid_to))||!is_numeric($now_epoch)||(int)$now_epoch<=0)return 'invalid';
 $from=strtotime($valid_from.' UTC');$to=$valid_to===null?null:strtotime($valid_to.' UTC');$now=(int)$now_epoch;
 if($from===false||($valid_to!==null&&$to===false))return 'invalid';
 return $from<=$now&&($to===null||$to>=$now)?'effective':'not_effective';
}
function pay_eco_001_time($value){return pay_ctry_001_iso_utc_epoch($value);}
function pay_eco_001_token($value,$max=96){return is_string($value)&&strlen($value)>0&&strlen($value)<=$max&&preg_match('/^[A-Za-z0-9][A-Za-z0-9._:-]*$/D',$value)===1;}
function pay_eco_001_exact_fields($value,$fields){if(!is_array($value)||count($value)!==count($fields))return false;foreach($fields as $field)if(!array_key_exists($field,$value))return false;return true;}
function pay_eco_001_positive_id($value){return (is_int($value)||ctype_digit((string)$value))&&(int)$value>0;}

/** Foundation authority is append-only custody only; marketplace/runtime authority remains denied. */
function pay_eco_001_authority_contract(){return array(
 'immutable_publisher_profile_custody'=>true,
 'immutable_pack_candidate_custody'=>true,
 'immutable_catalog_decision_custody'=>true,
 'immutable_sandbox_evaluation_custody'=>true,
 'publisher_profile_writer'=>true,
 'pack_candidate_writer'=>true,
 'sandbox_evaluation_writer'=>true,
 'immutable_dependency_scan_custody'=>true,
 'dependency_integrity_scanner'=>true,
 'immutable_vulnerability_snapshot_custody'=>true,
 'immutable_vulnerability_advisory_custody'=>true,
 'immutable_vulnerability_correlation_custody'=>true,
 'vulnerability_intelligence_ingest_writer'=>true,
 'dependency_vulnerability_scanning'=>true,
 'catalog_decision_writer'=>false,
 'sandbox_execution'=>false,
 'signed_reproducible_artifact_admission'=>false,
 'marketplace_publication'=>false,
 'automatic_catalog_admission'=>false,
 'pack_installation'=>false,
 'company_activation'=>false,
 'payroll_execution'=>false,
 'payroll_posting'=>false,
 'payment_authority'=>false,
 'general_ledger_authority'=>false,
 'statutory_filing_authority'=>false,
 'external_network_delivery'=>false,
 'permission_grant'=>false,
 'publisher_self_verification'=>false,
 'certification_self_claim'=>false,
 'historical_payroll_rewrite'=>false,
 'core_source_write'=>false
);}

function pay_eco_001_publisher_profile_payload(array $profile){return array(
 'publisher_id'=>(int)$profile['publisher_id'],
 'profile_code'=>(string)$profile['profile_code'],
 'identity_evidence_sha256'=>(string)$profile['identity_evidence_sha256'],
 'support_policy_sha256'=>(string)$profile['support_policy_sha256'],
 'security_contact_sha256'=>(string)$profile['security_contact_sha256'],
 'terms_sha256'=>(string)$profile['terms_sha256'],
 'requested_by'=>(int)$profile['requested_by'],
 'requested_at'=>(string)$profile['requested_at']
);}
function pay_eco_001_publisher_profile_sha256(array $profile){return pay_eco_001_hash(pay_eco_001_publisher_profile_payload($profile));}
function pay_eco_001_validate_publisher_profile($profile,&$error){$error=null;$fields=array('publisher_id','profile_code','identity_evidence_sha256','support_policy_sha256','security_contact_sha256','terms_sha256','requested_by','requested_at','profile_sha256');if(!pay_eco_001_exact_fields($profile,$fields))return pay_eco_001_fail($error,'publisher_profile_fields_invalid');if(!pay_eco_001_positive_id($profile['publisher_id'])||!pay_eco_001_positive_id($profile['requested_by'])||!pay_eco_001_token($profile['profile_code']))return pay_eco_001_fail($error,'publisher_profile_identity_invalid');foreach(array('identity_evidence_sha256','support_policy_sha256','security_contact_sha256','terms_sha256','profile_sha256') as $field)if(!pay_eco_001_sha($profile[$field]))return pay_eco_001_fail($error,'publisher_profile_hash_invalid');if(pay_eco_001_time($profile['requested_at'])===false)return pay_eco_001_fail($error,'publisher_profile_time_invalid');if(!hash_equals(pay_eco_001_publisher_profile_sha256($profile),(string)$profile['profile_sha256']))return pay_eco_001_fail($error,'publisher_profile_digest_mismatch');return true;}

function pay_eco_001_pack_candidate_payload(array $candidate){return array(
 'publisher_profile_id'=>(int)$candidate['publisher_profile_id'],
 'pack_version_id'=>(int)$candidate['pack_version_id'],
 'candidate_code'=>(string)$candidate['candidate_code'],
 'compatibility_sha256'=>(string)$candidate['compatibility_sha256'],
 'certification_sha256'=>(string)$candidate['certification_sha256'],
 'support_sha256'=>(string)$candidate['support_sha256'],
 'vulnerability_sha256'=>(string)$candidate['vulnerability_sha256'],
 'revocation_state_sha256'=>(string)$candidate['revocation_state_sha256'],
 'sandbox_profile_sha256'=>(string)$candidate['sandbox_profile_sha256'],
 'submitted_by'=>(int)$candidate['submitted_by'],
 'submitted_at'=>(string)$candidate['submitted_at']
);}
function pay_eco_001_pack_candidate_sha256(array $candidate){return pay_eco_001_hash(pay_eco_001_pack_candidate_payload($candidate));}
function pay_eco_001_validate_pack_candidate($candidate,&$error){$error=null;$fields=array('publisher_profile_id','pack_version_id','candidate_code','compatibility_sha256','certification_sha256','support_sha256','vulnerability_sha256','revocation_state_sha256','sandbox_profile_sha256','submitted_by','submitted_at','candidate_sha256');if(!pay_eco_001_exact_fields($candidate,$fields))return pay_eco_001_fail($error,'pack_candidate_fields_invalid');if(!pay_eco_001_positive_id($candidate['publisher_profile_id'])||!pay_eco_001_positive_id($candidate['pack_version_id'])||!pay_eco_001_positive_id($candidate['submitted_by'])||!pay_eco_001_token($candidate['candidate_code']))return pay_eco_001_fail($error,'pack_candidate_identity_invalid');foreach(array('compatibility_sha256','certification_sha256','support_sha256','vulnerability_sha256','revocation_state_sha256','sandbox_profile_sha256','candidate_sha256') as $field)if(!pay_eco_001_sha($candidate[$field]))return pay_eco_001_fail($error,'pack_candidate_hash_invalid');if(pay_eco_001_time($candidate['submitted_at'])===false)return pay_eco_001_fail($error,'pack_candidate_time_invalid');if(!hash_equals(pay_eco_001_pack_candidate_sha256($candidate),(string)$candidate['candidate_sha256']))return pay_eco_001_fail($error,'pack_candidate_digest_mismatch');return true;}

function pay_eco_001_catalog_decisions(){return array('defer','reject','admit_for_review');}
function pay_eco_001_catalog_decision_payload(array $decision){return array(
 'catalog_candidate_id'=>(int)$decision['catalog_candidate_id'],
 'decision'=>(string)$decision['decision'],
 'trust_snapshot_sha256'=>(string)$decision['trust_snapshot_sha256'],
 'certification_snapshot_sha256'=>(string)$decision['certification_snapshot_sha256'],
 'compatibility_snapshot_sha256'=>(string)$decision['compatibility_snapshot_sha256'],
 'vulnerability_snapshot_sha256'=>(string)$decision['vulnerability_snapshot_sha256'],
 'decision_reason_sha256'=>(string)$decision['decision_reason_sha256'],
 'maker_id'=>(int)$decision['maker_id'],
 'checker_id'=>(int)$decision['checker_id'],
 'decided_at'=>(string)$decision['decided_at']
);}
function pay_eco_001_catalog_decision_sha256(array $decision){return pay_eco_001_hash(pay_eco_001_catalog_decision_payload($decision));}
function pay_eco_001_validate_catalog_decision($decision,&$error){$error=null;$fields=array('catalog_candidate_id','decision','trust_snapshot_sha256','certification_snapshot_sha256','compatibility_snapshot_sha256','vulnerability_snapshot_sha256','decision_reason_sha256','maker_id','checker_id','decided_at','decision_sha256');if(!pay_eco_001_exact_fields($decision,$fields))return pay_eco_001_fail($error,'catalog_decision_fields_invalid');if(!pay_eco_001_positive_id($decision['catalog_candidate_id'])||!pay_eco_001_positive_id($decision['maker_id'])||!pay_eco_001_positive_id($decision['checker_id'])||(int)$decision['maker_id']===(int)$decision['checker_id'])return pay_eco_001_fail($error,'catalog_decision_separation_invalid');if(!in_array($decision['decision'],pay_eco_001_catalog_decisions(),true))return pay_eco_001_fail($error,'catalog_decision_value_invalid');foreach(array('trust_snapshot_sha256','certification_snapshot_sha256','compatibility_snapshot_sha256','vulnerability_snapshot_sha256','decision_reason_sha256','decision_sha256') as $field)if(!pay_eco_001_sha($decision[$field]))return pay_eco_001_fail($error,'catalog_decision_hash_invalid');if(pay_eco_001_time($decision['decided_at'])===false)return pay_eco_001_fail($error,'catalog_decision_time_invalid');if(!hash_equals(pay_eco_001_catalog_decision_sha256($decision),(string)$decision['decision_sha256']))return pay_eco_001_fail($error,'catalog_decision_digest_mismatch');return true;}


/** Partner onboarding reuses the already accepted PAY-CTRY trust/install authorities; no role is auto-granted. */
function pay_eco_001_onboarding_permissions(){return array(
 'publisher_profile'=>PAY_ECO_001_PUBLISHER_PROFILE_PERMISSION,
 'pack_candidate'=>PAY_ECO_001_PACK_CANDIDATE_PERMISSION
);}

/** Validate the closed digest-only publisher-profile request envelope. */
function pay_eco_001_validate_publisher_profile_request($request,&$error){
 $error=null;$fields=array('publisher_id','profile_code','identity_evidence_sha256','support_policy_sha256','security_contact_sha256','terms_sha256');
 if(!pay_eco_001_exact_fields($request,$fields))return pay_eco_001_fail($error,'publisher_profile_request_fields_invalid');
 if(!pay_eco_001_positive_id($request['publisher_id'])||!pay_eco_001_token($request['profile_code']))return pay_eco_001_fail($error,'publisher_profile_request_identity_invalid');
 foreach(array('identity_evidence_sha256','support_policy_sha256','security_contact_sha256','terms_sha256') as $field)if(!pay_eco_001_sha($request[$field]))return pay_eco_001_fail($error,'publisher_profile_request_hash_invalid');
 return true;
}
/** Build an immutable publisher profile using server-supplied actor and time. */
function pay_eco_001_build_publisher_profile($request,$actor_id,$requested_at,&$error){
 if(!pay_eco_001_validate_publisher_profile_request($request,$error))return false;
 if(!pay_eco_001_positive_id($actor_id)||pay_eco_001_time($requested_at)===false)return pay_eco_001_fail($error,'publisher_profile_server_context_invalid');
 $profile=$request;$profile['requested_by']=(int)$actor_id;$profile['requested_at']=(string)$requested_at;$profile['profile_sha256']='';$profile['profile_sha256']=pay_eco_001_publisher_profile_sha256($profile);
 return pay_eco_001_validate_publisher_profile($profile,$error)?$profile:false;
}
/** Validate the closed digest-only pack-candidate request envelope. */
function pay_eco_001_validate_pack_candidate_request($request,&$error){
 $error=null;$fields=array('publisher_profile_id','pack_version_id','candidate_code','compatibility_sha256','certification_sha256','support_sha256','vulnerability_sha256','revocation_state_sha256','sandbox_profile_sha256');
 if(!pay_eco_001_exact_fields($request,$fields))return pay_eco_001_fail($error,'pack_candidate_request_fields_invalid');
 if(!pay_eco_001_positive_id($request['publisher_profile_id'])||!pay_eco_001_positive_id($request['pack_version_id'])||!pay_eco_001_token($request['candidate_code']))return pay_eco_001_fail($error,'pack_candidate_request_identity_invalid');
 foreach(array('compatibility_sha256','certification_sha256','support_sha256','vulnerability_sha256','revocation_state_sha256','sandbox_profile_sha256') as $field)if(!pay_eco_001_sha($request[$field]))return pay_eco_001_fail($error,'pack_candidate_request_hash_invalid');
 return true;
}
/** Build an immutable pack candidate using server-supplied actor and time. */
function pay_eco_001_build_pack_candidate($request,$actor_id,$submitted_at,&$error){
 if(!pay_eco_001_validate_pack_candidate_request($request,$error))return false;
 if(!pay_eco_001_positive_id($actor_id)||pay_eco_001_time($submitted_at)===false)return pay_eco_001_fail($error,'pack_candidate_server_context_invalid');
 $candidate=$request;$candidate['submitted_by']=(int)$actor_id;$candidate['submitted_at']=(string)$submitted_at;$candidate['candidate_sha256']='';$candidate['candidate_sha256']=pay_eco_001_pack_candidate_sha256($candidate);
 return pay_eco_001_validate_pack_candidate($candidate,$error)?$candidate:false;
}

/** Declarative-only sandbox profile. It cannot execute partner code or access production/runtime services. */
function pay_eco_001_sandbox_profile(){return array(
 'profile_code'=>'pay-eco-declarative-conformance-v1',
 'declarative_only'=>true,
 'partner_executable_code'=>false,
 'network_access'=>false,
 'filesystem_write'=>false,
 'database_write'=>false,
 'process_spawn'=>false,
 'core_source_write'=>false,
 'production_data_access'=>false,
 'external_delivery'=>false
);}
function pay_eco_001_sandbox_profile_sha256(){return pay_eco_001_hash(pay_eco_001_sandbox_profile());}
/** Stable source-owned conformance suite descriptor for the first bounded evaluation profile. */
function pay_eco_001_sandbox_suite_descriptor(){return array(
 'suite_code'=>'pay-eco-sandbox-conformance-v1',
 'profile_sha256'=>pay_eco_001_sandbox_profile_sha256(),
 'checks'=>array('accepted_verification_report','candidate_digest_integrity','candidate_profile_binding','artifact_inventory_integrity','artifact_count_ceiling','artifact_total_size_ceiling','artifact_ordinal_integrity','artifact_path_uniqueness','artifact_evidence_classification','artifact_type_media_allowlist','normalized_safe_paths','member_size_ceiling','declarative_only_boundary')
);}
function pay_eco_001_sandbox_suite_sha256(){return pay_eco_001_hash(pay_eco_001_sandbox_suite_descriptor());}
function pay_eco_001_sandbox_results(){return array('passed','failed');}
/** Build the immutable server-owned evaluation digest payload. */
function pay_eco_001_sandbox_evaluation_payload(array $evaluation){return array(
 'catalog_candidate_id'=>(int)$evaluation['catalog_candidate_id'],
 'sandbox_profile_sha256'=>(string)$evaluation['sandbox_profile_sha256'],
 'conformance_suite_sha256'=>(string)$evaluation['conformance_suite_sha256'],
 'manifest_sha256'=>(string)$evaluation['manifest_sha256'],
 'semantic_root'=>(string)$evaluation['semantic_root'],
 'evidence_root'=>(string)$evaluation['evidence_root'],
 'artifact_inventory_sha256'=>(string)$evaluation['artifact_inventory_sha256'],
 'result'=>(string)$evaluation['result'],
 'findings_sha256'=>(string)$evaluation['findings_sha256'],
 'evaluated_by'=>(int)$evaluation['evaluated_by'],
 'evaluated_at'=>(string)$evaluation['evaluated_at']
);}
function pay_eco_001_sandbox_evaluation_sha256(array $evaluation){return pay_eco_001_hash(pay_eco_001_sandbox_evaluation_payload($evaluation));}
function pay_eco_001_validate_sandbox_evaluation($evaluation,&$error){
 $error=null;$fields=array('catalog_candidate_id','sandbox_profile_sha256','conformance_suite_sha256','manifest_sha256','semantic_root','evidence_root','artifact_inventory_sha256','result','findings_sha256','evaluated_by','evaluated_at','evaluation_sha256');
 if(!pay_eco_001_exact_fields($evaluation,$fields))return pay_eco_001_fail($error,'sandbox_evaluation_fields_invalid');
 if(!pay_eco_001_positive_id($evaluation['catalog_candidate_id'])||!pay_eco_001_positive_id($evaluation['evaluated_by']))return pay_eco_001_fail($error,'sandbox_evaluation_identity_invalid');
 foreach(array('sandbox_profile_sha256','conformance_suite_sha256','manifest_sha256','semantic_root','evidence_root','artifact_inventory_sha256','findings_sha256','evaluation_sha256') as $field)if(!pay_eco_001_sha($evaluation[$field]))return pay_eco_001_fail($error,'sandbox_evaluation_hash_invalid');
 if(!in_array($evaluation['result'],pay_eco_001_sandbox_results(),true))return pay_eco_001_fail($error,'sandbox_evaluation_result_invalid');
 if(pay_eco_001_time($evaluation['evaluated_at'])===false)return pay_eco_001_fail($error,'sandbox_evaluation_time_invalid');
 if(!hash_equals(pay_eco_001_sandbox_evaluation_sha256($evaluation),(string)$evaluation['evaluation_sha256']))return pay_eco_001_fail($error,'sandbox_evaluation_digest_mismatch');
 return true;
}
/** The caller may identify only the immutable candidate; profile, suite, result, actor and time are server-derived. */
function pay_eco_001_validate_sandbox_evaluation_request($request,&$error){$error=null;$fields=array('catalog_candidate_id');if(!pay_eco_001_exact_fields($request,$fields))return pay_eco_001_fail($error,'sandbox_evaluation_request_fields_invalid');if(!pay_eco_001_positive_id($request['catalog_candidate_id']))return pay_eco_001_fail($error,'sandbox_evaluation_request_identity_invalid');return true;}

/** Dependency-integrity scan profile. Vulnerability intelligence correlation remains explicitly unconfigured. */
function pay_eco_001_dependency_scan_profile(){return array(
 'profile_code'=>'pay-eco-dependency-integrity-v1',
 'accepted_verification_required'=>true,
 'passed_sandbox_required'=>true,
 'dependency_lock_required'=>true,
 'resolved_dependency_verification_required'=>true,
 'resolved_dependency_revocation_check'=>true,
 'network_advisory_lookup'=>false,
 'publisher_vulnerability_claim_authoritative'=>false,
 'vulnerability_intelligence_configured'=>false,
 'artifact_admission_authority'=>false
);}
function pay_eco_001_dependency_scan_profile_sha256(){return pay_eco_001_hash(pay_eco_001_dependency_scan_profile());}
function pay_eco_001_dependency_scan_suite_descriptor(){return array(
 'suite_code'=>'pay-eco-dependency-integrity-v1',
 'profile_sha256'=>pay_eco_001_dependency_scan_profile_sha256(),
 'checks'=>array('accepted_candidate_integrity','accepted_verification_report','passed_sandbox_evaluation','dependency_lock_integrity','dependency_member_ordinal_integrity','resolved_pack_identity','resolved_pack_accepted_verification','resolved_publisher_effective','resolved_publisher_not_revoked','publisher_vulnerability_evidence_bound','vulnerability_intelligence_unconfigured_fail_closed')
);}
function pay_eco_001_dependency_scan_suite_sha256(){return pay_eco_001_hash(pay_eco_001_dependency_scan_suite_descriptor());}
function pay_eco_001_dependency_results(){return array('passed','failed');}
function pay_eco_001_vulnerability_results(){return array('incomplete');}
function pay_eco_001_dependency_scan_results(){return array('incomplete','failed');}
function pay_eco_001_dependency_scan_payload(array $scan){return array(
 'catalog_candidate_id'=>(int)$scan['catalog_candidate_id'],
 'sandbox_evaluation_id'=>(int)$scan['sandbox_evaluation_id'],
 'dependency_lock_id'=>(int)$scan['dependency_lock_id'],
 'scan_profile_sha256'=>(string)$scan['scan_profile_sha256'],
 'scan_suite_sha256'=>(string)$scan['scan_suite_sha256'],
 'dependency_lock_sha256'=>(string)$scan['dependency_lock_sha256'],
 'dependency_inventory_sha256'=>(string)$scan['dependency_inventory_sha256'],
 'dependency_trust_snapshot_sha256'=>(string)$scan['dependency_trust_snapshot_sha256'],
 'publisher_vulnerability_evidence_sha256'=>(string)$scan['publisher_vulnerability_evidence_sha256'],
 'dependency_result'=>(string)$scan['dependency_result'],
 'vulnerability_result'=>(string)$scan['vulnerability_result'],
 'result'=>(string)$scan['result'],
 'findings_sha256'=>(string)$scan['findings_sha256'],
 'scanned_by'=>(int)$scan['scanned_by'],
 'scanned_at'=>(string)$scan['scanned_at']
);}
function pay_eco_001_dependency_scan_sha256(array $scan){return pay_eco_001_hash(pay_eco_001_dependency_scan_payload($scan));}
function pay_eco_001_validate_dependency_scan($scan,&$error){
 $error=null;$fields=array('catalog_candidate_id','sandbox_evaluation_id','dependency_lock_id','scan_profile_sha256','scan_suite_sha256','dependency_lock_sha256','dependency_inventory_sha256','dependency_trust_snapshot_sha256','publisher_vulnerability_evidence_sha256','dependency_result','vulnerability_result','result','findings_sha256','scanned_by','scanned_at','scan_sha256');
 if(!pay_eco_001_exact_fields($scan,$fields))return pay_eco_001_fail($error,'dependency_scan_fields_invalid');
 foreach(array('catalog_candidate_id','sandbox_evaluation_id','dependency_lock_id','scanned_by') as $field)if(!pay_eco_001_positive_id($scan[$field]))return pay_eco_001_fail($error,'dependency_scan_identity_invalid');
 foreach(array('scan_profile_sha256','scan_suite_sha256','dependency_lock_sha256','dependency_inventory_sha256','dependency_trust_snapshot_sha256','publisher_vulnerability_evidence_sha256','findings_sha256','scan_sha256') as $field)if(!pay_eco_001_sha($scan[$field]))return pay_eco_001_fail($error,'dependency_scan_hash_invalid');
 if(!in_array($scan['dependency_result'],pay_eco_001_dependency_results(),true)||!in_array($scan['vulnerability_result'],pay_eco_001_vulnerability_results(),true)||!in_array($scan['result'],pay_eco_001_dependency_scan_results(),true))return pay_eco_001_fail($error,'dependency_scan_result_invalid');
 if($scan['dependency_result']==='passed'&&$scan['result']!=='incomplete')return pay_eco_001_fail($error,'dependency_scan_vulnerability_fail_closed');
 if($scan['dependency_result']==='failed'&&$scan['result']!=='failed')return pay_eco_001_fail($error,'dependency_scan_failure_state_invalid');
 if(pay_eco_001_time($scan['scanned_at'])===false)return pay_eco_001_fail($error,'dependency_scan_time_invalid');
 if(!hash_equals(pay_eco_001_dependency_scan_sha256($scan),(string)$scan['scan_sha256']))return pay_eco_001_fail($error,'dependency_scan_digest_mismatch');return true;
}
function pay_eco_001_validate_dependency_scan_request($request,&$error){$error=null;$fields=array('catalog_candidate_id');if(!pay_eco_001_exact_fields($request,$fields))return pay_eco_001_fail($error,'dependency_scan_request_fields_invalid');if(!pay_eco_001_positive_id($request['catalog_candidate_id']))return pay_eco_001_fail($error,'dependency_scan_request_identity_invalid');return true;}


/** Offline authoritative vulnerability-intelligence profile. No network or artifact-admission authority is granted. */
function pay_eco_001_vulnerability_intelligence_profile(){return array(
 'profile_code'=>'pay-eco-vulnerability-intelligence-v1',
 'trusted_operator_permission'=>PAY_ECO_001_VULNERABILITY_INTELLIGENCE_PERMISSION,
 'accepted_source_authority_sha256'=>pay_eco_001_hash(array('authority_code'=>'pay-eco-vulnerability-authority-v1','custody'=>'operator-validated-offline-snapshot')),
 'offline_snapshot_ingestion'=>true,
 'network_advisory_lookup'=>false,
 'publisher_claim_authoritative'=>false,
 'exact_package_version_correlation'=>true,
 'max_advisories'=>256,
 'snapshot_expiry_required'=>true,
 'artifact_admission_authority'=>false,
 'publication_authority'=>false,
 'installation_authority'=>false
);}
function pay_eco_001_vulnerability_intelligence_profile_sha256(){return pay_eco_001_hash(pay_eco_001_vulnerability_intelligence_profile());}
function pay_eco_001_vulnerability_severities(){return array('low','medium','high','critical');}
function pay_eco_001_vulnerability_advisory_payload(array $a){return array(
 'advisory_code'=>(string)$a['advisory_code'],'package_code'=>(string)$a['package_code'],'affected_version'=>(string)$a['affected_version'],
 'severity'=>(string)$a['severity'],'advisory_evidence_sha256'=>(string)$a['advisory_evidence_sha256']
);}
function pay_eco_001_vulnerability_advisory_sha256(array $a){return pay_eco_001_hash(pay_eco_001_vulnerability_advisory_payload($a));}
function pay_eco_001_validate_vulnerability_advisory($a,&$error){
 $error=null;$fields=array('advisory_code','package_code','affected_version','severity','advisory_evidence_sha256','advisory_sha256');
 if(!pay_eco_001_exact_fields($a,$fields))return pay_eco_001_fail($error,'vulnerability_advisory_fields_invalid');
 foreach(array('advisory_code','package_code','affected_version') as $field)if(!pay_eco_001_token($a[$field],96))return pay_eco_001_fail($error,'vulnerability_advisory_identity_invalid');
 if(!in_array($a['severity'],pay_eco_001_vulnerability_severities(),true))return pay_eco_001_fail($error,'vulnerability_advisory_severity_invalid');
 foreach(array('advisory_evidence_sha256','advisory_sha256') as $field)if(!pay_eco_001_sha($a[$field]))return pay_eco_001_fail($error,'vulnerability_advisory_hash_invalid');
 if(!hash_equals(pay_eco_001_vulnerability_advisory_sha256($a),(string)$a['advisory_sha256']))return pay_eco_001_fail($error,'vulnerability_advisory_digest_mismatch');return true;
}
function pay_eco_001_vulnerability_snapshot_payload(array $s){return array(
 'source_code'=>(string)$s['source_code'],'snapshot_code'=>(string)$s['snapshot_code'],'source_authority_sha256'=>(string)$s['source_authority_sha256'],
 'profile_sha256'=>(string)$s['profile_sha256'],'issued_at'=>(string)$s['issued_at'],'expires_at'=>(string)$s['expires_at'],
 'advisory_count'=>(int)$s['advisory_count'],'advisories_root'=>(string)$s['advisories_root']
);}
function pay_eco_001_vulnerability_snapshot_sha256(array $s){return pay_eco_001_hash(pay_eco_001_vulnerability_snapshot_payload($s));}
function pay_eco_001_validate_vulnerability_snapshot($s,&$error){
 $error=null;$fields=array('source_code','snapshot_code','source_authority_sha256','profile_sha256','issued_at','expires_at','advisory_count','advisories_root','snapshot_sha256','ingested_by','ingested_at');
 if(!pay_eco_001_exact_fields($s,$fields))return pay_eco_001_fail($error,'vulnerability_snapshot_fields_invalid');
 if(!pay_eco_001_token($s['source_code'],64)||!pay_eco_001_token($s['snapshot_code'],96)||!pay_eco_001_positive_id($s['ingested_by']))return pay_eco_001_fail($error,'vulnerability_snapshot_identity_invalid');
 foreach(array('source_authority_sha256','profile_sha256','advisories_root','snapshot_sha256') as $field)if(!pay_eco_001_sha($s[$field]))return pay_eco_001_fail($error,'vulnerability_snapshot_hash_invalid');
 $profile=pay_eco_001_vulnerability_intelligence_profile();if(!hash_equals($profile['accepted_source_authority_sha256'],(string)$s['source_authority_sha256'])||!hash_equals(pay_eco_001_vulnerability_intelligence_profile_sha256(),(string)$s['profile_sha256']))return pay_eco_001_fail($error,'vulnerability_snapshot_authority_invalid');
 $issued=pay_eco_001_time($s['issued_at']);$expires=pay_eco_001_time($s['expires_at']);$ingested=pay_eco_001_time($s['ingested_at']);if($issued===false||$expires===false||$ingested===false||$expires<=$issued||$ingested<$issued)return pay_eco_001_fail($error,'vulnerability_snapshot_time_invalid');
 if(!is_int($s['advisory_count'])&& !ctype_digit((string)$s['advisory_count']))return pay_eco_001_fail($error,'vulnerability_snapshot_count_invalid');$count=(int)$s['advisory_count'];if($count<0||$count>(int)$profile['max_advisories'])return pay_eco_001_fail($error,'vulnerability_snapshot_count_invalid');
 if(!hash_equals(pay_eco_001_vulnerability_snapshot_sha256($s),(string)$s['snapshot_sha256']))return pay_eco_001_fail($error,'vulnerability_snapshot_digest_mismatch');return true;
}
function pay_eco_001_validate_vulnerability_snapshot_request($request,&$error){
 $error=null;$fields=array('source_code','snapshot_code','source_authority_sha256','issued_at','expires_at','advisories');if(!pay_eco_001_exact_fields($request,$fields))return pay_eco_001_fail($error,'vulnerability_snapshot_request_fields_invalid');
 if(!pay_eco_001_token($request['source_code'],64)||!pay_eco_001_token($request['snapshot_code'],96)||!pay_eco_001_sha($request['source_authority_sha256'])||!is_array($request['advisories']))return pay_eco_001_fail($error,'vulnerability_snapshot_request_invalid');
 $profile=pay_eco_001_vulnerability_intelligence_profile();if(!hash_equals($profile['accepted_source_authority_sha256'],(string)$request['source_authority_sha256']))return pay_eco_001_fail($error,'vulnerability_snapshot_source_untrusted');
 $issued=pay_eco_001_time($request['issued_at']);$expires=pay_eco_001_time($request['expires_at']);if($issued===false||$expires===false||$expires<=$issued)return pay_eco_001_fail($error,'vulnerability_snapshot_request_time_invalid');
 if(count($request['advisories'])>(int)$profile['max_advisories'])return pay_eco_001_fail($error,'vulnerability_snapshot_request_count_invalid');$previous='';$seen=array();
 foreach($request['advisories'] as $a){if(!pay_eco_001_validate_vulnerability_advisory($a,$error))return false;$key=$a['advisory_code'].'|'.$a['package_code'].'|'.$a['affected_version'];if(isset($seen[$key])||($previous!==''&&strcmp($key,$previous)<=0))return pay_eco_001_fail($error,'vulnerability_snapshot_advisory_order_invalid');$seen[$key]=true;$previous=$key;}return true;
}
function pay_eco_001_vulnerability_correlation_results(){return array('passed','failed');}
function pay_eco_001_vulnerability_correlation_payload(array $c){return array(
 'dependency_scan_id'=>(int)$c['dependency_scan_id'],'vulnerability_snapshot_id'=>(int)$c['vulnerability_snapshot_id'],'subject_inventory_sha256'=>(string)$c['subject_inventory_sha256'],
 'matched_advisories_sha256'=>(string)$c['matched_advisories_sha256'],'result'=>(string)$c['result'],'max_severity'=>(string)$c['max_severity'],'findings_sha256'=>(string)$c['findings_sha256'],
 'correlated_by'=>(int)$c['correlated_by'],'correlated_at'=>(string)$c['correlated_at']
);}
function pay_eco_001_vulnerability_correlation_sha256(array $c){return pay_eco_001_hash(pay_eco_001_vulnerability_correlation_payload($c));}
function pay_eco_001_validate_vulnerability_correlation($c,&$error){$error=null;$fields=array('dependency_scan_id','vulnerability_snapshot_id','subject_inventory_sha256','matched_advisories_sha256','result','max_severity','findings_sha256','correlated_by','correlated_at','correlation_sha256');if(!pay_eco_001_exact_fields($c,$fields))return pay_eco_001_fail($error,'vulnerability_correlation_fields_invalid');foreach(array('dependency_scan_id','vulnerability_snapshot_id','correlated_by') as $field)if(!pay_eco_001_positive_id($c[$field]))return pay_eco_001_fail($error,'vulnerability_correlation_identity_invalid');foreach(array('subject_inventory_sha256','matched_advisories_sha256','findings_sha256','correlation_sha256') as $field)if(!pay_eco_001_sha($c[$field]))return pay_eco_001_fail($error,'vulnerability_correlation_hash_invalid');if(!in_array($c['result'],pay_eco_001_vulnerability_correlation_results(),true))return pay_eco_001_fail($error,'vulnerability_correlation_result_invalid');if($c['result']==='passed'&&$c['max_severity']!=='none')return pay_eco_001_fail($error,'vulnerability_correlation_pass_severity_invalid');if($c['result']==='failed'&&!in_array($c['max_severity'],pay_eco_001_vulnerability_severities(),true))return pay_eco_001_fail($error,'vulnerability_correlation_fail_severity_invalid');if(pay_eco_001_time($c['correlated_at'])===false)return pay_eco_001_fail($error,'vulnerability_correlation_time_invalid');if(!hash_equals(pay_eco_001_vulnerability_correlation_sha256($c),(string)$c['correlation_sha256']))return pay_eco_001_fail($error,'vulnerability_correlation_digest_mismatch');return true;}
function pay_eco_001_validate_vulnerability_correlation_request($request,&$error){$error=null;$fields=array('dependency_scan_id');if(!pay_eco_001_exact_fields($request,$fields))return pay_eco_001_fail($error,'vulnerability_correlation_request_fields_invalid');if(!pay_eco_001_positive_id($request['dependency_scan_id']))return pay_eco_001_fail($error,'vulnerability_correlation_request_identity_invalid');return true;}


?>
