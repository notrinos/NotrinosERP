<?php
/**
 * DIM-MFG-001: classification lineage of work-order costs, WIP, completion and variance.
 *
 * A work order is the cost object of a manufacturing run. Its classification is a durable header
 * revision of the `manufacturing` source family (anchor: `ST_WORKORDER`, the work order id), frozen
 * once when the order is created (or, for an order created before this slice, once at its first cost
 * event as a new event). Every later cost event then repeats exactly that frozen pair, so a changed
 * item default, a changed finished-item pair or a drifting page default can never re-code an order
 * that already carries cost:
 *
 *  - material issue / return to stock (`ST_MANUISSUE`)     both legs (inventory or service account, WIP)
 *  - bill-of-material consumption on production (`ST_MANURECEIVE`)  both legs
 *  - labour / overhead cost journal (`ST_JOURNAL` via `wo_costing`)  both legs (funding, WIP)
 *  - completion, partial completion and unassembly (`ST_WORKORDER`)  WIP release to finished inventory
 *  - average-cost / negative-stock correction posted by the completion  inherited from the order
 *
 * Each posted event is snapshotted once, before its first ledger row, in the `inventory` family under
 * the identity the ledger rows carry, with the work order, the frozen cost-object revision and the
 * quantities/amounts sealed as evidence. Capitalisation into WIP and release to inventory are balance
 * sheet movements and never period cost: the reader at the end of this file reports them separately
 * from any expense recognised on a service-item leg.
 *
 * The adapter builds on the DIM-INV-001/INV-002 capture. It contains no INSERT/UPDATE/DELETE, no DDL,
 * no ledger call and no stock call of its own, never backfills a historical order or ledger row, and
 * never derives labour from payroll (no implicit payroll labour costing). An install without the
 * accepted companion revision schema posts exactly as before.
 *
 * PHP 5.6 compatible.
 */
require_once __DIR__.'/../../../inventory/includes/db/inventory_event_classification_db.inc';

// The canonical cost-type values are loaded by includes/types.inc in a normal request; focused CLI
// harnesses admit the same values without replacing an existing definition.
foreach (array('WO_LABOUR' => 0, 'WO_OVERHEAD' => 1) as $manufacturing_cost_constant => $manufacturing_cost_value)
	if (!defined($manufacturing_cost_constant))
		define($manufacturing_cost_constant, $manufacturing_cost_value);

/**
 * Is the accepted companion revision schema installed so that work-order costs can be classified?
 *
 * @return bool
 */
function manufacturing_cost_classification_ready()
{
	return inventory_classification_schema_ready();
}

/**
 * Read the frozen cost-object pair of one work order.
 *
 * @param mixed $woid Work order id
 * @return array|null axis pair plus `revision_id`, `evidence`, or null when the order was never frozen
 * @throws DimensionFinancialReferenceWriteException A present but unverifiable revision
 */
function manufacturing_cost_classification_workorder($woid)
{
	if (!manufacturing_cost_classification_ready())
		return null;
	$key = dimension_source_revision_document_key($woid);
	if ($key === false)
		return null;
	$snapshot = dimension_source_revision_snapshot('manufacturing', ST_WORKORDER, $key, 'header', 'header');
	if ($snapshot['state'] === 'absent' || $snapshot['state'] === 'unavailable')
		return null;
	if ($snapshot['state'] !== 'available')
		throw new DimensionFinancialReferenceWriteException(
			_('The frozen classification of this work order cannot be verified. Nothing was saved.'));
	$resolution = json_decode($snapshot['revision']['resolution_payload'], true);
	return array(1 => (int)$snapshot['revision']['dimension_id'], 2 => (int)$snapshot['revision']['dimension2_id'],
		'revision_id' => (int)$snapshot['revision']['source_revision_id'],
		'evidence' => is_array($resolution) && isset($resolution['evidence']) ? $resolution['evidence'] : array());
}

/**
 * Resolve the cost-object pair of a new work order.
 *
 * Precedence: an explicit/restored header decision of the carrier, then the finished item's configured
 * default as a weak last source of a genuinely new event.
 *
 * @param string $stock_id Finished item code
 * @param object|null $carrier Header carrier holding `classification_intents`
 * @param array $evidence Bounded scalar evidence
 * @return array ok result of inventory_classification_resolve_entry()
 * @throws DimensionFinancialReferenceWriteException
 */
function manufacturing_cost_classification_resolve_workorder($stock_id, $carrier, $evidence)
{
	$header = is_object($carrier) ? $carrier : new stdClass();
	$line = (object)array('stock_id' => (string)$stock_id);
	$evidence = is_array($evidence) ? $evidence : array();
	$evidence['event'] = 'work_order_cost_object';
	$resolved = inventory_classification_resolve_entry(ST_WORKORDER, $header, $line, true,
		array('evidence' => $evidence));
	if (!$resolved['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $resolved['errors']));
	return $resolved;
}

/**
 * Persist the frozen cost-object header revision of one work order.
 *
 * @param mixed $woid Work order id
 * @param string $date_ Event date in the display format
 * @param array $resolved manufacturing_cost_classification_resolve_workorder() result
 * @return array axis pair plus `revision_id`
 * @throws DimensionFinancialReferenceWriteException
 */
function manufacturing_cost_classification_persist_workorder($woid, $date_, $resolved)
{
	$key = dimension_source_revision_document_key($woid);
	$effective = date2sql($date_);
	if ($key === false || dimension_source_revision_date($effective) === false)
		throw new DimensionFinancialReferenceWriteException(_('The work order classification identity is invalid.'));
	$stored = inventory_classification_persist_revision(ST_WORKORDER, $key, 'header', 'header',
		$resolved['resolution'], $effective, '0.00000000', 'manufacturing');
	if (!$stored['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $stored['errors']));
	return array(1 => (int)$resolved['axes'][1]['value'], 2 => (int)$resolved['axes'][2]['value'],
		'revision_id' => (int)$stored['revision_id']);
}

/**
 * Freeze the cost object of a newly created work order, exactly once.
 *
 * A carrier with sealed approval draft revisions binds the reviewed pair, so a finished-item default
 * that drifted while the draft waited changes nothing. An order that already carries a frozen pair
 * converges on it. Returns null on an install without the companion schema.
 *
 * @param mixed $woid Work order id
 * @param string $stock_id Finished item code
 * @param string $date_ Event date in the display format
 * @param object|null $carrier Header carrier (restored intents, kept sealed draft revisions)
 * @param array $evidence Bounded scalar evidence
 * @return array|null axis pair plus `revision_id`
 * @throws DimensionFinancialReferenceWriteException
 */
function manufacturing_cost_classification_capture_workorder($woid, $stock_id, $date_, $carrier = null,
	$evidence = array())
{
	if (!manufacturing_cost_classification_ready())
		return null;
	$existing = manufacturing_cost_classification_workorder($woid);
	if ($existing !== null)
		return $existing;
	$key = dimension_source_revision_document_key($woid);
	if ($key === false)
		throw new DimensionFinancialReferenceWriteException(_('The work order classification identity is invalid.'));
	$sealed = is_object($carrier) && isset($carrier->classification_draft_revisions)
		&& is_array($carrier->classification_draft_revisions) ? $carrier->classification_draft_revisions : array();
	if (!empty($sealed['header'])) {
		$bound = inventory_classification_bind_draft((int)$sealed['header'], ST_WORKORDER, $key, 'header',
			'header', '0.00000000', 'manufacturing');
		if (!$bound['ok'])
			throw new DimensionFinancialReferenceWriteException(implode(' ', $bound['errors']));
		return array(1 => (int)$bound['axes'][1]['value'], 2 => (int)$bound['axes'][2]['value'],
			'revision_id' => (int)$bound['revision_id']);
	}
	$resolved = manufacturing_cost_classification_resolve_workorder($stock_id, $carrier, $evidence);
	return manufacturing_cost_classification_persist_workorder($woid, $date_, $resolved);
}

/**
 * Return the frozen pair of a work order, freezing it now when an order created before this slice
 * records its first cost event.
 *
 * The first event of an unfrozen legacy order resolves the finished item's configured default as a new
 * event, seals that lineage in the evidence, and every later event repeats it. No earlier posting is
 * touched.
 *
 * @param mixed $woid Work order id
 * @param string $stock_id Finished item code
 * @param string $date_ Event date in the display format
 * @return array|null axis pair plus `revision_id`, or null on an install without the companion schema
 * @throws DimensionFinancialReferenceWriteException
 */
function manufacturing_cost_classification_ensure_workorder($woid, $stock_id, $date_)
{
	if (!manufacturing_cost_classification_ready())
		return null;
	$existing = manufacturing_cost_classification_workorder($woid);
	if ($existing !== null)
		return $existing;
	$resolved = manufacturing_cost_classification_resolve_workorder($stock_id, null,
		array('lineage' => 'legacy_first_cost_event'));
	return manufacturing_cost_classification_persist_workorder($woid, $date_, $resolved);
}

/**
 * Capture the reviewed cost-object pair of a work order that will wait for approval.
 *
 * The returned `payload` keys are merged into the draft payload and the `context` is passed to the
 * approval submit call, which seals the reviewed pair into the envelope. An install without the
 * companion schema returns empty arrays and the draft stays legacy.
 *
 * @param string $stock_id Finished item code
 * @param string $date_ Work order date in the display format
 * @param object|null $carrier Header carrier holding the posted `classification_intents`
 * @param array $evidence Bounded scalar evidence
 * @return array payload, context
 * @throws DimensionFinancialReferenceWriteException
 */
function manufacturing_cost_classification_draft($stock_id, $date_, $carrier = null, $evidence = array())
{
	$result = array('payload' => array(), 'context' => array());
	if (!manufacturing_cost_classification_ready())
		return $result;
	$token = inventory_classification_token('wodraft');
	$key = dimension_source_revision_draft_key($token);
	$effective = date2sql($date_);
	if ($key === false || dimension_source_revision_date($effective) === false)
		throw new DimensionFinancialReferenceWriteException(_('The work order draft classification identity is invalid.'));
	$resolved = manufacturing_cost_classification_resolve_workorder($stock_id, $carrier, $evidence);
	$stored = inventory_classification_persist_revision(ST_WORKORDER, $key, 'header', 'header',
		$resolved['resolution'], $effective, '0.00000000', 'manufacturing');
	if (!$stored['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $stored['errors']));
	$result['payload'] = array('classification_draft_revisions' => array('document_key' => $key,
		'header' => (int)$stored['revision_id'], 'lines' => array()));
	$result['context'] = array('dimension_snapshot' => array('source_family' => 'manufacturing',
		'document_key' => $key, 'header' => array('revision_id' => (int)$stored['revision_id']),
		'lines' => array()));
	return $result;
}

/**
 * Rebuild the header carrier of a restored approval draft payload.
 *
 * @param mixed $draft_data Decoded draft payload after the approval snapshot was restored
 * @return stdClass
 */
function manufacturing_cost_classification_carrier($draft_data)
{
	$carrier = new stdClass();
	if (!is_array($draft_data))
		return $carrier;
	if (isset($draft_data['classification_intents']) && is_array($draft_data['classification_intents']))
		$carrier->classification_intents = $draft_data['classification_intents'];
	if (isset($draft_data['classification_draft_revisions']) && is_array($draft_data['classification_draft_revisions']))
		$carrier->classification_draft_revisions = $draft_data['classification_draft_revisions'];
	return $carrier;
}

/**
 * Build the header carrier of a page from its posted category selectors.
 *
 * @return stdClass|false Carrier, or false for a conflicting posted pair (selected and unassigned)
 */
function manufacturing_cost_classification_carrier_from_post()
{
	$carrier = new stdClass();
	if (!isset($_POST['wo_dimension_id']) && !isset($_POST['wo_dimension2_id'])
		&& !isset($_POST['_wo_classification_unassigned_1']) && !isset($_POST['_wo_classification_unassigned_2']))
		return $carrier;
	$intents = inventory_classification_intents_from_values(
		get_post('wo_dimension_id', 0), get_post('wo_dimension2_id', 0),
		get_post('_wo_classification_unassigned_1', 0), get_post('_wo_classification_unassigned_2', 0),
		'work_order_header');
	if ($intents === false)
		return false;
	$carrier->classification_intents = $intents;
	return $carrier;
}

/**
 * Build the header carrier an event is captured under, from the frozen cost-object pair.
 *
 * @param array $pair axis pair
 * @return stdClass
 */
function manufacturing_cost_classification_event_header($pair)
{
	$header = new stdClass();
	$intents = inventory_classification_intents_from_values($pair[1], $pair[2],
		$pair[1] > 0 ? 0 : 1, $pair[2] > 0 ? 0 : 1, 'work_order_cost_object');
	if ($intents !== false)
		$header->classification_intents = $intents;
	return $header;
}

/**
 * Capture one posted work-order cost event once, before its first ledger row.
 *
 * Every line repeats the frozen pair of the work order as its preserved original, so the line revision
 * names the order and its cost-object revision and the ledger rows reconcile to it by
 * `(type, type_no)`. A refused pair (closed category, unreadable lineage) throws; the caller cancels
 * its transaction.
 *
 * @param int $anchor_type ST_MANUISSUE, ST_MANURECEIVE, ST_JOURNAL or ST_WORKORDER
 * @param mixed $anchor_no Posted document number
 * @param string $date_ Event date in the display format
 * @param array $cost_object Frozen pair of the work order (with `revision_id`)
 * @param mixed $woid Work order id
 * @param array $lines list of array(stock_id, quantity, evidence)
 * @param string $event Event name sealed into the evidence
 * @param array $evidence Bounded scalar evidence
 * @return array|null capture result (axes per line index), or null on an install without the companion schema
 * @throws DimensionFinancialReferenceWriteException
 */
function manufacturing_cost_classification_capture_event($anchor_type, $anchor_no, $date_, $cost_object, $woid,
	$lines, $event, $evidence = array())
{
	if (!manufacturing_cost_classification_ready() || !is_array($cost_object))
		return null;
	$carriers = array();
	foreach ($lines as $line) {
		$line_evidence = isset($line[2]) && is_array($line[2]) ? $line[2] : array();
		$carrier = inventory_event_classification_line($line[0], $line[1],
			array(1 => $cost_object[1], 2 => $cost_object[2]), $line_evidence);
		$carrier->classification_evidence['lineage'] = 'work_order_cost_object';
		$carrier->classification_evidence['work_order'] = (int)$woid;
		$carrier->classification_evidence['cost_object_revision'] = (int)$cost_object['revision_id'];
		$carriers[] = $carrier;
	}
	if (!count($carriers))
		return null;
	$evidence = is_array($evidence) ? $evidence : array();
	$evidence['work_order'] = (int)$woid;
	return inventory_event_classification_capture($anchor_type, $anchor_no, $date_, $carriers, $event, $evidence,
		manufacturing_cost_classification_event_header($cost_object));
}

/**
 * Resolve the pair a work-order labour/overhead journal posts with.
 *
 * With the companion schema the work order's frozen pair is the only admissible pair: a caller-supplied
 * pair (a page default of the finished item, say) that differs is refused instead of silently
 * re-coding the order. Without the schema the caller's legacy pair is returned unchanged.
 *
 * @param mixed $woid Work order id
 * @param string $stock_id Finished item code
 * @param string $date_ Event date in the display format
 * @param mixed $dim1 Caller pair axis 1 (null = not supplied)
 * @param mixed $dim2 Caller pair axis 2 (null = not supplied)
 * @return array axes (1, 2) and the frozen cost object (`object`, null for a legacy install)
 * @throws DimensionFinancialReferenceWriteException
 */
function manufacturing_cost_classification_cost_pair($woid, $stock_id, $date_, $dim1, $dim2)
{
	$object = manufacturing_cost_classification_ensure_workorder($woid, $stock_id, $date_);
	if ($object === null)
		return array(1 => $dim1 === null ? 0 : $dim1, 2 => $dim2 === null ? 0 : $dim2, 'object' => null);
	foreach (array(1 => $dim1, 2 => $dim2) as $axis => $supplied)
		if ($supplied !== null && (int)$supplied !== (int)$object[$axis])
			throw new DimensionFinancialReferenceWriteException(
				_('Work order costs post on the classification frozen on the work order. Nothing was saved.'));
	return array(1 => (int)$object[1], 2 => (int)$object[2], 'object' => $object);
}

/**
 * Report the classified cost of one work order, lifetime and for an optional period.
 *
 * Capitalisation into WIP (material issue, BOM consumption, labour, overhead) and the release of WIP to
 * finished inventory are reported as balance-sheet movements. `period_expense` is only what a leg on a
 * non-WIP service/cost account recognised; capitalisation is never period cost.
 *
 * @param mixed $woid Work order id
 * @param string|null $from ISO date, inclusive (null = unbounded)
 * @param string|null $to ISO date, inclusive (null = unbounded)
 * @return array|null null for an unknown work order
 */
function manufacturing_cost_classification_report($woid, $from = null, $to = null)
{
	$id = dimension_source_revision_int($woid, 1);
	if ($id === false)
		return null;
	$order = db_query('SELECT wo.id, wo.stock_id, sm.wip_account FROM '.TB_PREF.'workorders wo
		LEFT JOIN '.TB_PREF.'stock_master sm ON sm.stock_id=wo.stock_id WHERE wo.id='.$id);
	$order = $order ? db_fetch_assoc($order) : false;
	if (!$order)
		return null;
	$sources = array(
		'material_issue' => 'SELECT '.ST_MANUISSUE.' AS type, issue_no AS no FROM '.TB_PREF.'wo_issues WHERE workorder_id='.$id,
		'bom_consumption' => 'SELECT '.ST_MANURECEIVE.' AS type, id AS no FROM '.TB_PREF.'wo_manufacture WHERE workorder_id='.$id,
		'labour' => 'SELECT trans_type AS type, trans_no AS no FROM '.TB_PREF.'wo_costing WHERE workorder_id='.$id
			.' AND cost_type='.(int)WO_LABOUR,
		'overhead' => 'SELECT trans_type AS type, trans_no AS no FROM '.TB_PREF.'wo_costing WHERE workorder_id='.$id
			.' AND cost_type='.(int)WO_OVERHEAD,
		'completion' => 'SELECT '.ST_WORKORDER.' AS type, '.$id.' AS no');
	$report = array('work_order' => $id, 'wip_account' => (string)$order['wip_account'],
		'cost_object' => manufacturing_cost_classification_workorder($id), 'categories' => array(),
		'wip_balance' => '0.00', 'period_expense' => '0.00', 'from' => $from, 'to' => $to);
	$wip_balance = 0.0;
	$period_expense = 0.0;
	foreach ($sources as $category => $source_sql) {
		$entry = array('lifetime' => 0.0, 'period' => 0.0, 'pairs' => array());
		$rows = db_query('SELECT gl.account, gl.dimension_id, gl.dimension2_id, gl.tran_date, cl.ctype, SUM(gl.amount) AS amount
			FROM ('.$source_sql.') src INNER JOIN '.TB_PREF.'gl_trans gl ON gl.type=src.type AND gl.type_no=src.no
			LEFT JOIN '.TB_PREF.'chart_master cm ON cm.account_code=gl.account
			LEFT JOIN '.TB_PREF.'chart_types ct ON ct.id=cm.account_type
			LEFT JOIN '.TB_PREF.'chart_class cl ON cl.cid=ct.class_id
			GROUP BY gl.account, gl.dimension_id, gl.dimension2_id, gl.tran_date, cl.ctype');
		while ($rows && ($row = db_fetch_assoc($rows))) {
			$amount = (float)$row['amount'];
			$in_period = ($from === null || $row['tran_date'] >= $from) && ($to === null || $row['tran_date'] <= $to);
			if ((string)$row['account'] === (string)$order['wip_account']) {
				// WIP legs: a debit is capitalisation into WIP, a credit is the release at completion.
				$entry['lifetime'] += $amount;
				if ($in_period)
					$entry['period'] += $amount;
				$wip_balance += $amount;
				$pair = (int)$row['dimension_id'].':'.(int)$row['dimension2_id'];
				$entry['pairs'][$pair] = (isset($entry['pairs'][$pair]) ? $entry['pairs'][$pair] : 0.0) + $amount;
			} elseif ($in_period && (int)$row['ctype'] >= 5) {
				// Only a leg that reached an expense/cost-of-sales class is period cost.
				$period_expense += $amount;
			}
		}
		foreach (array('lifetime', 'period') as $name)
			$entry[$name] = number_format($entry[$name], 2, '.', '');
		foreach ($entry['pairs'] as $pair => $amount)
			$entry['pairs'][$pair] = number_format($amount, 2, '.', '');
		$report['categories'][$category] = $entry;
	}
	$report['wip_balance'] = number_format($wip_balance, 2, '.', '');
	$report['period_expense'] = number_format($period_expense, 2, '.', '');
	return $report;
}
