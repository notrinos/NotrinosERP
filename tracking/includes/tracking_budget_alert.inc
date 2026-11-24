<?php
require_once(__DIR__.'/../../includes/value_helpers.inc');
// DIM-NAM-003: the company name of each tracking category (also when this file is loaded without a page).
include_once(dirname(__FILE__).'/tracking_axis_label.inc');

/**
 * DIM-BUD-004: budget overrun alerts at company-configured thresholds.
 *
 * Plan section 4 item 2. Where DIM-BUD-003 answers "what does THIS document do to the free budget", this authority answers the
 * standing question "which budgets of the fiscal year are close to or over their limit?" for the dashboard and an alerts page.
 * It is a pure reader on top of the existing authorities and adds no schema:
 *
 *  - WHICH BUDGETS. Every account of a cost class (COGS or expense) that has an effective budget in the fiscal year, once per
 *    exact budget pair (account, Dimension 1, Dimension 2). The effective budget is the BUD-001 reader's answer, so an active
 *    BUD-002 version replaces the live rows exactly as it does in the reports. A budget row coordinate of 0 is read as "all of that
 *    axis" (the reports' aggregate rule), never as the unassigned scope. Income, balance-sheet and unclassified accounts are not
 *    alerted: their budget is a target or a balance, not a spending limit.
 *  - WHICH MONEY. The ADV-001 funds position of the pair over the whole fiscal year: GL actual, plus the commitments still
 *    remaining in the register, plus open reservations, against the fiscal-year budget (the same `consumed` and `free` the funds
 *    position and the DIM-BUD-003 panel show). The share used is consumed / budget.
 *  - WHICH LEVEL. A warning threshold (default 90%) and a limit threshold (default 100%) of the share used. At or above the
 *    warning the pair is `warning`; at or above the limit it is `limit` (over budget when it is strictly above). A zero budget with
 *    any consumption is `limit` (no share exists); a budget that is not comparable (a month without a budget row, a fiscal year
 *    that does not start on a month boundary) or negative is `not_evaluated` with its reason and is never guessed.
 *  - THRESHOLDS are company preferences in `sys_prefs` (no schema): `dimension_budget_alert_warning`, `dimension_budget_alert_limit`
 *    and a bounded change history `dimension_budget_alert_history`. They are changed only with the `SA_SETUPCOMPANY` area, with a
 *    mandatory reason, 0 < warning < limit <= 1000 (one decimal). Without a stored value the defaults apply, so the alerts work the
 *    moment the budget exists, and a stored value that fails validation falls back to the defaults and is reported.
 *  - SECURITY. The Dimension view area (the page and menu entry) AND the funds-position rule (`SA_GLANALYTIC` or `SA_BUDGETENTRY`)
 *    are checked before any figure is read. No security area, role or menu permission is added.
 *  - ADVISORY. Nothing here blocks a document, writes a ledger, budget, purchasing or register row, or sends a message. Commitments
 *    are those the register has observed; the alerts page states so. At most `dimension_alert_max_keys()` budget pairs are
 *    evaluated and the result says when it was truncated.
 *
 * PHP 5.6 compatible syntax. Dates are SQL `Y-m-d`.
 */
include_once dirname(__FILE__).'/tracking_commitment.inc';

/** Default warning threshold (percent of the budget used). */
function dimension_alert_default_warning()
{
	return 90.0;
}

/** Default limit threshold (percent of the budget used). */
function dimension_alert_default_limit()
{
	return 100.0;
}

/** Largest threshold that may be stored (percent). */
function dimension_alert_max_percent()
{
	return 1000.0;
}

/** Largest number of budget pairs one evaluation reads. */
function dimension_alert_max_keys()
{
	return 500;
}

/** Number of threshold changes the bounded history keeps. */
function dimension_alert_history_size()
{
	return 20;
}

/** Name of the company preference that holds the warning threshold. */
function dimension_alert_warning_pref()
{
	return 'dimension_budget_alert_warning';
}

/** Name of the company preference that holds the limit threshold. */
function dimension_alert_limit_pref()
{
	return 'dimension_budget_alert_limit';
}

/** Name of the company preference that holds the bounded change history. */
function dimension_alert_history_pref()
{
	return 'dimension_budget_alert_history';
}

/**
 * May the session see budget alerts? The Dimension view area (page and menu entry) and the funds-position rule.
 *
 * @return bool
 */
function dimension_alert_access()
{
	return dimension_commit_has_access(array('SA_DIMTRANSVIEW')) && dimension_commit_has_access(array('SA_GLANALYTIC', 'SA_BUDGETENTRY'));
}

/**
 * Is the active company ready to evaluate alerts (the commitment tables and the preference table exist)?
 *
 * @return bool
 */
function dimension_alert_schema_ready()
{
	return dimension_commit_schema_ready() === true;
}

/**
 * Read one company preference value without any cache.
 *
 * @param string $name Preference name
 * @return string|null|false The value, null when absent, false when unreadable
 */
function dimension_alert_read_pref($name)
{
	$rows = db_query('SELECT value FROM '.TB_PREF.'sys_prefs WHERE name='.dimension_commit_quote($name));
	if ($rows === false)
		return false;
	$row = db_fetch_row($rows);
	return $row ? (string)$row[0] : null;
}

/**
 * Parse one percentage: digits with at most one decimal, greater than 0 and not above the maximum.
 *
 * @param mixed $value User or stored text
 * @return float|false False when invalid
 */
function dimension_alert_parse_percent($value)
{
	if (is_int($value) || is_float($value))
		$value = (string)$value;
	if (!is_string($value))
		return false;
	$value = trim($value);
	if (!preg_match('/^[0-9]{1,4}(\.[0-9])?$/D', $value))
		return false;
	$percent = round((float)$value, 1);
	if ($percent <= 0 || $percent > dimension_alert_max_percent())
		return false;
	return $percent;
}

/**
 * The thresholds in force.
 *
 * @return array warning, limit, source (default|company|invalid_stored|unreadable)
 */
function dimension_alert_thresholds()
{
	$thresholds = array('warning' => dimension_alert_default_warning(), 'limit' => dimension_alert_default_limit(), 'source' => 'default');
	$warning = dimension_alert_read_pref(dimension_alert_warning_pref());
	$limit = dimension_alert_read_pref(dimension_alert_limit_pref());
	if ($warning === false || $limit === false) {
		$thresholds['source'] = 'unreadable';
		return $thresholds;
	}
	if ($warning === null && $limit === null)
		return $thresholds;
	$warning_value = $warning === null ? false : dimension_alert_parse_percent($warning);
	$limit_value = $limit === null ? false : dimension_alert_parse_percent($limit);
	if ($warning_value === false || $limit_value === false || $warning_value >= $limit_value) {
		$thresholds['source'] = 'invalid_stored';
		return $thresholds;
	}
	return array('warning' => $warning_value, 'limit' => $limit_value, 'source' => 'company');
}

/**
 * The bounded history of threshold changes, newest first.
 *
 * @return array|false Entries (at, actor, warning, limit, previous_warning, previous_limit, reason), or false when denied or unreadable
 */
function dimension_alert_history()
{
	if (!dimension_commit_has_access(array('SA_SETUPCOMPANY', 'SA_DIMTRANSVIEW')))
		return false;
	$stored = dimension_alert_read_pref(dimension_alert_history_pref());
	if ($stored === false)
		return false;
	if ($stored === null || $stored === '')
		return array();
	$decoded = json_decode($stored, true);
	if (!is_array($decoded))
		return array();
	$entries = array();
	foreach ($decoded as $entry) {
		if (!is_array($entry) || !isset($entry['at'], $entry['actor'], $entry['warning'], $entry['limit'], $entry['reason']))
			continue;
		$entries[] = array('at' => (string)$entry['at'], 'actor' => (int)$entry['actor'], 'warning' => (float)$entry['warning'], 'limit' => (float)$entry['limit'],
			'previous_warning' => isset($entry['previous_warning']) ? (float)$entry['previous_warning'] : null,
			'previous_limit' => isset($entry['previous_limit']) ? (float)$entry['previous_limit'] : null, 'reason' => (string)$entry['reason']);
	}
	return array_slice($entries, 0, dimension_alert_history_size());
}

/**
 * Store one preference row (insert or replace) with the framework's column layout.
 *
 * @param string $name Preference name
 * @param string $type Stored type label
 * @param string $value Value
 * @return bool
 */
function dimension_alert_write_pref($name, $type, $value)
{
	return db_query('REPLACE '.TB_PREF.'sys_prefs (name, category, type, length, value) VALUES ('.dimension_commit_quote($name).",'dimension.budget_alert',"
		.dimension_commit_quote($type).',0,'.dimension_commit_quote($value).')') !== false;
}

/**
 * Change the thresholds (an authorized, reasoned choice kept in a bounded history; the defaults apply until it is done).
 *
 * @param mixed $warning Warning percent
 * @param mixed $limit Limit percent
 * @param string $reason Mandatory reason of 1 to 200 characters
 * @return array ok, changed, warning, limit, errors, codes
 */
function dimension_alert_set_thresholds($warning, $limit, $reason)
{
	$result = dimension_commit_result(array('changed' => false, 'warning' => null, 'limit' => null));
	if (!dimension_commit_has_access(array('SA_SETUPCOMPANY')))
		return dimension_commit_fail($result, 'permission_denied', _('You are not allowed to change the budget alert thresholds.'));
	$actor = current_user_id_or_zero();
	if ($actor <= 0)
		return dimension_commit_fail($result, 'permission_denied', _('The deciding user could not be identified.'));
	$warning_value = dimension_alert_parse_percent($warning);
	$limit_value = dimension_alert_parse_percent($limit);
	if ($warning_value === false || $limit_value === false)
		return dimension_commit_fail($result, 'invalid_threshold', sprintf(_('Each threshold must be a percentage above 0 and not above %s, with at most one decimal.'),
			rtrim(rtrim(number_format(dimension_alert_max_percent(), 1, '.', ''), '0'), '.')));
	if ($warning_value >= $limit_value)
		return dimension_commit_fail($result, 'invalid_order', _('The warning threshold must be below the limit threshold.'));
	$reason = is_string($reason) ? trim($reason) : '';
	if ($reason === '' || strlen($reason) > 200)
		return dimension_commit_fail($result, 'reason_required', _('A reason of 1 to 200 characters is required.'));
	if (!dimension_commit_lock('dimension_alert_thresholds'))
		return dimension_commit_fail($result, 'busy', _('Another threshold change is running; try again.'));
	$current = dimension_alert_thresholds();
	$result['warning'] = $current['warning'];
	$result['limit'] = $current['limit'];
	if ($current['source'] === 'company' && $current['warning'] === $warning_value && $current['limit'] === $limit_value) {
		dimension_commit_unlock('dimension_alert_thresholds');
		return $result;
	}
	$stored = dimension_alert_read_pref(dimension_alert_history_pref());
	$entries = $stored === null || $stored === false || $stored === '' ? array() : json_decode($stored, true);
	if (!is_array($entries))
		$entries = array();
	array_unshift($entries, array('at' => date('Y-m-d H:i:s'), 'actor' => $actor, 'warning' => $warning_value, 'limit' => $limit_value,
		'previous_warning' => $current['warning'], 'previous_limit' => $current['limit'], 'reason' => $reason));
	$entries = array_slice($entries, 0, dimension_alert_history_size());
	$done = begin_transaction()
		&& dimension_alert_write_pref(dimension_alert_warning_pref(), 'varchar', (string)$warning_value)
		&& dimension_alert_write_pref(dimension_alert_limit_pref(), 'varchar', (string)$limit_value)
		&& dimension_alert_write_pref(dimension_alert_history_pref(), 'text', json_encode($entries));
	if ($done)
		$done = commit_transaction();
	else
		cancel_transaction();
	dimension_commit_unlock('dimension_alert_thresholds');
	if (!$done)
		return dimension_commit_fail($result, 'unavailable', _('The thresholds could not be recorded.'));
	dimension_alert_cache_reset();
	$result['changed'] = true;
	$result['warning'] = $warning_value;
	$result['limit'] = $limit_value;
	return $result;
}

/**
 * Classify the used share of one budget (pure; no database).
 *
 * @param float|null $budget Comparable fiscal-year budget, null when none
 * @param float $consumed GL actual plus remaining commitments plus open reservations
 * @param array $thresholds warning and limit percentages
 * @return array state (ok|warning|limit|not_evaluated), percent (null for a zero budget), reason (empty|zero_budget|negative_budget|no_budget)
 */
function dimension_alert_classify($budget, $consumed, $thresholds)
{
	if ($budget === null)
		return array('state' => 'not_evaluated', 'percent' => null, 'reason' => 'no_budget');
	$budget = (float)$budget;
	$consumed = (float)$consumed;
	if ($budget < 0)
		return array('state' => 'not_evaluated', 'percent' => null, 'reason' => 'negative_budget');
	if ($budget == 0)
		return array('state' => $consumed > 0.000001 ? 'limit' : 'ok', 'percent' => null, 'reason' => 'zero_budget');
	$percent = round($consumed / $budget * 100, 1);
	$state = 'ok';
	if (round($consumed * 100 - $thresholds['limit'] * $budget, 6) >= 0)
		$state = 'limit';
	elseif (round($consumed * 100 - $thresholds['warning'] * $budget, 6) >= 0)
		$state = 'warning';
	return array('state' => $state, 'percent' => $percent, 'reason' => '');
}

/**
 * The fiscal year an evaluation reads.
 *
 * @param int $fiscal_year_id Fiscal year id, 0 for the company's current fiscal year
 * @return array|false id, from, to (SQL dates), closed, or false when it does not exist
 */
function dimension_alert_fiscal_year($fiscal_year_id = 0)
{
	$fiscal_year_id = (int)$fiscal_year_id;
	if ($fiscal_year_id <= 0) {
		$current = dimension_alert_read_pref('f_year');
		$fiscal_year_id = $current === null || $current === false ? 0 : (int)$current;
	}
	if ($fiscal_year_id <= 0)
		return false;
	$rows = db_query('SELECT id, `begin`, `end`, closed FROM '.TB_PREF.'fiscal_year WHERE id='.$fiscal_year_id);
	$row = $rows ? db_fetch_row($rows) : false;
	return $row ? array('id' => (int)$row[0], 'from' => (string)$row[1], 'to' => (string)$row[2], 'closed' => (int)$row[3]) : false;
}

/**
 * Cost-class accounts that carry an effective budget in a window: the live rows and the rows of an active approved version.
 *
 * @param string $first First day of the first month of the period
 * @param string $last Last day of the last month of the period
 * @return array|false Account codes in code order, or false when unreadable
 */
function dimension_alert_budgeted_accounts($first, $last)
{
	$class_join = ' INNER JOIN '.TB_PREF.'chart_master m ON m.account_code=%s INNER JOIN '.TB_PREF.'chart_types t ON t.id=m.account_type INNER JOIN '
		.TB_PREF.'chart_class c ON c.cid=t.class_id';
	$class_where = ' AND c.ctype IN ('.(int)CL_COGS.','.(int)CL_EXPENSE.')';
	$accounts = array();
	$rows = db_query('SELECT DISTINCT b.account FROM '.TB_PREF.'budget_trans b'.sprintf($class_join, 'b.account').' WHERE b.tran_date>='.dimension_commit_quote($first)
		.' AND b.tran_date<='.dimension_commit_quote($last).$class_where);
	if ($rows === false)
		return false;
	while ($row = db_fetch_row($rows))
		$accounts[(string)$row[0]] = true;
	if (function_exists('dimension_budget_version_active_in_range')) {
		$active = dimension_budget_version_active_in_range($first, $last);
		if ($active === false)
			return false;
		foreach ($active as $year) {
			$low = strcmp($first, $year['period_from']) > 0 ? $first : $year['period_from'];
			$high = strcmp($last, $year['period_to']) < 0 ? $last : $year['period_to'];
			$rows = db_query('SELECT DISTINCT l.account_code FROM '.TB_PREF.'dimension_budget_version_lines l'.sprintf($class_join, 'l.account_code').' WHERE l.version_id='
				.(int)$year['version_id'].' AND l.period_start>='.dimension_commit_quote($low).' AND l.period_start<='.dimension_commit_quote($high).$class_where);
			if ($rows === false)
				return false;
			while ($row = db_fetch_row($rows))
				$accounts[(string)$row[0]] = true;
		}
	}
	$codes = array_keys($accounts);
	sort($codes, SORT_STRING);
	return $codes;
}

/**
 * The exact budget pairs (account, Dimension 1, Dimension 2) of the fiscal year, from the effective budget rows.
 *
 * @param string $from SQL date
 * @param string $to SQL date
 * @return array|false keys (account, coordinate_1, coordinate_2), total (before the cap), truncated; or false when unreadable
 */
function dimension_alert_budget_keys($from, $to)
{
	$months = dimension_budget_months($from, $to);
	if ($months === false)
		return false;
	$month_keys = array_keys($months);
	$first = $months[$month_keys[0]]['start'];
	$last = $months[$month_keys[count($month_keys) - 1]]['end'];
	$accounts = dimension_alert_budgeted_accounts($first, $last);
	if ($accounts === false)
		return false;
	$pairs = array();
	foreach (array_chunk($accounts, 50) as $chunk) {
		dimension_budget_prefetch_rows($chunk, 0, 0, $from, $to);
		foreach ($chunk as $account) {
			$rows = dimension_budget_read_rows($account, 0, 0, $from, $to);
			if ($rows === false) {
				dimension_budget_prefetch_clear();
				return false;
			}
			$seen = array();
			foreach ($rows as $row) {
				$key = (int)$row['dimension_id'].'|'.(int)$row['dimension2_id'];
				if (!isset($seen[$key])) {
					$seen[$key] = true;
					$pairs[] = array('account' => (string)$account, 'coordinate_1' => (int)$row['dimension_id'], 'coordinate_2' => (int)$row['dimension2_id']);
				}
			}
		}
		dimension_budget_prefetch_clear();
	}
	usort($pairs, 'dimension_alert_compare_keys');
	$total = count($pairs);
	return array('keys' => array_slice($pairs, 0, dimension_alert_max_keys()), 'total' => $total, 'truncated' => $total > dimension_alert_max_keys());
}

/** Order two budget pairs by account, Dimension 1, Dimension 2. */
function dimension_alert_compare_keys($left, $right)
{
	$order = strcmp($left['account'], $right['account']);
	if ($order !== 0)
		return $order;
	if ($left['coordinate_1'] !== $right['coordinate_1'])
		return $left['coordinate_1'] < $right['coordinate_1'] ? -1 : 1;
	if ($left['coordinate_2'] !== $right['coordinate_2'])
		return $left['coordinate_2'] < $right['coordinate_2'] ? -1 : 1;
	return 0;
}

/** Severity rank of a state, most urgent first. */
function dimension_alert_rank($state)
{
	$ranks = array('limit' => 0, 'warning' => 1, 'not_evaluated' => 2, 'ok' => 3);
	return isset($ranks[$state]) ? $ranks[$state] : 4;
}

/** Order two result rows: urgency, then the larger used share, then account and scope. */
function dimension_alert_compare_rows($left, $right)
{
	$order = dimension_alert_rank($left['state']) - dimension_alert_rank($right['state']);
	if ($order !== 0)
		return $order;
	$left_percent = $left['percent'] === null ? ($left['state'] === 'limit' ? 1.0E+9 : -1.0) : $left['percent'];
	$right_percent = $right['percent'] === null ? ($right['state'] === 'limit' ? 1.0E+9 : -1.0) : $right['percent'];
	if ($left_percent !== $right_percent)
		return $left_percent > $right_percent ? -1 : 1;
	return dimension_alert_compare_keys($left, $right);
}

/** A fresh evaluation skeleton. */
function dimension_alert_result($state, $extra = array())
{
	return array_merge(array('state' => $state, 'year' => null, 'thresholds' => null, 'rows' => array(), 'counts' => array('limit' => 0, 'warning' => 0, 'ok' => 0, 'not_evaluated' => 0),
		'budget_total' => 0, 'truncated' => false, 'incomplete' => 0, 'reason' => '', 'registered_at' => null,
		'label' => _('Advisory only: alerts never block a document or post anything. Commitments are those recorded in the commitment register.')), $extra);
}

/** Forget the per-request evaluation cache (after a change of company, threshold, budget or ledger). */
function dimension_alert_cache_reset()
{
	$store = &dimension_alert_cache_store();
	$store = array();
}

/** The per-request evaluation cache (the dashboard evaluates the same fiscal year in several widgets). */
function &dimension_alert_cache_store()
{
	static $store = array();
	return $store;
}

/**
 * Evaluate every cost budget of one fiscal year against the thresholds.
 *
 * @param int $fiscal_year_id Fiscal year id, 0 for the company's current fiscal year
 * @return array state (available|denied|unready|no_year|not_comparable|unreadable), year, thresholds, rows, counts, budget_total, truncated, incomplete, reason, label
 */
function dimension_alert_evaluate($fiscal_year_id = 0)
{
	if (!dimension_alert_access())
		return dimension_alert_result('denied');
	if (!dimension_alert_schema_ready())
		return dimension_alert_result('unready');
	$company = isset($_SESSION['wa_current_user']->cur_con) ? (int)$_SESSION['wa_current_user']->cur_con : 0;
	$thresholds = dimension_alert_thresholds();
	$cache_key = $company.'|'.(int)$fiscal_year_id.'|'.$thresholds['warning'].'|'.$thresholds['limit'].'|'.current_user_id_or_zero();
	$store = &dimension_alert_cache_store();
	if (isset($store[$cache_key]))
		return $store[$cache_key];
	$year = dimension_alert_fiscal_year($fiscal_year_id);
	if ($year === false)
		return dimension_alert_result('no_year', array('thresholds' => $thresholds));
	$period = dimension_budget_period($year['from'], $year['to']);
	if (!$period['comparable'])
		return dimension_alert_result('not_comparable', array('year' => $year, 'thresholds' => $thresholds, 'reason' => dimension_budget_reason_text($period['reason'])));
	$found = dimension_alert_budget_keys($year['from'], $year['to']);
	if ($found === false)
		return dimension_alert_result('unreadable', array('year' => $year, 'thresholds' => $thresholds));
	$rows = array();
	$counts = array('limit' => 0, 'warning' => 0, 'ok' => 0, 'not_evaluated' => 0);
	$incomplete = 0;
	foreach ($found['keys'] as $key) {
		$position = dimension_commit_position_unchecked($key['account'], $key['coordinate_1'], $key['coordinate_2'], $year['from'], $year['to']);
		if ($position === false)
			return dimension_alert_result('unreadable', array('year' => $year, 'thresholds' => $thresholds));
		$available = !empty($position['budget_available']);
		$class = $available ? dimension_alert_classify($position['budget'], $position['consumed'], $thresholds)
			: array('state' => 'not_evaluated', 'percent' => null, 'reason' => 'no_budget');
		if (empty($position['complete']))
			$incomplete++;
		$counts[$class['state']]++;
		$rows[] = array('account' => $key['account'], 'coordinate_1' => $key['coordinate_1'], 'coordinate_2' => $key['coordinate_2'], 'state' => $class['state'],
			'percent' => $class['percent'], 'reason' => $class['reason'], 'budget_reason' => $position['budget_reason'], 'budget' => $position['budget'],
			'actual' => $position['actual'], 'actual_percent' => $available && $position['budget'] > 0 ? round($position['actual'] / $position['budget'] * 100, 1) : null,
			'commitments' => $position['commitment_remaining'], 'reservations' => $position['reservations_open'], 'consumed' => $position['consumed'],
			'free' => $position['free'], 'complete' => !empty($position['complete']));
	}
	usort($rows, 'dimension_alert_compare_rows');
	$result = dimension_alert_result('available', array('year' => $year, 'thresholds' => $thresholds, 'rows' => $rows, 'counts' => $counts, 'budget_total' => $found['total'],
		'truncated' => $found['truncated'], 'incomplete' => $incomplete, 'registered_at' => dimension_alert_register_time()));
	$store[$cache_key] = $result;
	return $result;
}

/**
 * When the commitment register last observed an order (what the alerts' commitments are as of).
 *
 * @return string|null
 */
function dimension_alert_register_time()
{
	$rows = db_query('SELECT MAX(observed_at) FROM '.TB_PREF.'dimension_commitment_lines');
	$row = $rows ? db_fetch_row($rows) : false;
	return $row && $row[0] !== null ? (string)$row[0] : null;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Presentation (pure: no output, no database beyond the name lookups)
// ---------------------------------------------------------------------------------------------------------------------------------

/** HTML-escape one text. */
function dimension_alert_h($text)
{
	return htmlspecialchars((string)$text, ENT_QUOTES, 'UTF-8');
}

/** One amount in the user's price format. */
function dimension_alert_money($amount)
{
	if ($amount === null)
		return '-';
	return function_exists('price_format') ? price_format($amount) : number_format((float)$amount, 2, '.', ',');
}

/** One percentage with its sign of absence. */
function dimension_alert_percent_text($percent)
{
	return $percent === null ? '-' : number_format((float)$percent, 1, '.', '').'%';
}

/** The name of a budget coordinate: 0 is "all" for a budget pair. */
function dimension_alert_dimension_text($coordinate)
{
	if ((int)$coordinate <= 0)
		return _('All');
	return function_exists('get_dimension_string') ? get_dimension_string((int)$coordinate, false) : '#'.(int)$coordinate;
}

/** The readable state of a result row. */
function dimension_alert_state_text($row)
{
	if ($row['state'] === 'limit') {
		if ($row['reason'] === 'zero_budget')
			return _('Limit: spending against a zero budget');
		return $row['free'] !== null && $row['free'] < -0.000001 ? sprintf(_('Over budget by %s'), dimension_alert_money(-$row['free'])) : _('Limit reached');
	}
	if ($row['state'] === 'warning')
		return _('Warning');
	if ($row['state'] === 'not_evaluated') {
		if ($row['reason'] === 'negative_budget')
			return _('Not evaluated: the budget is negative');
		$reason = function_exists('dimension_budget_reason_text') ? dimension_budget_reason_text($row['budget_reason']) : '';
		return _('Not evaluated: no comparable budget').($reason !== '' ? ' ('.$reason.')' : '');
	}
	return _('Within the thresholds');
}

/**
 * The rows table as HTML.
 *
 * @param array $rows Result rows
 * @param int $limit Largest number of rows to print (0 for all)
 * @return string
 */
function dimension_alert_rows_html($rows, $limit = 0)
{
	if (!$rows)
		return '';
	$html = '<table class="tablestyle" style="margin:0 auto;"><tr>';
	foreach (array(_('Account'), dimension_axis_label_html(1), dimension_axis_label_html(2), _('Budget'), _('Actual'), _('Commitments'), _('Reserved'), _('Used'), _('Free'), _('Status')) as $heading)
		$html .= '<th>'.dimension_alert_h($heading).'</th>';
	$html .= '</tr>';
	$printed = 0;
	foreach ($rows as $row) {
		if ($limit > 0 && $printed >= $limit)
			break;
		$printed++;
		$account = $row['account'].(function_exists('get_gl_account_name') ? ' '.get_gl_account_name($row['account']) : '');
		$style = $row['state'] === 'limit' ? ' style="color:#b00020;font-weight:bold;"' : ($row['state'] === 'warning' ? ' style="color:#9a5b00;font-weight:bold;"' : '');
		$html .= '<tr><td>'.dimension_alert_h($account).'</td><td>'.dimension_alert_h(dimension_alert_dimension_text($row['coordinate_1'])).'</td><td>'
			.dimension_alert_h(dimension_alert_dimension_text($row['coordinate_2'])).'</td>';
		foreach (array('budget', 'actual', 'commitments', 'reservations') as $field)
			$html .= '<td style="text-align:right;">'.dimension_alert_h(dimension_alert_money($row[$field])).'</td>';
		$html .= '<td style="text-align:right;">'.dimension_alert_h($row['reason'] === 'zero_budget' ? '-' : dimension_alert_percent_text($row['percent'])).'</td>';
		$html .= '<td style="text-align:right;">'.dimension_alert_h(dimension_alert_money($row['free'])).'</td>';
		$html .= '<td'.$style.'>'.dimension_alert_h(dimension_alert_state_text($row)).'</td></tr>';
	}
	return $html.'</table>';
}

/** The one-sentence state of a result that has no rows to show. */
function dimension_alert_state_note($result)
{
	$notes = array(
		'denied' => _('Budget alerts are shown to users who may view the tracking value inquiries and either budgets or GL analytics.'),
		'unready' => _('Budget alerts are not available: the commitment register is not installed. Upgrade the company database first.'),
		'no_year' => _('Budget alerts are not available: the fiscal year could not be found.'),
		'unreadable' => _('Budget alerts could not be read.'));
	if ($result['state'] === 'not_comparable')
		return sprintf(_('Budget alerts are not shown: the fiscal year cannot be compared month by month. %s'), $result['reason']);
	return isset($notes[$result['state']]) ? $notes[$result['state']] : '';
}

/** The sentence that names the thresholds in force. */
function dimension_alert_threshold_text($thresholds)
{
	$text = sprintf(_('Warning at %s of the budget used, limit at %s.'), dimension_alert_percent_text($thresholds['warning']), dimension_alert_percent_text($thresholds['limit']));
	if ($thresholds['source'] === 'default')
		$text .= ' '._('These are the default thresholds; no company setting has been saved.');
	elseif ($thresholds['source'] === 'invalid_stored')
		$text .= ' '._('The stored thresholds are not valid, so the defaults are used.');
	elseif ($thresholds['source'] === 'unreadable')
		$text .= ' '._('The stored thresholds could not be read, so the defaults are used.');
	return $text;
}

/** The sentence that explains the basis of the figures (printed under the table). */
function dimension_alert_basis_text($result)
{
	$text = sprintf(_('Basis: budget of the fiscal year %s - %s for cost accounts (cost of goods sold and expense). Used = GL actual + commitments still open in the register + open reservations.'),
		sql2date($result['year']['from']), sql2date($result['year']['to']));
	$observed = $result['registered_at'] === null ? _('The commitment register has not observed any order yet.')
		: sprintf(_('The commitment register last observed an order at %s; orders saved since then are not counted.'), $result['registered_at']);
	return $text.' '.$observed;
}
