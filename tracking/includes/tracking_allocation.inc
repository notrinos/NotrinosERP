<?php
/**
 * DIM-ALC-001: guided, approved allocation and reclassification journals.
 *
 * The module turns "split this posted expense between Dimensions" into one explainable contract:
 *
 *  - a source pool of already posted ledger rows (`gl_trans.counter`), each allocated in full;
 *  - a fixed rule, either percentages that total exactly 100 or fixed amounts that total the pool;
 *  - a deterministic preview (integer minor units, largest-remainder rounding with ties to the lowest
 *    Dimension, so the shares total the row exactly) with source links per row and one balancing journal on the SAME accounts,
 *    so the company total of every account is unchanged and only the classification moves;
 *  - one unique run identity (a digest of the normalized request): an identical live request is answered
 *    with the existing run and posts once; a rejected, cancelled or failed run is released and may be
 *    retried as a new run;
 *  - posting through the ordinary journal approval path (`ApprovalWorkflow`, `write_journal_entries`);
 *  - an exact inverse journal (every posted row negated) linked to the run, at most one live inverse.
 *
 * Only two additive evidence tables are written (`dimension_allocation_runs` and
 * `dimension_allocation_sources`). No posted ledger row, budget, approval or HRM row is ever updated by
 * this module. Reusable named rules, driver-based allocation and closed-scope reclassification are not part
 * of this release (DIM-ALC-002 / the unsupported `reclassification` validation command).
 *
 * PHP 5.6 compatible.
 */
require_once(__DIR__.'/../../includes/value_helpers.inc');

/** Kinds of guided run. */
function dimension_allocation_kinds()
{
	return array('allocation', 'reclassification');
}

/** Rule modes of a guided run. */
function dimension_allocation_modes()
{
	return array('percent', 'amount');
}

/** Number of stored decimals of a monetary amount (the company price precision). */
function dimension_allocation_decimals()
{
	$decimals = function_exists('user_price_dec') ? (int)user_price_dec() : 2;
	return max(0, min(4, $decimals));
}

/** Minor units per currency unit. */
function dimension_allocation_scale()
{
	return (int)pow(10, dimension_allocation_decimals());
}

/** Format minor units as a plain decimal string at the company precision. */
function dimension_allocation_format_minor($minor)
{
	return number_format($minor / dimension_allocation_scale(), dimension_allocation_decimals(), '.', '');
}

/**
 * Quote one text value exactly as stored.
 *
 * The generic `db_escape()` helper HTML-encodes quotes and ampersands for page safety, which would corrupt a JSON
 * payload; the evidence tables store the exact bytes.
 *
 * @param mixed $value Text to store
 * @return string Quoted SQL literal
 */
function dimension_allocation_quote($value)
{
	return "'".db_escape_function((string)$value)."'";
}

/** Record one stable reason code and its readable text on a result. */
function dimension_allocation_fail(&$result, $code, $text)
{
	$result['ok'] = false;
	if (!in_array($code, $result['codes'], true))
		$result['codes'][] = $code;
	$result['errors'][] = $text;
	return $result;
}

/**
 * Exact floor share of one amount at a percentage in 1/10000 percent units, with its remainder.
 *
 * `floor($total_minor * $percent_e4 / 1000000)` and the remainder in millionths of a minor unit, computed
 * without a 64-bit overflow.
 *
 * @param int $total_minor Non-negative amount in minor units
 * @param int $percent_e4 Percentage times 10000 (100% = 1000000)
 * @return array floor (minor units) and remainder (0..999999)
 */
function dimension_allocation_share_parts($total_minor, $percent_e4)
{
	$total_minor = (int)$total_minor;
	$percent_e4 = (int)$percent_e4;
	$whole = (int)floor($total_minor / 1000000);
	$rest = $total_minor - $whole * 1000000;
	$product = $rest * $percent_e4;
	$carry = (int)floor($product / 1000000);
	return array('floor' => $whole * $percent_e4 + $carry, 'remainder' => $product - $carry * 1000000);
}

/**
 * Normalize and validate one allocation request without touching the database.
 *
 * @param array $request kind, axis (1|2), mode (percent|amount), date (Y-m-d), memo, sources (gl counters)
 *   and targets (list of dimension_id + value; a percentage or an amount in currency units)
 * @return array ok, errors, codes and the canonical `request` (targets sorted by Dimension, values in
 *   1/10000 percent or minor units)
 */
function dimension_allocation_normalize_request($request)
{
	$result = array('ok' => true, 'errors' => array(), 'codes' => array(), 'request' => null);
	if (!is_array($request))
		return dimension_allocation_fail($result, 'invalid_request', _('The allocation request could not be read.'));
	$kind = isset($request['kind']) ? $request['kind'] : '';
	$mode = isset($request['mode']) ? $request['mode'] : '';
	$axis = isset($request['axis']) ? $request['axis'] : 0;
	if (!in_array($kind, dimension_allocation_kinds(), true))
		dimension_allocation_fail($result, 'invalid_kind', _('Choose whether this run splits the cost (allocation) or moves it entirely (reclassification).'));
	if (!in_array($mode, dimension_allocation_modes(), true))
		dimension_allocation_fail($result, 'invalid_mode', _('Choose a fixed percentage or a fixed amount rule.'));
	if (!in_array((string)$axis, array('1', '2'), true))
		dimension_allocation_fail($result, 'invalid_axis', _('Choose the tracking category to allocate across.'));
	$date = isset($request['date']) ? $request['date'] : '';
	if (!is_string($date) || !preg_match('/^([0-9]{4})-([0-9]{2})-([0-9]{2})$/D', $date, $match)
		|| !checkdate((int)$match[2], (int)$match[3], (int)$match[1]))
		dimension_allocation_fail($result, 'invalid_date', _('The allocation date is invalid.'));
	$memo = isset($request['memo']) && is_string($request['memo']) ? trim($request['memo']) : '';
	if (strlen($memo) > 200)
		dimension_allocation_fail($result, 'memo_too_long', _('The memo may not be longer than 200 characters.'));

	$sources = array();
	$raw_sources = isset($request['sources']) && is_array($request['sources']) ? $request['sources'] : array();
	foreach ($raw_sources as $counter) {
		if (is_string($counter) && ctype_digit($counter))
			$counter = (int)$counter;
		if (!is_int($counter) || $counter <= 0 || $counter > 2147483647) {
			dimension_allocation_fail($result, 'invalid_source', _('A source ledger row identity is invalid.'));
			continue;
		}
		$sources[$counter] = $counter;
	}
	$sources = array_values($sources);
	sort($sources);
	if (!count($sources))
		dimension_allocation_fail($result, 'no_source', _('Select at least one posted ledger row to allocate.'));
	elseif (count($sources) > 200)
		dimension_allocation_fail($result, 'too_many_sources', _('A run may allocate at most 200 ledger rows.'));

	$targets = array();
	$raw_targets = isset($request['targets']) && is_array($request['targets']) ? $request['targets'] : array();
	$scale = dimension_allocation_scale();
	$sum = 0;
	foreach ($raw_targets as $target) {
		$identity = is_array($target) && isset($target['dimension_id']) ? $target['dimension_id'] : null;
		if (is_string($identity) && ctype_digit($identity))
			$identity = (int)$identity;
		if (!is_int($identity) || $identity <= 0 || $identity > 2147483647) {
			dimension_allocation_fail($result, 'invalid_target', _('Each target must be an existing tracking value.'));
			continue;
		}
		if (isset($targets[$identity])) {
			dimension_allocation_fail($result, 'duplicate_target', sprintf(_('Tracking value #%d is listed more than once.'), $identity));
			continue;
		}
		$value = is_array($target) && isset($target['value']) ? $target['value'] : null;
		if (!is_numeric($value) || (float)$value <= 0) {
			dimension_allocation_fail($result, 'invalid_value', sprintf(_('The value for tracking value #%d must be greater than zero.'), $identity));
			continue;
		}
		$multiplier = $mode === 'percent' ? 10000 : $scale;
		$stored = (int)round((float)$value * $multiplier);
		if (abs((float)$value * $multiplier - $stored) > 0.000001) {
			dimension_allocation_fail($result, 'precision', sprintf($mode === 'percent'
				? _('The percentage for tracking value #%d has more than four decimals.')
				: _('The amount for tracking value #%d has more decimals than the company precision.'), $identity));
			continue;
		}
		if ($mode === 'percent' && $stored > 1000000) {
			dimension_allocation_fail($result, 'percent_range', sprintf(_('The percentage for tracking value #%d is above 100.'), $identity));
			continue;
		}
		$targets[$identity] = $stored;
		$sum += $stored;
	}
	ksort($targets);
	if (!count($targets))
		dimension_allocation_fail($result, 'no_target', _('Add at least one target tracking value.'));
	elseif ($kind === 'reclassification' && count($targets) !== 1)
		dimension_allocation_fail($result, 'reclassification_targets', _('A reclassification moves the whole amount to exactly one target.'));
	elseif ($kind === 'allocation' && count($targets) < 2)
		dimension_allocation_fail($result, 'allocation_targets', _('An allocation needs at least two targets; use a reclassification for a single one.'));
	if ($mode === 'percent' && count($targets) && !in_array('percent_range', $result['codes'], true)
		&& !in_array('invalid_value', $result['codes'], true) && $sum !== 1000000)
		dimension_allocation_fail($result, 'percent_sum', sprintf(_('The percentages must total exactly 100 (currently %s).'),
			rtrim(rtrim(number_format($sum / 10000, 4, '.', ''), '0'), '.')));
	if (!$result['ok'])
		return $result;
	$list = array();
	foreach ($targets as $identity => $value)
		$list[] = array('dimension_id' => (int)$identity, 'value' => (int)$value);
	$result['request'] = array('kind' => $kind, 'axis' => (int)$axis, 'mode' => $mode, 'date' => $date, 'memo' => $memo,
		'sources' => $sources, 'targets' => $list);
	return $result;
}

/**
 * Unique run identity of a normalized request.
 *
 * The memo is not part of the identity: the same rule over the same rows on the same date is one run.
 *
 * @param array $request Normalized request
 * @return string 64 hex characters
 */
function dimension_allocation_run_key($request)
{
	return hash('sha256', json_encode(array('alc1', $request['kind'], $request['axis'], $request['mode'], $request['date'],
		$request['sources'], $request['targets'])));
}

/** Digest of the canonical journal lines a run is bound to. */
function dimension_allocation_lines_digest($lines)
{
	$canonical = array();
	foreach ($lines as $line)
		$canonical[] = array((string)$line['code_id'], (int)$line['dimension_id'], (int)$line['dimension2_id'],
			(int)round((float)$line['amount'] * dimension_allocation_scale()));
	return hash('sha256', json_encode($canonical));
}

/**
 * Read the posted ledger rows named by a request.
 *
 * @param int[] $counters Ledger identities
 * @param bool $lock Lock the rows for the owning transaction
 * @return array|false Counter => row, or false when the read failed
 */
function dimension_allocation_load_sources($counters, $lock = false)
{
	$ids = array();
	foreach ($counters as $counter)
		$ids[] = (int)$counter;
	if (!count($ids))
		return array();
	$sql = 'SELECT g.counter, g.type, g.type_no, g.tran_date, g.account, g.memo_, g.amount, g.dimension_id, g.dimension2_id,'
		.' g.person_type_id, g.person_id, IF(v.date_ IS NULL, 0, 1) AS voided'
		.' FROM '.TB_PREF.'gl_trans g LEFT JOIN '.TB_PREF.'voided v ON v.type=g.type AND v.id=g.type_no'
		.' WHERE g.counter IN ('.implode(',', $ids).') ORDER BY g.counter'.($lock ? ' FOR UPDATE' : '');
	$rows = db_query($sql);
	if ($rows === false)
		return false;
	$found = array();
	while ($row = db_fetch_assoc($rows))
		$found[(int)$row['counter']] = $row;
	return $found;
}

/**
 * Live source reservations of a set of ledger rows.
 *
 * @param int[] $counters Ledger identities
 * @param bool $lock Lock the reservation rows
 * @return array|false Counter => run identity
 */
function dimension_allocation_reservations($counters, $lock = false)
{
	$ids = array();
	foreach ($counters as $counter)
		$ids[] = (int)$counter;
	if (!count($ids))
		return array();
	$rows = db_query('SELECT gl_counter, run_id FROM '.TB_PREF.'dimension_allocation_sources WHERE active_counter IN ('
		.implode(',', $ids).')'.($lock ? ' FOR UPDATE' : ''));
	if ($rows === false)
		return false;
	$reserved = array();
	while ($row = db_fetch_assoc($rows))
		$reserved[(int)$row['gl_counter']] = (int)$row['run_id'];
	return $reserved;
}

/**
 * Check that every requested ledger row can be allocated.
 *
 * Refused: a missing or voided row, a zero row, a row bound to a customer or supplier, a bank, tax or
 * subledger control account, a pool of mixed debit and credit rows and a row already reserved by a live run.
 *
 * @param array $request Normalized request
 * @param array $rows Counter => stored row
 * @param array $reserved Counter => run identity
 * @return array ok, errors, codes
 */
function dimension_allocation_check_sources($request, $rows, $reserved)
{
	$result = array('ok' => true, 'errors' => array(), 'codes' => array());
	$sign = 0;
	foreach ($request['sources'] as $counter) {
		if (!isset($rows[$counter])) {
			dimension_allocation_fail($result, 'source_missing', sprintf(_('Ledger row #%d does not exist.'), $counter));
			continue;
		}
		$row = $rows[$counter];
		if ((int)$row['voided'] === 1 || (float)$row['amount'] == 0) {
			dimension_allocation_fail($result, 'source_void', sprintf(_('Ledger row #%d is voided or has no amount.'), $counter));
			continue;
		}
		$has_person = $row['person_type_id'] !== null && (string)$row['person_id'] !== '';
		if ($has_person || is_subledger_account($row['account']) || is_bank_account($row['account'])
			|| is_tax_account($row['account'])) {
			dimension_allocation_fail($result, 'source_control', sprintf(_('Ledger row #%d belongs to a customer, supplier, bank or tax control account and cannot be allocated.'), $counter));
			continue;
		}
		if (isset($reserved[$counter])) {
			dimension_allocation_fail($result, 'source_reserved', sprintf(_('Ledger row #%d is already part of allocation run #%d.'), $counter, $reserved[$counter]));
			continue;
		}
		$row_sign = (float)$row['amount'] > 0 ? 1 : -1;
		if ($sign !== 0 && $sign !== $row_sign)
			dimension_allocation_fail($result, 'mixed_sign', _('The source pool mixes debit and credit rows; allocate them in separate runs.'));
		$sign = $row_sign;
	}
	return $result;
}

/**
 * Build the deterministic preview and balancing journal lines of one run.
 *
 * @param array $request Normalized request
 * @param array $rows Counter => stored source row (already checked)
 * @return array ok, errors, codes, run_key, rows (per source: links, shares, residual), lines (journal lines),
 *   totals, journal_sha256
 */
function dimension_allocation_build_preview($request, $rows)
{
	$result = array('ok' => true, 'errors' => array(), 'codes' => array(), 'run_key' => dimension_allocation_run_key($request),
		'rows' => array(), 'lines' => array(), 'totals' => array(), 'journal_sha256' => '');
	$scale = dimension_allocation_scale();
	$axis = (int)$request['axis'];
	$targets = $request['targets'];
	$pool = 0;
	$sign = 0;
	foreach ($request['sources'] as $counter) {
		if (!isset($rows[$counter])) {
			dimension_allocation_fail($result, 'source_missing', sprintf(_('Ledger row #%d does not exist.'), $counter));
			return $result;
		}
		$pool += (int)round(abs((float)$rows[$counter]['amount']) * $scale);
		$sign = (float)$rows[$counter]['amount'] > 0 ? 1 : -1;
	}
	if ($request['mode'] === 'amount') {
		$sum = 0;
		foreach ($targets as $target)
			$sum += $target['value'];
		if ($sum !== $pool)
			return dimension_allocation_fail($result, 'amount_total', sprintf(_('The fixed amounts total %s but the source pool is %s; they must be equal.'),
				dimension_allocation_format_minor($sum), dimension_allocation_format_minor($pool)));
	}
	$remaining = array();
	foreach ($targets as $index => $target)
		$remaining[$index] = $target['value'];
	$memo = 'DIM-ALC '.substr($result['run_key'], 0, 12).' '.$request['kind'];
	$keyed = array();
	$order = array();
	$add = function ($account, $dimension_id, $dimension2_id, $minor) use (&$keyed, &$order) {
		$key = $account.'|'.$dimension_id.'|'.$dimension2_id;
		if (!isset($keyed[$key])) {
			$keyed[$key] = array('code_id' => (string)$account, 'dimension_id' => (int)$dimension_id,
				'dimension2_id' => (int)$dimension2_id, 'minor' => 0);
			$order[] = $key;
		}
		$keyed[$key]['minor'] += $minor;
	};
	foreach ($request['sources'] as $counter) {
		$row = $rows[$counter];
		$minor = (int)round(abs((float)$row['amount']) * $scale);
		$source1 = (int)$row['dimension_id'];
		$source2 = (int)$row['dimension2_id'];
		$shares = array();
		if ($request['mode'] === 'percent') {
			// Largest-remainder rounding: every share is floored exactly, then the leftover minor units go one
			// each to the largest remainders (ties to the lowest Dimension), so the shares total the row exactly.
			$given = 0;
			$remainders = array();
			foreach ($targets as $index => $target) {
				$parts = dimension_allocation_share_parts($minor, $target['value']);
				$shares[$index] = $parts['floor'];
				$remainders[$index] = $parts['remainder'];
				$given += $parts['floor'];
			}
			$residual = $minor - $given;
			$ranked = array_keys($remainders);
			usort($ranked, function ($left, $right) use ($remainders) {
				return $remainders[$left] === $remainders[$right] ? $left - $right : ($remainders[$left] > $remainders[$right] ? -1 : 1);
			});
			for ($extra = 0; $extra < $residual; $extra++)
				$shares[$ranked[$extra]]++;
		} else {
			$need = $minor;
			$residual = 0;
			foreach ($targets as $index => $target) {
				$take = min($need, $remaining[$index]);
				$shares[$index] = $take;
				$remaining[$index] -= $take;
				$need -= $take;
			}
		}
		$entry = array('counter' => $counter, 'type' => (int)$row['type'], 'type_no' => (int)$row['type_no'],
			'tran_date' => $row['tran_date'], 'account' => (string)$row['account'], 'dimension_id' => $source1,
			'dimension2_id' => $source2, 'amount_minor' => $sign * $minor, 'residual_minor' => $residual, 'shares' => array());
		$add($row['account'], $source1, $source2, -$sign * $minor);
		foreach ($targets as $index => $target) {
			$target1 = $axis === 1 ? $target['dimension_id'] : $source1;
			$target2 = $axis === 2 ? $target['dimension_id'] : $source2;
			if ($target1 === $source1 && $target2 === $source2)
				return dimension_allocation_fail($result, 'target_is_source', sprintf(_('Tracking value #%d is already the classification of ledger row #%d; choose another target.'),
					$target['dimension_id'], $counter));
			$entry['shares'][] = array('dimension_id' => $target['dimension_id'], 'minor' => $shares[$index]);
			if ($shares[$index] > 0)
				$add($row['account'], $target1, $target2, $sign * $shares[$index]);
		}
		$result['rows'][] = $entry;
	}
	$lines = array();
	$check = 0;
	foreach ($order as $key) {
		if ($keyed[$key]['minor'] === 0)
			continue;
		$line = $keyed[$key];
		$check += $line['minor'];
		$lines[] = array('code_id' => $line['code_id'], 'dimension_id' => $line['dimension_id'],
			'dimension2_id' => $line['dimension2_id'], 'amount' => round($line['minor'] / $scale, dimension_allocation_decimals()),
			'minor' => $line['minor'], 'memo' => $memo);
	}
	if ($check !== 0 || !count($lines))
		return dimension_allocation_fail($result, 'unbalanced', _('The allocation does not balance; nothing was prepared.'));
	// DIM-FND-004 / FND-007: every line must be a valid new-event classification for its own account.
	$classification = dimension_validation_gl_cart($lines, 'new_event');
	if (!$classification['ok']) {
		foreach ($classification['errors'] as $error)
			dimension_allocation_fail($result, 'classification', $error);
		return $result;
	}
	$result['lines'] = $lines;
	$result['totals'] = array('source_pool_minor' => $sign * $pool, 'target_minor' => $sign * $pool, 'net_minor' => $check,
		'sign' => $sign);
	$result['journal_sha256'] = dimension_allocation_lines_digest($lines);
	return $result;
}

/**
 * Are both evidence tables installed in the active company?
 *
 * @return bool|string True, 'absent' or false when it cannot be answered
 */
function dimension_allocation_schema_ready()
{
	// db_escape() would HTML-encode the prefix placeholder, so the physical names are quoted literally.
	$rows = db_query("SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME IN ('"
		.TB_PREF."dimension_allocation_runs','".TB_PREF."dimension_allocation_sources')");
	$row = $rows ? db_fetch_row($rows) : false;
	if (!$row)
		return false;
	return (int)$row[0] === 2 ? true : 'absent';
}

/** Is a SQL date inside an open fiscal year (not closed for new entries)? */
function dimension_allocation_date_open($sql_date)
{
	return (bool)is_date_in_fiscalyear(sql2date($sql_date));
}

/** Read one run row, optionally locked. */
function dimension_allocation_get_run($run_id, $lock = false)
{
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_allocation_runs WHERE run_id='.(int)$run_id.($lock ? ' FOR UPDATE' : ''));
	return $rows ? db_fetch_assoc($rows) : false;
}

/** Decode a stored run payload. */
function dimension_allocation_run_payload($run)
{
	$payload = is_array($run) && isset($run['request_payload']) ? json_decode($run['request_payload'], true) : null;
	return is_array($payload) ? $payload : false;
}

/** Release the live identity and source reservations of one run (state evidence only). */
function dimension_allocation_release_run($run_id, $status, $reason)
{
	$ok = db_query('UPDATE '.TB_PREF.'dimension_allocation_runs SET status='.db_escape($status).', active_key=NULL, active_inverse_of=NULL,'
		.' failure_reason='.db_escape(substr((string)$reason, 0, 255)).', resolved_at=NOW() WHERE run_id='.(int)$run_id
		." AND status='pending'");
	if ($ok === false)
		return false;
	return db_query('UPDATE '.TB_PREF.'dimension_allocation_sources SET active_counter=NULL WHERE run_id='.(int)$run_id) !== false;
}

/**
 * Release every pending run whose approval draft was rejected, cancelled or expired.
 *
 * The approval hooks do this atomically; this reconciliation makes the state converge even when a draft ended
 * through another path. A pending run without a draft is left alone (it is being posted).
 *
 * @return int|false Number of released runs
 */
function dimension_allocation_reconcile_pending()
{
	$rows = db_query('SELECT r.run_id, d.status FROM '.TB_PREF.'dimension_allocation_runs r INNER JOIN '.TB_PREF.'approval_drafts d'
		.' ON d.id=r.approval_draft_id WHERE r.status='."'pending'".' AND d.status IN ('.(int)APPROVAL_STATUS_REJECTED.','
		.(int)APPROVAL_STATUS_CANCELLED.','.(int)APPROVAL_STATUS_EXPIRED.')');
	if ($rows === false)
		return false;
	$ended = array();
	while ($row = db_fetch_assoc($rows))
		$ended[(int)$row['run_id']] = (int)$row['status'];
	$names = array(APPROVAL_STATUS_REJECTED => 'rejected', APPROVAL_STATUS_CANCELLED => 'cancelled', APPROVAL_STATUS_EXPIRED => 'cancelled');
	foreach ($ended as $run_id => $draft_status)
		if (!dimension_allocation_release_run($run_id, $names[$draft_status], _('The approval draft ended without posting.')))
			return false;
	return count($ended);
}

/**
 * Create (or find) the run of one normalized request.
 *
 * @param array $request Normalized request
 * @param callable|null $link Optional evidence writer called with the new run id inside the same transaction
 *   (DIM-ALC-002 driver link); a false result rolls the run back
 * @param array|null $extra Optional additional payload members stored with the run (the `driver` block)
 * @return array ok, duplicate (existing live run), run_id, preview, errors, codes
 */
function dimension_allocation_create_run($request, $link = null, $extra = null)
{
	$result = array('ok' => false, 'duplicate' => false, 'run_id' => 0, 'preview' => null, 'errors' => array(), 'codes' => array());
	if (!dimension_allocation_date_open($request['date'])) {
		dimension_allocation_fail($result, 'period_closed', _('The allocation date is out of the fiscal year or in a closed period.'));
		return $result;
	}
	if (dimension_allocation_reconcile_pending() === false) {
		dimension_allocation_fail($result, 'unavailable', _('The allocation state could not be read.'));
		return $result;
	}
	$key = dimension_allocation_run_key($request);
	if (!begin_transaction()) {
		dimension_allocation_fail($result, 'unavailable', _('The allocation could not start a transaction.'));
		return $result;
	}
	$existing = db_query('SELECT run_id FROM '.TB_PREF.'dimension_allocation_runs WHERE active_key='.db_escape($key).' FOR UPDATE');
	$existing = $existing ? db_fetch_assoc($existing) : false;
	if ($existing) {
		cancel_transaction();
		$result['ok'] = true;
		$result['duplicate'] = true;
		$result['run_id'] = (int)$existing['run_id'];
		return $result;
	}
	$rows = dimension_allocation_load_sources($request['sources'], true);
	$reserved = $rows === false ? false : dimension_allocation_reservations($request['sources'], true);
	if ($rows === false || $reserved === false) {
		cancel_transaction();
		dimension_allocation_fail($result, 'unavailable', _('The source ledger rows could not be read.'));
		return $result;
	}
	$sources = dimension_allocation_check_sources($request, $rows, $reserved);
	if (!$sources['ok']) {
		cancel_transaction();
		return array_merge($result, array('errors' => $sources['errors'], 'codes' => $sources['codes']));
	}
	$preview = dimension_allocation_build_preview($request, $rows);
	if (!$preview['ok']) {
		cancel_transaction();
		return array_merge($result, array('errors' => $preview['errors'], 'codes' => $preview['codes']));
	}
	$members = array('request' => $request, 'rows' => $preview['rows'], 'lines' => $preview['lines']);
	if (is_array($extra))
		foreach ($extra as $member => $value)
			if (!isset($members[$member]))
				$members[$member] = $value;
	$payload = json_encode($members);
	$inserted = db_query('INSERT INTO '.TB_PREF.'dimension_allocation_runs (run_key, active_key, run_kind, axis, rule_mode, tran_date,'
		.' status, source_total, request_payload, journal_sha256, actor_id, created_at) VALUES ('.db_escape($key).','.db_escape($key).','
		.db_escape($request['kind']).','.(int)$request['axis'].','.db_escape($request['mode']).','.db_escape($request['date']).",'pending',"
		.db_escape(round(abs($preview['totals']['source_pool_minor']) / dimension_allocation_scale(), dimension_allocation_decimals())).','
		.dimension_allocation_quote($payload).','.db_escape($preview['journal_sha256']).','.current_user_id_or_zero().',NOW())');
	$run_id = $inserted ? (int)db_insert_id() : 0;
	$linked = $run_id > 0;
	if ($linked)
		foreach ($request['sources'] as $counter)
			if (db_query('INSERT INTO '.TB_PREF.'dimension_allocation_sources (run_id, gl_counter, active_counter, account, source_amount)'
				.' VALUES ('.$run_id.','.(int)$counter.','.(int)$counter.','.db_escape($rows[$counter]['account']).','
				.db_escape($rows[$counter]['amount']).')') === false)
				$linked = false;
	if ($linked && $link !== null && !call_user_func($link, $run_id))
		$linked = false;
	if (!$linked || !commit_transaction()) {
		cancel_transaction();
		dimension_allocation_fail($result, 'unavailable', _('The allocation run could not be recorded; nothing was posted.'));
		return $result;
	}
	$result['ok'] = true;
	$result['run_id'] = $run_id;
	$result['preview'] = $preview;
	return $result;
}

/**
 * Build the unsaved journal cart of a recorded run.
 *
 * @param array $run Run row
 * @param array $lines Canonical journal lines
 * @param string $memo Document memo
 * @return object Journal cart
 */
function dimension_allocation_build_cart($run, $lines, $memo)
{
	global $Refs;
	$cart = new items_cart(ST_JOURNAL);
	$display_date = sql2date($run['tran_date']);
	$cart->tran_date = $display_date;
	$cart->doc_date = $display_date;
	$cart->event_date = $display_date;
	$cart->reference = $Refs->get_next(ST_JOURNAL, null, $display_date);
	$cart->source_ref = 'DIM-ALC:'.substr($run['run_key'], 0, 16);
	$cart->memo_ = $memo;
	$cart->currency = get_company_pref('curr_default');
	$cart->rate = 1;
	$cart->tax_info = false;
	foreach ($lines as $line)
		$cart->add_gl_item($line['code_id'], $line['dimension_id'], $line['dimension2_id'], $line['amount'], $line['memo']);
	return $cart;
}

/**
 * Post (or submit for approval) the journal of one recorded run.
 *
 * @param int $run_id Recorded pending run
 * @return array state (posted|pending|failed), run_id, trans_no, draft_id, message
 */
function dimension_allocation_dispatch($run_id)
{
	$outcome = array('state' => 'failed', 'run_id' => (int)$run_id, 'trans_no' => 0, 'draft_id' => 0, 'message' => '');
	$run = dimension_allocation_get_run($run_id);
	$payload = dimension_allocation_run_payload($run);
	if (!$run || $run['status'] !== 'pending' || !$payload || !isset($payload['lines'])
		|| dimension_allocation_lines_digest($payload['lines']) !== $run['journal_sha256']) {
		$outcome['message'] = _('The allocation run is not in a postable state.');
		return $outcome;
	}
	if (isset($payload['driver'])) {
		// DIM-ALC-002: a superseded, changed or out-of-period driver makes the run stale; nothing is posted.
		$guard = function_exists('dimension_driver_run_guard') ? dimension_driver_run_guard($run, $payload) : array('ok' => false, 'text' => '');
		if (!$guard['ok']) {
			dimension_allocation_release_run($run_id, 'failed', isset($guard['code']) ? $guard['code'] : 'driver_stale');
			$outcome['message'] = $guard['text'] !== '' ? $guard['text'] : _('The driver of the run is no longer usable.');
			return $outcome;
		}
	}
	$memo = ($run['run_kind'] === 'inverse' ? _('Exact inverse of allocation run').' #'.(int)$run['inverse_of_run_id']
		: _('Allocation run').' #'.(int)$run['run_id']).(isset($payload['request']['memo']) && $payload['request']['memo'] !== ''
		? ' - '.$payload['request']['memo'] : '');
	try {
		$cart = dimension_allocation_build_cart($run, $payload['lines'], $memo);
		$draft_data = collect_items_cart_data($cart);
		$draft_data['dimension_allocation'] = array('run_id' => (int)$run['run_id'], 'run_key' => $run['run_key'],
			'journal_sha256' => $run['journal_sha256']);
		$amount = abs($cart->gl_items_total_debit());
		$service = get_approval_workflow_service();
		if ($service->isApprovalRequired(ST_JOURNAL, $amount)) {
			$submitted = $service->submit(ST_JOURNAL, $draft_data, $amount, array('summary' => sprintf(_('Cost allocation journal: %s'),
				$cart->reference), 'currency' => $cart->currency));
			if ($submitted['status'] === 'auto_approved') {
				$fresh = dimension_allocation_get_run($run_id);
				$outcome['state'] = $fresh && $fresh['status'] === 'posted' ? 'posted' : 'failed';
				$outcome['trans_no'] = (int)$submitted['trans_no'];
				return $outcome;
			}
			if ($submitted['status'] === 'pending') {
				$outcome['state'] = 'pending';
				$outcome['draft_id'] = (int)$submitted['draft_id'];
				return $outcome;
			}
			if ($submitted['status'] !== 'no_workflow') {
				dimension_allocation_release_run($run_id, 'failed', 'approval:'.$submitted['status']);
				$outcome['message'] = sprintf(_('The approval submission was refused (%s). Nothing was posted.'), $submitted['status']);
				return $outcome;
			}
		}
		if (!begin_transaction())
			throw new RuntimeException('transaction');
		$trans_no = write_journal_entries($cart);
		if (!$trans_no || !dimension_allocation_confirm_posted($draft_data, $trans_no, 0)) {
			cancel_transaction();
			dimension_allocation_release_run($run_id, 'failed', 'post_refused');
			$outcome['message'] = _('The journal could not be posted. Nothing was changed; the run was released and may be retried.');
			return $outcome;
		}
		if (!commit_transaction())
			throw new RuntimeException('commit');
		$outcome['state'] = 'posted';
		$outcome['trans_no'] = (int)$trans_no;
		return $outcome;
	} catch (Exception $exception) {
		cancel_transaction();
		dimension_allocation_release_run($run_id, 'failed', substr($exception->getMessage(), 0, 200));
		$outcome['message'] = $exception->getMessage();
		return $outcome;
	}
}

/**
 * Record, validate and dispatch one allocation request (the guided "run" step).
 *
 * @param array $request Raw request (see dimension_allocation_normalize_request)
 * @param callable|null $link Optional evidence writer run inside the run transaction (see dimension_allocation_create_run)
 * @param array|null $extra Optional additional payload members (the DIM-ALC-002 `driver` block)
 * @return array ok, state, duplicate, run_id, trans_no, errors, codes
 */
function dimension_allocation_submit($request, $link = null, $extra = null)
{
	$result = array('ok' => false, 'state' => 'refused', 'duplicate' => false, 'run_id' => 0, 'trans_no' => 0, 'errors' => array(),
		'codes' => array());
	$normal = dimension_allocation_normalize_request($request);
	if (!$normal['ok'])
		return array_merge($result, array('errors' => $normal['errors'], 'codes' => $normal['codes']));
	$created = dimension_allocation_create_run($normal['request'], $link, $extra);
	if (!$created['ok'])
		return array_merge($result, array('errors' => $created['errors'], 'codes' => $created['codes']));
	$result['run_id'] = $created['run_id'];
	if ($created['duplicate']) {
		$run = dimension_allocation_get_run($created['run_id']);
		$result['ok'] = true;
		$result['duplicate'] = true;
		$result['state'] = $run ? $run['status'] : 'pending';
		$result['trans_no'] = $run && $run['journal_trans_no'] !== null ? (int)$run['journal_trans_no'] : 0;
		return $result;
	}
	$outcome = dimension_allocation_dispatch($created['run_id']);
	$result['state'] = $outcome['state'];
	$result['trans_no'] = $outcome['trans_no'];
	$result['ok'] = $outcome['state'] !== 'failed';
	if (!$result['ok'])
		$result['errors'][] = $outcome['message'];
	return $result;
}

/** Re-submit the stored request of a failed, rejected or cancelled run as a new run. */
function dimension_allocation_retry($run_id)
{
	$run = dimension_allocation_get_run($run_id);
	$payload = dimension_allocation_run_payload($run);
	if (!$run || !$payload || !in_array($run['status'], array('failed', 'rejected', 'cancelled'), true)
		|| $run['run_kind'] === 'inverse' || !isset($payload['request']))
		return array('ok' => false, 'state' => 'refused', 'duplicate' => false, 'run_id' => 0, 'trans_no' => 0,
			'errors' => array(_('Only a failed, rejected or cancelled allocation run can be retried.')), 'codes' => array('not_retryable'));
	if (isset($payload['driver'])) {
		// DIM-ALC-002: a driver run is retried against the CURRENT driver state; a stale driver refuses the retry.
		return function_exists('dimension_driver_retry') ? dimension_driver_retry($payload)
			: array('ok' => false, 'state' => 'refused', 'duplicate' => false, 'run_id' => 0, 'trans_no' => 0,
				'errors' => array(_('Driver runs cannot be retried here.')), 'codes' => array('not_retryable'));
	}
	$request = $payload['request'];
	foreach ($request['targets'] as $index => $target)
		$request['targets'][$index]['value'] = $request['mode'] === 'percent' ? $target['value'] / 10000
			: $target['value'] / dimension_allocation_scale();
	return dimension_allocation_submit($request);
}

/**
 * Create (or find) the exact inverse run of a posted allocation.
 *
 * @param int $run_id Posted original run
 * @param string $date Inverse date (Y-m-d)
 * @param string $memo Optional memo
 * @return array ok, duplicate, state, run_id, trans_no, errors, codes
 */
function dimension_allocation_submit_inverse($run_id, $date, $memo = '')
{
	$result = array('ok' => false, 'state' => 'refused', 'duplicate' => false, 'run_id' => 0, 'trans_no' => 0, 'errors' => array(),
		'codes' => array());
	if (!is_string($date) || !preg_match('/^([0-9]{4})-([0-9]{2})-([0-9]{2})$/D', $date, $match)
		|| !checkdate((int)$match[2], (int)$match[3], (int)$match[1]))
		return dimension_allocation_fail($result, 'invalid_date', _('The inverse date is invalid.'));
	if (!dimension_allocation_date_open($date))
		return dimension_allocation_fail($result, 'period_closed', _('The inverse date is out of the fiscal year or in a closed period.'));
	if (dimension_allocation_reconcile_pending() === false)
		return dimension_allocation_fail($result, 'unavailable', _('The allocation state could not be read.'));
	if (!begin_transaction())
		return dimension_allocation_fail($result, 'unavailable', _('The inverse could not start a transaction.'));
	$original = dimension_allocation_get_run($run_id, true);
	if (!$original || $original['status'] !== 'posted' || $original['run_kind'] === 'inverse' || !(int)$original['journal_trans_no']) {
		cancel_transaction();
		return dimension_allocation_fail($result, 'not_posted', _('Only a posted allocation run can be reversed.'));
	}
	$live = db_query('SELECT run_id FROM '.TB_PREF.'dimension_allocation_runs WHERE active_inverse_of='.(int)$run_id.' FOR UPDATE');
	$live = $live ? db_fetch_assoc($live) : false;
	if ($live) {
		cancel_transaction();
		$result['ok'] = true;
		$result['duplicate'] = true;
		$result['run_id'] = (int)$live['run_id'];
		$existing = dimension_allocation_get_run($live['run_id']);
		$result['state'] = $existing ? $existing['status'] : 'pending';
		$result['trans_no'] = $existing && $existing['journal_trans_no'] !== null ? (int)$existing['journal_trans_no'] : 0;
		return $result;
	}
	$rows = db_query('SELECT g.counter, g.account, g.dimension_id, g.dimension2_id, g.amount, g.memo_, IF(v.date_ IS NULL, 0, 1) AS voided FROM '
		.TB_PREF.'gl_trans g LEFT JOIN '.TB_PREF.'voided v ON v.type=g.type AND v.id=g.type_no WHERE g.type='.(int)ST_JOURNAL
		.' AND g.type_no='.(int)$original['journal_trans_no'].' ORDER BY g.counter FOR UPDATE');
	$lines = array();
	$intact = $rows !== false;
	$check = 0;
	$scale = dimension_allocation_scale();
	while ($rows && ($row = db_fetch_assoc($rows))) {
		if ((int)$row['voided'] === 1 || (float)$row['amount'] == 0) {
			$intact = false;
			break;
		}
		$minor = -(int)round((float)$row['amount'] * $scale);
		$check += $minor;
		$lines[] = array('code_id' => (string)$row['account'], 'dimension_id' => (int)$row['dimension_id'],
			'dimension2_id' => (int)$row['dimension2_id'], 'amount' => round($minor / $scale, dimension_allocation_decimals()),
			'minor' => $minor, 'memo' => 'DIM-ALC inverse of run #'.(int)$run_id, 'inverse_of_counter' => (int)$row['counter']);
	}
	if (!$intact || !count($lines) || $check !== 0) {
		cancel_transaction();
		return dimension_allocation_fail($result, 'journal_changed', _('The posted allocation journal was voided or changed, so an exact inverse is not possible.'));
	}
	$classification = dimension_validation_gl_cart($lines, 'new_event');
	if (!$classification['ok']) {
		cancel_transaction();
		$result['errors'] = $classification['errors'];
		$result['codes'][] = 'classification';
		return $result;
	}
	$key = hash('sha256', json_encode(array('alc1-inverse', (int)$run_id)));
	$payload = json_encode(array('request' => array('kind' => 'inverse', 'date' => $date, 'memo' => trim((string)$memo),
		'inverse_of_run_id' => (int)$run_id), 'lines' => $lines));
	$inserted = db_query('INSERT INTO '.TB_PREF.'dimension_allocation_runs (run_key, active_key, run_kind, axis, rule_mode, tran_date, status,'
		.' inverse_of_run_id, active_inverse_of, source_total, request_payload, journal_sha256, actor_id, created_at) VALUES ('
		.db_escape($key).','.db_escape($key).",'inverse',0,'inverse',".db_escape($date).",'pending',".(int)$run_id.','.(int)$run_id.','
		.db_escape($original['source_total']).','.dimension_allocation_quote($payload).','.db_escape(dimension_allocation_lines_digest($lines)).','
		.current_user_id_or_zero().',NOW())');
	$new_id = $inserted ? (int)db_insert_id() : 0;
	if (!$new_id || !commit_transaction()) {
		cancel_transaction();
		return dimension_allocation_fail($result, 'unavailable', _('The inverse run could not be recorded; nothing was posted.'));
	}
	$result['run_id'] = $new_id;
	$outcome = dimension_allocation_dispatch($new_id);
	$result['state'] = $outcome['state'];
	$result['trans_no'] = $outcome['trans_no'];
	$result['ok'] = $outcome['state'] !== 'failed';
	if (!$result['ok'])
		$result['errors'][] = $outcome['message'];
	return $result;
}

/**
 * Confirm one posted journal against its run, inside the posting transaction.
 *
 * Called by the approval executor and by the direct path. A draft without an allocation marker is not an
 * allocation journal and passes unchanged. A marked draft must match the stored digest, the posted ledger
 * rows must equal the recorded lines exactly, and an inverse must carry the exact original rows.
 *
 * @param array $draft_data Draft or request data
 * @param int $trans_no Posted journal number
 * @param int $draft_id Approval draft (0 for the direct path)
 * @return bool False aborts the owning transaction
 */
function dimension_allocation_confirm_posted($draft_data, $trans_no, $draft_id)
{
	if (!is_array($draft_data) || !isset($draft_data['dimension_allocation']))
		return true;
	$marker = $draft_data['dimension_allocation'];
	if (!is_array($marker) || !isset($marker['run_id']) || !(int)$trans_no)
		return false;
	$run = dimension_allocation_get_run((int)$marker['run_id'], true);
	$payload = dimension_allocation_run_payload($run);
	if (!$run || !$payload || $run['status'] !== 'pending' || !isset($marker['run_key']) || $marker['run_key'] !== $run['run_key']
		|| !isset($marker['journal_sha256']) || $marker['journal_sha256'] !== $run['journal_sha256']
		|| ($draft_id && $run['approval_draft_id'] !== null && (int)$run['approval_draft_id'] !== (int)$draft_id))
		return false;
	$lines = array();
	if (!isset($draft_data['gl_items']) || !is_array($draft_data['gl_items']))
		return false;
	foreach ($draft_data['gl_items'] as $item)
		$lines[] = array('code_id' => $item['code_id'], 'dimension_id' => $item['dimension_id'],
			'dimension2_id' => $item['dimension2_id'], 'amount' => $item['amount']);
	if (dimension_allocation_lines_digest($lines) !== $run['journal_sha256'])
		return false;
	$posted = db_query('SELECT account, dimension_id, dimension2_id, amount FROM '.TB_PREF.'gl_trans WHERE type='.(int)ST_JOURNAL
		.' AND type_no='.(int)$trans_no.' ORDER BY counter');
	if ($posted === false)
		return false;
	$stored = array();
	while ($row = db_fetch_assoc($posted))
		$stored[] = array('code_id' => $row['account'], 'dimension_id' => $row['dimension_id'],
			'dimension2_id' => $row['dimension2_id'], 'amount' => $row['amount']);
	if (dimension_allocation_lines_digest($stored) !== $run['journal_sha256'])
		return false;
	if (isset($payload['driver'])) {
		// DIM-ALC-002: the driver must still be the approved, unchanged, in-period version when the journal posts.
		if (!function_exists('dimension_driver_run_guard'))
			return false;
		$guard = dimension_driver_run_guard($run, $payload, true);
		if (!$guard['ok'])
			return false;
	}
	if (db_query('UPDATE '.TB_PREF."dimension_allocation_runs SET status='posted', journal_trans_no=".(int)$trans_no
		.', approval_draft_id='.($draft_id ? (int)$draft_id : 'approval_draft_id').', resolved_at=NOW() WHERE run_id='
		.(int)$run['run_id']." AND status='pending'") === false)
		return false;
	if ($run['run_kind'] === 'inverse') {
		// The original allocation is reversed: its source rows and its identity become available again.
		$original = (int)$run['inverse_of_run_id'];
		if (db_query('UPDATE '.TB_PREF.'dimension_allocation_runs SET active_key=NULL WHERE run_id='.$original) === false
			|| db_query('UPDATE '.TB_PREF.'dimension_allocation_sources SET active_counter=NULL WHERE run_id='.$original) === false)
			return false;
	}
	return true;
}

/**
 * Approval hook: bind the run to the submitted draft inside the submission transaction.
 *
 * @param array $data Draft data
 * @param int $draft_id New draft
 * @return bool False aborts the submission
 */
function dimension_allocation_approval_submitted($data, $draft_id)
{
	if (!is_array($data) || !isset($data['dimension_allocation']['run_id']))
		return true;
	$run = dimension_allocation_get_run((int)$data['dimension_allocation']['run_id'], true);
	if (!$run || $run['status'] !== 'pending' || !isset($data['dimension_allocation']['run_key'])
		|| $data['dimension_allocation']['run_key'] !== $run['run_key'])
		return false;
	return db_query('UPDATE '.TB_PREF.'dimension_allocation_runs SET approval_draft_id='.(int)$draft_id.' WHERE run_id='
		.(int)$run['run_id']." AND status='pending'") !== false;
}

/**
 * Approval hook: release a run whose draft was rejected or cancelled.
 *
 * @param array $draft_data Draft data
 * @param int $draft_id Draft
 * @param string $comments Reviewer comment
 * @return bool False aborts the rejection
 */
function dimension_allocation_approval_released($draft_data, $draft_id, $comments = '')
{
	if (!is_array($draft_data) || !isset($draft_data['dimension_allocation']['run_id']))
		return true;
	$run = dimension_allocation_get_run((int)$draft_data['dimension_allocation']['run_id'], true);
	if (!$run)
		return false;
	if ($run['status'] !== 'pending')
		return true;
	return dimension_allocation_release_run($run['run_id'], 'rejected', $comments !== '' ? $comments : _('The approval draft ended without posting.'));
}

/** Derived display state of a run: pending, posted, reversed, rejected, cancelled or failed. */
function dimension_allocation_display_state($run, $has_live_inverse)
{
	if ($run['status'] === 'posted' && $run['run_kind'] !== 'inverse' && $has_live_inverse)
		return 'reversed';
	return $run['status'];
}

/**
 * Recent runs with their derived state.
 *
 * @param int $limit Maximum rows
 * @return array|false
 */
function dimension_allocation_recent_runs($limit = 50)
{
	$rows = db_query('SELECT r.*, (SELECT COUNT(*) FROM '.TB_PREF.'dimension_allocation_runs i WHERE i.inverse_of_run_id=r.run_id'
		." AND i.status='posted') AS inverse_posted FROM ".TB_PREF.'dimension_allocation_runs r ORDER BY r.run_id DESC LIMIT '.(int)$limit);
	if ($rows === false)
		return false;
	$runs = array();
	while ($row = db_fetch_assoc($rows)) {
		$row['display_state'] = dimension_allocation_display_state($row, (int)$row['inverse_posted'] > 0);
		$runs[] = $row;
	}
	return $runs;
}

include_once(dirname(__FILE__).'/tracking_allocation_driver.inc');
