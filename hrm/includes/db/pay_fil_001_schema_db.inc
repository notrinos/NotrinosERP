<?php
/** PAY-FIL-001 exact zero-seed append-only filing obligation/source custody schema. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
function pay_fil_001_schema_tables(){return array('hrm_pay_fil_001_obligations','hrm_pay_fil_001_source_sets','hrm_pay_fil_001_source_links');}
function pay_fil_001_schema_columns(){return array(
'hrm_pay_fil_001_obligations'=>array('filing_obligation_id','company_id','legal_entity_id','jurisdiction_code','obligation_type','period_start','period_end','due_date','filing_frequency','country_pack_id','country_pack_version','schema_id','schema_version','connector_id','connector_version','obligation_sha256','prepared_by','prepared_at','created_at'),
'hrm_pay_fil_001_source_sets'=>array('filing_source_set_id','filing_obligation_id','source_set_sha256','run_count','result_count','line_count','balance_count','liability_count','currency_code','liability_total_minor','bound_by','bound_at','created_at'),
'hrm_pay_fil_001_source_links'=>array('filing_source_link_id','filing_source_set_id','link_no','source_kind','source_key','source_sha256','predecessor_source_sha256','run_kind','currency_code','amount_minor','source_link_sha256','created_at'));}
/** Return the ordered metadata contract for one PAY-FIL-001 index. */
function pay_fil_001_schema_index_columns($columns)
{
    $spec = array();
    foreach ($columns as $column) {
        $spec[] = array(
            'sequence' => count($spec) + 1,
            'column_name' => (string)$column,
            'collation' => 'A',
            'sub_part' => null,
            'nullable' => ''
        );
    }
    return $spec;
}

/** Return the exact name, uniqueness, order and type contract for PAY-FIL indexes. */
function pay_fil_001_schema_index_specs()
{
    return array(
        'hrm_pay_fil_001_obligations' => array(
            'PRIMARY' => array('non_unique' => 0, 'index_type' => 'BTREE', 'columns' => pay_fil_001_schema_index_columns(array('filing_obligation_id'))),
            'uq_pfl1_ob_sha' => array('non_unique' => 0, 'index_type' => 'BTREE', 'columns' => pay_fil_001_schema_index_columns(array('obligation_sha256'))),
            'uq_pfl1_ob_sem' => array('non_unique' => 0, 'index_type' => 'BTREE', 'columns' => pay_fil_001_schema_index_columns(array('company_id','legal_entity_id','jurisdiction_code','obligation_type','period_end','country_pack_version','schema_version'))),
            'ix_pfl1_ob_due' => array('non_unique' => 1, 'index_type' => 'BTREE', 'columns' => pay_fil_001_schema_index_columns(array('company_id','legal_entity_id','due_date')))
        ),
        'hrm_pay_fil_001_source_sets' => array(
            'PRIMARY' => array('non_unique' => 0, 'index_type' => 'BTREE', 'columns' => pay_fil_001_schema_index_columns(array('filing_source_set_id'))),
            'uq_pfl1_ss_sha' => array('non_unique' => 0, 'index_type' => 'BTREE', 'columns' => pay_fil_001_schema_index_columns(array('source_set_sha256'))),
            'uq_pfl1_ss_ob' => array('non_unique' => 0, 'index_type' => 'BTREE', 'columns' => pay_fil_001_schema_index_columns(array('filing_obligation_id','source_set_sha256'))),
            'fk_pfl1_ss_ob' => array('non_unique' => 1, 'index_type' => 'BTREE', 'columns' => pay_fil_001_schema_index_columns(array('filing_obligation_id')))
        ),
        'hrm_pay_fil_001_source_links' => array(
            'PRIMARY' => array('non_unique' => 0, 'index_type' => 'BTREE', 'columns' => pay_fil_001_schema_index_columns(array('filing_source_link_id'))),
            'uq_pfl1_sl_no' => array('non_unique' => 0, 'index_type' => 'BTREE', 'columns' => pay_fil_001_schema_index_columns(array('filing_source_set_id','link_no'))),
            'uq_pfl1_sl_sha' => array('non_unique' => 0, 'index_type' => 'BTREE', 'columns' => pay_fil_001_schema_index_columns(array('source_link_sha256'))),
            'ix_pfl1_sl_src' => array('non_unique' => 1, 'index_type' => 'BTREE', 'columns' => pay_fil_001_schema_index_columns(array('source_kind','source_sha256'))),
            'fk_pfl1_sl_set' => array('non_unique' => 1, 'index_type' => 'BTREE', 'columns' => pay_fil_001_schema_index_columns(array('filing_source_set_id')))
        )
    );
}

/** Return the PAY-FIL-001 expected index names for each table. */
function pay_fil_001_schema_indexes()
{
    $specs = pay_fil_001_schema_index_specs();
    $indexes = array();
    foreach ($specs as $table => $table_specs)
        $indexes[$table] = array_keys($table_specs);
    return $indexes;
}

/** Read the complete ordered index projection from INFORMATION_SCHEMA. */
function pay_fil_001_table_index_projection($table)
{
    $physical = hrm_migration_physical_table_name($table);
    if ($physical === false)
        return false;
    $result = db_query("SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, SUB_PART, NULLABLE, INDEX_TYPE "
        ."FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME=".db_escape($physical)
        .' ORDER BY INDEX_NAME, SEQ_IN_INDEX');
    if (!$result)
        return false;
    $projection = array();
    while ($row = db_fetch_assoc($result)) {
        $name = (string)$row['INDEX_NAME'];
        if (!isset($projection[$name])) {
            $projection[$name] = array(
                'non_unique' => (int)$row['NON_UNIQUE'],
                'index_type' => strtoupper((string)$row['INDEX_TYPE']),
                'columns' => array()
            );
        }
        $projection[$name]['columns'][] = array(
            'sequence' => (int)$row['SEQ_IN_INDEX'],
            'column_name' => is_null($row['COLUMN_NAME']) ? null : (string)$row['COLUMN_NAME'],
            'collation' => is_null($row['COLLATION']) ? null : strtoupper((string)$row['COLLATION']),
            'sub_part' => is_null($row['SUB_PART']) ? null : (int)$row['SUB_PART'],
            'nullable' => strtoupper((string)$row['NULLABLE'])
        );
    }
    ksort($projection, SORT_STRING);
    return $projection;
}

/** Return the exact PAY-FIL-001 foreign-key definitions by table. */
function pay_fil_001_schema_foreign_keys()
{
    return array(
        'hrm_pay_fil_001_obligations' => array(),
        'hrm_pay_fil_001_source_sets' => array(
            'fk_pfl1_ss_ob' => array(
                'referenced_table' => 'hrm_pay_fil_001_obligations', 'update_rule' => 'RESTRICT', 'delete_rule' => 'RESTRICT',
                'columns' => array(array('sequence' => 1, 'column_name' => 'filing_obligation_id', 'referenced_column_name' => 'filing_obligation_id'))
            )
        ),
        'hrm_pay_fil_001_source_links' => array(
            'fk_pfl1_sl_set' => array(
                'referenced_table' => 'hrm_pay_fil_001_source_sets', 'update_rule' => 'RESTRICT', 'delete_rule' => 'RESTRICT',
                'columns' => array(array('sequence' => 1, 'column_name' => 'filing_source_set_id', 'referenced_column_name' => 'filing_source_set_id'))
            )
        )
    );
}

/** Read foreign-key metadata separately; MariaDB 10.5 can full-scan on an information-schema join. */
function pay_fil_001_table_foreign_key_projection($table)
{
    $physical = hrm_migration_physical_table_name($table);
    if ($physical === false)
        return false;
    $key_result = db_query("SELECT CONSTRAINT_NAME, ORDINAL_POSITION, COLUMN_NAME, REFERENCED_TABLE_NAME, "
        ."REFERENCED_COLUMN_NAME FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
        ."WHERE CONSTRAINT_SCHEMA=DATABASE() AND TABLE_NAME=".db_escape($physical)
        .' AND REFERENCED_TABLE_NAME IS NOT NULL ORDER BY CONSTRAINT_NAME, ORDINAL_POSITION');
    if (!$key_result)
        return false;
    $projection = array();
    while ($row = db_fetch_assoc($key_result)) {
        $name = (string)$row['CONSTRAINT_NAME'];
        if (!isset($projection[$name])) {
            $projection[$name] = array(
                'referenced_table' => (string)$row['REFERENCED_TABLE_NAME'],
                'update_rule' => '',
                'delete_rule' => '',
                'columns' => array()
            );
        } elseif ($projection[$name]['referenced_table'] !== (string)$row['REFERENCED_TABLE_NAME']) {
            return false;
        }
        $projection[$name]['columns'][] = array(
            'sequence' => (int)$row['ORDINAL_POSITION'],
            'column_name' => (string)$row['COLUMN_NAME'],
            'referenced_column_name' => (string)$row['REFERENCED_COLUMN_NAME']
        );
    }

    $rule_result = db_query("SELECT CONSTRAINT_NAME, UPDATE_RULE, DELETE_RULE "
        ."FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS "
        ."WHERE CONSTRAINT_SCHEMA=DATABASE() AND TABLE_NAME=".db_escape($physical)
        .' ORDER BY CONSTRAINT_NAME');
    if (!$rule_result)
        return false;
    $rules = array();
    while ($row = db_fetch_assoc($rule_result)) {
        $name = (string)$row['CONSTRAINT_NAME'];
        if (isset($rules[$name]))
            return false;
        $rules[$name] = array(
            'update_rule' => strtoupper((string)$row['UPDATE_RULE']),
            'delete_rule' => strtoupper((string)$row['DELETE_RULE'])
        );
    }

    if (count($rules) !== count($projection))
        return false;
    foreach ($projection as $name => $foreign_key) {
        if (!isset($rules[$name]))
            return false;
        $projection[$name]['update_rule'] = $rules[$name]['update_rule'];
        $projection[$name]['delete_rule'] = $rules[$name]['delete_rule'];
    }
    ksort($projection, SORT_STRING);
    return $projection;
}
function pay_fil_001_trigger_code($t){$m=array('hrm_pay_fil_001_obligations'=>'ob','hrm_pay_fil_001_source_sets'=>'ss','hrm_pay_fil_001_source_links'=>'sl');return isset($m[$t])?$m[$t]:false;}
function pay_fil_001_table_columns($t){if(!hrm_migration_table_exists($t))return false;$q=db_query('SHOW COLUMNS FROM '.TB_PREF.$t);if(!$q)return false;$a=array();while($r=db_fetch_assoc($q))$a[]=(string)$r['Field'];return $a;}
function pay_fil_001_table_innodb($t){$p=hrm_migration_physical_table_name($t);if($p===false)return false;$q=db_query('SELECT ENGINE FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($p));$r=$q?db_fetch_assoc($q):false;return $r&&strcasecmp((string)$r['ENGINE'],'InnoDB')===0;}
function pay_fil_001_trigger_name($t,$op){$p=hrm_migration_active_table_prefix();$c=pay_fil_001_trigger_code($t);if($p===false||$c===false)return false;return $p.'pfl1_'.$c.'_'.(strtoupper($op)==='UPDATE'?'u':'d');}
function pay_fil_001_trigger_valid($t,$op){$name=pay_fil_001_trigger_name($t,$op);$phys=hrm_migration_physical_table_name($t);if($name===false||$phys===false)return false;$q=db_query('SELECT EVENT_MANIPULATION,EVENT_OBJECT_TABLE,ACTION_TIMING,ACTION_STATEMENT FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA=DATABASE() AND TRIGGER_NAME='.db_escape($name).' LIMIT 2');if(!$q||db_num_rows($q)!==1)return false;$r=db_fetch_assoc($q);return strtoupper((string)$r['EVENT_MANIPULATION'])===strtoupper($op)&&(string)$r['EVENT_OBJECT_TABLE']===$phys&&strtoupper((string)$r['ACTION_TIMING'])==='BEFORE'&&strpos((string)$r['ACTION_STATEMENT'],'PAY-FIL-001 immutable')!==false;}
function pay_fil_001_require_schema($patch)
{
    $columns = pay_fil_001_schema_columns();
    $indexes = pay_fil_001_schema_index_specs();
    $foreign_keys = pay_fil_001_schema_foreign_keys();
    $prefix = hrm_migration_active_table_prefix();
    if ($prefix === false)
        return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'PAY-FIL-001 table prefix unavailable.');
    foreach (pay_fil_001_schema_tables() as $table) {
        if (pay_fil_001_table_columns($table) !== $columns[$table] || !pay_fil_001_table_innodb($table))
            return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-FIL-001 table projection mismatch: '.$table);
        $actual_indexes = pay_fil_001_table_index_projection($table);
        $expected_indexes = $indexes[$table];
        ksort($expected_indexes, SORT_STRING);
        if (!is_array($actual_indexes) || $actual_indexes !== $expected_indexes)
            return hrm_migration_fail($patch, HRM_MIGRATION_INDEX_MISMATCH, 'PAY-FIL-001 exact index projection mismatch: '.$table);
        $actual_foreign_keys = pay_fil_001_table_foreign_key_projection($table);
        $expected_foreign_keys = $foreign_keys[$table];
        foreach ($expected_foreign_keys as $name => $spec)
            $expected_foreign_keys[$name]['referenced_table'] = $prefix.$spec['referenced_table'];
        if (!is_array($actual_foreign_keys) || $actual_foreign_keys !== $expected_foreign_keys)
            return hrm_migration_fail($patch, HRM_MIGRATION_FK_MISMATCH, 'PAY-FIL-001 exact foreign-key projection mismatch: '.$table);
        foreach (array('UPDATE','DELETE') as $operation) {
            if (!pay_fil_001_trigger_valid($table, $operation))
                return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'PAY-FIL-001 immutable trigger mismatch: '.$table.'.'.$operation);
        }
    }
    return true;
}

/** Add the missing named source-set FK index without replacing or dropping evidence. */
function pay_fil_001_ensure_source_set_fk_index($patch)
{
    $indexes = hrm_migration_index_names('hrm_pay_fil_001_source_sets');
    if (!is_array($indexes))
        return hrm_migration_fail($patch, HRM_MIGRATION_INDEX_MISMATCH, 'PAY-FIL-001 source-set index metadata unavailable.');
    if (isset($indexes['fk_pfl1_ss_ob']))
        return true;
    $table = hrm_migration_physical_table_name('hrm_pay_fil_001_source_sets');
    if ($table === false)
        return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'PAY-FIL-001 source-set table name unavailable.');
    if (!db_query('ALTER TABLE `'.$table.'` ADD KEY `fk_pfl1_ss_ob` (`filing_obligation_id`)')) {
        $indexes = hrm_migration_index_names('hrm_pay_fil_001_source_sets');
        if (!is_array($indexes) || !isset($indexes['fk_pfl1_ss_ob']))
            return hrm_migration_fail($patch, HRM_MIGRATION_INDEX_MISMATCH, 'PAY-FIL-001 source-set FK index repair failed.');
    }
    return true;
}

function pay_fil_001_install_schema($patch)
{
    global $path_to_root;
    $raw = @file_get_contents($path_to_root.'/sql/upgrade_pay_fil_001_custody_schema.sql');
    if (!is_string($raw) || strpos($raw, '-- PAY-FIL-001 GROUP obligation_source_custody') === false)
        return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, 'PAY-FIL-001 schema source missing.');
    $prefix = hrm_migration_active_table_prefix();
    if ($prefix === false)
        return false;
    $raw = preg_replace('/^--.*$/m', '', $raw);
    $statements = preg_split('/;\s*(?:\r?\n|$)/', $raw);
    foreach ($statements as $statement) {
        $statement = trim($statement);
        if ($statement === '' || strpos($statement, 'CREATE TRIGGER') === 0)
            continue;
        $statement = str_replace('`0_', '`'.$prefix, $statement);
        if (!db_query($statement))
            return hrm_migration_fail($patch, HRM_MIGRATION_PROJECTION_MISMATCH, 'PAY-FIL-001 schema statement failed.');
    }
    if (!pay_fil_001_ensure_source_set_fk_index($patch))
        return false;
    foreach (pay_fil_001_schema_tables() as $table) {
        foreach (array('UPDATE','DELETE') as $operation) {
            if (pay_fil_001_trigger_valid($table, $operation))
                continue;
            $name = pay_fil_001_trigger_name($table, $operation);
            $physical = hrm_migration_physical_table_name($table);
            $message = 'PAY-FIL-001 immutable '.$table.' row cannot be '.strtolower($operation).'d';
            $sql = 'CREATE TRIGGER `'.$name.'` BEFORE '.$operation.' ON `'.$physical.'` FOR EACH ROW SIGNAL SQLSTATE \'45000\' SET MESSAGE_TEXT='.db_escape($message);
            if (!db_query($sql))
                return false;
        }
    }
    return pay_fil_001_require_schema($patch);
}
function pay_fil_001_require_zero_rows($patch){foreach(pay_fil_001_schema_tables() as $t){$q=db_query('SELECT COUNT(*) n FROM '.TB_PREF.$t);$r=$q?db_fetch_assoc($q):false;if(!$r||(int)$r['n']!==0)return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-FIL-001 foundation must activate zero-seed: '.$t);}return true;}
?>
