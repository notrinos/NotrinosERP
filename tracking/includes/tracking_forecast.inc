<?php
/**
 * DIM-ADV-001: forecast = GL actual + remaining commitment + UNCOMMITTED estimate to complete, with approved revisions.
 *
 * A forecast version states, per account and exact Dimension pair of one fiscal year, the ESTIMATE TO COMPLETE (ETC): the total
 * cost still expected after the as-of date, including work that is already committed. The forecast therefore never counts a
 * commitment twice:
 *
 *   uncommitted ETC = max(0, ETC - remaining commitment)
 *   forecast        = GL actual + remaining commitment + uncommitted ETC
 *
 * Versions are immutable, complete (every version stores all of its lines), proposed by a maker with mandatory commentary and
 * approved or rejected by a DIFFERENT checker; a revision whose basis is no longer the latest approved version is stale. Content
 * digests are chained to the basis and verified on approval and on every position read. Nothing is written to the budget, the
 * general ledger, purchasing or the commitment register. Commitments and the forecast are advisory evidence. PHP 5.6 compatible.
 */
require_once(__DIR__.'/../../includes/value_helpers.inc');

/** Largest number of lines one forecast version may hold. */
function dimension_forecast_max_lines()
{
	return 20000;
}

/** Canonical identity key of one forecast line. */
function dimension_forecast_line_key($line)
{
	return $line['account_code']."\t".(int)$line['coordinate_1']."\t".(int)$line['coordinate_2'];
}

/** Digest of a forecast version's content, chained to its basis. */
function dimension_forecast_content_sha256($version)
{
	$lines = array();
	foreach ($version['lines'] as $line)
		$lines[] = array((string)$line['account_code'], (int)$line['coordinate_1'], (int)$line['coordinate_2'], dimension_commit_amount_text($line['etc_amount']));
	return hash('sha256', json_encode(array('adv1-forecast', (int)$version['fiscal_year'], (string)$version['version_kind'],
		$version['basis_sha256'] === null ? null : (string)$version['basis_sha256'], (string)$version['as_of_date'], $lines)));
}

/** Sort lines into the canonical order. */
function dimension_forecast_sort_lines($lines)
{
	usort($lines, function ($left, $right) {
		return strcmp(dimension_forecast_line_key($left), dimension_forecast_line_key($right));
	});
	return $lines;
}

/** Load one forecast version with its lines (optionally locked). */
function dimension_forecast_load($version_id, $lock = false, $with_lines = true)
{
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_forecast_versions WHERE version_id='.(int)$version_id.($lock ? ' FOR UPDATE' : ''));
	$version = $rows ? db_fetch_assoc($rows) : false;
	if (!$version)
		return false;
	$version['lines'] = array();
	if (!$with_lines)
		return $version;
	$rows = db_query('SELECT account_code, coordinate_1, coordinate_2, etc_amount FROM '.TB_PREF.'dimension_forecast_lines WHERE version_id='.(int)$version_id
		.' ORDER BY account_code, coordinate_1, coordinate_2'.($lock ? ' FOR UPDATE' : ''));
	if ($rows === false)
		return false;
	while ($row = db_fetch_assoc($rows))
		$version['lines'][] = array('account_code' => (string)$row['account_code'], 'coordinate_1' => (int)$row['coordinate_1'],
			'coordinate_2' => (int)$row['coordinate_2'], 'etc_amount' => round((float)$row['etc_amount'], 6));
	return $version;
}

/** Does a loaded version still match its digest, counts and total? */
function dimension_forecast_intact($version)
{
	if (!is_array($version) || !isset($version['lines']) || (int)$version['line_count'] !== count($version['lines']))
		return false;
	$total = 0.0;
	foreach ($version['lines'] as $line)
		$total += $line['etc_amount'];
	if (abs($total - (float)$version['total_amount']) > 0.0001 + abs($total) * 1.0E-9)
		return false;
	$basis = $version['basis_sha256'] === null || $version['basis_sha256'] === '' ? null : $version['basis_sha256'];
	return hash_equals((string)$version['content_sha256'], dimension_forecast_content_sha256(array('fiscal_year' => $version['fiscal_year'],
		'version_kind' => $version['version_kind'], 'basis_sha256' => $basis, 'as_of_date' => $version['as_of_date'], 'lines' => $version['lines'])));
}

/** The latest approved forecast version of a fiscal year (the only valid basis of the next revision). */
function dimension_forecast_latest_approved($fiscal_year, $lock = false)
{
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_forecast_versions WHERE fiscal_year='.(int)$fiscal_year." AND status='approved' ORDER BY version_no DESC LIMIT 1"
		.($lock ? ' FOR UPDATE' : ''));
	$row = $rows ? db_fetch_assoc($rows) : false;
	return $row ? $row : false;
}

/** Read one fiscal year (begin/end as SQL dates). */
function dimension_forecast_year($fiscal_year, $lock = false)
{
	$rows = db_query('SELECT id, `begin` AS period_from, `end` AS period_to FROM '.TB_PREF.'fiscal_year WHERE id='.(int)$fiscal_year.($lock ? ' FOR UPDATE' : ''));
	$row = $rows ? db_fetch_assoc($rows) : false;
	return $row ? $row : false;
}

/** Validate commentary text (1 to 500 characters). */
function dimension_forecast_check_commentary($text, &$result)
{
	$text = is_string($text) ? trim($text) : '';
	if ($text === '')
		dimension_commit_fail($result, 'commentary_required', _('Explain the forecast; commentary is required.'));
	elseif (strlen($text) > 500)
		dimension_commit_fail($result, 'commentary_too_long', _('The commentary may not be longer than 500 characters.'));
	return $text;
}

/**
 * Normalize and validate the raw changes of a proposal.
 *
 * @param array $changes Raw changes (account, coordinate_1, coordinate_2, etc_amount; null/'' removes a line)
 * @param array $result Result to record refusals on
 * @return array Normalized changes keyed by line key (etc_amount null = remove)
 */
function dimension_forecast_normalize_changes($changes, &$result)
{
	$out = array();
	if (!is_array($changes) || !count($changes)) {
		dimension_commit_fail($result, 'no_changes', _('Propose at least one forecast line.'));
		return $out;
	}
	if (count($changes) > dimension_forecast_max_lines()) {
		dimension_commit_fail($result, 'too_many_lines', _('The proposal carries more lines than one forecast may hold.'));
		return $out;
	}
	foreach ($changes as $change) {
		if (!is_array($change) || array_diff(array_keys($change), array('account', 'coordinate_1', 'coordinate_2', 'etc_amount'))) {
			dimension_commit_fail($result, 'invalid_change', _('A forecast line holds an unknown or missing field.'));
			return array();
		}
		$account = isset($change['account']) ? $change['account'] : '';
		$c1 = isset($change['coordinate_1']) ? $change['coordinate_1'] : 0;
		$c2 = isset($change['coordinate_2']) ? $change['coordinate_2'] : 0;
		if (is_string($c1) && preg_match('/^(0|[1-9][0-9]{0,9})$/D', $c1))
			$c1 = (int)$c1;
		if (is_string($c2) && preg_match('/^(0|[1-9][0-9]{0,9})$/D', $c2))
			$c2 = (int)$c2;
		if (!is_string($account) || !preg_match('/^[\x21-\x7e]{1,15}$/D', $account) || !is_int($c1) || !is_int($c2) || $c1 < 0 || $c2 < 0) {
			dimension_commit_fail($result, 'invalid_scope', _('A forecast line needs an account and exact non-negative tracking values.'));
			return array();
		}
		$raw = array_key_exists('etc_amount', $change) ? $change['etc_amount'] : null;
		$amount = ($raw === null || $raw === '') ? null : dimension_commit_parse_amount($raw);
		if ($amount === false) {
			dimension_commit_fail($result, 'invalid_amount', _('The estimate to complete must be a non-negative number.'));
			return array();
		}
		$line = array('account_code' => $account, 'coordinate_1' => $c1, 'coordinate_2' => $c2, 'etc_amount' => $amount);
		$key = dimension_forecast_line_key($line);
		if (isset($out[$key])) {
			dimension_commit_fail($result, 'duplicate_change', _('A forecast line is proposed twice.'));
			return array();
		}
		$out[$key] = $line;
	}
	return $out;
}

/** Check that the accounts and Dimensions of the lines that are being set exist (and are of the right axis). */
function dimension_forecast_check_references($changes, &$result)
{
	foreach ($changes as $line) {
		if ($line['etc_amount'] === null)
			continue;
		$rows = db_query('SELECT account_code FROM '.TB_PREF.'chart_master WHERE account_code='.dimension_commit_quote($line['account_code']));
		if (!$rows || !db_fetch_row($rows)) {
			dimension_commit_fail($result, 'account_missing', sprintf(_('Account "%s" does not exist.'), preg_replace('/[^\x20-\x7e]/', '?', $line['account_code'])));
			return false;
		}
		foreach (array(1, 2) as $axis) {
			$id = $line['coordinate_'.$axis];
			if ($id === 0)
				continue;
			$rows = db_query('SELECT type_ FROM '.TB_PREF.'dimensions WHERE id='.(int)$id);
			$row = $rows ? db_fetch_row($rows) : false;
			if (!$row) {
				dimension_commit_fail($result, 'dimension_missing', sprintf(_('Tracking value %d does not exist.'), $id));
				return false;
			}
			if ((int)$row[0] !== $axis) {
				dimension_commit_fail($result, 'dimension_axis', sprintf(_('Tracking value %d is not a category %d tracking value.'), $id, $axis));
				return false;
			}
		}
	}
	return true;
}

/**
 * Propose the first forecast of a fiscal year or a revision of its latest approved forecast (the maker step).
 *
 * @param int $fiscal_year Fiscal year
 * @param int $basis_version_id 0 for the first forecast, otherwise the latest approved version
 * @param array $changes Lines (account, coordinate_1, coordinate_2, etc_amount; null removes a line of the basis)
 * @param string $commentary Mandatory commentary
 * @param string $as_of_date Date inside the fiscal year the estimate is made as of
 * @return array ok, version_id, version_no, line_count, errors, codes
 */
function dimension_forecast_propose($fiscal_year, $basis_version_id, $changes, $commentary, $as_of_date)
{
	$result = dimension_commit_result(array('version_id' => 0, 'version_no' => 0, 'line_count' => 0));
	if (!dimension_commit_has_access(array('SA_BUDGETENTRY')))
		return dimension_commit_fail($result, 'permission_denied', _('You are not allowed to propose forecasts.'));
	$actor = current_user_id_or_zero();
	if ($actor <= 0)
		return dimension_commit_fail($result, 'permission_denied', _('The proposing user could not be identified.'));
	$commentary = dimension_forecast_check_commentary($commentary, $result);
	$normalized = dimension_forecast_normalize_changes($changes, $result);
	if (!is_iso_calendar_date((string)$as_of_date))
		dimension_commit_fail($result, 'invalid_date', _('The as-of date is not valid.'));
	if (!$result['ok'])
		return $result;
	if (dimension_commit_schema_ready() !== true)
		return dimension_commit_fail($result, 'schema_missing', _('The commitment tables are not installed. Upgrade the company database first.'));
	if (!begin_transaction())
		return dimension_commit_fail($result, 'unavailable', _('The proposal could not start a transaction.'));
	$year = dimension_forecast_year($fiscal_year, true);
	if (!$year) {
		cancel_transaction();
		return dimension_commit_fail($result, 'fiscal_year_missing', _('The fiscal year does not exist.'));
	}
	if ($as_of_date < $year['period_from'] || $as_of_date > $year['period_to']) {
		cancel_transaction();
		return dimension_commit_fail($result, 'invalid_date', _('The as-of date must lie inside the fiscal year.'));
	}
	if (!dimension_forecast_check_references($normalized, $result)) {
		cancel_transaction();
		return $result;
	}
	$basis = null;
	$lines = array();
	if ((int)$basis_version_id > 0) {
		$basis = dimension_forecast_load($basis_version_id, true);
		$latest = dimension_forecast_latest_approved($fiscal_year, true);
		if (!$basis || (int)$basis['fiscal_year'] !== (int)$fiscal_year || $basis['status'] !== 'approved' || !$latest || (int)$latest['version_id'] !== (int)$basis['version_id']) {
			cancel_transaction();
			return dimension_commit_fail($result, 'basis_invalid', _('A revision builds on the latest approved forecast of the same fiscal year.'));
		}
		if (!dimension_forecast_intact($basis)) {
			cancel_transaction();
			return dimension_commit_fail($result, 'version_tampered', _('The basis forecast no longer matches its digest.'));
		}
		if ($as_of_date < $basis['as_of_date']) {
			cancel_transaction();
			return dimension_commit_fail($result, 'as_of_before_basis', _('A revision cannot be made as of a date before its basis.'));
		}
		foreach ($basis['lines'] as $line)
			$lines[dimension_forecast_line_key($line)] = $line;
	} else {
		$rows = db_query('SELECT version_id FROM '.TB_PREF.'dimension_forecast_versions WHERE fiscal_year='.(int)$fiscal_year." AND status IN ('pending','approved') FOR UPDATE");
		if ($rows === false || db_num_rows($rows) > 0) {
			cancel_transaction();
			return dimension_commit_fail($result, 'forecast_exists', _('This fiscal year already has an open or approved forecast; propose a revision of it.'));
		}
	}
	$changed = 0;
	foreach ($normalized as $key => $change) {
		if ($change['etc_amount'] === null) {
			if (!isset($lines[$key])) {
				cancel_transaction();
				return dimension_commit_fail($result, 'remove_absent', _('A line that is not in the basis cannot be removed.'));
			}
			unset($lines[$key]);
			$changed++;
		} elseif (!isset($lines[$key]) || dimension_commit_amount_text($lines[$key]['etc_amount']) !== dimension_commit_amount_text($change['etc_amount'])) {
			$lines[$key] = $change;
			$changed++;
		}
	}
	if (!$changed) {
		cancel_transaction();
		return dimension_commit_fail($result, 'no_change', _('The proposal changes nothing.'));
	}
	if (!count($lines)) {
		cancel_transaction();
		return dimension_commit_fail($result, 'empty_forecast', _('A forecast needs at least one line.'));
	}
	$version_id = dimension_forecast_store($fiscal_year, $basis, $as_of_date, array_values($lines), $commentary, $actor);
	if ($version_id === false || !commit_transaction()) {
		cancel_transaction();
		return dimension_commit_fail($result, 'unavailable', _('The forecast could not be recorded.'));
	}
	$loaded = dimension_forecast_load($version_id, false, false);
	$result['version_id'] = $version_id;
	$result['version_no'] = $loaded ? (int)$loaded['version_no'] : 0;
	$result['line_count'] = count($lines);
	return $result;
}

/** Insert one forecast version and its lines inside the caller's transaction. */
function dimension_forecast_store($fiscal_year, $basis, $as_of_date, $lines, $commentary, $actor)
{
	$rows = db_query('SELECT MAX(version_no) FROM '.TB_PREF.'dimension_forecast_versions WHERE fiscal_year='.(int)$fiscal_year);
	$row = $rows ? db_fetch_row($rows) : false;
	if (!$row)
		return false;
	$next = (int)$row[0] + 1;
	$lines = dimension_forecast_sort_lines($lines);
	$kind = $basis === null ? 'initial' : 'revision';
	$basis_sha = $basis === null ? null : $basis['content_sha256'];
	$sha = dimension_forecast_content_sha256(array('fiscal_year' => $fiscal_year, 'version_kind' => $kind, 'basis_sha256' => $basis_sha, 'as_of_date' => $as_of_date, 'lines' => $lines));
	$total = 0.0;
	foreach ($lines as $line)
		$total += $line['etc_amount'];
	if (!db_query('INSERT INTO '.TB_PREF.'dimension_forecast_versions (fiscal_year, version_no, version_kind, basis_version_id, basis_sha256, status, as_of_date, line_count,'
		.' total_amount, content_sha256, commentary, maker_id, created_at) VALUES ('.(int)$fiscal_year.','.$next.','.dimension_commit_quote($kind).','
		.($basis === null ? 'NULL' : (int)$basis['version_id']).','.($basis_sha === null ? 'NULL' : dimension_commit_quote($basis_sha)).",'pending',"
		.dimension_commit_quote($as_of_date).','.count($lines).','.dimension_commit_amount_text($total).','.dimension_commit_quote($sha).','
		.dimension_commit_quote($commentary).','.(int)$actor.',NOW())'))
		return false;
	$version_id = (int)db_insert_id();
	if ($version_id <= 0)
		return false;
	foreach (array_chunk($lines, 400) as $chunk) {
		$values = array();
		foreach ($chunk as $line)
			$values[] = '('.$version_id.','.dimension_commit_quote($line['account_code']).','.(int)$line['coordinate_1'].','.(int)$line['coordinate_2'].','
				.dimension_commit_amount_text($line['etc_amount']).')';
		if (!db_query('INSERT INTO '.TB_PREF.'dimension_forecast_lines (version_id, account_code, coordinate_1, coordinate_2, etc_amount) VALUES '.implode(',', $values)))
			return false;
	}
	return $version_id;
}

/**
 * Approve or reject a proposed forecast (the independent checker step).
 *
 * @param int $version_id Version
 * @param string $decision approve|reject
 * @param string $note Optional decision note (200 characters)
 * @return array ok, status, errors, codes
 */
function dimension_forecast_decide($version_id, $decision, $note = '')
{
	$result = dimension_commit_result(array('status' => ''));
	if (!dimension_commit_has_access(array('SA_BUDGETENTRY')))
		return dimension_commit_fail($result, 'permission_denied', _('You are not allowed to decide forecasts.'));
	if (!in_array($decision, array('approve', 'reject'), true))
		return dimension_commit_fail($result, 'invalid_decision', _('The decision must be approve or reject.'));
	$actor = current_user_id_or_zero();
	if ($actor <= 0)
		return dimension_commit_fail($result, 'permission_denied', _('The deciding user could not be identified.'));
	$note = is_string($note) ? trim($note) : '';
	if (strlen($note) > 200)
		return dimension_commit_fail($result, 'note_too_long', _('The decision note may not be longer than 200 characters.'));
	if (dimension_commit_schema_ready() !== true)
		return dimension_commit_fail($result, 'schema_missing', _('The commitment tables are not installed. Upgrade the company database first.'));
	if (!begin_transaction())
		return dimension_commit_fail($result, 'unavailable', _('The decision could not start a transaction.'));
	$peek = dimension_forecast_load($version_id, false, false);
	if (!$peek || !dimension_forecast_year($peek['fiscal_year'], true)) {
		cancel_transaction();
		return dimension_commit_fail($result, 'not_pending', _('Only a proposed forecast can be decided.'));
	}
	$version = dimension_forecast_load($version_id, true);
	if (!$version || $version['status'] !== 'pending') {
		cancel_transaction();
		return dimension_commit_fail($result, 'not_pending', _('Only a proposed forecast can be decided.'));
	}
	if ((int)$version['maker_id'] === $actor) {
		cancel_transaction();
		return dimension_commit_fail($result, 'maker_checker', _('The proposing user cannot decide the same forecast; ask an independent checker.'));
	}
	$status = $decision === 'approve' ? 'approved' : 'rejected';
	if ($decision === 'approve') {
		if (!dimension_forecast_intact($version)) {
			cancel_transaction();
			return dimension_commit_fail($result, 'version_tampered', _('The stored forecast no longer matches its digest and cannot be approved.'));
		}
		$latest = dimension_forecast_latest_approved($version['fiscal_year'], true);
		if ($version['version_kind'] === 'revision') {
			$basis = dimension_forecast_load($version['basis_version_id'], true);
			if (!$latest || !$basis || (int)$latest['version_id'] !== (int)$version['basis_version_id'] || $basis['status'] !== 'approved') {
				cancel_transaction();
				return dimension_commit_fail($result, 'stale_basis', _('A newer forecast has been approved since this revision was proposed; propose it again on the latest approved forecast.'));
			}
			if ($basis['content_sha256'] !== $version['basis_sha256'] || !dimension_forecast_intact($basis)) {
				cancel_transaction();
				return dimension_commit_fail($result, 'version_tampered', _('The basis of this revision no longer matches its digest.'));
			}
		} elseif ($latest) {
			cancel_transaction();
			return dimension_commit_fail($result, 'forecast_exists', _('This fiscal year already has an approved forecast; an initial forecast cannot follow it.'));
		}
	}
	if (db_query('UPDATE '.TB_PREF.'dimension_forecast_versions SET status='.dimension_commit_quote($status).', checker_id='.$actor.', decision_note='.dimension_commit_quote($note)
		.', decided_at=NOW() WHERE version_id='.(int)$version_id." AND status='pending'") === false || !commit_transaction()) {
		cancel_transaction();
		return dimension_commit_fail($result, 'unavailable', _('The decision could not be recorded.'));
	}
	$result['status'] = $status;
	return $result;
}

/** Forecast versions of a fiscal year (newest first); false when denied or unreadable. */
function dimension_forecast_recent($fiscal_year = 0, $limit = 50)
{
	if (!dimension_commit_has_access(array('SA_GLANALYTIC', 'SA_BUDGETENTRY')) || dimension_commit_schema_ready() !== true)
		return false;
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_forecast_versions'.((int)$fiscal_year > 0 ? ' WHERE fiscal_year='.(int)$fiscal_year : '').' ORDER BY version_id DESC LIMIT '
		.max(1, min(500, (int)$limit)));
	if ($rows === false)
		return false;
	$out = array();
	while ($row = db_fetch_assoc($rows))
		$out[] = $row;
	return $out;
}

/**
 * The forecast of one exact account/Dimension pair of a fiscal year from its latest approved forecast version.
 *
 * @param int $fiscal_year Fiscal year
 * @param string $account Account code
 * @param int $coordinate_1 Exact axis 1 Dimension (0 = unassigned)
 * @param int $coordinate_2 Exact axis 2 Dimension (0 = unassigned)
 * @param string $as_of_date Actuals and the estimate are read up to this date (defaults to the forecast's own as-of date)
 * @return array|false actual, commitment, etc, uncommitted_etc, forecast, budget, state; false when denied or unreadable
 */
function dimension_forecast_position($fiscal_year, $account, $coordinate_1, $coordinate_2, $as_of_date = '')
{
	if (!dimension_commit_has_access(array('SA_GLANALYTIC', 'SA_BUDGETENTRY')) || dimension_commit_schema_ready() !== true)
		return false;
	if (!is_string($account) || !preg_match('/^[\x21-\x7e]{1,15}$/D', $account) || !is_int($coordinate_1) || !is_int($coordinate_2) || $coordinate_1 < 0 || $coordinate_2 < 0)
		return false;
	$year = dimension_forecast_year($fiscal_year);
	if (!$year)
		return false;
	$latest = dimension_forecast_latest_approved($fiscal_year);
	$out = array('fiscal_year' => (int)$fiscal_year, 'state' => 'no_forecast', 'version_id' => 0, 'version_no' => 0, 'etc' => null, 'uncommitted_etc' => 0.0,
		'forecast' => null, 'label' => _('The forecast is advisory: GL actual plus remaining commitment plus uncommitted estimate to complete.'));
	$version = null;
	if ($latest) {
		$version = dimension_forecast_load($latest['version_id']);
		if (!$version || !dimension_forecast_intact($version)) {
			$out['state'] = 'forecast_tampered';
			return $out;
		}
		$out['version_id'] = (int)$version['version_id'];
		$out['version_no'] = (int)$version['version_no'];
	}
	$as_of = $as_of_date !== '' ? $as_of_date : ($version ? $version['as_of_date'] : $year['period_to']);
	if (!is_iso_calendar_date($as_of) || $as_of < $year['period_from'] || $as_of > $year['period_to'])
		return false;
	$position = dimension_commit_position_unchecked($account, $coordinate_1 === 0 ? -1 : $coordinate_1, $coordinate_2 === 0 ? -1 : $coordinate_2, $year['period_from'], $year['period_to']);
	if ($position === false)
		return false;
	$sql = 'SELECT SUM(amount) FROM '.TB_PREF.'gl_trans WHERE account='.dimension_commit_quote($account).' AND tran_date>='.dimension_commit_quote($year['period_from'])
		.' AND tran_date<='.dimension_commit_quote($as_of).' AND dimension_id='.$coordinate_1.' AND dimension2_id='.$coordinate_2;
	$rows = db_query($sql);
	$row = $rows ? db_fetch_row($rows) : false;
	if (!$row)
		return false;
	$actual = round((float)$row[0], 6);
	$committed = $position['commitment_remaining'];
	$etc = null;
	if ($version)
		foreach ($version['lines'] as $line)
			if ($line['account_code'] === $account && $line['coordinate_1'] === $coordinate_1 && $line['coordinate_2'] === $coordinate_2)
				$etc = $line['etc_amount'];
	$uncommitted = $etc === null ? 0.0 : round(max(0.0, $etc - $committed), 6);
	$out = array_merge($out, array('as_of' => $as_of, 'actual' => $actual, 'commitment' => $committed, 'etc' => $etc, 'uncommitted_etc' => $uncommitted,
		'forecast' => round($actual + $committed + $uncommitted, 6), 'commitment_complete' => $position['complete'],
		'state' => !$version ? 'no_forecast' : ($etc === null ? 'no_estimate' : 'complete'), 'budget' => $position['budget'], 'budget_available' => $position['budget_available']));
	$out['variance'] = $position['budget_available'] ? round($position['budget'] - $out['forecast'], 6) : null;
	return $out;
}

/**
 * Compare two forecast versions line by line.
 *
 * @param int $left_id Earlier version
 * @param int $right_id Later version
 * @return array|false same/changed/added/removed counts and the changed lines, or false when denied or unreadable
 */
function dimension_forecast_compare($left_id, $right_id)
{
	if (!dimension_commit_has_access(array('SA_GLANALYTIC', 'SA_BUDGETENTRY')) || dimension_commit_schema_ready() !== true)
		return false;
	$left = dimension_forecast_load($left_id);
	$right = dimension_forecast_load($right_id);
	if (!$left || !$right || !dimension_forecast_intact($left) || !dimension_forecast_intact($right))
		return false;
	$index = array();
	foreach ($left['lines'] as $line)
		$index[dimension_forecast_line_key($line)] = $line;
	$out = array('same' => 0, 'changed' => 0, 'added' => 0, 'removed' => 0, 'lines' => array());
	foreach ($right['lines'] as $line) {
		$key = dimension_forecast_line_key($line);
		if (!isset($index[$key])) {
			$out['added']++;
			$out['lines'][] = array('key' => $key, 'left' => null, 'right' => $line['etc_amount']);
			continue;
		}
		if (dimension_commit_amount_text($index[$key]['etc_amount']) === dimension_commit_amount_text($line['etc_amount']))
			$out['same']++;
		else {
			$out['changed']++;
			$out['lines'][] = array('key' => $key, 'left' => $index[$key]['etc_amount'], 'right' => $line['etc_amount']);
		}
		unset($index[$key]);
	}
	foreach ($index as $key => $line) {
		$out['removed']++;
		$out['lines'][] = array('key' => $key, 'left' => $line['etc_amount'], 'right' => null);
	}
	return $out;
}
