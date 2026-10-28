<?php
/** Native standalone demand lines retain source, review and classification custody. */
require_once __DIR__.'/warehouse_material_core_db.inc';

/** Read the complete current standalone draft, including an empty line set. */
function material_line_source($identity)
{
    $headers = material_core_rows('SELECT * FROM '.TB_PREF.'wh_material_requests WHERE request_id='.$identity.' FOR UPDATE');
    if (!$headers || count($headers) !== 1 || $headers[0]['status'] !== 'draft'
        || (int)$headers[0]['requested_by'] !== (int)$_SESSION['wa_current_user']->user
        || !empty($headers[0]['linked_doc_type']) || !empty($headers[0]['linked_doc_no'])) return false;
    $pairs = material_core_rows('SELECT id FROM '.TB_PREF.'purch_requisitions WHERE material_request_id='.$identity.' ORDER BY id FOR UPDATE');
    $lines = material_core_rows('SELECT * FROM '.TB_PREF.'wh_material_request_lines WHERE request_id='.$identity.' ORDER BY line_id FOR UPDATE');
    if ($pairs === false || $pairs || $lines === false) return false;
    $ids = array();
    foreach ($lines as $line) {
        if (!is_numeric($line['qty_fulfilled']) || (float)$line['qty_fulfilled'] != 0) return false;
        $ids[] = (int)$line['line_id'];
    }
    if ($ids) {
        $links = material_core_rows('SELECT id FROM '.TB_PREF.'purch_requisition_lines WHERE material_request_line_id IN ('
            .implode(',', $ids).') ORDER BY id FOR UPDATE');
        if ($links === false || $links) return false;
    }
    return array('header'=>$headers[0], 'lines'=>$lines);
}

/** Validate stock-unit precision and native nullable date/text without silent rounding. */
function material_line_values($quantity, $date, $memo)
{
    if (!is_numeric($quantity) || !is_finite((float)$quantity)) return false;
    $rounded = round((float)$quantity, 4);
    // A relative epsilon grows beyond one stored quantum near DECIMAL's limit.
    // Cap it below that quantum, retaining small-value binary arithmetic noise.
    $epsilon = min(0.000000000001, max(1, abs($rounded)) * 2.220446049250313e-16 * 2);
    if ($rounded <= 0 || $rounded > 99999999999.9999 || abs($rounded - (float)$quantity) > $epsilon) return false;
    if ($date === '') $date = null;
    if ($date !== null && (!is_string($date) || !preg_match('/^([0-9]{4})-([0-9]{2})-([0-9]{2})$/D', $date, $parts)
        || (int)$parts[1] < 1000 || !checkdate((int)$parts[2], (int)$parts[3], (int)$parts[1]))) return false;
    if ($memo !== null && (!is_string($memo) || strpos($memo, "\0") !== false)) return false;
    // Compare against the exact native escaped representation, including '0'.
    $rows = material_core_rows('SELECT CAST('.db_escape(number_format($rounded, 4, '.', '')).' AS DECIMAL(15,4)) AS qty_requested,'
        .($date === null ? 'NULL' : db_escape($date)).' AS required_date,'
        .($memo === null || $memo === '' ? 'NULL' : db_escape($memo)).' AS memo');
    if (!$rows) return false;
    $length = material_core_rows('SELECT CHAR_LENGTH('.($rows[0]['memo'] === null ? 'NULL' : db_escape($rows[0]['memo'])).') AS length_');
    return $length && ($length[0]['length_'] === null || (int)$length[0]['length_'] <= 255) ? $rows[0] : false;
}

/**
 * Own native add/update/remove under the existing core-history and source locks.
 * A child lookup is only a parent hint. Paired demand remains requisition-owned;
 * no historical payload, decision, fulfillment or posting is rewritten here.
 */
function write_standalone_material_line($operation, $key, $data, $expected_parent = null)
{
    $context = material_core_context(); $identity = dimension_writer_reference_id($key);
    if (!$context || !$identity || !in_array($operation, array('add','update','delete'), true) || !is_array($data)
        || ($expected_parent !== null && !dimension_writer_reference_id($expected_parent))) return false;
    try {
        if (!begin_transaction()) return false;
        $parent = $identity;
        if ($operation !== 'add') {
            $hint = material_core_rows('SELECT request_id FROM '.TB_PREF.'wh_material_request_lines WHERE line_id='.$identity);
            $parent = $hint ? dimension_writer_reference_id($hint[0]['request_id']) : false;
        }
        if (!$parent || ($expected_parent !== null && $parent !== dimension_writer_reference_id($expected_parent))) { cancel_transaction(); return false; }
        // Match the existing native submit/terminal lock order: core history first.
        $history = material_core_history($parent);
        $source = material_line_source($parent);
        if ($history === false || !$source) { cancel_transaction(); return false; }
        foreach ($history['drafts'] as $draft)
            if (!in_array((int)$draft['status'], array(APPROVAL_STATUS_REJECTED, APPROVAL_STATUS_CANCELLED), true)
                || (int)$draft['submitted_by'] !== (int)$source['header']['requested_by']) { cancel_transaction(); return false; }
        $old = null;
        foreach ($source['lines'] as $line) if ((int)$line['line_id'] === $identity && $operation !== 'add') $old = $line;
        if ($operation !== 'add' && !$old) { cancel_transaction(); return false; }
        $values = array();
        if ($operation !== 'delete') {
            if (!array_key_exists('quantity', $data) || !array_key_exists('required_date', $data) || !array_key_exists('memo', $data)
                || !($values = material_line_values($data['quantity'], $data['required_date'], $data['memo']))) { cancel_transaction(); return false; }
            $stock_id = $old ? $old['stock_id'] : (isset($data['stock_id']) ? dimension_writer_stock_id($data['stock_id']) : false);
            if ($stock_id === false) { cancel_transaction(); return false; }
            $stock = material_core_rows('SELECT stock_id,inactive FROM '.TB_PREF.'stock_master WHERE stock_id='.db_escape($stock_id).' FOR UPDATE');
            if (!$stock || $stock[0]['stock_id'] !== $stock_id || (!$old && $stock[0]['inactive'])) { cancel_transaction(); return false; }
        }
        $result = dimension_ordinary_master_write('wh_material_request_lines', $old ? $identity : null, array(),
            function($ignored) use ($operation, $parent, $identity, $old, $values, $data, $source, $history, $context) {
                if (material_core_context() !== $context) return false;
                $expected = $source; $line_id = $identity;
                if ($operation === 'delete') {
                    if (!db_query('DELETE FROM '.TB_PREF.'wh_material_request_lines WHERE line_id='.$identity.' AND request_id='.$parent)
                        || db_num_affected_rows() !== 1) return false;
                    $expected['lines'] = array_values(array_filter($expected['lines'], function($line) use ($identity) { return (int)$line['line_id'] !== $identity; }));
                } else {
                    $sets = 'qty_requested='.db_escape($values['qty_requested']).',required_date='
                        .($values['required_date'] === null ? 'NULL' : db_escape($values['required_date'])).',memo='
                        .($values['memo'] === null ? 'NULL' : db_escape($values['memo']));
                    if ($operation === 'add') {
                        if (!db_query('INSERT INTO '.TB_PREF.'wh_material_request_lines SET request_id='.$parent.',stock_id='
                            .db_escape($data['stock_id']).',qty_fulfilled=0,'.$sets)) return false;
                        $line_id = db_insert_id();
                        if (!$line_id) return false;
                        $next = array('line_id'=>(string)$line_id,'request_id'=>(string)$parent,'stock_id'=>$data['stock_id'],
                            'qty_requested'=>$values['qty_requested'],'qty_fulfilled'=>'0.0000','required_date'=>$values['required_date'],
                            'memo'=>$values['memo'],'custom_data'=>null);
                        $expected['lines'][] = $next;
                    } else {
                        if (!db_query('UPDATE '.TB_PREF.'wh_material_request_lines SET '.$sets.' WHERE line_id='.$identity.' AND request_id='.$parent)) return false;
                        foreach ($expected['lines'] as $index=>$line)
                            if ((int)$line['line_id'] === $identity) $expected['lines'][$index] = array_merge($old, $values);
                    }
                }
                $current = material_line_source($parent);
                if (!$current || material_core_source_fingerprint($current) !== material_core_source_fingerprint($expected)
                    || material_core_history($parent) !== $history || material_core_context() !== $context) return false;
                if ($operation === 'add' && !db_preserve_insert_id($line_id)) return false;
                return $operation === 'add' ? $line_id : true;
            }, true, $operation === 'delete');
        if (!$result || material_core_context() !== $context || !commit_transaction()) { cancel_transaction(); return false; }
        if ($operation === 'add' && !db_preserve_insert_id($result)) return false;
        return $result;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
