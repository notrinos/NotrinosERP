<?php
/** Fixed source-owned schema preflight for advisory reference readers. */
require_once __DIR__.'/dimension_lifecycle_db.inc';
require_once __DIR__.'/dimension_reference_registry.inc';

/**
 * Return only the columns used by the fixed scalar, metadata, Sales and PAY-GL
 * observers. No caller may select a column or declare an owner available.
 * This inventory describes reads, never completeness or deletion authority.
 *
 * @return array Table => exact required column names
 */
function dimension_reference_schema_columns()
{
    $tables = array();
    foreach (dimension_reference_owners() as $owner)
        if ($owner['kind'] === 'scalar') $tables[$owner['table']] = $owner['columns'];
    return array_merge($tables, array(
        'comments'=>array('type','id'), 'attachments'=>array('type_no','trans_no'),
        'refs'=>array('type','id'), 'audit_trail'=>array('type','trans_no'),
        'tags'=>array('id','type'), 'tag_associations'=>array('tag_id','record_id'),
        'sales_orders'=>array('trans_type','custom_data'),
        'sales_order_details'=>array('trans_type','custom_data'),
        'hrm_pay_gl_001_allocation_axes'=>array('allocation_id','axis_value_key','core_slot_hint','axis_code'),
        'hrm_pay_gl_001_allocations'=>array('allocation_id','journal_line_id'),
        'hrm_pay_gl_001_journal_lines'=>array('journal_line_id','mapping_rule_sha256'),
        'hrm_pay_gl_001_mapping_rules'=>array('rule_sha256','dimension1_axis_code','dimension2_axis_code')));
}

/**
 * Check exact active-prefix table/columns before querying an optional owner.
 * Legacy query diagnostics may terminate a request, so a caught SELECT failure
 * is insufficient. Binary equality avoids case/collation/LIKE-prefix matches.
 * Recheck each invocation; no session/company/connection availability cache.
 * Concurrent DDL/connection loss can still fail through the existing driver;
 * this observation cannot certify unused state or serialize reference writers.
 *
 * @param string $table A fixed source-owned table name
 * @return bool Every required column is present in the active database
 */
function dimension_reference_schema_available($table)
{
    if (!dimension_lifecycle_authorized()) return false;
    $tables = dimension_reference_schema_columns();
    if (!is_string($table) || !isset($tables[$table])) return false;
    $columns = array();
    foreach ($tables[$table] as $column) $columns[] = db_escape($column);
    $rows = db_query('SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE()'
        ." AND BINARY TABLE_NAME=BINARY '".db_escape_function(TB_PREF.$table)."'"
        .' AND BINARY COLUMN_NAME IN ('.implode(',', $columns).')');
    if (!$rows) return false;
    $row = db_fetch_row($rows);
    return $row && (int)$row[0] === count($tables[$table]);
}
