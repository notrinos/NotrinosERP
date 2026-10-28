<?php
/** Standalone material demand uses the registered core, never local approval. */
require_once __DIR__.'/../../../../dimensions/includes/dimension_reference_writer_fence.inc';

/** Bind native admission to the current actor, role, company and connection. */
function material_core_context($portable = false)
{
    global $db, $db_connections;
    $actor = isset($_SESSION['wa_current_user']) ? $_SESSION['wa_current_user'] : null;
    if (!is_object($actor) || empty($actor->user) || !isset($actor->access, $actor->cur_con)
        || !isset($db_connections[$actor->cur_con]) || !method_exists($actor, 'can_access')
        || !$actor->can_access('SA_MATERIALREQUEST')) return false;
    $company = array($actor->cur_con, $db_connections[$actor->cur_con]);
    return hash('sha256', serialize($portable ? $company : array($company, $actor->user,
        $actor->access, is_object($db) ? spl_object_hash($db) : $db)));
}

/** Current associative rows, preserving exact intrinsic and opaque history. */
function material_core_rows($sql)
{
    $result = db_query($sql);
    if (!$result) return false;
    $rows = array();
    while ($row = db_fetch_assoc($result)) $rows[] = $row;
    return $rows;
}

/** Bind DB values without depending on PDO's PHP-version-specific numeric types. */
function material_core_source_fingerprint($source)
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
function material_core_permit($operation, $value = null)
{
    static $permit = null;
    if ($operation === 'set') { $permit = $value; return true; }
    if ($operation === 'peek') return $permit;
    $saved = $permit; $permit = null;
    return $operation === 'take' ? $saved : true;
}

/** Clear unfinished admission at the existing shared transaction boundary. */
function release_material_core_permit()
{
    return material_core_permit('clear');
}

/** All family histories are decoded by the core; unknown custody is not absent. */
function material_core_history($identity)
{
    $drafts = material_core_rows('SELECT * FROM '.TB_PREF.'approval_drafts WHERE trans_type='
        .ST_WH_MATERIAL_REQUEST.' ORDER BY id FOR UPDATE');
    if ($drafts === false) return false;
    $matches = array(); $ids = array(); $approval = get_approval_workflow_service();
    foreach ($drafts as $draft) {
        $data = $approval->decodeStoredDraftData($draft['draft_data']);
        if (!is_array($data) || !isset($data['request_id'])
            || !dimension_writer_reference_id($data['request_id'])) return false;
        if ((int)$data['request_id'] !== $identity) continue;
        $matches[] = $draft; $ids[] = (int)$draft['id'];
    }
    $actions = $ids ? material_core_rows('SELECT * FROM '.TB_PREF.'approval_actions WHERE draft_id IN ('
        .implode(',', $ids).') ORDER BY id FOR UPDATE') : array();
    return $actions === false ? false : array('drafts'=>$matches, 'actions'=>$actions);
}

/** Lock only standalone unconsumed demand; paired requisitions keep their owner. */
function material_core_source($identity, $status)
{
    $headers = material_core_rows('SELECT * FROM '.TB_PREF.'wh_material_requests WHERE request_id='.$identity.' FOR UPDATE');
    if (!$headers || count($headers) !== 1 || $headers[0]['status'] !== $status
        || !empty($headers[0]['linked_doc_type']) || !empty($headers[0]['linked_doc_no'])) return false;
    $pairs = material_core_rows('SELECT id FROM '.TB_PREF.'purch_requisitions WHERE material_request_id='.$identity.' ORDER BY id FOR UPDATE');
    if ($pairs === false || $pairs) return false;
    $lines = material_core_rows('SELECT * FROM '.TB_PREF.'wh_material_request_lines WHERE request_id='.$identity.' ORDER BY line_id FOR UPDATE');
    if (!$lines) return false;
    foreach ($lines as $line)
        if (!is_numeric($line['qty_requested']) || !is_finite((float)$line['qty_requested'])
            || (float)$line['qty_requested'] <= 0 || (float)$line['qty_fulfilled'] != 0) return false;
    return array('header'=>$headers[0], 'lines'=>$lines);
}

/** Verify newly authored core history independently of old raw payload encoding. */
function material_core_new_action($row, $draft, $type, $level, $permit)
{
    $now = material_core_rows('SELECT NOW() AS now_');
    return $now && (int)$row['draft_id'] === (int)$draft && $row['action_type'] === $type
        && (int)$row['user_id'] === (int)$permit['actor']['user_id'] && (int)$row['level'] === (int)$level
        && $row['comments'] === $permit['audit']['comments'] && $row['ip_address'] === $permit['audit']['ip']
        && $row['draft_data_snapshot'] === null && $row['action_date'] >= $permit['audit']['started']
        && $row['action_date'] <= $now[0]['now_'];
}

/** Match review fields to intrinsic demand; retained display labels may evolve. */
function material_core_review_matches($data, $source, $draft)
{
    foreach (array('request_id','request_no','request_type','status','warehouse_loc_code','requested_by','request_date','required_date','reference','memo') as $field) {
        $value = $source['header'][$field];
        if (!array_key_exists($field, $data) || is_array($data[$field]) || is_object($data[$field])
            || (string)$data[$field] !== (string)$value) return false;
    }
    if (!isset($data['lines'], $data['line_count'], $data['total_estimated']) || !is_array($data['lines'])
        || count($data['lines']) !== count($source['lines']) || (int)$data['line_count'] !== count($source['lines'])) return false;
    $total = 0.0;
    foreach ($source['lines'] as $index=>$line) {
        if (!isset($data['lines'][$index]) || !is_array($data['lines'][$index])) return false;
        $review = $data['lines'][$index];
        foreach (array('line_id','stock_id','required_date','memo') as $field)
            if (!array_key_exists($field, $review) || !is_scalar($review[$field]) || (string)$review[$field] !== (string)$line[$field]) return false;
        foreach (array('qty_requested','qty_fulfilled') as $field)
            if (!isset($review[$field]) || !is_numeric($review[$field]) || (float)$review[$field] !== (float)$line[$field]) return false;
        if (!isset($review['unit_cost']) || !is_numeric($review['unit_cost']) || !is_finite((float)$review['unit_cost']) || (float)$review['unit_cost'] < 0) return false;
        $total += (float)$review['qty_requested'] * (float)$review['unit_cost'];
    }
    return is_numeric($data['total_estimated']) && is_finite($total) && is_finite((float)$data['total_estimated'])
        && abs($total - (float)$data['total_estimated']) < 0.00000001
        && is_numeric($draft['amount']) && is_finite((float)$draft['amount'])
        && abs((float)$draft['amount'] - (float)$data['total_estimated']) < 0.00000001;
}

/** Current source/policy authority for every core action, including direct calls. */
function validate_material_core_action($action, $data, $draft, $actor, $comments)
{
    $context = material_core_context();
    if (!$context || empty($GLOBALS['transaction_level']) || !is_array($actor)
        || (int)$actor['user_id'] !== (int)$_SESSION['wa_current_user']->user
        || (int)$actor['role_id'] !== (int)$_SESSION['wa_current_user']->access) return false;
    if ($action === 'submit') {
        $permit = material_core_permit('take');
        return $permit && $permit['action'] === 'submit' && $permit['context'] === $context
            && $permit['data'] === $data && $permit['actor'] === $actor
            && material_core_source((int)$data['request_id'], 'submitted') === $permit['source'];
    }
    if (!in_array($action, array('approve','reject','cancel'), true) || !is_string($comments)
        || strpos($comments, "\0") !== false || !is_array($data) || !is_array($draft)
        || !isset($data['request_id'], $data['material_source_sha256'], $data['material_company_sha256'])
        || !($identity = dimension_writer_reference_id($data['request_id']))
        || $data['material_company_sha256'] !== material_core_context(true)) return false;
    $history = material_core_history($identity);
    $source = material_core_source($identity, 'submitted');
    if (!$history || !$source || material_core_source_fingerprint($source) !== $data['material_source_sha256']
        || !material_core_review_matches($data, $source, $draft)) return false;
    $reference = $source['header']['reference'] === null || $source['header']['reference'] === ''
        ? format_material_request_no($source['header']['request_no']) : $source['header']['reference'];
    if ($draft['loc_code'] !== $source['header']['warehouse_loc_code'] || $draft['reference'] !== $reference
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
    $owners = material_core_rows('SELECT id FROM '.TB_PREF.'approval_drafts WHERE trans_type='.ST_WH_MATERIAL_REQUEST
        .' AND reserved_trans_no='.$number.' ORDER BY id FOR UPDATE');
    $reserved = material_core_rows('SELECT * FROM '.TB_PREF.'voided WHERE type='.ST_WH_MATERIAL_REQUEST.' AND id='.$number.' FOR UPDATE');
    if ($number <= 0 || !$owners || count($owners) !== 1 || !$reserved || count($reserved) !== 1
        || $reserved[0]['memo_'] !== 'APPROVAL_RESERVED') return false;
    $workflow = get_approval_workflow_by_type(ST_WH_MATERIAL_REQUEST, true);
    if (!$workflow || empty($workflow['is_active']) || (int)$workflow['id'] !== (int)$draft['workflow_id']
        || (int)$workflow['version'] !== (int)$draft['workflow_version']
        || !workflow_has_active_levels($workflow['id'], true)) return false;
    if ($action === 'cancel') {
        if ((int)$draft['submitted_by'] !== (int)$actor['user_id']) return false;
    } elseif ((int)$draft['submitted_by'] === (int)$actor['user_id']
        || !get_approval_workflow_service()->canActOnDraft((int)$draft['id'], true)) return false;
    $audit = material_core_rows('SELECT NOW() AS started,'.db_escape($comments, true).' AS comments,'
        .db_escape(isset($_SERVER['REMOTE_ADDR']) ? $_SERVER['REMOTE_ADDR'] : '').' AS ip');
    if (!$audit || !register_transaction_resource_release('material-core', 'release_material_core_permit')) return false;
    return material_core_permit('set', array('action'=>$action, 'context'=>$context, 'actor'=>$actor,
        'data'=>$data, 'source'=>$source, 'history'=>$history, 'draft'=>$pending[0], 'audit'=>$audit[0], 'executed'=>false,
        'reserved'=>$reserved));
}

/** Native terminal effect is callable only after core validation in this transaction. */
function execute_material_core_transition($action, $data, $draft_id)
{
    $permit = material_core_permit('peek');
    if (!$permit || $permit['action'] !== $action || $permit['data'] !== $data || $permit['executed']
        || $permit['context'] !== material_core_context() || empty($GLOBALS['transaction_level'])
        || (int)$permit['draft']['id'] !== (int)$draft_id) return false;
    $status = $action === 'approve' ? 'approved' : ($action === 'reject' ? 'draft' : 'cancelled');
    $draft = get_approval_draft_for_update($draft_id);
    $terminal = $action === 'approve' ? APPROVAL_STATUS_APPROVED : ($action === 'reject' ? APPROVAL_STATUS_REJECTED : APPROVAL_STATUS_CANCELLED);
    $identity = (int)$data['request_id'];
    if (!$draft || (int)$draft['status'] !== $terminal
        || get_approval_workflow_service()->decodeStoredDraftData($draft['draft_data']) !== $data
        || material_core_source($identity, 'submitted') !== $permit['source']) return false;
    if (!db_query("UPDATE ".TB_PREF."wh_material_requests SET status=".db_escape($status).",updated_at=NOW()"
        .($action === 'approve' ? ',approved_by='.(int)$permit['actor']['user_id'] : '')
        .' WHERE request_id='.$identity." AND status='submitted'") || db_num_affected_rows() !== 1) return false;
    $current = material_core_source($identity, $status);
    if (!$current) return false;
    $expected = $permit['source']; $expected['header']['status'] = $status;
    $expected['header']['updated_at'] = $current['header']['updated_at'];
    if ($action === 'approve') {
        if ((int)$current['header']['approved_by'] !== (int)$permit['actor']['user_id']) return false;
        $expected['header']['approved_by'] = $current['header']['approved_by'];
    }
    if ($current !== $expected) return false;
    $permit['completed_source'] = $current; $permit['executed'] = true;
    material_core_permit('set', $permit);
    return $identity;
}

/** Reconcile source and immutable history after final core effects, before commit. */
function verify_material_core_completion($outcome, $draft, $actor, $comments)
{
    $permit = material_core_permit('take');
    if (!$permit || $permit['context'] !== material_core_context() || $permit['actor'] !== $actor) return false;
    $terminal = in_array($outcome, array('approved','rejected','cancelled'), true);
    if ($terminal !== $permit['executed']) return false;
    $source = $terminal ? $permit['completed_source'] : $permit['source'];
    if (material_core_source((int)$permit['data']['request_id'], $source['header']['status']) !== $source) return false;
    $history = material_core_history((int)$permit['data']['request_id']);
    if (!$history || count($history['drafts']) !== count($permit['history']['drafts'])
        || count($history['actions']) !== count($permit['history']['actions']) + 1) return false;
    $new = array_pop($history['actions']);
    if ($history['actions'] !== $permit['history']['actions']
        || !material_core_new_action($new, $draft['id'], $permit['action'], $draft['current_level'], $permit)) return false;
    foreach ($history['drafts'] as $index=>$row) {
        $old = $permit['history']['drafts'][$index];
        if ((int)$old['id'] !== (int)$draft['id']) { if ($row !== $old) return false; continue; }
        $status = $outcome === 'approved' ? APPROVAL_STATUS_APPROVED : ($outcome === 'rejected' ? APPROVAL_STATUS_REJECTED
            : ($outcome === 'cancelled' ? APPROVAL_STATUS_CANCELLED : APPROVAL_STATUS_PENDING));
        if ((int)$row['status'] !== $status || (int)$row['current_level'] !== (int)$old['current_level'] + ($outcome === 'pending_next_level' ? 1 : 0)) return false;
        $now = material_core_rows('SELECT NOW() AS now_');
        if (!$now || ($terminal && (!$row['completed_date'] || $row['completed_date'] < $permit['audit']['started']
            || $row['completed_date'] > $now[0]['now_']))) return false;
        if (!$terminal && ($row['completed_date'] !== $old['completed_date'] || $row['approved_trans_no'] !== $old['approved_trans_no'])) return false;
        if ($terminal && $outcome !== 'approved' && $row['approved_trans_no'] !== $old['approved_trans_no']) return false;
        if ($outcome === 'approved' && (int)$row['approved_trans_no'] !== (int)$permit['data']['request_id']) return false;
        foreach (array('status','current_level','completed_date','approved_trans_no') as $field) $row[$field] = $old[$field];
        if ($row !== $old) return false;
    }
    $reserved = material_core_rows('SELECT * FROM '.TB_PREF.'voided WHERE type='.ST_WH_MATERIAL_REQUEST
        .' AND id='.(int)$permit['draft']['reserved_trans_no'].' FOR UPDATE');
    $expected = $permit['reserved'];
    if ($outcome === 'approved') $expected = array();
    elseif ($outcome === 'rejected' || $outcome === 'cancelled') $expected[0]['memo_'] = 'APPROVAL_'.strtoupper($outcome);
    if ($reserved !== $expected) return false;
    if ($outcome === 'approved') {
        // This action was authored by the core in this transaction. Freeze the
        // complete terminal source only after every core effect reconciles;
        // never replace an older action, draft payload or submitted fingerprint.
        $receipt = json_encode(array('schema'=>'material-approved-source-v1',
            'request_id'=>(int)$permit['data']['request_id'], 'draft_id'=>(int)$draft['id'],
            'company_sha256'=>material_core_context(true),
            'submitted_source_sha256'=>$permit['data']['material_source_sha256'],
            'history_sha256'=>material_core_source_fingerprint(array('header'=>array(), 'lines'=>array_merge($history['drafts'], $history['actions']))),
            'action_sha256'=>material_core_source_fingerprint(array('header'=>$new, 'lines'=>array())),
            'source_sha256'=>material_core_source_fingerprint($source)));
        if ($receipt === false || !db_query('UPDATE '.TB_PREF.'approval_actions SET draft_data_snapshot='
            .db_escape($receipt).' WHERE id='.(int)$new['id'].' AND draft_data_snapshot IS NULL')
            || db_num_affected_rows() !== 1) return false;
        $literal = material_core_rows('SELECT '.db_escape($receipt).' AS snapshot_');
        if (!$literal) return false;
        $new['draft_data_snapshot'] = $literal[0]['snapshot_'];
        $history['actions'][] = $new;
        $expected_history = material_core_history((int)$permit['data']['request_id']);
        if ($expected_history !== $history
            || material_core_source((int)$permit['data']['request_id'], 'approved') !== $source
            || material_core_rows('SELECT * FROM '.TB_PREF.'voided WHERE type='.ST_WH_MATERIAL_REQUEST
                .' AND id='.(int)$permit['draft']['reserved_trans_no'].' FOR UPDATE') !== $expected
            || material_core_context() !== $permit['context']) return false;
    }
    return true;
}

/** Atomically submit current native demand; no missing-policy or automatic fallback. */
function submit_standalone_material_to_core($request_id)
{
    $context = material_core_context(); $identity = dimension_writer_reference_id($request_id);
    if (!$context || !$identity || !class_exists('ApprovalWorkflow') || !ApprovalWorkflow::isRegistered(ST_WH_MATERIAL_REQUEST)) return false;
    try {
        if (!begin_transaction()) return false;
        $history = material_core_history($identity);
        $source = material_core_source($identity, 'draft');
        if (!$source || $history === false || (int)$source['header']['requested_by'] !== (int)$_SESSION['wa_current_user']->user) { cancel_transaction(); return false; }
        foreach ($history['drafts'] as $draft) {
            $prior_workflow = material_core_rows('SELECT trans_type FROM '.TB_PREF.'approval_workflows WHERE id='.(int)$draft['workflow_id'].' FOR UPDATE');
            if (!in_array((int)$draft['status'], array(APPROVAL_STATUS_REJECTED, APPROVAL_STATUS_CANCELLED), true)
                || (int)$draft['submitted_by'] !== (int)$source['header']['requested_by']
                || !$prior_workflow || (int)$prior_workflow[0]['trans_type'] !== ST_WH_MATERIAL_REQUEST) { cancel_transaction(); return false; }
        }
        $workflow = get_approval_workflow_by_type(ST_WH_MATERIAL_REQUEST, true);
        if (!$workflow || empty($workflow['is_active']) || !workflow_has_active_levels($workflow['id'], true)) { cancel_transaction(); return false; }
        if (!in_array($source['header']['request_type'], array('purchase','transfer','manufacturing','issue'), true)
            || !material_core_rows('SELECT loc_code FROM '.TB_PREF.'locations WHERE loc_code='
                .db_escape($source['header']['warehouse_loc_code']).' AND inactive=0 FOR UPDATE')) { cancel_transaction(); return false; }
        // Freeze valuation inputs used by the existing review collector.
        foreach ($source['lines'] as $line) {
            $stock = material_core_rows('SELECT stock_id,material_cost FROM '.TB_PREF.'stock_master WHERE stock_id='.db_escape($line['stock_id']).' AND inactive=0 FOR UPDATE');
            if (!$stock || !is_numeric($stock[0]['material_cost']) || !is_finite((float)$stock[0]['material_cost']) || (float)$stock[0]['material_cost'] < 0) { cancel_transaction(); return false; }
        }
        if (!db_query("UPDATE ".TB_PREF."wh_material_requests SET status='submitted',updated_at=NOW() WHERE request_id=".$identity." AND status='draft'")
            || db_num_affected_rows() !== 1) { cancel_transaction(); return false; }
        $submitted = material_core_source($identity, 'submitted');
        $expected = $source; $expected['header']['status'] = 'submitted';
        if (!$submitted) { cancel_transaction(); return false; }
        $expected['header']['updated_at'] = $submitted['header']['updated_at'];
        if ($submitted !== $expected) { cancel_transaction(); return false; }
        $data = get_material_request_submission_data($identity);
        if (!$data || !is_finite((float)$data['total_estimated']) || (float)$data['total_estimated'] < 0) { cancel_transaction(); return false; }
        $data['material_source_sha256'] = material_core_source_fingerprint($submitted);
        $data['material_company_sha256'] = material_core_context(true);
        $actor = array('user_id'=>(int)$_SESSION['wa_current_user']->user, 'role_id'=>(int)$_SESSION['wa_current_user']->access);
        if (!register_transaction_resource_release('material-core', 'release_material_core_permit')) { cancel_transaction(); return false; }
        material_core_permit('set', array('action'=>'submit','context'=>$context,'data'=>$data,'actor'=>$actor,'source'=>$submitted));
        $result = get_approval_workflow_service()->submit(ST_WH_MATERIAL_REQUEST, $data, $data['total_estimated'], array(
            'currency'=>get_company_pref('curr_default'), 'person_type'=>PT_MISC, 'person_id'=>$actor['user_id'],
            'summary'=>sprintf(_('Material Request %s'), $data['request_no_formatted']), 'loc_code'=>$data['warehouse_loc_code'],
            'material_request_id'=>$identity), true);
        if (!is_array($result) || !isset($result['status'], $result['draft_id']) || $result['status'] !== 'pending') { cancel_transaction(); return false; }
        $current = material_core_history($identity);
        if (!$current || count($current['drafts']) !== count($history['drafts'])+1 || count($current['actions']) !== count($history['actions'])+1) { cancel_transaction(); return false; }
        $new_draft = array_pop($current['drafts']); $new_action = array_pop($current['actions']);
        if ($current !== $history || (int)$new_draft['id'] !== (int)$result['draft_id']
            || (int)$new_draft['submitted_by'] !== $actor['user_id'] || (int)$new_draft['status'] !== APPROVAL_STATUS_PENDING
            || (int)$new_draft['current_level'] !== 1 || $new_draft['completed_date'] !== null || $new_draft['approved_trans_no'] !== null
            || (int)$new_draft['workflow_id'] !== (int)$workflow['id'] || (int)$new_draft['workflow_version'] !== (int)$workflow['version']
            || get_approval_workflow_service()->decodeStoredDraftData($new_draft['draft_data']) !== json_decode(json_encode($data), true)
            || $new_action['action_type'] !== 'submit' || (int)$new_action['user_id'] !== $actor['user_id']
            || (int)$new_action['draft_id'] !== (int)$result['draft_id'] || (int)$new_action['level'] !== 0
            || material_core_source($identity, 'submitted') !== $submitted || material_core_context() !== $context
            || !commit_transaction()) { cancel_transaction(); return false; }
        return $result;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}

/** Native cancellation resolves exact decoded custody under one owning transaction. */
function cancel_standalone_material_core($request_id, $comments)
{
    $identity = dimension_writer_reference_id($request_id);
    if (!$identity || !material_core_context()) return false;
    try {
        if (!begin_transaction()) return false;
        $history = material_core_history($identity);
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

/** Removal cannot erase reviewed source or retained classification evidence. */
function delete_standalone_material_core($request_id)
{
    require_once __DIR__.'/warehouse_material_remove_db.inc';
    return remove_standalone_material_source($request_id);
}
