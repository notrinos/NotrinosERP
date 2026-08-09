<?php
/**
 * HRM-FND-001 duplicate-candidate review inventory.
 *
 * Candidate matching is review-only. It never links, merges, verifies, retires,
 * or rewrites a Person, Worker, employee, contact or identifier row.
 */

if (!function_exists('append_domain_audit_event'))
	include_once($path_to_root.'/includes/db/domain_audit_db.inc');
if (!function_exists('hrm_user_can_access_sensitive_field'))
	include_once($path_to_root.'/hrm/includes/hrm_security.inc');
if (!function_exists('hrm_person_sensitive_key'))
	include_once($path_to_root.'/hrm/includes/db/employee_person_sensitive_db.inc');

/** @return bool */
function hrm_person_duplicate_review_table_ready()
{
	$result = db_query("SHOW TABLES LIKE ".db_escape(TB_PREF.'hrm_person_duplicate_reviews'));
	return $result && db_num_rows($result) === 1;
}

/** @return bool */
function hrm_person_duplicate_review_user_can_access()
{
	return function_exists('user_check_access')
		&& hrm_user_can_access_sensitive_field(
			HRM_FIELD_RESTRICTED_IDENTITY, HRM_FIELD_ACTION_VIEW
		);
}

/** @return array */
function hrm_person_duplicate_review_dispositions()
{
	return array('needs_review', 'not_same_person', 'possible_same_person');
}

/** @return array */
function hrm_person_duplicate_review_reason_codes()
{
	return array(
		'potential_duplicate',
		'potential_rehire',
		'different_identity_evidence',
		'insufficient_evidence',
	);
}

/**
 * Mask one person name without returning a full name.
 *
 * @param string $first_name
 * @param string $last_name
 * @return string
 */
function hrm_person_duplicate_review_mask_name($first_name, $last_name)
{
	$first_name = trim((string)$first_name);
	$last_name = trim((string)$last_name);
	if ($first_name === '' || $last_name === '')
		return '';
	return substr($first_name, 0, 1).'*** '.substr($last_name, 0, 1).'***';
}

/**
 * Normalize an inventory page size.
 *
 * @param mixed $limit
 * @return int|false
 */
function hrm_person_duplicate_review_limit($limit)
{
	if (!is_numeric($limit))
		return false;
	$limit = (int)$limit;
	return $limit >= 1 && $limit <= 200 ? $limit : false;
}

/**
 * Return a current key used only to bind candidate fingerprints.
 *
 * @return array|false
 */
function hrm_person_duplicate_review_active_key()
{
	return hrm_person_sensitive_active_key(null, false);
}

/**
 * Validate one sensitive signal and return its exact plaintext/token pair.
 *
 * @param array $row
 * @param string $record_class
 * @param string $type
 * @param string $side left|right
 * @return array|false
 */
function hrm_person_duplicate_review_validate_sensitive_signal(
	$row,
	$record_class,
	$type,
	$side
) {
	$prefix = $side === 'left' ? 'left_' : ($side === 'right' ? 'right_' : '');
	if ($prefix === '' || !is_array($row))
		return false;
	$required = array('person_id', 'ciphertext', 'lookup_token', 'key_id', 'masked_value');
	foreach ($required as $name) {
		if (!array_key_exists($prefix.$name, $row))
			return false;
	}
	$key = hrm_person_sensitive_key((string)$row[$prefix.'key_id']);
	$context = hrm_person_sensitive_context(
		(int)$row[$prefix.'person_id'], $record_class, $type
	);
	$plaintext = $key === false || $context === false ? false
		: hrm_person_sensitive_decrypt($row[$prefix.'ciphertext'], $context, $key);
	$token = $plaintext === false ? false
		: hrm_person_sensitive_lookup_token($type, $plaintext, $key);
	if ($plaintext === false || $token === false
		|| !domain_audit_hash_equals((string)$row[$prefix.'lookup_token'], $token)
		|| (string)$row[$prefix.'masked_value'] !== hrm_person_sensitive_mask($plaintext))
		return false;
	return array('plaintext' => $plaintext, 'token' => $token);
}

/**
 * Add one validated candidate signal to an aggregate pair.
 *
 * @param array $pairs
 * @param int $left_person_id
 * @param int $right_person_id
 * @param int $left_employee_number
 * @param int $right_employee_number
 * @param bool $left_retired
 * @param bool $right_retired
 * @param string $evidence_type
 * @param string $masked_evidence
 * @param string $signal_secret
 * @param string $fingerprint_key
 * @return bool
 */
function hrm_person_duplicate_review_add_signal(
	&$pairs,
	$left_person_id,
	$right_person_id,
	$left_employee_number,
	$right_employee_number,
	$left_retired,
	$right_retired,
	$evidence_type,
	$masked_evidence,
	$signal_secret,
	$fingerprint_key
) {
	if ($left_person_id <= 0 || $right_person_id <= 0
		|| $left_person_id >= $right_person_id
		|| $left_employee_number <= 0 || $right_employee_number <= 0
		|| preg_match('/^[a-z][a-z0-9_]{0,31}$/D', $evidence_type) !== 1
		|| $signal_secret === '' || strlen($fingerprint_key) !== 32)
		return false;
	$key = $left_person_id.':'.$right_person_id;
	if (!isset($pairs[$key])) {
		$pairs[$key] = array(
			'left_person_id' => $left_person_id,
			'right_person_id' => $right_person_id,
			'left_employee_number' => $left_employee_number,
			'right_employee_number' => $right_employee_number,
			'left_retired' => (bool)$left_retired,
			'right_retired' => (bool)$right_retired,
			'evidence' => array(),
			'signals' => array(),
		);
	}
	$signal = hash_hmac('sha256', $evidence_type."\0".$signal_secret,
		$fingerprint_key);
	$pairs[$key]['signals'][$evidence_type.'|'.$signal] = true;
	$pairs[$key]['evidence'][$evidence_type.'|'.$masked_evidence] = array(
		'type' => $evidence_type,
		'masked' => (string)$masked_evidence,
	);
	return true;
}

/**
 * Collect deterministic current duplicate-candidate pairs.
 *
 * @param int|null $only_left_person
 * @param int|null $only_right_person
 * @param int $limit
 * @return array|false
 */
function hrm_person_duplicate_review_collect_pairs(
	$only_left_person = null,
	$only_right_person = null,
	$limit = 100
) {
	$limit = hrm_person_duplicate_review_limit($limit);
	$active = hrm_person_duplicate_review_active_key();
	if ($limit === false || !hrm_person_duplicate_review_table_ready()
		|| !hrm_person_sensitive_tables_ready() || !is_array($active)
		|| !isset($active['key']) || strlen($active['key']) !== 32)
		return false;
	$pair_filter = '';
	if ($only_left_person !== null || $only_right_person !== null) {
		$left = (int)$only_left_person;
		$right = (int)$only_right_person;
		if ($left <= 0 || $right <= 0 || $left >= $right)
			return false;
		$pair_filter = ' AND w1.person_id='.$left.' AND w2.person_id='.$right;
	}
	$pairs = array();
	$scan_limit = $limit * 12;

	$name_sql = "SELECT w1.person_id left_person_id,w2.person_id right_person_id,"
		."m1.employee_number left_employee_number,m2.employee_number right_employee_number,"
		."m1.retired_at left_retired_at,m2.retired_at right_retired_at,"
		."e1.first_name left_first,e1.last_name left_last,e1.birth_date left_birth "
		."FROM ".TB_PREF."employees e1 INNER JOIN ".TB_PREF."employees e2 ON "
		."e1.employee_number<e2.employee_number AND LOWER(TRIM(e1.first_name))=LOWER(TRIM(e2.first_name)) "
		."AND LOWER(TRIM(e1.last_name))=LOWER(TRIM(e2.last_name)) "
		."AND COALESCE(e1.birth_date,'0000-00-00')=COALESCE(e2.birth_date,'0000-00-00') "
		."INNER JOIN ".TB_PREF."hrm_employee_worker_map m1 ON m1.employee_number=e1.employee_number "
		."INNER JOIN ".TB_PREF."hrm_workers w1 ON w1.worker_id=m1.worker_id "
		."INNER JOIN ".TB_PREF."hrm_employee_worker_map m2 ON m2.employee_number=e2.employee_number "
		."INNER JOIN ".TB_PREF."hrm_workers w2 ON w2.worker_id=m2.worker_id "
		."WHERE w1.person_id<w2.person_id".$pair_filter
		." ORDER BY w1.person_id,w2.person_id LIMIT ".(int)$scan_limit;
	$name_result = db_query($name_sql, 'could not read name/birth duplicate candidates');
	if (!$name_result)
		return false;
	while ($row = db_fetch_assoc($name_result)) {
		$name_key = strtolower(trim((string)$row['left_first']))."\0"
			.strtolower(trim((string)$row['left_last']))."\0".(string)$row['left_birth'];
		$masked = hrm_person_duplicate_review_mask_name($row['left_first'], $row['left_last']);
		if (!hrm_person_duplicate_review_add_signal(
			$pairs, (int)$row['left_person_id'], (int)$row['right_person_id'],
			(int)$row['left_employee_number'], (int)$row['right_employee_number'],
			$row['left_retired_at'] !== null, $row['right_retired_at'] !== null,
			'name_birth', $masked, $name_key, $active['key']
		))
			return false;
	}

	foreach (array(
		'contact' => array('table' => 'hrm_person_contacts', 'id' => 'contact_id',
			'type' => 'contact_type'),
		'identifier' => array('table' => 'hrm_person_identifiers', 'id' => 'identifier_id',
			'type' => 'identifier_type'),
	) as $record_class => $meta) {
		$sql = "SELECT a.person_id left_person_id,b.person_id right_person_id,"
			."m1.employee_number left_employee_number,m2.employee_number right_employee_number,"
			."m1.retired_at left_retired_at,m2.retired_at right_retired_at,"
			."a.".$meta['type']." evidence_type,"
			."a.value_ciphertext left_ciphertext,a.lookup_token left_lookup_token,"
			."a.key_id left_key_id,a.masked_value left_masked_value,"
			."b.value_ciphertext right_ciphertext,b.lookup_token right_lookup_token,"
			."b.key_id right_key_id,b.masked_value right_masked_value "
			."FROM ".TB_PREF.$meta['table']." a INNER JOIN ".TB_PREF.$meta['table']." b ON "
			."a.person_id<b.person_id AND a.".$meta['type']."=b.".$meta['type']
			." AND a.lookup_token=b.lookup_token AND a.effective_to IS NULL AND b.effective_to IS NULL "
			."INNER JOIN ".TB_PREF."hrm_workers w1 ON w1.person_id=a.person_id "
			."INNER JOIN ".TB_PREF."hrm_employee_worker_map m1 ON m1.worker_id=w1.worker_id "
			."INNER JOIN ".TB_PREF."hrm_workers w2 ON w2.person_id=b.person_id "
			."INNER JOIN ".TB_PREF."hrm_employee_worker_map m2 ON m2.worker_id=w2.worker_id "
			."WHERE 1=1".$pair_filter
			." ORDER BY a.person_id,b.person_id,a.".$meta['type'].",a.".$meta['id']
			." LIMIT ".(int)$scan_limit;
		$result = db_query($sql, 'could not read sensitive duplicate candidates');
		if (!$result)
			return false;
		while ($row = db_fetch_assoc($result)) {
			$type = (string)$row['evidence_type'];
			$left = hrm_person_duplicate_review_validate_sensitive_signal(
				$row, $record_class, $type, 'left'
			);
			$right = hrm_person_duplicate_review_validate_sensitive_signal(
				$row, $record_class, $type, 'right'
			);
			if ($left === false || $right === false
				|| !domain_audit_hash_equals($left['token'], $right['token']))
				return false;
			$masked = $type.': '.(string)$row['left_masked_value'].' / '
				.(string)$row['right_masked_value'];
			if (!hrm_person_duplicate_review_add_signal(
				$pairs, (int)$row['left_person_id'], (int)$row['right_person_id'],
				(int)$row['left_employee_number'], (int)$row['right_employee_number'],
				$row['left_retired_at'] !== null, $row['right_retired_at'] !== null,
				$record_class.'_'.$type, $masked, $left['token'], $active['key']
			))
				return false;
		}
	}

	ksort($pairs, SORT_STRING);
	$output = array();
	foreach ($pairs as $pair) {
		$signals = array_keys($pair['signals']);
		sort($signals, SORT_STRING);
		$fingerprint = hash_hmac('sha256', 'duplicate-pair-v1'."\0"
			.$pair['left_person_id']."\0".$pair['right_person_id']."\0"
			.implode("\n", $signals), $active['key']);
		$evidence = array_values($pair['evidence']);
		usort($evidence, function($a, $b) {
			return strcmp($a['type'].'|'.$a['masked'], $b['type'].'|'.$b['masked']);
		});
		unset($pair['signals'], $pair['evidence']);
		$pair['candidate_fingerprint'] = $fingerprint;
		$pair['evidence'] = $evidence;
		$output[] = $pair;
		if (count($output) >= $limit)
			break;
	}
	return $output;
}

/**
 * Load latest append-only review evidence for one exact candidate fingerprint.
 *
 * @param int $left_person_id
 * @param int $right_person_id
 * @param string $fingerprint
 * @param bool $for_update
 * @return array|false|null
 */
function get_hrm_person_duplicate_latest_review(
	$left_person_id,
	$right_person_id,
	$fingerprint,
	$for_update = false
) {
	if ($left_person_id <= 0 || $right_person_id <= 0 || $left_person_id >= $right_person_id
		|| preg_match('/^[a-f0-9]{64}$/D', (string)$fingerprint) !== 1
		|| !hrm_person_duplicate_review_table_ready())
		return false;
	$sql = "SELECT review_id,disposition,reason_code,reviewed_at,reviewed_by,predecessor_review_id "
		."FROM ".TB_PREF."hrm_person_duplicate_reviews WHERE person_id_low=".(int)$left_person_id
		." AND person_id_high=".(int)$right_person_id
		." AND candidate_fingerprint=".db_escape($fingerprint)
		." ORDER BY review_id DESC LIMIT 1";
	if ($for_update)
		$sql .= ' FOR UPDATE';
	$result = db_query($sql, 'could not read duplicate-candidate review evidence');
	if (!$result)
		return false;
	$row = db_fetch_assoc($result);
	return $row ? $row : null;
}

/**
 * Return authorized masked duplicate-candidate inventory.
 *
 * @param int $limit
 * @return array|false
 */
function get_hrm_person_duplicate_review_inventory($limit = 100)
{
	if (!hrm_person_duplicate_review_user_can_access()) {
		if (function_exists('hrm_log_sensitive_field_access'))
			hrm_log_sensitive_field_access(
				HRM_FIELD_RESTRICTED_IDENTITY, HRM_FIELD_ACTION_VIEW,
				'denied_duplicate_review_inventory'
			);
		return false;
	}
	$pairs = hrm_person_duplicate_review_collect_pairs(null, null, $limit);
	if ($pairs === false)
		return false;
	foreach ($pairs as &$pair) {
		$review = get_hrm_person_duplicate_latest_review(
			$pair['left_person_id'], $pair['right_person_id'],
			$pair['candidate_fingerprint'], false
		);
		if ($review === false)
			return false;
		$pair['review'] = $review === null ? array(
			'disposition' => 'needs_review', 'reason_code' => null,
			'reviewed_at' => null, 'reviewed_by' => null,
		) : array(
			'disposition' => (string)$review['disposition'],
			'reason_code' => (string)$review['reason_code'],
			'reviewed_at' => (string)$review['reviewed_at'],
			'reviewed_by' => (int)$review['reviewed_by'],
		);
	}
	unset($pair);
	if (function_exists('hrm_log_sensitive_field_access'))
		hrm_log_sensitive_field_access(
			HRM_FIELD_RESTRICTED_IDENTITY, HRM_FIELD_ACTION_VIEW,
			'granted_duplicate_review_inventory'
		);
	return $pairs;
}

/**
 * Lock both Person rows in deterministic order for one review decision.
 *
 * The review table is append-only, so serializing writers on the immutable
 * pair prevents concurrent first decisions or changed decisions from forming
 * parallel predecessor branches. This lock does not mutate either Person.
 *
 * @param int $left_person_id
 * @param int $right_person_id
 * @return bool
 */
function hrm_person_duplicate_review_lock_pair($left_person_id, $right_person_id)
{
	if ($left_person_id <= 0 || $right_person_id <= 0
		|| $left_person_id >= $right_person_id)
		return false;
	$result = db_query("SELECT person_id FROM ".TB_PREF."hrm_persons WHERE person_id IN ("
		.(int)$left_person_id.",".(int)$right_person_id.") ORDER BY person_id FOR UPDATE",
		'could not lock duplicate-candidate Person pair');
	if (!$result)
		return false;
	$seen = array();
	while ($row = db_fetch_assoc($result))
		$seen[(int)$row['person_id']] = true;
	return isset($seen[$left_person_id], $seen[$right_person_id]) && count($seen) === 2;
}

/**
 * Append PII-free domain audit evidence for one review disposition.
 *
 * @param int $review_id
 * @param int $left_person_id
 * @param int $right_person_id
 * @param string $disposition
 * @param string $reason_code
 * @return array|false
 */
function append_hrm_person_duplicate_review_domain_audit(
	$review_id,
	$left_person_id,
	$right_person_id,
	$disposition,
	$reason_code
) {
	if ($review_id <= 0 || $left_person_id <= 0 || $right_person_id <= 0)
		return false;
	return append_domain_audit_event(array(
		'company_id' => function_exists('user_company') ? (int)user_company() : 0,
		'legal_entity_id' => 0,
		'entity_type' => 'hrm_person_duplicate_review',
		'entity_key' => array(
			'review_id' => (int)$review_id,
			'person_id_low' => (int)$left_person_id,
			'person_id_high' => (int)$right_person_id,
		),
		'action' => 'hrm.person_duplicate_review.disposition_recorded',
		'reason_code' => (string)$reason_code,
		'before' => null,
		'after' => array('disposition' => (string)$disposition),
		'restricted_diff' => array('restricted_values_stored' => false),
	));
}

/**
 * Append one bounded disposition for the exact current candidate evidence.
 *
 * This command records review evidence only. It deliberately contains no
 * Person/Worker/map update and cannot link or merge records.
 *
 * @param int $person_a
 * @param int $person_b
 * @param string $candidate_fingerprint
 * @param string $disposition
 * @param string $reason_code
 * @return int|false Review ID.
 */
function record_hrm_person_duplicate_review_disposition(
	$person_a,
	$person_b,
	$candidate_fingerprint,
	$disposition,
	$reason_code
) {
	if (!hrm_person_duplicate_review_user_can_access())
		return false;
	$left = min((int)$person_a, (int)$person_b);
	$right = max((int)$person_a, (int)$person_b);
	if ($left <= 0 || $right <= 0 || $left === $right
		|| preg_match('/^[a-f0-9]{64}$/D', (string)$candidate_fingerprint) !== 1
		|| !in_array($disposition, hrm_person_duplicate_review_dispositions(), true)
		|| !in_array($reason_code, hrm_person_duplicate_review_reason_codes(), true))
		return false;
	begin_transaction();
	if (!hrm_person_duplicate_review_lock_pair($left, $right)) {
		cancel_transaction();
		return false;
	}
	$current = hrm_person_duplicate_review_collect_pairs($left, $right, 1);
	if (!is_array($current) || count($current) !== 1
		|| !domain_audit_hash_equals(
			(string)$current[0]['candidate_fingerprint'], (string)$candidate_fingerprint
		)) {
		cancel_transaction();
		return false;
	}
	$latest = get_hrm_person_duplicate_latest_review(
		$left, $right, $candidate_fingerprint, true
	);
	if ($latest === false) {
		cancel_transaction();
		return false;
	}
	if (is_array($latest) && (string)$latest['disposition'] === (string)$disposition
		&& (string)$latest['reason_code'] === (string)$reason_code) {
		commit_transaction();
		return (int)$latest['review_id'];
	}
	$actor = function_exists('hrm_person_worker_actor_id')
		? hrm_person_worker_actor_id() : 0;
	$predecessor = is_array($latest) ? (int)$latest['review_id'] : 0;
	$decision_key = hash('sha256', implode("\0", array(
		'duplicate-review-decision-v1', $left, $right, $candidate_fingerprint,
		$disposition, $reason_code, $actor, $predecessor,
	)));
	$sql = "INSERT INTO ".TB_PREF."hrm_person_duplicate_reviews "
		."(person_id_low,person_id_high,candidate_fingerprint,disposition,reason_code,"
		."reviewed_at,reviewed_by,predecessor_review_id,decision_key) VALUES ("
		.(int)$left.','.(int)$right.','.db_escape($candidate_fingerprint).','
		.db_escape($disposition).','.db_escape($reason_code).',UTC_TIMESTAMP(),'
		.(int)$actor.','.($predecessor > 0 ? (int)$predecessor : 'NULL').','
		.db_escape($decision_key).')';
	if (!db_query($sql, 'could not append duplicate-candidate disposition')) {
		cancel_transaction();
		return false;
	}
	$review_id = (int)db_insert_id();
	if ($review_id <= 0 || !append_hrm_person_duplicate_review_domain_audit(
		$review_id, $left, $right, $disposition, $reason_code
	)) {
		cancel_transaction();
		return false;
	}
	commit_transaction();
	return $review_id;
}
