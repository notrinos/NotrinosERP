<?php
/**
 * DIM-FA-001: effective-dated asset responsibility and depreciation/disposal attribution.
 *
 * A fixed asset is an item (`stock_master.mb_flag = 'F'`). Its location and its current item default
 * do not establish who is responsible for its cost, so this adapter keeps an explicit, prospective,
 * append-only history of responsibility revisions per asset:
 *
 *  - a responsibility revision is a durable DIM-FND-006 source revision of the `inventory` family
 *    (anchor `INVENTORY_CLASSIFICATION_ASSET_RESPONSIBILITY_ANCHOR`, one identity per asset, one
 *    revision per responsibility, effective date = the first day it applies). It is stored through the
 *    shared resolver (explicit selection or explicit blank, never inferred) and can never be backdated:
 *    its effective date must follow both the last depreciation already posted and the previous
 *    responsibility, so earlier postings and their categories are never re-attributed;
 *  - every depreciation month takes the pair of the responsibility in force on its own date, so a
 *    change A -> B moves only later months. Both journal legs (the depreciation expense and the
 *    accumulated-depreciation contra account) carry that pair; the expense leg is no longer posted on
 *    `0/0`. Each depreciation journal is snapshotted once, line per month, and a depreciation register
 *    revision (anchor `INVENTORY_CLASSIFICATION_ASSET_DEPRECIATION_ANCHOR`) records the months posted
 *    so a month can never be depreciated twice;
 *  - an asset that never had a responsibility freezes its item default once, at its first depreciation,
 *    as a new event: the later change of that default can never re-code the asset;
 *  - a disposal through an inventory adjustment resolves the asset's responsibility in force on the
 *    event date instead of the item default (see `inventory_classification_item_defaults()`).
 *
 * Acquisition, capitalisation and sale keep their purchasing/sales lineage (DIM-PUR-003, DIM-SAL-002):
 * gross asset value is a balance-sheet movement and is never reported as expense.
 *
 * The adapter reads the ledger but contains no INSERT/UPDATE/DELETE, no DDL and no ledger or stock
 * call of its own, never backfills a historical posting and writes only source revisions through the
 * shared authority. An install without the accepted companion schema posts exactly as before.
 *
 * PHP 5.6 compatible.
 */
require_once __DIR__.'/../../../inventory/includes/db/inventory_event_classification_db.inc';

if (!defined('FIXED_ASSET_RESPONSIBILITY_SECURITY_AREA'))
	define('FIXED_ASSET_RESPONSIBILITY_SECURITY_AREA', 'SA_DEPRECIATION');
if (!defined('FIXED_ASSET_RESPONSIBILITY_MAX_REVISIONS'))
	define('FIXED_ASSET_RESPONSIBILITY_MAX_REVISIONS', 200);

/**
 * Is the accepted companion revision schema installed so that asset responsibility can be recorded?
 *
 * @return bool
 */
function fixed_asset_responsibility_ready()
{
	return inventory_classification_schema_ready();
}

/**
 * May the active user maintain asset responsibility?
 *
 * @return bool
 */
function fixed_asset_responsibility_authorized()
{
	return function_exists('user_check_access') && user_check_access(FIXED_ASSET_RESPONSIBILITY_SECURITY_AREA);
}

/**
 * Read the canonical row of one fixed asset.
 *
 * @param mixed $stock_id Item code
 * @return array|null stock_id, accounts and depreciation start of an existing fixed asset, else null
 */
function fixed_asset_responsibility_asset($stock_id)
{
	if (!is_string($stock_id) || $stock_id === '' || strlen($stock_id) > 20)
		return null;
	$rows = db_query('SELECT stock_id, mb_flag, cogs_account, adjustment_account, inventory_account, purchase_cost
		FROM '.TB_PREF.'stock_master WHERE stock_id='.db_escape($stock_id));
	$row = $rows ? db_fetch_assoc($rows) : false;
	return $row && $row['mb_flag'] === 'F' ? $row : null;
}

/**
 * Stable per-asset line identity inside one anchor.
 *
 * @param string $stock_id Canonical item code
 * @return string|false line key
 */
function fixed_asset_responsibility_line_key($stock_id)
{
	return dimension_source_revision_line_key('line', 'a'.substr(hash('sha256', 'fixed-asset|'.$stock_id), 0, 40));
}

/**
 * Read every verified revision of one asset identity in revision order.
 *
 * @param string $stock_id Canonical item code
 * @param int $anchor Responsibility or depreciation-register anchor
 * @return array list of revision_no, revision_id, axis pair (1, 2), effective (ISO) and evidence
 * @throws DimensionFinancialReferenceWriteException A present but unverifiable revision
 */
function fixed_asset_responsibility_chain($stock_id, $anchor)
{
	$chain = array();
	if (!fixed_asset_responsibility_ready())
		return $chain;
	$line_key = fixed_asset_responsibility_line_key($stock_id);
	if ($line_key === false)
		return $chain;
	for ($number = 1; $number <= FIXED_ASSET_RESPONSIBILITY_MAX_REVISIONS; $number++) {
		$snapshot = dimension_source_revision_snapshot('inventory', $anchor, '1', 'line', $line_key,
			array('revision_no' => $number));
		if ($snapshot['state'] === 'absent' || $snapshot['state'] === 'unavailable')
			break;
		if ($snapshot['state'] !== 'available')
			throw new DimensionFinancialReferenceWriteException(
				_('The recorded asset responsibility cannot be verified. Nothing was saved.'));
		$revision = $snapshot['revision'];
		$resolution = json_decode($revision['resolution_payload'], true);
		$chain[] = array('revision_no' => $number, 'revision_id' => (int)$revision['source_revision_id'],
			1 => (int)$revision['dimension_id'], 2 => (int)$revision['dimension2_id'],
			'effective' => (string)$revision['effective_date'],
			'evidence' => is_array($resolution) && isset($resolution['evidence']) ? $resolution['evidence'] : array());
	}
	return $chain;
}

/**
 * Responsibility revisions of one asset, oldest first.
 *
 * @param string $stock_id Canonical item code
 * @return array
 * @throws DimensionFinancialReferenceWriteException
 */
function fixed_asset_responsibility_history($stock_id)
{
	return fixed_asset_responsibility_chain($stock_id, INVENTORY_CLASSIFICATION_ASSET_RESPONSIBILITY_ANCHOR);
}

/**
 * The responsibility in force on one date.
 *
 * @param string $stock_id Canonical item code
 * @param string $date_iso ISO date
 * @return array|null revision entry, or null when no responsibility was effective yet
 * @throws DimensionFinancialReferenceWriteException
 */
function fixed_asset_responsibility_at($stock_id, $date_iso)
{
	$found = null;
	foreach (fixed_asset_responsibility_history($stock_id) as $entry)
		if ($entry['effective'] <= $date_iso && ($found === null || $entry['effective'] >= $found['effective']))
			$found = $entry;
	return $found;
}

/**
 * Is one journal voided?
 *
 * @param mixed $journal_no Journal number
 * @return bool
 */
function fixed_asset_responsibility_journal_voided($journal_no)
{
	$number = dimension_source_revision_int($journal_no, 1);
	if ($number === false)
		return false;
	$rows = db_query('SELECT id FROM '.TB_PREF.'voided WHERE type='.(int)ST_JOURNAL.' AND id='.$number);
	return $rows && db_fetch_assoc($rows) !== false;
}

/**
 * Last month-end already depreciated through a live (not voided) depreciation journal.
 *
 * @param string $stock_id Canonical item code
 * @return string|null ISO date
 * @throws DimensionFinancialReferenceWriteException
 */
function fixed_asset_depreciation_watermark($stock_id)
{
	$through = null;
	foreach (fixed_asset_responsibility_chain($stock_id, INVENTORY_CLASSIFICATION_ASSET_DEPRECIATION_ANCHOR) as $entry) {
		$journal = isset($entry['evidence']['journal']) ? $entry['evidence']['journal'] : 0;
		if (fixed_asset_responsibility_journal_voided($journal))
			continue;
		if ($through === null || $entry['effective'] > $through)
			$through = $entry['effective'];
	}
	return $through;
}

/**
 * Build the header carrier of the responsibility page from its posted category selectors.
 *
 * @return stdClass|false Carrier, or false for a conflicting posted pair (selected and unassigned)
 */
function fixed_asset_responsibility_carrier_from_post()
{
	$carrier = new stdClass();
	$intents = inventory_classification_intents_from_values(
		get_post('far_dimension_id', 0), get_post('far_dimension2_id', 0),
		get_post('_far_classification_unassigned_1', 0), get_post('_far_classification_unassigned_2', 0),
		'asset_responsibility');
	if ($intents === false)
		return false;
	$carrier->classification_intents = $intents;
	return $carrier;
}

/**
 * Resolve one responsibility pair through the shared resolver.
 *
 * Every enabled category needs an explicit decision (a selection or an explicit blank): neither the
 * item default nor the asset location may stand in for a responsibility.
 *
 * @param object|null $carrier Header carrier holding `classification_intents`
 * @param array $evidence Bounded scalar evidence
 * @return array ok result of inventory_classification_resolve_entry()
 */
function fixed_asset_responsibility_resolve_explicit($carrier, $evidence)
{
	$enabled = (int)get_company_pref('use_dimension');
	foreach (array(1, 2) as $axis) {
		if ($axis > $enabled)
			continue;
		$intent = inventory_classification_carried_intent($carrier, $axis, 'header_snapshot', 'header_explicit');
		if (!is_array($intent) || empty($intent['ok']) || !in_array($intent['intent']['mode'], array('selected', 'unassigned'), true))
			return array('ok' => false, 'errors' => array(sprintf(
				_('Choose a category or explicitly leave it unassigned for category %d: responsibility is never inferred.'), $axis)),
				'codes' => array('responsibility_not_explicit'));
	}
	$evidence = is_array($evidence) ? $evidence : array();
	$evidence['event'] = 'asset_responsibility';
	return inventory_classification_resolve_entry(INVENTORY_CLASSIFICATION_ASSET_RESPONSIBILITY_ANCHOR,
		is_object($carrier) ? $carrier : new stdClass(), null, false, array('evidence' => $evidence));
}

/**
 * Preview a prospective responsibility change without writing anything.
 *
 * The preview names the responsibility in force, the boundary before which every posting keeps its
 * categories, and every reason the change would be refused (retroactive date, closed period, closed
 * category, an unchanged pair).
 *
 * @param string $stock_id Item code
 * @param string $effective_date First day the new responsibility applies, in the display format
 * @param object|null $carrier Header carrier holding the explicit decision
 * @return array ok, errors, pair, effective, in_force, depreciated_through, resolved
 */
function fixed_asset_responsibility_preview($stock_id, $effective_date, $carrier)
{
	$result = array('ok' => false, 'errors' => array(), 'pair' => null, 'effective' => null, 'in_force' => null,
		'depreciated_through' => null, 'resolved' => null);
	if (!fixed_asset_responsibility_ready()) {
		$result['errors'][] = _('The Dimension source revision schema is not installed; asset responsibility cannot be recorded.');
		return $result;
	}
	$asset = fixed_asset_responsibility_asset($stock_id);
	if ($asset === null) {
		$result['errors'][] = _('The selected item is not a fixed asset.');
		return $result;
	}
	$stock_id = $asset['stock_id'];
	$effective = is_string($effective_date) ? date2sql($effective_date) : false;
	if ($effective === false || dimension_source_revision_date($effective) === false) {
		$result['errors'][] = _('The responsibility effective date is not a valid date.');
		return $result;
	}
	$result['effective'] = $effective;
	if (!is_date_in_fiscalyear($effective_date)) {
		$result['errors'][] = _('The responsibility effective date is in a closed or unavailable period.');
		return $result;
	}
	try {
		$history = fixed_asset_responsibility_history($stock_id);
		$through = fixed_asset_depreciation_watermark($stock_id);
	} catch (DimensionFinancialReferenceWriteException $exception) {
		$result['errors'][] = $exception->getMessage();
		return $result;
	}
	$result['depreciated_through'] = $through;
	if ($through !== null && $effective <= $through) {
		$result['errors'][] = sprintf(_('Responsibility is prospective: depreciation is already posted through %s.'), $through);
		return $result;
	}
	$latest = count($history) ? $history[count($history) - 1] : null;
	if ($latest !== null) {
		$result['in_force'] = $latest;
		if ($effective <= $latest['effective']) {
			$result['errors'][] = sprintf(_('A new responsibility must start after the previous one (%s).'), $latest['effective']);
			return $result;
		}
	}
	$evidence = array('stock_id' => (string)$stock_id, 'previous_revision' => $latest === null ? 0 : $latest['revision_id'],
		'effective' => $effective);
	$resolved = fixed_asset_responsibility_resolve_explicit($carrier, $evidence);
	if (empty($resolved['ok'])) {
		$result['errors'] = array_merge($result['errors'], isset($resolved['errors']) ? $resolved['errors'] : array());
		return $result;
	}
	$pair = array(1 => (int)$resolved['axes'][1]['value'], 2 => (int)$resolved['axes'][2]['value']);
	if ($latest !== null && $latest[1] === $pair[1] && $latest[2] === $pair[2]) {
		$result['errors'][] = _('The asset already carries this responsibility; nothing changes.');
		return $result;
	}
	$result['ok'] = true;
	$result['pair'] = $pair;
	$result['resolved'] = $resolved;
	return $result;
}

/**
 * Record a prospective responsibility revision of one asset.
 *
 * @param string $stock_id Item code
 * @param string $effective_date First day the new responsibility applies, in the display format
 * @param object|null $carrier Header carrier holding the explicit decision
 * @param string $reason Mandatory reason sealed with the revision
 * @return array pair, revision_id, effective
 * @throws DimensionFinancialReferenceWriteException A refused change; nothing is written
 */
function fixed_asset_responsibility_record($stock_id, $effective_date, $carrier, $reason)
{
	if (!fixed_asset_responsibility_authorized())
		throw new DimensionFinancialReferenceWriteException(_('You are not authorized to change asset responsibility.'));
	$reason = dimension_source_revision_clean_text($reason, 200);
	if ($reason === null || $reason === '')
		throw new DimensionFinancialReferenceWriteException(_('A responsibility change needs a reason.'));
	$preview = fixed_asset_responsibility_preview($stock_id, $effective_date, $carrier);
	if (!$preview['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $preview['errors']));
	$asset = fixed_asset_responsibility_asset($stock_id);
	$resolved = $preview['resolved'];
	$resolved['resolution']['evidence']['reason'] = $reason;
	$line_key = fixed_asset_responsibility_line_key($asset['stock_id']);
	if ($line_key === false)
		throw new DimensionFinancialReferenceWriteException(_('The asset responsibility identity is invalid.'));
	if (!begin_transaction())
		throw new DimensionFinancialReferenceWriteException(_('The asset responsibility change could not start a transaction.'));
	try {
		$stored = inventory_classification_persist_revision(INVENTORY_CLASSIFICATION_ASSET_RESPONSIBILITY_ANCHOR, '1',
			'line', $line_key, $resolved['resolution'], $preview['effective'], '0.00000000');
		if (!$stored['ok'])
			throw new DimensionFinancialReferenceWriteException(implode(' ', $stored['errors']));
		commit_transaction();
	} catch (DimensionFinancialReferenceWriteException $exception) {
		cancel_transaction();
		throw $exception;
	}
	return array('pair' => $preview['pair'], 'revision_id' => (int)$stored['revision_id'], 'effective' => $preview['effective']);
}

/**
 * Resolve the pair of one depreciation month and refuse a pair that cannot post.
 *
 * @param string $stock_id Canonical item code
 * @param array|null $pair Pair carried by the responsibility, or null for a new item-default event
 * @param string $lineage Lineage sealed into the evidence
 * @return array ok result of inventory_classification_resolve_entry()
 */
function fixed_asset_depreciation_resolve_month($stock_id, $pair, $lineage)
{
	$line = inventory_event_classification_line($stock_id, 1, $pair, array('lineage' => $lineage));
	$evidence = $line->classification_evidence;
	$evidence['lineage'] = $lineage;
	$context = array('evidence' => $evidence);
	if ($pair !== null)
		$context['original'] = array(1 => $pair[1], 2 => $pair[2]);
	return inventory_classification_resolve_entry(ST_JOURNAL, new stdClass(), $line, true, $context);
}

/**
 * Plan the pairs of a depreciation run without writing anything.
 *
 * Refuses a run that touches a month already depreciated (duplicate processing) and a month whose pair
 * cannot be admitted. An asset without any responsibility freezes its item default once, at the first
 * month of its first run (`freeze`).
 *
 * @param string $stock_id Item code
 * @param array $gl_rows compute_gl_rows_for_depreciation() rows (date in the display format, value)
 * @return array ok, errors, legacy (no companion schema), rows (date, iso, value, pair, revision, lineage), freeze
 */
function fixed_asset_depreciation_plan($stock_id, $gl_rows)
{
	$plan = array('ok' => false, 'errors' => array(), 'legacy' => false, 'rows' => array(), 'freeze' => null);
	if (!fixed_asset_responsibility_ready()) {
		$plan['ok'] = true;
		$plan['legacy'] = true;
		return $plan;
	}
	$asset = fixed_asset_responsibility_asset($stock_id);
	if ($asset === null) {
		$plan['errors'][] = _('The selected item is not a fixed asset.');
		return $plan;
	}
	$stock_id = $asset['stock_id'];
	$rows = array();
	foreach ($gl_rows as $row) {
		$iso = date2sql($row['date']);
		if (dimension_source_revision_date($iso) === false) {
			$plan['errors'][] = _('A depreciation month has an invalid date.');
			return $plan;
		}
		$rows[] = array('date' => $row['date'], 'iso' => $iso, 'value' => $row['value']);
	}
	if (!count($rows)) {
		$plan['errors'][] = _('There is no depreciation to post.');
		return $plan;
	}
	try {
		$through = fixed_asset_depreciation_watermark($stock_id);
		if ($through !== null)
			foreach ($rows as $row)
				if ($row['iso'] <= $through) {
					$plan['errors'][] = sprintf(_('Depreciation is already posted through %s; nothing was saved.'), $through);
					return $plan;
				}
		$history = fixed_asset_responsibility_history($stock_id);
	} catch (DimensionFinancialReferenceWriteException $exception) {
		$plan['errors'][] = $exception->getMessage();
		return $plan;
	}
	$first = $rows[0]['iso'];
	foreach ($rows as $row)
		if ($row['iso'] < $first)
			$first = $row['iso'];
	if (!count($history)) {
		// A new event: the item default is the weakest source and is frozen once as responsibility #1.
		$resolved = fixed_asset_depreciation_resolve_month($stock_id, null, 'item_default_first_depreciation');
		if (!$resolved['ok']) {
			$plan['errors'] = array_merge($plan['errors'], $resolved['errors']);
			return $plan;
		}
		$freeze = inventory_classification_item_defaults($stock_id);
		$freeze['effective'] = $first;
		$plan['freeze'] = $freeze;
	}
	foreach ($rows as $row) {
		$entry = null;
		foreach ($history as $candidate)
			if ($candidate['effective'] <= $row['iso'] && ($entry === null || $candidate['effective'] >= $entry['effective']))
				$entry = $candidate;
		if ($entry !== null) {
			$row['pair'] = array(1 => $entry[1], 2 => $entry[2]);
			$row['revision'] = $entry['revision_id'];
			$row['lineage'] = 'asset_responsibility';
		} elseif ($plan['freeze'] !== null) {
			$row['pair'] = array(1 => $plan['freeze'][1], 2 => $plan['freeze'][2]);
			$row['revision'] = 0;
			$row['lineage'] = 'item_default_first_depreciation';
		} else {
			// A responsibility exists but starts later: this month keeps the item default as a new event.
			$defaults = inventory_classification_item_defaults($stock_id);
			$row['pair'] = array(1 => $defaults[1], 2 => $defaults[2]);
			$row['revision'] = 0;
			$row['lineage'] = 'item_default_before_responsibility';
		}
		$resolved = fixed_asset_depreciation_resolve_month($stock_id, $row['pair'], $row['lineage']);
		if (!$resolved['ok']) {
			$plan['errors'] = array_merge($plan['errors'], $resolved['errors']);
			return $plan;
		}
		$plan['rows'][] = $row;
	}
	$plan['ok'] = true;
	return $plan;
}

/**
 * Freeze the item default of an asset that never had a responsibility, as responsibility #1.
 *
 * @param string $stock_id Canonical item code
 * @param array $freeze plan `freeze` pair with its effective ISO date
 * @return int Revision id
 * @throws DimensionFinancialReferenceWriteException
 */
function fixed_asset_responsibility_freeze_default($stock_id, $freeze)
{
	$intents = inventory_classification_intents_from_values($freeze[1], $freeze[2],
		$freeze[1] > 0 ? 0 : 1, $freeze[2] > 0 ? 0 : 1, 'asset_responsibility');
	$carrier = new stdClass();
	if ($intents !== false)
		$carrier->classification_intents = $intents;
	$resolved = fixed_asset_responsibility_resolve_explicit($carrier,
		array('stock_id' => (string)$stock_id, 'previous_revision' => 0, 'effective' => $freeze['effective'],
			'lineage' => 'item_default_first_depreciation'));
	if (empty($resolved['ok']))
		throw new DimensionFinancialReferenceWriteException(implode(' ', $resolved['errors']));
	$line_key = fixed_asset_responsibility_line_key($stock_id);
	$stored = inventory_classification_persist_revision(INVENTORY_CLASSIFICATION_ASSET_RESPONSIBILITY_ANCHOR, '1',
		'line', $line_key, $resolved['resolution'], $freeze['effective'], '0.00000000');
	if (!$stored['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $stored['errors']));
	return (int)$stored['revision_id'];
}

/**
 * Snapshot a posted depreciation journal once (one line per month) and register the months it covers.
 *
 * Called inside the posting transaction right after the journal was written, so a refusal rolls the
 * whole depreciation back. The journal header carries the pair in force at its last month; each line
 * carries the exact pair of its month and names the responsibility revision it followed.
 *
 * @param string $stock_id Canonical item code
 * @param mixed $journal_no Posted journal number
 * @param string $date_ Journal date in the display format
 * @param array $plan fixed_asset_depreciation_plan() result
 * @return array|null capture result, or null on an install without the companion schema
 * @throws DimensionFinancialReferenceWriteException
 */
function fixed_asset_depreciation_record($stock_id, $journal_no, $date_, $plan)
{
	if (!fixed_asset_responsibility_ready() || !empty($plan['legacy']) || !count($plan['rows']))
		return null;
	$carriers = array();
	foreach ($plan['rows'] as $row) {
		$carrier = inventory_event_classification_line($stock_id, 1, $row['pair'], array(
			'role_expense' => 'depreciation_expense', 'role_contra' => 'accumulated_depreciation',
			'month_end' => $row['iso'], 'amount' => inventory_classification_amount_text($row['value'])));
		$carrier->classification_evidence['lineage'] = $row['lineage'];
		$carrier->classification_evidence['responsibility_revision'] = (int)$row['revision'];
		$carriers[] = $carrier;
	}
	$last = $plan['rows'][count($plan['rows']) - 1];
	$header = new stdClass();
	$intents = inventory_classification_intents_from_values($last['pair'][1], $last['pair'][2],
		$last['pair'][1] > 0 ? 0 : 1, $last['pair'][2] > 0 ? 0 : 1, 'asset_responsibility');
	if ($intents !== false)
		$header->classification_intents = $intents;
	$captured = inventory_event_classification_capture(ST_JOURNAL, $journal_no, $date_, $carriers,
		'fixed_asset_depreciation', array('stock_id' => (string)$stock_id, 'months' => count($carriers)), $header);

	// Register the posted months so that duplicate processing is refused and the report can find them.
	$register = array();
	foreach ($plan['rows'] as $row)
		$register[] = array($row['iso'], $row['pair'][1], $row['pair'][2], inventory_classification_amount_text($row['value']),
			(int)$row['revision']);
	$resolved = inventory_classification_resolve_entry(INVENTORY_CLASSIFICATION_ASSET_DEPRECIATION_ANCHOR, $header, null,
		false, array('evidence' => array('event' => 'asset_depreciation_register', 'stock_id' => (string)$stock_id,
			'journal' => (int)$journal_no, 'rows' => array_slice($register, 0, 120))));
	if (!$resolved['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $resolved['errors']));
	$stored = inventory_classification_persist_revision(INVENTORY_CLASSIFICATION_ASSET_DEPRECIATION_ANCHOR, '1', 'line',
		fixed_asset_responsibility_line_key($stock_id), $resolved['resolution'], $last['iso'], '0.00000000');
	if (!$stored['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $stored['errors']));
	return $captured;
}

/**
 * Report the classified depreciation, disposal and acquisition value of one asset.
 *
 * Depreciation expense is only what the depreciation journals posted to an expense/cost-of-sales
 * account; the accumulated-depreciation contra leg and the gross asset value are balance-sheet
 * amounts and are never reported as expense. Voided journals contribute nothing (their ledger rows are
 * zeroed). Amounts are read from the ledger, never from the sealed evidence.
 *
 * @param string $stock_id Item code
 * @param string|null $from ISO date, inclusive (null = unbounded)
 * @param string|null $to ISO date, inclusive (null = unbounded)
 * @return array|null null for an unknown asset
 */
function fixed_asset_responsibility_report($stock_id, $from = null, $to = null)
{
	$asset = fixed_asset_responsibility_asset($stock_id);
	if ($asset === null)
		return null;
	$report = array('stock_id' => $asset['stock_id'], 'from' => $from, 'to' => $to,
		'gross_asset_value' => number_format((float)$asset['purchase_cost'], 2, '.', ''),
		'responsibility' => array(), 'journals' => array(), 'expense' => array('lifetime' => '0.00', 'period' => '0.00', 'pairs' => array()),
		'accumulated' => '0.00', 'reconciles' => true);
	try {
		$report['responsibility'] = fixed_asset_responsibility_history($asset['stock_id']);
		$register = fixed_asset_responsibility_chain($asset['stock_id'], INVENTORY_CLASSIFICATION_ASSET_DEPRECIATION_ANCHOR);
	} catch (DimensionFinancialReferenceWriteException $exception) {
		return $report;
	}
	$lifetime = 0.0;
	$period = 0.0;
	$accumulated = 0.0;
	$pairs = array();
	foreach ($register as $entry) {
		$journal = isset($entry['evidence']['journal']) ? (int)$entry['evidence']['journal'] : 0;
		if ($journal < 1)
			continue;
		$rows = db_query('SELECT gl.account, gl.dimension_id, gl.dimension2_id, gl.tran_date, gl.amount, cl.ctype
			FROM '.TB_PREF.'gl_trans gl
			LEFT JOIN '.TB_PREF.'chart_master cm ON cm.account_code=gl.account
			LEFT JOIN '.TB_PREF.'chart_types ct ON ct.id=cm.account_type
			LEFT JOIN '.TB_PREF.'chart_class cl ON cl.cid=ct.class_id
			WHERE gl.type='.(int)ST_JOURNAL.' AND gl.type_no='.$journal.' AND gl.amount<>0');
		$count = 0;
		while ($rows && ($row = db_fetch_assoc($rows))) {
			$amount = (float)$row['amount'];
			$in_period = ($from === null || $row['tran_date'] >= $from) && ($to === null || $row['tran_date'] <= $to);
			if ((string)$row['account'] === (string)$asset['cogs_account'] && (int)$row['ctype'] >= 5) {
				$lifetime += $amount;
				if ($in_period)
					$period += $amount;
				$pair = (int)$row['dimension_id'].':'.(int)$row['dimension2_id'];
				$pairs[$pair] = (isset($pairs[$pair]) ? $pairs[$pair] : 0.0) + $amount;
			} elseif ((string)$row['account'] === (string)$asset['adjustment_account']) {
				$accumulated += $amount;
			}
			$count++;
		}
		$report['journals'][] = array('journal' => $journal, 'revision' => $entry['revision_id'],
			'through' => $entry['effective'], 'voided' => fixed_asset_responsibility_journal_voided($journal), 'legs' => $count);
	}
	$report['expense'] = array('lifetime' => number_format($lifetime, 2, '.', ''), 'period' => number_format($period, 2, '.', ''), 'pairs' => array());
	foreach ($pairs as $pair => $amount)
		$report['expense']['pairs'][$pair] = number_format($amount, 2, '.', '');
	$report['accumulated'] = number_format($accumulated, 2, '.', '');
	$report['reconciles'] = abs($lifetime + $accumulated) < 0.005;
	return $report;
}
