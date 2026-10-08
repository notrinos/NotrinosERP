<?php
/**
 * PAY-GLB-001 Phase-7 read-only development contract (source-only slice S01).
 *
 * NOT an authorization endpoint. The future server-side caller MUST enforce
 * permission, legal-entity scope and permitted data residency independently
 * before constructing this internal context or supplying normalized rows.
 * No untrusted user parameter may be promoted to a permission grant.
 */
if (!defined('PAY_GLB_001_CONTRACT_VERSION')) define('PAY_GLB_001_CONTRACT_VERSION', '1.0.0');
if (!defined('PAY_GLB_001_SOURCE_VERSION')) define('PAY_GLB_001_SOURCE_VERSION', '1.0.1066');
if (!defined('PAY_GLB_001_CUSTODY_VERSION')) define('PAY_GLB_001_CUSTODY_VERSION', '1.0.544');

/** Describe the authority ceiling of this source-only foundation. */
function pay_glb_001_policy()
{
    return array(
        'scope' => 'PURE_OFFLINE_AUTHORIZED_AGGREGATES_ONLY',
        'production_dashboard_route_allowed' => false,
        'production_calendar_route_allowed' => false,
        'requires_independent_server_authorization' => true,
        'requires_independent_residency_approval' => true,
        'company_and_entity_scope_required' => true,
        'source_age_must_be_disclosed' => true,
        'delivery_model_must_be_disclosed' => true,
        'external_deadline_legally_certified' => false,
        'raw_employee_or_payroll_data_allowed' => false,
        'employee_drilldown_allowed' => false,
        'file_or_network_access_allowed' => false,
        'provider_exchange_allowed' => false,
        'database_write_allowed' => false,
        'payroll_or_accounting_mutation_allowed' => false,
        'posting_payment_or_filing_allowed' => false,
        'automatic_role_grant_allowed' => false
    );
}

/** Enumerate the distinct supported payroll delivery models. */
function pay_glb_001_models()
{
    return array('NATIVE', 'EXTERNAL_PROVIDER', 'PARTNER_OPERATED', 'MANAGED_SERVICE');
}

/** Reject missing or extra fields in a normalized internal record. */
function pay_glb_001_exact_keys($input, $expected)
{
    if (!is_array($input)) return false;
    $actual = array_keys($input);
    sort($actual, SORT_STRING);
    sort($expected, SORT_STRING);
    return $actual === $expected;
}

/** Validate a bounded, canonical internal entity code. */
function pay_glb_001_code($s)
{
    return is_string($s) && preg_match('/^[A-Z0-9][A-Z0-9_.-]{0,31}$/D', $s) === 1;
}

/** Validate an exact UTC timestamp without accepting date normalization. */
function pay_glb_001_iso_utc($s)
{
    if (!is_string($s) || preg_match('/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/D', $s) !== 1) return false;
    try {
        $date = DateTimeImmutable::createFromFormat('!Y-m-d\TH:i:s\Z', $s, new DateTimeZone('UTC'));
        return $date !== false && $date->format('Y-m-d\TH:i:s\Z') === $s;
    } catch (Throwable $e) { return false; }
}

/** Validate a canonical lowercase SHA-256 evidence digest. */
function pay_glb_001_sha256($s)
{
    return is_string($s) && preg_match('/^[a-f0-9]{64}$/D', $s) === 1;
}

/** Only a trusted server adapter may create this context after actual ACL checks. */
function pay_glb_001_context_valid($ctx, $capability)
{
    if (!in_array($capability, array('rollup', 'calendar'), true)) return false;
    if (!pay_glb_001_exact_keys($ctx,
        array('company_id', 'entity_codes', 'purpose', 'allow_rollup', 'allow_calendar', 'as_of_utc'))) return false;
    if (!is_int($ctx['company_id']) || $ctx['company_id'] < 0 ||
        !is_string($ctx['purpose']) || $ctx['purpose'] !== 'PAY_GLB_001_INTERNAL_READ' ||
        !is_bool($ctx['allow_rollup']) || !is_bool($ctx['allow_calendar']) ||
        !pay_glb_001_iso_utc($ctx['as_of_utc'])) return false;
    if ($capability === 'rollup' && !$ctx['allow_rollup']) return false;
    if ($capability === 'calendar' && (!$ctx['allow_calendar'] || !$ctx['allow_rollup'])) return false;
    if (!is_array($ctx['entity_codes']) || count($ctx['entity_codes']) < 1 ||
        count($ctx['entity_codes']) > 32 ||
        array_keys($ctx['entity_codes']) !== range(0, count($ctx['entity_codes']) - 1)) return false;
    $seen = array();
    foreach ($ctx['entity_codes'] as $code) {
        if (!pay_glb_001_code($code) || isset($seen[$code])) return false;
        $seen[$code] = true;
    }
    return true;
}
?>
