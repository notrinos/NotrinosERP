<?php
/**
 * DIM-ADV-001: advisory purchase commitments, atomic funds reservations and forecast evidence.
 *
 * Purchase order totals, receipts and bills are different stages of one spend; the GL actual only exists once a bill (or a
 * receipt that posts) has been recorded. This authority keeps the stages apart and never changes any of the owning documents:
 *
 *  - the COMMITMENT REGISTER observes the lines of the current purchase orders and states, per line and in company currency at
 *    the order date, how much is still OPEN (ordered, not yet received), RECEIVED (received, not yet billed) and BILLED. The
 *    remaining commitment is OPEN + RECEIVED; billing a quantity moves it from the commitment stages to the billed stage
 *    without ever raising the combined consumption (actual + remaining commitment). The stage values are derived from the
 *    source quantities, so observing twice (or a duplicate bill) cannot move a stage twice;
 *  - a purchase order that is cancelled disappears from the purchasing tables, so the register detects the vanished line and
 *    RELEASES its remaining commitment exactly once (a unique effect key); a closed-short quantity releases only the delta;
 *  - the FUNDS POSITION of an account/scope/period shows budget, GL actual, remaining commitments and open reservations next
 *    to each other (budget - actual - commitments - reservations = free funds). Commitments are ADVISORY evidence, never a GL
 *    actual, and a line whose rate, tax basis or account cannot be resolved is listed as unresolved and excluded from totals;
 *  - RESERVATIONS are atomic claims on free funds (one funds lock serializes every claim, a repeat of the same reference with
 *    the same payload answers with the existing claim). They are refused until an authorized user separately activates the
 *    reservation mode (append-only audit); no purchasing writer is wired to enforce them, so enforcement is a deliberate,
 *    separate activation;
 *  - every register row carries a state digest; a row that no longer matches its digest is never used and is reported.
 *
 * No purchase order, GRN, invoice, budget_trans, gl_trans or Dimension master row is written or re-pointed by this authority, a
 * commitment that was never observed is never inferred and the Dimension classification of a line is read from its durable
 * source revision (absent lineage reads as unassigned and is flagged). PHP 5.6 compatible syntax. Dates are SQL `Y-m-d`.
 */
require_once(__DIR__.'/../../includes/value_helpers.inc');
include_once dirname(__FILE__).'/tracking_budget_basis.inc';
include_once dirname(__FILE__).'/tracking_source_revision.inc';

/** Largest number of purchase order lines one observation may register. */
function dimension_commit_max_lines()
{
	return 100000;
}

/** Largest absolute amount a reservation may carry. */
function dimension_commit_max_amount()
{
	return 1.0E+11;
}

/** Does the session grant at least one of the named areas (denies when the helper is unavailable)? */
function dimension_commit_has_access($areas)
{
	if (!function_exists('user_check_access'))
		return false;
	foreach ((array)$areas as $area)
		if (user_check_access($area))
			return true;
	return false;
}

/** Quote free text for the evidence tables without the page-oriented HTML encoding of `db_escape()`. */
function dimension_commit_quote($value)
{
	return "'".db_escape_function((string)$value)."'";
}

/** A fresh result skeleton (it starts ok; every refusal goes through dimension_commit_fail). */
function dimension_commit_result($extra = array())
{
	return array_merge(array('ok' => true, 'errors' => array(), 'codes' => array()), $extra);
}

/** Record one stable reason code and its readable text on a result. */
function dimension_commit_fail(&$result, $code, $text)
{
	$result['ok'] = false;
	if (!in_array($code, $result['codes'], true))
		$result['codes'][] = $code;
	$result['errors'][] = $text;
	return $result;
}

/**
 * Parse an amount or quantity to six decimals.
 *
 * @param mixed $value int, float or canonical decimal string
 * @param bool $allow_negative Accept a negative number
 * @return float|false
 */
function dimension_commit_parse_amount($value, $allow_negative = false)
{
	if (is_int($value) || is_float($value))
		$number = (float)$value;
	elseif (is_string($value) && preg_match('/^-?(0|[1-9][0-9]{0,11})(\.[0-9]{1,6})?$/D', $value))
		$number = (float)$value;
	else
		return false;
	if (!is_finite($number) || abs($number) > dimension_commit_max_amount() || ($number < 0 && !$allow_negative))
		return false;
	$number = round($number, 6);
	return $number == 0 ? 0.0 : $number;
}

/** Canonical text of an amount, used by every digest and statement. */
function dimension_commit_amount_text($amount)
{
	$amount = round((float)$amount, 6);
	return sprintf('%.6F', $amount == 0 ? 0.0 : $amount);
}

/** Are the six commitment/forecast evidence tables of the active company installed? */
function dimension_commit_schema_ready($refresh = false)
{
	static $cache = array();
	$key = TB_PREF;
	if (!$refresh && isset($cache[$key]))
		return $cache[$key];
	$names = array('dimension_commitment_lines', 'dimension_commitment_events', 'dimension_commitment_reservations', 'dimension_commitment_activations',
		'dimension_forecast_versions', 'dimension_forecast_lines');
	$rows = db_query("SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME IN ('".TB_PREF
		.implode("','".TB_PREF, $names)."')");
	$row = $rows ? db_fetch_row($rows) : false;
	if (!$row)
		return false;
	$cache[$key] = (int)$row[0] === count($names) ? true : 'absent';
	return $cache[$key];
}

/** Forget the per-request schema cache (tests and long-running scripts). */
function dimension_commit_cache_reset()
{
	dimension_commit_schema_ready(true);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Pure stage arithmetic
// ---------------------------------------------------------------------------------------------------------------------------------

/**
 * The stages of one purchase order line (pure).
 *
 * Quantities are clamped so the stages always add up to the ordered amount: billing is never more than ordered, receiving is at
 * least the billed quantity (a bill before the receipt counts as received) and never more than ordered. The excess of an
 * over-receipt or an over-bill is not a commitment; it is actual spend and lives in the GL.
 *
 * @param float $ordered Ordered quantity
 * @param float $received Received quantity
 * @param float $billed Billed quantity
 * @param float $unit Net unit amount in company currency
 * @param string $state open|cancelled
 * @param float $released Remaining commitment released by a cancellation (stored)
 * @return array ordered, billed, received (received, not billed), open (not received), remaining, released
 */
function dimension_commit_stage_amounts($ordered, $received, $billed, $unit, $state, $released = 0.0)
{
	$ordered = max(0.0, (float)$ordered);
	$billed_qty = min(max(0.0, (float)$billed), $ordered);
	$received_qty = min(max((float)$received, $billed_qty), $ordered);
	$unit = (float)$unit;
	$stages = array('ordered' => round($ordered * $unit, 6), 'billed' => round($billed_qty * $unit, 6),
		'received' => round(($received_qty - $billed_qty) * $unit, 6), 'open' => round(($ordered - $received_qty) * $unit, 6),
		'remaining' => 0.0, 'released' => round((float)$released, 6));
	if ($state === 'cancelled') {
		$stages['received'] = 0.0;
		$stages['open'] = 0.0;
	}
	$stages['remaining'] = round($stages['received'] + $stages['open'], 6);
	return $stages;
}

/**
 * The net company-currency unit amount of a purchase order line (pure).
 *
 * @param float $price Unit price in the supplier's currency
 * @param float $net_price Price with included tax removed (equal to $price when the order excludes tax)
 * @param float|false $rate Supplier-currency units per company-currency unit at the order date (1 for the company currency)
 * @return float|false
 */
function dimension_commit_home_unit($price, $net_price, $rate)
{
	if ($rate === false || (float)$rate <= 0)
		return false;
	return round((float)$net_price / (float)$rate, 6);
}

/** State digest sealing one register row. */
function dimension_commit_state_sha($row)
{
	return hash('sha256', json_encode(array('adv1-line', (int)$row['po_detail_item'], (int)$row['order_no'], (string)$row['account_code'],
		(int)$row['coordinate_1'], (int)$row['coordinate_2'], (string)$row['lineage_state'], (string)$row['commit_date'],
		dimension_commit_amount_text($row['ordered_qty']), dimension_commit_amount_text($row['received_qty']), dimension_commit_amount_text($row['billed_qty']),
		dimension_commit_amount_text($row['unit_amount']), (string)$row['state'], dimension_commit_amount_text($row['released_amount']))));
}

/** Does a stored register row still match its digest? */
function dimension_commit_line_intact($row)
{
	if (!is_array($row) || !isset($row['state_sha256']))
		return false;
	foreach (array('po_detail_item', 'order_no', 'account_code', 'coordinate_1', 'coordinate_2', 'lineage_state', 'commit_date', 'ordered_qty', 'received_qty', 'billed_qty', 'unit_amount', 'state', 'released_amount') as $field)
		if (!array_key_exists($field, $row))
			return false;
	return hash_equals((string)$row['state_sha256'], dimension_commit_state_sha($row));
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Source resolution (read-only)
// ---------------------------------------------------------------------------------------------------------------------------------

/** The company currency, or '' when it cannot be read. */
function dimension_commit_company_currency()
{
	$rows = db_query("SELECT value FROM ".TB_PREF."sys_prefs WHERE name='curr_default'");
	$row = $rows ? db_fetch_row($rows) : false;
	return $row ? (string)$row[0] : '';
}

/**
 * Supplier-currency units per company-currency unit at an order date.
 *
 * @param string|null $currency Supplier currency
 * @param string $date Order date
 * @return float|false 1 for the company currency, the latest stored rate on or before the date, false when none exists
 */
function dimension_commit_exchange_rate($currency, $date)
{
	$home = dimension_commit_company_currency();
	if ($currency === null || $currency === '' || $currency === $home)
		return 1.0;
	$rows = db_query('SELECT rate_buy FROM '.TB_PREF.'exchange_rates WHERE curr_code='.dimension_commit_quote($currency).' AND date_<='
		.dimension_commit_quote($date).' ORDER BY date_ DESC LIMIT 1');
	$row = $rows ? db_fetch_row($rows) : false;
	return $row && (float)$row[0] > 0 ? (float)$row[0] : false;
}

/**
 * The Dimension coordinates of one purchase order line from its durable source revision (never from a current default).
 *
 * @param int $order_no Purchase order
 * @param int $line Purchase order line identity
 * @return array coordinate_1, coordinate_2, lineage_state revision|header|absent
 */
function dimension_commit_line_coordinates($order_no, $line)
{
	$out = array('coordinate_1' => 0, 'coordinate_2' => 0, 'lineage_state' => 'absent');
	if (!function_exists('dimension_source_revision_snapshot'))
		return $out;
	$trans_type = defined('ST_PURCHORDER') ? ST_PURCHORDER : 18;
	$document = dimension_source_revision_document_key($order_no);
	$line_key = dimension_source_revision_line_key('line', $line);
	if ($document === false || $line_key === false)
		return $out;
	foreach (array(array('line', $line_key, 'revision'), array('header', 'header', 'header')) as $try) {
		$snapshot = dimension_source_revision_snapshot('purchasing', $trans_type, $document, $try[0], $try[1]);
		if (!is_array($snapshot) || $snapshot['state'] !== 'available')
			continue;
		foreach (array(1, 2) as $axis) {
			$entry = $snapshot['revision']['axes'][$axis];
			$out['coordinate_'.$axis] = $entry['mode'] === 'selected' ? (int)$entry['value'] : 0;
		}
		$out['lineage_state'] = $try[2];
		return $out;
	}
	return $out;
}

/**
 * The purchase order lines to observe, with every fact needed to value them.
 *
 * @param int $order_no One order, or 0 for every order
 * @return array|false Rows, or false when the read fails
 */
function dimension_commit_source_rows($order_no = 0)
{
	$sql = 'SELECT d.po_detail_item, d.order_no, d.item_code, d.quantity_ordered, d.quantity_received, d.qty_invoiced, d.unit_price, o.ord_date,'
		.' o.tax_included, s.curr_code, s.tax_group_id, m.mb_flag, m.cogs_account, m.inventory_account FROM '.TB_PREF.'purch_order_details d INNER JOIN '
		.TB_PREF.'purch_orders o ON o.order_no=d.order_no LEFT JOIN '.TB_PREF.'suppliers s ON s.supplier_id=o.supplier_id LEFT JOIN '.TB_PREF
		.'stock_master m ON m.stock_id=d.item_code'.((int)$order_no > 0 ? ' WHERE d.order_no='.(int)$order_no : '').' ORDER BY d.po_detail_item';
	$result = db_query($sql, 'The purchase order lines could not be read');
	if (!$result)
		return false;
	$rows = array();
	while ($row = db_fetch_assoc($result))
		$rows[] = $row;
	return $rows;
}

/**
 * Value and classify one purchase order line.
 *
 * @param array $row Source row
 * @return array ok, code, text and (when ok) the register fields
 */
function dimension_commit_resolve_line($row)
{
	$fail = function ($code, $text) { return array('ok' => false, 'code' => $code, 'text' => $text); };
	$date = (string)$row['ord_date'];
	if (!is_iso_calendar_date($date))
		return $fail('order_date_invalid', _('The purchase order has no valid order date.'));
	foreach (array('quantity_ordered', 'quantity_received', 'qty_invoiced', 'unit_price') as $field)
		if (dimension_commit_parse_amount((float)$row[$field]) === false)
			return $fail('amount_invalid', _('The purchase order line holds a quantity or price that cannot be valued.'));
	$account = $row['mb_flag'] === 'D' ? (string)$row['cogs_account'] : (string)$row['inventory_account'];
	if ($account === '' || !preg_match('/^[\x21-\x7e]{1,15}$/D', $account))
		return $fail('account_missing', _('The item of this line has no account the spend will be posted to.'));
	$price = (float)$row['unit_price'];
	$net = $price;
	if ((int)$row['tax_included'] === 1) {
		if (!function_exists('get_tax_free_price_for_item'))
			return $fail('tax_unresolved', _('The tax-inclusive price of this line cannot be netted.'));
		$net = (float)get_tax_free_price_for_item($row['item_code'], $price, (int)$row['tax_group_id'], 1);
	}
	$rate = dimension_commit_exchange_rate($row['curr_code'], $date);
	if ($rate === false)
		return $fail('rate_missing', sprintf(_('No exchange rate exists for %s on or before %s.'), (string)$row['curr_code'], $date));
	$unit = dimension_commit_home_unit($price, $net, $rate);
	$coordinates = dimension_commit_line_coordinates($row['order_no'], $row['po_detail_item']);
	return array('ok' => true, 'code' => '', 'text' => '', 'account_code' => $account, 'coordinate_1' => $coordinates['coordinate_1'],
		'coordinate_2' => $coordinates['coordinate_2'], 'lineage_state' => $coordinates['lineage_state'], 'commit_date' => $date,
		'ordered_qty' => round((float)$row['quantity_ordered'], 6), 'received_qty' => round((float)$row['quantity_received'], 6),
		'billed_qty' => round((float)$row['qty_invoiced'], 6), 'unit_amount' => $unit);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Observation
// ---------------------------------------------------------------------------------------------------------------------------------

/** Take the observation lock that serializes register writers (returns false when it cannot be taken). */
function dimension_commit_lock($name)
{
	$rows = db_query('SELECT GET_LOCK('.dimension_commit_quote(TB_PREF.$name).', 10)');
	$row = $rows ? db_fetch_row($rows) : false;
	return $row && (int)$row[0] === 1;
}

/** Release a lock taken with dimension_commit_lock(). */
function dimension_commit_unlock($name)
{
	db_query('SELECT RELEASE_LOCK('.dimension_commit_quote(TB_PREF.$name).')');
}

/** Insert one append-only effect of a register row. */
function dimension_commit_write_event($commitment_id, $key, $type, $stages, $delta, $actor)
{
	return db_query('INSERT INTO '.TB_PREF.'dimension_commitment_events (commitment_id, event_key, event_type, ordered_amount, open_amount, received_amount,'
		.' billed_amount, released_amount, remaining_amount, delta_remaining, actor_id, created_at) VALUES ('.(int)$commitment_id.','.dimension_commit_quote($key).','
		.dimension_commit_quote($type).','.dimension_commit_amount_text($stages['ordered']).','.dimension_commit_amount_text($stages['open']).','
		.dimension_commit_amount_text($stages['received']).','.dimension_commit_amount_text($stages['billed']).','.dimension_commit_amount_text($stages['released']).','
		.dimension_commit_amount_text($stages['remaining']).','.dimension_commit_amount_text($delta).','.(int)$actor.',NOW())') !== false;
}

/**
 * Observe the current purchase order lines and update the commitment register.
 *
 * New lines are registered, changed stage quantities append one effect (an unchanged line appends nothing, so a repeat run is a
 * no-op) and a registered open line that no longer exists in purchasing is cancelled: its remaining commitment is released once.
 *
 * @param int $order_no One purchase order, or 0 for every order
 * @return array ok, registered, updated, unchanged, released, unresolved (list of line/code/text), errors, codes
 */
function dimension_commit_observe($order_no = 0)
{
	$result = dimension_commit_result(array('registered' => 0, 'updated' => 0, 'unchanged' => 0, 'released' => 0, 'unresolved' => array()));
	if (!dimension_commit_has_access(array('SA_BUDGETENTRY', 'SA_PURCHASEORDER')))
		return dimension_commit_fail($result, 'permission_denied', _('You are not allowed to observe purchase commitments.'));
	$actor = current_user_id_or_zero();
	if ($actor <= 0)
		return dimension_commit_fail($result, 'permission_denied', _('The observing user could not be identified.'));
	$order_no = (int)$order_no;
	if ($order_no < 0)
		return dimension_commit_fail($result, 'invalid_order', _('The purchase order number is not valid.'));
	if (dimension_commit_schema_ready() !== true)
		return dimension_commit_fail($result, 'schema_missing', _('The commitment tables are not installed. Upgrade the company database first.'));
	if (!dimension_commit_lock('dimension_commit_register'))
		return dimension_commit_fail($result, 'busy', _('Another commitment observation is running; try again.'));
	if (!begin_transaction()) {
		dimension_commit_unlock('dimension_commit_register');
		return dimension_commit_fail($result, 'unavailable', _('The observation could not start a transaction.'));
	}
	$done = dimension_commit_observe_locked($order_no, $actor, $result);
	if ($done && commit_transaction()) {
		dimension_commit_unlock('dimension_commit_register');
		return $result;
	}
	cancel_transaction();
	dimension_commit_unlock('dimension_commit_register');
	if ($result['ok'])
		dimension_commit_fail($result, 'unavailable', _('The commitment register could not be updated.'));
	return $result;
}

/** The body of dimension_commit_observe() inside the transaction and the lock; false means roll back. */
function dimension_commit_observe_locked($order_no, $actor, &$result)
{
	$source = dimension_commit_source_rows($order_no);
	if ($source === false || count($source) > dimension_commit_max_lines()) {
		dimension_commit_fail($result, $source === false ? 'unavailable' : 'too_many_lines', _('The purchase order lines could not be read within the supported size.'));
		return false;
	}
	$registered = array();
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_commitment_lines'.($order_no > 0 ? ' WHERE order_no='.(int)$order_no : '').' ORDER BY commitment_id FOR UPDATE');
	if ($rows === false)
		return false;
	while ($row = db_fetch_assoc($rows))
		$registered[(int)$row['po_detail_item']] = $row;
	$seen = array();
	foreach ($source as $line) {
		$id = (int)$line['po_detail_item'];
		$seen[$id] = true;
		$old = isset($registered[$id]) ? $registered[$id] : null;
		if ($old !== null && !dimension_commit_line_intact($old)) {
			$result['unresolved'][] = array('po_detail_item' => $id, 'code' => 'tampered', 'text' => _('The stored register row no longer matches its digest and is left untouched.'));
			continue;
		}
		if ($old !== null && $old['state'] === 'cancelled') {
			$result['unresolved'][] = array('po_detail_item' => $id, 'code' => 'cancelled_line_returned', 'text' => _('A cancelled commitment line reappeared in purchasing; it stays released.'));
			continue;
		}
		$resolved = dimension_commit_resolve_line($line);
		if (!$resolved['ok']) {
			$result['unresolved'][] = array('po_detail_item' => $id, 'code' => $resolved['code'], 'text' => $resolved['text']);
			continue;
		}
		$new = array('po_detail_item' => $id, 'order_no' => (int)$line['order_no'], 'account_code' => $resolved['account_code'],
			'coordinate_1' => $resolved['coordinate_1'], 'coordinate_2' => $resolved['coordinate_2'], 'lineage_state' => $resolved['lineage_state'],
			'commit_date' => $resolved['commit_date'], 'ordered_qty' => $resolved['ordered_qty'], 'received_qty' => $resolved['received_qty'],
			'billed_qty' => $resolved['billed_qty'], 'unit_amount' => $resolved['unit_amount'], 'state' => 'open', 'released_amount' => 0.0);
		$new['state_sha256'] = dimension_commit_state_sha($new);
		$stages = dimension_commit_stage_amounts($new['ordered_qty'], $new['received_qty'], $new['billed_qty'], $new['unit_amount'], 'open');
		if ($old === null) {
			if (!db_query('INSERT INTO '.TB_PREF.'dimension_commitment_lines (po_detail_item, order_no, account_code, coordinate_1, coordinate_2, lineage_state, commit_date,'
				.' ordered_qty, received_qty, billed_qty, unit_amount, state, released_amount, state_sha256, observed_at) VALUES ('.$id.','.(int)$new['order_no'].','
				.dimension_commit_quote($new['account_code']).','.(int)$new['coordinate_1'].','.(int)$new['coordinate_2'].','.dimension_commit_quote($new['lineage_state']).','
				.dimension_commit_quote($new['commit_date']).','.dimension_commit_amount_text($new['ordered_qty']).','.dimension_commit_amount_text($new['received_qty']).','
				.dimension_commit_amount_text($new['billed_qty']).','.dimension_commit_amount_text($new['unit_amount']).",'open',0,".dimension_commit_quote($new['state_sha256']).',NOW())'))
				return false;
			$commitment_id = (int)db_insert_id();
			if ($commitment_id <= 0 || !dimension_commit_write_event($commitment_id, 'observe', 'observe', $stages, $stages['remaining'], $actor))
				return false;
			$result['registered']++;
			continue;
		}
		if (hash_equals((string)$old['state_sha256'], $new['state_sha256'])) {
			$result['unchanged']++;
			continue;
		}
		$before = dimension_commit_stage_amounts($old['ordered_qty'], $old['received_qty'], $old['billed_qty'], $old['unit_amount'], 'open');
		$count = db_query('SELECT COUNT(*) FROM '.TB_PREF.'dimension_commitment_events WHERE commitment_id='.(int)$old['commitment_id']);
		$count = $count ? db_fetch_row($count) : false;
		if (!$count || !db_query('UPDATE '.TB_PREF.'dimension_commitment_lines SET account_code='.dimension_commit_quote($new['account_code']).', coordinate_1='.(int)$new['coordinate_1']
			.', coordinate_2='.(int)$new['coordinate_2'].', lineage_state='.dimension_commit_quote($new['lineage_state']).', commit_date='.dimension_commit_quote($new['commit_date'])
			.', ordered_qty='.dimension_commit_amount_text($new['ordered_qty']).', received_qty='.dimension_commit_amount_text($new['received_qty']).', billed_qty='
			.dimension_commit_amount_text($new['billed_qty']).', unit_amount='.dimension_commit_amount_text($new['unit_amount']).', state_sha256='.dimension_commit_quote($new['state_sha256'])
			.', observed_at=NOW() WHERE commitment_id='.(int)$old['commitment_id']))
			return false;
		$key = 'e'.((int)$count[0] + 1).':'.substr($new['state_sha256'], 0, 12);
		if (!dimension_commit_write_event((int)$old['commitment_id'], $key, 'stage', $stages, $stages['remaining'] - $before['remaining'], $actor))
			return false;
		$result['updated']++;
	}
	foreach ($registered as $id => $old) {
		if (isset($seen[$id]) || $old['state'] !== 'open')
			continue;
		if (!dimension_commit_line_intact($old)) {
			$result['unresolved'][] = array('po_detail_item' => $id, 'code' => 'tampered', 'text' => _('The stored register row no longer matches its digest and is left untouched.'));
			continue;
		}
		$before = dimension_commit_stage_amounts($old['ordered_qty'], $old['received_qty'], $old['billed_qty'], $old['unit_amount'], 'open');
		$old['state'] = 'cancelled';
		$old['released_amount'] = $before['remaining'];
		$sha = dimension_commit_state_sha($old);
		$stages = dimension_commit_stage_amounts($old['ordered_qty'], $old['received_qty'], $old['billed_qty'], $old['unit_amount'], 'cancelled', $before['remaining']);
		if (!db_query('UPDATE '.TB_PREF."dimension_commitment_lines SET state='cancelled', released_amount=".dimension_commit_amount_text($before['remaining']).', state_sha256='
			.dimension_commit_quote($sha).', observed_at=NOW() WHERE commitment_id='.(int)$old['commitment_id']." AND state='open'")
			|| !dimension_commit_write_event((int)$old['commitment_id'], 'release', 'release', $stages, -$before['remaining'], $actor))
			return false;
		$result['released']++;
	}
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Register and funds position
// ---------------------------------------------------------------------------------------------------------------------------------

/** Decorate a stored register row with its stage amounts. */
function dimension_commit_describe($row)
{
	$stages = dimension_commit_stage_amounts($row['ordered_qty'], $row['received_qty'], $row['billed_qty'], $row['unit_amount'], $row['state'], $row['released_amount']);
	return array_merge($row, array('intact' => dimension_commit_line_intact($row), 'stages' => $stages));
}

/**
 * The commitment register of one order (or every order).
 *
 * @param int $order_no One order, or 0 for all
 * @param int $limit Row limit
 * @return array|false Rows with stages and the intact verdict, or false when denied/unreadable
 */
function dimension_commit_register($order_no = 0, $limit = 500)
{
	if (!dimension_commit_has_access(array('SA_GLANALYTIC', 'SA_BUDGETENTRY')) || dimension_commit_schema_ready() !== true)
		return false;
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_commitment_lines'.((int)$order_no > 0 ? ' WHERE order_no='.(int)$order_no : '').' ORDER BY po_detail_item LIMIT '
		.max(1, min(5000, (int)$limit)));
	if ($rows === false)
		return false;
	$out = array();
	while ($row = db_fetch_assoc($rows))
		$out[] = dimension_commit_describe($row);
	return $out;
}

/**
 * The append-only effects of one register row.
 *
 * @param int $commitment_id Register row
 * @return array|false Effects oldest first, or false when denied/unreadable
 */
function dimension_commit_events($commitment_id)
{
	if (!dimension_commit_has_access(array('SA_GLANALYTIC', 'SA_BUDGETENTRY')) || dimension_commit_schema_ready() !== true)
		return false;
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_commitment_events WHERE commitment_id='.(int)$commitment_id.' ORDER BY event_id');
	if ($rows === false)
		return false;
	$out = array();
	while ($row = db_fetch_assoc($rows))
		$out[] = $row;
	return $out;
}

/**
 * Funds position without an authorization check (the callers check access and serialize when they must).
 *
 * @param string $account Account code
 * @param int $scope1 Axis 1 scope (0 all, negative unassigned, positive exact)
 * @param int $scope2 Axis 2 scope
 * @param string $from SQL date
 * @param string $to SQL date
 * @return array|false
 */
function dimension_commit_position_unchecked($account, $scope1, $scope2, $from, $to)
{
	$scope1 = (int)$scope1;
	$scope2 = (int)$scope2;
	$budget = dimension_budget_report_amount($account, $scope1, $scope2, $from, $to);
	$sql = 'SELECT SUM(amount) FROM '.TB_PREF.'gl_trans WHERE account='.dimension_commit_quote($account).' AND tran_date>='.dimension_commit_quote($from)
		.' AND tran_date<='.dimension_commit_quote($to);
	if ($scope1 !== 0)
		$sql .= ' AND dimension_id='.($scope1 < 0 ? 0 : $scope1);
	if ($scope2 !== 0)
		$sql .= ' AND dimension2_id='.($scope2 < 0 ? 0 : $scope2);
	$rows = db_query($sql);
	$row = $rows ? db_fetch_row($rows) : false;
	if (!$row)
		return false;
	$actual = round((float)$row[0], 6);
	$lines = db_query('SELECT * FROM '.TB_PREF.'dimension_commitment_lines WHERE account_code='.dimension_commit_quote($account).' AND commit_date>='
		.dimension_commit_quote($from).' AND commit_date<='.dimension_commit_quote($to).' ORDER BY po_detail_item');
	if ($lines === false)
		return false;
	$sums = array('open' => 0.0, 'received' => 0.0, 'billed' => 0.0, 'remaining' => 0.0, 'released' => 0.0);
	$counted = 0;
	$tampered = 0;
	$unclassified = 0;
	while ($line = db_fetch_assoc($lines)) {
		if (!dimension_budget_scope_matches($scope1, $line['coordinate_1']) || !dimension_budget_scope_matches($scope2, $line['coordinate_2']))
			continue;
		if (!dimension_commit_line_intact($line)) {
			$tampered++;
			continue;
		}
		$stages = dimension_commit_stage_amounts($line['ordered_qty'], $line['received_qty'], $line['billed_qty'], $line['unit_amount'], $line['state'], $line['released_amount']);
		foreach ($sums as $key => $value)
			$sums[$key] = round($value + $stages[$key], 6);
		if ($line['state'] === 'open') {
			$counted++;
			if ($line['lineage_state'] === 'absent')
				$unclassified++;
		}
	}
	$reserved = dimension_commit_open_reservations($account, $scope1, $scope2, $from, $to);
	if ($reserved === false)
		return false;
	$consumed = round($actual + $sums['remaining'] + $reserved, 6);
	$available = !empty($budget['available']);
	return array('account' => $account, 'from' => $from, 'to' => $to, 'scope1' => $scope1, 'scope2' => $scope2, 'budget_available' => $available,
		'budget' => $available ? round((float)$budget['amount'], 6) : null, 'budget_reason' => $budget['reason'], 'actual' => $actual,
		'commitment_open' => $sums['open'], 'commitment_received' => $sums['received'], 'commitment_billed' => $sums['billed'],
		'commitment_remaining' => $sums['remaining'], 'commitment_released' => $sums['released'], 'commitment_lines' => $counted,
		'unclassified_lines' => $unclassified, 'tampered_lines' => $tampered, 'reservations_open' => $reserved, 'consumed' => $consumed,
		'free' => $available ? round((float)$budget['amount'] - $consumed, 6) : null, 'complete' => $tampered === 0,
		'label' => _('Commitments and reservations are advisory evidence; only the actual column is posted to the general ledger.'));
}

/**
 * Budget, actual, commitment stages, reservations and free funds of one account/scope/period.
 *
 * @param string $account Account code
 * @param int $scope1 Axis 1 scope (0 all, negative unassigned, positive exact)
 * @param int $scope2 Axis 2 scope
 * @param string $from SQL date
 * @param string $to SQL date
 * @return array|false The position, or false when denied, invalid or unreadable
 */
function dimension_commit_funds_position($account, $scope1, $scope2, $from, $to)
{
	if (!dimension_commit_has_access(array('SA_GLANALYTIC', 'SA_BUDGETENTRY')) || dimension_commit_schema_ready() !== true)
		return false;
	if (!is_string($account) || !preg_match('/^[\x21-\x7e]{1,15}$/D', $account) || !is_iso_calendar_date($from) || !is_iso_calendar_date($to) || $from > $to)
		return false;
	return dimension_commit_position_unchecked($account, $scope1, $scope2, $from, $to);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Reservations (separately activated)
// ---------------------------------------------------------------------------------------------------------------------------------

/** Open (unconsumed, unreleased) reservation amount of a scope and period; false when unreadable. */
function dimension_commit_open_reservations($account, $scope1, $scope2, $from, $to)
{
	$rows = db_query('SELECT coordinate_1, coordinate_2, amount, consumed_amount, released_amount FROM '.TB_PREF.'dimension_commitment_reservations WHERE account_code='
		.dimension_commit_quote($account)." AND state='reserved' AND period_from<=".dimension_commit_quote($to).' AND period_to>='.dimension_commit_quote($from));
	if ($rows === false)
		return false;
	$total = 0.0;
	while ($row = db_fetch_assoc($rows))
		if (dimension_budget_scope_matches($scope1, $row['coordinate_1']) && dimension_budget_scope_matches($scope2, $row['coordinate_2']))
			$total += (float)$row['amount'] - (float)$row['consumed_amount'] - (float)$row['released_amount'];
	return round($total, 6);
}

/**
 * The enforcement mode: 'advisory' (the default) or 'reservations'.
 *
 * @return string|false False when unreadable
 */
function dimension_commit_mode()
{
	if (dimension_commit_schema_ready() !== true)
		return false;
	$rows = db_query('SELECT mode FROM '.TB_PREF.'dimension_commitment_activations ORDER BY activation_id DESC LIMIT 1');
	if ($rows === false)
		return false;
	$row = db_fetch_row($rows);
	return $row ? (string)$row[0] : 'advisory';
}

/**
 * Switch the reservation mode (an authorized, reasoned, append-only audited choice; the default stays advisory).
 *
 * @param string $mode advisory|reservations
 * @param string $reason Mandatory reason
 * @return array ok, mode, changed, errors, codes
 */
function dimension_commit_set_mode($mode, $reason)
{
	$result = dimension_commit_result(array('mode' => '', 'changed' => false));
	if (!dimension_commit_has_access(array('SA_SETUPCOMPANY')))
		return dimension_commit_fail($result, 'permission_denied', _('You are not allowed to change the reservation mode.'));
	$actor = current_user_id_or_zero();
	if ($actor <= 0)
		return dimension_commit_fail($result, 'permission_denied', _('The deciding user could not be identified.'));
	if (!in_array($mode, array('advisory', 'reservations'), true))
		return dimension_commit_fail($result, 'invalid_mode', _('The mode must be advisory or reservations.'));
	$reason = is_string($reason) ? trim($reason) : '';
	if ($reason === '' || strlen($reason) > 200)
		return dimension_commit_fail($result, 'reason_required', _('A reason of 1 to 200 characters is required.'));
	if (dimension_commit_schema_ready() !== true)
		return dimension_commit_fail($result, 'schema_missing', _('The commitment tables are not installed. Upgrade the company database first.'));
	if (!dimension_commit_lock('dimension_commit_funds'))
		return dimension_commit_fail($result, 'busy', _('Another funds operation is running; try again.'));
	$current = dimension_commit_mode();
	$result['mode'] = $current;
	if ($current === $mode) {
		dimension_commit_unlock('dimension_commit_funds');
		return $result;
	}
	$done = db_query('INSERT INTO '.TB_PREF.'dimension_commitment_activations (mode, reason, actor_id, created_at) VALUES ('.dimension_commit_quote($mode).','
		.dimension_commit_quote($reason).','.$actor.',NOW())') !== false;
	dimension_commit_unlock('dimension_commit_funds');
	if (!$done)
		return dimension_commit_fail($result, 'unavailable', _('The mode change could not be recorded.'));
	$result['mode'] = $mode;
	$result['changed'] = true;
	return $result;
}

/** The audited mode changes, newest first (false when denied). */
function dimension_commit_mode_history($limit = 50)
{
	if (!dimension_commit_has_access(array('SA_SETUPCOMPANY', 'SA_BUDGETENTRY')) || dimension_commit_schema_ready() !== true)
		return false;
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_commitment_activations ORDER BY activation_id DESC LIMIT '.max(1, min(500, (int)$limit)));
	if ($rows === false)
		return false;
	$out = array();
	while ($row = db_fetch_assoc($rows))
		$out[] = $row;
	return $out;
}

/** Does an axis coordinate name an existing Dimension of that axis (0 = unassigned is always valid)? */
function dimension_commit_coordinate_valid($coordinate, $axis)
{
	if ($coordinate === 0)
		return true;
	$rows = db_query('SELECT type_ FROM '.TB_PREF.'dimensions WHERE id='.(int)$coordinate);
	$row = $rows ? db_fetch_row($rows) : false;
	return $row && (int)$row[0] === (int)$axis;
}

/** Load one reservation (optionally locked). */
function dimension_commit_load_reservation($reservation_id, $lock = false)
{
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_commitment_reservations WHERE reservation_id='.(int)$reservation_id.($lock ? ' FOR UPDATE' : ''));
	$row = $rows ? db_fetch_assoc($rows) : false;
	return $row ? $row : false;
}

/**
 * Atomically reserve funds of an exact scope (the claim is refused when it exceeds the free funds).
 *
 * @param string $reference Caller idempotency reference (1 to 40 of A-Z a-z 0-9 . _ : -)
 * @param string $account Account code
 * @param int $coordinate_1 Exact Dimension of axis 1 (0 = unassigned)
 * @param int $coordinate_2 Exact Dimension of axis 2 (0 = unassigned)
 * @param string $from SQL date of the budget period
 * @param string $to SQL date of the budget period
 * @param mixed $amount Positive amount
 * @param string $reason Mandatory reason
 * @return array ok, reservation_id, existing, free_before, errors, codes
 */
function dimension_commit_reserve($reference, $account, $coordinate_1, $coordinate_2, $from, $to, $amount, $reason)
{
	$result = dimension_commit_result(array('reservation_id' => 0, 'existing' => false, 'free_before' => null));
	if (!dimension_commit_has_access(array('SA_PURCHASEORDER', 'SA_BUDGETENTRY')))
		return dimension_commit_fail($result, 'permission_denied', _('You are not allowed to reserve funds.'));
	$actor = current_user_id_or_zero();
	if ($actor <= 0)
		return dimension_commit_fail($result, 'permission_denied', _('The reserving user could not be identified.'));
	if (dimension_commit_schema_ready() !== true)
		return dimension_commit_fail($result, 'schema_missing', _('The commitment tables are not installed. Upgrade the company database first.'));
	if (!is_string($reference) || !preg_match('/^[A-Za-z0-9._:-]{1,40}$/D', $reference))
		return dimension_commit_fail($result, 'invalid_reference', _('The reservation reference must be 1 to 40 letters, digits or . _ : -.'));
	$value = dimension_commit_parse_amount($amount);
	if ($value === false || $value <= 0)
		return dimension_commit_fail($result, 'invalid_amount', _('The reservation amount must be a positive number.'));
	$reason = is_string($reason) ? trim($reason) : '';
	if ($reason === '' || strlen($reason) > 200)
		return dimension_commit_fail($result, 'reason_required', _('A reason of 1 to 200 characters is required.'));
	if (!is_string($account) || !preg_match('/^[\x21-\x7e]{1,15}$/D', $account) || !is_iso_calendar_date($from) || !is_iso_calendar_date($to) || $from > $to)
		return dimension_commit_fail($result, 'invalid_scope', _('The account or period of the reservation is not valid.'));
	if (!is_int($coordinate_1) || !is_int($coordinate_2) || $coordinate_1 < 0 || $coordinate_2 < 0)
		return dimension_commit_fail($result, 'invalid_scope', _('A reservation names an exact tracking value of each axis, or none.'));
	$sha = hash('sha256', json_encode(array('adv1-reservation', $account, $coordinate_1, $coordinate_2, $from, $to, dimension_commit_amount_text($value))));
	if (!dimension_commit_lock('dimension_commit_funds'))
		return dimension_commit_fail($result, 'busy', _('Another funds operation is running; try again.'));
	$out = dimension_commit_reserve_locked($reference, $account, $coordinate_1, $coordinate_2, $from, $to, $value, $reason, $sha, $actor, $result);
	dimension_commit_unlock('dimension_commit_funds');
	return $out;
}

/** The body of dimension_commit_reserve() while the funds lock is held. */
function dimension_commit_reserve_locked($reference, $account, $coordinate_1, $coordinate_2, $from, $to, $value, $reason, $sha, $actor, &$result)
{
	if (dimension_commit_mode() !== 'reservations')
		return dimension_commit_fail($result, 'reservations_inactive', _('Reservations are not activated; commitments stay advisory.'));
	$rows = db_query('SELECT reservation_id, request_sha256 FROM '.TB_PREF.'dimension_commitment_reservations WHERE reference='.dimension_commit_quote($reference));
	$row = $rows ? db_fetch_assoc($rows) : false;
	if ($row) {
		if (!hash_equals((string)$row['request_sha256'], $sha))
			return dimension_commit_fail($result, 'reference_conflict', _('This reference already holds a different reservation.'));
		$result['reservation_id'] = (int)$row['reservation_id'];
		$result['existing'] = true;
		return $result;
	}
	$account_row = db_query('SELECT account_code FROM '.TB_PREF.'chart_master WHERE account_code='.dimension_commit_quote($account));
	if (!$account_row || !db_fetch_row($account_row))
		return dimension_commit_fail($result, 'account_missing', _('The account does not exist.'));
	if (!dimension_commit_coordinate_valid($coordinate_1, 1) || !dimension_commit_coordinate_valid($coordinate_2, 2))
		return dimension_commit_fail($result, 'dimension_invalid', _('A tracking value does not exist on the axis it is reserved for.'));
	$position = dimension_commit_position_unchecked($account, $coordinate_1 === 0 ? -1 : $coordinate_1, $coordinate_2 === 0 ? -1 : $coordinate_2, $from, $to);
	if ($position === false)
		return dimension_commit_fail($result, 'unavailable', _('The funds position could not be read.'));
	if (!$position['budget_available'])
		return dimension_commit_fail($result, 'budget_unavailable', _('There is no comparable budget for this scope and period, so funds cannot be reserved.'));
	if (!$position['complete'])
		return dimension_commit_fail($result, 'register_tampered', _('A commitment row of this scope fails its digest; the funds position is not trustworthy.'));
	$result['free_before'] = $position['free'];
	if ($value > $position['free'] + 0.0000005)
		return dimension_commit_fail($result, 'insufficient_funds', sprintf(_('The reservation exceeds the free funds of %s.'), dimension_commit_amount_text($position['free'])));
	if (!db_query('INSERT INTO '.TB_PREF.'dimension_commitment_reservations (reference, request_sha256, account_code, coordinate_1, coordinate_2, period_from, period_to, amount,'
		." consumed_amount, released_amount, state, reason, actor_id, created_at) VALUES (".dimension_commit_quote($reference).','.dimension_commit_quote($sha).','
		.dimension_commit_quote($account).','.$coordinate_1.','.$coordinate_2.','.dimension_commit_quote($from).','.dimension_commit_quote($to).','
		.dimension_commit_amount_text($value).",0,0,'reserved',".dimension_commit_quote($reason).','.$actor.',NOW())'))
		return dimension_commit_fail($result, 'unavailable', _('The reservation could not be recorded.'));
	$result['reservation_id'] = (int)db_insert_id();
	return $result;
}

/** The state a reservation reaches after an update. */
function dimension_commit_reservation_state($amount, $consumed, $released)
{
	$open = round((float)$amount - (float)$consumed - (float)$released, 6);
	if ($open > 0)
		return 'reserved';
	if ((float)$released == 0)
		return 'consumed';
	return (float)$consumed == 0 ? 'released' : 'closed';
}

/**
 * Convert part (or all) of an open reservation into the commitment of a registered purchase order line.
 *
 * @param int $reservation_id Reservation
 * @param mixed $amount Positive amount no larger than the open amount
 * @param int $po_detail_item The registered line that now carries the commitment (same account and Dimensions)
 * @return array ok, state, errors, codes
 */
function dimension_commit_consume($reservation_id, $amount, $po_detail_item)
{
	$result = dimension_commit_result(array('state' => ''));
	if (!dimension_commit_has_access(array('SA_PURCHASEORDER', 'SA_BUDGETENTRY')))
		return dimension_commit_fail($result, 'permission_denied', _('You are not allowed to consume reservations.'));
	$value = dimension_commit_parse_amount($amount);
	if ($value === false || $value <= 0)
		return dimension_commit_fail($result, 'invalid_amount', _('The amount must be a positive number.'));
	if (dimension_commit_schema_ready() !== true)
		return dimension_commit_fail($result, 'schema_missing', _('The commitment tables are not installed. Upgrade the company database first.'));
	if (!dimension_commit_lock('dimension_commit_funds'))
		return dimension_commit_fail($result, 'busy', _('Another funds operation is running; try again.'));
	$out = dimension_commit_close_locked($reservation_id, $value, (int)$po_detail_item, $result);
	dimension_commit_unlock('dimension_commit_funds');
	return $out;
}

/**
 * Release the open remainder of a reservation exactly once.
 *
 * @param int $reservation_id Reservation
 * @return array ok, state, released, errors, codes
 */
function dimension_commit_release($reservation_id)
{
	$result = dimension_commit_result(array('state' => '', 'released' => 0.0));
	if (!dimension_commit_has_access(array('SA_PURCHASEORDER', 'SA_BUDGETENTRY')))
		return dimension_commit_fail($result, 'permission_denied', _('You are not allowed to release reservations.'));
	if (dimension_commit_schema_ready() !== true)
		return dimension_commit_fail($result, 'schema_missing', _('The commitment tables are not installed. Upgrade the company database first.'));
	if (!dimension_commit_lock('dimension_commit_funds'))
		return dimension_commit_fail($result, 'busy', _('Another funds operation is running; try again.'));
	$out = dimension_commit_close_locked($reservation_id, null, 0, $result);
	dimension_commit_unlock('dimension_commit_funds');
	return $out;
}

/** Consume (amount given) or release (amount null) a reservation while the funds lock is held. */
function dimension_commit_close_locked($reservation_id, $amount, $po_detail_item, &$result)
{
	if (!begin_transaction())
		return dimension_commit_fail($result, 'unavailable', _('The change could not start a transaction.'));
	$reservation = dimension_commit_load_reservation($reservation_id, true);
	if (!$reservation || $reservation['state'] !== 'reserved') {
		cancel_transaction();
		return dimension_commit_fail($result, $reservation ? 'nothing_open' : 'reservation_missing',
			$reservation ? _('This reservation has nothing open any more.') : _('The reservation does not exist.'));
	}
	$open = round((float)$reservation['amount'] - (float)$reservation['consumed_amount'] - (float)$reservation['released_amount'], 6);
	$consumed = (float)$reservation['consumed_amount'];
	$released = (float)$reservation['released_amount'];
	if ($amount === null) {
		$released = round($released + $open, 6);
		$result['released'] = $open;
	} else {
		if ($amount > $open + 0.0000005) {
			cancel_transaction();
			return dimension_commit_fail($result, 'exceeds_open', _('The amount is larger than the open reservation.'));
		}
		$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_commitment_lines WHERE po_detail_item='.(int)$po_detail_item.' FOR UPDATE');
		$line = $rows ? db_fetch_assoc($rows) : false;
		if (!$line || !dimension_commit_line_intact($line) || $line['state'] !== 'open' || $line['account_code'] !== $reservation['account_code']
			|| (int)$line['coordinate_1'] !== (int)$reservation['coordinate_1'] || (int)$line['coordinate_2'] !== (int)$reservation['coordinate_2']) {
			cancel_transaction();
			return dimension_commit_fail($result, 'commitment_mismatch', _('The commitment line is not an open, intact line of the same account and tracking values.'));
		}
		$consumed = round($consumed + $amount, 6);
	}
	$state = dimension_commit_reservation_state($reservation['amount'], $consumed, $released);
	$sql = 'UPDATE '.TB_PREF.'dimension_commitment_reservations SET consumed_amount='.dimension_commit_amount_text($consumed).', released_amount='
		.dimension_commit_amount_text($released).', state='.dimension_commit_quote($state).($state === 'reserved' ? '' : ', closed_at=NOW()')
		.' WHERE reservation_id='.(int)$reservation_id." AND state='reserved'";
	if (db_query($sql) === false || !commit_transaction()) {
		cancel_transaction();
		return dimension_commit_fail($result, 'unavailable', _('The reservation could not be updated.'));
	}
	$result['state'] = $state;
	return $result;
}

/** The reservations, newest first (false when denied). */
function dimension_commit_reservation_list($limit = 100)
{
	if (!dimension_commit_has_access(array('SA_GLANALYTIC', 'SA_BUDGETENTRY', 'SA_PURCHASEORDER')) || dimension_commit_schema_ready() !== true)
		return false;
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_commitment_reservations ORDER BY reservation_id DESC LIMIT '.max(1, min(1000, (int)$limit)));
	if ($rows === false)
		return false;
	$out = array();
	while ($row = db_fetch_assoc($rows))
		$out[] = $row;
	return $out;
}

// ADV-001: forecast versions over the commitment register.
include_once dirname(__FILE__).'/tracking_forecast.inc';
