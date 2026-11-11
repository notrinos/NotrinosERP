<?php
/**
 * DIM-LFC-001 B05: payroll-owned minimized native Dimension-reference authority.
 *
 * Payroll evidence that can hold, or could be mistaken for holding, a native
 * Dimension coordinate:
 *  - hrm_pay_gl_001_gl_posting_lines: the immutable projection of every PAY-GL
 *    post/reverse/repost allocation onto core_dimension_id/core_dimension2_id.
 *  - hrm_pay_gl_001_allocation_axes: immutable governed allocation axes. An axis
 *    is a native coordinate only when the line's own mapping rule binds its
 *    axis code to Dimension slot one or two (or the row carries a slot hint);
 *    any other axis is an independent payroll axis whose value key is NOT a core
 *    Dimension identity, so the two are never collapsed. A matching value whose
 *    allocation, line or rule cannot be resolved is unknown.
 *  - payslips.custom_data: ordinary retained JSON, read through the same
 *    canonical projection the ordinary writers use.
 *  - payroll_reversal_evidence.before_state and the PAY-GLB native receipts:
 *    fixed producer shapes with no coordinate (loan/leave/GL-hash projections,
 *    hashes and identities). A coordinate-like key anywhere is unknown.
 *  - ST_PAYROLL_PERIOD approval drafts/snapshots (decoded inside the approval
 *    observer through pay_dimension_approval_payload_status()).
 *
 * Every read is a locking current read inside the caller's transaction, so no
 * writer can add a reference after an "unused" answer. Only owner names and
 * used/unused/unknown leave these functions: no salary, person, axis, account
 * or receipt content. Missing tables, drifted columns, invalid JSON or SQL
 * failure are unknown, never unused. This is NOT a deletion certificate.
 */
if (!function_exists('dimension_master_payload_references'))
    require_once dirname(dirname(dirname(__DIR__))).'/dimensions/includes/dimension_master_payload_guard.inc';
if (!function_exists('hrm_dimension_retained_coordinate_key'))
    require_once __DIR__.'/hrm_dimension_retained_evidence_db.inc';

/**
 * The PAY-GLB native receipt tables and their JSON columns.
 *
 * @return array table => list of JSON columns
 */
function pay_dimension_native_receipt_tables()
{
    return array(
        'hrm_pay_glb_001_native_approval_bindings'=>array('receipt_json'),
        'hrm_pay_glb_001_native_command_receipts'=>array('request_context_json', 'receipt_json'),
        'hrm_pay_glb_001_native_executions'=>array('receipt_json'),
        'hrm_pay_glb_001_native_gl_bindings'=>array('receipt_json'),
        'hrm_pay_glb_001_native_members'=>array('receipt_json'),
        'hrm_pay_glb_001_native_selection_events'=>array('receipt_json'));
}

/**
 * Every live table this authority reads, for the recheck's single column query.
 *
 * @return string[] Unprefixed table names
 */
function pay_dimension_reference_tables()
{
    return array_merge(array('hrm_pay_gl_001_gl_posting_lines', 'hrm_pay_gl_001_allocation_axes', 'hrm_pay_gl_001_allocations',
        'hrm_pay_gl_001_journal_lines', 'hrm_pay_gl_001_mapping_rules', 'payslips', 'payroll_reversal_evidence'),
        array_keys(pay_dimension_native_receipt_tables()));
}

/**
 * The owner names this authority reports, in report order.
 *
 * @return string[]
 */
function pay_dimension_reference_owner_names()
{
    return array('hrm_pay_gl_001_gl_posting_lines', 'hrm_pay_gl_001_allocation_axes', 'payslips',
        'payroll_reversal_evidence', 'pay_glb_001_native_receipts');
}

/**
 * The payroll approval families this authority decodes.
 *
 * @return int[] Transaction type identities
 */
function pay_dimension_approval_family_types()
{
    return defined('ST_PAYROLL_PERIOD') ? array((int)ST_PAYROLL_PERIOD) : array();
}

/**
 * Is this key a coordinate name in a payroll payload? The HRM vocabulary plus
 * the PAY-GL allocation-axis fields, which only the PAY-GL tables may hold.
 *
 * @param mixed $key Array key
 * @return bool
 */
function pay_dimension_coordinate_key($key)
{
    return hrm_dimension_retained_coordinate_key($key)
        || (is_string($key) && in_array($key, array('axis_value_key', 'core_slot_hint', 'dimension1_axis_code', 'dimension2_axis_code'), true));
}

/**
 * Classify one decoded payroll payload whose producers store no coordinate.
 * Without any coordinate-like key it is unused; a coordinate-like key, an
 * object or anything deeper than 64 levels is unknown.
 *
 * @param mixed $payload Decoded payload
 * @return string unused or unknown
 */
function pay_dimension_fixed_payload_status($payload)
{
    if (!is_array($payload)) return 'unknown';
    $stack = array(array($payload, 0));
    while ($stack) {
        list($node, $depth) = array_pop($stack);
        if ($depth > 64) return 'unknown';
        foreach ($node as $key => $value) {
            if (pay_dimension_coordinate_key($key)) return 'unknown';
            if (is_array($value)) $stack[] = array($value, $depth + 1);
            elseif (is_object($value)) return 'unknown';
        }
    }
    return 'unused';
}

/**
 * Classify one decoded ST_PAYROLL_PERIOD draft or action snapshot. Neither the
 * legacy period producer nor the PAY-GLB native producer stores a coordinate.
 *
 * @param mixed $payload Decoded draft payload
 * @return string unused or unknown
 */
function pay_dimension_approval_payload_status($payload)
{
    return pay_dimension_fixed_payload_status($payload);
}

/**
 * Classify one JSON text column value of a fixed-shape payroll store. NULL
 * holds nothing; text that is not a JSON array/object is unknown.
 *
 * @param string|null $text Stored JSON text
 * @return string unused or unknown
 */
function pay_dimension_fixed_json_status($text)
{
    if ($text === null) return 'unused';
    $decoded = json_decode((string)$text, true);
    if (!is_array($decoded)) return 'unknown';
    return pay_dimension_fixed_payload_status($decoded);
}

/**
 * The live schema of one payroll table matches its registration: all needed
 * columns exist and no unregistered Dimension-like column appeared.
 *
 * @param array $columns column => true
 * @param array $needed  required columns
 * @param array $allowed Dimension-like columns that are registered
 * @return bool
 */
function pay_dimension_schema_matches($columns, $needed, $allowed = array())
{
    if (!is_array($columns) || !$columns) return false;
    foreach ($needed as $column) if (!isset($columns[$column])) return false;
    foreach (array_keys($columns) as $column)
        if ((hrm_dimension_retained_coordinate_key($column) || preg_match('/(^|_)dim(ension)?[0-9]*_?/', $column))
            && !in_array($column, $allowed, true)) return false;
    return true;
}

/**
 * Classify the governed allocation axes for one Dimension with a locking read.
 * Value keys compare as exact binary text, so "05" or " 5" never match 5 (the
 * native projection would refuse them anyway).
 *
 * @param array $columns table => column => true
 * @param int   $id      Exact positive Dimension identity
 * @return string used, unused or unknown
 */
function pay_dimension_allocation_axes_status($columns, $id)
{
    $needed = array(
        'hrm_pay_gl_001_allocation_axes'=>array('allocation_id', 'axis_code', 'axis_value_key', 'core_slot_hint'),
        'hrm_pay_gl_001_allocations'=>array('allocation_id', 'journal_line_id'),
        'hrm_pay_gl_001_journal_lines'=>array('journal_line_id', 'mapping_rule_sha256'),
        'hrm_pay_gl_001_mapping_rules'=>array('rule_sha256', 'dimension1_axis_code', 'dimension2_axis_code'));
    $allowed = array('dimension1_axis_code', 'dimension2_axis_code');
    foreach ($needed as $table => $fields)
        if (!pay_dimension_schema_matches(isset($columns[$table]) ? $columns[$table] : array(), $fields, $allowed)) return 'unknown';
    $rows = db_query('SELECT ax.core_slot_hint, ax.axis_code, a.allocation_id AS allocation, l.journal_line_id AS line,'
        .' r.rule_sha256 AS rule_hash, r.dimension1_axis_code, r.dimension2_axis_code'
        .' FROM '.TB_PREF.'hrm_pay_gl_001_allocation_axes ax'
        .' LEFT JOIN '.TB_PREF.'hrm_pay_gl_001_allocations a ON a.allocation_id=ax.allocation_id'
        .' LEFT JOIN '.TB_PREF.'hrm_pay_gl_001_journal_lines l ON l.journal_line_id=a.journal_line_id'
        .' LEFT JOIN '.TB_PREF.'hrm_pay_gl_001_mapping_rules r ON r.rule_sha256=l.mapping_rule_sha256'
        ." WHERE BINARY ax.axis_value_key=BINARY '".$id."' ORDER BY ax.allocation_axis_id LOCK IN SHARE MODE");
    if (!$rows) return 'unknown';
    $state = 'unused';
    while ($row = db_fetch_assoc($rows)) {
        $hint = $row['core_slot_hint'];
        if ($hint !== null && !in_array((string)$hint, array('1', '2'), true)) { $state = 'unknown'; continue; }
        if ($hint !== null) return 'used'; // A native slot hint blocks even with missing parents.
        if ($row['allocation'] === null || $row['line'] === null || $row['rule_hash'] === null) { $state = 'unknown'; continue; }
        if ($row['axis_code'] === $row['dimension1_axis_code'] || $row['axis_code'] === $row['dimension2_axis_code']) return 'used';
        // Otherwise an independent payroll axis whose key is not a core Dimension identity.
    }
    return $state;
}

/**
 * Classify the fixed-shape JSON columns of one payroll table with a locking read.
 *
 * @param string $table   Unprefixed table
 * @param array  $fields  JSON columns
 * @param array  $columns column => true (live)
 * @return string unused or unknown
 */
function pay_dimension_fixed_store_status($table, $fields, $columns)
{
    if (!pay_dimension_schema_matches($columns, $fields)) return 'unknown';
    $rows = db_query('SELECT '.implode(', ', $fields).' FROM '.TB_PREF.$table.' LOCK IN SHARE MODE');
    if (!$rows) return 'unknown';
    $state = 'unused';
    while ($row = db_fetch_row($rows))
        foreach ($row as $text) if (pay_dimension_fixed_json_status($text) !== 'unused') $state = 'unknown';
    return $state;
}

/**
 * Locked current reads of every payroll owner for one Dimension. Requires the
 * caller's open transaction (repeatable read); the shared row and gap locks
 * last until it ends. A SQL failure stops the scan and leaves the rest unknown.
 *
 * @param int|string $id      Exact positive Dimension identity
 * @param array      $columns table => column => true, for pay_dimension_reference_tables()
 * @return array owner => used|unused|unknown
 */
function pay_dimension_reference_observe($id, $columns)
{
    $result = array_fill_keys(pay_dimension_reference_owner_names(), 'unknown');
    if (empty($GLOBALS['transaction_level']) || !is_array($columns) || !function_exists('dimension_reference_recheck_owner')
        || !(is_int($id) || (is_string($id) && preg_match('/^[1-9][0-9]*$/D', $id))) || (int)$id <= 0) return $result;
    $id = (int)$id;
    $live = function($table) use ($columns) { return isset($columns[$table]) ? $columns[$table] : array(); };
    try {
        $state = dimension_reference_recheck_owner('hrm_pay_gl_001_gl_posting_lines', $live('hrm_pay_gl_001_gl_posting_lines'),
            array('core_dimension_id', 'core_dimension2_id'), false, $id);
        $result['hrm_pay_gl_001_gl_posting_lines'] = $live('hrm_pay_gl_001_gl_posting_lines') ? $state : 'unknown';
        if (db_error_no()) return $result;
        $result['hrm_pay_gl_001_allocation_axes'] = pay_dimension_allocation_axes_status($columns, $id);
        if (db_error_no()) return $result;
        $result['payslips'] = $live('payslips') ? dimension_reference_recheck_owner('payslips', $live('payslips'), array(), true, $id) : 'unknown';
        if (db_error_no()) return $result;
        $result['payroll_reversal_evidence'] = pay_dimension_fixed_store_status('payroll_reversal_evidence', array('before_state'),
            $live('payroll_reversal_evidence'));
        if (db_error_no()) return $result;
        $receipts = 'unused';
        foreach (pay_dimension_native_receipt_tables() as $table => $fields) {
            $state = pay_dimension_fixed_store_status($table, $fields, $live($table));
            if ($state !== 'unused') $receipts = 'unknown';
            if (db_error_no()) return $result;
        }
        $result['pay_glb_001_native_receipts'] = $receipts;
    } catch (Exception $exception) {
        return array_fill_keys(pay_dimension_reference_owner_names(), 'unknown');
    }
    return $result;
}

/**
 * Lock and admit the native coordinates a new PAY-GL journal intent will
 * retain. Only axes that the line's mapping rule binds to slot one or two and
 * whose value key is a canonical positive integer project onto a Dimension
 * (pay_gl_001_project_native_dimensions); every such reference must be an open
 * master of that axis. Masters are locked in ascending order inside the
 * caller's transaction, so a concurrent close or delete-side recheck
 * serializes with the intent. Non-numeric slotted values project to nothing
 * and keep failing at posting exactly as before.
 *
 * @param array $axes_by_allocation allocation => array('axes'=>list of axis rows, 'rule'=>mapping rule)
 * @return bool True when every projected master is locked and admitted
 */
function pay_gl_001_dimension_writer_admit($axes_by_allocation)
{
    if (!is_array($axes_by_allocation) || empty($GLOBALS['transaction_level'])) return false;
    if (!function_exists('hrm_dimension_writer_admit')) require_once __DIR__.'/hrm_dimension_reference_writer_db.inc';
    $coordinates = array();
    foreach ($axes_by_allocation as $entry) {
        if (!is_array($entry) || !isset($entry['axes'], $entry['rule']) || !is_array($entry['axes']) || !is_array($entry['rule'])) return false;
        foreach ($entry['axes'] as $axis) {
            if (!is_array($axis) || !isset($axis['axis_code'], $axis['axis_value_key'])) return false;
            $slot = 0;
            if (isset($entry['rule']['dimension1_axis_code']) && $entry['rule']['dimension1_axis_code'] === $axis['axis_code']) $slot = 1;
            if (isset($entry['rule']['dimension2_axis_code']) && $entry['rule']['dimension2_axis_code'] === $axis['axis_code']) $slot = 2;
            if (!$slot || !preg_match('/^[1-9][0-9]*$/D', (string)$axis['axis_value_key'])) continue;
            $coordinates[] = array('axis'=>$slot, 'old'=>0, 'new'=>(string)$axis['axis_value_key']);
        }
    }
    return hrm_dimension_writer_admit($coordinates);
}

/**
 * Lock the Dimension masters an exact inverse GL copy will carry, without
 * validating them: a linked reversal must reproduce the original's historical
 * coordinates byte-for-byte, including closed or orphan ones. Call after the
 * original GL rows are locked (owner rows first, then masters).
 *
 * @param array $lines Original GL line projections with dimension_id/dimension2_id
 * @return bool True when every referenced master is locked
 */
function pay_dimension_inverse_copy_lock($lines)
{
    if (!is_array($lines) || empty($GLOBALS['transaction_level'])) return false;
    if (!function_exists('hrm_dimension_writer_admit')) require_once __DIR__.'/hrm_dimension_reference_writer_db.inc';
    $coordinates = array();
    foreach ($lines as $line) {
        if (!is_array($line) || !array_key_exists('dimension_id', $line) || !array_key_exists('dimension2_id', $line)) return false;
        foreach (array(1=>'dimension_id', 2=>'dimension2_id') as $axis => $field)
            $coordinates[] = array('axis'=>$axis, 'old'=>$line[$field], 'new'=>$line[$field], 'validate'=>false);
    }
    return hrm_dimension_writer_admit($coordinates);
}
