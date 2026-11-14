<?php
/**
 * DIM-INV-001: durable inventory cost-recognition classification intent.
 *
 * This adapter resolves the classification of an inventory adjustment, a stock cost update, a
 * negative-stock cost correction and a location transfer through the shared DIM-FND-005/DIM-FND-004
 * contracts and stores it through DIM-FND-006 in the `inventory` source family. It never writes a
 * ledger row, never invents a default for an unsnapshotted legacy document, and never replaces the
 * native inventory tables (`stock_moves`, `loc_stock`, `gl_trans`) with a second source of truth.
 *
 * Decisions this adapter owns:
 *  - item-driven cost recognition (adjustment, cost update, zero/negative-stock handling) resolves
 *    the explicit header/line intent first and the item's configured default only as a weak last
 *    source of a genuinely new event, then snapshots the result so a later item-master change can
 *    never silently re-attribute the posted cost;
 *  - a negative-stock cost correction preserves its original source lineage: the pair that the
 *    corrected deliveries or the triggering adjustment were actually posted with outranks the
 *    current item default, and the original source references are sealed into the evidence;
 *  - a location transfer is classified for traceability only (`posting_effect` = `none`): it moves
 *    quantity between locations/bins for the same owner, so it manufactures no expense and writes
 *    no ledger row, and an unrelated company requiredness rule can never block a stock move.
 *
 * PHP 5.6 compatible.
 */
require_once __DIR__.'/../../../dimensions/includes/dimension_source_revision.inc';
require_once __DIR__.'/../../../dimensions/includes/dimension_category_policy.inc';

// The canonical transaction types are loaded by includes/types.inc in a normal request. Focused CLI
// harnesses evaluate named writers without the page bootstrap, so the same canonical values are
// admitted here without ever replacing an existing definition.
foreach (array('ST_JOURNAL' => 0, 'ST_LOCTRANSFER' => 16, 'ST_INVADJUST' => 17,
	'ST_COSTUPDATE' => 35, 'ST_CUSTDELIVERY' => 13, 'ST_WH_SCRAP' => 52, 'ST_WH_CYCLE_COUNT' => 54,
	'ST_SUPPRECEIVE' => 25) as $inventory_classification_type => $inventory_classification_value)
	if (!defined($inventory_classification_type))
		define($inventory_classification_type, $inventory_classification_value);

// DIM-INV-002: source-revision anchors of two warehouse events that have no native transaction type.
// They are revision identities only: no transaction table ever stores them, and they live in a range
// that no ST_* constant uses, so they can never collide with a posted document identity.
if (!defined('INVENTORY_CLASSIFICATION_WARRANTY_ANCHOR'))
	define('INVENTORY_CLASSIFICATION_WARRANTY_ANCHOR', 9054);
if (!defined('INVENTORY_CLASSIFICATION_CONSIGNMENT_ANCHOR'))
	define('INVENTORY_CLASSIFICATION_CONSIGNMENT_ANCHOR', 9055);

/**
 * Transaction types that this adapter classifies.
 *
 * ST_JOURNAL appears here only for the zero/negative-stock handling journal that inventory itself
 * posts; ordinary journals stay with DIM-GL-001/DIM-GL-002. DIM-INV-002 adds the two warehouse
 * approval documents (scrap and cycle count) and the two anchors of the events that are classified
 * before or beside their ledger posting (warranty provision and consignment receipt).
 *
 * @return int[]
 */
function inventory_classification_supported_types()
{
	return array(ST_INVADJUST, ST_COSTUPDATE, ST_LOCTRANSFER, ST_JOURNAL, ST_WH_SCRAP, ST_WH_CYCLE_COUNT,
		INVENTORY_CLASSIFICATION_WARRANTY_ANCHOR, INVENTORY_CLASSIFICATION_CONSIGNMENT_ANCHOR);
}

/**
 * Is one type a supported inventory classification source?
 *
 * @param mixed $trans_type Stored source type
 * @return bool
 */
function inventory_classification_type_supported($trans_type)
{
	$type = dimension_source_revision_int($trans_type, 0);
	return $type !== false && in_array($type, inventory_classification_supported_types(), true);
}

/**
 * Does one inventory source recognise cost in the ledger?
 *
 * A location transfer moves quantity for the same owner and posts nothing.
 *
 * @param mixed $trans_type Stored source type
 * @return bool
 */
function inventory_classification_posts_cost($trans_type)
{
	return inventory_classification_type_supported($trans_type) && (int)$trans_type !== ST_LOCTRANSFER;
}

/**
 * Is the accepted companion revision schema installed for the current company?
 *
 * An install that has not applied the accepted DIM-FND-006 upgrade keeps its legacy behavior: the
 * writer must not fail and no synthetic intent may be recorded.
 *
 * @return bool
 */
function inventory_classification_schema_ready()
{
	return dimension_source_revision_schema_ready() === true;
}

/**
 * Build a bounded opaque token accepted by the durable revision authority.
 *
 * @param string $prefix Token prefix
 * @return string
 */
function inventory_classification_token($prefix)
{
	$prefix = preg_replace('/[^a-z0-9_]/', '', strtolower((string)$prefix));
	if ($prefix === '' || !preg_match('/^[a-z]/', $prefix))
		$prefix = 'source';
	$random = function_exists('openssl_random_pseudo_bytes') ? openssl_random_pseudo_bytes(24) : null;
	$hex = is_string($random) && strlen($random) > 0
		? bin2hex($random) : hash('sha256', uniqid('', true).mt_rand().microtime(true));
	return substr($prefix, 0, 8).'_'.substr($hex, 0, 48);
}

/**
 * Canonical eight-decimal absolute quantity accepted by the source-revision contract.
 *
 * @param mixed $quantity Signed stock quantity
 * @return string
 */
function inventory_classification_quantity($quantity)
{
	$value = dimension_source_revision_decimal(number_format(abs((float)$quantity), 8, '.', ''));
	if ($value === false)
		throw new DimensionFinancialReferenceWriteException(
			_('An inventory source line has an invalid quantity and cannot retain classification intent.'));
	return $value;
}

/**
 * Canonical eight-decimal absolute money/cost text for sealed evidence (never a float).
 *
 * @param mixed $amount Signed amount
 * @return string
 */
function inventory_classification_amount_text($amount)
{
	return number_format((float)$amount, 8, '.', '');
}

/**
 * Read one item's configured default pair. The caller decides whether this source is eligible.
 *
 * @param mixed $stock_id Item code
 * @return array axis => configured identity
 */
function inventory_classification_item_defaults($stock_id)
{
	if (!is_string($stock_id) || $stock_id === '' || strlen($stock_id) > 20)
		return array(1 => 0, 2 => 0);
	$rows = db_query('SELECT dimension_id,dimension2_id FROM '.TB_PREF.'stock_master WHERE stock_id='
		.db_escape($stock_id));
	if (!$rows)
		throw new DimensionFinancialReferenceWriteException(
			_('The inventory item classification defaults cannot be read.'));
	$row = db_fetch_assoc($rows);
	return $row ? array(
		1 => (isset($row['dimension_id']) ? (int)$row['dimension_id'] : 0),
		2 => (isset($row['dimension2_id']) ? (int)$row['dimension2_id'] : 0)) : array(1 => 0, 2 => 0);
}

/**
 * Build both explicit axis intents from posted values.
 *
 * @param mixed $id1 Selected identity for axis 1 (zero = inherit)
 * @param mixed $id2 Selected identity for axis 2 (zero = inherit)
 * @param mixed $blank1 Explicit-unassigned marker for axis 1
 * @param mixed $blank2 Explicit-unassigned marker for axis 2
 * @param string $origin Decision origin
 * @return array|false axis => builder result, or false for a conflicting/invalid pair
 */
function inventory_classification_intents_from_values($id1, $id2, $blank1, $blank2, $origin)
{
	$intents = array();
	foreach (array(1 => array($id1, $blank1), 2 => array($id2, $blank2)) as $axis => $pair) {
		// A blank/absent posted selector is the "inherit" option of the list, never an invalid value.
		$identity = dimension_source_revision_int($pair[0] === '' || $pair[0] === null ? 0 : $pair[0], 0);
		if ($identity === false || (!empty($pair[1]) && $identity !== 0))
			return false;
		if (!empty($pair[1]))
			$intents[$axis] = dimension_resolution_unassigned($origin);
		elseif ($identity > 0)
			$intents[$axis] = dimension_resolution_selected($identity, $origin);
		else
			$intents[$axis] = dimension_resolution_inherit($origin);
		if (empty($intents[$axis]['ok']))
			return false;
	}
	return $intents;
}

/**
 * Normalize one intent stored on a cart, line, or approval payload.
 *
 * A reloaded or approval-restored carrier holds persisted evidence; it is admitted through the
 * snapshot shape so a later capture converges instead of re-claiming it as an explicit choice.
 *
 * @param object|array|null $carrier Carrier holding classification_intents
 * @param int $axis Native axis
 * @param string $persisted_kind Snapshot kind used for persisted evidence
 * @param string $explicit_kind Kind used for an explicit entry
 * @return array|null Normalized builder result, or null when no mode was recorded
 */
function inventory_classification_carried_intent($carrier, $axis, $persisted_kind, $explicit_kind)
{
	$intents = is_object($carrier) && isset($carrier->classification_intents)
		? $carrier->classification_intents
		: (is_array($carrier) && isset($carrier['classification_intents'])
			? $carrier['classification_intents'] : null);
	if (!is_array($intents) || !array_key_exists((int)$axis, $intents))
		return null;
	$raw = $intents[(int)$axis];
	$kind = is_array($raw) && !empty($raw['persisted']) ? $persisted_kind : $explicit_kind;
	return dimension_resolution_normalize_source($raw, $kind);
}

/**
 * Expose the carrier's decision to a page without re-deriving it from a current default.
 *
 * @param object|null $carrier Cart or line carrier
 * @return array ids and unassigned markers per axis
 */
function inventory_classification_carrier_values($carrier)
{
	$values = array('ids' => array(1 => 0, 2 => 0), 'unassigned' => array(1 => 0, 2 => 0));
	foreach (array(1, 2) as $axis) {
		$intent = inventory_classification_carried_intent($carrier, $axis, 'header_snapshot', 'header_explicit');
		if (!is_array($intent) || empty($intent['ok']))
			continue;
		if ($intent['intent']['mode'] === 'selected')
			$values['ids'][$axis] = (int)$intent['intent']['id'];
		elseif ($intent['intent']['mode'] === 'unassigned')
			$values['unassigned'][$axis] = 1;
	}
	return $values;
}

/**
 * Apply the posted header category decision of an adjustment page to its cart.
 *
 * @param object $cart Inventory cart
 * @return bool False for a conflicting posted pair
 */
function inventory_classification_apply_header_post($cart)
{
	if (!is_object($cart) || !isset($_POST['inv_dimension_id']) && !isset($_POST['inv_dimension2_id'])
		&& !isset($_POST['_inv_classification_unassigned_1']) && !isset($_POST['_inv_classification_unassigned_2']))
		return true;
	$intents = inventory_classification_intents_from_values(
		get_post('inv_dimension_id', 0), get_post('inv_dimension2_id', 0),
		get_post('_inv_classification_unassigned_1', 0), get_post('_inv_classification_unassigned_2', 0),
		'inventory_header');
	if ($intents === false)
		return false;
	$cart->classification_intents = $intents;
	return true;
}

/**
 * Stack of the posting contexts an inventory source is currently writing.
 *
 * The shared negative-stock helpers are called from deep inside another writer. The triggering
 * writer publishes its resolved pair here so the correction it provokes keeps its source lineage
 * instead of falling back to the current item default.
 *
 * @param string $action push, pop or current
 * @param array|null $context type, number, stock_id, axes
 * @return array|null
 */
function inventory_classification_context($action, $context = null)
{
	static $stack = array();
	if ($action === 'push') {
		$stack[] = $context;
		return $context;
	}
	if ($action === 'pop')
		return array_pop($stack);
	return count($stack) ? $stack[count($stack) - 1] : null;
}

/**
 * Resolve both axes for one inventory source entry and return its exact resolver evidence.
 *
 * @param int $trans_type Inventory source type
 * @param object|null $header Header carrier (cart or standalone object)
 * @param object|null $line Line carrier, or null for a header
 * @param bool $new_line Whether the item default is eligible
 * @param array $context original (axis => stored value or 0, a preserved posting pair), evidence
 * @return array ok, pair, axes, resolution, errors and codes
 */
function inventory_classification_resolve_entry($trans_type, $header, $line, $new_line, $context = array())
{
	$result = array('ok' => false, 'pair' => null, 'axes' => array(), 'resolution' => null,
		'errors' => array(), 'codes' => array());
	if (!inventory_classification_type_supported($trans_type)) {
		$result['codes'][] = 'unsupported_source';
		$result['errors'][] = _('This inventory source cannot retain classification intent.');
		return $result;
	}
	$posts = inventory_classification_posts_cost($trans_type);
	$item = $line !== null && is_object($line) && $posts
		? inventory_classification_item_defaults(isset($line->stock_id) ? $line->stock_id : '')
		: array(1 => 0, 2 => 0);
	$sources = array(1 => array(), 2 => array());
	foreach (array(1, 2) as $axis) {
		$line_intent = $line !== null
			? inventory_classification_carried_intent($line, $axis, 'line_snapshot', 'line_explicit') : null;
		$own_snapshot = false;
		if (is_array($line_intent) && count($line_intent)) {
			if (!empty($line_intent['intent']['persisted'])) {
				$sources[$axis]['line_snapshot'] = $line_intent;
				$own_snapshot = true;
			} else {
				$sources[$axis]['line_explicit'] = $line_intent;
			}
		}
		if (!isset($sources[$axis]['line_explicit']) && !$own_snapshot && isset($context['original'])
			&& is_array($context['original']) && array_key_exists($axis, $context['original'])) {
			$original = dimension_source_revision_int($context['original'][$axis], 0);
			$sources[$axis]['line_explicit'] = $original !== false && $original > 0
				? dimension_resolution_selected($original, 'original_posting')
				: dimension_resolution_unassigned('original_posting');
		}
		$header_intent = inventory_classification_carried_intent($header, $axis, 'header_snapshot', 'header_explicit');
		if (is_array($header_intent) && !empty($header_intent['intent']['persisted']))
			$sources[$axis]['header_snapshot'] = $header_intent;
		else
			$sources[$axis]['header_explicit'] = $header_intent === null
				? dimension_resolution_inherit('inventory_header') : $header_intent;
		if ($new_line && $posts)
			$sources[$axis]['item_default'] = dimension_resolution_default_intent($item[$axis], 'item_default');
	}
	$enabled = (int)get_company_pref('use_dimension');
	if ($posts) {
		$pair = dimension_resolution_pair($sources[1], $sources[2], array('enabled_axes' => $enabled));
	} else {
		// A location transfer has no ledger effect: resolve the evidence without the company
		// requiredness rules, and admit only an explicitly selected identity intrinsically.
		$pair = dimension_resolution_pair_values($sources[1], $sources[2], array('enabled_axes' => $enabled));
		$pair['policy_version'] = 0;
		if ($pair['ok'])
			foreach (array(1, 2) as $axis)
				if ($pair['axes'][$axis]['mode'] === 'selected') {
					$admission = dimension_posting_admission(
						$axis === 1 ? $pair['axes'][1]['value'] : 0, $axis === 2 ? $pair['axes'][2]['value'] : 0);
					if (!$admission['ok']) {
						$pair['ok'] = false;
						$pair['codes'] = array_merge($pair['codes'], $admission['codes']);
						$pair['errors'] = array_merge($pair['errors'], $admission['errors']);
					}
				}
	}
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
				'value' => 0, 'mode' => 'not_applicable', 'origin' => 'policy', 'persisted' => false,
				'policy_version' => $policy_version, 'basis' => 'axis_not_enabled'));
		} else {
			$pair['axes'][$axis] = array_merge($pair['axes'][$axis], array(
				'value' => 0, 'mode' => 'unassigned',
				'origin' => $axis > $enabled ? 'axis_disabled' : 'inventory_unassigned',
				'persisted' => false, 'policy_version' => 0, 'basis' => ''));
		}
	}
	$resolution = array(
		'contract' => 'inventory-classification-resolution/1',
		'trans_type' => (int)$trans_type,
		'posting_effect' => $posts ? 'cost_recognition' : 'none',
		'new_line' => (bool)$new_line,
		'enabled_axes' => $enabled,
		'policy_version' => (int)$pair['policy_version'],
		'axes' => array(1 => $pair['axes'][1], 2 => $pair['axes'][2]));
	if ($line !== null && is_object($line) && isset($line->stock_id))
		$resolution['stock_id'] = (string)$line->stock_id;
	if (isset($context['evidence']) && is_array($context['evidence']))
		$resolution['evidence'] = $context['evidence'];
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
function inventory_classification_revision_equivalent($current, $fields)
{
	foreach (array('dimension_id', 'dimension2_id', 'mode1', 'mode2', 'origin1', 'origin2',
		'basis1', 'basis2', 'policy1_version', 'policy2_version', 'effective_date',
		'line_quantity') as $name) {
		$current_value = array_key_exists($name, $current) ? $current[$name] : null;
		if ((string)$current_value !== (string)$fields[$name])
			return false;
	}
	$current_resolution = json_decode($current['resolution_payload'], true);
	$candidate_resolution = json_decode($fields['resolution_payload'], true);
	if (!is_array($current_resolution) || !is_array($candidate_resolution))
		return false;
	foreach (array('new_line', 'binding') as $volatile)
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
 * @return array ok, revision_id, revision, created, errors and codes
 */
function inventory_classification_persist_revision($trans_type, $document_key, $line_kind, $line_key,
	$resolution, $effective_date, $line_quantity)
{
	$result = array('ok' => false, 'revision_id' => 0, 'revision' => null,
		'created' => false, 'errors' => array(), 'codes' => array());
	if (!is_array($resolution) || empty($resolution['axes'][1]) || empty($resolution['axes'][2])) {
		$result['codes'][] = 'invalid_resolution';
		$result['errors'][] = _('The inventory classification resolution is incomplete.');
		return $result;
	}
	$options = array('effective_date' => $effective_date, 'line_quantity' => $line_quantity,
		'resolution' => $resolution);
	$prepared = dimension_source_revision_prepare('inventory', $trans_type, $document_key, $line_kind,
		$line_key, $resolution['axes'], $options);
	if (!$prepared['ok']) {
		$result['codes'] = $prepared['codes'];
		$result['errors'] = $prepared['errors'];
		return $result;
	}
	$current = dimension_source_revision_snapshot('inventory', $trans_type, $document_key,
		$line_kind, $line_key);
	if (!in_array($current['state'], array('available', 'absent', 'unavailable'), true)) {
		$result['codes'] = !empty($current['codes']) ? $current['codes'] : array('source_unreadable');
		$result['errors'] = !empty($current['errors']) ? $current['errors']
			: array(_('The current inventory classification revision cannot be verified.'));
		return $result;
	}
	if ($current['state'] === 'available'
		&& inventory_classification_revision_equivalent($current['revision'], $prepared['fields'])) {
		$result['ok'] = true;
		$result['revision_id'] = (int)$current['revision']['source_revision_id'];
		$result['revision'] = $current['revision'];
		return $result;
	}
	if ($current['state'] === 'available') {
		$options['predecessor_revision_id'] = (int)$current['revision']['source_revision_id'];
		$prepared = dimension_source_revision_prepare('inventory', $trans_type, $document_key, $line_kind,
			$line_key, $resolution['axes'], $options);
		if (!$prepared['ok']) {
			$result['codes'] = $prepared['codes'];
			$result['errors'] = $prepared['errors'];
			return $result;
		}
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
 * Ensure one inventory cart has stable draft identities for its header and lines.
 *
 * @param object $header Header carrier
 * @param array $items Line carriers
 * @return string Durable draft key
 */
function inventory_classification_ensure_draft_identity($header, $items)
{
	if (!is_object($header) || !is_array($items))
		throw new DimensionFinancialReferenceWriteException(_('The inventory classification carrier is unavailable.'));
	if (!isset($header->classification_draft_token) || !is_string($header->classification_draft_token)
		|| $header->classification_draft_token === '')
		$header->classification_draft_token = inventory_classification_token('invdraft');
	$key = dimension_source_revision_draft_key($header->classification_draft_token);
	if ($key === false)
		throw new DimensionFinancialReferenceWriteException(_('The inventory draft classification identity is invalid.'));
	foreach ($items as $line) {
		if (!is_object($line))
			throw new DimensionFinancialReferenceWriteException(_('An inventory classification line is invalid.'));
		if (!isset($line->classification_token) || !is_string($line->classification_token)
			|| $line->classification_token === '')
			$line->classification_token = inventory_classification_token('line');
	}
	return $key;
}

/**
 * Bind one sealed approval draft revision to its final source identity and return its axes.
 *
 * @param int $draft_id Stored draft revision
 * @param int $trans_type Source type
 * @param string $document_key Final document key
 * @param string $line_kind header or line
 * @param string $line_key Final line key
 * @param string $line_quantity Canonical quantity
 * @return array ok, revision_id, axes, errors
 */
function inventory_classification_bind_draft($draft_id, $trans_type, $document_key, $line_kind,
	$line_key, $line_quantity)
{
	$result = array('ok' => false, 'revision_id' => 0, 'axes' => array(), 'errors' => array());
	$draft = dimension_source_revision_read($draft_id);
	if (!is_array($draft) || $draft['state'] !== 'available'
		|| $draft['revision']['source_family'] !== 'inventory'
		|| (int)$draft['revision']['trans_type'] !== (int)$trans_type) {
		$result['errors'][] = _('The sealed inventory classification draft is missing or does not verify.');
		return $result;
	}
	$resolution = json_decode($draft['revision']['resolution_payload'], true);
	if (!is_array($resolution)) {
		$result['errors'][] = _('The sealed inventory classification evidence cannot be read.');
		return $result;
	}
	$resolution['binding'] = array('document_key' => $document_key, 'line_kind' => $line_kind,
		'line_key' => $line_key);
	$bound = dimension_source_revision_bind($draft_id, array('document_key' => $document_key,
		'line_kind' => $line_kind, 'line_key' => $line_key, 'resolution' => $resolution,
		'line_quantity' => $line_quantity));
	if (!$bound['ok']) {
		$result['errors'] = $bound['errors'];
		return $result;
	}
	$result['ok'] = true;
	$result['revision_id'] = (int)$bound['revision_id'];
	$result['axes'] = $draft['revision']['axes'];
	return $result;
}

/**
 * Capture the header and every line of one inventory document.
 *
 * With no document number this captures an unsaved draft (an approval submission). With a final
 * number it writes the final source identity: a line that carries a sealed approval draft revision
 * is bound to that draft (so the reviewed pair wins any later item-default change), any other line
 * is resolved as a new event and stored directly.
 *
 * @param int $trans_type Source type
 * @param object $header Header carrier
 * @param array $items Line carriers (line_item objects)
 * @param string $date_ Document date in the display format
 * @param mixed $document_no Final native number, or null for a draft
 * @param array $evidence Bounded scalar evidence sealed with the revisions (location etc.)
 * @param array $original_by_line Optional line index => (axis => stored identity) preserved pair of an
 *                                event that corrects or consumes an earlier posting (DIM-INV-002)
 * @return array document_key, draft, header_revision_id, line_revision_ids, header_axes, axes
 */
function inventory_classification_capture($trans_type, $header, $items, $date_, $document_no = null,
	$evidence = array(), $original_by_line = array())
{
	if (!inventory_classification_type_supported($trans_type) || !is_object($header) || !is_array($items))
		throw new DimensionFinancialReferenceWriteException(_('This inventory document cannot retain classification intent.'));
	$items = array_values($items);
	$draft = $document_no === null;
	$draft_key = inventory_classification_ensure_draft_identity($header, $items);
	$final_key = $draft ? false : dimension_source_revision_document_key($document_no);
	if (!$draft && $final_key === false)
		throw new DimensionFinancialReferenceWriteException(_('The saved inventory source identity is invalid.'));
	$document_key = $draft ? $draft_key : $final_key;
	$effective = date2sql($date_);
	if (dimension_source_revision_date($effective) === false)
		throw new DimensionFinancialReferenceWriteException(_('The inventory source date is invalid.'));
	$context = array('evidence' => is_array($evidence) ? $evidence : array(),
		'original_by_line' => is_array($original_by_line) ? $original_by_line : array());
	$sealed = isset($header->classification_draft_revisions) && is_array($header->classification_draft_revisions)
		? $header->classification_draft_revisions : array();
	$sealed_matches = !$draft && isset($sealed['document_key']) && $sealed['document_key'] === $draft_key;

	// Header.
	$header_axes = null;
	if ($sealed_matches && !empty($sealed['header'])) {
		$bound = inventory_classification_bind_draft((int)$sealed['header'], $trans_type, $final_key,
			'header', 'header', '0.00000000');
		if (!$bound['ok'])
			throw new DimensionFinancialReferenceWriteException(implode(' ', $bound['errors']));
		$header_revision_id = $bound['revision_id'];
		$header_axes = $bound['axes'];
	} else {
		$resolved = inventory_classification_resolve_entry($trans_type, $header, null, false, $context);
		if (!$resolved['ok'])
			throw new DimensionFinancialReferenceWriteException(implode(' ', $resolved['errors']));
		$stored = inventory_classification_persist_revision($trans_type, $document_key, 'header', 'header',
			$resolved['resolution'], $effective, '0.00000000');
		if (!$stored['ok'])
			throw new DimensionFinancialReferenceWriteException(implode(' ', $stored['errors']));
		$header_revision_id = (int)$stored['revision_id'];
		$header_axes = $resolved['axes'];
	}
	$header->classification_revision_id = (int)$header_revision_id;
	$header->classification_lineage_state = 'versioned';
	if ($draft)
		$header->classification_draft_revisions = array('document_key' => $draft_key,
			'header' => (int)$header_revision_id, 'lines' => array());

	// Lines.
	$line_ids = array();
	$axes = array();
	foreach ($items as $index => $line) {
		$token = (string)$line->classification_token;
		$quantity = inventory_classification_quantity(isset($line->quantity) ? $line->quantity : 0);
		$line_context = $context;
		if (isset($line->classification_evidence) && is_array($line->classification_evidence))
			$line_context['evidence'] = array_merge($line_context['evidence'], $line->classification_evidence);
		$line_context['evidence']['line'] = (int)$index + 1;
		if (isset($context['original_by_line'][$index]))
			$line_context['original'] = $context['original_by_line'][$index];
		if ($draft) {
			$line_key = dimension_source_revision_line_key('draft_line', $token);
			if ($line_key === false)
				throw new DimensionFinancialReferenceWriteException(_('An inventory classification line identity is invalid.'));
			$resolved = inventory_classification_resolve_entry($trans_type, $header, $line, true, $line_context);
			if (!$resolved['ok'])
				throw new DimensionFinancialReferenceWriteException(implode(' ', $resolved['errors']));
			$stored = inventory_classification_persist_revision($trans_type, $document_key, 'draft_line',
				$line_key, $resolved['resolution'], $effective, $quantity);
			if (!$stored['ok'])
				throw new DimensionFinancialReferenceWriteException(implode(' ', $stored['errors']));
			$line->draft_line_key = $line_key;
			$line->classification_draft_revision_id = (int)$stored['revision_id'];
			$header->classification_draft_revisions['lines'][$token] = array(
				'revision_id' => (int)$stored['revision_id'], 'line_key' => $line_key);
			$axes[$index] = array(1 => (int)$resolved['axes'][1]['value'], 2 => (int)$resolved['axes'][2]['value']);
			$line_ids[] = (int)$stored['revision_id'];
			continue;
		}
		$final_line_key = dimension_source_revision_line_key('line', (int)$index + 1);
		if ($final_line_key === false)
			throw new DimensionFinancialReferenceWriteException(_('An inventory classification line identity is invalid.'));
		if ($sealed_matches && isset($sealed['lines'][$token]['revision_id'])) {
			$bound = inventory_classification_bind_draft((int)$sealed['lines'][$token]['revision_id'],
				$trans_type, $final_key, 'line', $final_line_key, $quantity);
			if (!$bound['ok'])
				throw new DimensionFinancialReferenceWriteException(implode(' ', $bound['errors']));
			$revision_id = $bound['revision_id'];
			$axes[$index] = array(1 => (int)$bound['axes'][1]['value'], 2 => (int)$bound['axes'][2]['value']);
		} else {
			$resolved = inventory_classification_resolve_entry($trans_type, $header, $line, true, $line_context);
			if (!$resolved['ok'])
				throw new DimensionFinancialReferenceWriteException(implode(' ', $resolved['errors']));
			$stored = inventory_classification_persist_revision($trans_type, $final_key, 'line',
				$final_line_key, $resolved['resolution'], $effective, $quantity);
			if (!$stored['ok'])
				throw new DimensionFinancialReferenceWriteException(implode(' ', $stored['errors']));
			$revision_id = (int)$stored['revision_id'];
			$axes[$index] = array(1 => (int)$resolved['axes'][1]['value'], 2 => (int)$resolved['axes'][2]['value']);
		}
		$line->classification_revision_id = (int)$revision_id;
		$line->classification_lineage_state = 'versioned';
		$line_ids[] = (int)$revision_id;
	}
	$header_values = array(1 => (int)$header_axes[1]['value'], 2 => (int)$header_axes[2]['value']);
	return array('document_key' => $document_key, 'draft' => $draft,
		'header_revision_id' => (int)$header_revision_id, 'line_revision_ids' => $line_ids,
		'header_axes' => $header_values, 'axes' => $axes);
}

/**
 * Build the DIM-APR-001 request from a captured unsaved inventory source.
 *
 * @param array $capture inventory_classification_capture() draft result
 * @return array|null
 */
function inventory_classification_approval_request($capture)
{
	if (!is_array($capture) || empty($capture['draft']) || empty($capture['header_revision_id'])
		|| !isset($capture['line_revision_ids']) || !is_array($capture['line_revision_ids']))
		return null;
	$lines = array();
	foreach ($capture['line_revision_ids'] as $revision_id)
		$lines[] = array('revision_id' => (int)$revision_id);
	return array('source_family' => 'inventory', 'document_key' => $capture['document_key'],
		'header' => array('revision_id' => (int)$capture['header_revision_id']),
		'lines' => $lines);
}

/**
 * Prepare the approval submission context of an adjustment/transfer page.
 *
 * Captures the reviewed header/line intent as pending draft revisions and returns the sealed-request
 * context. An install without the accepted companion schema keeps its legacy approval payload.
 *
 * @param int $trans_type ST_INVADJUST or ST_LOCTRANSFER
 * @param object $cart Page cart
 * @param string $date_ Document date
 * @param array $evidence Bounded document evidence
 * @return array context (possibly empty)
 * @throws DimensionFinancialReferenceWriteException
 */
function inventory_classification_approval_context($trans_type, $cart, $date_, $evidence = array())
{
	if (!inventory_classification_schema_ready())
		return array();
	$capture = inventory_classification_capture($trans_type, $cart, $cart->line_items, $date_, null, $evidence);
	$request = inventory_classification_approval_request($capture);
	if ($request === null)
		throw new DimensionFinancialReferenceWriteException(_('The inventory classification envelope is unavailable.'));
	return array('dimension_snapshot' => $request);
}

/**
 * Find the pair that the corrected deliveries/adjustments were actually posted with.
 *
 * Only a single unambiguous original pair is used; none or a mixed set returns null so the caller
 * keeps the next lineage source instead of guessing.
 *
 * @param string $stock_id Item code
 * @param int $move_id First stock move of the negative period
 * @return array|null axis => stored identity plus `sources` evidence, or null
 */
function inventory_classification_original_pair($stock_id, $move_id)
{
	$move_id = dimension_source_revision_int($move_id, 1);
	if ($move_id === false || !is_string($stock_id) || $stock_id === '')
		return null;
	$rows = db_query('SELECT DISTINCT gl.dimension_id, gl.dimension2_id, m.type, m.trans_no
		FROM '.TB_PREF.'stock_moves m
		INNER JOIN '.TB_PREF.'stock_master sm ON sm.stock_id=m.stock_id
		INNER JOIN '.TB_PREF.'gl_trans gl ON gl.type=m.type AND gl.type_no=m.trans_no
			AND gl.account IN (sm.cogs_account, sm.adjustment_account)
		LEFT JOIN '.TB_PREF.'voided v ON v.type=m.type AND v.id=m.trans_no
		WHERE m.stock_id='.db_escape($stock_id).' AND m.qty<0 AND m.trans_id>='.$move_id.'
			AND ISNULL(v.id) ORDER BY m.type, m.trans_no');
	if (!$rows)
		return null;
	$pairs = array();
	$sources = array();
	while ($row = db_fetch_assoc($rows)) {
		$pairs[(int)$row['dimension_id'].':'.(int)$row['dimension2_id']] = array(
			1 => (int)$row['dimension_id'], 2 => (int)$row['dimension2_id']);
		if (count($sources) < 20)
			$sources[] = (int)$row['type'].':'.(int)$row['trans_no'];
	}
	if (count($pairs) !== 1)
		return null;
	$pair = array_pop($pairs);
	$pair['sources'] = $sources;
	return $pair;
}

/**
 * Resolve the pair of one stock cost correction (cost update or negative-stock handling).
 *
 * Lineage precedence: the pair of the corrected original postings, then the pair of the document
 * that is writing right now, then the item default as a new event. When a preserved original pair
 * no longer passes the current lifecycle admission the next source is used so a goods receipt is
 * never blocked by a retired project.
 *
 * @param int $trans_type Posted document type (ST_COSTUPDATE or ST_JOURNAL)
 * @param string $stock_id Item code
 * @param array|null $original Preserved pair (axis => identity, optional `sources`)
 * @param array $evidence Bounded scalar evidence
 * @return array|null ok result with resolution, or null when the install keeps legacy behavior
 */
function inventory_classification_resolve_correction($trans_type, $stock_id, $original, $evidence)
{
	if (!inventory_classification_schema_ready())
		return null;
	$line = (object)array('stock_id' => $stock_id);
	$header = new stdClass();
	$attempts = array();
	if (is_array($original))
		$attempts[] = array('original' => array(1 => $original[1], 2 => $original[2]),
			'lineage' => 'original_posting');
	$running = inventory_classification_context('current');
	if (is_array($running) && isset($running['stock_id']) && $running['stock_id'] === $stock_id
		&& isset($running['axes']))
		$attempts[] = array('original' => array(1 => $running['axes'][1], 2 => $running['axes'][2]),
			'lineage' => 'triggering_document', 'trigger' => $running['type'].':'.$running['number']);
	$attempts[] = array('original' => null, 'lineage' => 'item_default');
	foreach ($attempts as $attempt) {
		$context = array('evidence' => array_merge($evidence, array('lineage' => $attempt['lineage'])));
		if (isset($attempt['trigger']))
			$context['evidence']['trigger'] = $attempt['trigger'];
		if ($attempt['lineage'] === 'original_posting' && isset($original['sources']))
			$context['evidence']['original_sources'] = $original['sources'];
		if ($attempt['original'] !== null)
			$context['original'] = $attempt['original'];
		$resolved = inventory_classification_resolve_entry($trans_type, $header, $line, true, $context);
		if ($resolved['ok'])
			return $resolved;
		$last = $resolved;
	}
	return $last;
}

/**
 * Persist the final revision of one posted cost-correction document.
 *
 * @param int $trans_type Posted document type
 * @param mixed $document_no Posted document number
 * @param string $date_ Posting date in the display format
 * @param array $resolved inventory_classification_resolve_correction() result
 * @param mixed $quantity Quantity the correction applies to
 * @return int Header-level source revision identity of the line
 * @throws DimensionFinancialReferenceWriteException
 */
function inventory_classification_record_correction($trans_type, $document_no, $date_, $resolved, $quantity)
{
	$document_key = dimension_source_revision_document_key($document_no);
	$effective = date2sql($date_);
	if ($document_key === false || dimension_source_revision_date($effective) === false)
		throw new DimensionFinancialReferenceWriteException(_('The inventory correction source identity is invalid.'));
	$header_resolution = $resolved['resolution'];
	unset($header_resolution['stock_id']);
	$header = inventory_classification_persist_revision($trans_type, $document_key, 'header', 'header',
		$header_resolution, $effective, '0.00000000');
	if (!$header['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $header['errors']));
	$line = inventory_classification_persist_revision($trans_type, $document_key, 'line',
		dimension_source_revision_line_key('line', 1), $resolved['resolution'], $effective,
		inventory_classification_quantity($quantity));
	if (!$line['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $line['errors']));
	return (int)$line['revision_id'];
}

/**
 * Cart/approval carrier fields that travel with an inventory draft.
 *
 * @return string[]
 */
function inventory_classification_header_fields()
{
	return array('classification_intents', 'classification_draft_token', 'classification_draft_revisions',
		'classification_revision_id', 'classification_lineage_state');
}

/**
 * Line carrier fields that travel with an inventory draft.
 *
 * @return string[]
 */
function inventory_classification_line_fields()
{
	return array('classification_intents', 'classification_token', 'draft_line_key',
		'classification_draft_revision_id', 'classification_revision_id', 'classification_lineage_state',
		'classification_evidence');
}
