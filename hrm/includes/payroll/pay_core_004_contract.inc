<?php
/**
 * Pure PAY-CORE-004 contract for immutable payroll execution snapshots,
 * results, lines, traces, integrity evidence and correction lineage.
 *
 * This file deliberately owns no database, session, request, accounting,
 * posting, payment or production calculation authority.
 */
if (!defined('HRM_PAY_CORE_004_CONTRACT_VERSION'))
    define('HRM_PAY_CORE_004_CONTRACT_VERSION', '1.0');
if (!defined('HRM_PAY_CORE_004_PRODUCTION_CALCULATION_ALLOWED'))
    define('HRM_PAY_CORE_004_PRODUCTION_CALCULATION_ALLOWED', false);
if (!defined('HRM_PAY_CORE_004_ACCOUNTING_AUTHORITY_ALLOWED'))
    define('HRM_PAY_CORE_004_ACCOUNTING_AUTHORITY_ALLOWED', false);
if (!defined('HRM_PAY_CORE_004_HISTORICAL_MUTATION_ALLOWED'))
    define('HRM_PAY_CORE_004_HISTORICAL_MUTATION_ALLOWED', false);

function hrm_pay_core_004_is_sha256($value)
{
    return is_string($value) && preg_match('/^[a-f0-9]{64}$/D', $value) === 1;
}

function hrm_pay_core_004_is_iso_date($value)
{
    if (!is_string($value) || preg_match('/^\d{4}-\d{2}-\d{2}$/D', $value) !== 1)
        return false;
    $dt = DateTimeImmutable::createFromFormat('!Y-m-d', $value);
    return $dt !== false && $dt->format('Y-m-d') === $value;
}

function hrm_pay_core_004_is_iso_datetime($value)
{
    if (!is_string($value) || preg_match('/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/D', $value) !== 1)
        return false;
    $dt = DateTimeImmutable::createFromFormat('!Y-m-d\TH:i:s\Z', $value, new DateTimeZone('UTC'));
    return $dt !== false && $dt->format('Y-m-d\TH:i:s\Z') === $value;
}

/** Recursively canonicalize arrays for stable JSON hashing. */
function hrm_pay_core_004_canonicalize($value)
{
    if (!is_array($value))
        return $value;
    $is_list = array_keys($value) === range(0, count($value) - 1);
    if ($is_list) {
        $out = array();
        foreach ($value as $item)
            $out[] = hrm_pay_core_004_canonicalize($item);
        return $out;
    }
    ksort($value, SORT_STRING);
    foreach ($value as $key => $item)
        $value[$key] = hrm_pay_core_004_canonicalize($item);
    return $value;
}

function hrm_pay_core_004_canonical_json($value)
{
    $json = json_encode(hrm_pay_core_004_canonicalize($value), JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE | JSON_PRESERVE_ZERO_FRACTION);
    return is_string($json) ? $json : false;
}

function hrm_pay_core_004_hash($value)
{
    $json = hrm_pay_core_004_canonical_json($value);
    return $json === false ? false : hash('sha256', $json);
}

function hrm_pay_core_004_hash_set($hashes)
{
    if (!is_array($hashes))
        return false;
    $clean = array();
    foreach ($hashes as $hash) {
        if (!hrm_pay_core_004_is_sha256($hash))
            return false;
        $clean[] = $hash;
    }
    sort($clean, SORT_STRING);
    return hrm_pay_core_004_hash($clean);
}

function hrm_pay_core_004_lineage_kinds()
{
    return array('correction', 'supersession', 'reversal', 'adjustment');
}

function hrm_pay_core_004_trace_value_modes()
{
    return array('none', 'redacted', 'hash_only');
}

function hrm_pay_core_004_legacy_provenance_states()
{
    return array('complete', 'partial', 'unavailable', 'ambiguous');
}

function hrm_pay_core_004_recovery_states()
{
    return array('verified', 'quarantined', 'recovered', 'unresolved');
}

function hrm_pay_core_004_archive_states()
{
    return array('archive_verified', 'restore_verified', 'restore_failed');
}

function hrm_pay_core_004_validate_snapshot_descriptor($row)
{
    if (!is_array($row))
        return false;
    foreach (array('payroll_relationship_id','calendar_period_id','run_identity','legal_entity_id','currency','approved_inputs_sha256','element_versions_sha256','eligibility_sha256','rule_artifacts_sha256','source_events_sha256','configuration_sha256') as $key)
        if (!array_key_exists($key, $row)) return false;
    if ((int)$row['payroll_relationship_id'] <= 0 || (int)$row['calendar_period_id'] <= 0 || (int)$row['legal_entity_id'] <= 0)
        return false;
    if (!is_string($row['run_identity']) || preg_match('/^[A-Za-z0-9][A-Za-z0-9_.:-]{1,127}$/D', $row['run_identity']) !== 1)
        return false;
    if (!is_string($row['currency']) || preg_match('/^[A-Z]{3}$/D', $row['currency']) !== 1)
        return false;
    if (isset($row['assignment_id']) && $row['assignment_id'] !== null && (int)$row['assignment_id'] <= 0)
        return false;
    foreach (array('approved_inputs_sha256','element_versions_sha256','eligibility_sha256','rule_artifacts_sha256','source_events_sha256','configuration_sha256') as $key)
        if (!hrm_pay_core_004_is_sha256($row[$key])) return false;
    return true;
}

function hrm_pay_core_004_snapshot_sha256($row)
{
    if (!hrm_pay_core_004_validate_snapshot_descriptor($row))
        return false;
    $bound = array(
        'payroll_relationship_id'=>(int)$row['payroll_relationship_id'],
        'assignment_id'=>(isset($row['assignment_id']) && $row['assignment_id'] !== null) ? (int)$row['assignment_id'] : null,
        'calendar_period_id'=>(int)$row['calendar_period_id'],
        'run_identity'=>(string)$row['run_identity'],
        'legal_entity_id'=>(int)$row['legal_entity_id'],
        'currency'=>(string)$row['currency'],
        'approved_inputs_sha256'=>(string)$row['approved_inputs_sha256'],
        'element_versions_sha256'=>(string)$row['element_versions_sha256'],
        'eligibility_sha256'=>(string)$row['eligibility_sha256'],
        'rule_artifacts_sha256'=>(string)$row['rule_artifacts_sha256'],
        'source_events_sha256'=>(string)$row['source_events_sha256'],
        'configuration_sha256'=>(string)$row['configuration_sha256'],
    );
    return hrm_pay_core_004_hash($bound);
}

function hrm_pay_core_004_validate_result_line($row)
{
    if (!is_array($row))
        return false;
    foreach (array('line_no','element_version_id','line_kind','currency','amount','quantity','rate','source_sha256') as $key)
        if (!array_key_exists($key, $row)) return false;
    if ((int)$row['line_no'] < 1 || (int)$row['element_version_id'] <= 0)
        return false;
    if (!in_array($row['line_kind'], array('earning','deduction','employer_contribution','information'), true))
        return false;
    if (!is_string($row['currency']) || preg_match('/^[A-Z]{3}$/D', $row['currency']) !== 1)
        return false;
    foreach (array('amount','quantity','rate') as $key)
        if (!is_int($row[$key]) && !is_float($row[$key]) && !(is_string($row[$key]) && preg_match('/^-?\d+(?:\.\d+)?$/D', $row[$key]) === 1)) return false;
    return hrm_pay_core_004_is_sha256($row['source_sha256']);
}

function hrm_pay_core_004_result_line_sha256($row)
{
    if (!hrm_pay_core_004_validate_result_line($row))
        return false;
    return hrm_pay_core_004_hash(array(
        'line_no'=>(int)$row['line_no'],
        'element_version_id'=>(int)$row['element_version_id'],
        'line_kind'=>(string)$row['line_kind'],
        'currency'=>(string)$row['currency'],
        'amount'=>(string)$row['amount'],
        'quantity'=>(string)$row['quantity'],
        'rate'=>(string)$row['rate'],
        'source_sha256'=>(string)$row['source_sha256'],
    ));
}

function hrm_pay_core_004_validate_trace($row)
{
    if (!is_array($row))
        return false;
    foreach (array('trace_seq','trace_kind','artifact_sha256','input_sha256','output_sha256','value_mode') as $key)
        if (!array_key_exists($key, $row)) return false;
    if ((int)$row['trace_seq'] < 1 || !is_string($row['trace_kind']) || preg_match('/^[A-Za-z0-9_.:-]{1,64}$/D', $row['trace_kind']) !== 1)
        return false;
    if (!hrm_pay_core_004_is_sha256($row['artifact_sha256']) || !hrm_pay_core_004_is_sha256($row['input_sha256']) || !hrm_pay_core_004_is_sha256($row['output_sha256']))
        return false;
    if (!in_array($row['value_mode'], hrm_pay_core_004_trace_value_modes(), true))
        return false;
    if ($row['value_mode'] === 'none')
        return !isset($row['value_digest']) || $row['value_digest'] === null || $row['value_digest'] === '';
    return isset($row['value_digest']) && hrm_pay_core_004_is_sha256($row['value_digest']);
}

function hrm_pay_core_004_trace_sha256($row)
{
    if (!hrm_pay_core_004_validate_trace($row))
        return false;
    return hrm_pay_core_004_hash(array(
        'trace_seq'=>(int)$row['trace_seq'],
        'trace_kind'=>(string)$row['trace_kind'],
        'artifact_sha256'=>(string)$row['artifact_sha256'],
        'input_sha256'=>(string)$row['input_sha256'],
        'output_sha256'=>(string)$row['output_sha256'],
        'value_mode'=>(string)$row['value_mode'],
        'value_digest'=>(isset($row['value_digest']) && $row['value_digest'] !== '') ? $row['value_digest'] : null,
    ));
}

function hrm_pay_core_004_validate_result_descriptor($row)
{
    if (!is_array($row))
        return false;
    foreach (array('snapshot_sha256','currency','gross_amount','deduction_amount','net_amount','line_set_sha256','trace_set_sha256') as $key)
        if (!array_key_exists($key, $row)) return false;
    if (!hrm_pay_core_004_is_sha256($row['snapshot_sha256']) || !hrm_pay_core_004_is_sha256($row['line_set_sha256']) || !hrm_pay_core_004_is_sha256($row['trace_set_sha256']))
        return false;
    if (!is_string($row['currency']) || preg_match('/^[A-Z]{3}$/D', $row['currency']) !== 1)
        return false;
    foreach (array('gross_amount','deduction_amount','net_amount') as $key)
        if (!is_int($row[$key]) && !is_float($row[$key]) && !(is_string($row[$key]) && preg_match('/^-?\d+(?:\.\d+)?$/D', $row[$key]) === 1)) return false;
    return true;
}

function hrm_pay_core_004_result_sha256($row)
{
    if (!hrm_pay_core_004_validate_result_descriptor($row))
        return false;
    return hrm_pay_core_004_hash(array(
        'snapshot_sha256'=>(string)$row['snapshot_sha256'],
        'currency'=>(string)$row['currency'],
        'gross_amount'=>(string)$row['gross_amount'],
        'deduction_amount'=>(string)$row['deduction_amount'],
        'net_amount'=>(string)$row['net_amount'],
        'line_set_sha256'=>(string)$row['line_set_sha256'],
        'trace_set_sha256'=>(string)$row['trace_set_sha256'],
    ));
}

function hrm_pay_core_004_verify_result_bundle($snapshot_sha256, $result, $lines, $traces)
{
    if (!hrm_pay_core_004_is_sha256($snapshot_sha256) || !is_array($result) || !is_array($lines) || !is_array($traces))
        return false;
    $line_hashes = array();
    foreach ($lines as $line) {
        $hash = hrm_pay_core_004_result_line_sha256($line);
        if ($hash === false) return false;
        $line_hashes[] = $hash;
    }
    $trace_hashes = array();
    foreach ($traces as $trace) {
        $hash = hrm_pay_core_004_trace_sha256($trace);
        if ($hash === false) return false;
        $trace_hashes[] = $hash;
    }
    $expected_lines = hrm_pay_core_004_hash_set($line_hashes);
    $expected_traces = hrm_pay_core_004_hash_set($trace_hashes);
    if ($expected_lines === false || $expected_traces === false)
        return false;
    if (!isset($result['snapshot_sha256']) || !hash_equals($snapshot_sha256, (string)$result['snapshot_sha256']))
        return false;
    if (!isset($result['line_set_sha256']) || !hash_equals($expected_lines, (string)$result['line_set_sha256']))
        return false;
    if (!isset($result['trace_set_sha256']) || !hash_equals($expected_traces, (string)$result['trace_set_sha256']))
        return false;
    return hrm_pay_core_004_result_sha256($result);
}
