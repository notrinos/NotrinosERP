<?php
/** Current-state derived requisition commands; never grant transaction approval. */
require_once __DIR__.'/purch_requisition_header_db.inc';

/**
 * Recompute one derived header field under parent-first current row locks.
 * Retained classifications, source links, approval history and every child byte
 * are read-only. Total repair is limited to editable documents; order progress
 * requires an already approved document. The caller retains any outer commit.
 */
function write_purch_requisition_progress($requisition_id, $field)
{
    $context = purch_requisition_header_context();
    $identity = dimension_writer_reference_id($requisition_id);
    if ($context === false || !$identity || !in_array($field, array('total_estimated','status'), true)) return false;
    try {
        if (!begin_transaction()) return false;
        $headers = purch_requisition_header_rows('SELECT * FROM '.TB_PREF.'purch_requisitions WHERE id='.$identity.' FOR UPDATE');
        $allowed = $field === 'status' ? array('approved','partially_ordered','ordered') : array('draft','rejected');
        if (!$headers || !in_array($headers[0]['status'], $allowed, true)) { cancel_transaction(); return false; }
        $lines = purch_requisition_header_rows('SELECT * FROM '.TB_PREF.'purch_requisition_lines WHERE requisition_id='.$identity.' ORDER BY id FOR UPDATE');
        if ($lines === false) { cancel_transaction(); return false; }
        $ordered = 0; $full = 0;
        foreach ($lines as $line) {
            foreach (array('quantity','estimated_unit_price','qty_ordered') as $column)
                if (!is_numeric($line[$column]) || !is_finite((float)$line[$column]) || (float)$line[$column] < 0) {
                    cancel_transaction(); return false;
                }
            if ((float)$line['quantity'] <= 0) { cancel_transaction(); return false; }
            if ((float)$line['qty_ordered'] > 0) $ordered++;
            if ((float)$line['qty_ordered'] >= (float)$line['quantity']) $full++;
        }
        if ($field === 'status') {
            $value = count($lines) && $full === count($lines) ? 'ordered' : ($ordered ? 'partially_ordered' : 'approved');
        } else {
            // FOR UPDATE is a current read even when the outer caller has an old
            // repeatable-read snapshot. Preserve native SQL SUM/DOUBLE semantics.
            $totals = purch_requisition_header_rows('SELECT COALESCE(SUM(quantity*estimated_unit_price),0) AS total_estimated FROM '.TB_PREF.'purch_requisition_lines WHERE requisition_id='.$identity.' FOR UPDATE');
            if (!$totals || !is_numeric($totals[0]['total_estimated']) || !is_finite((float)$totals[0]['total_estimated'])) {
                cancel_transaction(); return false;
            }
            $value = $totals[0]['total_estimated'];
        }
        if (purch_requisition_header_context() !== $context
            || !db_query('UPDATE '.TB_PREF.'purch_requisitions SET '.$field.'='.db_escape($value).' WHERE id='.$identity)) {
            cancel_transaction(); return false;
        }
        $current = purch_requisition_header_rows('SELECT * FROM '.TB_PREF.'purch_requisitions WHERE id='.$identity.' FOR UPDATE');
        if (!$current || ($field === 'status' ? $current[0][$field] !== $value : (float)$current[0][$field] !== (float)$value)) {
            cancel_transaction(); return false;
        }
        foreach ($headers[0] as $column=>$old)
            if (is_string($column) && $column !== $field && $current[0][$column] !== $old) {
                cancel_transaction(); return false;
            }
        if (purch_requisition_header_rows('SELECT * FROM '.TB_PREF.'purch_requisition_lines WHERE requisition_id='.$identity.' ORDER BY id FOR UPDATE') !== $lines
            || purch_requisition_header_context() !== $context || !commit_transaction()) {
            cancel_transaction(); return false;
        }
        return true;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
