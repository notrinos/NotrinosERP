<?php
/**
 * HRM-CMP-001 governed base-compensation contract.
 *
 * This first compensation milestone owns effective-dated base ranges, reviewed
 * opening custody, review-cycle budgets and approved base awards. It does not
 * calculate final payroll, tax, accounting entries or payments.
 */
if (!defined('HRM_CMP_001_CONTRACT_VERSION')) define('HRM_CMP_001_CONTRACT_VERSION', 'hrm-cmp-001-base-award-v1');
if (!defined('HRM_CMP_001_PAYROLL_ADAPTER_VERSION')) define('HRM_CMP_001_PAYROLL_ADAPTER_VERSION', 'compensation-pay-core-003-v1');
if (!defined('HRM_CMP_001_PAYROLL_CALCULATION_AUTHORITY_ALLOWED')) define('HRM_CMP_001_PAYROLL_CALCULATION_AUTHORITY_ALLOWED', false);
if (!defined('HRM_CMP_001_PAYROLL_POSTING_AUTHORITY_ALLOWED')) define('HRM_CMP_001_PAYROLL_POSTING_AUTHORITY_ALLOWED', false);
if (!defined('HRM_CMP_001_GENERAL_LEDGER_AUTHORITY_ALLOWED')) define('HRM_CMP_001_GENERAL_LEDGER_AUTHORITY_ALLOWED', false);
if (!defined('HRM_CMP_001_PAYMENT_AUTHORITY_ALLOWED')) define('HRM_CMP_001_PAYMENT_AUTHORITY_ALLOWED', false);
if (!defined('HRM_CMP_001_TAX_AUTHORITY_ALLOWED')) define('HRM_CMP_001_TAX_AUTHORITY_ALLOWED', false);
if (!defined('HRM_CMP_001_HISTORICAL_REWRITE_ALLOWED')) define('HRM_CMP_001_HISTORICAL_REWRITE_ALLOWED', false);

/** Return a YYYY-MM-DD date or false. */
function hrm_cmp_001_date($value)
{
    $value = trim((string)$value);
    if (!preg_match('/^\d{4}-\d{2}-\d{2}$/', $value)) return false;
    $p = explode('-', $value);
    return checkdate((int)$p[1], (int)$p[2], (int)$p[0]) ? $value : false;
}

/** Return a bounded machine token or false. */
function hrm_cmp_001_token($value, $max_len)
{
    $value = trim((string)$value);
    if ($value === '' || strlen($value) > (int)$max_len || !preg_match('/^[A-Za-z0-9][A-Za-z0-9_.:-]*$/', $value)) return false;
    return $value;
}

/** Return a lowercase SHA-256 digest or false. */
function hrm_cmp_001_sha256($value)
{
    $value = strtolower(trim((string)$value));
    return preg_match('/^[0-9a-f]{64}$/', $value) ? $value : false;
}

/** Validate explicit ISO-like currency custody without providing a default. */
function hrm_cmp_001_currency($value)
{
    $value = strtoupper(trim((string)$value));
    return preg_match('/^[A-Z]{3}$/', $value) ? $value : false;
}

/** Validate an explicit pay basis token. No conversion semantics are inferred. */
function hrm_cmp_001_pay_basis($value)
{
    return hrm_cmp_001_token($value, 24);
}

/** Normalize a non-negative DECIMAL(24,6) without floating-point arithmetic. */
function hrm_cmp_001_decimal($value)
{
    $value = trim((string)$value);
    if (!preg_match('/^\+?(\d{1,18})(?:\.(\d{1,6}))?$/', $value, $m)) return false;
    $whole = ltrim($m[1], '0');
    if ($whole === '') $whole = '0';
    $fraction = isset($m[2]) ? $m[2] : '';
    $fraction = str_pad($fraction, 6, '0');
    return $whole.'.'.$fraction;
}

/** Convert a normalized amount to an unsigned micro-unit decimal string. */
function hrm_cmp_001_decimal_micros($value)
{
    $n = hrm_cmp_001_decimal($value);
    if ($n === false) return false;
    $p = explode('.', $n);
    return ltrim($p[0].$p[1], '0') === '' ? '0' : ltrim($p[0].$p[1], '0');
}

/** Compare two non-negative exact decimals. */
function hrm_cmp_001_decimal_compare($a, $b)
{
    $a = hrm_cmp_001_decimal_micros($a);
    $b = hrm_cmp_001_decimal_micros($b);
    if ($a === false || $b === false) return false;
    $la = strlen($a); $lb = strlen($b);
    if ($la !== $lb) return $la < $lb ? -1 : 1;
    if ($a === $b) return 0;
    return strcmp($a, $b) < 0 ? -1 : 1;
}

/** Add two exact non-negative DECIMAL(24,6) values. */
function hrm_cmp_001_decimal_add($a, $b)
{
    $a = hrm_cmp_001_decimal_micros($a); $b = hrm_cmp_001_decimal_micros($b);
    if ($a === false || $b === false) return false;
    $i = strlen($a)-1; $j = strlen($b)-1; $carry = 0; $out = '';
    while ($i >= 0 || $j >= 0 || $carry) {
        $sum = $carry + ($i >= 0 ? (int)$a[$i--] : 0) + ($j >= 0 ? (int)$b[$j--] : 0);
        $out = (string)($sum % 10).$out; $carry = (int)floor($sum / 10);
    }
    $out = str_pad($out, 7, '0', STR_PAD_LEFT);
    return substr($out, 0, -6).'.'.substr($out, -6);
}

/** Subtract b from a, rejecting negative results. */
function hrm_cmp_001_decimal_subtract($a, $b)
{
    if (hrm_cmp_001_decimal_compare($a, $b) < 0) return false;
    $a = hrm_cmp_001_decimal_micros($a); $b = str_pad(hrm_cmp_001_decimal_micros($b), strlen($a), '0', STR_PAD_LEFT);
    $borrow = 0; $out = '';
    for ($i = strlen($a)-1; $i >= 0; $i--) {
        $n = (int)$a[$i] - $borrow - (int)$b[$i];
        if ($n < 0) { $n += 10; $borrow = 1; } else $borrow = 0;
        $out = (string)$n.$out;
    }
    $out = ltrim($out, '0'); if ($out === '') $out = '0';
    $out = str_pad($out, 7, '0', STR_PAD_LEFT);
    return substr($out, 0, -6).'.'.substr($out, -6);
}

/** Canonical deterministic digest for governed evidence. */
function hrm_cmp_001_hash($value)
{
    if (is_array($value)) {
        $keys = array_keys($value);
        $assoc = $keys !== range(0, count($value)-1);
        if ($assoc) ksort($value, SORT_STRING);
        foreach ($value as $k => $v) $value[$k] = is_array($v) ? hrm_cmp_001_canonicalize($v) : $v;
    }
    $json = json_encode($value, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
    return is_string($json) ? hash('sha256', $json) : false;
}
function hrm_cmp_001_canonicalize($value)
{
    if (!is_array($value)) return $value;
    $keys = array_keys($value); $assoc = $keys !== range(0, count($value)-1);
    if ($assoc) ksort($value, SORT_STRING);
    foreach ($value as $k => $v) $value[$k] = hrm_cmp_001_canonicalize($v);
    return $value;
}

/** Validate an effective-dated salary-range version. */
function hrm_cmp_001_validate_range(array $x)
{
    foreach (array('legal_entity_id','grade_id','range_code','version_token','effective_from','currency_code','pay_basis','minimum_amount','midpoint_amount','maximum_amount','source_kind','source_evidence_sha256') as $k)
        if (!array_key_exists($k, $x)) return false;
    $from = hrm_cmp_001_date($x['effective_from']);
    $to = empty($x['effective_to']) ? null : hrm_cmp_001_date($x['effective_to']);
    $min = hrm_cmp_001_decimal($x['minimum_amount']); $mid = hrm_cmp_001_decimal($x['midpoint_amount']); $max = hrm_cmp_001_decimal($x['maximum_amount']);
    return (int)$x['legal_entity_id'] > 0 && (int)$x['grade_id'] > 0
        && hrm_cmp_001_token($x['range_code'], 64) !== false && hrm_cmp_001_token($x['version_token'], 64) !== false
        && $from !== false && ($to === null || ($to !== false && $to >= $from))
        && hrm_cmp_001_currency($x['currency_code']) !== false && hrm_cmp_001_pay_basis($x['pay_basis']) !== false
        && $min !== false && $mid !== false && $max !== false
        && hrm_cmp_001_decimal_compare($min, $mid) <= 0 && hrm_cmp_001_decimal_compare($mid, $max) <= 0
        && in_array((string)$x['source_kind'], array('employer','collective','legacy_review','governed_correction'), true)
        && hrm_cmp_001_sha256($x['source_evidence_sha256']) !== false;
}

/** Resolve exactly one matching effective range or fail closed. */
function hrm_cmp_001_resolve_range_candidates(array $rows, $date)
{
    $date = hrm_cmp_001_date($date); if ($date === false) return false;
    $active = array();
    foreach ($rows as $r) {
        if (!hrm_cmp_001_validate_range($r)) return false;
        if ($r['effective_from'] <= $date && (empty($r['effective_to']) || $r['effective_to'] >= $date)) $active[] = $r;
    }
    return count($active) === 1 ? $active[0] : false;
}

/** Validate a first-release base-award kind. */
function hrm_cmp_001_award_kind($value)
{
    $value = trim((string)$value);
    return in_array($value, array('base_merit','base_promotion','base_adjustment'), true) ? $value : false;
}

/** Validate that an award stays within an exact range and does not reduce base pay. */
function hrm_cmp_001_validate_base_award($old_amount, $new_amount, array $range)
{
    if (!hrm_cmp_001_validate_range($range)) return false;
    $old = hrm_cmp_001_decimal($old_amount); $new = hrm_cmp_001_decimal($new_amount);
    if ($old === false || $new === false || hrm_cmp_001_decimal_compare($new, $old) <= 0) return false;
    if (hrm_cmp_001_decimal_compare($new, $range['minimum_amount']) < 0 || hrm_cmp_001_decimal_compare($new, $range['maximum_amount']) > 0) return false;
    return array('old_amount'=>$old,'new_amount'=>$new,'budget_impact'=>hrm_cmp_001_decimal_subtract($new, $old));
}

/** Base-award first deliberately excludes bonus/benefit/allowance/total-reward editing. */
function hrm_cmp_001_base_award_scope_only()
{
    return array('base_merit','base_promotion','base_adjustment');
}
?>
