<?php
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
include_once($path_to_root.'/includes/hrm_tal_001_contract.inc');

/** @return array */
function hrm_tal_001_schema_groups()
{
    return array(
        'skill_definition_custody'=>array('sql'=>'sql/upgrade_hrm_tal_001_skill_definition_custody.sql','tables'=>array('hrm_tal_001_skill_definitions')),
        'skill_evidence_custody'=>array('sql'=>'sql/upgrade_hrm_tal_001_skill_evidence_custody.sql','tables'=>array('hrm_tal_001_employee_skill_evidence')),
        'certification_definition_custody'=>array('sql'=>'sql/upgrade_hrm_tal_001_certification_definition_custody.sql','tables'=>array('hrm_tal_001_certification_definitions')),
        'certification_evidence_custody'=>array('sql'=>'sql/upgrade_hrm_tal_001_certification_evidence_custody.sql','tables'=>array('hrm_tal_001_employee_certification_evidence')),
        'export_audit_custody'=>array('sql'=>'sql/upgrade_hrm_tal_001_export_audit_custody.sql','tables'=>array('hrm_tal_001_export_audit')),
        'performance_cycle_definition_custody'=>array('sql'=>'sql/upgrade_hrm_tal_001_performance_cycle_definition_custody.sql','tables'=>array('hrm_tal_001_performance_cycle_definitions')),
        'performance_review_subject_custody'=>array('sql'=>'sql/upgrade_hrm_tal_001_performance_review_subject_custody.sql','tables'=>array('hrm_tal_001_performance_review_subjects')),
        'performance_assessment_evidence_custody'=>array('sql'=>'sql/upgrade_hrm_tal_001_performance_assessment_evidence_custody.sql','tables'=>array('hrm_tal_001_performance_assessment_evidence')),
        'performance_workflow_event_custody'=>array('sql'=>'sql/upgrade_hrm_tal_001_performance_workflow_event_custody.sql','tables'=>array('hrm_tal_001_performance_workflow_events')),
        'learning_definition_custody'=>array('sql'=>'sql/upgrade_hrm_tal_001_learning_definition_custody.sql','tables'=>array('hrm_tal_001_learning_definitions')),
        'learning_assignment_custody'=>array('sql'=>'sql/upgrade_hrm_tal_001_learning_assignment_custody.sql','tables'=>array('hrm_tal_001_learning_assignments')),
        'learning_completion_custody'=>array('sql'=>'sql/upgrade_hrm_tal_001_learning_completion_custody.sql','tables'=>array('hrm_tal_001_learning_completion_evidence')),
    );
}

/** @return array */
function hrm_tal_001_expected_columns()
{
    return array(
        'hrm_tal_001_skill_definitions'=>array('skill_definition_id','company_id','skill_code','skill_name','category_code','description','proficiency_scale_code','effective_from','effective_to','definition_sha256','created_by_user_id','created_at'),
        'hrm_tal_001_employee_skill_evidence'=>array('employee_skill_evidence_id','company_id','skill_definition_id','employee_id','evidence_type_code','source_reference_sha256','proficiency_value','evidence_date','valid_from','valid_to','evidence_sha256','recorded_by_user_id','created_at'),
        'hrm_tal_001_certification_definitions'=>array('certification_definition_id','company_id','certification_code','certification_name','issuer_name','category_code','expiry_policy_code','default_validity_days','effective_from','effective_to','definition_sha256','created_by_user_id','created_at'),
        'hrm_tal_001_employee_certification_evidence'=>array('employee_certification_evidence_id','company_id','certification_definition_id','employee_id','credential_reference_sha256','issued_on','expires_on','verification_status_code','verification_evidence_sha256','evidence_sha256','recorded_by_user_id','created_at'),
        'hrm_tal_001_export_audit'=>array('export_audit_id','company_id','actor_user_id','scope_code','subject_reference_sha256','format_code','row_count','export_sha256','created_at'),
        'hrm_tal_001_performance_cycle_definitions'=>array('performance_cycle_definition_id','company_id','cycle_code','cycle_name','purpose_code','period_start','period_end','rating_scale_code','effective_from','effective_to','definition_sha256','created_by_user_id','created_at'),
        'hrm_tal_001_performance_review_subjects'=>array('performance_review_subject_id','company_id','performance_cycle_definition_id','employee_id','review_type_code','review_window_start','review_window_end','subject_reference_sha256','subject_sha256','created_by_user_id','created_at'),
        'hrm_tal_001_performance_assessment_evidence'=>array('performance_assessment_evidence_id','company_id','performance_review_subject_id','assessment_role_code','dimension_code','rating_value','comment_reference_sha256','evidence_date','evidence_sha256','recorded_by_user_id','created_at'),
        'hrm_tal_001_performance_workflow_events'=>array('performance_workflow_event_id','company_id','performance_review_subject_id','from_state_code','to_state_code','aggregate_rating_value','rating_basis_sha256','reason_reference_sha256','transition_sha256','actor_user_id','occurred_at','created_at'),
        'hrm_tal_001_learning_definitions'=>array('learning_definition_id','company_id','learning_code','learning_name','delivery_mode_code','provider_name','duration_minutes','effective_from','effective_to','definition_sha256','created_by_user_id','created_at'),
        'hrm_tal_001_learning_assignments'=>array('learning_assignment_id','company_id','learning_definition_id','employee_id','assigned_on','due_on','assignment_status_code','source_reference_sha256','assignment_sha256','assigned_by_user_id','created_at'),
        'hrm_tal_001_learning_completion_evidence'=>array('learning_completion_evidence_id','company_id','learning_assignment_id','completion_status_code','completed_on','score_value','evidence_reference_sha256','evidence_sha256','recorded_by_user_id','created_at'),
    );
}

/** @return array */
function hrm_tal_001_trigger_codes()
{
    return array(
        'hrm_tal_001_skill_definitions'=>'sd',
        'hrm_tal_001_employee_skill_evidence'=>'se',
        'hrm_tal_001_certification_definitions'=>'cd',
        'hrm_tal_001_employee_certification_evidence'=>'ce',
        'hrm_tal_001_export_audit'=>'xa',
        'hrm_tal_001_performance_cycle_definitions'=>'pc',
        'hrm_tal_001_performance_review_subjects'=>'pr',
        'hrm_tal_001_performance_assessment_evidence'=>'pa',
        'hrm_tal_001_performance_workflow_events'=>'pw',
        'hrm_tal_001_learning_definitions'=>'ld',
        'hrm_tal_001_learning_assignments'=>'la',
        'hrm_tal_001_learning_completion_evidence'=>'lc',
    );
}

/** @return array */
function hrm_tal_001_expected_foreign_keys()
{
    return array(
        'hrm_tal_001_employee_skill_evidence'=>array(
            'name'=>'fk_htal1_se_skill',
            'parent'=>'hrm_tal_001_skill_definitions',
            'columns'=>array('skill_definition_id','company_id'),
            'parent_columns'=>array('skill_definition_id','company_id'),
        ),
        'hrm_tal_001_employee_certification_evidence'=>array(
            'name'=>'fk_htal1_ce_cert',
            'parent'=>'hrm_tal_001_certification_definitions',
            'columns'=>array('certification_definition_id','company_id'),
            'parent_columns'=>array('certification_definition_id','company_id'),
        ),
        'hrm_tal_001_performance_review_subjects'=>array(
            'name'=>'fk_htal1_pr_cycle',
            'parent'=>'hrm_tal_001_performance_cycle_definitions',
            'columns'=>array('performance_cycle_definition_id','company_id'),
            'parent_columns'=>array('performance_cycle_definition_id','company_id'),
        ),
        'hrm_tal_001_performance_assessment_evidence'=>array(
            'name'=>'fk_htal1_pa_subject',
            'parent'=>'hrm_tal_001_performance_review_subjects',
            'columns'=>array('performance_review_subject_id','company_id'),
            'parent_columns'=>array('performance_review_subject_id','company_id'),
        ),
        'hrm_tal_001_performance_workflow_events'=>array(
            'name'=>'fk_htal1_pw_subject',
            'parent'=>'hrm_tal_001_performance_review_subjects',
            'columns'=>array('performance_review_subject_id','company_id'),
            'parent_columns'=>array('performance_review_subject_id','company_id'),
        ),
        'hrm_tal_001_learning_assignments'=>array(
            'name'=>'fk_htal1_la_def',
            'parent'=>'hrm_tal_001_learning_definitions',
            'columns'=>array('learning_definition_id','company_id'),
            'parent_columns'=>array('learning_definition_id','company_id'),
        ),
        'hrm_tal_001_learning_completion_evidence'=>array(
            'name'=>'fk_htal1_lc_assignment',
            'parent'=>'hrm_tal_001_learning_assignments',
            'columns'=>array('learning_assignment_id','company_id'),
            'parent_columns'=>array('learning_assignment_id','company_id'),
        ),
    );
}

/** @param string $table @return array|false */
function hrm_tal_001_table_columns($table)
{
    $physical = hrm_migration_physical_table_name($table);
    if ($physical === false) return false;
    $result = db_query('SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($physical).' ORDER BY ORDINAL_POSITION');
    if (!$result) return false;
    $columns = array();
    while ($row = db_fetch_assoc($result)) $columns[] = (string)$row['COLUMN_NAME'];
    return $columns;
}

/** @param string $table @return bool */
function hrm_tal_001_table_innodb($table)
{
    $physical = hrm_migration_physical_table_name($table);
    if ($physical === false) return false;
    $result = db_query('SELECT ENGINE FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($physical));
    $row = $result ? db_fetch_assoc($result) : false;
    return $row && strcasecmp((string)$row['ENGINE'], 'InnoDB') === 0;
}

/** @param string $table @param string $operation @return bool */
function hrm_tal_001_trigger_valid($table, $operation)
{
    $codes = hrm_tal_001_trigger_codes();
    $prefix = hrm_migration_active_table_prefix();
    $physical = hrm_migration_physical_table_name($table);
    $operation = strtoupper((string)$operation);
    if ($prefix === false || $physical === false || !isset($codes[$table]) || !in_array($operation, array('UPDATE','DELETE'), true)) return false;
    $name = $prefix.'htal1_'.$codes[$table].'_'.($operation === 'UPDATE' ? 'u' : 'd');
    $result = db_query('SELECT EVENT_MANIPULATION,EVENT_OBJECT_TABLE,ACTION_TIMING,ACTION_STATEMENT FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA=DATABASE() AND TRIGGER_NAME='.db_escape($name).' LIMIT 2');
    if (!$result || db_num_rows($result) !== 1) return false;
    $row = db_fetch_assoc($result);
    $expected = "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable ".$table.' row cannot be '.strtolower($operation).'d' . "'";
    return strtoupper((string)$row['EVENT_MANIPULATION']) === $operation
        && (string)$row['EVENT_OBJECT_TABLE'] === $physical
        && strtoupper((string)$row['ACTION_TIMING']) === 'BEFORE'
        && trim((string)$row['ACTION_STATEMENT']) === $expected;
}

/** @param object $patch @param string $table @param string $operation @return bool */
function hrm_tal_001_create_trigger($patch, $table, $operation)
{
    if (hrm_tal_001_trigger_valid($table, $operation)) return true;
    $codes = hrm_tal_001_trigger_codes();
    $prefix = hrm_migration_active_table_prefix();
    $physical = hrm_migration_physical_table_name($table);
    $operation = strtoupper((string)$operation);
    if ($prefix === false || $physical === false || !isset($codes[$table])) return false;
    $name = $prefix.'htal1_'.$codes[$table].'_'.($operation === 'UPDATE' ? 'u' : 'd');
    $message = 'HRM-TAL-001 immutable '.$table.' row cannot be '.strtolower($operation).'d';
    $sql = 'CREATE TRIGGER `'.$name.'` BEFORE '.$operation.' ON `'.$physical."` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT=".db_escape($message);
    if (!db_query($sql)) return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-TAL-001 trigger creation failed: '.$table.'.'.$operation);
    return hrm_tal_001_trigger_valid($table, $operation);
}

/** @param string $table @return bool */
function hrm_tal_001_foreign_key_valid($table)
{
    $expected = hrm_tal_001_expected_foreign_keys();
    if (!isset($expected[$table])) return true;
    $fk = $expected[$table];
    $prefix = hrm_migration_active_table_prefix();
    $child = hrm_migration_physical_table_name($table);
    $parent = hrm_migration_physical_table_name($fk['parent']);
    if ($prefix === false || $child === false || $parent === false) return false;
    $result = db_query('SELECT CONSTRAINT_NAME,REFERENCED_TABLE_NAME,UPDATE_RULE,DELETE_RULE FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS WHERE CONSTRAINT_SCHEMA=DATABASE() AND CONSTRAINT_NAME='.db_escape($fk['name']).' AND TABLE_NAME='.db_escape($child));
    if (!$result || db_num_rows($result) !== 1) return false;
    $row = db_fetch_assoc($result);
    if ((string)$row['REFERENCED_TABLE_NAME'] !== $parent || strtoupper((string)$row['UPDATE_RULE']) !== 'RESTRICT' || strtoupper((string)$row['DELETE_RULE']) !== 'RESTRICT') return false;
    $columns = db_query('SELECT COLUMN_NAME,REFERENCED_COLUMN_NAME FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE WHERE CONSTRAINT_SCHEMA=DATABASE() AND CONSTRAINT_NAME='.db_escape($fk['name']).' AND TABLE_NAME='.db_escape($child).' ORDER BY ORDINAL_POSITION');
    if (!$columns) return false;
    $actual = array();
    while ($col = db_fetch_assoc($columns)) $actual[] = array((string)$col['COLUMN_NAME'], (string)$col['REFERENCED_COLUMN_NAME']);
    $want = array();
    foreach ($fk['columns'] as $i=>$column) $want[] = array($column, $fk['parent_columns'][$i]);
    return $actual === $want;
}

/** @param object $patch @param string $table @return bool */
function hrm_tal_001_require_table($patch, $table)
{
    $columns = hrm_tal_001_expected_columns();
    if (!isset($columns[$table]) || hrm_tal_001_table_columns($table) !== $columns[$table] || !hrm_tal_001_table_innodb($table))
        return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-TAL-001 table projection mismatch: '.$table);
    foreach (array('UPDATE','DELETE') as $operation)
        if (!hrm_tal_001_trigger_valid($table, $operation))
            return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'HRM-TAL-001 immutable trigger mismatch: '.$table.'.'.$operation);
    if (!hrm_tal_001_foreign_key_valid($table))
        return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-TAL-001 restrictive foreign-key mismatch: '.$table);
    return true;
}

/** @param object $patch @param array $tables @return bool */
function hrm_tal_001_require_zero_rows($patch, array $tables)
{
    foreach ($tables as $table) {
        $result = db_query('SELECT COUNT(*) n FROM '.TB_PREF.$table);
        $row = $result ? db_fetch_assoc($result) : false;
        if (!$row || (int)$row['n'] !== 0)
            return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-TAL-001 foundation must remain zero-seed: '.$table);
    }
    return true;
}

/** @param object $patch @param string $group @return bool */
function hrm_tal_001_install_group($patch, $group)
{
    global $path_to_root;
    $groups = hrm_tal_001_schema_groups();
    if (!isset($groups[$group])) return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'Unknown HRM-TAL-001 schema group: '.$group);
    $relative = $groups[$group]['sql'];
    $raw = @file_get_contents($path_to_root.'/'.$relative);
    if (!is_string($raw) || strpos($raw, '-- HRM-TAL-001 GROUP '.$group) === false)
        return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'HRM-TAL-001 schema source missing: '.$relative);
    $prefix = hrm_migration_active_table_prefix();
    if ($prefix === false) return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-TAL-001 table prefix unavailable.');
    $raw = preg_replace('/^--.*$/m', '', $raw);
    $statements = preg_split('/;\s*(?:\r?\n|$)/', $raw);
    foreach ($statements as $statement) {
        $statement = trim($statement);
        if ($statement === '' || strpos($statement, 'CREATE TRIGGER') === 0) continue;
        $statement = str_replace('`0_', '`'.$prefix, $statement);
        if (!db_query($statement)) return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'HRM-TAL-001 schema statement failed: '.$relative);
    }
    foreach ($groups[$group]['tables'] as $table) {
        foreach (array('UPDATE','DELETE') as $operation) if (!hrm_tal_001_create_trigger($patch, $table, $operation)) return false;
        if (!hrm_tal_001_require_table($patch, $table)) return false;
    }
    return hrm_tal_001_require_zero_rows($patch, $groups[$group]['tables']);
}

/** @param object $patch @param string $relative @param array $tokens @param string $label @return bool */
function hrm_tal_001_require_source_tokens($patch, $relative, array $tokens, $label)
{
    global $path_to_root;
    $source = @file_get_contents($path_to_root.'/'.$relative);
    if (!is_string($source)) return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, $label.' source is missing.');
    foreach ($tokens as $token) if (strpos($source, $token) === false)
        return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, $label.' source contract is incomplete.');
    return true;
}

/** @param object $patch @return bool */
function hrm_tal_001_require_predecessor($patch)
{
    $allowed = array('1.0.936','1.0.937','1.0.938','1.0.939','1.0.940','1.0.941','1.0.942','1.0.943','1.0.944','1.0.945','1.0.946','1.0.947','1.0.948','1.0.949','1.0.950','1.0.951','1.0.952','1.0.953','1.0.954','1.0.955','1.0.956','1.0.957','1.0.958','1.0.959','1.0.960','1.0.961','1.0.962','1.0.963','1.0.964','1.0.965','1.0.966','1.0.967','1.0.968','1.0.969','1.0.970','1.0.971','1.0.972');
    if (!is_object($patch) || !isset($patch->previous) || !in_array((string)$patch->previous, $allowed, true))
        return hrm_migration_fail($patch, HRM_MIGRATION_PREDECESSOR_MIGRATION_ABSENT, 'HRM-TAL-001 predecessor version is not accepted.');
    if (!hrm_migration_registered_edge_matches('pay_ai_001_assistive_ui','1.0.935','1.0.936'))
        return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'HRM-TAL-001 requires the accepted PAY-AI-001 source predecessor.');
    return hrm_tal_001_require_source_tokens($patch, 'Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/IMPLEMENTATION_BACKLOG.md', array(
        'HRM-TAL-001 — Modular talent and HR case capabilities',
        'PAY-AI-001 final acceptance and closure',
        'no remaining PAY-AI-001 implementation slices',
        'PAY-FIL-001 remains **OPEN / DEPENDENCY-BLOCKED / HUMAN-EVIDENCE-DEFERRED**'
    ), 'HRM-TAL-001 dependency/backlog');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_catalog_writer_source($patch)
{
    return hrm_tal_001_require_source_tokens($patch, 'hrm/includes/db/hrm_tal_001_catalog_db.inc', array(
        "user_check_access('SA_HRSETTINGS')",
        'hrm_tal_001_catalog_writer_allowed_for_database_version',
        'hrm_tal_001_catalog_begin_serialized_transaction',
        'FOR UPDATE',
        'hrm_tal_001_register_skill_definition',
        'hrm_tal_001_register_certification_definition',
        'catalog_effective_window_overlaps',
        "INSERT INTO '.TB_PREF.'hrm_tal_001_skill_definitions",
        "INSERT INTO '.TB_PREF.'hrm_tal_001_certification_definitions"
    ), 'HRM-TAL-001 catalog writer');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_evidence_writer_source($patch)
{
    return hrm_tal_001_require_source_tokens($patch, 'hrm/includes/db/hrm_tal_001_evidence_db.inc', array(
        'hrm_tal_001_evidence_writer_allowed_for_database_version',
        'hrm_tal_001_employee_exists',
        'hrm_tal_001_record_skill_evidence',
        'hrm_tal_001_record_certification_evidence',
        'skill_definition_missing_or_cross_company',
        'certification_definition_missing_or_cross_company',
        "INSERT INTO '.TB_PREF.'hrm_tal_001_employee_skill_evidence",
        "INSERT INTO '.TB_PREF.'hrm_tal_001_employee_certification_evidence"
    ), 'HRM-TAL-001 evidence writer');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_read_only_view_source($patch)
{
    return hrm_tal_001_require_source_tokens($patch, 'hrm/includes/db/hrm_tal_001_view_db.inc', array(
        'hrm_tal_001_read_only_view_allowed_for_database_version',
        "user_check_access('SA_HRSETTINGS')",
        'hrm_tal_001_read_skill_evidence_for_employee',
        'hrm_tal_001_read_certification_evidence_for_employee',
        'WHERE e.company_id=',
        'AND e.employee_id=',
        'ORDER BY e.evidence_date DESC',
        'ORDER BY e.issued_on DESC'
    ), 'HRM-TAL-001 read-only view');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_governed_admin_ui_source($patch)
{
    return hrm_tal_001_require_source_tokens($patch, 'hrm/manage/talent_skills_certifications.php', array(
        "\$page_security = 'SA_HRSETTINGS'",
        'check_csrf_token()',
        'hrm_tal_001_governed_admin_ui_allowed_for_database_version',
        'hrm_tal_001_register_skill_definition',
        'hrm_tal_001_register_certification_definition',
        'hrm_tal_001_record_skill_evidence',
        'hrm_tal_001_record_certification_evidence',
        'hrm_tal_001_read_skill_evidence_for_employee',
        'hrm_tal_001_read_certification_evidence_for_employee',
        'It cannot update/delete evidence, export talent data'
    ), 'HRM-TAL-001 governed admin browser UI');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_browser_navigation_source($patch)
{
    return hrm_tal_001_require_source_tokens($patch, 'applications/hrm.php', array(
        "version_compare((string)get_company_pref('version_id', true), '1.0.946', '>=')",
        "_('Talent Skills & Certifications')",
        "'hrm/manage/talent_skills_certifications.php?'",
        "'SA_HRSETTINGS'"
    ), 'HRM-TAL-001 browser navigation');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_self_service_view_source($patch)
{
    if (!hrm_tal_001_require_source_tokens($patch, 'hrm/includes/db/hrm_tal_001_self_view_db.inc', array(
        "user_check_access('SA_HRM_ESS_VIEW_SELF')",
        'get_hrm_ess_current_self_scope()',
        'hrm_tal_001_self_service_view_allowed_for_database_version',
        'hrm_tal_001_read_skill_evidence_for_self',
        'hrm_tal_001_read_certification_evidence_for_self',
        'WHERE e.company_id=',
        'AND e.employee_id='
    ), 'HRM-TAL-001 ESS self read service')) return false;
    if (!hrm_tal_001_require_source_tokens($patch, 'hrm/ess/talent.php', array(
        "\$page_security = 'SA_HRM_ESS_VIEW_SELF'",
        'hrm_tal_001_read_skill_evidence_for_self',
        'hrm_tal_001_read_certification_evidence_for_self',
        'accepts no employee identifier from the browser'
    ), 'HRM-TAL-001 ESS self route')) return false;
    return hrm_tal_001_require_source_tokens($patch, 'hrm/ess/dashboard.php', array(
        "version_compare((string)get_company_pref('version_id',true),'1.0.947','>=')",
        "hrm_ess_002_ui_card_link('talent.php'"
    ), 'HRM-TAL-001 ESS navigation');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_manager_scope_source($patch)
{
    return hrm_tal_001_require_source_tokens($patch, 'hrm/includes/db/hrm_tal_001_manager_scope_db.inc', array(
        "user_check_access('SA_HRM_ESS_VIEW_TEAM')",
        'hrm_ess_002_context_db.inc',
        'get_hrm_ess_002_current_manager_scope',
        "'team_view'",
        'hrm_ess_002_today()',
        'hrm_employee_worker_map',
        'retired_at IS NULL',
        'manager_scope_employee_mapping_missing_or_ambiguous',
        'manager_scope_employee_mapping_invalid',
        '\'scope_origin\' => (string)$row[\'scope_origin\']'
    ), 'HRM-TAL-001 manager scope adapter');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_manager_team_view_source($patch)
{
    return hrm_tal_001_require_source_tokens($patch, 'hrm/includes/db/hrm_tal_001_manager_view_db.inc', array(
        'hrm_tal_001_manager_team_view_allowed_for_database_version',
        'hrm_tal_001_manager_scope_subjects',
        'hrm_tal_001_read_skill_evidence_for_manager_team',
        'hrm_tal_001_read_certification_evidence_for_manager_team',
        'WHERE e.company_id=',
        'e.employee_id IN (',
        "'scope_origin'",
        'manager_skill_scope_projection_mismatch',
        'manager_certification_scope_projection_mismatch'
    ), 'HRM-TAL-001 manager team read service');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_manager_browser_source($patch)
{
    if (!hrm_tal_001_require_source_tokens($patch, 'hrm/ess/team_talent.php', array(
        "\$page_security = 'SA_HRM_ESS_VIEW_TEAM'",
        'hrm_tal_001_manager_browser_allowed_for_database_version',
        'hrm_ess_002_today()',
        'hrm_tal_001_read_skill_evidence_for_manager_team',
        'hrm_tal_001_read_certification_evidence_for_manager_team',
        'accepts no employee selector or historical as-of date'
    ), 'HRM-TAL-001 manager browser route')) return false;
    return hrm_tal_001_require_source_tokens($patch, 'hrm/ess/team.php', array(
        "version_compare((string)get_company_pref('version_id',true),'1.0.950','>=')",
        "hrm_ess_002_access('SA_HRM_ESS_VIEW_TEAM')",
        'team_talent.php'
    ), 'HRM-TAL-001 manager browser navigation');
}


/** @param object $patch @return bool */
function hrm_tal_001_require_export_service_source($patch)
{
    return hrm_tal_001_require_source_tokens($patch, 'hrm/includes/db/hrm_tal_001_export_db.inc', array(
        "user_check_access('SA_HRSETTINGS')",
        'hrm_tal_001_export_service_allowed_for_database_version',
        'HRM_TAL_001_EXPORT_MAX_ROWS',
        'HRM_TAL_001_EXPORT_MAX_BYTES',
        'hrm_tal_001_export_exact_employee_csv',
        "e.employee_id='.db_escape(\$employee_id)",
        "LIMIT '.\$limit",
        'hrm_tal_001_export_csv_cell',
        "preg_match('/^[\\x00-\\x20]*[=+\\-@]/', \$value)",
        "INSERT INTO '.TB_PREF.'hrm_tal_001_export_audit",
        "'admin_exact_employee'",
        "'scope_code'=>'admin_exact_employee'"
    ), 'HRM-TAL-001 bounded export service');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_export_browser_source($patch)
{
    if (!hrm_tal_001_require_source_tokens($patch, 'hrm/manage/talent_export.php', array(
        "\$page_security='SA_HRSETTINGS'",
        'hrm_tal_001_export_browser_allowed_for_database_version',
        'REQUEST_METHOD',
        'check_csrf_token()',
        'hrm_tal_001_export_exact_employee_csv',
        'while (ob_get_level()) ob_end_clean();',
        'X-Content-Type-Options: nosniff',
        'Cache-Control: no-store, private',
        'Content-Disposition: attachment'
    ), 'HRM-TAL-001 export browser route')) return false;
    return hrm_tal_001_require_source_tokens($patch, 'hrm/manage/talent_skills_certifications.php', array(
        "action='talent_export.php'",
        "hidden('_token', ensure_csrf_token())",
        "name='employee_id'",
        'Export Governed Talent CSV'
    ), 'HRM-TAL-001 export browser navigation');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_performance_cycle_writer_source($patch)
{
    return hrm_tal_001_require_source_tokens($patch, 'hrm/includes/db/hrm_tal_001_performance_db.inc', array(
        "user_check_access('SA_HRSETTINGS')",
        'hrm_tal_001_performance_cycle_writer_allowed_for_database_version',
        'hrm_tal_001_register_performance_cycle_definition',
        'performance_cycle_effective_window_overlaps',
        "INSERT INTO '.TB_PREF.'hrm_tal_001_performance_cycle_definitions",
        "array('development','performance_review','goal_review')"
    ), 'HRM-TAL-001 performance cycle writer');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_performance_evidence_writer_source($patch)
{
    return hrm_tal_001_require_source_tokens($patch, 'hrm/includes/db/hrm_tal_001_performance_db.inc', array(
        'hrm_tal_001_performance_evidence_writer_allowed_for_database_version',
        'hrm_tal_001_register_performance_review_subject',
        'hrm_tal_001_record_performance_assessment_evidence',
        'performance_cycle_missing_or_cross_company',
        'performance_subject_missing_or_cross_company',
        'performance_raw_comment_denied',
        "array('self','manager','reviewer')",
        "INSERT INTO '.TB_PREF.'hrm_tal_001_performance_review_subjects",
        "INSERT INTO '.TB_PREF.'hrm_tal_001_performance_assessment_evidence"
    ), 'HRM-TAL-001 performance evidence writer');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_performance_read_only_source($patch)
{
    return hrm_tal_001_require_source_tokens($patch, 'hrm/includes/db/hrm_tal_001_performance_db.inc', array(
        'hrm_tal_001_performance_read_only_view_allowed_for_database_version',
        'hrm_tal_001_read_performance_for_employee',
        'WHERE s.company_id=',
        'AND s.employee_id=',
        'ORDER BY s.review_window_start DESC',
        'LEFT JOIN',
        'hrm_tal_001_performance_assessment_evidence'
    ), 'HRM-TAL-001 performance read-only view');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_performance_admin_browser_source($patch)
{
    return hrm_tal_001_require_source_tokens($patch, 'hrm/manage/talent_skills_certifications.php', array(
        "\$page_security = 'SA_HRSETTINGS'",
        'check_csrf_token()',
        'hrm_tal_001_performance_admin_browser_allowed_for_database_version',
        'performance_cycle',
        'performance_subject',
        'performance_assessment',
        'performance_read',
        'hrm_tal_001_register_performance_cycle_definition',
        'hrm_tal_001_register_performance_review_subject',
        'hrm_tal_001_record_performance_assessment_evidence',
        'hrm_tal_001_read_performance_for_employee'
    ), 'HRM-TAL-001 performance admin browser');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_performance_self_service_source($patch)
{
    if (!hrm_tal_001_require_source_tokens($patch, 'hrm/includes/db/hrm_tal_001_self_view_db.inc', array(
        'hrm_tal_001_performance_self_service_view_allowed_for_database_version',
        'hrm_tal_001_read_performance_for_self',
        "user_check_access('SA_HRM_ESS_VIEW_SELF')",
        'get_hrm_ess_current_self_scope()',
        'WHERE s.company_id=',
        'AND s.employee_id='
    ), 'HRM-TAL-001 ESS self performance service')) return false;
    return hrm_tal_001_require_source_tokens($patch, 'hrm/ess/talent.php', array(
        "\$page_security = 'SA_HRM_ESS_VIEW_SELF'",
        'hrm_tal_001_performance_self_service_view_allowed_for_database_version',
        'hrm_tal_001_read_performance_for_self',
        'accepts no employee identifier from the browser'
    ), 'HRM-TAL-001 ESS self performance route');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_performance_manager_team_source($patch)
{
    if (!hrm_tal_001_require_source_tokens($patch, 'hrm/includes/db/hrm_tal_001_manager_view_db.inc', array(
        'hrm_tal_001_performance_manager_team_view_allowed_for_database_version',
        'hrm_tal_001_manager_scope_subjects',
        'hrm_tal_001_read_performance_for_manager_team',
        'WHERE s.company_id=',
        's.employee_id IN (',
        "'scope_origin'"
    ), 'HRM-TAL-001 manager/team performance service')) return false;
    return hrm_tal_001_require_source_tokens($patch, 'hrm/ess/team_talent.php', array(
        "\$page_security = 'SA_HRM_ESS_VIEW_TEAM'",
        'hrm_tal_001_performance_manager_team_view_allowed_for_database_version',
        'hrm_ess_002_today()',
        'hrm_tal_001_read_performance_for_manager_team',
        'accepts no employee selector or historical as-of date'
    ), 'HRM-TAL-001 manager/team performance route');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_performance_workflow_source($patch)
{
    if (!hrm_tal_001_require_source_tokens($patch, 'hrm/includes/db/hrm_tal_001_performance_workflow_db.inc', array(
        'hrm_tal_001_performance_workflow_writer_allowed_for_database_version',
        'hrm_tal_001_record_performance_workflow_transition',
        "array('open'=>array('submitted'), 'submitted'=>array('reviewed'), 'reviewed'=>array('finalized')",
        'performance_reviewer_evidence_required',
        "INSERT INTO '.TB_PREF.'hrm_tal_001_performance_workflow_events",
        'employment-decision authority'
    ), 'HRM-TAL-001 bounded performance workflow service')) return false;
    return hrm_tal_001_require_source_tokens($patch, 'hrm/manage/talent_skills_certifications.php', array(
        'performance_workflow_transition',
        'hrm_tal_001_record_performance_workflow_transition',
        "hidden('tal_action', 'performance_workflow_transition')",
        'performance_workflow_reason_sha256'
    ), 'HRM-TAL-001 bounded performance workflow browser');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_performance_score_finalization_source($patch)
{
    if (!hrm_tal_001_require_source_tokens($patch, 'hrm/includes/db/hrm_tal_001_performance_workflow_db.inc', array(
        'hrm_tal_001_performance_score_finalization_allowed_for_database_version',
        'hrm_tal_001_finalize_performance_review',
        'performance_finalization_requires_reviewed_state',
        'performance_reviewer_numeric_evidence_required',
        'hrm_tal_001_rating_to_units',
        'hrm_tal_001_units_to_rating',
        'rating_basis_sha256',
        'aggregate_rating_value'
    ), 'HRM-TAL-001 deterministic performance finalization service')) return false;
    return hrm_tal_001_require_source_tokens($patch, 'hrm/manage/talent_skills_certifications.php', array(
        'performance_finalize',
        'hrm_tal_001_finalize_performance_review',
        "hidden('tal_action', 'performance_finalize')",
        'performance_finalize_reason_sha256'
    ), 'HRM-TAL-001 deterministic performance finalization browser');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_performance_export_contract_source($patch)
{
    if (!hrm_tal_001_require_source_tokens($patch, 'includes/hrm_tal_001_contract.inc', array(
        "'performance_export_enabled' => true",
        'hrm_tal_001_performance_export_contract_required_for_database_version',
        "'1.0.966'",
        'hrm_tal_001_performance_export_service_allowed_for_database_version',
        "'1.0.967'",
        'hrm_tal_001_performance_export_browser_allowed_for_database_version',
        "'1.0.968'"
    ), 'HRM-TAL-001 performance export contract')) return false;
    return hrm_tal_001_require_source_tokens($patch, 'hrm/includes/db/hrm_tal_001_export_db.inc', array(
        "INSERT INTO '.TB_PREF.'hrm_tal_001_export_audit",
        'subject_reference_sha256',
        'export_sha256'
    ), 'HRM-TAL-001 immutable export audit custody reuse');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_performance_export_service_source($patch)
{
    return hrm_tal_001_require_source_tokens($patch, 'hrm/includes/db/hrm_tal_001_performance_export_db.inc', array(
        "user_check_access('SA_HRSETTINGS')",
        'hrm_tal_001_performance_export_service_allowed_for_database_version',
        'HRM_TAL_001_PERFORMANCE_EXPORT_MAX_ROWS',
        'HRM_TAL_001_PERFORMANCE_EXPORT_MAX_BYTES',
        'hrm_tal_001_export_exact_employee_performance_csv',
        "s.employee_id='.db_escape(\$employee_id)",
        "LIMIT '.\$limit",
        'hrm_tal_001_export_write_row',
        "INSERT INTO '.TB_PREF.'hrm_tal_001_export_audit",
        "'admin_performance_exact_employee'",
        "'scope_code'=>'admin_performance_exact_employee'"
    ), 'HRM-TAL-001 bounded performance export service');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_performance_export_browser_source($patch)
{
    if (!hrm_tal_001_require_source_tokens($patch, 'hrm/manage/talent_performance_export.php', array(
        "\$page_security='SA_HRSETTINGS'",
        'hrm_tal_001_performance_export_browser_allowed_for_database_version',
        'REQUEST_METHOD',
        'check_csrf_token()',
        'hrm_tal_001_export_exact_employee_performance_csv',
        'while (ob_get_level()) ob_end_clean();',
        'X-Content-Type-Options: nosniff',
        'Cache-Control: no-store, private',
        'Content-Disposition: attachment'
    ), 'HRM-TAL-001 performance export browser route')) return false;
    return hrm_tal_001_require_source_tokens($patch, 'hrm/manage/talent_skills_certifications.php', array(
        "action='talent_performance_export.php'",
        "hidden('_token', ensure_csrf_token())",
        "name='employee_id'",
        'Export Governed Performance CSV'
    ), 'HRM-TAL-001 performance export browser navigation');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_learning_contract_source($patch)
{
    return hrm_tal_001_require_source_tokens($patch, 'includes/hrm_tal_001_contract.inc', array(
        "'learning_foundation_enabled' => true",
        "'learning_runtime_enabled' => true",
        "'learning_browser_ui_enabled' => false",
        "'learning_legacy_training_bridge_allowed' => false",
        'hrm_tal_001_learning_authority_contract_required_for_database_version',
        "'1.0.969'",
        'hrm_tal_001_learning_definition_custody_required_for_database_version',
        "'1.0.970'",
        'hrm_tal_001_learning_assignment_custody_required_for_database_version',
        "'1.0.971'",
        'hrm_tal_001_learning_completion_custody_required_for_database_version',
        "'1.0.972'",
        'hrm_tal_001_learning_runtime_service_allowed_for_database_version',
        "'1.0.973'"
    ), 'HRM-TAL-001 bounded learning authority contract');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_learning_runtime_source($patch)
{
    return hrm_tal_001_require_source_tokens($patch, 'hrm/includes/db/hrm_tal_001_learning_db.inc', array(
        "user_check_access('SA_HRSETTINGS')",
        'hrm_tal_001_learning_runtime_service_allowed_for_database_version',
        'hrm_tal_001_learning_begin_transaction',
        'FOR UPDATE',
        'hrm_tal_001_runtime_company_id()',
        'hrm_tal_001_runtime_actor_id()',
        'hrm_tal_001_employee_exists',
        'hrm_tal_001_register_learning_definition',
        'hrm_tal_001_assign_learning_to_employee',
        'hrm_tal_001_record_learning_completion',
        'hrm_tal_001_read_learning_for_employee',
        'INSERT INTO ".TB_PREF."hrm_tal_001_learning_definitions',
        'INSERT INTO ".TB_PREF."hrm_tal_001_learning_assignments',
        'INSERT INTO ".TB_PREF."hrm_tal_001_learning_completion_evidence',
        'created_by_user_id',
        'assigned_by_user_id',
        'recorded_by_user_id',
        'UTC_TIMESTAMP()',
        'learning_assignment_outside_definition_window',
        'learning_completion_before_assignment',
        'learning_definition_missing_or_cross_company',
        'learning_assignment_missing_or_cross_company',
        'SELECT a.learning_assignment_id,d.learning_code,d.learning_name',
        'WHERE a.company_id=',
        'AND a.employee_id=',
        'ON d.learning_definition_id=a.learning_definition_id AND d.company_id=a.company_id',
        'ON c.learning_assignment_id=a.learning_assignment_id AND c.company_id=a.company_id',
        'legacy training mutation is not authorized'
    ), 'HRM-TAL-001 bounded learning runtime service');
}

/** @param object $patch @return bool */
function hrm_tal_001_require_governance_contract($patch)
{
    $contract = hrm_tal_001_governance_contract();
    return isset($contract['optional_module']) && $contract['optional_module'] === true
        && $contract['effective_history_required'] === true
        && $contract['object_security_required'] === true
        && $contract['skills_foundation_enabled'] === true
        && $contract['certifications_foundation_enabled'] === true
        && $contract['performance_foundation_enabled'] === true
        && $contract['performance_runtime_enabled'] === false
        && $contract['learning_foundation_enabled'] === true
        && $contract['learning_runtime_enabled'] === true
        && $contract['learning_browser_ui_enabled'] === false
        && $contract['learning_legacy_training_bridge_allowed'] === false
        && $contract['recruitment_mutation_allowed'] === false
        && $contract['training_mutation_allowed'] === false
        && $contract['appraisal_mutation_allowed'] === false
        && $contract['payroll_mutation_allowed'] === false
        && $contract['employment_decision_authority_allowed'] === false
        && $contract['cross_company_authority_allowed'] === false
        && $contract['writer_runtime_allowed'] === false
        && $contract['browser_ui_allowed'] === false;
}
?>
