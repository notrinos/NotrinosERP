<?php
/**
 * DIM-PAY-002: governed payroll native classification validation adapter.
 *
 * Payroll keeps its own governed Dimension path: the PAY-GL mapping rule names which journal axis supplies each native slot
 * (`dimension_id` axis-1, `dimension2_id` axis-2, master axis stored in `dimensions.type_`), the approved journal intent carries the
 * immutable allocation axes, and a single reviewed bridge projects them onto `gl_trans`. This adapter does not replace that path, does not
 * create or resolve a mapping, does not recalculate an allocation and does not collapse the two native slots into a generic classification.
 * It only gives the existing writers one shared, structured answer to "may this native pair be written now?":
 *
 *  - `post` / `repost` (new ledger events) must name, for every non-zero slot, a stored master on the correct axis, enabled for new entries
 *    in the active company and still open (the shared DIM-FND-007 identity/lifecycle admission). A zero slot is the explicit
 *    "no classification on this axis" value and is never resolved to an employee, department or any other default;
 *  - `reverse` is an exact inverse: the projected pair must equal the original posted ledger row's pair, so a closed, disabled or deleted
 *    original value is legitimately reproduced and a re-pointed target is refused before anything is written. The exact negated base amount
 *    and the locked proof remain owned by `dimension_inverse_authorize()` inside the shared writer;
 *  - a generated payslip cart and the payroll payment settlement carry an explicit 0/0 classification (the payable/bank settlement is a
 *    separate non-expense event) and the cart is admitted line by line before the first row is written.
 *
 * There is no skip/force/bypass argument. The writer still performs its own post-lock recheck, so a concurrent close after this preflight is
 * refused inside the posting transaction. Failure evidence contains only stable codes, an allocation sequence number and axis states, never an
 * employee, account holder, amount or salary value.
 *
 * PHP 5.6 compatible.
 */
require_once __DIR__.'/../../../dimensions/includes/dimension_validation_contract.inc';
require_once __DIR__.'/../../../includes/pay_gl_001_contract.inc';

if (!defined('PAY_NATIVE_CLASSIFICATION_VERSION'))
	define('PAY_NATIVE_CLASSIFICATION_VERSION', '1.0');

/**
 * State the reviewed boundary of the payroll native classification adapter.
 *
 * @return array Stable contract flags consumed by the acceptance suites
 */
function pay_native_classification_contract()
{
	return array(
		'version' => PAY_NATIVE_CLASSIFICATION_VERSION,
		'native_slots' => array(1 => 'dimension_id', 2 => 'dimension2_id'),
		'master_axis_field' => 'type_',
		'new_event_kinds' => array('post', 'repost'),
		'inverse_kinds' => array('reverse'),
		'generated_cart_classification' => 'explicit_zero_zero',
		'settlement_classification' => 'explicit_zero_zero_non_expense',
		'shared_admission' => 'dimension_posting_admission',
		'exact_inverse_authority' => 'dimension_inverse_authorize',
		'mapping_writer' => false,
		'recalculation' => false,
		'generic_classification' => false,
		'employee_default_inheritance' => false,
		'bypass_argument' => false,
		'historical_mutation' => false);
}

/**
 * Record the latest refusal evidence without any personal or monetary value.
 *
 * @param string $kind post, repost, reverse or cart
 * @param string $code Stable failure code
 * @param array $codes Shared admission codes
 * @param array $states Axis states reported by the shared admission
 * @param int $position Allocation or cart line sequence number
 * @return void
 */
function pay_native_classification_record_failure($kind, $code, $codes, $states, $position)
{
	$GLOBALS['pay_native_classification_last_failure'] = array('kind' => (string)$kind, 'code' => (string)$code,
		'position' => (int)$position, 'codes' => array_values((array)$codes), 'states' => (array)$states);
}

/**
 * Admit one native pair for a new ledger event through the shared identity/lifecycle contract.
 *
 * @param mixed $dimension_id Axis-1 value (0 or blank is the explicit unassigned value)
 * @param mixed $dimension2_id Axis-2 value
 * @return array ok, codes, errors, states, ids
 */
function pay_native_classification_pair_admission($dimension_id, $dimension2_id)
{
	if (!function_exists('dimension_posting_admission'))
		return array('ok' => false, 'codes' => array('unavailable'), 'errors' => array(_('The shared Dimension admission is unavailable, so the payroll posting is refused.')),
			'states' => array(), 'ids' => array('dimension_id' => 0, 'dimension2_id' => 0));
	$admission = dimension_posting_admission($dimension_id, $dimension2_id);
	return array('ok' => !empty($admission['ok']), 'codes' => isset($admission['codes']) ? $admission['codes'] : array(),
		'errors' => isset($admission['errors']) ? $admission['errors'] : array(),
		'states' => isset($admission['states']) ? $admission['states'] : array(),
		'ids' => isset($admission['ids']) ? $admission['ids'] : array('dimension_id' => 0, 'dimension2_id' => 0));
}

/**
 * Admit every line of a payroll cart before the first ledger row is written.
 *
 * Generated payroll lines are explicit 0/0 and always admitted; a manual line must name a real, open, enabled value on its own axis.
 *
 * @param array $lines Cart gl lines (objects with dimension_id/dimension2_id or equivalent arrays)
 * @return array ok, failures (line number, codes) and lines
 */
function pay_native_classification_cart_admission($lines)
{
	$result = array('ok' => true, 'failures' => array(), 'lines' => 0);
	if (!is_array($lines)) {
		$result['ok'] = false;
		$result['failures'][] = array('line' => 0, 'codes' => array('unreadable_cart'));
		return $result;
	}
	$number = 0;
	foreach ($lines as $line) {
		$number++;
		if (is_object($line)) {
			$first = isset($line->dimension_id) ? $line->dimension_id : 0;
			$second = isset($line->dimension2_id) ? $line->dimension2_id : 0;
		}
		else {
			$first = is_array($line) && isset($line['dimension_id']) ? $line['dimension_id'] : 0;
			$second = is_array($line) && isset($line['dimension2_id']) ? $line['dimension2_id'] : 0;
		}
		$result['lines'] = $number;
		// An explicit zero/blank pair is the generated-line classification: nothing to resolve, and the shared writer still admits it.
		if (($first === 0 || $first === '0' || $first === '' || $first === null) && ($second === 0 || $second === '0' || $second === '' || $second === null))
			continue;
		$admission = pay_native_classification_pair_admission($first, $second);
		if ($admission['ok'])
			continue;
		$result['ok'] = false;
		$result['failures'][] = array('line' => $number, 'codes' => $admission['codes'], 'states' => $admission['states']);
	}
	return $result;
}

/**
 * Refuse a payroll cart write before any row is written, using the financial reference failure semantics.
 *
 * @param array $lines Cart gl lines
 * @return bool True when every line was admitted
 * @throws DimensionFinancialReferenceWriteException (or RuntimeException before that class is loaded) on refusal
 */
function pay_native_classification_cart_require($lines)
{
	$admission = pay_native_classification_cart_admission($lines);
	if ($admission['ok'])
		return true;
	$first = $admission['failures'][0];
	pay_native_classification_record_failure('cart', 'native_classification_cart_denied', $first['codes'],
		isset($first['states']) ? $first['states'] : array(), $first['line']);
	if (function_exists('dimension_financial_reference_failure'))
		dimension_financial_reference_failure();
	if (function_exists('cancel_transaction'))
		cancel_transaction();
	throw new RuntimeException(_('Financial classification could not be verified. No transaction changes were saved.'));
}

/**
 * List the user-facing refusal messages for one manually entered payslip GL line.
 *
 * Manual lines are user-entered economic classifications, so the full new-event page contract (axis, enabled, open, published policy) applies,
 * exactly like the journal page. Generated lines never use this function.
 *
 * @param string $account GL account code
 * @param mixed $dimension_id Axis-1 value
 * @param mixed $dimension2_id Axis-2 value
 * @return string[] Messages; empty when the line is admitted
 */
function pay_native_classification_manual_line_errors($account, $dimension_id, $dimension2_id)
{
	$validation = dimension_validation_classification($dimension_id, $dimension2_id, 'new_event', array('account' => $account));
	return $validation['ok'] ? array() : $validation['errors'];
}

/**
 * Read the pair stored on the original posted ledger row that a reversal line will invert.
 *
 * @param int $counter Original gl_trans counter
 * @return array|false dimension_id and dimension2_id integers, or false when the row does not exist
 */
function pay_native_classification_original_pair($counter)
{
	$counter = (int)$counter;
	if ($counter <= 0)
		return false;
	$result = db_query("SELECT dimension_id, dimension2_id FROM ".TB_PREF."gl_trans WHERE counter=".db_escape($counter), 'could not read the original payroll ledger classification');
	if (!$result)
		return false;
	$row = db_fetch_assoc($result);
	if (!$row)
		return false;
	return array('dimension_id' => (int)$row['dimension_id'], 'dimension2_id' => (int)$row['dimension2_id']);
}

/**
 * Preflight the native targets of one PAY-GL posting effect before the effect is written.
 *
 * @param string $kind post, repost or reverse
 * @param array $entries Each: allocation_no, axes (immutable allocation axes), rule (mapping rule axis codes), predecessor_counter (reverse only)
 * @param string $error Stable failure code on refusal
 * @return bool True when every projected target is admissible for this kind
 */
function pay_native_classification_effect_preflight($kind, $entries, &$error = null)
{
	$error = null;
	$contract = pay_native_classification_contract();
	$new_event = in_array($kind, $contract['new_event_kinds'], true);
	$inverse = in_array($kind, $contract['inverse_kinds'], true);
	if ((!$new_event && !$inverse) || !is_array($entries) || !count($entries)) {
		$error = 'native_classification_preflight_invalid';
		pay_native_classification_record_failure($kind, $error, array('invalid_input'), array(), 0);
		return false;
	}
	foreach ($entries as $entry) {
		$position = is_array($entry) && isset($entry['allocation_no']) ? (int)$entry['allocation_no'] : 0;
		if (!is_array($entry) || !isset($entry['axes'], $entry['rule']) || !is_array($entry['axes']) || !is_array($entry['rule'])) {
			$error = 'native_classification_preflight_invalid';
			pay_native_classification_record_failure($kind, $error, array('invalid_input'), array(), $position);
			return false;
		}
		$projection = pay_gl_001_project_native_dimensions($entry['axes'], $entry['rule'], $projection_error);
		if (!$projection) {
			$error = 'native_classification_target_denied';
			pay_native_classification_record_failure($kind, $error, array((string)$projection_error), array(), $position);
			return false;
		}
		if ($inverse) {
			$original = pay_native_classification_original_pair(isset($entry['predecessor_counter']) ? $entry['predecessor_counter'] : 0);
			if ($original === false || $original['dimension_id'] !== (int)$projection['dimension_id']
				|| $original['dimension2_id'] !== (int)$projection['dimension2_id']) {
				$error = 'reversal_original_classification_mismatch';
				pay_native_classification_record_failure($kind, $error, array('inverse_pair_mismatch'), array(), $position);
				return false;
			}
			continue;
		}
		$admission = pay_native_classification_pair_admission($projection['dimension_id'], $projection['dimension2_id']);
		if (!$admission['ok']) {
			$error = 'native_classification_target_denied';
			pay_native_classification_record_failure($kind, $error, $admission['codes'], $admission['states'], $position);
			return false;
		}
	}
	return true;
}
