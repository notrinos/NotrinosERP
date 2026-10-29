<?php
/** Requisition PO source custody through the final shared-core business boundary. */

/** Bind source admission to current native permissions, actor, company and connection. */
function requisition_po_completion_context()
{
    global $db, $db_connections;
    if (!requisition_purchase_authorized()) return false;
    $actor = $_SESSION['wa_current_user'];
    return hash('sha256', serialize(array($actor->user, $actor->access, $actor->cur_con,
        $db_connections[$actor->cur_con], is_object($db) ? spl_object_hash($db) : $db)));
}

/** One action owns one expected result; rollback and outer commit discard it. */
function requisition_po_completion($operation, $value = null)
{
    static $state = null;
    if ($operation === 'set') { $state = $value; return true; }
    if ($operation === 'peek') return $state;
    $saved = $state; $state = null;
    return $operation === 'take' ? $saved : true;
}

/** Release incomplete and intermediate approval custody at the transaction boundary. */
function release_requisition_po_completion()
{
    return requisition_po_completion('clear');
}

/** Capture raw retained rows, including every pair owner and prior native order. */
function read_requisition_po_completion_source($data)
{
    $id = (int)$data['requisition_conversion']['requisition_id'];
    $queries = array(
        'header'=>'SELECT * FROM '.TB_PREF.'purch_requisitions WHERE id='.$id.' FOR UPDATE',
        'lines'=>'SELECT * FROM '.TB_PREF.'purch_requisition_lines WHERE requisition_id='.$id.' ORDER BY id FOR UPDATE',
        'orders'=>'SELECT * FROM '.TB_PREF.'purch_orders WHERE requisition_id='.$id.' ORDER BY order_no FOR UPDATE',
        'details'=>'SELECT detail.* FROM '.TB_PREF.'purch_order_details detail INNER JOIN '.TB_PREF.'purch_orders po ON po.order_no=detail.order_no WHERE po.requisition_id='.$id.' ORDER BY detail.po_detail_item FOR UPDATE');
    $rows = array();
    foreach ($queries as $key=>$query) $rows[$key] = read_requisition_purchase_rows($query);
    // Rejection/cancellation can retain an independently changed or removed source.
    $material = $rows['header'] ? (int)$rows['header'][0]['material_request_id'] : 0;
    if ($material) {
        if (!$_SESSION['wa_current_user']->can_access('SA_MATERIALREQUEST'))
            throw new RuntimeException('Linked Warehouse completion authority is unavailable.');
        $queries['pair'] = 'SELECT * FROM '.TB_PREF.'purch_requisitions WHERE material_request_id='.$material.' ORDER BY id FOR UPDATE';
        $queries['material'] = 'SELECT * FROM '.TB_PREF.'wh_material_requests WHERE request_id='.$material.' FOR UPDATE';
        $queries['material_lines'] = 'SELECT * FROM '.TB_PREF.'wh_material_request_lines WHERE request_id='.$material.' ORDER BY line_id FOR UPDATE';
        foreach (array('pair','material','material_lines') as $key) $rows[$key] = read_requisition_purchase_rows($queries[$key]);
    }
    return array('queries'=>$queries, 'rows'=>$rows);
}

/** Freeze raw family histories without decoding, rewriting or inferring old approval. */
function read_requisition_po_completion_history($draft_id, $data)
{
    $json = approval_draft_current_json_sql();
    $scope = '(trans_type IN ('.ST_PURCHREQUISITION.','.ST_WH_MATERIAL_REQUEST.') OR id='.(int)$draft_id
        .' OR (trans_type='.ST_PURCHORDER.' AND CASE WHEN JSON_VALID('.$json.') THEN '
        ."JSON_UNQUOTE(JSON_EXTRACT(".$json.", '$.requisition_conversion.requisition_id'))="
        .db_escape((string)$data['requisition_conversion']['requisition_id']).' ELSE 1 END))';
    return array(
        'drafts'=>read_requisition_purchase_rows('SELECT * FROM '.TB_PREF.'approval_drafts WHERE '.$scope.' ORDER BY id FOR UPDATE'),
        'actions'=>read_requisition_purchase_rows('SELECT action.* FROM '.TB_PREF.'approval_actions action INNER JOIN '.TB_PREF.'approval_drafts draft ON draft.id=action.draft_id WHERE '.str_replace(' OR id=', ' OR draft.id=', $scope).' ORDER BY action.id FOR UPDATE'));
}

/** Lock this action's notification rows and reserved identity as part of core evidence. */
function read_requisition_po_completion_effects($draft)
{
    return array(
        'notifications'=>read_requisition_purchase_rows('SELECT * FROM '.TB_PREF.'approval_notifications WHERE draft_id='
            .(int)$draft['id'].' ORDER BY id FOR UPDATE'),
        'reservation'=>read_requisition_purchase_rows('SELECT * FROM '.TB_PREF.'voided WHERE type='
            .(int)$draft['trans_type'].' AND id='.(int)$draft['reserved_trans_no'].' FOR UPDATE'));
}

/** Start completion custody after core and native source admission, before core effects. */
function prepare_requisition_po_completion($action, $data, $draft, $actor, $comments, $snapshot = null)
{
    requisition_po_completion('clear');
    $context = requisition_po_completion_context();
    if (!$draft || (int)$draft['status'] !== APPROVAL_STATUS_PENDING || !$context || !in_array($action, array('approve','reject','cancel'), true)
        || !register_transaction_resource_release('requisition-po-completion', 'release_requisition_po_completion')) return false;
    $audit = read_requisition_purchase_rows('SELECT '.db_escape($comments, true).' AS comments, '
        .db_escape(isset($_SERVER['REMOTE_ADDR']) ? $_SERVER['REMOTE_ADDR'] : '').' AS ip, NOW() AS started');
    if (!$audit) return false;
    $native_rows = array();
    if ($snapshot) foreach ($snapshot['queries'] as $key=>$query)
        $native_rows[$key] = read_requisition_purchase_rows($query);
    return requisition_po_completion('set', array('action'=>$action, 'draft'=>$draft, 'actor'=>$actor,
        'comments'=>$comments, 'context'=>$context, 'data'=>$data, 'audit'=>$audit[0],
        'source'=>read_requisition_po_completion_source($data),
        'history'=>read_requisition_po_completion_history($draft['id'], $data), 'terminal'=>null, 'intent'=>null, 'cart'=>null,
        'effects'=>read_requisition_po_completion_effects($draft),
        'snapshot'=>$snapshot, 'native_rows'=>$native_rows));
}

/** Native primary-key admission rechecks the same core action without replacing its history. */
function revalidate_requisition_po_completion($data, $draft, $actor)
{
    $state = requisition_po_completion('peek');
    return $state && $state['action'] === 'approve' && $state['terminal'] === null
        && $state['data'] === $data && $state['actor'] === $actor
        && (int)$state['draft']['id'] === (int)$draft['id']
        && $state['context'] === requisition_po_completion_context()
        && $state['source'] === read_requisition_po_completion_source($data);
}

/** Retain only the already reconciled native effect; an executor call cannot invent admission. */
function finish_requisition_po_native_write($snapshot, $number, $draft_id, $intent)
{
    $state = requisition_po_completion('peek');
    if (!$state || $state['action'] !== 'approve' || (int)$state['draft']['id'] !== (int)$draft_id
        || $state['context'] !== requisition_po_completion_context()
        || (int)$state['data']['requisition_conversion']['requisition_id'] !== (int)$snapshot['header']['id'])
        throw new RuntimeException('Requisition purchase core completion custody is unavailable.');
    $state['terminal'] = read_requisition_po_completion_source($state['data']);
    if (isset($state['source']['rows']['pair'])
        && ($state['source']['rows']['pair'] !== $state['source']['rows']['header']
            || $state['terminal']['rows']['pair'] !== $state['terminal']['rows']['header']))
        throw new RuntimeException('Requisition purchase pair ownership changed.');
    $state['snapshot'] = $snapshot;
    $state['native_rows'] = array();
    foreach ($snapshot['queries'] as $key=>$query)
        $state['native_rows'][$key] = read_requisition_purchase_rows($query);
    $state['intent'] = $intent; $state['cart'] = $snapshot['cart']; $state['number'] = (int)$number;
    requisition_po_completion('set', $state);
}

/** Reconcile source, native PO, immutable histories and the one attributable core action. */
function verify_requisition_purchase_order_completion($outcome, $draft, $actor, $comments)
{
    $state = requisition_po_completion('take');
    if (!$state || $state['draft'] !== $draft || $state['actor'] !== $actor || $state['comments'] !== $comments
        || !$state['context'] || $state['context'] !== requisition_po_completion_context()
        || !requisition_purchase_order_envelope_authorized($state['data'])) return false;
    $current = read_requisition_po_completion_source($state['data']);
    if ($outcome === 'approved' && $state['action'] === 'approve') {
        if (!$state['terminal'] || !$state['intent'] || $current !== $state['terminal']) return false;
        verify_agreement_purchase_order_effect($state['intent'], $state['cart'], $state['number']);
    } elseif ($state['terminal'] !== null || $current !== $state['source']) return false;
    if ($state['snapshot']) {
        foreach ($state['snapshot']['queries'] as $key=>$query)
            if (read_requisition_purchase_rows($query) !== $state['native_rows'][$key]) return false;
        $cart = $state['snapshot']['cart'];
        if (!$cart->tax_included && $state['snapshot']['tax_configuration'] !== read_agreement_purchase_tax_configuration(true)) return false;
        foreach ($state['snapshot']['pricing'] as $stock=>$inputs)
            if ($inputs !== read_requisition_purchase_pricing($cart->supplier_id, $stock, true)) return false;
    }
    $history = read_requisition_po_completion_history($draft['id'], $state['data']);
    if (count($history['drafts']) !== count($state['history']['drafts'])
        || count($history['actions']) !== count($state['history']['actions']) + 1) return false;
    $now = read_requisition_purchase_rows('SELECT NOW() AS now_');
    $found = false;
    foreach ($history['drafts'] as $key=>$row) {
        if ((int)$row['id'] !== (int)$draft['id']) continue;
        $found = true; $allowed = array();
        if (($outcome === 'approved' && $state['action'] === 'approve')
            || ($outcome === 'rejected' && $state['action'] === 'reject')
            || ($outcome === 'cancelled' && $state['action'] === 'cancel')) {
            $status = $outcome === 'approved' ? APPROVAL_STATUS_APPROVED
                : ($outcome === 'rejected' ? APPROVAL_STATUS_REJECTED : APPROVAL_STATUS_CANCELLED);
            if ((int)$row['status'] !== $status || !is_string($row['completed_date'])
                || $row['completed_date'] < $state['audit']['started'] || $row['completed_date'] > $now[0]['now_']) return false;
            $allowed = array('status','completed_date');
            if ($outcome === 'approved') {
                if ((int)$row['approved_trans_no'] !== $state['number']) return false;
                $allowed[] = 'approved_trans_no';
            }
        } elseif ($state['action'] === 'approve' && in_array($outcome, array('pending_next_level','pending_more_approvers'), true)) {
            if ($outcome === 'pending_next_level') {
                if ((int)$row['current_level'] !== (int)$draft['current_level'] + 1) return false;
                $allowed[] = 'current_level';
            }
        } else return false;
        foreach ($allowed as $field) $history['drafts'][$key][$field] = $state['history']['drafts'][$key][$field];
    }
    $added = array_pop($history['actions']);
    if (!$found || $history !== $state['history']) return false;
    if (in_array($outcome, array('approved','rejected','cancelled'), true)
        && !verify_requisition_po_completion_effects($state, $outcome, $now[0]['now_'])) return false;
    return (int)$added['draft_id'] === (int)$draft['id'] && $added['action_type'] === $state['action']
        && (int)$added['user_id'] === (int)$actor['user_id'] && (int)$added['level'] === (int)$draft['current_level']
        && $added['comments'] === $state['audit']['comments'] && $added['ip_address'] === $state['audit']['ip']
        && $added['draft_data_snapshot'] === null && $added['action_date'] >= $state['audit']['started']
        && $added['action_date'] <= $now[0]['now_'] && $state['context'] === requisition_po_completion_context();
}

/** Accept only core-owned reservation and notification transitions for terminal decisions. */
function verify_requisition_po_completion_effects($state, $outcome, $finished)
{
    $draft = $state['draft']; $current = read_requisition_po_completion_effects($draft);
    $expected_type = $outcome === 'approved' ? null : ($outcome === 'rejected' ? 'APPROVAL_REJECTED' : 'APPROVAL_CANCELLED');
    if ($outcome === 'approved') {
        if ($current['reservation']) return false;
    } else {
        if (count($state['effects']['reservation']) !== 1 || count($current['reservation']) !== 1) return false;
        $old = $state['effects']['reservation'][0]; $new = $current['reservation'][0];
        if ((int)$new['type'] !== (int)$old['type'] || (int)$new['id'] !== (int)$old['id']
            || $new['date_'] !== $old['date_'] || $new['memo_'] !== $expected_type) return false;
    }
    $before = $state['effects']['notifications']; $after = $current['notifications'];
    $expected_add = $outcome === 'approved' ? 'approved' : ($outcome === 'rejected' ? 'rejected' : null);
    if (count($after) !== count($before) + ($expected_add === null ? 0 : 1)) return false;
    $identity = (int)$draft['id'];
    foreach ($before as $index=>$row) {
        if (!isset($after[$index]) || (int)$after[$index]['id'] !== (int)$row['id']) return false;
        if ((int)$row['is_read'] === 0) {
            $read = $after[$index]['read_date'];
            if ((int)$after[$index]['is_read'] !== 1 || !is_string($read)
                || $read < $state['audit']['started'] || $read > $finished) return false;
            foreach (array('is_read','read_date') as $field) $row[$field] = $after[$index][$field];
        }
        if ($after[$index] !== $row) return false;
    }
    if ($expected_add !== null) {
        $added = $after[count($before)];
        if ((int)$added['id'] <= ($before ? (int)$before[count($before)-1]['id'] : 0)
            || (int)$added['draft_id'] !== $identity || (int)$added['user_id'] !== (int)$draft['submitted_by']
            || $added['notification_type'] !== $expected_add || (int)$added['is_read'] !== 1
            || (int)$added['is_sent'] !== 0 || !is_string($added['created_date'])
            || $added['created_date'] < $state['audit']['started'] || $added['created_date'] > $finished) return false;
        foreach (array('delivery_attempts'=>0,'attempt_limit'=>5,'repair_count'=>0,'last_error_code'=>'') as $field=>$value)
            if ($added[$field] !== (string)$value && $added[$field] !== $value) return false;
        foreach (array('last_repaired_by','last_repaired_at','next_attempt_at','lease_token','lease_expires_at',
            'last_attempt_at','dead_lettered_at','sent_date') as $field) if ($added[$field] !== null) return false;
        if (!is_string($added['read_date']) || $added['read_date'] < $state['audit']['started']
            || $added['read_date'] > $finished) return false;
    }
    return true;
}
