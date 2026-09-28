<?php
/** PAY-API-001 bounded outbound webhook event and claim contracts. */
include_once(dirname(__FILE__).'/pay_api_001_business_read.inc');
include_once(dirname(__FILE__).'/pay_api_001_provisioning.inc');
include_once(dirname(__FILE__).'/pay_api_001_contract.inc');
if (!defined('PAY_API_001_WEBHOOK_EVENT_TYPE')) define('PAY_API_001_WEBHOOK_EVENT_TYPE', 'payroll.period.summary.available');

/** Validate a webhook evidence timestamp using provisioning's UTC rules. */
function pay_api_001_webhook_writer_datetime($value)
{
    return pay_api_001_provisioning_datetime($value);
}

/** Build the deterministic minimized payroll-period-summary webhook envelope. */
function pay_api_001_webhook_event_envelope($company_id,array $summary,$source_evidence_sha256,$occurred_at)
{
    $company=pay_api_001_uint_value($company_id,true,4294967295);$projected=pay_api_001_payroll_period_summary_project_row($summary);
    if($company===false||$projected===false||!pay_api_001_is_sha256($source_evidence_sha256)||pay_api_001_webhook_writer_datetime($occurred_at)===false)return false;
    $payload=array('contract_version'=>PAY_API_001_PAYROLL_PERIOD_SUMMARY_CONTRACT,'event_type'=>PAY_API_001_WEBHOOK_EVENT_TYPE,'resource'=>PAY_API_001_PAYROLL_PERIOD_SUMMARY_RESOURCE,'summary'=>$projected);
    $payload_json=json_encode($payload,JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE);
    if(!is_string($payload_json))return false;
    $payload_sha=hash('sha256',$payload_json);$scope_key=(string)$projected['period_id'];
    $dedupe=pay_api_001_provisioning_hash(array('company_id'=>$company,'event_type'=>PAY_API_001_WEBHOOK_EVENT_TYPE,'contract_version'=>'v1','resource_key'=>PAY_API_001_PAYROLL_PERIOD_SUMMARY_RESOURCE,'object_scope_type'=>PAY_API_001_PAYROLL_PERIOD_SCOPE_TYPE,'object_scope_key'=>$scope_key,'payload_sha256'=>$payload_sha,'source_evidence_sha256'=>$source_evidence_sha256));
    if($dedupe===false)return false;
    $event_sha=pay_api_001_provisioning_hash(array('company_id'=>$company,'dedupe_key_sha256'=>$dedupe,'occurred_at'=>$occurred_at));
    return array('company_id'=>$company,'event_type'=>PAY_API_001_WEBHOOK_EVENT_TYPE,'contract_version'=>'v1','resource_key'=>PAY_API_001_PAYROLL_PERIOD_SUMMARY_RESOURCE,'object_scope_type'=>PAY_API_001_PAYROLL_PERIOD_SCOPE_TYPE,'object_scope_key'=>$scope_key,'payload_json'=>$payload_json,'payload_sha256'=>$payload_sha,'source_evidence_sha256'=>$source_evidence_sha256,'dedupe_key_sha256'=>$dedupe,'event_sha256'=>$event_sha,'occurred_at'=>$occurred_at);
}

/** Accept only an opaque SHA-256 claim-token digest. */
function pay_api_001_webhook_claim_token($value){return pay_api_001_is_sha256($value)?$value:false;}
?>
