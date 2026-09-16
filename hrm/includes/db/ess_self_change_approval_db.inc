<?php
/**
 * HRM-ESS-001 distinct-checker approval for bounded self-profile changes.
 *
 * Approval validates custody and marks the request approved. It deliberately
 * does not apply requested values to Person, Worker, legacy employee, payroll,
 * or accounting state.
 */
if (!function_exists('request_hrm_ess_self_profile_change'))
    include_once($path_to_root.'/hrm/includes/db/ess_self_change_request_db.inc');

/** Require existing employee authority plus the dedicated checker authority. */
function hrm_ess_self_change_user_can_approve()
{
    return function_exists('user_check_access')
        && user_check_access('SA_EMPLOYEE')
        && user_check_access('SA_HRM_APPROVE_ESS_SELF_PROFILE_CHANGE');
}

/** Approve one exact encrypted request as a checker distinct from its maker. */
function approve_hrm_ess_self_profile_change($request_id, $approval_digest)
{
    $request_id = (int)$request_id;
    $approval_digest = strtolower(trim((string)$approval_digest));
    $actor = hrm_ess_self_change_actor_id();
    if ($actor <= 0 || $request_id <= 0 || !hrm_ess_self_change_user_can_approve()
        || !hrm_ess_self_change_digest_is_valid($approval_digest) || !hrm_ess_self_change_tables_ready())
        return false;

    begin_transaction();
    $request = get_hrm_ess_self_change_request($request_id, true);
    if (!is_array($request) || (int)$request['requested_by'] <= 0 || (int)$request['requested_by']===$actor
        || (int)$request['user_id'] !== (int)$request['requested_by']
        || (string)$request['field_set_code'] !== 'contact_private_v1'
        || !hrm_ess_self_change_digest_is_valid((string)$request['payload_digest'])
        || !hrm_ess_self_change_digest_is_valid((string)$request['baseline_auth_tag'])
        || !hrm_ess_self_change_digest_is_valid((string)$request['request_digest'])
        || !hash_equals((string)$request['request_digest'], hrm_ess_self_change_request_digest(
            (int)$request['user_id'], (int)$request['worker_id'], (string)$request['payload_digest'],
            (string)$request['baseline_auth_tag'], (string)$request['request_nonce_hash']))) {
        cancel_transaction();
        return false;
    }

    $locked = hrm_ess_self_change_link_matches((int)$request['user_id'], (int)$request['worker_id']);
    $payload = $locked ? hrm_ess_self_change_open_payload($request) : false;
    if (!$locked || $payload === false) {
        cancel_transaction();
        return false;
    }

    if ((string)$request['request_status'] === 'approved') {
        $ok = (int)$request['approved_by'] === $actor
            && hash_equals((string)$request['approval_digest'], $approval_digest)
            && (int)$request['row_version'] === 2;
        if ($ok) {
            commit_transaction();
            return true;
        }
        cancel_transaction();
        return false;
    }

    $time_result = db_query('SELECT UTC_TIMESTAMP() utc_now', 'could not read ESS self change approval time');
    $now = $time_result ? db_fetch_assoc($time_result) : false;
    if ((string)$request['request_status'] !== 'draft' || (int)$request['row_version'] !== 1
        || $request['approved_by'] !== null || $request['approval_digest'] !== null || !$now
        || (string)$request['expires_at']<=(string)$now['utc_now']) {
        cancel_transaction();
        return false;
    }

    $ok = db_query('UPDATE '.TB_PREF."hrm_ess_self_change_requests SET request_status='approved',approved_by=".$actor
        .',approval_digest='.db_escape($approval_digest).',approved_at=UTC_TIMESTAMP(),row_version=2 WHERE request_id='
        .$request_id." AND request_status='draft' AND row_version=1 AND approved_by IS NULL",
        'could not approve ESS self change request') !== false;
    if (!$ok || (function_exists('db_num_affected_rows') && db_num_affected_rows() !== 1)) {
        cancel_transaction();
        return false;
    }

    $audit = append_domain_audit_event(array(
        'company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,
        'entity_type'=>'hrm_ess_self_change_request',
        'entity_key'=>array('request_id'=>$request_id,'worker_id'=>(int)$request['worker_id']),
        'action'=>'hrm.ess.self_change.approved','reason_code'=>'distinct_checker_bounded_self_profile_change',
        'before'=>array('request_status'=>'draft'),
        'after'=>array('request_status'=>'approved','field_set_code'=>'contact_private_v1','field_names'=>array_keys($payload),
            'maker_actor_id'=>(int)$request['requested_by'],'checker_actor_id'=>$actor),
        'restricted_diff'=>array('values_logged'=>false,'payload_integrity_revalidated'=>true,
            'identity_link_revalidated'=>true,'checker_distinct_from_maker'=>true,'profile_mutation_performed'=>false,
            'bank_tax_fields_allowed'=>false,'compensation_fields_allowed'=>false,'public_routes_added'=>0,
            'historical_payroll_accounting_rows_rewritten'=>0)
    ));
    if ($audit === false) {
        cancel_transaction();
        return false;
    }
    commit_transaction();
    return true;
}
