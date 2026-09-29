<?php
/**
 * HRM-ANL-001 bounded headcount metric contract.
 *
 * This contract is intentionally narrow: one aggregate active-headcount metric
 * aligned to report 892 semantics, exact dual-query reconciliation, a fixed
 * small-cohort policy, no employee identifiers in results, and no export or
 * drill-down authority.
 */
if (!defined('HRM_ANL_001_HEADCOUNT_METRIC_KEY')) define('HRM_ANL_001_HEADCOUNT_METRIC_KEY', 'workforce.active_headcount.total');
if (!defined('HRM_ANL_001_HEADCOUNT_DEFINITION_VERSION')) define('HRM_ANL_001_HEADCOUNT_DEFINITION_VERSION', '1');
if (!defined('HRM_ANL_001_HEADCOUNT_POLICY_KEY')) define('HRM_ANL_001_HEADCOUNT_POLICY_KEY', 'workforce.small_cohort.default');
if (!defined('HRM_ANL_001_HEADCOUNT_POLICY_VERSION')) define('HRM_ANL_001_HEADCOUNT_POLICY_VERSION', '1');
if (!defined('HRM_ANL_001_HEADCOUNT_MINIMUM_COHORT')) define('HRM_ANL_001_HEADCOUNT_MINIMUM_COHORT', 5);
if (!defined('HRM_ANL_001_HEADCOUNT_PURPOSE')) define('HRM_ANL_001_HEADCOUNT_PURPOSE', 'hrm_report_892_headcount');
if (!defined('HRM_ANL_001_HEADCOUNT_CURRENCY')) define('HRM_ANL_001_HEADCOUNT_CURRENCY', 'XXX');

/** @return bool */
function hrm_anl_001_headcount_is_assoc($value)
{
    return is_array($value) && count($value) > 0 && array_keys($value) !== range(0, count($value) - 1);
}

/** @return mixed */
function hrm_anl_001_headcount_normalize($value)
{
    if (is_object($value)) $value = get_object_vars($value);
    if (is_array($value)) {
        $out = array();
        if (hrm_anl_001_headcount_is_assoc($value)) {
            $keys = array_keys($value);
            sort($keys, SORT_STRING);
            foreach ($keys as $key) $out[(string)$key] = hrm_anl_001_headcount_normalize($value[$key]);
        } else {
            foreach ($value as $item) $out[] = hrm_anl_001_headcount_normalize($item);
        }
        return $out;
    }
    if (is_float($value)) {
        $formatted = rtrim(rtrim(sprintf('%.12F', $value), '0'), '.');
        return $formatted === '-0' ? '0' : $formatted;
    }
    if (is_int($value) || is_bool($value) || $value === null) return $value;
    return (string)$value;
}

/** @return string|false */
function hrm_anl_001_headcount_canonical_json($value)
{
    $flags = 0;
    if (defined('JSON_UNESCAPED_SLASHES')) $flags |= JSON_UNESCAPED_SLASHES;
    if (defined('JSON_UNESCAPED_UNICODE')) $flags |= JSON_UNESCAPED_UNICODE;
    $json = json_encode(hrm_anl_001_headcount_normalize($value), $flags);
    return is_string($json) ? $json : false;
}

/** @return string|false */
function hrm_anl_001_headcount_hash($value)
{
    $json = hrm_anl_001_headcount_canonical_json($value);
    return $json === false ? false : hash('sha256', $json);
}

/** @return bool */
function hrm_anl_001_headcount_sha256($value)
{
    return is_string($value) && preg_match('/^[a-f0-9]{64}$/D', $value) === 1;
}

/** @return bool */
function hrm_anl_001_headcount_datetime($value)
{
    if (!is_string($value) || preg_match('/^(\d{4})-(\d{2})-(\d{2}) (\d{2}):(\d{2}):(\d{2})$/D', $value, $m) !== 1) return false;
    return checkdate((int)$m[2], (int)$m[3], (int)$m[1])
        && (int)$m[4] <= 23 && (int)$m[5] <= 59 && (int)$m[6] <= 59;
}

/** @return bool */
function hrm_anl_001_headcount_valid_window($valid_from, $expires_at)
{
    return hrm_anl_001_headcount_datetime($valid_from)
        && hrm_anl_001_headcount_datetime($expires_at)
        && strcmp($valid_from, $expires_at) < 0;
}

/** @return array */
function hrm_anl_001_headcount_source_contract()
{
    return array(
        'report_id' => 892,
        'report_source' => 'hrm/reporting/rep_headcount.php',
        'source_table' => 'employees',
        'metric_scope' => 'current_company',
        'active_predicate' => 'inactive=0',
        'hire_date_semantics' => 'null_or_zero_or_lte_as_of_utc_date',
        'released_date_semantics' => 'null_or_zero_or_empty_or_gt_as_of_utc_date',
        'primary_aggregation' => 'count_rows',
        'independent_reconciliation' => 'sum_department_counts',
        'result_contains_employee_identifiers' => false,
        'export_allowed' => false,
        'drilldown_allowed' => false,
    );
}

/** @return array */
function hrm_anl_001_headcount_field_policy()
{
    return array(
        'allowed_source_fields' => array('department_id', 'hire_date', 'released_date', 'inactive'),
        'result_fields' => array('metric_key', 'as_of_utc', 'currency_code', 'value', 'unit_code', 'suppressed', 'cohort_size_band_code'),
        'direct_employee_identifiers_allowed' => false,
        'employee_name_allowed' => false,
        'contact_identity_bank_fields_allowed' => false,
    );
}

/** @return array */
function hrm_anl_001_headcount_object_policy()
{
    return array(
        'scope_type' => 'company',
        'scope_source' => 'authenticated_session',
        'cross_company_allowed' => false,
        'department_drilldown_allowed' => false,
        'employee_drilldown_allowed' => false,
    );
}

/** @return array|false */
function hrm_anl_001_headcount_metric_definition()
{
    $source_hash = hrm_anl_001_headcount_hash(hrm_anl_001_headcount_source_contract());
    $field_hash = hrm_anl_001_headcount_hash(hrm_anl_001_headcount_field_policy());
    $object_hash = hrm_anl_001_headcount_hash(hrm_anl_001_headcount_object_policy());
    if ($source_hash === false || $field_hash === false || $object_hash === false) return false;
    $definition = array(
        'metric_key' => HRM_ANL_001_HEADCOUNT_METRIC_KEY,
        'definition_version' => HRM_ANL_001_HEADCOUNT_DEFINITION_VERSION,
        'source_model_code' => 'hrm.report.892.active_headcount',
        'value_type_code' => 'integer',
        'unit_code' => 'people',
        'currency_semantics_code' => 'not_applicable_xxx',
        'as_of_semantics_code' => 'employee_effective_date_utc_day',
        'aggregation_code' => 'exact_count_reconciled',
        'source_contract_sha256' => $source_hash,
        'field_policy_sha256' => $field_hash,
        'object_policy_sha256' => $object_hash,
    );
    $definition['definition_sha256'] = hrm_anl_001_headcount_hash($definition);
    return $definition;
}

/** @return array|false */
function hrm_anl_001_headcount_suppression_policy()
{
    $policy = array(
        'policy_key' => HRM_ANL_001_HEADCOUNT_POLICY_KEY,
        'policy_version' => HRM_ANL_001_HEADCOUNT_POLICY_VERSION,
        'minimum_cohort_size' => HRM_ANL_001_HEADCOUNT_MINIMUM_COHORT,
        'suppress_exact_counts' => 1,
        'export_allowed' => 0,
    );
    $policy['policy_sha256'] = hrm_anl_001_headcount_hash($policy);
    return $policy;
}

/** @return array|false */
function hrm_anl_001_headcount_metric_definition_for_company($company_id)
{
    $company_id = (int)$company_id;
    if ($company_id < 0) return false;
    $definition = hrm_anl_001_headcount_metric_definition();
    if ($definition === false) return false;
    unset($definition['definition_sha256']);
    $definition['definition_sha256'] = hrm_anl_001_headcount_hash(array(
        'company_id' => $company_id,
        'definition' => $definition,
    ));
    return $definition;
}

/** @return array|false */
function hrm_anl_001_headcount_suppression_policy_for_company($company_id)
{
    $company_id = (int)$company_id;
    if ($company_id < 0) return false;
    $policy = hrm_anl_001_headcount_suppression_policy();
    if ($policy === false) return false;
    unset($policy['policy_sha256']);
    $policy['policy_sha256'] = hrm_anl_001_headcount_hash(array(
        'company_id' => $company_id,
        'policy' => $policy,
    ));
    return $policy;
}

/** @return array|false */
function hrm_anl_001_headcount_scope_hashes($company_id)
{
    $company_id = (int)$company_id;
    if ($company_id < 0) return false;
    $filter = hrm_anl_001_headcount_hash(array('filter' => 'none'));
    $object = hrm_anl_001_headcount_hash(array('scope_type' => 'company', 'company_id' => $company_id));
    $field = hrm_anl_001_headcount_hash(hrm_anl_001_headcount_field_policy());
    if ($filter === false || $object === false || $field === false) return false;
    return array('filter_sha256' => $filter, 'object_scope_sha256' => $object, 'field_scope_sha256' => $field);
}

/** @return array|false */
function hrm_anl_001_headcount_reconcile($primary_count, array $department_counts, $minimum_cohort_size, &$error = null)
{
    $error = null;
    if (!is_int($primary_count) && !(is_string($primary_count) && preg_match('/^\d+$/D', $primary_count) === 1)) {
        $error = 'primary_count_invalid';
        return false;
    }
    $primary_count = (int)$primary_count;
    $minimum_cohort_size = (int)$minimum_cohort_size;
    if ($primary_count < 0 || $minimum_cohort_size < 1) {
        $error = 'headcount_bounds_invalid';
        return false;
    }
    $normalized = array();
    $sum = 0;
    foreach ($department_counts as $row) {
        if (!is_array($row) || !array_key_exists('department_id', $row) || !array_key_exists('active_count', $row)) {
            $error = 'department_count_shape_invalid';
            return false;
        }
        $department_id = (int)$row['department_id'];
        $count = $row['active_count'];
        if ($department_id < 0 || (!is_int($count) && !(is_string($count) && preg_match('/^\d+$/D', $count) === 1))) {
            $error = 'department_count_value_invalid';
            return false;
        }
        $count = (int)$count;
        if ($count < 0) {
            $error = 'department_count_value_invalid';
            return false;
        }
        $normalized[] = array('department_id' => $department_id, 'active_count' => $count);
        $sum += $count;
    }
    usort($normalized, 'hrm_anl_001_headcount_department_compare');
    if ($sum !== $primary_count) {
        $error = 'exact_source_total_mismatch';
        return false;
    }
    $suppressed = $primary_count < $minimum_cohort_size;
    if ($primary_count === 0) $band = 'zero';
    elseif ($suppressed) $band = 'below_threshold';
    else $band = 'threshold_or_above';
    $source_total_sha = hrm_anl_001_headcount_hash(array(
        'metric_key' => HRM_ANL_001_HEADCOUNT_METRIC_KEY,
        'primary_count' => $primary_count,
        'independent_count' => $sum,
        'department_counts' => $normalized,
    ));
    if ($source_total_sha === false) {
        $error = 'source_total_hash_failed';
        return false;
    }
    return array(
        'source_total' => $primary_count,
        'value' => $suppressed ? null : $primary_count,
        'unit_code' => 'people',
        'suppressed' => $suppressed,
        'cohort_size_band_code' => $band,
        'source_total_sha256' => $source_total_sha,
        'department_counts' => $normalized,
    );
}

/** @return int */
function hrm_anl_001_headcount_department_compare($left, $right)
{
    $a = isset($left['department_id']) ? (int)$left['department_id'] : -1;
    $b = isset($right['department_id']) ? (int)$right['department_id'] : -1;
    if ($a === $b) return 0;
    return $a < $b ? -1 : 1;
}
?>
