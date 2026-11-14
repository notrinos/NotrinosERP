<?php
/**
 * DIM-CRM-001: explicit lead conversion and opportunity attribution.
 *
 * A CRM lead is not a Dimension and a customer's Dimension default is not a CRM fact. This adapter keeps
 * the two apart and makes every crossing explicit:
 *
 *  - converting a lead to a customer no longer writes a literal 0/0 classification behind the operator's
 *    back. The operator chooses, per enabled category, a value or an explicit "unassigned"; nothing is
 *    inferred from the lead, its name, its source or any other customer. The decision is validated through
 *    the shared DIM-FND-004 contract and sealed as a durable DIM-FND-006 `sales` source revision
 *    (anchor `CRM_CONVERSION_ANCHOR`, one identity per lead, header line) naming the lead, the customer and
 *    the follow-up it still owes;
 *  - when the company declares a category required and the operator leaves it unassigned, the conversion
 *    still keeps the customer identity but the revision carries an explicit follow-up exception, and the
 *    follow-up queue lists every customer whose required category is still unassigned. The queue is derived
 *    from the verified revisions and the customer's current pair, so it empties when the customer is
 *    classified and never needs a second source of truth;
 *  - the conversion is serialized on the lead row, so a repeated or concurrent conversion cannot create a
 *    second customer: it returns the customer that already exists. An existing customer can be linked
 *    explicitly instead of being duplicated, and that link never copies, overwrites or guesses the
 *    customer's classification;
 *  - an opportunity keeps its customer, Sales agreement and lead-source links as CRM attribution evidence
 *    (anchor `CRM_OPPORTUNITY_ANCHOR`). The link never reads or copies the customer's Dimension, never
 *    claims a native project or cost center, and a customer identity established by the conversion cannot
 *    be silently replaced.
 *
 * The customer itself is written through the maintained `add_customer()` master writer, so the DIM-B02
 * master protocol (open, enabled, correct-axis Dimension) applies. The adapter writes no ledger row, no
 * GL account and no Dimension master; it never backfills or remaps a historical customer or lead. A
 * company without the accepted companion schema keeps the legacy conversion (customer with the explicit
 * choice, no revision, no queue).
 *
 * PHP 5.6 compatible.
 */
require_once __DIR__.'/../../../dimensions/includes/dimension_source_revision.inc';
require_once __DIR__.'/../../../dimensions/includes/dimension_category_policy.inc';

if (!defined('CRM_CONVERSION_ANCHOR'))
	define('CRM_CONVERSION_ANCHOR', 9058);
if (!defined('CRM_OPPORTUNITY_ANCHOR'))
	define('CRM_OPPORTUNITY_ANCHOR', 9059);
if (!defined('CRM_CONVERSION_FAMILY'))
	define('CRM_CONVERSION_FAMILY', 'sales');
if (!defined('CRM_CONVERSION_MAX_REVISIONS'))
	define('CRM_CONVERSION_MAX_REVISIONS', 50);
if (!defined('CRM_CONVERSION_QUEUE_LIMIT'))
	define('CRM_CONVERSION_QUEUE_LIMIT', 200);
if (!defined('CRM_CONVERSION_CUSTOMER_AREA'))
	define('CRM_CONVERSION_CUSTOMER_AREA', 'SA_CUSTOMER');

/**
 * Is the accepted companion revision schema installed for the active company?
 *
 * @return bool
 */
function crm_conversion_ready()
{
	return dimension_source_revision_schema_ready() === true;
}

/**
 * May the active user create or link a customer (the authoritative customer-master writer area)?
 *
 * @return bool
 */
function crm_conversion_can_manage_customer()
{
	return function_exists('user_check_access') && user_check_access(CRM_CONVERSION_CUSTOMER_AREA);
}

/**
 * Number of Dimension categories the company enables for new entries.
 *
 * @return int 0, 1 or 2
 */
function crm_conversion_enabled_axes()
{
	$enabled = (int)get_company_pref('use_dimension');
	return $enabled < 0 ? 0 : ($enabled > 2 ? 2 : $enabled);
}

/**
 * Enabled categories the company policy declares required for new events.
 *
 * @return int[] Required axes
 */
function crm_conversion_required_axes()
{
	$required = array();
	$policy = function_exists('dimension_category_policy_current') ? dimension_category_policy_current() : array();
	$enabled = crm_conversion_enabled_axes();
	foreach (array(1, 2) as $axis)
		if ($axis <= $enabled && !empty($policy['axes'][$axis]['required']))
			$required[] = $axis;
	return $required;
}

/**
 * Build the operator's explicit decision per category from posted values.
 *
 * A blank selector is "not decided yet", never "inherit a default": there is no default source for a
 * converted lead. An explicit unassigned marker cannot be combined with a selection.
 *
 * @param mixed $id1 Selected category 1 identity or blank
 * @param mixed $id2 Selected category 2 identity or blank
 * @param mixed $blank1 Explicit-unassigned marker for category 1
 * @param mixed $blank2 Explicit-unassigned marker for category 2
 * @return array|false axis => mode (selected|unassigned|undecided) and id, or false for a conflicting/invalid pair
 */
function crm_conversion_decision_from_values($id1, $id2, $blank1, $blank2)
{
	$decision = array();
	foreach (array(1 => array($id1, $blank1), 2 => array($id2, $blank2)) as $axis => $pair) {
		$identity = dimension_source_revision_int($pair[0] === '' || $pair[0] === null ? 0 : $pair[0], 0);
		if ($identity === false || (!empty($pair[1]) && $identity !== 0))
			return false;
		if (!empty($pair[1]))
			$decision[$axis] = array('mode' => 'unassigned', 'id' => 0);
		elseif ($identity > 0)
			$decision[$axis] = array('mode' => 'selected', 'id' => $identity);
		else
			$decision[$axis] = array('mode' => 'undecided', 'id' => 0);
	}
	return $decision;
}

/**
 * Resolve the operator's decision through the shared resolver and validation contract.
 *
 * Every enabled category needs a decision. A category the company requires may be left explicitly
 * unassigned: that is recorded as a follow-up exception instead of being refused, so the conversion keeps
 * the customer identity and the owed classification stays visible. Every other rule (closed or wrong-axis
 * value, disabled category, declared value pair) refuses the whole conversion.
 *
 * @param array|false $decision crm_conversion_decision_from_values() result
 * @param array $evidence Bounded scalar evidence sealed into the resolution
 * @return array ok, pair, follow_up (axes), resolution, errors and codes
 */
function crm_conversion_resolve($decision, $evidence)
{
	$result = array('ok' => false, 'pair' => array(1 => 0, 2 => 0), 'follow_up' => array(),
		'resolution' => null, 'errors' => array(), 'codes' => array());
	if (!is_array($decision) || !isset($decision[1], $decision[2])) {
		$result['codes'][] = 'invalid_decision';
		$result['errors'][] = _('A category cannot be both selected and explicitly unassigned.');
		return $result;
	}
	$enabled = crm_conversion_enabled_axes();
	$ready = crm_conversion_ready();
	$sources = array(1 => array(), 2 => array());
	foreach (array(1, 2) as $axis) {
		$entry = $decision[$axis];
		if ($axis > $enabled) {
			if ($entry['mode'] !== 'undecided') {
				$result['codes'][] = 'axis_not_enabled';
				$result['errors'][] = sprintf(_('Category %d is not enabled for new entries in this company.'), $axis);
			}
			$sources[$axis]['header_explicit'] = dimension_resolution_inherit('crm_conversion');
			continue;
		}
		if ($entry['mode'] === 'undecided' && $ready) {
			$result['codes'][] = 'decision_required';
			$result['errors'][] = sprintf(_('Choose a category or explicitly leave it unassigned for follow-up for category %d: it is never inferred from the lead or another customer.'), $axis);
			$sources[$axis]['header_explicit'] = dimension_resolution_inherit('crm_conversion');
			continue;
		}
		// A company without the companion schema keeps the legacy conversion: an undecided category is blank.
		$sources[$axis]['header_explicit'] = $entry['mode'] === 'selected'
			? dimension_resolution_selected($entry['id'], 'crm_conversion')
			: dimension_resolution_unassigned('crm_conversion');
	}
	if (count($result['errors']))
		return $result;
	$pair = dimension_resolution_pair_values($sources[1], $sources[2], array('enabled_axes' => $enabled));
	if (!$pair['ok']) {
		$result['codes'] = $pair['codes'];
		$result['errors'] = $pair['errors'];
		return $result;
	}
	$policy = function_exists('dimension_category_policy_current') ? dimension_category_policy_current() : array('version' => 0);
	$policy_version = isset($policy['version']) ? (int)$policy['version'] : 0;
	$axes = $pair['axes'];
	foreach (array(1, 2) as $axis) {
		if ($axes[$axis]['mode'] !== 'inherit')
			continue;
		if ($axis > $enabled && $policy_version > 0)
			$axes[$axis] = array_merge($axes[$axis], array('value' => 0, 'mode' => 'not_applicable', 'origin' => 'policy',
				'persisted' => false, 'policy_version' => $policy_version, 'basis' => 'axis_not_enabled'));
		else
			$axes[$axis] = array_merge($axes[$axis], array('value' => 0, 'mode' => 'unassigned', 'origin' => 'axis_disabled',
				'persisted' => false, 'policy_version' => 0, 'basis' => ''));
	}
	$validation = dimension_validation_classification($axes[1]['value'], $axes[2]['value'], 'new_event');
	$required_codes = 0;
	foreach ($validation['codes'] as $index => $code) {
		if ($code === 'required') {
			$required_codes++;
			continue;
		}
		$result['codes'][] = $code;
		$result['errors'][] = isset($validation['errors'][$index]) ? $validation['errors'][$index] : $code;
	}
	foreach (crm_conversion_required_axes() as $axis)
		if ($axes[$axis]['mode'] === 'unassigned')
			$result['follow_up'][] = $axis;
	if ($required_codes !== count($result['follow_up'])) {
		$result['codes'][] = 'required_unresolved';
		$result['errors'][] = _('A required category could not be resolved. Choose a value or leave it unassigned for follow-up.');
	}
	if (count($result['errors']))
		return $result;
	$validated_version = isset($validation['policy_version']) ? (int)$validation['policy_version'] : 0;
	foreach (array(1, 2) as $axis)
		if (in_array($axes[$axis]['mode'], array('selected', 'unassigned'), true) && (int)$axes[$axis]['policy_version'] === 0)
			$axes[$axis]['policy_version'] = $validated_version;
	$resolution = array(
		'contract' => 'crm-conversion-resolution/1',
		'enabled_axes' => $enabled,
		'policy_version' => $validated_version,
		'follow_up_axes' => $result['follow_up'],
		'axes' => array(1 => $axes[1], 2 => $axes[2]));
	if (is_array($evidence))
		$resolution['evidence'] = $evidence;
	$result['ok'] = true;
	$result['pair'] = array(1 => (int)$axes[1]['value'], 2 => (int)$axes[2]['value']);
	$result['resolution'] = $resolution;
	return $result;
}

/**
 * Preview a customer conversion decision without writing anything.
 *
 * @param array|false $decision crm_conversion_decision_from_values() result
 * @return array ok, pair, follow_up, errors and codes
 */
function crm_conversion_preview($decision)
{
	return crm_conversion_resolve($decision, array('event' => 'customer_conversion_preview'));
}

/**
 * Persist one CRM revision, converging on an identical retry and appending a predecessor on change.
 *
 * @param int $anchor Conversion or opportunity anchor
 * @param int $lead_id Lead identity (document key)
 * @param array $resolution Resolved axes and evidence
 * @param string $effective_date ISO date
 * @return array ok, revision_id, created
 * @throws DimensionFinancialReferenceWriteException A refused or unverifiable revision; nothing is stored
 */
function crm_conversion_persist_revision($anchor, $lead_id, $resolution, $effective_date)
{
	$key = dimension_source_revision_document_key($lead_id);
	if ($key === false)
		throw new DimensionFinancialReferenceWriteException(_('The CRM lead identity is invalid. Nothing was saved.'));
	$options = array('effective_date' => $effective_date, 'line_quantity' => '0.00000000', 'resolution' => $resolution);
	$prepared = dimension_source_revision_prepare(CRM_CONVERSION_FAMILY, $anchor, $key, 'header', 'header',
		$resolution['axes'], $options);
	if (!$prepared['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $prepared['errors']));
	$current = dimension_source_revision_snapshot(CRM_CONVERSION_FAMILY, $anchor, $key, 'header', 'header');
	if (!in_array($current['state'], array('available', 'absent', 'unavailable'), true))
		throw new DimensionFinancialReferenceWriteException(
			_('The recorded CRM classification cannot be verified. Nothing was saved.'));
	if ($current['state'] === 'available') {
		if ((string)$current['revision']['resolution_payload'] === (string)$prepared['fields']['resolution_payload']
			&& (string)$current['revision']['effective_date'] === (string)$prepared['fields']['effective_date'])
			return array('ok' => true, 'revision_id' => (int)$current['revision']['source_revision_id'], 'created' => false);
		$options['predecessor_revision_id'] = (int)$current['revision']['source_revision_id'];
		$prepared = dimension_source_revision_prepare(CRM_CONVERSION_FAMILY, $anchor, $key, 'header', 'header',
			$resolution['axes'], $options);
		if (!$prepared['ok'])
			throw new DimensionFinancialReferenceWriteException(implode(' ', $prepared['errors']));
	}
	$stored = dimension_source_revision_persist($prepared);
	if (!$stored['ok'])
		throw new DimensionFinancialReferenceWriteException(implode(' ', $stored['errors']));
	return array('ok' => true, 'revision_id' => (int)$stored['revision_id'], 'created' => !empty($stored['created']));
}

/**
 * Every verified revision of one lead and anchor, oldest first.
 *
 * @param int $lead_id Lead identity
 * @param int $anchor Conversion or opportunity anchor
 * @return array list of revision_no, revision_id, pair (1, 2), effective (ISO) and evidence
 * @throws DimensionFinancialReferenceWriteException A present but unverifiable revision
 */
function crm_conversion_history($lead_id, $anchor)
{
	$chain = array();
	$key = dimension_source_revision_document_key($lead_id);
	if ($key === false || !crm_conversion_ready())
		return $chain;
	for ($number = 1; $number <= CRM_CONVERSION_MAX_REVISIONS; $number++) {
		$snapshot = dimension_source_revision_snapshot(CRM_CONVERSION_FAMILY, $anchor, $key, 'header', 'header',
			array('revision_no' => $number));
		if ($snapshot['state'] === 'absent' || $snapshot['state'] === 'unavailable')
			break;
		if ($snapshot['state'] !== 'available')
			throw new DimensionFinancialReferenceWriteException(
				_('The recorded CRM classification cannot be verified. Nothing was saved.'));
		$revision = $snapshot['revision'];
		$resolution = json_decode($revision['resolution_payload'], true);
		$chain[] = array('revision_no' => $number, 'revision_id' => (int)$revision['source_revision_id'],
			1 => (int)$revision['dimension_id'], 2 => (int)$revision['dimension2_id'],
			'effective' => (string)$revision['effective_date'],
			'follow_up_axes' => is_array($resolution) && isset($resolution['follow_up_axes']) ? $resolution['follow_up_axes'] : array(),
			'evidence' => is_array($resolution) && isset($resolution['evidence']) ? $resolution['evidence'] : array());
	}
	return $chain;
}

/**
 * Read and lock one lead row for the rest of the transaction.
 *
 * @param int $lead_id Lead identity
 * @return array|null|false The lead row, null when it does not exist, false when the lock could not be taken
 */
function crm_conversion_lock_lead($lead_id)
{
	$lead_id = dimension_source_revision_int($lead_id, 1);
	if ($lead_id === false)
		return null;
	$rows = db_query('SELECT id, lead_ref, lead_status, is_opportunity, linked_customer_id, linked_person_id, lead_source_id, sales_team_id
		FROM '.TB_PREF.'crm_leads WHERE id='.$lead_id.' FOR UPDATE');
	if (!$rows)
		return false;
	$row = db_fetch_assoc($rows);
	return $row ? $row : null;
}

/**
 * Read one customer for linking (shared lock), never its notes or contacts.
 *
 * @param int $customer_id Customer identity
 * @return array|null debtor_no, inactive and the current native pair, or null
 */
function crm_conversion_read_customer($customer_id)
{
	$customer_id = dimension_source_revision_int($customer_id, 1);
	if ($customer_id === false)
		return null;
	$rows = db_query('SELECT debtor_no, inactive, dimension_id, dimension2_id FROM '.TB_PREF
		.'debtors_master WHERE debtor_no='.$customer_id.' LOCK IN SHARE MODE');
	$row = $rows ? db_fetch_assoc($rows) : false;
	return $row ? $row : null;
}

/**
 * Convert a lead into a new customer with an explicit classification decision.
 *
 * The lead row is locked first, so a repeated or concurrent conversion returns the customer that
 * already exists instead of creating a second one. The whole conversion is one transaction: a refused
 * decision, a refused customer write or an unverifiable revision leaves no customer, lead change or
 * revision behind.
 *
 * @param int $lead_id Lead identity
 * @param array $input customer_name, address, sales_type, payment_terms, curr_code, credit_limit, debtor_ref
 * @param array|false $decision crm_conversion_decision_from_values() result
 * @param callable|null $companions function($customer_id, $lead) run in the transaction after the customer exists; returns the CRM person id or 0
 * @return array state (created|already_converted), customer_id, pair, follow_up and revision_id
 * @throws DimensionFinancialReferenceWriteException A refused conversion; nothing is written
 */
function crm_convert_lead_to_customer($lead_id, $input, $decision, $companions = null)
{
	if (!crm_conversion_can_manage_customer())
		throw new DimensionFinancialReferenceWriteException(_('You are not authorized to create customers.'));
	if (!is_array($input) || !isset($input['customer_name']) || trim($input['customer_name']) === ''
		|| (int)$input['sales_type'] < 1 || (int)$input['payment_terms'] < 1)
		throw new DimensionFinancialReferenceWriteException(_('The customer name, sales type and payment terms are required.'));
	if (!begin_transaction())
		throw new DimensionFinancialReferenceWriteException(_('The lead conversion could not start a transaction.'));
	try {
		$lead = crm_conversion_lock_lead($lead_id);
		if ($lead === false)
			throw new DimensionFinancialReferenceWriteException(_('The lead is being changed by another request. Retry in a moment.'));
		if ($lead === null)
			throw new DimensionFinancialReferenceWriteException(_('Lead not found.'));
		if ((int)$lead['linked_customer_id'] > 0) {
			cancel_transaction();
			return array('state' => 'already_converted', 'customer_id' => (int)$lead['linked_customer_id'],
				'pair' => array(1 => 0, 2 => 0), 'follow_up' => array(), 'revision_id' => 0);
		}
		$resolved = crm_conversion_resolve($decision, array('event' => 'customer_conversion', 'lead_id' => (int)$lead['id'],
			'enabled_axes' => crm_conversion_enabled_axes()));
		if (!$resolved['ok'])
			throw new DimensionFinancialReferenceWriteException(implode(' ', $resolved['errors']));
		$created = add_customer(trim($input['customer_name']), $input['debtor_ref'], $input['address'], '',
			$input['curr_code'], $resolved['pair'][1], $resolved['pair'][2], 1, (int)$input['payment_terms'], 0, 0,
			(float)$input['credit_limit'], (int)$input['sales_type'], '');
		$customer_id = $created ? (int)db_insert_id() : 0;
		if ($customer_id < 1)
			throw new DimensionFinancialReferenceWriteException(
				_('The customer could not be created. Check the Dimension selections and retry.'));
		$person_id = is_callable($companions) ? (int)call_user_func($companions, $customer_id, $lead) : 0;
		db_query('UPDATE '.TB_PREF.'crm_leads SET linked_customer_id='.$customer_id.', linked_person_id='
			.($person_id > 0 ? $person_id : 'NULL').", lead_status='converted', date_converted="
			.db_escape(date('Y-m-d H:i:s')).' WHERE id='.(int)$lead['id'], 'could not update lead');
		$revision_id = 0;
		if (crm_conversion_ready()) {
			$resolved['resolution']['evidence']['customer_id'] = $customer_id;
			$resolved['resolution']['evidence']['follow_up_axes'] = $resolved['follow_up'];
			$stored = crm_conversion_persist_revision(CRM_CONVERSION_ANCHOR, $lead['id'], $resolved['resolution'], date('Y-m-d'));
			$revision_id = $stored['revision_id'];
		}
		elseif (count($resolved['follow_up']))
			throw new DimensionFinancialReferenceWriteException(
				_('A required category needs a value: the follow-up cannot be recorded without the Dimension source revision schema.'));
		commit_transaction();
	}
	catch (Exception $exception) {
		cancel_transaction();
		throw $exception;
	}
	return array('state' => 'created', 'customer_id' => $customer_id, 'pair' => $resolved['pair'],
		'follow_up' => $resolved['follow_up'], 'revision_id' => $revision_id);
}

/**
 * Link a lead to an existing customer instead of creating a duplicate.
 *
 * The link is a CRM identity link: the customer's classification is observed as evidence but never
 * copied, overwritten or inferred. A lead already linked to another customer is refused; linking the same
 * customer again converges.
 *
 * @param int $lead_id Lead identity
 * @param int $customer_id Existing, active customer
 * @return array state (linked|already_linked), customer_id and revision_id
 * @throws DimensionFinancialReferenceWriteException A refused link; nothing is written
 */
function crm_convert_lead_link_customer($lead_id, $customer_id)
{
	if (!crm_conversion_can_manage_customer())
		throw new DimensionFinancialReferenceWriteException(_('You are not authorized to link customers.'));
	if (!begin_transaction())
		throw new DimensionFinancialReferenceWriteException(_('The lead link could not start a transaction.'));
	try {
		$lead = crm_conversion_lock_lead($lead_id);
		if ($lead === false)
			throw new DimensionFinancialReferenceWriteException(_('The lead is being changed by another request. Retry in a moment.'));
		if ($lead === null)
			throw new DimensionFinancialReferenceWriteException(_('Lead not found.'));
		$customer = crm_conversion_read_customer($customer_id);
		if ($customer === null || (int)$customer['inactive'] !== 0)
			throw new DimensionFinancialReferenceWriteException(_('Choose an existing, active customer.'));
		$customer_id = (int)$customer['debtor_no'];
		if ((int)$lead['linked_customer_id'] > 0) {
			if ((int)$lead['linked_customer_id'] !== $customer_id)
				throw new DimensionFinancialReferenceWriteException(
					_('The lead is already converted to another customer; the customer identity is not replaced.'));
			cancel_transaction();
			return array('state' => 'already_linked', 'customer_id' => $customer_id, 'revision_id' => 0);
		}
		db_query('UPDATE '.TB_PREF."crm_leads SET linked_customer_id=".$customer_id.", lead_status='converted', date_converted="
			.db_escape(date('Y-m-d H:i:s')).' WHERE id='.(int)$lead['id'], 'could not update lead');
		$revision_id = 0;
		if (crm_conversion_ready()) {
			$unassigned = array('value' => 0, 'mode' => 'unassigned', 'origin' => 'crm_existing_customer_link',
				'persisted' => false, 'policy_version' => 0, 'basis' => '', 'provenance' => array());
			$stored = crm_conversion_persist_revision(CRM_CONVERSION_ANCHOR, $lead['id'], array(
				'contract' => 'crm-conversion-resolution/1', 'enabled_axes' => crm_conversion_enabled_axes(),
				'policy_version' => 0, 'follow_up_axes' => array(),
				'axes' => array(1 => $unassigned, 2 => $unassigned),
				'evidence' => array('event' => 'existing_customer_link', 'lead_id' => (int)$lead['id'],
					'customer_id' => $customer_id, 'classification_decision' => 'none',
					'customer_pair_observed' => array(1 => (int)$customer['dimension_id'], 2 => (int)$customer['dimension2_id']))),
				date('Y-m-d'));
			$revision_id = $stored['revision_id'];
		}
		commit_transaction();
	}
	catch (Exception $exception) {
		cancel_transaction();
		throw $exception;
	}
	return array('state' => 'linked', 'customer_id' => $customer_id, 'revision_id' => $revision_id);
}

/**
 * Was the customer of one lead established by the conversion (created or linked through it)?
 *
 * @param int $lead_id Lead identity
 * @return int Conversion customer identity, or 0
 */
function crm_conversion_customer_of($lead_id)
{
	try {
		$history = crm_conversion_history($lead_id, CRM_CONVERSION_ANCHOR);
	}
	catch (DimensionFinancialReferenceWriteException $exception) {
		return 0;
	}
	if (!count($history))
		return 0;
	$latest = $history[count($history) - 1];
	return isset($latest['evidence']['customer_id']) ? (int)$latest['evidence']['customer_id'] : 0;
}

/**
 * Validate the explicit links of an opportunity before it is saved.
 *
 * Customer and agreement are CRM identity links. The customer must exist, a customer established by the
 * conversion cannot be replaced, and an agreement must belong to that same customer and still be open.
 * The customer's Dimension default is deliberately never read.
 *
 * @param int $lead_id Opportunity lead identity, or 0 for a new opportunity
 * @param int $customer_id Linked customer, or 0 for none
 * @param int $agreement_id Linked Sales agreement, or 0 for none
 * @return array customer_id and agreement_id when valid
 * @throws DimensionFinancialReferenceWriteException A refused link; nothing is written
 */
function crm_opportunity_validate_links($lead_id, $customer_id, $agreement_id)
{
	$customer_id = dimension_source_revision_int($customer_id === '' || $customer_id === null ? 0 : $customer_id, 0);
	$agreement_id = dimension_source_revision_int($agreement_id === '' || $agreement_id === null ? 0 : $agreement_id, 0);
	if ($customer_id === false || $agreement_id === false)
		throw new DimensionFinancialReferenceWriteException(_('The customer or agreement link is not a stored identity.'));
	$conversion_customer = (int)$lead_id > 0 ? crm_conversion_customer_of($lead_id) : 0;
	if ($conversion_customer > 0 && $customer_id !== $conversion_customer)
		throw new DimensionFinancialReferenceWriteException(
			_('The customer identity established by the lead conversion cannot be replaced.'));
	if ($customer_id > 0) {
		$customer = db_fetch_assoc(db_query('SELECT debtor_no, inactive FROM '.TB_PREF.'debtors_master WHERE debtor_no='.$customer_id));
		if (!$customer || ((int)$customer['inactive'] !== 0 && $customer_id !== $conversion_customer))
			throw new DimensionFinancialReferenceWriteException(_('Choose an existing, active customer.'));
	}
	if ($agreement_id > 0) {
		if ($customer_id < 1)
			throw new DimensionFinancialReferenceWriteException(_('Link a customer before linking one of its Sales agreements.'));
		$agreement = db_fetch_assoc(db_query('SELECT id FROM '.TB_PREF.'sales_agreements WHERE id='.$agreement_id.' AND debtor_no='
			.$customer_id." AND inactive=0 AND status IN ('draft','confirmed','active')"));
		if (!$agreement)
			throw new DimensionFinancialReferenceWriteException(
				_('The Sales agreement must be an open agreement of the linked customer.'));
	}
	return array('customer_id' => $customer_id, 'agreement_id' => $agreement_id);
}

/**
 * The attribution links currently recorded for one opportunity.
 *
 * @param int $lead_id Lead identity
 * @return array customer_id, agreement_id and revision_id (all 0 when nothing was recorded)
 */
function crm_opportunity_attribution_current($lead_id)
{
	$links = array('customer_id' => 0, 'agreement_id' => 0, 'revision_id' => 0);
	try {
		$history = crm_conversion_history($lead_id, CRM_OPPORTUNITY_ANCHOR);
	}
	catch (DimensionFinancialReferenceWriteException $exception) {
		return $links;
	}
	if (!count($history))
		return $links;
	$latest = $history[count($history) - 1];
	$links['customer_id'] = isset($latest['evidence']['customer_id']) ? (int)$latest['evidence']['customer_id'] : 0;
	$links['agreement_id'] = isset($latest['evidence']['agreement_id']) ? (int)$latest['evidence']['agreement_id'] : 0;
	$links['revision_id'] = $latest['revision_id'];
	return $links;
}

/**
 * Record the explicit attribution links of an opportunity as durable CRM evidence.
 *
 * The call is idempotent: unchanged links append nothing. It reads the links the caller already
 * validated, stores customer, agreement, source and team as CRM identities, and states that no
 * classification is inferred from any of them. Call it inside the transaction that saved the opportunity.
 *
 * @param int $lead_id Saved opportunity identity
 * @param string $event opportunity_link or opportunity_won
 * @param int $customer_id Linked customer, or 0
 * @param int $agreement_id Linked Sales agreement, or 0
 * @param int $source_id Lead source identity, or 0
 * @param int $team_id Sales team identity, or 0
 * @return array recorded (bool) and revision_id
 * @throws DimensionFinancialReferenceWriteException A refused or unverifiable revision; nothing is stored
 */
function crm_opportunity_record_attribution($lead_id, $event, $customer_id, $agreement_id, $source_id, $team_id)
{
	if (!crm_conversion_ready())
		return array('recorded' => false, 'revision_id' => 0);
	if (!in_array($event, array('opportunity_link', 'opportunity_won'), true))
		throw new DimensionFinancialReferenceWriteException(_('The opportunity attribution event is not registered.'));
	$links = crm_opportunity_validate_links($lead_id, $customer_id, $agreement_id);
	$current = crm_opportunity_attribution_current($lead_id);
	$source_id = max(0, (int)$source_id);
	$team_id = max(0, (int)$team_id);
	if ($event === 'opportunity_link' && $current['revision_id'] > 0
		&& $current['customer_id'] === $links['customer_id'] && $current['agreement_id'] === $links['agreement_id'])
		return array('recorded' => false, 'revision_id' => $current['revision_id']);
	if ($event === 'opportunity_link' && $current['revision_id'] === 0 && $links['customer_id'] === 0)
		return array('recorded' => false, 'revision_id' => 0);
	$unassigned = array('value' => 0, 'mode' => 'unassigned', 'origin' => 'crm_link_only', 'persisted' => false,
		'policy_version' => 0, 'basis' => '', 'provenance' => array());
	$stored = crm_conversion_persist_revision(CRM_OPPORTUNITY_ANCHOR, $lead_id, array(
		'contract' => 'crm-opportunity-attribution/1', 'enabled_axes' => crm_conversion_enabled_axes(),
		'policy_version' => 0, 'follow_up_axes' => array(),
		'axes' => array(1 => $unassigned, 2 => $unassigned),
		'evidence' => array('event' => $event, 'lead_id' => (int)$lead_id, 'customer_id' => $links['customer_id'],
			'agreement_id' => $links['agreement_id'], 'lead_source_id' => $source_id, 'sales_team_id' => $team_id,
			'previous_customer_id' => $current['customer_id'], 'previous_agreement_id' => $current['agreement_id'],
			'classification_decision' => 'not_inferred')),
		date('Y-m-d'));
	return array('recorded' => !empty($stored['created']), 'revision_id' => $stored['revision_id']);
}

/**
 * Customers converted from a lead whose required category is still unassigned.
 *
 * The queue is derived from the verified conversion revisions and the customer's current pair, so a
 * customer leaves it as soon as every owed category is classified. A customer name is returned only to a
 * user who may open customers; the queue never reads another company's rows.
 *
 * @param int $limit Maximum number of conversion identities scanned
 * @return array list of lead_id, lead_ref, customer_id, customer_name (or null), pending axes and converted date
 */
function crm_conversion_follow_up_queue($limit = CRM_CONVERSION_QUEUE_LIMIT)
{
	$queue = array();
	if (!crm_conversion_ready())
		return $queue;
	$limit = max(1, min((int)$limit, CRM_CONVERSION_QUEUE_LIMIT));
	$rows = db_query('SELECT DISTINCT document_key FROM '.TB_PREF.'dimension_source_revisions WHERE source_family='
		.db_escape(CRM_CONVERSION_FAMILY).' AND trans_type='.(int)CRM_CONVERSION_ANCHOR
		." AND line_kind='header' ORDER BY CAST(document_key AS UNSIGNED) DESC LIMIT ".$limit);
	$can_name = crm_conversion_can_manage_customer();
	while ($rows && ($row = db_fetch_assoc($rows))) {
		try {
			$history = crm_conversion_history($row['document_key'], CRM_CONVERSION_ANCHOR);
		}
		catch (DimensionFinancialReferenceWriteException $exception) {
			continue;
		}
		if (!count($history))
			continue;
		$latest = $history[count($history) - 1];
		$customer_id = isset($latest['evidence']['customer_id']) ? (int)$latest['evidence']['customer_id'] : 0;
		if ($customer_id < 1 || !count($latest['follow_up_axes']))
			continue;
		$customer = db_fetch_assoc(db_query('SELECT debtor_no, name, dimension_id, dimension2_id FROM '.TB_PREF
			.'debtors_master WHERE debtor_no='.$customer_id));
		if (!$customer)
			continue;
		$pending = array();
		foreach ($latest['follow_up_axes'] as $axis)
			if ((int)($axis === 2 ? $customer['dimension2_id'] : $customer['dimension_id']) === 0)
				$pending[] = (int)$axis;
		if (!count($pending))
			continue;
		$lead = db_fetch_assoc(db_query('SELECT lead_ref FROM '.TB_PREF.'crm_leads WHERE id='.(int)$row['document_key']));
		$queue[] = array('lead_id' => (int)$row['document_key'], 'lead_ref' => $lead ? $lead['lead_ref'] : '',
			'customer_id' => $customer_id, 'customer_name' => $can_name ? $customer['name'] : null,
			'pending_axes' => $pending, 'converted' => $latest['effective']);
	}
	return $queue;
}
