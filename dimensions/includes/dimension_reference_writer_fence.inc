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

/** Validate an exact native stock identity without numeric coercion or truncation. */
function dimension_writer_stock_id($value)
{
    return is_string($value) && trim($value) === $value
        && preg_match('/^[^\x00-\x1f\x7f]{1,20}$/uD', $value) ? $value : false;
}

/** Normalize chart identities exactly as native db_escape, before locking varchar(15). */
function dimension_writer_chart_id($value)
{
    if (is_int($value) && $value >= 0) $value = (string)$value;
    if (!is_string($value) || trim($value) !== $value
        || !preg_match('/^[^\x00-\x1f\x7f]+$/uD', $value)) return false;
    $value = html_specials_encode(html_entity_decode($value, ENT_QUOTES,
        $_SESSION['language']->encoding === 'iso-8859-2' ? 'ISO-8859-1' : $_SESSION['language']->encoding));
    return preg_match('/^[^\x00-\x1f\x7f]{1,15}$/uD', $value) ? $value : false;
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
    $sources = array('suppliers'=>'supplier_id', 'debtors_master'=>'debtor_no', 'quick_entry_lines'=>'id', 'stock_category'=>'category_id', 'stock_master'=>'stock_id', 'chart_master'=>'account_code', 'bank_accounts'=>'id', 'budget_trans'=>'id', 'purch_rfq'=>'id');
    if (!isset($sources[$owner]) || !is_callable($callback) || !is_array($next)) return false;
    $columns = $owner === 'stock_category' ? array('dflt_dim1'=>1, 'dflt_dim2'=>2) : array('dimension_id'=>1, 'dimension2_id'=>2);
    // Chart and bank accounts retain JSON but have no scalar Dimension slots.
    if ($owner === 'chart_master' || $owner === 'bank_accounts') $columns = array();
    $string_key = $owner === 'stock_master' || $owner === 'chart_master';
    $names = array_keys($columns);
    if (!$partial) foreach ($names as $name) if (!array_key_exists($name, $next)) return false;
    $new = array();
    foreach ($names as $column) {
        if (!array_key_exists($column, $next)) continue;
        $id = dimension_writer_reference_id($next[$column]);
        if ($id === false) return false;
        $new[$column] = $id;
        $next[$column] = $id;
    }
    $identity = null;
    if ($key !== null) {
        $identity = $owner === 'stock_master' ? dimension_writer_stock_id($key) : dimension_writer_reference_id($key);
        if ($owner === 'chart_master') $identity = dimension_writer_chart_id($key);
        if ($identity === false || $identity === 0) return false;
    }
    try {
        if (!function_exists('begin_transaction') || !begin_transaction()) return false;
        // An omitted payload must not certify a bank owner whose retained column
        // is unavailable. The transactional read also holds its metadata lock.
        if ($identity === null && in_array($owner, array('bank_accounts', 'budget_trans', 'purch_rfq'), true)
            && !db_query('SELECT custom_data FROM '.TB_PREF.$owner.' LIMIT 0')) {
            cancel_transaction(); return false;
        }
        $old = array_fill_keys($names, 0);
        $old_payload = null;
        if ($identity !== null) {
            // Quick-entry lines have no retained payload column in either
            // canonical schema. Never infer absence for another owner.
            $payload_column = $owner === 'quick_entry_lines' ? 'NULL AS custom_data' : 'custom_data';
            if ($string_key) $payload_column .= ', '.$sources[$owner].' AS owner_identity';
            $rows = db_query('SELECT '.(count($names) ? implode(', ', $names).', ' : '').$payload_column.' FROM '.TB_PREF.$owner
                .' WHERE '.$sources[$owner].'='.($string_key ? db_escape($identity) : $identity).' FOR UPDATE');
            $found = $rows ? db_fetch($rows) : false;
            if (!is_array($found)) { cancel_transaction(); return false; }
            // A case-insensitive SQL key must not rename a historical string ID.
            if ($string_key && (!isset($found['owner_identity'])
                || $found['owner_identity'] !== $identity)) { cancel_transaction(); return false; }
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
        foreach ($columns as $column=>$axis) {
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
        if ($identity === null && !$string_key && !db_preserve_insert_id($insert_id)) return false;
        return $result;
    } catch (Exception $exception) {
        cancel_transaction();
        return false;
    } catch (Throwable $exception) {
        cancel_transaction();
        return false;
    }
}
