<?php
/**
 * PAY-CTRY-004 certification and regulatory operating-model contract.
 *
 * This contract governs evidence, independent human review, certification,
 * publication, expiry, suspension/revocation and recovery.  It never asserts
 * that a jurisdiction is legally compliant merely because software tests pass.
 */
if (!defined('PAY_CTRY_004_CONTRACT_VERSION')) define('PAY_CTRY_004_CONTRACT_VERSION', '1.0.0');
if (!defined('PAY_CTRY_004_SOURCE_DATABASE_VERSION')) define('PAY_CTRY_004_SOURCE_DATABASE_VERSION', '1.0.812');
if (!defined('PAY_CTRY_004_SOURCE_CUSTODY_VERSION')) define('PAY_CTRY_004_SOURCE_CUSTODY_VERSION', '1.0.458');
if (!defined('PAY_CTRY_004_EVIDENCE_ROOT_VERSION')) define('PAY_CTRY_004_EVIDENCE_ROOT_VERSION', 'evidence-root-v1');
if (!defined('PAY_CTRY_004_CAPABILITY_REGISTRY_VERSION')) define('PAY_CTRY_004_CAPABILITY_REGISTRY_VERSION', 'pay-ctry-capabilities-v2');
if (!defined('PAY_CTRY_004_SIGNATURE_ALGORITHM')) define('PAY_CTRY_004_SIGNATURE_ALGORITHM', 'rsa-sha256');
if (!defined('PAY_CTRY_004_TECHNICAL_PERMISSION')) define('PAY_CTRY_004_TECHNICAL_PERMISSION', 'SA_HRM_VALIDATE_PAY_CTRY_CERTIFICATION');
if (!defined('PAY_CTRY_004_PAYROLL_EXPERT_PERMISSION')) define('PAY_CTRY_004_PAYROLL_EXPERT_PERMISSION', 'SA_HRM_REVIEW_PAY_CTRY_PAYROLL');
if (!defined('PAY_CTRY_004_LEGAL_PERMISSION')) define('PAY_CTRY_004_LEGAL_PERMISSION', 'SA_HRM_REVIEW_PAY_CTRY_LEGAL');
if (!defined('PAY_CTRY_004_CERTIFY_PERMISSION')) define('PAY_CTRY_004_CERTIFY_PERMISSION', 'SA_HRM_CERTIFY_PAY_CTRY');
if (!defined('PAY_CTRY_004_PUBLISH_PERMISSION')) define('PAY_CTRY_004_PUBLISH_PERMISSION', 'SA_HRM_PUBLISH_PAY_CTRY');
if (!defined('PAY_CTRY_004_EMERGENCY_PERMISSION')) define('PAY_CTRY_004_EMERGENCY_PERMISSION', 'SA_HRM_SUSPEND_PAY_CTRY_CERTIFICATION');
if (!defined('PAY_CTRY_004_RECOVERY_PERMISSION')) define('PAY_CTRY_004_RECOVERY_PERMISSION', 'SA_HRM_RECOVER_PAY_CTRY_CERTIFICATION');

function pay_ctry_004_sha256($value)
{
    $value = (string)$value;
    return preg_match('/^[a-f0-9]{64}$/D', $value) === 1 ? $value : false;
}
function pay_ctry_004_uint($value, $allow_zero=false)
{
    if (is_int($value)) $n=$value;
    elseif (is_string($value) && preg_match('/^[0-9]+$/D',$value)) $n=(int)$value;
    else return false;
    return ($allow_zero ? $n>=0 : $n>0) ? $n : false;
}
function pay_ctry_004_canonical_hash($value)
{
    $json=json_encode($value, JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE);
    return $json===false ? false : hash('sha256',$json);
}
function pay_ctry_004_datetime_epoch($value)
{
    if (!is_string($value) || !preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/D',$value)) return false;
    $utc=new DateTimeZone('UTC');$dt=DateTimeImmutable::createFromFormat('!Y-m-d H:i:s',$value,$utc);$errors=DateTimeImmutable::getLastErrors();
    if($dt===false||($errors!==false&&((int)$errors['warning_count']>0||(int)$errors['error_count']>0))||$dt->format('Y-m-d H:i:s')!==$value)return false;
    return $dt->getTimestamp();
}
function pay_ctry_004_normalize_public_key_pem($pem)
{
    if(!is_string($pem)||trim($pem)===''||!function_exists('openssl_pkey_get_public')||!function_exists('openssl_pkey_get_details'))return false;
    $pem=str_replace(array("\r\n","\r"),"\n",trim($pem))."\n";$key=@openssl_pkey_get_public($pem);if($key===false)return false;$details=@openssl_pkey_get_details($key);if(!is_array($details)||!isset($details['type'],$details['bits'],$details['key'])||(int)$details['type']!==OPENSSL_KEYTYPE_RSA||(int)$details['bits']<2048)return false;
    return str_replace(array("\r\n","\r"),"\n",trim((string)$details['key']))."\n";
}
function pay_ctry_004_certification_signing_key_fields(array $key)
{
    return array(
        'key_id'=>isset($key['key_id'])?(string)$key['key_id']:'',
        'actor_id'=>isset($key['actor_id'])?(int)$key['actor_id']:0,
        'actor_role'=>isset($key['actor_role'])?(string)$key['actor_role']:'',
        'algorithm'=>isset($key['algorithm'])?(string)$key['algorithm']:'',
        'public_key_sha256'=>isset($key['public_key_sha256'])?(string)$key['public_key_sha256']:'',
        'valid_from'=>isset($key['valid_from'])?(string)$key['valid_from']:'',
        'valid_to'=>isset($key['valid_to'])&&$key['valid_to']!==null?(string)$key['valid_to']:''
    );
}
function pay_ctry_004_certification_signing_key_sha256(array $key){return pay_ctry_004_canonical_hash(pay_ctry_004_certification_signing_key_fields($key));}
function pay_ctry_004_validate_certification_signing_key(array $key,$at,&$error=null)
{
    $error=null;foreach(array('key_id','actor_id','actor_role','algorithm','public_key_pem','public_key_sha256','valid_from','key_sha256') as $k)if(!isset($key[$k])||(string)$key[$k]===''){$error='certification_signing_key_field_missing';return false;}
    if(!preg_match('/^[A-Za-z0-9._:-]{1,96}$/D',(string)$key['key_id'])){$error='certification_signing_key_id_invalid';return false;}
    if(pay_ctry_004_uint($key['actor_id'],false)===false||(string)$key['actor_role']!=='certification_authority'){$error='certification_signing_key_authority_invalid';return false;}
    if((string)$key['algorithm']!==PAY_CTRY_004_SIGNATURE_ALGORITHM){$error='certification_signature_algorithm_unsupported';return false;}
    $pem=pay_ctry_004_normalize_public_key_pem((string)$key['public_key_pem']);if($pem===false){$error='certification_public_key_invalid';return false;}
    $finger=hash('sha256',$pem);if(pay_ctry_004_sha256((string)$key['public_key_sha256'])===false||(string)$key['public_key_sha256']!==$finger){$error='certification_public_key_digest_mismatch';return false;}
    if(pay_ctry_004_sha256((string)$key['key_sha256'])===false||(string)$key['key_sha256']!==pay_ctry_004_certification_signing_key_sha256($key)){$error='certification_signing_key_digest_mismatch';return false;}
    $now=pay_ctry_004_datetime_epoch((string)$at);$from=pay_ctry_004_datetime_epoch((string)$key['valid_from']);$to=(isset($key['valid_to'])&&$key['valid_to']!==null&&$key['valid_to']!=='')?pay_ctry_004_datetime_epoch((string)$key['valid_to']):null;
    if($now===false||$from===false||$now<$from||($to!==null&&($to===false||$now>=$to))){$error='certification_signing_key_not_current';return false;}
    if(isset($key['revoked_at'])&&$key['revoked_at']!==null&&$key['revoked_at']!==''){$rev=pay_ctry_004_datetime_epoch((string)$key['revoked_at']);if($rev===false){$error='certification_signing_key_revocation_invalid';return false;}if($now>=$rev){$error='certification_signing_key_revoked';return false;}}
    return true;
}
function pay_ctry_004_certification_decision_fields(array $event)
{
    $nullable=array('predecessor_decision_sha256','parallel_evidence_id','claim_scope_sha256','valid_from','expires_at','assurance_sha256');
    $out=array(
        'certification_key'=>isset($event['certification_key'])?(string)$event['certification_key']:'',
        'event_no'=>isset($event['event_no'])?(int)$event['event_no']:0,
        'predecessor_decision_sha256'=>isset($event['predecessor_decision_sha256'])&&$event['predecessor_decision_sha256']!==null?(string)$event['predecessor_decision_sha256']:'',
        'pack_version_id'=>isset($event['pack_version_id'])?(int)$event['pack_version_id']:0,
        'evidence_inventory_id'=>isset($event['evidence_inventory_id'])?(int)$event['evidence_inventory_id']:0,
        'registry_version_id'=>isset($event['registry_version_id'])?(int)$event['registry_version_id']:0,
        'parallel_evidence_id'=>isset($event['parallel_evidence_id'])&&$event['parallel_evidence_id']!==null?(int)$event['parallel_evidence_id']:0,
        'from_status'=>isset($event['from_status'])?(string)$event['from_status']:'',
        'to_status'=>isset($event['to_status'])?(string)$event['to_status']:'',
        'jurisdiction'=>isset($event['jurisdiction'])?(string)$event['jurisdiction']:'',
        'semantic_root'=>isset($event['semantic_root'])?(string)$event['semantic_root']:'',
        'evidence_root'=>isset($event['evidence_root'])?(string)$event['evidence_root']:'',
        'review_chain_sha256'=>isset($event['review_chain_sha256'])?(string)$event['review_chain_sha256']:'',
        'claim_scope_sha256'=>isset($event['claim_scope_sha256'])&&$event['claim_scope_sha256']!==null?(string)$event['claim_scope_sha256']:'',
        'valid_from'=>isset($event['valid_from'])&&$event['valid_from']!==null?(string)$event['valid_from']:'',
        'expires_at'=>isset($event['expires_at'])&&$event['expires_at']!==null?(string)$event['expires_at']:'',
        'reason_code'=>isset($event['reason_code'])?(string)$event['reason_code']:'',
        'actor_id'=>isset($event['actor_id'])?(int)$event['actor_id']:0,
        'actor_role'=>isset($event['actor_role'])?(string)$event['actor_role']:'',
        'assurance_sha256'=>isset($event['assurance_sha256'])&&$event['assurance_sha256']!==null?(string)$event['assurance_sha256']:''
    );
    return $out;
}
function pay_ctry_004_certification_decision_sha256(array $event){return pay_ctry_004_canonical_hash(pay_ctry_004_certification_decision_fields($event));}
function pay_ctry_004_validate_certification_event(array $event,&$error=null)
{
    $error=null;$f=pay_ctry_004_certification_decision_fields($event);
    if(!preg_match('/^[A-Za-z0-9._:-]{1,96}$/D',$f['certification_key'])||$f['event_no']<1||$f['pack_version_id']<1||$f['evidence_inventory_id']<1||$f['registry_version_id']<1){$error='certification_identity_invalid';return false;}
    if(!in_array($f['from_status'],pay_ctry_004_lifecycle_states(),true)||!in_array($f['to_status'],pay_ctry_004_lifecycle_states(),true)){$error='certification_status_invalid';return false;}
    if($f['jurisdiction']===''||strlen($f['jurisdiction'])>64){$error='certification_jurisdiction_invalid';return false;}
    foreach(array('semantic_root','evidence_root','review_chain_sha256') as $k)if(pay_ctry_004_sha256($f[$k])===false){$error='certification_digest_invalid';return false;}
    foreach(array('predecessor_decision_sha256','claim_scope_sha256','assurance_sha256') as $k)if($f[$k]!==''&&pay_ctry_004_sha256($f[$k])===false){$error='certification_digest_invalid';return false;}
    if($f['event_no']===1&&$f['predecessor_decision_sha256']!==''){$error='certification_predecessor_unexpected';return false;}if($f['event_no']>1&&$f['predecessor_decision_sha256']===''){$error='certification_predecessor_required';return false;}
    if(!preg_match('/^[A-Za-z0-9._:-]{1,64}$/D',$f['reason_code'])||pay_ctry_004_uint($f['actor_id'],false)===false||!in_array($f['actor_role'],pay_ctry_004_roles(),true)){$error='certification_actor_or_reason_invalid';return false;}
    if($f['valid_from']!==''&&pay_ctry_004_datetime_epoch($f['valid_from'])===false){$error='certification_valid_from_invalid';return false;}if($f['expires_at']!==''&&(pay_ctry_004_datetime_epoch($f['expires_at'])===false||$f['valid_from']===''||pay_ctry_004_datetime_epoch($f['expires_at'])<=pay_ctry_004_datetime_epoch($f['valid_from']))){$error='certification_expiry_invalid';return false;}
    if($f['to_status']==='Certified'){
        if($f['from_status']!=='ParallelRunPassed'||$f['parallel_evidence_id']<1||$f['claim_scope_sha256']===''||$f['valid_from']===''||$f['expires_at']===''||$f['actor_role']!=='certification_authority'){$error='certification_binding_incomplete';return false;}
    }
    if(!isset($event['decision_sha256'])||pay_ctry_004_sha256((string)$event['decision_sha256'])===false||(string)$event['decision_sha256']!==pay_ctry_004_certification_decision_sha256($event)){$error='certification_decision_digest_mismatch';return false;}
    return true;
}
function pay_ctry_004_certification_signature_fields(array $event)
{
    return array('decision'=>pay_ctry_004_certification_decision_fields($event),'decision_sha256'=>isset($event['decision_sha256'])?(string)$event['decision_sha256']:'','certification_signing_key_id'=>isset($event['certification_signing_key_id'])?(int)$event['certification_signing_key_id']:0,'signature_algorithm'=>isset($event['signature_algorithm'])?(string)$event['signature_algorithm']:'','signed_at'=>isset($event['signed_at'])?(string)$event['signed_at']:'');
}
function pay_ctry_004_certification_signature_payload(array $event)
{
    $json=json_encode(pay_ctry_004_certification_signature_fields($event),JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE);return $json===false?false:$json;
}
function pay_ctry_004_verify_certification_signature(array $event,array $key,&$error=null)
{
    $error=null;if(!pay_ctry_004_validate_certification_event($event,$error))return false;if((string)$event['to_status']!=='Certified'){$error='certification_signature_status_invalid';return false;}
    foreach(array('certification_signing_key_id','signature_algorithm','signed_at','signature_base64','signature_payload_sha256') as $k)if(!isset($event[$k])||(string)$event[$k]===''){$error='certification_signature_field_missing';return false;}
    if((string)$event['signature_algorithm']!==PAY_CTRY_004_SIGNATURE_ALGORITHM){$error='certification_signature_algorithm_unsupported';return false;}
    if((string)$event['actor_role']!=='certification_authority'){$error='certification_signer_role_invalid';return false;}
    if(!isset($key['certification_signing_key_id'])||(int)$event['certification_signing_key_id']!==(int)$key['certification_signing_key_id']){$error='certification_signing_key_mismatch';return false;}
    if(!pay_ctry_004_validate_certification_signing_key($key,(string)$event['signed_at'],$error))return false;
    if((int)$key['actor_id']!==(int)$event['actor_id']){$error='certification_signer_actor_mismatch';return false;}
    $payload=pay_ctry_004_certification_signature_payload($event);if($payload===false||pay_ctry_004_sha256((string)$event['signature_payload_sha256'])===false||(string)$event['signature_payload_sha256']!==hash('sha256',$payload)){$error='certification_signature_payload_mismatch';return false;}
    $sig=base64_decode((string)$event['signature_base64'],true);if(!is_string($sig)||strlen($sig)<128||strlen($sig)>1024){$error='certification_signature_encoding_invalid';return false;}
    $pem=pay_ctry_004_normalize_public_key_pem((string)$key['public_key_pem']);if($pem===false){$error='certification_public_key_invalid';return false;}$pub=@openssl_pkey_get_public($pem);if($pub===false){$error='certification_public_key_invalid';return false;}$ok=@openssl_verify($payload,$sig,$pub,OPENSSL_ALGO_SHA256);if($ok!==1){$error='certification_signature_invalid';return false;}return true;
}
function pay_ctry_004_roles()
{
    return array('technical_validator','payroll_expert','legal_reviewer','certification_authority','publisher','emergency_authority','recovery_authority');
}
function pay_ctry_004_role_permissions()
{
    return array(
        'technical_validator'=>PAY_CTRY_004_TECHNICAL_PERMISSION,
        'payroll_expert'=>PAY_CTRY_004_PAYROLL_EXPERT_PERMISSION,
        'legal_reviewer'=>PAY_CTRY_004_LEGAL_PERMISSION,
        'certification_authority'=>PAY_CTRY_004_CERTIFY_PERMISSION,
        'publisher'=>PAY_CTRY_004_PUBLISH_PERMISSION,
        'emergency_authority'=>PAY_CTRY_004_EMERGENCY_PERMISSION,
        'recovery_authority'=>PAY_CTRY_004_RECOVERY_PERMISSION
    );
}
function pay_ctry_004_review_roles()
{
    return array('technical'=>'technical_validator','payroll_expert'=>'payroll_expert','legal'=>'legal_reviewer');
}
function pay_ctry_004_lifecycle_states()
{
    return array('Draft','TechnicallyValidated','PayrollExpertReviewed','LegallyReviewed','ParallelRunPassed','Certified','Published','Superseded','Suspended','Revoked','Expired');
}
function pay_ctry_004_allowed_transitions()
{
    return array(
        'Draft'=>array('TechnicallyValidated'),
        'TechnicallyValidated'=>array('PayrollExpertReviewed'),
        'PayrollExpertReviewed'=>array('LegallyReviewed'),
        'LegallyReviewed'=>array('ParallelRunPassed'),
        'ParallelRunPassed'=>array('Certified'),
        'Certified'=>array('Published','Suspended','Revoked'),
        'Published'=>array('Superseded','Suspended','Revoked'),
        'Suspended'=>array('Draft','Revoked'),
        'Expired'=>array('Draft'),
        'Superseded'=>array(),
        'Revoked'=>array()
    );
}
function pay_ctry_004_evidence_media()
{
    return array(
        'legislation'=>array('application/pdf','text/plain'),
        'regulation'=>array('application/pdf','text/plain'),
        'authority-publication'=>array('application/pdf','text/plain','application/json','application/xml'),
        'technical-specification'=>array('application/pdf','text/plain','application/json','application/xml'),
        'technical-validation'=>array('application/json','text/plain'),
        'payroll-expert-review'=>array('application/json','text/plain','application/pdf'),
        'legal-review'=>array('application/json','text/plain','application/pdf'),
        'parallel-run'=>array('application/json'),
        'conformance-result'=>array('application/json')
    );
}
function pay_ctry_004_evidence_identity_fields(array $item)
{
    return array(
        'source_identity_sha256'=>isset($item['source_identity_sha256'])?(string)$item['source_identity_sha256']:'',
        'source_version'=>isset($item['source_version'])?(string)$item['source_version']:'',
        'evidence_type'=>isset($item['evidence_type'])?(string)$item['evidence_type']:'',
        'media_type'=>isset($item['media_type'])?(string)$item['media_type']:'',
        'content_sha256'=>isset($item['content_sha256'])?(string)$item['content_sha256']:''
    );
}
function pay_ctry_004_evidence_identity(array $item)
{
    return pay_ctry_004_canonical_hash(pay_ctry_004_evidence_identity_fields($item));
}
function pay_ctry_004_validate_evidence_item(array $item, &$error=null)
{
    $error=null;$media=pay_ctry_004_evidence_media();$fields=pay_ctry_004_evidence_identity_fields($item);
    if (pay_ctry_004_sha256($fields['source_identity_sha256'])===false || pay_ctry_004_sha256($fields['content_sha256'])===false) { $error='evidence_digest_invalid'; return false; }
    if ($fields['source_version']==='' || strlen($fields['source_version'])>96) { $error='evidence_source_version_invalid'; return false; }
    if (!isset($media[$fields['evidence_type']])) { $error='unsupported_evidence_type'; return false; }
    if (!in_array($fields['media_type'],$media[$fields['evidence_type']],true)) { $error='unsupported_evidence_media'; return false; }
    $identity=isset($item['evidence_identity_sha256'])?(string)$item['evidence_identity_sha256']:'';
    if (pay_ctry_004_sha256($identity)===false || $identity!==pay_ctry_004_evidence_identity($item)) { $error='evidence_identity_mismatch'; return false; }
    return true;
}
function pay_ctry_004_evidence_root(array $inventory, &$error=null)
{
    $error=null;if (!$inventory) { $error='evidence_inventory_empty'; return false; }
    $canonical=array();$seen=array();$slots=array();
    foreach ($inventory as $item) {
        if (!is_array($item) || !pay_ctry_004_validate_evidence_item($item,$error)) return false;
        $identity=(string)$item['evidence_identity_sha256'];$row=pay_ctry_004_evidence_identity_fields($item);$row['evidence_identity_sha256']=$identity;
        if(isset($seen[$identity])){$error='duplicate_evidence_member';return false;}
        $slot=pay_ctry_004_canonical_hash(array('source_identity_sha256'=>$row['source_identity_sha256'],'source_version'=>$row['source_version'],'evidence_type'=>$row['evidence_type'],'media_type'=>$row['media_type']));
        if(isset($slots[$slot])&&$slots[$slot]!==$identity){$error='conflicting_evidence_member';return false;}
        $slots[$slot]=$identity;$seen[$identity]=true;$canonical[]=$row;
    }
    usort($canonical,function($a,$b){$c=strcmp($a['evidence_identity_sha256'],$b['evidence_identity_sha256']);if($c!==0)return $c;return strcmp($a['content_sha256'],$b['content_sha256']);});
    $payload=array('algorithm'=>PAY_CTRY_004_EVIDENCE_ROOT_VERSION,'members'=>$canonical);
    $json=json_encode($payload,JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE);
    if ($json===false) { $error='evidence_canonicalization_failed'; return false; }
    return array('algorithm'=>PAY_CTRY_004_EVIDENCE_ROOT_VERSION,'root_sha256'=>hash('sha256',$json),'canonical_sha256'=>hash('sha256',$json),'item_count'=>count($canonical),'canonical_json'=>$json);
}
function pay_ctry_004_source_identity_fields(array $source)
{
    return array('source_code'=>isset($source['source_code'])?(string)$source['source_code']:'','source_version'=>isset($source['source_version'])?(string)$source['source_version']:'','issuer'=>isset($source['issuer'])?(string)$source['issuer']:'','jurisdiction'=>isset($source['jurisdiction'])?(string)$source['jurisdiction']:'','effective_from'=>isset($source['effective_from'])?(string)$source['effective_from']:'','effective_to'=>isset($source['effective_to'])&&$source['effective_to']!==null?(string)$source['effective_to']:'','acquired_at'=>isset($source['acquired_at'])?(string)$source['acquired_at']:'','recorded_at'=>isset($source['recorded_at'])?(string)$source['recorded_at']:'','evidence_digest'=>isset($source['evidence_digest'])?(string)$source['evidence_digest']:'','trust_status'=>isset($source['trust_status'])?(string)$source['trust_status']:'');
}
function pay_ctry_004_source_identity(array $source){return pay_ctry_004_canonical_hash(pay_ctry_004_source_identity_fields($source));}
function pay_ctry_004_validate_source(array $source, &$error=null)
{
    $error=null;foreach(array('source_code','source_version','issuer','jurisdiction','effective_from','acquired_at','recorded_at','source_identity_sha256','evidence_digest','trust_status') as $k) if(!isset($source[$k])||(string)$source[$k]===''){ $error='source_field_missing'; return false; }
    if(!preg_match('/^[A-Za-z0-9._:-]{1,96}$/D',(string)$source['source_code'])){$error='source_code_invalid';return false;}
    if(strlen((string)$source['source_version'])>96||strlen((string)$source['issuer'])>191||strlen((string)$source['jurisdiction'])>64){$error='source_field_too_long';return false;}
    $from=pay_ctry_004_datetime_epoch((string)$source['effective_from']);$to=(isset($source['effective_to'])&&$source['effective_to']!==null&&$source['effective_to']!=='')?pay_ctry_004_datetime_epoch((string)$source['effective_to']):null;
    if($from===false){$error='source_effective_from_invalid';return false;}if($to===false||($to!==null&&$to<=$from)){$error='source_effective_to_invalid';return false;}
    $acquired=pay_ctry_004_datetime_epoch((string)$source['acquired_at']);$recorded=pay_ctry_004_datetime_epoch((string)$source['recorded_at']);if($acquired===false||$recorded===false||$recorded<$acquired){$error='source_custody_time_invalid';return false;}
    if(!in_array((string)$source['trust_status'],array('recorded','reviewed','trusted','superseded','withdrawn'),true)){$error='source_trust_status_invalid';return false;}
    if(pay_ctry_004_sha256((string)$source['evidence_digest'])===false){$error='source_digest_invalid';return false;}
    if(pay_ctry_004_sha256((string)$source['source_identity_sha256'])===false||(string)$source['source_identity_sha256']!==pay_ctry_004_source_identity($source)){$error='source_identity_mismatch';return false;}
    return true;
}
function pay_ctry_004_review_independence_sha256($reviewer_id,array $prior_reviews)
{
    $id=pay_ctry_004_uint($reviewer_id,false);if($id===false)return false;$prior_ids=array();foreach($prior_reviews as $prior){if(!is_array($prior)||!isset($prior['reviewer_id'])||pay_ctry_004_uint($prior['reviewer_id'],false)===false)return false;$prior_ids[]=(int)$prior['reviewer_id'];}return pay_ctry_004_canonical_hash(array('reviewer_id'=>$id,'prior_reviewer_ids'=>$prior_ids));
}
function pay_ctry_004_review_decision_fields(array $review)
{
    return array('review_type'=>isset($review['review_type'])?(string)$review['review_type']:'','reviewer_id'=>isset($review['reviewer_id'])?(int)$review['reviewer_id']:0,'reviewer_role'=>isset($review['reviewer_role'])?(string)$review['reviewer_role']:'','independence_sha256'=>isset($review['independence_sha256'])?(string)$review['independence_sha256']:'','decision'=>isset($review['decision'])?(string)$review['decision']:'','reason_code'=>isset($review['reason_code'])?(string)$review['reason_code']:'','evidence_identity_sha256'=>isset($review['evidence_identity_sha256'])?(string)$review['evidence_identity_sha256']:'','semantic_root'=>isset($review['semantic_root'])?(string)$review['semantic_root']:'','evidence_root'=>isset($review['evidence_root'])?(string)$review['evidence_root']:'','decided_at'=>isset($review['decided_at'])?(string)$review['decided_at']:'');
}
function pay_ctry_004_review_decision_sha256(array $review){return pay_ctry_004_canonical_hash(pay_ctry_004_review_decision_fields($review));}
function pay_ctry_004_validate_review(array $review, array $prior_reviews=array(), &$error=null)
{
    $error=null;$roles=pay_ctry_004_review_roles();
    foreach(array('review_type','reviewer_id','reviewer_role','independence_sha256','decision','reason_code','evidence_identity_sha256','semantic_root','evidence_root','decision_sha256','decided_at') as $k)if(!isset($review[$k])){$error='review_field_missing';return false;}
    $type=(string)$review['review_type'];if(!isset($roles[$type])||(string)$review['reviewer_role']!==$roles[$type]){$error='review_role_invalid';return false;}
    $id=pay_ctry_004_uint($review['reviewer_id'],false);if($id===false){$error='reviewer_invalid';return false;}
    if(!in_array((string)$review['decision'],array('accepted','rejected'),true)){$error='review_decision_invalid';return false;}
    if(!preg_match('/^[A-Za-z0-9._:-]{1,64}$/D',(string)$review['reason_code'])){$error='review_reason_invalid';return false;}
    if(pay_ctry_004_datetime_epoch((string)$review['decided_at'])===false){$error='review_time_invalid';return false;}
    foreach(array('evidence_identity_sha256','semantic_root','evidence_root') as $k)if(pay_ctry_004_sha256((string)$review[$k])===false){$error='review_digest_invalid';return false;}
    foreach($prior_reviews as $prior)if(is_array($prior)&&isset($prior['reviewer_id'])&&(int)$prior['reviewer_id']===$id){$error='reviewer_independence_violation';return false;}
    $independence=pay_ctry_004_review_independence_sha256($id,$prior_reviews);if($independence===false||pay_ctry_004_sha256((string)$review['independence_sha256'])===false||(string)$review['independence_sha256']!==$independence){$error='review_independence_evidence_invalid';return false;}
    if(pay_ctry_004_sha256((string)$review['decision_sha256'])===false||(string)$review['decision_sha256']!==pay_ctry_004_review_decision_sha256($review)){$error='review_decision_digest_mismatch';return false;}
    return true;
}
function pay_ctry_004_validate_parallel_run(array $run, &$error=null)
{
    $error=null;foreach(array('semantic_root','evidence_root','runner_identity','test_identity','test_version','fixture_identity','result_digest','expected_digest','observed_digest','acceptance_criteria_sha256','executed_at','evidence_sha256') as $k)if(!isset($run[$k])||(string)$run[$k]===''){$error='parallel_field_missing';return false;}
    foreach(array('semantic_root','evidence_root','result_digest','expected_digest','observed_digest','acceptance_criteria_sha256') as $k)if(pay_ctry_004_sha256((string)$run[$k])===false){$error='parallel_digest_invalid';return false;}
    if(pay_ctry_004_datetime_epoch((string)$run['executed_at'])===false){$error='parallel_time_invalid';return false;}
    if(!array_key_exists('accepted',$run)||!in_array($run['accepted'],array(true,false,1,0,'1','0'),true)){$error='parallel_acceptance_missing';return false;}
    if(empty($run['accepted'])){$error='parallel_run_not_accepted';return false;}
    if(!empty($run['synthetic']) && empty($run['engineering_only'])){$error='synthetic_parallel_must_be_engineering_only';return false;}
    if(pay_ctry_004_sha256((string)$run['evidence_sha256'])===false||(string)$run['evidence_sha256']!==pay_ctry_004_parallel_evidence_sha256($run)){$error='parallel_evidence_digest_mismatch';return false;}
    return true;
}
/** Return the canonical fields bound by a parallel-run evidence digest. */
function pay_ctry_004_parallel_evidence_fields(array $run)
{
    return array('semantic_root'=>(string)$run['semantic_root'],'evidence_root'=>(string)$run['evidence_root'],'runner_identity'=>(string)$run['runner_identity'],'test_identity'=>(string)$run['test_identity'],'test_version'=>(string)$run['test_version'],'fixture_identity'=>(string)$run['fixture_identity'],'result_digest'=>(string)$run['result_digest'],'expected_digest'=>(string)$run['expected_digest'],'observed_digest'=>(string)$run['observed_digest'],'acceptance_criteria_sha256'=>(string)$run['acceptance_criteria_sha256'],'executed_at'=>(string)$run['executed_at'],'accepted'=>empty($run['accepted'])?0:1,'synthetic'=>empty($run['synthetic'])?0:1,'engineering_only'=>empty($run['engineering_only'])?0:1);
}
/** Return the deterministic custody digest for parallel-run evidence. */
function pay_ctry_004_parallel_evidence_sha256(array $run){return pay_ctry_004_canonical_hash(pay_ctry_004_parallel_evidence_fields($run));}
function pay_ctry_004_claim_scope_fields(array $claim)
{
    return array(
        'jurisdiction'=>isset($claim['jurisdiction'])?(string)$claim['jurisdiction']:'',
        'scope_type'=>isset($claim['scope_type'])?(string)$claim['scope_type']:'jurisdiction',
        'company_id'=>isset($claim['company_id'])&&$claim['company_id']!==null?(string)$claim['company_id']:'',
        'legal_entity_id'=>isset($claim['legal_entity_id'])&&$claim['legal_entity_id']!==null?(string)$claim['legal_entity_id']:'',
        'payroll_scope_key'=>isset($claim['payroll_scope_key'])?(string)$claim['payroll_scope_key']:'',
        'effective_from'=>isset($claim['effective_from'])?(string)$claim['effective_from']:'',
        'effective_to'=>isset($claim['effective_to'])?(string)$claim['effective_to']:'',
        'payroll_type'=>isset($claim['payroll_type'])?(string)$claim['payroll_type']:'',
        'employee_category'=>isset($claim['employee_category'])?(string)$claim['employee_category']:'',
        'filing_capability'=>isset($claim['filing_capability'])?(string)$claim['filing_capability']:'',
        'payment_capability'=>isset($claim['payment_capability'])?(string)$claim['payment_capability']:'',
        'host_version'=>isset($claim['host_version'])?(string)$claim['host_version']:'',
        'capability_registry_version'=>isset($claim['capability_registry_version'])?(string)$claim['capability_registry_version']:'',
        'limitations'=>isset($claim['limitations'])?(string)$claim['limitations']:''
    );
}
function pay_ctry_004_validate_claim(array $claim, &$error=null)
{
    $error=null;$f=pay_ctry_004_claim_scope_fields($claim);
    if($f['jurisdiction']===''||$f['effective_from']===''||$f['effective_to']===''||$f['host_version']===''||$f['capability_registry_version']===''){$error='claim_scope_incomplete';return false;}
    if(!in_array($f['scope_type'],array('jurisdiction','company','legal_entity','payroll_scope'),true)){$error='claim_scope_type_invalid';return false;}
    if($f['scope_type']==='jurisdiction'&&($f['company_id']!==''||$f['legal_entity_id']!==''||$f['payroll_scope_key']!=='')){$error='claim_scope_shape_invalid';return false;}
    if($f['scope_type']==='company'&&pay_ctry_004_uint($f['company_id'],true)===false){$error='claim_company_scope_invalid';return false;}
    if($f['scope_type']==='legal_entity'&&(pay_ctry_004_uint($f['company_id'],true)===false||pay_ctry_004_uint($f['legal_entity_id'],false)===false)){$error='claim_legal_entity_scope_invalid';return false;}
    if($f['scope_type']==='payroll_scope'&&(pay_ctry_004_uint($f['company_id'],true)===false||$f['payroll_scope_key']===''||strlen($f['payroll_scope_key'])>96)){$error='claim_payroll_scope_invalid';return false;}
    $from=pay_ctry_004_datetime_epoch($f['effective_from']);$to=pay_ctry_004_datetime_epoch($f['effective_to']);if($from===false||$to===false||$to<=$from){$error='claim_period_invalid';return false;}
    if($f['capability_registry_version']!==PAY_CTRY_004_CAPABILITY_REGISTRY_VERSION){$error='claim_registry_mismatch';return false;}
    foreach(array('all','all-laws','global-compliance','compliant-with-all-laws') as $forbidden)if(strtolower(trim($f['jurisdiction']))===$forbidden){$error='claim_scope_overbroad';return false;}
    return true;
}
function pay_ctry_004_claim_scope_sha256(array $claim, &$error=null)
{
    if(!pay_ctry_004_validate_claim($claim,$error))return false;return pay_ctry_004_canonical_hash(pay_ctry_004_claim_scope_fields($claim));
}
function pay_ctry_004_claim_allows(array $claim,array $context,$at,&$error=null)
{
    if(!pay_ctry_004_validate_claim($claim,$error))return false;$f=pay_ctry_004_claim_scope_fields($claim);$now=pay_ctry_004_datetime_epoch((string)$at);$from=pay_ctry_004_datetime_epoch($f['effective_from']);$to=pay_ctry_004_datetime_epoch($f['effective_to']);
    if($now===false||$now<$from||$now>=$to){$error='claim_outside_effective_period';return false;}
    if(!isset($context['jurisdiction'])||(string)$context['jurisdiction']!==$f['jurisdiction']){$error='claim_jurisdiction_mismatch';return false;}
    if($f['scope_type']!=='jurisdiction'&&(!isset($context['company_id'])||(int)$context['company_id']!==(int)$f['company_id'])){$error='claim_company_mismatch';return false;}
    if($f['scope_type']==='legal_entity'&&(!isset($context['legal_entity_id'])||(int)$context['legal_entity_id']!==(int)$f['legal_entity_id'])){$error='claim_legal_entity_mismatch';return false;}
    if($f['scope_type']==='payroll_scope'&&(!isset($context['payroll_scope_key'])||(string)$context['payroll_scope_key']!==$f['payroll_scope_key'])){$error='claim_payroll_scope_mismatch';return false;}
    foreach(array('payroll_type','employee_category','filing_capability','payment_capability') as $k)if($f[$k]!==''&&(!isset($context[$k])||(string)$context[$k]!==$f[$k])){$error='claim_dimension_mismatch';return false;}
    if(!isset($context['host_version'])||(string)$context['host_version']!==$f['host_version']){$error='claim_host_version_mismatch';return false;}
    if(!isset($context['capability_registry_version'])||(string)$context['capability_registry_version']!==$f['capability_registry_version']){$error='claim_registry_mismatch';return false;}
    return true;
}
function pay_ctry_004_independent_principals(array $principal_ids,&$error=null)
{
    $error=null;$seen=array();foreach($principal_ids as $id){$id=pay_ctry_004_uint($id,false);if($id===false){$error='principal_invalid';return false;}if(isset($seen[$id])){$error='role_independence_violation';return false;}$seen[$id]=true;}return count($seen)===count($principal_ids);
}
function pay_ctry_004_validate_review_chain(array $reviews,$semantic_root,$evidence_root,&$error=null)
{
    $error=null;$expected=array('technical','payroll_expert','legal');if(count($reviews)!==3){$error='review_chain_incomplete';return false;}$prior=array();$ids=array();
    foreach($expected as $i=>$type){$review=$reviews[$i];if(!is_array($review)||(isset($review['review_type'])?(string)$review['review_type']:'')!==$type){$error='review_chain_order_invalid';return false;}if(!pay_ctry_004_validate_review($review,$prior,$error))return false;if((string)$review['decision']!=='accepted'){$error='review_chain_rejected';return false;}if((string)$review['semantic_root']!==(string)$semantic_root||(string)$review['evidence_root']!==(string)$evidence_root){$error='review_chain_root_mismatch';return false;}$prior[]=$review;$ids[]=$review['reviewer_id'];}
    if(!pay_ctry_004_independent_principals($ids,$error))return false;return true;
}
function pay_ctry_004_review_chain_sha256(array $reviews,$semantic_root,$evidence_root,&$error=null)
{
    if(!pay_ctry_004_validate_review_chain($reviews,$semantic_root,$evidence_root,$error))return false;$canonical=array();foreach($reviews as $r)$canonical[]=array('review_type'=>(string)$r['review_type'],'reviewer_id'=>(int)$r['reviewer_id'],'reviewer_role'=>(string)$r['reviewer_role'],'decision'=>(string)$r['decision'],'evidence_identity_sha256'=>(string)$r['evidence_identity_sha256'],'semantic_root'=>(string)$r['semantic_root'],'evidence_root'=>(string)$r['evidence_root']);return pay_ctry_004_canonical_hash($canonical);
}
function pay_ctry_004_effective_status($status,$expires_at,$at)
{
    if(!in_array($status,pay_ctry_004_lifecycle_states(),true))return false;
    if(in_array($status,array('Revoked','Suspended','Superseded'),true))return $status;
    $expiry=pay_ctry_004_datetime_epoch((string)$expires_at);$now=pay_ctry_004_datetime_epoch((string)$at);
    if($expiry!==false&&$now!==false&&$now>=$expiry&&in_array($status,array('Certified','Published'),true))return 'Expired';
    return $status;
}
function pay_ctry_004_validate_transition($from,$to,array $context,&$error=null)
{
    $error=null;$allowed=pay_ctry_004_allowed_transitions();if(!isset($allowed[$from])||!in_array($to,$allowed[$from],true)){$error='lifecycle_transition_invalid';return false;}
    $role=isset($context['actor_role'])?(string)$context['actor_role']:'';$roles=pay_ctry_004_roles();if(!in_array($role,$roles,true)){$error='actor_role_invalid';return false;}
    if($from==='Draft'&&$to==='TechnicallyValidated'&&($role!=='technical_validator'||empty($context['technical_review_accepted']))){$error='technical_review_required';return false;}
    if($from==='TechnicallyValidated'&&$to==='PayrollExpertReviewed'&&($role!=='payroll_expert'||empty($context['payroll_expert_review_accepted'])||empty($context['reviewer_independence']))){$error='independent_payroll_expert_review_required';return false;}
    if($from==='PayrollExpertReviewed'&&$to==='LegallyReviewed'&&($role!=='legal_reviewer'||empty($context['legal_review_accepted'])||empty($context['reviewer_independence']))){$error='independent_legal_review_required';return false;}
    if($from==='LegallyReviewed'&&$to==='ParallelRunPassed'&&($role!=='technical_validator'||empty($context['parallel_run_passed']))){$error='parallel_run_evidence_required';return false;}
    if($from==='ParallelRunPassed'&&$to==='Certified'){
        if($role!=='certification_authority'||empty($context['review_chain_complete'])||empty($context['reviewer_independence'])||empty($context['actor_independent'])||empty($context['step_up_verified'])||empty($context['roots_match'])||empty($context['claim_valid'])||empty($context['signed_certification_verified'])){$error='certification_prerequisite_missing';return false;}
        if(!isset($context['valid_from'],$context['expires_at'])||pay_ctry_004_datetime_epoch((string)$context['valid_from'])===false||pay_ctry_004_datetime_epoch((string)$context['expires_at'])===false||pay_ctry_004_datetime_epoch((string)$context['expires_at'])<=pay_ctry_004_datetime_epoch((string)$context['valid_from'])){$error='certification_validity_invalid';return false;}
    }
    if($from==='Certified'&&$to==='Published'&&($role!=='publisher'||empty($context['certification_current'])||empty($context['actor_independent'])||empty($context['step_up_verified'])||empty($context['roots_match'])||empty($context['claim_valid'])||empty($context['review_chain_complete']))){$error='publication_prerequisite_missing';return false;}
    if(in_array($to,array('Suspended','Revoked'),true)&&($role!=='emergency_authority'||empty($context['step_up_verified']))){$error='emergency_authority_required';return false;}
    if($from==='Suspended'&&$to==='Draft'&&($role!=='recovery_authority'||empty($context['recovery_reconciled'])||empty($context['new_certification_lineage']))){$error='governed_recertification_required';return false;}
    if($from==='Expired'&&$to==='Draft'&&($role!=='recovery_authority'||empty($context['new_certification_lineage']))){$error='expired_recertification_required';return false;}
    return true;
}
function pay_ctry_004_publication_guard(array $certification,array $claim,$at,&$error=null)
{
    $error=null;foreach(array('status','semantic_root','evidence_root','expected_semantic_root','expected_evidence_root','claim_scope_sha256','expires_at','review_chain_complete','signature_verified','suspended','revoked') as $k)if(!array_key_exists($k,$certification)){$error='certification_field_missing';return false;}
    if((string)$certification['status']!=='Certified'){$error='certification_not_certified';return false;}
    if(pay_ctry_004_sha256((string)$certification['semantic_root'])===false||$certification['semantic_root']!==$certification['expected_semantic_root']){$error='semantic_root_mismatch';return false;}
    if(pay_ctry_004_sha256((string)$certification['evidence_root'])===false||$certification['evidence_root']!==$certification['expected_evidence_root']){$error='evidence_root_mismatch';return false;}
    if(empty($certification['review_chain_complete'])){$error='review_chain_incomplete';return false;}if(empty($certification['signature_verified'])){$error='certification_signature_unverified';return false;}
    if(!empty($certification['suspended'])){$error='certification_suspended';return false;}if(!empty($certification['revoked'])){$error='certification_revoked';return false;}
    if(pay_ctry_004_datetime_epoch((string)$certification['expires_at'])===false){$error='certification_expiry_invalid';return false;}
    if(pay_ctry_004_effective_status('Certified',(string)$certification['expires_at'],(string)$at)==='Expired'){$error='certification_expired';return false;}
    if(!pay_ctry_004_validate_claim($claim,$error))return false;$claim_hash=pay_ctry_004_claim_scope_sha256($claim,$error);if($claim_hash===false||(string)$certification['claim_scope_sha256']!==$claim_hash){$error='claim_scope_mismatch';return false;}
    $now=pay_ctry_004_datetime_epoch((string)$at);$cf=pay_ctry_004_claim_scope_fields($claim);if($now===false||$now<pay_ctry_004_datetime_epoch($cf['effective_from'])||$now>=pay_ctry_004_datetime_epoch($cf['effective_to'])){$error='claim_outside_effective_period';return false;}
    return true;
}
function pay_ctry_004_validate_recertification(array $prior,array $next,&$error=null)
{
    $error=null;foreach(array('certification_id','decision_sha256','semantic_root','evidence_root') as $k)if(!isset($prior[$k])||!isset($next[$k])){$error='recertification_field_missing';return false;}
    if(pay_ctry_004_uint($prior['certification_id'],false)===false||!isset($next['predecessor_certification_id'])||(int)$next['predecessor_certification_id']!==(int)$prior['certification_id']){$error='recertification_lineage_invalid';return false;}
    if((string)$prior['decision_sha256']===(string)$next['decision_sha256']){$error='recertification_decision_reused';return false;}
    if(pay_ctry_004_sha256((string)$next['semantic_root'])===false||pay_ctry_004_sha256((string)$next['evidence_root'])===false){$error='recertification_root_invalid';return false;}
    if(empty($next['new_review_chain'])||empty($next['new_evidence_inventory'])){$error='recertification_new_evidence_required';return false;}if(empty($next['signed_certification_verified'])){$error='recertification_signature_required';return false;}
    return true;
}
function pay_ctry_004_future_execution_policy($certification_status,$activation_status,$expires_at,$at)
{
    $effective=pay_ctry_004_effective_status($certification_status,$expires_at,$at);
    if(!in_array($effective,array('Published'),true))return array('allowed'=>false,'reason'=>'certification_not_current','historical_read_allowed'=>true,'historical_rewrite_allowed'=>false);
    if(!in_array($activation_status,array('active','activated'),true))return array('allowed'=>false,'reason'=>'activation_not_current','historical_read_allowed'=>true,'historical_rewrite_allowed'=>false);
    return array('allowed'=>true,'reason'=>'current_certification_and_activation','historical_read_allowed'=>true,'historical_rewrite_allowed'=>false);
}
function pay_ctry_004_publication_events(){return array('Published','Superseded','Deprecated','SupportEnded');}
/** Return the canonical fields bound by a publication custody digest. */
function pay_ctry_004_publication_fields(array $event)
{
    return array('certification_event_id'=>isset($event['certification_event_id'])?(int)$event['certification_event_id']:0,'certification_claim_id'=>isset($event['certification_claim_id'])?(int)$event['certification_claim_id']:0,'publication_event'=>isset($event['publication_event'])?(string)$event['publication_event']:'','predecessor_publication_id'=>isset($event['predecessor_publication_id'])&&$event['predecessor_publication_id']!==null?(int)$event['predecessor_publication_id']:0,'published_at'=>isset($event['published_at'])?(string)$event['published_at']:'','support_until'=>isset($event['support_until'])&&$event['support_until']!==null?(string)$event['support_until']:'','actor_id'=>isset($event['actor_id'])?(int)$event['actor_id']:0,'actor_role'=>isset($event['actor_role'])?(string)$event['actor_role']:'','assurance_sha256'=>isset($event['assurance_sha256'])?(string)$event['assurance_sha256']:'');
}
/** Return the deterministic custody digest for a publication event. */
function pay_ctry_004_publication_sha256(array $event){return pay_ctry_004_canonical_hash(pay_ctry_004_publication_fields($event));}
function pay_ctry_004_validate_publication_event(array $event,&$error=null)
{
    $error=null;foreach(array('certification_event_id','certification_claim_id','publication_event','published_at','actor_id','actor_role','assurance_sha256','publication_sha256') as $k)if(!isset($event[$k])){$error='publication_field_missing';return false;}
    if(pay_ctry_004_uint($event['certification_event_id'],false)===false||pay_ctry_004_uint($event['certification_claim_id'],false)===false||pay_ctry_004_uint($event['actor_id'],false)===false||!in_array((string)$event['actor_role'],array('publisher'),true)){$error='publication_identity_invalid';return false;}
    if(!in_array((string)$event['publication_event'],pay_ctry_004_publication_events(),true)){$error='publication_event_invalid';return false;}
    if(!isset($event['publication_sha256'])||pay_ctry_004_sha256((string)$event['publication_sha256'])===false){$error='publication_digest_invalid';return false;}
    if(!isset($event['published_at'])||pay_ctry_004_datetime_epoch((string)$event['published_at'])===false){$error='publication_time_invalid';return false;}
    if(isset($event['support_until'])&&$event['support_until']!==null&&$event['support_until']!==''&&pay_ctry_004_datetime_epoch((string)$event['support_until'])===false){$error='support_until_invalid';return false;}
    if(pay_ctry_004_sha256((string)$event['assurance_sha256'])===false||(string)$event['publication_sha256']!==pay_ctry_004_publication_sha256($event)){$error='publication_digest_mismatch';return false;}
    return true;
}
/** Return the canonical fields bound by an emergency custody digest. */
function pay_ctry_004_emergency_action_fields(array $action)
{
    return array('certification_event_id'=>isset($action['certification_event_id'])?(int)$action['certification_event_id']:0,'certification_claim_id'=>isset($action['certification_claim_id'])&&$action['certification_claim_id']!==null?(int)$action['certification_claim_id']:0,'action_type'=>isset($action['action_type'])?(string)$action['action_type']:'','effective_at'=>isset($action['effective_at'])?(string)$action['effective_at']:'','reason_code'=>isset($action['reason_code'])?(string)$action['reason_code']:'','reason_sha256'=>isset($action['reason_sha256'])?(string)$action['reason_sha256']:'','affected_scope_sha256'=>isset($action['affected_scope_sha256'])?(string)$action['affected_scope_sha256']:'','follow_up_required'=>empty($action['follow_up_required'])?0:1,'assurance_event_id'=>isset($action['assurance_event_id'])&&$action['assurance_event_id']!==null?(int)$action['assurance_event_id']:0,'acted_by'=>isset($action['acted_by'])?(int)$action['acted_by']:0,'actor_role'=>isset($action['actor_role'])?(string)$action['actor_role']:'');
}
/** Return the deterministic custody digest for an emergency action. */
function pay_ctry_004_emergency_action_sha256(array $action){return pay_ctry_004_canonical_hash(pay_ctry_004_emergency_action_fields($action));}
function pay_ctry_004_validate_emergency_action(array $action,&$error=null)
{
    $error=null;foreach(array('certification_event_id','action_type','effective_at','reason_code','reason_sha256','affected_scope_sha256','follow_up_required','action_sha256','acted_by','actor_role') as $k)if(!isset($action[$k])){$error='emergency_field_missing';return false;}if(!in_array((string)$action['action_type'],array('suspend','revoke'),true)){$error='emergency_action_invalid';return false;}
    if(pay_ctry_004_uint($action['certification_event_id'],false)===false){$error='emergency_identity_invalid';return false;}
    if(!isset($action['actor_role'])||(string)$action['actor_role']!=='emergency_authority'||!isset($action['acted_by'])||pay_ctry_004_uint($action['acted_by'],false)===false){$error='emergency_authority_required';return false;}
    if(empty($action['step_up_verified'])){$error='emergency_step_up_required';return false;}
    foreach(array('reason_sha256','affected_scope_sha256','action_sha256') as $k)if(pay_ctry_004_sha256((string)$action[$k])===false){$error='emergency_digest_invalid';return false;}
    if(!isset($action['effective_at'])||pay_ctry_004_datetime_epoch((string)$action['effective_at'])===false||empty($action['follow_up_required'])){$error='emergency_follow_up_required';return false;}
    if(pay_ctry_004_sha256((string)$action['action_sha256'])===false||(string)$action['action_sha256']!==pay_ctry_004_emergency_action_sha256($action)){$error='emergency_digest_mismatch';return false;}
    return true;
}
/** Return the canonical fields bound by a RACI custody digest. */
function pay_ctry_004_raci_assignment_fields(array $assignment)
{
    return array('governance_role'=>isset($assignment['governance_role'])?(string)$assignment['governance_role']:'','principal_id'=>isset($assignment['principal_id'])?(int)$assignment['principal_id']:0,'responsibility'=>isset($assignment['responsibility'])?(string)$assignment['responsibility']:'','jurisdiction'=>isset($assignment['jurisdiction'])?(string)$assignment['jurisdiction']:'','valid_from'=>isset($assignment['valid_from'])?(string)$assignment['valid_from']:'','valid_to'=>isset($assignment['valid_to'])&&$assignment['valid_to']!==null?(string)$assignment['valid_to']:'','assigned_by'=>isset($assignment['assigned_by'])?(int)$assignment['assigned_by']:0);
}
/** Return the deterministic custody digest for a RACI assignment. */
function pay_ctry_004_raci_assignment_sha256(array $assignment){return pay_ctry_004_canonical_hash(pay_ctry_004_raci_assignment_fields($assignment));}
function pay_ctry_004_validate_raci(array $assignment,&$error=null)
{
    $error=null;foreach(array('governance_role','principal_id','responsibility','jurisdiction','valid_from','assignment_sha256','assigned_by') as $k)if(!isset($assignment[$k])){$error='raci_field_missing';return false;}if(!in_array((string)$assignment['governance_role'],pay_ctry_004_roles(),true)){$error='raci_role_invalid';return false;}
    if(!isset($assignment['principal_id'])||pay_ctry_004_uint($assignment['principal_id'],false)===false){$error='raci_principal_invalid';return false;}
    if(!isset($assignment['responsibility'])||!in_array((string)$assignment['responsibility'],array('R','A','C','I'),true)){$error='raci_responsibility_invalid';return false;}
    if(!isset($assignment['jurisdiction'])||trim((string)$assignment['jurisdiction'])===''||strlen((string)$assignment['jurisdiction'])>64){$error='raci_scope_invalid';return false;}
    if(!isset($assignment['valid_from'])||pay_ctry_004_datetime_epoch((string)$assignment['valid_from'])===false){$error='raci_period_invalid';return false;}if(isset($assignment['valid_to'])&&$assignment['valid_to']!==null&&$assignment['valid_to']!==''&&(pay_ctry_004_datetime_epoch((string)$assignment['valid_to'])===false||pay_ctry_004_datetime_epoch((string)$assignment['valid_to'])<=pay_ctry_004_datetime_epoch((string)$assignment['valid_from']))){$error='raci_period_invalid';return false;}
    if(pay_ctry_004_uint($assignment['assigned_by'],false)===false){$error='raci_assigner_invalid';return false;}
    if(pay_ctry_004_sha256((string)$assignment['assignment_sha256'])===false||(string)$assignment['assignment_sha256']!==pay_ctry_004_raci_assignment_sha256($assignment)){$error='raci_digest_mismatch';return false;}
    return true;
}
function pay_ctry_004_reconciliation_sha256(array $record,&$error=null)
{
    $error=null;foreach(array('certification_event_id','recovery_type','observed_state_sha256','expected_state_sha256','decision','reason_code','recovered_by') as $k)if(!isset($record[$k])){$error='recovery_field_missing';return false;}
    if(pay_ctry_004_uint($record['certification_event_id'],false)===false||pay_ctry_004_uint($record['recovered_by'],false)===false){$error='recovery_identity_invalid';return false;}
    if(pay_ctry_004_sha256((string)$record['observed_state_sha256'])===false||pay_ctry_004_sha256((string)$record['expected_state_sha256'])===false){$error='recovery_digest_invalid';return false;}
    if(!in_array((string)$record['decision'],array('reconciled','quarantined','recertify'),true)){$error='recovery_decision_invalid';return false;}
    return pay_ctry_004_canonical_hash(array('certification_event_id'=>(int)$record['certification_event_id'],'recovery_type'=>(string)$record['recovery_type'],'observed_state_sha256'=>(string)$record['observed_state_sha256'],'expected_state_sha256'=>(string)$record['expected_state_sha256'],'decision'=>(string)$record['decision'],'reason_code'=>(string)$record['reason_code'],'recovered_by'=>(int)$record['recovered_by']));
}
function pay_ctry_004_authority_contract()
{
    return array(
        'software_self_certification_allowed'=>false,
        'installation_implies_certification'=>false,
        'activation_implies_certification'=>false,
        'certification_implies_activation'=>false,
        'synthetic_evidence_implies_legal_approval'=>false,
        'developer_approval_implies_expert_review'=>false,
        'payroll_posting_authority_allowed'=>false,
        'gl_authority_allowed'=>false,
        'payment_authority_allowed'=>false,
        'bank_account_mutation_allowed'=>false,
        'employee_mutation_allowed'=>false,
        'identity_security_admin_allowed'=>false,
        'cross_company_authority_allowed'=>false,
        'historical_payroll_accounting_rewrite_allowed'=>false,
        'arbitrary_php_sql_execution_allowed'=>false
    );
}
function pay_ctry_004_descriptor()
{
    return array(
        'contract_version'=>PAY_CTRY_004_CONTRACT_VERSION,
        'source_database_version'=>PAY_CTRY_004_SOURCE_DATABASE_VERSION,
        'source_custody_version'=>PAY_CTRY_004_SOURCE_CUSTODY_VERSION,
        'evidence_root_version'=>PAY_CTRY_004_EVIDENCE_ROOT_VERSION,
        'capability_registry_version'=>PAY_CTRY_004_CAPABILITY_REGISTRY_VERSION,
        'signature_algorithm'=>PAY_CTRY_004_SIGNATURE_ALGORITHM,
        'states'=>pay_ctry_004_lifecycle_states(),
        'roles'=>pay_ctry_004_roles(),
        'role_permissions'=>pay_ctry_004_role_permissions(),
        'authority'=>pay_ctry_004_authority_contract()
    );
}
?>
