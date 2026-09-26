<?php
/** PAY-CTRY-007 immutable predecessor custody and dependency readiness. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
include_once($path_to_root.'/includes/pay_ctry_007_contract.inc');
function pay_ctry_007_fail_patch($patch,$message){return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,$message);}
function pay_ctry_007_predecessor_files(){return array(
 'hrm/country_packs/astervale_conformance/1.0.0/package-candidate.json'=>'dcef593f49706042f0ba171eb1d05a09520d869dd4738cb19f091a0f07dd8abb',
 'hrm/country_packs/astervale_conformance/1.0.0/custody.json'=>'bb0ee184f91508504196c65cc4eaef38a92c9723e1420a0d77e39d83a9d15072',
 'hrm/includes/db/pay_ctry_006_readiness_db.inc'=>'019de58a0094d960c60fac3c6d63d78ea56edd989efe889da54afbae4310b023',
 'sql/upgrade_pay_ctry_006_proof_3_portability_completion.php'=>'50cc62b72cb0fe7876594298402803970d1e6662fa048b18824ec2dbce2f2dcf',
 'Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/EXECUTION_REPORT_PAY_CTRY_006_COMPLETION_2026-09-25.md'=>'94834a23ed8183e275d1b2375e649b7d414043205a105e9937dc1aa57ca699e3');}
function pay_ctry_007_require_predecessor($patch){global $path_to_root;foreach(pay_ctry_007_predecessor_files() as $rel=>$sha){$file=$path_to_root.'/'.$rel;if(!is_file($file)||!hash_equals($sha,hash_file('sha256',$file)))return pay_ctry_007_fail_patch($patch,'PAY-CTRY-007 immutable PAY-CTRY-006 predecessor custody mismatch: '.$rel.'.');}return true;}
function pay_ctry_007_require_dependencies($patch){global $path_to_root;$checks=array(
 'includes/pay_ctry_001_verifier.inc'=>array('function pay_ctry_001_validate_trust_and_signature','function pay_ctry_001_canonical_json'),
 'hrm/includes/db/pay_ctry_002_runtime_db.inc'=>array('function pay_ctry_002_append_outbox_intent','function pay_ctry_002_deliver_outbox_event'),
 'includes/pay_ctry_004_contract.inc'=>array('function pay_ctry_004_verify_certification_signature','function pay_ctry_004_publication_guard'),
 'includes/notification_manager.inc'=>array('interface NotificationProvider','class NotificationManager'));
 foreach($checks as $rel=>$tokens){$file=$path_to_root.'/'.$rel;$raw=@file_get_contents($file);if(!is_string($raw)||$raw==='')return pay_ctry_007_fail_patch($patch,'PAY-CTRY-007 dependency source missing: '.$rel.'.');foreach($tokens as $token)if(strpos($raw,$token)===false)return pay_ctry_007_fail_patch($patch,'PAY-CTRY-007 dependency source/custody contract incomplete: '.$rel.' missing '.$token.'.');}
 return true;}
function pay_ctry_007_require_contract($patch){$a=pay_ctry_007_authority_contract();foreach($a as $name=>$allowed)if($allowed!==false)return pay_ctry_007_fail_patch($patch,'PAY-CTRY-007 authority ceiling violated: '.$name.'.');return true;}

function pay_ctry_007_require_permissions($patch){global $path_to_root;$raw=@file_get_contents($path_to_root.'/includes/access_levels.inc');if(!is_string($raw)||$raw==='')return pay_ctry_007_fail_patch($patch,'PAY-CTRY-007 permission source missing.');foreach(array(PAY_CTRY_007_MANAGE_PERMISSION,PAY_CTRY_007_ACK_PERMISSION,PAY_CTRY_007_ESCALATE_PERMISSION,PAY_CTRY_007_RESOLVE_PERMISSION,PAY_CTRY_007_VIEW_PERMISSION,PAY_CTRY_007_DELIVER_PERMISSION,'no automatic role grant') as $token)if(strpos($raw,$token)===false)return pay_ctry_007_fail_patch($patch,'PAY-CTRY-007 least-privilege permission source incomplete: '.$token.'.');return true;}
function pay_ctry_007_require_recipient_runtime($patch){global $path_to_root;$raw=@file_get_contents($path_to_root.'/hrm/includes/db/pay_ctry_007_notification_db.inc');if(!is_string($raw)||$raw==='')return pay_ctry_007_fail_patch($patch,'PAY-CTRY-007 recipient runtime source missing.');foreach(array('function pay_ctry_007_create_notice_audience','function pay_ctry_007_append_recipient_event','function pay_ctry_007_acknowledge_notice','function pay_ctry_007_append_notice_lifecycle_event','append_domain_audit_event','external delivery remains denied') as $token)if(strpos($raw,$token)===false)return pay_ctry_007_fail_patch($patch,'PAY-CTRY-007 recipient runtime source/custody contract incomplete: '.$token.'.');return true;}

function pay_ctry_007_require_delivery_contract($patch){$a=pay_ctry_007_delivery_authority_contract();$required=array('append_only_delivery_outbox'=>true,'in_app_notification_provider'=>true,'scoped_delivery_dispatch'=>true,'retry_dead_letter_evidence'=>true,'external_network_delivery'=>false,'automatic_pack_activation'=>false,'employee_fact_payload'=>false,'cross_company_delivery'=>false);foreach($required as $name=>$expected)if(!array_key_exists($name,$a)||$a[$name]!==$expected)return pay_ctry_007_fail_patch($patch,'PAY-CTRY-007 delivery authority contract mismatch: '.$name.'.');return true;}
function pay_ctry_007_require_delivery_runtime($patch){global $path_to_root;$checks=array(
 'hrm/includes/db/pay_ctry_007_notification_db.inc'=>array('function pay_ctry_007_enqueue_notice_delivery','function pay_ctry_007_dispatch_notice_delivery','function pay_ctry_007_get_visible_deliveries','external network delivery remains denied'),
 'includes/pay_ctry_007_notification_provider.inc'=>array('class PayCtry007NotificationProvider implements NotificationProvider',"return 'pay_ctry_update'",'PAY_CTRY_007_VIEW_PERMISSION'),
 'includes/session.inc'=>array("includes/pay_ctry_007_notification_provider.inc",'new PayCtry007NotificationProvider('),
 'hrm/inquiry/pay_ctry_update_notices.php'=>array("SA_HRM_VIEW_PAY_CTRY_UPDATE",'pay_ctry_007_get_visible_deliveries'));
 foreach($checks as $rel=>$tokens){$raw=@file_get_contents($path_to_root.'/'.$rel);if(!is_string($raw)||$raw==='')return pay_ctry_007_fail_patch($patch,'PAY-CTRY-007 delivery runtime source missing: '.$rel.'.');foreach($tokens as $token)if(strpos($raw,$token)===false)return pay_ctry_007_fail_patch($patch,'PAY-CTRY-007 delivery runtime source/custody contract incomplete: '.$rel.' missing '.$token.'.');}return true;}
?>
