<?php
/** Native draft line commands retain complete Transfer source and core history. */
require_once __DIR__.'/warehouse_transfer_core_db.inc';

/** Lock a maker-owned, unconsumed standalone draft, including an empty line set. */
function transfer_line_source($identity)
{
    $source = transfer_core_source($identity, 'draft', true);
    if (!$source || (int)$source['header']['requested_by'] !== (int)$_SESSION['wa_current_user']->user
        || $source['header']['approved_by'] !== null || $source['header']['approval_date'] !== null) return false;
    foreach ($source['lines'] as $line)
        foreach (array('qty_shipped','qty_received') as $field)
            if (!is_numeric($line[$field]) || !is_finite((float)$line[$field]) || (float)$line[$field] != 0) return false;
    return $source;
}

/** Reject lossy DECIMAL input instead of silently rounding native quantities/costs. */
function transfer_line_decimal($value, $positive)
{
    if (!is_numeric($value) || !is_finite((float)$value)) return false;
    $rounded = round((float)$value, 4);
    $epsilon = min(0.000000000001, max(1, abs($rounded)) * 2.220446049250313e-16 * 2);
    if ($rounded < 0 || ($positive && $rounded == 0) || $rounded > 99999999999.9999
        || abs($rounded - (float)$value) > $epsilon) return false;
    return number_format($rounded, 4, '.', '');
}

/** Normalize only authored intrinsic fields using the native driver's text encoding. */
function transfer_line_values($data)
{
    foreach (array('qty_requested','unit_cost','memo','from_bin_id','to_bin_id','batch_id','serial_id') as $field)
        if (!array_key_exists($field, $data)) return false;
    $quantity = transfer_line_decimal($data['qty_requested'], true);
    $cost = transfer_line_decimal($data['unit_cost'], false);
    if ($quantity === false || $cost === false || ($data['memo'] !== null
        && (!is_string($data['memo']) || strpos($data['memo'], "\0") !== false))) return false;
    $sets = array('CAST('.db_escape($quantity).' AS DECIMAL(15,4)) AS qty_requested',
        'CAST('.db_escape($cost).' AS DECIMAL(15,4)) AS unit_cost');
    foreach (array('from_bin_id','to_bin_id','batch_id','serial_id') as $field) {
        $id = $data[$field] === null ? 0 : dimension_writer_reference_id($data[$field]);
        if ($id === false) return false;
        $sets[] = ($id ? $id : 'NULL').' AS '.$field;
    }
    $sets[] = ($data['memo'] === null || $data['memo'] === '' ? 'NULL' : db_escape($data['memo'])).' AS memo';
    $values = transfer_core_rows('SELECT '.implode(',', $sets));
    if (!$values) return false;
    $length = transfer_core_rows('SELECT CHAR_LENGTH('.($values[0]['memo'] === null ? 'NULL' : db_escape($values[0]['memo'])).') AS length_');
    return $length && ($length[0]['length_'] === null || (int)$length[0]['length_'] <= 255) ? $values[0] : false;
}

/**
 * Own add/update/remove with history-first, parent-then-child locks. A child ID
 * lookup is only a hint; its current parent and optional rendered parent must
 * still agree under lock. This command grants no approval or posting authority.
 */
function write_native_transfer_line($operation, $key, $data, $expected_parent = null)
{
    $context = transfer_core_context(); $identity = dimension_writer_reference_id($key);
    if (!$context || !$identity || !in_array($operation, array('add','update','delete'), true) || !is_array($data)
        || ($expected_parent !== null && !dimension_writer_reference_id($expected_parent))) return false;
    try {
        if (!begin_transaction()) return false;
        $parent = $identity;
        if ($operation !== 'add') {
            $hint = transfer_core_rows('SELECT transfer_id FROM '.TB_PREF.'wh_transfer_order_lines WHERE line_id='.$identity);
            $parent = $hint ? dimension_writer_reference_id($hint[0]['transfer_id']) : false;
        }
        if (!$parent || ($expected_parent !== null && $parent !== dimension_writer_reference_id($expected_parent))) { cancel_transaction(); return false; }
        $history = transfer_core_history($parent); $source = transfer_line_source($parent);
        if ($history === false || !$source) { cancel_transaction(); return false; }
        foreach ($history['drafts'] as $draft) {
            $workflow = transfer_core_rows('SELECT trans_type FROM '.TB_PREF.'approval_workflows WHERE id='.(int)$draft['workflow_id'].' FOR UPDATE');
            if (!in_array((int)$draft['status'], array(APPROVAL_STATUS_REJECTED, APPROVAL_STATUS_CANCELLED), true)
                || (int)$draft['submitted_by'] !== (int)$source['header']['requested_by']
                || !$workflow || (int)$workflow[0]['trans_type'] !== ST_WH_TRANSFER_ORDER) { cancel_transaction(); return false; }
        }
        $old = null;
        foreach ($source['lines'] as $line) if ($operation !== 'add' && (int)$line['line_id'] === $identity) $old = $line;
        if ($operation !== 'add' && !$old) { cancel_transaction(); return false; }
        $values = array(); $stock_id = null;
        if ($operation !== 'delete') {
            $values = transfer_line_values($data);
            $stock_id = $old ? $old['stock_id'] : (isset($data['stock_id']) ? dimension_writer_stock_id($data['stock_id']) : false);
            if (!$values || $stock_id === false) { cancel_transaction(); return false; }
        }
        $expected = $source;
        if ($operation === 'delete') {
            $expected['lines'] = array_values(array_filter($expected['lines'], function($line) use ($identity) { return (int)$line['line_id'] !== $identity; }));
        } elseif ($old) {
            foreach ($expected['lines'] as $index=>$line)
                if ((int)$line['line_id'] === $identity) $expected['lines'][$index] = array_merge($old, $values);
        } else {
            $expected['lines'][] = array('line_id'=>0,'transfer_id'=>$parent,'stock_id'=>$stock_id,
                'qty_requested'=>$values['qty_requested'],'qty_shipped'=>'0.0000','qty_received'=>'0.0000',
                'from_bin_id'=>$values['from_bin_id'],'to_bin_id'=>$values['to_bin_id'],
                'batch_id'=>$values['batch_id'],'serial_id'=>$values['serial_id'],
                'unit_cost'=>$values['unit_cost'],'memo'=>$values['memo'],'custom_data'=>null);
        }
        $dependencies = transfer_core_dependencies($expected);
        if (!$dependencies) { cancel_transaction(); return false; }
        foreach ($expected['lines'] as $line)
            if ($dependencies['stock'][$line['stock_id']][0]['stock_id'] !== $line['stock_id']) { cancel_transaction(); return false; }
        foreach ($dependencies['locations'] as $code=>$rows)
            if ($rows[0]['loc_code'] !== (string)$code) { cancel_transaction(); return false; }
        $result = dimension_ordinary_master_write('wh_transfer_order_lines', $old ? $identity : null, array(),
            function($ignored) use ($operation, $parent, $identity, $values, $stock_id, &$expected, $history, $dependencies, $context) {
                if (transfer_core_context() !== $context) return false;
                $line_id = $identity;
                if ($operation === 'delete') {
                    if (!db_query('DELETE FROM '.TB_PREF.'wh_transfer_order_lines WHERE line_id='.$identity.' AND transfer_id='.$parent)
                        || db_num_affected_rows() !== 1) return false;
                } else {
                    $sets = array();
                    foreach ($values as $field=>$value) $sets[] = $field.'='.($value === null ? 'NULL' : db_escape($value));
                    if ($operation === 'add') {
                        if (!db_query('INSERT INTO '.TB_PREF.'wh_transfer_order_lines SET transfer_id='.$parent.',stock_id='
                            .db_escape($stock_id).',qty_shipped=0,qty_received=0,'.implode(',', $sets))) return false;
                        $line_id = db_insert_id();
                        if (!$line_id) return false;
                        $expected['lines'][count($expected['lines'])-1]['line_id'] = $line_id;
                    } elseif (!db_query('UPDATE '.TB_PREF.'wh_transfer_order_lines SET '.implode(',', $sets)
                        .' WHERE line_id='.$identity.' AND transfer_id='.$parent)) return false;
                }
                $current = transfer_line_source($parent);
                if (!$current || transfer_core_source_fingerprint($current) !== transfer_core_source_fingerprint($expected)
                    || transfer_core_history($parent) !== $history || transfer_core_dependencies($expected) !== $dependencies
                    || transfer_core_context() !== $context) return false;
                if ($operation === 'add' && !db_preserve_insert_id($line_id)) return false;
                return $operation === 'add' ? $line_id : true;
            }, true, $operation === 'delete');
        $current = $result ? transfer_line_source($parent) : false;
        if (!$current || transfer_core_source_fingerprint($current) !== transfer_core_source_fingerprint($expected)
            || transfer_core_history($parent) !== $history || transfer_core_dependencies($expected) !== $dependencies
            || transfer_core_context() !== $context || !commit_transaction()) { cancel_transaction(); return false; }
        if ($operation === 'add' && !db_preserve_insert_id($result)) return false;
        return $result;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
