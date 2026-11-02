<?php
/** Receipt-bound recovery of an unconsumed replacement review; no decision reset. */
require_once __DIR__.'/warehouse_material_transfer_release_db.inc';

/** Resolve the immediately preceding release or receipt-bound terminal retry. */
function material_submission_previous_release($history, $draft, $data)
{
    $previous = null; $submit = null; $prefix = array('drafts'=>array(), 'actions'=>array());
    foreach ($history['drafts'] as $row)
        if ((int)$row['id'] < (int)$draft['id']) $prefix['drafts'][] = $row;
    foreach ($history['actions'] as $row) {
        if ((int)$row['draft_id'] === (int)$draft['id'] && $row['action_type'] === 'submit') { $submit = $row; break; }
        $prefix['actions'][] = $row; $previous = $row;
    }
    if (!$previous || !$submit) return false;
    if ($previous['action_type'] === 'source_release')
        return !isset($data['material_previous_terminal_id']) ? $previous : false;
    $terminal = material_terminal_review_receipt((int)$data['request_id'], $prefix);
    if (!$terminal || !isset($data['material_previous_terminal_id'], $data['material_previous_release_id'])
        || !is_int($data['material_previous_terminal_id'])
        || $data['material_previous_terminal_id'] !== (int)$previous['id']
        || $data['material_previous_release_id'] !== $terminal['data']['material_previous_release_id']
        || (int)$submit['id'] <= (int)$previous['id'] || $submit['action_date'] < $previous['action_date']
        || $draft['submitted_date'] < $previous['action_date']
        || (int)$terminal['draft']['submitted_by'] !== (int)$draft['submitted_by']) return false;
    foreach ($prefix['actions'] as $row)
        if ((int)$row['id'] === $data['material_previous_release_id'] && $row['action_type'] === 'source_release') return $row;
    return false;
}

/** Verify one complete rejected/cancelled native terminal prefix without writes. */
function material_terminal_review_receipt($identity, $history)
{
    if (!material_core_context() || empty($GLOBALS['transaction_level'])
        || !isset($history['drafts'], $history['actions']) || !$history['drafts'] || !$history['actions']) return false;
    $draft = $history['drafts'][count($history['drafts'])-1];
    $prior = $history; $action = array_pop($prior['actions']);
    $approval = get_approval_workflow_service();
    $data = $approval->decodeStoredDraftData($draft['draft_data']);
    $receipt = $approval->decodeStoredDraftData($action['draft_data_snapshot']);
    $status = (int)$draft['status'];
    $kind = $status === APPROVAL_STATUS_REJECTED ? 'reject' : ($status === APPROVAL_STATUS_CANCELLED ? 'cancel' : false);
    if (!$kind || !is_array($data) || !is_array($receipt)
        || !isset($data['request_id'], $data['material_company_sha256'], $data['material_source_sha256'], $data['material_previous_release_id'])
        || dimension_writer_reference_id($data['request_id']) !== (int)$identity
        || !is_int($data['material_previous_release_id']) || $data['material_previous_release_id'] <= 0
        || $data['material_company_sha256'] !== material_core_context(true)
        || (int)$action['draft_id'] !== (int)$draft['id'] || $action['action_type'] !== $kind
        || (int)$action['level'] !== (int)$draft['current_level'] || $draft['approved_trans_no'] !== null
        || !material_transfer_v2_timestamp($draft['completed_date'])
        || !material_transfer_v2_timestamp($action['action_date'])
        || $draft['completed_date'] < $draft['submitted_date'] || $action['action_date'] < $draft['completed_date']
        || ($kind === 'reject' && (int)$action['user_id'] === (int)$draft['submitted_by'])
        || ($kind === 'cancel' && (int)$action['user_id'] !== (int)$draft['submitted_by'])) return false;
    $unsigned = $action; $unsigned['draft_data_snapshot'] = null;
    $expected = array('schema'=>'material-terminal-source-v1', 'request_id'=>(int)$identity,
        'draft_id'=>(int)$draft['id'], 'company_sha256'=>material_core_context(true),
        'submitted_source_sha256'=>$data['material_source_sha256'],
        'history_sha256'=>material_transfer_release_history_hash($prior),
        'action_sha256'=>material_core_source_fingerprint(array('header'=>$unsigned, 'lines'=>array())),
        'source_sha256'=>isset($receipt['source_sha256']) ? $receipt['source_sha256'] : null);
    if ($receipt !== $expected || !is_string($expected['source_sha256'])
        || !preg_match('/^[a-f0-9]{64}$/D', $expected['source_sha256'])) return false;
    return array('receipt'=>$receipt, 'action'=>$action, 'draft'=>$draft, 'data'=>$data);
}

/**
 * Current recovery admission requires the exact new terminal receipt AND its
 * prior immutable v2 release. Historical reads alone never grant resubmission.
 * All source, line, review and Transfer locks use the native family lock order.
 */
function material_replacement_recovery_custody($identity, $history)
{
    if (!transfer_core_context()) return false;
    $terminal = material_terminal_review_receipt($identity, $history);
    if (!$terminal) return false;
    $status = (int)$terminal['draft']['status'] === APPROVAL_STATUS_REJECTED ? 'draft' : 'cancelled';
    $source = material_core_source((int)$identity, $status);
    if (!$source || (int)$source['header']['requested_by'] !== (int)$_SESSION['wa_current_user']->user
        || (int)$terminal['draft']['submitted_by'] !== (int)$source['header']['requested_by']
        || material_core_source_fingerprint($source) !== $terminal['receipt']['source_sha256']) return false;
    $release = material_historical_transfer_release($identity, $terminal['data']['material_previous_release_id']);
    if (!$release || $release['receipt']['schema'] !== 'material-transfer-release-v2') return false;
    // Reconstruct only fields changed by native submit and terminal commands.
    // Any edit, fulfilled quantity, link, old checker or opaque field drift denies.
    $expected = $release['source'];
    $expected['header']['status'] = $source['header']['status'];
    $expected['header']['updated_at'] = $source['header']['updated_at'];
    if ($source !== $expected) return false;
    return array('source'=>$source, 'release'=>$release, 'terminal'=>$terminal);
}
