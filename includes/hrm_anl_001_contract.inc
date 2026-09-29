<?php
/**
 * HRM-ANL-001 governed analytics authority contract.
 *
 * This foundation deliberately creates no live analytics query, export, HR-write
 * or payroll-write authority. Database versions only unlock immutable custody
 * needed by later separately reviewed runtime slices.
 */
if (!defined('HRM_ANL_001_CONTRACT_VERSION')) define('HRM_ANL_001_CONTRACT_VERSION', '1.0.927');

/** @return bool */
function hrm_anl_001_version_at_least($database_version, $minimum)
{
    return is_string($database_version) && preg_match('/^1\.0\.\d+$/D', $database_version) === 1
        && version_compare($database_version, $minimum, '>=');
}

/** @return array */
function hrm_anl_001_authority_contract($database_version)
{
    return array(
        'contract_foundation' => hrm_anl_001_version_at_least($database_version, '1.0.906'),
        'metric_catalog_custody' => hrm_anl_001_version_at_least($database_version, '1.0.907'),
        'snapshot_custody' => hrm_anl_001_version_at_least($database_version, '1.0.908'),
        'query_governance_custody' => hrm_anl_001_version_at_least($database_version, '1.0.909'),
        'catalog_registration_writer_allowed' => hrm_anl_001_version_at_least($database_version, '1.0.910'),
        'headcount_metric_source_ready' => hrm_anl_001_version_at_least($database_version, '1.0.911'),
        'bounded_headcount_runtime_allowed' => hrm_anl_001_version_at_least($database_version, '1.0.912'),
        'bounded_headcount_snapshot_writer_allowed' => hrm_anl_001_version_at_least($database_version, '1.0.912'),
        'catalog_governance_ui_allowed' => hrm_anl_001_version_at_least($database_version, '1.0.913'),
        'bounded_headcount_inquiry_ui_allowed' => hrm_anl_001_version_at_least($database_version, '1.0.914'),
        'bounded_headcount_navigation_allowed' => hrm_anl_001_version_at_least($database_version, '1.0.915'),
        'department_headcount_source_ready' => hrm_anl_001_version_at_least($database_version, '1.0.916'),
        'bounded_department_headcount_runtime_allowed' => hrm_anl_001_version_at_least($database_version, '1.0.917'),
        'bounded_department_headcount_inquiry_ui_allowed' => hrm_anl_001_version_at_least($database_version, '1.0.918'),
        'bounded_department_headcount_navigation_allowed' => hrm_anl_001_version_at_least($database_version, '1.0.919'),
        'employment_type_headcount_source_ready' => hrm_anl_001_version_at_least($database_version, '1.0.920'),
        'bounded_employment_type_headcount_runtime_allowed' => hrm_anl_001_version_at_least($database_version, '1.0.921'),
        'bounded_employment_type_headcount_inquiry_ui_allowed' => hrm_anl_001_version_at_least($database_version, '1.0.922'),
        'bounded_employment_type_headcount_navigation_allowed' => hrm_anl_001_version_at_least($database_version, '1.0.923'),
        'department_employment_type_headcount_source_ready' => hrm_anl_001_version_at_least($database_version, '1.0.924'),
        'bounded_department_employment_type_headcount_runtime_allowed' => hrm_anl_001_version_at_least($database_version, '1.0.925'),
        'bounded_department_employment_type_headcount_inquiry_ui_allowed' => hrm_anl_001_version_at_least($database_version, '1.0.926'),
        'bounded_department_employment_type_headcount_navigation_allowed' => hrm_anl_001_version_at_least($database_version, '1.0.927'),
        'company_scope_server_derived' => true,
        'as_of_required' => true,
        'currency_context_required' => true,
        'field_scope_required' => true,
        'object_scope_required' => true,
        'small_cohort_policy_required' => true,
        'exact_source_total_reconciliation_required' => true,
        'metric_query_runtime_allowed' => false,
        'snapshot_writer_runtime_allowed' => false,
        'drilldown_runtime_allowed' => false,
        'export_runtime_allowed' => false,
        'hr_mutation_authority' => false,
        'payroll_mutation_authority' => false,
        'payment_authority' => false,
        'gl_authority' => false,
        'filing_authority' => false,
        'network_authority' => false,
    );
}

/** @return bool */
function hrm_anl_001_catalog_registration_writer_allowed_for_database_version($database_version)
{
    $c = hrm_anl_001_authority_contract($database_version);
    return $c['catalog_registration_writer_allowed'] === true;
}

/** @return bool */
function hrm_anl_001_headcount_metric_source_ready_for_database_version($database_version)
{
    $c = hrm_anl_001_authority_contract($database_version);
    return $c['headcount_metric_source_ready'] === true;
}

/** @return bool */
function hrm_anl_001_bounded_headcount_runtime_allowed_for_database_version($database_version)
{
    $c = hrm_anl_001_authority_contract($database_version);
    return $c['bounded_headcount_runtime_allowed'] === true
        && $c['bounded_headcount_snapshot_writer_allowed'] === true
        && $c['metric_query_runtime_allowed'] === false
        && $c['snapshot_writer_runtime_allowed'] === false
        && $c['drilldown_runtime_allowed'] === false
        && $c['export_runtime_allowed'] === false;
}


/** @return bool */
function hrm_anl_001_catalog_governance_ui_allowed_for_database_version($database_version)
{
    $c = hrm_anl_001_authority_contract($database_version);
    return $c['catalog_governance_ui_allowed'] === true
        && $c['catalog_registration_writer_allowed'] === true
        && $c['metric_query_runtime_allowed'] === false
        && $c['export_runtime_allowed'] === false
        && $c['drilldown_runtime_allowed'] === false;
}

/** @return bool */
function hrm_anl_001_bounded_headcount_inquiry_ui_allowed_for_database_version($database_version)
{
    $c = hrm_anl_001_authority_contract($database_version);
    return $c['bounded_headcount_inquiry_ui_allowed'] === true
        && $c['catalog_governance_ui_allowed'] === true
        && hrm_anl_001_bounded_headcount_runtime_allowed_for_database_version($database_version)
        && $c['metric_query_runtime_allowed'] === false
        && $c['export_runtime_allowed'] === false
        && $c['drilldown_runtime_allowed'] === false;
}

/** @return bool */
function hrm_anl_001_bounded_headcount_navigation_allowed_for_database_version($database_version)
{
    $c = hrm_anl_001_authority_contract($database_version);
    return $c['bounded_headcount_navigation_allowed'] === true
        && hrm_anl_001_catalog_governance_ui_allowed_for_database_version($database_version)
        && hrm_anl_001_bounded_headcount_inquiry_ui_allowed_for_database_version($database_version)
        && $c['metric_query_runtime_allowed'] === false
        && $c['snapshot_writer_runtime_allowed'] === false
        && $c['export_runtime_allowed'] === false
        && $c['drilldown_runtime_allowed'] === false;
}


/** @return bool */
function hrm_anl_001_department_headcount_source_ready_for_database_version($database_version)
{
    $c = hrm_anl_001_authority_contract($database_version);
    return $c['department_headcount_source_ready'] === true
        && $c['metric_query_runtime_allowed'] === false
        && $c['export_runtime_allowed'] === false
        && $c['drilldown_runtime_allowed'] === false;
}

/** @return bool */
function hrm_anl_001_bounded_department_headcount_runtime_allowed_for_database_version($database_version)
{
    $c = hrm_anl_001_authority_contract($database_version);
    return $c['bounded_department_headcount_runtime_allowed'] === true
        && $c['department_headcount_source_ready'] === true
        && hrm_anl_001_bounded_headcount_runtime_allowed_for_database_version($database_version)
        && $c['metric_query_runtime_allowed'] === false
        && $c['snapshot_writer_runtime_allowed'] === false
        && $c['export_runtime_allowed'] === false
        && $c['drilldown_runtime_allowed'] === false;
}

/** @return bool */
function hrm_anl_001_bounded_department_headcount_inquiry_ui_allowed_for_database_version($database_version)
{
    $c = hrm_anl_001_authority_contract($database_version);
    return $c['bounded_department_headcount_inquiry_ui_allowed'] === true
        && hrm_anl_001_bounded_department_headcount_runtime_allowed_for_database_version($database_version)
        && $c['metric_query_runtime_allowed'] === false
        && $c['export_runtime_allowed'] === false
        && $c['drilldown_runtime_allowed'] === false;
}

/** @return bool */
function hrm_anl_001_bounded_department_headcount_navigation_allowed_for_database_version($database_version)
{
    $c = hrm_anl_001_authority_contract($database_version);
    return $c['bounded_department_headcount_navigation_allowed'] === true
        && hrm_anl_001_bounded_department_headcount_inquiry_ui_allowed_for_database_version($database_version)
        && $c['metric_query_runtime_allowed'] === false
        && $c['snapshot_writer_runtime_allowed'] === false
        && $c['export_runtime_allowed'] === false
        && $c['drilldown_runtime_allowed'] === false;
}

/** @return bool */
function hrm_anl_001_employment_type_headcount_source_ready_for_database_version($database_version)
{
    $c = hrm_anl_001_authority_contract($database_version);
    return $c['employment_type_headcount_source_ready'] === true
        && $c['metric_query_runtime_allowed'] === false
        && $c['export_runtime_allowed'] === false
        && $c['drilldown_runtime_allowed'] === false;
}

/** @return bool */
function hrm_anl_001_bounded_employment_type_headcount_runtime_allowed_for_database_version($database_version)
{
    $c = hrm_anl_001_authority_contract($database_version);
    return $c['bounded_employment_type_headcount_runtime_allowed'] === true
        && $c['employment_type_headcount_source_ready'] === true
        && hrm_anl_001_bounded_headcount_runtime_allowed_for_database_version($database_version)
        && $c['metric_query_runtime_allowed'] === false
        && $c['snapshot_writer_runtime_allowed'] === false
        && $c['export_runtime_allowed'] === false
        && $c['drilldown_runtime_allowed'] === false;
}

/** @return bool */
function hrm_anl_001_bounded_employment_type_headcount_inquiry_ui_allowed_for_database_version($database_version)
{
    $c = hrm_anl_001_authority_contract($database_version);
    return $c['bounded_employment_type_headcount_inquiry_ui_allowed'] === true
        && hrm_anl_001_bounded_employment_type_headcount_runtime_allowed_for_database_version($database_version)
        && $c['metric_query_runtime_allowed'] === false
        && $c['export_runtime_allowed'] === false
        && $c['drilldown_runtime_allowed'] === false;
}

/** @return bool */
function hrm_anl_001_bounded_employment_type_headcount_navigation_allowed_for_database_version($database_version)
{
    $c = hrm_anl_001_authority_contract($database_version);
    return $c['bounded_employment_type_headcount_navigation_allowed'] === true
        && hrm_anl_001_bounded_employment_type_headcount_inquiry_ui_allowed_for_database_version($database_version)
        && $c['metric_query_runtime_allowed'] === false
        && $c['snapshot_writer_runtime_allowed'] === false
        && $c['export_runtime_allowed'] === false
        && $c['drilldown_runtime_allowed'] === false;
}

/** @return bool */
function hrm_anl_001_department_employment_type_headcount_source_ready_for_database_version($database_version)
{
    $c = hrm_anl_001_authority_contract($database_version);
    return $c['department_employment_type_headcount_source_ready'] === true
        && hrm_anl_001_bounded_employment_type_headcount_navigation_allowed_for_database_version($database_version)
        && $c['metric_query_runtime_allowed'] === false
        && $c['export_runtime_allowed'] === false
        && $c['drilldown_runtime_allowed'] === false;
}

/** @return bool */
function hrm_anl_001_bounded_department_employment_type_headcount_runtime_allowed_for_database_version($database_version)
{
    $c = hrm_anl_001_authority_contract($database_version);
    return $c['bounded_department_employment_type_headcount_runtime_allowed'] === true
        && $c['department_employment_type_headcount_source_ready'] === true
        && hrm_anl_001_bounded_headcount_runtime_allowed_for_database_version($database_version)
        && $c['metric_query_runtime_allowed'] === false
        && $c['snapshot_writer_runtime_allowed'] === false
        && $c['export_runtime_allowed'] === false
        && $c['drilldown_runtime_allowed'] === false;
}

/** @return bool */
function hrm_anl_001_bounded_department_employment_type_headcount_inquiry_ui_allowed_for_database_version($database_version)
{
    $c = hrm_anl_001_authority_contract($database_version);
    return $c['bounded_department_employment_type_headcount_inquiry_ui_allowed'] === true
        && hrm_anl_001_bounded_department_employment_type_headcount_runtime_allowed_for_database_version($database_version)
        && $c['metric_query_runtime_allowed'] === false
        && $c['export_runtime_allowed'] === false
        && $c['drilldown_runtime_allowed'] === false;
}

/** @return bool */
function hrm_anl_001_bounded_department_employment_type_headcount_navigation_allowed_for_database_version($database_version)
{
    $c = hrm_anl_001_authority_contract($database_version);
    return $c['bounded_department_employment_type_headcount_navigation_allowed'] === true
        && hrm_anl_001_bounded_department_employment_type_headcount_inquiry_ui_allowed_for_database_version($database_version)
        && $c['metric_query_runtime_allowed'] === false
        && $c['snapshot_writer_runtime_allowed'] === false
        && $c['export_runtime_allowed'] === false
        && $c['drilldown_runtime_allowed'] === false;
}

/** @return bool */
function hrm_anl_001_require_foundation_authority($database_version)
{
    $c = hrm_anl_001_authority_contract($database_version);
    return $c['contract_foundation'] === true
        && $c['company_scope_server_derived'] === true
        && $c['as_of_required'] === true
        && $c['currency_context_required'] === true
        && $c['field_scope_required'] === true
        && $c['object_scope_required'] === true
        && $c['small_cohort_policy_required'] === true
        && $c['metric_query_runtime_allowed'] === false
        && $c['snapshot_writer_runtime_allowed'] === false
        && $c['drilldown_runtime_allowed'] === false
        && $c['export_runtime_allowed'] === false
        && $c['hr_mutation_authority'] === false
        && $c['payroll_mutation_authority'] === false;
}
?>
