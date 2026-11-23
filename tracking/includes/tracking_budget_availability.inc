<?php
// DIM-NAM-003: the company name of each tracking category (also when this file is loaded without a page).
include_once(dirname(__FILE__).'/tracking_axis_label.inc');

/**
 * DIM-BUD-003: advisory budget availability at the point where money is committed.
 *
 * A purchase order or requisition entry, the saved order view and the approval review each answer one question for a reader
 * who may not open the GL pages: "how much of the budget of this account and Dimension is still free, and what does this
 * document do to it?". The answer is NEVER a new calculation. It reuses, line by line:
 *
 *  - the DIM-ADV-001 funds position (`dimension_commit_position_unchecked`: budget - GL actual - remaining commitments -
 *    open reservations = free funds), which itself reads the DIM-BUD-001/002 comparable budget;
 *  - the DIM-ADV-001 line valuation (`dimension_commit_resolve_line`: account rule, tax netting, company-currency rate at the
 *    document date), so a line is valued exactly as the commitment register would value it once the order is saved.
 *
 * What this authority adds is only the grouping (account + Dimension 1 + Dimension 2), the subtraction of the document's own
 * commitments when it is already in the register (so editing an order never counts it twice), and the presentation.
 *
 * Rules:
 *  - ADVISORY. Nothing here blocks, warns a save, writes, reserves or posts anything; no table is written.
 *  - AUTHORIZATION is the funds-position rule (`SA_GLANALYTIC` or `SA_BUDGETENTRY`). A reader without it sees a one-line
 *    statement that the figures exist, never an amount. No new security area is added.
 *  - PERIOD is the fiscal year of the document date. When the budget of the whole year cannot be compared (months without a
 *    budget), the calendar month of the document date is tried and the row states which period it used.
 *  - A line whose account, rate, tax basis or classification cannot be resolved is listed as not evaluated and is never
 *    guessed; a Dimension coordinate that is unassigned is evaluated as the unassigned scope, never as "all".
 *  - The register holds only the orders it has OBSERVED; the panel states when it last observed, so an order saved since then
 *    is visibly not counted yet.
 *
 * PHP 5.6 compatible syntax. Dates are SQL `Y-m-d`.
 */
include_once dirname(__FILE__).'/tracking_commitment.inc';
include_once dirname(__FILE__).'/tracking_budget_alert.inc';

/** Does the session hold a security area that may see budget figures (the ADV-001 funds-position rule)? */
function dimension_availability_access()
{
	return dimension_commit_has_access(array('SA_GLANALYTIC', 'SA_BUDGETENTRY'));
}

/** A fresh result skeleton. */
function dimension_availability_result($state, $extra = array())
{
	return array_merge(array('state' => $state, 'groups' => array(), 'unresolved' => array(), 'over' => 0, 'registered_at' => null,
		'label' => _('Advisory only: this does not block the document. Commitments are those recorded in the commitment register.')), $extra);
}

/**
 * The fiscal year that holds a date.
 *
 * @param string $date SQL date
 * @return array|false from, to (SQL dates) or false when no fiscal year holds the date
 */
function dimension_availability_year($date)
{
	if (!dimension_commit_valid_date($date))
		return false;
	$rows = db_query('SELECT `begin`, `end` FROM '.TB_PREF.'fiscal_year WHERE `begin`<='.dimension_commit_quote($date).' AND `end`>='.dimension_commit_quote($date).' ORDER BY `begin` LIMIT 1');
	$row = $rows ? db_fetch_row($rows) : false;
	return $row ? array('from' => (string)$row[0], 'to' => (string)$row[1]) : false;
}

/**
 * The calendar month that holds a date.
 *
 * @param string $date SQL date
 * @return array from, to
 */
function dimension_availability_month($date)
{
	$stamp = strtotime($date.' 00:00:00 UTC');
	return array('from' => gmdate('Y-m-01', $stamp), 'to' => gmdate('Y-m-t', $stamp));
}

/**
 * The commitment this document already holds in the register for one scope (to subtract from the position).
 *
 * @param int $order_no Saved purchase order, or 0
 * @param string $account Account code
 * @param int $scope1 Axis 1 scope
 * @param int $scope2 Axis 2 scope
 * @param string $from SQL date
 * @param string $to SQL date
 * @return float
 */
function dimension_availability_own_remaining($order_no, $account, $scope1, $scope2, $from, $to)
{
	if ((int)$order_no <= 0)
		return 0.0;
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_commitment_lines WHERE order_no='.(int)$order_no.' AND account_code='.dimension_commit_quote($account)
		.' AND commit_date>='.dimension_commit_quote($from).' AND commit_date<='.dimension_commit_quote($to));
	$total = 0.0;
	while ($rows && ($line = db_fetch_assoc($rows))) {
		if (!dimension_budget_scope_matches($scope1, $line['coordinate_1']) || !dimension_budget_scope_matches($scope2, $line['coordinate_2']))
			continue;
		if (!dimension_commit_line_intact($line))
			continue;
		$stages = dimension_commit_stage_amounts($line['ordered_qty'], $line['received_qty'], $line['billed_qty'], $line['unit_amount'], $line['state'], $line['released_amount']);
		$total = round($total + $stages['remaining'], 6);
	}
	return $total;
}

/** When the commitment register last observed an order (a datetime text), or null when it never did. */
function dimension_availability_register_time()
{
	$rows = db_query('SELECT MAX(observed_at) FROM '.TB_PREF.'dimension_commitment_lines');
	$row = $rows ? db_fetch_row($rows) : false;
	return $row && $row[0] !== null ? (string)$row[0] : null;
}

/**
 * The Dimension coordinates of one persisted purchasing source line (its own revision, else its header's, else unassigned).
 *
 * @param int $trans_type Source type (purchase order or requisition)
 * @param int $document_no Document identity
 * @param int $line_id Native line identity
 * @return array coordinate_1, coordinate_2 and lineage_state revision|header|absent
 */
function dimension_availability_stored_coordinates($trans_type, $document_no, $line_id)
{
	$out = array('coordinate_1' => 0, 'coordinate_2' => 0, 'lineage_state' => 'absent');
	if (!function_exists('dimension_source_revision_snapshot'))
		return $out;
	$document = dimension_source_revision_document_key($document_no);
	$line_key = dimension_source_revision_line_key('line', $line_id);
	if ($document === false || $line_key === false)
		return $out;
	foreach (array(array('line', $line_key, 'revision'), array('header', 'header', 'header')) as $try) {
		$snapshot = dimension_source_revision_snapshot('purchasing', (int)$trans_type, $document, $try[0], $try[1]);
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
 * Build one evaluable line. Every figure is given in the document's own terms; the commitment authority values it.
 *
 * @param string $label What the line is called in the panel
 * @param string $stock_id Item
 * @param float $quantity Quantity ordered or requested
 * @param float $price Unit price (supplier currency; the requisition estimate is company currency)
 * @param float $billed Quantity already billed (counted in the GL actual, not in the commitment)
 * @param string $date SQL document date
 * @param int $tax_included 1 when the price includes tax
 * @param string $currency Supplier currency ('' for the company currency)
 * @param int $tax_group Supplier tax group
 * @param int $coordinate_1 Axis 1 coordinate (0 unassigned)
 * @param int $coordinate_2 Axis 2 coordinate (0 unassigned)
 * @return array
 */
function dimension_availability_line($label, $stock_id, $quantity, $price, $billed, $date, $tax_included, $currency, $tax_group, $coordinate_1, $coordinate_2)
{
	$rows = db_query('SELECT mb_flag, cogs_account, inventory_account FROM '.TB_PREF.'stock_master WHERE stock_id='.dimension_commit_quote($stock_id));
	$item = $rows ? db_fetch_assoc($rows) : false;
	if (!is_array($item))
		$item = array('mb_flag' => '', 'cogs_account' => '', 'inventory_account' => '');
	return array('label' => (string)$label, 'coordinate_1' => (int)$coordinate_1, 'coordinate_2' => (int)$coordinate_2,
		'row' => array('po_detail_item' => 0, 'order_no' => 0, 'item_code' => (string)$stock_id, 'quantity_ordered' => (float)$quantity, 'quantity_received' => 0.0,
			'qty_invoiced' => (float)$billed, 'unit_price' => (float)$price, 'ord_date' => (string)$date, 'tax_included' => (int)$tax_included,
			'curr_code' => (string)$currency, 'tax_group_id' => (int)$tax_group, 'mb_flag' => $item['mb_flag'], 'cogs_account' => $item['cogs_account'],
			'inventory_account' => $item['inventory_account']));
}

/**
 * Funds position of one scope for the document's period: the fiscal year, or the month when the year cannot be compared.
 *
 * @return array|false position (with period and basis) or false when unreadable
 */
function dimension_availability_position($account, $scope1, $scope2, $date)
{
	$year = dimension_availability_year($date);
	$tries = array();
	if ($year !== false)
		$tries[] = array('year', $year);
	$tries[] = array('month', dimension_availability_month($date));
	$first = false;
	foreach ($tries as $try) {
		$position = dimension_commit_position_unchecked($account, $scope1, $scope2, $try[1]['from'], $try[1]['to']);
		if ($position === false)
			return false;
		$position['basis'] = $try[0];
		if ($first === false)
			$first = $position;
		if (!empty($position['budget_available']))
			return $position;
	}
	return $first;
}

/**
 * Evaluate the effect of a set of lines on the free budget of their account and Dimension scopes.
 *
 * @param array $lines Lines from dimension_availability_line()
 * @param string $date SQL document date
 * @param int $exclude_order_no Saved order whose register lines are replaced by these lines (0 for none)
 * @return array state (available|denied|unready|bad_date|too_many|empty), groups, unresolved, over, registered_at, label
 */
function dimension_availability_evaluate($lines, $date, $exclude_order_no = 0)
{
	if (!dimension_availability_access())
		return dimension_availability_result('denied');
	if (dimension_commit_schema_ready() !== true)
		return dimension_availability_result('unready');
	if (!dimension_commit_valid_date($date))
		return dimension_availability_result('bad_date');
	if (count($lines) > dimension_commit_max_lines())
		return dimension_availability_result('too_many');
	$groups = array();
	$unresolved = array();
	foreach ($lines as $line) {
		$valued = dimension_commit_resolve_line($line['row']);
		if (empty($valued['ok'])) {
			$unresolved[] = array('label' => $line['label'], 'text' => $valued['text']);
			continue;
		}
		$stages = dimension_commit_stage_amounts($valued['ordered_qty'], $valued['received_qty'], $valued['billed_qty'], $valued['unit_amount'], 'open');
		if ($stages['remaining'] <= 0)
			continue;
		$key = $valued['account_code']."\t".(int)$line['coordinate_1']."\t".(int)$line['coordinate_2'];
		if (!isset($groups[$key]))
			$groups[$key] = array('account' => $valued['account_code'], 'coordinate_1' => (int)$line['coordinate_1'], 'coordinate_2' => (int)$line['coordinate_2'],
				'amount' => 0.0, 'lines' => 0);
		$groups[$key]['amount'] = round($groups[$key]['amount'] + $stages['remaining'], 6);
		$groups[$key]['lines']++;
	}
	$out = array();
	$over = 0;
	$thresholds = dimension_alert_thresholds();
	ksort($groups);
	foreach ($groups as $group) {
		$scope1 = $group['coordinate_1'] > 0 ? $group['coordinate_1'] : -1;
		$scope2 = $group['coordinate_2'] > 0 ? $group['coordinate_2'] : -1;
		$position = dimension_availability_position($group['account'], $scope1, $scope2, $date);
		if ($position === false) {
			$unresolved[] = array('label' => $group['account'], 'text' => _('The budget position of this account and tracking value could not be read.'));
			continue;
		}
		$own = dimension_availability_own_remaining($exclude_order_no, $group['account'], $scope1, $scope2, $position['from'], $position['to']);
		$commitments = round($position['commitment_remaining'] - $own, 6);
		$consumed = round($position['consumed'] - $own, 6);
		$available = !empty($position['budget_available']);
		$free_now = $available ? round($position['budget'] - $consumed, 6) : null;
		$free_after = $available ? round($free_now - $group['amount'], 6) : null;
		$status = !$available ? 'no_budget' : ($free_after < 0 ? 'exceeds' : 'within');
		if ($status === 'exceeds')
			$over++;
		$out[] = array_merge($group, array('basis' => $position['basis'], 'from' => $position['from'], 'to' => $position['to'], 'budget_available' => $available,
			'budget' => $position['budget'], 'budget_reason' => $position['budget_reason'], 'actual' => $position['actual'], 'commitments' => $commitments,
			'reservations' => $position['reservations_open'], 'free_now' => $free_now, 'free_after' => $free_after, 'status' => $status,
			'used_after_percent' => $available && $position['budget'] > 0 ? round(($consumed + $group['amount']) / $position['budget'] * 100, 1) : null,
			// DIM-BUD-004: the same company thresholds as the budget alerts, applied to the share used after this document.
			'alert' => $available ? dimension_alert_classify($position['budget'], $consumed + $group['amount'], $thresholds) : null,
			'complete' => !empty($position['complete'])));
	}
	if (!$out && !$unresolved)
		return dimension_availability_result('empty');
	return dimension_availability_result('available', array('groups' => $out, 'unresolved' => $unresolved, 'over' => $over, 'registered_at' => dimension_availability_register_time()));
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Adapters (one per surface; each only gathers facts and hands them to dimension_availability_evaluate)
// ---------------------------------------------------------------------------------------------------------------------------------

/**
 * The Dimension coordinates a purchase order cart line will be saved with, from the real purchasing resolver and without
 * persisting anything.
 *
 * @return array|false coordinate_1, coordinate_2, or false when the resolver refuses
 */
function dimension_availability_cart_coordinates($cart, $line, $index, $header_source, $header_snapshot)
{
	if (!function_exists('purchasing_classification_resolve_entry'))
		return false;
	$line_source = purchasing_classification_native_line_source($cart, $line);
	$line_snapshot = $line_source === false ? array('state' => 'none') : purchasing_classification_source_snapshot($line_source);
	$resolved = purchasing_classification_resolve_entry($cart, $line, $index, empty($line->po_detail_rec), $header_source, $header_snapshot,
		$line_source, $line_snapshot, 'purchasing');
	if (empty($resolved['ok']))
		return false;
	$out = array();
	foreach (array(1, 2) as $axis)
		$out[$axis] = $resolved['axes'][$axis]['mode'] === 'selected' ? (int)$resolved['axes'][$axis]['value'] : 0;
	return array('coordinate_1' => $out[1], 'coordinate_2' => $out[2]);
}

/**
 * Availability of the purchase order being entered (the session cart).
 *
 * @param object $cart Purchase order cart
 * @return array evaluation, or a `not_applicable` result when the cart is not a purchase order
 */
function dimension_availability_from_cart($cart)
{
	if (!is_object($cart) || (int)$cart->trans_type !== (defined('ST_PURCHORDER') ? ST_PURCHORDER : 18) || empty($cart->line_items))
		return dimension_availability_result('not_applicable');
	if (!dimension_availability_access())
		return dimension_availability_result('denied');
	$date = isset($cart->orig_order_date) ? date2sql($cart->orig_order_date) : '';
	$header_source = function_exists('purchasing_classification_native_header_source') ? purchasing_classification_native_header_source($cart) : false;
	$header_snapshot = $header_source === false || !function_exists('purchasing_classification_source_snapshot') ? array('state' => 'none')
		: purchasing_classification_source_snapshot($header_source);
	$lines = array();
	$refused = array();
	foreach (array_values($cart->line_items) as $index => $line) {
		$label = $line->stock_id.' - '.$line->item_description;
		$coordinates = dimension_availability_cart_coordinates($cart, $line, $index, $header_source, $header_snapshot);
		if ($coordinates === false) {
			$refused[] = array('label' => $label, 'text' => _('The tracking classification of this line cannot be resolved yet.'));
			continue;
		}
		$lines[] = dimension_availability_line($label, $line->stock_id, $line->quantity, $line->price, isset($line->qty_inv) ? $line->qty_inv : 0, $date,
			(int)$cart->tax_included, isset($cart->curr_code) ? $cart->curr_code : '', isset($cart->tax_group_id) ? $cart->tax_group_id : 0,
			$coordinates['coordinate_1'], $coordinates['coordinate_2']);
	}
	$result = dimension_availability_evaluate($lines, $date, isset($cart->order_no) ? (int)$cart->order_no : 0);
	if ($refused) {
		$result['unresolved'] = array_merge($refused, $result['unresolved']);
		if ($result['state'] === 'empty')
			$result['state'] = 'available';
	}
	return $result;
}

/**
 * Availability of a saved purchase order (view page and approval review of a saved order).
 *
 * @param int $order_no Purchase order
 * @return array evaluation
 */
function dimension_availability_from_saved_order($order_no)
{
	if (!dimension_availability_access())
		return dimension_availability_result('denied');
	$rows = dimension_commit_source_rows((int)$order_no);
	if ($rows === false || !$rows)
		return dimension_availability_result('not_applicable');
	$date = (string)$rows[0]['ord_date'];
	$lines = array();
	foreach ($rows as $row) {
		$coordinates = dimension_commit_line_coordinates($row['order_no'], $row['po_detail_item']);
		$lines[] = array('label' => $row['item_code'], 'coordinate_1' => $coordinates['coordinate_1'], 'coordinate_2' => $coordinates['coordinate_2'],
			'row' => $row);
	}
	return dimension_availability_evaluate($lines, $date, (int)$order_no);
}

/**
 * Availability of the open quantity of a requisition (entry page and approval review).
 *
 * The unit price is the requester's estimate in company currency, and a quantity already turned into a purchase order is left
 * to that order's commitment, so nothing is counted twice.
 *
 * @param int $requisition_id Requisition
 * @return array evaluation, or `not_applicable` for a requisition that no longer needs funds
 */
function dimension_availability_from_requisition($requisition_id)
{
	if (!function_exists('get_purch_requisition') || !function_exists('get_requisition_lines'))
		return dimension_availability_result('not_applicable');
	$requisition = get_purch_requisition((int)$requisition_id);
	if (!is_array($requisition) || !in_array($requisition['status'], array('draft', 'submitted', 'approved', 'partially_ordered'), true))
		return dimension_availability_result('not_applicable');
	if (!dimension_availability_access())
		return dimension_availability_result('denied');
	$date = (string)$requisition['request_date'];
	$type = defined('ST_PURCHREQUISITION') ? ST_PURCHREQUISITION : 58;
	$lines = array();
	$result = get_requisition_lines((int)$requisition_id);
	while ($result && ($line = db_fetch($result))) {
		if ($line['status'] === 'rejected')
			continue;
		$open = round((float)$line['quantity'] - (float)$line['qty_ordered'], 6);
		if ($open <= 0)
			continue;
		$coordinates = dimension_availability_stored_coordinates($type, (int)$requisition_id, (int)$line['id']);
		$lines[] = dimension_availability_line($line['stock_id'].' - '.$line['line_description'], $line['stock_id'], $open, (float)$line['estimated_unit_price'], 0.0,
			$date, 0, '', 0, $coordinates['coordinate_1'], $coordinates['coordinate_2']);
	}
	return dimension_availability_evaluate($lines, $date, 0);
}

/**
 * Availability for an approval review draft (requisition or purchase order).
 *
 * A requisition draft names a saved requisition. A purchase order draft that is not saved yet carries its lines and, in the
 * sealed classification snapshot, the exact Dimension decision of each line.
 *
 * @param int $trans_type Draft transaction type
 * @param array $data Decoded draft payload
 * @return array evaluation (`not_applicable` for any other type or payload)
 */
function dimension_availability_from_draft($trans_type, $data)
{
	$requisition_type = defined('ST_PURCHREQUISITION') ? ST_PURCHREQUISITION : 58;
	$order_type = defined('ST_PURCHORDER') ? ST_PURCHORDER : 18;
	if (!is_array($data))
		return dimension_availability_result('not_applicable');
	if ((int)$trans_type === $requisition_type) {
		$id = isset($data['requisition_id']) ? (int)$data['requisition_id'] : 0;
		return $id > 0 ? dimension_availability_from_requisition($id) : dimension_availability_result('not_applicable');
	}
	if ((int)$trans_type !== $order_type)
		return dimension_availability_result('not_applicable');
	if (!dimension_availability_access())
		return dimension_availability_result('denied');
	if (!empty($data['order_no']))
		return dimension_availability_from_saved_order((int)$data['order_no']);
	if (empty($data['line_items']) || !is_array($data['line_items']))
		return dimension_availability_result('not_applicable');
	$snapshot = function_exists('approval_dimension_snapshot_read') ? approval_dimension_snapshot_read($data) : array('state' => 'absent');
	$by_key = array();
	$ordered = array();
	if ($snapshot['state'] === 'available') {
		foreach ($snapshot['snapshot']['lines'] as $entry) {
			$by_key[$entry['line_key']] = $entry;
			$ordered[] = $entry;
		}
	}
	$date = isset($data['orig_order_date']) && $data['orig_order_date'] !== '' ? date2sql($data['orig_order_date']) : '';
	$lines = array();
	$refused = array();
	$index = 0;
	foreach ($data['line_items'] as $item) {
		$label = $item['stock_id'].' - '.(isset($item['item_description']) ? $item['item_description'] : '');
		$entry = null;
		if (!empty($item['draft_line_key']) && isset($by_key[$item['draft_line_key']]))
			$entry = $by_key[$item['draft_line_key']];
		elseif (count($ordered) === count($data['line_items']))
			$entry = $ordered[$index];
		$index++;
		if ($entry === null) {
			$refused[] = array('label' => $label, 'text' => _('The sealed classification of this line is not available.'));
			continue;
		}
		$coordinates = array();
		foreach (array(1, 2) as $axis)
			$coordinates[$axis] = $entry['axes'][$axis]['mode'] === 'selected' ? (int)$entry['axes'][$axis]['value'] : 0;
		$lines[] = dimension_availability_line($label, $item['stock_id'], $item['quantity'], $item['price'], isset($item['qty_inv']) ? $item['qty_inv'] : 0, $date,
			isset($data['tax_included']) ? (int)$data['tax_included'] : 0, isset($data['curr_code']) ? $data['curr_code'] : '',
			isset($data['tax_group_id']) ? $data['tax_group_id'] : 0, $coordinates[1], $coordinates[2]);
	}
	$result = dimension_availability_evaluate($lines, $date, 0);
	if ($refused) {
		$result['unresolved'] = array_merge($refused, $result['unresolved']);
		if ($result['state'] === 'empty')
			$result['state'] = 'available';
	}
	return $result;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Presentation
// ---------------------------------------------------------------------------------------------------------------------------------

/** HTML-escape one text. */
function dimension_availability_h($text)
{
	return htmlspecialchars((string)$text, ENT_QUOTES, 'UTF-8');
}

/** One amount in the user's price format (plain decimals when the format helper is not loaded). */
function dimension_availability_money($amount)
{
	if ($amount === null)
		return '-';
	return function_exists('price_format') ? price_format($amount) : number_format((float)$amount, 2, '.', ',');
}

/** The name of a Dimension coordinate for the panel. */
function dimension_availability_dimension_text($coordinate)
{
	if ((int)$coordinate <= 0)
		return _('Unassigned');
	return function_exists('get_dimension_string') ? get_dimension_string((int)$coordinate, false) : '#'.(int)$coordinate;
}

/**
 * The panel as HTML. Pure: no output, no database.
 *
 * @param array $result dimension_availability_evaluate() result
 * @return string HTML ('' when the surface has nothing to say)
 */
function dimension_availability_panel_html($result)
{
	$state = $result['state'];
	if ($state === 'not_applicable' || $state === 'empty')
		return '';
	$title = '<h3 style="margin:14px 0 6px 0;">'.dimension_availability_h(_('Budget availability (advisory)')).'</h3>';
	$notes = array(
		'denied' => _('Budget availability for this document is shown to users who may view budgets or GL analytics.'),
		'unready' => _('Budget availability is not available: the commitment register is not installed.'),
		'bad_date' => _('Budget availability is not shown: the document date is not valid.'),
		'too_many' => _('Budget availability is not shown: the document has more lines than the register evaluates at once.'));
	if (isset($notes[$state]))
		return '<div class="dimension-availability">'.$title.'<p>'.dimension_availability_h($notes[$state]).'</p></div>';
	$html = '<div class="dimension-availability">'.$title;
	if ($result['over'] > 0)
		$html .= '<p style="color:#b00020;font-weight:bold;">'.dimension_availability_h(sprintf(_('This document would take %d budget scope(s) over their free budget. It can still be saved.'),
			$result['over'])).'</p>';
	if ($result['groups']) {
		$html .= '<table class="tablestyle" style="margin:0 auto;"><tr>';
		foreach (array(_('Account'), dimension_axis_label_html(1), dimension_axis_label_html(2), _('Period'), _('Budget'), _('Actual'), _('Commitments'), _('Reserved'), _('Free now'),
			_('This document'), _('Free after'), _('Result')) as $heading)
			$html .= '<th>'.dimension_availability_h($heading).'</th>';
		$html .= '</tr>';
		foreach ($result['groups'] as $group) {
			$account = $group['account'].(function_exists('get_gl_account_name') ? ' '.get_gl_account_name($group['account']) : '');
			$period = ($group['basis'] === 'year' ? _('Fiscal year') : _('Month')).' '.$group['from'].' / '.$group['to'];
			if ($group['status'] === 'within') {
				$verdict = $group['used_after_percent'] === null ? _('Within budget') : sprintf(_('Within budget (%s%% used after)'), $group['used_after_percent']);
				if (!empty($group['alert']) && $group['alert']['state'] === 'limit')
					$verdict .= ' - '._('limit threshold reached');
				elseif (!empty($group['alert']) && $group['alert']['state'] === 'warning')
					$verdict .= ' - '._('warning threshold reached');
			}
			elseif ($group['status'] === 'exceeds')
				$verdict = sprintf(_('Over by %s'), dimension_availability_money(-$group['free_after']));
			else {
				$reason = function_exists('dimension_budget_reason_text') ? dimension_budget_reason_text($group['budget_reason']) : '';
				$verdict = _('No comparable budget').($reason !== '' ? ': '.$reason : '');
			}
			$style = $group['status'] === 'exceeds' || (!empty($group['alert']) && $group['alert']['state'] === 'limit') ? ' style="color:#b00020;font-weight:bold;"'
				: (!empty($group['alert']) && $group['alert']['state'] === 'warning' ? ' style="color:#9a5b00;font-weight:bold;"' : '');
			$html .= '<tr><td>'.dimension_availability_h($account).'</td><td>'.dimension_availability_h(dimension_availability_dimension_text($group['coordinate_1'])).'</td><td>'
				.dimension_availability_h(dimension_availability_dimension_text($group['coordinate_2'])).'</td><td>'.dimension_availability_h($period).'</td>';
			foreach (array('budget', 'actual', 'commitments', 'reservations', 'free_now', 'amount', 'free_after') as $field)
				$html .= '<td style="text-align:right;">'.dimension_availability_h(dimension_availability_money($group[$field])).'</td>';
			$html .= '<td'.$style.'>'.dimension_availability_h($verdict).'</td></tr>';
		}
		$html .= '</table>';
	}
	if ($result['unresolved']) {
		$html .= '<p>'.dimension_availability_h(_('Not evaluated:')).'</p><ul>';
		foreach ($result['unresolved'] as $item)
			$html .= '<li>'.dimension_availability_h($item['label'].': '.$item['text']).'</li>';
		$html .= '</ul>';
	}
	$observed = $result['registered_at'] === null ? _('The commitment register has not observed any order yet.')
		: sprintf(_('The commitment register last observed an order at %s; orders saved since then are not counted.'), $result['registered_at']);
	$html .= '<p style="font-size:90%;">'.dimension_availability_h($result['label'].' '.$observed).'</p></div>';
	return $html;
}

/** Print the panel (page output). */
function dimension_availability_display($result)
{
	echo dimension_availability_panel_html($result);
}

/**
 * Show the panel of one surface. A failure here must never break the document page, so it is logged and shows nothing.
 *
 * @param string $surface cart|order|requisition|draft
 * @param mixed $subject Cart object, order number, requisition id, or draft transaction type
 * @param array|null $data Decoded draft payload (draft surface only)
 */
function dimension_availability_show($surface, $subject, $data = null)
{
	try {
		if ($surface === 'cart')
			$result = dimension_availability_from_cart($subject);
		elseif ($surface === 'order')
			$result = dimension_availability_from_saved_order((int)$subject);
		elseif ($surface === 'requisition')
			$result = dimension_availability_from_requisition((int)$subject);
		elseif ($surface === 'draft')
			$result = dimension_availability_from_draft((int)$subject, $data);
		else
			return;
		dimension_availability_display($result);
	} catch (Exception $exception) {
		error_log('DIM-BUD-003 availability panel failed: '.get_class($exception));
	} catch (Throwable $exception) {
		error_log('DIM-BUD-003 availability panel failed: '.get_class($exception));
	}
}
