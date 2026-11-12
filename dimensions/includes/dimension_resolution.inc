<?php
/**********************************************************************
	Copyright (C) NotrinosERP.
	Released under the terms of the GNU General Public License, GPL,
	as published by the Free Software Foundation, either version 3
	of the License, or (at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
	See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/

/**
 * DIM-FND-005: shared ADR-02 deterministic resolution with explicit intent.
 *
 * This is the single resolver for every source adapter that inherits a Dimension value. It owns the
 * ADR-02 precedence order, the four proposed input modes plus the legacy adapter disposition, and
 * the per-axis provenance that explains which input decided a value. It is read-only: it never
 * changes a master, a document, a policy or a ledger row.
 *
 * Precedence, highest first, for each axis independently:
 *  1. explicit line selection (including Explicitly Unassigned);
 *  2. persisted source-line snapshot;
 *  3. explicit header selection (including Explicitly Unassigned);
 *  4. persisted source-header snapshot;
 *  5. configured partner default, then configured item default;
 *  6. no decision (the caller validates requiredness after both axes are resolved).
 *
 * A positive `selected` identity must be a canonical stored whole identity; the shared DIM-FND-004
 * contract still proves that it exists, is on the correct axis, is open, is enabled and satisfies
 * the company's published rules. `unassigned` is a deliberate blank and never falls through to a
 * lower default. `inherit` is the only mode that asks a lower source to decide. `not_applicable` is
 * policy-derived and is accepted only for an axis the company does not enable for new entries.
 *
 * Legacy documents keep the characterized E08/E10 behavior through the adapters lower in this file.
 * A module slice that records explicit modes (DIM-FND-006 and the owning module task) can pass them
 * through the document intent carrier; without that record the resolver reports `legacy_unspecified`
 * and never infers intent from a current customer, supplier or item value.
 */
require_once __DIR__.'/dimension_validation_contract.inc';
require_once __DIR__.'/dimension_financial_reference_writer.inc';

/**
 * The four ADR-02 input modes a publisher may record.
 *
 * @return string[] Stable mode identifiers
 */
function dimension_resolution_modes()
{
	return array('inherit', 'selected', 'unassigned', 'not_applicable');
}

/**
 * The adapter-only disposition for an unsnapshotted legacy document.
 *
 * It is never accepted as proof of intent and never wins a lower default; it is named so reports
 * and tests can distinguish "we do not know" from both inheritance and a deliberate blank.
 *
 * @return string
 */
function dimension_resolution_legacy_mode()
{
	return 'legacy_unspecified';
}

/**
 * The fixed ADR-02 source order.
 *
 * @return string[] Source identifiers, highest precedence first
 */
function dimension_resolution_source_kinds()
{
	return array('line_explicit', 'line_snapshot', 'header_explicit', 'header_snapshot',
		'partner_default', 'item_default');
}

/**
 * Current policy-derived reasons for Not Applicable.
 *
 * At this source boundary the only published rule that makes an axis inapplicable is that the
 * company does not enable it for new entries. A future policy rule must register its own basis here
 * rather than accepting a caller-supplied label.
 *
 * @return string[]
 */
function dimension_resolution_not_applicable_bases()
{
	return array('axis_not_enabled');
}

/**
 * Is one axis a real native Dimension axis?
 *
 * @param mixed $axis Requested axis
 * @return bool
 */
function dimension_resolution_axis_valid($axis)
{
	return $axis === 1 || $axis === 2 || $axis === '1' || $axis === '2';
}

/**
 * Validate and normalize the descriptive origin of an intent.
 *
 * The origin is presentation/provenance only and grants no authority. It is bounded so a stored or
 * rendered payload cannot smuggle control characters or an unbounded string into an explanation.
 *
 * @param mixed $origin Requested origin token
 * @return string|false Clean token, or false when it is not a bounded identifier
 */
function dimension_resolution_clean_origin($origin)
{
	if ($origin === null || $origin === '')
		return '';
	if (!is_string($origin) || !preg_match('/^[a-z][a-z0-9_]{0,39}$/D', $origin))
		return false;
	return $origin;
}

/**
 * Validate one intent descriptor and return its normalized form.
 *
 * Unknown keys are refused: in particular no caller may attach a skip/bypass flag or an extra
 * identity to an intent. `selected` carries a canonical positive identity; every other mode carries
 * no identity. `not_applicable` is accepted only with an explicit policy origin and a registered
 * policy basis.
 *
 * @param string $mode Input mode or the legacy adapter disposition
 * @param mixed $id Stored identity (positive only for selected/legacy)
 * @param string $origin Descriptive decision origin
 * @param array $options persisted, policy_version and basis evidence
 * @return array ok, intent, errors and codes
 */
function dimension_resolution_intent($mode, $id = 0, $origin = '', $options = array())
{
	$result = array('ok' => false, 'intent' => null, 'errors' => array(), 'codes' => array());
	if (!is_array($options)) {
		$result['codes'][] = 'invalid_options';
		$result['errors'][] = _('The Dimension intent evidence must be an array.');
		return $result;
	}
	foreach ($options as $key => $unused) {
		if (!in_array($key, array('persisted', 'policy_version', 'basis'), true)) {
			$result['codes'][] = 'invalid_option';
			$result['errors'][] = sprintf(_('Unsupported Dimension intent evidence: %s.'), (string)$key);
			return $result;
		}
	}
	$modes = array_merge(dimension_resolution_modes(), array(dimension_resolution_legacy_mode()));
	if (!is_string($mode) || !in_array($mode, $modes, true)) {
		$result['codes'][] = 'invalid_mode';
		$result['errors'][] = _('The Dimension resolution mode is not part of the ADR-02 contract.');
		return $result;
	}
	$origin = dimension_resolution_clean_origin($origin);
	if ($origin === false) {
		$result['codes'][] = 'invalid_origin';
		$result['errors'][] = _('The Dimension resolution origin is not a bounded identifier.');
		return $result;
	}
	$identity = dimension_validation_legacy_id($id);
	if ($identity === false) {
		$result['codes'][] = 'invalid_identity';
		$result['errors'][] = _('A Dimension intent identity must be the stored whole identity or zero.');
		return $result;
	}
	$persisted = !empty($options['persisted']);
	$policy_version = isset($options['policy_version']) ? $options['policy_version'] : 0;
	if (!is_int($policy_version) || $policy_version < 0 || $policy_version > 2147483647) {
		$result['codes'][] = 'invalid_policy_version';
		$result['errors'][] = _('The Dimension policy version evidence must be a non-negative whole number.');
		return $result;
	}
	$basis = isset($options['basis']) ? $options['basis'] : '';
	if ($mode === 'selected') {
		if ($identity < 1) {
			$result['codes'][] = 'missing_identity';
			$result['errors'][] = _('A selected Dimension intent needs a positive stored identity.');
			return $result;
		}
		if ($policy_version !== 0 || $basis !== '') {
			$result['codes'][] = 'unexpected_policy_evidence';
			$result['errors'][] = _('Only a policy-derived Not Applicable intent may carry policy evidence.');
			return $result;
		}
	} elseif ($mode === 'legacy_unspecified') {
		if ($policy_version !== 0 || $basis !== '') {
			$result['codes'][] = 'unexpected_policy_evidence';
			$result['errors'][] = _('A legacy Dimension disposition carries no policy evidence.');
			return $result;
		}
		if ($origin === '')
			$origin = 'legacy';
	} else {
		if ($identity !== 0) {
			$result['codes'][] = 'unexpected_identity';
			$result['errors'][] = _('This Dimension intent must not carry an identity.');
			return $result;
		}
		if ($mode === 'not_applicable') {
			if ($origin !== 'policy' || !in_array($basis, dimension_resolution_not_applicable_bases(), true)) {
				$result['codes'][] = 'not_applicable_not_policy_derived';
				$result['errors'][] = _('Not Applicable is admitted only from a published company policy.');
				return $result;
			}
			if ($policy_version < 1) {
				$result['codes'][] = 'missing_policy_version';
				$result['errors'][] = _('Not Applicable must name the published policy version that declares it.');
				return $result;
			}
		} elseif ($policy_version !== 0 || $basis !== '') {
			$result['codes'][] = 'unexpected_policy_evidence';
			$result['errors'][] = _('Only a policy-derived Not Applicable intent may carry policy evidence.');
			return $result;
		}
	}
	$result['ok'] = true;
	$result['intent'] = array('mode' => $mode, 'id' => $identity, 'origin' => $origin,
		'persisted' => $persisted, 'policy_version' => (int)$policy_version, 'basis' => (string)$basis);
	return $result;
}

/**
 * Build a `selected` intent.
 *
 * @param mixed $id Positive stored identity
 * @param string $origin Descriptive origin
 * @param array $options persisted evidence
 * @return array intent result
 */
function dimension_resolution_selected($id, $origin = '', $options = array())
{
	return dimension_resolution_intent('selected', $id, $origin, $options);
}

/**
 * Build an Explicitly Unassigned intent.
 *
 * @param string $origin Descriptive origin
 * @param array $options persisted evidence
 * @return array intent result
 */
function dimension_resolution_unassigned($origin = '', $options = array())
{
	return dimension_resolution_intent('unassigned', 0, $origin, $options);
}

/**
 * Build an inherit intent.
 *
 * @param string $origin Descriptive origin
 * @param array $options unused, kept for a uniform builder signature
 * @return array intent result
 */
function dimension_resolution_inherit($origin = '', $options = array())
{
	return dimension_resolution_intent('inherit', 0, $origin, $options);
}

/**
 * Build a persisted source-snapshot decision.
 *
 * The caller owns the companion record that proves the snapshot; this helper only labels the
 * decision as persisted so the resolver can enforce its higher precedence than a destination header.
 *
 * @param string $source_kind line_snapshot or header_snapshot
 * @param mixed $id Stored identity, or 0 for a deliberately unassigned snapshot
 * @param array $options persisted evidence only; the companion revision row is owned by FND-006
 * @return array intent result
 */
function dimension_resolution_source_snapshot($source_kind, $id = 0, $options = array())
{
	if (!in_array($source_kind, array('line_snapshot', 'header_snapshot'), true)) {
		return array('ok' => false, 'intent' => null, 'codes' => array('invalid_source_snapshot'),
			'errors' => array(_('Only a source line or source header snapshot may use this adapter.')));
	}
	$options['persisted'] = true;
	$identity = dimension_validation_legacy_id($id);
	if ($identity === false) {
		return array('ok' => false, 'intent' => null, 'codes' => array('invalid_identity'),
			'errors' => array(_('A source snapshot identity must be the stored whole identity or zero.')));
	}
	if ($identity === 0)
		return dimension_resolution_unassigned($source_kind, $options);
	return dimension_resolution_selected($identity, $source_kind, $options);
}

/**
 * Build the policy-derived Not Applicable intent.
 *
 * @param mixed $axis Native axis
 * @param string $basis Registered basis token
 * @param int $policy_version Published policy version that supplies the basis
 * @return array intent result
 */
function dimension_resolution_policy_not_applicable($axis, $basis = 'axis_not_enabled',
	$policy_version = 0)
{
	if (!dimension_resolution_axis_valid($axis)) {
		return array('ok' => false, 'intent' => null, 'codes' => array('invalid_axis'),
			'errors' => array(_('Not Applicable needs a real Dimension axis.')));
	}
	return dimension_resolution_intent('not_applicable', 0, 'policy', array('basis' => $basis,
		'policy_version' => (int)$policy_version));
}

/**
 * Represent one configured default as a source intent.
 *
 * A zero default is an inherit intent so the next lower source is asked; a real default is a
 * selected intent whose provenance names the configured source, not a user choice.
 *
 * @param mixed $id Configured identity
 * @param string $origin partner_default or item_default
 * @return array intent result
 */
function dimension_resolution_default_intent($id, $origin)
{
	$identity = dimension_validation_legacy_id($id);
	if ($identity === false) {
		return array('ok' => false, 'intent' => null, 'codes' => array('invalid_default'),
			'errors' => array(_('A configured Dimension default is not a stored whole identity.')));
	}
	if ($identity === 0)
		return dimension_resolution_inherit($origin);
	return dimension_resolution_selected($identity, $origin);
}

/**
 * Normalize one source entry supplied to the resolver.
 *
 * Accepts a builder result, or a previously normalized intent array. Any other shape, a failed
 * builder result or an unknown key is refused rather than skipped, so a malformed higher-precedence
 * source can never silently fall through to a default.
 *
 * @param mixed $value One source entry
 * @param string $source_kind Source being normalized
 * @return array ok, intent, errors and codes
 */
function dimension_resolution_normalize_source($value, $source_kind)
{
	if (is_array($value) && array_key_exists('ok', $value) && array_key_exists('intent', $value)) {
		if ($value['ok'] !== true || !is_array($value['intent'])) {
			return array('ok' => false, 'intent' => null,
				'codes' => isset($value['codes']) ? $value['codes'] : array('invalid_source'),
				'errors' => isset($value['errors']) ? $value['errors']
					: array(_('The Dimension resolution source is not a valid intent.')));
		}
		$value = $value['intent'];
	}
	if (!is_array($value))
		return array('ok' => false, 'intent' => null, 'codes' => array('invalid_source'),
			'errors' => array(_('A Dimension resolution source must be an intent descriptor.')));
	$allowed = array('mode', 'id', 'origin', 'persisted', 'policy_version', 'basis');
	foreach ($value as $key => $unused) {
		if (!in_array($key, $allowed, true)) {
			return array('ok' => false, 'intent' => null, 'codes' => array('invalid_source_key'),
				'errors' => array(sprintf(_('Unsupported Dimension source evidence: %s.'), (string)$key)));
		}
	}
	if (!isset($value['mode'])) {
		return array('ok' => false, 'intent' => null, 'codes' => array('invalid_source'),
			'errors' => array(_('A Dimension resolution source names no input mode.')));
	}
	$options = array();
	foreach (array('persisted', 'policy_version', 'basis') as $key)
		if (array_key_exists($key, $value)) $options[$key] = $value[$key];
	$intent = dimension_resolution_intent($value['mode'],
		array_key_exists('id', $value) ? $value['id'] : 0,
		array_key_exists('origin', $value) ? $value['origin'] : '', $options);
	if (!$intent['ok']) {
		return array('ok' => false, 'intent' => null, 'codes' => $intent['codes'],
			'errors' => $intent['errors']);
	}
	// Source-specific shape: only snapshots may claim persisted evidence and a configured default
	// cannot express a deliberate blank or a policy-derived Not Applicable state.
	if (in_array($source_kind, array('line_snapshot', 'header_snapshot'), true)) {
		if (!empty($intent['intent']['persisted']))
			return $intent;
		return array('ok' => false, 'intent' => null, 'codes' => array('unpersisted_snapshot'),
			'errors' => array(_('A source snapshot decision must identify its persisted record.')));
	}
	if (!empty($intent['intent']['persisted']))
		return array('ok' => false, 'intent' => null, 'codes' => array('unexpected_persisted'),
			'errors' => array(_('Only a persisted source snapshot uses persisted evidence.')));
	if (in_array($source_kind, array('partner_default', 'item_default'), true)
		&& !in_array($intent['intent']['mode'], array('selected', 'inherit'), true))
		return array('ok' => false, 'intent' => null, 'codes' => array('invalid_default_mode'),
			'errors' => array(_('A configured default can only contribute a value or inherit.')));
	return $intent;
}

/**
 * Resolve one axis from the fixed ADR-02 sources.
 *
 * Every source above and including the winner is normalized and appears in provenance; lower
 * sources are not evaluated and never appear as decisions. `ok` is false only for malformed input
 * or a policy-invalid Not Applicable. An unresolved axis is an admitted "inherit with no value"
 * result whose requiredness is decided by the pair validator.
 *
 * @param mixed $axis Native axis 1 or 2
 * @param array $sources Source kind => intent descriptor/result
 * @param array $options enabled_axes for the policy Not Applicable check
 * @return array ok, axis, value, mode, origin, persisted, provenance, errors and codes
 */
function dimension_resolution_resolve_axis($axis, $sources, $options = array())
{
	$result = array('ok' => false, 'axis' => (int)$axis, 'value' => 0, 'mode' => 'inherit',
		'origin' => '', 'persisted' => false, 'provenance' => array(), 'errors' => array(),
		'codes' => array());
	if (!dimension_resolution_axis_valid($axis)) {
		$result['codes'][] = 'invalid_axis';
		$result['errors'][] = _('A Dimension axis must be category 1 or 2.');
		return $result;
	}
	$result['axis'] = (int)$axis;
	if (!is_array($sources)) {
		$result['codes'][] = 'invalid_sources';
		$result['errors'][] = _('The Dimension resolution sources must be an array.');
		return $result;
	}
	$allowed_options = array('enabled_axes');
	if (!is_array($options)) {
		$result['codes'][] = 'invalid_options';
		$result['errors'][] = _('The Dimension resolution options must be an array.');
		return $result;
	}
	foreach ($options as $key => $unused) {
		if (!in_array($key, $allowed_options, true)) {
			$result['codes'][] = 'invalid_option';
			$result['errors'][] = sprintf(_('Unsupported Dimension resolution option: %s.'), (string)$key);
			return $result;
		}
	}
	$order = dimension_resolution_source_kinds();
	foreach ($sources as $source_kind => $unused) {
		if (!in_array($source_kind, $order, true)) {
			$result['codes'][] = 'unknown_source';
			$result['errors'][] = sprintf(_('Unsupported Dimension resolution source: %s.'),
				(string)$source_kind);
			return $result;
		}
	}
	$winner = null;
	foreach ($order as $source_kind) {
		if (!array_key_exists($source_kind, $sources))
			continue;
		$normalized = dimension_resolution_normalize_source($sources[$source_kind], $source_kind);
		if (!$normalized['ok']) {
			$result['codes'] = array_merge($result['codes'], $normalized['codes']);
			foreach ($normalized['errors'] as $error)
				$result['errors'][] = sprintf(_('%s: %s'), $source_kind, $error);
			return $result;
		}
		$intent = $normalized['intent'];
		$entry = array('source' => $source_kind, 'mode' => $intent['mode'], 'id' => $intent['id'],
			'persisted' => $intent['persisted'], 'decision' => 'skipped');
		if ($intent['mode'] === 'selected' || $intent['mode'] === 'unassigned'
			|| $intent['mode'] === 'not_applicable') {
			$entry['decision'] = 'winner';
			$result['provenance'][] = $entry;
			$winner = $entry;
			if ($intent['mode'] === 'not_applicable' && isset($options['enabled_axes'])
				&& $result['axis'] <= (int)$options['enabled_axes']) {
				$result['codes'][] = 'not_applicable_not_declared';
				$result['errors'][] = _('Not Applicable is not declared for an enabled Dimension category.');
				return $result;
			}
			break;
		}
		$result['provenance'][] = $entry;
	}
	$result['ok'] = true;
	if ($winner === null) {
		$result['mode'] = 'inherit';
		$result['origin'] = 'none';
		return $result;
	}
	$result['value'] = $winner['mode'] === 'selected' ? (int)$winner['id'] : 0;
	$result['mode'] = $winner['mode'];
	$result['origin'] = $winner['source'];
	$result['persisted'] = !empty($winner['persisted']);
	return $result;
}

/**
 * Resolve both axes without applying the company rule validator.
 *
 * Both axes are always resolved before any result is returned, so a rule conflict on one axis never
 * prevents the other axis from being resolved or explained.
 *
 * @param array $sources1 Sources for axis 1
 * @param array $sources2 Sources for axis 2
 * @param array $options enabled_axes
 * @return array ok, axes, values, errors and codes
 */
function dimension_resolution_pair_values($sources1, $sources2, $options = array())
{
	$result = array('ok' => false, 'axes' => array(), 'values' => array(1 => 0, 2 => 0),
		'errors' => array(), 'codes' => array());
	$one = dimension_resolution_resolve_axis(1, $sources1, $options);
	$two = dimension_resolution_resolve_axis(2, $sources2, $options);
	$result['axes'] = array(1 => $one, 2 => $two);
	if ($one['ok']) $result['values'][1] = (int)$one['value'];
	if ($two['ok']) $result['values'][2] = (int)$two['value'];
	foreach (array(1, 2) as $axis) {
		if ($result['axes'][$axis]['ok'])
			continue;
		foreach ($result['axes'][$axis]['codes'] as $code)
			$result['codes'][] = 'axis'.$axis.'_'.$code;
		foreach ($result['axes'][$axis]['errors'] as $error)
			$result['errors'][] = sprintf(_('Category %d: %s'), $axis, $error);
	}
	$result['ok'] = $one['ok'] && $two['ok'];
	return $result;
}

/**
 * Resolve both axes and then validate requiredness and allowed combinations.
 *
 * The DIM-FND-004 contract is called only after both numeric values are resolved; its policy
 * version is reported and appended to each conflict so a delayed rule can be identified. This
 * function is read-only and does not itself post or save a classification.
 *
 * @param array $sources1 Sources for axis 1
 * @param array $sources2 Sources for axis 2
 * @param array $options account, command, enabled_axes
 * @return array ok, axes, values, validation, policy_version, errors and codes
 */
function dimension_resolution_pair($sources1, $sources2, $options = array())
{
	$result = array('ok' => false, 'axes' => array(), 'values' => array(1 => 0, 2 => 0),
		'validation' => null, 'policy_version' => 0, 'errors' => array(), 'codes' => array());
	if (!is_array($options)) {
		$result['codes'][] = 'invalid_options';
		$result['errors'][] = _('The Dimension resolution options must be an array.');
		return $result;
	}
	foreach ($options as $key => $unused) {
		if (!in_array($key, array('account', 'command', 'enabled_axes'), true)) {
			$result['codes'][] = 'invalid_option';
			$result['errors'][] = sprintf(_('Unsupported Dimension resolution option: %s.'), (string)$key);
			return $result;
		}
	}
	$command = isset($options['command']) ? $options['command'] : 'new_event';
	$resolution_options = array();
	if (isset($options['enabled_axes']))
		$resolution_options['enabled_axes'] = $options['enabled_axes'];
	$resolved = dimension_resolution_pair_values($sources1, $sources2, $resolution_options);
	$result['axes'] = $resolved['axes'];
	$result['values'] = $resolved['values'];
	$result['codes'] = $resolved['codes'];
	$result['errors'] = $resolved['errors'];
	if (!$resolved['ok'])
		return $result;
	$account = isset($options['account']) ? $options['account'] : null;
	$validation = dimension_validation_classification($resolved['values'][1], $resolved['values'][2],
		$command, array('account' => $account));
	$result['validation'] = $validation;
	$result['policy_version'] = isset($validation['policy_version'])
		? (int)$validation['policy_version'] : 0;
	$result['codes'] = array_merge($result['codes'], $validation['codes']);
	foreach ($validation['errors'] as $error)
		$result['errors'][] = $error;
	if ($command === 'new_event') {
		$enabled = isset($options['enabled_axes']) ? (int)$options['enabled_axes']
			: (int)get_company_pref('use_dimension');
		foreach (array(1, 2) as $axis) {
			$axis_result = $result['axes'][$axis];
			if ($axis_result['mode'] !== 'not_applicable' || $axis > $enabled)
				continue;
			$result['codes'][] = 'not_applicable_not_declared';
			$result['errors'][] = sprintf(_('Category %d is enabled, so Not Applicable is not a declared state for it.'),
				$axis);
		}
	}
	if ($validation['ok'] !== true)
		$result['ok'] = false;
	else
		$result['ok'] = count($result['errors']) === 0;
	if (!$result['ok'] && $result['policy_version'] > 0) {
		foreach ($result['errors'] as $index => $error)
			if (strpos($error, 'policy version') === false)
				$result['errors'][$index] = $error.' '
					.sprintf(_('(policy version %d)'), $result['policy_version']);
	}
	return $result;
}

/**
 * Characterize the E08 sales equality/default adapter without changing its behavior.
 *
 * Current Sales delivery/invoice/credit code compares the document header with the customer's
 * current default and only then considers the item default. This adapter is the one shared place
 * that keeps that legacy behavior for an unsnapshotted document. Explicit modes use
 * dimension_resolution_document_axis() instead.
 *
 * @param mixed $document_value Stored/entered document header identity
 * @param mixed $customer_value Current customer default
 * @param mixed $item_value Current item default
 * @return array ok, value, mode, origin, legacy_behavior, errors and codes
 */
function dimension_resolution_legacy_sales_axis($document_value, $customer_value, $item_value)
{
	foreach (array($document_value, $customer_value, $item_value) as $legacy_input)
		if ($legacy_input !== null && !is_scalar($legacy_input))
			return dimension_resolution_legacy_failure();
	// Keep the exact E08 loose comparison and truthiness. Normalizing the three inputs before the
	// comparison would change a blank/`'01'` header outcome, so only the winning value is normalized.
	if ($document_value != $customer_value)
		$raw = $document_value;
	elseif ($customer_value != 0)
		$raw = $customer_value;
	else
		$raw = $item_value;
	$value = dimension_validation_legacy_id($raw);
	if ($value === false)
		return dimension_resolution_legacy_failure();
	return array('ok' => true, 'value' => (int)$value, 'mode' => dimension_resolution_legacy_mode(),
		'origin' => 'sales_equality_default', 'legacy_behavior' => 'E08',
		'provenance' => array(array('source' => 'legacy_document', 'mode' => 'legacy_unspecified',
			'id' => (int)$value, 'persisted' => true, 'decision' => 'characterized')),
		'errors' => array(), 'codes' => array());
}

/**
 * Characterize the E10 purchasing nonempty fallback without changing its behavior.
 *
 * The header form only falls back to the supplier default; an expense/GRN line additionally falls
 * back to the item default. A zero result stays zero when both defaults are zero. Explicit modes
 * use dimension_resolution_document_axis() instead.
 *
 * @param mixed $document_value Stored/entered document header identity
 * @param mixed $supplier_value Current supplier default
 * @param mixed $item_value Current item default, or null on a header
 * @param bool $line Whether the item default participates
 * @return array ok, value, mode, origin, legacy_behavior, errors and codes
 */
function dimension_resolution_legacy_purchase_axis($document_value, $supplier_value,
	$item_value = null, $line = false)
{
	$inputs = array($document_value, $supplier_value);
	if ($line) $inputs[] = $item_value;
	foreach ($inputs as $legacy_input)
		if ($legacy_input !== null && !is_scalar($legacy_input))
			return dimension_resolution_legacy_failure();
	// Keep the exact E10 `!empty()` fallback chain. Only the winning value is normalized, so a
	// blank header or a non-canonical stored value cannot be reinterpreted as a different default.
	if (!empty($document_value))
		$raw = $document_value;
	elseif (!empty($supplier_value))
		$raw = $supplier_value;
	elseif ($line)
		$raw = $item_value;
	else
		$raw = 0;
	$value = dimension_validation_legacy_id($raw);
	if ($value === false)
		return dimension_resolution_legacy_failure();
	return array('ok' => true, 'value' => (int)$value, 'mode' => dimension_resolution_legacy_mode(),
		'origin' => $line ? 'purchase_nonempty_line_default' : 'purchase_nonempty_header_default',
		'legacy_behavior' => 'E10', 'provenance' => array(array('source' => 'legacy_document',
			'mode' => 'legacy_unspecified', 'id' => (int)$value, 'persisted' => true,
			'decision' => 'characterized')), 'errors' => array(), 'codes' => array());
}

/**
 * One fail-closed result for a non-canonical legacy value.
 *
 * @return array
 */
function dimension_resolution_legacy_failure()
{
	return array('ok' => false, 'value' => 0, 'mode' => dimension_resolution_legacy_mode(),
		'origin' => 'legacy_invalid', 'legacy_behavior' => 'characterized',
		'provenance' => array(), 'codes' => array('invalid_legacy_value'),
		'errors' => array(_('A stored Dimension value is not a whole identity, so the document cannot be classified.')));
}

/**
 * Resolve one source document axis for the Sales or Purchasing adapter.
 *
 * With no intent and no extra source this is exactly the characterized legacy adapter. With an
 * intent or a supplied snapshot it is the shared ADR-02 resolver: an Explicitly Unassigned header
 * wins and never falls through to a configured default, an explicit `inherit` asks the partner then
 * item defaults, and a selected value wins outright. Snapshot sources may be supplied by the module
 * slice that persists them, even when the destination recorded no mode.
 *
 * @param string $family sales or purchasing
 * @param mixed $document_value Stored/entered document header identity
 * @param mixed $partner_value Current customer/supplier default
 * @param mixed $item_value Current item default, or null on a purchase header
 * @param mixed $axis Native axis 1 or 2
 * @param array|null $intent Explicit header intent, or null for an unsnapshotted legacy document
 * @param array $extra_sources Persisted source snapshots or explicit line decisions
 * @return array resolver result
 */
function dimension_resolution_document_axis($family, $document_value, $partner_value, $item_value,
	$axis, $intent = null, $extra_sources = array())
{
	if ($family !== 'sales' && $family !== 'purchasing') {
		return array('ok' => false, 'value' => 0, 'mode' => 'inherit', 'origin' => '',
			'provenance' => array(), 'codes' => array('invalid_family'),
			'errors' => array(_('The document family has no shared Dimension adapter.')));
	}
	if (!dimension_resolution_axis_valid($axis)) {
		return array('ok' => false, 'value' => 0, 'mode' => 'inherit', 'origin' => '',
			'provenance' => array(), 'codes' => array('invalid_axis'),
			'errors' => array(_('A Dimension axis must be category 1 or 2.')));
	}
	if (!is_array($extra_sources)) {
		return array('ok' => false, 'value' => 0, 'mode' => 'inherit', 'origin' => '',
			'provenance' => array(), 'codes' => array('invalid_sources'),
			'errors' => array(_('The extra Dimension sources must be an array.')));
	}
	if ($intent === null && !count($extra_sources)) {
		if ($family === 'sales')
			return dimension_resolution_legacy_sales_axis($document_value, $partner_value, $item_value);
		return dimension_resolution_legacy_purchase_axis($document_value, $partner_value, $item_value,
			$item_value !== null);
	}
	// A persisted source snapshot is authoritative even when the destination header recorded no
	// mode at all; an implicit inherit is the destination's only claim in that case.
	if ($intent === null)
		$intent = dimension_resolution_inherit('header_implicit');
	$sources = array();
	foreach ($extra_sources as $source_kind => $source_intent) {
		if (!in_array($source_kind, array('line_explicit', 'line_snapshot', 'header_snapshot'), true)) {
			return array('ok' => false, 'value' => 0, 'mode' => 'inherit', 'origin' => '',
				'provenance' => array(), 'codes' => array('invalid_extra_source'),
				'errors' => array(_('Only a line or source snapshot may be supplied beside the document header.')));
		}
		$sources[$source_kind] = $source_intent;
	}
	$sources['header_explicit'] = $intent;
	$sources['partner_default'] = dimension_resolution_default_intent($partner_value, 'partner_default');
	if ($item_value !== null)
		$sources['item_default'] = dimension_resolution_default_intent($item_value, 'item_default');
	return dimension_resolution_resolve_axis($axis, $sources);
}

/**
 * Read the in-memory explicit intent of one document axis.
 *
 * An absent or malformed carrier is a legacy document, not permission to guess intent. Persisting
 * these modes belongs to DIM-FND-006; until then this carrier only affects the current request. The
 * property deliberately does not carry a native identity name: a selected/unassigned decision is
 * mirrored into the document's own `dimension`/`dimension2` (or `_id`) field so the B08 session
 * draft authority and every legacy reader keep seeing the same native reference.
 *
 * @param object $document Source cart/order/transaction
 * @param mixed $axis Native axis
 * @return array|null Intent descriptor, or null for a legacy document
 */
function dimension_resolution_document_intent($document, $axis)
{
	if (!is_object($document) || !dimension_resolution_axis_valid($axis)
		|| !isset($document->classification_intents) || !is_array($document->classification_intents)
		|| !isset($document->classification_intents[(int)$axis]))
		return null;
	$intent = $document->classification_intents[(int)$axis];
	if (!is_array($intent) || !isset($intent['mode']))
		return null;
	return $intent;
}

/**
 * Mirror one resolved intent into the document's native Dimension field.
 *
 * The field names differ by cart family (`dimension` on purchasing, `dimension_id` on Sales). Only
 * a direct selected identity or a deliberate blank becomes a native scalar; `inherit` leaves the
 * field to the posting-time resolution because it carries no final value yet.
 *
 * @param object $document Source cart/order/transaction
 * @param int $axis Native axis
 * @param array $intent Normalized intent
 * @return void
 */
function dimension_resolution_mirror_native_intent($document, $axis, $intent)
{
	if (!is_object($document) || !is_array($intent) || !isset($intent['mode']))
		return;
	if (!in_array($intent['mode'], array('selected', 'unassigned', 'not_applicable'), true))
		return;
	$value = $intent['mode'] === 'selected' ? (int)$intent['id'] : 0;
	$fields = $axis === 2 ? array('dimension2_id', 'dimension2') : array('dimension_id', 'dimension');
	foreach ($fields as $field)
		if (property_exists($document, $field)) {
			$document->$field = $value;
			return;
		}
}

/**
 * Record one explicit intent on a source document for the current request.
 *
 * The intent is validated through the same builder used by the resolver. This grants no posting or
 * approval authority and is not a persistence mechanism.
 *
 * @param object $document Source cart/order/transaction
 * @param mixed $axis Native axis
 * @param array $intent Intent descriptor or builder result
 * @return bool True when the validated intent was attached
 */
function dimension_resolution_set_document_intent($document, $axis, $intent)
{
	if (!is_object($document) || !dimension_resolution_axis_valid($axis))
		return false;
	if (!property_exists($document, 'classification_intents') && get_class($document) !== 'stdClass')
		return false;
	$normalized = dimension_resolution_normalize_source($intent, 'header_explicit');
	if (!$normalized['ok'])
		return false;
	if (!isset($document->classification_intents) || !is_array($document->classification_intents))
		$document->classification_intents = array();
	$document->classification_intents[(int)$axis] = $normalized['intent'];
	dimension_resolution_mirror_native_intent($document, (int)$axis, $normalized['intent']);
	return true;
}

/**
 * Return a resolved numeric value or unwind the posting transaction with the actionable reason.
 *
 * A module writer calls this with the adapter result; an invalid explicit intent or a non-canonical
 * legacy value therefore fails closed before any ledger row is written.
 *
 * @param array $result Adapter/resolver result
 * @return int Admitted numeric identity
 */
function dimension_resolution_document_value($result)
{
	if (is_array($result) && !empty($result['ok']))
		return (int)$result['value'];
	$message = _('The Dimension classification of this document could not be resolved.');
	if (is_array($result) && !empty($result['errors']))
		$message = implode(' ', $result['errors']);
	if (function_exists('dimension_financial_reference_failure') && !empty($GLOBALS['transaction_level']))
		dimension_financial_reference_failure();
	throw new DimensionFinancialReferenceWriteException($message);
}
