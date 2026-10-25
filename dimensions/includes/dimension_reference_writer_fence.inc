<?php
/**
 * DIM-LFC-001B / B02: bounded ordinary master-reference writer serialization.
 *
 * A writer must lock its existing owner row and each referenced Dimension master
 * in the SAME transaction before a create/change/remove. Master locks are taken
 * in ascending numeric order, including former references. New references must
 * be on their correct axis and open; unchanged legacy closed/orphan references
 * remain readable and maintainable, never silently remapped.
 *
 * This is NOT a deletion certificate. B02 remains open until every writer,
 * including direct Entity/import/legacy paths, is covered. B10 must reconcile
 * lock ordering with all other modules before permitting Dimension deletion.
 */
require_once __DIR__.'/dimension_master_payload_guard.inc';

/** Canonical native DB coordinate; only zero denotes a blank reference. */
function dimension_writer_reference_id($value)
{
    if ($value === null || $value === '') return 0;
    if (is_int($value)) return $value >= 0 ? $value : false;
    if (!is_string($value) || !preg_match('/^(?:0|[1-9][0-9]*)$/D', $value)) return false;
    $integer = (int)$value;
    return $integer >= 0 && (string)$integer === $value ? $integer : false;
}

/**
 * Fail closed on invalid owner/identity, lock or query failures. The caller's
 * original authorization checks are still required; this does not grant access.
 * The callback receives the normalized payload and executes while both owner
 * and master locks remain held. Its result retains the native Entity contract.
 * Entity callers may opt into partial data: omitted coordinates are validated
 * and locked from current owner state but are never added to the write payload.
 */
function dimension_ordinary_master_write($owner, $key, $next, $callback, $partial = false, $remove = false)
{
    $sources = array('suppliers'=>'supplier_id', 'debtors_master'=>'debtor_no', 'quick_entry_lines'=>'id');
    if (!isset($sources[$owner]) || !is_callable($callback) || !is_array($next)) return false;
    if (!$partial && (!array_key_exists('dimension_id', $next) || !array_key_exists('dimension2_id', $next))) return false;
    $new = array();
    foreach (array('dimension_id', 'dimension2_id') as $column) {
        if (!array_key_exists($column, $next)) continue;
        $id = dimension_writer_reference_id($next[$column]);
        if ($id === false) return false;
        $new[$column] = $id;
        $next[$column] = $id;
    }
    $identity = null;
    if ($key !== null) {
        $identity = dimension_writer_reference_id($key);
        if ($identity === false || $identity === 0) return false;
    }
    try {
        if (!function_exists('begin_transaction') || !begin_transaction()) return false;
        $old = array('dimension_id'=>0, 'dimension2_id'=>0);
        $old_payload = null;
        if ($identity !== null) {
            // Quick-entry lines have no retained payload column in either
            // canonical schema. Never infer absence for another owner.
            $payload_column = $owner === 'quick_entry_lines' ? 'NULL AS custom_data' : 'custom_data';
            $rows = db_query('SELECT dimension_id, dimension2_id, '.$payload_column.' FROM '.TB_PREF.$owner
                .' WHERE '.$sources[$owner].'='.$identity.' FOR UPDATE');
            $found = $rows ? db_fetch($rows) : false;
            if (!is_array($found)) { cancel_transaction(); return false; }
            if (!array_key_exists('custom_data', $found)) { cancel_transaction(); return false; }
            $old_payload = $found['custom_data'];
            foreach ($old as $column=>$value) {
                $ref = dimension_writer_reference_id($found[$column]);
                if ($ref === false) { cancel_transaction(); return false; }
                $old[$column] = $ref;
            }
        }
        $next_payload = $remove ? null : (array_key_exists('custom_data', $next) ? $next['custom_data'] : $old_payload);
        $payload = dimension_master_payload_transition($old_payload, $next_payload, $remove);
        if ($payload === false) { cancel_transaction(); return false; }
        // Entity partial updates retain omitted coordinates from the current
        // locked row, never from a caller snapshot or an unlocked prior read.
        foreach ($old as $column=>$id) {
            if (!array_key_exists($column, $new)) $new[$column] = $id;
        }
        // No reference is ever omitted simply because it is being cleared.
        $locks = $payload['locks'];
        foreach (array($old, $new) as $coords)
            foreach ($coords as $id) if ($id > 0) $locks[$id] = $id;
        sort($locks, SORT_NUMERIC);
        $masters = array();
        foreach ($locks as $id) {
            $rows = db_query('SELECT id, type_, closed FROM '.TB_PREF.'dimensions'
                .' WHERE id='.$id.' FOR UPDATE');
            if (!$rows) { cancel_transaction(); return false; }
            $masters[$id] = db_fetch($rows); // False is allowed only for an existing orphan.
        }
        foreach (array('dimension_id'=>1, 'dimension2_id'=>2) as $column=>$axis) {
            $id = $new[$column];
            if (!$id) continue;
            $unchanged = ($id === $old[$column]);
            $master = $masters[$id];
            if (!$unchanged && (!is_array($master) || (int)$master['type_'] !== $axis
                || (int)$master['closed'] !== 0)) { cancel_transaction(); return false; }
        }
        foreach ($payload['new'] as $reference) {
            $master = $masters[$reference[0]];
            if (!is_array($master) || (int)$master['type_'] !== $reference[1]
                || (int)$master['closed'] !== 0) { cancel_transaction(); return false; }
        }
        $result = call_user_func($callback, $next);
        if (!$result) { cancel_transaction(); return false; }
        $insert_id = $identity === null ? db_insert_id() : 0;
        if (!commit_transaction()) { cancel_transaction(); return false; }
        if ($identity === null && !db_preserve_insert_id($insert_id)) return false;
        return $result;
    } catch (Exception $exception) {
        cancel_transaction();
        return false;
    } catch (Throwable $exception) {
        cancel_transaction();
        return false;
    }
}
