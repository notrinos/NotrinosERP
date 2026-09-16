<?php
/**
 * HRM-ESS-001 maker command for dated direct-team manager delegation.
 *
 * The command records only a pending request. Final delegation authority is
 * created only by the distinct checker command in the approval helper.
 */
if (!function_exists('append_domain_audit_event'))
    include_once($path_to_root.'/includes/db/domain_audit_db.inc');
if (!function_exists('hrm_ess_manager_scope_normalize_date'))
    include_once($path_to_root.'/hrm/includes/db/ess_manager_scope_db.inc');

/** Return the current application actor id. */
function hrm_ess_manager_delegation_actor_id()
{
    if (function_exists('user_id'))
        return (int)user_id();
    if (isset($_SESSION['wa_current_user']->user))
        return (int)$_SESSION['wa_current_user']->user;
    return 0;
}

/** Require the explicit administrative maker authority; no generic manager implication. */
function hrm_ess_manager_delegation_user_can_request()
{
    return function_exists('user_check_access')
        && user_check_access('SA_EMPLOYEE')
        && user_check_access('SA_HRM_MANAGE_ESS_MANAGER_DELEGATION');
}

/** Validate one redacted SHA-256 custody digest. */
function hrm_ess_manager_delegation_command_digest_is_valid($digest)
{
    return is_string($digest) && preg_match('/^[a-f0-9]{64}$/D', $digest) === 1;
}

/** Validate a request nonce without retaining plaintext. */
function hrm_ess_manager_delegation_request_nonce_is_valid($nonce)
{
    return is_string($nonce) && strlen($nonce) >= 16 && strlen($nonce) <= 128
        && preg_match('/^[A-Za-z0-9][A-Za-z0-9_.:-]{15,127}$/D', $nonce) === 1;
}

/** Prove all command custody tables exist before any write. */
function hrm_ess_manager_delegation_command_tables_ready()
{
    foreach (array('hrm_workers','hrm_employments','hrm_assignments',
        'hrm_ess_manager_delegation_requests','hrm_ess_manager_delegations') as $table) {
        $r = db_query("SHOW TABLES LIKE '".TB_PREF.db_escape_function($table)."'");
        if (!$r || db_num_rows($r) !== 1)
            return false;
    }
    return true;
}

/** Lock both Worker principals and the delegator's effective Assignment custody. */
function hrm_ess_manager_delegation_lock_subjects($delegator_worker_id, $delegate_worker_id, $effective_from, $effective_to)
{
    $delegator_worker_id = (int)$delegator_worker_id;
    $delegate_worker_id = (int)$delegate_worker_id;
    $effective_from = hrm_ess_manager_scope_normalize_date($effective_from);
    $effective_to = hrm_ess_manager_scope_normalize_date($effective_to);
    if ($delegator_worker_id <= 0 || $delegate_worker_id <= 0 || $delegator_worker_id === $delegate_worker_id
        || $effective_from === false || $effective_to === false || strcmp($effective_from, $effective_to) > 0)
        return false;

    $ids = $delegator_worker_id.','.$delegate_worker_id;
    $r = db_query('SELECT worker_id,worker_status FROM '.TB_PREF.'hrm_workers WHERE worker_id IN ('.$ids.') ORDER BY worker_id FOR UPDATE',
        'could not lock ESS manager delegation Workers');
    if (!$r || db_num_rows($r) !== 2)
        return false;
    while ($row = db_fetch_assoc($r))
        if ((string)$row['worker_status'] !== 'active')
            return false;

    $r = db_query('SELECT h.employment_id,h.worker_id,h.legal_entity_id,h.start_date,h.end_date,'
        .'a.assignment_id,a.effective_from,a.effective_to FROM '.TB_PREF.'hrm_employments h '
        .'INNER JOIN '.TB_PREF.'hrm_assignments a ON a.employment_id=h.employment_id '
        .'WHERE h.worker_id IN ('.$ids.') ORDER BY h.worker_id,h.employment_id,a.assignment_id FOR UPDATE',
        'could not lock ESS manager delegation Assignment custody');
    if (!$r)
        return false;
    $delegator_from = false;
    $delegator_to = false;
    while ($row = db_fetch_assoc($r)) {
        if ((int)$row['worker_id'] !== $delegator_worker_id)
            continue;
        $employment_from = $row['start_date'] === null || (string)$row['start_date'] <= $effective_from;
        $employment_from = $employment_from && ($row['end_date'] === null || (string)$row['end_date'] >= $effective_from);
        $assignment_from = $row['effective_from'] === null || (string)$row['effective_from'] <= $effective_from;
        $assignment_from = $assignment_from && ($row['effective_to'] === null || (string)$row['effective_to'] >= $effective_from);
        $employment_to = $row['start_date'] === null || (string)$row['start_date'] <= $effective_to;
        $employment_to = $employment_to && ($row['end_date'] === null || (string)$row['end_date'] >= $effective_to);
        $assignment_to = $row['effective_from'] === null || (string)$row['effective_from'] <= $effective_to;
        $assignment_to = $assignment_to && ($row['effective_to'] === null || (string)$row['effective_to'] >= $effective_to);
        if ($employment_from && $assignment_from)
            $delegator_from = true;
        if ($employment_to && $assignment_to)
            $delegator_to = true;
    }
    if (!$delegator_from || !$delegator_to)
        return false;

    return array('delegator_worker_id'=>$delegator_worker_id,'delegate_worker_id'=>$delegate_worker_id,
        'effective_from'=>$effective_from,'effective_to'=>$effective_to,'scope_code'=>'direct_team');
}

/** Read one delegation request, optionally locking it for checker processing. */
function get_hrm_ess_manager_delegation_request($request_id, $for_update = false)
{
    $request_id = (int)$request_id;
    if ($request_id <= 0)
        return false;
    $sql = 'SELECT request_id,delegator_worker_id,delegate_worker_id,scope_code,effective_from,effective_to,evidence_digest,'
        .'request_nonce_hash,request_digest,request_status,requested_by,approved_by,approval_digest,approved_at,expires_at,'
        .'final_delegation_id,row_version,created_at FROM '.TB_PREF.'hrm_ess_manager_delegation_requests WHERE request_id='.$request_id;
    if ($for_update)
        $sql .= ' FOR UPDATE';
    $r = db_query($sql, 'could not read ESS manager delegation request');
    if (!$r)
        return false;
    $row = db_fetch_assoc($r);
    return $row ? $row : null;
}

/** Return false if an already-approved final delegation overlaps this exact pair and interval. */
function hrm_ess_manager_delegation_pair_interval_available($delegator_worker_id, $delegate_worker_id, $effective_from, $effective_to)
{
    $r = db_query('SELECT delegation_id FROM '.TB_PREF.'hrm_ess_manager_delegations WHERE delegator_worker_id='.(int)$delegator_worker_id
        .' AND delegate_worker_id='.(int)$delegate_worker_id." AND scope_code='direct_team'"
        .' AND effective_from<='.db_escape($effective_to).' AND effective_to>='.db_escape($effective_from)
        .' ORDER BY delegation_id FOR UPDATE', 'could not lock overlapping ESS manager delegations');
    return $r ? db_num_rows($r) === 0 : false;
}

/**
 * Maker command. The raw nonce is hash-only custody and the evidence input must
 * already be a redacted reviewed digest. Exact retry returns the same request.
 */
function request_hrm_ess_manager_delegation($delegator_worker_id, $delegate_worker_id, $effective_from, $effective_to, $evidence_digest, $request_nonce)
{
    $delegator_worker_id = (int)$delegator_worker_id;
    $delegate_worker_id = (int)$delegate_worker_id;
    $effective_from = hrm_ess_manager_scope_normalize_date($effective_from);
    $effective_to = hrm_ess_manager_scope_normalize_date($effective_to);
    $evidence_digest = strtolower(trim((string)$evidence_digest));
    $request_nonce = (string)$request_nonce;
    $actor = hrm_ess_manager_delegation_actor_id();
    if (!hrm_ess_manager_delegation_user_can_request() || !hrm_ess_manager_delegation_command_tables_ready()
        || $actor <= 0 || $delegator_worker_id <= 0 || $delegate_worker_id <= 0
        || $delegator_worker_id === $delegate_worker_id || $effective_from === false || $effective_to === false
        || strcmp($effective_from, $effective_to) > 0 || !hrm_ess_manager_delegation_command_digest_is_valid($evidence_digest)
        || !hrm_ess_manager_delegation_request_nonce_is_valid($request_nonce))
        return false;

    $nonce_hash = hash('sha256', 'hrm-ess-manager-delegation-nonce-v1|'.$request_nonce);
    $request_digest = hash('sha256', implode("\0", array('hrm-ess-manager-delegation-request-v1','direct_team',
        $delegator_worker_id,$delegate_worker_id,$effective_from,$effective_to,$evidence_digest,$nonce_hash)));

    begin_transaction();
    $subjects = hrm_ess_manager_delegation_lock_subjects($delegator_worker_id, $delegate_worker_id, $effective_from, $effective_to);
    if ($subjects === false || !hrm_ess_manager_delegation_pair_interval_available($delegator_worker_id, $delegate_worker_id, $effective_from, $effective_to)) {
        cancel_transaction();
        return false;
    }

    $r = db_query('SELECT request_id,delegator_worker_id,delegate_worker_id,scope_code,effective_from,effective_to,evidence_digest,'
        .'request_nonce_hash,request_digest,request_status,requested_by,approved_by,expires_at,row_version '
        .'FROM '.TB_PREF.'hrm_ess_manager_delegation_requests WHERE request_digest='.db_escape($request_digest)
        .' OR request_nonce_hash='.db_escape($nonce_hash).' ORDER BY request_id FOR UPDATE',
        'could not lock existing ESS manager delegation request');
    if (!$r || db_num_rows($r) > 1) {
        cancel_transaction();
        return false;
    }
    if (db_num_rows($r) === 1) {
        $row = db_fetch_assoc($r);
        $ok = (int)$row['delegator_worker_id'] === $delegator_worker_id
            && (int)$row['delegate_worker_id'] === $delegate_worker_id
            && (string)$row['scope_code'] === 'direct_team'
            && (string)$row['effective_from'] === $effective_from && (string)$row['effective_to'] === $effective_to
            && hash_equals((string)$row['evidence_digest'], $evidence_digest)
            && hash_equals((string)$row['request_nonce_hash'], $nonce_hash)
            && hash_equals((string)$row['request_digest'], $request_digest)
            && (string)$row['request_status'] === 'draft' && (int)$row['requested_by'] === $actor
            && $row['approved_by'] === null && (int)$row['row_version'] === 1;
        if ($ok) {
            $t = db_query('SELECT UTC_TIMESTAMP() utc_now', 'could not read ESS manager delegation retry time');
            $now = $t ? db_fetch_assoc($t) : false;
            $ok = $now && (string)$row['expires_at'] > (string)$now['utc_now'];
        }
        if ($ok) {
            commit_transaction();
            return (int)$row['request_id'];
        }
        cancel_transaction();
        return false;
    }

    $ok = db_query('INSERT INTO '.TB_PREF.'hrm_ess_manager_delegation_requests '
        .'(delegator_worker_id,delegate_worker_id,scope_code,effective_from,effective_to,evidence_digest,request_nonce_hash,'
        .'request_digest,request_status,requested_by,approved_by,approval_digest,approved_at,expires_at,final_delegation_id,row_version,created_at) VALUES ('
        .$delegator_worker_id.','.$delegate_worker_id.",'direct_team',".db_escape($effective_from).','.db_escape($effective_to).','
        .db_escape($evidence_digest).','.db_escape($nonce_hash).','.db_escape($request_digest).",'draft',".$actor
        .',NULL,NULL,NULL,DATE_ADD(UTC_TIMESTAMP(), INTERVAL 24 HOUR),NULL,1,UTC_TIMESTAMP())',
        'could not create ESS manager delegation request');
    $request_id = $ok ? (int)db_insert_id() : 0;
    if ($request_id <= 0) {
        cancel_transaction();
        return false;
    }
    $audit = append_domain_audit_event(array(
        'company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,
        'entity_type'=>'hrm_ess_manager_delegation_request','entity_key'=>array('request_id'=>$request_id),
        'action'=>'hrm.ess.manager_delegation.requested','reason_code'=>'explicit_dated_direct_team_delegation_request',
        'before'=>null,'after'=>array('request_id'=>$request_id,'delegator_worker_id'=>$delegator_worker_id,
            'delegate_worker_id'=>$delegate_worker_id,'scope_code'=>'direct_team','effective_from'=>$effective_from,
            'effective_to'=>$effective_to,'maker_actor_id'=>$actor,'checker_required'=>true),
        'restricted_diff'=>array('evidence_digest'=>$evidence_digest,'request_nonce_hash'=>$nonce_hash,
            'raw_nonce_stored'=>false,'final_delegation_created'=>false,'public_routes_added'=>0,
            'automatic_role_grants'=>0,'compensation_scope_added'=>0,'bank_tax_scope_added'=>0)
    ));
    if ($audit === false) {
        cancel_transaction();
        return false;
    }
    commit_transaction();
    return $request_id;
}
