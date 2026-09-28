<?php
/** PAY-API-001 predecessor, dependency and fail-closed authority checks. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
include_once($path_to_root.'/includes/pay_api_001_contract.inc');
function pay_api_001_fail_patch($patch,$m){return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,$m);}
function pay_api_001_require_predecessor($patch){global $path_to_root;$f=$path_to_root.'/tests/fixtures/payroll/legacy_v1/manifest_v1.0.470.json';$sha='c192d305f4fd93fa9afad96d5059bb3813c322733c882d150c8b325defadc672';if(!is_file($f)||!hash_equals($sha,hash_file('sha256',$f)))return pay_api_001_fail_patch($patch,'PAY-API-001 immutable predecessor custody mismatch: manifest_v1.0.470.json.');$files=array(
 'Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/API_AND_HOOK_CATALOG.md'=>'No general REST API, OAuth/JWT API authentication layer, durable job queue, webhook delivery subsystem',
 'Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/TARGET_ARCHITECTURE.md'=>'A public API/webhook platform is a separately governed phase');
 foreach($files as $rel=>$token){$raw=@file_get_contents($path_to_root.'/'.$rel);if(!is_string($raw)||strpos($raw,$token)===false)return pay_api_001_fail_patch($patch,'PAY-API-001 immutable predecessor/source contract incomplete: '.$rel.'.');}return true;}
function pay_api_001_require_threat_model($patch){global $path_to_root;$rel='Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/PAY_API_001_THREAT_MODEL_1.0.859_2026-09-27.md';$raw=@file_get_contents($path_to_root.'/'.$rel);foreach(array('Browser sessions are not API credentials','Cleartext client secrets','External HTTP exposure remains disabled','Read-only request plumbing must reject non-GET methods') as $token)if(!is_string($raw)||strpos($raw,$token)===false)return pay_api_001_fail_patch($patch,'PAY-API-001 threat-model source incomplete: '.$token.'.');return true;}
function pay_api_001_require_contract($patch){
 $previous=isset($patch->previous)?(string)$patch->previous:'';$a=pay_api_001_authority_contract($previous);
 foreach(array('versioned_resource_contract','deny_by_default','machine_principal_required','company_scope_required','object_scope_required_when_resource_declares','explicit_action_scope_required','immutable_resource_registry','immutable_principal_scope_custody','authentication_required','authorization_required','audit_required','idempotency_required_for_commands','pagination_required_for_collections','rate_limit_required_before_external_route','credential_verifier_hash_custody','credential_revocation_custody','authentication_decision_audit','read_only_request_gate','tls_required_for_external_route','provision_request_custody','independent_provision_review_custody','independent_provision_approval_custody','provisioning_sod_gate','provision_execution_receipt_custody','provisioning_execution_writer','credential_provisioning_writer','rate_limit_policy_custody','rate_limit_decision_custody','rate_limit_decision_gate','atomic_rate_limit_enforcement','read_only_adapter_contract','machine_credential_authentication_runtime','machine_read_authority_resolution','read_request_audit_custody','http_readonly_precontroller_contract','public_readonly_http_route_contract','public_route_database_version_gate','business_data_minimization_contract','read_response_audit_custody','payroll_period_summary_contract','business_data_dispatcher_contract','outbound_webhook_contract','webhook_target_contract','webhook_event_contract','webhook_outbox_contract','webhook_delivery_attempt_contract','webhook_dead_letter_contract','webhook_target_lifecycle_contract','webhook_secret_reference_contract','webhook_secret_rotation_contract','webhook_delivery_policy_contract','webhook_claim_lease_contract') as $k)if(!isset($a[$k])||$a[$k]!==true)return pay_api_001_fail_patch($patch,'PAY-API-001 required contract capability missing: '.$k.'.');
 foreach(array('query_string_credentials_allowed','browser_session_as_api_credential','cleartext_secret_storage','credential_or_secret_custody','webhook_enqueue_writer_allowed','webhook_target_lifecycle_writer_allowed','webhook_secret_resolver_allowed','webhook_claim_writer_allowed','webhook_retry_executor_allowed','webhook_delivery','webhook_receive','payroll_command_authority','payment_authority','gl_authority','statutory_filing_authority','network_authority','cross_company_authority','historical_payroll_or_accounting_mutation','generic_extension_dispatch') as $k)if(!array_key_exists($k,$a)||$a[$k]!==false)return pay_api_001_fail_patch($patch,'PAY-API-001 authority ceiling violated: '.$k.'.');
 $expected_route=pay_api_001_public_readonly_route_allowed_for_database_version($previous);if(!array_key_exists('external_http_route',$a)||$a['external_http_route']!==$expected_route)return pay_api_001_fail_patch($patch,'PAY-API-001 public route database-version boundary mismatch.');
 $expected_dispatch=pay_api_001_business_data_dispatch_allowed_for_database_version($previous);foreach(array('business_data_dispatcher_allowed','authorized_business_payload_allowed') as $k)if(!array_key_exists($k,$a)||$a[$k]!==$expected_dispatch)return pay_api_001_fail_patch($patch,'PAY-API-001 business-data database-version boundary mismatch: '.$k.'.');
 $expected_webhook_outbox=pay_api_001_webhook_outbox_custody_required_for_database_version($previous);if(!array_key_exists('webhook_outbox_custody_required',$a)||$a['webhook_outbox_custody_required']!==$expected_webhook_outbox)return pay_api_001_fail_patch($patch,'PAY-API-001 webhook outbox custody database-version boundary mismatch.');
 $expected_webhook_audit=pay_api_001_webhook_delivery_audit_custody_required_for_database_version($previous);if(!array_key_exists('webhook_delivery_audit_custody_required',$a)||$a['webhook_delivery_audit_custody_required']!==$expected_webhook_audit)return pay_api_001_fail_patch($patch,'PAY-API-001 webhook delivery-audit custody database-version boundary mismatch.');
 $expected_target_lifecycle=pay_api_001_webhook_target_lifecycle_custody_required_for_database_version($previous);if(!array_key_exists('webhook_target_lifecycle_custody_required',$a)||$a['webhook_target_lifecycle_custody_required']!==$expected_target_lifecycle)return pay_api_001_fail_patch($patch,'PAY-API-001 webhook target-lifecycle custody database-version boundary mismatch.');
 $expected_secret_reference=pay_api_001_webhook_secret_reference_custody_required_for_database_version($previous);if(!array_key_exists('webhook_secret_reference_custody_required',$a)||$a['webhook_secret_reference_custody_required']!==$expected_secret_reference)return pay_api_001_fail_patch($patch,'PAY-API-001 webhook secret-reference custody database-version boundary mismatch.');
 $expected_delivery_policy=pay_api_001_webhook_delivery_policy_lease_custody_required_for_database_version($previous);if(!array_key_exists('webhook_delivery_policy_lease_custody_required',$a)||$a['webhook_delivery_policy_lease_custody_required']!==$expected_delivery_policy)return pay_api_001_fail_patch($patch,'PAY-API-001 webhook delivery-policy/lease custody database-version boundary mismatch.');
 foreach(array('credential_provisioning_writer_allowed','provisioning_execution_writer_allowed') as $k)if(!isset($a[$k])||$a[$k]!==true)return pay_api_001_fail_patch($patch,'PAY-API-001 required bounded writer capability missing: '.$k.'.');return true;}
function pay_api_001_require_execution_authority_source($patch){global $path_to_root;$rel='includes/access_levels.inc';$raw=@file_get_contents($path_to_root.'/'.$rel);foreach(array('SA_HRM_MANAGE_PAY_API','SA_HRM_REVIEW_PAY_API','SA_HRM_APPROVE_PAY_API','SA_HRM_EXECUTE_PAY_API') as $token)if(!is_string($raw)||strpos($raw,$token)===false)return pay_api_001_fail_patch($patch,'PAY-API-001 execution authority source incomplete: '.$token.'.');return true;}

function pay_api_001_require_http_preroute_source($patch){global $path_to_root;$files=array(
 'includes/pay_api_001_http_readonly.inc'=>array('function pay_api_001_http_basic_credentials','function pay_api_001_http_preroute_envelope'),
 'hrm/includes/db/pay_api_001_http_runtime_db.inc'=>array('function pay_api_001_preroute_read_request','function pay_api_001_record_read_request_audit'));
 foreach($files as $rel=>$tokens){$raw=@file_get_contents($path_to_root.'/'.$rel);foreach($tokens as $token)if(!is_string($raw)||strpos($raw,$token)===false)return pay_api_001_fail_patch($patch,'PAY-API-001 HTTP pre-route source incomplete: '.$rel.' '.$token.'.');}
 return function_exists('pay_api_001_public_readonly_http_route_enabled')&&!pay_api_001_public_readonly_http_route_enabled();}
function pay_api_001_require_public_route_review($patch){global $path_to_root;$rel='Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/PAY_API_001_PUBLIC_READONLY_ROUTE_REVIEW_1.0.872_2026-09-28.md';$raw=@file_get_contents($path_to_root.'/'.$rel);foreach(array('The three historical guard-candidate files remain absent','remains false through database `1.0.872`','becomes true only at `1.0.873` or later','Browser session identity is never accepted as API authentication','There is no payroll business-data dispatcher') as $token)if(!is_string($raw)||strpos($raw,$token)===false)return pay_api_001_fail_patch($patch,'PAY-API-001 public route review incomplete: '.$token.'.');return true;}
function pay_api_001_require_public_route_source($patch){global $path_to_root;$files=array(
 'includes/pay_api_001_public_readonly_route.inc'=>array('function pay_api_001_public_route_parse_query','function pay_api_001_public_route_http_status','function pay_api_001_public_route_respond'),
 'payroll_api.php'=>array('pay_api_001_preroute_read_request','PAY_API_001_PUBLIC_READONLY_ROUTE_PATH','pay_api_001_public_readonly_http_route_enabled'));
 foreach($files as $rel=>$tokens){$raw=@file_get_contents($path_to_root.'/'.$rel);foreach($tokens as $token)if(!is_string($raw)||strpos($raw,$token)===false)return pay_api_001_fail_patch($patch,'PAY-API-001 public route source incomplete: '.$rel.' '.$token.'.');}
 foreach(array('api/payroll.php','api/v1/payroll.php','api/v1/index.php') as $rel)if(is_file($path_to_root.'/'.$rel))return pay_api_001_fail_patch($patch,'PAY-API-001 accepted 1.0.868 guard-candidate route unexpectedly exists: '.$rel.'.');
 return function_exists('pay_api_001_public_readonly_http_route_enabled_for_database_version')
  && !pay_api_001_public_readonly_http_route_enabled_for_database_version('1.0.872')
  && pay_api_001_public_readonly_http_route_enabled_for_database_version('1.0.873');}

function pay_api_001_require_business_read_review($patch){global $path_to_root;$rel='Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/PAY_API_001_PAYROLL_PERIOD_SUMMARY_REVIEW_1.0.875_2026-09-28.md';$raw=@file_get_contents($path_to_root.'/'.$rel);foreach(array('`payroll.period.summary` contract `v1`','Only approved-or-later payroll periods are externally readable','excludes employee identity','append-only response-audit custody','disabled through database `1.0.875`','eligible only at database `1.0.876` or later','webhooks, provider/network transport') as $token)if(!is_string($raw)||strpos($raw,$token)===false)return pay_api_001_fail_patch($patch,'PAY-API-001 business-read review incomplete: '.$token.'.');return true;}
function pay_api_001_require_read_response_audit_table($patch){return pay_api_001_require_table($patch,'hrm_pay_api_001_read_responses')&&pay_api_001_require_unique_index($patch,'hrm_pay_api_001_read_responses','uq_papi1_rres_request',array('api_read_request_id'))&&pay_api_001_require_unique_index($patch,'hrm_pay_api_001_read_responses','uq_papi1_rres_sha',array('response_sha256'))&&pay_api_001_require_restrictive_foreign_key($patch,'hrm_pay_api_001_read_responses','fk_papi1_rres_request','api_read_request_id','hrm_pay_api_001_read_requests','api_read_request_id');}
function pay_api_001_require_business_read_source($patch){global $path_to_root;$files=array(
 'includes/pay_api_001_business_read.inc'=>array('function pay_api_001_payroll_period_summary_request','function pay_api_001_payroll_period_summary_project_row','PAY_API_001_PAYROLL_PERIOD_SUMMARY_RESOURCE'),
 'hrm/includes/db/pay_api_001_business_read_db.inc'=>array('function pay_api_001_record_read_response_audit','function pay_api_001_dispatch_authorized_read','status IN (2,3,4,5,6)'),
 'payroll_api.php'=>array('pay_api_001_dispatch_authorized_read','pay_api_001_public_route_respond_payload'));
 foreach($files as $rel=>$tokens){$raw=@file_get_contents($path_to_root.'/'.$rel);foreach($tokens as $token)if(!is_string($raw)||strpos($raw,$token)===false)return pay_api_001_fail_patch($patch,'PAY-API-001 business-read source incomplete: '.$rel.' '.$token.'.');}
 return !pay_api_001_business_data_dispatch_enabled_for_database_version('1.0.875')&&pay_api_001_business_data_dispatch_enabled_for_database_version('1.0.876');}


/** Verify the reviewed non-delivering outbound webhook foundation source. */
function pay_api_001_require_webhook_foundation_review($patch)
{
 global $path_to_root;
 $rel='Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/PAY_API_001_WEBHOOK_OUTBOX_REVIEW_1.0.876_2026-09-28.md';
 $raw=@file_get_contents($path_to_root.'/'.$rel);
 foreach(array('non-delivering outbound webhook custody foundation','stores **no endpoint URL, bearer token, signing secret, private key, password, or cleartext credential**','`1.0.876 -> 1.0.877`','`1.0.877 -> 1.0.878`','`1.0.878 -> 1.0.879`','webhook_enqueue_writer_allowed','network_authority') as $token)if(!is_string($raw)||strpos($raw,$token)===false)return pay_api_001_fail_patch($patch,'PAY-API-001 webhook foundation review incomplete: '.$token.'.');
 return true;
}

/** Verify exact immutable target/event/outbox custody and lineage. */
function pay_api_001_require_webhook_outbox_tables($patch)
{
 foreach(array('hrm_pay_api_001_webhook_targets','hrm_pay_api_001_webhook_events','hrm_pay_api_001_webhook_outbox') as $table)if(!pay_api_001_require_table($patch,$table))return false;
 return pay_api_001_require_unique_index($patch,'hrm_pay_api_001_webhook_targets','uq_papi1_wt_key',array('company_id','target_key'))
  &&pay_api_001_require_unique_index($patch,'hrm_pay_api_001_webhook_targets','uq_papi1_wt_sha',array('target_sha256'))
  &&pay_api_001_require_unique_index($patch,'hrm_pay_api_001_webhook_events','uq_papi1_we_dedupe',array('company_id','dedupe_key_sha256'))
  &&pay_api_001_require_unique_index($patch,'hrm_pay_api_001_webhook_events','uq_papi1_we_sha',array('event_sha256'))
  &&pay_api_001_require_unique_index($patch,'hrm_pay_api_001_webhook_outbox','uq_papi1_wo_pair',array('webhook_event_id','webhook_target_id'))
  &&pay_api_001_require_unique_index($patch,'hrm_pay_api_001_webhook_outbox','uq_papi1_wo_queue',array('queue_key_sha256'))
  &&pay_api_001_require_unique_index($patch,'hrm_pay_api_001_webhook_outbox','uq_papi1_wo_sha',array('outbox_sha256'))
  &&pay_api_001_require_restrictive_foreign_key($patch,'hrm_pay_api_001_webhook_outbox','fk_papi1_wo_event','webhook_event_id','hrm_pay_api_001_webhook_events','webhook_event_id')
  &&pay_api_001_require_restrictive_foreign_key($patch,'hrm_pay_api_001_webhook_outbox','fk_papi1_wo_target','webhook_target_id','hrm_pay_api_001_webhook_targets','webhook_target_id');
}

/** Verify exact immutable delivery-attempt/dead-letter custody and lineage. */
function pay_api_001_require_webhook_delivery_audit_tables($patch)
{
 foreach(array('hrm_pay_api_001_webhook_delivery_attempts','hrm_pay_api_001_webhook_dead_letters') as $table)if(!pay_api_001_require_table($patch,$table))return false;
 return pay_api_001_require_unique_index($patch,'hrm_pay_api_001_webhook_delivery_attempts','uq_papi1_wa_attempt',array('webhook_outbox_id','attempt_no'))
  &&pay_api_001_require_unique_index($patch,'hrm_pay_api_001_webhook_delivery_attempts','uq_papi1_wa_lineage',array('webhook_outbox_id','webhook_delivery_attempt_id'))
  &&pay_api_001_require_unique_index($patch,'hrm_pay_api_001_webhook_delivery_attempts','uq_papi1_wa_sha',array('attempt_sha256'))
  &&pay_api_001_require_unique_index($patch,'hrm_pay_api_001_webhook_dead_letters','uq_papi1_wd_outbox',array('webhook_outbox_id'))
  &&pay_api_001_require_unique_index($patch,'hrm_pay_api_001_webhook_dead_letters','uq_papi1_wd_sha',array('dead_letter_sha256'))
  &&pay_api_001_require_restrictive_foreign_key($patch,'hrm_pay_api_001_webhook_delivery_attempts','fk_papi1_wa_outbox','webhook_outbox_id','hrm_pay_api_001_webhook_outbox','webhook_outbox_id')
  &&pay_api_001_require_restrictive_foreign_key($patch,'hrm_pay_api_001_webhook_dead_letters','fk_papi1_wd_outbox','webhook_outbox_id','hrm_pay_api_001_webhook_outbox','webhook_outbox_id')
  &&pay_api_001_require_restrictive_foreign_key_columns($patch,'hrm_pay_api_001_webhook_dead_letters','fk_papi1_wd_attempt',array('webhook_outbox_id','last_attempt_id'),'hrm_pay_api_001_webhook_delivery_attempts',array('webhook_outbox_id','webhook_delivery_attempt_id'));
}

/** Verify the reviewed non-delivering webhook transport-preparation source. */
function pay_api_001_require_webhook_transport_prep_review($patch)
{
 global $path_to_root;
 $rel='Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/PAY_API_001_WEBHOOK_TRANSPORT_PREP_REVIEW_1.0.879_2026-09-28.md';
 $raw=@file_get_contents($path_to_root.'/'.$rel);
 foreach(array('non-delivering transport-preparation batch','`1.0.879 -> 1.0.880`','`1.0.880 -> 1.0.881`','`1.0.881 -> 1.0.882`','no endpoint URL, bearer token, signing secret, private key, password, API key','webhook_retry_executor_allowed','network_authority') as $token)if(!is_string($raw)||stripos($raw,$token)===false)return pay_api_001_fail_patch($patch,'PAY-API-001 webhook transport-prep review incomplete: '.$token.'.');
 return true;
}

function pay_api_001_require_webhook_target_lifecycle_table($patch)
{
 return pay_api_001_require_table($patch,'hrm_pay_api_001_webhook_target_lifecycle')
  &&pay_api_001_require_unique_index($patch,'hrm_pay_api_001_webhook_target_lifecycle','uq_papi1_wtl_event',array('webhook_target_id','effective_at','action_code'))
  &&pay_api_001_require_unique_index($patch,'hrm_pay_api_001_webhook_target_lifecycle','uq_papi1_wtl_sha',array('lifecycle_sha256'))
  &&pay_api_001_require_restrictive_foreign_key($patch,'hrm_pay_api_001_webhook_target_lifecycle','fk_papi1_wtl_target','webhook_target_id','hrm_pay_api_001_webhook_targets','webhook_target_id')
  &&pay_api_001_require_restrictive_foreign_key($patch,'hrm_pay_api_001_webhook_target_lifecycle','fk_papi1_wtl_successor','successor_target_id','hrm_pay_api_001_webhook_targets','webhook_target_id');
}

function pay_api_001_require_webhook_secret_reference_tables($patch)
{
 foreach(array('hrm_pay_api_001_webhook_secret_refs','hrm_pay_api_001_webhook_secret_rotations') as $table)if(!pay_api_001_require_table($patch,$table))return false;
 return pay_api_001_require_unique_index($patch,'hrm_pay_api_001_webhook_secret_refs','uq_papi1_wsr_ref',array('company_id','ref_kind','secret_ref_sha256'))
  &&pay_api_001_require_unique_index($patch,'hrm_pay_api_001_webhook_secret_refs','uq_papi1_wsr_sha',array('secret_ref_sha256'))
  &&pay_api_001_require_unique_index($patch,'hrm_pay_api_001_webhook_secret_rotations','uq_papi1_wsrn_successor',array('successor_secret_ref_id'))
  &&pay_api_001_require_unique_index($patch,'hrm_pay_api_001_webhook_secret_rotations','uq_papi1_wsrn_sha',array('rotation_sha256'))
  &&pay_api_001_require_restrictive_foreign_key($patch,'hrm_pay_api_001_webhook_secret_rotations','fk_papi1_wsrn_previous','previous_secret_ref_id','hrm_pay_api_001_webhook_secret_refs','webhook_secret_ref_id')
  &&pay_api_001_require_restrictive_foreign_key($patch,'hrm_pay_api_001_webhook_secret_rotations','fk_papi1_wsrn_successor','successor_secret_ref_id','hrm_pay_api_001_webhook_secret_refs','webhook_secret_ref_id');
}

function pay_api_001_require_webhook_delivery_policy_lease_tables($patch)
{
 foreach(array('hrm_pay_api_001_webhook_delivery_policies','hrm_pay_api_001_webhook_claim_leases') as $table)if(!pay_api_001_require_table($patch,$table))return false;
 return pay_api_001_require_unique_index($patch,'hrm_pay_api_001_webhook_delivery_policies','uq_papi1_wdp_version',array('webhook_target_id','policy_version'))
  &&pay_api_001_require_unique_index($patch,'hrm_pay_api_001_webhook_delivery_policies','uq_papi1_wdp_sha',array('policy_sha256'))
  &&pay_api_001_require_unique_index($patch,'hrm_pay_api_001_webhook_claim_leases','uq_papi1_wcl_ordinal',array('webhook_outbox_id','lease_ordinal'))
  &&pay_api_001_require_unique_index($patch,'hrm_pay_api_001_webhook_claim_leases','uq_papi1_wcl_token',array('claim_token_sha256'))
  &&pay_api_001_require_unique_index($patch,'hrm_pay_api_001_webhook_claim_leases','uq_papi1_wcl_sha',array('claim_sha256'))
  &&pay_api_001_require_restrictive_foreign_key($patch,'hrm_pay_api_001_webhook_delivery_policies','fk_papi1_wdp_target','webhook_target_id','hrm_pay_api_001_webhook_targets','webhook_target_id')
  &&pay_api_001_require_restrictive_foreign_key($patch,'hrm_pay_api_001_webhook_claim_leases','fk_papi1_wcl_outbox','webhook_outbox_id','hrm_pay_api_001_webhook_outbox','webhook_outbox_id')
  &&pay_api_001_require_restrictive_foreign_key($patch,'hrm_pay_api_001_webhook_claim_leases','fk_papi1_wcl_policy','webhook_delivery_policy_id','hrm_pay_api_001_webhook_delivery_policies','webhook_delivery_policy_id');
}

?>
