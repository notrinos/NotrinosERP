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
    $allowed = array('1.0.936','1.0.937','1.0.938','1.0.939','1.0.940','1.0.941','1.0.942','1.0.943','1.0.944','1.0.945','1.0.946');
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
        "\$page_security='SA_HRM_ESS_VIEW_SELF'",
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
function hrm_tal_001_require_governance_contract($patch)
{
    $contract = hrm_tal_001_governance_contract();
    return isset($contract['optional_module']) && $contract['optional_module'] === true
        && $contract['effective_history_required'] === true
        && $contract['object_security_required'] === true
        && $contract['skills_foundation_enabled'] === true
        && $contract['certifications_foundation_enabled'] === true
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
