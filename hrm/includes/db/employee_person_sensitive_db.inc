<?php
/**
 * HRM-FND-001 encrypted person contact and identifier shadow model.
 *
 * Legacy employee columns remain compatibility projections. Governed identifier
 * replacements use explicit lineage; this file maintains encrypted, effective-dated
 * custody plus masked, permissioned projections and fails closed on legacy mutation.
 */

if (!function_exists('append_domain_audit_event'))
	include_once($path_to_root.'/includes/db/domain_audit_db.inc');
if (!function_exists('hrm_user_can_access_sensitive_field'))
	include_once($path_to_root.'/hrm/includes/hrm_security.inc');
if (!function_exists('get_hrm_person_worker_binding'))
	include_once($path_to_root.'/hrm/includes/db/employee_person_worker_db.inc');

/**
 * Return the private per-company root for person encryption keys.
 *
 * @param int|null $company
 * @return string|false
 */
function hrm_person_sensitive_private_root($company = null)
{
	global $path_to_root, $db_connections;
	if ($company === null)
		$company = function_exists('user_company') ? (int)user_company() : 0;
	$application_root = realpath($path_to_root);
	if ($application_root === false)
		return false;
	if (defined('HRM_PERSON_SENSITIVE_PRIVATE_ROOT')) {
		$base = rtrim((string)HRM_PERSON_SENSITIVE_PRIVATE_ROOT, "\\/");
	} elseif (isset($db_connections[$company]['hrm_person_sensitive_private_root'])) {
		$base = rtrim((string)$db_connections[$company]['hrm_person_sensitive_private_root'], "\\/");
	} else {
		$base = dirname(dirname($application_root)).DIRECTORY_SEPARATOR
			.'.notrinoserp-private'.DIRECTORY_SEPARATOR
			.substr(hash('sha256', $application_root), 0, 16)
			.DIRECTORY_SEPARATOR.'hrm-person-sensitive';
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

/**
 * Create and validate the private key directory.
 *
 * @param int|null $company
 * @return array|false
 */
function ensure_hrm_person_sensitive_private_storage($company = null)
{
	$root = hrm_person_sensitive_private_root($company);
	if ($root === false)
		return false;
	$paths = array('root' => $root, 'keys' => $root.DIRECTORY_SEPARATOR.'keys');
	foreach ($paths as $path) {
		if (is_link($path))
			return false;
		if (!is_dir($path) && !@mkdir($path, 0700, true) && !is_dir($path))
			return false;
		@chmod($path, 0700);
		$permissions = @fileperms($path);
		if (DIRECTORY_SEPARATOR === '/'
			&& ($permissions === false || ($permissions & 0077) !== 0))
			return false;
	}
	return $paths;
}

/**
 * Return fixed keyring paths without creating storage.
 *
 * @param int|null $company
 * @return array|false
 */
function hrm_person_sensitive_keyring_paths($company = null)
{
	$root = hrm_person_sensitive_private_root($company);
	if ($root === false)
		return false;
	return array(
		'root' => $root,
		'keys' => $root.DIRECTORY_SEPARATOR.'keys',
		'manifest' => $root.DIRECTORY_SEPARATOR.'keys'
			.DIRECTORY_SEPARATOR.'person-sensitive-keyring-v1.json',
		'pending' => $root.DIRECTORY_SEPARATOR.'keys'
			.DIRECTORY_SEPARATOR.'person-sensitive-key-rotation-v1.pending.json',
	);
}

/**
 * Clear the request-local marker used to identify an exact migration-created key.
 *
 * @param int|null $company
 * @return void
 */
function hrm_person_sensitive_clear_created_key_marker($company = null)
{
	if ($company === null)
		$company = function_exists('user_company') ? (int)user_company() : 0;
	if (isset($GLOBALS['hrm_person_sensitive_created_keyrings'][(int)$company]))
		unset($GLOBALS['hrm_person_sensitive_created_keyrings'][(int)$company]);
}

/**
 * Return the key ID created by this request, never one won by a concurrent writer.
 *
 * @param int|null $company
 * @return string
 */
function hrm_person_sensitive_created_key_id($company = null)
{
	if ($company === null)
		$company = function_exists('user_company') ? (int)user_company() : 0;
	return isset($GLOBALS['hrm_person_sensitive_created_keyrings'][(int)$company])
		? (string)$GLOBALS['hrm_person_sensitive_created_keyrings'][(int)$company] : '';
}

/**
 * Remove only the exact generation-1 keyring created by this PHP request.
 *
 * Callers must first prove that no encrypted business row needs the key.
 *
 * @param int|null $company
 * @return bool
 */
function remove_hrm_person_sensitive_request_created_keyring($company = null)
{
	if ($company === null)
		$company = function_exists('user_company') ? (int)user_company() : 0;
	$key_id = hrm_person_sensitive_created_key_id($company);
	if ($key_id === '')
		return true;
	$paths = hrm_person_sensitive_keyring_paths($company);
	$manifest = hrm_person_sensitive_keyring($company, false);
	if ($paths === false || !is_array($manifest)
		|| (int)$manifest['generation'] !== 1
		|| count($manifest['keys']) !== 1
		|| (string)$manifest['active_key_id'] !== $key_id)
		return false;
	$key_path = $paths['keys'].DIRECTORY_SEPARATOR
		.'person-sensitive-v1-'.$key_id.'.key';
	$manifest_removed = @unlink($paths['manifest']);
	$key_removed = @unlink($key_path);
	if (!$manifest_removed || !$key_removed)
		return false;
	@rmdir($paths['keys']);
	@rmdir($paths['root']);
	hrm_person_sensitive_clear_created_key_marker($company);
	return true;
}

/**
 * Validate a protected 32-byte key file and its non-secret identifier.
 *
 * @param string $path
 * @param string $key_id
 * @return string|false
 */
function hrm_person_sensitive_load_key_file($path, $key_id)
{
	if (!is_file($path) || is_link($path)
		|| preg_match('/^[a-f0-9]{16}$/D', (string)$key_id) !== 1)
		return false;
	@chmod($path, 0600);
	$permissions = @fileperms($path);
	if (DIRECTORY_SEPARATOR === '/'
		&& ($permissions === false || ($permissions & 0077) !== 0))
		return false;
	$key = base64_decode(trim((string)@file_get_contents($path)), true);
	return is_string($key) && strlen($key) === 32
		&& substr(hash('sha256', $key), 0, 16) === $key_id ? $key : false;
}

/**
 * Validate and load the per-company encryption keyring.
 *
 * Missing keys are created only by an explicitly authorized migration call.
 *
 * @param int|null $company
 * @param bool $allow_create
 * @return array|false
 */
function hrm_person_sensitive_keyring($company = null, $allow_create = false)
{
	if ($company === null)
		$company = function_exists('user_company') ? (int)user_company() : 0;
	$paths = hrm_person_sensitive_keyring_paths($company);
	if ($paths === false)
		return false;
	$rotation_internal = isset($GLOBALS['hrm_person_sensitive_key_rotation_company'])
		&& (int)$GLOBALS['hrm_person_sensitive_key_rotation_company'] === (int)$company;
	if (is_link($paths['root']) || is_link($paths['keys'])
		|| is_link($paths['manifest']) || is_link($paths['pending'])
		|| (is_file($paths['pending']) && !$rotation_internal))
		return false;
	if (!is_file($paths['manifest'])) {
		if (!$allow_create || !function_exists('openssl_random_pseudo_bytes')
			|| ensure_hrm_person_sensitive_private_storage($company) === false)
			return false;
		$strong = false;
		$key = openssl_random_pseudo_bytes(32, $strong);
		if (!is_string($key) || strlen($key) !== 32 || !$strong)
			return false;
		$key_id = substr(hash('sha256', $key), 0, 16);
		$filename = 'person-sensitive-v1-'.$key_id.'.key';
		$key_path = $paths['keys'].DIRECTORY_SEPARATOR.$filename;
		if (!domain_audit_write_key_file($key_path, $key)) {
			if (is_file($paths['manifest']))
				return hrm_person_sensitive_keyring($company, false);
			return false;
		}
		$manifest = array(
			'schema_version' => 'notrinoserp.hrm-person-sensitive-keyring.v1',
			'company_id' => (int)$company,
			'active_key_id' => $key_id,
			'generation' => 1,
			'keys' => array(array(
				'key_id' => $key_id,
				'generation' => 1,
				'filename' => $filename,
				'activated_at' => gmdate('Y-m-d H:i:s'),
				'retired_at' => '',
			)),
		);
		if (!domain_audit_write_private_json($paths['manifest'], $manifest, false)) {
			@unlink($key_path);
			return is_file($paths['manifest'])
				? hrm_person_sensitive_keyring($company, false) : false;
		}
		if (!isset($GLOBALS['hrm_person_sensitive_created_keyrings']))
			$GLOBALS['hrm_person_sensitive_created_keyrings'] = array();
		$GLOBALS['hrm_person_sensitive_created_keyrings'][(int)$company] = $key_id;
	}
	@chmod($paths['manifest'], 0600);
	$json = @file_get_contents($paths['manifest']);
	$manifest = is_string($json) ? json_decode($json, true) : null;
	$expected_manifest_fields = array('active_key_id', 'company_id', 'generation', 'keys', 'schema_version');
	$actual_manifest_fields = is_array($manifest) ? array_keys($manifest) : array();
	sort($expected_manifest_fields);
	sort($actual_manifest_fields);
	if (!is_array($manifest) || $actual_manifest_fields !== $expected_manifest_fields
		|| $manifest['schema_version'] !== 'notrinoserp.hrm-person-sensitive-keyring.v1'
		|| !is_int($manifest['company_id']) || $manifest['company_id'] !== (int)$company
		|| !is_int($manifest['generation']) || $manifest['generation'] < 1
		|| preg_match('/^[a-f0-9]{16}$/D', (string)$manifest['active_key_id']) !== 1
		|| !is_array($manifest['keys'])
		|| count($manifest['keys']) !== (int)$manifest['generation'])
		return false;
	$seen = array();
	$previous_generation = 0;
	$previous_retired_at = null;
	foreach ($manifest['keys'] as $index => $entry) {
		$expected_entry_fields = array('activated_at', 'filename', 'generation', 'key_id', 'retired_at');
		$actual_entry_fields = is_array($entry) ? array_keys($entry) : array();
		sort($expected_entry_fields);
		sort($actual_entry_fields);
		if (!is_array($entry) || $actual_entry_fields !== $expected_entry_fields
			|| !is_int($entry['generation'])
			|| $entry['generation'] !== $previous_generation + 1
			|| preg_match('/^[a-f0-9]{16}$/D', (string)$entry['key_id']) !== 1
			|| isset($seen[$entry['key_id']])
			|| $entry['filename'] !== 'person-sensitive-v1-'.$entry['key_id'].'.key'
			|| preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/D',
				(string)$entry['activated_at']) !== 1
			|| ($entry['retired_at'] !== ''
				&& preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/D',
					(string)$entry['retired_at']) !== 1)
			|| ($index < count($manifest['keys']) - 1 && $entry['retired_at'] === '')
			|| ($index === count($manifest['keys']) - 1 && $entry['retired_at'] !== '')
			|| ($previous_retired_at !== null
				&& $previous_retired_at !== (string)$entry['activated_at'])
			|| hrm_person_sensitive_load_key_file(
				$paths['keys'].DIRECTORY_SEPARATOR.$entry['filename'], $entry['key_id']
			) === false)
			return false;
		$seen[$entry['key_id']] = true;
		$previous_generation = $entry['generation'];
		$previous_retired_at = (string)$entry['retired_at'];
	}
	$last = $manifest['keys'][count($manifest['keys']) - 1];
	return $last['key_id'] === $manifest['active_key_id']
		&& $last['generation'] === $manifest['generation'] ? $manifest : false;
}

/**
 * Load one keyring key by its non-secret identifier.
 *
 * @param string $key_id
 * @param int|null $company
 * @return string|false
 */
function hrm_person_sensitive_key($key_id, $company = null)
{
	$manifest = hrm_person_sensitive_keyring($company, false);
	$paths = hrm_person_sensitive_keyring_paths($company);
	if ($manifest === false || $paths === false)
		return false;
	foreach ($manifest['keys'] as $entry) {
		if ($entry['key_id'] === (string)$key_id)
			return hrm_person_sensitive_load_key_file(
				$paths['keys'].DIRECTORY_SEPARATOR.$entry['filename'], $entry['key_id']
			);
	}
	return false;
}

/**
 * Return the active key record for encryption.
 *
 * @param int|null $company
 * @param bool $allow_create
 * @return array|false
 */
function hrm_person_sensitive_active_key($company = null, $allow_create = false)
{
	$manifest = hrm_person_sensitive_keyring($company, $allow_create);
	if ($manifest === false)
		return false;
	$key = hrm_person_sensitive_key($manifest['active_key_id'], $company);
	return $key === false ? false : array(
		'key_id' => $manifest['active_key_id'],
		'generation' => (int)$manifest['generation'],
		'key' => $key,
	);
}

/**
 * Return stable authenticated context for one encrypted field.
 *
 * @param int $person_id
 * @param string $record_class
 * @param string $value_type
 * @param int|null $company
 * @return string|false
 */
function hrm_person_sensitive_context($person_id, $record_class, $value_type, $company = null)
{
	if ($company === null)
		$company = function_exists('user_company') ? (int)user_company() : 0;
	if ((int)$person_id <= 0
		|| !in_array($record_class, array('contact', 'identifier'), true)
		|| preg_match('/^[a-z][a-z0-9_]{0,31}$/D', (string)$value_type) !== 1)
		return false;
	return 'company='.(int)$company.';person='.(int)$person_id
		.';class='.$record_class.';type='.$value_type;
}

/**
 * Encrypt one value with context-bound encrypt-then-MAC protection.
 *
 * @param string $plaintext
 * @param string $context
 * @param string $master_key
 * @return string|false
 */
function hrm_person_sensitive_encrypt($plaintext, $context, $master_key)
{
	if (!function_exists('openssl_encrypt') || !is_string($plaintext)
		|| $plaintext === '' || strlen($plaintext) > 1024
		|| !is_string($context) || $context === '' || strlen($master_key) !== 32)
		return false;
	$strong = false;
	$iv = openssl_random_pseudo_bytes(16, $strong);
	if (!is_string($iv) || strlen($iv) !== 16 || !$strong)
		return false;
	$encryption_key = hash_hmac('sha256', 'person-sensitive-encryption-v1', $master_key, true);
	$authentication_key = hash_hmac('sha256', 'person-sensitive-authentication-v1', $master_key, true);
	$ciphertext = openssl_encrypt($plaintext, 'AES-256-CBC', $encryption_key,
		OPENSSL_RAW_DATA, $iv);
	if ($ciphertext === false)
		return false;
	$magic = 'NERPHR01';
	$mac = hash_hmac('sha256', $magic.$context.$iv.$ciphertext,
		$authentication_key, true);
	return $magic.$iv.$mac.$ciphertext;
}

/**
 * Authenticate and decrypt one context-bound value.
 *
 * @param string $payload
 * @param string $context
 * @param string $master_key
 * @return string|false
 */
function hrm_person_sensitive_decrypt($payload, $context, $master_key)
{
	if (!function_exists('openssl_decrypt') || !is_string($payload)
		|| strlen($payload) < 72 || substr($payload, 0, 8) !== 'NERPHR01'
		|| !is_string($context) || $context === '' || strlen($master_key) !== 32)
		return false;
	$magic = substr($payload, 0, 8);
	$iv = substr($payload, 8, 16);
	$mac = substr($payload, 24, 32);
	$ciphertext = substr($payload, 56);
	$authentication_key = hash_hmac('sha256', 'person-sensitive-authentication-v1', $master_key, true);
	$expected = hash_hmac('sha256', $magic.$context.$iv.$ciphertext,
		$authentication_key, true);
	if (!domain_audit_hash_equals($expected, $mac))
		return false;
	$encryption_key = hash_hmac('sha256', 'person-sensitive-encryption-v1', $master_key, true);
	return openssl_decrypt($ciphertext, 'AES-256-CBC', $encryption_key,
		OPENSSL_RAW_DATA, $iv);
}

/**
 * Validate a bounded legacy sensitive value without silently normalizing it.
 *
 * @param mixed $value
 * @param int $maximum_length
 * @return string|false
 */
function hrm_person_sensitive_value($value, $maximum_length = 255)
{
	$value = trim((string)$value);
	if ($value === '')
		return '';
	if (strlen($value) > (int)$maximum_length
		|| preg_match('/[\x00-\x1F\x7F]/', $value))
		return false;
	return $value;
}

/**
 * Return a purpose-separated lookup token without storing plaintext.
 *
 * @param string $type
 * @param string $value
 * @param string $master_key
 * @return string|false
 */
function hrm_person_sensitive_lookup_token($type, $value, $master_key)
{
	if (preg_match('/^[a-z][a-z0-9_]{0,31}$/D', (string)$type) !== 1
		|| !is_string($value) || $value === '' || strlen($master_key) !== 32)
		return false;
	$normalized = strtolower(preg_replace('/\s+/', '', trim($value)));
	$key = hash_hmac('sha256', 'person-sensitive-search-v1', $master_key, true);
	return hash_hmac('sha256', $type."\0".$normalized, $key);
}

/**
 * Return a fixed masked value exposing at most four normalized characters.
 *
 * @param string $value
 * @return string
 */
function hrm_person_sensitive_mask($value)
{
	return function_exists('hrm_mask_restricted_value')
		? hrm_mask_restricted_value($value, 4) : '';
}

/**
 * Validate the current encrypted row and compare its exact plaintext value.
 *
 * Lookup tokens are intentionally normalized for duplicate review, so they
 * cannot prove that the authoritative legacy value is byte-for-byte unchanged.
 *
 * @param array $current
 * @param int $person_id
 * @param string $record_class
 * @param string $type
 * @param string $value
 * @param string|null $valid_to
 * @param string|null $issuing_jurisdiction
 * @param array $active_key
 * @return int|false One for exact match, zero for a valid changed row, false on integrity failure.
 */
function hrm_person_sensitive_current_value_state(
	$current,
	$person_id,
	$record_class,
	$type,
	$value,
	$valid_to,
	$issuing_jurisdiction,
	$active_key
) {
	if (!is_array($current) || !isset($current['value_ciphertext'],
		$current['lookup_token'], $current['key_id'], $current['masked_value'])
		|| !is_array($active_key) || !isset($active_key['key_id'], $active_key['key']))
		return false;
	$context = hrm_person_sensitive_context($person_id, $record_class, $type);
	if ($context === false)
		return false;
	$current_key = (string)$current['key_id'] === (string)$active_key['key_id']
		? $active_key['key'] : hrm_person_sensitive_key((string)$current['key_id']);
	if ($current_key === false)
		return false;
	$plaintext = hrm_person_sensitive_decrypt(
		$current['value_ciphertext'], $context, $current_key
	);
	$stored_token = $plaintext === false ? false
		: hrm_person_sensitive_lookup_token($type, $plaintext, $current_key);
	if ($plaintext === false || $stored_token === false
		|| !domain_audit_hash_equals((string)$current['lookup_token'], $stored_token)
		|| (string)$current['masked_value'] !== hrm_person_sensitive_mask($plaintext))
		return false;
	if ((string)$current['key_id'] !== (string)$active_key['key_id']
		|| !domain_audit_hash_equals($plaintext, $value))
		return 0;
	if ($record_class === 'identifier') {
		if ($current['valid_to'] !== $valid_to)
			return 0;
		if ($issuing_jurisdiction !== null
			&& (!isset($current['issuing_jurisdiction'])
				|| (string)$current['issuing_jurisdiction'] !== $issuing_jurisdiction))
			return 0;
	}
	return 1;
}

/**
 * Return true only when both encrypted shadow tables exist.
 *
 * @return bool
 */
function hrm_person_sensitive_tables_ready()
{
	return hrm_person_worker_table_exists('hrm_person_contacts')
		&& hrm_person_worker_table_exists('hrm_person_identifiers');
}

/**
 * Return the allowed legacy-to-contact mapping.
 *
 * @return array
 */
function hrm_person_contact_field_map()
{
	return array('phone' => 'phone', 'mobile' => 'mobile',
		'work_email' => 'email', 'personal_email' => 'personal_email');
}

/**
 * Return the allowed legacy-to-identifier mapping.
 *
 * @return array
 */
function hrm_person_identifier_field_map()
{
	return array('national_id' => 'national_id', 'passport' => 'passport',
		'tax_number' => 'tax_number', 'social_security' => 'social_security_no');
}

/**
 * Deny legacy employee-row mutation of governed identifier values/metadata.
 *
 * Existing employee CRUD may continue to carry unchanged legacy fields for
 * compatibility, but any identifier value deletion/replacement or passport
 * expiry change must use the governed supersession workflow instead. No
 * protected value is returned, logged, or audited by this guard.
 *
 * @param array $before Authoritative pre-update employee row.
 * @param array $changes Requested legacy employee changes (SQL-normalized date when present).
 * @return bool
 */
function hrm_person_identifier_legacy_update_guard($before, $changes)
{
	$GLOBALS['hrm_person_identifier_legacy_update_guard_result'] = 'invalid_input';
	if (!is_array($before) || !is_array($changes))
		return false;

	$fields = array_values(hrm_person_identifier_field_map());
	$fields[] = 'passport_expiry';
	foreach ($fields as $field) {
		if (!array_key_exists($field, $changes))
			continue;
		$prior = array_key_exists($field, $before) && $before[$field] !== null
			? (string)$before[$field] : '';
		$next = $changes[$field] !== null ? (string)$changes[$field] : '';
		if (!hash_equals($prior, $next)) {
			$GLOBALS['hrm_person_identifier_legacy_update_guard_result'] =
				'legacy_employee_identifier_mutation_denied';
			return false;
		}
	}
	$GLOBALS['hrm_person_identifier_legacy_update_guard_result'] = 'unchanged_legacy_identifier_fields';
	return true;
}

/**
 * Normalize an explicitly supplied issuer jurisdiction without inference.
 *
 * The bounded core accepts an ISO-3166-style country code with one optional
 * subdivision token. Country packs remain responsible for authoritative
 * jurisdiction registries and identifier-specific statutory rules.
 *
 * @param mixed $jurisdiction
 * @param bool $allow_blank
 * @return string|false
 */
function hrm_person_sensitive_issuing_jurisdiction($jurisdiction, $allow_blank = false)
{
	$jurisdiction = strtoupper(trim((string)$jurisdiction));
	if ($jurisdiction === '')
		return $allow_blank ? '' : false;
	if (strlen($jurisdiction) > 16
		|| preg_match('/^[A-Z]{2}(?:-[A-Z0-9]{1,8})?$/D', $jurisdiction) !== 1)
		return false;
	return $jurisdiction;
}

/**
 * Validate only portable semantic shape, never jurisdiction-specific validity.
 *
 * Legacy seed rows are retained as unverified evidence. New/changed contacts
 * must be plausible phone/email values. New/changed identifiers require an
 * explicit issuer jurisdiction and a bounded non-empty identifier shape.
 *
 * @param string $record_class
 * @param string $type
 * @param string $value
 * @param mixed $issuing_jurisdiction
 * @return bool
 */
function hrm_person_sensitive_semantic_value_is_valid(
	$record_class,
	$type,
	$value,
	$issuing_jurisdiction = ''
) {
	$value = hrm_person_sensitive_value($value);
	if ($value === false)
		return false;
	if ($value === '')
		return true;

	if ($record_class === 'contact') {
		if (!array_key_exists($type, hrm_person_contact_field_map()))
			return false;
		if ($type === 'work_email' || $type === 'personal_email')
			return strlen($value) <= 254
				&& filter_var($value, FILTER_VALIDATE_EMAIL) !== false;

		if ($type !== 'phone' && $type !== 'mobile'
			|| preg_match('/^[+0-9().\-\/ ]+$/D', $value) !== 1
			|| substr_count($value, '+') > 1
			|| (strpos($value, '+') !== false && strpos($value, '+') !== 0))
			return false;
		$digits = preg_replace('/\D+/', '', $value);
		return strlen($digits) >= 7 && strlen($digits) <= 15;
	}

	if ($record_class !== 'identifier'
		|| !array_key_exists($type, hrm_person_identifier_field_map())
		|| hrm_person_sensitive_issuing_jurisdiction($issuing_jurisdiction, false) === false
		|| strlen($value) > 100)
		return false;
	$alphanumeric = preg_replace('/[^A-Za-z0-9]/', '', $value);
	return strlen($alphanumeric) >= 3;
}

/**
 * Verify stored issuer metadata without promoting legacy identity verification.
 *
 * Blank values remain valid for historical legacy-seed rows because the source
 * did not record an issuer. Non-blank values must already be normalized exact.
 *
 * @return array|false
 */
function verify_hrm_person_sensitive_semantic_metadata()
{
	if (!hrm_person_sensitive_tables_ready())
		return false;
	$result = db_query("SELECT identifier_id,issuing_jurisdiction FROM "
		.TB_PREF."hrm_person_identifiers ORDER BY identifier_id",
		'could not verify HRM person identifier jurisdiction metadata');
	if (!$result)
		return false;
	$rows = 0;
	$captured = 0;
	while ($row = db_fetch_assoc($result)) {
		$rows++;
		$value = (string)$row['issuing_jurisdiction'];
		if ($value === '')
			continue;
		$normalized = hrm_person_sensitive_issuing_jurisdiction($value, false);
		if ($normalized === false || $normalized !== $value)
			return false;
		$captured++;
	}
	return array('identifier_rows' => $rows, 'issuer_jurisdictions_captured' => $captured);
}

/**
 * Append a PII-free shadow synchronization event in the caller transaction.
 *
 * @param array $binding
 * @param array $contact_types
 * @param array $identifier_types
 * @param string $source
 * @return array|false
 */
function append_hrm_person_sensitive_domain_audit(
	$binding,
	$contact_types,
	$identifier_types,
	$source
) {
	if (!is_array($binding) || empty($binding['person_id'])
		|| empty($binding['worker_id']) || empty($binding['employee_number']))
		return false;
	$contact_types = array_values(array_unique($contact_types));
	$identifier_types = array_values(array_unique($identifier_types));
	sort($contact_types);
	sort($identifier_types);
	return append_domain_audit_event(array(
		'company_id' => function_exists('user_company') ? (int)user_company() : 0,
		'legal_entity_id' => 0,
		'entity_type' => 'hrm_person_sensitive_profile',
		'entity_key' => array('person_id' => (int)$binding['person_id'],
			'worker_id' => (int)$binding['worker_id'],
			'employee_number' => (int)$binding['employee_number']),
		'action' => 'hrm.person_sensitive.synchronized',
		'reason_code' => 'hrm_fnd_001_person_sensitive_sync',
		'before' => null,
		'after' => array('shadow_versions_changed' => true),
		'restricted_diff' => array(
			'contact_types' => $contact_types,
			'identifier_types' => $identifier_types,
			'source' => preg_replace('/[^a-z0-9_]/', '', strtolower((string)$source)),
			'restricted_values_stored' => false,
		),
	));
}

/**
 * Synchronize one contact or identifier type from a legacy value.
 *
 * @param string $record_class
 * @param int $person_id
 * @param string $type
 * @param string $value
 * @param string|null $valid_to
 * @param string $source
 * @param array $active_key
 * @param string|null $issuing_jurisdiction Explicit only; never inferred.
 * @return int|false Zero means no change; one means a new effective version.
 */
function synchronize_hrm_person_sensitive_value(
	$record_class,
	$person_id,
	$type,
	$value,
	$valid_to,
	$source,
	$active_key,
	$issuing_jurisdiction = null
) {
	$table = $record_class === 'contact'
		? 'hrm_person_contacts' : ($record_class === 'identifier'
			? 'hrm_person_identifiers' : '');
	$id_column = $record_class === 'contact' ? 'contact_id' : 'identifier_id';
	$type_column = $record_class === 'contact' ? 'contact_type' : 'identifier_type';
	if ($table === '' || (int)$person_id <= 0
		|| !is_array($active_key) || !isset($active_key['key_id'], $active_key['key']))
		return false;
	$value = hrm_person_sensitive_value($value,
		$record_class === 'contact' ? 255 : 255);
	if ($value === false)
		return false;
	$sql = "SELECT ".$id_column.",value_ciphertext,lookup_token,key_id,masked_value,effective_from,"
		."effective_to".($record_class === 'identifier' ? ',valid_to,issuing_jurisdiction' : '')
		." FROM ".TB_PREF.$table." WHERE person_id=".(int)$person_id
		." AND ".$type_column."=".db_escape($type)
		." AND effective_to IS NULL ORDER BY ".$id_column." DESC LIMIT 2 FOR UPDATE";
	$result = db_query($sql, 'could not read current HRM person sensitive value');
	if (!$result || db_num_rows($result) > 1)
		return false;
	$current = db_num_rows($result) === 1 ? db_fetch_assoc($result) : false;
	if ($record_class === 'identifier' && $source === 'runtime_employee' && $current && $value === '')
		return false;
	if ($value === '') {
		if (!$current)
			return 0;
		$now = hrm_person_worker_utc_now();
		return $now !== false && db_query("UPDATE ".TB_PREF.$table
			." SET effective_to=".db_escape($now)." WHERE ".$id_column."="
			.(int)$current[$id_column]." AND effective_to IS NULL",
			'could not close HRM person sensitive value') ? 1 : false;
	}
	$token = hrm_person_sensitive_lookup_token($type, $value, $active_key['key']);
	$masked = hrm_person_sensitive_mask($value);
	$normalized_valid_to = $valid_to === null || $valid_to === '' ? null : (string)$valid_to;
	$normalized_jurisdiction = null;
	if ($record_class === 'identifier' && $issuing_jurisdiction !== null) {
		$normalized_jurisdiction = hrm_person_sensitive_issuing_jurisdiction(
			$issuing_jurisdiction, $value === ''
		);
		if ($normalized_jurisdiction === false)
			return false;
	}
	if ($token === false || $normalized_valid_to !== null) {
		$date = $normalized_valid_to === null ? false
			: DateTime::createFromFormat('!Y-m-d', $normalized_valid_to);
		if ($token === false || !$date || $date->format('Y-m-d') !== $normalized_valid_to)
			return false;
	}
	if ($current) {
		$current_state = hrm_person_sensitive_current_value_state(
			$current, $person_id, $record_class, $type, $value,
			$normalized_valid_to, $normalized_jurisdiction, $active_key
		);
		if ($current_state === false)
			return false;
		if ($current_state === 1)
			return 0;
		if ($record_class === 'identifier' && $source === 'runtime_employee')
			return false;
	}
	if ($current && $source === 'legacy_employee_seed')
		return false;
	if ($source !== 'legacy_employee_seed') {
		if ($record_class === 'identifier' && $normalized_jurisdiction === null)
			return false;
		if (!hrm_person_sensitive_semantic_value_is_valid(
			$record_class, $type, $value,
			$record_class === 'identifier' ? $normalized_jurisdiction : ''
		))
			return false;
	}
	$now = $source === 'legacy_employee_seed' ? null : hrm_person_worker_utc_now();
	if ($source !== 'legacy_employee_seed' && $now === false)
		return false;
	if ($current && !db_query("UPDATE ".TB_PREF.$table." SET effective_to="
		.db_escape($now)." WHERE ".$id_column."=".(int)$current[$id_column]
		." AND effective_to IS NULL", 'could not close HRM person sensitive version'))
		return false;
	$context = hrm_person_sensitive_context($person_id, $record_class, $type);
	$ciphertext = hrm_person_sensitive_encrypt($value, $context, $active_key['key']);
	if ($context === false || $ciphertext === false)
		return false;
	$columns = 'person_id,'.$type_column.',value_ciphertext,masked_value,lookup_token,'
		.'key_id,effective_from,effective_to,source,verification_status,created_by';
	$values = (int)$person_id.','.db_escape($type).','.db_escape($ciphertext).','
		.db_escape($masked).','.db_escape($token).','.db_escape($active_key['key_id']).','
		.($now === null ? 'NULL' : db_escape($now)).",NULL,"
		.db_escape($source).",'unverified',".hrm_person_worker_actor_id();
	if ($record_class === 'contact') {
		$columns .= ',is_primary';
		$values .= ',1';
	} else {
		$columns .= ',issuing_jurisdiction,valid_to,verification_method,approval_status,row_version';
		$values .= ','.db_escape($normalized_jurisdiction === null ? '' : $normalized_jurisdiction)
			.','.($normalized_valid_to === null
			? 'NULL' : db_escape($normalized_valid_to))
			.",''".",'unapproved',1";
	}
	return db_query("INSERT INTO ".TB_PREF.$table." (".$columns.") VALUES ("
		.$values.")", 'could not create encrypted HRM person sensitive version') ? 1 : false;
}

/**
 * Synchronize all bounded contact and identifier fields from one employee row.
 *
 * @param array $employee
 * @param bool $audit
 * @param string $source
 * @param bool $allow_key_create
 * @return bool
 */
function synchronize_hrm_person_sensitive_from_employee(
	$employee,
	$audit = true,
	$source = 'runtime_employee',
	$allow_key_create = false
) {
	if (!is_array($employee) || empty($employee['employee_number'])
		|| !hrm_person_sensitive_tables_ready())
		return false;
	$binding = get_hrm_person_worker_binding((int)$employee['employee_number'], true);
	$active_key = hrm_person_sensitive_active_key(null, $allow_key_create);
	if (!$binding || $binding['retired_at'] !== null || $active_key === false)
		return false;
	$changed_contacts = array();
	$changed_identifiers = array();
	foreach (hrm_person_contact_field_map() as $type => $field) {
		$changed = synchronize_hrm_person_sensitive_value(
			'contact', (int)$binding['person_id'], $type,
			isset($employee[$field]) ? $employee[$field] : '', null,
			$source, $active_key
		);
		if ($changed === false)
			return false;
		if ($changed === 1)
			$changed_contacts[] = $type;
	}
	foreach (hrm_person_identifier_field_map() as $type => $field) {
		$valid_to = $type === 'passport' && !empty($employee['passport_expiry'])
			? (string)$employee['passport_expiry'] : null;
		$jurisdiction_field = $field.'_issuing_jurisdiction';
		$issuing_jurisdiction = array_key_exists($jurisdiction_field, $employee)
			? $employee[$jurisdiction_field] : null;
		$changed = synchronize_hrm_person_sensitive_value(
			'identifier', (int)$binding['person_id'], $type,
			isset($employee[$field]) ? $employee[$field] : '', $valid_to,
			$source, $active_key, $issuing_jurisdiction
		);
		if ($changed === false)
			return false;
		if ($changed === 1)
			$changed_identifiers[] = $type;
	}
	if (empty($changed_contacts) && empty($changed_identifiers))
		return true;
	return !$audit || append_hrm_person_sensitive_domain_audit(
		$binding, $changed_contacts, $changed_identifiers, $source
	) !== false;
}

/**
 * Build one masked as-of projection without ciphertext, token or key metadata.
 *
 * @param string $record_class
 * @param int|string $employee_ref
 * @param string $as_of
 * @return string|false
 */
function get_hrm_person_sensitive_masked_as_of_sql($record_class, $employee_ref, $as_of)
{
	$as_of = hrm_person_worker_normalize_as_of($as_of);
	$table = $record_class === 'contact' ? 'hrm_person_contacts'
		: ($record_class === 'identifier' ? 'hrm_person_identifiers' : '');
	if ($as_of === false || $table === '')
		return false;
	$column = is_numeric($employee_ref) ? 'employee_number' : 'employee_id';
	$value = is_numeric($employee_ref) ? (int)$employee_ref : trim((string)$employee_ref);
	if (($column === 'employee_number' && $value <= 0)
		|| ($column === 'employee_id' && $value === ''))
		return false;
	$alias = $record_class === 'contact' ? 'c' : 'i';
	$type_column = $record_class === 'contact' ? 'contact_type' : 'identifier_type';
	$extra = $record_class === 'contact' ? ',c.is_primary'
		: ',i.issuing_jurisdiction,i.valid_to,i.verification_method,i.approval_status,i.row_version';
	$as_of_sql = db_escape($as_of);
	return "SELECT ".$alias.".".$type_column.",".$alias.".masked_value,"
		.$alias.".verification_status,".$alias.".effective_from,"
		.$alias.".effective_to".$extra." FROM ".TB_PREF."hrm_employee_worker_map m "
		."INNER JOIN ".TB_PREF."hrm_workers w ON w.worker_id=m.worker_id "
		."INNER JOIN ".TB_PREF.$table." ".$alias." ON ".$alias.".person_id=w.person_id "
		."WHERE m.".$column."=".db_escape($value)
		." AND (m.retired_at IS NULL OR m.retired_at>".$as_of_sql.")"
		." AND (".$alias.".effective_from IS NULL OR ".$alias.".effective_from<=".$as_of_sql.")"
		." AND (".$alias.".effective_to IS NULL OR ".$alias.".effective_to>".$as_of_sql.")"
		." ORDER BY ".$alias.".".$type_column.",".$alias.".effective_from DESC";
}

/**
 * Return permissioned masked contact rows for employee maintenance only.
 *
 * @param int|string $employee_ref
 * @param string $as_of
 * @return array|false
 */
function get_hrm_person_contacts_masked_as_of($employee_ref, $as_of)
{
	if (!function_exists('user_check_access') || !user_check_access('SA_EMPLOYEE')
		|| !hrm_person_sensitive_tables_ready())
		return false;
	$sql = get_hrm_person_sensitive_masked_as_of_sql('contact', $employee_ref, $as_of);
	if ($sql === false)
		return false;
	$result = db_query($sql, 'could not read masked HRM person contacts');
	if (!$result)
		return false;
	$rows = array();
	while ($row = db_fetch_assoc($result))
		$rows[] = $row;
	hrm_log_sensitive_field_access('private_contact', 'view', 'granted_masked_shadow');
	return $rows;
}

/**
 * Return permissioned masked identifier rows under the existing identity policy.
 *
 * @param int|string $employee_ref
 * @param string $as_of
 * @return array|false
 */
function get_hrm_person_identifiers_masked_as_of($employee_ref, $as_of)
{
	if (!hrm_user_can_access_sensitive_field(
		HRM_FIELD_RESTRICTED_IDENTITY, HRM_FIELD_ACTION_VIEW
	) || !hrm_person_sensitive_tables_ready())
		return false;
	$sql = get_hrm_person_sensitive_masked_as_of_sql('identifier', $employee_ref, $as_of);
	if ($sql === false)
		return false;
	$result = db_query($sql, 'could not read masked HRM person identifiers');
	if (!$result)
		return false;
	$rows = array();
	while ($row = db_fetch_assoc($result))
		$rows[] = $row;
	hrm_log_sensitive_field_access(
		HRM_FIELD_RESTRICTED_IDENTITY, HRM_FIELD_ACTION_VIEW,
		'granted_masked_shadow'
	);
	return $rows;
}
