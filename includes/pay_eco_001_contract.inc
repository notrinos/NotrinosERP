<?php
/** PAY-ECO-001 third-party pack ecosystem governance foundation contract. */
include_once(dirname(__FILE__).'/pay_ctry_001_verifier.inc');

if (!defined('PAY_ECO_001_CONTRACT_VERSION')) define('PAY_ECO_001_CONTRACT_VERSION','1.0.0');
if (!defined('PAY_ECO_001_SOURCE_DATABASE_VERSION')) define('PAY_ECO_001_SOURCE_DATABASE_VERSION','1.0.993');
if (!defined('PAY_ECO_001_LEGACY_PAYROLL_CONTRACT_VERSION')) define('PAY_ECO_001_LEGACY_PAYROLL_CONTRACT_VERSION','1.0.515');
if (!defined('PAY_ECO_001_PUBLISHER_PROFILE_PERMISSION')) define('PAY_ECO_001_PUBLISHER_PROFILE_PERMISSION','SA_PAY_CTRY_TRUST');
if (!defined('PAY_ECO_001_PACK_CANDIDATE_PERMISSION')) define('PAY_ECO_001_PACK_CANDIDATE_PERMISSION','SA_PAY_CTRY_INSTALL');
if (!defined('PAY_ECO_001_SANDBOX_EVALUATION_PERMISSION')) define('PAY_ECO_001_SANDBOX_EVALUATION_PERMISSION','SA_PAY_CTRY_TRUST');

function pay_eco_001_fail(&$error,$code){$error=$code;return false;}
function pay_eco_001_canonical_json($value){return pay_ctry_001_canonical_json($value);}
function pay_eco_001_sha($value){return pay_ctry_001_sha($value);}
function pay_eco_001_hash($value){return hash('sha256',pay_eco_001_canonical_json($value));}
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
 'catalog_decision_writer'=>false,
 'sandbox_execution'=>false,
 'dependency_vulnerability_scanning'=>false,
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

?>
