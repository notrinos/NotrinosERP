<?php
/** Persist standalone movement custody on the existing immutable core action trail. */
require_once __DIR__.'/warehouse_transfer_terminal_db.inc';

/**
 * Admit a new stock event against current committed accounting settings.
 * Apply the native GL closing-date/current-year/multiyear policy using locking
 * reads: cached company preferences and an earlier page check cannot authorize
 * a posting after another connection closes its period. Historical receipt
 * verification deliberately does not call this command-only gate.
 */
function transfer_fulfillment_period_allowed($date)
{
    if (!transfer_core_context() || empty($GLOBALS['transaction_level'])
        || !is_string($date) || !preg_match('/^([0-9]{4})-([0-9]{2})-([0-9]{2})$/D', $date, $parts)
        || (int)$parts[1] < 1000 || !checkdate((int)$parts[2], (int)$parts[3], (int)$parts[1])) return false;
    $rows = transfer_core_rows('SELECT name,value FROM '.TB_PREF."sys_prefs WHERE name IN ('f_year','gl_closing_date') ORDER BY name FOR UPDATE");
    if (!$rows || count($rows) !== 2) return false;
    $prefs = array();
    foreach ($rows as $row) $prefs[$row['name']] = $row['value'];
    if (!isset($prefs['f_year'], $prefs['gl_closing_date'])
        || !dimension_writer_reference_id($prefs['f_year'])) return false;
    $closing = $prefs['gl_closing_date'];
    // Empty is the canonical install's explicit "no closing date" setting.
    if ($closing !== '' && (!preg_match('/^([0-9]{4})-([0-9]{2})-([0-9]{2})$/D', $closing, $parts)
        || (int)$parts[1] < 1000 || !checkdate((int)$parts[2], (int)$parts[3], (int)$parts[1])
        || $date <= $closing)) return false;
    $years = transfer_core_rows('SELECT id,closed FROM '.TB_PREF.'fiscal_year WHERE begin<='
        .db_escape($date).' AND end>='.db_escape($date).' ORDER BY id FOR UPDATE');
    // Ambiguous/absent or closed years cannot acquire new posting authority.
    return $years && count($years) === 1 && (string)$years[0]['closed'] === '0'
        && ((int)$years[0]['id'] === (int)$prefs['f_year']
            || $_SESSION['wa_current_user']->can_access('SA_MULTIFISCALYEARS'));
}

/** Load exact current rows; the caller owns Material/Transfer family locks. */
function transfer_fulfillment_source($identity)
{
    if (!transfer_core_context() || !($identity = dimension_writer_reference_id($identity))) return false;
    $headers = transfer_core_rows('SELECT * FROM '.TB_PREF.'wh_transfer_orders WHERE transfer_id='.$identity.' FOR UPDATE');
    $lines = transfer_core_rows('SELECT * FROM '.TB_PREF.'wh_transfer_order_lines WHERE transfer_id='.$identity.' ORDER BY line_id FOR UPDATE');
    return $headers && count($headers) === 1 && $lines ? array('header'=>$headers[0], 'lines'=>$lines) : false;
}

/** Compare DB/JSON snapshots without PHP driver numeric-type differences. */
function transfer_fulfillment_equal($left, $right)
{
    return is_array($left) && is_array($right)
        && transfer_core_source_fingerprint($left) === transfer_core_source_fingerprint($right);
}

/** Bind every raw movement column, including retained JSON, rather than amounts alone. */
function transfer_fulfillment_movement_hash($moves)
{
    if (!transfer_core_context() || empty($GLOBALS['transaction_level']) || !is_array($moves) || !$moves) return false;
    $ids = array();
    foreach ($moves as $id) {
        if (!dimension_writer_reference_id($id)) return false;
        $ids[] = (int)$id;
    }
    $rows = transfer_core_rows('SELECT * FROM '.TB_PREF.'stock_moves WHERE trans_id IN ('
        .implode(',', $ids).') ORDER BY trans_id FOR UPDATE');
    return $rows && count($rows) === count($ids)
        ? transfer_core_source_fingerprint(array('header'=>array(), 'lines'=>$rows)) : false;
}

/** Validate an exact source transition and its actual, locked movement identities. */
function transfer_fulfillment_transition($before, $after, $event, $actor, $date, $moves)
{
    if (!transfer_core_context() || empty($GLOBALS['transaction_level'])
        || !is_array($before) || !is_array($after) || !isset($before['header'], $before['lines'], $after['header'], $after['lines'])
        || !is_array($moves) || !$moves || !dimension_writer_reference_id($actor)
        || !is_string($date) || !preg_match('/^([0-9]{4})-([0-9]{2})-([0-9]{2})$/D', $date, $parts)
        || !checkdate((int)$parts[2], (int)$parts[3], (int)$parts[1])
        || !in_array($event, array('dispatch','receipt'), true)
        || count($before['lines']) !== count($after['lines'])) return false;
    $dispatch = $event === 'dispatch';
    $expected = $before; $outstanding = false; $used = array(); $trans_no = null;
    if ($dispatch ? $before['header']['status'] !== 'approved'
        : !in_array($before['header']['status'], array('in_transit','shipped'), true)) return false;
    $minimum = $dispatch ? $before['header']['request_date'] : $before['header']['actual_ship_date'];
    if (!is_string($minimum) || $date < $minimum) return false;
    foreach ($before['lines'] as $index=>$line) {
        $next = $after['lines'][$index];
        $id = (int)$line['line_id'];
        // Native quantities are DECIMAL(15,4); subtract at that same precision.
        $qty = $dispatch ? (float)$line['qty_requested'] : round((float)$next['qty_received'] - (float)$line['qty_received'], 4);
        if (!is_finite($qty) || $qty < 0 || ($dispatch && $qty <= 0)
            || ($dispatch && (float)$next['qty_shipped'] !== $qty)
            || (!$dispatch && (float)$next['qty_received'] > (float)$line['qty_shipped'])) return false;
        $expected['lines'][$index][$dispatch ? 'qty_shipped' : 'qty_received'] = $next[$dispatch ? 'qty_shipped' : 'qty_received'];
        if (!$dispatch && (float)$next['qty_received'] < (float)$line['qty_shipped']) $outstanding = true;
        if ($qty == 0) { if (isset($moves[$id])) return false; continue; }
        if (!isset($moves[$id]) || !dimension_writer_reference_id($moves[$id]) || isset($used[$moves[$id]])) return false;
        $used[$moves[$id]] = true;
        $rows = transfer_core_rows('SELECT * FROM '.TB_PREF.'stock_moves WHERE trans_id='.(int)$moves[$id].' FOR UPDATE');
        if (!$rows || count($rows) !== 1) return false;
        $move = $rows[0];
        if ((int)$move['type'] !== ST_LOCTRANSFER || !dimension_writer_reference_id($move['trans_no'])
            || ($trans_no !== null && $trans_no !== (int)$move['trans_no'])
            || $move['stock_id'] !== $line['stock_id'] || $move['tran_date'] !== $date
            || $move['loc_code'] !== $before['header'][$dispatch ? 'from_loc_code' : 'to_loc_code']
            || (float)$move['qty'] !== ($dispatch ? -$qty : $qty)
            || (float)$move['standard_cost'] !== (float)$line['unit_cost'] || (float)$move['price'] !== 0.0
            || $move['stock_status'] !== ($dispatch ? 'in_transit' : 'available')
            || $move['reference'] !== ($before['header']['reference'] ? $before['header']['reference'] : 'TO-'.$before['header']['transfer_no'])) return false;
        foreach (array('serial_id','batch_id', $dispatch ? 'from_bin_id' : 'to_bin_id') as $field)
            if ((int)$move[$field] !== (int)$line[$field]) return false;
        $trans_no = (int)$move['trans_no'];
    }
    if (count($used) !== count($moves) || $trans_no === null) return false;
    // A transaction identity must contain exactly this event's moves, not a
    // colliding generic location transfer or an extra unbound line.
    $all = transfer_core_rows('SELECT trans_id FROM '.TB_PREF.'stock_moves WHERE type='.ST_LOCTRANSFER
        .' AND trans_no='.$trans_no.' ORDER BY trans_id FOR UPDATE');
    if (!$all || count($all) !== count($used)) return false;
    foreach ($all as $row) if (!isset($used[$row['trans_id']])) return false;
    $expected['header']['status'] = $dispatch || $outstanding ? 'in_transit' : 'received';
    if ($dispatch || !$outstanding) {
        $expected['header'][$dispatch ? 'shipped_by' : 'received_by'] = $actor;
        $expected['header'][$dispatch ? 'actual_ship_date' : 'actual_recv_date'] = $date;
        $expected['header']['updated_at'] = $after['header']['updated_at'];
    }
    return transfer_fulfillment_equal($expected, $after);
}

/** Verify the full persisted prefix before accepting another receipt; no legacy inference. */
function transfer_fulfillment_custody($identity)
{
    if (!transfer_core_context() || empty($GLOBALS['transaction_level'])
        || !($identity = dimension_writer_reference_id($identity)) || !transfer_fulfillment_origin_allowed($identity)) return false;
    $history = transfer_core_history($identity); $source = transfer_fulfillment_source($identity);
    if (!$history || !$source) return false;
    $prefix = array('drafts'=>$history['drafts'], 'actions'=>array()); $current = null; $draft = null;
    $approval = get_approval_workflow_service(); $seen = array(); $last_date = null;
    foreach ($history['actions'] as $action) {
        if ($current === null && $action['action_type'] !== 'stock_dispatch') {
            $prefix['actions'][] = $action; continue;
        }
        $event = $current === null ? 'dispatch' : 'receipt';
        if ($action['action_type'] !== 'stock_'.$event || $action['draft_data_snapshot'] === null) return false;
        $receipt = $approval->decodeStoredDraftData($action['draft_data_snapshot']);
        if (!is_array($receipt) || array_keys($receipt) !== array('schema','transfer_id','company_sha256',
            'history_sha256','action_sha256','date','before','after','moves','movement_sha256')
            || $receipt['schema'] !== 'transfer-stock-'.$event.'-v1' || $receipt['transfer_id'] !== $identity
            || $receipt['company_sha256'] !== transfer_core_context(true)) return false;
        $unsigned = $action; $unsigned['draft_data_snapshot'] = null;
        if ($receipt['history_sha256'] !== transfer_core_source_fingerprint(array('header'=>array(),
                'lines'=>array_merge($prefix['drafts'], $prefix['actions'])))
            || $receipt['action_sha256'] !== transfer_core_source_fingerprint(array('header'=>$unsigned, 'lines'=>array()))) return false;
        if ($current === null) {
            $approved = transfer_approved_projection_custody($identity, $receipt['before'], $prefix, null);
            if (!$approved) return false;
            $draft = $approved['draft'];
        } elseif (!transfer_fulfillment_equal($current, $receipt['before'])) return false;
        $prior_action = end($prefix['actions']);
        if (!$prior_action || $action['action_date'] < $prior_action['action_date']
            || ($last_date !== null && $receipt['date'] < $last_date)
            || (int)$action['draft_id'] !== (int)$draft['id'] || (int)$action['level'] !== (int)$draft['current_level']
            || $receipt['movement_sha256'] !== transfer_fulfillment_movement_hash($receipt['moves'])
            || !transfer_fulfillment_transition($receipt['before'], $receipt['after'], $event,
                $action['user_id'], $receipt['date'], $receipt['moves'])) return false;
        foreach ($receipt['moves'] as $move) { if (isset($seen[$move])) return false; $seen[$move] = true; }
        $current = $receipt['after']; $prefix['actions'][] = $action; $last_date = $receipt['date'];
    }
    return $current !== null && transfer_fulfillment_equal($current, $source)
        ? array('source'=>$source, 'history'=>$history, 'draft'=>$draft) : false;
}

/** Append and independently reread a movement receipt in the owning stock transaction. */
function transfer_fulfillment_record($identity, $custody, $event, $date, $moves)
{
    $after = transfer_fulfillment_source($identity); $before = $custody['source'];
    $history = transfer_core_history($identity);
    if (!$after || $history !== $custody['history'] || !transfer_fulfillment_transition($before, $after, $event,
        $_SESSION['wa_current_user']->user, $date, $moves)) return false;
    $id = add_approval_action($custody['draft']['id'], 'stock_'.$event, null,
        $custody['draft']['current_level'], null, null, null);
    if (!$id) return false;
    $rows = transfer_core_rows('SELECT * FROM '.TB_PREF.'approval_actions WHERE id='.(int)$id.' FOR UPDATE');
    if (!$rows || count($rows) !== 1) return false;
    $receipt = array('schema'=>'transfer-stock-'.$event.'-v1', 'transfer_id'=>$identity,
        'company_sha256'=>transfer_core_context(true),
        'history_sha256'=>transfer_core_source_fingerprint(array('header'=>array(),
            'lines'=>array_merge($history['drafts'], $history['actions']))),
        'action_sha256'=>transfer_core_source_fingerprint(array('header'=>$rows[0], 'lines'=>array())),
        'date'=>$date, 'before'=>$before, 'after'=>$after, 'moves'=>$moves,
        'movement_sha256'=>transfer_fulfillment_movement_hash($moves));
    $snapshot = json_encode($receipt);
    if ($snapshot === false || !db_query('UPDATE '.TB_PREF.'approval_actions SET draft_data_snapshot='
        .db_escape($snapshot, true).' WHERE id='.(int)$id.' AND draft_data_snapshot IS NULL')
        || db_num_affected_rows() !== 1) return false;
    $verified = transfer_fulfillment_custody($identity);
    return $verified && count($verified['history']['actions']) === count($history['actions'])+1;
}
