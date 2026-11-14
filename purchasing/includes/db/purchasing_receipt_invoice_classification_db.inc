<?php
/**
 * DIM-PUR-003: goods-receipt (GRN), supplier-invoice and matching classification lineage.
 *
 * A goods receipt and a supplier bill are posted from a purchase order whose classification was
 * already snapshotted by DIM-PUR-002. Before this adapter both writers recomputed intent from the
 * current item/supplier master defaults (the characterized E10 behavior), so a receipt or a price
 * adjustment could post a classification different from the order that authorized it. This adapter
 * makes the writers consume the durable snapshot instead:
 *
 *  - every posted line resolves through the shared DIM-FND-005 order from its persisted parent line
 *    (purchase-order line for a receipt, goods-receipt line for a bill, falling back to the order
 *    line behind it for a receipt that pre-dates this lineage). The parent snapshot outranks the
 *    document header and any current default, so partial receipts, partial bills, price variances
 *    and returned quantities of one order line carry one immutable pair and the GRN clearing
 *    credit equals the invoice clearing debit by source, account and category;
 *  - an order line with no snapshot at all (a legacy order or receipt) keeps the characterized E10
 *    behavior untouched and nothing is reconstructed from a current default;
 *  - the item default is deliberately not a source: receiving or billing a line is never a new
 *    classification event. A changed document header or a line override carrier that names its
 *    reason is a recorded override that keeps the source pair it replaced;
 *  - direct (GL-account) bill lines are explicit by construction: the pair entered on the line is
 *    recorded as that line's evidence and is never replaced by a header or default;
 *  - every posted receipt/bill header and line stores one append-only DIM-FND-006 revision in the
 *    `purchasing` family that names its parent source revision, the flow (standard, direct, GL-only)
 *    and the distinct posting roles (capitalized stock or expense, GRN clearing, price-variance
 *    provision, payable, tax). Tax is a distinct header-attributed row and is never part of an
 *    expense or capitalized amount;
 *  - capital/stock/expense timing stays explicit: a receipt posts the inventory/clearing legs only
 *    when a GRN clearing account is configured, otherwise the bill capitalizes or expenses the line.
 *
 * The adapter contains no ledger or native-table write of its own: every posting still passes the
 * maintained writers and DIM-FND-007 admission. Supplier credit notes and payments stay with
 * DIM-PUR-004. PHP 5.6 compatible.
 */
require_once __DIR__.'/purchasing_classification_db.inc';

// Focused CLI harnesses evaluate the writers without the page bootstrap. The canonical values are
// admitted here without ever replacing an existing definition.
foreach (array('ST_SUPPINVOICE' => 20, 'ST_SUPPRECEIVE' => 25, 'ST_PURCHORDER' => 18)
	as $purchasing_receipt_type_name => $purchasing_receipt_type_value)
	if (!defined($purchasing_receipt_type_name))
		define($purchasing_receipt_type_name, $purchasing_receipt_type_value);

/**
 * Transaction types owned by this adapter.
 *
 * @return int[]
 */
function purchasing_receipt_supported_types()
{
	return array(ST_SUPPRECEIVE, ST_SUPPINVOICE);
}

/**
 * Is one type a supported receipt/bill source?
 *
 * @param mixed $trans_type Stored type
 * @return bool
 */
function purchasing_receipt_type_supported($trans_type)
{
	$type = dimension_source_revision_int($trans_type, 0);
	return $type !== false && in_array($type, purchasing_receipt_supported_types(), true);
}

/**
 * Is the accepted companion revision schema installed for the current company?
 *
 * A company without it posts exactly as before and records no synthetic intent.
 *
 * @return bool
 */
function purchasing_receipt_schema_ready()
{
	return dimension_source_revision_schema_ready() === true;
}

/**
 * Canonical eight-decimal text for sealed evidence (never a float).
 *
 * @param mixed $amount Quantity or amount
 * @return string
 */
function purchasing_receipt_decimal_text($amount)
{
	return number_format((float)$amount, 8, '.', '');
}

/**
 * Read one stored scalar owned by the active company.
 *
 * @param string $sql Single-value query
 * @return mixed|null Value, or null when no row exists
 */
function purchasing_receipt_scalar($sql)
{
	$rows = db_query($sql);
	if (!$rows)
		throw new DimensionFinancialReferenceWriteException(
			_('The purchasing source of this document cannot be read. Nothing was saved.'));
	$row = db_fetch_row($rows);
	return $row ? $row[0] : null;
}

/**
 * Describe the purchase-order line that one receipt line was received against.
 *
 * @param mixed $po_detail_item Persisted purch_order_details identity
 * @return array|false trans_type, document_key and line_key; false when the line has no order parent
 */
function purchasing_receipt_order_line_parent($po_detail_item)
{
	$id = dimension_source_revision_int($po_detail_item, 1);
	if ($id === false)
		return false;
	$order_no = purchasing_receipt_scalar('SELECT order_no FROM '.TB_PREF.'purch_order_details WHERE po_detail_item='.$id);
	if ($order_no === null)
		return false;
	$document = dimension_source_revision_document_key($order_no);
	$line_key = dimension_source_revision_line_key('line', $id);
	if ($document === false || $line_key === false)
		return false;
	return array('trans_type' => ST_PURCHORDER, 'document_key' => $document, 'line_key' => $line_key);
}

/**
 * Describe the goods-receipt line that one bill line invoices.
 *
 * @param mixed $grn_item_id Persisted grn_items identity
 * @return array|false trans_type, document_key and line_key; false when the line has no receipt parent
 */
function purchasing_receipt_grn_line_parent($grn_item_id)
{
	$id = dimension_source_revision_int($grn_item_id, 1);
	if ($id === false)
		return false;
	$batch = purchasing_receipt_scalar('SELECT grn_batch_id FROM '.TB_PREF.'grn_items WHERE id='.$id);
	if ($batch === null)
		return false;
	$document = dimension_source_revision_document_key($batch);
	$line_key = dimension_source_revision_line_key('line', $id);
	if ($document === false || $line_key === false)
		return false;
	return array('trans_type' => ST_SUPPRECEIVE, 'document_key' => $document, 'line_key' => $line_key);
}

/**
 * Read one parent snapshot and fail closed when it cannot be verified.
 *
 * @param int $trans_type Parent type
 * @param string $document_key Parent document key
 * @param string $line_kind header or line
 * @param string $line_key Stable line key
 * @return array Snapshot-shaped result; state is available, absent or unavailable
 */
function purchasing_receipt_read_snapshot($trans_type, $document_key, $line_kind, $line_key)
{
	$snapshot = dimension_source_revision_snapshot('purchasing', $trans_type, $document_key, $line_kind,
		$line_key);
	if (!in_array($snapshot['state'], array('available', 'absent', 'unavailable'), true))
		throw new DimensionFinancialReferenceWriteException(
			_('The source classification of this purchasing line cannot be verified. Nothing was saved.'));
	return $snapshot;
}

/**
 * Locate and read the snapshot of one line's parent chain.
 *
 * A bill line prefers its goods-receipt line; a receipt that pre-dates this lineage has no revision,
 * so the order line behind it supplies the snapshot instead.
 *
 * @param array|false $parents Ordered parent descriptors, nearest first
 * @return array parent (used descriptor or false), snapshot and the chain read
 */
function purchasing_receipt_read_chain($parents)
{
	$chain = array();
	foreach ($parents as $parent) {
		if ($parent === false)
			continue;
		$snapshot = purchasing_receipt_read_snapshot($parent['trans_type'], $parent['document_key'], 'line',
			$parent['line_key']);
		$chain[] = array('parent' => $parent, 'snapshot' => $snapshot);
		if ($snapshot['state'] === 'available')
			return array('parent' => $parent, 'snapshot' => $snapshot, 'chain' => $chain);
	}
	$first = count($chain) ? $chain[0] : array('parent' => false, 'snapshot' => array('state' => 'none'));
	return array('parent' => $first['parent'], 'snapshot' => $first['snapshot'], 'chain' => $chain);
}

/**
 * Describe one parent read for sealed evidence.
 *
 * @param array|false $parent Parent descriptor actually used
 * @param array $snapshot Snapshot-shaped read
 * @param int $chain_length Number of ancestors that were read
 * @return array
 */
function purchasing_receipt_source_evidence($parent, $snapshot, $chain_length)
{
	if ($parent === false)
		return array('state' => 'none');
	$evidence = array('state' => (string)$snapshot['state'], 'trans_type' => (int)$parent['trans_type'],
		'document_key' => (string)$parent['document_key'], 'line_key' => (string)$parent['line_key'],
		'chain' => (int)$chain_length);
	if (isset($snapshot['revision']['source_revision_id']))
		$evidence['revision_id'] = (int)$snapshot['revision']['source_revision_id'];
	return $evidence;
}

/**
 * Settle the parent header snapshot for a document built from one or more parents.
 *
 * Several receipts on one bill can disagree; the header is then not attributable to a single
 * parent and the document header selection stands, recorded as `diverged`.
 *
 * @param array $parents Distinct parent descriptors keyed by `type:number`
 * @return array state (none, absent, available, diverged), snapshot and evidence
 */
function purchasing_receipt_parent_header($parents)
{
	$result = array('state' => 'none', 'snapshot' => null, 'evidence' => array('state' => 'none'));
	if (!count($parents))
		return $result;
	$first = null;
	$evidence = array();
	$state = 'available';
	foreach ($parents as $parent) {
		$snapshot = purchasing_receipt_read_snapshot($parent['trans_type'], $parent['document_key'],
			'header', 'header');
		if ($snapshot['state'] !== 'available') {
			$state = 'absent';
		} elseif ($first === null) {
			$first = $snapshot;
		} else {
			foreach (array(1, 2) as $axis) {
				$left = $first['revision']['axes'][$axis];
				$right = $snapshot['revision']['axes'][$axis];
				if ((int)$left['value'] !== (int)$right['value'] || $left['mode'] !== $right['mode'])
					$state = $state === 'absent' ? 'absent' : 'diverged';
			}
		}
		if (count($evidence) < 20)
			$evidence[] = array('trans_type' => (int)$parent['trans_type'],
				'document_key' => (string)$parent['document_key'], 'state' => (string)$snapshot['state'],
				'revision_id' => isset($snapshot['revision']['source_revision_id'])
					? (int)$snapshot['revision']['source_revision_id'] : 0);
	}
	$result['state'] = $state;
	$result['snapshot'] = $state === 'available' ? $first : null;
	$result['evidence'] = array('state' => $state, 'parents' => $evidence);
	return $result;
}

/**
 * Finish a resolved pair: an axis nobody decided is an explicit unassigned (or policy N/A) state.
 *
 * @param array $pair dimension_resolution_pair() result
 * @param int $enabled Enabled category count
 * @return array Pair with every axis final
 */
function purchasing_receipt_finalize_pair($pair, $enabled)
{
	$policy = function_exists('dimension_category_policy_current')
		? dimension_category_policy_current() : array('version' => 0);
	$policy_version = isset($policy['version']) ? (int)$policy['version'] : 0;
	foreach (array(1, 2) as $axis) {
		if ($pair['axes'][$axis]['mode'] !== 'inherit')
			continue;
		if ($axis > $enabled && $policy_version > 0) {
			$pair['axes'][$axis] = array_merge($pair['axes'][$axis], array(
				'value' => 0, 'mode' => 'not_applicable', 'origin' => 'policy', 'persisted' => false,
				'policy_version' => $policy_version, 'basis' => 'axis_not_enabled'));
		} else {
			$pair['axes'][$axis] = array_merge($pair['axes'][$axis], array(
				'value' => 0, 'mode' => 'unassigned',
				'origin' => $axis > $enabled ? 'axis_disabled' : 'purchasing_unassigned',
				'persisted' => false, 'policy_version' => 0, 'basis' => ''));
		}
	}
	return $pair;
}

/**
 * Read and validate one line's explicit override carrier.
 *
 * The carrier is `array('reason' => text, 'intents' => axis => intent)`. An override with no reason
 * is refused: an unexplained re-classification of a received source is exactly what this section
 * prevents.
 *
 * @param object $line Receipt/bill line
 * @return array|null reason and intents, or null when the line carries no override
 */
function purchasing_receipt_line_override($line)
{
	if (!is_object($line) || !isset($line->classification_override)
		|| !is_array($line->classification_override))
		return null;
	$carrier = $line->classification_override;
	$reason = isset($carrier['reason']) && is_string($carrier['reason']) ? trim($carrier['reason']) : '';
	if ($reason === '' || strlen($reason) > 255 || preg_match('/[\x00-\x1f\x7f]/', $reason))
		throw new DimensionFinancialReferenceWriteException(
			_('A purchasing classification override needs a clear reason. Nothing was saved.'));
	$intents = array();
	foreach (array(1, 2) as $axis) {
		$source = isset($carrier['intents'][$axis]) ? $carrier['intents'][$axis] : null;
		if ($source === null)
			continue;
		$normalized = dimension_resolution_normalize_source($source, 'line_explicit');
		if (!$normalized['ok'])
			throw new DimensionFinancialReferenceWriteException(implode(' ', $normalized['errors']));
		if ($normalized['intent']['mode'] !== 'inherit')
			$intents[$axis] = $normalized['intent'];
	}
	if (!count($intents))
		return null;
	return array('reason' => $reason, 'intents' => $intents);
}

/**
 * Name the commercial flow of one receipt/bill.
 *
 * @param int $trans_type Document type
 * @param array $parents Distinct header parent descriptors
 * @param int $line_count Parent-bound line count
 * @return string
 */
function purchasing_receipt_flow($trans_type, $parents, $line_count)
{
	if (!$line_count)
		return 'gl_only';
	foreach ($parents as $parent) {
		$table = (int)$parent['trans_type'] === ST_SUPPRECEIVE ? 'grn_batch WHERE id=' : 'purch_orders WHERE order_no=';
		$key = dimension_source_revision_int($parent['document_key'], 1);
		if ($key !== false && purchasing_receipt_scalar('SELECT reference FROM '.TB_PREF.$table.$key) === 'auto')
			return 'direct';
	}
	return 'standard';
}

/**
 * Read the supplier's current default pair as the weak fallback source.
 *
 * @param mixed $supplier_id Supplier identity
 * @return array axis => configured identity
 */
function purchasing_receipt_supplier_defaults($supplier_id)
{
	// Read fresh: the PUR-002 reader caches per request and a supplier default that changed after
	// the order was created is exactly the drift this adapter must observe correctly.
	$id = dimension_source_revision_int($supplier_id, 0);
	if ($id === false || $id < 1)
		return array(1 => 0, 2 => 0);
	$rows = db_query('SELECT dimension_id,dimension2_id FROM '.TB_PREF.'suppliers WHERE supplier_id='.$id);
	if (!$rows)
		throw new DimensionFinancialReferenceWriteException(
			_('The purchasing supplier classification defaults cannot be read.'));
	$row = db_fetch_assoc($rows);
	return $row ? array(1 => (int)$row['dimension_id'], 2 => (int)$row['dimension2_id'])
		: array(1 => 0, 2 => 0);
}

/**
 * Resolve the explicit direct-bill pair of one GL expense line.
 *
 * @param object $gl_line Bill GL line (gl_dim and gl_dim2 are the pair the user entered)
 * @param int $enabled Enabled category count
 * @return array Resolved pair with every axis final
 */
function purchasing_receipt_resolve_gl_line($gl_line, $enabled)
{
	$sources = array(1 => array(), 2 => array());
	foreach (array(1, 2) as $axis) {
		$field = $axis === 1 ? 'gl_dim' : 'gl_dim2';
		$value = dimension_source_revision_int(isset($gl_line->$field) ? $gl_line->$field : 0, 0);
		if ($value === false)
			throw new DimensionFinancialReferenceWriteException(
				_('A bill line classification identity is invalid. Nothing was saved.'));
		if ($axis > $enabled)
			continue;
		$sources[$axis]['line_explicit'] = $value > 0
			? dimension_resolution_selected($value, 'direct_gl_line')
			: dimension_resolution_unassigned('direct_gl_line');
	}
	$pair = dimension_resolution_pair($sources[1], $sources[2], array('enabled_axes' => $enabled));
	if (!$pair['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $pair['errors']));
	return purchasing_receipt_finalize_pair($pair, $enabled);
}

/**
 * Resolve the classification of one receipt or bill from its parents, without any approval check.
 *
 * The optional DIM-PUR-004 hooks let a supplier credit reuse this resolver without changing the
 * receipt/bill behavior: `types` (accepted types), `parents` (callable returning the ordered parent
 * descriptors of one bill-shaped line), `flow` (callable naming the flow) and `strict_lines` (a line
 * with no snapshot of its own is legacy and never borrows the header snapshot).
 *
 * @param object $doc Order cart being received (ST_SUPPRECEIVE) or supp_trans being billed
 * @param int $trans_type ST_SUPPRECEIVE or ST_SUPPINVOICE
 * @param array $hooks Optional resolver hooks
 * @return array|null Plan, or null when the document keeps the legacy behavior
 */
function purchasing_receipt_resolve_plan($doc, $trans_type, $hooks = array())
{
	$accepted_types = isset($hooks['types']) && is_array($hooks['types'])
		? $hooks['types'] : purchasing_receipt_supported_types();
	if (!is_object($doc) || !in_array(dimension_source_revision_int($trans_type, 0), $accepted_types, true)
		|| !purchasing_receipt_schema_ready())
		return null;
	$is_receipt = (int)$trans_type === ST_SUPPRECEIVE;
	if ($is_receipt) {
		if (!isset($doc->line_items) || !is_array($doc->line_items))
			throw new DimensionFinancialReferenceWriteException(
				_('The purchasing classification lines are unavailable.'));
		$source_lines = $doc->line_items;
		$date_text = isset($doc->orig_order_date) ? $doc->orig_order_date : '';
	} else {
		if (!isset($doc->grn_items) || !is_array($doc->grn_items))
			throw new DimensionFinancialReferenceWriteException(
				_('The purchasing classification lines are unavailable.'));
		$source_lines = $doc->grn_items;
		$date_text = isset($doc->tran_date) ? $doc->tran_date : '';
	}
	$effective = dimension_source_revision_date(date2sql($date_text));
	if ($effective === false)
		throw new DimensionFinancialReferenceWriteException(_('The purchasing source date is invalid.'));
	$enabled = (int)get_company_pref('use_dimension');
	$supplier = purchasing_receipt_supplier_defaults(isset($doc->supplier_id) ? $doc->supplier_id : 0);

	$entries = array();
	$parents = array();
	foreach ($source_lines as $index => $line) {
		if (!is_object($line))
			throw new DimensionFinancialReferenceWriteException(_('A purchasing classification line is invalid.'));
		if ($is_receipt) {
			if (!isset($line->receive_qty) || $line->receive_qty == 0 || $line->receive_qty === '')
				continue;
			$chain = purchasing_receipt_read_chain(array(
				purchasing_receipt_order_line_parent(isset($line->po_detail_rec) ? $line->po_detail_rec : 0)));
		} else {
			if (!isset($line->this_quantity_inv) || (float)$line->this_quantity_inv == 0)
				continue;
			$chain = purchasing_receipt_read_chain(isset($hooks['parents'])
				? call_user_func($hooks['parents'], $line, $doc)
				: array(
					purchasing_receipt_grn_line_parent(isset($line->id) ? $line->id : 0),
					purchasing_receipt_order_line_parent(isset($line->po_detail_item) ? $line->po_detail_item : 0)));
		}
		if ($chain['parent'] !== false) {
			$header_type = (int)$chain['parent']['trans_type'];
			$parents[$header_type.':'.$chain['parent']['document_key']] = array('trans_type' => $header_type,
				'document_key' => $chain['parent']['document_key']);
		}
		$entries[$index] = $chain;
	}

	// GL (direct expense) lines are explicit by construction and never need a parent.
	$gl_plans = array();
	if (!$is_receipt && isset($doc->gl_codes) && is_array($doc->gl_codes))
		foreach ($doc->gl_codes as $gl_index => $gl_line) {
			if (!is_object($gl_line))
				throw new DimensionFinancialReferenceWriteException(_('A bill line is invalid.'));
			$pair = purchasing_receipt_resolve_gl_line($gl_line, $enabled);
			$gl_plans[$gl_index] = array('axes' => $pair['axes'], 'policy_version' => (int)$pair['policy_version']);
		}

	$header_source = purchasing_receipt_parent_header($parents);
	// An invoice header the user did not choose is the supplier-master prefill of the page. It cannot
	// override a recorded source: default drift is precisely what this lineage protects against.
	$header_sources = array(1 => array(), 2 => array());
	$header_override = array(1 => false, 2 => false);
	$usable_header = array(1 => array(), 2 => array());
	foreach (array(1, 2) as $axis) {
		$field = $axis === 1 ? 'dimension' : 'dimension2';
		$native = $is_receipt ? 0 : dimension_source_revision_int(isset($doc->$field) ? $doc->$field : 0, 0);
		if ($native === false)
			throw new DimensionFinancialReferenceWriteException(
				_('The purchasing header classification identity is invalid.'));
		if ($axis > $enabled)
			$native = 0;
		$carried = $is_receipt ? null : purchasing_classification_carried_intent($doc, $axis, 'document_header');
		$explicit_header = is_array($carried) && !empty($carried['ok']) && isset($carried['intent']['mode'])
			&& $carried['intent']['mode'] !== 'inherit' && empty($carried['intent']['persisted'])
			? $carried['intent'] : null;
		$snapshot_source = array();
		$snapshot_value = null;
		if ($header_source['state'] === 'available' && $axis <= $enabled) {
			$row = $header_source['snapshot']['revision']['axes'][$axis];
			if ($row['mode'] !== 'not_applicable') {
				$snapshot_source = purchasing_classification_snapshot_axis_source($header_source['snapshot'], $axis);
				$snapshot_value = (int)$row['value'];
			}
		}
		$chosen_native = $native > 0 && $native !== (int)$supplier[$axis] ? $native : 0;
		if (count($snapshot_source)) {
			$usable_header[$axis] = $snapshot_source;
			if ($explicit_header !== null || ($chosen_native > 0 && $chosen_native !== $snapshot_value)) {
				$header_override[$axis] = true;
				$header_sources[$axis]['header_explicit'] = $explicit_header !== null
					? $carried : dimension_resolution_selected($chosen_native, 'document_header');
			} else {
				$header_sources[$axis]['header_explicit'] = dimension_resolution_inherit('purchasing_header');
			}
			$header_sources[$axis]['header_snapshot'] = $snapshot_source;
		} elseif ($explicit_header !== null) {
			$header_sources[$axis]['header_explicit'] = $carried;
		} else {
			$header_sources[$axis]['header_explicit'] = $native > 0
				? dimension_resolution_selected($native, 'document_header')
				: dimension_resolution_inherit('purchasing_header');
		}
		$header_sources[$axis]['partner_default'] = dimension_resolution_default_intent(
			$supplier[$axis], 'partner_default');
	}

	$line_plans = array();
	$legacy_lines = array();
	$any_override = false;
	foreach ($entries as $index => $entry) {
		$line = $source_lines[$index];
		$override = purchasing_receipt_line_override($line);
		$has_line_snapshot = $entry['snapshot']['state'] === 'available';
		if (!$has_line_snapshot && $override === null
			&& (!empty($hooks['strict_lines']) || (!count($usable_header[1]) && !count($usable_header[2])))) {
			$legacy_lines[] = $index;
			continue;
		}
		$sources = array(1 => array(), 2 => array());
		foreach (array(1, 2) as $axis) {
			if ($override !== null && isset($override['intents'][$axis]))
				$sources[$axis]['line_explicit'] = $override['intents'][$axis];
			if ($has_line_snapshot && $axis <= $enabled
				&& $entry['snapshot']['revision']['axes'][$axis]['mode'] !== 'not_applicable')
				$sources[$axis]['line_snapshot'] = purchasing_classification_snapshot_axis_source(
					$entry['snapshot'], $axis);
			$sources[$axis]['header_explicit'] = $header_override[$axis]
				? $header_sources[$axis]['header_explicit'] : dimension_resolution_inherit('purchasing_header');
			if (count($usable_header[$axis]))
				$sources[$axis]['header_snapshot'] = $usable_header[$axis];
			$sources[$axis]['partner_default'] = dimension_resolution_default_intent(
				$supplier[$axis], 'partner_default');
		}
		$pair = dimension_resolution_pair($sources[1], $sources[2], array('enabled_axes' => $enabled));
		if (!$pair['ok'])
			throw new DimensionFinancialReferenceWriteException(implode(' ', $pair['errors']));
		$pair = purchasing_receipt_finalize_pair($pair, $enabled);
		$line_plan = array('axes' => $pair['axes'], 'policy_version' => (int)$pair['policy_version'],
			'parent' => $entry['parent'], 'source' => purchasing_receipt_source_evidence($entry['parent'],
				$entry['snapshot'], count($entry['chain'])), 'override' => null);
		if ($override !== null) {
			$source_pair = array();
			foreach (array(1, 2) as $axis)
				$source_pair[$axis] = $has_line_snapshot
					? (int)$entry['snapshot']['revision']['axes'][$axis]['value'] : null;
			$line_plan['override'] = array('reason' => $override['reason'],
				'axes' => array_keys($override['intents']), 'source_values' => $source_pair);
			$any_override = true;
		}
		$line_plans[$index] = $line_plan;
	}
	if (!count($line_plans) && !count($gl_plans) && $header_source['state'] !== 'available')
		return null;

	$header_pair = dimension_resolution_pair($header_sources[1], $header_sources[2],
		array('enabled_axes' => $enabled));
	if (!$header_pair['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $header_pair['errors']));
	$header_pair = purchasing_receipt_finalize_pair($header_pair, $enabled);
	return array('contract' => 'purchasing-receipt-invoice-plan/1', 'trans_type' => (int)$trans_type,
		'flow' => isset($hooks['flow']) ? call_user_func($hooks['flow'], $doc, $trans_type, $parents, count($entries))
			: purchasing_receipt_flow($trans_type, $parents, count($entries)), 'effective_date' => $effective,
		'enabled_axes' => $enabled, 'parents' => array_values($parents),
		'header' => array('axes' => $header_pair['axes'], 'policy_version' => (int)$header_pair['policy_version'],
			'override' => $header_override, 'source' => $header_source['evidence']),
		'lines' => $line_plans, 'gl_lines' => $gl_plans, 'legacy_lines' => $legacy_lines,
		'has_override' => $any_override || $header_override[1] || $header_override[2]);
}

/**
 * Bounded, deterministic summary of a plan that an approver reviews and that execution must reproduce.
 *
 * @param array|null $plan Plan, or null for a legacy document
 * @return array version, state and the per-line pairs with their parent revision ids
 */
function purchasing_receipt_plan_evidence($plan)
{
	if (!is_array($plan))
		return array('version' => 1, 'state' => 'legacy');
	$lines = array();
	foreach ($plan['lines'] as $index => $line_plan)
		$lines[(string)$index] = array('pair' => array((int)$line_plan['axes'][1]['value'],
			(int)$line_plan['axes'][2]['value']),
			'source_revision_id' => isset($line_plan['source']['revision_id'])
				? (int)$line_plan['source']['revision_id'] : 0,
			'override' => $line_plan['override'] !== null);
	$gl_lines = array();
	foreach ($plan['gl_lines'] as $index => $gl_plan)
		$gl_lines[(string)$index] = array((int)$gl_plan['axes'][1]['value'], (int)$gl_plan['axes'][2]['value']);
	$header = purchasing_receipt_header_pair($plan);
	return array('version' => 1, 'state' => 'snapshot', 'trans_type' => (int)$plan['trans_type'],
		'flow' => (string)$plan['flow'],
		'header' => array((int)$header[1], (int)$header[2],
			!empty($plan['header']['override'][1]), !empty($plan['header']['override'][2])),
		'lines' => $lines, 'gl_lines' => $gl_lines, 'legacy_lines' => array_values($plan['legacy_lines']));
}

/**
 * Evidence to attach to a supplier-bill approval submission, so the reviewed classification is what
 * executes.
 *
 * @param object $doc supp_trans about to be submitted
 * @param int $trans_type Always ST_SUPPINVOICE for the bill approval
 * @return array|null Evidence, or null for a type or company this adapter does not govern
 */
function purchasing_receipt_approval_evidence($doc, $trans_type)
{
	if (!purchasing_receipt_type_supported($trans_type) || !purchasing_receipt_schema_ready())
		return null;
	return purchasing_receipt_plan_evidence(purchasing_receipt_resolve_plan($doc, $trans_type));
}

/**
 * Plan the classification of one receipt/bill before any row is written.
 *
 * A bill restored from an approved draft carries the evidence that was reviewed; the plan resolved
 * at execution must reproduce it exactly, or the document is refused rather than silently posted
 * under a classification nobody approved (for example after the source order was amended).
 *
 * @param object $doc Order cart being received, or supp_trans being billed
 * @param int $trans_type ST_SUPPRECEIVE or ST_SUPPINVOICE
 * @return array|null Plan, or null when the document keeps the legacy behavior
 */
function purchasing_receipt_plan($doc, $trans_type)
{
	$plan = purchasing_receipt_resolve_plan($doc, $trans_type);
	if (is_object($doc) && isset($doc->classification_receipt_invoice) && is_array($doc->classification_receipt_invoice)
		&& purchasing_receipt_type_supported($trans_type) && purchasing_receipt_schema_ready()
		&& json_encode(purchasing_receipt_plan_evidence($plan))
			!== json_encode($doc->classification_receipt_invoice))
		throw new DimensionFinancialReferenceWriteException(
			_('The classification of the source changed after this document was submitted for approval. Nothing was saved; submit it again.'));
	return $plan;
}

/**
 * Header values of a plan.
 *
 * @param array|null $plan Plan
 * @return array|null axis => value
 */
function purchasing_receipt_header_pair($plan)
{
	if (!is_array($plan) || !isset($plan['header']['axes'][1], $plan['header']['axes'][2]))
		return null;
	return array(1 => (int)$plan['header']['axes'][1]['value'], 2 => (int)$plan['header']['axes'][2]['value']);
}

/**
 * Line values of a plan; null keeps the legacy resolver for that line.
 *
 * @param array|null $plan Plan
 * @param mixed $index Cart line index
 * @return array|null axis => value
 */
function purchasing_receipt_line_pair($plan, $index)
{
	if (!is_array($plan) || !isset($plan['lines'][$index]['axes'][1], $plan['lines'][$index]['axes'][2]))
		return null;
	return array(1 => (int)$plan['lines'][$index]['axes'][1]['value'],
		2 => (int)$plan['lines'][$index]['axes'][2]['value']);
}

/**
 * Compare the durable classification-relevant bytes of one stored revision.
 *
 * @param array $current Current verified revision
 * @param array $fields Prepared candidate fields
 * @return bool
 */
function purchasing_receipt_revision_equivalent($current, $fields)
{
	foreach (array('dimension_id', 'dimension2_id', 'mode1', 'mode2', 'origin1', 'origin2',
		'basis1', 'basis2', 'policy1_version', 'policy2_version', 'effective_date',
		'line_quantity', 'override_reason', 'resolution_payload') as $name) {
		$current_value = array_key_exists($name, $current) ? $current[$name] : null;
		if ((string)$current_value !== (string)$fields[$name])
			return false;
	}
	return true;
}

/**
 * Persist one receipt/bill revision, converging on an identical retry and appending on change.
 *
 * @param int $trans_type Receipt or bill type
 * @param string $document_key Final document number
 * @param string $line_kind header or line
 * @param string $line_key Stable line key
 * @param array $resolution Resolved axes and evidence
 * @param string $effective_date ISO date
 * @param string $line_quantity Canonical quantity
 * @param string $override_reason Reason, or an empty string
 * @return array ok, revision_id, created
 */
function purchasing_receipt_persist_revision($trans_type, $document_key, $line_kind, $line_key,
	$resolution, $effective_date, $line_quantity, $override_reason)
{
	$options = array('effective_date' => $effective_date, 'line_quantity' => $line_quantity,
		'resolution' => $resolution);
	if ($override_reason !== '')
		$options['override_reason'] = $override_reason;
	$prepared = dimension_source_revision_prepare('purchasing', $trans_type, $document_key, $line_kind,
		$line_key, $resolution['axes'], $options);
	if (!$prepared['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $prepared['errors']));
	$current = dimension_source_revision_snapshot('purchasing', $trans_type, $document_key, $line_kind,
		$line_key);
	if (!in_array($current['state'], array('available', 'absent', 'unavailable'), true))
		throw new DimensionFinancialReferenceWriteException(
			_('The current purchasing classification revision cannot be verified. Nothing was saved.'));
	if ($current['state'] === 'available') {
		if (purchasing_receipt_revision_equivalent($current['revision'], $prepared['fields']))
			return array('ok' => true, 'revision_id' => (int)$current['revision']['source_revision_id'],
				'created' => false);
		$options['predecessor_revision_id'] = (int)$current['revision']['source_revision_id'];
		$prepared = dimension_source_revision_prepare('purchasing', $trans_type, $document_key, $line_kind,
			$line_key, $resolution['axes'], $options);
		if (!$prepared['ok'])
			throw new DimensionFinancialReferenceWriteException(implode(' ', $prepared['errors']));
	}
	$stored = dimension_source_revision_persist($prepared);
	if (!$stored['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $stored['errors']));
	return array('ok' => true, 'revision_id' => (int)$stored['revision_id'], 'created' => !empty($stored['created']));
}

/**
 * The distinct posting roles a header pair attributes on this document type.
 *
 * Capitalized stock and expense follow each line's immutable pair; the payable and the tax rows are
 * header-attributed and tax is never part of an expense or capitalized amount. With no GRN clearing
 * account configured, a receipt posts no ledger row and the bill carries stock/expense.
 *
 * @param int $trans_type Document type
 * @param string $flow Flow name
 * @return array role => basis
 */
function purchasing_receipt_posting_roles($trans_type, $flow)
{
	if ((int)$trans_type === ST_SUPPRECEIVE)
		return array('inventory_debit' => 'line', 'grn_clearing_credit' => 'line_pair_group',
			'stock_move' => 'no_classification');
	return array('stock_or_expense' => 'line', 'grn_clearing_debit' => 'line',
		'price_variance_provision' => 'line', 'zero_stock_adjustment' => 'line', 'payable' => 'header',
		'tax' => 'header_not_expense', 'direct_gl_expense' => 'explicit_line');
}

/**
 * Record the header revision of one written receipt/bill.
 *
 * @param int $trans_type Document type
 * @param mixed $document_no Final native number
 * @param array|null $plan Plan, or null for a legacy document
 * @return int Revision id, or 0 when nothing is recorded
 */
function purchasing_receipt_record_header($trans_type, $document_no, $plan)
{
	if (!is_array($plan))
		return 0;
	$document = dimension_source_revision_document_key($document_no);
	if ($document === false)
		throw new DimensionFinancialReferenceWriteException(_('The saved purchasing document identity is invalid.'));
	$resolution = array('contract' => 'purchasing-receipt-invoice-classification/1',
		'trans_type' => (int)$trans_type, 'role' => 'header', 'flow' => $plan['flow'],
		'enabled_axes' => (int)$plan['enabled_axes'], 'policy_version' => (int)$plan['header']['policy_version'],
		'axes' => array(1 => $plan['header']['axes'][1], 2 => $plan['header']['axes'][2]),
		'source' => $plan['header']['source'],
		'posting_roles' => purchasing_receipt_posting_roles($trans_type, $plan['flow']),
		'line_count' => count($plan['lines']), 'gl_line_count' => count($plan['gl_lines']),
		'legacy_line_count' => count($plan['legacy_lines']));
	$override_reason = '';
	if ($plan['header']['override'][1] || $plan['header']['override'][2]) {
		$resolution['override'] = array('axes' => array_keys(array_filter($plan['header']['override'])));
		$override_reason = 'Document header differs from the source classification';
	}
	$stored = purchasing_receipt_persist_revision($trans_type, $document, 'header', 'header', $resolution,
		$plan['effective_date'], '0.00000000', $override_reason);
	return (int)$stored['revision_id'];
}

/**
 * Record the line revision of one written receipt/bill line.
 *
 * @param object $line Written receipt (order) or bill (grn_items) line
 * @param int $trans_type Document type
 * @param mixed $document_no Final native number
 * @param mixed $index Cart line index
 * @param mixed $detail_id Persisted grn_items / supp_invoice_items id
 * @param array|null $plan Plan
 * @return int Revision id, or 0 when the line is legacy
 */
function purchasing_receipt_record_line($line, $trans_type, $document_no, $index, $detail_id, $plan)
{
	if (!is_array($plan) || !isset($plan['lines'][$index]))
		return 0;
	$document = dimension_source_revision_document_key($document_no);
	$line_key = dimension_source_revision_line_key('line', $detail_id);
	if ($document === false || $line_key === false)
		throw new DimensionFinancialReferenceWriteException(_('The saved purchasing line identity is invalid.'));
	$line_plan = $plan['lines'][$index];
	$is_receipt = (int)$trans_type === ST_SUPPRECEIVE;
	$quantity = $is_receipt ? $line->receive_qty : $line->this_quantity_inv;
	$resolution = array('contract' => 'purchasing-receipt-invoice-classification/1',
		'trans_type' => (int)$trans_type, 'role' => 'line', 'flow' => $plan['flow'],
		'enabled_axes' => (int)$plan['enabled_axes'], 'policy_version' => (int)$line_plan['policy_version'],
		'axes' => array(1 => $line_plan['axes'][1], 2 => $line_plan['axes'][2]),
		'stock_id' => (string)($is_receipt ? $line->stock_id : $line->item_code), 'source' => $line_plan['source'],
		'posting_roles' => purchasing_receipt_posting_roles($trans_type, $plan['flow']));
	if (!$is_receipt) {
		$resolution['matching'] = array('grn_item_id' => (int)$line->id, 'po_detail_item' => (int)$line->po_detail_item,
			'order_price' => purchasing_receipt_decimal_text($line->order_price),
			'invoiced_price' => purchasing_receipt_decimal_text($line->chg_price),
			'price_changed' => (float)$line->order_price != (float)$line->chg_price);
	}
	$override_reason = '';
	if ($line_plan['override'] !== null) {
		$resolution['override'] = array('axes' => $line_plan['override']['axes'],
			'source_values' => $line_plan['override']['source_values']);
		$override_reason = $line_plan['override']['reason'];
	}
	$stored = purchasing_receipt_persist_revision($trans_type, $document, 'line', $line_key, $resolution,
		$plan['effective_date'], purchasing_classification_quantity($quantity), $override_reason);
	return (int)$stored['revision_id'];
}

/**
 * Record the explicit pair of one direct (GL account) bill line.
 *
 * @param object $gl_line Bill GL line
 * @param mixed $document_no Final bill number
 * @param mixed $index gl_codes index
 * @param mixed $detail_id Persisted supp_invoice_items id
 * @param array|null $plan Plan
 * @return int Revision id, or 0 when the bill is legacy
 */
function purchasing_receipt_record_gl_line($gl_line, $document_no, $index, $detail_id, $plan)
{
	if (!is_array($plan) || !isset($plan['gl_lines'][$index]))
		return 0;
	$document = dimension_source_revision_document_key($document_no);
	$line_key = dimension_source_revision_line_key('line', $detail_id);
	if ($document === false || $line_key === false)
		throw new DimensionFinancialReferenceWriteException(_('The saved purchasing line identity is invalid.'));
	$gl_plan = $plan['gl_lines'][$index];
	$resolution = array('contract' => 'purchasing-receipt-invoice-classification/1',
		'trans_type' => ST_SUPPINVOICE, 'role' => 'gl_line', 'flow' => $plan['flow'],
		'enabled_axes' => (int)$plan['enabled_axes'], 'policy_version' => (int)$gl_plan['policy_version'],
		'axes' => array(1 => $gl_plan['axes'][1], 2 => $gl_plan['axes'][2]),
		'gl_code' => (string)$gl_line->gl_code, 'source' => array('state' => 'explicit_direct_gl'),
		'posting_roles' => array('direct_gl_expense' => 'explicit_line'));
	$stored = purchasing_receipt_persist_revision(ST_SUPPINVOICE, $document, 'line', $line_key, $resolution,
		$plan['effective_date'], '0.00000000', '');
	return (int)$stored['revision_id'];
}

/**
 * Publish one received line's pair to the shared inventory writer context for the duration of a
 * stock-cost update, so a zero/negative-stock correction that the receipt provokes keeps the
 * receipt's lineage instead of falling back to the current item default.
 *
 * @param string $action push or pop
 * @param int $grn GRN batch number (push only)
 * @param string $stock_id Item (push only)
 * @param array|null $pair axis => value (push only)
 * @return void
 */
function purchasing_receipt_inventory_context($action, $grn = 0, $stock_id = '', $pair = null)
{
	if (!function_exists('inventory_classification_context'))
		return;
	if ($action === 'push') {
		if ($pair === null)
			return;
		inventory_classification_context('push', array('type' => ST_SUPPRECEIVE, 'number' => $grn,
			'stock_id' => $stock_id, 'axes' => array(1 => $pair[1], 2 => $pair[2])));
		return;
	}
	inventory_classification_context('pop');
}
