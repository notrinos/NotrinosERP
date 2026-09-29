<?php
/**
 * HRM-ANL-001 bounded read-only headcount execution and immutable evidence.
 *
 * The only supported runtime metric is workforce.active_headcount.total using
 * report-892-compatible active/effective-date semantics. Execution is limited
 * to the authenticated company, requires SA_HRMREPORTS, reconciles two
 * independent aggregate queries inside one database transaction, applies the
 * fixed small-cohort policy, stores hashes/aggregate evidence only, and denies
 * export, drill-down, HR mutation, payroll mutation and network authority.
 */
include_once($path_to_root.'/hrm/includes/db/hrm_anl_001_catalog_db.inc');

/** @return false */
function hrm_anl_001_headcount_runtime_fail(&$error, $code)
{
    $error = $code;
    return false;
}

/** @return array|false */
function hrm_anl_001_headcount_runtime_rows($sql)
{
    $result = db_query($sql);
    if (!$result) return false;
    $rows = array();
    while ($row = db_fetch_assoc($result)) $rows[] = $row;
    return $rows;
}

/** @return array|false */
function hrm_anl_001_headcount_runtime_one($sql)
{
    $result = db_query($sql);
    if (!$result || db_num_rows($result) !== 1) return false;
    return db_fetch_assoc($result);
}

/** @return bool */
function hrm_anl_001_headcount_runtime_ready(&$error)
{
    $version = hrm_anl_001_runtime_database_version();
    if (!is_string($version) || !hrm_anl_001_bounded_headcount_runtime_allowed_for_database_version($version))
        return hrm_anl_001_headcount_runtime_fail($error, 'bounded_headcount_runtime_not_active');
    if (!hrm_anl_001_runtime_has_report_access())
        return hrm_anl_001_headcount_runtime_fail($error, 'permission_denied');
    if (hrm_anl_001_runtime_company_id() < 0 || hrm_anl_001_runtime_actor_id() <= 0)
        return hrm_anl_001_headcount_runtime_fail($error, 'authenticated_scope_required');
    return true;
}

/** @return array|false */
function hrm_anl_001_headcount_active_catalog($as_of_utc, &$error = null)
{
    $company = hrm_anl_001_runtime_company_id();
    $definition = hrm_anl_001_headcount_metric_definition_for_company($company);
    $policy = hrm_anl_001_headcount_suppression_policy_for_company($company);
    if ($definition === false || $policy === false || !hrm_anl_001_headcount_datetime($as_of_utc))
        return hrm_anl_001_headcount_runtime_fail($error, 'catalog_lookup_input_invalid');
    $metric_sql = 'SELECT * FROM '.TB_PREF.'hrm_anl_001_metric_definitions WHERE company_id='.$company
        .' AND metric_key='.db_escape(HRM_ANL_001_HEADCOUNT_METRIC_KEY)
        .' AND definition_version='.db_escape(HRM_ANL_001_HEADCOUNT_DEFINITION_VERSION)
        .' AND definition_sha256='.db_escape($definition['definition_sha256'])
        .' AND valid_from<='.db_escape($as_of_utc).' AND expires_at>'.db_escape($as_of_utc)
        .' ORDER BY valid_from DESC,metric_definition_id DESC LIMIT 2';
    $policy_sql = 'SELECT * FROM '.TB_PREF.'hrm_anl_001_suppression_policies WHERE company_id='.$company
        .' AND policy_key='.db_escape(HRM_ANL_001_HEADCOUNT_POLICY_KEY)
        .' AND policy_version='.db_escape(HRM_ANL_001_HEADCOUNT_POLICY_VERSION)
        .' AND policy_sha256='.db_escape($policy['policy_sha256'])
        .' AND valid_from<='.db_escape($as_of_utc).' AND expires_at>'.db_escape($as_of_utc)
        .' ORDER BY valid_from DESC,suppression_policy_id DESC LIMIT 2';
    $metric_rows = hrm_anl_001_headcount_runtime_rows($metric_sql);
    $policy_rows = hrm_anl_001_headcount_runtime_rows($policy_sql);
    if ($metric_rows === false || $policy_rows === false || count($metric_rows) !== 1 || count($policy_rows) !== 1)
        return hrm_anl_001_headcount_runtime_fail($error, 'approved_headcount_catalog_missing_or_ambiguous');
    $metric = $metric_rows[0];
    $suppression = $policy_rows[0];
    foreach (array('metric_key','definition_version','source_model_code','value_type_code','unit_code','currency_semantics_code','as_of_semantics_code','aggregation_code','source_contract_sha256','field_policy_sha256','object_policy_sha256','definition_sha256') as $field) {
        if (!isset($metric[$field]) || (string)$metric[$field] !== (string)$definition[$field])
            return hrm_anl_001_headcount_runtime_fail($error, 'metric_definition_tamper_suspected');
    }
    foreach (array('policy_key','policy_version','minimum_cohort_size','suppress_exact_counts','export_allowed','policy_sha256') as $field) {
        if (!isset($suppression[$field]) || (string)$suppression[$field] !== (string)$policy[$field])
            return hrm_anl_001_headcount_runtime_fail($error, 'suppression_policy_tamper_suspected');
    }
    if ((int)$metric['reviewed_by'] <= 0 || (int)$metric['approved_by'] <= 0 || (int)$metric['reviewed_by'] === (int)$metric['approved_by']
        || (int)$suppression['reviewed_by'] <= 0 || (int)$suppression['approved_by'] <= 0 || (int)$suppression['reviewed_by'] === (int)$suppression['approved_by'])
        return hrm_anl_001_headcount_runtime_fail($error, 'catalog_separation_of_duties_invalid');
    if ((int)$metric['reviewed_by'] !== (int)$suppression['reviewed_by']
        || (int)$metric['approved_by'] !== (int)$suppression['approved_by']
        || (string)$metric['valid_from'] !== (string)$suppression['valid_from']
        || (string)$metric['expires_at'] !== (string)$suppression['expires_at'])
        return hrm_anl_001_headcount_runtime_fail($error, 'catalog_pair_lineage_mismatch');
    return array('metric' => $metric, 'policy' => $suppression);
}

/** @return string */
function hrm_anl_001_headcount_effective_where($as_of_date)
{
    $date = db_escape($as_of_date);
    return 'e.inactive=0'
        .' AND (e.hire_date IS NULL OR e.hire_date=\'0000-00-00\' OR e.hire_date<='.$date.')'
        .' AND (e.released_date IS NULL OR e.released_date=\'0000-00-00\' OR e.released_date=\'\' OR e.released_date>'.$date.')';
}

/** @return array|false */
function hrm_anl_001_headcount_source_totals($as_of_utc, &$error = null)
{
    if (!hrm_anl_001_headcount_datetime($as_of_utc)) return hrm_anl_001_headcount_runtime_fail($error, 'as_of_utc_invalid');
    $where = hrm_anl_001_headcount_effective_where(substr($as_of_utc, 0, 10));
    $primary = hrm_anl_001_headcount_runtime_one('SELECT COUNT(*) active_count FROM '.TB_PREF.'employees e WHERE '.$where);
    if ($primary === false || !isset($primary['active_count'])) return hrm_anl_001_headcount_runtime_fail($error, 'headcount_primary_query_failed');
    $groups = hrm_anl_001_headcount_runtime_rows('SELECT e.department_id,COUNT(*) active_count FROM '.TB_PREF.'employees e WHERE '.$where.' GROUP BY e.department_id ORDER BY e.department_id');
    if ($groups === false) return hrm_anl_001_headcount_runtime_fail($error, 'headcount_reconciliation_query_failed');
    $normalized = array();
    foreach ($groups as $row) {
        $normalized[] = array('department_id' => (int)$row['department_id'], 'active_count' => (string)$row['active_count']);
    }
    return array('primary_count' => (string)$primary['active_count'], 'department_counts' => $normalized);
}

/** @return array|false */
function hrm_anl_001_headcount_existing_evidence($company, $request_sha256, $result_sha256, $query_sha256, &$error = null)
{
    $request_rows = hrm_anl_001_headcount_runtime_rows('SELECT * FROM '.TB_PREF.'hrm_anl_001_snapshot_requests WHERE company_id='.(int)$company.' AND request_sha256='.db_escape($request_sha256).' LIMIT 2');
    if ($request_rows === false) return hrm_anl_001_headcount_runtime_fail($error, 'snapshot_request_lookup_failed');
    if (count($request_rows) === 0) return null;
    if (count($request_rows) !== 1) return hrm_anl_001_headcount_runtime_fail($error, 'snapshot_request_duplicate_state');
    $request = $request_rows[0];
    $snapshot_rows = hrm_anl_001_headcount_runtime_rows('SELECT * FROM '.TB_PREF.'hrm_anl_001_snapshots WHERE company_id='.(int)$company.' AND snapshot_request_id='.(int)$request['snapshot_request_id'].' LIMIT 2');
    if ($snapshot_rows === false || count($snapshot_rows) !== 1) return hrm_anl_001_headcount_runtime_fail($error, 'snapshot_evidence_incomplete');
    $snapshot = $snapshot_rows[0];
    if (!isset($snapshot['result_sha256']) || !hash_equals((string)$snapshot['result_sha256'], (string)$result_sha256))
        return hrm_anl_001_headcount_runtime_fail($error, 'snapshot_result_hash_mismatch');
    $audit_rows = hrm_anl_001_headcount_runtime_rows('SELECT * FROM '.TB_PREF.'hrm_anl_001_query_audits WHERE company_id='.(int)$company.' AND snapshot_id='.(int)$snapshot['snapshot_id'].' AND query_sha256='.db_escape($query_sha256).' LIMIT 2');
    if ($audit_rows === false || count($audit_rows) !== 1) return hrm_anl_001_headcount_runtime_fail($error, 'query_audit_evidence_incomplete');
    return array('request' => $request, 'snapshot' => $snapshot, 'audit' => $audit_rows[0]);
}

/**
 * Execute the one bounded aggregate headcount metric.
 *
 * @param string $as_of_utc UTC datetime YYYY-MM-DD HH:MM:SS.
 * @param string $purpose_code Must be the fixed report-892 purpose.
 * @param bool $export_requested Always denied when true.
 * @param bool $drilldown_requested Always denied when true.
 * @param string|null $error
 * @return array|false
 */
function hrm_anl_001_execute_headcount_metric($as_of_utc, $purpose_code, $export_requested = false, $drilldown_requested = false, &$error = null)
{
    $error = null;
    if (!hrm_anl_001_headcount_runtime_ready($error)) return false;
    if (!hrm_anl_001_headcount_datetime($as_of_utc)) return hrm_anl_001_headcount_runtime_fail($error, 'as_of_utc_invalid');
    if ((string)$purpose_code !== HRM_ANL_001_HEADCOUNT_PURPOSE) return hrm_anl_001_headcount_runtime_fail($error, 'query_purpose_denied');
    if ($export_requested) return hrm_anl_001_headcount_runtime_fail($error, 'export_runtime_denied');
    if ($drilldown_requested) return hrm_anl_001_headcount_runtime_fail($error, 'drilldown_runtime_denied');

    global $transaction_level;
    if (!empty($transaction_level)) return hrm_anl_001_headcount_runtime_fail($error, 'headcount_owned_transaction_required');
    $company = hrm_anl_001_runtime_company_id();
    $actor = hrm_anl_001_runtime_actor_id();
    $scope = hrm_anl_001_headcount_scope_hashes($company);
    if ($scope === false) return hrm_anl_001_headcount_runtime_fail($error, 'scope_hash_failed');

    begin_transaction();
    $catalog = hrm_anl_001_headcount_active_catalog($as_of_utc, $error);
    if ($catalog === false) { cancel_transaction(); return false; }
    $totals = hrm_anl_001_headcount_source_totals($as_of_utc, $error);
    if ($totals === false) { cancel_transaction(); return false; }
    $reconciled = hrm_anl_001_headcount_reconcile($totals['primary_count'], $totals['department_counts'], (int)$catalog['policy']['minimum_cohort_size'], $error);
    if ($reconciled === false) { cancel_transaction(); return false; }

    $source_watermark_sha256 = hrm_anl_001_headcount_hash(array(
        'as_of_utc' => $as_of_utc,
        'department_counts' => $reconciled['department_counts'],
        'definition_sha256' => (string)$catalog['metric']['definition_sha256'],
    ));
    $result_basis = array(
        'metric_key' => HRM_ANL_001_HEADCOUNT_METRIC_KEY,
        'as_of_utc' => $as_of_utc,
        'currency_code' => HRM_ANL_001_HEADCOUNT_CURRENCY,
        'value' => $reconciled['value'],
        'unit_code' => $reconciled['unit_code'],
        'suppressed' => $reconciled['suppressed'],
        'cohort_size_band_code' => $reconciled['cohort_size_band_code'],
        'definition_sha256' => (string)$catalog['metric']['definition_sha256'],
        'policy_sha256' => (string)$catalog['policy']['policy_sha256'],
    );
    $result_sha256 = hrm_anl_001_headcount_hash($result_basis);
    if ($source_watermark_sha256 === false || $result_sha256 === false) { cancel_transaction(); return hrm_anl_001_headcount_runtime_fail($error, 'runtime_hash_failed'); }

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
        'purpose_code' => HRM_ANL_001_HEADCOUNT_PURPOSE,
    );
    $request_sha256 = hrm_anl_001_headcount_hash($request_basis);
    $snapshot_basis = array(
        'request_sha256' => $request_sha256,
        'source_total_sha256' => $reconciled['source_total_sha256'],
        'result_sha256' => $result_sha256,
        'status_code' => 'reconciled',
    );
    $snapshot_sha256 = hrm_anl_001_headcount_hash($snapshot_basis);
    $decision_code = $reconciled['suppressed'] ? 'suppressed' : 'allowed';
    $query_basis = array(
        'snapshot_sha256' => $snapshot_sha256,
        'actor_user_id' => $actor,
        'query_purpose_code' => HRM_ANL_001_HEADCOUNT_PURPOSE,
        'filter_sha256' => $scope['filter_sha256'],
        'object_scope_sha256' => $scope['object_scope_sha256'],
        'field_scope_sha256' => $scope['field_scope_sha256'],
        'as_of_utc' => $as_of_utc,
        'currency_code' => HRM_ANL_001_HEADCOUNT_CURRENCY,
        'decision_code' => $decision_code,
        'suppression_applied' => $reconciled['suppressed'],
        'cohort_size_band_code' => $reconciled['cohort_size_band_code'],
        'export_requested' => false,
        'export_allowed' => false,
        'drilldown_requested' => false,
    );
    $query_sha256 = hrm_anl_001_headcount_hash($query_basis);
    if ($request_sha256 === false || $snapshot_sha256 === false || $query_sha256 === false) { cancel_transaction(); return hrm_anl_001_headcount_runtime_fail($error, 'evidence_hash_failed'); }

    $existing = hrm_anl_001_headcount_existing_evidence($company, $request_sha256, $result_sha256, $query_sha256, $error);
    if ($existing === false) { cancel_transaction(); return false; }
    $now = gmdate('Y-m-d H:i:s');
    if ($existing === null) {
        $request_sql = 'INSERT INTO '.TB_PREF.'hrm_anl_001_snapshot_requests '
            .'(company_id,metric_definition_id,suppression_policy_id,as_of_utc,currency_code,filter_sha256,object_scope_sha256,field_scope_sha256,source_watermark_sha256,requested_by,purpose_code,request_sha256,requested_at,created_at) VALUES ('
            .$company.','.(int)$catalog['metric']['metric_definition_id'].','.(int)$catalog['policy']['suppression_policy_id'].','.db_escape($as_of_utc).','.db_escape(HRM_ANL_001_HEADCOUNT_CURRENCY).','
            .db_escape($scope['filter_sha256']).','.db_escape($scope['object_scope_sha256']).','.db_escape($scope['field_scope_sha256']).','.db_escape($source_watermark_sha256).','.$actor.','
            .db_escape(HRM_ANL_001_HEADCOUNT_PURPOSE).','.db_escape($request_sha256).','.db_escape($now).','.db_escape($now).')';
        if (!db_query($request_sql)) { cancel_transaction(); return hrm_anl_001_headcount_runtime_fail($error, 'snapshot_request_insert_failed'); }
        $request_id = (int)db_insert_id();
        $snapshot_sql = 'INSERT INTO '.TB_PREF.'hrm_anl_001_snapshots '
            .'(company_id,snapshot_request_id,row_count,suppressed_row_count,source_total_sha256,result_sha256,snapshot_sha256,status_code,captured_at,created_at) VALUES ('
            .$company.','.$request_id.',1,'.($reconciled['suppressed'] ? 1 : 0).','.db_escape($reconciled['source_total_sha256']).','.db_escape($result_sha256).','.db_escape($snapshot_sha256).','
            .db_escape('reconciled').','.db_escape($now).','.db_escape($now).')';
        if (!db_query($snapshot_sql)) { cancel_transaction(); return hrm_anl_001_headcount_runtime_fail($error, 'snapshot_insert_failed'); }
        $snapshot_id = (int)db_insert_id();
        $audit_sql = 'INSERT INTO '.TB_PREF.'hrm_anl_001_query_audits '
            .'(company_id,metric_definition_id,snapshot_id,actor_user_id,query_purpose_code,filter_sha256,object_scope_sha256,field_scope_sha256,as_of_utc,currency_code,decision_code,suppression_applied,cohort_size_band_code,export_requested,export_allowed,drilldown_requested,query_sha256,occurred_at,created_at) VALUES ('
            .$company.','.(int)$catalog['metric']['metric_definition_id'].','.$snapshot_id.','.$actor.','.db_escape(HRM_ANL_001_HEADCOUNT_PURPOSE).','
            .db_escape($scope['filter_sha256']).','.db_escape($scope['object_scope_sha256']).','.db_escape($scope['field_scope_sha256']).','.db_escape($as_of_utc).','.db_escape(HRM_ANL_001_HEADCOUNT_CURRENCY).','
            .db_escape($decision_code).','.($reconciled['suppressed'] ? 1 : 0).','.db_escape($reconciled['cohort_size_band_code']).',0,0,0,'.db_escape($query_sha256).','.db_escape($now).','.db_escape($now).')';
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
        'as_of_utc' => $as_of_utc,
        'currency_code' => HRM_ANL_001_HEADCOUNT_CURRENCY,
        'value' => $reconciled['value'],
        'unit_code' => $reconciled['unit_code'],
        'suppressed' => $reconciled['suppressed'],
        'cohort_size_band_code' => $reconciled['cohort_size_band_code'],
        'snapshot_request_id' => $request_id,
        'snapshot_id' => $snapshot_id,
        'query_audit_id' => $audit_id,
        'result_sha256' => $result_sha256,
        'idempotent' => $idempotent,
    );
}
?>
