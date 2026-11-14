<?php
/**
 * DIM-SAL-002: delivery/invoice revenue, COGS and tax classification lineage.
 *
 * A delivery (COGS, and revenue for a prepaid order) and an invoice (revenue, discount, receivable,
 * freight and tax) are posted from an order or a parent delivery whose classification was already
 * snapshotted by DIM-SAL-001. This adapter makes the posting consume that snapshot instead of
 * recomputing intent from the current customer/item defaults:
 *
 *  - every posted line resolves through the shared DIM-FND-005 order. The parent line snapshot
 *    (order line for a delivery, delivery line for an invoice, order line for a prepayment invoice)
 *    outranks the document header and any current default, so partial deliveries and partial
 *    invoices of one order line carry the same immutable pair and revenue equals COGS lineage;
 *  - a document with no parent snapshot at all (a legacy order/delivery) keeps the characterized
 *    E08 behavior untouched; nothing is reconstructed from a current default;
 *  - a permitted override is explicit and recorded: a changed document header, or a line override
 *    carrier that names its reason, is stored with the source pair it replaced;
 *  - every posted delivery/invoice header and line stores one append-only DIM-FND-006 revision in the
 *    `sales` family that names its parent source revision, the flow (standard, direct, prepayment)
 *    and the distinct posting roles (revenue, COGS owner, receivable, freight, discount, tax).
 *
 * The adapter contains no ledger or native-table write of its own: every posting still passes the
 * maintained writers and DIM-FND-007 admission. PHP 5.6 compatible.
 */
require_once __DIR__.'/sales_classification_db.inc';

// Focused CLI harnesses evaluate the writers without the page bootstrap. The canonical values are
// admitted here without ever replacing an existing definition.
foreach (array('ST_SALESINVOICE' => 10, 'ST_CUSTDELIVERY' => 13, 'ST_SALESORDER' => 30)
	as $sales_fulfillment_type_name => $sales_fulfillment_type_value)
	if (!defined($sales_fulfillment_type_name))
		define($sales_fulfillment_type_name, $sales_fulfillment_type_value);

/**
 * Transaction types owned by this adapter.
 *
 * @return int[]
 */
function sales_fulfillment_supported_types()
{
	return array(ST_CUSTDELIVERY, ST_SALESINVOICE);
}

/**
 * Is one type a supported fulfillment source?
 *
 * @param mixed $trans_type Stored type
 * @return bool
 */
function sales_fulfillment_type_supported($trans_type)
{
	$type = dimension_source_revision_int($trans_type, 0);
	return $type !== false && in_array($type, sales_fulfillment_supported_types(), true);
}

/**
 * Is the accepted companion revision schema installed for the current company?
 *
 * A company without it posts exactly as before and records no synthetic intent.
 *
 * @return bool
 */
function sales_fulfillment_schema_ready()
{
	return dimension_source_revision_schema_ready() === true;
}

/**
 * Canonical eight-decimal text for sealed evidence (never a float).
 *
 * @param mixed $amount Quantity or amount
 * @return string
 */
function sales_fulfillment_decimal_text($amount)
{
	return number_format((float)$amount, 8, '.', '');
}

/**
 * Locate the parent line that one delivery/invoice line was created from.
 *
 * @param int $trans_type ST_CUSTDELIVERY or ST_SALESINVOICE
 * @param mixed $src_id Stored parent line id
 * @param bool $prepaid Whether an invoice is a prepayment invoice (its parent is the order line)
 * @return array|false trans_type, document_key and line_key; false when the line has no parent
 */
function sales_fulfillment_parent_line($trans_type, $src_id, $prepaid)
{
	$id = dimension_source_revision_int($src_id, 1);
	if ($id === false)
		return false;
	if ((int)$trans_type === ST_CUSTDELIVERY || $prepaid) {
		$rows = db_query('SELECT order_no,trans_type FROM '.TB_PREF.'sales_order_details WHERE id='.$id);
		$expected = ST_SALESORDER;
		$number = 'order_no';
		$type = 'trans_type';
	} else {
		$rows = db_query('SELECT debtor_trans_no,debtor_trans_type FROM '.TB_PREF
			.'debtor_trans_details WHERE id='.$id);
		$expected = ST_CUSTDELIVERY;
		$number = 'debtor_trans_no';
		$type = 'debtor_trans_type';
	}
	if (!$rows)
		throw new DimensionFinancialReferenceWriteException(
			_('The Sales source line of this document cannot be read.'));
	$row = db_fetch_assoc($rows);
	if (!$row || (int)$row[$type] !== (int)$expected)
		return false;
	$document = dimension_source_revision_document_key($row[$number]);
	$line_key = dimension_source_revision_line_key('line', $id);
	if ($document === false || $line_key === false)
		return false;
	return array('trans_type' => (int)$expected, 'document_key' => $document, 'line_key' => $line_key);
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
function sales_fulfillment_read_snapshot($trans_type, $document_key, $line_kind, $line_key)
{
	$snapshot = dimension_source_revision_snapshot('sales', $trans_type, $document_key, $line_kind,
		$line_key);
	if (!in_array($snapshot['state'], array('available', 'absent', 'unavailable'), true))
		throw new DimensionFinancialReferenceWriteException(
			_('The source classification of this Sales line cannot be verified. Nothing was saved.'));
	return $snapshot;
}

/**
 * Describe one parent read for sealed evidence.
 *
 * @param array|false $parent Parent descriptor
 * @param array $snapshot Snapshot-shaped read
 * @return array
 */
function sales_fulfillment_source_evidence($parent, $snapshot)
{
	if ($parent === false)
		return array('state' => 'none');
	$evidence = array('state' => (string)$snapshot['state'], 'trans_type' => (int)$parent['trans_type'],
		'document_key' => (string)$parent['document_key'], 'line_key' => (string)$parent['line_key']);
	if (isset($snapshot['revision']['source_revision_id']))
		$evidence['revision_id'] = (int)$snapshot['revision']['source_revision_id'];
	return $evidence;
}

/**
 * Settle the parent header snapshot for a document built from one or more parents.
 *
 * Several deliveries on one invoice can disagree; the header is then not attributable to a single
 * parent and the document header selection stands, recorded as `diverged`.
 *
 * @param array $parents Distinct parent descriptors keyed by `type:number`
 * @return array state (none, absent, available, diverged), snapshot and evidence
 */
function sales_fulfillment_parent_header($parents)
{
	$result = array('state' => 'none', 'snapshot' => null, 'evidence' => array('state' => 'none'));
	if (!count($parents))
		return $result;
	$first = null;
	$evidence = array();
	$state = 'available';
	foreach ($parents as $parent) {
		$snapshot = sales_fulfillment_read_snapshot($parent['trans_type'], $parent['document_key'],
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
function sales_fulfillment_finalize_pair($pair, $enabled)
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
				'origin' => $axis > $enabled ? 'axis_disabled' : 'sales_unassigned',
				'persisted' => false, 'policy_version' => 0, 'basis' => ''));
		}
	}
	return $pair;
}

/**
 * Read and validate one line's explicit override carrier.
 *
 * The carrier is `array('reason' => text, 'intents' => axis => intent)`. An override with no reason
 * is refused: an unexplained re-classification of a posted source is exactly what this section
 * prevents.
 *
 * @param object $line Delivery/invoice line
 * @return array|null reason and intents, or null when the line carries no override
 */
function sales_fulfillment_line_override($line)
{
	if (!is_object($line) || !isset($line->classification_override)
		|| !is_array($line->classification_override))
		return null;
	$carrier = $line->classification_override;
	$reason = isset($carrier['reason']) && is_string($carrier['reason']) ? trim($carrier['reason']) : '';
	if ($reason === '' || strlen($reason) > 255 || preg_match('/[\x00-\x1f\x7f]/', $reason))
		throw new DimensionFinancialReferenceWriteException(
			_('A Sales classification override needs a clear reason. Nothing was saved.'));
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
 * Name the commercial flow of one fulfillment document.
 *
 * @param object $doc Delivery/invoice cart
 * @param int $trans_type Document type
 * @param array $parents Distinct parent descriptors
 * @return string standard, direct, prepaid_delivery or prepayment_invoice
 */
function sales_fulfillment_flow($doc, $trans_type, $parents)
{
	$prepaid = is_object($doc) && !empty($doc->prepaid);
	if ($prepaid)
		return (int)$trans_type === ST_SALESINVOICE ? 'prepayment_invoice' : 'prepaid_delivery';
	if (function_exists('get_reference'))
		foreach ($parents as $parent)
			if (get_reference($parent['trans_type'], $parent['document_key']) === 'auto')
				return 'direct';
	return 'standard';
}

/**
 * Resolve the classification of one delivery/invoice from its parents, without any approval check.
 *
 * The optional hooks let a sibling inverse adapter (DIM-SAL-003 credit notes) reuse exactly this
 * resolution order with its own parent lookup: `types` (int[] replacing the supported types),
 * `parent` (callable(line, prepaid) returning a parent descriptor or false) and `flow`
 * (callable(doc, trans_type, parents) returning the flow name).
 *
 * @param object $doc Sales cart being written
 * @param int $trans_type ST_CUSTDELIVERY or ST_SALESINVOICE (or a hook-declared type)
 * @param array $hooks Optional types, parent and flow hooks
 * @return array|null Plan, or null when the document keeps the legacy behavior
 */
function sales_fulfillment_resolve_plan($doc, $trans_type, $hooks = array())
{
	$type_allowed = isset($hooks['types']) && is_array($hooks['types'])
		? in_array((int)$trans_type, $hooks['types'], true) : sales_fulfillment_type_supported($trans_type);
	if (!is_object($doc) || !$type_allowed || !sales_fulfillment_schema_ready())
		return null;
	if (!isset($doc->line_items) || !is_array($doc->line_items))
		throw new DimensionFinancialReferenceWriteException(
			_('The Sales classification lines are unavailable.'));
	$effective = dimension_source_revision_date(date2sql($doc->document_date));
	if ($effective === false)
		throw new DimensionFinancialReferenceWriteException(_('The Sales source date is invalid.'));
	$enabled = (int)get_company_pref('use_dimension');
	$prepaid = !empty($doc->prepaid);
	$customer = sales_classification_customer_defaults(isset($doc->customer_id) ? $doc->customer_id : 0);

	$entries = array();
	$parents = array();
	foreach ($doc->line_items as $index => $line) {
		if (!is_object($line))
			throw new DimensionFinancialReferenceWriteException(_('A Sales classification line is invalid.'));
		if ((float)$line->qty_dispatched == 0)
			continue;
		if (isset($hooks['parent']))
			$parent = call_user_func($hooks['parent'], $line, $prepaid);
		else
			$parent = isset($line->src_id) ? sales_fulfillment_parent_line($trans_type, $line->src_id, $prepaid)
				: false;
		$snapshot = array('state' => 'none');
		if ($parent !== false) {
			$snapshot = sales_fulfillment_read_snapshot($parent['trans_type'], $parent['document_key'],
				'line', $parent['line_key']);
			$parents[$parent['trans_type'].':'.$parent['document_key']] = $parent;
		}
		$entries[$index] = array('parent' => $parent, 'snapshot' => $snapshot);
	}
	$header_source = sales_fulfillment_parent_header($parents);

	// Header: the parent header snapshot stands unless the document header explicitly names another
	// category. A blank header selection is "no choice" (the pages offer no unassigned control).
	$header_sources = array(1 => array(), 2 => array());
	$header_override = array(1 => false, 2 => false);
	$usable_header = array(1 => array(), 2 => array());
	foreach (array(1, 2) as $axis) {
		$field = $axis === 1 ? 'dimension_id' : 'dimension2_id';
		$native = dimension_source_revision_int(isset($doc->$field) ? $doc->$field : 0, 0);
		if ($native === false)
			throw new DimensionFinancialReferenceWriteException(
				_('The Sales header classification identity is invalid.'));
		if ($axis > $enabled)
			$native = 0;
		$snapshot_source = array();
		$snapshot_value = null;
		if ($header_source['state'] === 'available' && $axis <= $enabled) {
			$row = $header_source['snapshot']['revision']['axes'][$axis];
			// A snapshot that was "not applicable" while the category was off cannot decide a new
			// event now that the category is enabled; it is skipped and the lower sources decide.
			if ($row['mode'] !== 'not_applicable') {
				$snapshot_source = sales_classification_snapshot_axis_source($header_source['snapshot'], $axis);
				$snapshot_value = (int)$row['value'];
			}
		}
		if (count($snapshot_source)) {
			$usable_header[$axis] = $snapshot_source;
			if ($native > 0 && $native !== $snapshot_value) {
				$header_override[$axis] = true;
				$header_sources[$axis]['header_explicit'] = dimension_resolution_selected($native, 'document_header');
			} else {
				$header_sources[$axis]['header_explicit'] = dimension_resolution_inherit('sales_header');
			}
			$header_sources[$axis]['header_snapshot'] = $snapshot_source;
		} else {
			$header_sources[$axis]['header_explicit'] = $native > 0
				? dimension_resolution_selected($native, 'document_header')
				: dimension_resolution_inherit('sales_header');
		}
		$header_sources[$axis]['partner_default'] = dimension_resolution_default_intent(
			$customer[$axis], 'partner_default');
	}

	$line_plans = array();
	$legacy_lines = array();
	$any_override = false;
	foreach ($entries as $index => $entry) {
		$line = $doc->line_items[$index];
		$override = sales_fulfillment_line_override($line);
		$has_line_snapshot = $entry['snapshot']['state'] === 'available';
		if (!$has_line_snapshot && $override === null && !count($usable_header[1]) && !count($usable_header[2])) {
			$legacy_lines[] = $index;
			continue;
		}
		$sources = array(1 => array(), 2 => array());
		foreach (array(1, 2) as $axis) {
			if ($override !== null && isset($override['intents'][$axis]))
				$sources[$axis]['line_explicit'] = $override['intents'][$axis];
			if ($has_line_snapshot && $axis <= $enabled
				&& $entry['snapshot']['revision']['axes'][$axis]['mode'] !== 'not_applicable')
				$sources[$axis]['line_snapshot'] = sales_classification_snapshot_axis_source(
					$entry['snapshot'], $axis);
			$sources[$axis]['header_explicit'] = $header_override[$axis]
				? $header_sources[$axis]['header_explicit'] : dimension_resolution_inherit('sales_header');
			if (count($usable_header[$axis]))
				$sources[$axis]['header_snapshot'] = $usable_header[$axis];
			$sources[$axis]['partner_default'] = dimension_resolution_default_intent(
				$customer[$axis], 'partner_default');
		}
		$pair = dimension_resolution_pair($sources[1], $sources[2], array('enabled_axes' => $enabled));
		if (!$pair['ok'])
			throw new DimensionFinancialReferenceWriteException(implode(' ', $pair['errors']));
		$pair = sales_fulfillment_finalize_pair($pair, $enabled);
		$line_plan = array('axes' => $pair['axes'], 'policy_version' => (int)$pair['policy_version'],
			'parent' => $entry['parent'], 'source' => sales_fulfillment_source_evidence($entry['parent'],
				$entry['snapshot']), 'override' => null);
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
	if (!count($line_plans) && $header_source['state'] !== 'available')
		return null;

	$header_pair = dimension_resolution_pair($header_sources[1], $header_sources[2],
		array('enabled_axes' => $enabled));
	if (!$header_pair['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $header_pair['errors']));
	$header_pair = sales_fulfillment_finalize_pair($header_pair, $enabled);
	return array('contract' => 'sales-fulfillment-plan/1', 'trans_type' => (int)$trans_type,
		'flow' => isset($hooks['flow']) ? (string)call_user_func($hooks['flow'], $doc, $trans_type, $parents)
				: sales_fulfillment_flow($doc, $trans_type, $parents), 'effective_date' => $effective,
		'enabled_axes' => $enabled, 'parents' => array_values($parents),
		'header' => array('axes' => $header_pair['axes'], 'policy_version' => (int)$header_pair['policy_version'],
			'override' => $header_override, 'source' => $header_source['evidence']),
		'lines' => $line_plans, 'legacy_lines' => $legacy_lines, 'has_override' => $any_override
			|| $header_override[1] || $header_override[2]);
}

/**
 * Bounded, deterministic summary of a plan that an approver reviews and that execution must reproduce.
 *
 * @param array|null $plan Plan, or null for a legacy document
 * @return array version, state and the per-line pairs with their parent revision ids
 */
function sales_fulfillment_plan_evidence($plan)
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
	$header = sales_fulfillment_header_pair($plan);
	return array('version' => 1, 'state' => 'snapshot', 'trans_type' => (int)$plan['trans_type'],
		'flow' => (string)$plan['flow'],
		'header' => array((int)$header[1], (int)$header[2],
			!empty($plan['header']['override'][1]), !empty($plan['header']['override'][2])),
		'lines' => $lines, 'legacy_lines' => array_values($plan['legacy_lines']));
}

/**
 * Evidence to attach to an approval submission, so the reviewed classification is what executes.
 *
 * @param object $doc Delivery/invoice cart about to be submitted
 * @param int $trans_type ST_CUSTDELIVERY or ST_SALESINVOICE
 * @return array|null Evidence, or null for a type or company this adapter does not govern
 */
function sales_fulfillment_approval_evidence($doc, $trans_type)
{
	if (!sales_fulfillment_type_supported($trans_type) || !sales_fulfillment_schema_ready())
		return null;
	return sales_fulfillment_plan_evidence(sales_fulfillment_resolve_plan($doc, $trans_type));
}

/**
 * Plan the classification of one delivery/invoice before any row is written.
 *
 * A cart restored from an approved draft carries the evidence that was reviewed; the plan resolved
 * at execution must reproduce it exactly, or the document is refused rather than silently posted
 * under a classification nobody approved (for example after the source order was amended).
 *
 * @param object $doc Sales cart being written
 * @param int $trans_type ST_CUSTDELIVERY or ST_SALESINVOICE
 * @return array|null Plan, or null when the document keeps the legacy behavior
 */
function sales_fulfillment_plan($doc, $trans_type)
{
	$plan = sales_fulfillment_resolve_plan($doc, $trans_type);
	if (is_object($doc) && isset($doc->classification_fulfillment) && is_array($doc->classification_fulfillment)
		&& sales_fulfillment_type_supported($trans_type) && sales_fulfillment_schema_ready()
		&& json_encode(sales_fulfillment_plan_evidence($plan))
			!== json_encode($doc->classification_fulfillment))
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
function sales_fulfillment_header_pair($plan)
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
function sales_fulfillment_line_pair($plan, $index)
{
	if (!is_array($plan) || !isset($plan['lines'][$index]['axes'][1], $plan['lines'][$index]['axes'][2]))
		return null;
	return array(1 => (int)$plan['lines'][$index]['axes'][1]['value'],
		2 => (int)$plan['lines'][$index]['axes'][2]['value']);
}

/**
 * Mirror the planned header pair onto the cart so the debtor header and every header-attributed
 * ledger row (receivable, discount, freight, tax) carry the one recorded pair.
 *
 * @param object $doc Cart
 * @param array|null $plan Plan
 * @return void
 */
function sales_fulfillment_apply_header($doc, $plan)
{
	$pair = sales_fulfillment_header_pair($plan);
	if ($pair === null || !is_object($doc))
		return;
	$doc->dimension_id = $pair[1];
	$doc->dimension2_id = $pair[2];
}

/**
 * Header values that a new delivery should start from: the order header snapshot.
 *
 * Returns false for a legacy order, which keeps its current-customer-default behavior.
 *
 * @param object $cart Order cart that was just read
 * @return array|false axis => value
 */
function sales_fulfillment_source_header_values($cart)
{
	if (!is_object($cart) || !isset($cart->classification_lineage_state)
		|| $cart->classification_lineage_state !== 'versioned'
		|| !isset($cart->classification_intents) || !is_array($cart->classification_intents))
		return false;
	$values = array();
	foreach (array(1, 2) as $axis) {
		if (!isset($cart->classification_intents[$axis]['mode']))
			return false;
		$intent = $cart->classification_intents[$axis];
		if ($intent['mode'] === 'selected')
			$values[$axis] = (int)$intent['id'];
		elseif (in_array($intent['mode'], array('unassigned', 'not_applicable'), true))
			$values[$axis] = 0;
		else
			return false;
	}
	return $values;
}

/**
 * Compare the durable classification-relevant bytes of one stored revision.
 *
 * @param array $current Current verified revision
 * @param array $fields Prepared candidate fields
 * @return bool
 */
function sales_fulfillment_revision_equivalent($current, $fields)
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
 * Persist one fulfillment revision, converging on an identical retry and appending on change.
 *
 * @param int $trans_type Delivery or invoice type
 * @param string $document_key Final document number
 * @param string $line_kind header or line
 * @param string $line_key Stable line key
 * @param array $resolution Resolved axes and evidence
 * @param string $effective_date ISO date
 * @param string $line_quantity Canonical quantity
 * @param string $override_reason Reason, or an empty string
 * @return array ok, revision_id, created
 */
function sales_fulfillment_persist_revision($trans_type, $document_key, $line_kind, $line_key,
	$resolution, $effective_date, $line_quantity, $override_reason)
{
	$options = array('effective_date' => $effective_date, 'line_quantity' => $line_quantity,
		'resolution' => $resolution);
	if ($override_reason !== '')
		$options['override_reason'] = $override_reason;
	$prepared = dimension_source_revision_prepare('sales', $trans_type, $document_key, $line_kind,
		$line_key, $resolution['axes'], $options);
	if (!$prepared['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $prepared['errors']));
	$current = dimension_source_revision_snapshot('sales', $trans_type, $document_key, $line_kind,
		$line_key);
	if (!in_array($current['state'], array('available', 'absent', 'unavailable'), true))
		throw new DimensionFinancialReferenceWriteException(
			_('The current Sales classification revision cannot be verified. Nothing was saved.'));
	if ($current['state'] === 'available') {
		if (sales_fulfillment_revision_equivalent($current['revision'], $prepared['fields']))
			return array('ok' => true, 'revision_id' => (int)$current['revision']['source_revision_id'],
				'created' => false);
		$options['predecessor_revision_id'] = (int)$current['revision']['source_revision_id'];
		$prepared = dimension_source_revision_prepare('sales', $trans_type, $document_key, $line_kind,
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
 * Revenue/discount follow each line's immutable pair and COGS follows the delivery line; the
 * receivable, freight, promotion discount and tax rows are header-attributed. The prepayment
 * deferred-income balancing leg is a balance-sheet row posted without classification by design.
 *
 * @param int $trans_type Document type
 * @param string $flow Flow name
 * @return array role => basis
 */
function sales_fulfillment_posting_roles($trans_type, $flow)
{
	if ((int)$trans_type === ST_CUSTDELIVERY) {
		$roles = array('cogs' => 'line', 'inventory_credit' => 'line');
		if ($flow === 'prepaid_delivery') {
			$roles['revenue_from_deferred'] = 'line';
			$roles['deferred_income'] = 'unclassified_balance_sheet';
		}
		return $roles;
	}
	$roles = array('revenue' => $flow === 'prepayment_invoice' ? 'line_deferred' : 'line',
		'line_discount' => 'line', 'receivable' => 'header', 'promotion_discount' => 'header',
		'freight' => 'header', 'tax' => 'header', 'cogs' => 'delivery_parent');
	return $roles;
}

/**
 * Record the header revision of one written delivery/invoice.
 *
 * @param object $doc Written cart
 * @param int $trans_type Document type
 * @param mixed $document_no Final native number
 * @param array|null $plan Plan, or null for a legacy document
 * @return int Revision id, or 0 when nothing is recorded
 */
function sales_fulfillment_record_header($doc, $trans_type, $document_no, $plan)
{
	if (!is_array($plan))
		return 0;
	$document = dimension_source_revision_document_key($document_no);
	if ($document === false)
		throw new DimensionFinancialReferenceWriteException(_('The saved Sales document identity is invalid.'));
	$resolution = array('contract' => 'sales-fulfillment-classification/1',
		'trans_type' => (int)$trans_type, 'role' => 'header', 'flow' => $plan['flow'],
		'enabled_axes' => (int)$plan['enabled_axes'], 'policy_version' => (int)$plan['header']['policy_version'],
		'axes' => array(1 => $plan['header']['axes'][1], 2 => $plan['header']['axes'][2]),
		'source' => $plan['header']['source'],
		'posting_roles' => sales_fulfillment_posting_roles($trans_type, $plan['flow']),
		'line_count' => count($plan['lines']), 'legacy_line_count' => count($plan['legacy_lines']));
	$override_reason = '';
	if ($plan['header']['override'][1] || $plan['header']['override'][2]) {
		$resolution['override'] = array('axes' => array_keys(array_filter($plan['header']['override'])));
		$override_reason = 'Document header differs from the source classification';
	}
	$stored = sales_fulfillment_persist_revision($trans_type, $document, 'header', 'header', $resolution,
		$plan['effective_date'], '0.00000000', $override_reason);
	return (int)$stored['revision_id'];
}

/**
 * Record the line revision of one written delivery/invoice line.
 *
 * @param object $doc Written cart
 * @param int $trans_type Document type
 * @param mixed $document_no Final native number
 * @param mixed $index Cart line index
 * @param mixed $detail_id Persisted debtor_trans_details id
 * @param array|null $plan Plan
 * @return int Revision id, or 0 when the line is legacy
 */
function sales_fulfillment_record_line($doc, $trans_type, $document_no, $index, $detail_id, $plan)
{
	if (!is_array($plan) || !isset($plan['lines'][$index]))
		return 0;
	$document = dimension_source_revision_document_key($document_no);
	$line_key = dimension_source_revision_line_key('line', $detail_id);
	if ($document === false || $line_key === false)
		throw new DimensionFinancialReferenceWriteException(_('The saved Sales line identity is invalid.'));
	$line = $doc->line_items[$index];
	$line_plan = $plan['lines'][$index];
	$resolution = array('contract' => 'sales-fulfillment-classification/1',
		'trans_type' => (int)$trans_type, 'role' => 'line', 'flow' => $plan['flow'],
		'enabled_axes' => (int)$plan['enabled_axes'], 'policy_version' => (int)$line_plan['policy_version'],
		'axes' => array(1 => $line_plan['axes'][1], 2 => $line_plan['axes'][2]),
		'stock_id' => (string)$line->stock_id, 'source' => $line_plan['source'],
		'posting_roles' => sales_fulfillment_posting_roles($trans_type, $plan['flow']));
	if ((int)$trans_type === ST_SALESINVOICE && $line_plan['parent'] !== false
		&& (int)$line_plan['parent']['trans_type'] === ST_CUSTDELIVERY)
		$resolution['cogs_recognized_on'] = array('trans_type' => ST_CUSTDELIVERY,
			'document_key' => (string)$line_plan['parent']['document_key'],
			'line_key' => (string)$line_plan['parent']['line_key']);
	$override_reason = '';
	if ($line_plan['override'] !== null) {
		$resolution['override'] = array('axes' => $line_plan['override']['axes'],
			'source_values' => $line_plan['override']['source_values']);
		$override_reason = $line_plan['override']['reason'];
	}
	$stored = sales_fulfillment_persist_revision($trans_type, $document, 'line', $line_key, $resolution,
		$plan['effective_date'], sales_classification_quantity($line->qty_dispatched), $override_reason);
	return (int)$stored['revision_id'];
}
