<?php
/**
 * DIM-ADV-002: optional single-parent hierarchy inside one Dimension axis, with effective versions and portfolio rollups.
 *
 * Nothing here is inferred. The HRM organization hierarchy and any naming or numbering pattern of Dimensions are never read;
 * a hierarchy exists only because an authorized maker proposed it and an independent checker approved it:
 *
 *  - a VERSION is the complete set of member links of ONE axis (every member names exactly one parent of the same axis, or no
 *    parent for a root). Dimensions of the other axis, unknown Dimensions, self parents, parents outside the version and cycles
 *    are refused; the parent posting policy says whether a parent may carry direct postings (`allowed`) or only leaves may
 *    (`leaf_only`; a direct parent posting is then reported as a policy exception - this authority never blocks a posting);
 *  - a version takes effect on an explicit date that must follow its basis. Moving a node is therefore a new version: the
 *    AS-POSTED view reads each posting through the version that was effective on its posting date, so history is never silently
 *    restated, while the CURRENT view applies the latest approved structure to every posting and says so. A version dated before
 *    the day it is proposed must be marked retroactive on purpose and is flagged in the as-posted view;
 *  - the ROLLUP of a node is its own (direct) postings plus those of every descendant; a posting is added to its own node and
 *    once to each ancestor, and the portfolio total counts each posting once, so root totals never double count. Postings with no
 *    Dimension, with an unknown Dimension, or on a Dimension the version does not contain stay visible in their own buckets;
 *  - versions are immutable, carry a content digest chained to their basis, and a version that fails its digest is never used;
 *  - no GL, budget, document, Dimension master or HRM row is written, and no legal-entity balancing is created.
 *
 * PHP 5.6 compatible syntax. Dates are SQL `Y-m-d`.
 */
require_once(__DIR__.'/../../includes/value_helpers.inc');

/** Largest number of members one hierarchy version may hold. */
function dimension_hier_max_members()
{
	return 5000;
}

/** Parent posting policies. */
function dimension_hier_policies()
{
	return array('allowed', 'leaf_only');
}

/** Does the session grant EVERY named area (denies when the helper is unavailable)? */
function dimension_hier_has_access($areas)
{
	if (!function_exists('user_check_access'))
		return false;
	foreach ((array)$areas as $area)
		if (!user_check_access($area))
			return false;
	return true;
}

/** Quote free text for the evidence tables. */
function dimension_hier_quote($value)
{
	return "'".db_escape_function((string)$value)."'";
}

/** A fresh result skeleton (it starts ok; every refusal goes through dimension_hier_fail). */
function dimension_hier_result($extra = array())
{
	return array_merge(array('ok' => true, 'errors' => array(), 'codes' => array()), $extra);
}

/** Record one stable reason code and its readable text on a result. */
function dimension_hier_fail(&$result, $code, $text)
{
	$result['ok'] = false;
	if (!in_array($code, $result['codes'], true))
		$result['codes'][] = $code;
	$result['errors'][] = $text;
	return $result;
}

/** Today as a SQL date (the application's own clock). */
function dimension_hier_today()
{
	if (function_exists('Today') && function_exists('date2sql')) {
		$today = date2sql(Today());
		if (is_iso_calendar_date($today))
			return $today;
	}
	return date('Y-m-d');
}

/** Are the two hierarchy tables installed in the active company? */
function dimension_hier_schema_ready($refresh = false)
{
	static $cache = array();
	$key = TB_PREF;
	if (!$refresh && isset($cache[$key]))
		return $cache[$key];
	$rows = db_query("SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME IN ('".TB_PREF."dimension_hierarchy_versions','"
		.TB_PREF."dimension_hierarchy_members')");
	$row = $rows ? db_fetch_row($rows) : false;
	if (!$row)
		return false;
	$cache[$key] = (int)$row[0] === 2 ? true : 'absent';
	return $cache[$key];
}

/** Forget the per-request schema cache (tests and long-running scripts). */
function dimension_hier_cache_reset()
{
	dimension_hier_schema_ready(true);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Pure structure rules
// ---------------------------------------------------------------------------------------------------------------------------------

/**
 * Validate a member => parent map (pure): every parent is a member, nobody is its own parent and there is no cycle.
 *
 * @param array $members member id => parent id (0 = root)
 * @return array ok, code (empty when ok), text, member (the offending member)
 */
function dimension_hier_validate_structure($members)
{
	foreach ($members as $member => $parent) {
		if ($parent === 0)
			continue;
		if ($parent === $member)
			return array('ok' => false, 'code' => 'self_parent', 'text' => sprintf(_('Tracking value %d cannot be its own parent.'), $member), 'member' => $member);
		if (!isset($members[$parent]))
			return array('ok' => false, 'code' => 'parent_missing', 'text' => sprintf(_('The parent %d of tracking value %d is not part of the hierarchy.'), $parent, $member), 'member' => $member);
	}
	foreach ($members as $member => $parent) {
		$seen = array($member => true);
		$cursor = $parent;
		while ($cursor !== 0) {
			if (isset($seen[$cursor]))
				return array('ok' => false, 'code' => 'cycle', 'text' => sprintf(_('Tracking value %d would become its own ancestor.'), $member), 'member' => $member);
			$seen[$cursor] = true;
			$cursor = $members[$cursor];
		}
	}
	return array('ok' => true, 'code' => '', 'text' => '', 'member' => 0);
}

/**
 * The ancestor chain of a member, nearest parent first (pure).
 *
 * @param array $members member => parent
 * @param int $member Member
 * @return array Ancestors (empty for a root or an unknown member)
 */
function dimension_hier_ancestors($members, $member)
{
	$chain = array();
	$cursor = isset($members[$member]) ? $members[$member] : 0;
	while ($cursor !== 0 && isset($members[$cursor]) && !in_array($cursor, $chain, true)) {
		$chain[] = $cursor;
		$cursor = $members[$cursor];
	}
	return $chain;
}

/** Digest of a hierarchy version's content, chained to its basis. */
function dimension_hier_content_sha256($version)
{
	$members = array();
	foreach ($version['members'] as $member => $parent)
		$members[] = array((int)$member, (int)$parent);
	usort($members, function ($left, $right) {
		return $left[0] - $right[0];
	});
	return hash('sha256', json_encode(array('adv2-hierarchy', (int)$version['axis'], $version['basis_sha256'] === null ? null : (string)$version['basis_sha256'],
		(string)$version['effective_from'], (int)$version['retroactive'], (string)$version['parent_policy'], $members)));
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Versions
// ---------------------------------------------------------------------------------------------------------------------------------

/** Load one hierarchy version with its `members` map (member => parent), optionally locked. */
function dimension_hier_load($version_id, $lock = false, $with_members = true)
{
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_hierarchy_versions WHERE version_id='.(int)$version_id.($lock ? ' FOR UPDATE' : ''));
	$version = $rows ? db_fetch_assoc($rows) : false;
	if (!$version)
		return false;
	$version['members'] = array();
	if (!$with_members)
		return $version;
	$rows = db_query('SELECT member_id, parent_member_id FROM '.TB_PREF.'dimension_hierarchy_members WHERE version_id='.(int)$version_id.' ORDER BY member_id'.($lock ? ' FOR UPDATE' : ''));
	if ($rows === false)
		return false;
	while ($row = db_fetch_assoc($rows))
		$version['members'][(int)$row['member_id']] = (int)$row['parent_member_id'];
	return $version;
}

/** Does a loaded version still match its digest and member count? */
function dimension_hier_intact($version)
{
	if (!is_array($version) || !isset($version['members']) || (int)$version['member_count'] !== count($version['members']))
		return false;
	$basis = $version['basis_sha256'] === null || $version['basis_sha256'] === '' ? null : $version['basis_sha256'];
	return hash_equals((string)$version['content_sha256'], dimension_hier_content_sha256(array('axis' => $version['axis'], 'basis_sha256' => $basis,
		'effective_from' => $version['effective_from'], 'retroactive' => $version['retroactive'], 'parent_policy' => $version['parent_policy'], 'members' => $version['members'])));
}

/** The latest approved version row of an axis (the only valid basis of the next version). */
function dimension_hier_latest_approved($axis, $lock = false)
{
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_hierarchy_versions WHERE axis='.(int)$axis." AND status='approved' ORDER BY version_no DESC LIMIT 1".($lock ? ' FOR UPDATE' : ''));
	$row = $rows ? db_fetch_assoc($rows) : false;
	return $row ? $row : false;
}

/**
 * Every approved version of an axis in effective order, each verified; a version that fails its digest is returned with `intact` false.
 *
 * @param int $axis Axis 1 or 2
 * @return array|false Versions with members, oldest effective first
 */
function dimension_hier_approved_versions($axis)
{
	$rows = db_query('SELECT version_id FROM '.TB_PREF.'dimension_hierarchy_versions WHERE axis='.(int)$axis." AND status='approved' ORDER BY effective_from, version_no");
	if ($rows === false)
		return false;
	$out = array();
	while ($row = db_fetch_assoc($rows)) {
		$version = dimension_hier_load($row['version_id']);
		if (!$version)
			return false;
		$version['intact'] = dimension_hier_intact($version);
		$out[] = $version;
	}
	return $out;
}

/**
 * The structure effective on a date (the as-posted mapping of that date).
 *
 * @param int $axis Axis 1 or 2
 * @param string $date SQL date
 * @return array|null|false The verified version, null when none is effective yet, false when denied/unreadable or a version fails its digest
 */
function dimension_hier_effective_version($axis, $date)
{
	if (!dimension_hier_has_access(array('SA_DIMTRANSVIEW')) || dimension_hier_schema_ready() !== true || !is_iso_calendar_date($date))
		return false;
	$versions = dimension_hier_approved_versions($axis);
	if ($versions === false)
		return false;
	$found = null;
	foreach ($versions as $version) {
		if ($version['effective_from'] > $date)
			break;
		$found = $version;
	}
	return $found !== null && !$found['intact'] ? false : $found;
}

/** Normalize the raw changes of a proposal into member => parent|null (null removes the member). */
function dimension_hier_normalize_changes($changes, &$result)
{
	$out = array();
	if (!is_array($changes) || !count($changes)) {
		dimension_hier_fail($result, 'no_changes', _('Propose at least one hierarchy change.'));
		return $out;
	}
	if (count($changes) > dimension_hier_max_members()) {
		dimension_hier_fail($result, 'too_many_members', _('The proposal carries more members than one hierarchy may hold.'));
		return $out;
	}
	foreach ($changes as $change) {
		if (!is_array($change) || array_diff(array_keys($change), array('member', 'parent')) || !array_key_exists('member', $change)) {
			dimension_hier_fail($result, 'invalid_change', _('A hierarchy change holds an unknown or missing field.'));
			return array();
		}
		$member = $change['member'];
		$parent = array_key_exists('parent', $change) ? $change['parent'] : 0;
		if (is_string($member) && preg_match('/^[1-9][0-9]{0,9}$/D', $member))
			$member = (int)$member;
		if (is_string($parent) && preg_match('/^(0|[1-9][0-9]{0,9})$/D', $parent))
			$parent = (int)$parent;
		if (!is_int($member) || $member < 1 || ($parent !== null && $parent !== '' && (!is_int($parent) || $parent < 0))) {
			dimension_hier_fail($result, 'invalid_change', _('A hierarchy change needs a tracking value identity and a parent identity (0 for none) or null to remove the member.'));
			return array();
		}
		if (isset($out[$member])) {
			dimension_hier_fail($result, 'duplicate_change', _('A tracking value is proposed twice.'));
			return array();
		}
		$out[$member] = ($parent === null || $parent === '') ? null : $parent;
	}
	return $out;
}

/**
 * Propose the first hierarchy of an axis or a new version over its latest approved version (the maker step).
 *
 * @param int $axis Axis 1 or 2
 * @param array $changes Changes: member, parent (0 = root, null removes the member)
 * @param string $effective_from Date the structure takes effect (must follow the basis)
 * @param string $parent_policy allowed|leaf_only
 * @param string $commentary Mandatory commentary
 * @param bool $retroactive Acknowledge a date before today
 * @return array ok, version_id, version_no, member_count, errors, codes
 */
function dimension_hier_propose($axis, $changes, $effective_from, $parent_policy, $commentary, $retroactive = false)
{
	$result = dimension_hier_result(array('version_id' => 0, 'version_no' => 0, 'member_count' => 0));
	if (!dimension_hier_has_access(array('SA_DIMENSION')))
		return dimension_hier_fail($result, 'permission_denied', _('You are not allowed to propose tracking hierarchies.'));
	$actor = current_user_id_or_zero();
	if ($actor <= 0)
		return dimension_hier_fail($result, 'permission_denied', _('The proposing user could not be identified.'));
	if ($axis !== 1 && $axis !== 2)
		return dimension_hier_fail($result, 'invalid_axis', _('The axis must be category 1 or 2.'));
	$commentary = is_string($commentary) ? trim($commentary) : '';
	if ($commentary === '' || strlen($commentary) > 500)
		dimension_hier_fail($result, 'commentary_required', _('Commentary of 1 to 500 characters is required.'));
	if (!in_array($parent_policy, dimension_hier_policies(), true))
		dimension_hier_fail($result, 'invalid_policy', _('The parent posting policy must be allowed or leaf_only.'));
	if (!is_iso_calendar_date((string)$effective_from))
		dimension_hier_fail($result, 'invalid_date', _('The effective date is not valid.'));
	$normalized = dimension_hier_normalize_changes($changes, $result);
	if (!$result['ok'])
		return $result;
	if ($effective_from < dimension_hier_today() && $retroactive !== true)
		return dimension_hier_fail($result, 'retroactive_requires_flag', _('The effective date is before today; a retroactive structure must be marked retroactive on purpose.'));
	if (dimension_hier_schema_ready() !== true)
		return dimension_hier_fail($result, 'schema_missing', _('The hierarchy tables are not installed. Upgrade the company database first.'));
	if (!begin_transaction())
		return dimension_hier_fail($result, 'unavailable', _('The proposal could not start a transaction.'));
	$basis = dimension_hier_latest_approved($axis, true);
	$members = array();
	if ($basis) {
		$loaded = dimension_hier_load($basis['version_id'], true);
		if (!$loaded || !dimension_hier_intact($loaded)) {
			cancel_transaction();
			return dimension_hier_fail($result, 'version_tampered', _('The basis hierarchy no longer matches its digest.'));
		}
		if ($effective_from <= $basis['effective_from']) {
			cancel_transaction();
			return dimension_hier_fail($result, 'effective_not_after_basis', _('A new structure must take effect after its basis; history is never rewritten by an earlier date.'));
		}
		$members = $loaded['members'];
	}
	$changed = 0;
	foreach ($normalized as $member => $parent) {
		if ($parent === null) {
			if (!isset($members[$member])) {
				cancel_transaction();
				return dimension_hier_fail($result, 'remove_absent', _('A tracking value that is not in the basis cannot be removed.'));
			}
			unset($members[$member]);
			$changed++;
			continue;
		}
		$rows = db_query('SELECT type_ FROM '.TB_PREF.'dimensions WHERE id='.(int)$member);
		$row = $rows ? db_fetch_row($rows) : false;
		if (!$row || (int)$row[0] !== $axis) {
			cancel_transaction();
			return dimension_hier_fail($result, $row ? 'wrong_axis' : 'dimension_missing', $row
				? sprintf(_('Tracking value %d is not a category %d tracking value; a hierarchy never mixes categories.'), $member, $axis) : sprintf(_('Tracking value %d does not exist.'), $member));
		}
		if (!isset($members[$member]) || $members[$member] !== $parent) {
			$members[$member] = $parent;
			$changed++;
		}
	}
	$check = dimension_hier_validate_structure($members);
	if (!$check['ok']) {
		cancel_transaction();
		return dimension_hier_fail($result, $check['code'], $check['text']);
	}
	if (!count($members) || count($members) > dimension_hier_max_members()) {
		cancel_transaction();
		return dimension_hier_fail($result, 'member_count', _('A hierarchy needs at least one and at most 5000 members.'));
	}
	if (!$changed && !($basis && $basis['parent_policy'] !== $parent_policy)) {
		cancel_transaction();
		return dimension_hier_fail($result, 'no_change', _('The proposal changes nothing.'));
	}
	$version_id = dimension_hier_store($axis, $basis, $members, $effective_from, $parent_policy, $retroactive === true, $commentary, $actor);
	if ($version_id === false || !commit_transaction()) {
		cancel_transaction();
		return dimension_hier_fail($result, 'unavailable', _('The hierarchy could not be recorded.'));
	}
	$loaded = dimension_hier_load($version_id, false, false);
	$result['version_id'] = $version_id;
	$result['version_no'] = $loaded ? (int)$loaded['version_no'] : 0;
	$result['member_count'] = count($members);
	return $result;
}

/** Insert one hierarchy version and its members inside the caller's transaction. */
function dimension_hier_store($axis, $basis, $members, $effective_from, $parent_policy, $retroactive, $commentary, $actor)
{
	$rows = db_query('SELECT MAX(version_no) FROM '.TB_PREF.'dimension_hierarchy_versions WHERE axis='.(int)$axis);
	$row = $rows ? db_fetch_row($rows) : false;
	if (!$row)
		return false;
	$next = (int)$row[0] + 1;
	ksort($members);
	$basis_sha = $basis ? $basis['content_sha256'] : null;
	$sha = dimension_hier_content_sha256(array('axis' => $axis, 'basis_sha256' => $basis_sha, 'effective_from' => $effective_from, 'retroactive' => $retroactive ? 1 : 0,
		'parent_policy' => $parent_policy, 'members' => $members));
	if (!db_query('INSERT INTO '.TB_PREF.'dimension_hierarchy_versions (axis, version_no, basis_version_id, basis_sha256, status, effective_from, retroactive, parent_policy,'
		.' member_count, content_sha256, commentary, maker_id, created_at) VALUES ('.(int)$axis.','.$next.','.($basis ? (int)$basis['version_id'] : 'NULL').','
		.($basis_sha === null ? 'NULL' : dimension_hier_quote($basis_sha)).",'pending',".dimension_hier_quote($effective_from).','.($retroactive ? 1 : 0).','
		.dimension_hier_quote($parent_policy).','.count($members).','.dimension_hier_quote($sha).','.dimension_hier_quote($commentary).','.(int)$actor.',NOW())'))
		return false;
	$version_id = (int)db_insert_id();
	if ($version_id <= 0)
		return false;
	foreach (array_chunk(array_keys($members), 400) as $chunk) {
		$values = array();
		foreach ($chunk as $member)
			$values[] = '('.$version_id.','.(int)$member.','.(int)$members[$member].')';
		if (!db_query('INSERT INTO '.TB_PREF.'dimension_hierarchy_members (version_id, member_id, parent_member_id) VALUES '.implode(',', $values)))
			return false;
	}
	return $version_id;
}

/**
 * Approve or reject a proposed hierarchy version (the independent checker step).
 *
 * @param int $version_id Version
 * @param string $decision approve|reject
 * @param string $note Optional decision note (200 characters)
 * @return array ok, status, errors, codes
 */
function dimension_hier_decide($version_id, $decision, $note = '')
{
	$result = dimension_hier_result(array('status' => ''));
	if (!dimension_hier_has_access(array('SA_DIMENSION')))
		return dimension_hier_fail($result, 'permission_denied', _('You are not allowed to decide tracking hierarchies.'));
	if (!in_array($decision, array('approve', 'reject'), true))
		return dimension_hier_fail($result, 'invalid_decision', _('The decision must be approve or reject.'));
	$actor = current_user_id_or_zero();
	if ($actor <= 0)
		return dimension_hier_fail($result, 'permission_denied', _('The deciding user could not be identified.'));
	$note = is_string($note) ? trim($note) : '';
	if (strlen($note) > 200)
		return dimension_hier_fail($result, 'note_too_long', _('The decision note may not be longer than 200 characters.'));
	if (dimension_hier_schema_ready() !== true)
		return dimension_hier_fail($result, 'schema_missing', _('The hierarchy tables are not installed. Upgrade the company database first.'));
	if (!begin_transaction())
		return dimension_hier_fail($result, 'unavailable', _('The decision could not start a transaction.'));
	$peek = dimension_hier_load($version_id, false, false);
	if (!$peek) {
		cancel_transaction();
		return dimension_hier_fail($result, 'not_pending', _('Only a proposed hierarchy can be decided.'));
	}
	// Serialize every decision of the axis on its latest approved row (or the version itself when none exists yet).
	dimension_hier_latest_approved($peek['axis'], true);
	$version = dimension_hier_load($version_id, true);
	if (!$version || $version['status'] !== 'pending') {
		cancel_transaction();
		return dimension_hier_fail($result, 'not_pending', _('Only a proposed hierarchy can be decided.'));
	}
	if ((int)$version['maker_id'] === $actor) {
		cancel_transaction();
		return dimension_hier_fail($result, 'maker_checker', _('The proposing user cannot decide the same hierarchy; ask an independent checker.'));
	}
	$status = $decision === 'approve' ? 'approved' : 'rejected';
	if ($decision === 'approve') {
		if (!dimension_hier_intact($version)) {
			cancel_transaction();
			return dimension_hier_fail($result, 'version_tampered', _('The stored hierarchy no longer matches its digest and cannot be approved.'));
		}
		$latest = dimension_hier_latest_approved($version['axis'], true);
		if ($version['basis_version_id'] !== null) {
			$basis = dimension_hier_load($version['basis_version_id'], true);
			if (!$latest || !$basis || (int)$latest['version_id'] !== (int)$version['basis_version_id'] || $basis['status'] !== 'approved') {
				cancel_transaction();
				return dimension_hier_fail($result, 'stale_basis', _('A newer hierarchy has been approved since this one was proposed; propose it again on the latest approved hierarchy.'));
			}
			if ($basis['content_sha256'] !== $version['basis_sha256'] || !dimension_hier_intact($basis)) {
				cancel_transaction();
				return dimension_hier_fail($result, 'version_tampered', _('The basis of this hierarchy no longer matches its digest.'));
			}
		} elseif ($latest) {
			cancel_transaction();
			return dimension_hier_fail($result, 'hierarchy_exists', _('This axis already has an approved hierarchy; an initial hierarchy cannot follow it.'));
		}
		$check = dimension_hier_validate_structure($version['members']);
		if (!$check['ok']) {
			cancel_transaction();
			return dimension_hier_fail($result, $check['code'], $check['text']);
		}
	}
	if (db_query('UPDATE '.TB_PREF.'dimension_hierarchy_versions SET status='.dimension_hier_quote($status).', checker_id='.$actor.', decision_note='.dimension_hier_quote($note)
		.', decided_at=NOW() WHERE version_id='.(int)$version_id." AND status='pending'") === false || !commit_transaction()) {
		cancel_transaction();
		return dimension_hier_fail($result, 'unavailable', _('The decision could not be recorded.'));
	}
	$result['status'] = $status;
	return $result;
}

/** Versions of an axis, newest first (false when denied or unreadable). */
function dimension_hier_recent($axis = 0, $limit = 50)
{
	if (!dimension_hier_has_access(array('SA_DIMTRANSVIEW')) || dimension_hier_schema_ready() !== true)
		return false;
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_hierarchy_versions'.((int)$axis > 0 ? ' WHERE axis='.(int)$axis : '').' ORDER BY version_id DESC LIMIT '.max(1, min(500, (int)$limit)));
	if ($rows === false)
		return false;
	$out = array();
	while ($row = db_fetch_assoc($rows))
		$out[] = $row;
	return $out;
}

/**
 * Compare two hierarchy versions of one axis.
 *
 * @param int $left_id Earlier version
 * @param int $right_id Later version
 * @return array|false same, moved, added, removed counts and the changed members, or false when denied or unreadable
 */
function dimension_hier_compare($left_id, $right_id)
{
	if (!dimension_hier_has_access(array('SA_DIMTRANSVIEW')) || dimension_hier_schema_ready() !== true)
		return false;
	$left = dimension_hier_load($left_id);
	$right = dimension_hier_load($right_id);
	if (!$left || !$right || (int)$left['axis'] !== (int)$right['axis'] || !dimension_hier_intact($left) || !dimension_hier_intact($right))
		return false;
	$out = array('same' => 0, 'moved' => 0, 'added' => 0, 'removed' => 0, 'members' => array());
	foreach ($right['members'] as $member => $parent) {
		if (!isset($left['members'][$member])) {
			$out['added']++;
			$out['members'][$member] = array('left' => null, 'right' => $parent);
		} elseif ($left['members'][$member] !== $parent) {
			$out['moved']++;
			$out['members'][$member] = array('left' => $left['members'][$member], 'right' => $parent);
		} else
			$out['same']++;
	}
	foreach ($left['members'] as $member => $parent)
		if (!isset($right['members'][$member])) {
			$out['removed']++;
			$out['members'][$member] = array('left' => $parent, 'right' => null);
		}
	return $out;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Rollups
// ---------------------------------------------------------------------------------------------------------------------------------

/** An empty measure triple. */
function dimension_hier_measure()
{
	return array('revenue' => 0.0, 'costs' => 0.0, 'lines' => 0);
}

/** Add one measure into another (in place). */
function dimension_hier_add(&$target, $source)
{
	$target['revenue'] = round($target['revenue'] + $source['revenue'], 6);
	$target['costs'] = round($target['costs'] + $source['costs'], 6);
	$target['lines'] += $source['lines'];
}

/**
 * Split a period into the date segments of the as-posted view (pure).
 *
 * @param array $versions Approved versions oldest effective first (each with effective_from)
 * @param string $from Period start
 * @param string $to Period end
 * @return array Segments (from, to, version index or -1 for no structure yet)
 */
function dimension_hier_segments($versions, $from, $to)
{
	$bounds = array();
	foreach ($versions as $index => $version)
		$bounds[] = array($version['effective_from'], $index);
	$segments = array();
	$cursor = $from;
	$active = -1;
	foreach ($bounds as $bound) {
		if ($bound[0] <= $cursor) {
			$active = $bound[1];
			continue;
		}
		if ($bound[0] > $to)
			break;
		$segments[] = array('from' => $cursor, 'to' => date('Y-m-d', strtotime($bound[0].' -1 day')), 'version' => $active);
		$cursor = $bound[0];
		$active = $bound[1];
	}
	$segments[] = array('from' => $cursor, 'to' => $to, 'version' => $active);
	return $segments;
}

/** Read the P&L postings of one axis column grouped by Dimension for a date range. */
function dimension_hier_read_postings($column, $from, $to, $allowed)
{
	$sql = 'SELECT t.'.$column.' AS member, SUM(CASE WHEN c.ctype='.CL_INCOME.' THEN -t.amount ELSE 0 END) AS revenue,'
		.' SUM(CASE WHEN c.ctype IN ('.CL_COGS.','.CL_EXPENSE.') THEN t.amount ELSE 0 END) AS costs, COUNT(*) AS posting_lines FROM '.TB_PREF.'gl_trans t INNER JOIN '.TB_PREF
		.'chart_master a ON a.account_code=t.account INNER JOIN '.TB_PREF.'chart_types ct ON ct.id=a.account_type INNER JOIN '.TB_PREF.'chart_class c ON c.cid=ct.class_id'
		.' WHERE c.ctype IN ('.CL_INCOME.','.CL_COGS.','.CL_EXPENSE.") AND t.tran_date>='".$from."' AND t.tran_date<='".$to."'";
	if (is_array($allowed))
		$sql .= ' AND t.'.$column.' IN ('.(count($allowed) ? implode(',', array_map('intval', $allowed)) : '-1').')';
	$sql .= ' GROUP BY t.'.$column.' ORDER BY t.'.$column;
	$rows = db_query($sql, 'The Dimension postings could not be read');
	if ($rows === false)
		return false;
	$out = array();
	while ($row = db_fetch_assoc($rows))
		$out[(int)$row['member']] = array('revenue' => round((float)$row['revenue'], 6), 'costs' => round((float)$row['costs'], 6), 'lines' => (int)$row['posting_lines']);
	return $out;
}

/**
 * The portfolio rollup of one axis.
 *
 * `as_posted` reads every posting through the structure effective on its posting date (history is never restated); `current`
 * applies the latest approved structure to every posting and is labelled as restating history. Each posting is added to its own
 * node and once to each ancestor; the portfolio total counts it once. Revenue is credit-positive, costs are COGS plus expense.
 *
 * @param int $axis Axis 1 or 2
 * @param string $from SQL date
 * @param string $to SQL date
 * @param string $view as_posted|current
 * @param array|null $allowed_ids Visible Dimension identities (null = all); hidden Dimensions are excluded BEFORE aggregation
 * @return array|false nodes, buckets, totals, segments, exceptions; false when denied, invalid or unreadable
 */
function dimension_hier_rollup($axis, $from, $to, $view = 'as_posted', $allowed_ids = null)
{
	if (!dimension_hier_has_access(array('SA_DIMTRANSVIEW', 'SA_GLANALYTIC')) || dimension_hier_schema_ready() !== true)
		return false;
	if (($axis !== 1 && $axis !== 2) || !is_iso_calendar_date($from) || !is_iso_calendar_date($to) || $from > $to || !in_array($view, array('as_posted', 'current'), true))
		return false;
	$allowed = null;
	if ($allowed_ids !== null) {
		$allowed = array();
		foreach ((array)$allowed_ids as $id)
			if (is_int($id) && $id >= 0)
				$allowed[$id] = $id;
		$allowed = array_values($allowed);
	}
	$versions = dimension_hier_approved_versions($axis);
	if ($versions === false)
		return false;
	foreach ($versions as $version)
		if (!$version['intact'])
			return false;
	if ($view === 'current') {
		$segments = array(array('from' => $from, 'to' => $to, 'version' => count($versions) ? count($versions) - 1 : -1));
	} else
		$segments = dimension_hier_segments($versions, $from, $to);
	$column = $axis === 1 ? 'dimension_id' : 'dimension2_id';
	$nodes = array();
	$out = array('axis' => $axis, 'view' => $view, 'from' => $from, 'to' => $to, 'scope_limited' => $allowed !== null, 'segments' => array(), 'nodes' => array(),
		'portfolio' => dimension_hier_measure(), 'outside' => dimension_hier_measure(), 'unassigned' => dimension_hier_measure(), 'unknown' => dimension_hier_measure(),
		'total' => dimension_hier_measure(), 'exceptions' => array(), 'retroactive_segments' => 0,
		'label' => $view === 'current'
			? _('Current structure: the latest approved hierarchy is applied to every posting, so earlier periods are restated.')
			: _('As posted: every posting is read through the hierarchy that was effective on its posting date.'));
	$masters = array();
	$rows = db_query('SELECT id, type_ FROM '.TB_PREF.'dimensions');
	if ($rows === false)
		return false;
	while ($row = db_fetch_assoc($rows))
		$masters[(int)$row['id']] = (int)$row['type_'];
	foreach ($segments as $segment) {
		$version = $segment['version'] >= 0 ? $versions[$segment['version']] : null;
		$out['segments'][] = array('from' => $segment['from'], 'to' => $segment['to'], 'version_id' => $version ? (int)$version['version_id'] : 0,
			'version_no' => $version ? (int)$version['version_no'] : 0, 'retroactive' => $version ? (int)$version['retroactive'] : 0);
		if ($version && $view === 'as_posted' && (int)$version['retroactive'] === 1)
			$out['retroactive_segments']++;
		$postings = dimension_hier_read_postings($column, $segment['from'], $segment['to'], $allowed);
		if ($postings === false)
			return false;
		$members = $version ? $version['members'] : array();
		$parents_with_children = array();
		foreach ($members as $member => $parent)
			if ($parent !== 0)
				$parents_with_children[$parent] = true;
		foreach ($postings as $member => $measure) {
			dimension_hier_add($out['total'], $measure);
			if ($member === 0) {
				dimension_hier_add($out['unassigned'], $measure);
				continue;
			}
			if (!isset($masters[$member]) || $masters[$member] !== $axis) {
				dimension_hier_add($out['unknown'], $measure);
				continue;
			}
			if (!isset($nodes[$member]))
				$nodes[$member] = array('member' => $member, 'direct' => dimension_hier_measure(), 'rollup' => dimension_hier_measure(), 'parent' => 0, 'in_hierarchy' => false);
			dimension_hier_add($nodes[$member]['direct'], $measure);
			dimension_hier_add($nodes[$member]['rollup'], $measure);
			if (!isset($members[$member])) {
				dimension_hier_add($out['outside'], $measure);
				continue;
			}
			$nodes[$member]['in_hierarchy'] = true;
			$nodes[$member]['parent'] = $members[$member];
			dimension_hier_add($out['portfolio'], $measure);
			foreach (dimension_hier_ancestors($members, $member) as $ancestor) {
				if (!isset($nodes[$ancestor]))
					$nodes[$ancestor] = array('member' => $ancestor, 'direct' => dimension_hier_measure(), 'rollup' => dimension_hier_measure(), 'parent' => $members[$ancestor], 'in_hierarchy' => true);
				dimension_hier_add($nodes[$ancestor]['rollup'], $measure);
			}
			if ($version['parent_policy'] === 'leaf_only' && isset($parents_with_children[$member]))
				$out['exceptions'][] = array('member' => $member, 'version_id' => (int)$version['version_id'], 'from' => $segment['from'], 'to' => $segment['to'],
					'revenue' => $measure['revenue'], 'costs' => $measure['costs'], 'lines' => $measure['lines'], 'code' => 'direct_parent_posting');
		}
		foreach ($members as $member => $parent)
			if (!isset($nodes[$member]))
				$nodes[$member] = array('member' => $member, 'direct' => dimension_hier_measure(), 'rollup' => dimension_hier_measure(), 'parent' => $parent, 'in_hierarchy' => true);
	}
	ksort($nodes);
	$out['nodes'] = array_values($nodes);
	return $out;
}
