<?php
/**
 * HRM-FND-001 maker/checker canonical same-person link execution.
 *
 * This service performs only an explicitly reviewed logical identity link. It
 * keeps both Person rows and every legacy employee-to-Worker map, rebinds the
 * alias Person's existing Worker rows to the selected canonical Person, and
 * retains exact numeric binding snapshots for recovery. It does not merge or
 * delete Person/private/sensitive history and it does not execute rehire state.
 */

if (!function_exists('append_domain_audit_event'))
	include_once($path_to_root.'/includes/db/domain_audit_db.inc');
if (!function_exists('hrm_person_link_proposal_table_ready'))
	include_once($path_to_root.'/hrm/includes/db/employee_person_link_proposal_db.inc');
if (!function_exists('hrm_user_can_access_sensitive_field'))
	include_once($path_to_root.'/hrm/includes/hrm_security.inc');

/** @return bool */
function hrm_person_canonical_link_tables_ready()
{
	foreach (array('hrm_person_canonical_links', 'hrm_person_canonical_link_workers') as $table) {
		$result = db_query("SHOW TABLES LIKE ".db_escape(TB_PREF.$table));
		if (!$result || db_num_rows($result) !== 1)
			return false;
	}
	return true;
}

/** @return bool */
function hrm_person_canonical_link_user_can_execute()
{
	return function_exists('user_check_access')
		&& hrm_user_can_access_sensitive_field(
			HRM_FIELD_RESTRICTED_IDENTITY, HRM_FIELD_ACTION_VERIFY_LINK
		);
}

/**
 * Read one proposal by its numeric ID.
 *
 * @param int $proposal_id
 * @param bool $for_update
 * @return array|false|null
 */
function get_hrm_person_link_proposal_by_id($proposal_id, $for_update = false)
{
	$proposal_id = (int)$proposal_id;
	if ($proposal_id <= 0 || !hrm_person_link_proposal_table_ready())
		return false;
	$sql = "SELECT proposal_id,review_id,person_id_low,person_id_high,proposal_type,"
		."proposed_at,proposed_by,proposal_key FROM ".TB_PREF."hrm_person_link_proposals "
		."WHERE proposal_id=".$proposal_id;
	if ($for_update)
		$sql .= ' FOR UPDATE';
	$result = db_query($sql, 'could not read reviewed Person-link proposal for execution');
	if (!$result)
		return false;
	$row = db_fetch_assoc($result);
	return $row ? $row : null;
}

/**
 * Read an already executed canonical link for one proposal.
 *
 * @param int $proposal_id
 * @param bool $for_update
 * @return array|false|null
 */
function get_hrm_person_canonical_link_for_proposal($proposal_id, $for_update = false)
{
	$proposal_id = (int)$proposal_id;
	if ($proposal_id <= 0 || !hrm_person_canonical_link_tables_ready())
		return false;
	$sql = "SELECT link_id,proposal_id,canonical_person_id,alias_person_id,worker_count,"
		."linked_at,linked_by,link_key FROM ".TB_PREF."hrm_person_canonical_links "
		."WHERE proposal_id=".$proposal_id;
	if ($for_update)
		$sql .= ' FOR UPDATE';
	$result = db_query($sql, 'could not read canonical Person link');
	if (!$result)
		return false;
	$row = db_fetch_assoc($result);
	return $row ? $row : null;
}

/**
 * Verify the exact Worker/mapping snapshot of an existing idempotent result.
 *
 * @param array $link
 * @param bool $for_update
 * @return bool
 */
function hrm_person_canonical_link_snapshot_is_intact($link, $for_update = false)
{
	if (!is_array($link) || empty($link['link_id']) || empty($link['worker_count']))
		return false;
	$sql = "SELECT s.worker_id,s.mapping_id,s.previous_person_id,s.canonical_person_id,"
		."w.person_id current_person_id,m.worker_id mapped_worker_id "
		."FROM ".TB_PREF."hrm_person_canonical_link_workers s "
		."LEFT JOIN ".TB_PREF."hrm_workers w ON w.worker_id=s.worker_id "
		."LEFT JOIN ".TB_PREF."hrm_employee_worker_map m ON m.mapping_id=s.mapping_id "
		."WHERE s.link_id=".(int)$link['link_id']." ORDER BY s.worker_id";
	if ($for_update)
		$sql .= ' FOR UPDATE';
	$result = db_query($sql, 'could not verify canonical Person link Worker snapshot');
	if (!$result)
		return false;
	$count = 0;
	while ($row = db_fetch_assoc($result)) {
		$count++;
		if ((int)$row['previous_person_id'] !== (int)$link['alias_person_id']
			|| (int)$row['canonical_person_id'] !== (int)$link['canonical_person_id']
			|| (int)$row['current_person_id'] !== (int)$link['canonical_person_id']
			|| (int)$row['mapped_worker_id'] !== (int)$row['worker_id'])
			return false;
	}
	return $count === (int)$link['worker_count'];
}

/**
 * Reject aliases already consolidated elsewhere and prevent canonical chains.
 * A canonical Person may retain more than one distinct alias.
 *
 * @param int $canonical_person_id
 * @param int $alias_person_id
 * @return bool
 */
function hrm_person_canonical_link_pair_is_available($canonical_person_id, $alias_person_id)
{
	$canonical_person_id = (int)$canonical_person_id;
	$alias_person_id = (int)$alias_person_id;
	if ($canonical_person_id <= 0 || $alias_person_id <= 0
		|| $canonical_person_id === $alias_person_id)
		return false;
	$sql = "SELECT link_id,canonical_person_id,alias_person_id FROM "
		.TB_PREF."hrm_person_canonical_links WHERE alias_person_id IN ("
		.$canonical_person_id.','.$alias_person_id.") OR canonical_person_id="
		.$alias_person_id." ORDER BY link_id FOR UPDATE";
	$result = db_query($sql, 'could not lock existing canonical Person links');
	if (!$result)
		return false;
	while ($row = db_fetch_assoc($result)) {
		if ((int)$row['alias_person_id'] === $canonical_person_id
			|| (int)$row['alias_person_id'] === $alias_person_id
			|| (int)$row['canonical_person_id'] === $alias_person_id)
			return false;
	}
	return true;
}

/**
 * Lock every alias Worker and its unchanged legacy mapping.
 *
 * @param int $alias_person_id
 * @return array|false
 */
function hrm_person_canonical_link_lock_alias_workers($alias_person_id)
{
	$alias_person_id = (int)$alias_person_id;
	if ($alias_person_id <= 0)
		return false;
	$sql = "SELECT w.worker_id,w.person_id,m.mapping_id,m.employee_number,m.employee_id,m.retired_at "
		."FROM ".TB_PREF."hrm_workers w LEFT JOIN ".TB_PREF."hrm_employee_worker_map m "
		."ON m.worker_id=w.worker_id WHERE w.person_id=".$alias_person_id
		." ORDER BY w.worker_id FOR UPDATE";
	$result = db_query($sql, 'could not lock alias Person Worker bindings');
	if (!$result)
		return false;
	$rows = array();
	while ($row = db_fetch_assoc($result)) {
		if ((int)$row['worker_id'] <= 0 || (int)$row['mapping_id'] <= 0
			|| (int)$row['person_id'] !== $alias_person_id)
			return false;
		$rows[] = $row;
	}
	return count($rows) > 0 ? $rows : false;
}

/**
 * Append PII-free evidence for one canonical link execution.
 *
 * @param int $link_id
 * @param int $proposal_id
 * @param int $review_id
 * @param int $canonical_person_id
 * @param int $alias_person_id
 * @param int $worker_count
 * @return array|false
 */
function append_hrm_person_canonical_link_domain_audit(
	$link_id,
	$proposal_id,
	$review_id,
	$canonical_person_id,
	$alias_person_id,
	$worker_count
) {
	if ($link_id <= 0 || $proposal_id <= 0 || $review_id <= 0
		|| $canonical_person_id <= 0 || $alias_person_id <= 0
		|| $canonical_person_id === $alias_person_id || $worker_count <= 0)
		return false;
	return append_domain_audit_event(array(
		'company_id' => function_exists('user_company') ? (int)user_company() : 0,
		'legal_entity_id' => 0,
		'entity_type' => 'hrm_person_canonical_link',
		'entity_key' => array(
			'link_id' => (int)$link_id,
			'proposal_id' => (int)$proposal_id,
			'review_id' => (int)$review_id,
			'canonical_person_id' => (int)$canonical_person_id,
			'alias_person_id' => (int)$alias_person_id,
		),
		'action' => 'hrm.person_canonical_link.executed',
		'reason_code' => 'reviewed_same_person_maker_checker',
		'before' => null,
		'after' => array(
			'logical_person_link_executed' => true,
			'physical_person_merge_executed' => false,
			'person_rows_deleted' => 0,
			'worker_rebinding_count' => (int)$worker_count,
			'legacy_mapping_rows_changed' => 0,
			'rehire_executed' => false,
		),
		'restricted_diff' => array('restricted_values_stored' => false),
	));
}

/**
 * Execute one reviewed same-person proposal with explicit checker authority.
 *
 * The checker chooses which proposal Person is canonical. The other Person is
 * retained as a reversible alias/evidence row. Exact retries return the same
 * link ID only while the persisted Worker/mapping snapshot remains intact.
 *
 * @param int $proposal_id
 * @param int $canonical_person_id
 * @return int|false Canonical link ID.
 */
function execute_hrm_person_canonical_link($proposal_id, $canonical_person_id)
{
	$proposal_id = (int)$proposal_id;
	$canonical_person_id = (int)$canonical_person_id;
	if (!hrm_person_canonical_link_user_can_execute() || $proposal_id <= 0
		|| $canonical_person_id <= 0 || !hrm_person_canonical_link_tables_ready())
		return false;
	$actor = function_exists('hrm_person_worker_actor_id')
		? (int)hrm_person_worker_actor_id() : 0;
	if ($actor <= 0)
		return false;

	begin_transaction();
	$proposal = get_hrm_person_link_proposal_by_id($proposal_id, true);
	if (!is_array($proposal) || (string)$proposal['proposal_type'] !== 'same_person_candidate'
		|| (int)$proposal['proposed_by'] <= 0 || (int)$proposal['proposed_by'] === $actor) {
		cancel_transaction();
		return false;
	}
	$left = (int)$proposal['person_id_low'];
	$right = (int)$proposal['person_id_high'];
	if ($left <= 0 || $right <= 0 || $left >= $right
		|| ($canonical_person_id !== $left && $canonical_person_id !== $right)) {
		cancel_transaction();
		return false;
	}
	$alias_person_id = $canonical_person_id === $left ? $right : $left;
	if (!hrm_person_duplicate_review_lock_pair($left, $right)) {
		cancel_transaction();
		return false;
	}

	$existing = get_hrm_person_canonical_link_for_proposal($proposal_id, true);
	if ($existing === false) {
		cancel_transaction();
		return false;
	}
	if (is_array($existing)) {
		$ok = (int)$existing['canonical_person_id'] === $canonical_person_id
			&& (int)$existing['alias_person_id'] === $alias_person_id
			&& hrm_person_canonical_link_snapshot_is_intact($existing, true);
		if (!$ok) {
			cancel_transaction();
			return false;
		}
		commit_transaction();
		return (int)$existing['link_id'];
	}

	$review = get_hrm_person_duplicate_review_by_id((int)$proposal['review_id'], true);
	if (!is_array($review) || (int)$review['person_id_low'] !== $left
		|| (int)$review['person_id_high'] !== $right
		|| (string)$review['disposition'] !== 'possible_same_person'
		|| (string)$review['reason_code'] !== 'potential_duplicate') {
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
	if (!is_array($latest) || (int)$latest['review_id'] !== (int)$review['review_id']
		|| (string)$latest['disposition'] !== 'possible_same_person'
		|| (string)$latest['reason_code'] !== 'potential_duplicate') {
		cancel_transaction();
		return false;
	}
	$expected_proposal_key = hash('sha256', implode("\0", array(
		'person-link-proposal-v1', (int)$proposal['review_id'], $left, $right,
		'same_person_candidate',
	)));
	if (!domain_audit_hash_equals((string)$proposal['proposal_key'], $expected_proposal_key)
		|| !hrm_person_canonical_link_pair_is_available($canonical_person_id, $alias_person_id)) {
		cancel_transaction();
		return false;
	}
	$workers = hrm_person_canonical_link_lock_alias_workers($alias_person_id);
	if ($workers === false) {
		cancel_transaction();
		return false;
	}
	if (function_exists('hrm_employment_table_ready') && hrm_employment_table_ready()
		&& (!function_exists('hrm_employment_canonical_link_interval_policy_allows')
			|| !hrm_employment_canonical_link_interval_policy_allows($canonical_person_id, $workers))) {
		cancel_transaction();
		return false;
	}

	$link_key = hash('sha256', implode("\0", array(
		'person-canonical-link-v1', $proposal_id, $canonical_person_id, $alias_person_id,
	)));
	$sql = "INSERT INTO ".TB_PREF."hrm_person_canonical_links "
		."(proposal_id,canonical_person_id,alias_person_id,worker_count,linked_at,linked_by,link_key) VALUES ("
		.$proposal_id.','.$canonical_person_id.','.$alias_person_id.','.(int)count($workers)
		.",UTC_TIMESTAMP(),".$actor.','.db_escape($link_key).')';
	if (!db_query($sql, 'could not append canonical Person link execution')) {
		cancel_transaction();
		return false;
	}
	$link_id = (int)db_insert_id();
	if ($link_id <= 0) {
		cancel_transaction();
		return false;
	}
	foreach ($workers as $worker) {
		$worker_id = (int)$worker['worker_id'];
		$mapping_id = (int)$worker['mapping_id'];
		$sql = "INSERT INTO ".TB_PREF."hrm_person_canonical_link_workers "
			."(link_id,worker_id,mapping_id,previous_person_id,canonical_person_id) VALUES ("
			.$link_id.','.$worker_id.','.$mapping_id.','.$alias_person_id.','.$canonical_person_id.')';
		if (!db_query($sql, 'could not snapshot canonical Person Worker rebinding')) {
			cancel_transaction();
			return false;
		}
		if (!db_query("UPDATE ".TB_PREF."hrm_workers SET person_id=".$canonical_person_id
			." WHERE worker_id=".$worker_id." AND person_id=".$alias_person_id,
			'could not rebind Worker to canonical Person')
			|| !function_exists('db_num_affected_rows') || (int)db_num_affected_rows() !== 1) {
			cancel_transaction();
			return false;
		}
	}
	$link = get_hrm_person_canonical_link_for_proposal($proposal_id, true);
	if (!is_array($link) || !hrm_person_canonical_link_snapshot_is_intact($link, true)
		|| !append_hrm_person_canonical_link_domain_audit(
			$link_id, $proposal_id, (int)$proposal['review_id'], $canonical_person_id,
			$alias_person_id, count($workers)
		)) {
		cancel_transaction();
		return false;
	}
	commit_transaction();
	return $link_id;
}
