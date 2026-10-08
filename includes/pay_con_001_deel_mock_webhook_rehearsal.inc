<?php
/**
 * PAY-CON-001 source-only Deel webhook SHAPE rehearsal.
 * Pure function: no listener, secret resolver, I/O, event handler or live authority.
 * Any caller-provided signature key MUST be synthetic, never a real webhook key.
 */
require_once dirname(__FILE__).'/pay_con_001_deel_mock_adapter.inc';

/**
 * Check an explicitly invented JSON envelope in a bounded, replay-aware mock.
 * $now and $seen_digests are test inputs, NOT persistent replay protection.
 */
function pay_con_001_deel_mock_rehearse_webhook($raw, $signature, $synthetic_key, $now, $seen_digests)
{
    if (!is_string($raw) || strlen($raw) < 2 || strlen($raw) > 4096
        || !is_int($now) || $now < 0 || !is_array($seen_digests) || count($seen_digests) > 256
        || !pay_con_001_deel_mock_verify_signature($raw, $signature, $synthetic_key)) return false;
    foreach ($seen_digests as $seen) {
        if (!is_string($seen) || preg_match('/^[a-f0-9]{64}$/D', $seen) !== 1) return false;
    }
    $body = json_decode($raw, true);
    if (!is_array($body) || json_last_error() !== JSON_ERROR_NONE
        || json_encode($body, JSON_UNESCAPED_SLASHES) !== $raw) return false;
    // Reconstructed mock-only schema intentionally does NOT accept arbitrary real payloads.
    if (array_keys($body) !== array('fixture_kind', 'company_id', 'data', 'timestamp')
        || $body['fixture_kind'] !== 'SYNTHETIC_DEEL_WEBHOOK_NOT_VENDOR_EVENT'
        || $body['company_id'] !== 0 || !is_array($body['data'])
        || array_keys($body['data']) !== array('meta', 'resource')
        || !is_array($body['data']['meta'])
        || array_keys($body['data']['meta']) !== array('event_type', 'organization_id')
        || $body['data']['meta']['event_type'] !== 'contract.created'
        || $body['data']['meta']['organization_id'] !== '3fe30c6d-e07a-471e-ac9d-9b5e03a78925'
        || $body['data']['resource'] !== array()
        || !is_string($body['timestamp'])
        || preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$/D', $body['timestamp']) !== 1)
        return false;
    $time = DateTimeImmutable::createFromFormat('!Y-m-d\TH:i:s\Z', $body['timestamp'], new DateTimeZone('UTC'));
    if (!$time || $time->format('Y-m-d\TH:i:s\Z') !== $body['timestamp']
        || abs($now - $time->getTimestamp()) > 300) return false;
    $digest = hash('sha256', 'PAY-CON-001:DEEL:SYNTHETIC_WEBHOOK:v1:'.$raw);
    if (in_array($digest, $seen_digests, true)) return false;
    return array(
        'classification' => 'OFFLINE_SYNTHETIC_WEBHOOK_REHEARSAL_ONLY',
        'company_id' => 0,
        'event_type' => 'contract.created',
        'body_digest_sha256' => $digest,
        'verified_with_synthetic_key' => true,
        'replay_custody_persistent' => false,
        'received_live_event' => false,
        'provider_admitted' => false,
        'payroll_result_authority' => false
    );
}
?>
