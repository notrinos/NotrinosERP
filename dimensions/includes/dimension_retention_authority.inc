<?php
/**
 * DIM-LFC-001 B07: retention authority for destructive helpers (fiscal-year purge
 * and source-row deletion).
 *
 * A destructive helper must never erase or rewrite a retained Dimension
 * reference. This file answers one question for a set of rows that a helper is
 * about to destroy: does any of them carry a Dimension reference (a native
 * scalar coordinate, a decoded ordinary custom_data coordinate, or evidence
 * that cannot be decoded)? Terminal, zero-amount, voided and orphan rows count
 * exactly like live ones; only rows that provably carry no coordinate are clear.
 * Anything unreadable, unregistered, schema-drifted or protected-with-a-coordinate
 * is unknown and blocks. Nothing here authorizes a deletion or rewrites history:
 * the only outcomes are "clear" (the caller's own rules still apply) and "blocked".
 *
 * Reuses the B02/B06 authorities: the registered owner columns come from
 * dimension_reference_recheck_owners(), ordinary JSON decodes through the
 * canonical projection, and shared rows are assigned to their domain family by
 * dimension_shared_record_family(). No payload is ever returned or logged; the
 * result names stores and states only.
 */
require_once __DIR__.'/dimension_reference_recheck.inc';

/**
 * The stores the fiscal-year purge destroys or rewrites, with the exact row set
 * the purge selects by date. The selection is a deliberate superset of what the
 * purge deletes (it ignores the allocation and quantity completion tests), so a
 * row the purge would keep is never a reason to allow it to run.
 *
 * Placeholder {to} is the validated closing date of the purged fiscal year.
 *
 * @return array store => array(from => FROM clause with t as the owner alias, where => predicate)
 */
function dimension_retention_purge_stores()
{
    $p = TB_PREF;
    return array(
        'gl_trans'=>array('from'=>$p.'gl_trans t', 'where'=>"t.tran_date <= '{to}'"),
        'bank_trans'=>array('from'=>$p.'bank_trans t', 'where'=>"t.trans_date <= '{to}'"),
        'budget_trans'=>array('from'=>$p.'budget_trans t', 'where'=>"t.tran_date <= '{to}'"),
        'debtor_trans'=>array('from'=>$p.'debtor_trans t', 'where'=>"t.tran_date <= '{to}'"),
        'debtor_trans_details'=>array('from'=>$p.'debtor_trans_details t INNER JOIN '.$p.'debtor_trans p ON p.trans_no=t.debtor_trans_no AND p.type=t.debtor_trans_type',
            'where'=>"p.tran_date <= '{to}'"),
        'sales_orders'=>array('from'=>$p.'sales_orders t', 'where'=>"t.ord_date <= '{to}' AND t.type <> 1"),
        'sales_order_details'=>array('from'=>$p.'sales_order_details t INNER JOIN '.$p.'sales_orders p ON p.order_no=t.order_no AND p.trans_type=t.trans_type',
            'where'=>"p.ord_date <= '{to}' AND p.type <> 1"),
        'purch_orders'=>array('from'=>$p.'purch_orders t', 'where'=>"t.ord_date <= '{to}'"),
        'purch_order_details'=>array('from'=>$p.'purch_order_details t INNER JOIN '.$p.'purch_orders p ON p.order_no=t.order_no',
            'where'=>"p.ord_date <= '{to}'"),
        'grn_batch'=>array('from'=>$p.'grn_batch t', 'where'=>"t.delivery_date <= '{to}'"),
        'grn_items'=>array('from'=>$p.'grn_items t INNER JOIN '.$p.'grn_batch p ON p.id=t.grn_batch_id', 'where'=>"p.delivery_date <= '{to}'"),
        'supp_trans'=>array('from'=>$p.'supp_trans t', 'where'=>"t.tran_date <= '{to}'"),
        'supp_invoice_items'=>array('from'=>$p.'supp_invoice_items t INNER JOIN '.$p.'supp_trans p ON p.trans_no=t.supp_trans_no AND p.type=t.supp_trans_type',
            'where'=>"p.tran_date <= '{to}'"),
        'workorders'=>array('from'=>$p.'workorders t', 'where'=>"t.released_date <= '{to}' AND t.closed = 1"),
        'wo_issue_items'=>array('from'=>$p.'wo_issue_items t INNER JOIN '.$p.'wo_issues i ON i.issue_no=t.issue_id INNER JOIN '.$p.'workorders p ON p.id=i.workorder_id',
            'where'=>"p.released_date <= '{to}' AND p.closed = 1"),
        'stock_moves'=>array('from'=>$p.'stock_moves t', 'where'=>"t.tran_date <= '{to}'"));
}

/**
 * Shared stores the purge destroys, keyed by store, with the discriminator that
 * assigns each row to its domain family. The Dimension's own identity-bound
 * evidence (type ST_DIMENSION) is never in the selection: the purge preserves it.
 *
 * @return array store => array(from, where, type column)
 */
function dimension_retention_purge_shared_stores()
{
    $p = TB_PREF;
    $own = defined('ST_DIMENSION') ? (int)ST_DIMENSION : 0;
    return array(
        'audit_trail'=>array('from'=>$p.'audit_trail t', 'where'=>"t.gl_date <= '{to}' AND t.type <> ".$own, 'type'=>'type'),
        'attachments'=>array('from'=>$p.'attachments t', 'where'=>'t.type_no <> '.$own, 'type'=>'type_no'));
}

/**
 * Classify one retained custom_data value of a row that is about to be destroyed.
 *
 * @param string $family ordinary (decode the canonical projection) or any other family
 * @param mixed $raw Raw custom_data column value
 * @return string clear, retained or unknown
 */
function dimension_retention_payload_state($family, $raw)
{
    if ($family === 'ordinary') {
        $references = dimension_master_payload_references($raw);
        if ($references === false) return 'unknown';
        return count($references) ? 'retained' : 'clear';
    }
    if ($raw === null || $raw === '') return 'clear';
    if (!is_string($raw)) return 'unknown';
    $payload = json_decode($raw, true);
    if (!is_array($payload) || json_last_error() !== JSON_ERROR_NONE) return 'unknown';
    if (!count($payload)) return 'clear';
    // Protected families deny a coordinate-like key; an unassigned family with any content stays unknown.
    if ($family === 'hrm' || $family === 'payroll')
        return dimension_shared_record_has_coordinate_key($payload) === false ? 'clear' : 'unknown';
    return 'unknown';
}

/**
 * Classify one registered owner's rows selected by a destructive helper.
 *
 * @param string $table Unprefixed registered owner table
 * @param array $columns Live schema map of the owner
 * @param array $scalar Registered scalar Dimension columns of the owner
 * @param bool $retained Whether the owner retains custom_data
 * @param string $from FROM clause (owner alias t, optional joined parents)
 * @param string $where Row selection
 * @param bool $lock Take shared locks that last until the caller's transaction ends
 * @return string clear, retained or unknown
 */
function dimension_retention_owner_state($table, $columns, $scalar, $retained, $from, $where, $lock)
{
    if (!dimension_reference_recheck_schema($columns, $scalar, $retained)) return 'unknown';
    $suffix = $lock ? ' LOCK IN SHARE MODE' : '';
    if (count($scalar)) {
        $conditions = array();
        foreach ($scalar as $column) $conditions[] = 'IFNULL(t.`'.$column.'`,0)<>0';
        $rows = db_query('SELECT 1 FROM '.$from.' WHERE ('.$where.') AND ('.implode(' OR ', $conditions).') LIMIT 1'.$suffix);
        if (!$rows) return 'unknown';
        if (db_num_rows($rows)) return 'retained';
    }
    if (!$retained) return 'clear';
    $rows = db_query('SELECT t.custom_data FROM '.$from.' WHERE ('.$where.')'.$suffix);
    if (!$rows) return 'unknown';
    $state = 'clear';
    while ($row = db_fetch_row($rows)) {
        $status = dimension_retention_payload_state('ordinary', $row[0]);
        if ($status === 'retained') return 'retained';
        if ($status === 'unknown') $state = 'unknown';
    }
    return $state;
}

/**
 * Classify the shared-store rows a destructive helper is about to destroy, by
 * domain family of each row.
 *
 * @return string clear, retained or unknown
 */
function dimension_retention_shared_state($table, $columns, $spec, $lock)
{
    if (!dimension_reference_recheck_schema($columns, array(), true) || !isset($columns[$spec['type']])) return 'unknown';
    $rows = db_query('SELECT t.`'.$spec['type'].'`, t.custom_data FROM '.$spec['from'].' WHERE ('.$spec['where'].')'.($lock ? ' LOCK IN SHARE MODE' : ''));
    if (!$rows) return 'unknown';
    $state = 'clear';
    while ($row = db_fetch_row($rows)) {
        $status = dimension_retention_payload_state(dimension_shared_record_family($table, $row[0]), $row[1]);
        if ($status === 'retained') return 'retained';
        if ($status === 'unknown') $state = 'unknown';
    }
    return $state;
}

/**
 * Assess the fiscal-year purge up to and including one closing date.
 *
 * With $lock the reads hold shared locks until the caller's transaction ends, so
 * no writer can add a reference to a selected row between this answer and the
 * purge; the caller must own an open transaction. Without it the answer is an
 * early, lock-free refusal that must be repeated under lock before any change.
 * Any failure, drift, undecodable value or missing store is blocked.
 *
 * @param string $to Closing date of the purged fiscal year, Y-m-d
 * @param bool $lock Use shared locking reads inside the caller's transaction
 * @return array state clear|blocked, stores name=>clear|retained|unknown; no payload
 */
function dimension_retention_purge_assess($to, $lock = false)
{
    $result = array('state'=>'blocked', 'stores'=>array());
    $stores = dimension_retention_purge_stores();
    $shared = dimension_retention_purge_shared_stores();
    foreach (array_merge(array_keys($stores), array_keys($shared)) as $name) $result['stores'][$name] = 'unknown';
    if (!is_string($to) || !preg_match('/^[0-9]{4}-(?:0[1-9]|1[0-2])-(?:0[1-9]|[12][0-9]|3[01])$/D', $to)) return $result;
    if ($lock && empty($GLOBALS['transaction_level'])) return $result;
    try {
        $owners = dimension_reference_recheck_owners();
        $scalar_only = dimension_reference_recheck_scalar_only();
        $columns = dimension_reference_recheck_columns(array_merge(array_keys($stores), array_keys($shared)));
        if ($columns === false) return $result;
        foreach ($stores as $name=>$spec) {
            if (!isset($owners[$name])) continue;
            $result['stores'][$name] = dimension_retention_owner_state($name, $columns[$name], $owners[$name],
                !in_array($name, $scalar_only, true), str_replace('{to}', $to, $spec['from']), str_replace('{to}', $to, $spec['where']), $lock);
            if ($result['stores'][$name] === 'unknown' && db_error_no()) return $result;
        }
        foreach ($shared as $name=>$spec) {
            $spec['where'] = str_replace('{to}', $to, $spec['where']);
            $result['stores'][$name] = isset($columns[$name]) ? dimension_retention_shared_state($name, $columns[$name], $spec, $lock) : 'unknown';
            if ($result['stores'][$name] === 'unknown' && db_error_no()) return $result;
        }
    } catch (Exception $exception) {
        return $result;
    }
    $result['state'] = count(array_diff($result['stores'], array('clear'))) ? 'blocked' : 'clear';
    return $result;
}

/**
 * Names of the stores that block, for an operator message (no values, no counts).
 *
 * @param array $assessment Result of dimension_retention_purge_assess()
 * @return string[]
 */
function dimension_retention_blocking_stores($assessment)
{
    $names = array();
    foreach ($assessment['stores'] as $name=>$state) if ($state !== 'clear') $names[] = $name;
    return $names;
}

/**
 * Admit a fiscal-year purge: the early lock-free check when $lock is false, the
 * binding check under shared locks when true. Displays the blocking stores.
 *
 * @param string $to Closing date, Y-m-d
 * @param bool $lock True inside the purge transaction
 * @return bool True only when no destroyed row retains a Dimension reference
 */
function dimension_retention_purge_admit($to, $lock)
{
    $assessment = dimension_retention_purge_assess($to, $lock);
    if ($assessment['state'] === 'clear') return true;
    display_error(_('This fiscal year cannot be deleted because deleting it would erase retained Dimension references or evidence that cannot be verified. Blocking records: ')
        .implode(', ', dimension_retention_blocking_stores($assessment)));
    return false;
}

/**
 * Admit the deletion of ordinary owner rows that carry a retained reference: the
 * delete must not erase a native scalar coordinate or a retained custom_data
 * coordinate. Locking read inside the caller's transaction; refused outside one.
 *
 * @param string $table Unprefixed registered ordinary owner
 * @param string $where Predicate selecting the rows to delete (owner alias t); built by the caller from escaped values
 * @return bool True only when every selected row is clear
 */
function dimension_retention_row_delete_admit($table, $where)
{
    if (empty($GLOBALS['transaction_level'])) return false;
    $owners = dimension_reference_recheck_owners();
    if (!isset($owners[$table])) return false;
    try {
        $columns = dimension_reference_recheck_columns(array($table));
        if ($columns === false) return false;
        $retained = !in_array($table, dimension_reference_recheck_scalar_only(), true);
        return dimension_retention_owner_state($table, $columns[$table], $owners[$table], $retained, TB_PREF.$table.' t', $where, true) === 'clear';
    } catch (Exception $exception) {
        return false;
    }
}

/**
 * Admit the deletion of shared-store rows (attachments): a row whose retained
 * custom_data still carries a Dimension coordinate, or whose payload cannot be
 * verified, is retained evidence and is never deleted. The row's own file must
 * not be removed before this answers true. Lock-free is sufficient: the B06
 * writer fence admits no new coordinate into a shared payload, so a clear row
 * cannot become referenced.
 *
 * @param string $table Shared store with retained JSON (attachments)
 * @param string $where Predicate selecting the rows to delete (owner alias t), built from escaped values
 * @return bool True only when every selected row is clear
 */
function dimension_retention_shared_row_delete_admit($table, $where)
{
    $stores = dimension_shared_record_stores();
    if (!isset($stores[$table])) return false;
    try {
        $columns = dimension_reference_recheck_columns(array($table));
        if ($columns === false) return false;
        return dimension_retention_shared_state($table, $columns[$table],
            array('from'=>TB_PREF.$table.' t', 'where'=>$where, 'type'=>$stores[$table]), false) === 'clear';
    } catch (Exception $exception) {
        return false;
    }
}

/**
 * Admit the deletion of one attachment by identity, before its file is removed.
 *
 * @param int|string $attachment_id Attachment identity
 * @return bool True when the row may be deleted
 */
function dimension_retention_attachment_delete_admit($attachment_id)
{
    if (!is_scalar($attachment_id) || !preg_match('/^[1-9][0-9]*$/D', (string)$attachment_id)) return false;
    return dimension_retention_shared_row_delete_admit('attachments', 't.id='.(int)$attachment_id);
}
