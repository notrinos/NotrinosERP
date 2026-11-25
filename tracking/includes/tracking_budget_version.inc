<?php
/**
 * DIM-BUD-002: immutable budget baselines, approved revisions and an explicit active comparison.
 *
 * Budget entry (`gl/gl_budget.php`) edits `budget_trans` in place, so a budget that was later changed leaves no trace of the
 * plan the business approved. This authority adds versioned evidence beside that live table without changing it:
 *
 *  - a BASELINE is the exact snapshot of one fiscal year's `budget_trans` rows at the moment it is captured (months of the
 *    same account / Dimension pair are folded exactly like the reports, with the number of source rows kept). It is the
 *    plan as it stood; no earlier revision that was overwritten before the capture is ever inferred or invented;
 *  - a REVISION proposes explicit line changes (set an amount, an explicit zero, or remove a line) over the CURRENT
 *    approved version of the same fiscal year, with mandatory commentary. Every version stores its complete lines, so
 *    the baseline stays byte-exact while the revision carries its own full plan; a line that exists with amount 0 is a
 *    real zero, an absent line is a missing budget, and every line keeps the exact date it is dated;
 *  - a version is proposed by a maker and approved or rejected by a DIFFERENT checker. A revision whose basis is no longer
 *    the latest approved version of its fiscal year is STALE and cannot be approved (revision race); an identical open
 *    proposal answers with the existing version;
 *  - approving a version never changes what the reports compare. The ACTIVE comparison of a fiscal year is chosen
 *    explicitly (a version, or the legacy live budget) by an authorized user with a mandatory reason, and every change is
 *    an append-only audit row. The default stays the legacy live budget, so nothing moves until someone chooses;
 *  - every version carries a content digest chained to its basis; approval, activation and report reads verify it, and a
 *    version whose stored lines no longer match is refused (the report shows the budget as unavailable, never a tampered
 *    figure).
 *
 * No `budget_trans`, `gl_trans`, document, approval or HRM row is ever written, re-pointed or backfilled by this authority.
 * PHP 5.6 compatible syntax. Dates are SQL `Y-m-d`.
 */
require_once(__DIR__.'/../../includes/value_helpers.inc');

/** Largest number of lines one version may hold. */
function dimension_budget_version_max_lines()
{
	return 100000;
}

/** Largest number of changes one revision proposal may carry. */
function dimension_budget_version_max_changes()
{
	return 5000;
}

/** Largest absolute amount a line may carry. */
function dimension_budget_version_max_amount()
{
	return 1.0E+11;
}

/** Quote free text for the evidence tables without the page-oriented HTML encoding of `db_escape()`. */
function dimension_budget_version_quote($value)
{
	return "'".db_escape_function((string)$value)."'";
}

/** Record one stable reason code and its readable text on a result. */
function dimension_budget_version_fail(&$result, $code, $text)
{
	$result['ok'] = false;
	if (!in_array($code, $result['codes'], true))
		$result['codes'][] = $code;
	$result['errors'][] = $text;
	return $result;
}

/** A fresh result skeleton for a mutating or answering call (it starts ok; every refusal goes through dimension_budget_version_fail). */
function dimension_budget_version_result($extra = array())
{
	return array_merge(array('ok' => true, 'errors' => array(), 'codes' => array()), $extra);
}

/**
 * Are the three budget version tables installed in the active company?
 *
 * The answer is cached for the request (and the company prefix) because every report read asks it.
 *
 * @param bool $refresh Ask the catalog again
 * @return bool|string True, 'absent' or false when it cannot be answered
 */
function dimension_budget_version_schema_ready($refresh = false)
{
	static $cache = array();
	$key = TB_PREF;
	if (!$refresh && isset($cache[$key]))
		return $cache[$key];
	$rows = db_query("SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME IN ('"
		.TB_PREF."dimension_budget_versions','".TB_PREF."dimension_budget_version_lines','".TB_PREF."dimension_budget_activations')");
	$row = $rows ? db_fetch_row($rows) : false;
	if (!$row)
		return false;
	$cache[$key] = (int)$row[0] === 3 ? true : 'absent';
	return $cache[$key];
}

/** Forget the per-request verification cache (tests and long-running scripts). */
function dimension_budget_version_cache_reset()
{
	dimension_budget_version_verified(0, null);
	dimension_budget_version_schema_ready(true);
}

/**
 * Per-request memory of which versions were verified intact.
 *
 * @param int $version_id Version identity (0 with $state null clears the memory)
 * @param bool|null $state Store a verdict, or null to read it
 * @return bool|null The remembered verdict
 */
function dimension_budget_version_verified($version_id, $state = null)
{
	static $memory = array();
	if ((int)$version_id === 0 && $state === null) {
		$memory = array();
		dimension_budget_version_active_memory(false);
		return null;
	}
	$key = TB_PREF.(int)$version_id;
	if ($state !== null)
		$memory[$key] = (bool)$state;
	return isset($memory[$key]) ? $memory[$key] : null;
}

/**
 * Per-request memory of the active versions found for a date range (a report asks once per account).
 *
 * @param string|false $key Range key, or false to forget everything
 * @param array|null $value Value to store, or null to read
 * @return array|null The remembered list
 */
function dimension_budget_version_active_memory($key, $value = null)
{
	static $memory = array();
	if ($key === false) {
		$memory = array();
		return null;
	}
	$key = TB_PREF.$key;
	if ($value !== null)
		$memory[$key] = $value;
	return isset($memory[$key]) ? $memory[$key] : null;
}

/**
 * Parse a budget amount to six decimals (an explicit zero stays 0.0, never absent).
 *
 * @param mixed $value int, float or a canonical decimal string
 * @return float|false
 */
function dimension_budget_version_parse_amount($value)
{
	if (is_int($value) || is_float($value))
		$number = (float)$value;
	elseif (is_string($value) && preg_match('/^-?(0|[1-9][0-9]{0,11})(\.[0-9]{1,6})?$/D', $value))
		$number = (float)$value;
	else
		return false;
	if (!is_finite($number) || abs($number) > dimension_budget_version_max_amount())
		return false;
	$number = round($number, 6);
	return $number == 0 ? 0.0 : $number;
}

/** Canonical text of an amount, used by every digest. */
function dimension_budget_version_amount_text($amount)
{
	$amount = round((float)$amount, 6);
	return sprintf('%.6F', $amount == 0 ? 0.0 : $amount);
}

/** Parse a non-negative integer (an integer or a canonical digit string). */
function dimension_budget_version_parse_uint($value, $max)
{
	if (is_string($value) && preg_match('/^(0|[1-9][0-9]{0,15})$/D', $value))
		$value = (int)$value;
	if (!is_int($value) || $value < 0 || $value > $max)
		return false;
	return $value;
}

/** Identity key of one budget line (account, both coordinates and the exact date). */
function dimension_budget_version_line_key($line)
{
	return $line['account_code']."\t".(int)$line['coordinate_1']."\t".(int)$line['coordinate_2']."\t".$line['period_start'];
}

/**
 * Normalize one proposed change line.
 *
 * A change is `account`, `coordinate_1`, `coordinate_2`, `period_start` and an `amount`; an amount of null or an empty
 * string removes the line (it becomes absent), the number 0 sets an explicit zero.
 *
 * @param mixed $change Raw change
 * @param string $period_from Fiscal year begin
 * @param string $period_to Fiscal year end
 * @return array ok, code, text, line (account_code, coordinate_1, coordinate_2, period_start, amount|null)
 */
function dimension_budget_version_normalize_change($change, $period_from, $period_to)
{
	$bad = function ($code, $text) { return array('ok' => false, 'code' => $code, 'text' => $text, 'line' => null); };
	if (!is_array($change))
		return $bad('invalid_change', _('A budget change could not be read.'));
	foreach (array_keys($change) as $field)
		if (!in_array($field, array('account', 'coordinate_1', 'coordinate_2', 'period_start', 'amount'), true))
			return $bad('invalid_change', sprintf(_('The field "%s" is not part of a budget change.'), preg_replace('/[^A-Za-z0-9_]/', '', (string)$field)));
	$account = isset($change['account']) && is_string($change['account']) ? $change['account'] : '';
	if (!preg_match('/^[\x21-\x7e]{1,15}$/D', $account))
		return $bad('invalid_account', _('Each change needs a ledger account code of 1 to 15 plain characters.'));
	$coordinate_1 = dimension_budget_version_parse_uint(isset($change['coordinate_1']) ? $change['coordinate_1'] : 0, 2147483647);
	$coordinate_2 = dimension_budget_version_parse_uint(isset($change['coordinate_2']) ? $change['coordinate_2'] : 0, 2147483647);
	if ($coordinate_1 === false || $coordinate_2 === false)
		return $bad('invalid_coordinate', _('Each tracking value coordinate must be 0 (unassigned) or a tracking value number.'));
	$date = isset($change['period_start']) ? $change['period_start'] : '';
	if (!is_iso_calendar_date($date) || strcmp($date, $period_from) < 0 || strcmp($date, $period_to) > 0)
		return $bad('invalid_date', _('Each change needs a valid date inside the fiscal year of the version.'));
	$amount = null;
	if (isset($change['amount']) && $change['amount'] !== '' && $change['amount'] !== null) {
		$amount = dimension_budget_version_parse_amount($change['amount']);
		if ($amount === false)
			return $bad('invalid_amount', _('A budget amount must be a plain number with at most six decimals (0 is an explicit zero; leave it empty to remove the line).'));
	}
	return array('ok' => true, 'code' => '', 'text' => '', 'line' => array('account_code' => $account, 'coordinate_1' => $coordinate_1,
		'coordinate_2' => $coordinate_2, 'period_start' => $date, 'amount' => $amount));
}

/**
 * Content digest of a version: everything that decides a comparison, chained to its basis digest.
 *
 * @param array $version fiscal_year, version_kind, basis_sha256 (or null), period_from, period_to and the ordered `lines`
 * @return string 64 hex characters
 */
function dimension_budget_version_content_sha256($version)
{
	$lines = array();
	foreach ($version['lines'] as $line)
		$lines[] = array((string)$line['account_code'], (int)$line['coordinate_1'], (int)$line['coordinate_2'], (string)$line['period_start'],
			dimension_budget_version_amount_text($line['amount']), (int)$line['source_rows']);
	return hash('sha256', json_encode(array('bud2-version', (int)$version['fiscal_year'], (string)$version['version_kind'],
		$version['basis_sha256'] === null ? null : (string)$version['basis_sha256'], (string)$version['period_from'], (string)$version['period_to'], $lines)));
}

/** Sort lines into the canonical order (account, coordinate 1, coordinate 2, date). */
function dimension_budget_version_sort_lines($lines)
{
	usort($lines, function ($left, $right) {
		return strcmp(dimension_budget_version_line_key($left), dimension_budget_version_line_key($right));
	});
	return $lines;
}

/** Index a list of lines by their identity key. */
function dimension_budget_version_index($lines)
{
	$index = array();
	foreach ($lines as $line)
		$index[dimension_budget_version_line_key($line)] = $line;
	return $index;
}

/**
 * Read one fiscal year.
 *
 * @param int $fiscal_year Fiscal year identity
 * @param bool $lock Lock the row for the owning transaction (serializes every mutation of the year)
 * @return array|false begin, end (SQL dates) and closed, or false
 */
function dimension_budget_version_year($fiscal_year, $lock = false)
{
	$rows = db_query('SELECT id, `begin` AS period_from, `end` AS period_to, closed FROM '.TB_PREF.'fiscal_year WHERE id='.(int)$fiscal_year
		.($lock ? ' FOR UPDATE' : ''));
	$row = $rows ? db_fetch_assoc($rows) : false;
	return $row ? $row : false;
}

/**
 * Load one version with its lines.
 *
 * @param int $version_id Version identity
 * @param bool $lock Lock the rows for the owning transaction
 * @param bool $with_lines Read the lines as well
 * @return array|false The version row with `lines` (list), or false
 */
function dimension_budget_version_load($version_id, $lock = false, $with_lines = true)
{
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_budget_versions WHERE version_id='.(int)$version_id.($lock ? ' FOR UPDATE' : ''));
	$version = $rows ? db_fetch_assoc($rows) : false;
	if (!$version)
		return false;
	$version['lines'] = array();
	if (!$with_lines)
		return $version;
	$rows = db_query('SELECT account_code, coordinate_1, coordinate_2, period_start, amount, source_rows FROM '.TB_PREF
		.'dimension_budget_version_lines WHERE version_id='.(int)$version_id
		.' ORDER BY account_code, coordinate_1, coordinate_2, period_start'.($lock ? ' FOR UPDATE' : ''));
	if ($rows === false)
		return false;
	while ($row = db_fetch_assoc($rows))
		$version['lines'][] = array('account_code' => (string)$row['account_code'], 'coordinate_1' => (int)$row['coordinate_1'],
			'coordinate_2' => (int)$row['coordinate_2'], 'period_start' => (string)$row['period_start'],
			'amount' => round((float)$row['amount'], 6), 'source_rows' => (int)$row['source_rows']);
	return $version;
}

/** Does the stored content of a loaded version still match its digest and counts? */
function dimension_budget_version_intact($version)
{
	if (!is_array($version) || !isset($version['lines']) || (int)$version['line_count'] !== count($version['lines']))
		return false;
	$total = 0.0;
	foreach ($version['lines'] as $line)
		$total += $line['amount'];
	if (abs($total - (float)$version['total_amount']) > 0.0001 + abs($total) * 1.0E-9)
		return false;
	$basis = $version['basis_sha256'] === null || $version['basis_sha256'] === '' ? null : $version['basis_sha256'];
	$check = array('fiscal_year' => $version['fiscal_year'], 'version_kind' => $version['version_kind'], 'basis_sha256' => $basis,
		'period_from' => $version['period_from'], 'period_to' => $version['period_to'], 'lines' => $version['lines']);
	return hash_equals((string)$version['content_sha256'], dimension_budget_version_content_sha256($check));
}

/**
 * The latest approved version of a fiscal year (the only valid basis of the next revision).
 *
 * @param int $fiscal_year Fiscal year identity
 * @param bool $lock Lock the row
 * @return array|false Version row without lines, or false when none is approved
 */
function dimension_budget_version_latest_approved($fiscal_year, $lock = false)
{
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_budget_versions WHERE fiscal_year='.(int)$fiscal_year." AND status='approved' ORDER BY version_no DESC LIMIT 1"
		.($lock ? ' FOR UPDATE' : ''));
	$row = $rows ? db_fetch_assoc($rows) : false;
	return $row ? $row : false;
}

/**
 * Insert one version and its lines inside the caller's transaction.
 *
 * @param int $fiscal_year Fiscal year identity
 * @param string $kind baseline|revision
 * @param array|null $basis Basis version row (revisions)
 * @param array $year Fiscal year row
 * @param array $lines Lines (account_code, coordinate_1, coordinate_2, period_start, amount, source_rows)
 * @param string $commentary Commentary text
 * @param int $actor Maker
 * @return int|false The new version identity
 */
function dimension_budget_version_store($fiscal_year, $kind, $basis, $year, $lines, $commentary, $actor)
{
	$rows = db_query('SELECT MAX(version_no) FROM '.TB_PREF.'dimension_budget_versions WHERE fiscal_year='.(int)$fiscal_year);
	$row = $rows ? db_fetch_row($rows) : false;
	if (!$row)
		return false;
	$next = (int)$row[0] + 1;
	$lines = dimension_budget_version_sort_lines($lines);
	$basis_sha = $basis === null ? null : $basis['content_sha256'];
	$sha = dimension_budget_version_content_sha256(array('fiscal_year' => $fiscal_year, 'version_kind' => $kind, 'basis_sha256' => $basis_sha,
		'period_from' => $year['period_from'], 'period_to' => $year['period_to'], 'lines' => $lines));
	$total = 0.0;
	foreach ($lines as $line)
		$total += $line['amount'];
	if (!db_query('INSERT INTO '.TB_PREF.'dimension_budget_versions (fiscal_year, version_no, version_kind, basis_version_id, basis_sha256, status,'
		.' period_from, period_to, line_count, total_amount, content_sha256, commentary, maker_id, created_at) VALUES ('.(int)$fiscal_year.','.$next.','
		.dimension_budget_version_quote($kind).','.($basis === null ? 'NULL' : (int)$basis['version_id']).','
		.($basis_sha === null ? 'NULL' : dimension_budget_version_quote($basis_sha)).",'pending',".dimension_budget_version_quote($year['period_from']).','
		.dimension_budget_version_quote($year['period_to']).','.count($lines).','.dimension_budget_version_amount_text($total).','
		.dimension_budget_version_quote($sha).','.dimension_budget_version_quote($commentary).','.(int)$actor.',NOW())'))
		return false;
	$version_id = (int)db_insert_id();
	if ($version_id <= 0)
		return false;
	foreach (array_chunk($lines, 400) as $chunk) {
		$values = array();
		foreach ($chunk as $line)
			$values[] = '('.$version_id.','.dimension_budget_version_quote($line['account_code']).','.(int)$line['coordinate_1'].','.(int)$line['coordinate_2'].','
				.dimension_budget_version_quote($line['period_start']).','.dimension_budget_version_amount_text($line['amount']).','.(int)$line['source_rows'].')';
		if (count($values) && !db_query('INSERT INTO '.TB_PREF.'dimension_budget_version_lines (version_id, account_code, coordinate_1, coordinate_2,'
			.' period_start, amount, source_rows) VALUES '.implode(',', $values)))
			return false;
	}
	return $version_id;
}

/** Validate commentary text (1 to 500 plain characters when required). */
function dimension_budget_version_check_commentary($text, $required, &$result)
{
	$text = is_string($text) ? trim($text) : '';
	if ($required && $text === '')
		dimension_budget_version_fail($result, 'commentary_required', _('Explain why the budget changes; commentary is required.'));
	elseif (strlen($text) > 500)
		dimension_budget_version_fail($result, 'commentary_too_long', _('The commentary may not be longer than 500 characters.'));
	return $text;
}

/**
 * Capture the exact baseline of a fiscal year from the live budget (the maker step).
 *
 * The baseline is the plan as it stands at capture time: a revision overwritten earlier in the live budget is not recoverable
 * and is never reconstructed. One open or approved baseline per fiscal year is allowed.
 *
 * @param int $fiscal_year Fiscal year identity
 * @param string $commentary Optional commentary
 * @return array ok, version_id, version_no, line_count, errors, codes
 */
function dimension_budget_version_capture_baseline($fiscal_year, $commentary = '')
{
	$result = dimension_budget_version_result(array('version_id' => 0, 'version_no' => 0, 'line_count' => 0));
	if (!current_user_has_access('SA_BUDGETENTRY'))
		return dimension_budget_version_fail($result, 'permission_denied', _('You are not allowed to capture budget baselines.'));
	$actor = current_user_id_or_zero();
	if ($actor <= 0)
		return dimension_budget_version_fail($result, 'permission_denied', _('The proposing user could not be identified.'));
	$commentary = dimension_budget_version_check_commentary($commentary, false, $result);
	if (!$result['ok'])
		return $result;
	if (dimension_budget_version_schema_ready() !== true)
		return dimension_budget_version_fail($result, 'schema_missing', _('The budget version tables are not installed. Upgrade the company database first.'));
	if (!begin_transaction())
		return dimension_budget_version_fail($result, 'unavailable', _('The baseline could not start a transaction.'));
	$year = dimension_budget_version_year($fiscal_year, true);
	if (!$year) {
		cancel_transaction();
		return dimension_budget_version_fail($result, 'fiscal_year_missing', _('The fiscal year does not exist.'));
	}
	$rows = db_query('SELECT version_id, version_kind, status FROM '.TB_PREF.'dimension_budget_versions WHERE fiscal_year='.(int)$fiscal_year
		." AND version_kind='baseline' AND status IN ('pending','approved') FOR UPDATE");
	if ($rows === false || db_num_rows($rows) > 0) {
		cancel_transaction();
		return dimension_budget_version_fail($result, $rows === false ? 'unavailable' : 'baseline_exists',
			$rows === false ? _('The existing versions could not be read.') : _('This fiscal year already has an open or approved baseline.'));
	}
	$source = db_query('SELECT account, COALESCE(dimension_id,0) AS c1, COALESCE(dimension2_id,0) AS c2, tran_date, SUM(amount) AS amount, COUNT(*) AS source_rows FROM '
		.TB_PREF.'budget_trans WHERE tran_date>='.dimension_budget_version_quote($year['period_from']).' AND tran_date<='.dimension_budget_version_quote($year['period_to'])
		.' GROUP BY account, c1, c2, tran_date ORDER BY account, c1, c2, tran_date');
	if ($source === false) {
		cancel_transaction();
		return dimension_budget_version_fail($result, 'unavailable', _('The live budget could not be read.'));
	}
	$lines = array();
	while ($row = db_fetch_assoc($source)) {
		$amount = dimension_budget_version_parse_amount((float)$row['amount']);
		if (!preg_match('/^[\x21-\x7e]{1,15}$/D', (string)$row['account']) || $amount === false || !is_iso_calendar_date((string)$row['tran_date'])) {
			cancel_transaction();
			return dimension_budget_version_fail($result, 'unsupported_budget_row', sprintf(_('The live budget row of account "%s" cannot be versioned; correct it first.'),
				preg_replace('/[^\x20-\x7e]/', '?', (string)$row['account'])));
		}
		$lines[] = array('account_code' => (string)$row['account'], 'coordinate_1' => (int)$row['c1'], 'coordinate_2' => (int)$row['c2'],
			'period_start' => (string)$row['tran_date'], 'amount' => $amount, 'source_rows' => (int)$row['source_rows']);
		if (count($lines) > dimension_budget_version_max_lines()) {
			cancel_transaction();
			return dimension_budget_version_fail($result, 'too_many_lines', _('The fiscal year has more budget lines than one version may hold.'));
		}
	}
	if (!count($lines)) {
		cancel_transaction();
		return dimension_budget_version_fail($result, 'no_budget', _('The live budget of this fiscal year has no rows to capture.'));
	}
	$version_id = dimension_budget_version_store($fiscal_year, 'baseline', null, $year, $lines, $commentary, $actor);
	if ($version_id === false || !commit_transaction()) {
		cancel_transaction();
		return dimension_budget_version_fail($result, 'unavailable', _('The baseline could not be recorded.'));
	}
	$loaded = dimension_budget_version_load($version_id, false, false);
	$result['ok'] = true;
	$result['version_id'] = $version_id;
	$result['version_no'] = $loaded ? (int)$loaded['version_no'] : 0;
	$result['line_count'] = count($lines);
	return $result;
}

/**
 * Apply normalized changes over the lines of a basis version.
 *
 * @param array $basis_lines Basis lines keyed by identity
 * @param array $changes Normalized change lines (amount null removes)
 * @return array ok, errors, codes, lines (list), changed (int)
 */
function dimension_budget_version_apply_changes($basis_lines, $changes)
{
	$result = dimension_budget_version_result(array('ok' => true, 'lines' => array(), 'changed' => 0, 'added_keys' => array()));
	$lines = $basis_lines;
	$touched = array();
	foreach ($changes as $change) {
		$key = dimension_budget_version_line_key($change);
		if (isset($touched[$key])) {
			dimension_budget_version_fail($result, 'duplicate_change', sprintf(_('The line of account %s dated %s is changed more than once.'), $change['account_code'], $change['period_start']));
			continue;
		}
		$touched[$key] = true;
		if ($change['amount'] === null) {
			if (!isset($lines[$key])) {
				dimension_budget_version_fail($result, 'remove_absent', sprintf(_('The line of account %s dated %s does not exist, so it cannot be removed.'), $change['account_code'], $change['period_start']));
				continue;
			}
			unset($lines[$key]);
			$result['changed']++;
			continue;
		}
		if (!isset($lines[$key]))
			$result['added_keys'][] = $key;
		elseif (dimension_budget_version_amount_text($lines[$key]['amount']) === dimension_budget_version_amount_text($change['amount']))
			continue;
		$lines[$key] = array('account_code' => $change['account_code'], 'coordinate_1' => $change['coordinate_1'], 'coordinate_2' => $change['coordinate_2'],
			'period_start' => $change['period_start'], 'amount' => $change['amount'], 'source_rows' => isset($lines[$key]) ? $lines[$key]['source_rows'] : 1);
		$result['changed']++;
	}
	if (!$result['ok'])
		return $result;
	$result['lines'] = array_values($lines);
	return $result;
}

/**
 * Check that the accounts and Dimensions of newly introduced lines exist and fit their category.
 *
 * @param array $changes Normalized changes that create a new line
 * @param array $result Result to record failures on
 * @return void
 */
function dimension_budget_version_check_references($changes, &$result)
{
	$accounts = array();
	$dimensions = array();
	foreach ($changes as $change) {
		$accounts[$change['account_code']] = true;
		if ($change['coordinate_1'] > 0)
			$dimensions[$change['coordinate_1']] = 1;
		if ($change['coordinate_2'] > 0)
			$dimensions[$change['coordinate_2']] = isset($dimensions[$change['coordinate_2']]) && $dimensions[$change['coordinate_2']] === 1 ? -1 : 2;
	}
	if (count($accounts)) {
		$list = array();
		foreach (array_keys($accounts) as $account)
			$list[] = dimension_budget_version_quote($account);
		$rows = db_query('SELECT account_code FROM '.TB_PREF.'chart_master WHERE account_code IN ('.implode(',', $list).')');
		$found = array();
		while ($rows && ($row = db_fetch_row($rows)))
			$found[strtolower((string)$row[0])] = true;
		foreach (array_keys($accounts) as $account)
			if (!isset($found[strtolower($account)]))
				dimension_budget_version_fail($result, 'account_missing', sprintf(_('The ledger account %s does not exist.'), $account));
	}
	if (count($dimensions)) {
		$rows = db_query('SELECT id, type_ FROM '.TB_PREF.'dimensions WHERE id IN ('.implode(',', array_map('intval', array_keys($dimensions))).')');
		$types = array();
		while ($rows && ($row = db_fetch_assoc($rows)))
			$types[(int)$row['id']] = (int)$row['type_'];
		foreach ($changes as $change)
			foreach (array(1 => 'coordinate_1', 2 => 'coordinate_2') as $axis => $field)
				if ($change[$field] > 0) {
					if (!isset($types[$change[$field]]))
						dimension_budget_version_fail($result, 'dimension_missing', sprintf(_('Tracking value #%d does not exist.'), $change[$field]));
					elseif ($types[$change[$field]] !== $axis)
						dimension_budget_version_fail($result, 'dimension_axis', sprintf(_('Tracking value #%d does not belong to tracking category %d.'), $change[$field], $axis));
				}
	}
}

/**
 * Propose a revision over the current approved version of a fiscal year (the maker step).
 *
 * @param int $fiscal_year Fiscal year identity
 * @param int $basis_version_id The approved version the revision builds on (must still be the latest approved one)
 * @param array $changes Raw changes (see dimension_budget_version_normalize_change)
 * @param string $commentary Required commentary
 * @return array ok, duplicate, version_id, version_no, changed, errors, codes
 */
function dimension_budget_version_propose_revision($fiscal_year, $basis_version_id, $changes, $commentary)
{
	$result = dimension_budget_version_result(array('duplicate' => false, 'version_id' => 0, 'version_no' => 0, 'changed' => 0));
	if (!current_user_has_access('SA_BUDGETENTRY'))
		return dimension_budget_version_fail($result, 'permission_denied', _('You are not allowed to propose budget revisions.'));
	$actor = current_user_id_or_zero();
	if ($actor <= 0)
		return dimension_budget_version_fail($result, 'permission_denied', _('The proposing user could not be identified.'));
	$commentary = dimension_budget_version_check_commentary($commentary, true, $result);
	if (!is_array($changes) || !count($changes))
		dimension_budget_version_fail($result, 'no_changes', _('A revision needs at least one change.'));
	elseif (count($changes) > dimension_budget_version_max_changes())
		dimension_budget_version_fail($result, 'too_many_changes', sprintf(_('A revision may carry at most %d changes.'), dimension_budget_version_max_changes()));
	if (!$result['ok'])
		return $result;
	if (dimension_budget_version_schema_ready() !== true)
		return dimension_budget_version_fail($result, 'schema_missing', _('The budget version tables are not installed. Upgrade the company database first.'));
	if (!begin_transaction())
		return dimension_budget_version_fail($result, 'unavailable', _('The revision could not start a transaction.'));
	$year = dimension_budget_version_year($fiscal_year, true);
	if (!$year) {
		cancel_transaction();
		return dimension_budget_version_fail($result, 'fiscal_year_missing', _('The fiscal year does not exist.'));
	}
	$basis = dimension_budget_version_load($basis_version_id, true);
	$latest = dimension_budget_version_latest_approved($fiscal_year);
	if (!$basis || (int)$basis['fiscal_year'] !== (int)$fiscal_year || $basis['status'] !== 'approved') {
		cancel_transaction();
		return dimension_budget_version_fail($result, 'basis_invalid', _('A revision must build on an approved version of the same fiscal year.'));
	}
	if (!$latest || (int)$latest['version_id'] !== (int)$basis['version_id']) {
		cancel_transaction();
		return dimension_budget_version_fail($result, 'stale_basis', _('A newer version has been approved since; build the revision on the latest approved version.'));
	}
	if (!dimension_budget_version_intact($basis)) {
		cancel_transaction();
		return dimension_budget_version_fail($result, 'version_tampered', _('The stored basis version no longer matches its digest and cannot be revised.'));
	}
	$normal = array();
	foreach ($changes as $change) {
		$checked = dimension_budget_version_normalize_change($change, $basis['period_from'], $basis['period_to']);
		if (!$checked['ok']) {
			dimension_budget_version_fail($result, $checked['code'], $checked['text']);
			continue;
		}
		$normal[] = $checked['line'];
	}
	if (!$result['ok']) {
		cancel_transaction();
		return $result;
	}
	$index = dimension_budget_version_index($basis['lines']);
	$applied = dimension_budget_version_apply_changes($index, $normal);
	if (!$applied['ok']) {
		cancel_transaction();
		return array_merge($result, array('ok' => false, 'errors' => $applied['errors'], 'codes' => $applied['codes']));
	}
	$new_lines = array();
	foreach ($normal as $change)
		if ($change['amount'] !== null && in_array(dimension_budget_version_line_key($change), $applied['added_keys'], true))
			$new_lines[] = $change;
	dimension_budget_version_check_references($new_lines, $result);
	if (!$result['ok']) {
		cancel_transaction();
		return $result;
	}
	if ($applied['changed'] === 0) {
		cancel_transaction();
		return dimension_budget_version_fail($result, 'no_change', _('The proposal does not change any line of its basis.'));
	}
	if (count($applied['lines']) > dimension_budget_version_max_lines()) {
		cancel_transaction();
		return dimension_budget_version_fail($result, 'too_many_lines', _('The revision would hold more budget lines than one version may.'));
	}
	$sorted = dimension_budget_version_sort_lines($applied['lines']);
	$sha = dimension_budget_version_content_sha256(array('fiscal_year' => $fiscal_year, 'version_kind' => 'revision', 'basis_sha256' => $basis['content_sha256'],
		'period_from' => $year['period_from'], 'period_to' => $year['period_to'], 'lines' => $sorted));
	$existing = db_query('SELECT version_id, version_no FROM '.TB_PREF.'dimension_budget_versions WHERE fiscal_year='.(int)$fiscal_year.' AND content_sha256='
		.dimension_budget_version_quote($sha)." AND status IN ('pending','approved') LIMIT 1 FOR UPDATE");
	$found = $existing ? db_fetch_assoc($existing) : false;
	if ($found) {
		cancel_transaction();
		$result['ok'] = true;
		$result['duplicate'] = true;
		$result['version_id'] = (int)$found['version_id'];
		$result['version_no'] = (int)$found['version_no'];
		return $result;
	}
	$version_id = dimension_budget_version_store($fiscal_year, 'revision', $basis, $year, $sorted, $commentary, $actor);
	if ($version_id === false || !commit_transaction()) {
		cancel_transaction();
		return dimension_budget_version_fail($result, 'unavailable', _('The revision could not be recorded.'));
	}
	$loaded = dimension_budget_version_load($version_id, false, false);
	$result['ok'] = true;
	$result['version_id'] = $version_id;
	$result['version_no'] = $loaded ? (int)$loaded['version_no'] : 0;
	$result['changed'] = $applied['changed'];
	return $result;
}

/**
 * Approve or reject a proposed version (the independent checker step).
 *
 * Approval does not change the active comparison; that is a separate, explicit and audited choice.
 *
 * @param int $version_id Proposed version
 * @param string $decision approve|reject
 * @param string $note Optional decision note
 * @return array ok, status, errors, codes
 */
function dimension_budget_version_decide($version_id, $decision, $note = '')
{
	$result = dimension_budget_version_result(array('status' => ''));
	if (!current_user_has_access('SA_BUDGETENTRY'))
		return dimension_budget_version_fail($result, 'permission_denied', _('You are not allowed to decide budget versions.'));
	if (!in_array($decision, array('approve', 'reject'), true))
		return dimension_budget_version_fail($result, 'invalid_decision', _('The decision must be approve or reject.'));
	$actor = current_user_id_or_zero();
	if ($actor <= 0)
		return dimension_budget_version_fail($result, 'permission_denied', _('The deciding user could not be identified.'));
	$note = is_string($note) ? trim($note) : '';
	if (strlen($note) > 200)
		return dimension_budget_version_fail($result, 'note_too_long', _('The decision note may not be longer than 200 characters.'));
	if (dimension_budget_version_schema_ready() !== true)
		return dimension_budget_version_fail($result, 'schema_missing', _('The budget version tables are not installed. Upgrade the company database first.'));
	if (!begin_transaction())
		return dimension_budget_version_fail($result, 'unavailable', _('The decision could not start a transaction.'));
	$peek = dimension_budget_version_load($version_id, false, false);
	if (!$peek || !dimension_budget_version_year($peek['fiscal_year'], true)) {
		cancel_transaction();
		return dimension_budget_version_fail($result, 'not_pending', _('Only a proposed budget version can be decided.'));
	}
	$version = dimension_budget_version_load($version_id, true);
	if (!$version || $version['status'] !== 'pending') {
		cancel_transaction();
		return dimension_budget_version_fail($result, 'not_pending', _('Only a proposed budget version can be decided.'));
	}
	if ((int)$version['maker_id'] === $actor) {
		cancel_transaction();
		return dimension_budget_version_fail($result, 'maker_checker', _('The proposing user cannot decide the same budget version; ask an independent checker.'));
	}
	$status = $decision === 'approve' ? 'approved' : 'rejected';
	if ($decision === 'approve') {
		if (!dimension_budget_version_intact($version)) {
			cancel_transaction();
			return dimension_budget_version_fail($result, 'version_tampered', _('The stored version no longer matches its digest and cannot be approved.'));
		}
		if ($version['version_kind'] === 'revision') {
			$latest = dimension_budget_version_latest_approved($version['fiscal_year'], true);
			$basis = dimension_budget_version_load($version['basis_version_id'], true);
			if (!$latest || !$basis || (int)$latest['version_id'] !== (int)$version['basis_version_id'] || $basis['status'] !== 'approved') {
				cancel_transaction();
				return dimension_budget_version_fail($result, 'stale_basis', _('A newer version has been approved since this revision was proposed; propose it again on the latest approved version.'));
			}
			if ($basis['content_sha256'] !== $version['basis_sha256'] || !dimension_budget_version_intact($basis)) {
				cancel_transaction();
				return dimension_budget_version_fail($result, 'version_tampered', _('The basis of this revision no longer matches its digest and the revision cannot be approved.'));
			}
		} elseif (dimension_budget_version_latest_approved($version['fiscal_year'], true)) {
			cancel_transaction();
			return dimension_budget_version_fail($result, 'baseline_exists', _('This fiscal year already has an approved version; a baseline cannot follow it.'));
		}
	}
	if (db_query('UPDATE '.TB_PREF.'dimension_budget_versions SET status='.dimension_budget_version_quote($status).', checker_id='.$actor.', decision_note='
		.dimension_budget_version_quote($note).', decided_at=NOW() WHERE version_id='.(int)$version_id." AND status='pending'") === false || !commit_transaction()) {
		cancel_transaction();
		return dimension_budget_version_fail($result, 'unavailable', _('The decision could not be recorded.'));
	}
	dimension_budget_version_verified(0, null);
	$result['ok'] = true;
	$result['status'] = $status;
	return $result;
}

/**
 * The active comparison of a fiscal year.
 *
 * @param int $fiscal_year Fiscal year identity
 * @param bool $lock Lock the newest activation row
 * @return int|null|false The active version identity, null for the legacy live budget, false when unreadable
 */
function dimension_budget_version_active($fiscal_year, $lock = false)
{
	$rows = db_query('SELECT version_id FROM '.TB_PREF.'dimension_budget_activations WHERE fiscal_year='.(int)$fiscal_year
		.' ORDER BY activation_id DESC LIMIT 1'.($lock ? ' FOR UPDATE' : ''));
	if ($rows === false)
		return false;
	$row = db_fetch_row($rows);
	return $row && $row[0] !== null ? (int)$row[0] : null;
}

/**
 * Choose the comparison the reports use for a fiscal year (an explicit, audited decision).
 *
 * @param int $fiscal_year Fiscal year identity
 * @param int|null $version_id An approved version of the year, or 0/null for the legacy live budget
 * @param string $reason Required reason (1 to 200 characters)
 * @return array ok, duplicate, version_id (0 = legacy), errors, codes
 */
function dimension_budget_version_activate($fiscal_year, $version_id, $reason)
{
	$result = dimension_budget_version_result(array('duplicate' => false, 'version_id' => 0));
	if (!current_user_has_access('SA_BUDGETENTRY'))
		return dimension_budget_version_fail($result, 'permission_denied', _('You are not allowed to change the active budget comparison.'));
	$actor = current_user_id_or_zero();
	if ($actor <= 0)
		return dimension_budget_version_fail($result, 'permission_denied', _('The acting user could not be identified.'));
	$reason = is_string($reason) ? trim($reason) : '';
	if ($reason === '' || strlen($reason) > 200)
		return dimension_budget_version_fail($result, 'reason_required', _('Give a reason of 1 to 200 characters for the active comparison.'));
	$version_id = $version_id === null || $version_id === '' ? 0 : dimension_budget_version_parse_uint($version_id, 9007199254740991);
	if ($version_id === false)
		return dimension_budget_version_fail($result, 'invalid_version', _('The version could not be read.'));
	if (dimension_budget_version_schema_ready() !== true)
		return dimension_budget_version_fail($result, 'schema_missing', _('The budget version tables are not installed. Upgrade the company database first.'));
	if (!begin_transaction())
		return dimension_budget_version_fail($result, 'unavailable', _('The activation could not start a transaction.'));
	if (!dimension_budget_version_year($fiscal_year, true)) {
		cancel_transaction();
		return dimension_budget_version_fail($result, 'fiscal_year_missing', _('The fiscal year does not exist.'));
	}
	if ($version_id > 0) {
		$version = dimension_budget_version_load($version_id, true);
		if (!$version || (int)$version['fiscal_year'] !== (int)$fiscal_year || $version['status'] !== 'approved') {
			cancel_transaction();
			return dimension_budget_version_fail($result, 'version_not_approved', _('Only an approved version of this fiscal year can become the active comparison.'));
		}
		if (!dimension_budget_version_intact($version)) {
			cancel_transaction();
			return dimension_budget_version_fail($result, 'version_tampered', _('The stored version no longer matches its digest and cannot become the active comparison.'));
		}
	}
	$current = dimension_budget_version_active($fiscal_year, true);
	if ($current === false) {
		cancel_transaction();
		return dimension_budget_version_fail($result, 'unavailable', _('The current activation could not be read.'));
	}
	if ((int)$current === $version_id) {
		cancel_transaction();
		$result['ok'] = true;
		$result['duplicate'] = true;
		$result['version_id'] = $version_id;
		return $result;
	}
	if (!db_query('INSERT INTO '.TB_PREF.'dimension_budget_activations (fiscal_year, version_id, reason, actor_id, created_at) VALUES ('.(int)$fiscal_year.','
		.($version_id > 0 ? $version_id : 'NULL').','.dimension_budget_version_quote($reason).','.$actor.',NOW())') || !commit_transaction()) {
		cancel_transaction();
		return dimension_budget_version_fail($result, 'unavailable', _('The activation could not be recorded.'));
	}
	dimension_budget_version_verified(0, null);
	$result['ok'] = true;
	$result['version_id'] = $version_id;
	return $result;
}

/**
 * Versions of a fiscal year (or all years), newest first, without their lines.
 *
 * @param int $fiscal_year Fiscal year identity (0 = every year)
 * @param int $limit Maximum rows
 * @return array|false
 */
function dimension_budget_version_recent($fiscal_year = 0, $limit = 50)
{
	if (!current_user_has_access('SA_BUDGETENTRY') || dimension_budget_version_schema_ready() !== true)
		return false;
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_budget_versions'.($fiscal_year ? ' WHERE fiscal_year='.(int)$fiscal_year : '')
		.' ORDER BY fiscal_year DESC, version_no DESC LIMIT '.(int)$limit);
	if ($rows === false)
		return false;
	$versions = array();
	while ($row = db_fetch_assoc($rows))
		$versions[] = $row;
	return $versions;
}

/**
 * The append-only activation history of a fiscal year, newest first.
 *
 * @param int $fiscal_year Fiscal year identity (0 = every year)
 * @param int $limit Maximum rows
 * @return array|false
 */
function dimension_budget_version_activations($fiscal_year = 0, $limit = 50)
{
	if (!current_user_has_access('SA_BUDGETENTRY') || dimension_budget_version_schema_ready() !== true)
		return false;
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_budget_activations'.($fiscal_year ? ' WHERE fiscal_year='.(int)$fiscal_year : '')
		.' ORDER BY activation_id DESC LIMIT '.(int)$limit);
	if ($rows === false)
		return false;
	$history = array();
	while ($row = db_fetch_assoc($rows))
		$history[] = $row;
	return $history;
}

/**
 * Compare two versions of a fiscal year, or a version against the legacy live budget.
 *
 * Every line is classified `same`, `changed`, `added` (only on the right), `removed` (only on the left) or flagged as a
 * zero-versus-absent difference when one side holds an explicit zero and the other no line at all.
 *
 * @param int $left_id The earlier version
 * @param int $right_id The later version, or 0 for the live `budget_trans` of the left version's fiscal year
 * @param int $limit Maximum number of difference rows returned (counts are always complete)
 * @return array ok, counts (same, changed, added, removed, zero_vs_absent), rows, truncated, errors, codes
 */
function dimension_budget_version_compare($left_id, $right_id, $limit = 500)
{
	$result = dimension_budget_version_result(array('counts' => array('same' => 0, 'changed' => 0, 'added' => 0, 'removed' => 0, 'zero_vs_absent' => 0),
		'rows' => array(), 'truncated' => false));
	if (!current_user_has_access('SA_BUDGETENTRY'))
		return dimension_budget_version_fail($result, 'permission_denied', _('You are not allowed to compare budget versions.'));
	if (dimension_budget_version_schema_ready() !== true)
		return dimension_budget_version_fail($result, 'schema_missing', _('The budget version tables are not installed. Upgrade the company database first.'));
	$left = dimension_budget_version_load($left_id);
	if (!$left || !dimension_budget_version_intact($left))
		return dimension_budget_version_fail($result, 'version_unavailable', _('The first version does not exist or no longer matches its digest.'));
	if ((int)$right_id > 0) {
		$right = dimension_budget_version_load($right_id);
		if (!$right || (int)$right['fiscal_year'] !== (int)$left['fiscal_year'] || !dimension_budget_version_intact($right))
			return dimension_budget_version_fail($result, 'version_unavailable', _('The second version does not exist, belongs to another fiscal year or no longer matches its digest.'));
		$right_lines = $right['lines'];
	} else {
		$source = db_query('SELECT account, COALESCE(dimension_id,0) AS c1, COALESCE(dimension2_id,0) AS c2, tran_date, SUM(amount) AS amount, COUNT(*) AS source_rows FROM '
			.TB_PREF.'budget_trans WHERE tran_date>='.dimension_budget_version_quote($left['period_from']).' AND tran_date<='.dimension_budget_version_quote($left['period_to'])
			.' GROUP BY account, c1, c2, tran_date');
		if ($source === false)
			return dimension_budget_version_fail($result, 'unavailable', _('The live budget could not be read.'));
		$right_lines = array();
		while ($row = db_fetch_assoc($source))
			$right_lines[] = array('account_code' => (string)$row['account'], 'coordinate_1' => (int)$row['c1'], 'coordinate_2' => (int)$row['c2'],
				'period_start' => (string)$row['tran_date'], 'amount' => round((float)$row['amount'], 6), 'source_rows' => (int)$row['source_rows']);
	}
	$left_index = dimension_budget_version_index($left['lines']);
	$right_index = dimension_budget_version_index($right_lines);
	$keys = array_unique(array_merge(array_keys($left_index), array_keys($right_index)));
	sort($keys, SORT_STRING);
	foreach ($keys as $key) {
		$a = isset($left_index[$key]) ? $left_index[$key] : null;
		$b = isset($right_index[$key]) ? $right_index[$key] : null;
		if ($a !== null && $b !== null) {
			if (dimension_budget_version_amount_text($a['amount']) === dimension_budget_version_amount_text($b['amount'])) {
				$result['counts']['same']++;
				continue;
			}
			$state = 'changed';
		} else
			$state = $a === null ? 'added' : 'removed';
		$result['counts'][$state]++;
		$line = $a !== null ? $a : $b;
		$zero_absent = ($a === null && $b !== null && $b['amount'] == 0) || ($b === null && $a !== null && $a['amount'] == 0);
		if ($zero_absent)
			$result['counts']['zero_vs_absent']++;
		if (count($result['rows']) < (int)$limit)
			$result['rows'][] = array('state' => $state, 'account_code' => $line['account_code'], 'coordinate_1' => $line['coordinate_1'],
				'coordinate_2' => $line['coordinate_2'], 'period_start' => $line['period_start'], 'left' => $a === null ? null : $a['amount'],
				'right' => $b === null ? null : $b['amount'], 'zero_vs_absent' => $zero_absent);
		else
			$result['truncated'] = true;
	}
	$result['ok'] = true;
	return $result;
}

/**
 * Active versions whose fiscal year touches a date range.
 *
 * @param string $first SQL date
 * @param string $last SQL date
 * @return array|false List of fiscal_year, version_id, period_from, period_to; false when unreadable
 */
function dimension_budget_version_active_in_range($first, $last)
{
	$ready = dimension_budget_version_schema_ready();
	if ($ready !== true)
		return $ready === 'absent' ? array() : false;
	$remembered = dimension_budget_version_active_memory($first.'|'.$last);
	if ($remembered !== null)
		return $remembered;
	$rows = db_query('SELECT a.fiscal_year, a.version_id, f.`begin` AS period_from, f.`end` AS period_to FROM '.TB_PREF.'dimension_budget_activations a INNER JOIN '
		.TB_PREF.'fiscal_year f ON f.id=a.fiscal_year WHERE a.version_id IS NOT NULL AND a.activation_id=(SELECT MAX(x.activation_id) FROM '
		.TB_PREF.'dimension_budget_activations x WHERE x.fiscal_year=a.fiscal_year) AND f.`begin`<='.dimension_budget_version_quote($last)
		.' AND f.`end`>='.dimension_budget_version_quote($first).' ORDER BY a.fiscal_year');
	if ($rows === false)
		return false;
	$active = array();
	while ($row = db_fetch_assoc($rows))
		$active[] = array('fiscal_year' => (int)$row['fiscal_year'], 'version_id' => (int)$row['version_id'],
			'period_from' => $row['period_from'], 'period_to' => $row['period_to']);
	dimension_budget_version_active_memory($first.'|'.$last, $active);
	return $active;
}

/**
 * Is an active version approved and byte-identical to its digest (verified once per request)?
 *
 * @param int $version_id Version identity
 * @param int $fiscal_year Fiscal year it must belong to
 * @return bool
 */
function dimension_budget_version_usable($version_id, $fiscal_year)
{
	$known = dimension_budget_version_verified($version_id);
	if ($known !== null)
		return $known;
	$version = dimension_budget_version_load($version_id);
	$ok = $version && (int)$version['fiscal_year'] === (int)$fiscal_year && $version['status'] === 'approved' && dimension_budget_version_intact($version);
	dimension_budget_version_verified($version_id, $ok);
	return (bool)$ok;
}

/**
 * Replace the legacy rows of every fiscal year that has an active version by that version's rows.
 *
 * Called by the BUD-001 reader. A fiscal year without an active version keeps its live rows exactly; an active version that
 * is not approved or does not match its digest makes the budget unavailable (false) instead of showing an altered figure.
 *
 * @param array $rows Legacy rows (tran_date, dimension_id, dimension2_id, amount)
 * @param string $account Account code
 * @param int $scope1 Axis 1 scope (0 = all, negative = unassigned, positive = exact)
 * @param int $scope2 Axis 2 scope
 * @param string $first First day read
 * @param string $last Last day read
 * @return array|false Rows to compare, or false
 */
function dimension_budget_version_overlay($rows, $account, $scope1, $scope2, $first, $last)
{
	$active = dimension_budget_version_active_in_range($first, $last);
	if ($active === false)
		return false;
	if (!count($active))
		return $rows;
	$kept = array();
	foreach ($rows as $row) {
		$covered = false;
		foreach ($active as $year)
			if (strcmp($row['tran_date'], $year['period_from']) >= 0 && strcmp($row['tran_date'], $year['period_to']) <= 0)
				$covered = true;
		if (!$covered)
			$kept[] = $row;
	}
	foreach ($active as $year) {
		if (!dimension_budget_version_usable($year['version_id'], $year['fiscal_year']))
			return false;
		$low = strcmp($first, $year['period_from']) > 0 ? $first : $year['period_from'];
		$high = strcmp($last, $year['period_to']) < 0 ? $last : $year['period_to'];
		$sql = 'SELECT period_start, coordinate_1, coordinate_2, amount FROM '.TB_PREF.'dimension_budget_version_lines WHERE version_id='.(int)$year['version_id']
			.' AND account_code='.dimension_budget_version_quote($account).' AND period_start>='.dimension_budget_version_quote($low)
			.' AND period_start<='.dimension_budget_version_quote($high);
		if ((int)$scope1 !== 0)
			$sql .= ' AND coordinate_1='.((int)$scope1 < 0 ? 0 : (int)$scope1);
		if ((int)$scope2 !== 0)
			$sql .= ' AND coordinate_2='.((int)$scope2 < 0 ? 0 : (int)$scope2);
		$found = db_query($sql, 'The budget version rows could not be read');
		if ($found === false)
			return false;
		while ($row = db_fetch_assoc($found))
			$kept[] = array('tran_date' => $row['period_start'], 'dimension_id' => $row['coordinate_1'], 'dimension2_id' => $row['coordinate_2'], 'amount' => $row['amount']);
	}
	return $kept;
}

/**
 * Short labels of the budget source a period uses, for the report notes.
 *
 * @param string $from SQL date
 * @param string $to SQL date
 * @return string[] One label per fiscal year with an active version; empty when the live budget is used
 */
function dimension_budget_version_source_labels($from, $to)
{
	$active = dimension_budget_version_active_in_range($from, $to);
	if (!$active)
		return array();
	$labels = array();
	foreach ($active as $year) {
		$version = dimension_budget_version_load($year['version_id'], false, false);
		if ($version)
			$labels[] = sprintf(_('Budget source: approved %s version %d of the fiscal year %s - %s (not the live budget entry).'),
				$version['version_kind'] === 'baseline' ? _('baseline') : _('revision'), (int)$version['version_no'],
				sql2date($year['period_from']), sql2date($year['period_to']));
	}
	return $labels;
}
