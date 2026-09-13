<?php
/**
 * FND-006 Worker-owned retained profile facts. Internal command/migration API.
 * These immutable versions fill gaps in the normalized model; they do not
 * replace Person names/contacts, Employment intervals or Assignment ownership.
 * Unknown custom JSON is opaque compatibility data, never a typed attribute.
 */
if (!function_exists('hrm_person_sensitive_encrypt'))
    include_once(dirname(__FILE__).'/employee_person_sensitive_db.inc');

/** Return the exact retained field allowlist, in canonical serialization order. */
function hrm_compatibility_profile_fields($schema_version = 2)
{
    $fields = array('gender','birth_date','nationality','marital_status','dependents_no',
        'confirmation_date','probation_end_date','personal_salary','login_id','notes','custom_data');
    return $schema_version === 1 ? $fields : array_merge($fields, array('payment_method','separation_reason'));
}

/** Preserve SQL NULL and exact retained bytes; reject nested or unknown values. */
function hrm_compatibility_profile_values($employee, $schema_version = 2)
{
    if (!is_array($employee)) return false;
    $values = array();
    foreach (hrm_compatibility_profile_fields($schema_version) as $field) {
        if (!array_key_exists($field, $employee)) return false;
        $value = $employee[$field];
        if ($value !== null && !is_scalar($value)) return false;
        $values[$field] = $value === null ? null : (string)$value;
    }
    return $values;
}

/** Is the additive profile schema present in this company? No cross-company cache. */
function hrm_compatibility_profile_ready()
{
    return hrm_person_worker_table_exists('hrm_worker_profile_versions')
        && hrm_person_worker_table_exists('hrm_employee_projection_policy');
}

/** Bind every encrypted chunk to company, Worker, sequence, predecessor and time. */
function hrm_compatibility_profile_context($row)
{
    $company = hrm_person_worker_current_company_id();
    if ($company === false || !is_array($row) || (int)$row['worker_id'] <= 0
        || (int)$row['version_no'] <= 0 || !isset($row['effective_from'])
        || hrm_person_worker_normalize_as_of($row['effective_from']) !== $row['effective_from']) return false;
    return 'hrm-worker-profile-v1|company='.$company.'|worker='.(int)$row['worker_id']
        .'|version='.(int)$row['version_no'].'|predecessor='.(int)$row['predecessor_id']
        .'|effective='.$row['effective_from'];
}

/** Encrypt bounded chunks using the existing authenticated company-key envelope. */
function hrm_compatibility_profile_seal($values, $row, $key)
{
    $canonical = hrm_compatibility_profile_values($values);
    $context = hrm_compatibility_profile_context($row);
    if ($canonical === false || $context === false || !is_array($key)) return false;
    $json = json_encode($canonical);
    if ($json === false || strlen($json) > 1048576) return false;
    $pieces = str_split($json, 768); $sealed = array(); $count = count($pieces);
    foreach ($pieces as $index=>$piece) {
        $cipher = hrm_person_sensitive_encrypt($piece, $context.'|chunk='.$index.'|count='.$count, $key['key']);
        if ($cipher === false) return false;
        $sealed[] = base64_encode($cipher);
    }
    return json_encode($sealed);
}

/** Authenticate complete retained facts; never return partial or unauthenticated data. */
function hrm_compatibility_profile_open($row)
{
    $context = hrm_compatibility_profile_context($row);
    if ($context === false || !isset($row['payload_ciphertext'], $row['key_id'])) return false;
    $key = hrm_person_sensitive_key($row['key_id']);
    $pieces = json_decode($row['payload_ciphertext'], true);
    if ($key === false || !is_array($pieces) || !$pieces || count($pieces) > 1366
        || array_keys($pieces) !== range(0, count($pieces)-1)) return false;
    $json = ''; $count = count($pieces);
    foreach ($pieces as $index=>$piece) {
        if (!is_string($piece)) return false;
        $cipher = base64_decode($piece, true);
        if ($cipher === false) return false;
        $plain = hrm_person_sensitive_decrypt($cipher, $context.'|chunk='.$index.'|count='.$count, $key);
        if ($plain === false) return false;
        $json .= $plain;
    }
    $values = json_decode($json, true);
    // The 1.0.376 payload had eleven fields. Its immutable bytes remain readable.
    $schema_version = is_array($values) && count($values) === 11 ? 1 : 2;
    $canonical = hrm_compatibility_profile_values($values, $schema_version);
    return $canonical !== false && json_encode($canonical) === $json ? $canonical : false;
}

/** Read the exact latest Worker version internally; caller owns authorization. */
function hrm_compatibility_profile_current($worker_id, $for_update = false)
{
    if ((int)$worker_id <= 0) return false;
    $result = db_query('SELECT * FROM '.TB_PREF.'hrm_worker_profile_versions WHERE worker_id='.(int)$worker_id
        .' ORDER BY version_no DESC LIMIT 1'.($for_update ? ' FOR UPDATE' : ''),
        'could not read Worker profile version');
    return $result ? db_fetch_assoc($result) : false;
}

/** Append changed facts under the caller's Person/Worker transaction; never update history. */
function hrm_compatibility_profile_append($employee, $source = 'command')
{
    if (!in_array($source, array('command','migration'), true) || !hrm_compatibility_profile_ready()) return false;
    $values = hrm_compatibility_profile_values($employee);
    if ($values === false || empty($employee['employee_number'])) return false;
    $binding = hrm_employment_lock_binding_for_interval_policy((int)$employee['employee_number']);
    if (!$binding || (string)$binding['employee_id'] !== (string)$employee['employee_id']) return false;
    $current = hrm_compatibility_profile_current((int)$binding['worker_id'], true);
    if ($current) {
        $before = hrm_compatibility_profile_open($current);
        if ($before === false) return false;
        if ($before === $values) return (int)$current['profile_version_id'];
        if ($source === 'migration' && count($before) !== 11) return false;
    }
    $row = array('worker_id'=>(int)$binding['worker_id'],
        'version_no'=>$current ? (int)$current['version_no']+1 : 1,
        'predecessor_id'=>$current ? (int)$current['profile_version_id'] : null,
        'effective_from'=>hrm_person_worker_utc_now());
    $key = hrm_person_sensitive_active_key();
    if ($key === false || $row['effective_from'] === false) return false;
    $cipher = hrm_compatibility_profile_seal($values, $row, $key);
    if ($cipher === false) return false;
    if (!db_query('INSERT INTO '.TB_PREF.'hrm_worker_profile_versions '
        .'(worker_id,version_no,predecessor_id,effective_from,key_id,payload_ciphertext,source,created_by) VALUES ('
        .$row['worker_id'].','.$row['version_no'].','.
        ($row['predecessor_id'] === null ? 'NULL' : $row['predecessor_id']).','
        .db_escape($row['effective_from']).','.db_escape($key['key_id']).',0x'.bin2hex($cipher).','
        .db_escape($source).','.hrm_person_worker_actor_id().')', 'could not append Worker profile version')) return false;
    $id = (int)db_insert_id();
    return $id > 0 && append_domain_audit_event(array(
        'company_id'=>hrm_person_worker_current_company_id(),'legal_entity_id'=>0,
        'entity_type'=>'hrm_worker_profile','entity_key'=>array('worker_id'=>$row['worker_id'],'version_id'=>$id),
        'action'=>'hrm.worker_profile.appended','reason_code'=>'hrm_fnd_006_'.$source,
        'before'=>null,'after'=>array('version_no'=>$row['version_no']),
        'restricted_diff'=>array('values_logged'=>false,'predecessor_preserved'=>true))) !== false ? $id : false;
}

/** Read and validate a company-local policy, optionally serializing activation and writes. */
function hrm_employee_projection_policy($for_update = false)
{
    $company = hrm_person_worker_current_company_id();
    if ($company === false || !hrm_compatibility_profile_ready()) return false;
    $result = db_query('SELECT policy_id,company_id,mode,revision FROM '.TB_PREF.'hrm_employee_projection_policy'
        .' WHERE policy_id=1'.($for_update ? ' FOR UPDATE' : ''), 'could not read employee projection policy');
    $row = $result ? db_fetch_assoc($result) : false;
    return $row && (int)$row['company_id'] === $company && (int)$row['revision'] > 0
        && in_array($row['mode'], array('shadow','active','fallback'), true) ? $row : false;
}
