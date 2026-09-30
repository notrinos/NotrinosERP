<?php
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
include_once($path_to_root.'/includes/pay_ai_001_contract.inc');

/** @return array */
function pay_ai_001_schema_groups()
{
    return array(
        'request_custody' => array('sql'=>'sql/upgrade_pay_ai_001_request_custody.sql', 'tables'=>array('hrm_pay_ai_001_requests')),
        'output_custody' => array('sql'=>'sql/upgrade_pay_ai_001_output_custody.sql', 'tables'=>array('hrm_pay_ai_001_outputs')),
        'review_custody' => array('sql'=>'sql/upgrade_pay_ai_001_review_custody.sql', 'tables'=>array('hrm_pay_ai_001_reviews')),
    );
}

/** @return array */
function pay_ai_001_expected_columns()
{
    return array(
        'hrm_pay_ai_001_requests'=>array('ai_request_id','company_id','capability_code','purpose_code','actor_user_id','authorized_input_refs_sha256','object_scope_sha256','field_scope_sha256','data_minimization_sha256','provider_policy_sha256','opt_out_checked','request_sha256','requested_at','created_at'),
        'hrm_pay_ai_001_outputs'=>array('ai_output_id','company_id','ai_request_id','provider_code','model_version','generated_content_sha256','grounding_refs_sha256','confidence_evidence_sha256','abstained','provider_failure','output_sha256','generated_at','created_at'),
        'hrm_pay_ai_001_reviews'=>array('ai_review_id','company_id','ai_output_id','reviewer_user_id','disposition_code','reason_sha256','false_positive','appeal_requested','review_sha256','reviewed_at','created_at'),
    );
}

/** @return array */
function pay_ai_001_trigger_codes()
{
    return array('hrm_pay_ai_001_requests'=>'rq','hrm_pay_ai_001_outputs'=>'ou','hrm_pay_ai_001_reviews'=>'rv');
}

/** @param string $table @return array|false */
function pay_ai_001_table_columns($table)
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
function pay_ai_001_table_innodb($table)
{
    $physical = hrm_migration_physical_table_name($table);
    if ($physical === false) return false;
    $result = db_query('SELECT ENGINE FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($physical));
    $row = $result ? db_fetch_assoc($result) : false;
    return $row && strcasecmp((string)$row['ENGINE'], 'InnoDB') === 0;
}

/** @param string $table @param string $operation @return bool */
function pay_ai_001_trigger_valid($table, $operation)
{
    $codes = pay_ai_001_trigger_codes();
    $prefix = hrm_migration_active_table_prefix();
    $physical = hrm_migration_physical_table_name($table);
    $operation = strtoupper((string)$operation);
    if ($prefix === false || $physical === false || !isset($codes[$table]) || !in_array($operation, array('UPDATE','DELETE'), true)) return false;
    $name = $prefix.'pai1_'.$codes[$table].'_'.($operation === 'UPDATE' ? 'u' : 'd');
    $result = db_query('SELECT EVENT_MANIPULATION,EVENT_OBJECT_TABLE,ACTION_TIMING,ACTION_STATEMENT FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA=DATABASE() AND TRIGGER_NAME='.db_escape($name).' LIMIT 2');
    if (!$result || db_num_rows($result) !== 1) return false;
    $row = db_fetch_assoc($result);
    $expected = "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-AI-001 immutable ".$table.' row cannot be '.strtolower($operation).'d' . "'";
    return strtoupper((string)$row['EVENT_MANIPULATION']) === $operation
        && (string)$row['EVENT_OBJECT_TABLE'] === $physical
        && strtoupper((string)$row['ACTION_TIMING']) === 'BEFORE'
        && trim((string)$row['ACTION_STATEMENT']) === $expected;
}

/** @param object $patch @param string $table @return bool */
function pay_ai_001_create_trigger($patch, $table, $operation)
{
    if (pay_ai_001_trigger_valid($table, $operation)) return true;
    $codes = pay_ai_001_trigger_codes();
    $prefix = hrm_migration_active_table_prefix();
    $physical = hrm_migration_physical_table_name($table);
    $operation = strtoupper((string)$operation);
    if ($prefix === false || $physical === false || !isset($codes[$table])) return false;
    $name = $prefix.'pai1_'.$codes[$table].'_'.($operation === 'UPDATE' ? 'u' : 'd');
    $message = 'PAY-AI-001 immutable '.$table.' row cannot be '.strtolower($operation).'d';
    $sql = 'CREATE TRIGGER `'.$name.'` BEFORE '.$operation.' ON `'.$physical."` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT=".db_escape($message);
    if (!db_query($sql)) return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-AI-001 trigger creation failed: '.$table.'.'.$operation);
    return pay_ai_001_trigger_valid($table, $operation);
}

/** @param object $patch @param string $table @return bool */
function pay_ai_001_require_table($patch, $table)
{
    $columns = pay_ai_001_expected_columns();
    if (!isset($columns[$table]) || pay_ai_001_table_columns($table) !== $columns[$table] || !pay_ai_001_table_innodb($table))
        return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-AI-001 table projection mismatch: '.$table);
    foreach (array('UPDATE','DELETE') as $operation)
        if (!pay_ai_001_trigger_valid($table, $operation))
            return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'PAY-AI-001 immutable trigger mismatch: '.$table.'.'.$operation);
    return true;
}

/** @param object $patch @param array $tables @return bool */
function pay_ai_001_require_zero_rows($patch, array $tables)
{
    foreach ($tables as $table) {
        $result = db_query('SELECT COUNT(*) n FROM '.TB_PREF.$table);
        $row = $result ? db_fetch_assoc($result) : false;
        if (!$row || (int)$row['n'] !== 0)
            return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-AI-001 foundation must remain zero-seed: '.$table);
    }
    return true;
}

/** @param object $patch @param string $group @return bool */
function pay_ai_001_install_group($patch, $group)
{
    global $path_to_root;
    $groups = pay_ai_001_schema_groups();
    if (!isset($groups[$group])) return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'Unknown PAY-AI-001 schema group: '.$group);
    $relative = $groups[$group]['sql'];
    $raw = @file_get_contents($path_to_root.'/'.$relative);
    if (!is_string($raw) || strpos($raw, '-- PAY-AI-001 GROUP '.$group) === false)
        return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'PAY-AI-001 schema source missing: '.$relative);
    $prefix = hrm_migration_active_table_prefix();
    if ($prefix === false) return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-AI-001 table prefix unavailable.');
    $raw = preg_replace('/^--.*$/m', '', $raw);
    $statements = preg_split('/;\s*(?:\r?\n|$)/', $raw);
    foreach ($statements as $statement) {
        $statement = trim($statement);
        if ($statement === '' || strpos($statement, 'CREATE TRIGGER') === 0) continue;
        $statement = str_replace('`0_', '`'.$prefix, $statement);
        if (!db_query($statement)) return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-AI-001 schema statement failed: '.$relative);
    }
    foreach ($groups[$group]['tables'] as $table) {
        foreach (array('UPDATE','DELETE') as $operation) if (!pay_ai_001_create_trigger($patch, $table, $operation)) return false;
        if (!pay_ai_001_require_table($patch, $table)) return false;
    }
    return pay_ai_001_require_zero_rows($patch, $groups[$group]['tables']);
}

/** @param object $patch @param string $relative @param array $tokens @param string $label @return bool */
function pay_ai_001_require_source_tokens($patch, $relative, array $tokens, $label)
{
    global $path_to_root;
    $source = @file_get_contents($path_to_root.'/'.$relative);
    if (!is_string($source)) return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, $label.' source is missing.');
    foreach ($tokens as $token) if (strpos($source, $token) === false)
        return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, $label.' source contract is incomplete.');
    return true;
}

/** @param object $patch @return bool */
function pay_ai_001_require_predecessor($patch)
{
    $allowed = array('1.0.927','1.0.928','1.0.929','1.0.930','1.0.931','1.0.932');
    if (!is_object($patch) || !isset($patch->previous) || !in_array((string)$patch->previous, $allowed, true))
        return hrm_migration_fail($patch, HRM_MIGRATION_PREDECESSOR_MIGRATION_ABSENT, 'PAY-AI-001 predecessor version is not accepted.');
    if (!hrm_migration_registered_edge_matches('hrm_anl_001_department_employment_type_headcount_navigation','1.0.926','1.0.927'))
        return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'PAY-AI-001 requires the accepted HRM-ANL-001 source predecessor.');
    if (!pay_ai_001_require_source_tokens($patch, 'Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/IMPLEMENTATION_BACKLOG.md', array('PAY-CORE-008 validation, variance and reconciliation is **CLOSED / LOCAL ACCEPTANCE PASS**','HRM-ANL-001 status:** **CLOSED / LOCAL ACCEPTANCE PASS**','PAY-AI-001 — Optional anomaly and explanation assistant'), 'PAY-AI-001 dependency/backlog')) return false;
    return true;
}

/** @param object $patch @return bool */
function pay_ai_001_require_governance_contract($patch)
{
    $contract = pay_ai_001_governance_contract();
    return isset($contract['optional']) && $contract['optional'] === true
        && $contract['human_review_required'] === true
        && $contract['grounded_authorized_references_required'] === true
        && $contract['authoritative_write_allowed'] === false
        && $contract['network_authority_allowed'] === false
        && $contract['provider_runtime_allowed'] === false
        && $contract['raw_prompt_storage_allowed'] === false
        && $contract['raw_output_storage_allowed'] === false;
}

/** @param object $patch @return bool */
function pay_ai_001_require_request_review($patch)
{
    return pay_ai_001_require_source_tokens($patch, 'includes/pay_ai_001_contract.inc', array(
        "'anomaly_ranking'", "'variance_explanation'", "'missing_data_suggestion'",
        "'human_review_required' => true", "'grounded_authorized_references_required' => true",
        "'authoritative_write_allowed' => false", "'legal_advice_allowed' => false",
        "'network_authority_allowed' => false", "'raw_prompt_storage_allowed' => false"
    ), 'PAY-AI-001 governance/request custody');
}

/** @param object $patch @return bool */
function pay_ai_001_require_output_review($patch)
{
    return pay_ai_001_require_source_tokens($patch, 'includes/pay_ai_001_provider_contract.inc', array(
        'interface PayAI001ProviderDescriptor', 'getProviderCode()', 'getModelVersion()', 'getProviderPolicySha256()',
        'intentionally not an invocation interface'
    ), 'PAY-AI-001 provider-neutral metadata contract');
}

/** @param object $patch @return bool */
function pay_ai_001_require_review_review($patch)
{
    return pay_ai_001_require_source_tokens($patch, 'includes/pay_ai_001_contract.inc', array(
        "'bias_review_required' => true", "'opt_out_respected' => true", "'autonomous_employment_decision_allowed' => false",
        "'formula_or_rule_publish_authority_allowed' => false", "'approval_authority_allowed' => false"
    ), 'PAY-AI-001 human-review authority ceiling');
}

/** @param object $patch @return bool */
function pay_ai_001_require_grounding_review($patch)
{
    return pay_ai_001_require_source_tokens($patch, 'includes/pay_ai_001_grounding_contract.inc', array(
        'pay_ai_001_build_grounded_input', 'grounding_reference_unauthorized', 'grounding_reference_opted_out',
        'grounding_field_denied', 'contains_raw_prompt', 'contains_employee_identity', 'contains_bank_or_tax_detail'
    ), 'PAY-AI-001 permission-filtered grounding contract');
}

/** @param object $patch @return bool */
function pay_ai_001_require_baseline_review($patch)
{
    return pay_ai_001_require_source_tokens($patch, 'includes/pay_ai_001_baseline_contract.inc', array(
        'pay_ai_001_build_deterministic_baseline', 'deterministic-aggregate-delta-v1', 'requires_human_review',
        'suppressed', 'missing', 'anomaly_score', 'magnitude_band'
    ), 'PAY-AI-001 deterministic anomaly baseline contract');
}

/** @param object $patch @return bool */
function pay_ai_001_require_runtime_review($patch)
{
    return pay_ai_001_require_source_tokens($patch, 'includes/pay_ai_001_runtime_adapter.inc', array(
        'pay_ai_001_execute_deterministic_assistance', 'assistive-deterministic-not-authoritative',
        "'human_review_required'=>true", "'provider_invoked'=>false", "'network_used'=>false", "'authoritative_write_allowed'=>false"
    ), 'PAY-AI-001 deterministic assistive runtime');
}

?>
