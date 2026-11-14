<?php
/**
 * DIM-PUR-004: supplier credit-note, supplier payment and source-read classification lineage.
 *
 * A supplier credit note is the inverse of an already posted supplier bill line, and a supplier payment is
 * a settlement. Neither is a new cost-recognition event, so this adapter never recomputes intent from a
 * current supplier/item default:
 *
 *  - a credit line resolves through the shared DIM-FND-005 order (reused from the DIM-PUR-003 resolver
 *    through its optional hooks) with the revision of the supplier-bill line it credits as the only
 *    parent. The stock/expense reversal carries that pair, the payable and tax reversal carry the
 *    source bill's header pair, and the item default is never a source. A bill that pre-dates the
 *    lineage (no revision) or a standalone credit keeps the characterized E10 behavior and records
 *    nothing: no lineage is reconstructed from a current default;
 *  - a direct (GL account) credit line is explicit by construction; the pair reloaded from the source
 *    bill line or entered on the line is recorded as that line's evidence;
 *  - a credit never posts a new GRN clearing leg: the revision states the reversal policy, the original
 *    billed quantity/price that was credited and whether the credited price differs from the source;
 *  - a payment stores one settlement header revision under its own selected pair. It states
 *    `recognition = cash_settlement` and `profit_effect = none`, names the payable, discount, bank
 *    charge, bank and exchange-variance rows as settlement postings and documents that allocations are
 *    settlement links only, so a payment can never duplicate the cost the bill recognized;
 *  - the reload of a recorded bill/credit takes the header pair from its own revision instead of the
 *    supplier master, so a credit started from an invoice starts from the original classification.
 *
 * The adapter contains no ledger, stock or native-table write of its own; revisions go only through the
 * DIM-PUR-003 persist helper over the DIM-FND-006 authority and every posting still passes the maintained
 * writers and DIM-FND-007 admission. PHP 5.6 compatible.
 */
require_once __DIR__.'/purchasing_receipt_invoice_classification_db.inc';

// Focused CLI harnesses evaluate the writers without the page bootstrap; existing definitions win.
foreach (array('ST_SUPPCREDIT' => 21, 'ST_SUPPAYMENT' => 22) as $purchasing_credit_type_name => $purchasing_credit_type_value)
	if (!defined($purchasing_credit_type_name))
		define($purchasing_credit_type_name, $purchasing_credit_type_value);

/**
 * Transaction types whose classification lineage this adapter records.
 *
 * @return int[]
 */
function purchasing_credit_supported_types()
{
	return array(ST_SUPPCREDIT, ST_SUPPAYMENT);
}

/**
 * Locate the supplier-bill line that one credit line reverses.
 *
 * A credit line is a goods-receipt line that was already billed. The live (non-voided) bill line of that
 * receipt line is the source; a bill named by the credit's source documents wins over any other bill of
 * the same receipt line, then the latest bill.
 *
 * @param object $line Credit line (`id` is the goods-receipt line)
 * @param object $doc Credit document (`src_docs` names the source bills)
 * @return array|false trans_type, document_key and line_key; false when no bill line exists
 */
function purchasing_credit_source_bill_line($line, $doc)
{
	$grn_item = dimension_source_revision_int(is_object($line) && isset($line->id) ? $line->id : 0, 1);
	if ($grn_item === false)
		return false;
	$preferred = array();
	if (is_object($doc) && isset($doc->src_docs) && is_array($doc->src_docs))
		foreach (array_keys($doc->src_docs) as $bill_no) {
			$bill = dimension_source_revision_int($bill_no, 1);
			if ($bill !== false)
				$preferred[] = $bill;
		}
	$order = count($preferred)
		? 'FIELD(supp_trans_no,'.implode(',', array_reverse($preferred)).') DESC,' : '';
	$rows = db_query('SELECT id,supp_trans_no FROM '.TB_PREF.'supp_invoice_items WHERE supp_trans_type='.ST_SUPPINVOICE
		.' AND grn_item_id='.$grn_item.' AND quantity>0 ORDER BY '.$order.'supp_trans_no DESC,id DESC LIMIT 1');
	if (!$rows)
		throw new DimensionFinancialReferenceWriteException(
			_('The supplier bill that this credit reverses cannot be read. Nothing was saved.'));
	$row = db_fetch_assoc($rows);
	if (!$row)
		return false;
	$document = dimension_source_revision_document_key($row['supp_trans_no']);
	$line_key = dimension_source_revision_line_key('line', $row['id']);
	if ($document === false || $line_key === false)
		return false;
	return array('trans_type' => ST_SUPPINVOICE, 'document_key' => $document, 'line_key' => $line_key);
}

/**
 * Resolver hook: the ordered parents of one credit line (the bill line only).
 *
 * @param object $line Credit line
 * @param object $doc Credit document
 * @return array
 */
function purchasing_credit_line_parents($line, $doc)
{
	return array(purchasing_credit_source_bill_line($line, $doc));
}

/**
 * Resolver hook: name the credit flow.
 *
 * @param object $doc Credit document
 * @param int $trans_type Document type
 * @param array $parents Distinct header parents
 * @param int $line_count Parent-bound line count
 * @return string invoice_credit, or standalone_credit when no billed line is reversed
 */
function purchasing_credit_flow($doc, $trans_type, $parents, $line_count)
{
	return count($parents) ? 'invoice_credit' : 'standalone_credit';
}

/**
 * Plan the classification of one supplier credit note before any row is written.
 *
 * @param object $doc supp_trans credit about to be posted (trans_type ST_SUPPCREDIT)
 * @return array|null Plan, or null when the credit keeps the characterized legacy behavior
 */
function purchasing_credit_plan($doc)
{
	if (!is_object($doc) || !isset($doc->trans_type) || (int)$doc->trans_type !== ST_SUPPCREDIT)
		return null;
	$plan = purchasing_receipt_resolve_plan($doc, ST_SUPPCREDIT, array('types' => array(ST_SUPPCREDIT),
		'parents' => 'purchasing_credit_line_parents', 'flow' => 'purchasing_credit_flow', 'strict_lines' => true));
	if (!is_array($plan))
		return null;
	$sources = array();
	foreach ($plan['parents'] as $parent)
		$sources[] = (string)$parent['document_key'];
	$plan['source_documents'] = $sources;
	return $plan;
}

/**
 * The distinct posting roles of a credit note and how each is classified.
 *
 * @return array role => basis
 */
function purchasing_credit_posting_roles()
{
	return array('stock_or_expense_reversal' => 'source_bill_line_pair', 'payable_reduction' => 'header',
		'tax_reversal' => 'header_not_expense', 'direct_gl_expense_reversal' => 'explicit_line',
		'grn_clearing' => 'not_posted_by_credit', 'supplier_discount' => 'header');
}

/**
 * Read the stored bill line a credit line reverses (quantity and price as billed).
 *
 * @param array $parent Parent descriptor
 * @return array quantity and unit price in canonical text, or empty when unreadable
 */
function purchasing_credit_source_line_facts($parent)
{
	if (!is_array($parent) || !isset($parent['line_key']) || strpos($parent['line_key'], 'line:') !== 0)
		return array();
	$id = dimension_source_revision_int(substr($parent['line_key'], 5), 1);
	if ($id === false)
		return array();
	$rows = db_query('SELECT quantity,unit_price FROM '.TB_PREF.'supp_invoice_items WHERE id='.$id);
	if (!$rows)
		throw new DimensionFinancialReferenceWriteException(
			_('The supplier bill that this credit reverses cannot be read. Nothing was saved.'));
	$row = db_fetch_assoc($rows);
	return $row ? array('quantity' => purchasing_receipt_decimal_text($row['quantity']),
		'unit_price' => purchasing_receipt_decimal_text($row['unit_price'])) : array();
}

/**
 * Record the header revision of one written supplier credit note.
 *
 * @param mixed $credit_no Final native number
 * @param array|null $plan Credit plan, or null for a legacy credit
 * @param array $amounts items, tax and discount of the credit (any sign; stored as magnitudes)
 * @param array $fx currency and rate the credit posted with
 * @return int Revision id, or 0 when nothing is recorded
 */
function purchasing_credit_record_header($credit_no, $plan, $amounts = array(), $fx = array())
{
	if (!is_array($plan))
		return 0;
	$document = dimension_source_revision_document_key($credit_no);
	if ($document === false)
		throw new DimensionFinancialReferenceWriteException(_('The saved supplier credit identity is invalid.'));
	$magnitude = function($name) use ($amounts) {
		return purchasing_receipt_decimal_text(abs(isset($amounts[$name]) ? (float)$amounts[$name] : 0));
	};
	$resolution = array('contract' => 'purchasing-credit-classification/1', 'trans_type' => ST_SUPPCREDIT,
		'role' => 'header', 'flow' => $plan['flow'], 'enabled_axes' => (int)$plan['enabled_axes'],
		'policy_version' => (int)$plan['header']['policy_version'],
		'axes' => array(1 => $plan['header']['axes'][1], 2 => $plan['header']['axes'][2]),
		'source' => $plan['header']['source'], 'source_documents' => $plan['source_documents'],
		'posting_roles' => purchasing_credit_posting_roles(),
		'inverse' => array('basis' => 'source_bill_line_pair', 'default_recalculation' => false),
		'amounts' => array('items' => $magnitude('items'), 'tax' => $magnitude('tax'),
			'discount' => $magnitude('discount')),
		'fx_policy' => 'pair is currency independent; amounts convert through the unchanged writers',
		'fx' => array('currency' => isset($fx['currency']) ? substr((string)$fx['currency'], 0, 3) : '',
			'rate' => purchasing_receipt_decimal_text(isset($fx['rate']) ? $fx['rate'] : 1)),
		'line_count' => count($plan['lines']), 'gl_line_count' => count($plan['gl_lines']),
		'legacy_line_count' => count($plan['legacy_lines']));
	$override_reason = '';
	if ($plan['header']['override'][1] || $plan['header']['override'][2]) {
		$resolution['override'] = array('axes' => array_keys(array_filter($plan['header']['override'])));
		$override_reason = 'Credit header differs from the source classification';
	}
	$stored = purchasing_receipt_persist_revision(ST_SUPPCREDIT, $document, 'header', 'header', $resolution,
		$plan['effective_date'], '0.00000000', $override_reason);
	return (int)$stored['revision_id'];
}

/**
 * Record the line revision of one written credit line.
 *
 * @param object $line Written credit line (its quantity may already be negated by the writer)
 * @param mixed $credit_no Final native number
 * @param mixed $index Cart line index
 * @param mixed $detail_id Persisted supp_invoice_items id
 * @param array|null $plan Credit plan
 * @return int Revision id, or 0 when the line is legacy
 */
function purchasing_credit_record_line($line, $credit_no, $index, $detail_id, $plan)
{
	if (!is_array($plan) || !isset($plan['lines'][$index]))
		return 0;
	$document = dimension_source_revision_document_key($credit_no);
	$line_key = dimension_source_revision_line_key('line', $detail_id);
	if ($document === false || $line_key === false)
		throw new DimensionFinancialReferenceWriteException(_('The saved supplier credit line identity is invalid.'));
	$line_plan = $plan['lines'][$index];
	$quantity = abs((float)$line->this_quantity_inv);
	$source = purchasing_credit_source_line_facts($line_plan['parent']);
	$resolution = array('contract' => 'purchasing-credit-classification/1', 'trans_type' => ST_SUPPCREDIT,
		'role' => 'line', 'flow' => $plan['flow'], 'enabled_axes' => (int)$plan['enabled_axes'],
		'policy_version' => (int)$line_plan['policy_version'],
		'axes' => array(1 => $line_plan['axes'][1], 2 => $line_plan['axes'][2]),
		'stock_id' => (string)$line->item_code, 'source' => $line_plan['source'],
		'inverse' => array('basis' => 'source_bill_line_pair', 'default_recalculation' => false),
		'source_line' => $source,
		'credited' => array('grn_item_id' => (int)$line->id, 'po_detail_item' => (int)$line->po_detail_item,
			'quantity' => purchasing_receipt_decimal_text($quantity),
			'unit_price' => purchasing_receipt_decimal_text($line->chg_price),
			'price_differs_from_source' => isset($source['unit_price'])
				&& $source['unit_price'] !== purchasing_receipt_decimal_text($line->chg_price),
			'quantity_exceeds_source' => isset($source['quantity'])
				&& $quantity > (float)$source['quantity'] + 0.00000001),
		'posting_roles' => purchasing_credit_posting_roles());
	$override_reason = '';
	if ($line_plan['override'] !== null) {
		$resolution['override'] = array('axes' => $line_plan['override']['axes'],
			'source_values' => $line_plan['override']['source_values']);
		$override_reason = $line_plan['override']['reason'];
	}
	$stored = purchasing_receipt_persist_revision(ST_SUPPCREDIT, $document, 'line', $line_key, $resolution,
		$plan['effective_date'], purchasing_receipt_decimal_text($quantity), $override_reason);
	return (int)$stored['revision_id'];
}

/**
 * Record the explicit pair of one direct (GL account) credit line.
 *
 * @param object $gl_line Credit GL line
 * @param mixed $credit_no Final credit number
 * @param mixed $index gl_codes index
 * @param mixed $detail_id Persisted supp_invoice_items id
 * @param array|null $plan Credit plan
 * @return int Revision id, or 0 when the credit is legacy
 */
function purchasing_credit_record_gl_line($gl_line, $credit_no, $index, $detail_id, $plan)
{
	if (!is_array($plan) || !isset($plan['gl_lines'][$index]))
		return 0;
	$document = dimension_source_revision_document_key($credit_no);
	$line_key = dimension_source_revision_line_key('line', $detail_id);
	if ($document === false || $line_key === false)
		throw new DimensionFinancialReferenceWriteException(_('The saved supplier credit line identity is invalid.'));
	$gl_plan = $plan['gl_lines'][$index];
	$resolution = array('contract' => 'purchasing-credit-classification/1', 'trans_type' => ST_SUPPCREDIT,
		'role' => 'gl_line', 'flow' => $plan['flow'], 'enabled_axes' => (int)$plan['enabled_axes'],
		'policy_version' => (int)$gl_plan['policy_version'],
		'axes' => array(1 => $gl_plan['axes'][1], 2 => $gl_plan['axes'][2]),
		'gl_code' => (string)$gl_line->gl_code, 'source' => array('state' => 'explicit_direct_gl'),
		'source_documents' => $plan['source_documents'],
		'inverse' => array('basis' => 'explicit_line_pair', 'default_recalculation' => false),
		'posting_roles' => array('direct_gl_expense_reversal' => 'explicit_line'));
	$stored = purchasing_receipt_persist_revision(ST_SUPPCREDIT, $document, 'line', $line_key, $resolution,
		$plan['effective_date'], '0.00000000', '');
	return (int)$stored['revision_id'];
}

/**
 * Resolve the classification of one supplier payment before any row is written.
 *
 * The payment page's selectors are the payment's own settlement classification; a blank selector is an
 * explicit unassigned (or policy not-applicable) state. The pair goes through the DIM-FND-004 new-event
 * contract so a closed, disabled or wrong-axis identity is refused before the first native row.
 *
 * @param mixed $dimension_id Selected category 1
 * @param mixed $dimension2_id Selected category 2
 * @param string $date_ User-format payment date
 * @return array|null Plan, or null when the company has no companion revision schema
 */
function purchasing_payment_plan($dimension_id, $dimension2_id, $date_)
{
	if (!purchasing_receipt_schema_ready())
		return null;
	$effective = dimension_source_revision_date(date2sql($date_));
	if ($effective === false)
		throw new DimensionFinancialReferenceWriteException(_('The supplier payment date is invalid.'));
	$enabled = (int)get_company_pref('use_dimension');
	$sources = array(1 => array(), 2 => array());
	foreach (array(1 => $dimension_id, 2 => $dimension2_id) as $axis => $raw) {
		$native = dimension_source_revision_int($raw === null || $raw === '' ? 0 : $raw, 0);
		if ($native === false)
			throw new DimensionFinancialReferenceWriteException(
				_('The payment classification identity is invalid. Nothing was saved.'));
		if ($axis > $enabled)
			$native = 0;
		$sources[$axis]['header_explicit'] = $native > 0
			? dimension_resolution_selected($native, 'document_header')
			: dimension_resolution_inherit('payment_header');
	}
	$pair = dimension_resolution_pair($sources[1], $sources[2], array('enabled_axes' => $enabled));
	if (!$pair['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $pair['errors']));
	$pair = purchasing_receipt_finalize_pair($pair, $enabled);
	return array('contract' => 'purchasing-payment-plan/1', 'effective_date' => $effective,
		'enabled_axes' => $enabled, 'axes' => $pair['axes'], 'policy_version' => (int)$pair['policy_version']);
}

/**
 * Record the settlement revision of one written supplier payment.
 *
 * @param mixed $payment_no Final native number
 * @param array|null $plan Payment plan
 * @param array $amounts amount, discount, charge and bank amount in their own currencies
 * @return int Revision id, or 0 when nothing is recorded
 */
function purchasing_payment_record($payment_no, $plan, $amounts)
{
	if (!is_array($plan))
		return 0;
	$document = dimension_source_revision_document_key($payment_no);
	if ($document === false)
		throw new DimensionFinancialReferenceWriteException(_('The saved supplier payment identity is invalid.'));
	$value = function($name) use ($amounts) {
		return purchasing_receipt_decimal_text(isset($amounts[$name]) ? $amounts[$name] : 0);
	};
	$resolution = array('contract' => 'purchasing-payment-classification/1', 'trans_type' => ST_SUPPAYMENT,
		'role' => 'header', 'flow' => 'payment', 'enabled_axes' => (int)$plan['enabled_axes'],
		'policy_version' => (int)$plan['policy_version'], 'axes' => array(1 => $plan['axes'][1], 2 => $plan['axes'][2]),
		'recognition' => 'cash_settlement', 'cost_effect' => 'none',
		'posting_roles' => array('payable' => 'settlement', 'bank' => 'settlement',
			'payment_discount' => 'settlement', 'bank_charge' => 'settlement', 'exchange_variance' => 'settlement'),
		'allocation_policy' => 'settlement_links_only; no re-attribution of the payment or the settled bill; '
			.'cross-classification allocation is unsupported here',
		'amounts' => array('amount' => $value('amount'), 'discount' => $value('discount'),
			'charge' => $value('charge'), 'bank_amount' => $value('bank_amount')));
	$stored = purchasing_receipt_persist_revision(ST_SUPPAYMENT, $document, 'header', 'header', $resolution,
		$plan['effective_date'], '0.00000000', '');
	return (int)$stored['revision_id'];
}

/**
 * Read the recorded classification of a stored bill or credit for reload and display.
 *
 * Only revisions that verify are returned; a document with no revision (legacy) reports `legacy`.
 *
 * @param int $trans_type ST_SUPPINVOICE or ST_SUPPCREDIT
 * @param mixed $trans_no Native document number
 * @return array state (legacy, snapshot, unavailable), header pair/revision id and up to 200 lines
 */
function purchasing_credit_read_source($trans_type, $trans_no)
{
	$legacy = array('state' => 'legacy', 'header' => null, 'lines' => array());
	$type = dimension_source_revision_int($trans_type, 0);
	$document = dimension_source_revision_document_key($trans_no);
	if (!purchasing_receipt_schema_ready() || $document === false
		|| !in_array($type, array(ST_SUPPINVOICE, ST_SUPPCREDIT), true))
		return $legacy;
	$header = dimension_source_revision_snapshot('purchasing', $type, $document, 'header', 'header');
	if ($header['state'] === 'unavailable')
		return array('state' => 'unavailable', 'header' => null, 'lines' => array());
	if ($header['state'] !== 'available')
		return $legacy;
	$result = array('state' => 'snapshot', 'lines' => array(), 'header' => array(
		'pair' => array(1 => (int)$header['revision']['axes'][1]['value'], 2 => (int)$header['revision']['axes'][2]['value']),
		'revision_id' => (int)$header['revision']['source_revision_id']));
	$rows = db_query('SELECT id FROM '.TB_PREF.'supp_invoice_items WHERE supp_trans_type='.$type
		.' AND supp_trans_no='.(int)$trans_no.' ORDER BY id LIMIT 200');
	if (!$rows)
		return array('state' => 'unavailable', 'header' => null, 'lines' => array());
	while ($row = db_fetch_assoc($rows)) {
		$line_key = dimension_source_revision_line_key('line', $row['id']);
		if ($line_key === false)
			continue;
		$line = dimension_source_revision_snapshot('purchasing', $type, $document, 'line', $line_key);
		if ($line['state'] === 'available')
			$result['lines'][$line_key] = array('pair' => array(1 => (int)$line['revision']['axes'][1]['value'],
				2 => (int)$line['revision']['axes'][2]['value']),
				'revision_id' => (int)$line['revision']['source_revision_id']);
	}
	return $result;
}

/**
 * Reload the recorded header classification into a bill/credit cart read from the database.
 *
 * The cart header otherwise falls back to the supplier master, which cannot prove the classification the
 * document was posted with. A legacy document is left unchanged.
 *
 * @param object $supp_trans Cart that was just read (dimension and dimension2 are replaced)
 * @return bool True when the recorded pair was applied
 */
function purchasing_credit_reload_classification(&$supp_trans)
{
	if (!is_object($supp_trans) || empty($supp_trans->trans_no) || !isset($supp_trans->trans_type))
		return false;
	$source = purchasing_credit_read_source($supp_trans->trans_type, $supp_trans->trans_no);
	if ($source['state'] !== 'snapshot')
		return false;
	$enabled = (int)get_company_pref('use_dimension');
	$supp_trans->dimension = $enabled >= 1 ? $source['header']['pair'][1] : 0;
	$supp_trans->dimension2 = $enabled >= 2 ? $source['header']['pair'][2] : 0;
	$supp_trans->classification_reload = array('trans_type' => (int)$supp_trans->trans_type,
		'trans_no' => (int)$supp_trans->trans_no, 'revision_id' => (int)$source['header']['revision_id']);
	return true;
}
