<?php
/** PAY-API-001 governed provisioning and atomic read-rate runtime service. */
include_once($path_to_root.'/includes/pay_api_001_provisioning.inc');
include_once($path_to_root.'/includes/pay_api_001_rate_limit.inc');
include_once(dirname(__FILE__).'/pay_api_001_schema_db.inc');
include_once($path_to_root.'/includes/pay_api_001_contract.inc');

function pay_api_001_runtime_actor(){return isset($_SESSION['wa_current_user']->user)?(int)$_SESSION['wa_current_user']->user:0;}
function pay_api_001_runtime_company(){return isset($_SESSION['wa_current_user']->company)?pay_api_001_uint_value($_SESSION['wa_current_user']->company,true,4294967295):false;}
function pay_api_001_runtime_require_company($company,&$error){$current=pay_api_001_runtime_company();if($current===false||$current!==pay_api_001_uint_value($company,true,4294967295)){$error='company_scope_denied';return false;}return true;}
function pay_api_001_runtime_access($token){return function_exists('user_check_access')&&user_check_access($token);}
function pay_api_001_runtime_manage_transaction(){return empty($GLOBALS['transaction_level']);}
function pay_api_001_runtime_one($sql,$message){$q=db_query($sql,$message);return $q&&db_num_rows($q)===1?db_fetch_assoc($q):false;}
function pay_api_001_runtime_now(){return gmdate('Y-m-d H:i:s');}
/** Return the configured company database version for runtime authority checks. */
function pay_api_001_runtime_database_version(){if(!function_exists('get_company_pref'))return false;$v=get_company_pref('version_id',true);return is_string($v)?$v:false;}
/** Return whether the database version enables governed webhook configuration writes. */
function pay_api_001_runtime_webhook_configuration_enabled(){ $v=pay_api_001_runtime_database_version(); return $v!==false&&pay_api_001_webhook_configuration_writer_allowed_for_database_version($v); }
function pay_api_001_runtime_webhook_inbound_configuration_enabled(){ $v=pay_api_001_runtime_database_version(); return $v!==false&&pay_api_001_webhook_inbound_verifier_allowed_for_database_version($v); }

function pay_api_001_create_provision_request($target_kind,$company_id,array $payload,$reason_code,&$error=null)
{
    $error=null;$actor=pay_api_001_runtime_actor();if($actor<=0||!pay_api_001_runtime_access('SA_HRM_MANAGE_PAY_API')){$error='access_denied';return false;}
    if($target_kind==='webhook_inbound_source'&&!pay_api_001_runtime_webhook_inbound_configuration_enabled()){$error='webhook_inbound_configuration_writer_disabled';return false;}
    if(pay_api_001_provisioning_webhook_target_kind($target_kind)&&!pay_api_001_runtime_webhook_configuration_enabled()){$error='webhook_configuration_writer_disabled';return false;}
    $d=pay_api_001_provisioning_target_descriptor($target_kind,$company_id,$payload);if($d===false||!pay_api_001_runtime_require_company($d['company_id'],$error))return false;$reason=pay_api_001_provisioning_token($reason_code,64);if($reason===false){$error='provision_request_invalid';return false;}
    $request_sha=pay_api_001_provisioning_hash(array('company_id'=>$d['company_id'],'target_kind'=>$d['target_kind'],'target_key_sha256'=>$d['target_key_sha256'],'target_payload_sha256'=>$d['target_payload_sha256'],'reason_code'=>$reason,'requested_by'=>$actor));
    $q=db_query('SELECT provision_request_id,requested_by,target_payload_sha256 FROM '.TB_PREF.'hrm_pay_api_001_provision_requests WHERE request_sha256='.db_escape($request_sha).' LIMIT 2','could not read PAY-API-001 request replay');
    if($q&&db_num_rows($q)===1){$r=db_fetch_assoc($q);if((int)$r['requested_by']!==$actor||!hash_equals((string)$r['target_payload_sha256'],$d['target_payload_sha256'])){$error='provision_request_replay_mismatch';return false;}return array('provision_request_id'=>(int)$r['provision_request_id'],'request_sha256'=>$request_sha,'replayed'=>true);}
    if(!$q||db_num_rows($q)>1){$error='provision_request_state_invalid';return false;}
    $now=pay_api_001_runtime_now();$sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_provision_requests (company_id,target_kind,target_key_sha256,target_payload_sha256,reason_code,requested_by,requested_at,request_sha256,created_at) VALUES ('.(int)$d['company_id'].','.db_escape($d['target_kind']).','.db_escape($d['target_key_sha256']).','.db_escape($d['target_payload_sha256']).','.db_escape($reason).','.$actor.','.db_escape($now).','.db_escape($request_sha).','.db_escape($now).')';
    if(!db_query($sql)){ $error='provision_request_insert_failed';return false;}return array('provision_request_id'=>(int)db_insert_id(),'request_sha256'=>$request_sha,'replayed'=>false);
}

function pay_api_001_review_provision_request($request_id,$decision_code,&$error=null)
{
    $error=null;$actor=pay_api_001_runtime_actor();$request_id=pay_api_001_positive_id($request_id);if($actor<=0||!pay_api_001_runtime_access('SA_HRM_REVIEW_PAY_API')||$request_id===false||!in_array($decision_code,array('approve','reject'),true)){$error='access_or_review_invalid';return false;}
    $manage=pay_api_001_runtime_manage_transaction();if($manage)begin_transaction();
    $r=pay_api_001_runtime_one('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_provision_requests WHERE provision_request_id='.$request_id.' LIMIT 2 FOR UPDATE','could not lock PAY-API-001 provision request');if($r&&!pay_api_001_runtime_require_company($r['company_id'],$error)){if($manage)cancel_transaction();return false;}if(!$r||$actor===(int)$r['requested_by']){if($manage)cancel_transaction();$error=$r?'distinct_reviewer_required':'provision_request_missing';return false;}
    $q=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_provision_reviews WHERE provision_request_id='.$request_id.' LIMIT 2 FOR UPDATE');if(!$q||db_num_rows($q)>1){if($manage)cancel_transaction();$error='provision_review_state_invalid';return false;}
    $review_sha=pay_api_001_provisioning_hash(array('provision_request_id'=>$request_id,'decision_code'=>$decision_code,'request_sha256'=>$r['request_sha256'],'reviewed_by'=>$actor));
    if(db_num_rows($q)===1){$old=db_fetch_assoc($q);if((int)$old['reviewed_by']!==$actor||(string)$old['decision_code']!==$decision_code||!hash_equals((string)$old['review_sha256'],$review_sha)){if($manage)cancel_transaction();$error='provision_review_replay_mismatch';return false;}if($manage)commit_transaction();return array('provision_review_id'=>(int)$old['provision_review_id'],'review_sha256'=>$review_sha,'replayed'=>true);}
    $now=pay_api_001_runtime_now();$sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_provision_reviews (provision_request_id,decision_code,request_sha256,reviewed_by,reviewed_at,review_sha256,created_at) VALUES ('.$request_id.','.db_escape($decision_code).','.db_escape($r['request_sha256']).','.$actor.','.db_escape($now).','.db_escape($review_sha).','.db_escape($now).')';if(!db_query($sql)){if($manage)cancel_transaction();$error='provision_review_insert_failed';return false;}$id=(int)db_insert_id();if($manage)commit_transaction();return array('provision_review_id'=>$id,'review_sha256'=>$review_sha,'replayed'=>false);
}

function pay_api_001_approve_provision_request($request_id,&$error=null)
{
    $error=null;$actor=pay_api_001_runtime_actor();$request_id=pay_api_001_positive_id($request_id);if($actor<=0||!pay_api_001_runtime_access('SA_HRM_APPROVE_PAY_API')||$request_id===false){$error='access_or_approval_invalid';return false;}
    $manage=pay_api_001_runtime_manage_transaction();if($manage)begin_transaction();
    $r=pay_api_001_runtime_one('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_provision_requests WHERE provision_request_id='.$request_id.' LIMIT 2 FOR UPDATE','could not lock PAY-API-001 request for approval');if($r&&!pay_api_001_runtime_require_company($r['company_id'],$error)){if($manage)cancel_transaction();return false;}$v=pay_api_001_runtime_one('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_provision_reviews WHERE provision_request_id='.$request_id.' LIMIT 2 FOR UPDATE','could not lock PAY-API-001 review for approval');
    if(!$r||!$v||(string)$v['decision_code']!=='approve'||$actor===(int)$r['requested_by']||$actor===(int)$v['reviewed_by']){if($manage)cancel_transaction();$error='three_party_approval_required';return false;}
    $q=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_provision_approvals WHERE provision_request_id='.$request_id.' LIMIT 2 FOR UPDATE');if(!$q||db_num_rows($q)>1){if($manage)cancel_transaction();$error='provision_approval_state_invalid';return false;}
    $approval_sha=pay_api_001_provisioning_hash(array('provision_request_id'=>$request_id,'provision_review_id'=>(int)$v['provision_review_id'],'decision_code'=>'approve','request_sha256'=>$r['request_sha256'],'review_sha256'=>$v['review_sha256'],'approved_by'=>$actor));
    if(db_num_rows($q)===1){$old=db_fetch_assoc($q);if((int)$old['approved_by']!==$actor||!hash_equals((string)$old['approval_sha256'],$approval_sha)){if($manage)cancel_transaction();$error='provision_approval_replay_mismatch';return false;}if($manage)commit_transaction();return array('provision_approval_id'=>(int)$old['provision_approval_id'],'approval_sha256'=>$approval_sha,'replayed'=>true);}
    $now=pay_api_001_runtime_now();$sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_provision_approvals (provision_request_id,provision_review_id,decision_code,request_sha256,review_sha256,approved_by,approved_at,approval_sha256,created_at) VALUES ('.$request_id.','.(int)$v['provision_review_id'].',\'approve\','.db_escape($r['request_sha256']).','.db_escape($v['review_sha256']).','.$actor.','.db_escape($now).','.db_escape($approval_sha).','.db_escape($now).')';if(!db_query($sql)){if($manage)cancel_transaction();$error='provision_approval_insert_failed';return false;}$id=(int)db_insert_id();if($manage)commit_transaction();return array('provision_approval_id'=>$id,'approval_sha256'=>$approval_sha,'replayed'=>false);
}

function pay_api_001_runtime_insert_target($kind,$company,array $p,$reviewer,$approver,$approved_at,$executor,&$error)
{
    $now=pay_api_001_runtime_now();$sha=pay_api_001_provisioning_hash($p);$id=false;
    if($kind==='resource'){
        $sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_resources (resource_key,contract_version,resource_kind,company_scope_required,object_scope_required,read_capable,command_capable,resource_sha256,reviewed_by,approved_by,approved_at,created_at) VALUES ('.db_escape($p['resource_key']).','.db_escape($p['contract_version']).','.db_escape($p['resource_kind']).',1,'.(int)$p['object_scope_required'].',1,0,'.db_escape($sha).','.$reviewer.','.$approver.','.db_escape($approved_at).','.db_escape($now).')';
    }elseif($kind==='machine_principal'){
        if((int)$p['company_id']!==(int)$company){$error='principal_company_mismatch';return false;}$sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_machine_principals (company_id,principal_key,display_name,purpose_code,principal_status,principal_sha256,reviewed_by,approved_by,approved_at,created_at) VALUES ('.(int)$company.','.db_escape($p['principal_key']).','.db_escape($p['display_name']).','.db_escape($p['purpose_code']).',\'active\','.db_escape($sha).','.$reviewer.','.$approver.','.db_escape($approved_at).','.db_escape($now).')';
    }elseif($kind==='principal_scope'){
        $q=db_query('SELECT company_id,principal_status FROM '.TB_PREF.'hrm_pay_api_001_machine_principals WHERE api_principal_id='.(int)$p['api_principal_id'].' LIMIT 2 FOR UPDATE');$pr=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;$q2=db_query('SELECT read_capable,command_capable FROM '.TB_PREF.'hrm_pay_api_001_resources WHERE api_resource_id='.(int)$p['api_resource_id'].' LIMIT 2 FOR UPDATE');$rs=$q2&&db_num_rows($q2)===1?db_fetch_assoc($q2):false;if(!$pr||!$rs||(int)$pr['company_id']!==(int)$company||(string)$pr['principal_status']!=='active'||empty($rs['read_capable'])||!empty($rs['command_capable'])){$error='scope_principal_or_resource_invalid';return false;}$sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_principal_scopes (api_principal_id,api_resource_id,action_code,object_scope_type,object_scope_key,scope_sha256,reviewed_by,approved_by,approved_at,created_at) VALUES ('.(int)$p['api_principal_id'].','.(int)$p['api_resource_id'].',\'read\','.($p['object_scope_type']===null?'NULL':db_escape($p['object_scope_type'])).','.($p['object_scope_key']===null?'NULL':db_escape($p['object_scope_key'])).','.db_escape($sha).','.$reviewer.','.$approver.','.db_escape($approved_at).','.db_escape($now).')';
    }elseif($kind==='credential'){
        $q=db_query('SELECT company_id,principal_status FROM '.TB_PREF.'hrm_pay_api_001_machine_principals WHERE api_principal_id='.(int)$p['api_principal_id'].' LIMIT 2 FOR UPDATE');$pr=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;if(!$pr||(int)$pr['company_id']!==(int)$company||(string)$pr['principal_status']!=='active'){$error='credential_principal_invalid';return false;}$sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_credentials (api_principal_id,credential_key,verifier_scheme,secret_verifier_hash,valid_from,expires_at,credential_sha256,reviewed_by,approved_by,approved_at,created_at) VALUES ('.(int)$p['api_principal_id'].','.db_escape($p['credential_key']).',\'password_hash\','.db_escape($p['secret_verifier_hash']).','.db_escape($p['valid_from']).','.db_escape($p['expires_at']).','.db_escape($sha).','.$reviewer.','.$approver.','.db_escape($approved_at).','.db_escape($now).')';
    }elseif($kind==='credential_revocation'){
        $q=db_query('SELECT c.api_credential_id,p.company_id FROM '.TB_PREF.'hrm_pay_api_001_credentials c INNER JOIN '.TB_PREF.'hrm_pay_api_001_machine_principals p ON p.api_principal_id=c.api_principal_id WHERE c.api_credential_id='.(int)$p['api_credential_id'].' LIMIT 2 FOR UPDATE');$cr=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;if(!$cr||(int)$cr['company_id']!==(int)$company){$error='credential_revocation_scope_invalid';return false;}$sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_credential_revocations (api_credential_id,reason_code,revocation_sha256,revoked_by,revoked_at,created_at) VALUES ('.(int)$p['api_credential_id'].','.db_escape($p['reason_code']).','.db_escape($sha).','.$executor.','.db_escape($now).','.db_escape($now).')';
    }elseif($kind==='webhook_target'){
        if((int)$p['company_id']!==(int)$company){$error='webhook_target_company_mismatch';return false;}$sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_webhook_targets (company_id,target_key,event_type,contract_version,resource_key,object_scope_type,endpoint_config_ref_sha256,signing_key_ref_sha256,target_sha256,reviewed_by,approved_by,approved_at,created_at) VALUES ('.(int)$company.','.db_escape($p['target_key']).','.db_escape($p['event_type']).','.db_escape($p['contract_version']).','.db_escape($p['resource_key']).','.db_escape($p['object_scope_type']).','.db_escape($p['endpoint_config_ref_sha256']).','.db_escape($p['signing_key_ref_sha256']).','.db_escape($sha).','.$reviewer.','.$approver.','.db_escape($approved_at).','.db_escape($now).')';
    }elseif($kind==='webhook_target_lifecycle'){
        $t=pay_api_001_runtime_one('SELECT webhook_target_id,company_id FROM '.TB_PREF.'hrm_pay_api_001_webhook_targets WHERE webhook_target_id='.(int)$p['webhook_target_id'].' LIMIT 2 FOR UPDATE','could not lock webhook target lifecycle parent');if(!$t||(int)$t['company_id']!==(int)$company){$error='webhook_target_scope_invalid';return false;}
        $latest=pay_api_001_runtime_one('SELECT action_code FROM '.TB_PREF.'hrm_pay_api_001_webhook_target_lifecycle WHERE webhook_target_id='.(int)$p['webhook_target_id'].' ORDER BY effective_at DESC,webhook_target_lifecycle_id DESC LIMIT 1 FOR UPDATE','could not read webhook target lifecycle');$state=$latest?(string)$latest['action_code']:null;$action=$p['action_code'];
        if(($state===null&&$action!=='activate')||in_array($state,array('retire','supersede'),true)||($state==='activate'&&!in_array($action,array('suspend','retire','supersede'),true))||($state==='suspend'&&!in_array($action,array('activate','retire','supersede'),true))){$error='webhook_target_lifecycle_transition_denied';return false;}
        if($p['successor_target_id']!==null){$st=pay_api_001_runtime_one('SELECT webhook_target_id,company_id FROM '.TB_PREF.'hrm_pay_api_001_webhook_targets WHERE webhook_target_id='.(int)$p['successor_target_id'].' LIMIT 2 FOR UPDATE','could not lock webhook successor target');if(!$st||(int)$st['company_id']!==(int)$company){$error='webhook_successor_target_scope_invalid';return false;}}
        $sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_webhook_target_lifecycle (company_id,webhook_target_id,action_code,successor_target_id,reason_code,lifecycle_sha256,reviewed_by,approved_by,recorded_by,effective_at,created_at) VALUES ('.(int)$company.','.(int)$p['webhook_target_id'].','.db_escape($action).','.($p['successor_target_id']===null?'NULL':(int)$p['successor_target_id']).','.db_escape($p['reason_code']).','.db_escape($sha).','.$reviewer.','.$approver.','.$executor.','.db_escape($p['effective_at']).','.db_escape($now).')';
    }elseif($kind==='webhook_secret_ref'){
        $sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_webhook_secret_refs (company_id,ref_kind,provider_key,provider_reference_sha256,secret_ref_sha256,reviewed_by,approved_by,valid_from,created_at) VALUES ('.(int)$company.','.db_escape($p['ref_kind']).','.db_escape($p['provider_key']).','.db_escape($p['provider_reference_sha256']).','.db_escape($sha).','.$reviewer.','.$approver.','.db_escape($p['valid_from']).','.db_escape($now).')';
    }elseif($kind==='webhook_secret_rotation'){
        $a=pay_api_001_runtime_one('SELECT company_id,ref_kind FROM '.TB_PREF.'hrm_pay_api_001_webhook_secret_refs WHERE webhook_secret_ref_id='.(int)$p['previous_secret_ref_id'].' LIMIT 2 FOR UPDATE','could not lock prior webhook secret ref');$b=pay_api_001_runtime_one('SELECT company_id,ref_kind FROM '.TB_PREF.'hrm_pay_api_001_webhook_secret_refs WHERE webhook_secret_ref_id='.(int)$p['successor_secret_ref_id'].' LIMIT 2 FOR UPDATE','could not lock successor webhook secret ref');if(!$a||!$b||(int)$a['company_id']!==(int)$company||(int)$b['company_id']!==(int)$company||(string)$a['ref_kind']!=='webhook_signing_key'||(string)$b['ref_kind']!=='webhook_signing_key'){$error='webhook_secret_rotation_scope_invalid';return false;}$sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_webhook_secret_rotations (company_id,ref_kind,previous_secret_ref_id,successor_secret_ref_id,reason_code,rotation_sha256,reviewed_by,approved_by,effective_at,created_at) VALUES ('.(int)$company.',\'webhook_signing_key\','.(int)$p['previous_secret_ref_id'].','.(int)$p['successor_secret_ref_id'].','.db_escape($p['reason_code']).','.db_escape($sha).','.$reviewer.','.$approver.','.db_escape($p['effective_at']).','.db_escape($now).')';
    }elseif($kind==='webhook_delivery_policy'){
        $t=pay_api_001_runtime_one('SELECT company_id FROM '.TB_PREF.'hrm_pay_api_001_webhook_targets WHERE webhook_target_id='.(int)$p['webhook_target_id'].' LIMIT 2 FOR UPDATE','could not lock webhook delivery-policy target');if(!$t||(int)$t['company_id']!==(int)$company){$error='webhook_delivery_policy_scope_invalid';return false;}$sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_webhook_delivery_policies (company_id,webhook_target_id,policy_version,max_attempts,initial_backoff_seconds,max_backoff_seconds,lease_seconds,policy_sha256,reviewed_by,approved_by,approved_at,created_at) VALUES ('.(int)$company.','.(int)$p['webhook_target_id'].','.db_escape($p['policy_version']).','.(int)$p['max_attempts'].','.(int)$p['initial_backoff_seconds'].','.(int)$p['max_backoff_seconds'].','.(int)$p['lease_seconds'].','.db_escape($sha).','.$reviewer.','.$approver.','.db_escape($approved_at).','.db_escape($now).')';
    }elseif($kind==='webhook_egress_tls_policy'){
        $t=pay_api_001_runtime_one('SELECT company_id FROM '.TB_PREF.'hrm_pay_api_001_webhook_targets WHERE webhook_target_id='.(int)$p['webhook_target_id'].' LIMIT 2 FOR UPDATE','could not lock webhook egress target');if(!$t||(int)$t['company_id']!==(int)$company){$error='webhook_egress_policy_scope_invalid';return false;}$sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_webhook_egress_tls_policies (company_id,webhook_target_id,policy_version,destination_host,destination_port,scheme_code,minimum_tls_version,verify_peer_required,verify_hostname_required,policy_sha256,reviewed_by,approved_by,approved_at,created_at) VALUES ('.(int)$company.','.(int)$p['webhook_target_id'].','.db_escape($p['policy_version']).','.db_escape($p['destination_host']).',443,\'https\','.db_escape($p['minimum_tls_version']).',1,1,'.db_escape($sha).','.$reviewer.','.$approver.','.db_escape($approved_at).','.db_escape($now).')';
    }elseif($kind==='webhook_signing_profile'){
        $t=pay_api_001_runtime_one('SELECT company_id,signing_key_ref_sha256 FROM '.TB_PREF.'hrm_pay_api_001_webhook_targets WHERE webhook_target_id='.(int)$p['webhook_target_id'].' LIMIT 2 FOR UPDATE','could not lock webhook signing target');$r=pay_api_001_runtime_one('SELECT company_id,ref_kind,secret_ref_sha256 FROM '.TB_PREF.'hrm_pay_api_001_webhook_secret_refs WHERE webhook_secret_ref_id='.(int)$p['webhook_secret_ref_id'].' LIMIT 2 FOR UPDATE','could not lock webhook signing secret ref');if(!$t||!$r||(int)$t['company_id']!==(int)$company||(int)$r['company_id']!==(int)$company||(string)$r['ref_kind']!=='webhook_signing_key'||!hash_equals((string)$t['signing_key_ref_sha256'],(string)$r['secret_ref_sha256'])){$error='webhook_signing_profile_scope_invalid';return false;}$sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_webhook_signing_profiles (company_id,webhook_target_id,webhook_secret_ref_id,profile_version,canonicalization_code,signature_algorithm,signature_header_name,timestamp_header_name,profile_sha256,reviewed_by,approved_by,approved_at,created_at) VALUES ('.(int)$company.','.(int)$p['webhook_target_id'].','.(int)$p['webhook_secret_ref_id'].','.db_escape($p['profile_version']).','.db_escape($p['canonicalization_code']).','.db_escape($p['signature_algorithm']).','.db_escape($p['signature_header_name']).','.db_escape($p['timestamp_header_name']).','.db_escape($sha).','.$reviewer.','.$approver.','.db_escape($approved_at).','.db_escape($now).')';
    }elseif($kind==='webhook_inbound_source'){
        if(!pay_api_001_runtime_webhook_inbound_configuration_enabled()){$error='webhook_inbound_configuration_writer_disabled';return false;}$r=pay_api_001_runtime_one('SELECT company_id,ref_kind FROM '.TB_PREF.'hrm_pay_api_001_webhook_secret_refs WHERE webhook_secret_ref_id='.(int)$p['webhook_secret_ref_id'].' LIMIT 2 FOR UPDATE','could not lock inbound webhook secret ref');if(!$r||(int)$r['company_id']!==(int)$company||(string)$r['ref_kind']!=='webhook_signing_key'){$error='webhook_inbound_secret_scope_invalid';return false;}$sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_webhook_inbound_sources (company_id,source_key,profile_version,event_type,webhook_secret_ref_id,canonicalization_code,signature_algorithm,signature_header_name,timestamp_header_name,max_clock_skew_seconds,replay_ttl_seconds,valid_from,expires_at,source_sha256,reviewed_by,approved_by,approved_at,created_at) VALUES ('.(int)$company.','.db_escape($p['source_key']).','.db_escape($p['profile_version']).','.db_escape($p['event_type']).','.(int)$p['webhook_secret_ref_id'].','.db_escape($p['canonicalization_code']).','.db_escape($p['signature_algorithm']).','.db_escape($p['signature_header_name']).','.db_escape($p['timestamp_header_name']).','.(int)$p['max_clock_skew_seconds'].','.(int)$p['replay_ttl_seconds'].','.db_escape($p['valid_from']).','.db_escape($p['expires_at']).','.db_escape($sha).','.$reviewer.','.$approver.','.db_escape($approved_at).','.db_escape($now).')';
    }else{$error='provision_target_kind_unsupported';return false;}
    if(!db_query($sql)){if($error===null)$error='provision_target_insert_failed';return false;}$id=(int)db_insert_id();return array('target_record_id'=>$id,'target_sha256'=>$sha);
}

function pay_api_001_execute_provision_request($request_id,array $payload,&$error=null)
{
    $error=null;$actor=pay_api_001_runtime_actor();$request_id=pay_api_001_positive_id($request_id);if($actor<=0||!pay_api_001_runtime_access('SA_HRM_EXECUTE_PAY_API')||$request_id===false){$error='access_or_execution_invalid';return false;}
    $manage=pay_api_001_runtime_manage_transaction();if($manage)begin_transaction();
    $r=pay_api_001_runtime_one('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_provision_requests WHERE provision_request_id='.$request_id.' LIMIT 2 FOR UPDATE','could not lock PAY-API-001 request for execution');if($r&&!pay_api_001_runtime_require_company($r['company_id'],$error)){if($manage)cancel_transaction();return false;}if($r&&$r['target_kind']==='webhook_inbound_source'&&!pay_api_001_runtime_webhook_inbound_configuration_enabled()){if($manage)cancel_transaction();$error='webhook_inbound_configuration_writer_disabled';return false;}if($r&&pay_api_001_provisioning_webhook_target_kind($r['target_kind'])&&!pay_api_001_runtime_webhook_configuration_enabled()){if($manage)cancel_transaction();$error='webhook_configuration_writer_disabled';return false;}$v=pay_api_001_runtime_one('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_provision_reviews WHERE provision_request_id='.$request_id.' LIMIT 2 FOR UPDATE','could not lock PAY-API-001 review for execution');$a=pay_api_001_runtime_one('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_provision_approvals WHERE provision_request_id='.$request_id.' LIMIT 2 FOR UPDATE','could not lock PAY-API-001 approval for execution');
    $d=$r&&$v&&$a?pay_api_001_provision_execution_gate($r,$v,$a,$actor,$payload):false;if($d===false){if($manage)cancel_transaction();$error='provision_execution_gate_denied';return false;}
    $q=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_provision_executions WHERE provision_request_id='.$request_id.' LIMIT 2 FOR UPDATE');if(!$q||db_num_rows($q)>1){if($manage)cancel_transaction();$error='provision_execution_state_invalid';return false;}
    if(db_num_rows($q)===1){$old=db_fetch_assoc($q);if((int)$old['executor_id']!==$actor||!hash_equals((string)$old['target_payload_sha256'],$d['target_payload_sha256'])){if($manage)cancel_transaction();$error='provision_execution_replay_mismatch';return false;}if($manage)commit_transaction();return array('provision_execution_id'=>(int)$old['provision_execution_id'],'target_record_id'=>(int)$old['target_record_id'],'execution_sha256'=>$old['execution_sha256'],'replayed'=>true);}
    $t=pay_api_001_runtime_insert_target($d['target_kind'],$d['company_id'],$d['normalized_payload'],(int)$v['reviewed_by'],(int)$a['approved_by'],$a['approved_at'],$actor,$error);if($t===false){if($manage)cancel_transaction();return false;}
    $execution_sha=pay_api_001_provisioning_hash(array('provision_request_id'=>$request_id,'provision_approval_id'=>(int)$a['provision_approval_id'],'target_kind'=>$d['target_kind'],'target_record_id'=>$t['target_record_id'],'target_payload_sha256'=>$d['target_payload_sha256'],'executor_id'=>$actor));$now=pay_api_001_runtime_now();$sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_provision_executions (provision_request_id,provision_approval_id,target_kind,target_record_id,target_payload_sha256,executor_id,execution_sha256,executed_at,created_at) VALUES ('.$request_id.','.(int)$a['provision_approval_id'].','.db_escape($d['target_kind']).','.(int)$t['target_record_id'].','.db_escape($d['target_payload_sha256']).','.$actor.','.db_escape($execution_sha).','.db_escape($now).','.db_escape($now).')';if(!db_query($sql)){if($manage)cancel_transaction();$error='provision_execution_receipt_insert_failed';return false;}$id=(int)db_insert_id();if($manage)commit_transaction();return array('provision_execution_id'=>$id,'target_record_id'=>$t['target_record_id'],'execution_sha256'=>$execution_sha,'replayed'=>false);
}


function pay_api_001_rate_limit_runtime_core($company_id,$api_principal_id,$api_resource_id,$api_rate_limit_policy_id,$correlation_sha256,$unix_time,$require_browser_company,$api_principal_scope_id,&$error)
{
    $error=null;$company=pay_api_001_uint_value($company_id,true,4294967295);
    if($company===false){$error='rate_limit_input_invalid';return false;}
    if($require_browser_company&&!pay_api_001_runtime_require_company($company,$error))return false;
    $principal=pay_api_001_positive_id($api_principal_id);$resource=pay_api_001_positive_id($api_resource_id);$policy_id=pay_api_001_positive_id($api_rate_limit_policy_id);
    $scope_id=$api_principal_scope_id===null?null:pay_api_001_positive_id($api_principal_scope_id);
    if($principal===false||$resource===false||$policy_id===false||($api_principal_scope_id!==null&&$scope_id===false)||!pay_api_001_is_sha256($correlation_sha256)){$error='rate_limit_input_invalid';return false;}
    $t=$unix_time===null?time():pay_api_001_uint_value($unix_time,true,PHP_INT_MAX);if($t===false){$error='rate_limit_clock_invalid';return false;}
    $manage=pay_api_001_runtime_manage_transaction();if($manage)begin_transaction();
    $pr=pay_api_001_runtime_one('SELECT company_id,principal_status FROM '.TB_PREF.'hrm_pay_api_001_machine_principals WHERE api_principal_id='.$principal.' LIMIT 2 FOR UPDATE','could not lock PAY-API-001 principal for rate limit');
    $rs=pay_api_001_runtime_one('SELECT api_resource_id,read_capable,command_capable FROM '.TB_PREF.'hrm_pay_api_001_resources WHERE api_resource_id='.$resource.' LIMIT 2 FOR UPDATE','could not lock PAY-API-001 resource for rate limit');
    $pol=pay_api_001_runtime_one('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_rate_limit_policies WHERE api_rate_limit_policy_id='.$policy_id.' AND api_resource_id='.$resource.' AND action_code=\'read\' LIMIT 2 FOR UPDATE','could not lock PAY-API-001 rate policy');
    if(!$pr||!$rs||!$pol||(int)$pr['company_id']!==$company||(string)$pr['principal_status']!=='active'||empty($rs['read_capable'])||!empty($rs['command_capable'])||!pay_api_001_rate_limit_policy_valid($pol)){if($manage)cancel_transaction();$error='rate_limit_scope_or_policy_invalid';return false;}
    $scope_sql='SELECT api_principal_scope_id FROM '.TB_PREF.'hrm_pay_api_001_principal_scopes WHERE api_principal_id='.$principal.' AND api_resource_id='.$resource.' AND action_code=\'read\'';
    if($scope_id!==null)$scope_sql.=' AND api_principal_scope_id='.$scope_id;
    $scopeq=db_query($scope_sql.' LIMIT 2 FOR UPDATE');
    $scope_rows=$scopeq?db_num_rows($scopeq):0;
    if(!$scopeq||($scope_id===null?$scope_rows<1:$scope_rows!==1)){if($manage)cancel_transaction();$error='read_scope_required';return false;}
    $replayq=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_rate_limit_decisions WHERE company_id='.$company.' AND api_principal_id='.$principal.' AND api_resource_id='.$resource.' AND api_rate_limit_policy_id='.$policy_id.' AND correlation_sha256='.db_escape($correlation_sha256).' ORDER BY api_rate_limit_decision_id DESC LIMIT 2 FOR UPDATE');
    if(!$replayq||db_num_rows($replayq)>1){if($manage)cancel_transaction();$error='rate_limit_replay_state_invalid';return false;}
    if(db_num_rows($replayq)===1){$old=db_fetch_assoc($replayq);if($manage)commit_transaction();return array('allowed'=>(string)$old['outcome_code']==='rate_limit_allowed','code'=>$old['outcome_code'],'company_id'=>$company,'api_principal_id'=>$principal,'api_resource_id'=>$resource,'api_rate_limit_policy_id'=>$policy_id,'api_rate_limit_decision_id'=>(int)$old['api_rate_limit_decision_id'],'decision_sha256'=>$old['decision_sha256'],'replayed'=>true);}
    $window=pay_api_001_rate_limit_window_start($t,$pol['window_seconds']);if($window===false){if($manage)cancel_transaction();$error='rate_limit_clock_invalid';return false;}$window_dt=gmdate('Y-m-d H:i:s',$window);
    $q=db_query('SELECT COUNT(*) total,SUM(CASE WHEN outcome_code=\'rate_limit_allowed\' THEN 1 ELSE 0 END) accepted FROM '.TB_PREF.'hrm_pay_api_001_rate_limit_decisions WHERE api_rate_limit_policy_id='.$policy_id.' AND api_principal_id='.$principal.' AND window_start='.db_escape($window_dt));
    $counts=$q?db_fetch_assoc($q):false;if(!$counts){if($manage)cancel_transaction();$error='rate_limit_state_read_failed';return false;}
    $total=(int)$counts['total'];$accepted=(int)$counts['accepted'];if($total>=4294967294){if($manage)cancel_transaction();$error='rate_limit_ordinal_exhausted';return false;}
    $gate=pay_api_001_rate_limit_gate($pol,$accepted,$t);$ordinal=$total+1;$occurred=gmdate('Y-m-d H:i:s',$t);
    $decision_sha=pay_api_001_provisioning_hash(array('company_id'=>$company,'api_principal_id'=>$principal,'api_resource_id'=>$resource,'api_rate_limit_policy_id'=>$policy_id,'window_start'=>$window_dt,'request_ordinal'=>$ordinal,'outcome_code'=>$gate['code'],'correlation_sha256'=>$correlation_sha256,'occurred_at'=>$occurred));
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_rate_limit_decisions (company_id,api_principal_id,api_resource_id,api_rate_limit_policy_id,window_start,request_ordinal,outcome_code,correlation_sha256,decision_sha256,occurred_at,created_at) VALUES ('.$company.','.$principal.','.$resource.','.$policy_id.','.db_escape($window_dt).','.$ordinal.','.db_escape($gate['code']).','.db_escape($correlation_sha256).','.db_escape($decision_sha).','.db_escape($occurred).','.db_escape($occurred).')';
    if(!db_query($sql)){if($manage)cancel_transaction();$error='rate_limit_decision_insert_failed';return false;}
    $id=(int)db_insert_id();if($manage)commit_transaction();
    return array('allowed'=>!empty($gate['allowed']),'code'=>$gate['code'],'company_id'=>$company,'api_principal_id'=>$principal,'api_resource_id'=>$resource,'api_rate_limit_policy_id'=>$policy_id,'api_rate_limit_decision_id'=>$id,'decision_sha256'=>$decision_sha,'remaining'=>isset($gate['remaining'])?$gate['remaining']:0,'replayed'=>false);
}

function pay_api_001_enforce_read_rate_limit($company_id,$api_principal_id,$api_resource_id,$api_rate_limit_policy_id,$correlation_sha256,$unix_time=null,&$error=null)
{
    return pay_api_001_rate_limit_runtime_core($company_id,$api_principal_id,$api_resource_id,$api_rate_limit_policy_id,$correlation_sha256,$unix_time,true,null,$error);
}

function pay_api_001_authn_replay($company,$credential_key,$credential_key_sha256,$presented_secret,$correlation_sha256,$unix_time,&$error)
{
    $error=null;
    $q=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_authn_decisions WHERE company_id='.(int)$company.' AND correlation_sha256='.db_escape($correlation_sha256).' LIMIT 2 FOR UPDATE');
    if(!$q||db_num_rows($q)!==1){$error='authentication_replay_state_invalid';return false;}
    $r=db_fetch_assoc($q);
    if(!hash_equals((string)$r['credential_key_sha256'],$credential_key_sha256)){$error='correlation_reuse_mismatch';return false;}
    $ok=(string)$r['outcome_code']==='authenticated';
    $out=array('authenticated'=>$ok,'code'=>$ok?'authenticated':'authentication_failed','company_id'=>(int)$company,'api_authn_decision_id'=>(int)$r['api_authn_decision_id'],'decision_sha256'=>$r['decision_sha256'],'replayed'=>true);
    if($ok){
        $principal=pay_api_001_positive_id($r['api_principal_id']);$credential=pay_api_001_positive_id($r['api_credential_id']);
        if($principal===false||$credential===false){$error='authentication_replay_state_invalid';return false;}
        $sql='SELECT c.*,p.company_id,p.principal_status FROM '.TB_PREF.'hrm_pay_api_001_credentials c INNER JOIN '.TB_PREF.'hrm_pay_api_001_machine_principals p ON p.api_principal_id=c.api_principal_id WHERE c.api_credential_id='.$credential.' AND c.api_principal_id='.$principal.' AND c.credential_key='.db_escape($credential_key).' LIMIT 2 FOR UPDATE';
        $cq=db_query($sql,'could not revalidate PAY-API-001 credential replay');
        if(!$cq||db_num_rows($cq)!==1){$error='authentication_replay_state_invalid';return false;}
        $c=db_fetch_assoc($cq);$occurred=gmdate('Y-m-d H:i:s',$unix_time);
        $valid=(int)$c['company_id']===(int)$company&&(string)$c['principal_status']==='active'&&(string)$c['verifier_scheme']==='password_hash'&&strcmp($occurred,(string)$c['valid_from'])>=0&&strcmp($occurred,(string)$c['expires_at'])<0&&pay_api_001_verify_client_secret($presented_secret,(string)$c['secret_verifier_hash']);
        if($valid){
            $rq=db_query('SELECT credential_revocation_id FROM '.TB_PREF.'hrm_pay_api_001_credential_revocations WHERE api_credential_id='.$credential.' LIMIT 2 FOR UPDATE');
            if(!$rq||db_num_rows($rq)>1){$error='authentication_revocation_state_invalid';return false;}
            if(db_num_rows($rq)!==0)$valid=false;
        }
        if(!$valid){$out['authenticated']=false;$out['code']='authentication_failed';$out['replay_revalidated']=false;return $out;}
        $out['api_principal_id']=$principal;$out['api_credential_id']=$credential;$out['replay_revalidated']=true;
    }
    return $out;
}

function pay_api_001_authenticate_machine_credential($company_id,$credential_key,$presented_secret,$correlation_sha256,$unix_time=null,&$error=null)
{
    $error=null;$company=pay_api_001_uint_value($company_id,true,4294967295);
    $key=pay_api_001_provisioning_token($credential_key,128,'/^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$/D');
    $t=$unix_time===null?time():pay_api_001_uint_value($unix_time,true,PHP_INT_MAX);
    if($company===false||$key===false||!is_string($presented_secret)||$presented_secret===''||strlen($presented_secret)>4096||!pay_api_001_is_sha256($correlation_sha256)||$t===false){$error='authentication_input_invalid';return false;}
    $key_sha=hash('sha256',$key);$manage=pay_api_001_runtime_manage_transaction();if($manage)begin_transaction();
    $rq=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_authn_decisions WHERE company_id='.$company.' AND correlation_sha256='.db_escape($correlation_sha256).' LIMIT 2 FOR UPDATE');
    if(!$rq||db_num_rows($rq)>1){if($manage)cancel_transaction();$error='authentication_replay_state_invalid';return false;}
    if(db_num_rows($rq)===1){
        $out=pay_api_001_authn_replay($company,$key,$key_sha,$presented_secret,$correlation_sha256,$t,$error);
        if($out===false){if($manage)cancel_transaction();return false;}
        if($manage)commit_transaction();return $out;
    }
    $sql='SELECT c.*,p.company_id,p.principal_status FROM '.TB_PREF.'hrm_pay_api_001_credentials c INNER JOIN '.TB_PREF.'hrm_pay_api_001_machine_principals p ON p.api_principal_id=c.api_principal_id WHERE c.credential_key='.db_escape($key).' LIMIT 2 FOR UPDATE';
    $q=db_query($sql,'could not lock PAY-API-001 credential for authentication');
    if(!$q||db_num_rows($q)>1){if($manage)cancel_transaction();$error='authentication_credential_state_invalid';return false;}
    $row=db_num_rows($q)===1?db_fetch_assoc($q):false;
    $principal_id=$row?pay_api_001_positive_id($row['api_principal_id']):false;$credential_id=$row?pay_api_001_positive_id($row['api_credential_id']):false;
    if($row&&($principal_id===false||$credential_id===false)){if($manage)cancel_transaction();$error='authentication_credential_state_invalid';return false;}
    $occurred=gmdate('Y-m-d H:i:s',$t);$authenticated=false;
    if($row&&(int)$row['company_id']===$company&&(string)$row['principal_status']==='active'&&(string)$row['verifier_scheme']==='password_hash'&&strcmp($occurred,(string)$row['valid_from'])>=0&&strcmp($occurred,(string)$row['expires_at'])<0){
        $revocation_q=db_query('SELECT credential_revocation_id FROM '.TB_PREF.'hrm_pay_api_001_credential_revocations WHERE api_credential_id='.$credential_id.' LIMIT 2 FOR UPDATE');
        if(!$revocation_q||db_num_rows($revocation_q)>1){if($manage)cancel_transaction();$error='authentication_revocation_state_invalid';return false;}
        if(db_num_rows($revocation_q)===0&&pay_api_001_verify_client_secret($presented_secret,(string)$row['secret_verifier_hash']))$authenticated=true;
    }
    $outcome=$authenticated?'authenticated':'authentication_failed';
    $decision_sha=pay_api_001_provisioning_hash(array('company_id'=>$company,'api_principal_id'=>$row?$principal_id:null,'api_credential_id'=>$row?$credential_id:null,'credential_key_sha256'=>$key_sha,'outcome_code'=>$outcome,'correlation_sha256'=>$correlation_sha256,'occurred_at'=>$occurred));
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_authn_decisions (company_id,api_principal_id,api_credential_id,credential_key_sha256,outcome_code,correlation_sha256,decision_sha256,occurred_at,created_at) VALUES ('.$company.','.($row?(int)$principal_id:'NULL').','.($row?(int)$credential_id:'NULL').','.db_escape($key_sha).','.db_escape($outcome).','.db_escape($correlation_sha256).','.db_escape($decision_sha).','.db_escape($occurred).','.db_escape($occurred).')';
    if(!db_query($sql)){
        $replay_error=null;$replay=pay_api_001_authn_replay($company,$key,$key_sha,$presented_secret,$correlation_sha256,$t,$replay_error);
        if(is_array($replay)){if($manage)commit_transaction();return $replay;}
        if($manage)cancel_transaction();$error=$replay_error?:'authentication_decision_insert_failed';return false;
    }
    $id=(int)db_insert_id();if($manage)commit_transaction();
    $out=array('authenticated'=>$authenticated,'code'=>$outcome,'company_id'=>$company,'api_authn_decision_id'=>$id,'decision_sha256'=>$decision_sha,'replayed'=>false);
    if($authenticated){$out['api_principal_id']=$principal_id;$out['api_credential_id']=$credential_id;}
    return $out;
}

function pay_api_001_resolve_machine_read_authority($company_id,$api_principal_id,$resource_key,$contract_version,$object_scope_type=null,$object_scope_key=null,&$error=null)
{
    $error=null;$company=pay_api_001_uint_value($company_id,true,4294967295);$principal=pay_api_001_positive_id($api_principal_id);
    $rk=pay_api_001_provisioning_token($resource_key,96,'/^[a-z0-9][a-z0-9._\/-]{0,95}$/D');$cv=pay_api_001_provisioning_token($contract_version,32,'/^[A-Za-z0-9][A-Za-z0-9._-]{0,31}$/D');
    if($company===false||$principal===false||$rk===false||$cv===false||(($object_scope_type===null)!==($object_scope_key===null))){$error='read_authority_input_invalid';return false;}
    $pr=pay_api_001_runtime_one('SELECT api_principal_id,company_id,principal_status FROM '.TB_PREF.'hrm_pay_api_001_machine_principals WHERE api_principal_id='.$principal.' LIMIT 2','could not resolve PAY-API-001 machine principal');
    if(!$pr||(int)$pr['company_id']!==$company||(string)$pr['principal_status']!=='active'){$error='read_authority_denied';return false;}
    $rs=pay_api_001_runtime_one('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_resources WHERE resource_key='.db_escape($rk).' AND contract_version='.db_escape($cv).' LIMIT 2','could not resolve PAY-API-001 resource');
    if(!$rs||empty($rs['company_scope_required'])||empty($rs['read_capable'])||!empty($rs['command_capable'])){$error='read_authority_denied';return false;}
    $resource_id=pay_api_001_positive_id($rs['api_resource_id']);if($resource_id===false){$error='read_authority_denied';return false;}
    if(!empty($rs['object_scope_required'])){
        $ost=pay_api_001_provisioning_token($object_scope_type,32);$osk=pay_api_001_provisioning_token($object_scope_key,128,'/^[A-Za-z0-9][A-Za-z0-9._:\/-]{0,127}$/D');
        if($ost===false||$osk===false){$error='object_scope_required';return false;}
        $scope_sql='SELECT * FROM '.TB_PREF.'hrm_pay_api_001_principal_scopes WHERE api_principal_id='.$principal.' AND api_resource_id='.$resource_id.' AND action_code=\'read\' AND object_scope_type='.db_escape($ost).' AND object_scope_key='.db_escape($osk).' LIMIT 2';
    }else{
        if($object_scope_type!==null||$object_scope_key!==null){$error='object_scope_not_applicable';return false;}
        $scope_sql='SELECT * FROM '.TB_PREF.'hrm_pay_api_001_principal_scopes WHERE api_principal_id='.$principal.' AND api_resource_id='.$resource_id.' AND action_code=\'read\' AND object_scope_type IS NULL AND object_scope_key IS NULL LIMIT 2';
    }
    $scope=pay_api_001_runtime_one($scope_sql,'could not resolve PAY-API-001 read scope');
    if(!$scope){$error='read_scope_required';return false;}
    $policy=pay_api_001_runtime_one('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_rate_limit_policies WHERE api_resource_id='.$resource_id.' AND action_code=\'read\' LIMIT 2','could not resolve PAY-API-001 rate policy');
    if(!$policy||!pay_api_001_rate_limit_policy_valid($policy)){$error='rate_policy_required';return false;}
    return array('company_id'=>$company,'api_principal_id'=>$principal,'resource'=>$rs,'scope'=>$scope,'rate_policy'=>$policy);
}

function pay_api_001_enforce_machine_read_rate_limit($authn,$authority,$correlation_sha256,$unix_time=null,&$error=null)
{
    $error=null;
    if(!is_array($authn)||empty($authn['authenticated'])||!is_array($authority)||!isset($authority['resource'],$authority['scope'],$authority['rate_policy'])){$error='machine_rate_context_invalid';return false;}
    $company=pay_api_001_uint_value($authn['company_id']??null,true,4294967295);$principal=pay_api_001_positive_id($authn['api_principal_id']??null);
    if($company===false||$principal===false||$company!==pay_api_001_uint_value($authority['company_id']??null,true,4294967295)||$principal!==pay_api_001_positive_id($authority['api_principal_id']??null)){$error='machine_rate_context_mismatch';return false;}
    $resource=pay_api_001_positive_id($authority['resource']['api_resource_id']??null);$policy=pay_api_001_positive_id($authority['rate_policy']['api_rate_limit_policy_id']??null);$scope=pay_api_001_positive_id($authority['scope']['api_principal_scope_id']??null);
    if($resource===false||$policy===false||$scope===false){$error='machine_rate_context_invalid';return false;}
    return pay_api_001_rate_limit_runtime_core($company,$principal,$resource,$policy,$correlation_sha256,$unix_time,false,$scope,$error);
}
?>
