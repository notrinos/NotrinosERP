<?php
/** PAY-API-001 pure provisioning request/review/approval validation. No target writer is enabled here. */
include_once(dirname(__FILE__).'/pay_api_001_authn.inc');
if (!defined('PAY_API_001_PROVISIONING_CONTRACT_VERSION')) define('PAY_API_001_PROVISIONING_CONTRACT_VERSION', '1.0.864');

function pay_api_001_provisioning_positive_id($value)
{
    if (is_int($value)) return $value > 0 ? $value : false;
    if (!is_string($value) || preg_match('/^[1-9][0-9]*$/D', $value) !== 1) return false;
    $n=(int)$value;
    return $n > 0 && (string)$n === $value ? $n : false;
}

function pay_api_001_provisioning_sha256($value)
{
    return is_string($value) && preg_match('/^[a-f0-9]{64}$/D', $value) === 1;
}

function pay_api_001_provisioning_target_kind($value)
{
    return in_array((string)$value, array('resource','machine_principal','principal_scope','credential','credential_revocation'), true);
}

function pay_api_001_provision_request_gate($request)
{
    if (!is_array($request)) return false;
    if (!isset($request['company_id']) || !pay_api_001_is_company_id($request['company_id'])) return false;
    if (!isset($request['target_kind']) || !pay_api_001_provisioning_target_kind($request['target_kind'])) return false;
    foreach (array('target_key_sha256','target_payload_sha256','request_sha256') as $k)
        if (!isset($request[$k]) || !pay_api_001_provisioning_sha256($request[$k])) return false;
    if (!isset($request['reason_code']) || !is_string($request['reason_code']) || preg_match('/^[a-z0-9][a-z0-9._-]{0,63}$/D', $request['reason_code']) !== 1) return false;
    if (!isset($request['requested_by']) || pay_api_001_provisioning_positive_id($request['requested_by']) === false) return false;
    return true;
}

function pay_api_001_provision_review_gate($request, $review)
{
    if (!pay_api_001_provision_request_gate($request) || !is_array($review)) return false;
    $request_id=isset($request['provision_request_id']) ? pay_api_001_provisioning_positive_id($request['provision_request_id']) : false;
    if ($request_id === false || !isset($review['provision_request_id']) || pay_api_001_provisioning_positive_id($review['provision_request_id']) !== $request_id) return false;
    $reviewer=isset($review['reviewed_by']) ? pay_api_001_provisioning_positive_id($review['reviewed_by']) : false;
    $maker=pay_api_001_provisioning_positive_id($request['requested_by']);
    if ($reviewer === false || $reviewer === $maker) return false;
    if (!isset($review['decision_code']) || !in_array((string)$review['decision_code'], array('approve','reject'), true)) return false;
    if (!isset($review['request_sha256']) || !hash_equals((string)$request['request_sha256'], (string)$review['request_sha256'])) return false;
    if (!isset($review['review_sha256']) || !pay_api_001_provisioning_sha256($review['review_sha256'])) return false;
    return true;
}

function pay_api_001_provision_approval_gate($request, $review, $approval)
{
    if (!pay_api_001_provision_review_gate($request,$review) || (string)$review['decision_code'] !== 'approve' || !is_array($approval)) return false;
    $request_id=pay_api_001_provisioning_positive_id($request['provision_request_id']);
    $review_id=isset($review['provision_review_id']) ? pay_api_001_provisioning_positive_id($review['provision_review_id']) : false;
    if ($review_id === false) return false;
    if (!isset($approval['provision_request_id']) || pay_api_001_provisioning_positive_id($approval['provision_request_id']) !== $request_id) return false;
    if (!isset($approval['provision_review_id']) || pay_api_001_provisioning_positive_id($approval['provision_review_id']) !== $review_id) return false;
    $maker=pay_api_001_provisioning_positive_id($request['requested_by']);
    $reviewer=pay_api_001_provisioning_positive_id($review['reviewed_by']);
    $approver=isset($approval['approved_by']) ? pay_api_001_provisioning_positive_id($approval['approved_by']) : false;
    if ($approver === false || $approver === $maker || $approver === $reviewer) return false;
    if (!isset($approval['decision_code']) || (string)$approval['decision_code'] !== 'approve') return false;
    if (!isset($approval['request_sha256']) || !hash_equals((string)$request['request_sha256'], (string)$approval['request_sha256'])) return false;
    if (!isset($approval['review_sha256']) || !hash_equals((string)$review['review_sha256'], (string)$approval['review_sha256'])) return false;
    if (!isset($approval['approval_sha256']) || !pay_api_001_provisioning_sha256($approval['approval_sha256'])) return false;
    return true;
}

function pay_api_001_provisioning_execution_writer_enabled()
{
    return false;
}
?>
