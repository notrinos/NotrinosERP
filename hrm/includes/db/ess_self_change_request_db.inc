<?php
/**
 * HRM-ESS-001 bounded current-session self-profile change request command.
 *
 * This helper records encrypted request custody only. It never applies the
 * requested values to Person, Worker, legacy employee, payroll, or accounting
 * state and exposes no public route.
 */
if (!function_exists('append_domain_audit_event'))
    include_once($path_to_root.'/includes/db/domain_audit_db.inc');
if (!function_exists('get_hrm_ess_current_self_scope'))
    include_once($path_to_root.'/hrm/includes/db/ess_self_scope_db.inc');
if (!function_exists('hrm_employee_projection_build'))
    include_once($path_to_root.'/hrm/includes/db/employee_compatibility_projector_db.inc');
if (!function_exists('hrm_person_sensitive_active_key'))
    include_once($path_to_root.'/hrm/includes/db/employee_person_sensitive_db.inc');
if (!function_exists('hrm_person_private_profile_text'))
    include_once($path_to_root.'/hrm/includes/db/employee_person_private_profile_db.inc');

/** Return the current authenticated application actor id. */
function hrm_ess_self_change_actor_id()
{
    if (function_exists('user_id'))
        return (int)user_id();
    return isset($_SESSION['wa_current_user']->user)
        ? (int)$_SESSION['wa_current_user']->user : 0;
}

/** Validate one lowercase SHA-256 custody digest. */
function hrm_ess_self_change_digest_is_valid($value)
{
    return is_string($value) && preg_match('/^[a-f0-9]{64}$/D', $value) === 1;
}

/** Require the dedicated self-change maker authority. */
function hrm_ess_self_change_user_can_request()
{
    return function_exists('user_check_access')
        && user_check_access('SA_HRM_ESS_REQUEST_SELF_PROFILE_CHANGE');
}

/** Return the immutable bounded contact/private-profile field allowlist. */
function hrm_ess_self_change_allowed_fields()
{
    return array('address','city','state','country','phone','mobile','email','personal_email','emergency_name','emergency_relation','emergency_phone');
}

/** Validate the raw retry nonce before retaining only its SHA-256 hash. */
function hrm_ess_self_change_request_nonce_is_valid($nonce)
{
    return is_string($nonce) && strlen($nonce) >= 16 && strlen($nonce) <= 128
        && preg_match('/^[A-Za-z0-9][A-Za-z0-9_.:-]{15,127}$/D', $nonce) === 1;
}

/** Build authenticated encryption context for one request payload. */
function hrm_ess_self_change_payload_context($company, $user_id, $worker_id, $nonce_hash)
{
    return 'hrm-ess-self-change-v1|company='.(int)$company.'|user='.(int)$user_id
        .'|worker='.(int)$worker_id.'|nonce='.$nonce_hash;
}

/** Prove every command custody table exists before any request write. */
function hrm_ess_self_change_tables_ready()
{
    foreach (array('users','hrm_workers','hrm_employee_worker_map','hrm_ess_identity_links','hrm_ess_self_change_requests') as $table) {
        $result = db_query("SHOW TABLES LIKE '".TB_PREF.db_escape_function($table)."'");
        if (!$result || db_num_rows($result) !== 1)
            return false;
    }
    return true;
}

/** Validate and canonicalize the bounded requested field/value map. */
function hrm_ess_self_change_validate_payload($changes)
{
    if (!is_array($changes) || !$changes)
        return false;
    $allowed = hrm_ess_self_change_allowed_fields();
    $out = array();
    foreach ($changes as $field=>$value) {
        if (!is_string($field) || !in_array($field, $allowed, true) || !is_scalar($value))
            return false;
        $value = trim((string)$value);
        if (in_array($field, array('email','personal_email'), true)) {
            if ($value !== '' && (!function_exists('hrm_person_sensitive_semantic_value_is_valid')
                || !hrm_person_sensitive_semantic_value_is_valid('contact', $field === 'email' ? 'work_email' : 'personal_email', $value)))
                return false;
        } elseif (in_array($field, array('phone','mobile','emergency_phone'), true)) {
            if ($value !== '' && (!function_exists('hrm_person_sensitive_semantic_value_is_valid')
                || !hrm_person_sensitive_semantic_value_is_valid('contact', $field === 'mobile' ? 'mobile' : 'phone', $value)))
                return false;
        } else {
            $limits = array('address'=>255,'city'=>60,'state'=>60,'country'=>60,'emergency_name'=>100,'emergency_relation'=>60);
            $value = hrm_person_private_profile_text($value, $limits[$field], $field === 'address');
            if ($value === false)
                return false;
        }
        $out[$field] = $value;
    }
    ksort($out);
    return $out;
}

/** Build the immutable digest binding subject, encrypted payload, baseline, and nonce. */
function hrm_ess_self_change_request_digest($user_id, $worker_id, $payload_digest, $baseline_tag, $nonce_hash)
{
    return hash('sha256', implode("\0", array('hrm-ess-self-change-request-v1',(int)$user_id,
        (int)$worker_id,'contact_private_v1',(string)$payload_digest,(string)$baseline_tag,(string)$nonce_hash)));
}

/** Remove unchanged values while preserving legitimate explicit clears. */
function hrm_ess_self_change_normalize_payload($changes, $current)
{
    if (!is_array($current))
        return false;
    $validated = hrm_ess_self_change_validate_payload($changes);
    if ($validated === false)
        return false;
    $out = array();
    foreach ($validated as $field=>$value) {
        $prior = array_key_exists($field, $current) && $current[$field] !== null
            ? (string)$current[$field] : '';
        if (!hash_equals($prior, $value))
            $out[$field] = $value;
    }
    return $out ? $out : false;
}

/** Capture only the current values for fields that are actually changing. */
function hrm_ess_self_change_baseline_values($current, $fields)
{
    $out = array();
    foreach ($fields as $field)
        $out[$field] = array_key_exists($field, $current) && $current[$field] !== null
            ? (string)$current[$field] : '';
    ksort($out);
    return $out;
}

/** Read one request, optionally locking it for checker processing. */
function get_hrm_ess_self_change_request($request_id, $for_update = false)
{
    $request_id = (int)$request_id;
    if ($request_id <= 0)
        return false;
    $sql = 'SELECT * FROM '.TB_PREF.'hrm_ess_self_change_requests WHERE request_id='.$request_id;
    if ($for_update)
        $sql .= ' FOR UPDATE';
    $result = db_query($sql, 'could not read ESS self change request');
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    return $row ? $row : null;
}

/**
 * Lock the exact approved account/Worker/live compatibility mapping.
 * Historical retired mappings are excluded rather than treated as ambiguity.
 */
function hrm_ess_self_change_link_matches($user_id, $worker_id)
{
    $result = db_query('SELECT l.link_id,l.user_id,l.worker_id,w.worker_status,m.mapping_id,m.employee_number,m.employee_id,m.retired_at '
        .'FROM '.TB_PREF.'hrm_ess_identity_links l '
        .'INNER JOIN '.TB_PREF.'users u ON u.id=l.user_id AND u.inactive=0 '
        .'INNER JOIN '.TB_PREF.'hrm_workers w ON w.worker_id=l.worker_id '
        .'INNER JOIN '.TB_PREF.'hrm_employee_worker_map m ON m.worker_id=l.worker_id AND m.retired_at IS NULL '
        .'WHERE l.user_id='.(int)$user_id.' AND l.worker_id='.(int)$worker_id.' FOR UPDATE',
        'could not lock ESS self change identity custody');
    if (!$result || db_num_rows($result) !== 1)
        return false;
    $row = db_fetch_assoc($result);
    return $row && (string)$row['worker_status'] === 'active' && $row['retired_at'] === null
        && (int)$row['mapping_id'] > 0 ? $row : false;
}

/** Authenticate, decrypt, and revalidate one stored canonical request payload. */
function hrm_ess_self_change_open_payload($request)
{
    if (!is_array($request) || !isset($request['payload_key_id'], $request['payload_ciphertext'],
        $request['payload_digest'], $request['request_nonce_hash']))
        return false;
    $cipher = (string)$request['payload_ciphertext'];
    if (!hash_equals((string)$request['payload_digest'], hash('sha256', $cipher)))
        return false;
    $key = hrm_person_sensitive_key((string)$request['payload_key_id']);
    if ($key === false)
        return false;
    $company = function_exists('user_company') ? (int)user_company() : 0;
    $context = hrm_ess_self_change_payload_context($company, (int)$request['user_id'],
        (int)$request['worker_id'], (string)$request['request_nonce_hash']);
    $plain = hrm_person_sensitive_decrypt($cipher, $context, $key);
    if ($plain === false)
        return false;
    $payload = json_decode($plain, true);
    if (!is_array($payload) || json_encode($payload) !== $plain)
        return false;
    $normalized = hrm_ess_self_change_validate_payload($payload);
    if ($normalized === false || $normalized !== $payload)
        return false;
    return $payload;
}

/**
 * Record one encrypted self-profile change request for the current session.
 * Exact retry is valid only while the same unexpired draft remains pending.
 */
function request_hrm_ess_self_profile_change($changes, $request_nonce)
{
    $actor = hrm_ess_self_change_actor_id();
    $nonce = (string)$request_nonce;
    if ($actor <= 0 || !hrm_ess_self_change_user_can_request() || !hrm_ess_self_change_tables_ready()
        || !hrm_ess_self_change_request_nonce_is_valid($nonce))
        return false;

    $scope = get_hrm_ess_current_self_scope();
    if (!is_array($scope) || (int)$scope['user_id'] !== $actor)
        return false;
    $current = hrm_employee_projection_build((int)$scope['employee_number']);
    $payload = hrm_ess_self_change_normalize_payload($changes, $current);
    if ($payload === false)
        return false;
    $fields = array_keys($payload);
    $baseline = hrm_ess_self_change_baseline_values($current, $fields);
    $json = json_encode($payload);
    $baseline_json = json_encode($baseline);
    if ($json === false || $baseline_json === false || strlen($json) > 1024)
        return false;

    $nonce_hash = hash('sha256', $nonce);
    $company = function_exists('user_company') ? (int)user_company() : 0;

    begin_transaction();
    $locked = hrm_ess_self_change_link_matches($actor, (int)$scope['worker_id']);
    if (!$locked || (int)$locked['employee_number'] !== (int)$scope['employee_number']) {
        cancel_transaction();
        return false;
    }

    /* Retry identity is the stored nonce hash, not newly randomized ciphertext. */
    $result = db_query('SELECT * FROM '.TB_PREF.'hrm_ess_self_change_requests WHERE request_nonce_hash='
        .db_escape($nonce_hash).' FOR UPDATE', 'could not lock existing ESS self change request nonce');
    if (!$result) {
        cancel_transaction();
        return false;
    }
    if (db_num_rows($result) === 1) {
        $row = db_fetch_assoc($result);
        $stored_payload = hrm_ess_self_change_open_payload($row);
        $stored_key = hrm_person_sensitive_key((string)$row['payload_key_id']);
        $expected_baseline = $stored_key === false ? false : hash_hmac('sha256', $baseline_json, $stored_key);
        $expected_digest = hrm_ess_self_change_request_digest($actor, (int)$scope['worker_id'],
            (string)$row['payload_digest'], (string)$row['baseline_auth_tag'], $nonce_hash);
        $time_result = db_query('SELECT UTC_TIMESTAMP() utc_now', 'could not read ESS self change retry time');
        $now = $time_result ? db_fetch_assoc($time_result) : false;
        $ok = (int)$row['user_id'] === $actor && (int)$row['worker_id'] === (int)$scope['worker_id']
            && (int)$row['requested_by'] === $actor && (string)$row['field_set_code'] === 'contact_private_v1'
            && (string)$row['request_status'] === 'draft' && (int)$row['row_version'] === 1
            && $row['approved_by'] === null && $row['approval_digest'] === null
            && $now && (string)$row['expires_at'] > (string)$now['utc_now']
            && is_array($stored_payload) && $stored_payload === $payload
            && is_string($expected_baseline) && hash_equals((string)$row['baseline_auth_tag'], $expected_baseline)
            && hash_equals((string)$row['request_digest'], $expected_digest);
        if ($ok) {
            commit_transaction();
            return (int)$row['request_id'];
        }
        cancel_transaction();
        return false;
    }
    if (db_num_rows($result) !== 0) {
        cancel_transaction();
        return false;
    }

    $pending = db_query('SELECT request_id FROM '.TB_PREF."hrm_ess_self_change_requests WHERE worker_id="
        .(int)$scope['worker_id']." AND request_status='draft' AND expires_at>UTC_TIMESTAMP() FOR UPDATE",
        'could not lock pending ESS self change requests');
    if (!$pending || db_num_rows($pending) !== 0) {
        cancel_transaction();
        return false;
    }

    $key = hrm_person_sensitive_active_key(null, false);
    if ($key === false) {
        cancel_transaction();
        return false;
    }
    $context = hrm_ess_self_change_payload_context($company, $actor, (int)$scope['worker_id'], $nonce_hash);
    $cipher = hrm_person_sensitive_encrypt($json, $context, $key['key']);
    if ($cipher === false || strlen($cipher) > 4096) {
        cancel_transaction();
        return false;
    }
    $payload_digest = hash('sha256', $cipher);
    $baseline_tag = hash_hmac('sha256', $baseline_json, $key['key']);
    $request_digest = hrm_ess_self_change_request_digest($actor, (int)$scope['worker_id'], $payload_digest,
        $baseline_tag, $nonce_hash);
    $collision = db_query('SELECT request_id FROM '.TB_PREF.'hrm_ess_self_change_requests WHERE request_digest='
        .db_escape($request_digest).' FOR UPDATE', 'could not lock ESS self change request digest');
    if (!$collision || db_num_rows($collision) !== 0) {
        cancel_transaction();
        return false;
    }

    $ok = db_query('INSERT INTO '.TB_PREF.'hrm_ess_self_change_requests '
        .'(user_id,worker_id,field_set_code,payload_ciphertext,payload_digest,payload_key_id,baseline_auth_tag,'
        .'request_nonce_hash,request_digest,request_status,requested_by,approved_by,approval_digest,approved_at,'
        .'expires_at,row_version,created_at) VALUES ('.$actor.','.(int)$scope['worker_id'].",'contact_private_v1',"
        .db_escape($cipher).','.db_escape($payload_digest).','.db_escape($key['key_id']).','.db_escape($baseline_tag).','
        .db_escape($nonce_hash).','.db_escape($request_digest).",'draft',".$actor
        .',NULL,NULL,NULL,DATE_ADD(UTC_TIMESTAMP(),INTERVAL 72 HOUR),1,UTC_TIMESTAMP())',
        'could not create ESS self change request');
    $request_id = $ok ? (int)db_insert_id() : 0;
    if ($request_id <= 0) {
        cancel_transaction();
        return false;
    }

    $audit = append_domain_audit_event(array(
        'company_id'=>$company,'legal_entity_id'=>0,'entity_type'=>'hrm_ess_self_change_request',
        'entity_key'=>array('request_id'=>$request_id,'worker_id'=>(int)$scope['worker_id']),
        'action'=>'hrm.ess.self_change.requested','reason_code'=>'current_session_bounded_self_profile_change',
        'before'=>null,
        'after'=>array('request_id'=>$request_id,'field_set_code'=>'contact_private_v1','field_names'=>$fields,
            'maker_actor_id'=>$actor,'checker_required'=>true),
        'restricted_diff'=>array('values_logged'=>false,'payload_encrypted'=>true,'raw_nonce_stored'=>false,
            'bank_tax_fields_allowed'=>false,'compensation_fields_allowed'=>false,'identity_fields_allowed'=>false,
            'profile_mutation_performed'=>false,'public_routes_added'=>0)
    ));
    if ($audit === false) {
        cancel_transaction();
        return false;
    }
    commit_transaction();
    return $request_id;
}
