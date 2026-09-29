<?php
/**
 * HRM-ANL-001 bounded department x employment-type headcount matrix contract.
 *
 * This is a two-dimensional aggregate of the already-approved report-892
 * active-headcount metric. It intentionally exposes no row/column marginals,
 * employee detail, arbitrary filters, export or drill-down. To prevent a user
 * from reconstructing a small cell by subtracting other matrix values, the
 * presence of any non-zero cell below the minimum cohort threshold suppresses
 * every non-zero cell in the returned matrix. Zero cells may remain visible.
 */
if (!defined('HRM_ANL_001_DEPARTMENT_EMPLOYMENT_TYPE_HEADCOUNT_PURPOSE')) define('HRM_ANL_001_DEPARTMENT_EMPLOYMENT_TYPE_HEADCOUNT_PURPOSE', 'hrm_report_892_department_employment_type_headcount');

/** @return array */
function hrm_anl_001_department_employment_type_headcount_source_contract()
{
    return array(
        'report_id' => 892,
        'report_source' => 'hrm/reporting/rep_headcount.php',
        'source_tables' => array('employees','departments'),
        'metric_key' => HRM_ANL_001_HEADCOUNT_METRIC_KEY,
        'department_grouping_semantics' => "COALESCE(department_name,'Unassigned')",
        'employment_type_grouping_semantics' => 'employees.employment_type mapped to fixed permanent/contract/probation/intern/part_time/other buckets',
        'known_employment_type_codes' => array(0,1,2,3,4),
        'unknown_code_bucket' => 'other',
        'active_predicate' => 'inactive=0',
        'hire_date_semantics' => 'null_or_zero_or_lte_as_of_utc_date',
        'released_date_semantics' => 'null_or_zero_or_empty_or_gt_as_of_utc_date',
        'primary_reconciliation' => 'exact_total_headcount',
        'matrix_wide_nonzero_suppression_on_any_small_cell_required' => true,
        'row_column_marginals_allowed' => false,
        'result_contains_employee_identifiers' => false,
        'export_allowed' => false,
        'drilldown_allowed' => false,
    );
}

/** @return array */
function hrm_anl_001_department_employment_type_headcount_field_policy()
{
    return array(
        'allowed_source_fields' => array('department_id','department_name','employment_type','hire_date','released_date','inactive'),
        'result_fields' => array('department_name','employment_type_code','value','unit_code','suppressed','suppression_reason','cohort_size_band_code'),
        'row_column_marginals_allowed' => false,
        'direct_employee_identifiers_allowed' => false,
        'employee_name_allowed' => false,
        'contact_identity_bank_fields_allowed' => false,
    );
}

/** @return array */
function hrm_anl_001_department_employment_type_headcount_object_policy()
{
    return array(
        'scope_type' => 'company_department_employment_type_aggregate',
        'scope_source' => 'authenticated_session',
        'cross_company_allowed' => false,
        'department_employment_type_aggregate_allowed' => true,
        'department_record_drilldown_allowed' => false,
        'employment_type_record_drilldown_allowed' => false,
        'employee_drilldown_allowed' => false,
    );
}

/** @return array|false */
function hrm_anl_001_department_employment_type_headcount_scope_hashes($company_id)
{
    $company_id = (int)$company_id;
    if ($company_id < 0) return false;
    $filter = hrm_anl_001_headcount_hash(array('dimension' => 'department_employment_type', 'filter' => 'all_departments_all_employment_types'));
    $object = hrm_anl_001_headcount_hash(array('scope_type' => 'company_department_employment_type_aggregate', 'company_id' => $company_id));
    $field = hrm_anl_001_headcount_hash(hrm_anl_001_department_employment_type_headcount_field_policy());
    if ($filter === false || $object === false || $field === false) return false;
    return array('filter_sha256' => $filter, 'object_scope_sha256' => $object, 'field_scope_sha256' => $field);
}

/** @return int */
function hrm_anl_001_department_employment_type_headcount_compare($left, $right)
{
    $d = strcmp((string)$left['department_name'], (string)$right['department_name']);
    if ($d !== 0) return $d < 0 ? -1 : 1;
    $order = hrm_anl_001_employment_type_bucket_order();
    $a = isset($order[$left['employment_type_code']]) ? $order[$left['employment_type_code']] : 99;
    $b = isset($order[$right['employment_type_code']]) ? $order[$right['employment_type_code']] : 99;
    if ($a === $b) return strcmp((string)$left['employment_type_code'], (string)$right['employment_type_code']);
    return $a < $b ? -1 : 1;
}

/**
 * Reconcile and conservatively suppress a report-892-compatible two-dimensional matrix.
 * @return array|false
 */
function hrm_anl_001_department_employment_type_headcount_reconcile($primary_count, array $matrix_rows, $minimum_cohort_size, &$error = null)
{
    $error = null;
    if (!is_int($primary_count) && !(is_string($primary_count) && preg_match('/^\\d+$/D', $primary_count) === 1)) {
        $error = 'primary_count_invalid'; return false;
    }
    $primary_count = (int)$primary_count;
    $minimum_cohort_size = (int)$minimum_cohort_size;
    if ($primary_count < 0 || $minimum_cohort_size < 1) { $error = 'department_employment_type_headcount_bounds_invalid'; return false; }

    $allowed = hrm_anl_001_employment_type_bucket_order();
    $normalized = array(); $seen = array(); $sum = 0; $has_small_nonzero = false;
    foreach ($matrix_rows as $row) {
        if (!is_array($row) || !array_key_exists('department_name', $row) || !array_key_exists('employment_type_code', $row) || !array_key_exists('active_count', $row)) {
            $error = 'department_employment_type_headcount_shape_invalid'; return false;
        }
        $department = trim((string)$row['department_name']);
        $code = trim((string)$row['employment_type_code']);
        $count = $row['active_count'];
        if ($department === '' || strlen($department) > 255 || preg_match('/[\\x00-\\x1F\\x7F]/', $department)) {
            $error = 'department_employment_type_department_invalid'; return false;
        }
        if (!isset($allowed[$code]) || (!is_int($count) && !(is_string($count) && preg_match('/^\\d+$/D', $count) === 1))) {
            $error = 'department_employment_type_headcount_value_invalid'; return false;
        }
        $count = (int)$count;
        $key = $department."\0".$code;
        if ($count < 0 || isset($seen[$key])) { $error = $count < 0 ? 'department_employment_type_headcount_value_invalid' : 'department_employment_type_headcount_duplicate_cell'; return false; }
        $seen[$key] = true;
        $normalized[] = array('department_name'=>$department,'employment_type_code'=>$code,'active_count'=>$count);
        $sum += $count;
        if ($count > 0 && $count < $minimum_cohort_size) $has_small_nonzero = true;
    }
    usort($normalized, 'hrm_anl_001_department_employment_type_headcount_compare');
    if ($sum !== $primary_count) { $error = 'exact_source_total_mismatch'; return false; }

    $public_rows = array(); $suppressed_count = 0;
    foreach ($normalized as $row) {
        $count = (int)$row['active_count'];
        $suppressed = $has_small_nonzero && $count > 0;
        if ($suppressed) $suppressed_count++;
        $small = $count > 0 && $count < $minimum_cohort_size;
        $band = $count === 0 ? 'zero' : ($small ? 'below_threshold' : 'threshold_or_above');
        $public_rows[] = array(
            'department_name'=>(string)$row['department_name'],
            'employment_type_code'=>(string)$row['employment_type_code'],
            'value'=>$suppressed ? null : $count,
            'unit_code'=>'people',
            'suppressed'=>$suppressed,
            'suppression_reason'=>$suppressed ? ($small ? 'matrix_small_cohort' : 'matrix_complementary_full_nonzero') : 'none',
            'cohort_size_band_code'=>$band,
        );
    }
    $source_total_sha = hrm_anl_001_headcount_hash(array('metric_key'=>HRM_ANL_001_HEADCOUNT_METRIC_KEY,'primary_count'=>$primary_count,'department_employment_type_rows'=>$normalized));
    if ($source_total_sha === false) { $error='source_total_hash_failed'; return false; }
    return array(
        'source_total'=>$primary_count,
        'rows'=>$public_rows,
        'raw_department_employment_type_rows'=>$normalized,
        'row_count'=>count($public_rows),
        'suppressed_row_count'=>$suppressed_count,
        'suppression_applied'=>$suppressed_count>0,
        'cohort_size_band_code'=>$suppressed_count>0?'mixed_or_suppressed':'threshold_or_above',
        'source_total_sha256'=>$source_total_sha,
    );
}
?>
