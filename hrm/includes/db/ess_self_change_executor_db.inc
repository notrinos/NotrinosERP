<?php
/**
 * HRM-ESS-001 approved bounded self-profile application executor.
 *
 * Normalized Person custody is updated first. The legacy employee row is then
 * materialized strictly from the normalized projection. No public route lives
 * here and bank/tax/identifier/compensation fields are never accepted.
 */
if (!function_exists('approve_hrm_ess_self_profile_change'))
    include_once($path_to_root.'/hrm/includes/db/ess_self_change_approval_db.inc');
if (!function_exists('hrm_employee_projection_store'))
    include_once($path_to_root.'/hrm/includes/db/employee_compatibility_projector_db.inc');
if (!function_exists('synchronize_hrm_person_sensitive_value'))
    include_once($path_to_root.'/hrm/includes/db/employee_person_sensitive_db.inc');
if (!function_exists('synchronize_hrm_person_address_shadow'))
    include_once($path_to_root.'/hrm/includes/db/employee_person_private_profile_db.inc');

function hrm_ess_self_change_user_can_execute()
{
    return function_exists('user_check_access')
        && user_check_access('SA_EMPLOYEE')
        && user_check_access('SA_HRM_EXECUTE_ESS_SELF_PROFILE_CHANGE');
}

function hrm_ess_self_change_execution_nonce_is_valid($nonce)
{
    return is_string($nonce) && strlen($nonce) >= 16 && strlen($nonce) <= 128
        && preg_match('/^[A-Za-z0-9][A-Za-z0-9_.:-]{15,127}$/D', $nonce) === 1;
}

function hrm_ess_self_change_execution_digest($request, $nonce_hash, $executor_id)
{
    if (!is_array($request) || !isset($request['request_digest'], $request['approval_digest']))
        return false;
    return hash('sha256', implode("\0", array('hrm-ess-self-change-execution-v1',
        (string)$request['request_digest'], (string)$request['approval_digest'],
        (string)$nonce_hash, (int)$executor_id)));
}

function hrm_ess_self_change_execution_tables_ready()
{
    foreach (array('employees','hrm_ess_self_change_execution_receipts','hrm_person_contacts',
        'hrm_person_addresses','hrm_person_emergency_contacts') as $table) {
        $result = db_query("SHOW TABLES LIKE '".TB_PREF.db_escape_function($table)."'");
        if (!$result || db_num_rows($result) !== 1)
            return false;
    }
    return hrm_ess_self_change_tables_ready();
}

/** Recompute the authenticated changed-field baseline from current normalized custody. */
function hrm_ess_self_change_baseline_matches($request, $payload, $current)
{
    if (!is_array($request) || !is_array($payload) || !is_array($current))
        return false;
    $baseline = hrm_ess_self_change_baseline_values($current, array_keys($payload));
    $json = json_encode($baseline);
    $key = hrm_person_sensitive_key((string)$request['payload_key_id']);
    if ($json === false || $key === false)
        return false;
    $expected = hash_hmac('sha256', $json, $key);
    return hrm_ess_self_change_digest_is_valid((string)$request['baseline_auth_tag'])
        && hash_equals((string)$request['baseline_auth_tag'], $expected);
}

/** Apply only the frozen contact/private allowlist to normalized Person custody. */
function hrm_ess_self_change_apply_normalized($binding, $current, $payload)
{
    if (!is_array($binding) || !is_array($current) || !is_array($payload)
        || empty($binding['person_id']))
        return false;
    $merged = $current;
    foreach ($payload as $field=>$value)
        $merged[$field] = $value;

    $active_key = hrm_person_sensitive_active_key(null, false);
    if ($active_key === false)
        return false;
    $contact_map = array('phone'=>'phone','mobile'=>'mobile','email'=>'work_email','personal_email'=>'personal_email');
    foreach ($contact_map as $field=>$type) {
        if (!array_key_exists($field, $payload))
            continue;
        $changed = synchronize_hrm_person_sensitive_value('contact', (int)$binding['person_id'],
            $type, (string)$payload[$field], null, 'ess_self_change', $active_key);
        if ($changed === false)
            return false;
    }

    $address_fields = array('address','city','state','country');
    if (array_intersect($address_fields, array_keys($payload))) {
        $snapshot = hrm_person_private_profile_address_snapshot($merged);
        if ($snapshot === false || synchronize_hrm_person_address_shadow(
            (int)$binding['person_id'], $snapshot, 'ess_self_change') === false)
            return false;
    }
    $emergency_fields = array('emergency_name','emergency_relation','emergency_phone');
    if (array_intersect($emergency_fields, array_keys($payload))) {
        $snapshot = hrm_person_private_profile_emergency_snapshot($merged);
        if ($snapshot === false || synchronize_hrm_person_emergency_contact_shadow(
            (int)$binding['person_id'], $snapshot, 'ess_self_change') === false)
            return false;
    }
    return true;
}

/**
 * Apply one approved request exactly once.
 * Exact retry returns the immutable receipt only for the same nonce/executor.
 */
function execute_hrm_ess_self_profile_change($request_id, $execution_nonce)
{
    $request_id = (int)$request_id;
    $actor = hrm_ess_self_change_actor_id();
    $nonce = (string)$execution_nonce;
    if ($actor <= 0 || $request_id <= 0 || !hrm_ess_self_change_user_can_execute()
        || !hrm_ess_self_change_execution_nonce_is_valid($nonce)
        || !hrm_ess_self_change_execution_tables_ready())
        return false;
    $nonce_hash = hash('sha256', $nonce);

    begin_transaction();
    $request = get_hrm_ess_self_change_request($request_id, true);
    if (!is_array($request) || (string)$request['field_set_code'] !== 'contact_private_v1'
        || (int)$request['requested_by'] <= 0 || (int)$request['requested_by'] === $actor
        || !hrm_ess_self_change_digest_is_valid((string)$request['request_digest'])
        || !hrm_ess_self_change_digest_is_valid((string)$request['approval_digest'])) {
        cancel_transaction(); return false;
    }
    $execution_digest = hrm_ess_self_change_execution_digest($request, $nonce_hash, $actor);
    if ($execution_digest === false) { cancel_transaction(); return false; }

    $receipt_result = db_query('SELECT * FROM '.TB_PREF.'hrm_ess_self_change_execution_receipts WHERE request_id='.
        $request_id.' FOR UPDATE', 'could not lock ESS self change execution receipt');
    if (!$receipt_result) { cancel_transaction(); return false; }
    if (db_num_rows($receipt_result) === 1) {
        $receipt = db_fetch_assoc($receipt_result);
        $ok = (string)$request['request_status'] === 'applied' && (int)$request['row_version'] === 3
            && (int)$receipt['executor_id'] === $actor
            && hash_equals((string)$receipt['execution_nonce_hash'], $nonce_hash)
            && hash_equals((string)$receipt['execution_digest'], $execution_digest)
            && hash_equals((string)$receipt['request_digest'], (string)$request['request_digest'])
            && hash_equals((string)$receipt['approval_digest'], (string)$request['approval_digest']);
        if ($ok) { commit_transaction(); return (int)$receipt['receipt_id']; }
        cancel_transaction(); return false;
    }
    if (db_num_rows($receipt_result) !== 0 || (string)$request['request_status'] !== 'approved'
        || (int)$request['row_version'] !== 2 || (int)$request['approved_by'] <= 0) {
        cancel_transaction(); return false;
    }
    $time_result = db_query('SELECT UTC_TIMESTAMP() utc_now', 'could not read ESS self change execution time');
    $now = $time_result ? db_fetch_assoc($time_result) : false;
    if (!$now || (string)$request['expires_at'] <= (string)$now['utc_now']) {
        cancel_transaction(); return false;
    }

    $locked = hrm_ess_self_change_link_matches((int)$request['user_id'], (int)$request['worker_id']);
    $payload = $locked ? hrm_ess_self_change_open_payload($request) : false;
    if (!$locked || $payload === false) { cancel_transaction(); return false; }
    $employee_number = (int)$locked['employee_number'];
    $stored_result = db_query('SELECT * FROM '.TB_PREF.'employees WHERE employee_number='.$employee_number.' FOR UPDATE',
        'could not lock legacy employee compatibility row');
    if (!$stored_result || db_num_rows($stored_result) !== 1) { cancel_transaction(); return false; }
    $stored = db_fetch_assoc($stored_result);
    $current = hrm_employee_projection_build($employee_number);
    $pre_drift = $current ? hrm_employee_projection_difference($stored, $current) : false;
    if ($current === false || $pre_drift === false || $pre_drift
        || !hrm_ess_self_change_baseline_matches($request, $payload, $current)) {
        cancel_transaction(); return false;
    }
    $binding = get_hrm_person_worker_binding($employee_number, true);
    if (!$binding || (int)$binding['worker_id'] !== (int)$request['worker_id'] || $binding['retired_at'] !== null
        || !hrm_ess_self_change_apply_normalized($binding, $current, $payload)) {
        cancel_transaction(); return false;
    }

    $expected = hrm_employee_projection_build($employee_number);
    $changed_fields = $expected ? hrm_employee_projection_difference($stored, $expected) : false;
    $payload_fields = array_keys($payload); sort($payload_fields);
    if ($changed_fields === false) { cancel_transaction(); return false; }
    sort($changed_fields);
    if ($changed_fields !== $payload_fields || !hrm_employee_projection_store($stored, $expected)) {
        cancel_transaction(); return false;
    }

    $field_json = json_encode($payload_fields);
    if ($field_json === false || strlen($field_json) > 512) { cancel_transaction(); return false; }
    $nonce_collision = db_query('SELECT receipt_id FROM '.TB_PREF.'hrm_ess_self_change_execution_receipts WHERE execution_nonce_hash='.
        db_escape($nonce_hash).' FOR UPDATE', 'could not lock ESS self change execution nonce');
    if (!$nonce_collision || db_num_rows($nonce_collision) !== 0) { cancel_transaction(); return false; }
    $ok = db_query('INSERT INTO '.TB_PREF.'hrm_ess_self_change_execution_receipts '
        .'(request_id,request_digest,approval_digest,execution_nonce_hash,execution_digest,executor_id,field_names_json,executed_at) VALUES ('
        .$request_id.','.db_escape($request['request_digest']).','.db_escape($request['approval_digest']).','
        .db_escape($nonce_hash).','.db_escape($execution_digest).','.$actor.','.db_escape($field_json).',UTC_TIMESTAMP())',
        'could not create ESS self change execution receipt');
    $receipt_id = $ok ? (int)db_insert_id() : 0;
    if ($receipt_id <= 0) { cancel_transaction(); return false; }
    $ok = db_query('UPDATE '.TB_PREF."hrm_ess_self_change_requests SET request_status='applied',row_version=3 WHERE request_id="
        .$request_id." AND request_status='approved' AND row_version=2",
        'could not finalize applied ESS self change request') !== false;
    if (!$ok || (function_exists('db_num_affected_rows') && db_num_affected_rows() !== 1)) {
        cancel_transaction(); return false;
    }

    $audit = append_domain_audit_event(array(
        'company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,
        'entity_type'=>'hrm_ess_self_change_request','entity_key'=>array('request_id'=>$request_id,'worker_id'=>(int)$request['worker_id']),
        'action'=>'hrm.ess.self_change.applied','reason_code'=>'approved_bounded_self_profile_change',
        'before'=>array('request_status'=>'approved'),'after'=>array('request_status'=>'applied','receipt_id'=>$receipt_id,'field_names'=>$payload_fields),
        'restricted_diff'=>array('values_logged'=>false,'stale_baseline_revalidated'=>true,'identity_link_revalidated'=>true,
            'normalized_person_updated_before_legacy_projection'=>true,'execution_nonce_stored_raw'=>false,
            'bank_tax_fields_allowed'=>false,'identifier_fields_allowed'=>false,'compensation_fields_allowed'=>false,
            'public_routes_added'=>0,'historical_payroll_accounting_rows_rewritten'=>0)
    ));
    if ($audit === false) { cancel_transaction(); return false; }
    commit_transaction();
    return $receipt_id;
}
