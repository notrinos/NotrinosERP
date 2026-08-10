<?php
/**
 * HRM-FND-001 controlled multi-generation rotation for Person-sensitive data.
 *
 * The encrypted contact/identifier shadow remains non-authoritative. Rotation
 * changes only ciphertext, keyed lookup tokens and key identifiers; row IDs,
 * masks, effective dates, verification state and legacy employee data remain
 * unchanged. The ceremony is CLI-only and fail-closed while recovery is pending.
 */

if (!function_exists('hrm_person_sensitive_active_key'))
	include_once($path_to_root.'/hrm/includes/db/employee_person_sensitive_db.inc');

/**
 * Authorize Person-sensitive key administration against a dedicated OS account.
 *
 * @param int $company
 * @return string|false
 */
function hrm_person_sensitive_key_rotation_authorized($company)
{
	global $db_connections;
	if (PHP_SAPI !== 'cli' || !function_exists('domain_audit_key_admin_os_identity'))
		return false;
	$expected = defined('HRM_PERSON_SENSITIVE_KEY_ADMIN_OS_USER')
		? (string)HRM_PERSON_SENSITIVE_KEY_ADMIN_OS_USER
		: (isset($db_connections[$company]['hrm_person_sensitive_key_admin_os_user'])
			? (string)$db_connections[$company]['hrm_person_sensitive_key_admin_os_user'] : '');
	$actual = domain_audit_key_admin_os_identity();
	if ($expected === '' || strlen($expected) > 128
		|| preg_match('/^[A-Za-z0-9_.@\\\\-]+$/D', $expected) !== 1
		|| $actual === false
		|| !domain_audit_hash_equals(strtolower($expected), strtolower($actual)))
		return false;
	return $actual;
}

/**
 * Acquire a company-scoped database mutex for the rotation coordinator.
 *
 * @param int $company
 * @return bool
 */
function hrm_person_sensitive_key_rotation_acquire_lock($company)
{
	$company = (int)$company;
	if (!isset($GLOBALS['hrm_person_sensitive_key_rotation_lock_depth']))
		$GLOBALS['hrm_person_sensitive_key_rotation_lock_depth'] = array();
	if (!empty($GLOBALS['hrm_person_sensitive_key_rotation_lock_depth'][$company])) {
		$GLOBALS['hrm_person_sensitive_key_rotation_lock_depth'][$company]++;
		return true;
	}
	$name = 'notrinoserp:hrm:person-sensitive:key:'
		.substr(hash('sha256', (string)$company), 0, 24);
	$result = db_query('SELECT GET_LOCK('.db_escape($name).',10) acquired',
		'could not acquire HRM Person-sensitive key rotation lock');
	$row = $result ? db_fetch_assoc($result) : false;
	if (!$row || (int)$row['acquired'] !== 1)
		return false;
	$GLOBALS['hrm_person_sensitive_key_rotation_lock_depth'][$company] = 1;
	$GLOBALS['hrm_person_sensitive_key_rotation_lock_name'][$company] = $name;
	return true;
}

/**
 * Release the company-scoped rotation mutex.
 *
 * @param int $company
 * @return bool
 */
function hrm_person_sensitive_key_rotation_release_lock($company)
{
	$company = (int)$company;
	if (empty($GLOBALS['hrm_person_sensitive_key_rotation_lock_depth'][$company]))
		return false;
	$GLOBALS['hrm_person_sensitive_key_rotation_lock_depth'][$company]--;
	if ($GLOBALS['hrm_person_sensitive_key_rotation_lock_depth'][$company] > 0)
		return true;
	$name = isset($GLOBALS['hrm_person_sensitive_key_rotation_lock_name'][$company])
		? $GLOBALS['hrm_person_sensitive_key_rotation_lock_name'][$company] : '';
	unset($GLOBALS['hrm_person_sensitive_key_rotation_lock_depth'][$company]);
	unset($GLOBALS['hrm_person_sensitive_key_rotation_lock_name'][$company]);
	$result = $name === '' ? false : db_query(
		'SELECT RELEASE_LOCK('.db_escape($name).') released',
		'could not release HRM Person-sensitive key rotation lock'
	);
	$row = $result ? db_fetch_assoc($result) : false;
	return $row && (int)$row['released'] === 1;
}

/**
 * Load and strictly validate a pending rotation journal.
 *
 * @param int $company
 * @return array|false
 */
function hrm_person_sensitive_key_rotation_pending($company)
{
	$paths = hrm_person_sensitive_keyring_paths($company);
	if ($paths === false || !is_file($paths['pending']) || is_link($paths['pending'])
		|| filesize($paths['pending']) > 65536)
		return false;
	@chmod($paths['pending'], 0600);
	$permissions = @fileperms($paths['pending']);
	if (DIRECTORY_SEPARATOR === '/'
		&& ($permissions === false || ($permissions & 0077) !== 0))
		return false;
	$json = @file_get_contents($paths['pending']);
	$pending = is_string($json) ? json_decode($json, true) : null;
	$expected = array('actor', 'company_id', 'correlation_id', 'filename', 'generation',
		'new_key_id', 'occurred_at', 'previous_key_id', 'reason_code', 'schema_version');
	$actual = is_array($pending) ? array_keys($pending) : array();
	sort($expected);
	sort($actual);
	if ($actual !== $expected
		|| $pending['schema_version'] !== 'notrinoserp.hrm-person-sensitive-key-rotation-pending.v1'
		|| !is_int($pending['company_id']) || $pending['company_id'] !== (int)$company
		|| !is_int($pending['generation']) || $pending['generation'] < 2
		|| preg_match('/^[a-f0-9]{16}$/D', (string)$pending['previous_key_id']) !== 1
		|| preg_match('/^[a-f0-9]{16}$/D', (string)$pending['new_key_id']) !== 1
		|| $pending['previous_key_id'] === $pending['new_key_id']
		|| $pending['filename'] !== 'person-sensitive-v1-'.$pending['new_key_id'].'.key'
		|| preg_match('/^[a-f0-9]{32}$/D', (string)$pending['correlation_id']) !== 1
		|| !domain_audit_valid_token($pending['reason_code'])
		|| preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/D',
			(string)$pending['occurred_at']) !== 1
		|| !is_string($pending['actor']) || $pending['actor'] === ''
		|| strlen($pending['actor']) > 128)
		return false;
	return $pending;
}

/**
 * Build the next persisted keyring after a committed rotation.
 *
 * @param array $manifest
 * @param array $pending
 * @return array|false
 */
function hrm_person_sensitive_keyring_after_rotation($manifest, $pending)
{
	if (!is_array($manifest) || !is_array($pending)
		|| (int)$manifest['generation'] + 1 !== (int)$pending['generation']
		|| !domain_audit_hash_equals((string)$manifest['active_key_id'],
			(string)$pending['previous_key_id']))
		return false;
	$keys = $manifest['keys'];
	$last = count($keys) - 1;
	if ($last < 0 || $keys[$last]['retired_at'] !== '')
		return false;
	$keys[$last]['retired_at'] = (string)$pending['occurred_at'];
	$keys[] = array(
		'key_id' => (string)$pending['new_key_id'],
		'generation' => (int)$pending['generation'],
		'filename' => (string)$pending['filename'],
		'activated_at' => (string)$pending['occurred_at'],
		'retired_at' => '',
	);
	return array(
		'schema_version' => 'notrinoserp.hrm-person-sensitive-keyring.v1',
		'company_id' => (int)$pending['company_id'],
		'active_key_id' => (string)$pending['new_key_id'],
		'generation' => (int)$pending['generation'],
		'keys' => $keys,
	);
}

/**
 * Pure row transformation used by the database coordinator and tests.
 *
 * @param array $row
 * @param string $record_class
 * @param string $type
 * @param string $new_key_id
 * @param string $new_key
 * @param int|null $company
 * @return array|false
 */
function hrm_person_sensitive_reencrypt_row(
	$row,
	$record_class,
	$type,
	$new_key_id,
	$new_key,
	$company = null
) {
	if (!is_array($row) || !isset($row['person_id'], $row['value_ciphertext'],
		$row['lookup_token'], $row['key_id'], $row['masked_value'])
		|| preg_match('/^[a-f0-9]{16}$/D', (string)$new_key_id) !== 1
		|| !is_string($new_key) || strlen($new_key) !== 32)
		return false;
	$context = hrm_person_sensitive_context((int)$row['person_id'], $record_class, $type, $company);
	$old_key = $context === false ? false
		: hrm_person_sensitive_key((string)$row['key_id'], $company);
	$plaintext = $old_key === false ? false
		: hrm_person_sensitive_decrypt($row['value_ciphertext'], $context, $old_key);
	$old_token = $plaintext === false ? false
		: hrm_person_sensitive_lookup_token($type, $plaintext, $old_key);
	if ($plaintext === false || $old_token === false
		|| !domain_audit_hash_equals((string)$row['lookup_token'], $old_token)
		|| (string)$row['masked_value'] !== hrm_person_sensitive_mask($plaintext))
		return false;
	$new_ciphertext = hrm_person_sensitive_encrypt($plaintext, $context, $new_key);
	$new_token = hrm_person_sensitive_lookup_token($type, $plaintext, $new_key);
	return $new_ciphertext === false || $new_token === false ? false : array(
		'value_ciphertext' => $new_ciphertext,
		'lookup_token' => $new_token,
		'key_id' => (string)$new_key_id,
		'masked_value' => (string)$row['masked_value'],
	);
}

/**
 * Re-encrypt every row in one shadow table while preserving business history.
 * Caller owns the database transaction and pending-journal recovery boundary.
 *
 * @param string $record_class
 * @param string $new_key_id
 * @param string $new_key
 * @param int $company
 * @return int|false
 */
function hrm_person_sensitive_reencrypt_table($record_class, $new_key_id, $new_key, $company)
{
	$meta = $record_class === 'contact'
		? array('table' => 'hrm_person_contacts', 'id' => 'contact_id', 'type' => 'contact_type')
		: ($record_class === 'identifier'
			? array('table' => 'hrm_person_identifiers', 'id' => 'identifier_id', 'type' => 'identifier_type')
			: false);
	if ($meta === false)
		return false;
	$result = db_query("SELECT ".$meta['id'].",person_id,".$meta['type']
		.",value_ciphertext,lookup_token,key_id,masked_value FROM ".TB_PREF.$meta['table']
		." ORDER BY ".$meta['id']." FOR UPDATE",
		'could not lock HRM Person-sensitive rows for key rotation');
	if (!$result)
		return false;
	$count = 0;
	while ($row = db_fetch_assoc($result)) {
		$type = (string)$row[$meta['type']];
		$next = hrm_person_sensitive_reencrypt_row(
			$row, $record_class, $type, $new_key_id, $new_key, $company
		);
		if ($next === false || !db_query("UPDATE ".TB_PREF.$meta['table']
			." SET value_ciphertext=".db_escape($next['value_ciphertext'])
			.",lookup_token=".db_escape($next['lookup_token'])
			.",key_id=".db_escape($next['key_id'])
			." WHERE ".$meta['id']."=".(int)$row[$meta['id']]
			." AND key_id=".db_escape((string)$row['key_id']),
			'could not re-encrypt HRM Person-sensitive row')
			|| !function_exists('db_num_affected_rows') || db_num_affected_rows() !== 1)
			return false;
		$count++;
	}
	return $count;
}


/**
 * Authenticate the complete encrypted shadow without changing any row.
 *
 * Successful rotations bind every historical row to the active generation, so
 * an old/unknown key ID, token mismatch, mask mismatch or ciphertext failure
 * blocks source activation and operator status checks.
 *
 * @param int $company
 * @return array|false
 */
function verify_hrm_person_sensitive_encrypted_inventory($company)
{
	$company = (int)$company;
	$manifest = hrm_person_sensitive_keyring($company, false);
	if ($company < 0 || $manifest === false || !hrm_person_sensitive_tables_ready())
		return false;
	$counts = array('contact_rows' => 0, 'identifier_rows' => 0);
	foreach (array(
		'contact' => array('table' => 'hrm_person_contacts', 'id' => 'contact_id',
			'type' => 'contact_type', 'count' => 'contact_rows'),
		'identifier' => array('table' => 'hrm_person_identifiers', 'id' => 'identifier_id',
			'type' => 'identifier_type', 'count' => 'identifier_rows'),
	) as $record_class => $meta) {
		$result = db_query("SELECT ".$meta['id'].",person_id,".$meta['type']
			.",value_ciphertext,lookup_token,key_id,masked_value FROM ".TB_PREF.$meta['table']
			." ORDER BY ".$meta['id'],
			'could not verify HRM Person-sensitive encrypted inventory');
		if (!$result)
			return false;
		while ($row = db_fetch_assoc($result)) {
			if (!domain_audit_hash_equals((string)$row['key_id'],
				(string)$manifest['active_key_id']))
				return false;
			$key = hrm_person_sensitive_key((string)$row['key_id'], $company);
			$context = hrm_person_sensitive_context((int)$row['person_id'], $record_class,
				(string)$row[$meta['type']], $company);
			$plaintext = $key === false || $context === false ? false
				: hrm_person_sensitive_decrypt($row['value_ciphertext'], $context, $key);
			$token = $plaintext === false ? false : hrm_person_sensitive_lookup_token(
				(string)$row[$meta['type']], $plaintext, $key
			);
			if ($plaintext === false || $token === false
				|| !domain_audit_hash_equals((string)$row['lookup_token'], $token)
				|| (string)$row['masked_value'] !== hrm_person_sensitive_mask($plaintext))
				return false;
			$counts[$meta['count']]++;
		}
	}
	return $counts;
}

/**
 * Find and validate the committed PAY-AUD transition for one pending journal.
 *
 * @param array $pending
 * @return array|false|null False=mismatch, null=not committed.
 */
function hrm_person_sensitive_committed_key_rotation($pending)
{
	$result = db_query("SELECT event_id,chain_scope,chain_sequence,occurred_at,company_id,legal_entity_id,"
		."entity_type,action,reason_code,correlation_id,restricted_diff FROM ".TB_PREF."domain_audit_events"
		." WHERE chain_scope=".db_escape('company:'.(int)$pending['company_id'].':legal_entity:0')
		." AND correlation_id=".db_escape($pending['correlation_id'])
		." ORDER BY event_id LIMIT 2",
		'could not inspect HRM Person-sensitive key rotation evidence');
	if (!$result)
		return false;
	$rows = array();
	while ($row = db_fetch_assoc($result))
		$rows[] = $row;
	if (count($rows) === 0)
		return null;
	if (count($rows) !== 1)
		return false;
	$row = $rows[0];
	$diff = json_decode((string)$row['restricted_diff'], true);
	$expected_diff_fields = array('contact_rows', 'generation', 'identifier_rows',
		'new_key_id', 'previous_key_id', 'restricted_values_stored');
	$actual_diff_fields = is_array($diff) ? array_keys($diff) : array();
	sort($expected_diff_fields);
	sort($actual_diff_fields);
	if ((string)$row['chain_scope'] !== 'company:'.(int)$pending['company_id'].':legal_entity:0'
		|| (int)$row['company_id'] !== (int)$pending['company_id']
		|| (int)$row['legal_entity_id'] !== 0
		|| (string)$row['entity_type'] !== 'hrm_person_sensitive_key'
		|| (string)$row['action'] !== 'hrm.person_sensitive.key_rotated'
		|| (string)$row['reason_code'] !== (string)$pending['reason_code']
		|| (string)$row['occurred_at'] !== (string)$pending['occurred_at']
		|| !is_array($diff) || $actual_diff_fields !== $expected_diff_fields
		|| (int)$diff['generation'] !== (int)$pending['generation']
		|| (string)$diff['new_key_id'] !== (string)$pending['new_key_id']
		|| (string)$diff['previous_key_id'] !== (string)$pending['previous_key_id']
		|| $diff['restricted_values_stored'] !== false
		|| !is_int($diff['contact_rows']) || $diff['contact_rows'] < 0
		|| !is_int($diff['identifier_rows']) || $diff['identifier_rows'] < 0)
		return false;
	$verification = verify_domain_audit_chain((int)$pending['company_id'], 0);
	return !empty($verification['ok']) ? $row : false;
}

/**
 * Verify that every encrypted row is now bound to the expected key generation.
 *
 * @param array $pending
 * @param array $event
 * @return bool
 */
function hrm_person_sensitive_rotation_rows_match($pending, $event, $new_key)
{
	$diff = json_decode((string)$event['restricted_diff'], true);
	if (!is_array($diff) || !is_string($new_key) || strlen($new_key) !== 32)
		return false;
	foreach (array(
		'contact_rows' => array('table' => 'hrm_person_contacts', 'id' => 'contact_id',
			'type' => 'contact_type', 'class' => 'contact'),
		'identifier_rows' => array('table' => 'hrm_person_identifiers', 'id' => 'identifier_id',
			'type' => 'identifier_type', 'class' => 'identifier'),
	) as $count_key => $meta) {
		$result = db_query("SELECT ".$meta['id'].",person_id,".$meta['type']
			.",value_ciphertext,lookup_token,key_id,masked_value FROM ".TB_PREF.$meta['table']
			." ORDER BY ".$meta['id'],
			'could not authenticate HRM Person-sensitive rotation rows');
		if (!$result)
			return false;
		$count = 0;
		while ($row = db_fetch_assoc($result)) {
			if (!domain_audit_hash_equals((string)$row['key_id'], (string)$pending['new_key_id']))
				return false;
			$context = hrm_person_sensitive_context((int)$row['person_id'], $meta['class'],
				(string)$row[$meta['type']], (int)$pending['company_id']);
			$plaintext = $context === false ? false
				: hrm_person_sensitive_decrypt($row['value_ciphertext'], $context, $new_key);
			$token = $plaintext === false ? false : hrm_person_sensitive_lookup_token(
				(string)$row[$meta['type']], $plaintext, $new_key
			);
			if ($plaintext === false || $token === false
				|| !domain_audit_hash_equals((string)$row['lookup_token'], $token)
				|| (string)$row['masked_value'] !== hrm_person_sensitive_mask($plaintext))
				return false;
			$count++;
		}
		if ($count !== (int)$diff[$count_key])
			return false;
	}
	return true;
}


/**
 * Test-only failure-injection checkpoint.
 *
 * @param string $step
 * @return bool
 */
function hrm_person_sensitive_key_rotation_checkpoint($step)
{
	return !defined('HRM_PERSON_SENSITIVE_KEY_ROTATION_TESTING')
		|| HRM_PERSON_SENSITIVE_KEY_ROTATION_TESTING !== true
		|| !isset($GLOBALS['hrm_person_sensitive_key_rotation_test_checkpoint'])
		|| !is_callable($GLOBALS['hrm_person_sensitive_key_rotation_test_checkpoint'])
		|| call_user_func($GLOBALS['hrm_person_sensitive_key_rotation_test_checkpoint'], (string)$step);
}

/**
 * Rotate the company Person-sensitive key and re-encrypt the complete shadow.
 *
 * @param int $company
 * @param string $reason_code
 * @return array|false
 */
function rotate_hrm_person_sensitive_key($company, $reason_code)
{
	$GLOBALS['hrm_person_sensitive_key_rotation_result'] = 'validation_started';
	$company = (int)$company;
	$actor = hrm_person_sensitive_key_rotation_authorized($company);
	if ($company < 0 || !domain_audit_valid_token($reason_code)) {
		$GLOBALS['hrm_person_sensitive_key_rotation_result'] = 'validation_failed';
		return false;
	}
	if ($actor === false) {
		$GLOBALS['hrm_person_sensitive_key_rotation_result'] = 'authorization_failed';
		return false;
	}
	if (!hrm_person_sensitive_key_rotation_acquire_lock($company)) {
		$GLOBALS['hrm_person_sensitive_key_rotation_result'] = 'key_lock_failed';
		return false;
	}
	$paths = hrm_person_sensitive_keyring_paths($company);
	if ($paths === false || is_file($paths['pending'])) {
		$GLOBALS['hrm_person_sensitive_key_rotation_result'] = 'recovery_required';
		hrm_person_sensitive_key_rotation_release_lock($company);
		return false;
	}
	$manifest = hrm_person_sensitive_keyring($company, false);
	$inventory = $manifest === false ? false
		: verify_hrm_person_sensitive_encrypted_inventory($company);
	if ($manifest === false || $inventory === false || !hrm_person_sensitive_tables_ready()) {
		$GLOBALS['hrm_person_sensitive_key_rotation_result'] = 'prerequisite_verification_failed';
		hrm_person_sensitive_key_rotation_release_lock($company);
		return false;
	}
	$strong = false;
	$new_key = openssl_random_pseudo_bytes(32, $strong);
	$new_key_id = is_string($new_key) && strlen($new_key) === 32 && $strong
		? substr(hash('sha256', $new_key), 0, 16) : false;
	if ($new_key_id === false || $new_key_id === $manifest['active_key_id']) {
		$GLOBALS['hrm_person_sensitive_key_rotation_result'] = 'key_generation_failed';
		hrm_person_sensitive_key_rotation_release_lock($company);
		return false;
	}
	$filename = 'person-sensitive-v1-'.$new_key_id.'.key';
	$new_key_path = $paths['keys'].DIRECTORY_SEPARATOR.$filename;
	if (!domain_audit_write_key_file($new_key_path, $new_key)) {
		$GLOBALS['hrm_person_sensitive_key_rotation_result'] = 'key_write_failed';
		hrm_person_sensitive_key_rotation_release_lock($company);
		return false;
	}
	$pending = array(
		'schema_version' => 'notrinoserp.hrm-person-sensitive-key-rotation-pending.v1',
		'company_id' => $company,
		'previous_key_id' => (string)$manifest['active_key_id'],
		'new_key_id' => $new_key_id,
		'generation' => (int)$manifest['generation'] + 1,
		'filename' => $filename,
		'occurred_at' => gmdate('Y-m-d H:i:s'),
		'correlation_id' => domain_audit_correlation_id(),
		'reason_code' => (string)$reason_code,
		'actor' => (string)$actor,
	);
	if ($pending['correlation_id'] === false
		|| !domain_audit_write_private_json($paths['pending'], $pending, false)) {
		$GLOBALS['hrm_person_sensitive_key_rotation_result'] = 'pending_journal_failed';
		@unlink($new_key_path);
		hrm_person_sensitive_key_rotation_release_lock($company);
		return false;
	}
	$GLOBALS['hrm_person_sensitive_key_rotation_company'] = $company;
	begin_transaction();
	$contact_rows = hrm_person_sensitive_reencrypt_table('contact', $new_key_id, $new_key, $company);
	$identifier_rows = $contact_rows === false ? false
		: hrm_person_sensitive_reencrypt_table('identifier', $new_key_id, $new_key, $company);
	$appended = $identifier_rows === false ? false : append_domain_audit_event(array(
		'company_id' => $company,
		'legal_entity_id' => 0,
		'entity_type' => 'hrm_person_sensitive_key',
		'entity_key' => array('key_id' => $new_key_id),
		'action' => 'hrm.person_sensitive.key_rotated',
		'reason_code' => (string)$reason_code,
		'correlation_id' => $pending['correlation_id'],
		'occurred_at' => $pending['occurred_at'],
		'security_context' => array(
			'actor_user_id' => 0,
			'company_id' => $company,
			'role_id' => 0,
			'role_set' => array(),
			'auth_method' => 'hrm_person_sensitive_key_rotation_cli',
			'os_actor' => (string)$actor,
		),
		'restricted_diff' => array(
			'generation' => (int)$pending['generation'],
			'previous_key_id' => (string)$pending['previous_key_id'],
			'new_key_id' => $new_key_id,
			'contact_rows' => (int)$contact_rows,
			'identifier_rows' => (int)$identifier_rows,
			'restricted_values_stored' => false,
		),
	));
	if ($contact_rows === false || $identifier_rows === false || $appended === false) {
		cancel_transaction();
		unset($GLOBALS['hrm_person_sensitive_key_rotation_company']);
		$GLOBALS['hrm_person_sensitive_key_rotation_result'] = 'database_rotation_failed';
		@unlink($paths['pending']);
		@unlink($new_key_path);
		hrm_person_sensitive_key_rotation_release_lock($company);
		return false;
	}
	commit_transaction();
	if (!hrm_person_sensitive_key_rotation_checkpoint('database_committed')) {
		unset($GLOBALS['hrm_person_sensitive_key_rotation_company']);
		$GLOBALS['hrm_person_sensitive_key_rotation_result'] = 'manifest_activation_pending';
		hrm_person_sensitive_key_rotation_release_lock($company);
		return false;
	}
	$next_manifest = hrm_person_sensitive_keyring_after_rotation($manifest, $pending);
	if ($next_manifest === false
		|| !domain_audit_write_private_json($paths['manifest'], $next_manifest, true)) {
		unset($GLOBALS['hrm_person_sensitive_key_rotation_company']);
		$GLOBALS['hrm_person_sensitive_key_rotation_result'] = 'manifest_activation_pending';
		hrm_person_sensitive_key_rotation_release_lock($company);
		return false;
	}
	if (!hrm_person_sensitive_key_rotation_checkpoint('manifest_activated')
		|| !@unlink($paths['pending'])) {
		unset($GLOBALS['hrm_person_sensitive_key_rotation_company']);
		$GLOBALS['hrm_person_sensitive_key_rotation_result'] = 'manifest_activated_pending_cleanup';
		hrm_person_sensitive_key_rotation_release_lock($company);
		return false;
	}
	unset($GLOBALS['hrm_person_sensitive_key_rotation_company']);
	hrm_person_sensitive_key_rotation_release_lock($company);
	$GLOBALS['hrm_person_sensitive_key_rotation_result'] = 'rotated';
	return array(
		'company_id' => $company,
		'generation' => (int)$next_manifest['generation'],
		'previous_key_id' => (string)$pending['previous_key_id'],
		'active_key_id' => (string)$pending['new_key_id'],
		'contact_rows' => (int)$contact_rows,
		'identifier_rows' => (int)$identifier_rows,
		'event_id' => (int)$appended['event_id'],
		'chain_sequence' => (int)$appended['chain_sequence'],
		'correlation_id' => (string)$pending['correlation_id'],
	);
}

/**
 * Recover an interrupted Person-sensitive key rotation without deleting history.
 *
 * @param int $company
 * @return array|false
 */
function recover_hrm_person_sensitive_key_rotation($company)
{
	$company = (int)$company;
	if (hrm_person_sensitive_key_rotation_authorized($company) === false
		|| !hrm_person_sensitive_key_rotation_acquire_lock($company))
		return false;
	$paths = hrm_person_sensitive_keyring_paths($company);
	$pending = hrm_person_sensitive_key_rotation_pending($company);
	if ($paths === false || $pending === false) {
		hrm_person_sensitive_key_rotation_release_lock($company);
		return false;
	}
	$new_key_path = $paths['keys'].DIRECTORY_SEPARATOR.$pending['filename'];
	$committed = hrm_person_sensitive_committed_key_rotation($pending);
	if ($committed === false) {
		hrm_person_sensitive_key_rotation_release_lock($company);
		return false;
	}
	if ($committed === null) {
		foreach (array('hrm_person_contacts', 'hrm_person_identifiers') as $table) {
			$result = db_query("SELECT COUNT(*) row_count FROM ".TB_PREF.$table
				." WHERE key_id=".db_escape($pending['new_key_id']),
				'could not inspect uncommitted HRM Person-sensitive rotation rows');
			$row = $result ? db_fetch_assoc($result) : false;
			if (!$row || (int)$row['row_count'] !== 0) {
				hrm_person_sensitive_key_rotation_release_lock($company);
				return false;
			}
		}
		$removed_key = !file_exists($new_key_path) || @unlink($new_key_path);
		$removed_pending = $removed_key && @unlink($paths['pending']);
		hrm_person_sensitive_key_rotation_release_lock($company);
		return $removed_pending
			? array('ok' => true, 'recovery' => 'aborted_uncommitted') : false;
	}
	$new_key = hrm_person_sensitive_load_key_file($new_key_path, $pending['new_key_id']);
	if ($new_key === false
		|| !hrm_person_sensitive_rotation_rows_match($pending, $committed, $new_key)) {
		hrm_person_sensitive_key_rotation_release_lock($company);
		return false;
	}
	$GLOBALS['hrm_person_sensitive_key_rotation_company'] = $company;
	$manifest = hrm_person_sensitive_keyring($company, false);
	if ($manifest === false) {
		unset($GLOBALS['hrm_person_sensitive_key_rotation_company']);
		hrm_person_sensitive_key_rotation_release_lock($company);
		return false;
	}
	$already_activated = (int)$manifest['generation'] === (int)$pending['generation']
		&& domain_audit_hash_equals((string)$manifest['active_key_id'],
			(string)$pending['new_key_id']);
	$next_manifest = $already_activated ? $manifest
		: hrm_person_sensitive_keyring_after_rotation($manifest, $pending);
	if ($next_manifest === false
		|| (!$already_activated
			&& !domain_audit_write_private_json($paths['manifest'], $next_manifest, true))
		|| !@unlink($paths['pending'])) {
		unset($GLOBALS['hrm_person_sensitive_key_rotation_company']);
		hrm_person_sensitive_key_rotation_release_lock($company);
		return false;
	}
	unset($GLOBALS['hrm_person_sensitive_key_rotation_company']);
	hrm_person_sensitive_key_rotation_release_lock($company);
	return array(
		'ok' => true,
		'recovery' => $already_activated
			? 'finalized_pending_cleanup' : 'finalized_committed',
		'generation' => (int)$next_manifest['generation'],
		'active_key_id' => (string)$next_manifest['active_key_id'],
		'chain_sequence' => (int)$committed['chain_sequence'],
	);
}
