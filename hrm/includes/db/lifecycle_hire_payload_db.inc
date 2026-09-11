<?php
/**
 * HRM-FND-005 pre-Worker Employee Hire encrypted staged-payload writer.
 *
 * This source is deliberately not connected to a browser/import route and does
 * not register an approval transaction type. It owns only bounded pre-approval
 * command metadata plus confidential staged payload custody. Final Hire
 * execution remains a separate reviewed dependency.
 */

if (!function_exists('append_domain_audit_event'))
    include_once($path_to_root.'/includes/db/domain_audit_db.inc');

if (!defined('HRM_LIFECYCLE_HIRE_PAYLOAD_ENVELOPE_VERSION'))
    define('HRM_LIFECYCLE_HIRE_PAYLOAD_ENVELOPE_VERSION', 1);
if (!defined('HRM_LIFECYCLE_HIRE_PAYLOAD_RETENTION_DAYS'))
    define('HRM_LIFECYCLE_HIRE_PAYLOAD_RETENTION_DAYS', 7);
if (!defined('HRM_LIFECYCLE_HIRE_PAYLOAD_HASH_CONTRACT'))
    define('HRM_LIFECYCLE_HIRE_PAYLOAD_HASH_CONTRACT', 'notrinoserp.hrm.lifecycle.hire-payload-request.v1');

/** @return int */
function hrm_lifecycle_hire_payload_actor_id()
{
    return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
        && isset($_SESSION['wa_current_user']->user) ? (int)$_SESSION['wa_current_user']->user : 0;
}

/** @return bool */
function hrm_lifecycle_hire_payload_permission()
{
    return function_exists('user_check_access') && user_check_access('SA_EMPLOYEE');
}

/** @param mixed $value @return int|false */
function hrm_lifecycle_hire_payload_positive_int($value)
{
    if (is_int($value))
        return $value > 0 ? $value : false;
    $value = trim((string)$value);
    if (preg_match('/^[1-9][0-9]*$/D', $value) !== 1)
        return false;
    $number = (int)$value;
    return $number > 0 && (string)$number === $value ? $number : false;
}

/** @param mixed $value @return string|false */
function hrm_lifecycle_hire_payload_date($value)
{
    $value = trim((string)$value);
    if (preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/D', $value) !== 1)
        return false;
    $date = DateTime::createFromFormat('!Y-m-d', $value);
    $errors = DateTime::getLastErrors();
    if (!$date || (is_array($errors)
        && ((int)$errors['warning_count'] !== 0 || (int)$errors['error_count'] !== 0))
        || $date->format('Y-m-d') !== $value)
        return false;
    return $value;
}

/** @param mixed $key @return string|false */
function hrm_lifecycle_hire_payload_idempotency_hash($key)
{
    if (!is_string($key) || strlen($key) < 16 || strlen($key) > 128
        || preg_match('/^[A-Za-z0-9._:-]+$/D', $key) !== 1)
        return false;
    return hash('sha256', $key);
}

/**
 * Return the private per-company root for Hire staging keys.
 *
 * @param int|null $company
 * @return string|false
 */
function hrm_hire_payload_private_root($company = null)
{
    global $path_to_root, $db_connections;
    if ($company === null)
        $company = function_exists('user_company') ? (int)user_company() : 0;
    $application_root = realpath($path_to_root);
    if ($application_root === false)
        return false;
    if (defined('HRM_HIRE_PAYLOAD_PRIVATE_ROOT')) {
        $base = rtrim((string)HRM_HIRE_PAYLOAD_PRIVATE_ROOT, "\\/");
    } elseif (isset($db_connections[$company]['hrm_hire_payload_private_root'])) {
        $base = rtrim((string)$db_connections[$company]['hrm_hire_payload_private_root'], "\\/");
    } else {
        $base = dirname(dirname($application_root)).DIRECTORY_SEPARATOR
            .'.notrinoserp-private'.DIRECTORY_SEPARATOR
            .substr(hash('sha256', $application_root), 0, 16)
            .DIRECTORY_SEPARATOR.'hrm-hire-payload';
    }
    if ($base === '' || !preg_match('/^(?:[A-Za-z]:[\\\\\/]|\/)/', $base))
        return false;
    $normalized_base = rtrim(str_replace('\\', '/', $base), '/');
    $normalized_app = rtrim(str_replace('\\', '/', $application_root), '/');
    if ($normalized_base === $normalized_app
        || strpos($normalized_base.'/', $normalized_app.'/') === 0)
        return false;
    return $base.DIRECTORY_SEPARATOR.'company-'.(int)$company;
}

/** @param int|null $company @return array|false */
function hrm_hire_payload_keyring_paths($company = null)
{
    $root = hrm_hire_payload_private_root($company);
    if ($root === false)
        return false;
    return array(
        'root'=>$root,
        'keys'=>$root.DIRECTORY_SEPARATOR.'keys',
        'manifest'=>$root.DIRECTORY_SEPARATOR.'keys'.DIRECTORY_SEPARATOR.'hire-payload-keyring-v1.json',
        'pending'=>$root.DIRECTORY_SEPARATOR.'keys'.DIRECTORY_SEPARATOR.'hire-payload-key-rotation-v1.pending.json',
    );
}

/** @param int|null $company @return array|false */
function ensure_hrm_hire_payload_private_storage($company = null)
{
    $paths = hrm_hire_payload_keyring_paths($company);
    if ($paths === false)
        return false;
    foreach (array($paths['root'], $paths['keys']) as $path) {
        if (is_link($path))
            return false;
        if (!is_dir($path) && !@mkdir($path, 0700, true) && !is_dir($path))
            return false;
        @chmod($path, 0700);
        $permissions = @fileperms($path);
        if (DIRECTORY_SEPARATOR === '/' && ($permissions === false || ($permissions & 0077) !== 0))
            return false;
    }
    return $paths;
}

/** @param int|null $company @return void */
function hrm_hire_payload_clear_created_key_marker($company = null)
{
    if ($company === null)
        $company = function_exists('user_company') ? (int)user_company() : 0;
    if (isset($GLOBALS['hrm_hire_payload_created_keyrings'][(int)$company]))
        unset($GLOBALS['hrm_hire_payload_created_keyrings'][(int)$company]);
}

/** @param int|null $company @return string */
function hrm_hire_payload_created_key_id($company = null)
{
    if ($company === null)
        $company = function_exists('user_company') ? (int)user_company() : 0;
    return isset($GLOBALS['hrm_hire_payload_created_keyrings'][(int)$company])
        ? (string)$GLOBALS['hrm_hire_payload_created_keyrings'][(int)$company] : '';
}

/** @param string $path @param string $key_id @return string|false */
function hrm_hire_payload_load_key_file($path, $key_id)
{
    if (!is_file($path) || is_link($path)
        || preg_match('/^[a-f0-9]{16}$/D', (string)$key_id) !== 1)
        return false;
    @chmod($path, 0600);
    $permissions = @fileperms($path);
    if (DIRECTORY_SEPARATOR === '/' && ($permissions === false || ($permissions & 0077) !== 0))
        return false;
    $key = base64_decode(trim((string)@file_get_contents($path)), true);
    return is_string($key) && strlen($key) === 32
        && substr(hash('sha256', $key), 0, 16) === $key_id ? $key : false;
}

/**
 * Load or, only when explicitly requested, provision the dedicated Hire keyring.
 *
 * @param int|null $company
 * @param bool $allow_create
 * @return array|false
 */
function hrm_hire_payload_keyring($company = null, $allow_create = false)
{
    if ($company === null)
        $company = function_exists('user_company') ? (int)user_company() : 0;
    $paths = hrm_hire_payload_keyring_paths($company);
    if ($paths === false || is_link($paths['root']) || is_link($paths['keys'])
        || is_link($paths['manifest']) || is_link($paths['pending']) || is_file($paths['pending']))
        return false;
    if (!is_file($paths['manifest'])) {
        if (!$allow_create || !function_exists('openssl_random_pseudo_bytes')
            || ensure_hrm_hire_payload_private_storage($company) === false)
            return false;
        $strong = false;
        $key = openssl_random_pseudo_bytes(32, $strong);
        if (!is_string($key) || strlen($key) !== 32 || !$strong)
            return false;
        $key_id = substr(hash('sha256', $key), 0, 16);
        $filename = 'hire-payload-v1-'.$key_id.'.key';
        $key_path = $paths['keys'].DIRECTORY_SEPARATOR.$filename;
        if (!function_exists('domain_audit_write_key_file')
            || !domain_audit_write_key_file($key_path, $key)) {
            if (is_file($paths['manifest']))
                return hrm_hire_payload_keyring($company, false);
            return false;
        }
        $manifest = array(
            'schema_version'=>'notrinoserp.hrm-hire-payload-keyring.v1',
            'company_id'=>(int)$company,
            'active_key_id'=>$key_id,
            'generation'=>1,
            'keys'=>array(array(
                'key_id'=>$key_id,'generation'=>1,'filename'=>$filename,
                'activated_at'=>gmdate('Y-m-d H:i:s'),'retired_at'=>'',
            )),
        );
        if (!function_exists('domain_audit_write_private_json')
            || !domain_audit_write_private_json($paths['manifest'], $manifest, false)) {
            @unlink($key_path);
            return is_file($paths['manifest']) ? hrm_hire_payload_keyring($company, false) : false;
        }
        if (!isset($GLOBALS['hrm_hire_payload_created_keyrings']))
            $GLOBALS['hrm_hire_payload_created_keyrings'] = array();
        $GLOBALS['hrm_hire_payload_created_keyrings'][(int)$company] = $key_id;
    }
    @chmod($paths['manifest'], 0600);
    $permissions = @fileperms($paths['manifest']);
    if (DIRECTORY_SEPARATOR === '/' && ($permissions === false || ($permissions & 0077) !== 0))
        return false;
    $json = @file_get_contents($paths['manifest']);
    $manifest = is_string($json) ? json_decode($json, true) : null;
    $expected = array('active_key_id','company_id','generation','keys','schema_version');
    $actual = is_array($manifest) ? array_keys($manifest) : array();
    sort($expected); sort($actual);
    if (!is_array($manifest) || $actual !== $expected
        || $manifest['schema_version'] !== 'notrinoserp.hrm-hire-payload-keyring.v1'
        || !is_int($manifest['company_id']) || $manifest['company_id'] !== (int)$company
        || !is_int($manifest['generation']) || $manifest['generation'] < 1
        || preg_match('/^[a-f0-9]{16}$/D', (string)$manifest['active_key_id']) !== 1
        || !is_array($manifest['keys']) || count($manifest['keys']) !== (int)$manifest['generation'])
        return false;
    $previous_generation = 0; $seen = array();
    foreach ($manifest['keys'] as $index=>$entry) {
        $fields = is_array($entry) ? array_keys($entry) : array();
        $expected_entry = array('activated_at','filename','generation','key_id','retired_at');
        sort($fields); sort($expected_entry);
        if (!is_array($entry) || $fields !== $expected_entry
            || !is_int($entry['generation']) || $entry['generation'] !== $previous_generation + 1
            || preg_match('/^[a-f0-9]{16}$/D', (string)$entry['key_id']) !== 1
            || isset($seen[$entry['key_id']])
            || $entry['filename'] !== 'hire-payload-v1-'.$entry['key_id'].'.key'
            || preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/D', (string)$entry['activated_at']) !== 1
            || ($index < count($manifest['keys']) - 1
                && preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/D', (string)$entry['retired_at']) !== 1)
            || ($index === count($manifest['keys']) - 1 && (string)$entry['retired_at'] !== '')
            || hrm_hire_payload_load_key_file($paths['keys'].DIRECTORY_SEPARATOR.$entry['filename'], $entry['key_id']) === false)
            return false;
        $seen[$entry['key_id']] = true;
        $previous_generation = (int)$entry['generation'];
    }
    $last = $manifest['keys'][count($manifest['keys']) - 1];
    return (string)$last['key_id'] === (string)$manifest['active_key_id']
        && (int)$last['generation'] === (int)$manifest['generation'] ? $manifest : false;
}

/**
 * Load one keyring key, optionally requiring its exact generation.
 *
 * @param string $key_id
 * @param int|null $company
 * @param int|null $expected_generation
 * @return string|false
 */
function hrm_hire_payload_key($key_id, $company = null, $expected_generation = null)
{
    $manifest = hrm_hire_payload_keyring($company, false);
    $paths = hrm_hire_payload_keyring_paths($company);
    if ($manifest === false || $paths === false)
        return false;
    if ($expected_generation !== null) {
        $expected_generation = hrm_lifecycle_hire_payload_positive_int($expected_generation);
        if ($expected_generation === false)
            return false;
    }
    foreach ($manifest['keys'] as $entry)
        if ((string)$entry['key_id'] === (string)$key_id
            && ($expected_generation === null
                || (int)$entry['generation'] === (int)$expected_generation))
            return hrm_hire_payload_load_key_file($paths['keys'].DIRECTORY_SEPARATOR.$entry['filename'], $key_id);
    return false;
}

/** @param int|null $company @param bool $allow_create @return array|false */
function hrm_hire_payload_active_key($company = null, $allow_create = false)
{
    $manifest = hrm_hire_payload_keyring($company, $allow_create);
    if ($manifest === false)
        return false;
    $key = hrm_hire_payload_key($manifest['active_key_id'], $company);
    return $key === false ? false : array(
        'key_id'=>(string)$manifest['active_key_id'],
        'generation'=>(int)$manifest['generation'],
        'key'=>$key,
    );
}

/**
 * Remove only the exact generation-1 keyring created by this request.
 * Caller must first prove no Hire payload row depends on it.
 *
 * @param int|null $company
 * @return bool
 */
function remove_hrm_hire_payload_request_created_keyring($company = null)
{
    if ($company === null)
        $company = function_exists('user_company') ? (int)user_company() : 0;
    $key_id = hrm_hire_payload_created_key_id($company);
    if ($key_id === '')
        return true;
    $paths = hrm_hire_payload_keyring_paths($company);
    $manifest = hrm_hire_payload_keyring($company, false);
    if ($paths === false || !is_array($manifest) || (int)$manifest['generation'] !== 1
        || count($manifest['keys']) !== 1 || (string)$manifest['active_key_id'] !== $key_id)
        return false;
    $key_path = $paths['keys'].DIRECTORY_SEPARATOR.'hire-payload-v1-'.$key_id.'.key';
    if (!@unlink($paths['manifest']) || !@unlink($key_path))
        return false;
    @rmdir($paths['keys']); @rmdir($paths['root']);
    hrm_hire_payload_clear_created_key_marker($company);
    return true;
}

/** @return array */
function hrm_lifecycle_hire_payload_schema()
{
    return array(
        'employee_id'=>'text20','first_name'=>'text100','last_name'=>'text100','middle_name'=>'text100',
        'gender'=>'int','birth_date'=>'date_optional','nationality'=>'text100','marital_status'=>'int','dependents_no'=>'int',
        'address'=>'text500','city'=>'text100','state'=>'text100','country'=>'text100','phone'=>'text100','mobile'=>'text100',
        'email'=>'email_optional','personal_email'=>'email_optional','national_id'=>'text255','passport'=>'text255',
        'passport_expiry'=>'date_optional','tax_number'=>'text255','social_security_no'=>'text255','bank_name'=>'text255',
        'bank_branch'=>'text255','bank_account'=>'text255','bank_routing'=>'text255','payment_method'=>'int',
        'emergency_name'=>'text255','emergency_relation'=>'text100','emergency_phone'=>'text100','hire_date'=>'date_required',
        'employment_type'=>'int','department_id'=>'int','position_id'=>'int','grade_id'=>'int','reporting_to'=>'text20',
        'shift_id'=>'int','personal_salary'=>'int','cost_center_id'=>'int','login_id'=>'text100','notes'=>'text2000',
    );
}

/** @param mixed $value @param int $max @return string|false */
function hrm_lifecycle_hire_payload_text($value, $max)
{
    if (is_array($value) || is_object($value) || is_resource($value))
        return false;
    $value = trim((string)$value);
    if (strlen($value) > (int)$max || preg_match('/[\x00-\x08\x0B\x0C\x0E-\x1F\x7F]/', $value))
        return false;
    return $value;
}

/**
 * Canonicalize the complete pre-Hire payload without dropping confidential data.
 * Unknown/nested fields fail closed. Output field order is deterministic.
 *
 * @param array $payload
 * @return array|false
 */
function hrm_lifecycle_hire_payload_canonicalize($payload)
{
    if (!is_array($payload))
        return false;
    $schema = hrm_lifecycle_hire_payload_schema();
    foreach ($payload as $field=>$unused)
        if (!array_key_exists($field, $schema))
            return false;
    $out = array();
    foreach ($schema as $field=>$rule) {
        $present = array_key_exists($field, $payload);
        $value = $present ? $payload[$field] : null;
        if ($rule === 'int') {
            if (!$present || $value === '' || $value === null) { $out[$field] = 0; continue; }
            if (is_bool($value) || is_array($value) || is_object($value)
                || preg_match('/^-?[0-9]+$/D', trim((string)$value)) !== 1)
                return false;
            $out[$field] = (int)$value;
            continue;
        }
        if ($rule === 'date_required' || $rule === 'date_optional') {
            if (!$present || trim((string)$value) === '') {
                if ($rule === 'date_required') return false;
                $out[$field] = '';
            } else {
                $date = hrm_lifecycle_hire_payload_date($value);
                if ($date === false) return false;
                $out[$field] = $date;
            }
            continue;
        }
        if ($rule === 'email_optional') {
            $text = hrm_lifecycle_hire_payload_text($present ? $value : '', 254);
            if ($text === false || ($text !== '' && filter_var($text, FILTER_VALIDATE_EMAIL) === false))
                return false;
            $out[$field] = $text;
            continue;
        }
        if (preg_match('/^text([0-9]+)$/D', $rule, $match) !== 1)
            return false;
        $text = hrm_lifecycle_hire_payload_text($present ? $value : '', (int)$match[1]);
        if ($text === false)
            return false;
        $out[$field] = $text;
    }
    if ($out['employee_id'] === '' || preg_match('/^[A-Za-z0-9_-]{1,20}$/D', $out['employee_id']) !== 1
        || $out['first_name'] === '' || $out['last_name'] === '' || $out['hire_date'] === '')
        return false;
    return $out;
}

/** @param array $payload @return string|false */
function hrm_lifecycle_hire_payload_canonical_json($payload)
{
    $payload = hrm_lifecycle_hire_payload_canonicalize($payload);
    if ($payload === false)
        return false;
    $json = json_encode($payload, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
    return is_string($json) && strlen($json) <= 32768 ? $json : false;
}

/** @return string|false */
function hrm_lifecycle_hire_payload_request_hash($company, $effective_date, $canonical_json)
{
    $effective_date = hrm_lifecycle_hire_payload_date($effective_date);
    if ($effective_date === false || !is_string($canonical_json) || $canonical_json === '' || strlen($canonical_json) > 32768)
        return false;
    return hash('sha256', HRM_LIFECYCLE_HIRE_PAYLOAD_HASH_CONTRACT.'|company='.(int)$company
        .'|effective_date='.$effective_date.'|payload_sha256='.hash('sha256', $canonical_json));
}

/** @return string|false */
function hrm_lifecycle_hire_payload_context($company, $command_id, $request_hash, $envelope_version=1)
{
    $command_id = hrm_lifecycle_hire_payload_positive_int($command_id);
    if ($command_id === false || preg_match('/^[a-f0-9]{64}$/D', (string)$request_hash) !== 1
        || (int)$envelope_version !== HRM_LIFECYCLE_HIRE_PAYLOAD_ENVELOPE_VERSION)
        return false;
    return 'company='.(int)$company.';hire_command_id='.$command_id.';request_hash='.$request_hash
        .';envelope_version='.(int)$envelope_version;
}

/** @return string|false */
function hrm_lifecycle_hire_payload_encrypt($plaintext, $context, $master_key)
{
    if (!function_exists('openssl_encrypt') || !is_string($plaintext) || $plaintext === '' || strlen($plaintext) > 32768
        || !is_string($context) || $context === '' || !is_string($master_key) || strlen($master_key) !== 32)
        return false;
    $strong = false; $iv = openssl_random_pseudo_bytes(16, $strong);
    if (!is_string($iv) || strlen($iv) !== 16 || !$strong)
        return false;
    $enc = hash_hmac('sha256', 'hire-payload-encryption-v1', $master_key, true);
    $auth = hash_hmac('sha256', 'hire-payload-authentication-v1', $master_key, true);
    $ciphertext = openssl_encrypt($plaintext, 'AES-256-CBC', $enc, OPENSSL_RAW_DATA, $iv);
    if ($ciphertext === false)
        return false;
    $magic = 'NERPHH01';
    $mac = hash_hmac('sha256', $magic.$context.$iv.$ciphertext, $auth, true);
    return $magic.$iv.$mac.$ciphertext;
}

/** @return string|false */
function hrm_lifecycle_hire_payload_decrypt($envelope, $context, $master_key)
{
    if (!function_exists('openssl_decrypt') || !is_string($envelope) || strlen($envelope) < 72
        || substr($envelope, 0, 8) !== 'NERPHH01' || !is_string($context) || $context === ''
        || !is_string($master_key) || strlen($master_key) !== 32)
        return false;
    $magic = substr($envelope, 0, 8); $iv = substr($envelope, 8, 16);
    $mac = substr($envelope, 24, 32); $ciphertext = substr($envelope, 56);
    $auth = hash_hmac('sha256', 'hire-payload-authentication-v1', $master_key, true);
    $expected = hash_hmac('sha256', $magic.$context.$iv.$ciphertext, $auth, true);
    if (!function_exists('domain_audit_hash_equals') || !domain_audit_hash_equals($expected, $mac))
        return false;
    $enc = hash_hmac('sha256', 'hire-payload-encryption-v1', $master_key, true);
    return openssl_decrypt($ciphertext, 'AES-256-CBC', $enc, OPENSSL_RAW_DATA, $iv);
}

/** @return array|false */
function hrm_lifecycle_hire_command_by_idempotency_for_update($idempotency_hash)
{
    $result = db_query("SELECT hire_command_id,command_status,effective_at,idempotency_key_hash,request_hash,"
        ."approval_draft_id,row_version,requested_by,completed_by FROM ".TB_PREF."hrm_lifecycle_hire_commands "
        ."WHERE idempotency_key_hash=".db_escape($idempotency_hash)." LIMIT 1 FOR UPDATE",
        'could not lock idempotent Employee Hire command');
    return $result ? db_fetch_assoc($result) : false;
}

/** @return array|false */
function hrm_lifecycle_hire_command_for_update($command_id)
{
    $command_id = hrm_lifecycle_hire_payload_positive_int($command_id);
    if ($command_id === false)
        return false;
    $result = db_query("SELECT hire_command_id,command_status,effective_at,idempotency_key_hash,request_hash,"
        ."approval_draft_id,row_version,requested_by,completed_by FROM ".TB_PREF."hrm_lifecycle_hire_commands "
        ."WHERE hire_command_id=".$command_id." LIMIT 1 FOR UPDATE", 'could not lock Employee Hire command');
    return $result ? db_fetch_assoc($result) : false;
}

/** @return array|false */
function hrm_lifecycle_hire_payload_for_update($command_id)
{
    $command_id = hrm_lifecycle_hire_payload_positive_int($command_id);
    if ($command_id === false)
        return false;
    $result = db_query("SELECT hire_command_id,payload_state,envelope_version,payload_ciphertext,ciphertext_sha256,"
        ."request_hash,key_id,key_generation,purge_after,purged_at,row_version,created_by FROM "
        .TB_PREF."hrm_lifecycle_hire_payloads WHERE hire_command_id=".$command_id." LIMIT 1 FOR UPDATE",
        'could not lock Employee Hire staged payload');
    return $result ? db_fetch_assoc($result) : false;
}

/** @return bool */
function hrm_lifecycle_hire_employee_code_available_for_update($employee_id)
{
    $result = db_query("SELECT employee_number FROM ".TB_PREF."employees WHERE employee_id=".db_escape($employee_id)
        ." ORDER BY employee_number LIMIT 1 FOR UPDATE", 'could not revalidate Employee Hire code uniqueness');
    return $result && db_fetch_assoc($result) ? false : true;
}

/** @return bool */
function hrm_lifecycle_hire_payload_audit($action, $command, $payload_meta, $restricted_diff=array())
{
    if (!function_exists('append_domain_audit_event') || !is_array($command) || !is_array($payload_meta))
        return false;
    $safe = array_merge(array(
        'fixed_command_type'=>'employee_hire','pre_worker_command'=>true,'plaintext_payload_audited'=>false,
        'payload_ciphertext_audited'=>false,'approval_registered'=>false,'browser_route_added'=>false,
        'employee_rows_mutated'=>0,'person_rows_mutated'=>0,'worker_rows_mutated'=>0,'employment_rows_mutated'=>0,
        'contract_rows_mutated'=>0,'assignment_rows_mutated'=>0,'payroll_rows_mutated'=>0,'payment_rows_mutated'=>0,
        'bank_rows_mutated'=>0,'gl_accounting_rows_mutated'=>0,
    ), is_array($restricted_diff) ? $restricted_diff : array());
    return append_domain_audit_event(array(
        'company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,
        'entity_type'=>'hrm_lifecycle_hire_command','entity_key'=>array('hire_command_id'=>(int)$command['hire_command_id']),
        'action'=>$action,'reason_code'=>'hrm_fnd_005_employee_hire_payload_writer_foundation',
        'before'=>array(),'after'=>array('command_status'=>(string)$command['command_status'],
            'command_row_version'=>(int)$command['row_version'],'payload_state'=>(string)$payload_meta['payload_state'],
            'payload_row_version'=>(int)$payload_meta['row_version'],'request_hash'=>(string)$command['request_hash'],
            'ciphertext_sha256'=>(string)$payload_meta['ciphertext_sha256'],'key_id'=>(string)$payload_meta['key_id'],
            'key_generation'=>(int)$payload_meta['key_generation'],'purge_after'=>(string)$payload_meta['purge_after']),
        'restricted_diff'=>$safe,
    )) !== false;
}

/**
 * Stage one pre-Worker Hire command plus encrypted payload. No approval row or
 * HR master row is created. Missing key custody fails closed.
 *
 * @return array|false
 */
function stage_hrm_lifecycle_employee_hire_payload($payload, $effective_date, $idempotency_key)
{
    $effective_date = hrm_lifecycle_hire_payload_date($effective_date);
    $idempotency_hash = hrm_lifecycle_hire_payload_idempotency_hash($idempotency_key);
    $canonical = hrm_lifecycle_hire_payload_canonicalize($payload);
    $canonical_json = $canonical === false ? false : hrm_lifecycle_hire_payload_canonical_json($canonical);
    $actor = hrm_lifecycle_hire_payload_actor_id();
    $company = function_exists('user_company') ? (int)user_company() : 0;
    $request_hash = $canonical_json === false ? false
        : hrm_lifecycle_hire_payload_request_hash($company, $effective_date, $canonical_json);
    $key = hrm_hire_payload_active_key($company, false);
    if ($effective_date === false || $idempotency_hash === false || $canonical === false || $canonical_json === false
        || $request_hash === false || $actor <= 0 || !hrm_lifecycle_hire_payload_permission() || !is_array($key)
        || !isset($key['key_id'],$key['generation'],$key['key']))
        return false;

    $ok = false; $return_value = false;
    begin_transaction();
    do {
        $existing = hrm_lifecycle_hire_command_by_idempotency_for_update($idempotency_hash);
        if (is_array($existing)) {
            $existing_payload = hrm_lifecycle_hire_payload_for_update($existing['hire_command_id']);
            if (!$existing_payload || (string)$existing['command_status'] !== 'staged'
                || (int)$existing['row_version'] !== 1 || (int)$existing['requested_by'] !== $actor
                || (string)$existing['effective_at'] !== $effective_date.' 00:00:00'
                || !domain_audit_hash_equals((string)$existing['request_hash'], $request_hash)
                || (string)$existing_payload['payload_state'] !== 'staged'
                || (int)$existing_payload['envelope_version'] !== HRM_LIFECYCLE_HIRE_PAYLOAD_ENVELOPE_VERSION
                || !domain_audit_hash_equals((string)$existing_payload['request_hash'], $request_hash)
                || empty($existing_payload['payload_ciphertext'])
                || !domain_audit_hash_equals(hash('sha256', $existing_payload['payload_ciphertext']),
                    (string)$existing_payload['ciphertext_sha256']))
                break;
            $existing_key = hrm_hire_payload_key((string)$existing_payload['key_id'], $company,
                (int)$existing_payload['key_generation']);
            $existing_context = hrm_lifecycle_hire_payload_context($company, $existing['hire_command_id'],
                $request_hash, (int)$existing_payload['envelope_version']);
            $existing_json = $existing_key === false || $existing_context === false ? false
                : hrm_lifecycle_hire_payload_decrypt($existing_payload['payload_ciphertext'], $existing_context, $existing_key);
            if (!is_string($existing_json) || !domain_audit_hash_equals(hash('sha256', $existing_json), hash('sha256', $canonical_json)))
                break;
            $ok = true;
            $return_value = array('hire_command_id'=>(int)$existing['hire_command_id'],'status'=>'staged',
                'row_version'=>1,'request_hash'=>$request_hash,'exact_retry'=>true);
            break;
        }
        if (!hrm_lifecycle_hire_employee_code_available_for_update($canonical['employee_id']))
            break;
        if (!db_query("INSERT INTO ".TB_PREF."hrm_lifecycle_hire_commands "
            ."(command_status,effective_at,idempotency_key_hash,request_hash,approval_draft_id,row_version,requested_by) VALUES "
            ."('staged',".db_escape($effective_date.' 00:00:00').','.db_escape($idempotency_hash).','
            .db_escape($request_hash).",NULL,1,".$actor.")", 'could not create staged Employee Hire command'))
            break;
        $command_id = (int)db_insert_id();
        if ($command_id <= 0)
            break;
        $context = hrm_lifecycle_hire_payload_context($company, $command_id, $request_hash,
            HRM_LIFECYCLE_HIRE_PAYLOAD_ENVELOPE_VERSION);
        $ciphertext = $context === false ? false
            : hrm_lifecycle_hire_payload_encrypt($canonical_json, $context, $key['key']);
        if ($ciphertext === false)
            break;
        $ciphertext_hash = hash('sha256', $ciphertext);
        $purge_after = gmdate('Y-m-d H:i:s', time() + HRM_LIFECYCLE_HIRE_PAYLOAD_RETENTION_DAYS * 86400);
        if (!db_query("INSERT INTO ".TB_PREF."hrm_lifecycle_hire_payloads "
            ."(hire_command_id,payload_state,envelope_version,payload_ciphertext,ciphertext_sha256,request_hash,key_id,"
            ."key_generation,purge_after,purged_at,row_version,created_by) VALUES (".$command_id.",'staged',"
            .HRM_LIFECYCLE_HIRE_PAYLOAD_ENVELOPE_VERSION.','.db_escape($ciphertext).','.db_escape($ciphertext_hash).','
            .db_escape($request_hash).','.db_escape($key['key_id']).','.(int)$key['generation'].','
            .db_escape($purge_after).",NULL,1,".$actor.")", 'could not store encrypted Employee Hire staged payload'))
            break;
        $command = array('hire_command_id'=>$command_id,'command_status'=>'staged','row_version'=>1,'request_hash'=>$request_hash);
        $payload_meta = array('payload_state'=>'staged','row_version'=>1,'ciphertext_sha256'=>$ciphertext_hash,
            'key_id'=>$key['key_id'],'key_generation'=>(int)$key['generation'],'purge_after'=>$purge_after);
        if (!hrm_lifecycle_hire_payload_audit('hrm.lifecycle.employee_hire.payload_staged', $command, $payload_meta,
            array('maker_user_id'=>$actor,'employee_code_hash'=>hash('sha256', $canonical['employee_id']),
                'retention_days'=>HRM_LIFECYCLE_HIRE_PAYLOAD_RETENTION_DAYS,'exact_request_hash_bound'=>true,
                'dedicated_key_domain'=>true)))
            break;
        $ok = true;
        $return_value = array('hire_command_id'=>$command_id,'status'=>'staged','row_version'=>1,
            'request_hash'=>$request_hash,'exact_retry'=>false);
    } while (false);
    if ($ok) commit_transaction(); else cancel_transaction();
    return $ok ? $return_value : false;
}

/**
 * Load and authenticate one staged payload for a later reviewed executor.
 * This helper is intentionally not connected to approval/browser code here.
 *
 * @return array|false
 */
function load_hrm_lifecycle_employee_hire_payload_for_update($command_id)
{
    $command = hrm_lifecycle_hire_command_for_update($command_id);
    $payload = $command ? hrm_lifecycle_hire_payload_for_update($command_id) : false;
    $company = function_exists('user_company') ? (int)user_company() : 0;
    if (!$command || !$payload || (string)$command['command_status'] !== 'staged'
        || (string)$payload['payload_state'] !== 'staged' || empty($payload['payload_ciphertext'])
        || !domain_audit_hash_equals((string)$command['request_hash'], (string)$payload['request_hash'])
        || !domain_audit_hash_equals(hash('sha256', $payload['payload_ciphertext']), (string)$payload['ciphertext_sha256']))
        return false;
    $key = hrm_hire_payload_key((string)$payload['key_id'], $company,
        (int)$payload['key_generation']);
    $context = hrm_lifecycle_hire_payload_context($company, $command['hire_command_id'], $command['request_hash'],
        (int)$payload['envelope_version']);
    $json = $key === false || $context === false ? false
        : hrm_lifecycle_hire_payload_decrypt($payload['payload_ciphertext'], $context, $key);
    $decoded = is_string($json) ? json_decode($json, true) : null;
    $canonical = is_array($decoded) ? hrm_lifecycle_hire_payload_canonicalize($decoded) : false;
    $reencoded = $canonical === false ? false : hrm_lifecycle_hire_payload_canonical_json($canonical);
    $expected_hash = $reencoded === false ? false
        : hrm_lifecycle_hire_payload_request_hash($company, substr((string)$command['effective_at'], 0, 10), $reencoded);
    if ($reencoded === false || $reencoded !== $json || $expected_hash === false
        || !domain_audit_hash_equals($expected_hash, (string)$command['request_hash']))
        return false;
    return array('command'=>$command,'payload_meta'=>$payload,'payload'=>$canonical);
}

/**
 * Purge ciphertext while retaining a non-secret tombstone. Approval/execution
 * cancellation semantics remain a later dependency; this helper does not
 * mutate HR master data.
 *
 * @return bool
 */
function purge_hrm_lifecycle_employee_hire_payload($command_id, $reason_code)
{
    $command_id = hrm_lifecycle_hire_payload_positive_int($command_id);
    $reason_code = trim((string)$reason_code);
    $actor = hrm_lifecycle_hire_payload_actor_id();
    if ($command_id === false || $actor <= 0 || !hrm_lifecycle_hire_payload_permission()
        || preg_match('/^[a-z][a-z0-9_.:-]{2,63}$/D', $reason_code) !== 1)
        return false;
    $ok = false; begin_transaction();
    do {
        $command = hrm_lifecycle_hire_command_for_update($command_id);
        $payload = $command ? hrm_lifecycle_hire_payload_for_update($command_id) : false;
        if (!$command || !$payload || (string)$payload['payload_state'] === 'purged') {
            if ($payload && (string)$payload['payload_state'] === 'purged' && empty($payload['payload_ciphertext']))
                $ok = true;
            break;
        }
        if ((string)$command['command_status'] !== 'staged' || (string)$payload['payload_state'] !== 'staged'
            || empty($payload['payload_ciphertext']) || (int)$payload['row_version'] < 1)
            break;
        $next_version = (int)$payload['row_version'] + 1;
        $now = gmdate('Y-m-d H:i:s');
        if (!db_query("UPDATE ".TB_PREF."hrm_lifecycle_hire_payloads SET payload_state='purged',payload_ciphertext=NULL,"
            ."purged_at=".db_escape($now).",row_version=".$next_version." WHERE hire_command_id=".$command_id
            ." AND payload_state='staged' AND row_version=".(int)$payload['row_version'], 'could not purge Employee Hire payload')
            || (int)db_num_affected_rows() !== 1)
            break;
        $meta = $payload; $meta['payload_state']='purged'; $meta['row_version']=$next_version;
        if (!hrm_lifecycle_hire_payload_audit('hrm.lifecycle.employee_hire.payload_purged', $command, $meta,
            array('purge_reason_code'=>$reason_code,'purged_by'=>$actor,'ciphertext_destroyed'=>true)))
            break;
        $ok = true;
    } while (false);
    if ($ok) commit_transaction(); else cancel_transaction();
    return $ok;
}
