<?php
/** HRM-ESS-001 distinct-checker finalization for dated direct-team delegation. */
if (!function_exists('request_hrm_ess_manager_delegation'))
    include_once($path_to_root.'/hrm/includes/db/ess_manager_delegation_request_db.inc');

/** Require the explicit checker authority; no generic manager implication. */
function hrm_ess_manager_delegation_user_can_approve()
{
    return function_exists('user_check_access')
        && user_check_access('SA_EMPLOYEE')
        && user_check_access('SA_HRM_APPROVE_ESS_MANAGER_DELEGATION');
}

/** Read and validate one final approved delegation against its request custody. */
function hrm_ess_manager_delegation_final_matches_request($delegation_id, $request, $approval_digest)
{
    $delegation_id = (int)$delegation_id;
    if ($delegation_id <= 0 || !is_array($request))
        return false;
    $r = db_query('SELECT delegation_id,delegator_worker_id,delegate_worker_id,scope_code,effective_from,effective_to,'
        .'evidence_digest,approval_digest,delegation_digest,requested_by,approved_by FROM '.TB_PREF.'hrm_ess_manager_delegations '
        .'WHERE delegation_id='.$delegation_id.' FOR UPDATE', 'could not lock final ESS manager delegation');
    $row = $r ? db_fetch_assoc($r) : false;
    return $row
        && (int)$row['delegator_worker_id'] === (int)$request['delegator_worker_id']
        && (int)$row['delegate_worker_id'] === (int)$request['delegate_worker_id']
        && (string)$row['scope_code'] === 'direct_team'
        && (string)$row['effective_from'] === (string)$request['effective_from']
        && (string)$row['effective_to'] === (string)$request['effective_to']
        && hash_equals((string)$row['evidence_digest'], (string)$request['evidence_digest'])
        && hash_equals((string)$row['approval_digest'], (string)$approval_digest)
        && hrm_ess_manager_delegation_command_digest_is_valid(strtolower((string)$row['delegation_digest']))
        && (int)$row['requested_by'] === (int)$request['requested_by']
        && (int)$row['approved_by'] === (int)$request['approved_by'];
}

/** Distinct-checker command; exact approved retry returns the same final delegation id. */
function approve_hrm_ess_manager_delegation($request_id, $approval_digest)
{
    $request_id = (int)$request_id;
    $approval_digest = strtolower(trim((string)$approval_digest));
    $actor = hrm_ess_manager_delegation_actor_id();
    if (!hrm_ess_manager_delegation_user_can_approve() || !hrm_ess_manager_delegation_command_tables_ready()
        || $actor <= 0 || $request_id <= 0 || !hrm_ess_manager_delegation_command_digest_is_valid($approval_digest))
        return false;

    begin_transaction();
    $request = get_hrm_ess_manager_delegation_request($request_id, true);
    if (!is_array($request) || (int)$request['requested_by'] <= 0 || (int)$request['requested_by'] === $actor
        || (string)$request['scope_code'] !== 'direct_team'
        || !hrm_ess_manager_delegation_command_digest_is_valid(strtolower((string)$request['evidence_digest']))
        || !hrm_ess_manager_delegation_command_digest_is_valid(strtolower((string)$request['request_digest']))) {
        cancel_transaction();
        return false;
    }

    if ((string)$request['request_status'] === 'approved') {
        $ok = (int)$request['approved_by'] === $actor
            && hash_equals((string)$request['approval_digest'], $approval_digest)
            && (int)$request['final_delegation_id'] > 0
            && hrm_ess_manager_delegation_final_matches_request((int)$request['final_delegation_id'], $request, $approval_digest);
        if ($ok) {
            commit_transaction();
            return (int)$request['final_delegation_id'];
        }
        cancel_transaction();
        return false;
    }

    $t = db_query('SELECT UTC_TIMESTAMP() utc_now', 'could not read ESS manager delegation approval time');
    $now = $t ? db_fetch_assoc($t) : false;
    if ((string)$request['request_status'] !== 'draft' || (int)$request['row_version'] !== 1
        || $request['approved_by'] !== null || $request['approval_digest'] !== null || $request['final_delegation_id'] !== null
        || !$now || (string)$request['expires_at'] <= (string)$now['utc_now']) {
        cancel_transaction();
        return false;
    }

    $subjects = hrm_ess_manager_delegation_lock_subjects((int)$request['delegator_worker_id'], (int)$request['delegate_worker_id'],
        (string)$request['effective_from'], (string)$request['effective_to']);
    if ($subjects === false || !hrm_ess_manager_delegation_pair_interval_available((int)$request['delegator_worker_id'],
        (int)$request['delegate_worker_id'], (string)$request['effective_from'], (string)$request['effective_to'])) {
        cancel_transaction();
        return false;
    }

    $delegation_digest = hash('sha256', implode("\0", array('hrm-ess-manager-delegation-v1',(string)$request['request_digest'],
        $approval_digest,(int)$request['delegator_worker_id'],(int)$request['delegate_worker_id'],'direct_team',
        (string)$request['effective_from'],(string)$request['effective_to'],(int)$request['requested_by'],$actor)));
    $ok = db_query('INSERT INTO '.TB_PREF.'hrm_ess_manager_delegations '
        .'(delegator_worker_id,delegate_worker_id,scope_code,effective_from,effective_to,evidence_digest,approval_digest,'
        .'delegation_digest,requested_by,approved_by,created_at) VALUES ('
        .(int)$request['delegator_worker_id'].','.(int)$request['delegate_worker_id'].",'direct_team',"
        .db_escape((string)$request['effective_from']).','.db_escape((string)$request['effective_to']).','
        .db_escape((string)$request['evidence_digest']).','.db_escape($approval_digest).','.db_escape($delegation_digest).','
        .(int)$request['requested_by'].','.$actor.',UTC_TIMESTAMP())', 'could not create approved ESS manager delegation');
    $delegation_id = $ok ? (int)db_insert_id() : 0;
    if ($delegation_id <= 0) {
        cancel_transaction();
        return false;
    }

    $ok = db_query('UPDATE '.TB_PREF.'hrm_ess_manager_delegation_requests SET request_status=\'approved\',approved_by='.$actor
        .',approval_digest='.db_escape($approval_digest).',approved_at=UTC_TIMESTAMP(),final_delegation_id='.$delegation_id
        .',row_version=2 WHERE request_id='.$request_id." AND request_status='draft' AND row_version=1 AND approved_by IS NULL AND final_delegation_id IS NULL",
        'could not finalize ESS manager delegation request') !== false;
    if (!$ok || (function_exists('db_num_affected_rows') && db_num_affected_rows() !== 1)) {
        cancel_transaction();
        return false;
    }

    $audit = append_domain_audit_event(array(
        'company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,
        'entity_type'=>'hrm_ess_manager_delegation','entity_key'=>array('delegation_id'=>$delegation_id,'request_id'=>$request_id),
        'action'=>'hrm.ess.manager_delegation.approved','reason_code'=>'distinct_checker_dated_direct_team_delegation',
        'before'=>array('request_status'=>'draft','final_delegation_id'=>null),
        'after'=>array('request_status'=>'approved','delegation_id'=>$delegation_id,
            'delegator_worker_id'=>(int)$request['delegator_worker_id'],'delegate_worker_id'=>(int)$request['delegate_worker_id'],
            'scope_code'=>'direct_team','effective_from'=>(string)$request['effective_from'],'effective_to'=>(string)$request['effective_to'],
            'maker_actor_id'=>(int)$request['requested_by'],'checker_actor_id'=>$actor),
        'restricted_diff'=>array('approval_digest'=>$approval_digest,'delegation_digest'=>$delegation_digest,
            'checker_distinct_from_maker'=>true,'subjects_revalidated_under_lock'=>true,'overlap_revalidated_under_lock'=>true,
            'public_routes_added'=>0,'automatic_role_grants'=>0,'compensation_scope_added'=>0,'bank_tax_scope_added'=>0,
            'historical_payroll_accounting_rows_rewritten'=>0)
    ));
    if ($audit === false) {
        cancel_transaction();
        return false;
    }
    commit_transaction();
    return $delegation_id;
}
