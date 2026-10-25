<?php
/** B02 native monthly budget admission; no baseline/revision or approval policy. */
require_once __DIR__.'/../../../dimensions/includes/dimension_reference_writer_fence.inc';

/**
 * Serialize the exact native coordinate and every matching row before writing.
 * The legacy index is deliberately nonunique: preserve duplicate-row behavior,
 * retained JSON and exact closed/orphan coordinates on amount maintenance.
 * A failed inner operation cancels the whole owning annual/outer transaction.
 */
function dimension_native_budget_write($date_, $account, $dimension, $dimension2, $amount, $remove = false)
{
    if (!is_string($date_) || !is_date($date_)) return false;
    $account = dimension_writer_chart_id($account);
    $dimension = dimension_writer_reference_id($dimension);
    $dimension2 = dimension_writer_reference_id($dimension2);
    if ($account === false || $dimension === false || $dimension2 === false
        || (!$remove && (!is_numeric($amount) || !is_finite((float)$amount)))) return false;
    $date = date2sql($date_);
    $where = 'account='.db_escape($account).' AND tran_date='.db_escape($date)
        .' AND dimension_id='.$dimension.' AND dimension2_id='.$dimension2;
    $next = array('dimension_id'=>$dimension, 'dimension2_id'=>$dimension2);
    try {
        if (!begin_transaction()) return false;
        // Locking reads see committed current state even after an old MVCC read.
        // This also resolves the required retained column before any mutation.
        $result = db_query('SELECT id, custom_data FROM '.TB_PREF.'budget_trans WHERE '.$where.' ORDER BY id FOR UPDATE');
        if (!$result) { cancel_transaction(); return false; }
        $rows = array();
        while ($row = db_fetch($result)) $rows[] = $row;
        if (!$rows && !$remove) {
            $ok = dimension_ordinary_master_write('budget_trans', null, $next,
                function($admitted) use ($date, $account, $amount) {
                    return db_query('INSERT INTO '.TB_PREF.'budget_trans (tran_date, account, dimension_id, dimension2_id, amount, memo_) VALUES ('
                        .db_escape($date).', '.db_escape($account).', '.$admitted['dimension_id'].', '.$admitted['dimension2_id'].', '.db_escape($amount).", '')");
                });
            if (!$ok) { cancel_transaction(); return false; }
        }
        foreach ($rows as $row) {
            $id = dimension_writer_reference_id($row['id']);
            $ok = dimension_ordinary_master_write('budget_trans', $id, $next,
                function($admitted) use ($id, $amount, $remove) {
                    return db_query($remove ? 'DELETE FROM '.TB_PREF.'budget_trans WHERE id='.$id
                        : 'UPDATE '.TB_PREF.'budget_trans SET amount='.db_escape($amount).' WHERE id='.$id);
                }, false, $remove);
            if (!$ok) { cancel_transaction(); return false; }
        }
        if (!commit_transaction()) { cancel_transaction(); return false; }
        return true;
    } catch (Exception $exception) {
        cancel_transaction(); return false;
    } catch (Throwable $exception) {
        cancel_transaction(); return false;
    }
}
