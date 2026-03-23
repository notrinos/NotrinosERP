<?php
/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	GL/Banking Module Registration & Executors

	Registers GL Journal Entry, Bank Payment, and Bank Deposit transaction
	types for the approval workflow. Provides executor callbacks that
	reconstruct and post approved drafts.

	PHP 5.6+ compatible.
***********************************************************************/

include_once($path_to_root . '/includes/approval/ui/approval_ui.inc');

// =====================================================
// TRANSACTION TYPE REGISTRATIONS
// =====================================================

ApprovalWorkflow::registerTransactionType(ST_JOURNAL, array(
	'label'          => _('Journal Entry'),
	'application'    => 'GL',
	'executor'       => 'execute_approved_journal_entry',
	'amount_getter'  => 'get_journal_draft_amount',
	'draft_renderer' => 'render_journal_draft',
	'edit_url'       => 'gl/gl_journal.php',
));

ApprovalWorkflow::registerTransactionType(ST_BANKPAYMENT, array(
	'label'          => _('Bank Payment'),
	'application'    => 'GL',
	'executor'       => 'execute_approved_bank_payment',
	'amount_getter'  => 'get_bank_draft_amount',
	'draft_renderer' => 'render_bank_draft',
	'edit_url'       => 'gl/gl_bank.php',
));

ApprovalWorkflow::registerTransactionType(ST_BANKDEPOSIT, array(
	'label'          => _('Bank Deposit'),
	'application'    => 'GL',
	'executor'       => 'execute_approved_bank_deposit',
	'amount_getter'  => 'get_bank_draft_amount',
	'draft_renderer' => 'render_bank_draft',
	'edit_url'       => 'gl/gl_bank.php',
));

// =====================================================
// EXECUTOR CALLBACKS
// =====================================================

/**
 * Execute an approved Journal Entry draft.
 *
 * Reconstructs the items_cart from saved draft data and calls
 * write_journal_entries() to post the actual transaction.
 *
 * @param array $draft_data  Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id    Approval draft ID
 * @return int|false Posted transaction number or false on failure
 */
function execute_approved_journal_entry($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;

	include_once($path_to_root . '/includes/ui/items_cart.inc');
	include_once($path_to_root . '/gl/includes/gl_db.inc');

	$cart = restore_items_cart_from_data($draft_data);

	// Set reference from draft data
	$cart->reference = $draft_data['reference'];
	$cart->tran_date = $draft_data['tran_date'];
	$cart->doc_date  = isset($draft_data['doc_date']) ? $draft_data['doc_date'] : $draft_data['tran_date'];
	$cart->event_date = isset($draft_data['event_date']) ? $draft_data['event_date'] : '';
	$cart->source_ref = isset($draft_data['source_ref']) ? $draft_data['source_ref'] : '';
	$cart->memo_     = isset($draft_data['memo_']) ? $draft_data['memo_'] : '';
	$cart->currency  = isset($draft_data['currency']) ? $draft_data['currency'] : '';
	$cart->rate      = isset($draft_data['rate']) ? $draft_data['rate'] : 1;

	// Re-apply tax_info if present
	if (isset($draft_data['tax_info']) && $draft_data['tax_info']) {
		$cart->tax_info = $draft_data['tax_info'];
	}

	$trans_no = write_journal_entries($cart);

	return $trans_no;
}

/**
 * Execute an approved Bank Payment draft.
 *
 * @param array $draft_data  Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id    Approval draft ID
 * @return int|false Posted transaction number or false on failure
 */
function execute_approved_bank_payment($draft_data, $reserved_trans_no, $draft_id)
{
	return execute_approved_bank_transaction($draft_data, $reserved_trans_no, $draft_id);
}

/**
 * Execute an approved Bank Deposit draft.
 *
 * @param array $draft_data  Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id    Approval draft ID
 * @return int|false Posted transaction number or false on failure
 */
function execute_approved_bank_deposit($draft_data, $reserved_trans_no, $draft_id)
{
	return execute_approved_bank_transaction($draft_data, $reserved_trans_no, $draft_id);
}

/**
 * Shared executor for Bank Payment and Bank Deposit.
 *
 * @param array $draft_data  Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id    Approval draft ID
 * @return int|false Posted transaction number or false on failure
 */
function execute_approved_bank_transaction($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;

	include_once($path_to_root . '/includes/ui/items_cart.inc');
	include_once($path_to_root . '/gl/includes/gl_db.inc');

	$cart = restore_items_cart_from_data($draft_data);

	// Restore bank-specific fields
	$trans_type      = (int)$draft_data['trans_type'];
	$bank_account    = $draft_data['bank_account'];
	$date_           = $draft_data['tran_date'];
	$pay_type        = isset($draft_data['pay_type']) ? (int)$draft_data['pay_type'] : 0;
	$person_id       = isset($draft_data['person_id']) ? $draft_data['person_id'] : '';
	$person_detail_id = isset($draft_data['person_detail_id']) ? $draft_data['person_detail_id'] : '';
	$ref             = $draft_data['reference'];
	$memo_           = isset($draft_data['memo_']) ? $draft_data['memo_'] : '';
	$settled_amount  = isset($draft_data['settled_amount']) ? $draft_data['settled_amount'] : null;

	$trans = write_bank_transaction(
		$trans_type, 0, $bank_account,
		$cart, $date_,
		$pay_type, $person_id, $person_detail_id,
		$ref, $memo_, true, $settled_amount
	);

	if (is_array($trans)) {
		return (int)$trans[1]; // $trans = array(trans_type, trans_no)
	}

	return false;
}

// =====================================================
// AMOUNT GETTERS
// =====================================================

/**
 * Calculate the total amount of a journal entry draft.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return float Total debit amount
 */
function get_journal_draft_amount($draft_data)
{
	$total = 0;
	if (!empty($draft_data['gl_items'])) {
		foreach ($draft_data['gl_items'] as $gl) {
			if ($gl['amount'] > 0)
				$total += $gl['amount'];
		}
	}
	return $total;
}

/**
 * Calculate the total amount of a bank transaction draft.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return float Total amount
 */
function get_bank_draft_amount($draft_data)
{
	$total = 0;
	if (!empty($draft_data['gl_items'])) {
		foreach ($draft_data['gl_items'] as $gl) {
			$total += abs($gl['amount']);
		}
	}
	return $total;
}

// =====================================================
// DRAFT RENDERERS
// =====================================================

/**
 * Render a journal entry draft for the approval review UI.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return string HTML representation
 */
function render_journal_draft($draft_data)
{
	$html = '<table class="tablestyle" width="100%">';
	$html .= '<tr><th>' . _('Date') . '</th><td>' . htmlspecialchars($draft_data['tran_date']) . '</td>';
	$html .= '<th>' . _('Reference') . '</th><td>' . htmlspecialchars($draft_data['reference']) . '</td></tr>';

	if (!empty($draft_data['source_ref']))
		$html .= '<tr><th>' . _('Source Ref') . '</th><td colspan="3">' . htmlspecialchars($draft_data['source_ref']) . '</td></tr>';

	if (!empty($draft_data['memo_']))
		$html .= '<tr><th>' . _('Memo') . '</th><td colspan="3">' . htmlspecialchars($draft_data['memo_']) . '</td></tr>';

	$html .= '</table>';

	// GL Items table
	if (!empty($draft_data['gl_items'])) {
		$html .= '<br><table class="tablestyle" width="100%">';
		$html .= '<tr><th>' . _('Account') . '</th><th>' . _('Description') . '</th>';
		$html .= '<th>' . _('Debit') . '</th><th>' . _('Credit') . '</th></tr>';

		$total_debit = 0;
		$total_credit = 0;
		foreach ($draft_data['gl_items'] as $gl) {
			$debit = $gl['amount'] > 0 ? number_format($gl['amount'], 2) : '';
			$credit = $gl['amount'] < 0 ? number_format(abs($gl['amount']), 2) : '';
			if ($gl['amount'] > 0) $total_debit += $gl['amount'];
			if ($gl['amount'] < 0) $total_credit += abs($gl['amount']);

			$html .= '<tr><td>' . htmlspecialchars($gl['code_id']) . '</td>';
			$html .= '<td>' . htmlspecialchars(isset($gl['description']) ? $gl['description'] : '') . '</td>';
			$html .= '<td align="right">' . $debit . '</td>';
			$html .= '<td align="right">' . $credit . '</td></tr>';
		}

		$html .= '<tr><td colspan="2"><b>' . _('Total') . '</b></td>';
		$html .= '<td align="right"><b>' . number_format($total_debit, 2) . '</b></td>';
		$html .= '<td align="right"><b>' . number_format($total_credit, 2) . '</b></td></tr>';
		$html .= '</table>';
	}

	return $html;
}

/**
 * Render a bank transaction draft for the approval review UI.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return string HTML representation
 */
function render_bank_draft($draft_data)
{
	$type_label = ((int)$draft_data['trans_type'] === ST_BANKPAYMENT) ? _('Bank Payment') : _('Bank Deposit');

	$html = '<table class="tablestyle" width="100%">';
	$html .= '<tr><th>' . _('Type') . '</th><td>' . $type_label . '</td>';
	$html .= '<th>' . _('Date') . '</th><td>' . htmlspecialchars($draft_data['tran_date']) . '</td></tr>';
	$html .= '<tr><th>' . _('Reference') . '</th><td>' . htmlspecialchars($draft_data['reference']) . '</td>';
	$html .= '<th>' . _('Bank Account') . '</th><td>' . htmlspecialchars(isset($draft_data['bank_account']) ? $draft_data['bank_account'] : '') . '</td></tr>';

	if (!empty($draft_data['memo_']))
		$html .= '<tr><th>' . _('Memo') . '</th><td colspan="3">' . htmlspecialchars($draft_data['memo_']) . '</td></tr>';

	$html .= '</table>';

	// GL Items
	if (!empty($draft_data['gl_items'])) {
		$html .= '<br><table class="tablestyle" width="100%">';
		$html .= '<tr><th>' . _('Account') . '</th><th>' . _('Description') . '</th><th>' . _('Amount') . '</th></tr>';

		$total = 0;
		foreach ($draft_data['gl_items'] as $gl) {
			$total += $gl['amount'];
			$html .= '<tr><td>' . htmlspecialchars($gl['code_id']) . '</td>';
			$html .= '<td>' . htmlspecialchars(isset($gl['description']) ? $gl['description'] : '') . '</td>';
			$html .= '<td align="right">' . number_format($gl['amount'], 2) . '</td></tr>';
		}

		$html .= '<tr><td colspan="2"><b>' . _('Total') . '</b></td>';
		$html .= '<td align="right"><b>' . number_format($total, 2) . '</b></td></tr>';
		$html .= '</table>';
	}

	return $html;
}
