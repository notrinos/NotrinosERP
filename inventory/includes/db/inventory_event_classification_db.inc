<?php
/**
 * DIM-INV-002: classification lineage of warehouse returns, warranty, count, scrap and consignment.
 *
 * This adapter sits on the DIM-INV-001 capture (`inventory_classification_capture()`), which already
 * resolves a pair through DIM-FND-005/DIM-FND-004 and stores it through DIM-FND-006 in the `inventory`
 * source family. It adds only what the warehouse events need on top of it:
 *
 *  - the pair that the original posting actually carried (a delivery or a receipt that a return
 *    undoes) is read from the ledger and outranks the current item default;
 *  - a warehouse document that is approved first (scrap, cycle count) captures a pending draft
 *    revision at submission and restores the sealed pair at execution, so the reviewed pair is the
 *    pair that posts even when the item default drifted while the draft waited;
 *  - a cycle count freezes its pair once at approval (anchor under the count identity) because the
 *    adjustment is posted by a later, separate action;
 *  - a consignment receipt freezes its intent under its own anchor so that recognising the consumed
 *    stock later is not driven by a changed item default;
 *  - a warranty accrual takes the pair of the delivery line revision that the delivery posted its COGS
 *    with, and a source-linked release repeats exactly the pair of the accrual it releases.
 *
 * Pure picking, putaway, packing and shipping move no value and have no financial engine: they are
 * deliberately not captured here. The adapter never writes a ledger row, never backfills a historical
 * document and never replaces `stock_moves`, `gl_trans` or the warranty log as the posted record.
 *
 * PHP 5.6 compatible.
 */
require_once __DIR__.'/inventory_classification_db.inc';

/**
 * Is the accepted companion revision schema installed so that warehouse events can be classified?
 *
 * @return bool
 */
function inventory_event_classification_ready()
{
	return inventory_classification_schema_ready();
}

/**
 * Normalize one stored/declared pair to the two-axis integer shape used by the capture.
 *
 * @param mixed $pair axis => identity, or null
 * @return array|null
 */
function inventory_event_classification_pair($pair)
{
	if (!is_array($pair) || !isset($pair[1]) || !isset($pair[2]))
		return null;
	$first = dimension_source_revision_int($pair[1], 0);
	$second = dimension_source_revision_int($pair[2], 0);
	if ($first === false || $second === false)
		return null;
	$normalized = array(1 => $first, 2 => $second);
	if (isset($pair['sources']) && is_array($pair['sources']))
		$normalized['sources'] = $pair['sources'];
	return $normalized;
}

/**
 * Find the single pair that an original stock document actually posted for one item.
 *
 * Only an unambiguous pair is returned: none, or a mixed set, returns null so the caller keeps the
 * next lineage source instead of guessing. A voided document has zeroed ledger rows and therefore
 * never establishes lineage.
 *
 * @param mixed $source_type Original document type (delivery or receipt)
 * @param mixed $source_no Original document number
 * @param string $stock_id Item code
 * @return array|null axis => stored identity plus `sources` evidence, or null
 */
function inventory_event_classification_source_pair($source_type, $source_no, $stock_id)
{
	$type = dimension_source_revision_int($source_type, 1);
	$number = dimension_source_revision_int($source_no, 1);
	if ($type === false || $number === false || !is_string($stock_id) || $stock_id === '')
		return null;
	$rows = db_query('SELECT DISTINCT gl.dimension_id, gl.dimension2_id
		FROM '.TB_PREF.'stock_moves m
		INNER JOIN '.TB_PREF.'stock_master sm ON sm.stock_id=m.stock_id
		INNER JOIN '.TB_PREF.'gl_trans gl ON gl.type=m.type AND gl.type_no=m.trans_no
			AND gl.account IN (sm.cogs_account, sm.inventory_account, sm.adjustment_account) AND gl.amount<>0
		WHERE m.stock_id='.db_escape($stock_id).' AND m.type='.$type.' AND m.trans_no='.$number);
	if (!$rows)
		return null;
	$pairs = array();
	while ($row = db_fetch_assoc($rows))
		$pairs[(int)$row['dimension_id'].':'.(int)$row['dimension2_id']] = array(
			1 => (int)$row['dimension_id'], 2 => (int)$row['dimension2_id']);
	if (count($pairs) !== 1)
		return null;
	$pair = array_pop($pairs);
	$pair['sources'] = array($type.':'.$number);
	return $pair;
}

/**
 * Build one line carrier for a warehouse event.
 *
 * @param string $stock_id Item code
 * @param mixed $quantity Event quantity (the sign is irrelevant: the record stores the absolute value)
 * @param array|null $original Preserved pair of the earlier posting this event undoes or consumes
 * @param array $evidence Bounded scalar evidence sealed with the line revision
 * @return stdClass
 */
function inventory_event_classification_line($stock_id, $quantity, $original = null, $evidence = array())
{
	$line = new stdClass();
	$line->stock_id = (string)$stock_id;
	$line->quantity = (float)$quantity;
	$line->classification_evidence = is_array($evidence) ? $evidence : array();
	$pair = inventory_event_classification_pair($original);
	if ($pair !== null) {
		$line->classification_original = array(1 => $pair[1], 2 => $pair[2]);
		if (isset($pair['sources'])) {
			$line->classification_evidence['lineage'] = 'original_posting';
			$line->classification_evidence['original_sources'] = array_slice($pair['sources'], 0, 20);
		}
	}
	return $line;
}

/**
 * Capture one warehouse event once, before its first ledger row, and return the pairs to post.
 *
 * Returns null on an install without the accepted companion schema, so the writer keeps its exact
 * legacy behavior. A refused pair (closed category, requiredness, unreadable lineage) throws, which
 * the surrounding transaction turns into a complete rollback.
 *
 * @param int $anchor_type ST_INVADJUST for a posted adjustment, or a revision anchor
 * @param mixed $anchor_no Posted adjustment number or anchor identity
 * @param string $date_ Event date in the display format
 * @param array $lines inventory_event_classification_line() carriers
 * @param string $event Event name sealed into the evidence
 * @param array $evidence Bounded scalar evidence
 * @param object|null $header Optional header carrier (restored approval intent)
 * @return array|null capture result (header_axes, axes per line index) or null for a legacy install
 * @throws DimensionFinancialReferenceWriteException
 */
function inventory_event_classification_capture($anchor_type, $anchor_no, $date_, $lines, $event,
	$evidence = array(), $header = null)
{
	if (!inventory_event_classification_ready())
		return null;
	$header = is_object($header) ? $header : new stdClass();
	$originals = array();
	$list = array_values($lines);
	foreach ($list as $index => $line)
		if (isset($line->classification_original))
			$originals[$index] = $line->classification_original;
	$evidence = is_array($evidence) ? $evidence : array();
	$evidence['event'] = (string)$event;
	return inventory_classification_capture($anchor_type, $header, $list, $date_, $anchor_no, $evidence, $originals);
}

/**
 * Overlay a captured pair on the legacy stock GL codes of one item.
 *
 * @param array $stock_gl get_stock_gl_code() result
 * @param array|null $captured Capture result
 * @param int $index Line index
 * @return array
 */
function inventory_event_classification_apply_pair($stock_gl, $captured, $index)
{
	if (is_array($captured) && isset($captured['axes'][$index])) {
		$stock_gl['dimension_id'] = (int)$captured['axes'][$index][1];
		$stock_gl['dimension2_id'] = (int)$captured['axes'][$index][2];
	}
	return $stock_gl;
}

/**
 * Capture a pending approval draft revision of a scrap or a cycle count.
 *
 * The returned `payload` keys are merged into the draft payload and the `context` is passed to the
 * approval submit call, which seals the reviewed pair into the envelope. An install without the
 * companion schema returns empty arrays and the draft stays legacy.
 *
 * @param int $trans_type ST_WH_SCRAP or ST_WH_CYCLE_COUNT
 * @param array $lines inventory_event_classification_line() carriers
 * @param string $date_ Event date in the display format
 * @param array $evidence Bounded scalar evidence
 * @return array payload, context
 * @throws DimensionFinancialReferenceWriteException
 */
function inventory_event_classification_draft($trans_type, $lines, $date_, $evidence = array())
{
	$result = array('payload' => array(), 'context' => array());
	if (!inventory_event_classification_ready() || !is_array($lines) || !count($lines))
		return $result;
	$cart = new stdClass();
	$cart->line_items = array_values($lines);
	$context = inventory_classification_approval_context($trans_type, $cart, $date_, $evidence);
	$items = array();
	foreach ($cart->line_items as $line)
		$items[] = array('stock_id' => $line->stock_id, 'quantity' => (float)$line->quantity,
			'classification_token' => $line->classification_token,
			'draft_line_key' => isset($line->draft_line_key) ? $line->draft_line_key : '',
			'classification_evidence' => isset($line->classification_evidence) ? $line->classification_evidence : array());
	$result['payload'] = array('classification_draft_token' => $cart->classification_draft_token,
		'classification_draft_revisions' => $cart->classification_draft_revisions, 'line_items' => $items);
	$result['context'] = $context;
	return $result;
}

/**
 * Rebuild the carriers of an executed approval draft from its (restored) payload.
 *
 * @param mixed $draft_data Decoded draft payload after the approval snapshot was restored
 * @param bool $keep_sealed Keep the pending draft revisions so a same-type final identity can bind them
 * @return array header carrier and line carriers
 */
function inventory_event_classification_carriers($draft_data, $keep_sealed = false)
{
	$header = new stdClass();
	$lines = array();
	if (!is_array($draft_data))
		return array('header' => $header, 'lines' => $lines);
	if (isset($draft_data['classification_intents']) && is_array($draft_data['classification_intents']))
		$header->classification_intents = $draft_data['classification_intents'];
	if ($keep_sealed) {
		if (isset($draft_data['classification_draft_token']))
			$header->classification_draft_token = (string)$draft_data['classification_draft_token'];
		if (isset($draft_data['classification_draft_revisions']) && is_array($draft_data['classification_draft_revisions']))
			$header->classification_draft_revisions = $draft_data['classification_draft_revisions'];
	}
	$items = isset($draft_data['line_items']) && is_array($draft_data['line_items']) ? $draft_data['line_items'] : array();
	foreach ($items as $item) {
		if (!is_array($item))
			continue;
		$line = new stdClass();
		$line->stock_id = isset($item['stock_id']) ? (string)$item['stock_id'] : '';
		$line->quantity = isset($item['quantity']) ? (float)$item['quantity'] : 0;
		if (isset($item['classification_intents']) && is_array($item['classification_intents']))
			$line->classification_intents = $item['classification_intents'];
		if ($keep_sealed && isset($item['classification_token']))
			$line->classification_token = (string)$item['classification_token'];
		if (isset($item['classification_evidence']) && is_array($item['classification_evidence']))
			$line->classification_evidence = $item['classification_evidence'];
		$lines[] = $line;
	}
	return array('header' => $header, 'lines' => $lines);
}

/**
 * Read the frozen pair of one line of an anchored event.
 *
 * @param int $anchor_type Anchor type
 * @param mixed $anchor_no Anchor identity
 * @param int $position One-based line position
 * @return array|null axis pair plus `revision_id` and the sealed `evidence`, or null when absent
 * @throws DimensionFinancialReferenceWriteException A present but unverifiable revision
 */
function inventory_event_classification_anchor_line($anchor_type, $anchor_no, $position)
{
	if (!inventory_event_classification_ready())
		return null;
	$key = dimension_source_revision_document_key($anchor_no);
	$line_key = dimension_source_revision_line_key('line', (int)$position);
	if ($key === false || $line_key === false)
		return null;
	$snapshot = dimension_source_revision_snapshot('inventory', $anchor_type, $key, 'line', $line_key);
	if ($snapshot['state'] === 'absent' || $snapshot['state'] === 'unavailable')
		return null;
	if ($snapshot['state'] !== 'available')
		throw new DimensionFinancialReferenceWriteException(
			_('The frozen classification of this warehouse event cannot be verified. Nothing was saved.'));
	$resolution = json_decode($snapshot['revision']['resolution_payload'], true);
	return array(1 => (int)$snapshot['revision']['dimension_id'], 2 => (int)$snapshot['revision']['dimension2_id'],
		'revision_id' => (int)$snapshot['revision']['source_revision_id'],
		'evidence' => is_array($resolution) && isset($resolution['evidence']) ? $resolution['evidence'] : array());
}

/**
 * Freeze a consignment receipt intent without ever blocking the receipt.
 *
 * A receipt recognises nothing in the ledger. A pair that cannot be resolved today (a company
 * requiredness rule, a closed item default) is simply not frozen, and the recognition resolves it as
 * its own new event instead of blocking vendor-owned stock from arriving.
 *
 * @param int $consignment_id Consignment record
 * @param string $stock_id Item code
 * @param mixed $quantity Received quantity
 * @param string $date_ Receipt date in the display format
 * @param array $evidence Bounded scalar evidence
 * @return array|null capture result, or null when nothing was frozen
 */
function inventory_event_classification_freeze_receipt($consignment_id, $stock_id, $quantity, $date_,
	$evidence = array())
{
	if (!inventory_event_classification_ready())
		return null;
	$line = inventory_event_classification_line($stock_id, $quantity, null, $evidence);
	$probe = inventory_classification_resolve_entry(ST_INVADJUST, new stdClass(), $line, true,
		array('evidence' => $line->classification_evidence));
	if (!$probe['ok'])
		return null;
	return inventory_event_classification_capture(INVENTORY_CLASSIFICATION_CONSIGNMENT_ANCHOR, (int)$consignment_id,
		$date_, array($line), 'consignment_receipt', $evidence);
}

/**
 * Read the pair that the delivery line posted its cost of goods sold with.
 *
 * @param mixed $delivery_no Delivery number
 * @param mixed $detail_id Persisted delivery line identity
 * @return array|null axis pair plus the sales `revision_id`, or null for a legacy delivery
 */
function inventory_event_classification_delivery_line($delivery_no, $detail_id)
{
	$document = dimension_source_revision_document_key($delivery_no);
	$line_key = dimension_source_revision_line_key('line', $detail_id);
	if ($document === false || $line_key === false)
		return null;
	$snapshot = dimension_source_revision_snapshot('sales', ST_CUSTDELIVERY, $document, 'line', $line_key);
	if ($snapshot['state'] !== 'available')
		return null;
	return array(1 => (int)$snapshot['revision']['dimension_id'], 2 => (int)$snapshot['revision']['dimension2_id'],
		'revision_id' => (int)$snapshot['revision']['source_revision_id']);
}

/**
 * Record the classification evidence of one warranty log entry.
 *
 * @param int $log_id Warranty log identity
 * @param string $date_ Effect date in the display format
 * @param string $stock_id Item code
 * @param mixed $quantity Quantity the provision relates to
 * @param array $pair Pair the effect was posted with
 * @param array $evidence Bounded scalar evidence (kind, source delivery, source accrual)
 * @return array|null capture result, or null on a legacy install
 * @throws DimensionFinancialReferenceWriteException
 */
function inventory_event_classification_record_warranty($log_id, $date_, $stock_id, $quantity, $pair,
	$evidence = array())
{
	if (!inventory_event_classification_ready())
		return null;
	$line = inventory_event_classification_line($stock_id, $quantity, $pair, $evidence);
	return inventory_event_classification_capture(INVENTORY_CLASSIFICATION_WARRANTY_ANCHOR, (int)$log_id, $date_,
		array($line), 'warranty_'.(isset($evidence['kind']) ? $evidence['kind'] : 'effect'), $evidence);
}
