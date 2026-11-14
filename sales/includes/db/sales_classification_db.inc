<?php
/**
 * DIM-SAL-001: durable quotation, order, agreement and template classification intent.
 *
 * This adapter is deliberately small: it resolves the Sales header/line intent through the shared
 * DIM-FND-005/DIM-FND-004 contracts and stores it through DIM-FND-006. It never writes a ledger
 * row, invents a default for an unsnapshotted legacy document, or replaces the native document
 * tables with a second source of business truth.
 *
 * PHP 5.6 compatible.
 */
require_once __DIR__.'/../../../dimensions/includes/dimension_source_revision.inc';
require_once __DIR__.'/../../../dimensions/includes/dimension_category_policy.inc';

// A quotation template is a source owner but is not a posted transaction type. Keeping a distinct
// source type prevents a template id from colliding with a quotation number in the same family.
if (!defined('ST_SALESQUOTE_TEMPLATE'))
	define('ST_SALESQUOTE_TEMPLATE', 65);

/**
 * Transaction/source types owned by this adapter.
 *
 * @return int[]
 */
function sales_classification_supported_types()
{
	return array(ST_SALESORDER, ST_SALESQUOTE, ST_SALESAGREEMENT, ST_SALESQUOTE_TEMPLATE);
}

/**
 * Is one type a supported Sales classification source?
 *
 * @param mixed $trans_type Stored source type
 * @return bool
 */
function sales_classification_type_supported($trans_type)
{
	$type = dimension_source_revision_int($trans_type, 0);
	return $type !== false && in_array($type, sales_classification_supported_types(), true);
}

/**
 * Build a bounded opaque token accepted by the durable revision authority.
 *
 * @param string $prefix Token prefix
 * @return string
 */
function sales_classification_token($prefix)
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
function sales_classification_quantity($quantity)
{
	$value = dimension_source_revision_decimal(number_format((float)$quantity, 8, '.', ''));
	if ($value === false)
		throw new DimensionFinancialReferenceWriteException(
			_('A Sales source line has an invalid quantity and cannot retain classification intent.'));
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
function sales_classification_intent_from_values($id, $unassigned, $axis, $origin)
{
	$identity = dimension_source_revision_int($id, 0);
	if ($identity === false)
		return array('ok' => false, 'intent' => null, 'codes' => array('invalid_identity'),
			'errors' => array(_('The selected Sales classification identity is invalid.')));
	if (!empty($unassigned) && $identity !== 0)
		return array('ok' => false, 'intent' => null, 'codes' => array('conflicting_intent'),
			'errors' => array(_('A Sales category cannot be both selected and explicitly unassigned.')));
	if (!empty($unassigned))
		return dimension_resolution_unassigned($origin);
	if ($identity > 0)
		return dimension_resolution_selected($identity, $origin);
	return null;
}

/**
 * Normalize one intent stored on a Cart, line, or approval payload.
 *
 * @param array|null $carrier Object/array carrying classification_intents
 * @param int $axis Native axis
 * @param string $origin Fallback descriptive origin
 * @return array|null Normalized builder result, or null when no explicit mode was recorded
 */
function sales_classification_carried_intent($carrier, $axis, $origin)
{
	$intents = is_object($carrier) && isset($carrier->classification_intents)
		? $carrier->classification_intents
		: (is_array($carrier) && isset($carrier['classification_intents'])
			? $carrier['classification_intents'] : null);
	if (!is_array($intents) || !array_key_exists((int)$axis, $intents))
		return null;
	return dimension_resolution_normalize_source($intents[(int)$axis], 'header_explicit');
}

/**
 * Read the current customer default pair without inventing a classification when unavailable.
 *
 * @param mixed $customer_id Customer identity
 * @return array axis => configured identity
 */
function sales_classification_customer_defaults($customer_id)
{
	static $cache = array();
	$id = dimension_source_revision_int($customer_id, 0);
	if ($id === false || $id < 1)
		return array(1 => 0, 2 => 0);
	$company = dimension_source_revision_company();
	$key = $company.'|'.$id;
	if (isset($cache[$key]))
		return $cache[$key];
	$rows = db_query('SELECT dimension_id,dimension2_id FROM '.TB_PREF.'debtors_master WHERE debtor_no='.$id);
	if (!$rows)
		throw new DimensionFinancialReferenceWriteException(
			_('The Sales customer classification defaults cannot be read.'));
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
function sales_classification_item_defaults($stock_id)
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
			_('The Sales item classification defaults cannot be read.'));
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
function sales_classification_source_descriptor($source)
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
	if ($type === false || !in_array($type, sales_classification_supported_types(), true)
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
function sales_classification_source_snapshot($source)
{
	$source = sales_classification_source_descriptor($source);
	if ($source === false)
		return array('state' => 'unknown', 'revision' => null, 'intents' => array(),
			'codes' => array('invalid_source'), 'errors' => array());
	if ($source['revision_id'] > 0) {
		$read = dimension_source_revision_read($source['revision_id']);
		if ($read['state'] !== 'available')
			return array('state' => $read['state'], 'revision' => null, 'intents' => array(),
				'codes' => $read['codes'], 'errors' => $read['errors']);
		$revision = $read['revision'];
		if ($revision['source_family'] !== 'sales'
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
	return dimension_source_revision_snapshot('sales', $source['trans_type'],
		$source['document_key'], $source['line_kind'], $source['line_key']);
}

/**
 * Return one axis source entry from a verified source snapshot.
 *
 * @param array $snapshot Snapshot-shaped source read
 * @param int $axis Native axis
 * @return array Empty source set or line_snapshot/header_snapshot entry
 */
function sales_classification_snapshot_axis_source($snapshot, $axis)
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
function sales_classification_source_evidence($descriptor, $snapshot)
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
 * new event. Saved orders with no companion revision stay legacy-unspecified on read.
 *
 * @param object $cart Sales cart or source object
 * @param int $axis Native axis
 * @param bool $allow_native_fallback Adopt an existing native axis value
 * @return array|null Normalized intent
 */
function sales_classification_header_intent($cart, $axis, $allow_native_fallback)
{
	$intent = sales_classification_carried_intent($cart, $axis, 'sales_header');
	if ($intent !== null)
		return $intent;
	if (!$allow_native_fallback)
		return null;
	$field = $axis === 1 ? 'dimension_id' : 'dimension2_id';
	$value = is_object($cart) && isset($cart->$field) ? $cart->$field : 0;
	$identity = dimension_source_revision_int($value, 0);
	if ($identity === false)
		return array('ok' => false, 'intent' => null, 'codes' => array('invalid_identity'),
			'errors' => array(_('The Sales header classification identity is invalid.')));
	return $identity > 0 ? dimension_resolution_selected($identity, 'sales_header_value')
		: dimension_resolution_inherit('sales_header');
}

/**
 * Return the in-memory line intent for one axis.
 *
 * @param object $line Native Sales line
 * @param int $axis Native axis
 * @return array|null Normalized intent
 */
function sales_classification_line_intent($line, $axis)
{
	return sales_classification_carried_intent($line, $axis, 'sales_line');
}

/**
 * Build the line's current conversion source when the native cart retained one.
 *
 * @param object $cart Sales cart
 * @param object $line Sales line
 * @return array|false
 */
function sales_classification_native_line_source($cart, $line)
{
	if (is_object($cart) && (int)$cart->trans_type === ST_SALESORDER) {
		$src_id = is_object($line) && isset($line->src_id)
			? dimension_source_revision_int($line->src_id, 0) : 0;
		if ($src_id && isset($cart->src_docs) && is_array($cart->src_docs) && $cart->src_docs) {
			$numbers = array_keys($cart->src_docs);
			$document = isset($numbers[0]) ? dimension_source_revision_document_key($numbers[0]) : false;
			if ($document !== false)
				return array('trans_type' => ST_SALESQUOTE, 'document_key' => $document,
					'line_kind' => 'line',
					'line_key' => dimension_source_revision_line_key('line', $src_id),
					'revision_id' => 0);
		}
	}
	if (is_object($line) && isset($line->classification_source)
		&& ($source = sales_classification_source_descriptor($line->classification_source)) !== false)
		return $source;
	return false;
}

/**
 * Build the document's current conversion/header source.
 *
 * @param object $cart Sales cart
 * @return array|false
 */
function sales_classification_native_header_source($cart)
{
	if (is_object($cart) && (int)$cart->trans_type === ST_SALESORDER) {
		if (!empty($cart->agreement_id)) {
			$document = dimension_source_revision_document_key($cart->agreement_id);
			if ($document !== false)
				return array('trans_type' => ST_SALESAGREEMENT, 'document_key' => $document,
					'line_kind' => 'header', 'line_key' => 'header', 'revision_id' => 0);
		}
		if (isset($cart->src_docs) && is_array($cart->src_docs) && $cart->src_docs) {
			$numbers = array_keys($cart->src_docs);
			$document = isset($numbers[0]) ? dimension_source_revision_document_key($numbers[0]) : false;
			if ($document !== false)
				return array('trans_type' => ST_SALESQUOTE, 'document_key' => $document,
					'line_kind' => 'header', 'line_key' => 'header', 'revision_id' => 0);
		}
	}
	if (is_object($cart) && isset($cart->classification_source)
		&& ($source = sales_classification_source_descriptor($cart->classification_source)) !== false)
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
function sales_classification_add_intent_source($sources, $kind, $intent)
{
	if (is_array($intent) && count($intent))
		$sources[$kind] = $intent;
	return $sources;
}

/**
 * Resolve both axes for one Sales source entry and return its exact resolver evidence.
 *
 * @param object $cart Sales cart
 * @param object|null $line Sales line, or null for a header
 * @param int $line_index Line index (diagnostic only, never an identity)
 * @param bool $new_line Whether an item default is eligible
 * @param array $header_source Header source descriptor
 * @param array $header_snapshot Header source snapshot
 * @param array $line_source Line source descriptor
 * @param array $line_snapshot Line source snapshot
 * @return array ok, pair, axes, resolution, errors and codes
 */
function sales_classification_resolve_entry($cart, $line, $line_index, $new_line,
	$header_source, $header_snapshot, $line_source, $line_snapshot)
{
	$result = array('ok' => false, 'pair' => null, 'axes' => array(), 'resolution' => null,
		'errors' => array(), 'codes' => array());
	if (!is_object($cart) || !in_array((int)$cart->trans_type,
		array(ST_SALESORDER, ST_SALESQUOTE, ST_SALESAGREEMENT, ST_SALESQUOTE_TEMPLATE), true)) {
		$result['codes'][] = 'unsupported_source';
		$result['errors'][] = _('This Sales source cannot retain classification intent.');
		return $result;
	}
	$customer = sales_classification_customer_defaults(isset($cart->customer_id) ? $cart->customer_id : 0);
	$item = $line !== null && is_object($line)
		? sales_classification_item_defaults(isset($line->stock_id) ? $line->stock_id : '')
		: array(1 => 0, 2 => 0);
	$sources = array(1 => array(), 2 => array());
	$header_intent = array(1 => null, 2 => null);
	foreach (array(1, 2) as $axis) {
		$allow_native = !isset($header_snapshot['state']) || $header_snapshot['state'] !== 'available';
		$header_intent[$axis] = sales_classification_header_intent($cart, $axis, $allow_native);
		if ($line !== null) {
			$line_intent = sales_classification_line_intent($line, $axis);
			$sources[$axis] = sales_classification_add_intent_source($sources[$axis],
				'line_explicit', $line_intent);
		}
		$sources[$axis] = sales_classification_add_intent_source($sources[$axis],
			'line_snapshot', sales_classification_snapshot_axis_source($line_snapshot, $axis));
		$sources[$axis] = sales_classification_add_intent_source($sources[$axis],
			'header_explicit', $header_intent[$axis] === null
				? dimension_resolution_inherit('sales_header') : $header_intent[$axis]);
		$sources[$axis] = sales_classification_add_intent_source($sources[$axis],
			'header_snapshot', sales_classification_snapshot_axis_source($header_snapshot, $axis));
		$sources[$axis]['partner_default'] = dimension_resolution_default_intent(
			$customer[$axis], 'partner_default');
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
				'origin' => $axis > $enabled ? 'axis_disabled' : 'sales_unassigned',
				'persisted' => false, 'policy_version' => 0, 'basis' => ''));
		}
	}
	$resolution = array(
		'contract' => 'sales-classification-resolution/1',
		'line_index' => (int)$line_index,
		'new_line' => (bool)$new_line,
		'enabled_axes' => $enabled,
		'policy_version' => (int)$pair['policy_version'],
		'axes' => array(1 => $pair['axes'][1], 2 => $pair['axes'][2]),
		'header_source' => sales_classification_source_evidence($header_source, $header_snapshot),
		'line_source' => sales_classification_source_evidence($line_source, $line_snapshot));
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
function sales_classification_revision_equivalent($current, $fields)
{
	foreach (array('dimension_id', 'dimension2_id', 'mode1', 'mode2', 'origin1', 'origin2',
		'basis1', 'basis2', 'policy1_version', 'policy2_version', 'effective_date',
		'line_quantity', 'resolution_payload') as $name) {
		$current_value = array_key_exists($name, $current) ? $current[$name] : null;
		if ((string)$current_value !== (string)$fields[$name])
			return false;
	}
	return true;
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
function sales_classification_persist_revision($trans_type, $document_key, $line_kind, $line_key,
	$resolution, $effective_date, $line_quantity)
{
	$result = array('ok' => false, 'revision_id' => 0, 'revision' => null,
		'created' => false, 'errors' => array(), 'codes' => array());
	if (!is_array($resolution) || empty($resolution['axes'][1]) || empty($resolution['axes'][2])) {
		$result['codes'][] = 'invalid_resolution';
		$result['errors'][] = _('The Sales classification resolution is incomplete.');
		return $result;
	}
	$prepared = dimension_source_revision_prepare('sales', $trans_type, $document_key, $line_kind,
		$line_key, $resolution['axes'], array('effective_date' => $effective_date,
			'line_quantity' => $line_quantity, 'resolution' => $resolution));
	if (!$prepared['ok']) {
		$result['codes'] = $prepared['codes'];
		$result['errors'] = $prepared['errors'];
		return $result;
	}
	$current = dimension_source_revision_snapshot('sales', $trans_type, $document_key,
		$line_kind, $line_key);
	if (!in_array($current['state'], array('available', 'absent', 'unavailable'), true)) {
		$result['codes'] = !empty($current['codes']) ? $current['codes'] : array('source_unreadable');
		$result['errors'] = !empty($current['errors']) ? $current['errors']
			: array(_('The current Sales classification revision cannot be verified.'));
		return $result;
	}
	if ($current['state'] === 'available'
		&& sales_classification_revision_equivalent($current['revision'], $prepared['fields'])) {
		$result['ok'] = true;
		$result['revision_id'] = (int)$current['revision']['source_revision_id'];
		$result['revision'] = $current['revision'];
		return $result;
	}
	$options = array('effective_date' => $effective_date, 'line_quantity' => $line_quantity,
		'resolution' => $resolution);
	if ($current['state'] === 'available')
		$options['predecessor_revision_id'] = (int)$current['revision']['source_revision_id'];
	$prepared = dimension_source_revision_prepare('sales', $trans_type, $document_key, $line_kind,
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
 * Ensure one cart has stable draft identities for its header and lines.
 *
 * @param object $cart Sales cart
 * @return string Durable draft key
 */
function sales_classification_ensure_draft_identity($cart)
{
	if (!is_object($cart))
		throw new DimensionFinancialReferenceWriteException(_('The Sales classification carrier is unavailable.'));
	if (!isset($cart->classification_draft_token) || !is_string($cart->classification_draft_token)
		|| $cart->classification_draft_token === '')
		$cart->classification_draft_token = sales_classification_token('salesdraft');
	$key = dimension_source_revision_draft_key($cart->classification_draft_token);
	if ($key === false)
		throw new DimensionFinancialReferenceWriteException(_('The Sales draft classification identity is invalid.'));
	if (!isset($cart->line_items) || !is_array($cart->line_items))
		throw new DimensionFinancialReferenceWriteException(_('The Sales classification lines are unavailable.'));
	foreach ($cart->line_items as $line) {
		if (!is_object($line))
			throw new DimensionFinancialReferenceWriteException(_('A Sales classification line is invalid.'));
		if (!isset($line->classification_token) || !is_string($line->classification_token)
			|| $line->classification_token === '')
			$line->classification_token = sales_classification_token('line');
	}
	return $key;
}

/**
 * Capture a new draft source, or a current edited final source, for a Sales cart.
 *
 * @param object $cart Sales cart
 * @param mixed $document_key Final source number, or null/zero for a draft
 * @param bool $final Whether regular line identities are already persisted
 * @return array capture summary
 */
function sales_classification_capture_cart($cart, $document_key = null, $final = false)
{
	if (!is_object($cart) || !in_array((int)$cart->trans_type,
		array(ST_SALESORDER, ST_SALESQUOTE), true))
		throw new DimensionFinancialReferenceWriteException(_('This Sales document cannot retain classification intent.'));
	$draft_key = sales_classification_ensure_draft_identity($cart);
	$final_key = $final ? dimension_source_revision_document_key($document_key) : false;
	if ($final && $final_key === false)
		throw new DimensionFinancialReferenceWriteException(_('The saved Sales source identity is invalid.'));
	$source_document_key = $final ? $final_key : $draft_key;
	$effective = isset($cart->document_date) ? date2sql($cart->document_date) : '';
	if (dimension_source_revision_date($effective) === false)
		throw new DimensionFinancialReferenceWriteException(_('The Sales source date is invalid.'));
	$header_source = sales_classification_native_header_source($cart);
	$header_snapshot = $header_source === false ? array('state' => 'none')
		: sales_classification_source_snapshot($header_source);
	$header = sales_classification_resolve_entry($cart, null, -1, false,
		$header_source, $header_snapshot, false, array('state' => 'none'));
	if (!$header['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $header['errors']));
	$cart->dimension_id = (int)$header['axes'][1]['value'];
	$cart->dimension2_id = (int)$header['axes'][2]['value'];
	$header_capture = sales_classification_persist_revision((int)$cart->trans_type,
		$source_document_key, $final ? 'header' : 'header', 'header',
		$header['resolution'], $effective, '0.00000000');
	if (!$header_capture['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $header_capture['errors']));
	$cart->classification_revision_id = (int)$header_capture['revision_id'];
	$cart->classification_lineage_state = 'versioned';
	$cart->classification_draft_revisions = array('document_key' => $source_document_key,
		'header' => (int)$header_capture['revision_id'], 'lines' => array());
	$line_result = array();
	foreach (array_values($cart->line_items) as $index => $line) {
		if (!is_object($line))
			throw new DimensionFinancialReferenceWriteException(_('A Sales classification line is invalid.'));
		$line_source = sales_classification_native_line_source($cart, $line);
		$line_snapshot = $line_source === false ? array('state' => 'none')
			: sales_classification_source_snapshot($line_source);
		// An amendment captures after the native write has already assigned ids to lines added in
		// this edit, so a missing id cannot identify them: a line never loaded from or captured
		// against a stored source still carries the `new` lineage state.
		$is_new = !$final || empty($line->id)
			|| (isset($line->classification_lineage_state)
				&& $line->classification_lineage_state === 'new');
		$line_kind = $final && !empty($line->id) ? 'line' : 'draft_line';
		$line_key = $line_kind === 'line'
			? dimension_source_revision_line_key('line', $line->id)
			: dimension_source_revision_line_key('draft_line', $line->classification_token);
		if ($line_key === false)
			throw new DimensionFinancialReferenceWriteException(_('A Sales classification line identity is invalid.'));
		$resolved = sales_classification_resolve_entry($cart, $line, $index, $is_new,
			$header_source, $header_snapshot, $line_source, $line_snapshot);
		if (!$resolved['ok'])
			throw new DimensionFinancialReferenceWriteException(implode(' ', $resolved['errors']));
		$line->dimension_id = (int)$resolved['axes'][1]['value'];
		$line->dimension2_id = (int)$resolved['axes'][2]['value'];
		$stored = sales_classification_persist_revision((int)$cart->trans_type,
			$source_document_key, $line_kind, $line_key, $resolved['resolution'],
			$effective, sales_classification_quantity($line->quantity));
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
 * Build the DIM-APR-001 request from a captured unsaved Sales source.
 *
 * @param array $capture sales_classification_capture_cart() result
 * @return array|null
 */
function sales_classification_approval_request($capture)
{
	if (!is_array($capture) || empty($capture['draft']) || empty($capture['header_revision_id'])
		|| !isset($capture['line_revision_ids']) || !is_array($capture['line_revision_ids']))
		return null;
	$lines = array();
	foreach ($capture['line_revision_ids'] as $revision_id)
		$lines[] = array('revision_id' => (int)$revision_id);
	return array('source_family' => 'sales', 'document_key' => $capture['document_key'],
		'header' => array('revision_id' => (int)$capture['header_revision_id']),
		'lines' => $lines);
}

/**
 * Remove the sealed approval envelope from a payload before exact business-data comparison.
 *
 * @param mixed $data Draft payload
 * @return mixed
 */
function sales_classification_without_approval_envelope($data)
{
	if (!is_array($data))
		return $data;
	unset($data[approval_dimension_snapshot_payload_key()]);
	return $data;
}

/**
 * Normalize source evidence for business-data comparison after the approval authority has already
 * restored and verified the sealed classification intents.
 *
 * @param mixed $data Sales draft/business payload
 * @return mixed
 */
function sales_classification_without_intent_carriers($data)
{
	if (!is_array($data))
		return $data;
	unset($data['classification_intents']);
	if (isset($data['line_items']) && is_array($data['line_items'])) {
		foreach ($data['line_items'] as $index => $line) {
			if (!is_array($line))
				continue;
			unset($data['line_items'][$index]['classification_intents']);
		}
	}
	return $data;
}

/**
 * Remove request/capture metadata that is expected to change while a source is written.
 *
 * Business fields, stock ids, quantities, prices and stable line tokens remain in the comparison.
 *
 * @param mixed $data Sales source payload
 * @return mixed
 */
function sales_classification_without_volatile_metadata($data)
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
				'classification_source','classification_token','draft_line_key',
				'dimension_id','dimension2_id') as $field)
				unset($data['line_items'][$index][$field]);
		}
	}
	return $data;
}

/**
 * Bind every captured draft revision to the final order/quotation identity.
 *
 * @param object $cart Written Sales cart
 * @param mixed $document_no Final native number
 * @return bool
 */
function sales_classification_bind_cart($cart, $document_no)
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
		$line_id = is_object($line) && isset($line->id)
			? dimension_source_revision_int($line->id, 1) : false;
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
			'line_quantity' => sales_classification_quantity($line->quantity)));
		if (!$line_bound['ok'])
			return false;
		$line->classification_revision_id = (int)$line_bound['revision_id'];
		$line->classification_lineage_state = 'versioned';
	}
	return true;
}

/**
 * Load the current durable classification of one saved Sales order/quotation.
 *
 * Missing lineage is reported as legacy-unspecified and is never reconstructed from a current
 * customer or item default.
 *
 * @param object $cart Sales cart
 * @param int $trans_type Stored type
 * @param int $number Stored document number
 * @return string header lineage state
 */
function sales_classification_load_order($cart, $trans_type, $number)
{
	$document = dimension_source_revision_document_key($number);
	if ($document === false || !is_object($cart))
		return 'unknown';
	$snapshot = dimension_source_revision_snapshot('sales', $trans_type, $document, 'header', 'header');
	if ($snapshot['state'] === 'available') {
		dimension_source_revision_apply($cart, $snapshot);
		$cart->classification_revision_id = (int)$snapshot['revision']['source_revision_id'];
		$cart->classification_lineage_state = 'versioned';
	} elseif ($snapshot['state'] === 'absent') {
		$cart->classification_lineage_state = 'legacy_unspecified';
	} else {
		$cart->classification_lineage_state = $snapshot['state'];
	}
	foreach ($cart->line_items as $line) {
		$line_id = is_object($line) && isset($line->id)
			? dimension_source_revision_int($line->id, 1) : false;
		if ($line_id === false)
			continue;
		$line_key = dimension_source_revision_line_key('line', $line_id);
		$line_snapshot = dimension_source_revision_snapshot('sales', $trans_type,
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
	return $cart->classification_lineage_state;
}

/**
 * Apply one saved agreement line's persisted intents without recalculating defaults.
 *
 * @param array $line Native agreement line
 * @param int $agreement_id Parent identity
 * @return array Native line with classification metadata added
 */
function sales_classification_load_agreement_line($line, $agreement_id)
{
	if (!is_array($line))
		return $line;
	$document = dimension_source_revision_document_key($agreement_id);
	$line_id = isset($line['id']) ? dimension_source_revision_int($line['id'], 1) : false;
	if ($document === false || $line_id === false) {
		$line['classification_lineage_state'] = 'unknown';
		return $line;
	}
	$snapshot = dimension_source_revision_snapshot('sales', ST_SALESAGREEMENT, $document, 'line',
		dimension_source_revision_line_key('line', $line_id));
	if ($snapshot['state'] === 'available')
		$line['classification_lineage_state'] = 'versioned';
	else
		$line['classification_lineage_state'] = $snapshot['state'] === 'absent'
			? 'legacy_unspecified' : $snapshot['state'];
	$line['classification_intents'] = $snapshot['state'] === 'available'
		? $snapshot['intents'] : null;
	return $line;
}

/**
 * Load the durable header intents of one saved agreement.
 *
 * @param array $agreement Native agreement header
 * @return array|null axis => normalized intent
 */
function sales_classification_load_agreement_header($agreement)
{
	if (!is_array($agreement) || !isset($agreement['id']))
		return null;
	$document = dimension_source_revision_document_key($agreement['id']);
	if ($document === false)
		return null;
	$snapshot = dimension_source_revision_snapshot('sales', ST_SALESAGREEMENT, $document,
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
 * Capture one agreement header/line after its native row has been written.
 *
 * @param int $trans_type ST_SALESAGREEMENT
 * @param int $document_no Agreement identity
 * @param array $header_intents axis => normalized intent
 * @param array|null $line Native line row for line capture
 * @param array|null $line_intents axis => normalized intent
 * @param array|false $source Source descriptor
 * @return array capture result
 */
function sales_classification_capture_agreement($trans_type, $document_no, $header_intents,
	$line = null, $line_intents = null, $source = false)
{
	$document = dimension_source_revision_document_key($document_no);
	if ($document === false || (int)$trans_type !== ST_SALESAGREEMENT)
		throw new DimensionFinancialReferenceWriteException(_('The sales agreement source identity is invalid.'));
	$header_rows = db_query('SELECT * FROM '.TB_PREF.'sales_agreements WHERE id='.(int)$document_no);
	$header = $header_rows ? db_fetch_assoc($header_rows) : false;
	if (!$header)
		throw new DimensionFinancialReferenceWriteException(_('The sales agreement source is unavailable.'));
	$cart = (object)array('trans_type' => ST_SALESAGREEMENT, 'customer_id' => $header['debtor_no'],
		'document_date' => $header['date_start'], 'dimension_id' => (int)$header['dimension_id'],
		'dimension2_id' => (int)$header['dimension2_id'], 'classification_intents' => $header_intents,
		'line_items' => array());
	$effective = dimension_source_revision_date($header['date_start']) !== false
		? $header['date_start'] : date2sql($header['date_start']);
	$header_source = $source === false ? false : sales_classification_source_descriptor($source);
	$header_snapshot = $header_source === false ? array('state' => 'none')
		: sales_classification_source_snapshot($header_source);
	$resolved = sales_classification_resolve_entry($cart, null, -1, false,
		$header_source, $header_snapshot, false, array('state' => 'none'));
	if (!$resolved['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $resolved['errors']));
	$header_capture = sales_classification_persist_revision(ST_SALESAGREEMENT, $document, 'header',
		'header', $resolved['resolution'], $effective, '0.00000000');
	if (!$header_capture['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $header_capture['errors']));
	if ($line === null)
		return array('header_revision_id' => (int)$header_capture['revision_id'],
			'line_revision_id' => 0);
	$line_id = isset($line['id']) ? dimension_source_revision_int($line['id'], 1) : false;
	if ($line_id === false)
		throw new DimensionFinancialReferenceWriteException(_('The sales agreement line identity is invalid.'));
	$line_cart = clone $cart;
	$line_cart->classification_intents = $header_intents;
	$line_object = (object)array('stock_id' => $line['stock_id'],
		'quantity' => $line['committed_qty'], 'classification_intents' => $line_intents,
		'id' => $line_id);
	$line_snapshot = dimension_source_revision_snapshot('sales', ST_SALESAGREEMENT, $document,
		'line', dimension_source_revision_line_key('line', $line_id));
	$line_source = $line_snapshot['state'] === 'available' ? array(
		'trans_type' => ST_SALESAGREEMENT, 'document_key' => $document, 'line_kind' => 'line',
		'line_key' => dimension_source_revision_line_key('line', $line_id),
		'revision_id' => (int)$line_snapshot['revision']['source_revision_id']) : false;
	$line_resolved = sales_classification_resolve_entry($line_cart, $line_object,
		(int)$line_id, false, $header_source, $header_snapshot, $line_source, $line_snapshot);
	if (!$line_resolved['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $line_resolved['errors']));
	$line_capture = sales_classification_persist_revision(ST_SALESAGREEMENT, $document, 'line',
		dimension_source_revision_line_key('line', $line_id), $line_resolved['resolution'],
		$effective, sales_classification_quantity($line['committed_qty']));
	if (!$line_capture['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $line_capture['errors']));
	return array('header_revision_id' => (int)$header_capture['revision_id'],
		'line_revision_id' => (int)$line_capture['revision_id']);
}

/**
 * Capture one quotation-template header or line after its native row exists.
 *
 * @param int $template_id Template identity
 * @param array $header_intents axis => normalized intent
 * @param array|null $line Template line row
 * @param array|null $line_intents axis => normalized intent
 * @return array
 */
function sales_classification_capture_template($template_id, $header_intents, $line = null,
	$line_intents = null)
{
	$document = dimension_source_revision_document_key($template_id);
	if ($document === false)
		throw new DimensionFinancialReferenceWriteException(_('The quotation template identity is invalid.'));
	$cart = (object)array('trans_type' => ST_SALESQUOTE_TEMPLATE, 'customer_id' => 0,
		'document_date' => Today(), 'dimension_id' => 0, 'dimension2_id' => 0,
		'classification_intents' => $header_intents, 'line_items' => array());
	$header_revision_id = 0;
	$header_source = false;
	$header_snapshot = array('state' => 'none');
	if ($line === null) {
		$resolved = sales_classification_resolve_entry($cart, null, -1, false, false,
			array('state' => 'none'), false, array('state' => 'none'));
		if (!$resolved['ok'])
			throw new DimensionFinancialReferenceWriteException(implode(' ', $resolved['errors']));
		$header = sales_classification_persist_revision(ST_SALESQUOTE_TEMPLATE, $document, 'header',
			'header', $resolved['resolution'], date2sql(Today()), '0.00000000');
		if (!$header['ok'])
			throw new DimensionFinancialReferenceWriteException(implode(' ', $header['errors']));
		return array('header_revision_id' => (int)$header['revision_id'], 'line_revision_id' => 0);
	}
	$header_snapshot = dimension_source_revision_snapshot('sales', ST_SALESQUOTE_TEMPLATE,
		$document, 'header', 'header');
	if ($header_snapshot['state'] === 'available') {
		$header_revision_id = (int)$header_snapshot['revision']['source_revision_id'];
		$header_source = array('trans_type' => ST_SALESQUOTE_TEMPLATE,
			'document_key' => $document, 'line_kind' => 'header', 'line_key' => 'header',
			'revision_id' => $header_revision_id);
	} else {
		$resolved_header = sales_classification_resolve_entry($cart, null, -1, false, false,
			array('state' => 'none'), false, array('state' => 'none'));
		if (!$resolved_header['ok'])
			throw new DimensionFinancialReferenceWriteException(implode(' ', $resolved_header['errors']));
		$stored_header = sales_classification_persist_revision(ST_SALESQUOTE_TEMPLATE,
			$document, 'header', 'header', $resolved_header['resolution'], date2sql(Today()),
			'0.00000000');
		if (!$stored_header['ok'])
			throw new DimensionFinancialReferenceWriteException(implode(' ', $stored_header['errors']));
		$header_revision_id = (int)$stored_header['revision_id'];
		$header_source = array('trans_type' => ST_SALESQUOTE_TEMPLATE,
			'document_key' => $document, 'line_kind' => 'header', 'line_key' => 'header',
			'revision_id' => $header_revision_id);
		$header_snapshot = dimension_source_revision_snapshot('sales', ST_SALESQUOTE_TEMPLATE,
			$document, 'header', 'header', array('revision_no' => 0));
	}
	$line_id = isset($line['id']) ? dimension_source_revision_int($line['id'], 1) : false;
	if ($line_id === false)
		throw new DimensionFinancialReferenceWriteException(_('The quotation template line identity is invalid.'));
	$line_object = (object)array('id' => $line_id, 'stock_id' => $line['stock_id'],
		'quantity' => $line['quantity'], 'classification_intents' => $line_intents);
	$line_resolved = sales_classification_resolve_entry($cart, $line_object, $line_id, false,
		$header_source, $header_snapshot, false, array('state' => 'none'));
	if (!$line_resolved['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $line_resolved['errors']));
	$stored = sales_classification_persist_revision(ST_SALESQUOTE_TEMPLATE, $document, 'line',
		dimension_source_revision_line_key('line', $line_id), $line_resolved['resolution'],
		date2sql(Today()), sales_classification_quantity($line['quantity']));
	if (!$stored['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $stored['errors']));
	return array('header_revision_id' => (int)$header_revision_id,
		'line_revision_id' => (int)$stored['revision_id']);
}

/**
 * Attach one template line's persisted intent/source to a quotation line.
 *
 * @param object $line Quotation cart line
 * @param int $template_id Template identity
 * @param int $template_line_id Template line identity
 * @return void
 */
function sales_classification_apply_template_line($line, $template_id, $template_line_id)
{
	$document = dimension_source_revision_document_key($template_id);
	$line_id = dimension_source_revision_int($template_line_id, 1);
	if (!is_object($line) || $document === false || $line_id === false)
		return;
	$snapshot = dimension_source_revision_snapshot('sales', ST_SALESQUOTE_TEMPLATE, $document,
		'line', dimension_source_revision_line_key('line', $line_id));
	if ($snapshot['state'] === 'available')
		dimension_source_revision_apply($line, $snapshot);
	$line->classification_source = array('trans_type' => ST_SALESQUOTE_TEMPLATE,
		'document_key' => $document, 'line_kind' => 'line',
		'line_key' => dimension_source_revision_line_key('line', $line_id),
		'revision_id' => $snapshot['state'] === 'available'
			? (int)$snapshot['revision']['source_revision_id'] : 0);
}

/**
 * Attach a template header snapshot to a quotation cart.
 *
 * @param object $cart Quotation cart
 * @param int $template_id Template identity
 * @return void
 */
function sales_classification_apply_template_header($cart, $template_id)
{
	$document = dimension_source_revision_document_key($template_id);
	if (!is_object($cart) || $document === false)
		return;
	$snapshot = dimension_source_revision_snapshot('sales', ST_SALESQUOTE_TEMPLATE, $document,
		'header', 'header');
	if ($snapshot['state'] === 'available')
		dimension_source_revision_apply($cart, $snapshot);
	$cart->classification_source = array('trans_type' => ST_SALESQUOTE_TEMPLATE,
		'document_key' => $document, 'line_kind' => 'header', 'line_key' => 'header',
		'revision_id' => $snapshot['state'] === 'available'
			? (int)$snapshot['revision']['source_revision_id'] : 0);
}

/**
 * Load one template header intent carrier for its management form.
 *
 * @param int $template_id Template identity
 * @return array|null axis => intent
 */
function sales_classification_load_template_header($template_id)
{
	$document = dimension_source_revision_document_key($template_id);
	if ($document === false)
		return null;
	$snapshot = dimension_source_revision_snapshot('sales', ST_SALESQUOTE_TEMPLATE, $document,
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
 * Load one template line intent carrier for its management form.
 *
 * @param int $template_id Template identity
 * @param int $line_id Template line identity
 * @return array|null axis => intent
 */
function sales_classification_load_template_line($template_id, $line_id)
{
	$document = dimension_source_revision_document_key($template_id);
	$line = dimension_source_revision_int($line_id, 1);
	if ($document === false || $line === false)
		return null;
	$snapshot = dimension_source_revision_snapshot('sales', ST_SALESQUOTE_TEMPLATE, $document,
		'line', dimension_source_revision_line_key('line', $line));
	if ($snapshot['state'] !== 'available')
		return null;
	$intents = array();
	foreach (array(1, 2) as $axis)
		if (isset($snapshot['intents'][$axis]['intent']))
			$intents[$axis] = $snapshot['intents'][$axis]['intent'];
	return $intents ? $intents : null;
}

/**
 * Attach current agreement source identities to an order conversion cart.
 *
 * @param object $cart Agreement conversion cart
 * @param array $source Locked agreement source
 * @param array $line_map Agreement line id => cart line index
 * @return void
 */
function sales_classification_apply_agreement_conversion($cart, $source, $line_map)
{
	if (!is_object($cart) || !is_array($source) || !isset($source['header']['id']))
		return;
	$document = dimension_source_revision_document_key($source['header']['id']);
	if ($document === false)
		return;
	$header_snapshot = dimension_source_revision_snapshot('sales', ST_SALESAGREEMENT,
		$document, 'header', 'header');
	$cart->classification_source = array('trans_type' => ST_SALESAGREEMENT,
		'document_key' => $document, 'line_kind' => 'header', 'line_key' => 'header',
		'revision_id' => $header_snapshot['state'] === 'available'
			? (int)$header_snapshot['revision']['source_revision_id'] : 0);
	foreach ((array)$line_map as $agreement_line_id => $line_index) {
		if (!isset($cart->line_items[$line_index]) || !is_object($cart->line_items[$line_index]))
			continue;
		$line_id = dimension_source_revision_int($agreement_line_id, 1);
		if ($line_id === false)
			continue;
		$snapshot = dimension_source_revision_snapshot('sales', ST_SALESAGREEMENT,
			$document, 'line', dimension_source_revision_line_key('line', $line_id));
		if ($snapshot['state'] === 'available')
			dimension_source_revision_apply($cart->line_items[$line_index], $snapshot);
		$cart->line_items[$line_index]->classification_source = array(
			'trans_type' => ST_SALESAGREEMENT, 'document_key' => $document,
			'line_kind' => 'line', 'line_key' => dimension_source_revision_line_key('line', $line_id),
			'revision_id' => $snapshot['state'] === 'available'
				? (int)$snapshot['revision']['source_revision_id'] : 0);
	}
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
function sales_classification_intents_from_values($id1, $id2, $blank1, $blank2, $origin)
{
	$result = array();
	foreach (array(1 => array($id1, $blank1), 2 => array($id2, $blank2)) as $axis => $values) {
		$intent = sales_classification_intent_from_values($values[0], $values[1], $axis, $origin);
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
function sales_classification_carrier_values($carrier)
{
	$result = array('ids' => array(1 => 0, 2 => 0),
		'unassigned' => array(1 => 0, 2 => 0));
	foreach (array(1, 2) as $axis) {
		$intent = sales_classification_carried_intent($carrier, $axis, 'sales_display');
		if ($intent === null || !$intent['ok'])
			continue;
		$result['ids'][$axis] = $intent['intent']['mode'] === 'selected'
			? (int)$intent['intent']['id'] : 0;
		$result['unassigned'][$axis] = $intent['intent']['mode'] === 'unassigned' ? 1 : 0;
	}
	return $result;
}
