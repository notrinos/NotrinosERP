<?php
/** Canonical generic Transfer CRUD delegates to the native command owners. */
require_once __DIR__.'/warehouse_transfers_db.inc';
require_once __DIR__.'/warehouse_transfer_header_db.inc';

/** Bind extensible creation validation to every existing native source row. */
function transfer_entity_creation_state()
{
    $state = transfer_header_creation_state();
    $lines = transfer_core_rows('SELECT * FROM '.TB_PREF.'wh_transfer_order_lines ORDER BY line_id FOR UPDATE');
    if ($state === false || $lines === false) return false;
    $state[] = $lines;
    return $state;
}

/**
 * Merge partial input under history/parent locks, retain native authorization and
 * validation, and own native CRUD plus exact metadata as one transaction. Generic
 * persistence grants no workflow, fulfillment, numbering or reparenting authority.
 */
function write_transfer_order_entity($owner, $operation, $key, $next, $validate)
{
    $context = transfer_core_context();
    if (!$context || !is_callable($validate) || !is_array($next)
        || !in_array($owner, array('wh_transfer_orders','wh_transfer_order_lines'), true)
        || !in_array($operation, array('insert','update','delete'), true)) return false;
    $header = $owner === 'wh_transfer_orders';
    $primary = $header ? 'transfer_id' : 'line_id';
    $identity = $operation === 'insert' ? null : dimension_writer_reference_id($key);
    if ($identity === false || $identity === 0 || ($operation !== 'delete' && !$next)
        || ($identity === null && array_key_exists($primary, $next))) return false;
    try {
        if (!begin_transaction()) return false;
        $parent = $header ? $identity : (isset($next['transfer_id']) ? dimension_writer_reference_id($next['transfer_id']) : false);
        if (!$header && $identity !== null) {
            $hint = transfer_core_rows('SELECT transfer_id FROM '.TB_PREF.$owner.' WHERE line_id='.$identity);
            $parent = $hint ? dimension_writer_reference_id($hint[0]['transfer_id']) : false;
        }
        if (!$header && !$parent) { cancel_transaction(); return false; }
        $history = $parent ? transfer_core_history($parent) : null;
        $source = $parent ? transfer_line_source($parent) : null;
        $creation = $header && $identity === null ? transfer_entity_creation_state() : null;
        if ($creation === false || ($parent && (!$source
            || !transfer_header_history_editable($history, $source['header']['requested_by'])))) { cancel_transaction(); return false; }
        $old = $header && $source ? $source['header'] : array();
        if (!$header && $identity !== null) {
            foreach ($source['lines'] as $line) if ((int)$line['line_id'] === $identity) $old = $line;
            if (!$old) { cancel_transaction(); return false; }
        }
        $defaults = $header
            ? array('from_loc_code'=>'','to_loc_code'=>'','request_date'=>null,'transfer_type'=>'standard',
                'reference'=>null,'memo'=>null,'expected_ship_date'=>null,'expected_recv_date'=>null)
            : array('transfer_id'=>$parent,'stock_id'=>'','qty_requested'=>0,'unit_cost'=>0,
                'from_bin_id'=>null,'to_bin_id'=>null,'batch_id'=>null,'serial_id'=>null,'memo'=>null);
        $editable = array_keys($defaults);
        if ($operation !== 'delete') $editable[] = 'custom_data';
        if (!$header && $identity !== null) $editable = array_diff($editable, array('transfer_id','stock_id'));
        foreach ($next as $field=>$value) {
            if (in_array($field, $editable, true)) continue;
            if (is_array($value) || is_object($value) || !array_key_exists($field, $old)
                || ($value === null ? $old[$field] !== null : $old[$field] === null || (string)$value !== (string)$old[$field])) {
                cancel_transaction(); return false;
            }
        }
        $data = $next + $old + $defaults;
        // Native text writers encode display input; omitted values are already stored.
        foreach ($editable as $field)
            if ($field !== 'custom_data' && !array_key_exists($field, $next) && isset($old[$field]) && is_string($data[$field]))
                $data[$field] = htmlspecialchars_decode($data[$field], ENT_QUOTES);
        $expected = $source;
        if ($operation !== 'delete') {
            $values = $header ? transfer_header_values($data['from_loc_code'], $data['to_loc_code'], $data['request_date'],
                $data['transfer_type'], $data['reference'], $data['memo'], $data['expected_ship_date'], $data['expected_recv_date'])
                : transfer_line_values($data);
            if (!$values) { cancel_transaction(); return false; }
            // Legacy noncanonical text/null encodings are not permission to
            // normalize an omitted field. Require explicit authored correction.
            foreach ($values as $field=>$value)
                if (!array_key_exists($field, $next) && array_key_exists($field, $old)
                    && ($value === null ? $old[$field] !== null
                        : $old[$field] === null || (string)$value !== (string)$old[$field])) {
                    cancel_transaction(); return false;
                }
            if ($header) $expected = array('header'=>array_merge($old, $values), 'lines'=>$source ? $source['lines'] : array());
            elseif ($identity === null) $expected['lines'][] = $data;
            else foreach ($expected['lines'] as $index=>$line)
                if ((int)$line['line_id'] === $identity) $expected['lines'][$index] = array_merge($line, $values);
        }
        // Native dependencies precede sorted old/new Dimension locks, including
        // both the proposed values and all retained children of this parent.
        $actor = (int)$_SESSION['wa_current_user']->user;
        $dependencies = transfer_header_dependencies($expected, $actor);
        if (!$dependencies || !$validate() || transfer_core_context() !== $context
            || transfer_header_dependencies($expected, $actor) !== $dependencies
            || ($creation !== null && transfer_entity_creation_state() !== $creation)
            || ($parent && (transfer_line_source($parent) !== $source || transfer_core_history($parent) !== $history))) {
            cancel_transaction(); return false;
        }
        /** Persist native intrinsic fields, then owner-serialized metadata. */
        $persist = function($ignored) use ($header, $operation, $identity, $parent, $data, $next, $owner, $context) {
            if ($header) {
                if ($operation === 'delete') $result = delete_transfer_order($identity);
                else $result = write_native_transfer_header($identity, $data['from_loc_code'], $data['to_loc_code'],
                    $data['request_date'], $data['transfer_type'], $data['reference'], $data['memo'],
                    $data['expected_ship_date'], $data['expected_recv_date']);
            } elseif ($operation === 'delete') $result = delete_transfer_order_line($identity, $parent);
            else $result = write_native_transfer_line($operation === 'insert' ? 'add' : 'update',
                $identity === null ? $parent : $identity, $data, $parent);
            if ($result && $operation !== 'delete' && array_key_exists('custom_data', $next)) {
                $written = $identity === null ? $result : $identity;
                if (!write_transfer_entity_metadata($owner, $written, $header ? $written : $parent,
                    $next['custom_data'], $context)) return false;
            }
            return $result;
        };
        $result = $identity !== null && $operation !== 'delete' && array_key_exists('custom_data', $next)
            ? dimension_ordinary_master_write($owner, $identity, array('custom_data'=>$next['custom_data']), $persist, true)
            : $persist(array());
        // A late native SQL effect cannot leave an attributable demand pointing
        // at a removed header, even within the same transaction/connection.
        if ($result && $header && $operation === 'delete'
            && transfer_core_rows('SELECT request_id FROM '.TB_PREF.'wh_material_requests WHERE linked_doc_type='
                .ST_LOCTRANSFER.' AND linked_doc_no='.$identity.' ORDER BY request_id FOR UPDATE') !== array()) {
            cancel_transaction(); return false;
        }
        if (!$result || transfer_core_context() !== $context
            || transfer_header_dependencies($expected, $actor) !== $dependencies || !commit_transaction()) { cancel_transaction(); return false; }
        if ($identity === null && !db_preserve_insert_id($result)) return false;
        return true;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}

/**
 * Retain raw metadata under native source/history and sorted old/new master locks.
 * Completion checks cover every source/child/history/dependency byte; this helper
 * requires an owning transaction and grants no standalone or workflow authority.
 */
function write_transfer_entity_metadata($owner, $identity, $parent, $payload, $context)
{
    $identity = dimension_writer_reference_id($identity); $parent = dimension_writer_reference_id($parent);
    if (!in_array($owner, array('wh_transfer_orders','wh_transfer_order_lines'), true) || !$identity || !$parent
        || empty($GLOBALS['transaction_level']) || !$context || $context !== transfer_core_context()
        || ($payload !== null && (!is_string($payload) || strlen($payload) > 65535))) return false;
    $history = transfer_core_history($parent); $source = transfer_line_source($parent);
    if (!$source || !transfer_header_history_editable($history, $source['header']['requested_by'])) return false;
    $header = $owner === 'wh_transfer_orders'; $expected = $source; $previous = null; $found = false;
    if ($header) {
        if ($identity !== $parent) return false;
        $previous = $source['header']['custom_data']; $expected['header']['custom_data'] = $payload; $found = true;
    } else foreach ($expected['lines'] as $index=>$line) if ((int)$line['line_id'] === $identity) {
        $previous = $line['custom_data']; $expected['lines'][$index]['custom_data'] = $payload; $found = true;
    }
    if (!$found || ($transition = dimension_master_payload_transition($previous, $payload)) === false) return false;
    $locks = $transition['locks']; sort($locks, SORT_NUMERIC);
    $query = $locks ? 'SELECT id,type_,closed FROM '.TB_PREF.'dimensions WHERE id IN ('
        .implode(',', $locks).') ORDER BY id FOR UPDATE' : null;
    $dependencies = transfer_header_dependencies($source, (int)$_SESSION['wa_current_user']->user);
    if (!$dependencies) return false;
    return dimension_ordinary_master_write($owner, $identity, array('custom_data'=>$payload),
        function($ignored) use ($owner, $identity, $parent, $payload, $header, $source, $expected, $history, $context, $query, $dependencies) {
            if ($context !== transfer_core_context() || transfer_line_source($parent) !== $source
                || transfer_core_history($parent) !== $history) return false;
            $masters = $query ? transfer_core_rows($query) : array();
            if ($masters === false || !db_query('UPDATE '.TB_PREF.$owner.' SET custom_data='
                .($payload === null ? 'NULL' : "'".db_escape_function($payload)."'")
                .($header ? ',updated_at=updated_at' : '').' WHERE '.($header ? 'transfer_id' : 'line_id').'='.$identity)) return false;
            $current = transfer_line_source($parent);
            return $current && transfer_core_source_fingerprint($current) === transfer_core_source_fingerprint($expected)
                && transfer_core_history($parent) === $history && transfer_core_context() === $context
                && transfer_header_dependencies($expected, (int)$_SESSION['wa_current_user']->user) === $dependencies
                && (!$query || transfer_core_rows($query) === $masters);
        }, true);
}
