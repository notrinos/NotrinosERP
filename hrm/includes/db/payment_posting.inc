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
 * Fetch a valid source bank account row.
 *
 * @param mixed $bank_account_id
 * @return array|false
 */
function get_payroll_payment_bank_account($bank_account_id) {
	$bank_account_id = (int)$bank_account_id;
	if ($bank_account_id <= 0)
		return false;

	$sql = "SELECT id, account_code, bank_curr_code FROM ".TB_PREF."bank_accounts"
		." WHERE id = ".db_escape($bank_account_id);
	$result = db_query($sql, 'could not fetch payroll payment bank account');
	$row = db_fetch($result);

	return $row ? $row : false;
}

/**
 * Resolve the payroll payable GL account configured in HR settings.
 *
 * @return string|false
 */
function get_payroll_payment_payable_account() {
	$payable_account = trim((string)get_company_pref('payroll_payable_act'));
	if ($payable_account === '')
		return false;
	if (class_exists('chart_master_entity') && chart_master_entity::find($payable_account) === null)
		return false;

	return $payable_account;
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
	$validation = validate_payslip_payment($payslip, $bank_account_code);
	if (!$validation['valid'])
		return false;

	$bank_account = get_payroll_payment_bank_account($bank_account_code);
	if (!$bank_account)
		return false;

	$payment_date = get_payroll_payment_date($payment_date);
	if ($use_transaction)
		begin_transaction();
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

	$period = get_payroll_period($period_id);
	if (!$period)
		return false;

	$current_status = isset($period['status']) ? (int)$period['status'] : 0;
	if (!in_array($current_status, array(2, 3), true))
		return false;

	$bank_account = get_payroll_payment_bank_account($bank_account_code);
	if (!$bank_account)
		return false;

	$payment_date = get_payroll_payment_date($payment_date);
	$payslip_rows = get_payslips_for_period($period_id);
	if (!$payslip_rows)
		return false;

	$transactions = array();
	if ($use_transaction)
		begin_transaction();

	while ($payslip = db_fetch_assoc($payslip_rows)) {
		if (!payslip_requires_payment($payslip) || payslip_is_voided($payslip) || payslip_is_paid($payslip))
			continue;

		$payment_reference = '';
		if ($reference !== '' && empty($transactions))
			$payment_reference = $reference;

		$trans_no = write_payslip_payment_transaction($payslip, $bank_account['id'], $payment_date, $payment_reference, $memo, false);
		if (!$trans_no || !mark_payslip_paid($payslip['payslip_id'], $trans_no, false)) {
			if ($use_transaction)
				cancel_transaction();
			return false;
		}

		$transactions[(int)$payslip['payslip_id']] = (int)$trans_no;
	}

	$set = array(
		"status = ".db_escape(4),
		"pay_date = ".db_escape(date2sql($payment_date))
	);
	if (!db_query("UPDATE ".TB_PREF."payroll_periods SET ".implode(', ', $set)." WHERE period_id = ".db_escape($period_id), 'could not update payroll period payment details')) {
		if ($use_transaction)
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
