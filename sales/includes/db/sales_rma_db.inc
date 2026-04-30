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
 * Sales RMA (Returns Management) — Database Functions
 *
 * Implements the customer-facing RMA authorization layer that delegates
 * physical goods processing to the existing WH Return Order system
 * (ST_WH_RETURN_ORDER = 53).
 *
 * RMA lifecycle:
 *   pending → authorized → wh_processing → refunded/closed
 *             ↘ rejected
 *
 * @package NotrinosERP
 * @subpackage Sales
 */

// ============================================================================
// RMA STATUS / REASON HELPERS
// ============================================================================

/**
 * Return all valid RMA status codes with translated labels.
 *
 * @return array status_code => label
 */
function get_rma_statuses()
{
	return array(
		'pending'       => _('Pending'),
		'authorized'    => _('Authorized'),
		'wh_processing' => _('WH Processing'),
		'refunded'      => _('Refunded'),
		'rejected'      => _('Rejected'),
		'closed'        => _('Closed'),
	);
}

/**
 * Return valid RMA return methods with translated labels.
 *
 * @return array method_code => label
 */
function get_rma_return_methods()
{
	return array(
		'credit_note'  => _('Credit Note'),
		'refund'       => _('Refund'),
		'replacement'  => _('Replacement'),
		'repair'       => _('Repair'),
	);
}

/**
 * Return CSS colour for an RMA status badge.
 *
 * @param string $status
 * @return string Hex colour
 */
function get_rma_status_color($status)
{
	$colors = array(
		'pending'       => '#fd7e14',
		'authorized'    => '#007bff',
		'wh_processing' => '#17a2b8',
		'refunded'      => '#28a745',
		'rejected'      => '#dc3545',
		'closed'        => '#6c757d',
	);
	return isset($colors[$status]) ? $colors[$status] : '#6c757d';
}

/**
 * Fetch all active return reasons.
 *
 * @param bool $include_inactive
 * @return resource DB result set
 */
function get_rma_reasons($include_inactive = false)
{
	$sql = "SELECT * FROM " . TB_PREF . "sales_rma_reasons";
	if (!$include_inactive)
		$sql .= " WHERE inactive = 0";
	$sql .= " ORDER BY description";
	return db_query($sql, 'Could not get RMA reasons');
}

/**
 * Get a single return reason by ID.
 *
 * @param int $id
 * @return array|false
 */
function get_rma_reason($id)
{
	$sql = "SELECT * FROM " . TB_PREF . "sales_rma_reasons WHERE id = " . (int)$id;
	$result = db_query($sql, 'Could not get RMA reason');
	return db_fetch($result);
}

/**
 * Add a return reason.
 *
 * @param string $description
 * @param int    $requires_inspection
 * @param string $default_disposition
 * @return int  New reason ID
 */
function add_rma_reason($description, $requires_inspection, $default_disposition)
{
	$sql = "INSERT INTO " . TB_PREF . "sales_rma_reasons (description, requires_inspection, default_disposition)
		VALUES (" . db_escape($description) . ", " . (int)$requires_inspection . ", " . db_escape($default_disposition) . ")";
	db_query($sql, 'Could not add RMA reason');
	return db_insert_id();
}

/**
 * Update a return reason.
 *
 * @param int    $id
 * @param string $description
 * @param int    $requires_inspection
 * @param string $default_disposition
 * @param int    $inactive
 * @return void
 */
function update_rma_reason($id, $description, $requires_inspection, $default_disposition, $inactive = 0)
{
	$sql = "UPDATE " . TB_PREF . "sales_rma_reasons SET
		description = " . db_escape($description) . ",
		requires_inspection = " . (int)$requires_inspection . ",
		default_disposition = " . db_escape($default_disposition) . ",
		inactive = " . (int)$inactive . "
		WHERE id = " . (int)$id;
	db_query($sql, 'Could not update RMA reason');
}

// ============================================================================
// RMA HEADER CRUD
// ============================================================================

/**
 * Ensure ST_SALESRETURN is registered for reference/transaction numbering.
 *
 * @return void
 */
function ensure_sales_rma_transaction_registration()
{
	static $is_registered = false;

	if ($is_registered || !function_exists('register_systype_db_info'))
		return;

	register_systype_db_info(ST_SALESRETURN, array(
		TB_PREF . 'sales_rma',
		null,
		'id',
		'reference',
		'request_date'
	));

	$is_registered = true;
}

/**
 * Create a new RMA header.
 *
 * @param int    $debtor_no         Customer ID
 * @param int    $branch_code       Branch code
 * @param string $request_date      Request date (display format)
 * @param int    $source_type       Source transaction type (ST_ constant)
 * @param int    $source_no         Source transaction number
 * @param int    $return_reason_id  Reason FK
 * @param string $return_method     Return method code
 * @param string $customer_notes    Customer-visible notes
 * @param string $internal_notes    Internal notes
 * @param float  $restocking_fee_pct Restocking fee percentage
 * @return int  New RMA ID
 */
function add_sales_rma($debtor_no, $branch_code, $request_date, $source_type, $source_no,
	$return_reason_id, $return_method = 'credit_note',
	$customer_notes = '', $internal_notes = '', $restocking_fee_pct = 0)
{
	global $SysPrefs;
	hook_invoke_all('sales_rma_before_save', $debtor_no, $source_type, $source_no);
	ensure_sales_rma_transaction_registration();

	$reference = get_next_trans_no(ST_SALESRETURN);

	$sql = "INSERT INTO " . TB_PREF . "sales_rma
		(reference, debtor_no, branch_code, request_date, status,
		 source_type, source_no, return_reason_id, return_method,
		 customer_notes, internal_notes, restocking_fee_percent, created_by)
		VALUES ("
		. db_escape($reference) . ", "
		. (int)$debtor_no . ", "
		. (int)$branch_code . ", "
		. "'" . date2sql($request_date) . "', "
		. "'pending', "
		. (int)$source_type . ", "
		. (int)$source_no . ", "
		. (int)$return_reason_id . ", "
		. db_escape($return_method) . ", "
		. db_escape($customer_notes) . ", "
		. db_escape($internal_notes) . ", "
		. (float)$restocking_fee_pct . ", "
		. (int)$_SESSION['wa_current_user']->user
		. ")";
	db_query($sql, 'Could not create RMA');
	return db_insert_id();
}

/**
 * Update an existing RMA header (only while status = 'pending').
 *
 * @param int    $rma_id
 * @param int    $return_reason_id
 * @param string $return_method
 * @param string $customer_notes
 * @param string $internal_notes
 * @param float  $restocking_fee_pct
 * @return void
 */
function update_sales_rma($rma_id, $return_reason_id, $return_method,
	$customer_notes, $internal_notes, $restocking_fee_pct)
{
	$sql = "UPDATE " . TB_PREF . "sales_rma SET
		return_reason_id = " . (int)$return_reason_id . ",
		return_method = " . db_escape($return_method) . ",
		customer_notes = " . db_escape($customer_notes) . ",
		internal_notes = " . db_escape($internal_notes) . ",
		restocking_fee_percent = " . (float)$restocking_fee_pct . "
		WHERE id = " . (int)$rma_id . " AND status = 'pending'";
	db_query($sql, 'Could not update RMA');
}

/**
 * Fetch a single RMA header by ID (joined with customer name and reason).
 *
 * @param int $rma_id
 * @return array|false
 */
function get_sales_rma($rma_id)
{
	$sql = "SELECT r.*,
		d.name AS customer_name,
		b.br_name AS branch_name,
		rs.description AS reason_description
		FROM " . TB_PREF . "sales_rma r
		LEFT JOIN " . TB_PREF . "debtors_master d ON d.debtor_no = r.debtor_no
		LEFT JOIN " . TB_PREF . "cust_branch b ON b.branch_code = r.branch_code AND b.debtor_no = r.debtor_no
		LEFT JOIN " . TB_PREF . "sales_rma_reasons rs ON rs.id = r.return_reason_id
		WHERE r.id = " . (int)$rma_id;
	$result = db_query($sql, 'Could not get RMA');
	return db_fetch($result);
}

/**
 * Fetch RMAs with optional filters.
 *
 * @param int    $debtor_no   0 = all customers
 * @param string $status      '' = all statuses
 * @param string $date_from   SQL format date or ''
 * @param string $date_to     SQL format date or ''
 * @return resource DB result
 */
function get_sales_rmas($debtor_no = 0, $status = '', $date_from = '', $date_to = '')
{
	$sql = "SELECT r.*,
		d.name AS customer_name,
		rs.description AS reason_description
		FROM " . TB_PREF . "sales_rma r
		LEFT JOIN " . TB_PREF . "debtors_master d ON d.debtor_no = r.debtor_no
		LEFT JOIN " . TB_PREF . "sales_rma_reasons rs ON rs.id = r.return_reason_id
		WHERE 1=1";

	if ($debtor_no)
		$sql .= " AND r.debtor_no = " . (int)$debtor_no;
	if ($status !== '')
		$sql .= " AND r.status = " . db_escape($status);
	if ($date_from !== '')
		$sql .= " AND r.request_date >= '" . $date_from . "'";
	if ($date_to !== '')
		$sql .= " AND r.request_date <= '" . $date_to . "'";

	$sql .= " ORDER BY r.id DESC";
	return db_query($sql, 'Could not get RMAs');
}

// ============================================================================
// RMA LINE CRUD
// ============================================================================

/**
 * Add a line item to an RMA.
 *
 * @param int    $rma_id
 * @param string $stock_id
 * @param string $description
 * @param float  $quantity_requested
 * @param float  $unit_price
 * @param string $return_condition  new|good|damaged|defective
 * @param string $serial_number
 * @param string $batch_number
 * @param string $notes
 * @return int  New line ID
 */
function add_rma_line($rma_id, $stock_id, $description, $quantity_requested,
	$unit_price, $return_condition = 'good',
	$serial_number = '', $batch_number = '', $notes = '')
{
	$sql = "INSERT INTO " . TB_PREF . "sales_rma_lines
		(rma_id, stock_id, description, quantity_requested, quantity_authorized,
		 unit_price, return_condition, serial_number, batch_number, notes)
		VALUES ("
		. (int)$rma_id . ", "
		. db_escape($stock_id) . ", "
		. db_escape($description) . ", "
		. (float)$quantity_requested . ", "
		. (float)$quantity_requested . ", "  // pre-fill authorized = requested
		. (float)$unit_price . ", "
		. db_escape($return_condition) . ", "
		. db_escape($serial_number) . ", "
		. db_escape($batch_number) . ", "
		. db_escape($notes)
		. ")";
	db_query($sql, 'Could not add RMA line');
	$line_id = db_insert_id();
	_recalc_rma_totals($rma_id);
	return $line_id;
}

/**
 * Update an existing RMA line.
 *
 * @param int    $line_id
 * @param float  $quantity_requested
 * @param float  $quantity_authorized
 * @param string $return_condition
 * @param string $notes
 * @return void
 */
function update_rma_line($line_id, $quantity_requested, $quantity_authorized,
	$return_condition, $notes)
{
	$sql = "UPDATE " . TB_PREF . "sales_rma_lines SET
		quantity_requested = " . (float)$quantity_requested . ",
		quantity_authorized = " . (float)$quantity_authorized . ",
		return_condition = " . db_escape($return_condition) . ",
		notes = " . db_escape($notes) . "
		WHERE id = " . (int)$line_id;
	db_query($sql, 'Could not update RMA line');

	// Recalculate totals on the parent RMA
	$row = db_fetch(db_query("SELECT rma_id FROM " . TB_PREF . "sales_rma_lines WHERE id = " . (int)$line_id, 'get rma_id'));
	if ($row)
		_recalc_rma_totals($row['rma_id']);
}

/**
 * Delete an RMA line.
 *
 * @param int $line_id
 * @return void
 */
function delete_rma_line($line_id)
{
	$row = db_fetch(db_query("SELECT rma_id FROM " . TB_PREF . "sales_rma_lines WHERE id = " . (int)$line_id, 'get rma_id'));
	db_query("DELETE FROM " . TB_PREF . "sales_rma_lines WHERE id = " . (int)$line_id, 'Could not delete RMA line');
	if ($row)
		_recalc_rma_totals($row['rma_id']);
}

/**
 * Fetch all lines for an RMA.
 *
 * @param int $rma_id
 * @return resource DB result
 */
function get_rma_lines($rma_id)
{
	$sql = "SELECT l.*, m.description AS item_name, m.units AS unit
		FROM " . TB_PREF . "sales_rma_lines l
		LEFT JOIN " . TB_PREF . "stock_master m
			ON m.stock_id COLLATE utf8_general_ci = l.stock_id COLLATE utf8_general_ci
		WHERE l.rma_id = " . (int)$rma_id . "
		ORDER BY l.id";
	return db_query($sql, 'Could not get RMA lines');
}

/**
 * Recalculate and persist total_amount and refund_amount on an RMA.
 *
 * @param int $rma_id
 * @return void
 */
function _recalc_rma_totals($rma_id)
{
	$sql = "SELECT SUM(quantity_authorized * unit_price) AS total
		FROM " . TB_PREF . "sales_rma_lines WHERE rma_id = " . (int)$rma_id;
	$row = db_fetch(db_query($sql, 'recalc totals'));
	$total = $row ? (float)$row['total'] : 0;

	$rma = get_sales_rma($rma_id);
	$fee_pct = $rma ? (float)$rma['restocking_fee_percent'] : 0;
	$fee_amount = round($total * $fee_pct / 100, 2);
	$refund_amount = round($total - $fee_amount, 2);

	$sql = "UPDATE " . TB_PREF . "sales_rma SET
		total_amount = " . $total . ",
		restocking_fee_amount = " . $fee_amount . ",
		refund_amount = " . $refund_amount . "
		WHERE id = " . (int)$rma_id;
	db_query($sql, 'Could not recalculate RMA totals');
}

// ============================================================================
// LIFECYCLE ACTIONS
// ============================================================================

/**
 * Fetch customer and branch defaults for downstream RMA documents.
 *
 * When the RMA was created without an explicit branch, the first active
 * customer branch is used as the downstream document context.
 *
 * @param int $debtor_no
 * @param int $branch_code
 * @return array
 */
function get_sales_rma_document_defaults($debtor_no, $branch_code)
{
	global $path_to_root;

	include_once($path_to_root . '/sales/includes/db/customers_db.inc');
	include_once($path_to_root . '/sales/includes/db/branches_db.inc');

	$customer_row = get_customer($debtor_no);
	$branch = false;

	if ($branch_code > 0)
		$branch = get_cust_branch($debtor_no, $branch_code);

	if (!$branch) {
		$sql = "SELECT * FROM " . TB_PREF . "cust_branch
			WHERE debtor_no = " . (int)$debtor_no . "
			ORDER BY branch_code
			LIMIT 1";
		$branch = db_fetch(db_query($sql, 'Could not get default customer branch for RMA'));
	}

	return array(
		'customer' => $customer_row,
		'branch' => $branch,
	);
}

/**
 * Authorize an RMA (pending → authorized).
 *
 * @param int    $rma_id
 * @param string $notes  Optional internal note
 * @return bool  True on success
 */
function authorize_rma($rma_id, $notes = '')
{
	$auth_notes_sql = '';
	if ($notes !== '')
		$auth_notes_sql = ", internal_notes = CONCAT(IFNULL(internal_notes,''), "
			. db_escape("\n[Auth] " . $notes) . ")";

	$sql = "UPDATE " . TB_PREF . "sales_rma SET
		status = 'authorized',
		authorized_by = " . (int)$_SESSION['wa_current_user']->user . ",
		authorized_date = NOW()"
		. $auth_notes_sql
		. " WHERE id = " . (int)$rma_id . " AND status = 'pending'";
	db_query($sql, 'Could not authorize RMA');
	$success = db_num_affected_rows() > 0;
	if ($success)
		hook_invoke_all('sales_rma_authorized', $rma_id);
	return $success;
}

/**
 * Reject an RMA (pending → rejected).
 *
 * @param int    $rma_id
 * @param string $rejection_reason
 * @return bool
 */
function reject_rma($rma_id, $rejection_reason)
{
	$sql = "UPDATE " . TB_PREF . "sales_rma SET
		status = 'rejected',
		rejected_by = " . (int)$_SESSION['wa_current_user']->user . ",
		rejected_date = NOW(),
		rejection_reason = " . db_escape($rejection_reason) . "
		WHERE id = " . (int)$rma_id . " AND status = 'pending'";
	db_query($sql, 'Could not reject RMA');
	$success = db_num_affected_rows() > 0;
	if ($success)
		hook_invoke_all('sales_rma_rejected', $rma_id, $rejection_reason);
	return $success;
}

/**
 * Create a WH Return Order from an authorized RMA.
 *
 * Delegates physical goods receipt/inspection/disposition to the existing
 * WH Return Order system (ST_WH_RETURN_ORDER = 53).
 *
 * @param int    $rma_id
 * @param string $warehouse_loc_code  Warehouse location code for the return
 * @return int|false  WH return_id on success, false on failure
 */
function create_wh_return_from_rma($rma_id, $warehouse_loc_code)
{
	global $path_to_root;
	include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_returns_db.inc');

	$rma = get_sales_rma($rma_id);
	if (!$rma || $rma['status'] !== 'authorized')
		return false;

	$lines_result = get_rma_lines($rma_id);

	$wh_return_id = add_return_order(
		'customer',
		$warehouse_loc_code,
		today(),
		(int)$rma['debtor_no'],
		null,
		ST_SALESRETURN,
		$rma_id,
		'RMA#' . $rma['id'],
		'Created from RMA ' . $rma['reference']
	);

	while ($line = db_fetch($lines_result)) {
		add_return_order_line(
			$wh_return_id,
			$line['stock_id'],
			(float)$line['quantity_authorized'],
			null,
			null,
			null,
			$line['notes']
		);
	}

	// Link WH return back to RMA and advance status
	$sql = "UPDATE " . TB_PREF . "sales_rma SET
		wh_return_order_id = " . (int)$wh_return_id . ",
		status = 'wh_processing'
		WHERE id = " . (int)$rma_id;
	db_query($sql, 'Could not link WH return to RMA');

	hook_invoke_all('sales_rma_wh_return_created', $rma_id, $wh_return_id);
	return $wh_return_id;
}

/**
 * Create a credit note from an RMA (after WH processing or upon authorization).
 *
 * Uses existing write_credit_note() via Cart.  The credit note amount is
 * the refund_amount (total minus any restocking fee).
 *
 * @param int $rma_id
 * @return int|false  Credit note trans_no or false on failure
 */
function create_credit_from_rma($rma_id)
{
	global $path_to_root;
	include_once($path_to_root . '/sales/includes/cart_class.inc');
	include_once($path_to_root . '/sales/includes/sales_db.inc');
	include_once($path_to_root . '/sales/includes/db/sales_credit_db.inc');

	$rma = get_sales_rma($rma_id);
	if (!$rma)
		return false;
	if (!in_array($rma['status'], array('authorized', 'wh_processing')))
		return false;

	$lines_result = get_rma_lines($rma_id);
	$lines = array();
	while ($line = db_fetch($lines_result))
		$lines[] = $line;

	if (empty($lines))
		return false;

	// Build a credit Cart
	$cart = new Cart(ST_CUSTCREDIT);
	$defaults = get_sales_rma_document_defaults($rma['debtor_no'], $rma['branch_code']);
	$customer_row = $defaults['customer'];
	$branch = $defaults['branch'];
	if (!$customer_row)
		return false;

	$branch_code = $branch ? (int)$branch['branch_code'] : (int)$rma['branch_code'];

	$cart->set_customer(
		$rma['debtor_no'],
		$customer_row['name'],
		$customer_row['curr_code'],
		isset($customer_row['discount']) ? (float)$customer_row['discount'] : 0,
		isset($customer_row['payment_terms']) ? (int)$customer_row['payment_terms'] : 0
	);
	$cart->set_branch($branch_code, $branch ? (int)$branch['tax_group_id'] : 0,
		'', '', '');
	$cart->set_sales_type(isset($customer_row['sales_type']) ? $customer_row['sales_type'] : 0,
		'', 0, 1);
	$cart->set_location(isset($branch['default_location']) ? $branch['default_location'] : '', '');
	$cart->Branch = $branch_code;
	$cart->ship_via = isset($branch['default_ship_via']) ? (int)$branch['default_ship_via'] : 0;
	$cart->deliver_to = isset($branch['br_name']) ? $branch['br_name'] : '';
	$cart->delivery_address = isset($branch['br_post_address']) && $branch['br_post_address'] !== ''
		? $branch['br_post_address']
		: (isset($branch['br_address']) ? $branch['br_address'] : '');
	$cart->cust_ref = isset($branch['branch_ref']) ? $branch['branch_ref'] : '';
	$cart->phone = isset($branch['phone']) ? $branch['phone'] : '';
	$cart->document_date = today();
	$cart->reference = $rma['reference'] . '-CN';
	$cart->Comments = _('Credit note for RMA ') . $rma['reference'];

	// Apply restocking fee as a discount so net amount = refund_amount
	$fee_pct = (float)$rma['restocking_fee_percent'];

	foreach ($lines as $line) {
		$cart->add_to_cart(
			count($cart->line_items),
			$line['stock_id'],
			(float)$line['quantity_authorized'],
			(float)$line['unit_price'],
			$fee_pct / 100,
			0,
			0,
			$line['description']
		);
	}

	$credit_no = $cart->write(0);
	if (!$credit_no || $credit_no == -1)
		return false;

	// Link credit note back to RMA and close it
	$sql = "UPDATE " . TB_PREF . "sales_rma SET
		credit_note_no = " . (int)$credit_no . ",
		status = 'refunded'
		WHERE id = " . (int)$rma_id;
	db_query($sql, 'Could not link credit note to RMA');

	hook_invoke_all('sales_rma_credit_created', $rma_id, $credit_no);
	return $credit_no;
}

/**
 * Create a replacement sales order from an RMA.
 *
 * @param int $rma_id
 * @return int|false  Sales order number or false on failure
 */
function create_replacement_from_rma($rma_id)
{
	global $path_to_root;
	include_once($path_to_root . '/sales/includes/cart_class.inc');
	include_once($path_to_root . '/sales/includes/sales_db.inc');

	$rma = get_sales_rma($rma_id);
	if (!$rma || !in_array($rma['status'], array('authorized', 'wh_processing')))
		return false;

	$lines_result = get_rma_lines($rma_id);
	$lines = array();
	while ($line = db_fetch($lines_result))
		$lines[] = $line;

	if (empty($lines))
		return false;

	$cart = new Cart(ST_SALESORDER);
	$defaults = get_sales_rma_document_defaults($rma['debtor_no'], $rma['branch_code']);
	$customer_row = $defaults['customer'];
	$branch = $defaults['branch'];
	if (!$customer_row)
		return false;

	$branch_code = $branch ? (int)$branch['branch_code'] : (int)$rma['branch_code'];

	$cart->set_customer(
		$rma['debtor_no'],
		$customer_row['name'],
		$customer_row['curr_code'],
		isset($customer_row['discount']) ? (float)$customer_row['discount'] : 0,
		isset($customer_row['payment_terms']) ? (int)$customer_row['payment_terms'] : 0
	);
	$cart->set_branch($branch_code, $branch ? (int)$branch['tax_group_id'] : 0,
		'', '', '');
	$cart->set_sales_type(isset($customer_row['sales_type']) ? $customer_row['sales_type'] : 0,
		'', 0, 1);
	$cart->set_location(isset($branch['default_location']) ? $branch['default_location'] : '', '');
	$cart->Branch = $branch_code;
	$cart->ship_via = isset($branch['default_ship_via']) ? (int)$branch['default_ship_via'] : 0;
	$cart->deliver_to = isset($branch['br_name']) ? $branch['br_name'] : '';
	$cart->delivery_address = isset($branch['br_post_address']) && $branch['br_post_address'] !== ''
		? $branch['br_post_address']
		: (isset($branch['br_address']) ? $branch['br_address'] : '');
	$cart->cust_ref = isset($branch['branch_ref']) ? $branch['branch_ref'] : '';
	$cart->phone = isset($branch['phone']) ? $branch['phone'] : '';
	$cart->document_date = today();
	$cart->reference = 'Replacement-' . $rma['reference'];
	$cart->Comments = _('Replacement order for RMA ') . $rma['reference'];

	foreach ($lines as $line) {
		$cart->add_to_cart(
			count($cart->line_items),
			$line['stock_id'],
			(float)$line['quantity_authorized'],
			(float)$line['unit_price'],
			0,
			0,
			0,
			$line['description']
		);
	}

	$order_no = $cart->write(1);
	if ($order_no == -1)
		return false;
	$order_no = key($cart->trans_no);

	$sql = "UPDATE " . TB_PREF . "sales_rma SET
		replacement_order_no = " . (int)$order_no . "
		WHERE id = " . (int)$rma_id;
	db_query($sql, 'Could not link replacement order to RMA');

	hook_invoke_all('sales_rma_replacement_created', $rma_id, $order_no);
	return $order_no;
}

// ============================================================================
// PRE-POPULATE FROM SOURCE DOCUMENT
// ============================================================================

/**
 * Fetch items from an original customer invoice suitable for pre-populating
 * RMA line items.
 *
 * @param int $trans_no  ST_SALESINVOICE transaction number
 * @return resource|false DB result or false if not found
 */
function get_invoice_items_for_rma($trans_no)
{
	$sql = "SELECT d.stock_id, d.description, d.quantity, d.unit_price, d.discount_percent,
		m.description AS item_name
		FROM " . TB_PREF . "debtor_trans_details d
		LEFT JOIN " . TB_PREF . "stock_master m
			ON m.stock_id COLLATE utf8_general_ci = d.stock_id COLLATE utf8_general_ci
		WHERE d.debtor_trans_type = " . ST_SALESINVOICE . "
		AND d.debtor_trans_no = " . (int)$trans_no . "
		AND d.quantity > 0
		ORDER BY d.id";
	return db_query($sql, 'Could not get invoice items for RMA');
}

/**
 * Fetch items from an original customer delivery suitable for RMA line items.
 *
 * @param int $trans_no  ST_CUSTDELIVERY transaction number
 * @return resource|false
 */
function get_delivery_items_for_rma($trans_no)
{
	$sql = "SELECT d.stock_id, d.description, d.quantity, d.unit_price, d.discount_percent,
		m.description AS item_name
		FROM " . TB_PREF . "debtor_trans_details d
		LEFT JOIN " . TB_PREF . "stock_master m
			ON m.stock_id COLLATE utf8_general_ci = d.stock_id COLLATE utf8_general_ci
		WHERE d.debtor_trans_type = " . ST_CUSTDELIVERY . "
		AND d.debtor_trans_no = " . (int)$trans_no . "
		AND d.quantity > 0
		ORDER BY d.id";
	return db_query($sql, 'Could not get delivery items for RMA');
}

// ============================================================================
// ANALYTICS
// ============================================================================

/**
 * Summarise RMA data for analytics display.
 *
 * Returns an associative array with keys:
 *   total_rmas, pending, authorized, wh_processing, refunded, rejected,
 *   total_refund_amount, avg_processing_days
 *
 * @param string $date_from  SQL date or ''
 * @param string $date_to    SQL date or ''
 * @return array
 */
function get_rma_analytics($date_from = '', $date_to = '')
{
	$where = " WHERE 1=1";
	if ($date_from !== '')
		$where .= " AND request_date >= '" . $date_from . "'";
	if ($date_to !== '')
		$where .= " AND request_date <= '" . $date_to . "'";

	$sql = "SELECT
		COUNT(*) AS total_rmas,
		SUM(status = 'pending') AS pending,
		SUM(status = 'authorized') AS authorized,
		SUM(status = 'wh_processing') AS wh_processing,
		SUM(status = 'refunded') AS refunded,
		SUM(status = 'rejected') AS rejected,
		SUM(refund_amount) AS total_refund_amount,
		AVG(CASE WHEN authorized_date IS NOT NULL
			THEN DATEDIFF(authorized_date, request_date) END) AS avg_processing_days
		FROM " . TB_PREF . "sales_rma" . $where;

	$result = db_query($sql, 'Could not get RMA analytics');
	$row = db_fetch($result);
	if (!$row)
		return array(
			'total_rmas' => 0, 'pending' => 0, 'authorized' => 0,
			'wh_processing' => 0, 'refunded' => 0, 'rejected' => 0,
			'total_refund_amount' => 0, 'avg_processing_days' => 0,
		);

	// Ensure numeric types
	foreach ($row as $k => $v)
		$row[$k] = $v !== null ? (float)$v : 0;

	return $row;
}

/**
 * Fetch top return reasons by frequency.
 *
 * @param int    $limit
 * @param string $date_from SQL date or ''
 * @param string $date_to   SQL date or ''
 * @return resource DB result
 */
function get_rma_reasons_breakdown($limit = 5, $date_from = '', $date_to = '')
{
	$where = " WHERE 1=1";
	if ($date_from !== '')
		$where .= " AND r.request_date >= '" . $date_from . "'";
	if ($date_to !== '')
		$where .= " AND r.request_date <= '" . $date_to . "'";

	$sql = "SELECT rs.description AS reason, COUNT(*) AS count
		FROM " . TB_PREF . "sales_rma r
		LEFT JOIN " . TB_PREF . "sales_rma_reasons rs ON rs.id = r.return_reason_id"
		. $where . "
		GROUP BY r.return_reason_id
		ORDER BY count DESC
		LIMIT " . (int)$limit;
	return db_query($sql, 'Could not get RMA reasons breakdown');
}
