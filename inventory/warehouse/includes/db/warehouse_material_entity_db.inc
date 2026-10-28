<?php
/** Canonical generic demand CRUD delegates to the existing native commands. */
require_once __DIR__.'/warehouse_material_requests_db.inc';
require_once __DIR__.'/warehouse_material_create_db.inc';
require_once __DIR__.'/warehouse_material_remove_db.inc';

/** Bind a creation validator to current native source, review and number custody. */
function material_entity_creation_state()
{
    $state = array();
    $queries = array(
        'SELECT * FROM '.TB_PREF.'approval_drafts WHERE trans_type='.ST_WH_MATERIAL_REQUEST.' ORDER BY id FOR UPDATE',
        'SELECT a.* FROM '.TB_PREF.'approval_actions a INNER JOIN '.TB_PREF.'approval_drafts d ON d.id=a.draft_id WHERE d.trans_type='.ST_WH_MATERIAL_REQUEST.' ORDER BY a.id FOR UPDATE',
        'SELECT * FROM '.TB_PREF.'wh_material_requests ORDER BY request_no FOR UPDATE',
        'SELECT * FROM '.TB_PREF.'voided WHERE type='.ST_WH_MATERIAL_REQUEST.' ORDER BY id FOR UPDATE',
        'SELECT * FROM '.TB_PREF.'wh_material_request_lines ORDER BY line_id FOR UPDATE');
    foreach ($queries as $query) {
        $rows = material_core_rows($query);
        if ($rows === false) return false;
        $state[] = $rows;
    }
    return $state;
}

/**
 * Merge partial fields under current core-history and standalone source locks.
 * Generic persistence cannot assign generated numbers, workflow, fulfillment,
 * parent/stock identity or opaque retained metadata. Paired requisitions retain
 * their native command authority. No approval or historical conversion is implied.
 */
function write_material_request_entity($owner, $operation, $key, $next, $validate)
{
    $context = material_core_context();
    if (!$context || !is_callable($validate) || !is_array($next)
        || !in_array($owner, array('wh_material_requests','wh_material_request_lines'), true)
        || !in_array($operation, array('insert','update','delete'), true)) return false;
    $header = $owner === 'wh_material_requests';
    $primary = $header ? 'request_id' : 'line_id';
    $identity = $operation === 'insert' ? null : dimension_writer_reference_id($key);
    if ($identity === false || $identity === 0 || ($operation !== 'delete' && !$next)
        || ($identity === null && array_key_exists($primary, $next))) return false;
    try {
        if (!begin_transaction()) return false;
        $parent = $header ? $identity : (isset($next['request_id']) ? dimension_writer_reference_id($next['request_id']) : false);
        if (!$header && $identity !== null) {
            $hint = material_core_rows('SELECT request_id FROM '.TB_PREF.$owner.' WHERE line_id='.$identity);
            $parent = $hint ? dimension_writer_reference_id($hint[0]['request_id']) : false;
        }
        if (!$header && !$parent) { cancel_transaction(); return false; }
        $history = $parent ? material_core_history($parent) : null;
        $removing = $header && $operation === 'delete';
        $source = $parent ? ($removing ? material_removal_source($parent) : material_line_source($parent)) : null;
        $creation = $header && $identity === null ? material_entity_creation_state() : null;
        if ($creation === false) { cancel_transaction(); return false; }
        if ($parent && ($history === false || !$source)) { cancel_transaction(); return false; }
        $old = $header && $source ? $source['header'] : array();
        if (!$header && $identity !== null) {
            foreach ($source['lines'] as $line) if ((int)$line['line_id'] === $identity) $old = $line;
            if (!$old) { cancel_transaction(); return false; }
        }
        $defaults = $header
            ? array('request_type'=>'purchase','warehouse_loc_code'=>'','request_date'=>null,'required_date'=>null,'reference'=>null,'memo'=>null)
            : array('request_id'=>$parent,'stock_id'=>'','qty_requested'=>0,'required_date'=>null,'memo'=>null);
        $editable = array_keys($defaults);
        if (!$header && $identity !== null) $editable = array_diff($editable, array('request_id','stock_id'));
        foreach ($next as $field=>$value) {
            if (in_array($field, $editable, true)) continue;
            // Null is not empty text; arrays/objects never represent stored scalars.
            if (is_array($value) || is_object($value) || !array_key_exists($field, $old)
                || ($value === null ? $old[$field] !== null : $old[$field] === null || (string)$value !== (string)$old[$field])) {
                cancel_transaction(); return false;
            }
        }
        // Validators are extensible code, never authority to alter locked source.
        if (!$validate() || material_core_context() !== $context
            || ($creation !== null && material_entity_creation_state() !== $creation)
            || ($parent && (($removing ? material_removal_source($parent) : material_line_source($parent)) !== $source
                || material_core_history($parent) !== $history))) {
            cancel_transaction(); return false;
        }
        $data = $next + $old + $defaults;
        // Native writers encode display strings. Omitted stored values already
        // carry that encoding and must survive a partial update byte for byte.
        foreach ($editable as $field)
            if (!array_key_exists($field, $next) && isset($old[$field]) && is_string($data[$field]))
                $data[$field] = htmlspecialchars_decode($data[$field], ENT_QUOTES);
        if ($header) {
            if ($operation === 'delete') $result = delete_material_request($identity);
            elseif ($operation === 'insert') $result = add_material_request($data['request_type'], $data['warehouse_loc_code'],
                $data['request_date'], $data['required_date'], $data['reference'], $data['memo']);
            else $result = update_material_request($identity, $data['request_type'], $data['warehouse_loc_code'],
                $data['request_date'], $data['required_date'], $data['reference'], $data['memo']);
        } elseif ($operation === 'delete') $result = delete_material_request_line($identity, $parent);
        elseif ($operation === 'insert') $result = add_material_request_line($parent, $data['stock_id'],
            $data['qty_requested'], $data['required_date'], $data['memo']);
        else $result = update_material_request_line($identity, $data['qty_requested'], $data['required_date'], $data['memo'], $parent);
        if ($result && $header && $operation === 'delete') {
            $result = material_core_rows('SELECT request_id FROM '.TB_PREF.$owner.' WHERE request_id='.$identity.' FOR UPDATE') === array()
                && material_core_rows('SELECT line_id FROM '.TB_PREF.'wh_material_request_lines WHERE request_id='.$identity.' FOR UPDATE') === array()
                && material_core_rows('SELECT id FROM '.TB_PREF.'purch_requisitions WHERE material_request_id='.$identity.' FOR UPDATE') === array()
                && material_core_history($identity) === $history;
        }
        if (!$result || material_core_context() !== $context || !commit_transaction()) { cancel_transaction(); return false; }
        if ($identity === null && !db_preserve_insert_id($result)) return false;
        return true;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
