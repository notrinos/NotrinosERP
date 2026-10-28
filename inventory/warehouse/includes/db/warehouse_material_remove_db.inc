<?php
/** Unreviewed source removal retains native ownership and historical custody. */
require_once __DIR__.'/warehouse_material_core_db.inc';

/** Lock an original maker's unreviewed, unconsumed Draft or cancelled source. */
function material_removal_source($identity)
{
    $headers = material_core_rows('SELECT * FROM '.TB_PREF.'wh_material_requests WHERE request_id='.$identity.' FOR UPDATE');
    if (!$headers || count($headers) !== 1
        || !in_array($headers[0]['status'], array('draft','cancelled'), true)
        || (int)$headers[0]['requested_by'] !== (int)$_SESSION['wa_current_user']->user
        || !empty($headers[0]['approved_by']) || !empty($headers[0]['linked_doc_type']) || !empty($headers[0]['linked_doc_no'])) return false;
    $pairs = material_core_rows('SELECT id FROM '.TB_PREF.'purch_requisitions WHERE material_request_id='.$identity.' ORDER BY id FOR UPDATE');
    $lines = material_core_rows('SELECT * FROM '.TB_PREF.'wh_material_request_lines WHERE request_id='.$identity.' ORDER BY line_id FOR UPDATE');
    if ($pairs === false || $pairs || $lines === false) return false;
    $ids = array();
    foreach ($lines as $line) {
        if (!is_numeric($line['qty_fulfilled']) || !is_finite((float)$line['qty_fulfilled']) || (float)$line['qty_fulfilled'] != 0) return false;
        $ids[] = (int)$line['line_id'];
    }
    if ($ids && material_core_rows('SELECT id FROM '.TB_PREF.'purch_requisition_lines WHERE material_request_line_id IN ('
        .implode(',', $ids).') ORDER BY id FOR UPDATE') !== array()) return false;
    return array('header'=>$headers[0], 'lines'=>$lines);
}

/**
 * Remove only positively unreviewed demand under current history/source locks.
 * No retained classification, decision, downstream link or posting is erased.
 * Any SQL failure or incomplete terminal effect aborts the whole outer command.
 */
function remove_standalone_material_source($request_id)
{
    $identity = dimension_writer_reference_id($request_id); $context = material_core_context();
    if (!$identity || !$context) return false;
    try {
        if (!begin_transaction()) return false;
        $history = material_core_history($identity);
        $source = material_removal_source($identity);
        if ($history === false || $history['drafts'] || !$source
            || dimension_master_payload_transition($source['header']['custom_data'], null, true) === false) { cancel_transaction(); return false; }
        $ids = array();
        foreach ($source['lines'] as $line) {
            if (dimension_master_payload_transition($line['custom_data'], null, true) === false) { cancel_transaction(); return false; }
            $ids[] = (int)$line['line_id'];
        }
        if (!db_query('DELETE FROM '.TB_PREF.'wh_material_request_lines WHERE request_id='.$identity)
            || db_num_affected_rows() !== count($source['lines'])
            || !db_query('DELETE FROM '.TB_PREF.'wh_material_requests WHERE request_id='.$identity) || db_num_affected_rows() !== 1
            || material_core_rows('SELECT request_id FROM '.TB_PREF.'wh_material_requests WHERE request_id='.$identity.' FOR UPDATE') !== array()
            || material_core_rows('SELECT line_id FROM '.TB_PREF.'wh_material_request_lines WHERE request_id='.$identity.' ORDER BY line_id FOR UPDATE') !== array()
            || material_core_rows('SELECT id FROM '.TB_PREF.'purch_requisitions WHERE material_request_id='.$identity.' ORDER BY id FOR UPDATE') !== array()
            || ($ids && material_core_rows('SELECT id FROM '.TB_PREF.'purch_requisition_lines WHERE material_request_line_id IN ('
                .implode(',', $ids).') ORDER BY id FOR UPDATE') !== array())
            || material_core_history($identity) !== $history || material_core_context() !== $context
            || !commit_transaction()) { cancel_transaction(); return false; }
        return true;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
