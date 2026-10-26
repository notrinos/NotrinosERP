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

/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	Transaction Number Reservation

	Handles reserving, releasing, and voiding transaction numbers
	for the approval draft system. Uses the existing voided table to
	prevent number conflicts with normal transaction flow.
***********************************************************************/

/**
 * Reserve the next transaction number for an approval draft.
 *
 * Atomically gets the next number from the transaction's normal sequence
 * and inserts a placeholder in the voided table to prevent any other
 * transaction from using the same number.
 *
 * @param int   $trans_type Transaction type constant (ST_*)
 * @param array $context    Context for reference generation (e.g., date, customer)
 * @return array ['trans_no' => int, 'reference' => string]
 */
function reserve_approval_trans_number($trans_type, $context = array())
{
	global $Refs;

	// Ensure refline placeholders like {MM}/{YY}/{YYYY} always have a date.
	// _parse_next() only auto-fills date when context is null/non-array.
	if (!is_array($context))
		$context = array();
	if (!isset($context['date']) || !$context['date'])
		$context['date'] = new_doc_date();

	$native_reference = null;
	if (defined('ST_PURCHREQUISITION') && (int)$trans_type === ST_PURCHREQUISITION && isset($context['requisition_id'])) {
		// This approval transitions an already numbered native source. Its
		// reference is current locked custody, not a second document refline.
		$identity = $context['requisition_id'];
		if ((!is_int($identity) && !is_string($identity)) || (int)$identity <= 0
			|| (string)(int)$identity !== (string)$identity || empty($GLOBALS['transaction_level'])) return false;
		$result = db_query('SELECT reference FROM '.TB_PREF.'purch_requisitions WHERE id='.(int)$identity
			." AND status='submitted' FOR UPDATE", 'could not verify requisition approval reference');
		$row = $result ? db_fetch_assoc($result) : false;
		if (!$row || !is_string($row['reference']) || trim($row['reference']) === ''
			|| strlen($row['reference']) > 60 || strpos($row['reference'], "\0") !== false) return false;
		$native_reference = $row['reference'];
	}

	// PO native keys auto-increment independently of retained approval numbers.
	// A caller's consistent snapshot must not reserve an already committed key.
	$next_no = (int)$trans_type === ST_PURCHORDER
		? get_next_purchase_order_approval_number() : get_next_trans_no($trans_type);
	if (!$next_no)
		return false;

	// Insert placeholder in voided table to consume the number.
	// get_next_trans_no() checks voided table, so this prevents reuse.
	$sql = "INSERT INTO " . TB_PREF . "voided (type, id, date_, memo_)
		VALUES ("
		. db_escape($trans_type) . ", "
		. db_escape($next_no) . ", "
		. "NOW(), "
		. db_escape('APPROVAL_RESERVED') . ")";

	if (!db_query($sql, 'could not reserve transaction number for approval'))
		return false;

	// Generate the reference string for this reserved number
	$reference = '';
	if ($native_reference !== null) {
		$reference = $native_reference;
	} elseif (is_object($Refs)) {
		$reference = $Refs->get_next($trans_type, null, $context);
	}

	return array('trans_no' => $next_no, 'reference' => $reference);
}

/**
 * Serialize PO reservations against native inserts using current InnoDB reads.
 * Hold both sequence ranges until the existing outer workflow transaction ends.
 * Native auto-increment and historical reservation/posted aliases stay unchanged.
 * @return int|false Next current company PO number, or unavailable ownership
 */
function get_next_purchase_order_approval_number()
{
	if (empty($GLOBALS['transaction_level']))
		return false;
	$last_no = 0;
	foreach (array(
		'SELECT order_no FROM '.TB_PREF.'purch_orders ORDER BY order_no DESC LIMIT 1 FOR UPDATE',
		'SELECT id FROM '.TB_PREF.'voided WHERE type='.ST_PURCHORDER.' ORDER BY id DESC LIMIT 1 FOR UPDATE'
	) as $sql) {
		$result = db_query($sql, 'could not verify current purchase order approval number');
		if (!$result)
			return false;
		$row = db_fetch_row($result);
		if ($row) {
			if (!is_numeric($row[0]) || (float)$row[0] < 0 || (float)$row[0] >= 2147483647)
				return false;
			$last_no = max($last_no, (int)$row[0]);
		}
	}
	return $last_no + 1;
}

/**
 * Release a reserved number when a draft is approved and executed.
 *
 * Removes the APPROVAL_RESERVED placeholder from the voided table,
 * allowing the transaction to be posted normally with that number.
 *
 * @param int $trans_type Transaction type constant
 * @param int $trans_no   Reserved transaction number
 * @return void
 */
function release_reserved_trans_number($trans_type, $trans_no)
{
	$sql = "DELETE FROM " . TB_PREF . "voided
		WHERE type = " . db_escape($trans_type) . "
		AND id = " . db_escape($trans_no) . "
		AND memo_ = 'APPROVAL_RESERVED'";

	db_query($sql, 'could not release reserved transaction number');
}

/**
 * Permanently void a reserved number when a draft is rejected, cancelled, or expired.
 *
 * Updates the voided record memo to indicate the reason, permanently
 * consuming the number so it is never reused.
 *
 * @param int    $trans_type Transaction type constant
 * @param int    $trans_no   Reserved transaction number
 * @param string $reason     Reason: 'Rejected', 'Cancelled', or 'Expired'
 * @return void
 */
function void_reserved_trans_number($trans_type, $trans_no, $reason)
{
	$memo = 'APPROVAL_' . strtoupper($reason);

	$sql = "UPDATE " . TB_PREF . "voided
		SET memo_ = " . db_escape($memo) . "
		WHERE type = " . db_escape($trans_type) . "
		AND id = " . db_escape($trans_no) . "
		AND memo_ = 'APPROVAL_RESERVED'";

	db_query($sql, 'could not void reserved transaction number');
}

/**
 * Check if a transaction number is reserved for an approval draft.
 *
 * @param int $trans_type Transaction type constant
 * @param int $trans_no   Transaction number to check
 * @return bool           True if number is reserved
 */
function is_trans_number_reserved($trans_type, $trans_no)
{
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "voided
		WHERE type = " . db_escape($trans_type) . "
		AND id = " . db_escape($trans_no) . "
		AND memo_ = 'APPROVAL_RESERVED'";

	$result = db_query($sql, 'could not check reserved transaction number');
	$row = db_fetch_row($result);

	return $row[0] > 0;
}

/**
 * Check if a transaction number was voided by the approval system
 * (rejected, cancelled, or expired draft).
 *
 * @param int $trans_type Transaction type constant
 * @param int $trans_no   Transaction number to check
 * @return string|false   Reason string (e.g., 'APPROVAL_REJECTED') or false
 */
function get_approval_void_reason($trans_type, $trans_no)
{
	$sql = "SELECT memo_ FROM " . TB_PREF . "voided
		WHERE type = " . db_escape($trans_type) . "
		AND id = " . db_escape($trans_no) . "
		AND memo_ LIKE 'APPROVAL_%'
		AND memo_ != 'APPROVAL_RESERVED'";

	$result = db_query($sql, 'could not check approval void reason');
	$row = db_fetch($result);

	return $row ? $row['memo_'] : false;
}
