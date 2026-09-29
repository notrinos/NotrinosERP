<?php
/**
 * HRM-ANL-001 bounded department headcount aggregate contract.
 *
 * This is a grouped view of the already-approved report-892-compatible active
 * headcount metric. It is not employee drill-down: rows are department labels
 * plus suppressed aggregate counts only. Exact values below the fixed cohort
 * threshold are hidden and one complementary cohort is secondarily suppressed
 * when a single primary suppression would otherwise be derivable by subtraction.
 */
if (!defined('HRM_ANL_001_DEPARTMENT_HEADCOUNT_PURPOSE')) define('HRM_ANL_001_DEPARTMENT_HEADCOUNT_PURPOSE', 'hrm_report_892_department_headcount');

/** @return array */
function hrm_anl_001_department_headcount_source_contract()
{
    return array(
        'report_id' => 892,
        'report_source' => 'hrm/reporting/rep_headcount.php',
        'source_tables' => array('employees', 'departments'),
        'metric_key' => HRM_ANL_001_HEADCOUNT_METRIC_KEY,
        'grouping_semantics' => "COALESCE(department_name,'Unassigned')",
        'active_predicate' => 'inactive=0',
        'hire_date_semantics' => 'null_or_zero_or_lte_as_of_utc_date',
        'released_date_semantics' => 'null_or_zero_or_empty_or_gt_as_of_utc_date',
        'primary_reconciliation' => 'exact_total_headcount',
        'complementary_suppression_required' => true,
        'result_contains_employee_identifiers' => false,
        'export_allowed' => false,
        'drilldown_allowed' => false,
    );
}

/** @return array */
function hrm_anl_001_department_headcount_field_policy()
{
    return array(
        'allowed_source_fields' => array('department_name', 'hire_date', 'released_date', 'inactive'),
        'result_fields' => array('department_name', 'value', 'unit_code', 'suppressed', 'suppression_reason', 'cohort_size_band_code'),
        'direct_employee_identifiers_allowed' => false,
        'employee_name_allowed' => false,
        'contact_identity_bank_fields_allowed' => false,
    );
}

/** @return array */
function hrm_anl_001_department_headcount_object_policy()
{
    return array(
        'scope_type' => 'company_department_aggregate',
        'scope_source' => 'authenticated_session',
        'cross_company_allowed' => false,
        'department_aggregate_allowed' => true,
        'department_record_drilldown_allowed' => false,
        'employee_drilldown_allowed' => false,
    );
}

/** @return array|false */
function hrm_anl_001_department_headcount_scope_hashes($company_id)
{
    $company_id = (int)$company_id;
    if ($company_id < 0) return false;
    $filter = hrm_anl_001_headcount_hash(array('dimension' => 'department', 'filter' => 'all_departments'));
    $object = hrm_anl_001_headcount_hash(array('scope_type' => 'company_department_aggregate', 'company_id' => $company_id));
    $field = hrm_anl_001_headcount_hash(hrm_anl_001_department_headcount_field_policy());
    if ($filter === false || $object === false || $field === false) return false;
    return array('filter_sha256' => $filter, 'object_scope_sha256' => $object, 'field_scope_sha256' => $field);
}

/** @return int */
function hrm_anl_001_department_headcount_name_compare($left, $right)
{
    return strcmp((string)$left['department_name'], (string)$right['department_name']);
}

/**
 * Reconcile and suppress a report-892-compatible department aggregate.
 *
 * @param int|string $primary_count Exact company active-headcount total.
 * @param array $department_rows Rows with department_name and active_count.
 * @param int $minimum_cohort_size
 * @param string|null $error
 * @return array|false
 */
function hrm_anl_001_department_headcount_reconcile($primary_count, array $department_rows, $minimum_cohort_size, &$error = null)
{
    $error = null;
    if (!is_int($primary_count) && !(is_string($primary_count) && preg_match('/^\d+$/D', $primary_count) === 1)) {
        $error = 'primary_count_invalid';
        return false;
    }
    $primary_count = (int)$primary_count;
    $minimum_cohort_size = (int)$minimum_cohort_size;
    if ($primary_count < 0 || $minimum_cohort_size < 1) {
        $error = 'department_headcount_bounds_invalid';
        return false;
    }

    $normalized = array();
    $names = array();
    $sum = 0;
    foreach ($department_rows as $row) {
        if (!is_array($row) || !array_key_exists('department_name', $row) || !array_key_exists('active_count', $row)) {
            $error = 'department_headcount_shape_invalid';
            return false;
        }
        $name = trim((string)$row['department_name']);
        $count = $row['active_count'];
        if ($name === '' || strlen($name) > 255 || preg_match('/[\x00-\x08\x0B\x0C\x0E-\x1F]/', $name)
            || (!is_int($count) && !(is_string($count) && preg_match('/^\d+$/D', $count) === 1))) {
            $error = 'department_headcount_value_invalid';
            return false;
        }
        $count = (int)$count;
        if ($count < 0 || isset($names[$name])) {
            $error = $count < 0 ? 'department_headcount_value_invalid' : 'department_headcount_duplicate_label';
            return false;
        }
        $names[$name] = true;
        $normalized[] = array('department_name' => $name, 'active_count' => $count);
        $sum += $count;
    }
    usort($normalized, 'hrm_anl_001_department_headcount_name_compare');
    if ($sum !== $primary_count) {
        $error = 'exact_source_total_mismatch';
        return false;
    }

    $primary_suppressed = array();
    foreach ($normalized as $idx => $row) {
        if ((int)$row['active_count'] > 0 && (int)$row['active_count'] < $minimum_cohort_size)
            $primary_suppressed[] = $idx;
    }

    $secondary_idx = null;
    if (count($primary_suppressed) === 1) {
        $best_count = null;
        foreach ($normalized as $idx => $row) {
            if ($idx === $primary_suppressed[0] || (int)$row['active_count'] < $minimum_cohort_size) continue;
            $count = (int)$row['active_count'];
            if ($best_count === null || $count < $best_count) {
                $best_count = $count;
                $secondary_idx = $idx;
            }
        }
        if ($secondary_idx === null) {
            $error = 'complementary_suppression_unavailable';
            return false;
        }
    }

    $public_rows = array();
    $suppressed_count = 0;
    foreach ($normalized as $idx => $row) {
        $count = (int)$row['active_count'];
        $primary = in_array($idx, $primary_suppressed, true);
        $secondary = $secondary_idx !== null && $idx === $secondary_idx;
        $suppressed = $primary || $secondary;
        if ($suppressed) $suppressed_count++;
        if ($count === 0) $band = 'zero';
        elseif ($count < $minimum_cohort_size) $band = 'below_threshold';
        else $band = 'threshold_or_above';
        $public_rows[] = array(
            'department_name' => (string)$row['department_name'],
            'value' => $suppressed ? null : $count,
            'unit_code' => 'people',
            'suppressed' => $suppressed,
            'suppression_reason' => $primary ? 'primary_small_cohort' : ($secondary ? 'secondary_complementary' : 'none'),
            'cohort_size_band_code' => $band,
        );
    }

    $source_total_sha = hrm_anl_001_headcount_hash(array(
        'metric_key' => HRM_ANL_001_HEADCOUNT_METRIC_KEY,
        'primary_count' => $primary_count,
        'department_rows' => $normalized,
    ));
    if ($source_total_sha === false) {
        $error = 'source_total_hash_failed';
        return false;
    }

    return array(
        'source_total' => $primary_count,
        'rows' => $public_rows,
        'raw_department_rows' => $normalized,
        'row_count' => count($public_rows),
        'suppressed_row_count' => $suppressed_count,
        'suppression_applied' => $suppressed_count > 0,
        'cohort_size_band_code' => $suppressed_count > 0 ? 'mixed_or_suppressed' : 'threshold_or_above',
        'source_total_sha256' => $source_total_sha,
    );
}
?>
