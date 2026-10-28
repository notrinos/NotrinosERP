<?php
/** Unreviewed standalone Draft cancellation retains complete source/history. */
require_once __DIR__.'/warehouse_material_line_db.inc';

/**
 * Cancel only the current maker's unconsumed standalone Draft. Rejected or
 * cancelled core reviews stay byte exact; pending, approved, unknown and paired
 * custody cannot be cancelled through this unreviewed-source command.
 */
function cancel_standalone_material_draft($request_id, $comments = '')
{
    $context = material_core_context();
    $identity = dimension_writer_reference_id($request_id);
    if (!$context || !$identity || !is_string($comments) || strpos($comments, "\0") !== false) return false;
    try {
        if (!begin_transaction()) return false;
        // Preserve the native submit/terminal lock order and current MVCC reads.
        $history = material_core_history($identity);
        $source = material_line_source($identity);
        if ($history === false || !$source) { cancel_transaction(); return false; }
        foreach ($history['drafts'] as $draft) {
            $workflow = material_core_rows('SELECT trans_type FROM '.TB_PREF.'approval_workflows WHERE id='.(int)$draft['workflow_id'].' FOR UPDATE');
            if (!in_array((int)$draft['status'], array(APPROVAL_STATUS_REJECTED, APPROVAL_STATUS_CANCELLED), true)
                || (int)$draft['submitted_by'] !== (int)$source['header']['requested_by']
                || !$workflow || (int)$workflow[0]['trans_type'] !== ST_WH_MATERIAL_REQUEST) { cancel_transaction(); return false; }
        }
        $clock = material_core_rows('SELECT NOW() AS now_');
        if (!$clock || !db_query('UPDATE '.TB_PREF."wh_material_requests SET status='cancelled',updated_at=NOW() WHERE request_id="
            .$identity." AND status='draft'") || db_num_affected_rows() !== 1) { cancel_transaction(); return false; }
        $headers = material_core_rows('SELECT * FROM '.TB_PREF.'wh_material_requests WHERE request_id='.$identity.' FOR UPDATE');
        $lines = material_core_rows('SELECT * FROM '.TB_PREF.'wh_material_request_lines WHERE request_id='.$identity.' ORDER BY line_id FOR UPDATE');
        $pairs = material_core_rows('SELECT id FROM '.TB_PREF.'purch_requisitions WHERE material_request_id='.$identity.' ORDER BY id FOR UPDATE');
        $ids = array();
        foreach ($source['lines'] as $line) $ids[] = (int)$line['line_id'];
        $links = $ids ? material_core_rows('SELECT id FROM '.TB_PREF.'purch_requisition_lines WHERE material_request_line_id IN ('
            .implode(',', $ids).') ORDER BY id FOR UPDATE') : array();
        $now = material_core_rows('SELECT NOW() AS now_');
        $expected = $source['header']; $expected['status'] = 'cancelled';
        if (!$headers || count($headers) !== 1 || !$now || !$headers[0]['updated_at']
            || $headers[0]['updated_at'] < $clock[0]['now_'] || $headers[0]['updated_at'] > $now[0]['now_']) { cancel_transaction(); return false; }
        $expected['updated_at'] = $headers[0]['updated_at'];
        if ($headers[0] !== $expected || $lines !== $source['lines'] || $pairs !== array() || $links !== array()
            || material_core_history($identity) !== $history || material_core_context() !== $context
            || !commit_transaction()) { cancel_transaction(); return false; }
        return true;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
