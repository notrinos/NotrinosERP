<?php
/** PAY-FIL-001 exact zero-seed append-only filing obligation/source custody schema. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
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

?>
