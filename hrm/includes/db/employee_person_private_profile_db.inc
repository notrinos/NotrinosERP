<?php
/**
 * HRM-FND-001 effective-dated Person address and emergency-contact shadow.
 *
 * Legacy employee columns remain authoritative during the compatibility phase.
 * This repository stores private history, exposes masked as-of projections, and
 * never creates a second employee selector or direct mutation authority.
 */

if (!function_exists('append_domain_audit_event'))
	include_once($path_to_root.'/includes/db/domain_audit_db.inc');
if (!function_exists('hrm_user_can_access_sensitive_field'))
	include_once($path_to_root.'/hrm/includes/hrm_security.inc');
if (!function_exists('get_hrm_person_worker_binding'))
	include_once($path_to_root.'/hrm/includes/db/employee_person_worker_db.inc');
if (!function_exists('hrm_person_sensitive_mask'))
	include_once($path_to_root.'/hrm/includes/db/employee_person_sensitive_db.inc');

/** @return bool */
function hrm_person_private_profile_tables_ready()
{
	return hrm_person_worker_table_exists('hrm_person_addresses')
		&& hrm_person_worker_table_exists('hrm_person_emergency_contacts');
}

/**
 * Validate bounded private text without altering meaningful content.
 *
 * @param mixed $value
 * @param int $maximum_length
 * @param bool $allow_multiline
 * @return string|false
 */
function hrm_person_private_profile_text($value, $maximum_length, $allow_multiline = false)
{
	$value = trim((string)$value);
	if ($value === '')
		return '';
	if (strlen($value) > (int)$maximum_length || strpos($value, "\0") !== false)
		return false;
	$pattern = $allow_multiline ? '/[\x01-\x08\x0B\x0C\x0E-\x1F\x7F]/' : '/[\x00-\x1F\x7F]/';
	return preg_match($pattern, $value) ? false : $value;
}

/** @param string $value @return string */
function hrm_person_private_profile_mask_text($value)
{
	return trim((string)$value) === '' ? '' : '[masked]';
}

/** @param string $value @return string */
function hrm_person_private_profile_mask_phone($value)
{
	return trim((string)$value) === '' ? '' : hrm_person_sensitive_mask($value);
}

/**
 * Build the exact legacy address snapshot used by the compatibility shadow.
 *
 * @param array $employee
 * @return array|false
 */
function hrm_person_private_profile_address_snapshot($employee)
{
	if (!is_array($employee))
		return false;
	$fields = array(
		'address_line' => array('address', 255, true),
		'city' => array('city', 60, false),
		'region' => array('state', 60, false),
		'country' => array('country', 60, false),
	);
	$row = array();
	foreach ($fields as $target => $config) {
		$value = hrm_person_private_profile_text(
			isset($employee[$config[0]]) ? $employee[$config[0]] : '',
			$config[1], $config[2]
		);
		if ($value === false)
			return false;
		$row[$target] = $value;
	}
	$row['postal_code'] = '';
	return $row;
}

/**
 * Build the exact legacy emergency-contact snapshot.
 *
 * @param array $employee
 * @return array|false
 */
function hrm_person_private_profile_emergency_snapshot($employee)
{
	if (!is_array($employee))
		return false;
	$name = hrm_person_private_profile_text(
		isset($employee['emergency_name']) ? $employee['emergency_name'] : '', 100, false
	);
	$relationship = hrm_person_private_profile_text(
		isset($employee['emergency_relation']) ? $employee['emergency_relation'] : '', 60, false
	);
	$phone = hrm_person_private_profile_text(
		isset($employee['emergency_phone']) ? $employee['emergency_phone'] : '', 30, false
	);
	if ($name === false || $relationship === false || $phone === false)
		return false;
	return array('contact_name' => $name, 'relationship' => $relationship, 'phone' => $phone);
}

/** @param array $snapshot @return bool */
function hrm_person_private_profile_has_value($snapshot)
{
	if (!is_array($snapshot))
		return false;
	foreach ($snapshot as $value) {
		if ((string)$value !== '')
			return true;
	}
	return false;
}

/**
 * Compare an address row with the authoritative legacy snapshot.
 *
 * @param array $current
 * @param array $snapshot
 * @return bool
 */
function hrm_person_private_profile_address_matches($current, $snapshot)
{
	foreach (array('address_line', 'city', 'region', 'country', 'postal_code') as $field) {
		if (!isset($current[$field]) || (string)$current[$field] !== (string)$snapshot[$field])
			return false;
	}
	return true;
}

/**
 * Compare an emergency-contact row with the authoritative legacy snapshot.
 *
 * @param array $current
 * @param array $snapshot
 * @return bool
 */
function hrm_person_private_profile_emergency_matches($current, $snapshot)
{
	foreach (array('contact_name', 'relationship', 'phone') as $field) {
		if (!isset($current[$field]) || (string)$current[$field] !== (string)$snapshot[$field])
			return false;
	}
	return true;
}

/**
 * Synchronize one effective-dated residential address version.
 *
 * @param int $person_id
 * @param array $snapshot
 * @param string $source
 * @return int|false One changed, zero unchanged, false on failure.
 */
function synchronize_hrm_person_address_shadow($person_id, $snapshot, $source)
{
	if ((int)$person_id <= 0 || !is_array($snapshot))
		return false;
	$result = db_query("SELECT address_id,address_line,city,region,country,postal_code "
		."FROM ".TB_PREF."hrm_person_addresses WHERE person_id=".(int)$person_id
		." AND address_type='residential' AND effective_to IS NULL "
		."ORDER BY address_id DESC LIMIT 2 FOR UPDATE",
		'could not read current HRM person address');
	if (!$result || db_num_rows($result) > 1)
		return false;
	$current = db_num_rows($result) === 1 ? db_fetch_assoc($result) : false;
	$has_value = hrm_person_private_profile_has_value($snapshot);
	if (!$has_value) {
		if (!$current)
			return 0;
		$now = hrm_person_worker_utc_now();
		return $now !== false && db_query("UPDATE ".TB_PREF."hrm_person_addresses SET effective_to="
			.db_escape($now)." WHERE address_id=".(int)$current['address_id']
			." AND effective_to IS NULL", 'could not close HRM person address') ? 1 : false;
	}
	if ($current && hrm_person_private_profile_address_matches($current, $snapshot))
		return 0;
	if ($current && $source === 'legacy_employee_seed')
		return false;
	$now = $source === 'legacy_employee_seed' ? null : hrm_person_worker_utc_now();
	if ($source !== 'legacy_employee_seed' && $now === false)
		return false;
	if ($current && !db_query("UPDATE ".TB_PREF."hrm_person_addresses SET effective_to="
		.db_escape($now)." WHERE address_id=".(int)$current['address_id']
		." AND effective_to IS NULL", 'could not close HRM person address version'))
		return false;
	$sql = "INSERT INTO ".TB_PREF."hrm_person_addresses "
		."(person_id,address_type,address_line,city,region,country,postal_code,is_residence,is_tax_residence,"
		."effective_from,effective_to,source,verification_status,created_by) VALUES ("
		.(int)$person_id.",'residential',".db_escape($snapshot['address_line']).",".db_escape($snapshot['city'])
		.",".db_escape($snapshot['region']).",".db_escape($snapshot['country']).",".db_escape($snapshot['postal_code'])
		.",1,0,".($now === null ? 'NULL' : db_escape($now)).",NULL,".db_escape($source)
		.",'unverified',".hrm_person_worker_actor_id().")";
	return db_query($sql, 'could not create HRM person address version') ? 1 : false;
}

/**
 * Synchronize one effective-dated primary emergency-contact version.
 *
 * @param int $person_id
 * @param array $snapshot
 * @param string $source
 * @return int|false
 */
function synchronize_hrm_person_emergency_contact_shadow($person_id, $snapshot, $source)
{
	if ((int)$person_id <= 0 || !is_array($snapshot))
		return false;
	$result = db_query("SELECT emergency_contact_id,contact_name,relationship,phone "
		."FROM ".TB_PREF."hrm_person_emergency_contacts WHERE person_id=".(int)$person_id
		." AND is_primary=1 AND effective_to IS NULL ORDER BY emergency_contact_id DESC LIMIT 2 FOR UPDATE",
		'could not read current HRM person emergency contact');
	if (!$result || db_num_rows($result) > 1)
		return false;
	$current = db_num_rows($result) === 1 ? db_fetch_assoc($result) : false;
	$has_value = hrm_person_private_profile_has_value($snapshot);
	if (!$has_value) {
		if (!$current)
			return 0;
		$now = hrm_person_worker_utc_now();
		return $now !== false && db_query("UPDATE ".TB_PREF."hrm_person_emergency_contacts SET effective_to="
			.db_escape($now)." WHERE emergency_contact_id=".(int)$current['emergency_contact_id']
			." AND effective_to IS NULL", 'could not close HRM emergency contact') ? 1 : false;
	}
	if ($current && hrm_person_private_profile_emergency_matches($current, $snapshot))
		return 0;
	if ($current && $source === 'legacy_employee_seed')
		return false;
	if ($source !== 'legacy_employee_seed' && $snapshot['phone'] !== ''
		&& !hrm_person_sensitive_semantic_value_is_valid('contact', 'phone', $snapshot['phone']))
		return false;
	$now = $source === 'legacy_employee_seed' ? null : hrm_person_worker_utc_now();
	if ($source !== 'legacy_employee_seed' && $now === false)
		return false;
	if ($current && !db_query("UPDATE ".TB_PREF."hrm_person_emergency_contacts SET effective_to="
		.db_escape($now)." WHERE emergency_contact_id=".(int)$current['emergency_contact_id']
		." AND effective_to IS NULL", 'could not close HRM emergency-contact version'))
		return false;
	$sql = "INSERT INTO ".TB_PREF."hrm_person_emergency_contacts "
		."(person_id,contact_name,relationship,phone,is_primary,consent_status,effective_from,effective_to,"
		."source,verification_status,created_by) VALUES (".(int)$person_id.","
		.db_escape($snapshot['contact_name']).",".db_escape($snapshot['relationship']).",".db_escape($snapshot['phone'])
		.",1,'unknown',".($now === null ? 'NULL' : db_escape($now)).",NULL,".db_escape($source)
		.",'unverified',".hrm_person_worker_actor_id().")";
	return db_query($sql, 'could not create HRM person emergency-contact version') ? 1 : false;
}

/**
 * Append PII-free address/emergency shadow synchronization evidence.
 *
 * @param array $binding
 * @param array $changed_profiles
 * @param string $source
 * @return array|false
 */
function append_hrm_person_private_profile_domain_audit($binding, $changed_profiles, $source)
{
	if (!is_array($binding) || empty($binding['person_id']) || empty($binding['worker_id'])
		|| empty($binding['employee_number']))
		return false;
	$changed_profiles = array_values(array_unique($changed_profiles));
	sort($changed_profiles);
	return append_domain_audit_event(array(
		'company_id' => function_exists('user_company') ? (int)user_company() : 0,
		'legal_entity_id' => 0,
		'entity_type' => 'hrm_person_private_profile',
		'entity_key' => array('person_id' => (int)$binding['person_id'],
			'worker_id' => (int)$binding['worker_id'],
			'employee_number' => (int)$binding['employee_number']),
		'action' => 'hrm.person_private_profile.synchronized',
		'reason_code' => 'hrm_fnd_001_person_private_profile_sync',
		'before' => null,
		'after' => array('shadow_versions_changed' => true),
		'restricted_diff' => array(
			'profile_types' => $changed_profiles,
			'source' => preg_replace('/[^a-z0-9_]/', '', strtolower((string)$source)),
			'restricted_values_stored' => false,
		),
	));
}

/**
 * Synchronize address and emergency contact from one authoritative employee row.
 *
 * @param array $employee
 * @param bool $audit
 * @param string $source
 * @return bool
 */
function synchronize_hrm_person_private_profile_from_employee(
	$employee,
	$audit = true,
	$source = 'runtime_employee'
) {
	if (!is_array($employee) || empty($employee['employee_number'])
		|| !hrm_person_private_profile_tables_ready())
		return false;
	$binding = get_hrm_person_worker_binding((int)$employee['employee_number'], true);
	if (!$binding || $binding['retired_at'] !== null)
		return false;
	$address = hrm_person_private_profile_address_snapshot($employee);
	$emergency = hrm_person_private_profile_emergency_snapshot($employee);
	if ($address === false || $emergency === false)
		return false;
	$changed = array();
	$address_changed = synchronize_hrm_person_address_shadow(
		(int)$binding['person_id'], $address, $source
	);
	if ($address_changed === false)
		return false;
	if ($address_changed === 1)
		$changed[] = 'residential_address';
	$emergency_changed = synchronize_hrm_person_emergency_contact_shadow(
		(int)$binding['person_id'], $emergency, $source
	);
	if ($emergency_changed === false)
		return false;
	if ($emergency_changed === 1)
		$changed[] = 'emergency_contact';
	return empty($changed) || !$audit || append_hrm_person_private_profile_domain_audit(
		$binding, $changed, $source
	) !== false;
}

/**
 * Return masked as-of address rows. Raw private values never leave this helper.
 *
 * @param int|string $employee_ref
 * @param string $as_of
 * @return array|false
 */
function get_hrm_person_addresses_masked_as_of($employee_ref, $as_of)
{
	if (!hrm_user_can_access_sensitive_field(HRM_FIELD_PRIVATE_PERSON, HRM_FIELD_ACTION_VIEW)
		|| !hrm_person_private_profile_tables_ready())
		return false;
	$as_of = hrm_person_worker_normalize_as_of($as_of);
	$column = is_numeric($employee_ref) ? 'employee_number' : 'employee_id';
	$value = is_numeric($employee_ref) ? (int)$employee_ref : trim((string)$employee_ref);
	if ($as_of === false || ($column === 'employee_number' && $value <= 0)
		|| ($column === 'employee_id' && $value === ''))
		return false;
	$time = db_escape($as_of);
	$sql = "SELECT a.address_type,a.address_line,a.city,a.region,a.country,a.postal_code,"
		."a.is_residence,a.is_tax_residence,a.verification_status,a.effective_from,a.effective_to "
		."FROM ".TB_PREF."hrm_employee_worker_map m INNER JOIN ".TB_PREF."hrm_workers w ON w.worker_id=m.worker_id "
		."INNER JOIN ".TB_PREF."hrm_person_addresses a ON a.person_id=w.person_id WHERE m.".$column."=".db_escape($value)
		." AND (m.retired_at IS NULL OR m.retired_at>".$time.")"
		." AND (a.effective_from IS NULL OR a.effective_from<=".$time.")"
		." AND (a.effective_to IS NULL OR a.effective_to>".$time.")"
		." ORDER BY a.address_type,a.effective_from DESC,a.address_id DESC";
	$result = db_query($sql, 'could not read masked HRM person addresses');
	if (!$result)
		return false;
	$rows = array();
	while ($row = db_fetch_assoc($result)) {
		$rows[] = array(
			'address_type' => $row['address_type'],
			'address_line_masked' => hrm_person_private_profile_mask_text($row['address_line']),
			'city_masked' => hrm_person_private_profile_mask_text($row['city']),
			'region_masked' => hrm_person_private_profile_mask_text($row['region']),
			'country_masked' => hrm_person_private_profile_mask_text($row['country']),
			'postal_code_masked' => hrm_person_private_profile_mask_text($row['postal_code']),
			'is_residence' => (int)$row['is_residence'],
			'is_tax_residence' => (int)$row['is_tax_residence'],
			'verification_status' => $row['verification_status'],
			'effective_from' => $row['effective_from'],
			'effective_to' => $row['effective_to'],
		);
	}
	hrm_log_sensitive_field_access(HRM_FIELD_PRIVATE_PERSON, HRM_FIELD_ACTION_VIEW, 'granted_masked_shadow');
	return $rows;
}

/**
 * Return masked as-of emergency-contact rows. Raw private values never leave this helper.
 *
 * @param int|string $employee_ref
 * @param string $as_of
 * @return array|false
 */
function get_hrm_person_emergency_contacts_masked_as_of($employee_ref, $as_of)
{
	if (!hrm_user_can_access_sensitive_field(HRM_FIELD_PRIVATE_PERSON, HRM_FIELD_ACTION_VIEW)
		|| !hrm_person_private_profile_tables_ready())
		return false;
	$as_of = hrm_person_worker_normalize_as_of($as_of);
	$column = is_numeric($employee_ref) ? 'employee_number' : 'employee_id';
	$value = is_numeric($employee_ref) ? (int)$employee_ref : trim((string)$employee_ref);
	if ($as_of === false || ($column === 'employee_number' && $value <= 0)
		|| ($column === 'employee_id' && $value === ''))
		return false;
	$time = db_escape($as_of);
	$sql = "SELECT c.contact_name,c.relationship,c.phone,c.is_primary,c.consent_status,"
		."c.verification_status,c.effective_from,c.effective_to FROM ".TB_PREF."hrm_employee_worker_map m "
		."INNER JOIN ".TB_PREF."hrm_workers w ON w.worker_id=m.worker_id "
		."INNER JOIN ".TB_PREF."hrm_person_emergency_contacts c ON c.person_id=w.person_id WHERE m.".$column."=".db_escape($value)
		." AND (m.retired_at IS NULL OR m.retired_at>".$time.")"
		." AND (c.effective_from IS NULL OR c.effective_from<=".$time.")"
		." AND (c.effective_to IS NULL OR c.effective_to>".$time.")"
		." ORDER BY c.is_primary DESC,c.effective_from DESC,c.emergency_contact_id DESC";
	$result = db_query($sql, 'could not read masked HRM emergency contacts');
	if (!$result)
		return false;
	$rows = array();
	while ($row = db_fetch_assoc($result)) {
		$rows[] = array(
			'contact_name_masked' => hrm_person_private_profile_mask_text($row['contact_name']),
			'relationship_masked' => hrm_person_private_profile_mask_text($row['relationship']),
			'phone_masked' => hrm_person_private_profile_mask_phone($row['phone']),
			'is_primary' => (int)$row['is_primary'],
			'consent_status' => $row['consent_status'],
			'verification_status' => $row['verification_status'],
			'effective_from' => $row['effective_from'],
			'effective_to' => $row['effective_to'],
		);
	}
	hrm_log_sensitive_field_access(HRM_FIELD_PRIVATE_PERSON, HRM_FIELD_ACTION_VIEW, 'granted_masked_shadow');
	return $rows;
}
