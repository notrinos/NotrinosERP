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
 * DIM-FND-004: shared Dimension validation and inverse policy contract.
 *
 * ADR-04 separates four commands, and this file implements the shared, fail-closed contract they
 * use: a new economic posting, a historical read, an exact historical inverse and an authorized
 * reclassification (owned by DIM-ALC-001 and refused here until that command exists). The contract
 * is descriptive of the stored native axes only; it never renumbers, remaps or rewrites a value.
 *
 * Command boundaries:
 *  - a **new event** needs a canonical positive identity, an installed value on the correct axis,
 *    an open value, a category currently enabled for new entries and, when the company published a
 *    DIM-CFG-001 policy, its required/account/value-pair rules. `0`, `''` and null keep the legacy
 *    "no Dimension on this axis" meaning; they are never an identity and never a stored default;
 *  - a **historical read** never denies. It names closed, deleted, wrong-axis and disabled values so
 *    a reader can render them instead of hiding them;
 *  - an **exact historical inverse** is authorized only by the locked original row it links to. It
 *    reproduces that row's account, both axes and the exactly negated base amount, so it may carry a
 *    closed, disabled or deleted historical value. No caller-supplied skip flag or identity list is
 *    accepted by this contract;
 *  - a **reclassification** is refused until DIM-ALC-001 owns the explicit command.
 *
 * DIM-FND-007 wires the new-event identity/lifecycle validator into the shared GL writer, so every
 * maintained shared GL writer caller is admitted at one authoritative boundary. The writer also
 * receives automatic balancing, settlement, rounding and payroll-control legs, so the economic
 * required/account/value-pair rules stay with each owning page adapter (the journal page owns the
 * complete DIM-FND-004 cart preflight). The shared boundary never trusts zero as an invalid identity
 * and never lets an unknown, wrong-axis, closed or disabled value through. Exact inverses keep using
 * the linked historical authority; they are not re-admitted as new events.
 */
require_once __DIR__.'/dimension_category_policy.inc';
require_once __DIR__.'/dimension_reference_writer_fence.inc';

/**
 * The command classes of ADR-04.
 *
 * @return string[] Stable command identifiers
 */
function dimension_validation_commands()
{
	return array('new_event', 'historical_read', 'exact_inverse', 'reclassification');
}

/**
 * Prove the actor and company every validation decision belongs to.
 *
 * The contract never trusts a request-supplied company. It reads the authenticated session, the
 * active company connection it names and the actual database handle, exactly like the ordinary
 * financial reference writer. A missing session, connection or handle fails closed.
 *
 * @return array|false company prefix, actor identity and connection identity
 */
function dimension_validation_context()
{
	global $db, $db_connections;
	$user = isset($_SESSION['wa_current_user']) ? $_SESSION['wa_current_user'] : null;
	if (!is_object($user) || !isset($user->cur_con) || !isset($db_connections[$user->cur_con])
		|| !is_array($db_connections[$user->cur_con]) || !isset($db_connections[$user->cur_con]['tbpref'])
		|| !is_object($db))
		return false;
	$prefix = $db_connections[$user->cur_con]['tbpref'];
	if (!is_string($prefix) || !preg_match('/^[A-Za-z0-9_]*$/D', $prefix))
		return false;
	return array('company' => $prefix, 'actor' => isset($user->user) ? (int)$user->user : 0,
		'connection' => spl_object_hash($db));
}

/**
 * Normalize the legacy numeric Dimension API contract.
 *
 * Before this contract every writer passed `dimension_id`/`dimension2_id` straight into SQL. The
 * stored and posted values are 0 for "no Dimension on this axis" or a positive native identity.
 * Only those two shapes are admitted: a blank string, null and the string `'0'` mean unassigned;
 * a positive integer or its canonical decimal spelling is an identity; a sign, leading zero,
 * fractional value, array, boolean or any other text is not an identity and must not be coerced.
 *
 * @param mixed $value Legacy dimension_id/dimension2_id input
 * @return int|false Unassigned as 0, the identity as a positive int, or false for an invalid value
 */
function dimension_validation_legacy_id($value)
{
	if ($value === null || $value === '' || $value === 0 || $value === '0')
		return 0;
	if (is_int($value))
		return $value > 0 && $value <= 2147483647 ? $value : false;
	if (!is_string($value) || !preg_match('/^[1-9][0-9]{0,9}$/D', $value))
		return false;
	$integer = (int)$value;
	return $integer > 0 && $integer <= 2147483647 && (string)$integer === $value ? $integer : false;
}

/**
 * Read the current master rows of a set of identities.
 *
 * @param int[] $identities Positive native identities
 * @return array|false Identity => id/type_/closed, or false when the read cannot be answered
 */
function dimension_validation_masters($identities)
{
	$clean = array();
	foreach ($identities as $identity)
		if (is_int($identity) && $identity > 0) $clean[$identity] = $identity;
	if (!count($clean))
		return array();
	sort($clean, SORT_NUMERIC);
	$rows = db_query('SELECT id, type_, closed FROM '.TB_PREF.'dimensions WHERE id IN ('
		.implode(',', $clean).')');
	if ($rows === false)
		return false;
	$masters = array();
	while ($row = db_fetch_assoc($rows))
		$masters[(int)$row['id']] = $row;
	return $masters;
}

/**
 * Read one GL account type for the declared account rules.
 *
 * An unknown or missing account contributes no account rule: the page's own account checks and the
 * ledger's foreign-key-free legacy shape stay authoritative. Only a real chart row can require an axis.
 *
 * @param mixed $account GL account code as posted by the page
 * @return string Stored account_type, or an empty string when it cannot be resolved
 */
function dimension_validation_account_type($account)
{
	if (!is_string($account) || $account === '' || strlen($account) > 15)
		return '';
	$rows = db_query('SELECT account_type FROM '.TB_PREF.'chart_master WHERE account_code='
		.db_escape($account));
	$row = $rows ? db_fetch_assoc($rows) : false;
	return $row && isset($row['account_type']) ? (string)$row['account_type'] : '';
}

/**
 * Does a declared value-pair rule admit one classification?
 *
 * A `pair:` rule names an axis-1 identity and an axis-2 identity; an empty stored side is the `*`
 * wildcard the policy page documents as "any value". A company that declared no value pair at all
 * stays permissive, which preserves the neutral initial state of a fresh company.
 *
 * @param array $rules Declared rules of the current policy version
 * @param int $dimension_id Normalized axis-1 identity
 * @param int $dimension2_id Normalized axis-2 identity
 * @return bool True when the pair is admitted
 */
function dimension_validation_pair_allowed($rules, $dimension_id, $dimension2_id)
{
	$declared = 0;
	foreach ($rules as $rule) {
		if (!is_array($rule) || !isset($rule['kind']) || $rule['kind'] !== 'value_pair')
			continue;
		$declared++;
		$left = isset($rule['left_value']) ? (string)$rule['left_value'] : '';
		$right = isset($rule['right_value']) ? (string)$rule['right_value'] : '';
		if (($left === '' || (int)$left === $dimension_id)
			&& ($right === '' || (int)$right === $dimension2_id))
			return true;
	}
	return $declared === 0;
}

/**
 * Display name of one axis for an actionable refusal.
 *
 * @param int $axis Native axis 1 or 2
 * @return string Published label or neutral category name
 */
function dimension_validation_axis_name($axis)
{
	return dimension_category_policy_label((int)$axis);
}

/**
 * Classify one stored pair for one command.
 *
 * The returned `codes` are stable machine-readable reasons; `errors` are ready to display. A caller
 * must treat `ok` as the only admission signal and must not weaken it from the request.
 *
 * @param mixed $dimension_id Stored axis-1 value
 * @param mixed $dimension2_id Stored axis-2 value
 * @param string $command One of dimension_validation_commands()
 * @param array $options account (GL code), original (locked historical row for exact_inverse)
 * @return array ok, command, errors, codes, ids, states and policy_version
 */
function dimension_validation_classification($dimension_id, $dimension2_id, $command, $options = array())
{
	$result = array('ok' => false, 'command' => (string)$command, 'errors' => array(), 'codes' => array(),
		'ids' => array('dimension_id' => 0, 'dimension2_id' => 0), 'states' => array(),
		'policy_version' => 0);
	if (!in_array($command, dimension_validation_commands(), true)) {
		$result['codes'][] = 'unknown_command';
		$result['errors'][] = _('The requested Dimension validation command is not part of the shared contract.');
		return $result;
	}
	$ids = array('dimension_id' => dimension_validation_legacy_id($dimension_id),
		'dimension2_id' => dimension_validation_legacy_id($dimension2_id));
	if ($ids['dimension_id'] === false || $ids['dimension2_id'] === false) {
		$result['codes'][] = 'invalid_reference';
		$result['errors'][] = _('A Dimension reference must be a stored whole identity, 0 or a blank value.');
		return $result;
	}
	$result['ids'] = $ids;
	if ($command === 'historical_read')
		return dimension_validation_historical_read($result);
	if ($command === 'exact_inverse')
		return dimension_validation_exact_inverse_classification($result, $options);
	if ($command === 'reclassification') {
		$result['codes'][] = 'unsupported_command';
		$result['errors'][] = _('An authorized reclassification command is not available in this release; use the dedicated allocation workstream.');
		return $result;
	}
	return dimension_validation_new_event($result, $options);
}

/**
 * Name the historical state of a stored pair without denying a read.
 *
 * @param array $result Classification result carrying the normalized ids
 * @return array Completed classification
 */
function dimension_validation_historical_read($result)
{
	$masters = dimension_validation_masters(array_values($result['ids']));
	if ($masters === false) {
		$result['codes'][] = 'unavailable';
		$result['states'] = array(1 => 'unknown', 2 => 'unknown');
		return $result;
	}
	foreach (array(1 => 'dimension_id', 2 => 'dimension2_id') as $axis => $column) {
		$identity = $result['ids'][$column];
		if ($identity === 0) {
			$result['states'][$axis] = 'unassigned';
			continue;
		}
		if (!isset($masters[$identity])) {
			$result['states'][$axis] = 'deleted';
			continue;
		}
		if ((int)$masters[$identity]['type_'] !== $axis) {
			$result['states'][$axis] = 'wrong_axis';
			continue;
		}
		$result['states'][$axis] = (int)$masters[$identity]['closed'] === 1 ? 'closed' : 'open';
	}
	$result['ok'] = true;
	return $result;
}

/**
 * Classify a pair that must reproduce one already proven historical row.
 *
 * The only supported producer of `original` is dimension_inverse_authorize(), which reads and locks
 * that row inside the posting transaction. A missing original or a different identity denies.
 *
 * @param array $result Classification result carrying the normalized ids
 * @param array $options original: locked gl_trans row from the inverse authorization
 * @return array Completed classification
 */
function dimension_validation_exact_inverse_classification($result, $options)
{
	$original = isset($options['original']) && is_array($options['original']) ? $options['original'] : false;
	if (!$original || !isset($original['dimension_id'], $original['dimension2_id'])) {
		$result['codes'][] = 'unlinked_inverse';
		$result['errors'][] = _('A historical inverse must be linked to the locked original posting before its classification is admitted.');
		return $result;
	}
	$original1 = dimension_validation_legacy_id($original['dimension_id']);
	$original2 = dimension_validation_legacy_id($original['dimension2_id']);
	if ($original1 === false || $original2 === false || $original1 !== $result['ids']['dimension_id']
		|| $original2 !== $result['ids']['dimension2_id']) {
		$result['codes'][] = 'axis_mismatch';
		$result['errors'][] = _('The historical inverse does not reproduce the original posting classification exactly.');
		return $result;
	}
	$result['states'] = array(1 => $original1 === 0 ? 'unassigned' : 'historical',
		2 => $original2 === 0 ? 'unassigned' : 'historical');
	$result['ok'] = true;
	return $result;
}

/**
 * Classify a new economic posting against masters, enabled categories and the declared policy.
 *
 * @param array $result Classification result carrying the normalized ids
 * @param array $options account: GL account code whose account rules apply
 * @return array Completed classification
 */
function dimension_validation_new_event($result, $options)
{
	if (dimension_validation_context() === false) {
		$result['codes'][] = 'missing_context';
		$result['errors'][] = _('No authenticated company context is available, so the Dimension classification cannot be validated.');
		return $result;
	}
	$policy = dimension_category_policy_current();
	$result['policy_version'] = isset($policy['version']) ? (int)$policy['version'] : 0;
	$enabled = (int)get_company_pref('use_dimension');
	$masters = dimension_validation_masters(array_values($result['ids']));
	if ($masters === false) {
		$result['codes'][] = 'unavailable';
		$result['errors'][] = _('The stored Dimension values could not be read, so new postings are refused.');
		return $result;
	}
	$states = array();
	foreach (array(1 => 'dimension_id', 2 => 'dimension2_id') as $axis => $column) {
		$identity = $result['ids'][$column];
		$name = dimension_validation_axis_name($axis);
		if ($identity === 0) {
			$states[$axis] = 'unassigned';
			if (!empty($policy['axes'][$axis]['required'])) {
				$result['codes'][] = 'required';
				$result['errors'][] = sprintf(_('%s is required for new postings. Choose a value.'), $name);
			}
			continue;
		}
		if ($axis > $enabled) {
			$states[$axis] = 'disabled';
			$result['codes'][] = 'disabled_axis';
			$result['errors'][] = sprintf(_('%s is not enabled for new entries in this company.'), $name);
			continue;
		}
		if (!isset($masters[$identity])) {
			$states[$axis] = 'deleted';
			$result['codes'][] = 'missing';
			$result['errors'][] = sprintf(_('%s value #%d does not exist. Choose an existing value or leave it blank.'), $name, $identity);
			continue;
		}
		if ((int)$masters[$identity]['type_'] !== $axis) {
			$states[$axis] = 'wrong_axis';
			$result['codes'][] = 'wrong_axis';
			$result['errors'][] = sprintf(_('%s value #%d belongs to the other category.'), $name, $identity);
			continue;
		}
		if ((int)$masters[$identity]['closed'] !== 0) {
			$states[$axis] = 'closed';
			$result['codes'][] = 'closed';
			$result['errors'][] = sprintf(_('%s value #%d is closed for new postings. Choose an open value.'), $name, $identity);
			continue;
		}
		$states[$axis] = 'open';
	}
	$result['states'] = $states;
	if (!dimension_validation_pair_allowed($policy['rules'], $result['ids']['dimension_id'],
		$result['ids']['dimension2_id'])) {
		$result['codes'][] = 'value_pair';
		$result['errors'][] = sprintf(_('The %s / %s value pair is not declared as allowed for new postings.'),
			dimension_validation_axis_name(1), dimension_validation_axis_name(2));
	}
	$account = isset($options['account']) ? $options['account'] : null;
	$account_type = dimension_validation_account_type($account);
	if ($account_type !== '') {
		foreach ($policy['rules'] as $rule) {
			if (!is_array($rule) || !isset($rule['kind']) || $rule['kind'] !== 'account'
				|| !isset($rule['axis'], $rule['left_value'])
				|| (string)$rule['left_value'] !== $account_type)
				continue;
			$axis = (int)$rule['axis'];
			$column = $axis === 2 ? 'dimension2_id' : 'dimension_id';
			if ($result['ids'][$column] === 0) {
				$result['codes'][] = 'account_required';
				$result['errors'][] = sprintf(_('Postings using this account type require %s.'),
					dimension_validation_axis_name($axis));
			}
		}
	}
	$result['ok'] = count($result['errors']) === 0;
	return $result;
}

/**
 * Admit one pair at the shared GL posting boundary.
 *
 * This is the DIM-FND-007 adapter used by the shared GL writer for every maintained caller. It enforces
 * the intrinsic identity and lifecycle rules only: a canonical positive identity must exist on the
 * named axis, that category must be enabled for new entries, and the value must be open. The legacy
 * zero/blank form remains "no Dimension on this axis" and is never resolved to a default or a master
 * identity.
 *
 * Economic requiredness, account rules and allowed value pairs are deliberately not applied here.
 * The same writer also receives automatic balancing, settlement, rounding and protected payroll legs
 * whose zero classification is not a user-entered economic event; the owning page adapter applies
 * the complete DIM-FND-004 preflight before handing a user-classified cart to the writer. There is no
 * caller-supplied skip flag: every call is checked, and a non-zero value can never bypass its master
 * lifecycle state. The current policy version is reported for evidence but is not used to switch
 * mandatory behavior on globally.
 *
 * @param mixed $dimension_id Axis-1 value as passed by the legacy numeric writer API
 * @param mixed $dimension2_id Axis-2 value as passed by the legacy numeric writer API
 * @return array ok, errors, codes, ids, states, policy_version and adapter
 */
function dimension_posting_admission($dimension_id, $dimension2_id)
{
	$result = array('ok' => false, 'adapter' => 'shared_gl_posting', 'errors' => array(),
		'codes' => array(), 'ids' => array('dimension_id' => 0, 'dimension2_id' => 0),
		'states' => array(), 'policy_version' => 0);
	if (dimension_validation_context() === false) {
		$result['codes'][] = 'missing_context';
		$result['errors'][] = _('No authenticated company context is available, so the posting classification cannot be validated.');
		return $result;
	}
	$ids = array('dimension_id' => dimension_validation_legacy_id($dimension_id),
		'dimension2_id' => dimension_validation_legacy_id($dimension2_id));
	if ($ids['dimension_id'] === false || $ids['dimension2_id'] === false) {
		$result['codes'][] = 'invalid_reference';
		$result['errors'][] = _('A posted Dimension reference must be a stored whole identity, 0 or a blank value.');
		return $result;
	}
	$result['ids'] = $ids;
	$policy = dimension_category_policy_current();
	$result['policy_version'] = isset($policy['version']) ? (int)$policy['version'] : 0;
	$enabled = (int)get_company_pref('use_dimension');
	$masters = dimension_validation_masters(array_values($ids));
	if ($masters === false) {
		$result['codes'][] = 'unavailable';
		$result['errors'][] = _('The stored Dimension values could not be read, so the posting is refused.');
		return $result;
	}
	foreach (array(1 => 'dimension_id', 2 => 'dimension2_id') as $axis => $column) {
		$identity = $ids[$column];
		$name = dimension_validation_axis_name($axis);
		if ($identity === 0) {
			$result['states'][$axis] = 'unassigned';
			continue;
		}
		if ($axis > $enabled) {
			$result['states'][$axis] = 'disabled';
			$result['codes'][] = 'disabled_axis';
			$result['errors'][] = sprintf(_('%s is not enabled for new entries in this company.'), $name);
			continue;
		}
		if (!isset($masters[$identity])) {
			$result['states'][$axis] = 'deleted';
			$result['codes'][] = 'missing';
			$result['errors'][] = sprintf(_('%s value #%d does not exist. Choose an existing value or leave it blank.'), $name, $identity);
			continue;
		}
		if ((int)$masters[$identity]['type_'] !== $axis) {
			$result['states'][$axis] = 'wrong_axis';
			$result['codes'][] = 'wrong_axis';
			$result['errors'][] = sprintf(_('%s value #%d belongs to the other category.'), $name, $identity);
			continue;
		}
		if ((int)$masters[$identity]['closed'] !== 0) {
			$result['states'][$axis] = 'closed';
			$result['codes'][] = 'closed';
			$result['errors'][] = sprintf(_('%s value #%d is closed for new postings. Choose an open value.'), $name, $identity);
			continue;
		}
		$result['states'][$axis] = 'open';
	}
	$result['ok'] = count($result['errors']) === 0;
	return $result;
}

/**
 * Authorize an exact historical inverse against its locked original ledger row.
 *
 * The candidate must reproduce the original account, both stored axes and the exactly negated base
 * amount at cent precision. Only then does the returned `historical` pair name the two original
 * identities that the shared writer may admit although they are closed, disabled or deleted. The
 * caller cannot supply that pair, a skip flag or another identity.
 *
 * Duplicate-reversal prevention stays with the owning command (PAY-GL-001 idempotence, journal
 * void); this contract proves the exact link, not the owner's one-effect guarantee.
 *
 * @param mixed $counter Original gl_trans counter to reverse
 * @param array $candidate account, dimension_id, dimension2_id and signed base amount
 * @return array ok, errors, codes, historical and the locked original row
 */
function dimension_inverse_authorize($counter, $candidate)
{
	$result = array('ok' => false, 'errors' => array(), 'codes' => array(),
		'historical' => array(), 'original' => null);
	if (dimension_validation_context() === false) {
		$result['codes'][] = 'missing_context';
		$result['errors'][] = _('No authenticated company context is available, so the inverse cannot be validated.');
		return $result;
	}
	if (empty($GLOBALS['transaction_level'])) {
		$result['codes'][] = 'outside_transaction';
		$result['errors'][] = _('An exact historical inverse is only authorized inside the posting transaction that locks its original row.');
		return $result;
	}
	$predecessor = dimension_writer_reference_id($counter);
	if ($predecessor === false || $predecessor === 0) {
		$result['codes'][] = 'invalid_link';
		$result['errors'][] = _('The linked original ledger identity is not valid.');
		return $result;
	}
	if (!is_array($candidate))
		$candidate = array();
	$account = isset($candidate['account']) ? $candidate['account'] : null;
	$dimension_id = dimension_validation_legacy_id(isset($candidate['dimension_id']) ? $candidate['dimension_id'] : null);
	$dimension2_id = dimension_validation_legacy_id(isset($candidate['dimension2_id']) ? $candidate['dimension2_id'] : null);
	$amount = isset($candidate['amount']) && is_numeric($candidate['amount']) ? (float)$candidate['amount'] : null;
	if ((!is_string($account) && !is_int($account)) || (string)$account === '' || strlen((string)$account) > 15
		|| $dimension_id === false || $dimension2_id === false || $amount === null) {
		$result['codes'][] = 'invalid_candidate';
		$result['errors'][] = _('The inverse ledger candidate is not a valid account, Dimension pair and amount.');
		return $result;
	}
	$account = (string)$account;
	$rows = db_query('SELECT counter, type, type_no, tran_date, account, dimension_id, dimension2_id, amount'
		.' FROM '.TB_PREF.'gl_trans WHERE counter='.$predecessor.' FOR UPDATE');
	if ($rows === false) {
		$result['codes'][] = 'unavailable';
		$result['errors'][] = _('The original ledger row could not be locked, so no inverse is authorized.');
		return $result;
	}
	if (db_num_rows($rows) !== 1) {
		$result['codes'][] = 'link_missing';
		$result['errors'][] = _('The linked original ledger row does not exist.');
		return $result;
	}
	$original = db_fetch_assoc($rows);
	$original1 = dimension_validation_legacy_id($original['dimension_id']);
	$original2 = dimension_validation_legacy_id($original['dimension2_id']);
	if ((string)$original['account'] !== $account || $original1 === false || $original2 === false
		|| $original1 !== $dimension_id || $original2 !== $dimension2_id) {
		$result['codes'][] = 'classification_mismatch';
		$result['errors'][] = _('The inverse does not reproduce the original account and Dimension pair exactly.');
		return $result;
	}
	if ((int)round((float)$original['amount'] * 100) + (int)round($amount * 100) !== 0) {
		$result['codes'][] = 'amount_mismatch';
		$result['errors'][] = _('The inverse amount is not the exact opposite of the original base amount.');
		return $result;
	}
	$result['ok'] = true;
	$result['original'] = $original;
	$result['historical'] = array('dimension_id' => $original1, 'dimension2_id' => $original2);
	return $result;
}

/**
 * Validate every user-entered GL line of one journal cart for a new event.
 *
 * Automatic balancing, rounding, tax and subledger legs are written by the command itself and are
 * not user classifications; DIM-FND-007 owns their writer-level adapters.
 *
 * @param array $lines Cart lines (gl_item objects or equivalent arrays)
 * @param string $command Classification command, new_event by default
 * @return array ok, errors and the per-line classifications
 */
function dimension_validation_gl_cart($lines, $command = 'new_event')
{
	$result = array('ok' => true, 'errors' => array(), 'lines' => array());
	if (!is_array($lines)) {
		$result['ok'] = false;
		$result['errors'][] = _('The journal lines could not be read for Dimension validation.');
		return $result;
	}
	$number = 0;
	foreach ($lines as $line) {
		$number++;
		if (is_object($line)) {
			$account = isset($line->code_id) ? $line->code_id : '';
			$dimension_id = isset($line->dimension_id) ? $line->dimension_id : 0;
			$dimension2_id = isset($line->dimension2_id) ? $line->dimension2_id : 0;
		} else {
			$account = is_array($line) && isset($line['code_id']) ? $line['code_id'] : '';
			$dimension_id = is_array($line) && isset($line['dimension_id']) ? $line['dimension_id'] : 0;
			$dimension2_id = is_array($line) && isset($line['dimension2_id']) ? $line['dimension2_id'] : 0;
		}
		$validation = dimension_validation_classification($dimension_id, $dimension2_id, $command,
			array('account' => $account));
		$result['lines'][$number] = $validation;
		if ($validation['ok'])
			continue;
		$result['ok'] = false;
		foreach ($validation['errors'] as $error)
			$result['errors'][] = sprintf(_('Line %d: %s'), $number, $error);
	}
	return $result;
}
