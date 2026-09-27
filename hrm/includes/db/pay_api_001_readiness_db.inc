<?php
/** PAY-API-001 predecessor, dependency and fail-closed authority checks. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
include_once($path_to_root.'/includes/pay_api_001_contract.inc');
function pay_api_001_fail_patch($patch,$m){return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,$m);}
function pay_api_001_require_predecessor($patch){global $path_to_root;$files=array(
 'Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/API_AND_HOOK_CATALOG.md'=>'No general REST API, OAuth/JWT API authentication layer, durable job queue, webhook delivery subsystem',
 'Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/TARGET_ARCHITECTURE.md'=>'A public API/webhook platform is a separately governed phase',
 'tests/fixtures/payroll/legacy_v1/manifest.json'=>'"version": "1.0.464"');
 foreach($files as $rel=>$token){$raw=@file_get_contents($path_to_root.'/'.$rel);if(!is_string($raw)||strpos($raw,$token)===false)return pay_api_001_fail_patch($patch,'PAY-API-001 immutable predecessor/source contract incomplete: '.$rel.'.');}return true;}
function pay_api_001_require_contract($patch){$a=pay_api_001_authority_contract();foreach(array('versioned_resource_contract','deny_by_default','machine_principal_required','company_scope_required','object_scope_required_when_resource_declares','explicit_action_scope_required','immutable_resource_registry','immutable_principal_scope_custody','authentication_required','authorization_required','audit_required','idempotency_required_for_commands','pagination_required_for_collections','rate_limit_required_before_external_route') as $k)if(!isset($a[$k])||$a[$k]!==true)return pay_api_001_fail_patch($patch,'PAY-API-001 required contract capability missing: '.$k.'.');foreach(array('credential_or_secret_custody','external_http_route','webhook_delivery','webhook_receive','payroll_command_authority','payment_authority','gl_authority','statutory_filing_authority','network_authority','cross_company_authority','historical_payroll_or_accounting_mutation','generic_extension_dispatch') as $k)if(!array_key_exists($k,$a)||$a[$k]!==false)return pay_api_001_fail_patch($patch,'PAY-API-001 authority ceiling violated: '.$k.'.');return true;}
?>
