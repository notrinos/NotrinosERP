<?php
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
include_once(dirname(dirname(dirname(__FILE__))).'/../includes/hrm_anl_001_contract.inc');

/** @return array */
function hrm_anl_001_groups()
{
    return array(
        'metric_catalog' => array(
            'sql' => 'sql/upgrade_hrm_anl_001_metric_catalog.sql',
            'tables' => array('hrm_anl_001_metric_definitions', 'hrm_anl_001_suppression_policies'),
        ),
        'snapshot_custody' => array(
            'sql' => 'sql/upgrade_hrm_anl_001_snapshot_custody.sql',
            'tables' => array('hrm_anl_001_snapshot_requests', 'hrm_anl_001_snapshots'),
        ),
        'query_governance' => array(
            'sql' => 'sql/upgrade_hrm_anl_001_query_governance.sql',
            'tables' => array('hrm_anl_001_query_audits', 'hrm_anl_001_drilldown_authorizations'),
        ),
    );
}

/** @return array */
function hrm_anl_001_columns()
{
    return array(
        'hrm_anl_001_metric_definitions' => array('metric_definition_id','company_id','metric_key','definition_version','source_model_code','value_type_code','unit_code','currency_semantics_code','as_of_semantics_code','aggregation_code','source_contract_sha256','field_policy_sha256','object_policy_sha256','definition_sha256','reviewed_by','approved_by','valid_from','expires_at','created_at'),
        'hrm_anl_001_suppression_policies' => array('suppression_policy_id','company_id','policy_key','policy_version','minimum_cohort_size','suppress_exact_counts','export_allowed','policy_sha256','reviewed_by','approved_by','valid_from','expires_at','created_at'),
        'hrm_anl_001_snapshot_requests' => array('snapshot_request_id','company_id','metric_definition_id','suppression_policy_id','as_of_utc','currency_code','filter_sha256','object_scope_sha256','field_scope_sha256','source_watermark_sha256','requested_by','purpose_code','request_sha256','requested_at','created_at'),
        'hrm_anl_001_snapshots' => array('snapshot_id','company_id','snapshot_request_id','row_count','suppressed_row_count','source_total_sha256','result_sha256','snapshot_sha256','status_code','captured_at','created_at'),
        'hrm_anl_001_query_audits' => array('query_audit_id','company_id','metric_definition_id','snapshot_id','actor_user_id','query_purpose_code','filter_sha256','object_scope_sha256','field_scope_sha256','as_of_utc','currency_code','decision_code','suppression_applied','cohort_size_band_code','export_requested','export_allowed','drilldown_requested','query_sha256','occurred_at','created_at'),
        'hrm_anl_001_drilldown_authorizations' => array('drilldown_authorization_id','company_id','query_audit_id','metric_definition_id','purpose_code','field_scope_sha256','object_scope_sha256','reviewer_user_id','approver_user_id','decision_code','expires_at','authorization_sha256','authorized_at','created_at'),
    );
}

/** @return array */
function hrm_anl_001_trigger_codes()
{
    return array(
        'hrm_anl_001_metric_definitions'=>'md',
        'hrm_anl_001_suppression_policies'=>'sp',
        'hrm_anl_001_snapshot_requests'=>'sr',
        'hrm_anl_001_snapshots'=>'ss',
        'hrm_anl_001_query_audits'=>'qa',
        'hrm_anl_001_drilldown_authorizations'=>'da',
    );
}

/** @return array|false */
function hrm_anl_001_table_columns($table)
{
    if (!hrm_migration_table_exists($table)) return false;
    $result = db_query('SHOW COLUMNS FROM '.TB_PREF.$table);
    if (!$result) return false;
    $columns = array();
    while ($row = db_fetch_assoc($result)) $columns[] = (string)$row['Field'];
    return $columns;
}

/** @return bool */
function hrm_anl_001_table_innodb($table)
{
    $physical = hrm_migration_physical_table_name($table);
    if ($physical === false) return false;
    $result = db_query('SELECT ENGINE FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($physical));
    $row = $result ? db_fetch_assoc($result) : false;
    return $row && strcasecmp((string)$row['ENGINE'], 'InnoDB') === 0;
}

/** @return bool */
function hrm_anl_001_trigger_valid($table, $operation)
{
    $codes = hrm_anl_001_trigger_codes();
    $prefix = hrm_migration_active_table_prefix();
    $physical = hrm_migration_physical_table_name($table);
    $operation = strtoupper((string)$operation);
    if ($prefix === false || $physical === false || !isset($codes[$table]) || !in_array($operation, array('UPDATE','DELETE'), true)) return false;
    $name = $prefix.'hanl1_'.$codes[$table].'_'.($operation === 'UPDATE' ? 'u' : 'd');
    $result = db_query('SELECT EVENT_MANIPULATION,EVENT_OBJECT_TABLE,ACTION_TIMING,ACTION_STATEMENT FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA=DATABASE() AND TRIGGER_NAME='.db_escape($name).' LIMIT 2');
    if (!$result || db_num_rows($result) !== 1) return false;
    $row = db_fetch_assoc($result);
    $expected = "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-ANL-001 immutable ".$table.' row cannot be '.strtolower($operation).'d' . "'";
    return strtoupper((string)$row['EVENT_MANIPULATION']) === $operation
        && (string)$row['EVENT_OBJECT_TABLE'] === $physical
        && strtoupper((string)$row['ACTION_TIMING']) === 'BEFORE'
        && trim((string)$row['ACTION_STATEMENT']) === $expected;
}

/** @return bool */
function hrm_anl_001_require_table($patch, $table)
{
    $columns = hrm_anl_001_columns();
    if (!isset($columns[$table]) || hrm_anl_001_table_columns($table) !== $columns[$table] || !hrm_anl_001_table_innodb($table))
        return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-ANL-001 table projection mismatch: '.$table);
    foreach (array('UPDATE','DELETE') as $operation)
        if (!hrm_anl_001_trigger_valid($table, $operation))
            return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'HRM-ANL-001 immutable trigger mismatch: '.$table.'.'.$operation);
    return true;
}

/** @return bool */
function hrm_anl_001_require_zero_rows($patch, array $tables)
{
    foreach ($tables as $table) {
        $result = db_query('SELECT COUNT(*) n FROM '.TB_PREF.$table);
        $row = $result ? db_fetch_assoc($result) : false;
        if (!$row || (int)$row['n'] !== 0)
            return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-ANL-001 foundation must remain zero-seed: '.$table);
    }
    return true;
}

/** @return bool */
function hrm_anl_001_create_trigger($patch, $table, $operation)
{
    if (hrm_anl_001_trigger_valid($table, $operation)) return true;
    $codes = hrm_anl_001_trigger_codes();
    $prefix = hrm_migration_active_table_prefix();
    $physical = hrm_migration_physical_table_name($table);
    $operation = strtoupper((string)$operation);
    if ($prefix === false || $physical === false || !isset($codes[$table])) return false;
    $name = $prefix.'hanl1_'.$codes[$table].'_'.($operation === 'UPDATE' ? 'u' : 'd');
    $message = 'HRM-ANL-001 immutable '.$table.' row cannot be '.strtolower($operation).'d';
    $sql = 'CREATE TRIGGER `'.$name.'` BEFORE '.$operation.' ON `'.$physical."` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT=".db_escape($message);
    if (!db_query($sql)) return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-ANL-001 trigger creation failed: '.$table.'.'.$operation);
    return hrm_anl_001_trigger_valid($table, $operation);
}

/** @return bool */
function hrm_anl_001_install_group($patch, $group)
{
    global $path_to_root;
    $groups = hrm_anl_001_groups();
    if (!isset($groups[$group])) return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'Unknown HRM-ANL-001 schema group: '.$group);
    $relative = $groups[$group]['sql'];
    $raw = @file_get_contents($path_to_root.'/'.$relative);
    if (!is_string($raw) || strpos($raw, '-- HRM-ANL-001 GROUP '.$group) === false)
        return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'HRM-ANL-001 schema source missing: '.$relative);
    $prefix = hrm_migration_active_table_prefix();
    if ($prefix === false) return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-ANL-001 table prefix unavailable.');
    $raw = preg_replace('/^--.*$/m', '', $raw);
    $statements = preg_split('/;\s*(?:\r?\n|$)/', $raw);
    foreach ($statements as $statement) {
        $statement = trim($statement);
        if ($statement === '' || strpos($statement, 'CREATE TRIGGER') === 0) continue;
        $statement = str_replace('`0_', '`'.$prefix, $statement);
        if (!db_query($statement)) return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-ANL-001 schema statement failed: '.$relative);
    }
    foreach ($groups[$group]['tables'] as $table) {
        foreach (array('UPDATE','DELETE') as $operation) if (!hrm_anl_001_create_trigger($patch, $table, $operation)) return false;
        if (!hrm_anl_001_require_table($patch, $table)) return false;
    }
    return hrm_anl_001_require_zero_rows($patch, $groups[$group]['tables']);
}

/** @return bool */
function hrm_anl_001_require_group($patch, $group)
{
    $groups = hrm_anl_001_groups();
    if (!isset($groups[$group])) return false;
    foreach ($groups[$group]['tables'] as $table) if (!hrm_anl_001_require_table($patch, $table)) return false;
    return true;
}

/** @return bool */
function hrm_anl_001_require_source_tokens($patch, $relative, array $tokens, $label)
{
    global $path_to_root;
    $source = @file_get_contents($path_to_root.'/'.$relative);
    if (!is_string($source)) return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, $label.' source is missing.');
    foreach ($tokens as $token) if (strpos($source, $token) === false)
        return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, $label.' source contract is incomplete.');
    return true;
}

/** @return bool */
function hrm_anl_001_require_catalog_registration_review($patch)
{
    if (!hrm_anl_001_require_source_tokens($patch, 'includes/hrm_anl_001_headcount_contract.inc', array(
        "HRM_ANL_001_HEADCOUNT_METRIC_KEY', 'workforce.active_headcount.total'",
        "HRM_ANL_001_HEADCOUNT_MINIMUM_COHORT', 5",
        "'export_allowed' => false",
        "'drilldown_allowed' => false",
        'function hrm_anl_001_headcount_metric_definition(',
        'function hrm_anl_001_headcount_suppression_policy(',
        'function hrm_anl_001_headcount_metric_definition_for_company(',
        'function hrm_anl_001_headcount_suppression_policy_for_company('
    ), 'HRM-ANL-001 headcount metric contract')) return false;
    return hrm_anl_001_require_source_tokens($patch, 'hrm/includes/db/hrm_anl_001_catalog_db.inc', array(
        "user_check_access('SA_HRMREPORTS')",
        'function hrm_anl_001_review_headcount_catalog(',
        'function hrm_anl_001_approve_headcount_catalog(',
        'catalog_review_owned_transaction_required',
        'catalog_reviewer_approver_separation_required',
        "entity_type' => 'hrm-anl-001-catalog'",
        'INSERT INTO '."'.TB_PREF.'".'hrm_anl_001_metric_definitions',
        'INSERT INTO '."'.TB_PREF.'".'hrm_anl_001_suppression_policies'
    ), 'HRM-ANL-001 catalog registration writer');
}

/** @return bool */
function hrm_anl_001_require_headcount_metric_review($patch)
{
    return hrm_anl_001_require_source_tokens($patch, 'includes/hrm_anl_001_headcount_contract.inc', array(
        "'report_id' => 892",
        "'primary_aggregation' => 'count_rows'",
        "'independent_reconciliation' => 'sum_department_counts'",
        'function hrm_anl_001_headcount_reconcile(',
        'exact_source_total_mismatch',
        "'cohort_size_band_code'"
    ), 'HRM-ANL-001 headcount metric reconciliation');
}

/** @return bool */
function hrm_anl_001_require_headcount_runtime_review($patch)
{
    return hrm_anl_001_require_source_tokens($patch, 'hrm/includes/db/hrm_anl_001_headcount_db.inc', array(
        'hrm_anl_001_runtime_has_report_access()',
        'function hrm_anl_001_execute_headcount_metric(',
        'SELECT COUNT(*) active_count FROM ',
        'GROUP BY e.department_id ORDER BY e.department_id',
        'export_runtime_denied',
        'drilldown_runtime_denied',
        'hrm_anl_001_snapshot_requests',
        'hrm_anl_001_snapshots',
        'hrm_anl_001_query_audits',
        "HRM_ANL_001_HEADCOUNT_CURRENCY",
        'hrm_anl_001_headcount_reconcile'
    ), 'HRM-ANL-001 bounded headcount runtime');
}


/** @return bool */
function hrm_anl_001_require_catalog_governance_ui_review($patch)
{
    return hrm_anl_001_require_source_tokens($patch, 'hrm/manage/analytics_headcount_catalog.php', array(
        "\$page_security = 'SA_HRMREPORTS'",
        "check_csrf_token()",
        "hrm_anl_001_review_headcount_catalog(",
        "hrm_anl_001_approve_headcount_catalog(",
        "htmlspecialchars((string)\$result['review_event_sha256']",
        "different authenticated users",
        "cannot create arbitrary metrics",
        "enable export or drill-down"
    ), 'HRM-ANL-001 catalog governance UI');
}

/** @return bool */
function hrm_anl_001_require_headcount_inquiry_ui_review($patch)
{
    return hrm_anl_001_require_source_tokens($patch, 'hrm/inquiry/governed_headcount.php', array(
        "\$page_security = 'SA_HRMREPORTS'",
        "check_csrf_token()",
        "hrm_anl_001_execute_headcount_metric(",
        "HRM_ANL_001_HEADCOUNT_PURPOSE, false, false",
        "Suppressed",
        "htmlspecialchars((string)\$result['metric_key']",
        "No source-total hash",
        "Export, drill-down, department detail and employee identifiers remain unavailable"
    ), 'HRM-ANL-001 bounded headcount inquiry UI');
}

/** @return bool */
function hrm_anl_001_require_headcount_navigation_review($patch)
{
    return hrm_anl_001_require_source_tokens($patch, 'applications/hrm.php', array(
        "Governed &Headcount",
        "hrm/inquiry/governed_headcount.php?",
        "Headcount Analytics &Catalog",
        "hrm/manage/analytics_headcount_catalog.php?",
        "'SA_HRMREPORTS'"
    ), 'HRM-ANL-001 bounded headcount navigation');
}


/** @return bool */
function hrm_anl_001_require_department_headcount_contract_review($patch)
{
    return hrm_anl_001_require_source_tokens($patch, 'includes/hrm_anl_001_department_headcount_contract.inc', array(
        "HRM_ANL_001_DEPARTMENT_HEADCOUNT_PURPOSE', 'hrm_report_892_department_headcount'",
        "'grouping_semantics' => \"COALESCE(department_name,'Unassigned')\"",
        "'complementary_suppression_required' => true",
        'function hrm_anl_001_department_headcount_reconcile(',
        'secondary_complementary',
        'complementary_suppression_unavailable',
        "'employee_drilldown_allowed' => false"
    ), 'HRM-ANL-001 bounded department headcount contract');
}

/** @return bool */
function hrm_anl_001_require_department_headcount_runtime_review($patch)
{
    return hrm_anl_001_require_source_tokens($patch, 'hrm/includes/db/hrm_anl_001_department_headcount_db.inc', array(
        'function hrm_anl_001_execute_department_headcount(',
        "COALESCE(d.department_name, 'Unassigned')",
        'hrm_anl_001_department_headcount_reconcile(',
        'export_runtime_denied',
        'drilldown_runtime_denied',
        'hrm_anl_001_snapshot_requests',
        'hrm_anl_001_snapshots',
        'hrm_anl_001_query_audits',
        'suppressed_row_count',
        'HRM_ANL_001_DEPARTMENT_HEADCOUNT_PURPOSE'
    ), 'HRM-ANL-001 bounded department headcount runtime');
}

/** @return bool */
function hrm_anl_001_require_department_headcount_inquiry_ui_review($patch)
{
    return hrm_anl_001_require_source_tokens($patch, 'hrm/inquiry/governed_department_headcount.php', array(
        "\$page_security = 'SA_HRMREPORTS'",
        'check_csrf_token()',
        'hrm_anl_001_execute_department_headcount(',
        'HRM_ANL_001_DEPARTMENT_HEADCOUNT_PURPOSE, false, false',
        "label_cell(_('Suppressed'))",
        'complementary suppression',
        'Employee detail, arbitrary filters, export and drill-down remain unavailable',
        'No source-total hash'
    ), 'HRM-ANL-001 department headcount inquiry UI');
}

/** @return bool */
function hrm_anl_001_require_department_headcount_navigation_review($patch)
{
    return hrm_anl_001_require_source_tokens($patch, 'applications/hrm.php', array(
        'Governed Headcount by &Department',
        'hrm/inquiry/governed_department_headcount.php?',
        "'1.0.919', '>='",
        "'SA_HRMREPORTS'"
    ), 'HRM-ANL-001 department headcount navigation');
}

/** @return bool */
function hrm_anl_001_require_employment_type_headcount_contract_review($patch)
{
    return hrm_anl_001_require_source_tokens($patch, 'includes/hrm_anl_001_employment_type_headcount_contract.inc', array(
        "HRM_ANL_001_EMPLOYMENT_TYPE_HEADCOUNT_PURPOSE', 'hrm_report_892_employment_type_headcount'",
        "'known_employment_type_codes' => array(0,1,2,3,4)",
        "'unknown_code_bucket' => 'other'",
        "'complementary_suppression_required' => true",
        'function hrm_anl_001_employment_type_headcount_reconcile(',
        'secondary_complementary',
        'complementary_suppression_unavailable',
        "'employee_drilldown_allowed' => false"
    ), 'HRM-ANL-001 bounded employment-type headcount contract');
}

/** @return bool */
function hrm_anl_001_require_employment_type_headcount_runtime_review($patch)
{
    return hrm_anl_001_require_source_tokens($patch, 'hrm/includes/db/hrm_anl_001_employment_type_headcount_db.inc', array(
        'function hrm_anl_001_execute_employment_type_headcount(',
        "CASE e.employment_type WHEN 0 THEN 'permanent'",
        'hrm_anl_001_employment_type_headcount_reconcile(',
        'export_runtime_denied',
        'drilldown_runtime_denied',
        'hrm_anl_001_snapshot_requests',
        'hrm_anl_001_snapshots',
        'hrm_anl_001_query_audits',
        'suppressed_row_count',
        'HRM_ANL_001_EMPLOYMENT_TYPE_HEADCOUNT_PURPOSE'
    ), 'HRM-ANL-001 bounded employment-type headcount runtime');
}

/** @return bool */
function hrm_anl_001_require_employment_type_headcount_inquiry_ui_review($patch)
{
    return hrm_anl_001_require_source_tokens($patch, 'hrm/inquiry/governed_employment_type_headcount.php', array(
        "\$page_security = 'SA_HRMREPORTS'",
        'check_csrf_token()',
        'hrm_anl_001_execute_employment_type_headcount(',
        'HRM_ANL_001_EMPLOYMENT_TYPE_HEADCOUNT_PURPOSE, false, false',
        "label_cell(_('Suppressed'))",
        'complementary suppression',
        'Employee detail, arbitrary filters, export and drill-down remain unavailable',
        'No source-total hash'
    ), 'HRM-ANL-001 employment-type headcount inquiry UI');
}

/** @return bool */
function hrm_anl_001_require_employment_type_headcount_navigation_review($patch)
{
    return hrm_anl_001_require_source_tokens($patch, 'applications/hrm.php', array(
        'Governed Headcount by Employment &Type',
        'hrm/inquiry/governed_employment_type_headcount.php?',
        "'1.0.923', '>='",
        "'SA_HRMREPORTS'"
    ), 'HRM-ANL-001 employment-type headcount navigation');
}

/** @return bool */
function hrm_anl_001_require_department_employment_type_headcount_contract_review($patch)
{
    return hrm_anl_001_require_source_tokens($patch, 'includes/hrm_anl_001_department_employment_type_headcount_contract.inc', array(
        "HRM_ANL_001_DEPARTMENT_EMPLOYMENT_TYPE_HEADCOUNT_PURPOSE', 'hrm_report_892_department_employment_type_headcount'",
        "'matrix_wide_nonzero_suppression_on_any_small_cell_required' => true",
        "'row_column_marginals_allowed' => false",
        'function hrm_anl_001_department_employment_type_headcount_reconcile(',
        'matrix_complementary_full_nonzero',
        "'employee_drilldown_allowed' => false"
    ), 'HRM-ANL-001 bounded department x employment-type headcount contract');
}

/** @return bool */
function hrm_anl_001_require_department_employment_type_headcount_runtime_review($patch)
{
    return hrm_anl_001_require_source_tokens($patch, 'hrm/includes/db/hrm_anl_001_department_employment_type_headcount_db.inc', array(
        'function hrm_anl_001_execute_department_employment_type_headcount(',
        "COALESCE(d.department_name, 'Unassigned')",
        "CASE e.employment_type WHEN 0 THEN 'permanent'",
        'hrm_anl_001_department_employment_type_headcount_reconcile(',
        'export_runtime_denied',
        'drilldown_runtime_denied',
        'hrm_anl_001_snapshot_requests',
        'hrm_anl_001_snapshots',
        'hrm_anl_001_query_audits',
        'HRM_ANL_001_DEPARTMENT_EMPLOYMENT_TYPE_HEADCOUNT_PURPOSE'
    ), 'HRM-ANL-001 bounded department x employment-type headcount runtime');
}

/** @return bool */
function hrm_anl_001_require_department_employment_type_headcount_inquiry_ui_review($patch)
{
    return hrm_anl_001_require_source_tokens($patch, 'hrm/inquiry/governed_department_employment_type_headcount.php', array(
        "\$page_security = 'SA_HRMREPORTS'",
        'check_csrf_token()',
        'hrm_anl_001_execute_department_employment_type_headcount(',
        'HRM_ANL_001_DEPARTMENT_EMPLOYMENT_TYPE_HEADCOUNT_PURPOSE, false, false',
        "label_cell(_('Suppressed'))",
        'every non-zero cell is suppressed',
        'Employee detail, row/column marginals, arbitrary filters, export and drill-down remain unavailable',
        'No source-total hash'
    ), 'HRM-ANL-001 department x employment-type headcount inquiry UI');
}

/** @return bool */
function hrm_anl_001_require_department_employment_type_headcount_navigation_review($patch)
{
    return hrm_anl_001_require_source_tokens($patch, 'applications/hrm.php', array(
        'Governed Headcount &Matrix',
        'hrm/inquiry/governed_department_employment_type_headcount.php?',
        "'1.0.927', '>='",
        "'SA_HRMREPORTS'"
    ), 'HRM-ANL-001 department x employment-type headcount navigation');
}

/** @return bool */
function hrm_anl_001_require_predecessor($patch)
{
    $allowed = array('1.0.905','1.0.906','1.0.907','1.0.908','1.0.909','1.0.910','1.0.911','1.0.912','1.0.913','1.0.914','1.0.915','1.0.916','1.0.917','1.0.918','1.0.919','1.0.920','1.0.921','1.0.922','1.0.923','1.0.924','1.0.925','1.0.926');
    if (!is_object($patch) || !isset($patch->previous) || !in_array((string)$patch->previous, $allowed, true))
        return hrm_migration_fail($patch, HRM_MIGRATION_PREDECESSOR_MIGRATION_ABSENT, 'HRM-ANL-001 predecessor version is not accepted.');
    if (!hrm_migration_registered_edge_matches('pay_api_001_webhook_inbound_route','1.0.904','1.0.905'))
        return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'HRM-ANL-001 requires accepted PAY-API-001 closure predecessor custody.');
    if (!hrm_anl_001_require_source_tokens($patch, 'Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/IMPLEMENTATION_BACKLOG.md', array('HRM-ANL-001 — Governed workforce/payroll analytics','PAY-FIL-001','HUMAN-EVIDENCE-DEFERRED'), 'HRM-ANL-001 backlog/deferred filing boundary')) return false;
    return true;
}
?>
