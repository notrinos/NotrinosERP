<?php
/**
 * Pure in-memory combination of three MOCK proofs. Never an attestation of
 * vendor admission, accounting reconciliation, payroll accuracy or production.
 */
require_once dirname(__FILE__).'/pay_con_001_deel_mock_webhook_rehearsal.inc';
require_once dirname(__FILE__).'/pay_con_001_deel_mock_report_rehearsal.inc';
require_once dirname(__FILE__).'/pay_con_001_deel_mock_shadow.inc';

/** Fail closed unless every independent synthetic input is fully canonical. */
function pay_con_001_deel_mock_reconcile_rehearsal($fixture, $report_request, $webhook_raw,
    $signature, $synthetic_key, $now, $seen_digests, $plan, $echo)
{
    $policy = pay_con_001_deel_mock_policy();
    foreach (array('external_transport_allowed', 'inbound_webhook_listener_allowed',
        'payroll_result_authority_allowed', 'payroll_execution_allowed', 'gl_posting_allowed',
        'payment_execution_allowed', 'filing_submission_allowed', 'production_activation_allowed') as $denied) {
        if (($policy[$denied] ?? true) !== false) return false;
    }
    if (($policy['source_database_version'] ?? '') !== '1.0.1066'
        || ($policy['legacy_payroll_custody'] ?? '') !== '1.0.544') return false;
    $webhook = pay_con_001_deel_mock_rehearse_webhook($webhook_raw, $signature, $synthetic_key, $now, $seen_digests);
    $report = pay_con_001_deel_mock_report_receipt($report_request, $fixture);
    $shadow = pay_con_001_deel_mock_shadow_reconcile($plan, $echo);
    if ($webhook === false || $report === false || $shadow === false
        || ($shadow['status'] ?? '') !== 'MOCK_RECONCILED_NOT_IMPORTED'
        || ($shadow['external_exchange_occurred'] ?? true) !== false
        || ($shadow['payroll_result_accepted'] ?? true) !== false) return false;
    $proof = array(
        'webhook_digest_sha256' => $webhook['body_digest_sha256'],
        'report_digest_sha256' => $report['report_digest_sha256'],
        'shadow_digest_sha256' => $shadow['shadow_digest_sha256'],
        'mapping_version' => 'mock-v1',
        'company_id' => 0
    );
    $json = json_encode($proof, JSON_UNESCAPED_SLASHES);
    if (!is_string($json)) return false;
    return array(
        'classification' => 'DEVELOPMENT_MOCK_REHEARSAL_NOT_PROVIDER_ACCEPTANCE',
        'receipt_digest_sha256' => hash('sha256', 'PAY-CON-001:DEEL:MOCK_REHEARSAL:v1:'.$json),
        'evidence' => $proof,
        'replay_custody_persistent' => false,
        'vendor_admission_approved' => false,
        'real_provider_activated' => false,
        'network_exchange_occurred' => false,
        'authoritative_payroll_results_imported' => false,
        'payroll_accounting_mutation' => false
    );
}
?>
