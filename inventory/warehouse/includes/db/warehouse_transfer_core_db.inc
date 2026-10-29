<?php
/** Transfer review uses the registered core, never local approval. */
require_once __DIR__.'/../../../../dimensions/includes/dimension_reference_writer_fence.inc';

/** Bind native admission to the current actor, role, company and connection. */
function transfer_core_context($portable = false)
{
    global $db, $db_connections;
    $actor = isset($_SESSION['wa_current_user']) ? $_SESSION['wa_current_user'] : null;
    if (!is_object($actor) || empty($actor->user) || !isset($actor->access, $actor->cur_con)
        || !isset($db_connections[$actor->cur_con]) || !method_exists($actor, 'can_access')
        || !$actor->can_access('SA_TRANSFERORDERS')) return false;
    $company = array($actor->cur_con, $db_connections[$actor->cur_con]);
    return hash('sha256', serialize($portable ? $company : array($company, $actor->user,
        $actor->access, is_object($db) ? spl_object_hash($db) : $db)));
}

/** Current associative rows, preserving exact intrinsic and opaque history. */
function transfer_core_rows($sql)
{
    $result = db_query($sql);
    if (!$result) return false;
    $rows = array();
    while ($row = db_fetch_assoc($result)) $rows[] = $row;
    return $rows;
}

/** Bind DB values without depending on PDO's PHP-version-specific numeric types. */
function transfer_core_source_fingerprint($source)
{
    $canonical = $source;
    foreach ($canonical['header'] as $field=>$value)
        if ($value !== null) $canonical['header'][$field] = (string)$value;
    foreach ($canonical['lines'] as $index=>$line)
        foreach ($line as $field=>$value)
            if ($value !== null) $canonical['lines'][$index][$field] = (string)$value;
    return hash('sha256', serialize($canonical));
}

/** Transaction-local admission and completion custody; never a session flag. */
function transfer_core_permit($operation, $value = null)
{
    static $permit = null;
    if ($operation === 'set') { $permit = $value; return true; }
    if ($operation === 'peek') return $permit;
    $saved = $permit; $permit = null;
    return $operation === 'take' ? $saved : true;
}

/** Clear unfinished admission at the existing shared transaction boundary. */
function release_transfer_core_permit()
{
    return transfer_core_permit('clear');
}

/** All family histories are decoded by the core; unknown custody is not absent. */
function transfer_core_history($identity)
{
    $drafts = transfer_core_rows('SELECT * FROM '.TB_PREF.'approval_drafts WHERE trans_type='
        .ST_WH_TRANSFER_ORDER.' ORDER BY id FOR UPDATE');
    if ($drafts === false) return false;
    $matches = array(); $ids = array(); $approval = get_approval_workflow_service();
    foreach ($drafts as $draft) {
        $data = $approval->decodeStoredDraftData($draft['draft_data']);
        if (!is_array($data) || !isset($data['transfer_id'])
            || !dimension_writer_reference_id($data['transfer_id'])) return false;
        if ((int)$data['transfer_id'] !== $identity) continue;
        $matches[] = $draft; $ids[] = (int)$draft['id'];
    }
    $actions = $ids ? transfer_core_rows('SELECT * FROM '.TB_PREF.'approval_actions WHERE draft_id IN ('
        .implode(',', $ids).') ORDER BY id FOR UPDATE') : array();
    return $actions === false ? false : array('drafts'=>$matches, 'actions'=>$actions);
}

/** Lock the complete unshipped transfer header and raw children. */
function transfer_core_source($identity, $status, $allow_empty = false)
{
    $headers = transfer_core_rows('SELECT * FROM '.TB_PREF.'wh_transfer_orders WHERE transfer_id='.$identity.' FOR UPDATE');
    if (!$headers || count($headers) !== 1 || $headers[0]['status'] !== $status
        || !empty($headers[0]['shipped_by']) || !empty($headers[0]['received_by'])
        || $headers[0]['actual_ship_date'] !== null || $headers[0]['actual_recv_date'] !== null) return false;
    // Material conversion must first establish its own attributable downstream
    // command. An existing numeric link alone is not that approval authority.
    $links = transfer_core_rows('SELECT request_id FROM '.TB_PREF.'wh_material_requests WHERE linked_doc_type='
        .ST_LOCTRANSFER.' AND linked_doc_no='.$identity.' ORDER BY request_id FOR UPDATE');
    if ($links === false || $links) return false;
    $lines = transfer_core_rows('SELECT * FROM '.TB_PREF.'wh_transfer_order_lines WHERE transfer_id='.$identity.' ORDER BY line_id FOR UPDATE');
    if ($lines === false || (!$allow_empty && !$lines)) return false;
    foreach ($lines as $line)
        if (!is_numeric($line['qty_requested']) || !is_finite((float)$line['qty_requested'])
            || (float)$line['qty_requested'] <= 0 || (float)$line['qty_shipped'] != 0 || (float)$line['qty_received'] != 0
            || !is_numeric($line['unit_cost']) || !is_finite((float)$line['unit_cost']) || (float)$line['unit_cost'] < 0) return false;
    return array('header'=>$headers[0], 'lines'=>$lines);
}

/** Verify newly authored core history independently of old raw payload encoding. */
function transfer_core_new_action($row, $draft, $type, $level, $permit)
{
    $now = transfer_core_rows('SELECT NOW() AS now_');
    return $now && (int)$row['draft_id'] === (int)$draft && $row['action_type'] === $type
        && (int)$row['user_id'] === (int)$permit['actor']['user_id'] && (int)$row['level'] === (int)$level
        && $row['comments'] === $permit['audit']['comments'] && $row['ip_address'] === $permit['audit']['ip']
        && $row['draft_data_snapshot'] === null && $row['action_date'] >= $permit['audit']['started']
        && $row['action_date'] <= $now[0]['now_'];
}

/** Match review fields to intrinsic transfer intent; display labels may evolve. */
function transfer_core_review_matches($data, $source, $draft)
{
    foreach (array('transfer_id','transfer_no','transfer_type','status','from_loc_code','to_loc_code','requested_by',
        'request_date','expected_ship_date','expected_recv_date','reference','memo') as $field) {
        if (!array_key_exists($field, $data) || !is_scalar($data[$field])
            || (string)$data[$field] !== (string)$source['header'][$field]) return false;
    }
    if (!isset($data['lines'], $data['line_count'], $data['total_estimated']) || !is_array($data['lines'])
        || count($data['lines']) !== count($source['lines']) || (int)$data['line_count'] !== count($source['lines'])) return false;
    $total = 0.0;
    foreach ($source['lines'] as $index=>$line) {
        if (!isset($data['lines'][$index]) || !is_array($data['lines'][$index])) return false;
        foreach (array('line_id','stock_id','from_bin_id','to_bin_id','batch_id','serial_id','memo') as $field) {
            if (!array_key_exists($field, $data['lines'][$index]) || !is_scalar($data['lines'][$index][$field])) return false;
            $expected = $line[$field] === null ? ($field === 'memo' ? '' : 0) : $line[$field];
            if ((string)$data['lines'][$index][$field] !== (string)$expected) return false;
        }
        foreach (array('qty_requested','qty_shipped','qty_received','unit_cost') as $field)
            if (!isset($data['lines'][$index][$field]) || !is_numeric($data['lines'][$index][$field])
                || (float)$data['lines'][$index][$field] !== (float)$line[$field]) return false;
        $total += (float)$line['qty_requested'] * (float)$line['unit_cost'];
    }
    return is_numeric($data['total_estimated']) && is_finite($total) && is_finite((float)$data['total_estimated'])
        && abs($total-(float)$data['total_estimated']) < 0.00000001 && is_numeric($draft['amount'])
        && is_finite((float)$draft['amount']) && abs($total-(float)$draft['amount']) < 0.00000001;
}

/** Lock and validate current native location, item and optional tracking dependencies. */
function transfer_core_dependencies($source)
{
    $header = $source['header']; $rows = array();
    if (!in_array($header['transfer_type'], array('standard','urgent','backorder'), true)
        || $header['from_loc_code'] === $header['to_loc_code']) return false;
    $locations = array($header['from_loc_code'], $header['to_loc_code']); sort($locations, SORT_STRING);
    foreach ($locations as $code) {
        $current = transfer_core_rows('SELECT * FROM '.TB_PREF.'locations WHERE loc_code='.db_escape($code).' AND inactive=0 FOR UPDATE');
        if (!$current || count($current) !== 1) return false;
        $rows['locations'][$code] = $current;
    }
    foreach ($source['lines'] as $line) {
        $stock = transfer_core_rows('SELECT * FROM '.TB_PREF.'stock_master WHERE stock_id='.db_escape($line['stock_id']).' AND inactive=0 FOR UPDATE');
        if (!$stock || count($stock) !== 1) return false;
        $rows['stock'][$line['stock_id']] = $stock;
        foreach (array('from_bin_id'=>'from_loc_code', 'to_bin_id'=>'to_loc_code') as $field=>$location) {
            if ($line[$field] === null || (int)$line[$field] === 0) continue;
            $bin = transfer_core_rows('SELECT * FROM '.TB_PREF.'wh_locations WHERE loc_id='.(int)$line[$field]
                .' AND warehouse_loc_code='.db_escape($header[$location]).' AND is_active=1 FOR UPDATE');
            if (!$bin || count($bin) !== 1) return false;
            $rows['bins'][(int)$line[$field]] = $bin;
        }
        foreach (array('batch_id'=>'stock_batches', 'serial_id'=>'serial_numbers') as $field=>$table) {
            if ($line[$field] === null || (int)$line[$field] === 0) continue;
            $tracking = transfer_core_rows('SELECT * FROM '.TB_PREF.$table.' WHERE id='.(int)$line[$field]
                .' AND stock_id='.db_escape($line['stock_id']).' AND inactive=0 FOR UPDATE');
            if (!$tracking || count($tracking) !== 1) return false;
            if ($field === 'serial_id' && ($tracking[0]['loc_code'] !== $header['from_loc_code']
                || $tracking[0]['status'] !== 'available' || (float)$line['qty_requested'] !== 1.0
                || ($line['batch_id'] !== null && (int)$tracking[0]['batch_id'] !== (int)$line['batch_id'])
                || ($line['from_bin_id'] !== null && (int)$tracking[0]['wh_loc_id'] !== (int)$line['from_bin_id']))) return false;
            if ($field === 'batch_id' && $tracking[0]['status'] !== 'active') return false;
            $rows[$table][(int)$line[$field]] = $tracking;
        }
    }
    return $rows;
}

/** Current source/policy authority for every core action, including direct calls. */
function validate_transfer_core_action($action, $data, $draft, $actor, $comments)
{
    $context = transfer_core_context();
    if (!$context || empty($GLOBALS['transaction_level']) || !is_array($actor)
        || (int)$actor['user_id'] !== (int)$_SESSION['wa_current_user']->user
        || (int)$actor['role_id'] !== (int)$_SESSION['wa_current_user']->access) return false;
    if ($action === 'submit') {
        $permit = transfer_core_permit('take');
        return $permit && $permit['action'] === 'submit' && $permit['context'] === $context
            && $permit['data'] === $data && $permit['actor'] === $actor
            && transfer_core_source((int)$data['transfer_id'], 'submitted') === $permit['source'];
    }
    if (!in_array($action, array('approve','reject','cancel'), true) || !is_string($comments)
        || strpos($comments, "\0") !== false || !is_array($data) || !is_array($draft)
        || !isset($data['transfer_id'], $data['transfer_source_sha256'], $data['transfer_company_sha256'])
        || !($identity = dimension_writer_reference_id($data['transfer_id']))
        || $data['transfer_company_sha256'] !== transfer_core_context(true)) return false;
    $history = transfer_core_history($identity);
    $source = transfer_core_source($identity, 'submitted');
    if (!$history || !$source || transfer_core_source_fingerprint($source) !== $data['transfer_source_sha256']
        || !transfer_core_review_matches($data, $source, $draft)
        || !($dependencies = transfer_core_dependencies($source))) return false;
    $reference = $source['header']['reference'] === null || $source['header']['reference'] === ''
        ? 'TO-'.str_pad($source['header']['transfer_no'], 4, '0', STR_PAD_LEFT) : $source['header']['reference'];
    if ($draft['loc_code'] !== $source['header']['from_loc_code'] || $draft['reference'] !== $reference
        || (int)$draft['person_type_id'] !== PT_MISC || (int)$draft['person_id'] !== (int)$source['header']['requested_by']
        || $draft['currency'] !== get_company_pref('curr_default')) return false;
    $pending = array();
    foreach ($history['drafts'] as $row) if ((int)$row['status'] === APPROVAL_STATUS_PENDING) $pending[] = $row;
    if (count($pending) !== 1 || (int)$pending[0]['id'] !== (int)$draft['id']
        || (int)$draft['submitted_by'] !== (int)$source['header']['requested_by']) return false;
    $submissions = 0;
    foreach ($history['actions'] as $row) {
        if ((int)$row['draft_id'] !== (int)$draft['id'] || $row['action_type'] !== 'submit') continue;
        if ((int)$row['user_id'] !== (int)$draft['submitted_by'] || (int)$row['level'] !== 0) return false;
        $submissions++;
    }
    if ($submissions !== 1) return false;
    $number = (int)$pending[0]['reserved_trans_no'];
    $owners = transfer_core_rows('SELECT id FROM '.TB_PREF.'approval_drafts WHERE trans_type='.ST_WH_TRANSFER_ORDER
        .' AND reserved_trans_no='.$number.' ORDER BY id FOR UPDATE');
    $reserved = transfer_core_rows('SELECT * FROM '.TB_PREF.'voided WHERE type='.ST_WH_TRANSFER_ORDER.' AND id='.$number.' FOR UPDATE');
    if ($number <= 0 || !$owners || count($owners) !== 1 || !$reserved || count($reserved) !== 1
        || $reserved[0]['memo_'] !== 'APPROVAL_RESERVED') return false;
    $workflow = get_approval_workflow_by_type(ST_WH_TRANSFER_ORDER, true);
    if (!$workflow || empty($workflow['is_active']) || (int)$workflow['id'] !== (int)$draft['workflow_id']
        || (int)$workflow['version'] !== (int)$draft['workflow_version']
        || !workflow_has_active_levels($workflow['id'], true)) return false;
    if ($action === 'cancel') {
        if ((int)$draft['submitted_by'] !== (int)$actor['user_id']) return false;
    } elseif ((int)$draft['submitted_by'] === (int)$actor['user_id']
        || !get_approval_workflow_service()->canActOnDraft((int)$draft['id'], true)) return false;
    $audit = transfer_core_rows('SELECT NOW() AS started,'.db_escape($comments, true).' AS comments,'
        .db_escape(isset($_SERVER['REMOTE_ADDR']) ? $_SERVER['REMOTE_ADDR'] : '').' AS ip');
    if (!$audit || !register_transaction_resource_release('transfer-core', 'release_transfer_core_permit')) return false;
    return transfer_core_permit('set', array('action'=>$action, 'context'=>$context, 'actor'=>$actor,
        'data'=>$data, 'source'=>$source, 'history'=>$history, 'draft'=>$pending[0], 'audit'=>$audit[0], 'executed'=>false,
        'reserved'=>$reserved, 'dependencies'=>$dependencies));
}

/** Native terminal effect is callable only after core validation in this transaction. */
function execute_transfer_core_transition($action, $data, $draft_id)
{
    $permit = transfer_core_permit('peek');
    if (!$permit || $permit['action'] !== $action || $permit['data'] !== $data || $permit['executed']
        || $permit['context'] !== transfer_core_context() || empty($GLOBALS['transaction_level'])
        || (int)$permit['draft']['id'] !== (int)$draft_id) return false;
    $status = $action === 'approve' ? 'approved' : ($action === 'reject' ? 'draft' : 'cancelled');
    $draft = get_approval_draft_for_update($draft_id);
    $terminal = $action === 'approve' ? APPROVAL_STATUS_APPROVED : ($action === 'reject' ? APPROVAL_STATUS_REJECTED : APPROVAL_STATUS_CANCELLED);
    $identity = (int)$data['transfer_id'];
    if (!$draft || (int)$draft['status'] !== $terminal
        || get_approval_workflow_service()->decodeStoredDraftData($draft['draft_data']) !== $data
        || transfer_core_source($identity, 'submitted') !== $permit['source']) return false;
    if (!db_query("UPDATE ".TB_PREF."wh_transfer_orders SET status=".db_escape($status).",updated_at=NOW()"
        .($action === 'approve' ? ',approval_date=NOW(),approved_by='.(int)$permit['actor']['user_id'] : '')
        .' WHERE transfer_id='.$identity." AND status='submitted'") || db_num_affected_rows() !== 1) return false;
    $current = transfer_core_source($identity, $status);
    if (!$current) return false;
    $expected = $permit['source']; $expected['header']['status'] = $status;
    $expected['header']['updated_at'] = $current['header']['updated_at'];
    if ($action === 'approve') {
        if ((int)$current['header']['approved_by'] !== (int)$permit['actor']['user_id']) return false;
        $expected['header']['approved_by'] = $current['header']['approved_by'];
        if (!$current['header']['approval_date']) return false;
        $expected['header']['approval_date'] = $current['header']['approval_date'];
    }
    if ($current !== $expected) return false;
    $permit['completed_source'] = $current; $permit['executed'] = true;
    transfer_core_permit('set', $permit);
    return $identity;
}

/** Reconcile source and immutable history after final core effects, before commit. */
function verify_transfer_core_completion($outcome, $draft, $actor, $comments)
{
    $permit = transfer_core_permit('take');
    if (!$permit || $permit['context'] !== transfer_core_context() || $permit['actor'] !== $actor) return false;
    $terminal = in_array($outcome, array('approved','rejected','cancelled'), true);
    if ($terminal !== $permit['executed']) return false;
    $source = $terminal ? $permit['completed_source'] : $permit['source'];
    if (transfer_core_source((int)$permit['data']['transfer_id'], $source['header']['status']) !== $source
        || transfer_core_dependencies($source) !== $permit['dependencies']) return false;
    $history = transfer_core_history((int)$permit['data']['transfer_id']);
    if (!$history || count($history['drafts']) !== count($permit['history']['drafts'])
        || count($history['actions']) !== count($permit['history']['actions']) + 1) return false;
    $new = array_pop($history['actions']);
    if ($history['actions'] !== $permit['history']['actions']
        || !transfer_core_new_action($new, $draft['id'], $permit['action'], $draft['current_level'], $permit)) return false;
    foreach ($history['drafts'] as $index=>$row) {
        $old = $permit['history']['drafts'][$index];
        if ((int)$old['id'] !== (int)$draft['id']) { if ($row !== $old) return false; continue; }
        $status = $outcome === 'approved' ? APPROVAL_STATUS_APPROVED : ($outcome === 'rejected' ? APPROVAL_STATUS_REJECTED
            : ($outcome === 'cancelled' ? APPROVAL_STATUS_CANCELLED : APPROVAL_STATUS_PENDING));
        if ((int)$row['status'] !== $status || (int)$row['current_level'] !== (int)$old['current_level'] + ($outcome === 'pending_next_level' ? 1 : 0)) return false;
        $now = transfer_core_rows('SELECT NOW() AS now_');
        if (!$now || ($terminal && (!$row['completed_date'] || $row['completed_date'] < $permit['audit']['started']
            || $row['completed_date'] > $now[0]['now_']))) return false;
        if (!$terminal && ($row['completed_date'] !== $old['completed_date'] || $row['approved_trans_no'] !== $old['approved_trans_no'])) return false;
        if ($terminal && $outcome !== 'approved' && $row['approved_trans_no'] !== $old['approved_trans_no']) return false;
        if ($outcome === 'approved' && (int)$row['approved_trans_no'] !== (int)$permit['data']['transfer_id']) return false;
        foreach (array('status','current_level','completed_date','approved_trans_no') as $field) $row[$field] = $old[$field];
        if ($row !== $old) return false;
    }
    $reserved = transfer_core_rows('SELECT * FROM '.TB_PREF.'voided WHERE type='.ST_WH_TRANSFER_ORDER
        .' AND id='.(int)$permit['draft']['reserved_trans_no'].' FOR UPDATE');
    $expected = $permit['reserved'];
    if ($outcome === 'approved') $expected = array();
    elseif ($outcome === 'rejected' || $outcome === 'cancelled') $expected[0]['memo_'] = 'APPROVAL_'.strtoupper($outcome);
    if ($reserved !== $expected) return false;
    if ($outcome === 'approved') {
        // This action was authored by the core in this transaction. Freeze the
        // complete terminal source only after every core effect reconciles;
        // never replace an older action, draft payload or submitted fingerprint.
        $receipt = json_encode(array('schema'=>'transfer-approved-source-v1',
            'transfer_id'=>(int)$permit['data']['transfer_id'], 'draft_id'=>(int)$draft['id'],
            'company_sha256'=>transfer_core_context(true),
            'submitted_source_sha256'=>$permit['data']['transfer_source_sha256'],
            'history_sha256'=>transfer_core_source_fingerprint(array('header'=>array(), 'lines'=>array_merge($history['drafts'], $history['actions']))),
            'action_sha256'=>transfer_core_source_fingerprint(array('header'=>$new, 'lines'=>array())),
            'source_sha256'=>transfer_core_source_fingerprint($source)));
        if ($receipt === false || !db_query('UPDATE '.TB_PREF.'approval_actions SET draft_data_snapshot='
            .db_escape($receipt).' WHERE id='.(int)$new['id'].' AND draft_data_snapshot IS NULL')
            || db_num_affected_rows() !== 1) return false;
        $literal = transfer_core_rows('SELECT '.db_escape($receipt).' AS snapshot_');
        if (!$literal) return false;
        $new['draft_data_snapshot'] = $literal[0]['snapshot_'];
        $history['actions'][] = $new;
        $expected_history = transfer_core_history((int)$permit['data']['transfer_id']);
        if ($expected_history !== $history
            || transfer_core_source((int)$permit['data']['transfer_id'], 'approved') !== $source
            || transfer_core_rows('SELECT * FROM '.TB_PREF.'voided WHERE type='.ST_WH_TRANSFER_ORDER
                .' AND id='.(int)$permit['draft']['reserved_trans_no'].' FOR UPDATE') !== $expected
            || transfer_core_dependencies($source) !== $permit['dependencies']
            || transfer_core_context() !== $permit['context']) return false;
    }
    return true;
}

/** Atomically submit current native transfer intent without an automatic fallback. */
function submit_native_transfer_to_core($transfer_id)
{
    $context = transfer_core_context(); $identity = dimension_writer_reference_id($transfer_id);
    if (!$context || !$identity || !class_exists('ApprovalWorkflow') || !ApprovalWorkflow::isRegistered(ST_WH_TRANSFER_ORDER)) return false;
    try {
        if (!begin_transaction()) return false;
        $audit = transfer_core_rows('SELECT NOW() AS started,NULL AS comments,'
            .db_escape(isset($_SERVER['REMOTE_ADDR']) ? $_SERVER['REMOTE_ADDR'] : '').' AS ip');
        if (!$audit) { cancel_transaction(); return false; }
        $history = transfer_core_history($identity);
        $source = transfer_core_source($identity, 'draft');
        if (!$source || $history === false || (int)$source['header']['requested_by'] !== (int)$_SESSION['wa_current_user']->user) { cancel_transaction(); return false; }
        foreach ($history['drafts'] as $draft) {
            $prior_workflow = transfer_core_rows('SELECT trans_type FROM '.TB_PREF.'approval_workflows WHERE id='.(int)$draft['workflow_id'].' FOR UPDATE');
            if (!in_array((int)$draft['status'], array(APPROVAL_STATUS_REJECTED, APPROVAL_STATUS_CANCELLED), true)
                || (int)$draft['submitted_by'] !== (int)$source['header']['requested_by']
                || !$prior_workflow || (int)$prior_workflow[0]['trans_type'] !== ST_WH_TRANSFER_ORDER) { cancel_transaction(); return false; }
        }
        $workflow = get_approval_workflow_by_type(ST_WH_TRANSFER_ORDER, true);
        if (!$workflow || empty($workflow['is_active']) || !workflow_has_active_levels($workflow['id'], true)) { cancel_transaction(); return false; }
        $dependencies = transfer_core_dependencies($source);
        if (!$dependencies) { cancel_transaction(); return false; }
        if (!db_query("UPDATE ".TB_PREF."wh_transfer_orders SET status='submitted',updated_at=NOW() WHERE transfer_id=".$identity." AND status='draft'")
            || db_num_affected_rows() !== 1) { cancel_transaction(); return false; }
        $submitted = transfer_core_source($identity, 'submitted');
        $expected = $source; $expected['header']['status'] = 'submitted';
        if (!$submitted) { cancel_transaction(); return false; }
        $expected['header']['updated_at'] = $submitted['header']['updated_at'];
        if ($submitted !== $expected) { cancel_transaction(); return false; }
        $data = get_transfer_order_submission_data($identity);
        if (!$data || !is_finite((float)$data['total_estimated']) || (float)$data['total_estimated'] < 0) { cancel_transaction(); return false; }
        $data['transfer_source_sha256'] = transfer_core_source_fingerprint($submitted);
        $data['transfer_company_sha256'] = transfer_core_context(true);
        $actor = array('user_id'=>(int)$_SESSION['wa_current_user']->user, 'role_id'=>(int)$_SESSION['wa_current_user']->access);
        if (!register_transaction_resource_release('transfer-core', 'release_transfer_core_permit')) { cancel_transaction(); return false; }
        transfer_core_permit('set', array('action'=>'submit','context'=>$context,'data'=>$data,'actor'=>$actor,'source'=>$submitted));
        $result = get_approval_workflow_service()->submit(ST_WH_TRANSFER_ORDER, $data, $data['total_estimated'], array(
            'currency'=>get_company_pref('curr_default'), 'person_type'=>PT_MISC, 'person_id'=>$actor['user_id'],
            'summary'=>sprintf(_('Transfer Order %s'), $data['transfer_no']), 'loc_code'=>$data['from_loc_code'],
            'transfer_order_id'=>$identity), true);
        if (!is_array($result) || !isset($result['status'], $result['draft_id']) || $result['status'] !== 'pending') { cancel_transaction(); return false; }
        $current = transfer_core_history($identity);
        if (!$current || count($current['drafts']) !== count($history['drafts'])+1 || count($current['actions']) !== count($history['actions'])+1) { cancel_transaction(); return false; }
        $new_draft = array_pop($current['drafts']); $new_action = array_pop($current['actions']);
        $reference = $submitted['header']['reference'] === null || $submitted['header']['reference'] === ''
            ? 'TO-'.str_pad($submitted['header']['transfer_no'], 4, '0', STR_PAD_LEFT) : $submitted['header']['reference'];
        $reserved = transfer_core_rows('SELECT * FROM '.TB_PREF.'voided WHERE type='.ST_WH_TRANSFER_ORDER
            .' AND id='.(int)$result['trans_no'].' FOR UPDATE');
        $now = transfer_core_rows('SELECT NOW() AS now_');
        if ($current !== $history || (int)$new_draft['id'] !== (int)$result['draft_id']
            || (int)$new_draft['submitted_by'] !== $actor['user_id'] || (int)$new_draft['status'] !== APPROVAL_STATUS_PENDING
            || (int)$new_draft['current_level'] !== 1 || $new_draft['completed_date'] !== null || $new_draft['approved_trans_no'] !== null
            || (int)$new_draft['workflow_id'] !== (int)$workflow['id'] || (int)$new_draft['workflow_version'] !== (int)$workflow['version']
            || get_approval_workflow_service()->decodeStoredDraftData($new_draft['draft_data']) !== json_decode(json_encode($data), true)
            || !transfer_core_review_matches($data, $submitted, $new_draft)
            || (int)$new_draft['trans_type'] !== ST_WH_TRANSFER_ORDER || (int)$new_draft['reserved_trans_no'] !== (int)$result['trans_no']
            || $new_draft['reference'] !== $reference || $new_draft['loc_code'] !== $submitted['header']['from_loc_code']
            || $new_draft['currency'] !== get_company_pref('curr_default') || (int)$new_draft['person_type_id'] !== PT_MISC
            || (int)$new_draft['person_id'] !== $actor['user_id'] || !$reserved || count($reserved) !== 1
            || $reserved[0]['memo_'] !== 'APPROVAL_RESERVED'
            || !$now || $new_draft['submitted_date'] < $audit[0]['started'] || $new_draft['submitted_date'] > $now[0]['now_']
            || !transfer_core_new_action($new_action, $result['draft_id'], 'submit', 0, array('actor'=>$actor,'audit'=>$audit[0]))
            || transfer_core_source($identity, 'submitted') !== $submitted || transfer_core_context() !== $context
            || transfer_core_dependencies($submitted) !== $dependencies
            || !commit_transaction()) { cancel_transaction(); return false; }
        return $result;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}

/** Native cancellation resolves exact decoded custody under one owning transaction. */
function cancel_native_transfer_core($transfer_id, $comments)
{
    $identity = dimension_writer_reference_id($transfer_id);
    if (!$identity || !transfer_core_context()) return false;
    try {
        if (!begin_transaction()) return false;
        $history = transfer_core_history($identity);
        if (!$history) { cancel_transaction(); return false; }
        $pending = array();
        foreach ($history['drafts'] as $draft)
            if ((int)$draft['status'] === APPROVAL_STATUS_PENDING) $pending[] = $draft;
        if (count($pending) !== 1) { cancel_transaction(); return false; }
        $result = get_approval_workflow_service()->cancel((int)$pending[0]['id'], $comments);
        if (!is_array($result) || $result['status'] !== 'cancelled' || !commit_transaction()) { cancel_transaction(); return false; }
        return true;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
