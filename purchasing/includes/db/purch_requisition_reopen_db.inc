<?php
/** Reopen only through retained core approval custody; never grant approval. */
require_once __DIR__.'/purch_requisition_header_db.inc';

/** Bind both native owners before reading either company's restricted source. */
function purch_requisition_reopen_context()
{
    $context = purch_requisition_header_context();
    return $context !== false && $_SESSION['wa_current_user']->can_access('SA_MATERIALREQUEST') ? $context : false;
}

/** Read associative current rows so retained comparisons are driver independent. */
function purch_requisition_reopen_rows($sql)
{
    $result = db_query($sql);
    if (!$result) return false;
    $rows = array();
    while ($row = db_fetch_assoc($result)) $rows[] = $row;
    return $rows;
}

/** One-use cancellation validation, released at the owning SQL boundary. */
function purch_requisition_reopen_permit($operation, $value = null)
{
    static $permit = null;
    if ($operation === 'set') { $permit = $value; return true; }
    if ($operation === 'take') { $saved = $permit; $permit = null; return $saved; }
    $permit = null;
    return true;
}

/** Prevent validation from surviving rollback, retry or an outer commit. */
function release_purch_requisition_reopen_permit()
{
    return purch_requisition_reopen_permit('clear');
}

/**
 * Locate complete legacy core custody with the core historical decoder. Unknown
 * pending payloads are never absence. Terminal payloads remain immutable; an
 * unreadable terminal blocks recovery rather than guessing its source identity.
 */
function purch_requisition_reopen_drafts($identity, $source, $approval)
{
    $rows = purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'approval_drafts WHERE trans_type IN ('
        .ST_PURCHREQUISITION.','.ST_WH_MATERIAL_REQUEST.') ORDER BY id FOR UPDATE');
    if ($rows === false) return false;
    $matches = array();
    foreach ($rows as $row) {
        $data = $approval->decodeStoredDraftData($row['draft_data']);
        if (!is_array($data)) return false;
        $field = (int)$row['trans_type'] === ST_PURCHREQUISITION ? 'requisition_id' : 'request_id';
        if (!isset($data[$field]) || !dimension_writer_reference_id($data[$field])) return false;
        if ((int)$row['trans_type'] === ST_WH_MATERIAL_REQUEST) {
            if ((int)$data[$field] === $source && (int)$row['status'] === APPROVAL_STATUS_PENDING) return false;
        } elseif ((int)$data[$field] === $identity) {
            $row['decoded'] = $data;
            $matches[] = $row;
        }
    }
    return $matches;
}

/**
 * Lock the native pair and exact children, deny downstream effects, and compare
 * intrinsic legacy submission fields. Labels/defaults are not re-resolved.
 * Historical custom data and closed/orphan classifications remain read-only.
 */
function purch_requisition_reopen_source($identity, $status, $data)
{
    $headers = purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'purch_requisitions WHERE id='.$identity.' FOR UPDATE');
    if (!$headers || $headers[0]['status'] !== $status) return false;
    $header = $headers[0];
    $source = dimension_writer_reference_id($header['material_request_id']);
    if (!$source) return false;
    $materials = purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'wh_material_requests WHERE request_id='.$source.' FOR UPDATE');
    if (!$materials || $materials[0]['request_type'] !== 'purchase'
        || $materials[0]['status'] !== ($status === 'submitted' ? 'submitted' : 'draft')
        || !empty($materials[0]['linked_doc_type']) || !empty($materials[0]['linked_doc_no'])) return false;
    $links = purch_requisition_reopen_rows('SELECT id FROM '.TB_PREF.'purch_requisitions WHERE material_request_id='.$source.' ORDER BY id FOR UPDATE');
    if (!$links || count($links) !== 1 || (int)$links[0]['id'] !== $identity) return false;
    foreach (array('purch_rfq','purch_orders') as $table) {
        $effects = purch_requisition_reopen_rows('SELECT requisition_id FROM '.TB_PREF.$table.' WHERE requisition_id='.$identity.' FOR UPDATE');
        if ($effects === false || $effects) return false;
    }
    $lines = purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'purch_requisition_lines WHERE requisition_id='.$identity.' ORDER BY id FOR UPDATE');
    $material_lines = purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'wh_material_request_lines WHERE request_id='.$source.' ORDER BY line_id FOR UPDATE');
    if ($lines === false || $material_lines === false || count($lines) !== count($material_lines)) return false;
    $paired = array();
    foreach ($material_lines as $line) {
        if ((float)$line['qty_fulfilled'] != 0) return false;
        $paired[(int)$line['line_id']] = $line;
    }
    foreach ($lines as $line) {
        $key = dimension_writer_reference_id($line['material_request_line_id']);
        if (!$key || !isset($paired[$key]) || (float)$line['qty_ordered'] != 0 || (int)$line['po_number'] != 0
            || !in_array($line['status'], $status === 'submitted' ? array('pending') : array('pending','rejected'), true)
            || $line['stock_id'] !== $paired[$key]['stock_id']
            || (float)$line['quantity'] <= 0 || (float)$line['quantity'] !== (float)$paired[$key]['qty_requested']) return false;
        $owners = purch_requisition_reopen_rows('SELECT id FROM '.TB_PREF.'purch_requisition_lines WHERE material_request_line_id='.$key.' ORDER BY id FOR UPDATE');
        if (!$owners || count($owners) !== 1 || (int)$owners[0]['id'] !== (int)$line['id']) return false;
        unset($paired[$key]);
    }
    if (!is_array($data) || !isset($data['line_items']) || !is_array($data['line_items']) || count($data['line_items']) !== count($lines)) return false;
    foreach (array('material_request_id','requester_id','department_id','dimension_id','dimension2_id') as $field)
        if (!isset($data[$field]) || dimension_writer_reference_id($data[$field]) === false || (int)$data[$field] !== (int)$header[$field]) return false;
    foreach (array('reference','request_date','required_date','priority','purpose','notes','location') as $field)
        if (!array_key_exists($field, $data) || $data[$field] !== $header[$field]) return false;
    if (!isset($data['total_estimated']) || !is_numeric($data['total_estimated'])
        || (float)$data['total_estimated'] !== (float)$header['total_estimated']) return false;
    foreach ($lines as $index=>$line) {
        if (!array_key_exists($index, $data['line_items'])) return false;
        $saved = $data['line_items'][$index];
        if (!is_array($saved)) return false;
        foreach (array('id','preferred_supplier_id') as $field)
            if (!isset($saved[$field]) || dimension_writer_reference_id($saved[$field]) === false || (int)$saved[$field] !== (int)$line[$field]) return false;
        foreach (array('quantity','estimated_unit_price','qty_ordered') as $field)
            if (!isset($saved[$field]) || !is_numeric($saved[$field]) || !is_finite((float)$saved[$field]) || (float)$saved[$field] !== (float)$line[$field]) return false;
        foreach (array('stock_id','notes') as $field)
            if (!array_key_exists($field, $saved) || $saved[$field] !== $line[$field]) return false;
    }
    return array('header'=>$header, 'material'=>$materials[0], 'lines'=>$lines, 'material_lines'=>$material_lines);
}

/** Persist only explicit reopen fields and independently reconcile every other byte. */
function write_purch_requisition_reopen($snapshot, $context)
{
    $identity = (int)$snapshot['header']['id'];
    $source = (int)$snapshot['material']['request_id'];
    if (purch_requisition_reopen_context() !== $context
        || !db_query("UPDATE ".TB_PREF."wh_material_requests SET status='draft',updated_at=NOW() WHERE request_id=".$source)
        || !db_query("UPDATE ".TB_PREF."purch_requisitions SET status='draft' WHERE id=".$identity)
        || !db_query("UPDATE ".TB_PREF."purch_requisition_lines SET status='pending' WHERE requisition_id=".$identity)) return false;
    return verify_purch_requisition_reopen($snapshot, $context);
}

/** Recheck native completion after all owned source/history writes. */
function verify_purch_requisition_reopen($snapshot, $context)
{
    $identity = (int)$snapshot['header']['id'];
    $source = (int)$snapshot['material']['request_id'];
    $headers = purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'purch_requisitions WHERE id='.$identity.' FOR UPDATE');
    $materials = purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'wh_material_requests WHERE request_id='.$source.' FOR UPDATE');
    $lines = purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'purch_requisition_lines WHERE requisition_id='.$identity.' ORDER BY id FOR UPDATE');
    if (!$headers || !$materials || $headers[0]['status'] !== 'draft' || $materials[0]['status'] !== 'draft'
        || $lines === false || count($lines) !== count($snapshot['lines'])) return false;
    $headers[0]['status'] = $snapshot['header']['status'];
    $materials[0]['status'] = $snapshot['material']['status'];
    $materials[0]['updated_at'] = $snapshot['material']['updated_at'];
    foreach ($lines as $index=>$line) {
        if ($line['status'] !== 'pending') return false;
        $lines[$index]['status'] = $snapshot['lines'][$index]['status'];
    }
    return $headers[0] === $snapshot['header'] && $materials[0] === $snapshot['material'] && $lines === $snapshot['lines']
        && purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'wh_material_request_lines WHERE request_id='.$source.' ORDER BY line_id FOR UPDATE') === $snapshot['material_lines']
        && purch_requisition_reopen_context() === $context;
}

/** Guard core cancellation and rejection; remaining actions keep their current registration behavior. */
function validate_purchase_requisition_reopen_action($action, $data, $draft, $actor, $comments = '')
{
    if (in_array($action, array('submit','approve','auto_approve'), true)) {
        require_once __DIR__.'/purch_requisition_submit_db.inc';
        if ($action === 'submit') return validate_purchase_requisition_submit_action($data, $actor);
        if ($action === 'auto_approve') return false;
        return validate_purchase_requisition_approve_action($data, $draft, $actor, $comments);
    }
    if ($action === 'reject') {
        require_once __DIR__.'/purch_requisition_reject_db.inc';
        return validate_purchase_requisition_reject_action($data, $draft, $actor, $comments);
    }
    if ($action !== 'cancel') return true;
    $context = purch_requisition_reopen_context();
    if ($context === false || empty($GLOBALS['transaction_level']) || !is_array($draft)
        || (int)$actor['user_id'] !== (int)$_SESSION['wa_current_user']->user
        || (int)$draft['submitted_by'] !== (int)$actor['user_id'] || (int)$draft['status'] !== APPROVAL_STATUS_PENDING
        || !isset($data['requisition_id']) || !($identity = dimension_writer_reference_id($data['requisition_id']))) return false;
    $snapshot = purch_requisition_reopen_source($identity, 'submitted', $data);
    if (!$snapshot) return false;
    $approval = get_approval_workflow_service();
    $drafts = purch_requisition_reopen_drafts($identity, (int)$snapshot['header']['material_request_id'], $approval);
    if (!$drafts || (int)$drafts[count($drafts)-1]['id'] !== (int)$draft['id']
        || $drafts[count($drafts)-1]['decoded'] !== $data) return false;
    foreach ($drafts as $row)
        if ((int)$row['status'] === APPROVAL_STATUS_PENDING && (int)$row['id'] !== (int)$draft['id']) return false;
    if (!register_transaction_resource_release('purchase-requisition-reopen', 'release_purch_requisition_reopen_permit')) return false;
    return purch_requisition_reopen_permit('set', array('context'=>$context,'draft_id'=>(int)$draft['id'],'data'=>$data,'snapshot'=>$snapshot));
}

/** Complete only the one validated core cancellation, inside its existing transaction. */
function cancel_purchase_requisition_reopen($data, $draft_id, $comments)
{
    $context = purch_requisition_reopen_context();
    if ($context === false || empty($GLOBALS['transaction_level'])) return false;
    $permit = purch_requisition_reopen_permit('take');
    if (!$permit || $permit['context'] !== $context || $permit['draft_id'] !== (int)$draft_id || $permit['data'] !== $data) return false;
    $draft = get_approval_draft_for_update($draft_id);
    if (!$draft || (int)$draft['status'] !== APPROVAL_STATUS_CANCELLED || (int)$draft['trans_type'] !== ST_PURCHREQUISITION
        || (int)$draft['submitted_by'] !== (int)$_SESSION['wa_current_user']->user
        || get_approval_workflow_service()->decodeStoredDraftData($draft['draft_data']) !== $data) return false;
    // Re-read after central status/action writes: triggers must not drift source intent.
    $snapshot = purch_requisition_reopen_source((int)$data['requisition_id'], 'submitted', $data);
    return $snapshot === $permit['snapshot'] && write_purch_requisition_reopen($snapshot, $context);
}

/**
 * Submitted requests cancel through the shared core, preserving maker policy,
 * history and reservations. Rejected requests require exact retained core history
 * and append a reopen action there. Orphan/ambiguous custody requires recovery.
 */
function reopen_purch_requisition_from_core($requisition_id)
{
    $context = purch_requisition_reopen_context();
    $identity = dimension_writer_reference_id($requisition_id);
    if ($context === false || !$identity || !class_exists('ApprovalWorkflow')
        || !ApprovalWorkflow::isRegistered(ST_PURCHREQUISITION) || !function_exists('get_approval_workflow_service')) return false;
    try {
        if (!begin_transaction()) return false;
        // Read the core custody before its native pair, matching central cancellation order.
        $approval = get_approval_workflow_service();
        $drafts = purch_requisition_reopen_drafts($identity, 0, $approval);
        if (!$drafts) { cancel_transaction(); return false; }
        $latest = $drafts[count($drafts)-1];
        if ((int)$latest['submitted_by'] !== (int)$_SESSION['wa_current_user']->user) { cancel_transaction(); return false; }
        if ((int)$latest['status'] === APPROVAL_STATUS_PENDING) {
            $result = $approval->cancel((int)$latest['id'], _('Reopened purchase requisition'));
            if (!is_array($result) || !isset($result['status']) || $result['status'] !== 'cancelled'
                || purch_requisition_reopen_context() !== $context || !commit_transaction()) { cancel_transaction(); return false; }
            return true;
        }
        if ((int)$latest['status'] !== APPROVAL_STATUS_REJECTED) { cancel_transaction(); return false; }
        foreach ($drafts as $row) if ((int)$row['status'] === APPROVAL_STATUS_PENDING) { cancel_transaction(); return false; }
        $snapshot = purch_requisition_reopen_source($identity, 'rejected', $latest['decoded']);
        if (!$snapshot || purch_requisition_reopen_drafts($identity, (int)$snapshot['header']['material_request_id'], $approval) === false
            || !write_purch_requisition_reopen($snapshot, $context)) { cancel_transaction(); return false; }
        $action = add_approval_action((int)$latest['id'], 'reopen', null, 0, _('Reopened rejected purchase requisition'));
        $actions = $action ? purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'approval_actions WHERE id='.(int)$action.' FOR UPDATE') : false;
        if (!$actions || (int)$actions[0]['draft_id'] !== (int)$latest['id'] || $actions[0]['action_type'] !== 'reopen'
            || (int)$actions[0]['user_id'] !== (int)$_SESSION['wa_current_user']->user
            || !verify_purch_requisition_reopen($snapshot, $context) || !commit_transaction()) { cancel_transaction(); return false; }
        return true;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
