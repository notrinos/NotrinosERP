<?php
/**
 * DIM-SAL-003: credit note, RMA return and customer-receipt classification lineage.
 *
 * A credit note (and the credit an RMA produces) is the inverse of an already posted invoice line, and a
 * customer receipt is a settlement. Neither is a new revenue event, so this adapter never recomputes
 * intent from a current customer/item default:
 *
 *  - a credit line resolves through the shared DIM-FND-005 order with the source invoice-line snapshot
 *    (recorded by DIM-SAL-002) as its authority. Revenue and line discount reversal carry that pair, and
 *    the COGS/stock (or write-off) legs carry the pair of the delivery line that owned the original COGS,
 *    so a partial return reproduces exactly what was originally posted;
 *  - an RMA credit links each credit line to a source document line through a bounded carrier instead of
 *    the quantity-bookkeeping `src_id`, so the RMA flow keeps its legacy allocation behavior;
 *  - a credit with no snapshotted source (standalone, or from a legacy invoice) keeps the characterized
 *    E08 behavior and records no synthetic lineage;
 *  - a receipt stores one header revision that names the settlement postings (bank, receivable, discount,
 *    charge, variance) and states that it recognizes no revenue or cost and re-attributes no allocation;
 *  - commission calculation and approval records stay operational: the credit revision only states that
 *    the reversal is a link and that no commission expense is posted.
 *
 * The adapter contains no ledger, stock or native-table write of its own; revisions go only through the
 * shared DIM-FND-006 authority and every posting still passes the maintained writers and DIM-FND-007
 * admission. PHP 5.6 compatible.
 */
require_once __DIR__.'/sales_fulfillment_classification_db.inc';

// Focused CLI harnesses evaluate the writers without the page bootstrap; existing definitions win.
foreach (array('ST_CUSTCREDIT' => 11, 'ST_CUSTPAYMENT' => 12) as $sales_credit_type_name => $sales_credit_type_value)
	if (!defined($sales_credit_type_name))
		define($sales_credit_type_name, $sales_credit_type_value);

/**
 * Transaction types whose classification lineage this adapter records.
 *
 * @return int[]
 */
function sales_credit_supported_types()
{
	return array(ST_CUSTCREDIT, ST_CUSTPAYMENT);
}

/**
 * Locate the source line of one credit line.
 *
 * The carrier `classification_source_line = array('trans_type' => 10|13, 'id' => detail id)` is used by
 * the RMA flow; the native invoice-credit flow uses the persisted `src_id`.
 *
 * @param object $line Credit line
 * @return array|false trans_type and id
 */
function sales_credit_source_carrier($line)
{
	if (!is_object($line))
		return false;
	if (isset($line->classification_source_line) && is_array($line->classification_source_line)) {
		$type = isset($line->classification_source_line['trans_type'])
			? dimension_source_revision_int($line->classification_source_line['trans_type'], 1) : false;
		$id = isset($line->classification_source_line['id'])
			? dimension_source_revision_int($line->classification_source_line['id'], 1) : false;
		if ($type === false || $id === false || !in_array($type, array(ST_SALESINVOICE, ST_CUSTDELIVERY), true))
			throw new DimensionFinancialReferenceWriteException(
				_('The source line of this credit cannot be identified. Nothing was saved.'));
		return array('trans_type' => $type, 'id' => $id);
	}
	if (isset($line->src_id) && dimension_source_revision_int($line->src_id, 1) !== false)
		return array('trans_type' => ST_SALESINVOICE, 'id' => (int)$line->src_id);
	return false;
}

/**
 * Locate the persisted parent line (invoice line, or delivery line for an RMA against a delivery).
 *
 * @param object $line Credit line
 * @param bool $prepaid Unused; credits have no prepayment flow
 * @return array|false trans_type, document_key and line_key; false when the line has no source
 */
function sales_credit_parent_line($line, $prepaid = false)
{
	$carrier = sales_credit_source_carrier($line);
	if ($carrier === false)
		return false;
	$rows = db_query('SELECT debtor_trans_no,debtor_trans_type FROM '.TB_PREF
		.'debtor_trans_details WHERE id='.(int)$carrier['id']);
	if (!$rows)
		throw new DimensionFinancialReferenceWriteException(
			_('The Sales source line of this credit cannot be read.'));
	$row = db_fetch_assoc($rows);
	if (!$row || (int)$row['debtor_trans_type'] !== (int)$carrier['trans_type'])
		return false;
	$document = dimension_source_revision_document_key($row['debtor_trans_no']);
	$line_key = dimension_source_revision_line_key('line', $carrier['id']);
	if ($document === false || $line_key === false)
		return false;
	return array('trans_type' => (int)$carrier['trans_type'], 'document_key' => $document, 'line_key' => $line_key);
}

/**
 * Name the credit flow.
 *
 * @param object $doc Credit cart
 * @param int $trans_type Document type
 * @param array $parents Distinct parent descriptors
 * @return string rma_credit or invoice_credit
 */
function sales_credit_flow($doc, $trans_type, $parents)
{
	return is_object($doc) && isset($doc->classification_rma) && is_array($doc->classification_rma)
		? 'rma_credit' : 'invoice_credit';
}

/**
 * Pair that the COGS/stock legs of one credit line reverse.
 *
 * The invoice line revision names the delivery line that owned the original COGS. The credit reverses
 * that delivery-line pair, which equals the revenue pair unless the invoice recorded an explicit
 * override. A source with no delivery-line evidence reverses the credit line's own pair.
 *
 * @param array $line_plan Resolved line plan
 * @return array pair (axis => value) and the evidence of where it came from
 */
function sales_credit_cogs_owner($line_plan)
{
	$line_pair = array(1 => (int)$line_plan['axes'][1]['value'], 2 => (int)$line_plan['axes'][2]['value']);
	$owner = array('pair' => $line_pair, 'source' => array('state' => 'line_pair'));
	$parent = $line_plan['parent'];
	if ($parent === false || (int)$parent['trans_type'] !== ST_SALESINVOICE)
		return $owner;
	$invoice_line = sales_fulfillment_read_snapshot($parent['trans_type'], $parent['document_key'], 'line',
		$parent['line_key']);
	if ($invoice_line['state'] !== 'available' || empty($invoice_line['revision']['resolution_payload']))
		return $owner;
	$payload = json_decode((string)$invoice_line['revision']['resolution_payload'], true);
	if (!is_array($payload) || !isset($payload['cogs_recognized_on']['document_key'],
		$payload['cogs_recognized_on']['line_key']))
		return $owner;
	$recognized = $payload['cogs_recognized_on'];
	$delivery_line = sales_fulfillment_read_snapshot(ST_CUSTDELIVERY, (string)$recognized['document_key'],
		'line', (string)$recognized['line_key']);
	if ($delivery_line['state'] !== 'available')
		return $owner;
	$pair = array(1 => (int)$delivery_line['revision']['axes'][1]['value'],
		2 => (int)$delivery_line['revision']['axes'][2]['value']);
	if ($pair !== $line_pair) {
		$validation = dimension_validation_classification($pair[1], $pair[2], 'new_event', array());
		if ($validation['ok'] !== true)
			throw new DimensionFinancialReferenceWriteException(implode(' ', $validation['errors']));
	}
	return array('pair' => $pair, 'source' => array('state' => 'delivery_line', 'trans_type' => ST_CUSTDELIVERY,
		'document_key' => (string)$recognized['document_key'], 'line_key' => (string)$recognized['line_key'],
		'revision_id' => isset($delivery_line['revision']['source_revision_id'])
			? (int)$delivery_line['revision']['source_revision_id'] : 0));
}

/**
 * Resolve the classification of one credit note from its source lines, before any row is written.
 *
 * @param object $doc Credit cart
 * @param string $credit_type Return or WriteOff
 * @return array|null Plan, or null when the credit keeps the characterized legacy behavior
 */
function sales_credit_plan($doc, $credit_type)
{
	$plan = sales_fulfillment_resolve_plan($doc, ST_CUSTCREDIT, array('types' => array(ST_CUSTCREDIT),
		'parent' => 'sales_credit_parent_line', 'flow' => 'sales_credit_flow'));
	if (!is_array($plan))
		return null;
	if (!in_array($credit_type, array('Return', 'WriteOff'), true))
		throw new DimensionFinancialReferenceWriteException(_('The credit type is invalid. Nothing was saved.'));
	$plan['credit_type'] = $credit_type;
	foreach ($plan['lines'] as $index => $line_plan)
		$plan['lines'][$index]['cogs'] = sales_credit_cogs_owner($line_plan);
	return $plan;
}

/**
 * Pair of the COGS/stock legs of one planned credit line; null keeps the legacy resolver.
 *
 * @param array|null $plan Credit plan
 * @param mixed $index Cart line index
 * @return array|null axis => value
 */
function sales_credit_cogs_pair($plan, $index)
{
	if (!is_array($plan) || !isset($plan['lines'][$index]['cogs']['pair'][1], $plan['lines'][$index]['cogs']['pair'][2]))
		return null;
	return array(1 => (int)$plan['lines'][$index]['cogs']['pair'][1],
		2 => (int)$plan['lines'][$index]['cogs']['pair'][2]);
}

/**
 * Distinct posting roles of a credit document and how each is classified.
 *
 * @param string $credit_type Return or WriteOff
 * @return array role => basis
 */
function sales_credit_posting_roles($credit_type)
{
	return array('revenue_reversal' => 'line', 'line_discount_reversal' => 'line',
		'cogs_reversal' => 'delivery_line_pair',
		$credit_type === 'WriteOff' ? 'write_off' : 'inventory_return' => 'delivery_line_pair',
		'receivable' => 'header', 'freight' => 'header', 'tax' => 'header',
		'commission' => 'operational_link_no_posting');
}

/**
 * Bounded evidence of the RMA that produced a credit.
 *
 * @param object $doc Credit cart
 * @return array|null
 */
function sales_credit_rma_evidence($doc)
{
	if (!is_object($doc) || !isset($doc->classification_rma) || !is_array($doc->classification_rma))
		return null;
	$rma = $doc->classification_rma;
	$evidence = array();
	foreach (array('rma_id', 'source_type', 'source_no') as $name)
		$evidence[$name] = isset($rma[$name]) ? (int)$rma[$name] : 0;
	$evidence['reference'] = isset($rma['reference']) && is_string($rma['reference'])
		? substr(preg_replace('/[\x00-\x1f\x7f]/', ' ', $rma['reference']), 0, 60) : '';
	return $evidence;
}

/**
 * Record the header revision of one written credit note.
 *
 * @param object $doc Written credit cart
 * @param mixed $credit_no Final native number
 * @param array|null $plan Plan, or null for a legacy credit
 * @return int Revision id, or 0 when nothing is recorded
 */
function sales_credit_record_header($doc, $credit_no, $plan)
{
	if (!is_array($plan))
		return 0;
	$document = dimension_source_revision_document_key($credit_no);
	if ($document === false)
		throw new DimensionFinancialReferenceWriteException(_('The saved credit identity is invalid.'));
	$resolution = array('contract' => 'sales-credit-classification/1', 'trans_type' => ST_CUSTCREDIT,
		'role' => 'header', 'flow' => $plan['flow'], 'credit_type' => $plan['credit_type'],
		'enabled_axes' => (int)$plan['enabled_axes'], 'policy_version' => (int)$plan['header']['policy_version'],
		'axes' => array(1 => $plan['header']['axes'][1], 2 => $plan['header']['axes'][2]),
		'source' => $plan['header']['source'], 'posting_roles' => sales_credit_posting_roles($plan['credit_type']),
		'inverse' => array('basis' => 'source_line_pair', 'default_recalculation' => false),
		'line_count' => count($plan['lines']), 'legacy_line_count' => count($plan['legacy_lines']));
	$rma = sales_credit_rma_evidence($doc);
	if ($rma !== null)
		$resolution['rma'] = $rma;
	if (!empty($doc->salesman) && get_company_pref('use_advanced_commissions'))
		$resolution['commission'] = array('treatment' => 'operational_reversal_link', 'posted_expense' => false);
	$override_reason = '';
	if ($plan['header']['override'][1] || $plan['header']['override'][2]) {
		$resolution['override'] = array('axes' => array_keys(array_filter($plan['header']['override'])));
		$override_reason = 'Credit header differs from the source classification';
	}
	$stored = sales_fulfillment_persist_revision(ST_CUSTCREDIT, $document, 'header', 'header', $resolution,
		$plan['effective_date'], '0.00000000', $override_reason);
	return (int)$stored['revision_id'];
}

/**
 * Record the line revision of one written credit line.
 *
 * @param object $doc Written credit cart
 * @param mixed $credit_no Final native number
 * @param mixed $index Cart line index
 * @param mixed $detail_id Persisted debtor_trans_details id
 * @param array|null $plan Plan
 * @return int Revision id, or 0 when the line is legacy
 */
function sales_credit_record_line($doc, $credit_no, $index, $detail_id, $plan)
{
	if (!is_array($plan) || !isset($plan['lines'][$index]))
		return 0;
	$document = dimension_source_revision_document_key($credit_no);
	$line_key = dimension_source_revision_line_key('line', $detail_id);
	if ($document === false || $line_key === false)
		throw new DimensionFinancialReferenceWriteException(_('The saved credit line identity is invalid.'));
	$line = $doc->line_items[$index];
	$line_plan = $plan['lines'][$index];
	$resolution = array('contract' => 'sales-credit-classification/1', 'trans_type' => ST_CUSTCREDIT,
		'role' => 'line', 'flow' => $plan['flow'], 'credit_type' => $plan['credit_type'],
		'enabled_axes' => (int)$plan['enabled_axes'], 'policy_version' => (int)$line_plan['policy_version'],
		'axes' => array(1 => $line_plan['axes'][1], 2 => $line_plan['axes'][2]),
		'stock_id' => (string)$line->stock_id, 'source' => $line_plan['source'],
		'cogs_pair' => array((int)$line_plan['cogs']['pair'][1], (int)$line_plan['cogs']['pair'][2]),
		'cogs_source' => $line_plan['cogs']['source'],
		'posting_roles' => sales_credit_posting_roles($plan['credit_type']));
	$override_reason = '';
	if ($line_plan['override'] !== null) {
		$resolution['override'] = array('axes' => $line_plan['override']['axes'],
			'source_values' => $line_plan['override']['source_values']);
		$override_reason = $line_plan['override']['reason'];
	}
	$stored = sales_fulfillment_persist_revision(ST_CUSTCREDIT, $document, 'line', $line_key, $resolution,
		$plan['effective_date'], sales_classification_quantity($line->qty_dispatched), $override_reason);
	return (int)$stored['revision_id'];
}

/**
 * Resolve the classification of one customer receipt before any row is written.
 *
 * The receipt page's selectors are the receipt's own settlement classification; a blank selector is an
 * explicit unassigned (or policy not-applicable) state. The pair goes through the DIM-FND-004 new-event
 * contract so a closed, disabled or wrong-axis identity is refused before the first native row.
 *
 * @param mixed $dimension_id Selected category 1
 * @param mixed $dimension2_id Selected category 2
 * @param string $date_ User-format receipt date
 * @return array|null Plan, or null when the company has no companion revision schema
 */
function sales_receipt_plan($dimension_id, $dimension2_id, $date_)
{
	if (!sales_fulfillment_schema_ready())
		return null;
	$effective = dimension_source_revision_date(date2sql($date_));
	if ($effective === false)
		throw new DimensionFinancialReferenceWriteException(_('The Sales receipt date is invalid.'));
	$enabled = (int)get_company_pref('use_dimension');
	$sources = array(1 => array(), 2 => array());
	foreach (array(1 => $dimension_id, 2 => $dimension2_id) as $axis => $raw) {
		$native = dimension_source_revision_int($raw === null || $raw === '' ? 0 : $raw, 0);
		if ($native === false)
			throw new DimensionFinancialReferenceWriteException(
				_('The receipt classification identity is invalid. Nothing was saved.'));
		if ($axis > $enabled)
			$native = 0;
		$sources[$axis]['header_explicit'] = $native > 0
			? dimension_resolution_selected($native, 'document_header')
			: dimension_resolution_inherit('receipt_header');
	}
	$pair = dimension_resolution_pair($sources[1], $sources[2], array('enabled_axes' => $enabled));
	if (!$pair['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $pair['errors']));
	$pair = sales_fulfillment_finalize_pair($pair, $enabled);
	return array('contract' => 'sales-receipt-plan/1', 'effective_date' => $effective, 'enabled_axes' => $enabled,
		'axes' => $pair['axes'], 'policy_version' => (int)$pair['policy_version']);
}

/**
 * Record the settlement revision of one written customer receipt.
 *
 * @param mixed $payment_no Final native number
 * @param array|null $plan Receipt plan
 * @param array $amounts amount, discount and charge in their own currencies
 * @return int Revision id, or 0 when nothing is recorded
 */
function sales_receipt_record($payment_no, $plan, $amounts)
{
	if (!is_array($plan))
		return 0;
	$document = dimension_source_revision_document_key($payment_no);
	if ($document === false)
		throw new DimensionFinancialReferenceWriteException(_('The saved receipt identity is invalid.'));
	$resolution = array('contract' => 'sales-receipt-classification/1', 'trans_type' => ST_CUSTPAYMENT,
		'role' => 'header', 'flow' => 'receipt', 'enabled_axes' => (int)$plan['enabled_axes'],
		'policy_version' => (int)$plan['policy_version'], 'axes' => array(1 => $plan['axes'][1], 2 => $plan['axes'][2]),
		'recognition' => 'cash_settlement', 'profit_effect' => 'none',
		'posting_roles' => array('bank' => 'settlement', 'receivable' => 'settlement',
			'payment_discount' => 'settlement', 'bank_charge' => 'settlement', 'exchange_variance' => 'settlement'),
		'allocation_policy' => 'settlement_links_only; no re-attribution of the receipt or the settled invoice; '
			.'cross-classification allocation is unsupported here',
		'amounts' => array('amount' => sales_fulfillment_decimal_text(isset($amounts['amount']) ? $amounts['amount'] : 0),
			'discount' => sales_fulfillment_decimal_text(isset($amounts['discount']) ? $amounts['discount'] : 0),
			'charge' => sales_fulfillment_decimal_text(isset($amounts['charge']) ? $amounts['charge'] : 0)));
	$stored = sales_fulfillment_persist_revision(ST_CUSTPAYMENT, $document, 'header', 'header', $resolution,
		$plan['effective_date'], '0.00000000', '');
	return (int)$stored['revision_id'];
}

/**
 * Link every line of one RMA to the source document line it returns.
 *
 * Only an invoice or delivery source can be linked. A source line is identified by item; when several
 * source lines carry the item they must agree on the recorded pair (then the first is the link), or one
 * of them must have the quantity to cover the return. An unresolvable item is refused instead of being
 * attributed from a current default.
 *
 * @param array $rma RMA header row
 * @param array $lines RMA line rows (stock_id and quantity_authorized)
 * @return array|null line index => array(trans_type, id, revision_id, pair); null for no snapshotted source
 */
function sales_credit_rma_source_links($rma, $lines)
{
	if (!sales_fulfillment_schema_ready() || !is_array($rma))
		return null;
	$source_type = isset($rma['source_type']) ? (int)$rma['source_type'] : 0;
	$source_no = isset($rma['source_no']) ? (int)$rma['source_no'] : 0;
	if (!in_array($source_type, array(ST_SALESINVOICE, ST_CUSTDELIVERY), true) || $source_no <= 0)
		return null;
	$links = array();
	$any_snapshot = false;
	foreach ($lines as $index => $line) {
		$rows = db_query('SELECT id,quantity,qty_done FROM '.TB_PREF.'debtor_trans_details WHERE debtor_trans_type='
			.(int)$source_type.' AND debtor_trans_no='.(int)$source_no.' AND stock_id='.db_escape($line['stock_id'])
			.' ORDER BY id');
		if (!$rows)
			throw new DimensionFinancialReferenceWriteException(_('The RMA source document cannot be read.'));
		$candidates = array();
		while ($row = db_fetch_assoc($rows)) {
			$key = dimension_source_revision_line_key('line', $row['id']);
			$snapshot = sales_fulfillment_read_snapshot($source_type, (string)$source_no, 'line', $key);
			$candidates[] = array('id' => (int)$row['id'], 'quantity' => (float)$row['quantity'],
				'qty_done' => (float)$row['qty_done'], 'snapshot' => $snapshot);
		}
		if (!count($candidates))
			continue;
		$snapshotted = array_values(array_filter($candidates, function($candidate) {
			return $candidate['snapshot']['state'] === 'available';
		}));
		if (!count($snapshotted))
			continue;
		$any_snapshot = true;
		$pairs = array();
		foreach ($snapshotted as $candidate)
			$pairs[json_encode(array((int)$candidate['snapshot']['revision']['axes'][1]['value'],
				(int)$candidate['snapshot']['revision']['axes'][2]['value']))] = true;
		$chosen = $snapshotted[0];
		if (count($pairs) > 1) {
			$chosen = null;
			foreach ($snapshotted as $candidate)
				if ($candidate['quantity'] - $candidate['qty_done'] + 1e-9 >= (float)$line['quantity_authorized']) {
					$chosen = $candidate;
					break;
				}
			if ($chosen === null)
				throw new DimensionFinancialReferenceWriteException(sprintf(
					_('The returned item %s matches several source lines with different classifications. Nothing was saved.'),
					(string)$line['stock_id']));
		}
		if (count($snapshotted) < count($candidates) && count($pairs) > 1)
			throw new DimensionFinancialReferenceWriteException(sprintf(
				_('The returned item %s matches source lines that are not all classified. Nothing was saved.'),
				(string)$line['stock_id']));
		$links[$index] = array('trans_type' => $source_type, 'id' => $chosen['id'],
			'revision_id' => (int)$chosen['snapshot']['revision']['source_revision_id'],
			'pair' => array((int)$chosen['snapshot']['revision']['axes'][1]['value'],
				(int)$chosen['snapshot']['revision']['axes'][2]['value']));
	}
	return $any_snapshot ? $links : null;
}

/**
 * Bounded, deterministic evidence of an RMA's source links for the approval draft.
 *
 * @param array $rma RMA header row
 * @param array $lines RMA line rows
 * @return array version, state and per-line source revision and pair
 */
function sales_credit_rma_approval_evidence($rma, $lines)
{
	$links = sales_credit_rma_source_links($rma, $lines);
	if ($links === null)
		return array('version' => 1, 'state' => 'legacy');
	$evidence = array();
	foreach ($links as $index => $link)
		$evidence[(string)$index] = array('pair' => $link['pair'], 'source_revision_id' => $link['revision_id'],
			'source' => array((int)$link['trans_type'], (int)$link['id']));
	return array('version' => 1, 'state' => 'snapshot', 'lines' => $evidence);
}
