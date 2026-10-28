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
 * parent/stock identity. Authored metadata joins the ordinary retained-reference
 * fence; opaque history can only be preserved exactly. Paired requisitions retain
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
        if ($operation !== 'delete') $editable[] = 'custom_data';
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
            if ($field !== 'custom_data' && !array_key_exists($field, $next) && isset($old[$field]) && is_string($data[$field]))
                $data[$field] = htmlspecialchars_decode($data[$field], ENT_QUOTES);
        /** Persist native fields and exact metadata as one fenced unit. */
        $persist = function($ignored) use ($header, $operation, $identity, $data, $parent, $owner, $next, $context) {
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
            if ($result && $operation !== 'delete' && array_key_exists('custom_data', $next)) {
                $written = $identity === null ? $result : $identity;
                $result = write_material_entity_metadata($owner, $written, $header ? $written : $parent,
                    $next['custom_data'], $context) ? $result : false;
            }
            return $result;
        };
        // Lock the complete old/new metadata set before native maintenance
        // takes its retained-only locks. Otherwise an old high ID followed by
        // a new low ID could invert the common sorted-master lock order.
        $metadata_update = $identity !== null && $operation !== 'delete' && array_key_exists('custom_data', $next);
        if ($metadata_update) {
            // Keep native location/stock dependencies ahead of master locks,
            // including other requests using the same stock or location row.
            if ($header) {
                $values = material_header_values($data['request_type'], $data['warehouse_loc_code'], $data['request_date'],
                    $data['required_date'], $data['reference'], $data['memo']);
                $dependency = $values ? material_core_rows('SELECT loc_code FROM '.TB_PREF.'locations WHERE loc_code='
                    .db_escape($values['warehouse_loc_code']).' FOR UPDATE') : false;
            } else $dependency = material_core_rows('SELECT stock_id FROM '.TB_PREF.'stock_master WHERE stock_id='
                .db_escape($old['stock_id']).' FOR UPDATE');
            if (!$dependency) { cancel_transaction(); return false; }
        }
        $result = $metadata_update
            ? dimension_ordinary_master_write($owner, $identity, array('custom_data'=>$next['custom_data']), $persist, true)
            : $persist(array());
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

/**
 * Persist exact native metadata after native CRUD, within its outer transaction.
 * Current source/history and sorted old/new Dimension locks cover the full
 * command. No display/HTML decoder may rewrite opaque or already encoded bytes.
 */
function write_material_entity_metadata($owner, $identity, $parent, $payload, $context)
{
    $identity = dimension_writer_reference_id($identity);
    $parent = dimension_writer_reference_id($parent);
    if (!in_array($owner, array('wh_material_requests','wh_material_request_lines'), true)
        || !$identity || !$parent
        || empty($GLOBALS['transaction_level']) || !$context || $context !== material_core_context()
        || ($payload !== null && (!is_string($payload) || strlen($payload) > 65535))) return false;
    $history = material_core_history($parent);
    $source = material_line_source($parent);
    if ($history === false || !$source) return false;
    foreach ($history['drafts'] as $draft)
        if (!in_array((int)$draft['status'], array(APPROVAL_STATUS_REJECTED, APPROVAL_STATUS_CANCELLED), true)
            || (int)$draft['submitted_by'] !== (int)$source['header']['requested_by']) return false;
    $header = $owner === 'wh_material_requests';
    if ($header && $identity !== $parent) return false;
    $expected = $source;
    $previous = $header ? $source['header']['custom_data'] : null;
    if ($header) $expected['header']['custom_data'] = $payload;
    else {
        $found = false;
        foreach ($expected['lines'] as $index=>$line)
            if ((int)$line['line_id'] === (int)$identity) {
                $previous = $line['custom_data'];
                $expected['lines'][$index]['custom_data'] = $payload; $found = true;
            }
        if (!$found) return false;
    }
    $transition = dimension_master_payload_transition($previous, $payload);
    if ($transition === false) return false;
    $locks = $transition['locks']; sort($locks, SORT_NUMERIC);
    $master_query = $locks ? 'SELECT id,type_,closed FROM '.TB_PREF.'dimensions WHERE id IN ('
        .implode(',', $locks).') ORDER BY id FOR UPDATE' : null;
    return dimension_ordinary_master_write($owner, $identity, array('custom_data'=>$payload),
        function($ignored) use ($owner, $identity, $parent, $payload, $header, $source, $expected, $history, $context, $master_query) {
            if ($context !== material_core_context() || material_line_source($parent) !== $source
                || material_core_history($parent) !== $history) return false;
            $masters = $master_query ? material_core_rows($master_query) : array();
            if ($masters === false) return false;
            // Suppress the header's automatic updated_at effect: the native
            // header command already owns that timestamp and its verification.
            $sql = 'UPDATE '.TB_PREF.$owner.' SET custom_data='.($payload === null ? 'NULL' : "'".db_escape_function($payload)."'")
                .($header ? ',updated_at=updated_at' : '').' WHERE '.($header ? 'request_id' : 'line_id').'='.(int)$identity;
            if (!db_query($sql)) return false;
            $current = material_line_source($parent);
            return $current && material_core_source_fingerprint($current) === material_core_source_fingerprint($expected)
                && material_core_history($parent) === $history && material_core_context() === $context
                && (!$master_query || material_core_rows($master_query) === $masters);
        }, true);
}
