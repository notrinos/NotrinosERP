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
 * DIM-FND-006: ADR-03 posting lineage for durable source classifications.
 *
 * A source revision explains a classification; it does not prove which ledger rows carry it. This
 * file records that attribution without ever creating, updating or deleting a `gl_trans` row:
 *
 *  - one source line may contribute to several posting rows (partial delivery/receipt, split
 *    attribution);
 *  - one consolidated posting row may be fed by several source revisions;
 *  - the unique effect key plus source and counter prevents a duplicate attribution of the same
 *    posting effect;
 *  - `quantity_base` consumes a declared source line quantity exactly once, so partial quantities
 *    reconcile against the source line without overwriting its snapshot;
 *  - every registered link proves the exact `gl_trans.counter`, account, type/number and signed base
 *    amount against the maintained writer's committed row.
 *
 * The ledger remains the monetary actual; a link is integrity evidence, never an independently
 * maintained analytic balance. Existing posted history is never backfilled, and a missing link is
 * reported as lineage unavailable.
 *
 * PHP 5.6 compatible syntax.
 */
require_once __DIR__.'/dimension_source_revision.inc';

/**
 * Reviewed posting families that may register a lineage link.
 *
 * @return string[]
 */
function dimension_posting_link_families()
{
	return array('sales', 'purchasing', 'gl', 'inventory', 'manufacturing');
}

/**
 * Physical companion table owned by this authority.
 *
 * @return string[]
 */
function dimension_posting_link_tables()
{
	return array('dimension_posting_links');
}

/**
 * Expected column inventory of the posting-link table.
 *
 * @return array Column name => true
 */
function dimension_posting_link_schema_columns()
{
	return array('posting_link_id'=>true, 'source_revision_id'=>true, 'source_identity_sha256'=>true,
		'effect_key'=>true, 'posting_family'=>true, 'gl_type'=>true, 'gl_number'=>true,
		'gl_counter'=>true, 'account'=>true, 'contribution_base'=>true, 'quantity_base'=>true,
		'base_currency'=>true, 'predecessor_link_id'=>true, 'revision_payload_sha256'=>true,
		'payload_sha256'=>true, 'created_at'=>true);
}

/**
 * Is the complete posting-link schema installed for the active company?
 *
 * @param bool $refresh Re-read the schema instead of reusing this request's answer
 * @return bool|string True, 'absent', 'partial' or false when unreadable
 */
function dimension_posting_link_schema_ready($refresh = false)
{
	if ($refresh)
		dimension_source_revision_table_state(null, true);
	$state = dimension_source_revision_table_state('dimension_posting_links');
	if ($state === 'unknown')
		return false;
	if ($state === 'absent')
		return 'absent';
	foreach (array_keys(dimension_posting_link_schema_columns()) as $column)
		if (!dimension_source_revision_column_available('dimension_posting_links', $column))
			return 'partial';
	return true;
}

/**
 * Base currency of the active company, or '' when the preference is unavailable.
 *
 * @return string
 */
function dimension_posting_link_base_currency()
{
	if (!function_exists('get_company_pref'))
		return '';
	$currency = get_company_pref('curr_default');
	return is_string($currency) && preg_match('/^[A-Z]{3}$/D', $currency) ? $currency : '';
}

/**
 * Canonical effect identity of one posting row attributable to one source revision.
 *
 * The effect names the posting event and the exact source identity; the caller cannot choose it, so
 * a retried registration of the same effect converges instead of inventing a second attribution.
 *
 * @param array $fields company, posting_family, gl_type, gl_number, gl_counter,
 *                      source_identity_sha256
 * @return string|false Lower-case SHA-256
 */
function dimension_posting_effect_key($fields)
{
	if (!is_array($fields))
		return false;
	foreach (array('company', 'posting_family', 'gl_type', 'gl_number', 'gl_counter',
		'source_identity_sha256') as $name)
		if (!array_key_exists($name, $fields))
			return false;
	if (!in_array($fields['posting_family'], dimension_posting_link_families(), true))
		return false;
	if (!preg_match('/^[a-f0-9]{64}$/D', (string)$fields['source_identity_sha256']))
		return false;
	return hash('sha256', dimension_source_revision_canonical(array(
		'schema' => 'dimension-posting-effect/1',
		'company' => (int)$fields['company'],
		'posting_family' => (string)$fields['posting_family'],
		'gl_type' => (int)$fields['gl_type'],
		'gl_number' => (int)$fields['gl_number'],
		'gl_counter' => (int)$fields['gl_counter'],
		'source_identity_sha256' => (string)$fields['source_identity_sha256'])));
}

/**
 * Canonical sealed payload of one posting link.
 *
 * @param array $fields Prepared link fields
 * @return string|false Lower-case SHA-256
 */
function dimension_posting_link_payload_digest($fields)
{
	if (!is_array($fields))
		return false;
	$required = array('company', 'source_revision_id', 'source_identity_sha256',
		'revision_payload_sha256', 'effect_key', 'posting_family', 'gl_type', 'gl_number',
		'gl_counter', 'account', 'contribution_base', 'quantity_base', 'base_currency',
		'predecessor_link_id');
	foreach ($required as $name)
		if (!array_key_exists($name, $fields))
			return false;
	if (dimension_posting_effect_key($fields) !== $fields['effect_key'])
		return false;
	if (dimension_source_revision_decimal($fields['contribution_base'], true)
		!== $fields['contribution_base'])
		return false;
	if (dimension_source_revision_decimal($fields['quantity_base']) !== $fields['quantity_base'])
		return false;
	$canonical = array('schema' => 'dimension-posting-link/1');
	foreach ($required as $name)
		$canonical[$name] = (string)$fields[$name];
	return hash('sha256', dimension_source_revision_canonical($canonical));
}

/**
 * Validate and seal one posting link against its verified revision.
 *
 * @param array $revision Verified revision row (row_state()['revision'])
 * @param array $input posting_family, gl_type, gl_number, gl_counter, account,
 *                    contribution_base, quantity_base, base_currency, predecessor_link_id
 * @return array ok, fields, errors and codes
 */
function dimension_posting_link_prepare($revision, $input)
{
	$result = array('ok' => false, 'fields' => null, 'errors' => array(), 'codes' => array());
	if (!is_array($revision) || !isset($revision['source_revision_id'], $revision['axes'],
		$revision['payload_sha256'], $revision['source_identity_sha256'])) {
		$result['codes'][] = 'invalid_revision';
		$result['errors'][] = _('A posting link needs one verified Dimension source revision.');
		return $result;
	}
	if (!is_array($input)) {
		$result['codes'][] = 'invalid_link';
		$result['errors'][] = _('A posting link must be an array.');
		return $result;
	}
	foreach ($input as $key => $unused)
		if (!in_array($key, array('posting_family', 'gl_type', 'gl_number', 'gl_counter', 'account',
			'contribution_base', 'quantity_base', 'base_currency', 'predecessor_link_id'), true)) {
			$result['codes'][] = 'invalid_option';
			$result['errors'][] = sprintf(_('Unsupported Dimension posting link evidence: %s.'), (string)$key);
			return $result;
		}
	$family = isset($input['posting_family']) ? (string)$input['posting_family'] : '';
	if (!in_array($family, dimension_posting_link_families(), true)) {
		$result['codes'][] = 'invalid_posting_family';
		$result['errors'][] = _('The posting family is not registered for Dimension lineage.');
		return $result;
	}
	$gl_type = isset($input['gl_type']) ? dimension_source_revision_int($input['gl_type'], 0, 32767) : false;
	$gl_number = isset($input['gl_number']) ? dimension_source_revision_int($input['gl_number'], 0) : false;
	$gl_counter = isset($input['gl_counter']) ? dimension_source_revision_int($input['gl_counter'], 1) : false;
	$account = isset($input['account']) ? dimension_source_revision_clean_key($input['account'], 15) : false;
	if ($gl_type === false || $gl_number === false || $gl_counter === false || $account === false) {
		$result['codes'][] = 'invalid_posting_identity';
		$result['errors'][] = _('A posting link needs its exact type, number, counter and account.');
		return $result;
	}
	$contribution = isset($input['contribution_base'])
		? dimension_source_revision_decimal($input['contribution_base'], true) : false;
	if ($contribution === false || $contribution === '0.00000000') {
		$result['codes'][] = 'invalid_contribution';
		$result['errors'][] = _('A posting link needs a non-zero signed base-currency contribution.');
		return $result;
	}
	$quantity = dimension_source_revision_decimal(
		isset($input['quantity_base']) ? $input['quantity_base'] : 0, false, 9);
	if ($quantity === false) {
		$result['codes'][] = 'invalid_quantity';
		$result['errors'][] = _('A posting link quantity must be an exact non-negative decimal within the source line bound.');
		return $result;
	}
	$line_quantity = dimension_source_revision_decimal($revision['line_quantity'], false, 9);
	if ($line_quantity === false) {
		$result['codes'][] = 'invalid_revision_quantity';
		$result['errors'][] = _('The source revision line quantity is not an exact bounded decimal.');
		return $result;
	}
	if ($revision['line_kind'] === 'header' && $quantity !== '0.00000000') {
		$result['codes'][] = 'header_quantity';
		$result['errors'][] = _('A header revision carries no quantity; link the contribution to the classified line.');
		return $result;
	}
	if ($line_quantity === '0.00000000' && $quantity !== '0.00000000') {
		$result['codes'][] = 'quantity_without_source';
		$result['errors'][] = _('A link cannot consume quantity from a source revision that declares none.');
		return $result;
	}
	$currency = isset($input['base_currency']) ? (string)$input['base_currency'] : '';
	if (!preg_match('/^[A-Z]{3}$/D', $currency)) {
		$result['codes'][] = 'invalid_currency';
		$result['errors'][] = _('A posting link needs its three-letter base currency.');
		return $result;
	}
	$predecessor = isset($input['predecessor_link_id'])
		? dimension_source_revision_int($input['predecessor_link_id'], 0) : 0;
	if ($predecessor === false) {
		$result['codes'][] = 'invalid_predecessor';
		$result['errors'][] = _('A posting link predecessor must be a stored positive link identity or zero.');
		return $result;
	}
	$company = dimension_source_revision_company();
	$effect = dimension_posting_effect_key(array(
		'company' => $company, 'posting_family' => $family, 'gl_type' => $gl_type,
		'gl_number' => $gl_number, 'gl_counter' => $gl_counter,
		'source_identity_sha256' => $revision['source_identity_sha256']));
	if ($effect === false) {
		$result['codes'][] = 'invalid_effect';
		$result['errors'][] = _('The posting effect identity could not be derived.');
		return $result;
	}
	$fields = array(
		'company' => $company,
		'source_revision_id' => (int)$revision['source_revision_id'],
		'source_identity_sha256' => (string)$revision['source_identity_sha256'],
		'revision_payload_sha256' => (string)$revision['payload_sha256'],
		'effect_key' => $effect,
		'posting_family' => $family,
		'gl_type' => $gl_type,
		'gl_number' => $gl_number,
		'gl_counter' => $gl_counter,
		'account' => $account,
		'contribution_base' => $contribution,
		'quantity_base' => $quantity,
		'base_currency' => $currency,
		'predecessor_link_id' => $predecessor);
	$fields['payload_sha256'] = dimension_posting_link_payload_digest($fields);
	if ($fields['payload_sha256'] === false) {
		$result['codes'][] = 'invalid_payload';
		$result['errors'][] = _('The posting link payload could not be sealed.');
		return $result;
	}
	$result['ok'] = true;
	$result['fields'] = $fields;
	return $result;
}

/**
 * Sum the contributions already attributed to one ledger counter in one posting family/currency.
 *
 * @param int $gl_counter Exact ledger counter
 * @param string $posting_family Reviewed posting family
 * @param string $base_currency Three-letter base currency
 * @param bool $lock Lock the contributing rows
 * @return string|false Canonical signed decimal, or false when unreadable
 */
function dimension_posting_link_counter_total($gl_counter, $posting_family, $base_currency, $lock = false)
{
	$ready = dimension_posting_link_schema_ready();
	if ($ready !== true)
		return false;
	$rows = db_query('SELECT contribution_base FROM '.TB_PREF.'dimension_posting_links WHERE gl_counter='
		.(int)$gl_counter.' AND posting_family='.db_escape($posting_family)
		.' AND base_currency='.db_escape($base_currency).($lock ? ' FOR UPDATE' : ''));
	if (!$rows)
		return false;
	$total = 0.0;
	while ($row = db_fetch_assoc($rows))
		$total += (float)$row['contribution_base'];
	return dimension_source_revision_decimal(sprintf('%.8F', $total), true);
}

/**
 * Is a signed contribution a remaining portion of one committed ledger amount?
 *
 * A consolidated posting may receive several source links; each must share the ledger row's sign
 * and the cumulative attribution may not exceed the actual amount. Equality is proven separately by
 * dimension_posting_link_reconcile().
 *
 * @param mixed $stored Committed ledger amount
 * @param string $existing Canonical already-attributed total
 * @param string $contribution Canonical new contribution
 * @return bool
 */
function dimension_posting_link_attribution_admitted($stored, $existing, $contribution)
{
	$stored_value = (float)$stored;
	$existing_canonical = dimension_source_revision_decimal($existing, true);
	$contribution_canonical = dimension_source_revision_decimal($contribution, true);
	if ($existing_canonical === false || $contribution_canonical === false)
		return false;
	$existing_value = (float)$existing_canonical;
	$contribution_value = (float)$contribution_canonical;
	if (abs($stored_value) < 0.000000005 || abs($contribution_value) < 0.000000005)
		return false;
	if ($stored_value > 0 && ($contribution_value < 0 || $existing_value < 0))
		return false;
	if ($stored_value < 0 && ($contribution_value > 0 || $existing_value > 0))
		return false;
	return abs($existing_value + $contribution_value) <= abs($stored_value) + 0.000000005;
}

/**
 * Register one posting attribution atomically against the maintained GL writer's committed row.
 *
 * The caller posts through the real writer first (inside a transaction, or this call owns one), then
 * registers the link. The ledger row is locked and compared on type, number, counter, account and
 * signed base amount. A duplicate effect+source+counter is idempotent only when the sealed payload is
 * identical.
 *
 * @param mixed $revision_id Verified source revision
 * @param array $input Link evidence
 * @return array ok, created, existing, link_id, link, errors and codes
 */
function dimension_posting_link_register($revision_id, $input)
{
	$failed = array('ok' => false, 'created' => false, 'existing' => false, 'link_id' => 0,
		'link' => null, 'errors' => array(), 'codes' => array());
	$revision = dimension_source_revision_read($revision_id);
	if (!is_array($revision) || $revision['state'] !== 'available') {
		$failed['codes'][] = 'invalid_revision';
		$failed['errors'][] = _('A posting link needs a verified Dimension source revision.');
		return $failed;
	}
	$prepared = dimension_posting_link_prepare($revision['revision'], $input);
	if (!$prepared['ok']) {
		$failed['codes'] = $prepared['codes'];
		$failed['errors'] = $prepared['errors'];
		return $failed;
	}
	$fields = $prepared['fields'];
	$ready = dimension_posting_link_schema_ready();
	if ($ready === 'absent') {
		$failed['codes'][] = 'schema_unavailable';
		$failed['errors'][] = _('The Dimension posting lineage schema is not installed for this company.');
		return $failed;
	}
	if ($ready !== true) {
		$failed['codes'][] = 'schema_unknown';
		$failed['errors'][] = _('The Dimension posting lineage schema could not be verified.');
		return $failed;
	}
	$company_currency = dimension_posting_link_base_currency();
	if ($company_currency !== '' && $company_currency !== $fields['base_currency']) {
		$failed['codes'][] = 'currency_mismatch';
		$failed['errors'][] = _('The link base currency is not the active company base currency.');
		return $failed;
	}
	$own_transaction = empty($GLOBALS['transaction_level']);
	if ($own_transaction && !begin_transaction()) {
		$failed['codes'][] = 'transaction_unavailable';
		$failed['errors'][] = _('The Dimension posting link could not be stored; try again.');
		return $failed;
	}
	try {
		$rows = db_query('SELECT type,type_no,account,amount FROM '.TB_PREF.'gl_trans WHERE counter='
			.(int)$fields['gl_counter'].' FOR UPDATE');
		$row = $rows ? db_fetch_assoc($rows) : false;
		if (!$row || (int)$row['type'] !== $fields['gl_type']
			|| (int)$row['type_no'] !== $fields['gl_number']
			|| (string)$row['account'] !== $fields['account']) {
			if ($own_transaction) cancel_transaction();
			$failed['codes'][] = 'ledger_mismatch';
			$failed['errors'][] = _('The posting link does not match one committed ledger row on type, number, counter and account.');
			return $failed;
		}
		$existing = dimension_posting_link_find($fields['effect_key'], $fields['source_revision_id'],
			$fields['gl_counter'], true);
		if ($existing !== false) {
			if (!is_array($existing) || $existing['payload_sha256'] !== $fields['payload_sha256']) {
				if ($own_transaction) cancel_transaction();
				$failed['codes'][] = 'conflicting_link';
				$failed['errors'][] = _('A different Dimension posting attribution already exists for this effect and source.');
				return $failed;
			}
			if ($own_transaction) commit_transaction();
			$failed['ok'] = true;
			$failed['existing'] = true;
			$failed['link_id'] = (int)$existing['posting_link_id'];
			$failed['link'] = $existing;
			return $failed;
		}
		if ($fields['predecessor_link_id'] > 0) {
			$predecessor = dimension_posting_link_read($fields['predecessor_link_id'], true);
			if (!is_array($predecessor) || $predecessor['state'] !== 'available'
				|| $predecessor['link']['posting_family'] !== $fields['posting_family']
				|| $predecessor['link']['base_currency'] !== $fields['base_currency']) {
				if ($own_transaction) cancel_transaction();
				$failed['codes'][] = 'invalid_predecessor';
				$failed['errors'][] = _('A posting link predecessor must be a readable link of the same posting family and currency.');
				return $failed;
			}
		}
		$used = dimension_posting_link_quantity_used($fields['source_revision_id'], true);
		if ($used === false) {
			if ($own_transaction) cancel_transaction();
			$failed['codes'][] = 'quantity_unreadable';
			$failed['errors'][] = _('The consumed quantity of this source line could not be verified.');
			return $failed;
		}
		if ($fields['quantity_base'] !== '0.00000000') {
			$consumed = dimension_posting_decimal_units($used);
			$requested = dimension_posting_decimal_units($fields['quantity_base']);
			$available = dimension_posting_decimal_units($revision['revision']['line_quantity']);
			if ($consumed === false || $requested === false || $available === false
				|| $consumed + $requested > $available) {
				if ($own_transaction) cancel_transaction();
				$failed['codes'][] = 'quantity_exceeded';
				$failed['errors'][] = _('The posting link would consume more quantity than the source line declares.');
				return $failed;
			}
		}
		$counter_total = dimension_posting_link_counter_total($fields['gl_counter'], $fields['posting_family'],
			$fields['base_currency'], true);
		if ($counter_total === false
			|| !dimension_posting_link_attribution_admitted($row['amount'], $counter_total,
				$fields['contribution_base'])) {
			if ($own_transaction) cancel_transaction();
			$failed['codes'][] = 'ledger_mismatch';
			$failed['errors'][] = _('The signed posting contribution is not a remaining portion of the committed ledger row amount.');
			return $failed;
		}
		$inserted = db_query('INSERT INTO '.TB_PREF.'dimension_posting_links (source_revision_id,'
			.'source_identity_sha256,effect_key,posting_family,gl_type,gl_number,gl_counter,account,'
			.'contribution_base,quantity_base,base_currency,predecessor_link_id,revision_payload_sha256,'
			.'payload_sha256,created_at) VALUES ('
			.(int)$fields['source_revision_id'].','.db_escape($fields['source_identity_sha256']).','
			.db_escape($fields['effect_key']).','
			.db_escape($fields['posting_family']).','.(int)$fields['gl_type'].','
			.(int)$fields['gl_number'].','.(int)$fields['gl_counter'].','
			.db_escape($fields['account']).','.db_escape($fields['contribution_base']).','
			.db_escape($fields['quantity_base']).','.db_escape($fields['base_currency']).','
			.((int)$fields['predecessor_link_id'] > 0 ? (int)$fields['predecessor_link_id'] : 'NULL').','
			.db_escape($fields['revision_payload_sha256']).','
			.db_escape($fields['payload_sha256']).',UTC_TIMESTAMP())');
		$link_id = $inserted ? (int)db_insert_id() : 0;
		if ($link_id < 1) {
			// A concurrent identical registration can win the unique key; the committed row is the retry.
			$concurrent = dimension_posting_link_find($fields['effect_key'],
				$fields['source_revision_id'], $fields['gl_counter'], true);
			if (is_array($concurrent) && $concurrent['payload_sha256'] === $fields['payload_sha256']) {
				if ($own_transaction) commit_transaction();
				$failed['ok'] = true;
				$failed['existing'] = true;
				$failed['link_id'] = (int)$concurrent['posting_link_id'];
				$failed['link'] = $concurrent;
				return $failed;
			}
			if ($own_transaction) cancel_transaction();
			$failed['codes'][] = 'store_failed';
			$failed['errors'][] = _('The Dimension posting link could not be stored; try again.');
			return $failed;
		}
		$read = dimension_posting_link_read($link_id);
		if (!is_array($read) || $read['state'] !== 'available') {
			if ($own_transaction) cancel_transaction();
			$failed['codes'][] = 'store_verification_failed';
			$failed['errors'][] = _('The stored Dimension posting link did not verify.');
			return $failed;
		}
		if ($own_transaction && !commit_transaction()) {
			$failed['codes'][] = 'commit_failed';
			$failed['errors'][] = _('The Dimension posting link could not be committed; try again.');
			return $failed;
		}
		$failed['ok'] = true;
		$failed['created'] = true;
		$failed['link_id'] = $link_id;
		$failed['link'] = $read['link'];
		return $failed;
	} catch (Exception $exception) {
		if ($own_transaction) cancel_transaction();
		$failed['codes'][] = 'store_failed';
		$failed['errors'][] = _('The Dimension posting link could not be stored; try again.');
		return $failed;
	}
}

/**
 * Read one stored posting link and verify its seal.
 *
 * @param mixed $link_id Stored identity
 * @param bool $lock Lock the row inside the caller's transaction
 * @return array state, link, errors and codes
 */
function dimension_posting_link_read($link_id, $lock = false)
{
	$result = array('state' => 'unknown', 'link' => null, 'errors' => array(), 'codes' => array());
	$identity = dimension_source_revision_int($link_id, 1);
	if ($identity === false) {
		$result['codes'][] = 'invalid_identity';
		$result['errors'][] = _('A posting link identity must be a stored positive number.');
		return $result;
	}
	$ready = dimension_posting_link_schema_ready();
	if ($ready === 'absent') {
		$result['state'] = 'unavailable';
		$result['codes'][] = 'schema_unavailable';
		return $result;
	}
	if ($ready !== true) {
		$result['codes'][] = 'schema_unknown';
		return $result;
	}
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_posting_links WHERE posting_link_id='
		.(int)$identity.($lock ? ' FOR UPDATE' : ''));
	if (!$rows)
		return $result;
	$row = db_fetch_assoc($rows);
	if (!$row) {
		$result['state'] = 'absent';
		$result['codes'][] = 'link_absent';
		return $result;
	}
	$state = dimension_posting_link_row_state($row);
	if ($state['state'] !== 'available')
		return $state;
	$result['state'] = 'available';
	$result['link'] = $state['link'];
	return $result;
}

/**
 * Find one link by its unique effect/source/counter tuple.
 *
 * @param string $effect_key Canonical effect digest
 * @param int $source_revision_id Source revision
 * @param int $gl_counter Exact ledger counter
 * @param bool $lock Lock the row
 * @return array|false Verified link, false for absence, or an error array for a failed read
 */
function dimension_posting_link_find($effect_key, $source_revision_id, $gl_counter, $lock = false)
{
	if (!is_string($effect_key) || !preg_match('/^[a-f0-9]{64}$/D', $effect_key))
		return false;
	$ready = dimension_posting_link_schema_ready();
	if ($ready !== true)
		return array('read_error' => $ready === 'absent' ? 'schema_unavailable' : 'schema_unknown');
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_posting_links WHERE effect_key='
		.db_escape($effect_key).' AND source_revision_id='.(int)$source_revision_id
		.' AND gl_counter='.(int)$gl_counter.' LIMIT 1'.($lock ? ' FOR UPDATE' : ''));
	if (!$rows)
		return array('read_error' => 'query_failed');
	$row = db_fetch_assoc($rows);
	if (!$row)
		return false;
	$state = dimension_posting_link_row_state($row);
	return $state['state'] === 'available' ? $state['link'] : array('read_error' => $state['state']);
}

/**
 * Verify one stored posting link from its own bytes (pure source logic).
 *
 * @param array $row Stored row
 * @return array state, link, errors and codes
 */
function dimension_posting_link_row_state($row)
{
	$result = array('state' => 'tamper', 'link' => null, 'errors' => array(), 'codes' => array());
	if (!is_array($row)) {
		$result['codes'][] = 'invalid_row';
		$result['errors'][] = _('A Dimension posting link row must be an array.');
		return $result;
	}
	foreach (array_keys(dimension_posting_link_schema_columns()) as $column)
		if (!array_key_exists($column, $row)) {
			$result['codes'][] = 'missing_column';
			$result['errors'][] = sprintf(_('The stored Dimension posting link row has no %s column.'), $column);
			return $result;
		}
	$link_id = dimension_source_revision_int($row['posting_link_id'], 1);
	$revision_id = dimension_source_revision_int($row['source_revision_id'], 1);
	$gl_type = dimension_source_revision_int($row['gl_type'], 0, 32767);
	$gl_number = dimension_source_revision_int($row['gl_number'], 0);
	$gl_counter = dimension_source_revision_int($row['gl_counter'], 1);
	$predecessor = $row['predecessor_link_id'] === null ? 0
		: dimension_source_revision_int($row['predecessor_link_id'], 1);
	$contribution = dimension_source_revision_decimal($row['contribution_base'], true);
	$quantity = dimension_source_revision_decimal($row['quantity_base'], false, 9);
	$currency = (string)$row['base_currency'];
	$account = dimension_source_revision_clean_key($row['account'], 15);
	$family = (string)$row['posting_family'];
	$source_identity = (string)$row['source_identity_sha256'];
	if ($link_id === false || $revision_id === false || $gl_type === false || $gl_number === false
		|| $gl_counter === false || $predecessor === false || $contribution === false
		|| $contribution === '0.00000000' || $quantity === false || $account === false
		|| !preg_match('/^[A-Z]{3}$/D', $currency)
		|| !in_array($family, dimension_posting_link_families(), true)) {
		$result['codes'][] = 'invalid_row';
		return $result;
	}
	$fields = array(
		'company' => dimension_source_revision_company(),
		'source_revision_id' => $revision_id,
		'source_identity_sha256' => $source_identity,
		'revision_payload_sha256' => (string)$row['revision_payload_sha256'],
		'effect_key' => (string)$row['effect_key'],
		'posting_family' => $family,
		'gl_type' => $gl_type,
		'gl_number' => $gl_number,
		'gl_counter' => $gl_counter,
		'account' => $account,
		'contribution_base' => $contribution,
		'quantity_base' => $quantity,
		'base_currency' => $currency,
		'predecessor_link_id' => $predecessor);
	if (dimension_posting_effect_key($fields) !== $fields['effect_key']
		|| dimension_posting_link_payload_digest($fields) !== (string)$row['payload_sha256']) {
		$result['codes'][] = 'payload_tamper';
		return $result;
	}
	$link = $row;
	foreach ($fields as $name => $value)
		$link[$name] = $value;
	$link['posting_link_id'] = $link_id;
	$result['state'] = 'available';
	$result['link'] = $link;
	return $result;
}

/**
 * Non-negative canonical decimal to signed 64-bit units of 1e-8, bounded to nine integer digits.
 *
 * Used only for source quantities, which are bounded in the link contract so the arithmetic cannot
 * overflow.
 *
 * @param string $decimal Canonical decimal
 * @return int|false
 */
function dimension_posting_decimal_units($decimal)
{
	if (dimension_source_revision_decimal($decimal, false, 9) !== $decimal)
		return false;
	$parts = explode('.', $decimal);
	return (int)$parts[0] * 100000000 + (int)$parts[1];
}

/**
 * Format a non-negative unit count back to a canonical decimal.
 *
 * @param int $units Units of 1e-8
 * @return string|false
 */
function dimension_posting_units_decimal($units)
{
	if (!is_int($units) || $units < 0)
		return false;
	$fraction = $units % 100000000;
	return intval($units / 100000000).'.'.str_pad((string)$fraction, 8, '0', STR_PAD_LEFT);
}

/**
 * Sum the quantity already consumed from one source line.
 *
 * @param int $source_revision_id Source revision
 * @param bool $lock Lock the link rows
 * @return string|false Canonical decimal, or false when the rows cannot be read
 */
function dimension_posting_link_quantity_used($source_revision_id, $lock = false)
{
	$ready = dimension_posting_link_schema_ready();
	if ($ready !== true)
		return false;
	$rows = db_query('SELECT quantity_base FROM '.TB_PREF.'dimension_posting_links WHERE source_revision_id='
		.(int)$source_revision_id.($lock ? ' FOR UPDATE' : ''));
	if (!$rows)
		return false;
	$units = 0;
	while ($row = db_fetch_assoc($rows)) {
		$value = dimension_posting_decimal_units((string)$row['quantity_base']);
		if ($value === false)
			return false;
		$units += $value;
	}
	return dimension_posting_units_decimal($units);
}

/**
 * Remaining declared quantity of one source line.
 *
 * @param mixed $revision_id Source revision
 * @return array state (available|unbounded|unavailable|unknown), quantity, errors and codes
 */
function dimension_posting_link_quantity_remaining($revision_id)
{
	$result = array('state' => 'unknown', 'quantity' => '0.00000000', 'errors' => array(),
		'codes' => array());
	$revision = dimension_source_revision_read($revision_id);
	if (!is_array($revision) || $revision['state'] !== 'available') {
		$result['codes'][] = 'invalid_revision';
		$result['errors'][] = _('A quantity balance needs a verified Dimension source revision.');
		return $result;
	}
	$declared = dimension_source_revision_decimal($revision['revision']['line_quantity'], false, 9);
	$used = dimension_posting_link_quantity_used((int)$revision['revision']['source_revision_id']);
	if ($declared === false || $used === false) {
		$result['codes'][] = 'quantity_unreadable';
		return $result;
	}
	$declared_units = dimension_posting_decimal_units($declared);
	$used_units = dimension_posting_decimal_units($used);
	if ($declared_units === false || $used_units === false) {
		$result['codes'][] = 'quantity_unreadable';
		return $result;
	}
	$remaining = $declared_units - $used_units;
	$result['state'] = $declared_units === 0 ? 'unbounded' : 'available';
	$result['quantity'] = dimension_posting_units_decimal($remaining < 0 ? 0 : $remaining);
	return $result;
}

/**
 * Total monetary contribution attributed to one source revision, read from links only.
 *
 * Deliberately never joins `gl_trans`, so one source line feeding several ledger rows or one ledger
 * row fed by several sources cannot multiply the actual amount.
 *
 * @param mixed $revision_id Source revision
 * @return array state, link_count, contribution, errors and codes
 */
function dimension_posting_link_posted_total($revision_id)
{
	$result = array('state' => 'unknown', 'link_count' => 0, 'contribution' => '0.00000000',
		'errors' => array(), 'codes' => array());
	$revision = dimension_source_revision_read($revision_id);
	if (!is_array($revision) || $revision['state'] !== 'available') {
		$result['codes'][] = 'invalid_revision';
		$result['errors'][] = _('A posted total needs a verified Dimension source revision.');
		return $result;
	}
	$ready = dimension_posting_link_schema_ready();
	if ($ready === 'absent') {
		$result['state'] = 'unavailable';
		$result['codes'][] = 'schema_unavailable';
		return $result;
	}
	if ($ready !== true) {
		$result['codes'][] = 'schema_unknown';
		return $result;
	}
	$rows = db_query('SELECT COUNT(*) link_count,COALESCE(SUM(contribution_base),0) contribution FROM '
		.TB_PREF.'dimension_posting_links WHERE source_revision_id='
		.(int)$revision['revision']['source_revision_id']);
	$row = $rows ? db_fetch_assoc($rows) : false;
	if (!$row) {
		$result['codes'][] = 'read_failed';
		return $result;
	}
	$total = dimension_source_revision_decimal($row['contribution'], true);
	if ($total === false) {
		$result['codes'][] = 'tamper';
		return $result;
	}
	$result['state'] = 'available';
	$result['link_count'] = (int)$row['link_count'];
	$result['contribution'] = $total;
	return $result;
}

/**
 * Reconcile one posting transaction's ledger rows against their lineage links.
 *
 * Each ledger counter appears once; its linked contribution is a grouped sum over links, not a join
 * fan-out, so a consolidated posting fed by several sources is compared to the actual once.
 *
 * @param string $posting_family Reviewed posting family
 * @param mixed $gl_type Stored transaction type
 * @param mixed $gl_number Stored transaction number
 * @return array state, rows, gl_row_count, linked_row_count, gl_total, linked_total, difference
 */
function dimension_posting_link_reconcile($posting_family, $gl_type, $gl_number)
{
	$result = array('state' => 'unknown', 'rows' => array(), 'gl_row_count' => 0,
		'linked_row_count' => 0, 'gl_total' => '0.00000000', 'linked_total' => '0.00000000',
		'difference' => '0.00000000', 'errors' => array(), 'codes' => array());
	if (!in_array($posting_family, dimension_posting_link_families(), true)) {
		$result['codes'][] = 'invalid_posting_family';
		return $result;
	}
	$type = dimension_source_revision_int($gl_type, 0, 32767);
	$number = dimension_source_revision_int($gl_number, 0);
	if ($type === false || $number === false) {
		$result['codes'][] = 'invalid_posting_identity';
		return $result;
	}
	$ready = dimension_posting_link_schema_ready();
	if ($ready === 'absent') {
		$result['state'] = 'unavailable';
		$result['codes'][] = 'schema_unavailable';
		return $result;
	}
	if ($ready !== true) {
		$result['codes'][] = 'schema_unknown';
		return $result;
	}
	$rows = db_query('SELECT counter,account,amount FROM '.TB_PREF.'gl_trans WHERE type='.(int)$type
		.' AND type_no='.(int)$number.' ORDER BY counter');
	if (!$rows) {
		$result['codes'][] = 'read_failed';
		return $result;
	}
	$ledger = array();
	while ($row = db_fetch_assoc($rows))
		$ledger[] = $row;
	$gl_total = 0.0;
	$linked_total = 0.0;
	$mismatch = false;
	$unlinked = false;
	foreach ($ledger as $row) {
		$counter = (int)$row['counter'];
		$currency = dimension_posting_link_base_currency();
		$links = db_query('SELECT COUNT(*) link_count,COALESCE(SUM(quantity_base),0) quantity,'
			.'COALESCE(SUM(contribution_base),0) contribution FROM '.TB_PREF.'dimension_posting_links'
			.' WHERE gl_counter='.$counter.' AND posting_family='.db_escape($posting_family)
			.($currency === '' ? '' : ' AND base_currency='.db_escape($currency)));
		$aggregate = $links ? db_fetch_assoc($links) : false;
		if (!$aggregate) {
			$result['codes'][] = 'read_failed';
			return $result;
		}
		$contribution = dimension_source_revision_decimal($aggregate['contribution'], true);
		if ($contribution === false) {
			$result['codes'][] = 'tamper';
			return $result;
		}
		$linked_count = (int)$aggregate['link_count'];
		$matched = dimension_source_revision_amount_matches($row['amount'], $contribution);
		$difference = (float)$row['amount'] - (float)$contribution;
		$gl_total += (float)$row['amount'];
		$linked_total += (float)$contribution;
		if (!$linked_count) {
			$unlinked = true;
			$state = 'unlinked';
		} elseif ($matched) {
			$state = 'matched';
		} else {
			$mismatch = true;
			$state = 'mismatch';
		}
		$result['rows'][] = array('counter' => $counter, 'account' => (string)$row['account'],
			'gl_amount' => sprintf('%.8F', (float)$row['amount']),
			'linked_contribution' => $contribution, 'link_count' => $linked_count,
			'difference' => sprintf('%.8F', $difference), 'state' => $state);
	}
	$result['gl_row_count'] = count($ledger);
	$result['linked_row_count'] = count($result['rows']);
	$result['gl_total'] = sprintf('%.8F', $gl_total);
	$result['linked_total'] = sprintf('%.8F', $linked_total);
	$result['difference'] = sprintf('%.8F', $gl_total - $linked_total);
	if (!count($ledger))
		$result['state'] = 'empty';
	elseif ($mismatch)
		$result['state'] = 'mismatch';
	elseif ($unlinked)
		$result['state'] = 'unlinked';
	else
		$result['state'] = 'matched';
	return $result;
}
