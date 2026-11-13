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
 * DIM-GL-002: documented intent and per-account exceptions for derived GL postings.
 *
 * A derived posting is a GL line the application itself creates while expanding a document: the
 * lines a quick-entry template expands into, the two legs of an accrual schedule, the bank-charge
 * and exchange-variance legs of a transfer, the balancing leg of a bank document and the automatic
 * rounding/FX differences of the shared writers. Those lines are not user classifications, so they
 * must not be judged by the ordinary requiredness rules of a new economic event (DIM-FND-007 keeps
 * the shared writer's identity/lifecycle admission, and the owning page keeps the economic
 * preflight). They still need an honest, reviewable intent:
 *
 *  - a line that expands a stored source (a quick-entry template line, an accrual schedule line, a
 *    page-selected document pair, a document charge) carries the exact pair that source recorded;
 *    it is never resolved from a partner/item default and never silently becomes unassigned;
 *  - an automatic control line (a balance-sheet balancing/accrual account) is explicitly unassigned
 *    by policy, but only when the account is genuinely non-P&L;
 *  - an automatic rounding difference is explicitly unassigned, but only on the company's declared
 *    rounding/exchange-difference accounts;
 *  - an automatic exchange-variance leg is explicitly unassigned, but only on the company's
 *    declared exchange-difference account.
 *
 * Every rule is decided from the account's chart class and the company preferences; nothing is
 * accepted from a request field, and there is no caller-supplied skip flag. The module is a pure
 * contract: it reads chart/preference configuration, writes nothing, opens no transaction and
 * grants no posting authority. The writers call it before they insert a derived line.
 *
 * PHP 5.6 compatible syntax.
 */
require_once __DIR__.'/dimension_resolution.inc';

/**
 * Versioned contract identifier named by every derived-line declaration.
 *
 * @return string
 */
function dimension_derived_posting_contract()
{
	return 'dimension-derived-posting/1';
}

/**
 * Reviewed derived-line roles.
 *
 * The role names the command that produced the line. Adding a role requires updating the reviewed
 * derived-leg inventory and its contract suite; an unknown role is refused.
 *
 * @return array Role => translated label
 */
function dimension_derived_posting_roles()
{
	return array(
		'template_line' => _('Quick-entry template line'),
		'schedule_line' => _('Accrual schedule line'),
		'document_line' => _('Document line'),
		'document_pair' => _('Page-selected document pair'),
		'document_header' => _('Document header declaration'),
		'charge_line' => _('Document charge line'),
		'non_pnl_control' => _('Non-P&L control leg'),
		'rounding' => _('Automatic rounding difference'),
		'fx_variance' => _('Automatic exchange variance'));
}

/**
 * Reviewed line dispositions.
 *
 * `source_intent` lines carry the exact pair their stored/entered source recorded (zero is an
 * explicit unassigned on that source). `explicit_unassigned` lines are automatic legs the policy
 * requires to be zero on both axes. `not_permitted` is a refusal, never a fallback.
 *
 * @return string[]
 */
function dimension_derived_posting_dispositions()
{
	return array('source_intent', 'explicit_unassigned', 'not_permitted');
}

/**
 * Normalize one role.
 *
 * @param mixed $role Requested role
 * @return string|false Reviewed role, or false
 */
function dimension_derived_posting_clean_role($role)
{
	$roles = dimension_derived_posting_roles();
	return is_string($role) && isset($roles[$role]) ? $role : false;
}

/**
 * Classify one GL account as balance-sheet, profit-and-loss or unknown.
 *
 * The chart class is the company's own published structure: classes 1-3 are balance-sheet
 * (non-P&L) and classes 4-6 are the income/cost-of-sales/expense side. A missing account is
 * `unknown`; an unreadable chart is `unavailable`; the caller must never treat either as P&L.
 *
 * @param mixed $account GL account code
 * @param array $options account_class, unavailable (test/embedding evidence only)
 * @return array ok, account, account_class, ctype, errors and codes
 */
function dimension_derived_posting_account_class($account, $options = array())
{
	$result = array('ok' => false, 'account' => '', 'account_class' => 'unknown', 'ctype' => 0,
		'errors' => array(), 'codes' => array());
	if (is_int($account))
		$account = (string)$account;
	if (!is_string($account) || $account === '' || strlen($account) > 15) {
		$result['codes'][] = 'invalid_account';
		$result['errors'][] = _('An automatic posting needs a non-empty GL account code.');
		return $result;
	}
	$result['account'] = $account;
	if (array_key_exists('account_class', $options)) {
		$class = (string)$options['account_class'];
		if (!in_array($class, array('balancesheet', 'pnl', 'unknown', 'unavailable'), true)) {
			$result['codes'][] = 'invalid_account_class';
			$result['errors'][] = _('The supplied account class is not part of the derived-posting contract.');
			return $result;
		}
		$result['account_class'] = $class;
		$result['ok'] = true;
		return $result;
	}
	if (!empty($options['unavailable'])) {
		$result['account_class'] = 'unavailable';
		$result['codes'][] = 'unavailable';
		$result['errors'][] = _('The chart of accounts could not be read, so the automatic posting policy cannot be resolved.');
		return $result;
	}
	$rows = db_query('SELECT cls.ctype FROM '.TB_PREF.'chart_master cm'
		.' LEFT JOIN '.TB_PREF.'chart_types typ ON typ.id = cm.account_type'
		.' LEFT JOIN '.TB_PREF.'chart_class cls ON cls.cid = typ.class_id'
		.' WHERE cm.account_code='.db_escape($account));
	if ($rows === false) {
		$result['account_class'] = 'unavailable';
		$result['codes'][] = 'unavailable';
		$result['errors'][] = _('The chart of accounts could not be read, so the automatic posting policy cannot be resolved.');
		return $result;
	}
	if (db_num_rows($rows) !== 1) {
		// A missing chart row is an unknown account, not an unclassified P&L one.
		$result['account_class'] = 'unknown';
		$result['ok'] = true;
		return $result;
	}
	$row = db_fetch_assoc($rows);
	$ctype = isset($row['ctype']) && is_numeric($row['ctype']) ? (int)$row['ctype'] : 0;
	$result['ctype'] = $ctype;
	$income_class = defined('CL_INCOME') ? CL_INCOME : 4;
	$result['account_class'] = ($ctype > 0 && $ctype < $income_class) ? 'balancesheet' : 'pnl';
	$result['ok'] = true;
	return $result;
}

/**
 * Read the company's declared rounding and exchange-difference accounts.
 *
 * `exchange_diff_act` is installed by the standard chart setup. The two rounding preferences are
 * optional legacy settings; when they are absent the shared rounding helper uses the configured
 * exchange-difference account exactly as the legacy `add_gl_balance()` already did. A blank value
 * is never a declared account.
 *
 * @param array $options declared_accounts override (contract tests only)
 * @return array fx, rounding_db, rounding_cr, declared (list of accounts)
 */
function dimension_derived_posting_declared_accounts($options = array())
{
	if (isset($options['declared_accounts']) && is_array($options['declared_accounts'])) {
		$declared = $options['declared_accounts'];
	} else {
		$declared = array(
			'fx' => get_company_pref('exchange_diff_act'),
			'rounding_db' => get_company_pref('rounding_db_act'),
			'rounding_cr' => get_company_pref('rounding_cr_act'));
	}
	$clean = array();
	$list = array();
	foreach (array('fx', 'rounding_db', 'rounding_cr') as $key) {
		$value = isset($declared[$key]) && is_scalar($declared[$key]) ? trim((string)$declared[$key]) : '';
		if ($value !== '' && strlen($value) <= 15) {
			$clean[$key] = $value;
			$list[$value] = $value;
		} else {
			$clean[$key] = '';
		}
	}
	$clean['declared'] = array_values($list);
	return $clean;
}

/**
 * Resolve the per-account policy of one derived line.
 *
 * The decision depends only on the reviewed role, the account's chart class and the company's
 * declared exception accounts. A missing or unreadable account fails closed: an automatic leg is
 * never posted to an account whose class is unknown.
 *
 * @param mixed $role Reviewed derived role
 * @param mixed $account GL account code
 * @param array $options account_class, declared_accounts (contract tests only)
 * @return array ok, role, account, account_class, disposition, exception, errors and codes
 */
function dimension_derived_posting_policy($role, $account, $options = array())
{
	$result = array('ok' => false, 'role' => '', 'account' => '', 'account_class' => 'unknown',
		'disposition' => 'not_permitted', 'exception' => '', 'errors' => array(), 'codes' => array());
	$clean_role = dimension_derived_posting_clean_role($role);
	if ($clean_role === false) {
		$result['codes'][] = 'unknown_role';
		$result['errors'][] = _('The derived posting role is not part of the reviewed contract.');
		return $result;
	}
	$result['role'] = $clean_role;
	$class = dimension_derived_posting_account_class($account, $options);
	$result['account'] = $class['account'];
	$result['account_class'] = $class['account_class'];
	if (!$class['ok']) {
		$result['codes'] = array_merge($result['codes'], $class['codes']);
		$result['errors'] = $class['errors'];
		return $result;
	}
	$declared = dimension_derived_posting_declared_accounts($options);
	$account_class = $class['account_class'];
	$is_source_intent = in_array($clean_role, array('template_line', 'schedule_line',
		'document_line', 'document_pair', 'document_header', 'charge_line'), true);
	if ($is_source_intent) {
		$result['ok'] = true;
		$result['disposition'] = 'source_intent';
		$result['exception'] = 'source_recorded_pair';
		return $result;
	}
	if ($clean_role === 'non_pnl_control') {
		if ($account_class === 'balancesheet') {
			$result['ok'] = true;
			$result['disposition'] = 'explicit_unassigned';
			$result['exception'] = 'balancesheet_control';
			return $result;
		}
		if ($account_class === 'pnl') {
			$result['codes'][] = 'pnl_control';
			$result['errors'][] = sprintf(_('Automatic control postings are only exempt from classification on a non-P&L (balance-sheet) account; %s is a profit-and-loss account.'), $result['account']);
			return $result;
		}
		$result['codes'][] = 'unknown_control_account';
		$result['errors'][] = sprintf(_('The control account %s has no readable chart classification, so the automatic posting is refused.'), $result['account']);
		return $result;
	}
	if ($clean_role === 'rounding') {
		if ($account_class === 'unavailable') {
			$result['codes'][] = 'unavailable';
			$result['errors'][] = _('The chart of accounts could not be read, so the rounding difference cannot be posted.');
			return $result;
		}
		if (in_array($result['account'], $declared['declared'], true)) {
			$result['ok'] = true;
			$result['disposition'] = 'explicit_unassigned';
			$result['exception'] = 'declared_rounding_account';
			return $result;
		}
		$result['codes'][] = 'rounding_account';
		$result['errors'][] = sprintf(_('A rounding difference may only post to a declared rounding or exchange-difference account; %s is not one of the company accounts.'), $result['account']);
		return $result;
	}
	if ($clean_role === 'fx_variance') {
		if ($account_class === 'unavailable') {
			$result['codes'][] = 'unavailable';
			$result['errors'][] = _('The chart of accounts could not be read, so the exchange variance cannot be posted.');
			return $result;
		}
		if ($declared['fx'] !== '' && $result['account'] === $declared['fx']) {
			$result['ok'] = true;
			$result['disposition'] = 'explicit_unassigned';
			$result['exception'] = 'declared_fx_account';
			return $result;
		}
		$result['codes'][] = 'fx_account';
		$result['errors'][] = sprintf(_('An automatic exchange variance may only post to the company exchange-difference account; %s is not that account.'), $result['account']);
		return $result;
	}
	$result['codes'][] = 'unknown_role';
	$result['errors'][] = _('The derived posting role has no reviewed policy branch.');
	return $result;
}

/**
 * Build the one declaration a writer must satisfy before inserting a derived line.
 *
 * @param mixed $role Reviewed derived role
 * @param mixed $account GL account code
 * @param mixed $dimension_id Axis-1 value the writer is about to post
 * @param mixed $dimension2_id Axis-2 value the writer is about to post
 * @param array $options account_class, declared_accounts (contract tests only)
 * @return array ok, contract, role, account, account_class, disposition, exception, pair, errors and codes
 */
function dimension_derived_posting_declaration($role, $account, $dimension_id, $dimension2_id,
	$options = array())
{
	$result = array('ok' => false, 'contract' => dimension_derived_posting_contract(), 'role' => '',
		'account' => '', 'account_class' => 'unknown', 'disposition' => 'not_permitted',
		'exception' => '', 'pair' => array('dimension_id' => 0, 'dimension2_id' => 0),
		'errors' => array(), 'codes' => array());
	$policy = dimension_derived_posting_policy($role, $account, $options);
	$result['role'] = $policy['role'];
	$result['account'] = $policy['account'];
	$result['account_class'] = $policy['account_class'];
	$result['disposition'] = $policy['disposition'];
	$result['exception'] = $policy['exception'];
	$id = dimension_validation_legacy_id($dimension_id);
	$id2 = dimension_validation_legacy_id($dimension2_id);
	if ($id === false || $id2 === false) {
		$result['codes'][] = 'invalid_pair';
		$result['errors'][] = _('A derived posting must carry a stored whole Dimension identity or an explicit blank on each axis.');
		return $result;
	}
	$result['pair'] = array('dimension_id' => $id, 'dimension2_id' => $id2);
	if (!$policy['ok']) {
		$result['codes'] = array_merge($result['codes'], $policy['codes']);
		$result['errors'] = $policy['errors'];
		return $result;
	}
	if ($policy['disposition'] === 'explicit_unassigned' && ($id !== 0 || $id2 !== 0)) {
		$roles = dimension_derived_posting_roles();
		$result['codes'][] = 'automatic_pair';
		$result['errors'][] = sprintf(_('The automatic %s leg must be explicitly unassigned; it cannot inherit or copy an economic classification.'), isset($roles[$policy['role']]) ? strtolower($roles[$policy['role']]) : $policy['role']);
		return $result;
	}
	$result['ok'] = true;
	return $result;
}

/**
 * Actionable refusal text of one failed derived declaration.
 *
 * @param array $result Failed declaration
 * @return string
 */
function dimension_derived_posting_refusal_text($result)
{
	$reason = '';
	if (is_array($result) && !empty($result['errors']))
		$reason = (string)$result['errors'][0];
	elseif (is_array($result) && !empty($result['codes']))
		$reason = (string)$result['codes'][0];
	if ($reason === '')
		$reason = _('the automatic posting policy could not be resolved');
	return sprintf(_('The automatic posting was refused: %s. No ledger line was written.'), rtrim($reason, '.'));
}

/**
 * Build the resolver intent of one source-recorded derived line.
 *
 * A quick-entry template line is a persisted source snapshot; an accrual schedule or a page pair is
 * an explicit source value for the event being created. A zero value is an explicit `unassigned`
 * intent with the same origin, so the DIM-FND-005 resolver never falls through to a partner/item
 * default (an automatic leg has no default at all).
 *
 * @param string $role Reviewed derived role
 * @param mixed $dimension_id Axis-1 value
 * @param mixed $dimension2_id Axis-2 value
 * @param string $origin Provenance token
 * @param bool $persisted True for a stored template snapshot
 * @return array ok, intents, sources, errors and codes
 */
function dimension_derived_posting_intents($role, $dimension_id, $dimension2_id, $origin, $persisted)
{
	$result = array('ok' => false, 'intents' => array(), 'sources' => array(),
		'errors' => array(), 'codes' => array());
	$clean_role = dimension_derived_posting_clean_role($role);
	if ($clean_role === false) {
		$result['codes'][] = 'unknown_role';
		$result['errors'][] = _('The derived posting role is not part of the reviewed contract.');
		return $result;
	}
	$source_kind = $persisted ? 'line_snapshot' : 'line_explicit';
	$ids = array(1 => dimension_validation_legacy_id($dimension_id),
		2 => dimension_validation_legacy_id($dimension2_id));
	if ($ids[1] === false || $ids[2] === false) {
		$result['codes'][] = 'invalid_pair';
		$result['errors'][] = _('A derived posting source must carry a stored whole Dimension identity or an explicit blank on each axis.');
		return $result;
	}
	foreach (array(1, 2) as $axis) {
		$intent = $ids[$axis] === 0
			? dimension_resolution_unassigned($origin, array('persisted' => $persisted))
			: dimension_resolution_selected($ids[$axis], $origin, array('persisted' => $persisted));
		if (!$intent['ok']) {
			$result['codes'] = array_merge($result['codes'], $intent['codes']);
			foreach ($intent['errors'] as $error)
				$result['errors'][] = sprintf(_('Category %d: %s'), $axis, $error);
			return $result;
		}
		$result['intents'][$axis] = $intent['intent'];
		$result['sources'][$axis] = array($source_kind => $intent);
	}
	$result['ok'] = true;
	return $result;
}

/**
 * Resolve and validate one source-recorded derived line through the shared DIM-FND-005 resolver.
 *
 * This is the adapter the quick-entry expansion uses: the stored/entered pair is the only source,
 * and the resulting pair is checked by the ordinary DIM-FND-004 new-event contract for that
 * account. A zero value stays zero and never falls back to a default.
 *
 * @param string $role Reviewed source-intent role
 * @param string $account GL account code
 * @param mixed $dimension_id Axis-1 value
 * @param mixed $dimension2_id Axis-2 value
 * @param string $origin Provenance token
 * @param bool $persisted True for a stored template snapshot
 * @return array ok, intents, resolution, values, errors and codes
 */
function dimension_derived_posting_resolve($role, $account, $dimension_id, $dimension2_id, $origin,
	$persisted)
{
	$result = array('ok' => false, 'intents' => array(), 'resolution' => null,
		'values' => array(1 => 0, 2 => 0), 'errors' => array(), 'codes' => array());
	$intents = dimension_derived_posting_intents($role, $dimension_id, $dimension2_id, $origin,
		$persisted);
	if (!$intents['ok']) {
		$result['codes'] = $intents['codes'];
		$result['errors'] = $intents['errors'];
		return $result;
	}
	$result['intents'] = $intents['intents'];
	$resolution = dimension_resolution_pair($intents['sources'][1], $intents['sources'][2],
		array('account' => $account));
	$result['resolution'] = $resolution;
	$result['values'] = $resolution['values'];
	if (!$resolution['ok']) {
		$result['codes'] = $resolution['codes'];
		$result['errors'] = $resolution['errors'];
		return $result;
	}
	$result['ok'] = true;
	return $result;
}
