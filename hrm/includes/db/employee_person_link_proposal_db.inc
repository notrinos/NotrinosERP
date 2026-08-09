<?php
/**
 * HRM-FND-001 reviewed Person-link/rehire proposal evidence.
 *
 * This service records proposal evidence only. It never links or merges Person
 * records, rewrites Worker/legacy employee mappings, or creates Employment or
 * rehire state. Legacy employees and selectors remain authoritative.
 */

if (!function_exists('append_domain_audit_event'))
	include_once($path_to_root.'/includes/db/domain_audit_db.inc');
if (!function_exists('hrm_person_duplicate_review_user_can_access'))
	include_once($path_to_root.'/hrm/includes/db/employee_person_duplicate_review_db.inc');

/** @return bool */
function hrm_person_link_proposal_table_ready()
{
	$result = db_query("SHOW TABLES LIKE ".db_escape(TB_PREF.'hrm_person_link_proposals'));
	return $result && db_num_rows($result) === 1;
}

/** @return array */
function hrm_person_link_proposal_types()
{
	return array('same_person_candidate', 'rehire_candidate');
}

/** @return bool */
function hrm_person_link_proposal_user_can_access()
{
	return hrm_person_duplicate_review_user_can_access();
}

/**
 * Return the required duplicate-review reason for one proposal type.
 *
 * @param string $proposal_type
 * @return string|false
 */
function hrm_person_link_proposal_required_reason($proposal_type)
{
	if ($proposal_type === 'same_person_candidate')
		return 'potential_duplicate';
	if ($proposal_type === 'rehire_candidate')
		return 'potential_rehire';
	return false;
}

/**
 * Read one duplicate review row by ID.
 *
 * @param int $review_id
 * @param bool $for_update
 * @return array|false|null
 */
function get_hrm_person_duplicate_review_by_id($review_id, $for_update = false)
{
	$review_id = (int)$review_id;
	if ($review_id <= 0 || !hrm_person_duplicate_review_table_ready())
		return false;
	$sql = "SELECT review_id,person_id_low,person_id_high,candidate_fingerprint,"
		."disposition,reason_code,reviewed_at,reviewed_by,predecessor_review_id "
		."FROM ".TB_PREF."hrm_person_duplicate_reviews WHERE review_id=".$review_id;
	if ($for_update)
		$sql .= ' FOR UPDATE';
	$result = db_query($sql, 'could not read duplicate review for link proposal');
	if (!$result)
		return false;
	$row = db_fetch_assoc($result);
	return $row ? $row : null;
}

/**
 * Read an existing exact proposal for idempotent retry.
 *
 * @param int $review_id
 * @param string $proposal_type
 * @param bool $for_update
 * @return array|false|null
 */
function get_hrm_person_link_proposal_for_review(
	$review_id,
	$proposal_type,
	$for_update = false
) {
	$review_id = (int)$review_id;
	if ($review_id <= 0 || !in_array($proposal_type, hrm_person_link_proposal_types(), true)
		|| !hrm_person_link_proposal_table_ready())
		return false;
	$sql = "SELECT proposal_id,review_id,person_id_low,person_id_high,proposal_type,"
		."proposed_at,proposed_by FROM ".TB_PREF."hrm_person_link_proposals WHERE review_id="
		.$review_id." AND proposal_type=".db_escape($proposal_type)." ORDER BY proposal_id DESC LIMIT 1";
	if ($for_update)
		$sql .= ' FOR UPDATE';
	$result = db_query($sql, 'could not read reviewed Person-link proposal');
	if (!$result)
		return false;
	$row = db_fetch_assoc($result);
	return $row ? $row : null;
}

/**
 * Append PII-free PAY-AUD evidence for one proposal.
 *
 * @param int $proposal_id
 * @param int $review_id
 * @param int $left_person_id
 * @param int $right_person_id
 * @param string $proposal_type
 * @return array|false
 */
function append_hrm_person_link_proposal_domain_audit(
	$proposal_id,
	$review_id,
	$left_person_id,
	$right_person_id,
	$proposal_type
) {
	if ($proposal_id <= 0 || $review_id <= 0 || $left_person_id <= 0
		|| $right_person_id <= 0
		|| !in_array($proposal_type, hrm_person_link_proposal_types(), true))
		return false;
	return append_domain_audit_event(array(
		'company_id' => function_exists('user_company') ? (int)user_company() : 0,
		'legal_entity_id' => 0,
		'entity_type' => 'hrm_person_link_proposal',
		'entity_key' => array(
			'proposal_id' => (int)$proposal_id,
			'review_id' => (int)$review_id,
			'person_id_low' => (int)$left_person_id,
			'person_id_high' => (int)$right_person_id,
		),
		'action' => 'hrm.person_link_proposal.recorded',
		'reason_code' => $proposal_type === 'rehire_candidate'
			? 'reviewed_potential_rehire' : 'reviewed_possible_same_person',
		'before' => null,
		'after' => array(
			'proposal_type' => (string)$proposal_type,
			'person_link_executed' => false,
			'person_merge_executed' => false,
			'worker_rebinding_executed' => false,
			'rehire_executed' => false,
		),
		'restricted_diff' => array('restricted_values_stored' => false),
	));
}

/**
 * Record a reviewed same-person or rehire proposal for exact current evidence.
 *
 * The latest duplicate review must remain the exact supplied review, must have
 * disposition possible_same_person, and must have the reason appropriate for
 * the requested proposal type. The command records evidence only.
 *
 * @param int $review_id
 * @param string $proposal_type
 * @return int|false Proposal ID.
 */
function propose_hrm_person_link_or_rehire($review_id, $proposal_type)
{
	$review_id = (int)$review_id;
	if (!hrm_person_link_proposal_user_can_access() || $review_id <= 0
		|| !in_array($proposal_type, hrm_person_link_proposal_types(), true)
		|| !hrm_person_link_proposal_table_ready())
		return false;
	$required_reason = hrm_person_link_proposal_required_reason($proposal_type);
	if ($required_reason === false)
		return false;

	begin_transaction();
	$review = get_hrm_person_duplicate_review_by_id($review_id, true);
	if (!is_array($review)
		|| (string)$review['disposition'] !== 'possible_same_person'
		|| (string)$review['reason_code'] !== $required_reason) {
		cancel_transaction();
		return false;
	}
	$left = (int)$review['person_id_low'];
	$right = (int)$review['person_id_high'];
	if (!hrm_person_duplicate_review_lock_pair($left, $right)) {
		cancel_transaction();
		return false;
	}
	$current = hrm_person_duplicate_review_collect_pairs($left, $right, 1);
	if (!is_array($current) || count($current) !== 1
		|| !domain_audit_hash_equals(
			(string)$current[0]['candidate_fingerprint'],
			(string)$review['candidate_fingerprint']
		)) {
		cancel_transaction();
		return false;
	}
	$latest = get_hrm_person_duplicate_latest_review(
		$left, $right, (string)$review['candidate_fingerprint'], true
	);
	if (!is_array($latest) || (int)$latest['review_id'] !== $review_id
		|| (string)$latest['disposition'] !== 'possible_same_person'
		|| (string)$latest['reason_code'] !== $required_reason) {
		cancel_transaction();
		return false;
	}
	$existing = get_hrm_person_link_proposal_for_review(
		$review_id, $proposal_type, true
	);
	if ($existing === false) {
		cancel_transaction();
		return false;
	}
	if (is_array($existing)) {
		commit_transaction();
		return (int)$existing['proposal_id'];
	}

	$actor = function_exists('hrm_person_worker_actor_id')
		? hrm_person_worker_actor_id() : 0;
	$proposal_key = hash('sha256', implode("\0", array(
		'person-link-proposal-v1', $review_id, $left, $right, $proposal_type,
	)));
	$sql = "INSERT INTO ".TB_PREF."hrm_person_link_proposals "
		."(review_id,person_id_low,person_id_high,proposal_type,proposed_at,proposed_by,proposal_key) VALUES ("
		.$review_id.",".$left.",".$right.",".db_escape($proposal_type)
		.",UTC_TIMESTAMP(),".(int)$actor.",".db_escape($proposal_key).")";
	if (!db_query($sql, 'could not append reviewed Person-link proposal')) {
		cancel_transaction();
		return false;
	}
	$proposal_id = (int)db_insert_id();
	if ($proposal_id <= 0 || !append_hrm_person_link_proposal_domain_audit(
		$proposal_id, $review_id, $left, $right, $proposal_type
	)) {
		cancel_transaction();
		return false;
	}
	commit_transaction();
	return $proposal_id;
}
