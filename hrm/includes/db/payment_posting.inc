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

include_once($path_to_root.'/gl/includes/gl_db.inc');
include_once($path_to_root.'/includes/ui/items_cart.inc');

/**
 * Build the exact payroll payment bank-account projection.
 *
 * Negative identifiers are reserved for rollback-only test fixtures. The
 * public repository helper below accepts only persisted positive identifiers.
 * The projection deliberately excludes bank name, address and account number.
 *
 * @param mixed $bank_account_id
 * @return string|false
 */
function get_payroll_payment_bank_account_sql($bank_account_id) {
	$bank_account_id = (int)$bank_account_id;
	if ($bank_account_id === 0)
		return false;

	return "SELECT id, account_code, bank_curr_code FROM ".TB_PREF."bank_accounts"
		." WHERE id = ".db_escape($bank_account_id);
}

/**
 * Fetch a valid source bank account row.
 *
 * @param mixed $bank_account_id
 * @return array|false
 */
function get_payroll_payment_bank_account($bank_account_id) {
	$bank_account_id = (int)$bank_account_id;
	if ($bank_account_id <= 0)
		return false;

	$sql = get_payroll_payment_bank_account_sql($bank_account_id);
	if ($sql === false)
		return false;
	$result = db_query($sql, 'could not fetch payroll payment bank account');
	$row = db_fetch($result);

	return $row ? $row : false;
}

/**
 * Build the exact payroll payable-account projection.
 *
 * The query returns only the configured GL account code. It deliberately
 * excludes preference metadata and all chart-master descriptive fields.
 *
 * @return string
 */
function get_payroll_payment_payable_account_sql() {
	return "SELECT coa.account_code FROM ".TB_PREF."sys_prefs prefs"
		." INNER JOIN ".TB_PREF."chart_master coa ON coa.account_code = TRIM(prefs.value)"
		." WHERE prefs.name = 'payroll_payable_act'";
}

/**
 * Resolve the payroll payable GL account configured in HR settings.
 *
 * @return string|false
 */
function get_payroll_payment_payable_account() {
	$result = db_query(
		get_payroll_payment_payable_account_sql(),
		'could not fetch payroll payable account'
	);
	$row = db_fetch($result);
	if (!$row || !isset($row['account_code']))
		return false;

	$payable_account = trim((string)$row['account_code']);
	return $payable_account === '' ? false : $payable_account;
}

/**
 * Normalize a payment date to the user date format expected by core posting helpers.
 *
 * @param string $payment_date
 * @return string
 */
function get_payroll_payment_date($payment_date) {
	$payment_date = trim((string)$payment_date);
	if ($payment_date === '')
		return Today();
	if (preg_match('/^\d{4}-\d{2}-\d{2}$/', $payment_date))
		return sql2date($payment_date);
	if (!is_date($payment_date))
		return Today();

	return $payment_date;
}

/**
 * Resolve a bank payment reference for payroll payments.
 *
 * @param string $reference
 * @param string $payment_date
 * @return string
 */
function get_payroll_payment_reference($reference, $payment_date) {
	global $Refs;

	$reference = trim((string)$reference);
	if ($reference !== '')
		return $reference;

	if (isset($Refs) && is_object($Refs) && method_exists($Refs, 'get_next'))
		return $Refs->get_next(ST_BANKPAYMENT, null, array('date' => $payment_date));

	return 'PAY-'.date('YmdHis');
}

/**
 * Build the memo used for payroll payment bank transactions.
 *
 * @param array $payslip
 * @param string $memo
 * @return string
 */
function get_payroll_payment_memo($payslip, $memo='') {
	$memo = trim((string)$memo);
	if ($memo !== '')
		return $memo;

	$parts = array(_('Payslip Payment'));
	if (!empty($payslip['reference']))
		$parts[] = $payslip['reference'];
	elseif (!empty($payslip['payslip_id']))
		$parts[] = '#'.(int)$payslip['payslip_id'];

	return implode(' - ', $parts);
}

/**
 * Create a core bank payment transaction for a validated payslip row.
 *
 * @param array $payslip
 * @param int $bank_account_id
 * @param string $payment_date
 * @param string $reference
 * @param string $memo
 * @param bool $use_transaction
 * @return int|false
 */
function write_payslip_payment_transaction($payslip, $bank_account_id, $payment_date='', $reference='', $memo='', $use_transaction=true) {
	if (!is_array($payslip) || empty($payslip['employee_id']) || empty($payslip['payable_amount']))
		return false;

	$payable_amount = (float)$payslip['payable_amount'];
	if ($payable_amount <= 0)
		return false;

	$payable_account = get_payroll_payment_payable_account();
	if ($payable_account === false)
		return false;

	$payment_date = get_payroll_payment_date($payment_date);
	$reference = get_payroll_payment_reference($reference, $payment_date);
	$memo = get_payroll_payment_memo($payslip, $memo);

	$items = new items_cart(ST_BANKPAYMENT);
	$items->tran_date = $payment_date;
	$items->reference = $reference;
	$items->memo_ = $memo;
	$items->add_gl_item($payable_account, 0, 0, $payable_amount, $memo, null, null, $payment_date);

	$trans = write_bank_transaction(
		ST_BANKPAYMENT,
		0,
		(int)$bank_account_id,
		$items,
		$payment_date,
		PT_EMPLOYEE,
		$payslip['employee_id'],
		null,
		$reference,
		$memo,
		$use_transaction,
		null
	);

	if (!is_array($trans) || empty($trans[1]))
		return false;

	return (int)$trans[1];
}

/**
 * Create a bank payment transaction for payslip payment
 *
 * @param int $payslip_id
 * @param string $bank_account_code
 * @param string $payment_date
 * @param string $reference
 * @param string $memo
 * @param bool $use_transaction
 * @return int|false Returns payment transaction number on success, false on failure
 */
function create_payment_transaction_for_payslip($payslip_id, $bank_account_code='', $payment_date='', $reference='', $memo='', $use_transaction=true) {
	$payslip_id = (int)$payslip_id;
	if ($payslip_id <= 0)
		return false;

	$payslip = get_payslip($payslip_id);
	if (!$payslip)
		return false;

	$locked_period_id = isset($payslip['payroll_period_id'])
		? (int)$payslip['payroll_period_id'] : 0;

	if ($use_transaction)
		begin_transaction();

	if ($use_transaction && $locked_period_id > 0) {
		$period_lock_sql = "SELECT period_id FROM ".TB_PREF."payroll_periods"
			." WHERE period_id = ".db_escape($locked_period_id)." FOR UPDATE";
		if (!db_query($period_lock_sql, 'could not lock payroll period for payslip payment')) {
			cancel_transaction();
			return false;
		}
	}

	if ($use_transaction) {
		$table_name = payslip_header_table();
		$lock_sql = "SELECT payslip_id FROM ".TB_PREF.$table_name
			." WHERE payslip_id = ".db_escape($payslip_id)." FOR UPDATE";
		if (!$table_name || !db_query($lock_sql, 'could not lock payslip for payment')) {
			cancel_transaction();
			return false;
		}
	}

	$payslip = get_payslip($payslip_id);
	$current_period_id = $payslip && isset($payslip['payroll_period_id'])
		? (int)$payslip['payroll_period_id'] : 0;
	if (!$payslip || ($use_transaction && $current_period_id !== $locked_period_id)) {
		if ($use_transaction)
			cancel_transaction();
		return false;
	}

	$validation = validate_payslip_payment($payslip, $bank_account_code);
	if (!$validation['valid']) {
		if ($use_transaction)
			cancel_transaction();
		return false;
	}

	$bank_account = get_payroll_payment_bank_account($bank_account_code);
	if (!$bank_account) {
		if ($use_transaction)
			cancel_transaction();
		return false;
	}

	$payment_date = get_payroll_payment_date($payment_date);
	$next_trans_no = write_payslip_payment_transaction($payslip, $bank_account['id'], $payment_date, $reference, $memo, false);
	if (!$next_trans_no || !mark_payslip_paid($payslip_id, $next_trans_no, false)) {
		if ($use_transaction)
			cancel_transaction();
		return false;
	}

	if ($use_transaction)
		commit_transaction();
	return $next_trans_no;
}

/**
 * Create bank payment transactions for every unpaid payslip in a payroll period.
 *
 * @param int $period_id
 * @param string $bank_account_code
 * @param string $payment_date
 * @param string $reference
 * @param string $memo
 * @param bool $use_transaction
 * @return array|false Returns created transaction numbers keyed by payslip id, or false on failure
 */
function create_payment_transactions_for_period($period_id, $bank_account_code='', $payment_date='', $reference='', $memo='', $use_transaction=true) {
	$period_id = (int)$period_id;
	if ($period_id <= 0)
		return false;
	$batch_authorized = function_exists('user_check_access')
		&& isset($_SESSION['wa_current_user'])
		&& user_check_access('SA_PAYMENTBATCH');
	if (!$batch_authorized) {
		payroll_payment_control_log_denial(
			'batch_payment_unauthorized',
			array('payroll_period_id' => $period_id),
			'payment_batch',
			$period_id
		);
		return false;
	}

	global $transaction_level;
	if (!$use_transaction && empty($transaction_level)) {
		payroll_payment_control_log_denial(
			'batch_payment_transaction_required',
			array('payroll_period_id' => $period_id),
			'payment_batch',
			$period_id
		);
		return false;
	}

	if ($use_transaction)
		begin_transaction();

	$locked_set = get_locked_payroll_period_payment_set($period_id);
	if ($locked_set === false) {
		cancel_transaction();
		return false;
	}
	$period = $locked_set['period'];
	$payslips = $locked_set['payslips'];
	$current_status = isset($period['status']) ? (int)$period['status'] : -1;

	$set_validation = validate_payroll_period_payment_set(
		$period_id,
		$period,
		$payslips,
		true
	);
	if (empty($set_validation['valid'])) {
		cancel_transaction();
		return false;
	}

	$bank_account = get_payroll_payment_bank_account($bank_account_code);
	if (!$bank_account) {
		cancel_transaction();
		return false;
	}

	$payment_date = get_payroll_payment_date($payment_date);
	$transactions = array();
	$expected_paid_set = array();
	foreach ($payslips as $payslip) {
		$payslip_id = (int)$payslip['payslip_id'];
		$payment_trans_no = null;
		if (!payslip_requires_payment($payslip)) {
			if (!mark_payslip_paid_for_batch_compare_and_set($payslip, null)) {
				cancel_transaction();
				return false;
			}
		} else {
			$payment_reference = '';
			if ($reference !== '' && empty($transactions))
				$payment_reference = $reference;

			$payment_trans_no = write_payslip_payment_transaction(
				$payslip,
				$bank_account['id'],
				$payment_date,
				$payment_reference,
				$memo,
				false
			);
			if (!$payment_trans_no
				|| !mark_payslip_paid_for_batch_compare_and_set($payslip, $payment_trans_no)
			) {
				cancel_transaction();
				return false;
			}

			$transactions[$payslip_id] = (int)$payment_trans_no;
		}

		$expected_paid_set[$payslip_id] = array(
			'payroll_period_id' => $period_id,
			'payroll_gl_trans_no' => payroll_payment_payslip_gl_trans_no($payslip),
			'payment_trans_no' => $payment_trans_no === null ? 0 : (int)$payment_trans_no,
		);
	}

	$post_write_verification = verify_payroll_period_payment_set_after_write(
		$period_id,
		$expected_paid_set
	);
	if (empty($post_write_verification['valid'])
		|| !mark_payroll_period_paid_for_batch_compare_and_set(
			$period_id,
			$current_status,
			$payment_date
		)
	) {
		cancel_transaction();
		return false;
	}

	global $path_to_root;
	include_once($path_to_root.'/includes/db/domain_audit_db.inc');
	$audit_result = function_exists('append_domain_audit_event')
		? append_domain_audit_event(array(
			'legal_entity_id' => 0,
			'entity_type' => 'payroll_period',
			'entity_key' => array('period_id' => $period_id),
			'action' => 'payroll_period.payment_batch_completed',
			'reason_code' => 'approved_posted_set_payment',
			'before' => array('period_id' => $period_id, 'status' => $current_status),
			'after' => array('period_id' => $period_id, 'status' => 4),
			'restricted_diff' => array(
				'changed_fields' => array('status', 'pay_date', 'payslip_status', 'payment_trans_no'),
				'mutation_kind' => 'payment_batch_completed',
				'approved_posted_set_verified' => true,
				'payslip_count' => count($expected_paid_set),
				'bank_transaction_count' => count($transactions),
				'nonpositive_payable_count' => count($expected_paid_set) - count($transactions),
				'restricted_values_stored' => false,
			),
		)) : false;
	if ($audit_result === false) {
		cancel_transaction();
		return false;
	}

	if ($use_transaction)
		commit_transaction();
	return $transactions;
}

/**
 * Post GL entries for payslip payment
 *
 * @param int $payslip_id
 * @param int $payment_trans_no
 * @param string $bank_account_code
 * @param string $payment_date
 * @return bool
 */
function post_payment_gl_entries($payslip_id, $payment_trans_no, $bank_account_code, $payment_date) {
	$payslip_id = (int)$payslip_id;
	$payment_trans_no = (int)$payment_trans_no;
	
	if ($payslip_id <= 0 || $payment_trans_no <= 0)
		return false;

	$sql = "SELECT COUNT(*) AS cnt FROM ".TB_PREF."gl_trans"
		." WHERE type = ".db_escape(ST_BANKPAYMENT)
		." AND type_no = ".db_escape($payment_trans_no);
	$result = db_query($sql, 'could not check existing payroll payment GL entries');
	$row = db_fetch($result);
	if ($row && (int)$row['cnt'] > 0)
		return true;

	$payslip = get_payslip($payslip_id);
	$bank_account = get_payroll_payment_bank_account($bank_account_code);
	$payable_account = get_payroll_payment_payable_account();
	if (!$payslip || !$bank_account || $payable_account === false || !payslip_requires_payment($payslip))
		return false;

	$payment_date = get_payroll_payment_date($payment_date);
	$memo = get_payroll_payment_memo($payslip);
	if (!add_gl_trans(ST_BANKPAYMENT, $payment_trans_no, $payment_date, $payable_account, 0, 0, $memo, (float)$payslip['payable_amount'], null, PT_EMPLOYEE, $payslip['employee_id'], 'could not post payroll payable GL entry'))
		return false;
	if (!add_gl_trans(ST_BANKPAYMENT, $payment_trans_no, $payment_date, get_bank_gl_account($bank_account['id']), 0, 0, $memo, -(float)$payslip['payable_amount'], null, PT_EMPLOYEE, $payslip['employee_id'], 'could not post payroll bank GL entry'))
		return false;

	return true;
}

/**
 * Resolve the current actor id for PII-free payment-control logging.
 *
 * @return int
 */
function payroll_payment_control_actor_id() {
	if (!isset($_SESSION['wa_current_user']))
		return 0;

	$user = $_SESSION['wa_current_user'];
	if (isset($user->user) && is_numeric($user->user))
		return (int)$user->user;
	if (isset($user->id) && is_numeric($user->id))
		return (int)$user->id;

	return 0;
}

/**
 * Record a minimized denial event for an individual or batch-payment boundary.
 *
 * No employee identity, amount, bank data, reference, memo, date, or GL value
 * is written to the log. PAY-AUD immutable evidence remains reserved for
 * committed domain mutations; this is a fail-closed containment signal.
 *
 * @param string $code
 * @param array $payslip
 * @param string $channel
 * @param int $period_id
 * @return void
 */
function payroll_payment_control_log_denial($code, $payslip, $channel='employee_bank_entry', $period_id=0) {
	$channel = $channel === 'payment_batch' ? 'payment_batch' : 'employee_bank_entry';
	$period_id = (int)$period_id;
	if ($period_id <= 0 && isset($payslip['payroll_period_id']))
		$period_id = (int)$payslip['payroll_period_id'];

	$payload = array(
		'contract' => 'notrinoserp.hrm.payroll.payment-control',
		'version' => '1.0.0',
		'action' => $channel === 'payment_batch'
			? 'batch_payment_denied' : 'individual_payment_denied',
		'code' => preg_replace('/[^a-z0-9_\-]/', '', strtolower((string)$code)),
		'company_id' => function_exists('user_company') ? (int)user_company() : 0,
		'actor_id' => payroll_payment_control_actor_id(),
		'payslip_id' => isset($payslip['payslip_id']) ? (int)$payslip['payslip_id'] : 0,
		'payroll_period_id' => $period_id,
		'channel' => $channel,
	);

	error_log('PAY-CORE-011 '.json_encode($payload));
}

/**
 * Return the payroll transaction identity linked to one payslip.
 *
 * @param array $payslip
 * @return int
 */
function payroll_payment_payslip_gl_trans_no($payslip) {
	if (!is_array($payslip))
		return 0;
	if (isset($payslip['gl_trans_no']))
		return (int)$payslip['gl_trans_no'];
	if (isset($payslip['trans_no']))
		return (int)$payslip['trans_no'];
	return 0;
}

/**
 * Lock one payroll period and its complete payslip relationship.
 *
 * The payslip query uses the existing period-leading unique index. Under the
 * platform's transaction boundary, the locking range prevents a concurrent
 * insert, removal, or reassignment from changing the payment set before the
 * command commits.
 *
 * @param int $period_id
 * @return array|false
 */
function get_locked_payroll_period_payment_set($period_id) {
	$period_id = (int)$period_id;
	$table_name = function_exists('payslip_header_table') ? payslip_header_table() : false;
	if ($period_id <= 0 || !$table_name)
		return false;

	$period_query = db_query(
		"SELECT * FROM ".TB_PREF."payroll_periods"
			." WHERE period_id = ".db_escape($period_id)." FOR UPDATE",
		'could not lock payroll period for payment batch'
	);
	$period = $period_query ? db_fetch_assoc($period_query) : false;
	if (!$period)
		return false;

	$payslip_query = db_query(
		"SELECT * FROM ".TB_PREF.$table_name
			." WHERE payroll_period_id = ".db_escape($period_id)
			." ORDER BY payslip_id FOR UPDATE",
		'could not lock payroll period payslips for payment batch'
	);
	if (!$payslip_query)
		return false;

	$payslips = array();
	while ($row = db_fetch_assoc($payslip_query))
		$payslips[] = $row;

	return array('period' => $period, 'payslips' => $payslips);
}

/**
 * Validate the complete locked batch-payment set before any bank write.
 *
 * Every row must remain linked to the selected Approved/Posted period, be an
 * unpaid non-voided Posted payslip, and have a unique, non-voided, balanced,
 * nonzero ST_PAYSLIP transaction. A single invalid row rejects the whole set.
 *
 * @param int $period_id
 * @param array $period
 * @param array $payslips
 * @param bool $audit_denial
 * @return array
 */
function validate_payroll_period_payment_set($period_id, $period, $payslips, $audit_denial=true) {
	$period_id = (int)$period_id;
	$result = array(
		'valid' => false,
		'code' => 'invalid_payment_set',
		'message' => 'Payroll batch payment requires one complete approved or posted payslip set'
	);
	$denial_row = array('payroll_period_id' => $period_id);

	if ($period_id <= 0 || !is_array($period)
		|| !isset($period['period_id']) || (int)$period['period_id'] !== $period_id
	) {
		$result['code'] = 'payroll_period_relationship_drift';
		if ($audit_denial)
			payroll_payment_control_log_denial($result['code'], $denial_row, 'payment_batch', $period_id);
		return $result;
	}

	$period_status = isset($period['status']) ? (int)$period['status'] : -1;
	if (!in_array($period_status, array(2, 3), true)) {
		$result['code'] = 'payroll_period_not_approved';
		if ($audit_denial)
			payroll_payment_control_log_denial($result['code'], $denial_row, 'payment_batch', $period_id);
		return $result;
	}

	if (!is_array($payslips) || empty($payslips)) {
		$result['code'] = 'payroll_payment_set_empty';
		if ($audit_denial)
			payroll_payment_control_log_denial($result['code'], $denial_row, 'payment_batch', $period_id);
		return $result;
	}

	$seen_payslips = array();
	$seen_trans_nos = array();
	foreach ($payslips as $payslip) {
		if (!is_array($payslip) || empty($payslip['payslip_id'])) {
			$result['code'] = 'invalid_payslip';
			if ($audit_denial)
				payroll_payment_control_log_denial($result['code'], $denial_row, 'payment_batch', $period_id);
			return $result;
		}

		$payslip_id = (int)$payslip['payslip_id'];
		$payslip_period_id = isset($payslip['payroll_period_id'])
			? (int)$payslip['payroll_period_id'] : 0;
		if ($payslip_period_id !== $period_id || isset($seen_payslips[$payslip_id])) {
			$result['code'] = $payslip_period_id !== $period_id
				? 'payroll_period_relationship_drift' : 'duplicate_payslip_identity';
			if ($audit_denial)
				payroll_payment_control_log_denial($result['code'], $payslip, 'payment_batch', $period_id);
			return $result;
		}
		$seen_payslips[$payslip_id] = true;

		if (payslip_is_voided($payslip)) {
			$result['code'] = 'voided_payslip_in_payment_set';
			if ($audit_denial)
				payroll_payment_control_log_denial($result['code'], $payslip, 'payment_batch', $period_id);
			return $result;
		}
		if (payslip_is_paid($payslip)
			|| (isset($payslip['payment_trans_no']) && (int)$payslip['payment_trans_no'] > 0)
		) {
			$result['code'] = 'partially_paid_payment_set';
			if ($audit_denial)
				payroll_payment_control_log_denial($result['code'], $payslip, 'payment_batch', $period_id);
			return $result;
		}
		if (!isset($payslip['status']) || (int)$payslip['status'] !== 2) {
			$result['code'] = 'payslip_not_posted';
			if ($audit_denial)
				payroll_payment_control_log_denial($result['code'], $payslip, 'payment_batch', $period_id);
			return $result;
		}

		$gl_trans_no = payroll_payment_payslip_gl_trans_no($payslip);
		if ($gl_trans_no <= 0 || isset($seen_trans_nos[$gl_trans_no])) {
			$result['code'] = $gl_trans_no <= 0
				? 'payroll_gl_link_missing' : 'duplicate_payroll_gl_link';
			if ($audit_denial)
				payroll_payment_control_log_denial($result['code'], $payslip, 'payment_batch', $period_id);
			return $result;
		}
		$seen_trans_nos[$gl_trans_no] = true;

		$posted_set = validate_payslip_approved_posted_set($payslip, false);
		if (empty($posted_set['valid'])) {
			$result['code'] = isset($posted_set['code'])
				? $posted_set['code'] : 'invalid_payroll_gl_posting';
			if ($audit_denial)
				payroll_payment_control_log_denial($result['code'], $payslip, 'payment_batch', $period_id);
			return $result;
		}
	}

	return array(
		'valid' => true,
		'code' => 'approved_posted_payment_set',
		'message' => '',
		'payslip_count' => count($payslips),
	);
}

/**
 * Compare-and-set one locked payslip to Paid for the batch command.
 *
 * @param array $payslip
 * @param int|null $payment_trans_no
 * @return bool
 */
function mark_payslip_paid_for_batch_compare_and_set($payslip, $payment_trans_no=null) {
	$table_name = function_exists('payslip_header_table') ? payslip_header_table() : false;
	if (!$table_name || !is_array($payslip)
		|| empty($payslip['payslip_id']) || empty($payslip['payroll_period_id'])
		|| !function_exists('payslip_has_column')
		|| !payslip_has_column($table_name, 'status')
		|| !payslip_has_column($table_name, 'payment_trans_no')
	) {
		return false;
	}

	$payslip_id = (int)$payslip['payslip_id'];
	$period_id = (int)$payslip['payroll_period_id'];
	$gl_trans_no = payroll_payment_payslip_gl_trans_no($payslip);
	$payment_trans_no = $payment_trans_no === null ? 0 : (int)$payment_trans_no;
	if ($payslip_id <= 0 || $period_id <= 0 || $gl_trans_no <= 0
		|| (int)$payslip['status'] !== 2
		|| (isset($payslip['payment_trans_no']) && (int)$payslip['payment_trans_no'] > 0)
		|| ($payment_trans_no <= 0 && payslip_requires_payment($payslip))
		|| ($payment_trans_no > 0 && !payslip_requires_payment($payslip))
	) {
		return false;
	}

	$gl_column = payslip_has_column($table_name, 'gl_trans_no')
		? 'gl_trans_no' : (payslip_has_column($table_name, 'trans_no') ? 'trans_no' : '');
	if ($gl_column === '')
		return false;

	$sql = "UPDATE ".TB_PREF.$table_name." SET status = ".db_escape(payslip_paid_status_value())
		.", payment_trans_no = ".($payment_trans_no > 0 ? db_escape($payment_trans_no) : 'NULL')
		." WHERE payslip_id = ".db_escape($payslip_id)
		." AND payroll_period_id = ".db_escape($period_id)
		." AND status = ".db_escape(2)
		." AND (payment_trans_no IS NULL OR payment_trans_no = 0)"
		." AND ".$gl_column." = ".db_escape($gl_trans_no);
	$ok = db_query($sql, 'could not compare-and-set payroll batch payslip payment');
	return $ok && db_num_affected_rows() === 1;
}

/**
 * Re-read and verify the exact paid set before promoting the period.
 *
 * @param int $period_id
 * @param array $expected_paid_set
 * @return array
 */
function verify_payroll_period_payment_set_after_write($period_id, $expected_paid_set) {
	$result = array('valid' => false, 'code' => 'post_write_payment_set_drift');
	$locked = get_locked_payroll_period_payment_set($period_id);
	if ($locked === false || !is_array($expected_paid_set)
		|| count($locked['payslips']) !== count($expected_paid_set)
	) {
		return $result;
	}

	foreach ($locked['payslips'] as $payslip) {
		$payslip_id = isset($payslip['payslip_id']) ? (int)$payslip['payslip_id'] : 0;
		if ($payslip_id <= 0 || !isset($expected_paid_set[$payslip_id]))
			return $result;

		$expected = $expected_paid_set[$payslip_id];
		$current_payment_trans_no = isset($payslip['payment_trans_no'])
			? (int)$payslip['payment_trans_no'] : 0;
		if ((int)$payslip['payroll_period_id'] !== (int)$expected['payroll_period_id']
			|| payslip_is_voided($payslip)
			|| (int)$payslip['status'] !== payslip_paid_status_value()
			|| payroll_payment_payslip_gl_trans_no($payslip) !== (int)$expected['payroll_gl_trans_no']
			|| $current_payment_trans_no !== (int)$expected['payment_trans_no']
		) {
			return $result;
		}

		$posted_set = validate_payslip_approved_posted_set($payslip, false);
		if (empty($posted_set['valid']))
			return $result;
	}

	return array('valid' => true, 'code' => 'paid_set_verified');
}

/**
 * Compare-and-set the locked payroll period to Paid.
 *
 * @param int $period_id
 * @param int $expected_status
 * @param string $payment_date
 * @return bool
 */
function mark_payroll_period_paid_for_batch_compare_and_set($period_id, $expected_status, $payment_date) {
	$period_id = (int)$period_id;
	$expected_status = (int)$expected_status;
	if ($period_id <= 0 || !in_array($expected_status, array(2, 3), true))
		return false;

	$sql_date = date2sql(get_payroll_payment_date($payment_date));
	$sql = "UPDATE ".TB_PREF."payroll_periods SET status = ".db_escape(4)
		.", pay_date = ".db_escape($sql_date)
		." WHERE period_id = ".db_escape($period_id)
		." AND status = ".db_escape($expected_status);
	$ok = db_query($sql, 'could not compare-and-set payroll period payment details');
	if (!$ok || db_num_affected_rows() !== 1)
		return false;

	$verify = db_query(
		"SELECT period_id,status,pay_date FROM ".TB_PREF."payroll_periods"
			." WHERE period_id = ".db_escape($period_id)." FOR UPDATE",
		'could not verify payroll period payment details'
	);
	$row = $verify ? db_fetch_assoc($verify) : false;
	return $row
		&& (int)$row['period_id'] === $period_id
		&& (int)$row['status'] === 4
		&& (string)$row['pay_date'] === (string)$sql_date;
}

/**
 * Verify that a payslip belongs to an approved payable set whose legacy GL
 * posting exists and balances to zero.
 *
 * Legacy payroll currently posts GL during calculation. Until PAY-CORE-011
 * separates calculation from posting, status 2 (Approved) or 3 (Posted) plus
 * a balanced ST_PAYSLIP transaction is the minimum compatible proof that an
 * individual payment is not consuming an unapproved or unposted result.
 *
 * @param array $payslip
 * @param bool $audit_denial
 * @return array Array with valid, code, and message keys
 */
function validate_payslip_approved_posted_set($payslip, $audit_denial=true) {
	$result = array(
		'valid' => false,
		'code' => 'invalid_payslip',
		'message' => 'Payslip payment requires a linked approved payroll period with balanced payroll GL posting'
	);

	if (!is_array($payslip) || empty($payslip['payslip_id'])) {
		if ($audit_denial)
			payroll_payment_control_log_denial($result['code'], is_array($payslip) ? $payslip : array());
		return $result;
	}

	$period_id = isset($payslip['payroll_period_id']) ? (int)$payslip['payroll_period_id'] : 0;
	if ($period_id <= 0) {
		$result['code'] = 'payroll_period_missing';
		if ($audit_denial)
			payroll_payment_control_log_denial($result['code'], $payslip);
		return $result;
	}

	if (!function_exists('get_payroll_period')) {
		$result['code'] = 'payroll_period_repository_unavailable';
		if ($audit_denial)
			payroll_payment_control_log_denial($result['code'], $payslip);
		return $result;
	}

	$period = get_payroll_period($period_id);
	$period_status = $period && isset($period['status']) ? (int)$period['status'] : -1;
	if (!$period || !in_array($period_status, array(2, 3), true)) {
		$result['code'] = 'payroll_period_not_approved';
		if ($audit_denial)
			payroll_payment_control_log_denial($result['code'], $payslip);
		return $result;
	}

	$gl_trans_no = 0;
	if (isset($payslip['gl_trans_no']))
		$gl_trans_no = (int)$payslip['gl_trans_no'];
	elseif (isset($payslip['trans_no']))
		$gl_trans_no = (int)$payslip['trans_no'];
	if ($gl_trans_no <= 0) {
		$result['code'] = 'payroll_gl_link_missing';
		if ($audit_denial)
			payroll_payment_control_log_denial($result['code'], $payslip);
		return $result;
	}

	$sql = "SELECT amount FROM ".TB_PREF."gl_trans"
		." WHERE type = ".db_escape(ST_PAYSLIP)
		." AND type_no = ".db_escape($gl_trans_no)
		." FOR UPDATE";
	$query = db_query($sql, 'could not verify payroll payable-set GL posting');
	$line_count = 0;
	$balance = 0.0;
	$gross_amount = 0.0;
	if ($query) {
		while ($row = db_fetch_assoc($query)) {
			$amount = isset($row['amount']) ? (float)$row['amount'] : 0.0;
			$line_count++;
			$balance += $amount;
			$gross_amount += abs($amount);
		}
	}

	$voided_sql = "SELECT type, id FROM ".TB_PREF."voided"
		." WHERE type = ".db_escape(ST_PAYSLIP)
		." AND id = ".db_escape($gl_trans_no)
		." FOR UPDATE";
	$voided_query = db_query($voided_sql, 'could not verify payroll payable-set void status');
	$voided = $voided_query ? db_fetch_assoc($voided_query) : false;
	$tolerance = defined('FLOAT_COMP_DELTA') ? (float)FLOAT_COMP_DELTA : 0.00001;
	if ($voided) {
		$result['code'] = 'payroll_gl_posting_voided';
		if ($audit_denial)
			payroll_payment_control_log_denial($result['code'], $payslip);
		return $result;
	}
	if ($line_count < 2 || abs($gross_amount) <= $tolerance) {
		$result['code'] = $line_count < 2
			? 'payroll_gl_posting_missing'
			: 'payroll_gl_posting_empty';
		if ($audit_denial)
			payroll_payment_control_log_denial($result['code'], $payslip);
		return $result;
	}
	if (abs($balance) > $tolerance) {
		$result['code'] = 'payroll_gl_posting_unbalanced';
		if ($audit_denial)
			payroll_payment_control_log_denial($result['code'], $payslip);
		return $result;
	}

	return array('valid' => true, 'code' => 'approved_posted_set', 'message' => '');
}

/**
 * Validate payment prerequisites for a payslip
 *
 * @param array $payslip Payslip record
 * @param string $bank_account_code Optional bank account code
 * @return array Array with 'valid' => bool and 'message' => string
 */
function validate_payslip_payment($payslip, $bank_account_code='') {
	$result = array('valid' => true, 'message' => '');

	if (!is_array($payslip) || empty($payslip['payslip_id'])) {
		$result['valid'] = false;
		$result['message'] = 'Invalid payslip data';
		return $result;
	}

	// Check if already paid
	if (isset($payslip['status']) && payslip_is_paid($payslip)) {
		$result['valid'] = false;
		$result['message'] = 'Payslip is already marked as paid';
		return $result;
	}

	// Check if voided
	if (isset($payslip['status']) && payslip_is_voided($payslip)) {
		$result['valid'] = false;
		$result['message'] = 'Voided payslips cannot be marked as paid';
		return $result;
	}

	// Check if requires payment
	if (!payslip_requires_payment($payslip)) {
		$result['valid'] = false;
		$result['message'] = 'Only payslips with positive payable amount can be paid';
		return $result;
	}

	$payable_set = validate_payslip_approved_posted_set($payslip, true);
	if (!$payable_set['valid']) {
		$result['valid'] = false;
		$result['message'] = $payable_set['message'];
		$result['code'] = $payable_set['code'];
		return $result;
	}

	if ((int)$bank_account_code <= 0) {
		$result['valid'] = false;
		$result['message'] = 'A source bank account is required';
		return $result;
	}

	if (!get_payroll_payment_bank_account($bank_account_code)) {
		$result['valid'] = false;
		$result['message'] = 'Invalid bank account specified';
		return $result;
	}

	if (get_payroll_payment_payable_account() === false) {
		$result['valid'] = false;
		$result['message'] = 'Payroll payable account is not configured in HR Settings';
		return $result;
	}

	return $result;
}

/**
 * Reverse a payslip payment (undo mark as paid)
 *
 * @param int $payslip_id
 * @return bool
 */
function reverse_payslip_payment($payslip_id) {
	$payslip_id = (int)$payslip_id;
	if ($payslip_id <= 0)
		return false;

	$payslip = get_payslip($payslip_id);
	if (!$payslip || empty($payslip['payment_trans_no']))
		return false;

	begin_transaction();
	$void_message = false;
	if (function_exists('void_transaction'))
		$void_message = void_transaction(ST_BANKPAYMENT, (int)$payslip['payment_trans_no'], Today(), _('Payroll payment reversed.'));
	if ($void_message) {
		cancel_transaction();
		return false;
	}

	$data = array(
		'status' => 2,
		'payment_trans_no' => null
	);
	if (!update_payslip($payslip_id, $data)) {
		cancel_transaction();
		return false;
	}

	if (!empty($payslip['payroll_period_id']) && function_exists('sync_payroll_period_payment_status')
		&& !sync_payroll_period_payment_status((int)$payslip['payroll_period_id'])) {
		cancel_transaction();
		return false;
	}

	commit_transaction();
	return true;
}
?>
