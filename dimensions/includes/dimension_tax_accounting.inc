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
 * DIM-TAX-001: tagged tax, discount and rounding accounting.
 *
 * The sales and purchasing invoice/credit writers post their tax leg to the account of the tax type
 * with the document header pair, and write the legal register row (`trans_tax_details`) beside it.
 * Whether that tag is meaningful depends only on the account the leg really posts to:
 *
 *  - a balance-sheet tax account (VAT payable / recoverable) is a **tax control**: its tag never
 *    changes a Dimension's result, and a "Dimension required" rule must not demand a classification
 *    for a leg that is not an economic cost or revenue of that Dimension;
 *  - a profit-and-loss tax account on the purchasing side is a **non-recoverable cost**: it follows
 *    the account it was actually posted to and is a result-bearing economic line like any expense;
 *  - a profit-and-loss tax account on the sales side would put collected tax into revenue, so it is
 *    reported as a configuration finding, never silently treated as a control.
 *
 * Nothing here is accepted from a request field and nothing is written: the module reads the tax
 * type master, the chart and one document's ledger/register rows. It carries no posting authority and
 * does not change a legal tax rate, register, report identity or historical row. Tax-schema,
 * withholding and cash-basis timing are NOT FOUND in the core AR/AP/GL/tax writers; payroll
 * withholding stays with the protected HRM/payroll custody (characterized by
 * dimension_tax_accounting_characterize_source()).
 *
 * PHP 5.6 compatible syntax.
 */
require_once __DIR__.'/dimension_derived_posting_policy.inc';

/**
 * Versioned contract identifier of the tax accounting authority.
 *
 * @return string
 */
function dimension_tax_accounting_contract()
{
	return 'dimension-tax-accounting/1';
}

/**
 * Reviewed document families whose tax leg the core writers post, with the ledger sign of that leg.
 *
 * `gl_sign` is the sign of the tax-account GL line relative to the positive `trans_tax_details` amount:
 * an output tax is credited on a sales invoice and debited on a credit note, an input tax is debited on
 * a supplier invoice and credited on a supplier credit.
 *
 * @return array Transaction type => family, side, gl_sign, label
 */
function dimension_tax_accounting_document_families()
{
	$sales_invoice = defined('ST_SALESINVOICE') ? ST_SALESINVOICE : 10;
	$sales_credit = defined('ST_CUSTCREDIT') ? ST_CUSTCREDIT : 11;
	$supplier_invoice = defined('ST_SUPPINVOICE') ? ST_SUPPINVOICE : 20;
	$supplier_credit = defined('ST_SUPPCREDIT') ? ST_SUPPCREDIT : 21;
	return array(
		(int)$sales_invoice => array('family' => 'sales_invoice', 'side' => 'sales', 'gl_sign' => -1,
			'label' => _('Sales invoice')),
		(int)$sales_credit => array('family' => 'sales_credit', 'side' => 'sales', 'gl_sign' => 1,
			'label' => _('Sales credit note')),
		(int)$supplier_invoice => array('family' => 'supplier_invoice', 'side' => 'purchasing',
			'gl_sign' => 1, 'label' => _('Supplier invoice')),
		(int)$supplier_credit => array('family' => 'supplier_credit', 'side' => 'purchasing',
			'gl_sign' => -1, 'label' => _('Supplier credit note')));
}

/**
 * Reviewed classification of a tax-type account.
 *
 * @return string[] Kind => translated label
 */
function dimension_tax_accounting_kinds()
{
	return array(
		'not_tax' => _('Not a tax-type account'),
		'tax_control' => _('Balance-sheet tax control'),
		'nonrecoverable_cost' => _('Non-recoverable tax cost on a profit-and-loss account'),
		'output_on_result' => _('Collected tax on a profit-and-loss account'),
		'unclassified' => _('Tax account without a readable chart classification'));
}

/**
 * Read the tax type master.
 *
 * @param array $options tax_types: list of rows (contract tests only)
 * @return array|false Rows with id, name, rate, sales_gl_code, purchasing_gl_code, inactive; false when unreadable
 */
function dimension_tax_accounting_tax_types($options = array())
{
	if (isset($options['tax_types']) && is_array($options['tax_types']))
		return $options['tax_types'];
	$result = db_query('SELECT id, name, rate, sales_gl_code, purchasing_gl_code, inactive FROM '
		.TB_PREF.'tax_types ORDER BY id');
	if ($result === false)
		return false;
	$rows = array();
	while ($row = db_fetch_assoc($result))
		$rows[] = $row;
	return $rows;
}

/**
 * Which tax types and sides post to one GL account.
 *
 * @param mixed $account GL account code
 * @param array $options tax_types override
 * @return array|false ids (tax type ids), sides (sales/purchasing => true); false when unreadable
 */
function dimension_tax_accounting_account_usage($account, $options = array())
{
	$usage = array('ids' => array(), 'sides' => array());
	if (is_int($account))
		$account = (string)$account;	// PHP turns a numeric array key into an integer
	if (!is_string($account) || $account === '' || strlen($account) > 15)
		return $usage;
	$rows = dimension_tax_accounting_tax_types($options);
	if ($rows === false)
		return false;
	foreach ($rows as $row) {
		$id = isset($row['id']) ? (int)$row['id'] : 0;
		if (isset($row['sales_gl_code']) && (string)$row['sales_gl_code'] === $account) {
			$usage['ids'][$id] = $id;
			$usage['sides']['sales'] = true;
		}
		if (isset($row['purchasing_gl_code']) && (string)$row['purchasing_gl_code'] === $account) {
			$usage['ids'][$id] = $id;
			$usage['sides']['purchasing'] = true;
		}
	}
	$usage['ids'] = array_values($usage['ids']);
	return $usage;
}

/**
 * Classify one GL account by what it really is, never by the tax type that names it.
 *
 * @param mixed $account GL account code
 * @param array $options tax_types, account_class (balancesheet|pnl|unknown|unavailable) for contract tests
 * @return array ok, account, kind, account_class, result_effect, tax_type_ids, sides, codes and errors
 */
function dimension_tax_accounting_classify_account($account, $options = array())
{
	if (is_int($account))
		$account = (string)$account;
	$result = array('ok' => false, 'account' => is_string($account) ? $account : '', 'kind' => 'unclassified',
		'account_class' => 'unknown', 'result_effect' => 'unknown', 'tax_type_ids' => array(),
		'sides' => array(), 'errors' => array(), 'codes' => array());
	$usage = dimension_tax_accounting_account_usage($account, $options);
	if ($usage === false) {
		$result['codes'][] = 'unavailable';
		$result['errors'][] = _('The tax type master could not be read, so the tax account cannot be classified.');
		return $result;
	}
	$result['tax_type_ids'] = $usage['ids'];
	$result['sides'] = array_keys($usage['sides']);
	if (count($usage['ids']) === 0) {
		$result['ok'] = true;
		$result['kind'] = 'not_tax';
		$result['result_effect'] = 'not_applicable';
		return $result;
	}
	$class_options = array_key_exists('account_class', $options) ? array('account_class' => $options['account_class']) : array();
	if (isset($options['account_classes']) && is_array($options['account_classes'])
		&& isset($options['account_classes'][$account]))
		$class_options = array('account_class' => $options['account_classes'][$account]);
	$class = dimension_derived_posting_account_class($account, $class_options);
	$result['account_class'] = $class['account_class'];
	if (!$class['ok'] || !in_array($class['account_class'], array('balancesheet', 'pnl'), true)) {
		$result['codes'][] = $class['account_class'] === 'unavailable' ? 'unavailable' : 'unknown_tax_account';
		$result['errors'][] = sprintf(_('The tax account %s has no readable chart classification.'), $result['account']);
		return $result;
	}
	$result['ok'] = true;
	if ($class['account_class'] === 'balancesheet') {
		$result['kind'] = 'tax_control';
		$result['result_effect'] = 'none';
	} elseif (isset($usage['sides']['sales'])) {
		$result['kind'] = 'output_on_result';
		$result['result_effect'] = 'inflates_result';
	} else {
		$result['kind'] = 'nonrecoverable_cost';
		$result['result_effect'] = 'cost';
	}
	return $result;
}

/**
 * Decide whether the global "Dimension required" rule applies to a line posted to one account.
 *
 * Only a genuine balance-sheet tax control is exempt: its leg is derived from an economic line that
 * the page already validated, so demanding a project on the VAT control is an irrelevant rule. Value
 * lifecycle, enabled-axis, value-pair and explicit per-account-type rules are never relaxed here. A
 * non-tax account, a non-recoverable tax cost and any account that cannot be classified stay fully
 * judged, so an unreadable configuration can only make the rule stricter.
 *
 * @param mixed $account GL account code
 * @param array $options Classification overrides (contract tests only)
 * @return array judge_global_required (bool), kind, reason
 */
function dimension_tax_accounting_required_scope($account, $options = array())
{
	$result = array('judge_global_required' => true, 'kind' => 'not_tax', 'reason' => 'economic_line');
	if (is_int($account))
		$account = (string)$account;
	if (!is_string($account) || $account === '')
		return $result;
	$class = dimension_tax_accounting_classify_account($account, $options);
	$result['kind'] = $class['kind'];
	if ($class['ok'] && $class['kind'] === 'tax_control') {
		$result['judge_global_required'] = false;
		$result['reason'] = 'balancesheet_tax_control';
	} elseif (!$class['ok']) {
		$result['reason'] = 'unreadable_fail_closed';
	}
	return $result;
}

/**
 * Audit the tax type master for accounts that would make a tagged tax leg misleading.
 *
 * Findings are reported, never repaired: tax type configuration belongs to the tax setup page and its
 * legal owner. `severity` is `error` for a configuration that would inflate a Dimension's result or
 * cannot be classified and `info` for a legitimate non-recoverable tax cost.
 *
 * @param array $options tax_types and account_classes overrides (contract tests only)
 * @return array ok (no error finding), findings, tax_types
 */
function dimension_tax_accounting_audit($options = array())
{
	$result = array('ok' => false, 'findings' => array(), 'tax_types' => array());
	$rows = dimension_tax_accounting_tax_types($options);
	if ($rows === false) {
		$result['findings'][] = array('code' => 'unavailable', 'severity' => 'error', 'tax_type_id' => 0,
			'account' => '', 'message' => _('The tax type master could not be read.'));
		return $result;
	}
	$errors = 0;
	foreach ($rows as $row) {
		$id = isset($row['id']) ? (int)$row['id'] : 0;
		$entry = array('id' => $id, 'name' => isset($row['name']) ? (string)$row['name'] : '',
			'sales' => null, 'purchasing' => null);
		foreach (array('sales' => 'sales_gl_code', 'purchasing' => 'purchasing_gl_code') as $side => $column) {
			$account = isset($row[$column]) ? trim((string)$row[$column]) : '';
			if ($account === '') {
				$result['findings'][] = array('code' => 'tax_account_missing', 'severity' => 'error',
					'tax_type_id' => $id, 'account' => '', 'side' => $side,
					'message' => sprintf(_('Tax type %d has no %s GL account.'), $id, $side));
				$errors++;
				continue;
			}
			$class = dimension_tax_accounting_classify_account($account, $options);
			$entry[$side] = array('account' => $account, 'kind' => $class['kind'],
				'account_class' => $class['account_class'], 'result_effect' => $class['result_effect']);
			if (!$class['ok']) {
				$result['findings'][] = array('code' => 'tax_account_unclassified', 'severity' => 'error',
					'tax_type_id' => $id, 'account' => $account, 'side' => $side,
					'message' => sprintf(_('The %s tax account %s of tax type %d has no chart classification.'), $side, $account, $id));
				$errors++;
			} elseif ($class['kind'] === 'output_on_result' && $side === 'sales') {
				$result['findings'][] = array('code' => 'output_tax_on_pnl', 'severity' => 'error',
					'tax_type_id' => $id, 'account' => $account, 'side' => $side,
					'message' => sprintf(_('The sales tax account %s of tax type %d is a profit-and-loss account: collected tax would inflate the Dimension result.'), $account, $id));
				$errors++;
			} elseif ($class['kind'] === 'nonrecoverable_cost' && $side === 'purchasing') {
				$result['findings'][] = array('code' => 'nonrecoverable_tax_cost', 'severity' => 'info',
					'tax_type_id' => $id, 'account' => $account, 'side' => $side,
					'message' => sprintf(_('The purchasing tax account %s of tax type %d is a profit-and-loss account: the tax follows that account as a cost.'), $account, $id));
			}
		}
		$result['tax_types'][$id] = $entry;
	}
	$result['ok'] = $errors === 0;
	return $result;
}

/**
 * Compute Dimension result per pair from posted lines: only profit-and-loss accounts count.
 *
 * Balance-sheet lines (receivable, payable, bank, tax control) are reported as `control` but never
 * change the result, so tagging a tax-control leg cannot inflate or reduce a Dimension's result.
 *
 * @param array $rows Lines with account_class (balancesheet|pnl), dimension_id, dimension2_id and amount
 * @return array ok, pairs (key `d1/d2` => result, control, lines), result, control, errors
 */
function dimension_tax_accounting_result_by_pair($rows)
{
	$out = array('ok' => false, 'pairs' => array(), 'result' => 0.0, 'control' => 0.0, 'errors' => array());
	if (!is_array($rows)) {
		$out['errors'][] = _('The ledger lines could not be read.');
		return $out;
	}
	foreach ($rows as $row) {
		$class = is_array($row) && isset($row['account_class']) ? $row['account_class'] : '';
		if ($class !== 'balancesheet' && $class !== 'pnl') {
			$out['errors'][] = _('A ledger line has no readable account class, so the result is not computed.');
			return $out;
		}
		$d1 = isset($row['dimension_id']) ? (int)$row['dimension_id'] : 0;
		$d2 = isset($row['dimension2_id']) ? (int)$row['dimension2_id'] : 0;
		$amount = isset($row['amount']) ? (float)$row['amount'] : 0.0;
		$key = $d1.'/'.$d2;
		if (!isset($out['pairs'][$key]))
			$out['pairs'][$key] = array('result' => 0.0, 'control' => 0.0, 'lines' => 0);
		$out['pairs'][$key]['lines']++;
		if ($class === 'pnl') {
			$out['pairs'][$key]['result'] += $amount;
			$out['result'] += $amount;
		} else {
			$out['pairs'][$key]['control'] += $amount;
			$out['control'] += $amount;
		}
	}
	ksort($out['pairs']);
	$out['ok'] = true;
	return $out;
}

/**
 * Read the ledger lines of one unvoided document with their chart class.
 *
 * @param int $type Transaction type
 * @param int $trans_no Transaction number
 * @return array|false Lines, or false when unreadable
 */
function dimension_tax_accounting_document_rows($type, $trans_no)
{
	$result = db_query('SELECT gl.account, gl.dimension_id, gl.dimension2_id, gl.amount, cls.ctype FROM '
		.TB_PREF.'gl_trans gl LEFT JOIN '.TB_PREF.'voided v ON v.type = gl.type AND v.id = gl.type_no'
		.' LEFT JOIN '.TB_PREF.'chart_master cm ON cm.account_code = gl.account'
		.' LEFT JOIN '.TB_PREF.'chart_types typ ON typ.id = cm.account_type'
		.' LEFT JOIN '.TB_PREF.'chart_class cls ON cls.cid = typ.class_id'
		.' WHERE gl.type='.(int)$type.' AND gl.type_no='.(int)$trans_no.' AND v.date_ IS NULL ORDER BY gl.counter');
	if ($result === false)
		return false;
	$income_class = defined('CL_INCOME') ? CL_INCOME : 4;
	$rows = array();
	while ($row = db_fetch_assoc($result)) {
		$ctype = isset($row['ctype']) && is_numeric($row['ctype']) ? (int)$row['ctype'] : 0;
		$row['account_class'] = $ctype > 0 ? ($ctype < $income_class ? 'balancesheet' : 'pnl') : 'unknown';
		$rows[] = $row;
	}
	return $rows;
}

/**
 * Reconcile one posted document's legal tax register to its tax-account ledger legs.
 *
 * For every GL account that a tax type of the document's side posts to, the expected ledger movement
 * is the document's register amount in home currency (`amount * ex_rate`, exactly the tax report's
 * conversion) times the family's reviewed sign. The difference must stay within rounding tolerance;
 * a register row without a leg, or a leg without a register row, is its own finding. Rounding and
 * exchange legs on the company's declared accounts are reported as a separate bucket and never hidden
 * inside a tax account. The function is read-only and never repairs a document.
 *
 * @param int $type Transaction type (sales/supplier invoice or credit)
 * @param int $trans_no Transaction number
 * @param array $options tolerance (default 0.02 per document), tax_types, rows, details for contract tests
 * @return array ok, family, accounts, balancing, project_result, findings, errors
 */
function dimension_tax_accounting_reconcile_document($type, $trans_no, $options = array())
{
	$out = array('ok' => false, 'family' => '', 'accounts' => array(), 'balancing' => 0.0,
		'result_by_pair' => array(), 'findings' => array(), 'errors' => array());
	$families = dimension_tax_accounting_document_families();
	$type = (int)$type;
	if (!isset($families[$type])) {
		$out['findings'][] = array('code' => 'unsupported_type', 'severity' => 'error', 'account' => '',
			'message' => _('Only sales/supplier invoices and credit notes carry a reconcilable tax leg.'));
		$out['errors'][] = _('Only sales/supplier invoices and credit notes carry a reconcilable tax leg.');
		return $out;
	}
	$family = $families[$type];
	$out['family'] = $family['family'];
	$tolerance = isset($options['tolerance']) && is_numeric($options['tolerance']) ? (float)$options['tolerance'] : 0.02;
	$types = dimension_tax_accounting_tax_types($options);
	if ($types === false) {
		$out['errors'][] = _('The tax type master could not be read.');
		return $out;
	}
	$details = array();
	if (isset($options['details']) && is_array($options['details'])) {
		$details = $options['details'];
	} else {
		$result = db_query('SELECT tax_type_id, SUM(amount * IF(ex_rate = 0, 1, ex_rate)) AS home_amount,'
			.' SUM(net_amount * IF(ex_rate = 0, 1, ex_rate)) AS home_net, COUNT(*) AS rows_count FROM '
			.TB_PREF.'trans_tax_details WHERE trans_type='.$type.' AND trans_no='.(int)$trans_no
			.' GROUP BY tax_type_id');
		if ($result === false) {
			$out['errors'][] = _('The legal tax register could not be read.');
			return $out;
		}
		while ($row = db_fetch_assoc($result))
			$details[(int)$row['tax_type_id']] = array('home_amount' => (float)$row['home_amount'],
				'home_net' => (float)$row['home_net'], 'rows' => (int)$row['rows_count']);
	}
	$rows = isset($options['rows']) && is_array($options['rows'])
		? $options['rows'] : dimension_tax_accounting_document_rows($type, $trans_no);
	if ($rows === false) {
		$out['errors'][] = _('The document ledger could not be read.');
		return $out;
	}
	$column = $family['side'] === 'sales' ? 'sales_gl_code' : 'purchasing_gl_code';
	$expected = array();
	foreach ($types as $row) {
		$id = isset($row['id']) ? (int)$row['id'] : 0;
		$account = isset($row[$column]) ? trim((string)$row[$column]) : '';
		if ($account === '')
			continue;
		if (!isset($expected[$account]))
			$expected[$account] = array('expected' => 0.0, 'actual' => 0.0, 'tax_type_ids' => array(),
				'register_rows' => 0, 'legs' => 0);
		$expected[$account]['tax_type_ids'][$id] = $id;
		if (isset($details[$id])) {
			$expected[$account]['expected'] += $family['gl_sign'] * (float)$details[$id]['home_amount'];
			$expected[$account]['register_rows'] += isset($details[$id]['rows']) ? (int)$details[$id]['rows'] : 1;
		}
	}
	$declared = dimension_derived_posting_declared_accounts($options);
	foreach ($rows as $row) {
		$account = isset($row['account']) ? (string)$row['account'] : '';
		$amount = isset($row['amount']) ? (float)$row['amount'] : 0.0;
		if (isset($expected[$account])) {
			$expected[$account]['actual'] += $amount;
			$expected[$account]['legs']++;
		} elseif (in_array($account, $declared['declared'], true)) {
			$out['balancing'] += $amount;
		}
	}
	foreach ($expected as $account => $entry) {
		$entry['tax_type_ids'] = array_values($entry['tax_type_ids']);
		$entry['difference'] = round($entry['actual'] - $entry['expected'], 4);
		$out['accounts'][$account] = $entry;
		if ($entry['register_rows'] > 0 && $entry['legs'] === 0 && abs($entry['expected']) > $tolerance)
			$out['findings'][] = array('code' => 'tax_detail_without_gl_leg', 'severity' => 'error',
				'account' => (string)$account, 'message' => sprintf(_('The tax register has %s for account %s but the document posted no ledger leg to it.'), round($entry['expected'], 2), $account));
		elseif ($entry['register_rows'] === 0 && $entry['legs'] > 0 && abs($entry['actual']) > $tolerance)
			$out['findings'][] = array('code' => 'gl_tax_leg_without_detail', 'severity' => 'error',
				'account' => (string)$account, 'message' => sprintf(_('Account %s carries a tax ledger leg with no legal tax register row.'), $account));
		elseif (abs($entry['difference']) > $tolerance)
			$out['findings'][] = array('code' => 'tax_gl_mismatch', 'severity' => 'error',
				'account' => (string)$account, 'message' => sprintf(_('The tax register and ledger differ by %s on account %s.'), $entry['difference'], $account));
	}
	$by_pair = dimension_tax_accounting_result_by_pair($rows);
	if ($by_pair['ok']) {
		$out['result_by_pair'] = $by_pair['pairs'];
	} else {
		$out['findings'][] = array('code' => 'unclassified_line', 'severity' => 'error', 'account' => '',
			'message' => $by_pair['errors'][0]);
	}
	ksort($out['accounts']);
	$out['ok'] = count($out['findings']) === 0;
	return $out;
}

/**
 * Characterize what the core writers do with tax, discount and rounding legs, from source only.
 *
 * Reads the reviewed writer files (no execution) so the reviewed facts cannot drift unnoticed: the
 * four tax legs post the document header pair through the tax-type account and write the register in
 * the same command, the shared rounding helper declares its role, and no core AR/AP/GL/tax writer
 * implements withholding or cash-basis tax timing.
 *
 * @param string $root Application root (defaults to the includes tree's root)
 * @return array ok, facts (name => bool), unreadable, errors
 */
function dimension_tax_accounting_characterize_source($root = null)
{
	$out = array('ok' => false, 'facts' => array(), 'unreadable' => array(), 'errors' => array());
	if ($root === null)
		$root = dirname(dirname(__DIR__));
	$files = array(
		'sales_invoice' => 'sales/includes/db/sales_invoice_db.inc',
		'sales_credit' => 'sales/includes/db/sales_credit_db.inc',
		'purchasing_invoice' => 'purchasing/includes/db/invoice_db.inc',
		'gl_trans' => 'gl/includes/db/gl_db_trans.inc',
		'items_cart' => 'includes/ui/items_cart.inc',
		'tax_report' => 'reporting/rep709.php');
	$source = array();
	foreach ($files as $name => $relative) {
		$text = is_file($root.'/'.$relative) ? file_get_contents($root.'/'.$relative) : false;
		if ($text === false) {
			$out['unreadable'][] = $relative;
			continue;
		}
		$source[$name] = $text;
	}
	if (count($out['unreadable']) > 0) {
		$out['errors'][] = _('A reviewed tax writer could not be read.');
		return $out;
	}
	$facts = array();
	$facts['sales_invoice_tax_leg_header_pair'] = (bool)preg_match(
		'/\$taxitem\[\'sales_gl_code\'\],\s*\$invoice->dimension_id,\s*\$invoice->dimension2_id/', $source['sales_invoice']);
	$facts['sales_credit_tax_leg_header_pair'] = (bool)preg_match(
		'/\$taxitem\[\'sales_gl_code\'\],\s*\$credit_note->dimension_id,\s*\$credit_note->dimension2_id/', $source['sales_credit']);
	$facts['supplier_invoice_tax_leg_document_pair'] = (bool)preg_match(
		'/\$taxitem\[\'purchasing_gl_code\'\],\s*\$dim,\s*\$dim2/', $source['purchasing_invoice']);
	$facts['sales_register_row_per_tax'] = (bool)preg_match('/add_trans_tax_details\(ST_SALESINVOICE[^;]*TR_OUTPUT/s', $source['sales_invoice']);
	$facts['supplier_register_row_per_tax'] = (bool)preg_match('/add_trans_tax_details\(\$trans_type[^;]*TR_INPUT/s', $source['purchasing_invoice']);
	$facts['balancing_helper_declares_rounding_role'] = (bool)preg_match(
		'/function add_gl_balance.*?\'rounding\'\);/s', $source['gl_trans']);
	$facts['cart_rounding_declares_rounding_role'] = (bool)preg_match(
		'/derived_role\s*=\s*\'rounding\'/', $source['items_cart']);
	$facts['tax_report_converts_with_register_rate'] = (bool)preg_match(
		'/taxrec\.amount\*ex_rate AS amount/', $source['tax_report']);
	$facts['balancing_called_after_tax_legs'] = strpos($source['sales_invoice'], 'add_gl_balance(ST_SALESINVOICE')
		> strpos($source['sales_invoice'], 'A tax GL posting could not be inserted');
	$timing = 0;
	foreach (array('sales', 'purchasing', 'gl', 'taxes') as $directory) {
		$path = $root.'/'.$directory;
		if (!is_dir($path))
			continue;
		$iterator = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($path, FilesystemIterator::SKIP_DOTS));
		foreach ($iterator as $file) {
			if (!$file->isFile() || !preg_match('/\.(inc|php)$/D', $file->getFilename()))
				continue;
			$text = file_get_contents($file->getPathname());
			if ($text !== false && preg_match('/withholding_tax|tax_withholding|cash_basis_tax|tax_on_payment/i', $text))
				$timing++;
		}
	}
	$facts['no_core_withholding_or_cash_basis_writer'] = $timing === 0;
	$out['facts'] = $facts;
	$out['ok'] = !in_array(false, $facts, true);
	return $out;
}
