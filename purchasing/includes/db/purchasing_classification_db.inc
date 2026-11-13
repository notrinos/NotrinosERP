<?php
/**
 * DIM-PUR-002: durable purchasing source classification intent.
 *
 * This adapter resolves the purchasing header/line intent through the shared DIM-FND-005/DIM-FND-004
 * contracts and stores it through DIM-FND-006. It never writes a ledger row, never invents a default
 * for an unsnapshotted legacy document, and never replaces the native purchasing tables with a second
 * source of business truth.
 *
 * Purchasing documents have no native per-line dimension columns: the purchase order header, its
 * lines, purchase requisitions, RFQs, agreements, procurement plans and purchase templates are all
 * classified through the accepted companion revision store. The native supplier/item defaults are
 * only a weak last source for a genuinely new document.
 *
 * PHP 5.6 compatible.
 */
require_once __DIR__.'/../../../dimensions/includes/dimension_source_revision.inc';
require_once __DIR__.'/../../../dimensions/includes/dimension_category_policy.inc';

// A purchase template is a source owner but is not a posted transaction type. Keeping a distinct
// source type prevents a template id from colliding with a purchase order number in the same family.
if (!defined('ST_PURCHTEMPLATE'))
	define('ST_PURCHTEMPLATE', 66);
// The canonical transaction types are loaded by includes/types.inc in a normal request. The focused
// CLI harnesses evaluate named writer functions without a full bootstrap, so the same canonical
// values are admitted here without ever replacing an existing definition.
foreach (array('ST_PURCHORDER' => 18, 'ST_PURCHREQUISITION' => 58, 'ST_PURCHRFQ' => 59,
	'ST_PURCHAGREEMENT' => 60, 'ST_PROCUREMENTPLAN' => 61) as $purchasing_type => $purchasing_value)
	if (!defined($purchasing_type))
		define($purchasing_type, $purchasing_value);

/**
 * Transaction/source types owned by this adapter.
 *
 * @return int[]
 */
function purchasing_classification_supported_types()
{
	return array(ST_PURCHORDER, ST_PURCHREQUISITION, ST_PURCHRFQ, ST_PURCHAGREEMENT,
		ST_PURCHTEMPLATE, ST_PROCUREMENTPLAN);
}

/**
 * Is one type a supported purchasing classification source?
 *
 * @param mixed $trans_type Stored source type
 * @return bool
 */
function purchasing_classification_type_supported($trans_type)
{
	$type = dimension_source_revision_int($trans_type, 0);
	return $type !== false && in_array($type, purchasing_classification_supported_types(), true);
}

/**
 * Build a bounded opaque token accepted by the durable revision authority.
 *
 * @param string $prefix Token prefix
 * @return string
 */
function purchasing_classification_token($prefix)
{
	$prefix = preg_replace('/[^a-z0-9_]/', '', strtolower((string)$prefix));
	if ($prefix === '' || !preg_match('/^[a-z]/', $prefix))
		$prefix = 'source';
	$random = function_exists('openssl_random_pseudo_bytes')
		? openssl_random_pseudo_bytes(24) : null;
	$hex = is_string($random) && strlen($random) > 0
		? bin2hex($random) : hash('sha256', uniqid('', true).mt_rand().microtime(true));
	return substr($prefix, 0, 8).'_'.substr($hex, 0, 48);
}

/**
 * Canonical quantity accepted by the eight-decimal source-revision contract.
 *
 * @param mixed $quantity Source line quantity
 * @return string
 */
function purchasing_classification_quantity($quantity)
{
	$value = dimension_source_revision_decimal(number_format((float)$quantity, 8, '.', ''));
	if ($value === false)
		throw new DimensionFinancialReferenceWriteException(
			_('A purchasing source line has an invalid quantity and cannot retain classification intent.'));
	return $value;
}

/**
 * Build one normalized explicit intent from a stored value and an explicit-unassigned marker.
 *
 * @param mixed $id Positive stored identity or zero
 * @param mixed $unassigned Explicit-unassigned marker
 * @param int $axis Native axis
 * @param string $origin Decision origin
 * @return array|null Builder result, or null for inheritance
 */
function purchasing_classification_intent_from_values($id, $unassigned, $axis, $origin)
{
	$identity = dimension_source_revision_int($id, 0);
	if ($identity === false)
		return array('ok' => false, 'intent' => null, 'codes' => array('invalid_identity'),
			'errors' => array(_('The selected purchasing classification identity is invalid.')));
	if (!empty($unassigned) && $identity !== 0)
		return array('ok' => false, 'intent' => null, 'codes' => array('conflicting_intent'),
			'errors' => array(_('A purchasing category cannot be both selected and explicitly unassigned.')));
	if (!empty($unassigned))
		return dimension_resolution_unassigned($origin);
	if ($identity > 0)
		return dimension_resolution_selected($identity, $origin);
	return null;
}

/**
 * Normalize one intent stored on a cart, line, or approval payload.
 *
 * A reloaded document carries persisted evidence; it is admitted through the snapshot shape of its
 * carrier so a later writer capture converges instead of re-claiming a persisted intent as explicit.
 *
 * @param array|null $carrier Object/array carrying classification_intents
 * @param int $axis Native axis
 * @param string $origin Fallback descriptive origin
 * @param string $persisted_kind Snapshot source kind used for persisted evidence
 * @return array|null Normalized builder result, or null when no explicit mode was recorded
 */
function purchasing_classification_carried_intent($carrier, $axis, $origin,
	$persisted_kind = 'header_snapshot')
{
	$intents = is_object($carrier) && isset($carrier->classification_intents)
		? $carrier->classification_intents
		: (is_array($carrier) && isset($carrier['classification_intents'])
			? $carrier['classification_intents'] : null);
	if (!is_array($intents) || !array_key_exists((int)$axis, $intents))
		return null;
	$raw = $intents[(int)$axis];
	$kind = is_array($raw) && !empty($raw['persisted']) ? $persisted_kind : 'header_explicit';
	return dimension_resolution_normalize_source($raw, $kind);
}

/**
 * Read the current supplier default pair without inventing a classification when unavailable.
 *
 * @param mixed $supplier_id Supplier identity
 * @return array axis => configured identity
 */
function purchasing_classification_supplier_defaults($supplier_id)
{
	static $cache = array();
	$id = dimension_source_revision_int($supplier_id, 0);
	if ($id === false || $id < 1)
		return array(1 => 0, 2 => 0);
	$company = dimension_source_revision_company();
	$key = $company.'|'.$id;
	if (isset($cache[$key]))
		return $cache[$key];
	$rows = db_query('SELECT dimension_id,dimension2_id FROM '.TB_PREF.'suppliers WHERE supplier_id='.$id);
	if (!$rows)
		throw new DimensionFinancialReferenceWriteException(
			_('The purchasing supplier classification defaults cannot be read.'));
	$row = db_fetch_assoc($rows);
	return $cache[$key] = $row ? array(
		1 => (isset($row['dimension_id']) ? (int)$row['dimension_id'] : 0),
		2 => (isset($row['dimension2_id']) ? (int)$row['dimension2_id'] : 0)) : array(1 => 0, 2 => 0);
}

/**
 * Read one item's default pair. The caller decides whether this source is eligible.
 *
 * @param mixed $stock_id Item code
 * @return array axis => configured identity
 */
function purchasing_classification_item_defaults($stock_id)
{
	static $cache = array();
	if (!is_string($stock_id) || $stock_id === '' || strlen($stock_id) > 20)
		return array(1 => 0, 2 => 0);
	$company = dimension_source_revision_company();
	$key = $company.'|'.$stock_id;
	if (isset($cache[$key]))
		return $cache[$key];
	$rows = db_query('SELECT dimension_id,dimension2_id FROM '.TB_PREF.'stock_master WHERE stock_id='
		.db_escape($stock_id));
	if (!$rows)
		throw new DimensionFinancialReferenceWriteException(
			_('The purchasing item classification defaults cannot be read.'));
	$row = db_fetch_assoc($rows);
	return $cache[$key] = $row ? array(
		1 => (isset($row['dimension_id']) ? (int)$row['dimension_id'] : 0),
		2 => (isset($row['dimension2_id']) ? (int)$row['dimension2_id'] : 0)) : array(1 => 0, 2 => 0);
}

/**
 * Validate and return one source descriptor.
 *
 * @param mixed $source Descriptor
 * @return array|false
 */
function purchasing_classification_source_descriptor($source)
{
	if (!is_array($source)
		|| !isset($source['trans_type'], $source['document_key'], $source['line_kind'], $source['line_key']))
		return false;
	$type = dimension_source_revision_int($source['trans_type'], 0);
	$document = dimension_source_revision_document_key($source['document_key']);
	$kind = isset($source['line_kind']) ? (string)$source['line_kind'] : '';
	$line = isset($source['line_key']) ? (string)$source['line_key'] : '';
	$revision = isset($source['revision_id'])
		? dimension_source_revision_int($source['revision_id'], 0) : 0;
	if ($type === false || !in_array($type, purchasing_classification_supported_types(), true)
		|| $document === false || !in_array($kind, array('header', 'line'), true)
		|| !dimension_source_revision_valid_line_key($kind, $line) || $revision === false)
		return false;
	return array('trans_type' => $type, 'document_key' => $document, 'line_kind' => $kind,
		'line_key' => $line, 'revision_id' => (int)$revision);
}

/**
 * Read one exact source descriptor, including its persisted resolver intents.
 *
 * @param array $source Validated descriptor
 * @return array Snapshot-shaped result
 */
function purchasing_classification_source_snapshot($source)
{
	$source = purchasing_classification_source_descriptor($source);
	if ($source === false)
		return array('state' => 'unknown', 'revision' => null, 'intents' => array(),
			'codes' => array('invalid_source'), 'errors' => array());
	if ($source['revision_id'] > 0) {
		$read = dimension_source_revision_read($source['revision_id']);
		if ($read['state'] !== 'available')
			return array('state' => $read['state'], 'revision' => null, 'intents' => array(),
				'codes' => $read['codes'], 'errors' => $read['errors']);
		$revision = $read['revision'];
		if ($revision['source_family'] !== 'purchasing'
			|| (int)$revision['trans_type'] !== (int)$source['trans_type']
			|| $revision['document_key'] !== $source['document_key']
			|| $revision['line_kind'] !== $source['line_kind']
			|| $revision['line_key'] !== $source['line_key'])
			return array('state' => 'tamper', 'revision' => null, 'intents' => array(),
				'codes' => array('source_mismatch'), 'errors' => array());
		if (!dimension_source_revision_policy_verified($revision))
			return array('state' => 'tamper', 'revision' => null, 'intents' => array(),
				'codes' => array('policy_version_unverified'), 'errors' => array());
		$intents = dimension_source_revision_row_intents($revision);
		if (!$intents['ok'])
			return array('state' => 'tamper', 'revision' => null, 'intents' => array(),
				'codes' => $intents['codes'], 'errors' => $intents['errors']);
		return array('state' => 'available', 'revision' => $revision,
			'intents' => $intents['intents'], 'codes' => array(), 'errors' => array());
	}
	return dimension_source_revision_snapshot('purchasing', $source['trans_type'],
		$source['document_key'], $source['line_kind'], $source['line_key']);
}

/**
 * Return one axis source entry from a verified source snapshot.
 *
 * @param array $snapshot Snapshot-shaped source read
 * @param int $axis Native axis
 * @return array Empty source set or line_snapshot/header_snapshot entry
 */
function purchasing_classification_snapshot_axis_source($snapshot, $axis)
{
	if (!is_array($snapshot) || !isset($snapshot['state']) || $snapshot['state'] !== 'available'
		|| !isset($snapshot['intents'][(int)$axis]))
		return array();
	return $snapshot['intents'][(int)$axis];
}

/**
 * Describe one source read for a sealed resolution payload.
 *
 * @param array|false $descriptor Source descriptor
 * @param array $snapshot Snapshot read
 * @return array
 */
function purchasing_classification_source_evidence($descriptor, $snapshot)
{
	if ($descriptor === false)
		return array('state' => 'none');
	$evidence = array('state' => isset($snapshot['state']) ? (string)$snapshot['state'] : 'unknown',
		'source' => $descriptor);
	if (isset($snapshot['revision']['source_revision_id']))
		$evidence['revision_id'] = (int)$snapshot['revision']['source_revision_id'];
	if (!empty($snapshot['codes']))
		$evidence['codes'] = array_values($snapshot['codes']);
	return $evidence;
}

/**
 * Return the in-memory header intent for one axis.
 *
 * A selected value recorded directly on a legacy carrier is adopted as explicit intent only for a
 * new event. Saved documents with no companion revision stay legacy-unspecified on read.
 *
 * @param object $cart Purchasing cart or source object
 * @param int $axis Native axis
 * @param bool $allow_native_fallback Adopt an existing native axis value
 * @return array|null Normalized intent
 */
function purchasing_classification_header_intent($cart, $axis, $allow_native_fallback)
{
	$intent = purchasing_classification_carried_intent($cart, $axis, 'purchasing_header');
	if ($intent !== null)
		return $intent;
	if (!$allow_native_fallback)
		return null;
	$field = $axis === 1 ? 'dimension' : 'dimension2';
	$value = is_object($cart) && isset($cart->$field) ? $cart->$field : 0;
	$identity = dimension_source_revision_int($value, 0);
	if ($identity === false)
		return array('ok' => false, 'intent' => null, 'codes' => array('invalid_identity'),
			'errors' => array(_('The purchasing header classification identity is invalid.')));
	return $identity > 0 ? dimension_resolution_selected($identity, 'purchasing_header_value')
		: dimension_resolution_inherit('purchasing_header');
}

/**
 * Return the in-memory line intent for one axis.
 *
 * @param object $line Native purchasing line
 * @param int $axis Native axis
 * @return array|null Normalized intent
 */
function purchasing_classification_line_intent($line, $axis)
{
	return purchasing_classification_carried_intent($line, $axis, 'purchasing_line');
}

/**
 * Build the cart's immediate purchase-order source from its retained conversion keys.
 *
 * Precedence follows the retained source specificity: an applied template, then the agreement that
 * authorizes the drawdown, then the awarded RFQ, then the originating requisition.
 *
 * @param object $cart Purchase order cart
 * @return array|false
 */
function purchasing_classification_native_header_source($cart)
{
	if (!is_object($cart))
		return false;
	$keys = array(
		'purch_template_id' => ST_PURCHTEMPLATE,
		'agreement_id' => ST_PURCHAGREEMENT,
		'rfq_id' => ST_PURCHRFQ,
		'requisition_id' => ST_PURCHREQUISITION);
	foreach ($keys as $field => $type) {
		$identity = isset($cart->$field) ? dimension_source_revision_int($cart->$field, 1) : false;
		if ($identity === false || $identity < 1)
			continue;
		$document = dimension_source_revision_document_key($identity);
		if ($document !== false)
			return array('trans_type' => $type, 'document_key' => $document,
				'line_kind' => 'header', 'line_key' => 'header', 'revision_id' => 0);
	}
	if (isset($cart->classification_source)
		&& ($source = purchasing_classification_source_descriptor($cart->classification_source)) !== false)
		return $source;
	return false;
}

/**
 * Build the line's immediate source descriptor when the cart retained one.
 *
 * @param object $cart Purchase order cart
 * @param object $line Purchase order line
 * @return array|false
 */
function purchasing_classification_native_line_source($cart, $line)
{
	if (is_object($line) && isset($line->classification_source)
		&& ($source = purchasing_classification_source_descriptor($line->classification_source)) !== false)
		return $source;
	return false;
}

/**
 * Add one normalized source to an axis source set.
 *
 * @param array $sources Source set
 * @param string $kind Source kind
 * @param array|null $intent Explicit intent
 * @return array
 */
function purchasing_classification_add_intent_source($sources, $kind, $intent)
{
	if (is_array($intent) && count($intent))
		$sources[$kind] = $intent;
	return $sources;
}

/**
 * Resolve both axes for one purchasing source entry and return its exact resolver evidence.
 *
 * @param object $cart Purchasing cart or source carrier
 * @param object|null $line Purchasing line, or null for a header
 * @param int $line_index Line index (diagnostic only, never an identity)
 * @param bool $new_line Whether an item default is eligible
 * @param array $header_source Header source descriptor
 * @param array $header_snapshot Header source snapshot
 * @param array $line_source Line source descriptor
 * @param array $line_snapshot Line source snapshot
 * @param string $origin Descriptive decision origin
 * @return array ok, pair, axes, resolution, errors and codes
 */
function purchasing_classification_resolve_entry($cart, $line, $line_index, $new_line,
	$header_source, $header_snapshot, $line_source, $line_snapshot, $origin = 'purchasing')
{
	$result = array('ok' => false, 'pair' => null, 'axes' => array(), 'resolution' => null,
		'errors' => array(), 'codes' => array());
	if (!is_object($cart) || !purchasing_classification_type_supported(
		isset($cart->trans_type) ? $cart->trans_type : 0)) {
		$result['codes'][] = 'unsupported_source';
		$result['errors'][] = _('This purchasing source cannot retain classification intent.');
		return $result;
	}
	$supplier = purchasing_classification_supplier_defaults(
		isset($cart->supplier_id) ? $cart->supplier_id : 0);
	$item = $line !== null && is_object($line)
		? purchasing_classification_item_defaults(isset($line->stock_id) ? $line->stock_id : '')
		: array(1 => 0, 2 => 0);
	$sources = array(1 => array(), 2 => array());
	$header_intent = array(1 => null, 2 => null);
	foreach (array(1, 2) as $axis) {
		$allow_native = !isset($header_snapshot['state']) || $header_snapshot['state'] !== 'available';
		$header_intent[$axis] = purchasing_classification_header_intent($cart, $axis, $allow_native);
		$own_line_snapshot = false;
		if ($line !== null) {
			$line_intent = purchasing_classification_line_intent($line, $axis);
			if (is_array($line_intent) && !empty($line_intent['intent']['persisted'])) {
				// A reloaded line already carries reviewed evidence; it outranks a conversion source
				// snapshot so an authorized amendment is not silently replaced by the source.
				$sources[$axis] = purchasing_classification_add_intent_source($sources[$axis],
					'line_snapshot', $line_intent);
				$own_line_snapshot = true;
			} else {
				$sources[$axis] = purchasing_classification_add_intent_source($sources[$axis],
					'line_explicit', $line_intent);
			}
		}
		if (!$own_line_snapshot)
			$sources[$axis] = purchasing_classification_add_intent_source($sources[$axis],
				'line_snapshot', purchasing_classification_snapshot_axis_source($line_snapshot, $axis));
		if (is_array($header_intent[$axis]) && !empty($header_intent[$axis]['intent']['persisted'])) {
			// The reviewed header evidence of this same document is the current decision.
			$sources[$axis] = purchasing_classification_add_intent_source($sources[$axis],
				'header_snapshot', $header_intent[$axis]);
		} else {
			$sources[$axis] = purchasing_classification_add_intent_source($sources[$axis],
				'header_explicit', $header_intent[$axis] === null
					? dimension_resolution_inherit($origin.'_header') : $header_intent[$axis]);
			$sources[$axis] = purchasing_classification_add_intent_source($sources[$axis],
				'header_snapshot', purchasing_classification_snapshot_axis_source($header_snapshot, $axis));
		}
		$sources[$axis]['partner_default'] = dimension_resolution_default_intent(
			$supplier[$axis], 'partner_default');
		if ($new_line)
			$sources[$axis]['item_default'] = dimension_resolution_default_intent(
				$item[$axis], 'item_default');
	}
	$enabled = (int)get_company_pref('use_dimension');
	$pair = dimension_resolution_pair($sources[1], $sources[2], array('enabled_axes' => $enabled));
	if (!$pair['ok']) {
		$result['codes'] = $pair['codes'];
		$result['errors'] = $pair['errors'];
		return $result;
	}
	$policy = function_exists('dimension_category_policy_current')
		? dimension_category_policy_current() : array('version' => 0);
	$policy_version = isset($policy['version']) ? (int)$policy['version'] : 0;
	foreach (array(1, 2) as $axis) {
		if ($pair['axes'][$axis]['mode'] !== 'inherit')
			continue;
		if ($axis > $enabled && $policy_version > 0) {
			$pair['axes'][$axis] = array_merge($pair['axes'][$axis], array(
				'value' => 0, 'mode' => 'not_applicable', 'origin' => 'policy',
				'persisted' => false, 'policy_version' => $policy_version,
				'basis' => 'axis_not_enabled'));
		} else {
			$pair['axes'][$axis] = array_merge($pair['axes'][$axis], array(
				'value' => 0, 'mode' => 'unassigned',
				'origin' => $axis > $enabled ? 'axis_disabled' : $origin.'_unassigned',
				'persisted' => false, 'policy_version' => 0, 'basis' => ''));
		}
	}
	$resolution = array(
		'contract' => 'purchasing-classification-resolution/1',
		'line_index' => (int)$line_index,
		'new_line' => (bool)$new_line,
		'enabled_axes' => $enabled,
		'policy_version' => (int)$pair['policy_version'],
		'axes' => array(1 => $pair['axes'][1], 2 => $pair['axes'][2]),
		'header_source' => purchasing_classification_source_evidence($header_source, $header_snapshot),
		'line_source' => purchasing_classification_source_evidence($line_source, $line_snapshot));
	$result['ok'] = true;
	$result['pair'] = $pair;
	$result['axes'] = $pair['axes'];
	$result['resolution'] = $resolution;
	return $result;
}

/**
 * Compare the durable classification-relevant bytes of one stored revision.
 *
 * @param array $current Current verified revision
 * @param array $fields Prepared candidate fields
 * @return bool
 */
function purchasing_classification_revision_equivalent($current, $fields)
{
	foreach (array('dimension_id', 'dimension2_id', 'mode1', 'mode2', 'origin1', 'origin2',
		'basis1', 'basis2', 'policy1_version', 'policy2_version', 'effective_date',
		'line_quantity') as $name) {
		$current_value = array_key_exists($name, $current) ? $current[$name] : null;
		if ((string)$current_value !== (string)$fields[$name])
			return false;
	}
	// Resolution metadata that describes how an entry was captured (a cart position, the first
	// capture of a new line, a draft binding note) is not part of the durable decision. Source
	// evidence and the resolved axes remain compared, so real lineage changes still append.
	$current_resolution = json_decode($current['resolution_payload'], true);
	$candidate_resolution = json_decode($fields['resolution_payload'], true);
	if (!is_array($current_resolution) || !is_array($candidate_resolution))
		return false;
	foreach (array('line_index', 'new_line', 'binding') as $volatile)
		unset($current_resolution[$volatile], $candidate_resolution[$volatile]);
	return json_encode($current_resolution) === json_encode($candidate_resolution);
}

/**
 * Persist one final/draft source revision, converging on an identical retry.
 *
 * @param int $trans_type Source type
 * @param string $document_key Final numeric key or durable draft key
 * @param string $line_kind header, line or draft_line
 * @param string $line_key Stable line key
 * @param array $resolution Resolved axes and source evidence
 * @param string $effective_date Canonical ISO date
 * @param string $line_quantity Canonical eight-decimal quantity
 * @return array ok, revision_id, revision and state
 */
function purchasing_classification_persist_revision($trans_type, $document_key, $line_kind, $line_key,
	$resolution, $effective_date, $line_quantity)
{
	$result = array('ok' => false, 'revision_id' => 0, 'revision' => null,
		'created' => false, 'errors' => array(), 'codes' => array());
	if (!is_array($resolution) || empty($resolution['axes'][1]) || empty($resolution['axes'][2])) {
		$result['codes'][] = 'invalid_resolution';
		$result['errors'][] = _('The purchasing classification resolution is incomplete.');
		return $result;
	}
	$prepared = dimension_source_revision_prepare('purchasing', $trans_type, $document_key, $line_kind,
		$line_key, $resolution['axes'], array('effective_date' => $effective_date,
			'line_quantity' => $line_quantity, 'resolution' => $resolution));
	if (!$prepared['ok']) {
		$result['codes'] = $prepared['codes'];
		$result['errors'] = $prepared['errors'];
		return $result;
	}
	$current = dimension_source_revision_snapshot('purchasing', $trans_type, $document_key,
		$line_kind, $line_key);
	if (!in_array($current['state'], array('available', 'absent', 'unavailable'), true)) {
		$result['codes'] = !empty($current['codes']) ? $current['codes'] : array('source_unreadable');
		$result['errors'] = !empty($current['errors']) ? $current['errors']
			: array(_('The current purchasing classification revision cannot be verified.'));
		return $result;
	}
	if ($current['state'] === 'available'
		&& purchasing_classification_revision_equivalent($current['revision'], $prepared['fields'])) {
		$result['ok'] = true;
		$result['revision_id'] = (int)$current['revision']['source_revision_id'];
		$result['revision'] = $current['revision'];
		return $result;
	}
	$options = array('effective_date' => $effective_date, 'line_quantity' => $line_quantity,
		'resolution' => $resolution);
	if ($current['state'] === 'available')
		$options['predecessor_revision_id'] = (int)$current['revision']['source_revision_id'];
	$prepared = dimension_source_revision_prepare('purchasing', $trans_type, $document_key, $line_kind,
		$line_key, $resolution['axes'], $options);
	if (!$prepared['ok']) {
		$result['codes'] = $prepared['codes'];
		$result['errors'] = $prepared['errors'];
		return $result;
	}
	$stored = dimension_source_revision_persist($prepared);
	if (!$stored['ok']) {
		$result['codes'] = $stored['codes'];
		$result['errors'] = $stored['errors'];
		return $result;
	}
	$result['ok'] = true;
	$result['revision_id'] = (int)$stored['revision_id'];
	$result['revision'] = $stored['revision'];
	$result['created'] = !empty($stored['created']);
	return $result;
}

/**
 * Is the accepted companion revision schema installed for the current company?
 *
 * An install that has not applied the accepted DIM-FND-006 upgrade keeps its legacy behavior: the
 * writer must not fail and no synthetic intent may be recorded. The caller marks the lineage
 * unavailable instead.
 *
 * @return bool
 */
function purchasing_classification_schema_ready()
{
	return dimension_source_revision_schema_ready() === true;
}

/**
 * Ensure one purchase order cart has stable draft identities for its header and lines.
 *
 * @param object $cart Purchase order cart
 * @return string Durable draft key
 */
function purchasing_classification_ensure_draft_identity($cart)
{
	if (!is_object($cart))
		throw new DimensionFinancialReferenceWriteException(_('The purchasing classification carrier is unavailable.'));
	if (!isset($cart->classification_draft_token) || !is_string($cart->classification_draft_token)
		|| $cart->classification_draft_token === '')
		$cart->classification_draft_token = purchasing_classification_token('purchdraft');
	$key = dimension_source_revision_draft_key($cart->classification_draft_token);
	if ($key === false)
		throw new DimensionFinancialReferenceWriteException(_('The purchasing draft classification identity is invalid.'));
	if (!isset($cart->line_items) || !is_array($cart->line_items))
		throw new DimensionFinancialReferenceWriteException(_('The purchasing classification lines are unavailable.'));
	foreach ($cart->line_items as $line) {
		if (!is_object($line))
			throw new DimensionFinancialReferenceWriteException(_('A purchasing classification line is invalid.'));
		if (!isset($line->classification_token) || !is_string($line->classification_token)
			|| $line->classification_token === '')
			$line->classification_token = purchasing_classification_token('line');
	}
	return $key;
}

/**
 * Capture a new draft source, or a current edited final source, for a purchase order cart.
 *
 * @param object $cart Purchase order cart
 * @param mixed $document_key Final source number, or null/zero for a draft
 * @param bool $final Whether regular line identities are already persisted
 * @return array capture summary
 */
function purchasing_classification_capture_cart($cart, $document_key = null, $final = false)
{
	if (!is_object($cart) || (int)(isset($cart->trans_type) ? $cart->trans_type : 0) !== ST_PURCHORDER)
		throw new DimensionFinancialReferenceWriteException(_('This purchasing document cannot retain classification intent.'));
	$draft_key = purchasing_classification_ensure_draft_identity($cart);
	$final_key = $final ? dimension_source_revision_document_key($document_key) : false;
	if ($final && $final_key === false)
		throw new DimensionFinancialReferenceWriteException(_('The saved purchasing source identity is invalid.'));
	$source_document_key = $final ? $final_key : $draft_key;
	$effective = isset($cart->orig_order_date) ? date2sql($cart->orig_order_date) : '';
	if (dimension_source_revision_date($effective) === false)
		throw new DimensionFinancialReferenceWriteException(_('The purchasing source date is invalid.'));
	$header_source = purchasing_classification_native_header_source($cart);
	$header_snapshot = $header_source === false ? array('state' => 'none')
		: purchasing_classification_source_snapshot($header_source);
	$header = purchasing_classification_resolve_entry($cart, null, -1, false,
		$header_source, $header_snapshot, false, array('state' => 'none'), 'purchasing');
	if (!$header['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $header['errors']));
	$cart->dimension = (int)$header['axes'][1]['value'];
	$cart->dimension2 = (int)$header['axes'][2]['value'];
	$header_capture = purchasing_classification_persist_revision(ST_PURCHORDER,
		$source_document_key, 'header', 'header', $header['resolution'], $effective, '0.00000000');
	if (!$header_capture['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $header_capture['errors']));
	$cart->classification_revision_id = (int)$header_capture['revision_id'];
	$cart->classification_lineage_state = 'versioned';
	$cart->classification_draft_revisions = array('document_key' => $source_document_key,
		'header' => (int)$header_capture['revision_id'], 'lines' => array());
	$line_result = array();
	foreach (array_values($cart->line_items) as $index => $line) {
		if (!is_object($line))
			throw new DimensionFinancialReferenceWriteException(_('A purchasing classification line is invalid.'));
		$line_source = purchasing_classification_native_line_source($cart, $line);
		$line_snapshot = $line_source === false ? array('state' => 'none')
			: purchasing_classification_source_snapshot($line_source);
		$is_new = !$final || empty($line->po_detail_rec);
		$line_kind = $final && !empty($line->po_detail_rec) ? 'line' : 'draft_line';
		$line_key = $line_kind === 'line'
			? dimension_source_revision_line_key('line', $line->po_detail_rec)
			: dimension_source_revision_line_key('draft_line', $line->classification_token);
		if ($line_key === false)
			throw new DimensionFinancialReferenceWriteException(_('A purchasing classification line identity is invalid.'));
		$resolved = purchasing_classification_resolve_entry($cart, $line, $index, $is_new,
			$header_source, $header_snapshot, $line_source, $line_snapshot, 'purchasing');
		if (!$resolved['ok'])
			throw new DimensionFinancialReferenceWriteException(implode(' ', $resolved['errors']));
		$stored = purchasing_classification_persist_revision(ST_PURCHORDER,
			$source_document_key, $line_kind, $line_key, $resolved['resolution'],
			$effective, purchasing_classification_quantity($line->quantity));
		if (!$stored['ok'])
			throw new DimensionFinancialReferenceWriteException(implode(' ', $stored['errors']));
		$line->classification_lineage_state = 'versioned';
		$line->draft_line_key = $line_kind === 'draft_line' ? $line_key : '';
		if ($line_kind === 'draft_line')
			$line->classification_draft_revision_id = (int)$stored['revision_id'];
		else
			$line->classification_revision_id = (int)$stored['revision_id'];
		$cart->classification_draft_revisions['lines'][$line->classification_token] = array(
			'revision_id' => (int)$stored['revision_id'], 'line_key' => $line_key);
		$line_result[$index] = (int)$stored['revision_id'];
	}
	return array('document_key' => $source_document_key, 'draft' => !$final,
		'header_revision_id' => (int)$header_capture['revision_id'],
		'line_revision_ids' => array_values($line_result),
		'cart' => $cart);
}

/**
 * Build the DIM-APR-001 request from a captured unsaved purchase order source.
 *
 * @param array $capture purchasing_classification_capture_cart() result
 * @return array|null
 */
function purchasing_classification_approval_request($capture)
{
	if (!is_array($capture) || empty($capture['draft']) || empty($capture['header_revision_id'])
		|| !isset($capture['line_revision_ids']) || !is_array($capture['line_revision_ids']))
		return null;
	$lines = array();
	foreach ($capture['line_revision_ids'] as $revision_id)
		$lines[] = array('revision_id' => (int)$revision_id);
	return array('source_family' => 'purchasing', 'document_key' => $capture['document_key'],
		'header' => array('revision_id' => (int)$capture['header_revision_id']),
		'lines' => $lines);
}

/**
 * Remove the sealed approval envelope from a payload before exact business-data comparison.
 *
 * @param mixed $data Draft payload
 * @return mixed
 */
function purchasing_classification_without_approval_envelope($data)
{
	if (!is_array($data))
		return $data;
	unset($data[approval_dimension_snapshot_payload_key()]);
	return $data;
}

/**
 * Remove request/capture metadata that is expected to change while a source is written.
 *
 * Business fields, stock ids, quantities, prices and stable line tokens remain in the comparison.
 *
 * @param mixed $data Purchasing source payload
 * @return mixed
 */
function purchasing_classification_without_volatile_metadata($data)
{
	if (!is_array($data))
		return $data;
	foreach (array('classification_intents','classification_draft_token',
		'classification_draft_revisions','classification_revision_id',
		'classification_lineage_state','classification_source') as $field)
		unset($data[$field]);
	if (isset($data['line_items']) && is_array($data['line_items'])) {
		foreach ($data['line_items'] as $index => $line) {
			if (!is_array($line))
				continue;
			foreach (array('classification_intents','classification_draft_revision_id',
				'classification_revision_id','classification_lineage_state',
				'classification_source','classification_token','draft_line_key') as $field)
				unset($data['line_items'][$index][$field]);
		}
	}
	return $data;
}

/**
 * Bind every captured draft revision to the final purchase order identity.
 *
 * @param object $cart Written purchase order cart
 * @param mixed $document_no Final native number
 * @return bool
 */
function purchasing_classification_bind_cart($cart, $document_no)
{
	if (!is_object($cart) || !isset($cart->classification_draft_revisions)
		|| !is_array($cart->classification_draft_revisions)
		|| empty($cart->classification_draft_revisions['document_key']))
		return true;
	$document = dimension_source_revision_document_key($document_no);
	if ($document === false)
		return false;
	$capture = $cart->classification_draft_revisions;
	$header_id = isset($capture['header']) ? dimension_source_revision_int($capture['header'], 1) : false;
	if ($header_id === false)
		return false;
	$header = dimension_source_revision_read($header_id);
	if ($header['state'] !== 'available')
		return false;
	$resolution = json_decode($header['revision']['resolution_payload'], true);
	if (!is_array($resolution))
		return false;
	$resolution['binding'] = array('document_key' => $document, 'line_kind' => 'header',
		'line_key' => 'header');
	$bound = dimension_source_revision_bind($header_id, array('document_key' => $document,
		'line_kind' => 'header', 'line_key' => 'header', 'resolution' => $resolution,
		'line_quantity' => '0.00000000'));
	if (!$bound['ok'])
		return false;
	$cart->classification_revision_id = (int)$bound['revision_id'];
	$cart->classification_lineage_state = 'versioned';
	foreach ($cart->line_items as $line) {
		$token = is_object($line) && isset($line->classification_token)
			? (string)$line->classification_token : '';
		if ($token === '' || !isset($capture['lines'][$token]))
			return false;
		$draft_id = dimension_source_revision_int($capture['lines'][$token]['revision_id'], 1);
		$line_id = is_object($line) && isset($line->po_detail_rec)
			? dimension_source_revision_int($line->po_detail_rec, 1) : false;
		$line_key = $line_id === false ? false
			: dimension_source_revision_line_key('line', $line_id);
		if ($draft_id === false || $line_key === false)
			return false;
		$draft = dimension_source_revision_read($draft_id);
		if ($draft['state'] !== 'available')
			return false;
		$line_resolution = json_decode($draft['revision']['resolution_payload'], true);
		if (!is_array($line_resolution))
			return false;
		$line_resolution['binding'] = array('document_key' => $document, 'line_kind' => 'line',
			'line_key' => $line_key);
		$line_bound = dimension_source_revision_bind($draft_id, array('document_key' => $document,
			'line_kind' => 'line', 'line_key' => $line_key, 'resolution' => $line_resolution,
			'line_quantity' => purchasing_classification_quantity($line->quantity)));
		if (!$line_bound['ok'])
			return false;
		$line->classification_revision_id = (int)$line_bound['revision_id'];
		$line->classification_lineage_state = 'versioned';
	}
	return true;
}

/**
 * Load the durable header classification of one saved purchase order.
 *
 * @param object $cart Purchase order cart
 * @param int $trans_type Stored type
 * @param int $number Stored document number
 * @return string header lineage state
 */
function purchasing_classification_load_header($cart, $trans_type, $number)
{
	$document = dimension_source_revision_document_key($number);
	if ($document === false || !is_object($cart))
		return 'unknown';
	$snapshot = dimension_source_revision_snapshot('purchasing', $trans_type, $document,
		'header', 'header');
	if ($snapshot['state'] === 'available') {
		dimension_source_revision_apply($cart, $snapshot);
		$cart->classification_revision_id = (int)$snapshot['revision']['source_revision_id'];
		$cart->classification_lineage_state = 'versioned';
	} elseif ($snapshot['state'] === 'absent') {
		$cart->classification_lineage_state = 'legacy_unspecified';
	} else {
		$cart->classification_lineage_state = $snapshot['state'];
	}
	return $cart->classification_lineage_state;
}

/**
 * Load the durable line classifications of one saved purchase order.
 *
 * @param object $cart Purchase order cart
 * @param int $trans_type Stored type
 * @param int $number Stored document number
 * @return void
 */
function purchasing_classification_load_lines($cart, $trans_type, $number)
{
	$document = dimension_source_revision_document_key($number);
	if ($document === false || !is_object($cart) || !isset($cart->line_items)
		|| !is_array($cart->line_items))
		return;
	foreach ($cart->line_items as $line) {
		$line_id = is_object($line) && isset($line->po_detail_rec)
			? dimension_source_revision_int($line->po_detail_rec, 1) : false;
		if ($line_id === false)
			continue;
		$line_key = dimension_source_revision_line_key('line', $line_id);
		$line_snapshot = dimension_source_revision_snapshot('purchasing', $trans_type,
			$document, 'line', $line_key);
		if ($line_snapshot['state'] === 'available') {
			dimension_source_revision_apply($line, $line_snapshot);
			$line->classification_revision_id =
				(int)$line_snapshot['revision']['source_revision_id'];
			$line->classification_lineage_state = 'versioned';
		} else {
			$line->classification_lineage_state = $line_snapshot['state'] === 'absent'
				? 'legacy_unspecified' : $line_snapshot['state'];
		}
	}
}

/**
 * Load the current durable classification of one saved purchase order.
 *
 * Missing lineage is reported as legacy-unspecified and is never reconstructed from a current
 * supplier or item default.
 *
 * @param object $cart Purchase order cart
 * @param int $trans_type Stored type
 * @param int $number Stored document number
 * @return string header lineage state
 */
function purchasing_classification_load_order($cart, $trans_type, $number)
{
	$state = purchasing_classification_load_header($cart, $trans_type, $number);
	purchasing_classification_load_lines($cart, $trans_type, $number);
	return $state;
}

/**
 * Load one saved document header's durable intents for an editing form.
 *
 * @param int $trans_type Stored source type
 * @param int $document_no Native identity
 * @return array|null axis => normalized intent
 */
function purchasing_classification_load_header_intents($trans_type, $document_no)
{
	$document = dimension_source_revision_document_key($document_no);
	if ($document === false || !purchasing_classification_type_supported($trans_type))
		return null;
	$snapshot = dimension_source_revision_snapshot('purchasing', $trans_type, $document,
		'header', 'header');
	if ($snapshot['state'] !== 'available')
		return null;
	$intents = array();
	foreach (array(1, 2) as $axis)
		if (isset($snapshot['intents'][$axis]['intent']))
			$intents[$axis] = $snapshot['intents'][$axis]['intent'];
	return $intents ? $intents : null;
}

/**
 * Load one saved document line's durable intents for an editing form.
 *
 * @param int $trans_type Stored source type
 * @param int $document_no Parent identity
 * @param int $line_id Native line identity
 * @return array|null axis => normalized intent
 */
function purchasing_classification_load_line_intents($trans_type, $document_no, $line_id)
{
	$document = dimension_source_revision_document_key($document_no);
	$line = dimension_source_revision_int($line_id, 1);
	if ($document === false || $line === false
		|| !purchasing_classification_type_supported($trans_type))
		return null;
	$snapshot = dimension_source_revision_snapshot('purchasing', $trans_type, $document, 'line',
		dimension_source_revision_line_key('line', $line));
	if ($snapshot['state'] !== 'available')
		return null;
	$intents = array();
	foreach (array(1, 2) as $axis)
		if (isset($snapshot['intents'][$axis]['intent']))
			$intents[$axis] = $snapshot['intents'][$axis]['intent'];
	return $intents ? $intents : null;
}

/**
 * Read the current header revision identity of one source, or zero when none exists.
 *
 * @param int $trans_type Stored source type
 * @param int $document_no Native identity
 * @return int
 */
function purchasing_classification_current_header_revision_id($trans_type, $document_no)
{
	$document = dimension_source_revision_document_key($document_no);
	if ($document === false || !purchasing_classification_type_supported($trans_type))
		return 0;
	$snapshot = dimension_source_revision_snapshot('purchasing', $trans_type, $document,
		'header', 'header');
	return $snapshot['state'] === 'available'
		? (int)$snapshot['revision']['source_revision_id'] : 0;
}

/**
 * Read the current line revision identity of one source line, or zero when none exists.
 *
 * @param int $trans_type Stored source type
 * @param int $document_no Parent identity
 * @param int $line_id Native line identity
 * @return int
 */
function purchasing_classification_current_line_revision_id($trans_type, $document_no, $line_id)
{
	$document = dimension_source_revision_document_key($document_no);
	$line = dimension_source_revision_int($line_id, 1);
	if ($document === false || $line === false
		|| !purchasing_classification_type_supported($trans_type))
		return 0;
	$snapshot = dimension_source_revision_snapshot('purchasing', $trans_type, $document, 'line',
		dimension_source_revision_line_key('line', $line));
	return $snapshot['state'] === 'available'
		? (int)$snapshot['revision']['source_revision_id'] : 0;
}

/**
 * Build a synthetic carrier for a persisted non-order purchasing document.
 *
 * @param int $trans_type Stored source type
 * @param int $document_no Native identity
 * @param array|false $source Additional source descriptor
 * @return object|false
 */
function purchasing_classification_document_carrier($trans_type, $document_no, $source = false)
{
	$document = dimension_source_revision_document_key($document_no);
	if ($document === false || !purchasing_classification_type_supported($trans_type))
		return false;
	$type = (int)$trans_type;
	$carrier = (object)array('trans_type' => $type, 'supplier_id' => 0,
		'dimension' => 0, 'dimension2' => 0, 'classification_intents' => null,
		'classification_source' => false, 'line_items' => array());
	$table = false;
	$fields = 'id';
	switch ($type) {
		case ST_PURCHREQUISITION:
			$table = 'purch_requisitions';
			$fields = 'id,requester_id,request_date,dimension_id,dimension2_id';
			break;
		case ST_PURCHRFQ:
			$table = 'purch_rfq';
			$fields = 'id,created_by,created_date,dimension_id,dimension2_id';
			break;
		case ST_PURCHAGREEMENT:
			$table = 'purch_agreements';
			$fields = 'id,supplier_id,date_start,dimension_id,dimension2_id';
			break;
		case ST_PURCHTEMPLATE:
			$table = 'purch_order_templates';
			$fields = 'id,supplier_id,inactive';
			break;
		case ST_PROCUREMENTPLAN:
			$table = 'procurement_plan';
			$fields = 'id,plan_date';
			break;
	}
	if ($table === false)
		return false;
	$rows = db_query('SELECT '.$fields.' FROM '.TB_PREF.$table.' WHERE id='.(int)$document_no);
	if (!$rows)
		throw new DimensionFinancialReferenceWriteException(
			_('The purchasing classification source cannot be read.'));
	$row = db_fetch_assoc($rows);
	if (!$row)
		throw new DimensionFinancialReferenceWriteException(
			_('The purchasing classification source is unavailable.'));
	$carrier->dimension = isset($row['dimension_id']) ? (int)$row['dimension_id'] : 0;
	$carrier->dimension2 = isset($row['dimension2_id']) ? (int)$row['dimension2_id'] : 0;
	$carrier->supplier_id = isset($row['supplier_id']) ? (int)$row['supplier_id'] : 0;
	$date = '';
	switch ($type) {
		case ST_PURCHREQUISITION:
			$date = $row['request_date'];
			break;
		case ST_PURCHRFQ:
			$date = $row['created_date'];
			break;
		case ST_PURCHAGREEMENT:
			$date = $row['date_start'];
			break;
		case ST_PROCUREMENTPLAN:
			$date = $row['plan_date'];
			break;
	}
	if ($source !== false) {
		$descriptor = purchasing_classification_source_descriptor($source);
		if ($descriptor !== false)
			$carrier->classification_source = $descriptor;
	}
	$carrier->classification_effective_date = $date;
	return $carrier;
}

/**
 * Capture one persisted non-order purchasing header and optional line.
 *
 * The caller has already written the native row inside its own transaction; both revisions are
 * appended to that same transaction through the durable authority.
 *
 * @param int $trans_type Stored source type
 * @param int $document_no Native identity
 * @param array|null $header_intents axis => normalized intent, or null to keep native values
 * @param array|null $line Native line row for line capture
 * @param array|null $line_intents axis => normalized intent
 * @param array|false $source Additional source descriptor
 * @return array header_revision_id and line_revision_id
 */
function purchasing_classification_capture_document($trans_type, $document_no, $header_intents,
	$line = null, $line_intents = null, $source = false)
{
	$document = dimension_source_revision_document_key($document_no);
	if ($document === false || !purchasing_classification_type_supported($trans_type)
		|| (int)$trans_type === ST_PURCHORDER)
		throw new DimensionFinancialReferenceWriteException(_('The purchasing source identity is invalid.'));
	$carrier = purchasing_classification_document_carrier($trans_type, $document_no, $source);
	if ($carrier === false)
		throw new DimensionFinancialReferenceWriteException(_('The purchasing source is unavailable.'));
	$carrier->classification_intents = $header_intents;
	$effective = isset($carrier->classification_effective_date)
		? $carrier->classification_effective_date : '';
	if (dimension_source_revision_date($effective) === false)
		$effective = date2sql(Today());
	$header_source = $source === false ? false : purchasing_classification_source_descriptor($source);
	$header_snapshot = $header_source === false ? array('state' => 'none')
		: purchasing_classification_source_snapshot($header_source);
	$header = purchasing_classification_resolve_entry($carrier, null, -1, false,
		$header_source, $header_snapshot, false, array('state' => 'none'), 'purchasing');
	if (!$header['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $header['errors']));
	$header_capture = purchasing_classification_persist_revision((int)$trans_type, $document,
		'header', 'header', $header['resolution'], $effective, '0.00000000');
	if (!$header_capture['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $header_capture['errors']));
	if ($line === null)
		return array('header_revision_id' => (int)$header_capture['revision_id'],
			'line_revision_id' => 0);
	$line_id = isset($line['id']) ? dimension_source_revision_int($line['id'], 1) : false;
	if ($line_id === false)
		throw new DimensionFinancialReferenceWriteException(_('The purchasing line identity is invalid.'));
	$line_carrier = (object)array('id' => $line_id,
		'stock_id' => isset($line['stock_id']) ? $line['stock_id'] : '',
		'quantity' => isset($line['quantity']) ? $line['quantity']
			: (isset($line['committed_qty']) ? $line['committed_qty']
				: (isset($line['default_quantity']) ? $line['default_quantity'] : 0)),
		'classification_intents' => $line_intents);
	$line_key = dimension_source_revision_line_key('line', $line_id);
	$existing = dimension_source_revision_snapshot('purchasing', $trans_type, $document, 'line',
		$line_key);
	$line_source = $existing['state'] === 'available' ? array(
		'trans_type' => (int)$trans_type, 'document_key' => $document, 'line_kind' => 'line',
		'line_key' => $line_key, 'revision_id' => (int)$existing['revision']['source_revision_id'])
		: false;
	// The line inherits the exact header revision this command just persisted, not a current default.
	$document_header_source = array('trans_type' => (int)$trans_type, 'document_key' => $document,
		'line_kind' => 'header', 'line_key' => 'header',
		'revision_id' => (int)$header_capture['revision_id']);
	$document_header_snapshot = purchasing_classification_source_snapshot($document_header_source);
	$line_resolved = purchasing_classification_resolve_entry($carrier, $line_carrier, (int)$line_id,
		false, $document_header_source, $document_header_snapshot, $line_source, $existing,
		'purchasing');
	if (!$line_resolved['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $line_resolved['errors']));
	$line_capture = purchasing_classification_persist_revision((int)$trans_type, $document, 'line',
		$line_key, $line_resolved['resolution'], $effective,
		purchasing_classification_quantity($line_carrier->quantity));
	if (!$line_capture['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $line_capture['errors']));
	return array('header_revision_id' => (int)$header_capture['revision_id'],
		'line_revision_id' => (int)$line_capture['revision_id']);
}

/**
 * Capture one purchase-template header or line after its native row exists.
 *
 * @param int $template_id Template identity
 * @param array|null $header_intents axis => normalized intent
 * @param array|null $line Template line row
 * @param array|null $line_intents axis => normalized intent
 * @return array
 */
function purchasing_classification_capture_template($template_id, $header_intents, $line = null,
	$line_intents = null)
{
	if (dimension_source_revision_document_key($template_id) === false)
		throw new DimensionFinancialReferenceWriteException(_('The purchase template identity is invalid.'));
	$row = array('id' => (int)$template_id, 'inactive' => 0);
	$rows = db_query('SELECT id,supplier_id,inactive FROM '.TB_PREF.'purch_order_templates WHERE id='
		.(int)$template_id);
	$template = $rows ? db_fetch_assoc($rows) : false;
	$source = $template ? array('trans_type' => ST_PURCHTEMPLATE,
		'document_key' => dimension_source_revision_document_key($template_id), 'line_kind' => 'header',
		'line_key' => 'header', 'revision_id' => 0) : false;
	$carrier = (object)array('trans_type' => ST_PURCHTEMPLATE,
		'supplier_id' => $template ? (int)$template['supplier_id'] : 0,
		'dimension' => 0, 'dimension2' => 0, 'classification_intents' => $header_intents,
		'classification_source' => $source, 'line_items' => array());
	$effective = date2sql(Today());
	$header_snapshot = array('state' => 'none');
	$header = purchasing_classification_resolve_entry($carrier, null, -1, false, $source,
		$header_snapshot, false, $header_snapshot, 'purchasing');
	if (!$header['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $header['errors']));
	$header_capture = purchasing_classification_persist_revision(ST_PURCHTEMPLATE,
		dimension_source_revision_document_key($template_id), 'header', 'header',
		$header['resolution'], $effective, '0.00000000');
	if (!$header_capture['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $header_capture['errors']));
	if ($line === null)
		return array('header_revision_id' => (int)$header_capture['revision_id'],
			'line_revision_id' => 0);
	$line_id = isset($line['id']) ? dimension_source_revision_int($line['id'], 1) : false;
	if ($line_id === false)
		throw new DimensionFinancialReferenceWriteException(_('The purchase template line identity is invalid.'));
	$document = dimension_source_revision_document_key($template_id);
	$line_key = dimension_source_revision_line_key('line', $line_id);
	$line_carrier = (object)array('id' => $line_id,
		'stock_id' => isset($line['stock_id']) ? $line['stock_id'] : '',
		'quantity' => isset($line['default_quantity']) ? $line['default_quantity']
			: (isset($line['quantity']) ? $line['quantity'] : 0),
		'classification_intents' => $line_intents);
	$existing = dimension_source_revision_snapshot('purchasing', ST_PURCHTEMPLATE, $document,
		'line', $line_key);
	$line_source = $existing['state'] === 'available' ? array(
		'trans_type' => ST_PURCHTEMPLATE, 'document_key' => $document, 'line_kind' => 'line',
		'line_key' => $line_key, 'revision_id' => (int)$existing['revision']['source_revision_id'])
		: false;
	$header_source = array('trans_type' => ST_PURCHTEMPLATE,
		'document_key' => $document, 'line_kind' => 'header', 'line_key' => 'header',
		'revision_id' => (int)$header_capture['revision_id']);
	$header_snapshot = purchasing_classification_source_snapshot($header_source);
	$line_resolved = purchasing_classification_resolve_entry($carrier, $line_carrier,
		(int)$line_id, false, $header_source, $header_snapshot, $line_source, $existing, 'purchasing');
	if (!$line_resolved['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $line_resolved['errors']));
	$line_capture = purchasing_classification_persist_revision(ST_PURCHTEMPLATE, $document, 'line',
		$line_key, $line_resolved['resolution'], $effective,
		purchasing_classification_quantity($line_carrier->quantity));
	if (!$line_capture['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $line_capture['errors']));
	return array('header_revision_id' => (int)$header_capture['revision_id'],
		'line_revision_id' => (int)$line_capture['revision_id']);
}

/**
 * Capture only one persisted non-order purchasing line under its current header revision.
 *
 * @param int $trans_type Stored source type
 * @param int $document_no Parent identity
 * @param array $line Native line row
 * @param array|null $line_intents axis => normalized intent
 * @return int Persisted line revision identity, or zero
 */
function purchasing_classification_capture_document_line($trans_type, $document_no, $line,
	$line_intents = null)
{
	$document = dimension_source_revision_document_key($document_no);
	if ($document === false || !purchasing_classification_type_supported($trans_type)
		|| (int)$trans_type === ST_PURCHORDER)
		return 0;
	$header_revision = purchasing_classification_current_header_revision_id($trans_type, $document_no);
	if (!$header_revision) {
		$header = purchasing_classification_capture_document($trans_type, $document_no, null);
		$header_revision = isset($header['header_revision_id']) ? (int)$header['header_revision_id'] : 0;
	}
	if (!$header_revision)
		return 0;
	$carrier = purchasing_classification_document_carrier($trans_type, $document_no, false);
	if ($carrier === false)
		return 0;
	$line_id = isset($line['id']) ? dimension_source_revision_int($line['id'], 1) : false;
	if ($line_id === false)
		throw new DimensionFinancialReferenceWriteException(_('The purchasing line identity is invalid.'));
	$effective = isset($carrier->classification_effective_date)
		? $carrier->classification_effective_date : '';
	if (dimension_source_revision_date($effective) === false)
		$effective = date2sql(Today());
	$line_carrier = (object)array('id' => $line_id,
		'stock_id' => isset($line['stock_id']) ? $line['stock_id'] : '',
		'quantity' => isset($line['quantity']) ? $line['quantity']
			: (isset($line['committed_qty']) ? $line['committed_qty']
				: (isset($line['default_quantity']) ? $line['default_quantity'] : 0)),
		'classification_intents' => $line_intents);
	$line_key = dimension_source_revision_line_key('line', $line_id);
	$existing = dimension_source_revision_snapshot('purchasing', $trans_type, $document, 'line',
		$line_key);
	$line_source = $existing['state'] === 'available' ? array(
		'trans_type' => (int)$trans_type, 'document_key' => $document, 'line_kind' => 'line',
		'line_key' => $line_key, 'revision_id' => (int)$existing['revision']['source_revision_id'])
		: false;
	$header_source = array('trans_type' => (int)$trans_type, 'document_key' => $document,
		'line_kind' => 'header', 'line_key' => 'header', 'revision_id' => (int)$header_revision);
	$header_snapshot = purchasing_classification_source_snapshot($header_source);
	$resolved = purchasing_classification_resolve_entry($carrier, $line_carrier, (int)$line_id, false,
		$header_source, $header_snapshot, $line_source, $existing, 'purchasing');
	if (!$resolved['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $resolved['errors']));
	$stored = purchasing_classification_persist_revision((int)$trans_type, $document, 'line',
		$line_key, $resolved['resolution'], $effective,
		purchasing_classification_quantity($line_carrier->quantity));
	if (!$stored['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $stored['errors']));
	return (int)$stored['revision_id'];
}

/**
 * Capture only one purchase-template line under its current header revision.
 *
 * @param int $template_id Template identity
 * @param array $line Template line row
 * @param array|null $line_intents axis => normalized intent
 * @return int Persisted line revision identity, or zero
 */
function purchasing_classification_capture_template_line($template_id, $line, $line_intents = null)
{
	$document = dimension_source_revision_document_key($template_id);
	if ($document === false)
		return 0;
	$header_revision = purchasing_classification_current_header_revision_id(ST_PURCHTEMPLATE,
		$template_id);
	if (!$header_revision) {
		$header = purchasing_classification_capture_template($template_id, null);
		$header_revision = isset($header['header_revision_id']) ? (int)$header['header_revision_id'] : 0;
	}
	if (!$header_revision)
		return 0;
	$rows = db_query('SELECT id,supplier_id,inactive FROM '.TB_PREF.'purch_order_templates WHERE id='
		.(int)$template_id);
	$template = $rows ? db_fetch_assoc($rows) : false;
	if (!$template)
		return 0;
	$carrier = (object)array('trans_type' => ST_PURCHTEMPLATE,
		'supplier_id' => (int)$template['supplier_id'], 'dimension' => 0, 'dimension2' => 0,
		'classification_intents' => null, 'classification_source' => false, 'line_items' => array());
	$line_id = isset($line['id']) ? dimension_source_revision_int($line['id'], 1) : false;
	if ($line_id === false)
		throw new DimensionFinancialReferenceWriteException(_('The purchase template line identity is invalid.'));
	$line_key = dimension_source_revision_line_key('line', $line_id);
	$existing = dimension_source_revision_snapshot('purchasing', ST_PURCHTEMPLATE, $document,
		'line', $line_key);
	$line_source = $existing['state'] === 'available' ? array(
		'trans_type' => ST_PURCHTEMPLATE, 'document_key' => $document, 'line_kind' => 'line',
		'line_key' => $line_key, 'revision_id' => (int)$existing['revision']['source_revision_id'])
		: false;
	$header_source = array('trans_type' => ST_PURCHTEMPLATE, 'document_key' => $document,
		'line_kind' => 'header', 'line_key' => 'header', 'revision_id' => (int)$header_revision);
	$header_snapshot = purchasing_classification_source_snapshot($header_source);
	$line_carrier = (object)array('id' => $line_id,
		'stock_id' => isset($line['stock_id']) ? $line['stock_id'] : '',
		'quantity' => isset($line['default_quantity']) ? $line['default_quantity']
			: (isset($line['quantity']) ? $line['quantity'] : 0),
		'classification_intents' => $line_intents);
	$resolved = purchasing_classification_resolve_entry($carrier, $line_carrier, (int)$line_id, false,
		$header_source, $header_snapshot, $line_source, $existing, 'purchasing');
	if (!$resolved['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $resolved['errors']));
	$stored = purchasing_classification_persist_revision(ST_PURCHTEMPLATE, $document, 'line',
		$line_key, $resolved['resolution'], date2sql(Today()),
		purchasing_classification_quantity($line_carrier->quantity));
	if (!$stored['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $stored['errors']));
	return (int)$stored['revision_id'];
}

/**
 * Attach one persisted document header's intents and source identity to a cart or form carrier.
 *
 * @param object $cart Purchase order cart
 * @param int $trans_type Source type
 * @param int $document_no Source identity
 * @return bool
 */
function purchasing_classification_apply_header_source($cart, $trans_type, $document_no)
{
	$document = dimension_source_revision_document_key($document_no);
	if (!is_object($cart) || $document === false || !purchasing_classification_type_supported($trans_type))
		return false;
	$snapshot = dimension_source_revision_snapshot('purchasing', $trans_type, $document, 'header', 'header');
	if ($snapshot['state'] === 'available')
		dimension_source_revision_apply($cart, $snapshot);
	$cart->classification_source = array('trans_type' => (int)$trans_type,
		'document_key' => $document, 'line_kind' => 'header', 'line_key' => 'header',
		'revision_id' => $snapshot['state'] === 'available'
			? (int)$snapshot['revision']['source_revision_id'] : 0);
	return true;
}

/**
 * Attach one source line's persisted intents to a purchase order line and record its source.
 *
 * @param object $line Purchase order line
 * @param int $trans_type Source type
 * @param int $document_no Source identity
 * @param int $line_id Source line identity
 * @return bool
 */
function purchasing_classification_apply_line_source($line, $trans_type, $document_no, $line_id)
{
	$document = dimension_source_revision_document_key($document_no);
	$source_line = dimension_source_revision_int($line_id, 1);
	if (!is_object($line) || $document === false || $source_line === false
		|| !purchasing_classification_type_supported($trans_type))
		return false;
	$line_key = dimension_source_revision_line_key('line', $source_line);
	$snapshot = dimension_source_revision_snapshot('purchasing', $trans_type, $document, 'line', $line_key);
	if ($snapshot['state'] === 'available')
		dimension_source_revision_apply($line, $snapshot);
	$line->classification_source = array('trans_type' => (int)$trans_type,
		'document_key' => $document, 'line_kind' => 'line', 'line_key' => $line_key,
		'revision_id' => $snapshot['state'] === 'available'
			? (int)$snapshot['revision']['source_revision_id'] : 0);
	return true;
}

/**
 * Attach one purchase template's header and line intent to a purchase order cart.
 *
 * @param object $cart Purchase order cart
 * @param int $template_id Template identity
 * @param array $line_map template line id => cart line index
 * @return bool
 */
function purchasing_classification_apply_template($cart, $template_id, $line_map)
{
	if (!purchasing_classification_apply_header_source($cart, ST_PURCHTEMPLATE, $template_id))
		return false;
	foreach ((array)$line_map as $template_line_id => $line_index) {
		if (!isset($cart->line_items[$line_index]) || !is_object($cart->line_items[$line_index]))
			continue;
		purchasing_classification_apply_line_source($cart->line_items[$line_index], ST_PURCHTEMPLATE,
			$template_id, $template_line_id);
	}
	return true;
}

/**
 * Attach one persisted source document's header and line intent to a conversion cart.
 *
 * @param object $cart Purchase order cart
 * @param int $trans_type Source type
 * @param int $document_no Source identity
 * @param array $line_map source line id => cart line index
 * @return bool
 */
function purchasing_classification_apply_conversion($cart, $trans_type, $document_no, $line_map)
{
	if (!purchasing_classification_apply_header_source($cart, $trans_type, $document_no))
		return false;
	foreach ((array)$line_map as $source_line_id => $line_index) {
		if (!isset($cart->line_items[$line_index]) || !is_object($cart->line_items[$line_index]))
			continue;
		purchasing_classification_apply_line_source($cart->line_items[$line_index], $trans_type,
			$document_no, $source_line_id);
	}
	return true;
}

/**
 * Create an axis=>intent carrier from posted native fields.
 *
 * @param mixed $id1 Axis 1 value
 * @param mixed $id2 Axis 2 value
 * @param mixed $blank1 Explicit-unassigned axis 1 marker
 * @param mixed $blank2 Explicit-unassigned axis 2 marker
 * @param string $origin Decision origin
 * @return array|false
 */
function purchasing_classification_intents_from_values($id1, $id2, $blank1, $blank2, $origin)
{
	$result = array();
	foreach (array(1 => array($id1, $blank1), 2 => array($id2, $blank2)) as $axis => $values) {
		$intent = purchasing_classification_intent_from_values($values[0], $values[1], $axis, $origin);
		if (is_array($intent)) {
			if (!$intent['ok'])
				return false;
			$result[$axis] = $intent['intent'];
		} else {
			$result[$axis] = dimension_resolution_inherit($origin)['intent'];
		}
	}
	return $result;
}

/**
 * Return display/form values for one stored intent carrier.
 *
 * @param object|array $carrier Cart/line carrying classification_intents
 * @return array ids and unassigned flags keyed by axis
 */
function purchasing_classification_carrier_values($carrier)
{
	$result = array('ids' => array(1 => 0, 2 => 0),
		'unassigned' => array(1 => 0, 2 => 0));
	foreach (array(1, 2) as $axis) {
		$intent = purchasing_classification_carried_intent($carrier, $axis, 'purchasing_display');
		if ($intent === null || !$intent['ok'])
			continue;
		$result['ids'][$axis] = $intent['intent']['mode'] === 'selected'
			? (int)$intent['intent']['id'] : 0;
		$result['unassigned'][$axis] = $intent['intent']['mode'] === 'unassigned' ? 1 : 0;
	}
	return $result;
}
