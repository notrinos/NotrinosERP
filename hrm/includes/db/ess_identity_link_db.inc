<?php
/**
 * HRM-ESS-001 explicit application-user <-> Worker identity linkage.
 *
 * This service never infers a link from employees.login_id, login name, email,
 * display name, identifier, federation subject, or any other fuzzy attribute.
 * A privileged maker records one exact account/Worker pair and a distinct
 * privileged checker revalidates the pair under lock before the immutable link
 * is created. No role/permission grant is written by this service.
 */
if (!function_exists('append_domain_audit_event'))
    include_once($path_to_root.'/includes/db/domain_audit_db.inc');

function hrm_ess_identity_link_actor_id()
{
    if (function_exists('user_id'))
        return (int)user_id();
    if (isset($_SESSION['wa_current_user']->user))
        return (int)$_SESSION['wa_current_user']->user;
    return 0;
}

function hrm_ess_identity_link_has_area($area)
{
    return function_exists('user_check_access') && user_check_access($area);
}

function hrm_ess_identity_link_user_can_request()
{
    return hrm_ess_identity_link_has_area('SA_EMPLOYEE')
        && hrm_ess_identity_link_has_area('SA_HRM_VIEW_IDENTITY_DETAILS')
        && hrm_ess_identity_link_has_area('SA_HRM_MANAGE_ESS_IDENTITY_LINK');
}

function hrm_ess_identity_link_user_can_approve()
{
    return hrm_ess_identity_link_has_area('SA_EMPLOYEE')
        && hrm_ess_identity_link_has_area('SA_HRM_VIEW_IDENTITY_DETAILS')
        && hrm_ess_identity_link_has_area('SA_HRM_APPROVE_ESS_IDENTITY_LINK');
}

function hrm_ess_identity_link_digest_is_valid($digest)
{
    return is_string($digest) && preg_match('/^[a-f0-9]{64}$/D', $digest) === 1;
}

function hrm_ess_identity_link_tables_ready()
{
    foreach (array('users','hrm_workers','hrm_employee_worker_map',
        'hrm_ess_identity_link_requests','hrm_ess_identity_links') as $table) {
        $r = db_query("SHOW TABLES LIKE '".TB_PREF.db_escape_function($table)."'");
        if (!$r || db_num_rows($r) !== 1)
            return false;
    }
    return true;
}

/**
 * Lock the exact account/Worker pair and prove the Worker still owns one live
 * legacy compatibility mapping. This intentionally does not read
 * employees.login_id or compare any account/profile attributes.
 */
function hrm_ess_identity_link_lock_subjects($user_id, $worker_id)
{
    $user_id = (int)$user_id;
    $worker_id = (int)$worker_id;
    if ($user_id <= 0 || $worker_id <= 0)
        return false;

    $ur = db_query('SELECT id,inactive FROM '.TB_PREF.'users WHERE id='.$user_id.' FOR UPDATE',
        'could not lock ESS identity account');
    $user = $ur ? db_fetch_assoc($ur) : false;
    if (!$user || (int)$user['id'] !== $user_id || (int)$user['inactive'] !== 0)
        return false;

    $wr = db_query('SELECT w.worker_id,w.person_id,w.worker_status,m.mapping_id,m.employee_number,m.employee_id,m.retired_at '
        .'FROM '.TB_PREF.'hrm_workers w INNER JOIN '.TB_PREF.'hrm_employee_worker_map m ON m.worker_id=w.worker_id '
        .'WHERE w.worker_id='.$worker_id." AND w.worker_status='active' AND m.retired_at IS NULL FOR UPDATE",
        'could not lock ESS identity Worker');
    $worker = $wr ? db_fetch_assoc($wr) : false;
    if (!$worker || (int)$worker['worker_id'] !== $worker_id || (int)$worker['mapping_id'] <= 0)
        return false;
    if ($wr && db_num_rows($wr) !== 1)
        return false;

    return array('user_id'=>$user_id,'worker_id'=>$worker_id,
        'person_id'=>(int)$worker['person_id'],'mapping_id'=>(int)$worker['mapping_id'],
        'employee_number'=>(int)$worker['employee_number'],'employee_id'=>(string)$worker['employee_id']);
}

function get_hrm_ess_identity_link_request($request_id, $for_update = false)
{
    $request_id = (int)$request_id;
    if ($request_id <= 0)
        return false;
    $sql = 'SELECT request_id,user_id,worker_id,evidence_digest,request_nonce_hash,request_digest,requested_at,requested_by '
        .'FROM '.TB_PREF.'hrm_ess_identity_link_requests WHERE request_id='.$request_id;
    if ($for_update)
        $sql .= ' FOR UPDATE';
    $r = db_query($sql, 'could not read ESS identity-link request');
    if (!$r)
        return false;
    $row = db_fetch_assoc($r);
    return $row ? $row : null;
}

function get_hrm_ess_identity_link_for_request($request_id, $for_update = false)
{
    $request_id = (int)$request_id;
    if ($request_id <= 0)
        return false;
    $sql = 'SELECT link_id,request_id,user_id,worker_id,approval_digest,link_digest,linked_at,linked_by '
        .'FROM '.TB_PREF.'hrm_ess_identity_links WHERE request_id='.$request_id;
    if ($for_update)
        $sql .= ' FOR UPDATE';
    $r = db_query($sql, 'could not read ESS identity link');
    if (!$r)
        return false;
    $row = db_fetch_assoc($r);
    return $row ? $row : null;
}

function hrm_ess_identity_link_pair_available($user_id, $worker_id)
{
    $r = db_query('SELECT link_id,user_id,worker_id FROM '.TB_PREF.'hrm_ess_identity_links '
        .'WHERE user_id='.(int)$user_id.' OR worker_id='.(int)$worker_id.' ORDER BY link_id FOR UPDATE',
        'could not lock ESS identity-link conflicts');
    if (!$r)
        return false;
    return db_num_rows($r) === 0;
}

/**
 * Maker command. $request_nonce is never persisted; only SHA-256 is retained.
 * $evidence_digest must already be a redacted evidence digest produced by the
 * reviewed administrative workflow. The command adds no route and grants no
 * account role/access area.
 */
function request_hrm_ess_identity_link($user_id, $worker_id, $evidence_digest, $request_nonce)
{
    $user_id = (int)$user_id;
    $worker_id = (int)$worker_id;
    $evidence_digest = strtolower(trim((string)$evidence_digest));
    $request_nonce = (string)$request_nonce;
    $actor = hrm_ess_identity_link_actor_id();
    if (!hrm_ess_identity_link_user_can_request() || !hrm_ess_identity_link_tables_ready()
        || $actor <= 0 || $actor === $user_id || $user_id <= 0 || $worker_id <= 0
        || !hrm_ess_identity_link_digest_is_valid($evidence_digest) || strlen($request_nonce) < 16)
        return false;

    $nonce_hash = hash('sha256', $request_nonce);
    $request_digest = hash('sha256', implode("\0", array(
        'hrm-ess-identity-link-request-v1',$user_id,$worker_id,$evidence_digest,$nonce_hash)));

    begin_transaction();
    $subjects = hrm_ess_identity_link_lock_subjects($user_id, $worker_id);
    if ($subjects === false || !hrm_ess_identity_link_pair_available($user_id, $worker_id)) {
        cancel_transaction();
        return false;
    }
    $r = db_query('SELECT request_id,user_id,worker_id,evidence_digest,request_nonce_hash,request_digest,requested_by '
        .'FROM '.TB_PREF.'hrm_ess_identity_link_requests WHERE request_digest='.db_escape($request_digest).' FOR UPDATE',
        'could not lock existing ESS identity-link request');
    if (!$r) {
        cancel_transaction();
        return false;
    }
    if (db_num_rows($r) === 1) {
        $row = db_fetch_assoc($r);
        $ok = (int)$row['user_id'] === $user_id && (int)$row['worker_id'] === $worker_id
            && (int)$row['requested_by'] === $actor
            && hash_equals((string)$row['evidence_digest'], $evidence_digest)
            && hash_equals((string)$row['request_nonce_hash'], $nonce_hash)
            && hash_equals((string)$row['request_digest'], $request_digest);
        if ($ok) {
            commit_transaction();
            return (int)$row['request_id'];
        }
        cancel_transaction();
        return false;
    }
    if (db_num_rows($r) !== 0) {
        cancel_transaction();
        return false;
    }

    $now = date('Y-m-d H:i:s');
    $ok = db_query('INSERT INTO '.TB_PREF.'hrm_ess_identity_link_requests '
        .'(user_id,worker_id,evidence_digest,request_nonce_hash,request_digest,requested_at,requested_by) VALUES ('
        .$user_id.','.$worker_id.','.db_escape($evidence_digest).','.db_escape($nonce_hash).','
        .db_escape($request_digest).','.db_escape($now).','.$actor.')', 'could not create ESS identity-link request');
    $request_id = $ok ? (int)db_insert_id() : 0;
    if ($request_id <= 0) {
        cancel_transaction();
        return false;
    }
    $audit = append_domain_audit_event(array(
        'company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,
        'entity_type'=>'hrm_ess_identity_link_request','entity_key'=>array('request_id'=>$request_id),
        'action'=>'hrm.ess.identity_link.requested','reason_code'=>'explicit_reviewed_user_worker_pair',
        'before'=>null,'after'=>array('request_id'=>$request_id,'user_id'=>$user_id,'worker_id'=>$worker_id,
            'maker_actor_id'=>$actor,'checker_required'=>true,'legacy_login_id_inference'=>false),
        'restricted_diff'=>array('evidence_digest'=>$evidence_digest,'request_nonce_hash'=>$nonce_hash,
            'raw_nonce_stored'=>false,'automatic_role_grants'=>0,'salary_scope_added'=>0)
    ));
    if ($audit === false) {
        cancel_transaction();
        return false;
    }
    commit_transaction();
    return $request_id;
}

/** Distinct checker command; exact retry is idempotent only for the same request pair. */
function approve_hrm_ess_identity_link($request_id, $approval_digest)
{
    $request_id = (int)$request_id;
    $approval_digest = strtolower(trim((string)$approval_digest));
    $actor = hrm_ess_identity_link_actor_id();
    if (!hrm_ess_identity_link_user_can_approve() || !hrm_ess_identity_link_tables_ready()
        || $actor <= 0 || $request_id <= 0 || !hrm_ess_identity_link_digest_is_valid($approval_digest))
        return false;

    begin_transaction();
    $request = get_hrm_ess_identity_link_request($request_id, true);
    if (!is_array($request) || (int)$request['requested_by'] <= 0
        || (int)$request['requested_by'] === $actor || (int)$request['user_id'] === $actor
        || !hrm_ess_identity_link_digest_is_valid((string)$request['evidence_digest'])
        || !hrm_ess_identity_link_digest_is_valid((string)$request['request_digest'])) {
        cancel_transaction();
        return false;
    }
    $existing = get_hrm_ess_identity_link_for_request($request_id, true);
    if ($existing === false) {
        cancel_transaction();
        return false;
    }
    if (is_array($existing)) {
        $ok = (int)$existing['user_id'] === (int)$request['user_id']
            && (int)$existing['worker_id'] === (int)$request['worker_id']
            && hash_equals((string)$existing['approval_digest'], $approval_digest);
        if ($ok) {
            commit_transaction();
            return (int)$existing['link_id'];
        }
        cancel_transaction();
        return false;
    }

    $subjects = hrm_ess_identity_link_lock_subjects((int)$request['user_id'], (int)$request['worker_id']);
    if ($subjects === false || !hrm_ess_identity_link_pair_available((int)$request['user_id'], (int)$request['worker_id'])) {
        cancel_transaction();
        return false;
    }
    $link_digest = hash('sha256', implode("\0", array('hrm-ess-identity-link-v1',
        (string)$request['request_digest'],$approval_digest,(int)$request['user_id'],(int)$request['worker_id'])));
    $now = date('Y-m-d H:i:s');
    $ok = db_query('INSERT INTO '.TB_PREF.'hrm_ess_identity_links '
        .'(request_id,user_id,worker_id,approval_digest,link_digest,linked_at,linked_by) VALUES ('
        .$request_id.','.(int)$request['user_id'].','.(int)$request['worker_id'].','
        .db_escape($approval_digest).','.db_escape($link_digest).','.db_escape($now).','.$actor.')',
        'could not create ESS identity link');
    $link_id = $ok ? (int)db_insert_id() : 0;
    if ($link_id <= 0) {
        cancel_transaction();
        return false;
    }
    $audit = append_domain_audit_event(array(
        'company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,
        'entity_type'=>'hrm_ess_identity_link','entity_key'=>array('link_id'=>$link_id,'request_id'=>$request_id),
        'action'=>'hrm.ess.identity_link.approved','reason_code'=>'distinct_checker_explicit_user_worker_pair',
        'before'=>null,'after'=>array('link_id'=>$link_id,'request_id'=>$request_id,
            'user_id'=>(int)$request['user_id'],'worker_id'=>(int)$request['worker_id'],
            'maker_actor_id'=>(int)$request['requested_by'],'checker_actor_id'=>$actor,
            'legacy_login_id_inference'=>false,'email_name_identifier_matching'=>false),
        'restricted_diff'=>array('approval_digest'=>$approval_digest,'link_digest'=>$link_digest,
            'automatic_role_grants'=>0,'manager_scope_added'=>0,'salary_scope_added'=>0,'bank_tax_route_added'=>0)
    ));
    if ($audit === false) {
        cancel_transaction();
        return false;
    }
    commit_transaction();
    return $link_id;
}
