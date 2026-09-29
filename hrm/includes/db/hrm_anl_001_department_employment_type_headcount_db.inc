<?php
/**
 * HRM-ANL-001 bounded department x employment-type headcount matrix runtime.
 *
 * Reuses the already-approved headcount catalog and immutable snapshot/query
 * custody. No generic metric runtime, employee drill-down, export, HR/payroll
 * mutation, or network authority is enabled.
 */
include_once($path_to_root.'/hrm/includes/db/hrm_anl_001_headcount_db.inc');
include_once($path_to_root.'/includes/hrm_anl_001_employment_type_headcount_contract.inc');
include_once($path_to_root.'/includes/hrm_anl_001_department_employment_type_headcount_contract.inc');

/** @return bool */
function hrm_anl_001_department_employment_type_headcount_runtime_ready(&$error)
{
    $version = hrm_anl_001_runtime_database_version();
    if (!is_string($version) || !hrm_anl_001_bounded_department_employment_type_headcount_runtime_allowed_for_database_version($version))
        return hrm_anl_001_headcount_runtime_fail($error, 'bounded_department_employment_type_headcount_runtime_not_active');
    if (!hrm_anl_001_runtime_has_report_access())
        return hrm_anl_001_headcount_runtime_fail($error, 'permission_denied');
    if (hrm_anl_001_runtime_company_id() < 0 || hrm_anl_001_runtime_actor_id() <= 0)
        return hrm_anl_001_headcount_runtime_fail($error, 'authenticated_scope_required');
    return true;
}

/** @return array|false */
function hrm_anl_001_department_employment_type_headcount_source_rows($as_of_utc, &$error = null)
{
    if (!hrm_anl_001_headcount_datetime($as_of_utc))
        return hrm_anl_001_headcount_runtime_fail($error, 'as_of_utc_invalid');
    $where = hrm_anl_001_headcount_effective_where(substr($as_of_utc, 0, 10));
    $bucket = "CASE e.employment_type WHEN 0 THEN 'permanent' WHEN 1 THEN 'contract' WHEN 2 THEN 'probation' WHEN 3 THEN 'intern' WHEN 4 THEN 'part_time' ELSE 'other' END";
    $sql = "SELECT COALESCE(d.department_name, 'Unassigned') department_name, ".$bucket." employment_type_code, COUNT(*) active_count "
        ."FROM ".TB_PREF."employees e LEFT JOIN ".TB_PREF."departments d ON d.department_id=e.department_id WHERE ".$where
        ." GROUP BY COALESCE(d.department_name, 'Unassigned'), ".$bucket
        ." ORDER BY department_name, CASE employment_type_code WHEN 'permanent' THEN 0 WHEN 'contract' THEN 1 WHEN 'probation' THEN 2 WHEN 'intern' THEN 3 WHEN 'part_time' THEN 4 ELSE 5 END";
    $rows = hrm_anl_001_headcount_runtime_rows($sql);
    if ($rows === false) return hrm_anl_001_headcount_runtime_fail($error, 'department_employment_type_headcount_query_failed');
    $out = array();
    foreach ($rows as $row)
        $out[] = array('department_name'=>(string)$row['department_name'], 'employment_type_code'=>(string)$row['employment_type_code'], 'active_count'=>(string)$row['active_count']);
    return $out;
}

/**
 * Execute the bounded department x employment-type matrix for the approved headcount metric.
 *
 * @return array|false
 */
function hrm_anl_001_execute_department_employment_type_headcount($as_of_utc, $purpose_code, $export_requested = false, $drilldown_requested = false, &$error = null)
{
    $error = null;
    if (!hrm_anl_001_department_employment_type_headcount_runtime_ready($error)) return false;
    if (!hrm_anl_001_headcount_datetime($as_of_utc)) return hrm_anl_001_headcount_runtime_fail($error, 'as_of_utc_invalid');
    if ((string)$purpose_code !== HRM_ANL_001_DEPARTMENT_EMPLOYMENT_TYPE_HEADCOUNT_PURPOSE)
        return hrm_anl_001_headcount_runtime_fail($error, 'query_purpose_denied');
    if ($export_requested) return hrm_anl_001_headcount_runtime_fail($error, 'export_runtime_denied');
    if ($drilldown_requested) return hrm_anl_001_headcount_runtime_fail($error, 'drilldown_runtime_denied');

    global $transaction_level;
    if (!empty($transaction_level))
        return hrm_anl_001_headcount_runtime_fail($error, 'department_employment_type_headcount_owned_transaction_required');

    $company = hrm_anl_001_runtime_company_id();
    $actor = hrm_anl_001_runtime_actor_id();
    $scope = hrm_anl_001_department_employment_type_headcount_scope_hashes($company);
    if ($scope === false) return hrm_anl_001_headcount_runtime_fail($error, 'scope_hash_failed');

    begin_transaction();
    $catalog = hrm_anl_001_headcount_active_catalog($as_of_utc, $error);
    if ($catalog === false) { cancel_transaction(); return false; }
    $totals = hrm_anl_001_headcount_source_totals($as_of_utc, $error);
    if ($totals === false) { cancel_transaction(); return false; }
    $rows = hrm_anl_001_department_employment_type_headcount_source_rows($as_of_utc, $error);
    if ($rows === false) { cancel_transaction(); return false; }
    $reconciled = hrm_anl_001_department_employment_type_headcount_reconcile(
        $totals['primary_count'], $rows, (int)$catalog['policy']['minimum_cohort_size'], $error
    );
    if ($reconciled === false) { cancel_transaction(); return false; }

    $source_watermark_sha256 = hrm_anl_001_headcount_hash(array(
        'as_of_utc' => $as_of_utc,
        'raw_department_employment_type_rows' => $reconciled['raw_department_employment_type_rows'],
        'definition_sha256' => (string)$catalog['metric']['definition_sha256'],
    ));
    $result_basis = array(
        'metric_key' => HRM_ANL_001_HEADCOUNT_METRIC_KEY,
        'purpose_code' => HRM_ANL_001_DEPARTMENT_EMPLOYMENT_TYPE_HEADCOUNT_PURPOSE,
        'as_of_utc' => $as_of_utc,
        'currency_code' => HRM_ANL_001_HEADCOUNT_CURRENCY,
        'rows' => $reconciled['rows'],
        'definition_sha256' => (string)$catalog['metric']['definition_sha256'],
        'policy_sha256' => (string)$catalog['policy']['policy_sha256'],
    );
    $result_sha256 = hrm_anl_001_headcount_hash($result_basis);
    if ($source_watermark_sha256 === false || $result_sha256 === false) {
        cancel_transaction();
        return hrm_anl_001_headcount_runtime_fail($error, 'runtime_hash_failed');
    }

    $request_basis = array(
        'company_id' => $company,
        'metric_definition_id' => (int)$catalog['metric']['metric_definition_id'],
        'suppression_policy_id' => (int)$catalog['policy']['suppression_policy_id'],
        'as_of_utc' => $as_of_utc,
        'currency_code' => HRM_ANL_001_HEADCOUNT_CURRENCY,
        'filter_sha256' => $scope['filter_sha256'],
        'object_scope_sha256' => $scope['object_scope_sha256'],
        'field_scope_sha256' => $scope['field_scope_sha256'],
        'source_watermark_sha256' => $source_watermark_sha256,
        'requested_by' => $actor,
        'purpose_code' => HRM_ANL_001_DEPARTMENT_EMPLOYMENT_TYPE_HEADCOUNT_PURPOSE,
    );
    $request_sha256 = hrm_anl_001_headcount_hash($request_basis);
    $snapshot_basis = array(
        'request_sha256' => $request_sha256,
        'source_total_sha256' => $reconciled['source_total_sha256'],
        'result_sha256' => $result_sha256,
        'status_code' => 'reconciled',
    );
    $snapshot_sha256 = hrm_anl_001_headcount_hash($snapshot_basis);
    $decision_code = $reconciled['suppression_applied'] ? 'allowed_suppressed' : 'allowed';
    $query_basis = array(
        'snapshot_sha256' => $snapshot_sha256,
        'actor_user_id' => $actor,
        'query_purpose_code' => HRM_ANL_001_DEPARTMENT_EMPLOYMENT_TYPE_HEADCOUNT_PURPOSE,
        'filter_sha256' => $scope['filter_sha256'],
        'object_scope_sha256' => $scope['object_scope_sha256'],
        'field_scope_sha256' => $scope['field_scope_sha256'],
        'as_of_utc' => $as_of_utc,
        'currency_code' => HRM_ANL_001_HEADCOUNT_CURRENCY,
        'decision_code' => $decision_code,
        'suppression_applied' => $reconciled['suppression_applied'],
        'cohort_size_band_code' => $reconciled['cohort_size_band_code'],
        'export_requested' => false,
        'export_allowed' => false,
        'drilldown_requested' => false,
    );
    $query_sha256 = hrm_anl_001_headcount_hash($query_basis);
    if ($request_sha256 === false || $snapshot_sha256 === false || $query_sha256 === false) {
        cancel_transaction();
        return hrm_anl_001_headcount_runtime_fail($error, 'evidence_hash_failed');
    }

    $existing = hrm_anl_001_headcount_existing_evidence($company, $request_sha256, $result_sha256, $query_sha256, $error);
    if ($existing === false) { cancel_transaction(); return false; }
    $now = gmdate('Y-m-d H:i:s');
    if ($existing === null) {
        $request_sql = 'INSERT INTO '.TB_PREF.'hrm_anl_001_snapshot_requests '
            .'(company_id,metric_definition_id,suppression_policy_id,as_of_utc,currency_code,filter_sha256,object_scope_sha256,field_scope_sha256,source_watermark_sha256,requested_by,purpose_code,request_sha256,requested_at,created_at) VALUES ('
            .$company.','.(int)$catalog['metric']['metric_definition_id'].','.(int)$catalog['policy']['suppression_policy_id'].','.db_escape($as_of_utc).','.db_escape(HRM_ANL_001_HEADCOUNT_CURRENCY).','
            .db_escape($scope['filter_sha256']).','.db_escape($scope['object_scope_sha256']).','.db_escape($scope['field_scope_sha256']).','.db_escape($source_watermark_sha256).','.$actor.','
            .db_escape(HRM_ANL_001_DEPARTMENT_EMPLOYMENT_TYPE_HEADCOUNT_PURPOSE).','.db_escape($request_sha256).','.db_escape($now).','.db_escape($now).')';
        if (!db_query($request_sql)) { cancel_transaction(); return hrm_anl_001_headcount_runtime_fail($error, 'snapshot_request_insert_failed'); }
        $request_id = (int)db_insert_id();

        $snapshot_sql = 'INSERT INTO '.TB_PREF.'hrm_anl_001_snapshots '
            .'(company_id,snapshot_request_id,row_count,suppressed_row_count,source_total_sha256,result_sha256,snapshot_sha256,status_code,captured_at,created_at) VALUES ('
            .$company.','.$request_id.','.(int)$reconciled['row_count'].','.(int)$reconciled['suppressed_row_count'].','.db_escape($reconciled['source_total_sha256']).','
            .db_escape($result_sha256).','.db_escape($snapshot_sha256).','.db_escape('reconciled').','.db_escape($now).','.db_escape($now).')';
        if (!db_query($snapshot_sql)) { cancel_transaction(); return hrm_anl_001_headcount_runtime_fail($error, 'snapshot_insert_failed'); }
        $snapshot_id = (int)db_insert_id();

        $audit_sql = 'INSERT INTO '.TB_PREF.'hrm_anl_001_query_audits '
            .'(company_id,metric_definition_id,snapshot_id,actor_user_id,query_purpose_code,filter_sha256,object_scope_sha256,field_scope_sha256,as_of_utc,currency_code,decision_code,suppression_applied,cohort_size_band_code,export_requested,export_allowed,drilldown_requested,query_sha256,occurred_at,created_at) VALUES ('
            .$company.','.(int)$catalog['metric']['metric_definition_id'].','.$snapshot_id.','.$actor.','.db_escape(HRM_ANL_001_DEPARTMENT_EMPLOYMENT_TYPE_HEADCOUNT_PURPOSE).','
            .db_escape($scope['filter_sha256']).','.db_escape($scope['object_scope_sha256']).','.db_escape($scope['field_scope_sha256']).','.db_escape($as_of_utc).','.db_escape(HRM_ANL_001_HEADCOUNT_CURRENCY).','
            .db_escape($decision_code).','.($reconciled['suppression_applied'] ? 1 : 0).','.db_escape($reconciled['cohort_size_band_code']).',0,0,0,'
            .db_escape($query_sha256).','.db_escape($now).','.db_escape($now).')';
        if (!db_query($audit_sql)) { cancel_transaction(); return hrm_anl_001_headcount_runtime_fail($error, 'query_audit_insert_failed'); }
        $audit_id = (int)db_insert_id();
        $idempotent = false;
    } else {
        $request_id = (int)$existing['request']['snapshot_request_id'];
        $snapshot_id = (int)$existing['snapshot']['snapshot_id'];
        $audit_id = (int)$existing['audit']['query_audit_id'];
        $idempotent = true;
    }
    commit_transaction();

    return array(
        'metric_key' => HRM_ANL_001_HEADCOUNT_METRIC_KEY,
        'purpose_code' => HRM_ANL_001_DEPARTMENT_EMPLOYMENT_TYPE_HEADCOUNT_PURPOSE,
        'as_of_utc' => $as_of_utc,
        'currency_code' => HRM_ANL_001_HEADCOUNT_CURRENCY,
        'rows' => $reconciled['rows'],
        'row_count' => $reconciled['row_count'],
        'suppressed_row_count' => $reconciled['suppressed_row_count'],
        'snapshot_request_id' => $request_id,
        'snapshot_id' => $snapshot_id,
        'query_audit_id' => $audit_id,
        'result_sha256' => $result_sha256,
        'idempotent' => $idempotent,
    );
}
?>
