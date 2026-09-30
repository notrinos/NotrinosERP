<?php
/**
 * HRM-TAL-001 governed skills/certifications catalog writer.
 *
 * The writer is append-only, company-scoped and limited to SA_HRSETTINGS. It
 * does not seed rows, mutate legacy recruitment/training/appraisal state, or
 * grant payroll/compensation/employment-decision authority.
 */
include_once($path_to_root.'/includes/hrm_tal_001_contract.inc');

/** @return false */
function hrm_tal_001_catalog_fail(&$error, $code)
{
    $error = $code;
    return false;
}

/** @return string|false */
function hrm_tal_001_runtime_database_version()
{
    if (!function_exists('get_company_pref')) return false;
    $version = get_company_pref('version_id', true);
    return is_string($version) ? $version : false;
}

/** @return int */
function hrm_tal_001_runtime_company_id()
{
    return isset($_SESSION['wa_current_user']) && isset($_SESSION['wa_current_user']->company)
        ? (int)$_SESSION['wa_current_user']->company : -1;
}

/** @return int */
function hrm_tal_001_runtime_actor_id()
{
    return isset($_SESSION['wa_current_user']) && isset($_SESSION['wa_current_user']->user)
        ? (int)$_SESSION['wa_current_user']->user : 0;
}

/** @return bool */
function hrm_tal_001_runtime_has_admin_access()
{
    return function_exists('user_check_access') && user_check_access('SA_HRSETTINGS');
}

/** @return bool */
function hrm_tal_001_catalog_writer_ready(&$error)
{
    $version = hrm_tal_001_runtime_database_version();
    if (!is_string($version) || !hrm_tal_001_catalog_writer_allowed_for_database_version($version))
        return hrm_tal_001_catalog_fail($error, 'catalog_writer_not_active');
    if (!hrm_tal_001_runtime_has_admin_access())
        return hrm_tal_001_catalog_fail($error, 'permission_denied');
    if (hrm_tal_001_runtime_company_id() < 0 || hrm_tal_001_runtime_actor_id() <= 0)
        return hrm_tal_001_catalog_fail($error, 'authenticated_scope_required');
    return true;
}

/** @return string|false */
function hrm_tal_001_token($value, $max)
{
    $value = trim((string)$value);
    if ($value === '' || strlen($value) > (int)$max || !preg_match('/^[A-Za-z0-9][A-Za-z0-9._-]*$/D', $value)) return false;
    return $value;
}

/** @return string|false */
function hrm_tal_001_date($value)
{
    $value = trim((string)$value);
    if (!preg_match('/^\\d{4}-\\d{2}-\\d{2}$/D', $value)) return false;
    $parts = explode('-', $value);
    if (count($parts) !== 3 || !checkdate((int)$parts[1], (int)$parts[2], (int)$parts[0])) return false;
    return $value;
}

/** @return string|null|false */
function hrm_tal_001_optional_date($value)
{
    if ($value === null || $value === '') return null;
    return hrm_tal_001_date($value);
}

/** @return string|false */
function hrm_tal_001_sha256($value)
{
    $value = strtolower(trim((string)$value));
    return preg_match('/^[a-f0-9]{64}$/D', $value) ? $value : false;
}

/** @return string */
function hrm_tal_001_payload_sha256(array $payload)
{
    return hash('sha256', json_encode($payload, JSON_UNESCAPED_SLASHES));
}

/** @return array|false */
function hrm_tal_001_single_row($sql)
{
    $result = db_query($sql);
    if (!$result || db_num_rows($result) !== 1) return false;
    return db_fetch_assoc($result);
}

/** @return bool */
function hrm_tal_001_catalog_window_available($table, $code_column, $code, $from, $to, &$error)
{
    $company = hrm_tal_001_runtime_company_id();
    $end = $to === null ? '9999-12-31' : $to;
    $sql = 'SELECT COUNT(*) n FROM '.TB_PREF.$table.' WHERE company_id='.(int)$company
        .' AND '.$code_column.'='.db_escape($code)
        .' AND effective_from<='.db_escape($end)
        .' AND COALESCE(effective_to,\'9999-12-31\')>='.db_escape($from);
    $row = hrm_tal_001_single_row($sql);
    if ($row === false) return hrm_tal_001_catalog_fail($error, 'catalog_overlap_check_failed');
    if ((int)$row['n'] !== 0) return hrm_tal_001_catalog_fail($error, 'catalog_effective_window_overlaps');
    return true;
}

/**
 * Start a serialized catalog transaction and lock this company's version row.
 * This prevents concurrent writers from both accepting overlapping effective windows.
 * @param string|null $error
 * @return bool
 */
function hrm_tal_001_catalog_begin_serialized_transaction(&$error)
{
    global $transaction_level;
    if (!empty($transaction_level)) return hrm_tal_001_catalog_fail($error, 'catalog_owned_transaction_required');
    begin_transaction();
    $result = db_query('SELECT value FROM '.TB_PREF.'sys_prefs WHERE name=\'version_id\' AND category=\'system\' LIMIT 2 FOR UPDATE');
    if (!$result || db_num_rows($result) !== 1) {
        cancel_transaction();
        return hrm_tal_001_catalog_fail($error, 'catalog_company_lock_failed');
    }
    $row = db_fetch_assoc($result);
    if (!isset($row['value']) || !hrm_tal_001_catalog_writer_allowed_for_database_version((string)$row['value'])) {
        cancel_transaction();
        return hrm_tal_001_catalog_fail($error, 'catalog_writer_not_active');
    }
    return true;
}

/**
 * Append one effective-dated skill definition.
 * @param array $x
 * @param string|null $error
 * @return array|false
 */
function hrm_tal_001_register_skill_definition(array $x, &$error = null)
{
    $error = null;
    if (!hrm_tal_001_catalog_writer_ready($error)) return false;
    foreach (array('skill_code','skill_name','category_code','proficiency_scale_code','effective_from') as $key)
        if (!array_key_exists($key, $x)) return hrm_tal_001_catalog_fail($error, 'skill_definition_field_missing');
    $code = hrm_tal_001_token($x['skill_code'], 64);
    $name = trim((string)$x['skill_name']);
    $category = hrm_tal_001_token($x['category_code'], 64);
    $scale = hrm_tal_001_token($x['proficiency_scale_code'], 32);
    $description = isset($x['description']) ? trim((string)$x['description']) : '';
    $from = hrm_tal_001_date($x['effective_from']);
    $to = hrm_tal_001_optional_date(isset($x['effective_to']) ? $x['effective_to'] : null);
    if ($code === false || $category === false || $scale === false || !in_array($scale, array('none','level_1_5','percent_0_100'), true)
        || $name === '' || strlen($name) > 140 || strlen($description) > 2000 || $from === false || ($to !== null && ($to === false || $to < $from)))
        return hrm_tal_001_catalog_fail($error, 'skill_definition_invalid');
    $company = hrm_tal_001_runtime_company_id();
    $actor = hrm_tal_001_runtime_actor_id();
    $payload = array('company_id'=>$company,'skill_code'=>$code,'skill_name'=>$name,'category_code'=>$category,'description'=>$description,
        'proficiency_scale_code'=>$scale,'effective_from'=>$from,'effective_to'=>$to);
    $sha = hrm_tal_001_payload_sha256($payload);
    if (!hrm_tal_001_catalog_begin_serialized_transaction($error)) return false;
    $existing = db_query('SELECT skill_definition_id,definition_sha256 FROM '.TB_PREF.'hrm_tal_001_skill_definitions WHERE company_id='.(int)$company
        .' AND skill_code='.db_escape($code).' AND effective_from='.db_escape($from).' LIMIT 2');
    if (!$existing || db_num_rows($existing) > 1) { cancel_transaction(); return hrm_tal_001_catalog_fail($error, 'skill_definition_lookup_failed'); }
    if (db_num_rows($existing) === 1) {
        $row = db_fetch_assoc($existing);
        if (!hash_equals((string)$row['definition_sha256'], $sha)) { cancel_transaction(); return hrm_tal_001_catalog_fail($error, 'skill_definition_replay_mismatch'); }
        commit_transaction();
        return array('skill_definition_id'=>(int)$row['skill_definition_id'],'definition_sha256'=>$sha,'idempotent'=>true);
    }
    if (!hrm_tal_001_catalog_window_available('hrm_tal_001_skill_definitions', 'skill_code', $code, $from, $to, $error)) { cancel_transaction(); return false; }
    $sql = 'INSERT INTO '.TB_PREF.'hrm_tal_001_skill_definitions (company_id,skill_code,skill_name,category_code,description,proficiency_scale_code,effective_from,effective_to,definition_sha256,created_by_user_id,created_at) VALUES ('
        .$company.','.db_escape($code).','.db_escape($name).','.db_escape($category).','.db_escape($description).','.db_escape($scale).','.db_escape($from).','
        .($to === null ? 'NULL' : db_escape($to)).','.db_escape($sha).','.$actor.','.db_escape(gmdate('Y-m-d H:i:s')).')';
    if (!db_query($sql)) { cancel_transaction(); return hrm_tal_001_catalog_fail($error, 'skill_definition_insert_failed'); }
    $id = (int)db_insert_id();
    commit_transaction();
    return array('skill_definition_id'=>$id,'definition_sha256'=>$sha,'idempotent'=>false);
}

/**
 * Append one effective-dated certification definition.
 * @param array $x
 * @param string|null $error
 * @return array|false
 */
function hrm_tal_001_register_certification_definition(array $x, &$error = null)
{
    $error = null;
    if (!hrm_tal_001_catalog_writer_ready($error)) return false;
    foreach (array('certification_code','certification_name','issuer_name','category_code','expiry_policy_code','effective_from') as $key)
        if (!array_key_exists($key, $x)) return hrm_tal_001_catalog_fail($error, 'certification_definition_field_missing');
    $code = hrm_tal_001_token($x['certification_code'], 64);
    $name = trim((string)$x['certification_name']);
    $issuer = trim((string)$x['issuer_name']);
    $category = hrm_tal_001_token($x['category_code'], 64);
    $policy = hrm_tal_001_token($x['expiry_policy_code'], 32);
    $days = isset($x['default_validity_days']) && $x['default_validity_days'] !== '' ? (int)$x['default_validity_days'] : null;
    $from = hrm_tal_001_date($x['effective_from']);
    $to = hrm_tal_001_optional_date(isset($x['effective_to']) ? $x['effective_to'] : null);
    if ($code === false || $category === false || $policy === false || !in_array($policy, array('none','fixed_days','explicit_on_evidence'), true)
        || $name === '' || strlen($name) > 140 || $issuer === '' || strlen($issuer) > 140 || $from === false || ($to !== null && ($to === false || $to < $from))
        || ($policy === 'fixed_days' && ($days === null || $days < 1 || $days > 36500)) || ($policy !== 'fixed_days' && $days !== null))
        return hrm_tal_001_catalog_fail($error, 'certification_definition_invalid');
    $company = hrm_tal_001_runtime_company_id();
    $actor = hrm_tal_001_runtime_actor_id();
    $payload = array('company_id'=>$company,'certification_code'=>$code,'certification_name'=>$name,'issuer_name'=>$issuer,'category_code'=>$category,
        'expiry_policy_code'=>$policy,'default_validity_days'=>$days,'effective_from'=>$from,'effective_to'=>$to);
    $sha = hrm_tal_001_payload_sha256($payload);
    if (!hrm_tal_001_catalog_begin_serialized_transaction($error)) return false;
    $existing = db_query('SELECT certification_definition_id,definition_sha256 FROM '.TB_PREF.'hrm_tal_001_certification_definitions WHERE company_id='.(int)$company
        .' AND certification_code='.db_escape($code).' AND effective_from='.db_escape($from).' LIMIT 2');
    if (!$existing || db_num_rows($existing) > 1) { cancel_transaction(); return hrm_tal_001_catalog_fail($error, 'certification_definition_lookup_failed'); }
    if (db_num_rows($existing) === 1) {
        $row = db_fetch_assoc($existing);
        if (!hash_equals((string)$row['definition_sha256'], $sha)) { cancel_transaction(); return hrm_tal_001_catalog_fail($error, 'certification_definition_replay_mismatch'); }
        commit_transaction();
        return array('certification_definition_id'=>(int)$row['certification_definition_id'],'definition_sha256'=>$sha,'idempotent'=>true);
    }
    if (!hrm_tal_001_catalog_window_available('hrm_tal_001_certification_definitions', 'certification_code', $code, $from, $to, $error)) { cancel_transaction(); return false; }
    $sql = 'INSERT INTO '.TB_PREF.'hrm_tal_001_certification_definitions (company_id,certification_code,certification_name,issuer_name,category_code,expiry_policy_code,default_validity_days,effective_from,effective_to,definition_sha256,created_by_user_id,created_at) VALUES ('
        .$company.','.db_escape($code).','.db_escape($name).','.db_escape($issuer).','.db_escape($category).','.db_escape($policy).','
        .($days === null ? 'NULL' : (int)$days).','.db_escape($from).','.($to === null ? 'NULL' : db_escape($to)).','.db_escape($sha).','.$actor.','.db_escape(gmdate('Y-m-d H:i:s')).')';
    if (!db_query($sql)) { cancel_transaction(); return hrm_tal_001_catalog_fail($error, 'certification_definition_insert_failed'); }
    $id = (int)db_insert_id();
    commit_transaction();
    return array('certification_definition_id'=>$id,'definition_sha256'=>$sha,'idempotent'=>false);
}
?>
