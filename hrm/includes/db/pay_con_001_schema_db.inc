<?php
/** PAY-CON-001 immutable provider profile and mapping-version custody. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');

function pay_con_001_tables()
{
    return array('hrm_pay_con_001_provider_profiles','hrm_pay_con_001_mapping_versions');
}

function pay_con_001_expected_columns()
{
    return array(
        'hrm_pay_con_001_provider_profiles' => array(
            'provider_profile_id','company_id','provider_code','delivery_model_code','jurisdiction_code','residency_region_code',
            'adapter_key','due_diligence_sha256','data_residency_review_sha256','security_review_sha256','provider_contract_sha256',
            'requested_by','reviewed_by','approved_by','approved_at','profile_sha256','created_at'
        ),
        'hrm_pay_con_001_mapping_versions' => array(
            'mapping_version_id','provider_profile_id','mapping_key','exchange_direction','payload_class','mapping_version',
            'source_schema_sha256','target_schema_sha256','mapping_sha256','effective_from','effective_to','reviewed_by',
            'approved_by','approved_at','mapping_version_sha256','created_at'
        )
    );
}

function pay_con_001_table_columns($table)
{
    $physical = hrm_migration_physical_table_name($table);
    if ($physical === false) return false;
    $result = db_query("SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME=".db_escape($physical)." ORDER BY ORDINAL_POSITION");
    if (!$result) return false;
    $columns = array();
    while ($row = db_fetch_assoc($result)) $columns[] = (string)$row['COLUMN_NAME'];
    return $columns;
}

function pay_con_001_table_innodb($table)
{
    $physical = hrm_migration_physical_table_name($table);
    if ($physical === false) return false;
    $result = db_query("SELECT ENGINE FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME=".db_escape($physical)." AND TABLE_TYPE='BASE TABLE'");
    $row = $result ? db_fetch_assoc($result) : false;
    return $row && strcasecmp((string)$row['ENGINE'], 'InnoDB') === 0;
}

function pay_con_001_trigger_code($table)
{
    return $table === 'hrm_pay_con_001_provider_profiles' ? 'pp' : ($table === 'hrm_pay_con_001_mapping_versions' ? 'mv' : false);
}

function pay_con_001_trigger_valid($table, $operation)
{
    $prefix = hrm_migration_active_table_prefix();
    $physical = hrm_migration_physical_table_name($table);
    $code = pay_con_001_trigger_code($table);
    if ($prefix === false || $physical === false || $code === false) return false;
    $suffix = $operation === 'UPDATE' ? 'u' : 'd';
    $name = $prefix.'pcon1_'.$code.'_'.$suffix;
    $result = db_query("SELECT EVENT_MANIPULATION,ACTION_TIMING,EVENT_OBJECT_TABLE,ACTION_STATEMENT FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA=DATABASE() AND TRIGGER_NAME=".db_escape($name));
    $row = $result ? db_fetch_assoc($result) : false;
    return $row
        && strtoupper((string)$row['EVENT_MANIPULATION']) === $operation
        && strtoupper((string)$row['ACTION_TIMING']) === 'BEFORE'
        && (string)$row['EVENT_OBJECT_TABLE'] === $physical
        && strpos((string)$row['ACTION_STATEMENT'], 'PAY-CON-001 immutable '.$table.' row cannot be') !== false;
}

function pay_con_001_create_trigger($patch, $table, $operation)
{
    if (pay_con_001_trigger_valid($table, $operation)) return true;
    $prefix = hrm_migration_active_table_prefix();
    $physical = hrm_migration_physical_table_name($table);
    $code = pay_con_001_trigger_code($table);
    if ($prefix === false || $physical === false || $code === false)
        return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-CON-001 trigger target invalid: '.$table.'.');
    $suffix = $operation === 'UPDATE' ? 'u' : 'd';
    $name = $prefix.'pcon1_'.$code.'_'.$suffix;
    $message = 'PAY-CON-001 immutable '.$table.' row cannot be '.strtolower($operation).'d';
    if (!db_query('CREATE TRIGGER `'.$name.'` BEFORE '.$operation.' ON `'.$physical.'` FOR EACH ROW SIGNAL SQLSTATE \'45000\' SET MESSAGE_TEXT='.db_escape($message)))
        return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-CON-001 immutable trigger creation failed: '.$name.'.');
    return pay_con_001_trigger_valid($table, $operation);
}

function pay_con_001_mapping_fk_valid()
{
    $physical = hrm_migration_physical_table_name('hrm_pay_con_001_mapping_versions');
    $referenced = hrm_migration_physical_table_name('hrm_pay_con_001_provider_profiles');
    if ($physical === false || $referenced === false) return false;
    $result = db_query("SELECT rc.UPDATE_RULE,rc.DELETE_RULE,kcu.COLUMN_NAME,kcu.REFERENCED_TABLE_NAME,kcu.REFERENCED_COLUMN_NAME "
        ."FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS rc JOIN INFORMATION_SCHEMA.KEY_COLUMN_USAGE kcu "
        ."ON kcu.CONSTRAINT_SCHEMA=rc.CONSTRAINT_SCHEMA AND kcu.CONSTRAINT_NAME=rc.CONSTRAINT_NAME "
        ."WHERE rc.CONSTRAINT_SCHEMA=DATABASE() AND rc.CONSTRAINT_NAME='fk_pcon1_mv_profile' AND rc.TABLE_NAME=".db_escape($physical));
    $row = $result ? db_fetch_assoc($result) : false;
    return $row
        && strtoupper((string)$row['UPDATE_RULE']) === 'RESTRICT'
        && strtoupper((string)$row['DELETE_RULE']) === 'RESTRICT'
        && (string)$row['COLUMN_NAME'] === 'provider_profile_id'
        && (string)$row['REFERENCED_TABLE_NAME'] === $referenced
        && (string)$row['REFERENCED_COLUMN_NAME'] === 'provider_profile_id';
}

function pay_con_001_require_table($patch, $table)
{
    $columns = pay_con_001_expected_columns();
    if (!isset($columns[$table]) || pay_con_001_table_columns($table) !== $columns[$table] || !pay_con_001_table_innodb($table))
        return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-CON-001 table projection mismatch: '.$table.'.');
    foreach (array('UPDATE','DELETE') as $operation)
        if (!pay_con_001_trigger_valid($table, $operation))
            return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'PAY-CON-001 immutable trigger mismatch: '.$table.'.'.$operation.'.');
    return true;
}

function pay_con_001_require_zero_rows($patch)
{
    foreach (pay_con_001_tables() as $table) {
        $physical = hrm_migration_physical_table_name($table);
        $result = $physical !== false ? db_query('SELECT COUNT(*) row_count FROM `'.$physical.'`') : false;
        $row = $result ? db_fetch_assoc($result) : false;
        if (!$row || (int)$row['row_count'] !== 0)
            return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-CON-001 foundation must remain zero-seed: '.$table.'.');
    }
    return true;
}

function pay_con_001_install_schema($patch)
{
    global $path_to_root;
    $raw = @file_get_contents($path_to_root.'/sql/upgrade_pay_con_001_schema.sql');
    if (!is_string($raw) || strpos($raw, '-- PAY-CON-001 GROUP provider_mapping_custody') === false)
        return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'PAY-CON-001 schema source missing.');
    $prefix = hrm_migration_active_table_prefix();
    if ($prefix === false)
        return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-CON-001 table prefix unavailable.');
    $raw = preg_replace('/^--.*$/m', '', $raw);
    $statements = preg_split('/;\s*(?:\r?\n|$)/', $raw);
    foreach ($statements as $statement) {
        $statement = trim($statement);
        if ($statement === '' || strpos($statement, 'CREATE TRIGGER') === 0) continue;
        $statement = str_replace('`0_', '`'.$prefix, $statement);
        if (!db_query($statement))
            return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-CON-001 schema statement failed.');
    }
    foreach (pay_con_001_tables() as $table) {
        foreach (array('UPDATE','DELETE') as $operation)
            if (!pay_con_001_create_trigger($patch, $table, $operation)) return false;
        if (!pay_con_001_require_table($patch, $table)) return false;
    }
    if (!pay_con_001_mapping_fk_valid())
        return hrm_migration_fail($patch, HRM_MIGRATION_FK_MISMATCH, 'PAY-CON-001 mapping/profile foreign-key projection mismatch.');
    return pay_con_001_require_zero_rows($patch);
}
?>
