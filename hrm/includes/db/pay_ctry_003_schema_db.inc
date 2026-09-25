<?php
/** PAY-CTRY-003 exact immutable schema contract. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
include_once($path_to_root.'/includes/pay_ctry_003_contract.inc');

/** Return the PAY-CTRY-003 schema groups in dependency order. */
function pay_ctry_003_schema_groups()
{
    return array(
        'registry' => array('pay_ctry_capability_contract_versions'),
        'definitions' => array('pay_ctry_capability_contract_definitions'),
        'adapters' => array('pay_ctry_host_adapters'),
        'artifact' => array('pay_ctry_pack_artifact_contracts'),
        'report' => array('pay_ctry_contract_validation_reports'),
        'migration' => array('pay_ctry_contract_migration_edges')
    );
}

/** Return the exact ordered columns for every PAY-CTRY-003 table. */
function pay_ctry_003_schema_columns()
{
    return array(
        'pay_ctry_capability_contract_versions' => array('registry_version_id','registry_version','registry_sha256','signature_scheme','signed_by','signature_sha256','host_min_version','host_max_version','status','created_by','created_at'),
        'pay_ctry_capability_contract_definitions' => array('capability_definition_id','registry_version_id','capability_code','namespace','semantic','cardinality','exclusive_group','artifact_type','host_adapter','host_version','definition_sha256','created_at'),
        'pay_ctry_host_adapters' => array('host_adapter_id','adapter_code','adapter_version','media_types_json','adapter_sha256','status','created_at'),
        'pay_ctry_pack_artifact_contracts' => array('artifact_contract_id','pack_version_id','artifact_id','registry_version_id','capability_definition_id','ordinal_no','artifact_type','media_type','host_adapter','host_version','payload_sha256','contract_sha256','contract_status','created_by','created_at'),
        'pay_ctry_contract_validation_reports' => array('validation_report_id','artifact_contract_id','pack_version_id','registry_version_id','capability_code','validation_status','failure_code','details_json','report_sha256','verified_by','created_at'),
        'pay_ctry_contract_migration_edges' => array('migration_edge_id','artifact_contract_id','predecessor_version','successor_version','migration_strategy','reversible','edge_sha256','created_at')
    );
}

/** Return primary keys for every PAY-CTRY-003 table. */
function pay_ctry_003_primary_keys()
{
    return array(
        'pay_ctry_capability_contract_versions' => array('registry_version_id'),
        'pay_ctry_capability_contract_definitions' => array('capability_definition_id'),
        'pay_ctry_host_adapters' => array('host_adapter_id'),
        'pay_ctry_pack_artifact_contracts' => array('artifact_contract_id'),
        'pay_ctry_contract_validation_reports' => array('validation_report_id'),
        'pay_ctry_contract_migration_edges' => array('migration_edge_id')
    );
}

/** Return required non-primary indexes for every PAY-CTRY-003 table. */
function pay_ctry_003_required_indexes()
{
    return array(
        'pay_ctry_capability_contract_versions' => array(
            'uq_pct3_cv_version' => array(1, array('registry_version')),
            'uq_pct3_cv_sha' => array(1, array('registry_sha256')),
            'uq_pct3_cv_signature' => array(1, array('signature_sha256'))
        ),
        'pay_ctry_capability_contract_definitions' => array(
            'uq_pct3_cd_code' => array(1, array('registry_version_id','capability_code')),
            'uq_pct3_cd_sha' => array(1, array('definition_sha256')),
            'ix_pct3_cd_adapter' => array(0, array('host_adapter','host_version'))
        ),
        'pay_ctry_host_adapters' => array(
            'uq_pct3_ha_code' => array(1, array('adapter_code','adapter_version')),
            'uq_pct3_ha_sha' => array(1, array('adapter_sha256'))
        ),
        'pay_ctry_pack_artifact_contracts' => array(
            'uq_pct3_ac_identity' => array(1, array('pack_version_id','artifact_id','capability_definition_id')),
            'uq_pct3_ac_sha' => array(1, array('contract_sha256')),
            'ix_pct3_ac_registry' => array(0, array('registry_version_id','contract_status')),
            'ix_pct3_ac_host' => array(0, array('host_adapter','host_version')),
            'fk_pct3_ac_artifact' => array(0, array('artifact_id')),
            'fk_pct3_ac_definition' => array(0, array('capability_definition_id'))
        ),
        'pay_ctry_contract_validation_reports' => array(
            'uq_pct3_vr_sha' => array(1, array('report_sha256')),
            'ix_pct3_vr_contract' => array(0, array('artifact_contract_id','validation_status')),
            'ix_pct3_vr_pack' => array(0, array('pack_version_id','capability_code')),
            'fk_pct3_vr_registry' => array(0, array('registry_version_id'))
        ),
        'pay_ctry_contract_migration_edges' => array(
            'uq_pct3_me_sha' => array(1, array('edge_sha256')),
            'uq_pct3_me_successor' => array(1, array('artifact_contract_id','successor_version')),
            'ix_pct3_me_predecessor' => array(0, array('predecessor_version'))
        )
    );
}

/** Return required foreign keys for every PAY-CTRY-003 table. */
function pay_ctry_003_required_foreign_keys()
{
    return array(
        'pay_ctry_capability_contract_versions' => array(),
        'pay_ctry_capability_contract_definitions' => array(
            'fk_pct3_cd_registry' => array('registry_version_id','pay_ctry_capability_contract_versions','registry_version_id')
        ),
        'pay_ctry_host_adapters' => array(),
        'pay_ctry_pack_artifact_contracts' => array(
            'fk_pct3_ac_pack' => array('pack_version_id','pay_ctry_pack_versions','pack_version_id'),
            'fk_pct3_ac_artifact' => array('artifact_id','pay_ctry_artifacts','artifact_id'),
            'fk_pct3_ac_registry' => array('registry_version_id','pay_ctry_capability_contract_versions','registry_version_id'),
            'fk_pct3_ac_definition' => array('capability_definition_id','pay_ctry_capability_contract_definitions','capability_definition_id')
        ),
        'pay_ctry_contract_validation_reports' => array(
            'fk_pct3_vr_contract' => array('artifact_contract_id','pay_ctry_pack_artifact_contracts','artifact_contract_id'),
            'fk_pct3_vr_pack' => array('pack_version_id','pay_ctry_pack_versions','pack_version_id'),
            'fk_pct3_vr_registry' => array('registry_version_id','pay_ctry_capability_contract_versions','registry_version_id')
        ),
        'pay_ctry_contract_migration_edges' => array(
            'fk_pct3_me_contract' => array('artifact_contract_id','pay_ctry_pack_artifact_contracts','artifact_contract_id')
        )
    );
}

/** Return deterministic trigger suffixes for the immutable tables. */
function pay_ctry_003_trigger_codes()
{
    return array(
        'pay_ctry_capability_contract_versions' => 'cv',
        'pay_ctry_capability_contract_definitions' => 'cd',
        'pay_ctry_host_adapters' => 'ha',
        'pay_ctry_pack_artifact_contracts' => 'ac',
        'pay_ctry_contract_validation_reports' => 'vr',
        'pay_ctry_contract_migration_edges' => 'me'
    );
}

/** Read ordered physical columns for one table. */
function pay_ctry_003_table_columns($table)
{
    if (!hrm_migration_table_exists($table)) return false;
    $query = db_query('SHOW COLUMNS FROM '.TB_PREF.$table);
    if (!$query) return false;
    $columns = array();
    while ($row = db_fetch_assoc($query)) $columns[] = (string)$row['Field'];
    return $columns;
}

/** Read the primary-key column order for one table. */
function pay_ctry_003_table_primary_key($table)
{
    $physical = hrm_migration_physical_table_name($table);
    if ($physical === false) return false;
    $query = db_query('SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($physical).' AND INDEX_NAME=\'PRIMARY\' ORDER BY SEQ_IN_INDEX');
    if (!$query) return false;
    $columns = array();
    while ($row = db_fetch_assoc($query)) $columns[] = (string)$row['COLUMN_NAME'];
    return $columns;
}

/** Read the storage engine for one table. */
function pay_ctry_003_table_engine($table)
{
    $physical = hrm_migration_physical_table_name($table);
    if ($physical === false) return false;
    $query = db_query('SELECT ENGINE FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($physical).' LIMIT 1');
    if (!$query || db_num_rows($query) !== 1) return false;
    $row = db_fetch_assoc($query);
    return $row ? (string)$row['ENGINE'] : false;
}

/** Read non-primary indexes in the source-independent shape used by tests. */
function pay_ctry_003_table_indexes($table)
{
    $physical = hrm_migration_physical_table_name($table);
    if ($physical === false) return false;
    $query = db_query('SELECT INDEX_NAME,NON_UNIQUE,SEQ_IN_INDEX,COLUMN_NAME FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($physical).' ORDER BY INDEX_NAME,SEQ_IN_INDEX');
    if (!$query) return false;
    $indexes = array();
    while ($row = db_fetch_assoc($query)) {
        if ($row['INDEX_NAME'] === 'PRIMARY') continue;
        $name = (string)$row['INDEX_NAME'];
        if (!isset($indexes[$name])) $indexes[$name] = array((int)$row['NON_UNIQUE'] === 0 ? 1 : 0, array());
        $indexes[$name][1][] = (string)$row['COLUMN_NAME'];
    }
    ksort($indexes);
    return $indexes;
}

/** Read foreign-key metadata using unprefixed repository table names. */
function pay_ctry_003_table_foreign_keys($table)
{
    $physical = hrm_migration_physical_table_name($table);
    $prefix = hrm_migration_active_table_prefix();
    if ($physical === false || $prefix === false) return false;
    $query = db_query('SELECT CONSTRAINT_NAME,COLUMN_NAME,REFERENCED_TABLE_NAME,REFERENCED_COLUMN_NAME FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($physical).' AND REFERENCED_TABLE_NAME IS NOT NULL ORDER BY CONSTRAINT_NAME,ORDINAL_POSITION');
    if (!$query) return false;
    $foreign_keys = array();
    while ($row = db_fetch_assoc($query)) {
        $referenced = (string)$row['REFERENCED_TABLE_NAME'];
        if (strpos($referenced, $prefix) === 0) $referenced = substr($referenced, strlen($prefix));
        $foreign_keys[(string)$row['CONSTRAINT_NAME']] = array((string)$row['COLUMN_NAME'], $referenced, (string)$row['REFERENCED_COLUMN_NAME']);
    }
    ksort($foreign_keys);
    return $foreign_keys;
}

/** Return the physical immutable trigger name for one table and operation. */
function pay_ctry_003_trigger_name($table, $operation)
{
    $codes = pay_ctry_003_trigger_codes();
    $prefix = hrm_migration_active_table_prefix();
    return isset($codes[$table]) && $prefix !== false ? $prefix.'ct3_'.$codes[$table].'_'.(strtoupper($operation) === 'UPDATE' ? 'u' : 'd') : false;
}

/** Return the exact immutable-trigger error text. */
function pay_ctry_003_trigger_message($table, $operation)
{
    return 'PAY-CTRY-003 immutable '.$table.' row cannot be '.(strtoupper($operation) === 'UPDATE' ? 'updated' : 'deleted');
}

/** Extract one schema group from the canonical migration SQL. */
function pay_ctry_003_schema_group_block($group)
{
    global $path_to_root;
    $raw = @file_get_contents($path_to_root.'/sql/upgrade_pay_ctry_003_schema.sql');
    if (!is_string($raw) || $raw === '') return false;
    $start = '-- PAY-CTRY-003 GROUP '.$group;
    $end = '-- PAY-CTRY-003 END GROUP '.$group;
    $a = strpos($raw, $start); $b = strpos($raw, $end);
    if ($a === false || $b === false || $b <= $a) return false;
    return substr($raw, $a + strlen($start), $b - ($a + strlen($start)));
}

/** Return the exact state of one immutable update/delete trigger. */
function pay_ctry_003_trigger_state($table, $operation)
{
    $physical = hrm_migration_physical_table_name($table);
    $name = pay_ctry_003_trigger_name($table, $operation);
    if ($physical === false || $name === false) return false;
    $query = db_query('SELECT EVENT_MANIPULATION,EVENT_OBJECT_TABLE,ACTION_TIMING,ACTION_STATEMENT FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA=DATABASE() AND TRIGGER_NAME='.db_escape($name).' LIMIT 2');
    if (!$query) return false;
    if (db_num_rows($query) === 0) return 'absent';
    if (db_num_rows($query) !== 1) return 'conflict';
    $row = db_fetch_assoc($query);
    $actual = strtoupper(rtrim(preg_replace('/\s+/', ' ', trim((string)$row['ACTION_STATEMENT'])), ';'));
    $expected = strtoupper("SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='".pay_ctry_003_trigger_message($table, $operation)."'");
    return strtoupper((string)$row['EVENT_MANIPULATION']) === strtoupper($operation) && (string)$row['EVENT_OBJECT_TABLE'] === $physical && strtoupper((string)$row['ACTION_TIMING']) === 'BEFORE' && $actual === $expected ? 'valid' : 'conflict';
}

/** Create one missing immutable trigger and reject any conflicting trigger. */
function pay_ctry_003_create_trigger($patch, $table, $operation)
{
    $state = pay_ctry_003_trigger_state($table, $operation);
    if ($state === 'valid') return true;
    if ($state !== 'absent') return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'PAY-CTRY-003 immutable trigger conflict: '.$table.'.'.$operation);
    $physical = hrm_migration_physical_table_name($table);
    $name = pay_ctry_003_trigger_name($table, $operation);
    $sql = 'CREATE TRIGGER `'.$name.'` BEFORE '.strtoupper($operation).' ON `'.$physical.'` FOR EACH ROW SIGNAL SQLSTATE \'45000\' SET MESSAGE_TEXT='.db_escape(pay_ctry_003_trigger_message($table, $operation));
    if (!db_query($sql)) return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-CTRY-003 immutable trigger creation failed: '.$table.'.'.$operation);
    return pay_ctry_003_trigger_state($table, $operation) === 'valid';
}

/** Verify the exact table projection and immutable triggers for one table. */
function pay_ctry_003_require_table_shape($patch, $table)
{
    $columns = pay_ctry_003_schema_columns(); $primary = pay_ctry_003_primary_keys(); $indexes = pay_ctry_003_required_indexes(); $foreign_keys = pay_ctry_003_required_foreign_keys();
    if (!isset($columns[$table], $primary[$table], $indexes[$table], $foreign_keys[$table])) return false;
    if (pay_ctry_003_table_columns($table) !== $columns[$table] || pay_ctry_003_table_primary_key($table) !== $primary[$table] || strcasecmp((string)pay_ctry_003_table_engine($table), 'InnoDB') !== 0)
        return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-CTRY-003 table projection mismatch: '.$table);
    $actual_indexes = pay_ctry_003_table_indexes($table); $wanted_indexes = $indexes[$table]; ksort($actual_indexes); ksort($wanted_indexes);
    if ($actual_indexes !== $wanted_indexes) return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-CTRY-003 index projection mismatch: '.$table);
    $actual_foreign_keys = pay_ctry_003_table_foreign_keys($table); $wanted_foreign_keys = $foreign_keys[$table]; ksort($actual_foreign_keys); ksort($wanted_foreign_keys);
    if ($actual_foreign_keys !== $wanted_foreign_keys) return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-CTRY-003 foreign-key projection mismatch: '.$table);
    foreach (array('UPDATE', 'DELETE') as $operation) if (pay_ctry_003_trigger_state($table, $operation) !== 'valid') return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'PAY-CTRY-003 immutable trigger mismatch: '.$table.'.'.$operation);
    return true;
}

/** Install one dependency-ordered schema group from the canonical SQL block. */
function pay_ctry_003_install_schema_group($patch, $group)
{
    $groups = pay_ctry_003_schema_groups();
    if (!isset($groups[$group])) return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-CTRY-003 schema group unknown: '.$group);
    $block = pay_ctry_003_schema_group_block($group);
    if ($block === false) return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'PAY-CTRY-003 schema source missing: '.$group);
    $prefix = hrm_migration_active_table_prefix();
    if ($prefix === false) return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-CTRY-003 table prefix invalid.');
    $statements = preg_split('/;\s*(?:\r?\n|$)/', $block);
    foreach ($statements as $statement) {
        $statement = trim($statement);
        if ($statement === '' || strpos($statement, 'CREATE TRIGGER') === 0) continue;
        $statement = str_replace('`0_', '`'.$prefix, $statement);
        if (!db_query($statement)) return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-CTRY-003 schema statement failed for group '.$group.'.');
    }
    foreach ($groups[$group] as $table) {
        foreach (array('UPDATE', 'DELETE') as $operation) if (!pay_ctry_003_create_trigger($patch, $table, $operation)) return false;
        if (!pay_ctry_003_require_table_shape($patch, $table)) return false;
    }
    return true;
}

/** Verify all six schema groups and their immutable trigger custody. */
function pay_ctry_003_require_schema($patch)
{
    foreach (pay_ctry_003_schema_groups() as $group => $tables) foreach ($tables as $table) if (!pay_ctry_003_require_table_shape($patch, $table)) return false;
    return true;
}

/** Verify one dependency-ordered schema group without requiring later groups. */
function pay_ctry_003_require_schema_group($patch, $group)
{
    $groups = pay_ctry_003_schema_groups();
    if (!isset($groups[$group])) return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-CTRY-003 schema group unknown: '.$group);
    foreach ($groups[$group] as $table) if (!pay_ctry_003_require_table_shape($patch, $table)) return false;
    return true;
}
?>
