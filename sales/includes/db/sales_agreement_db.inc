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

include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/hooks.inc');
require_once __DIR__.'/sales_agreement_lifecycle_db.inc';
require_once __DIR__.'/sales_classification_db.inc';

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * Return the available sales agreement types.
 *
 * @return array
 */
function get_sales_agreement_types()
{
	return array(
		'blanket_order'       => _('Blanket Order'),
		'framework_agreement' => _('Framework Agreement'),
		'contract'            => _('Contract'),
	);
}

/**
 * Return the available sales agreement statuses.
 *
 * @return array
 */
function get_sales_agreement_statuses()
{
	return array(
		'draft'     => _('Draft'),
		'confirmed' => _('Confirmed'),
		'active'    => _('Active'),
		'expired'   => _('Expired'),
		'cancelled' => _('Cancelled'),
	);
}

/**
 * Return statuses that allow header/line editing.
 *
 * @return array
 */
function get_sales_agreement_editable_statuses()
{
	return array('draft');
}

/**
 * Determine whether one agreement status allows header/line editing.
 *
 * @param string $status
 * @return bool
 */
function is_sales_agreement_editable_status($status)
{
	return in_array($status, get_sales_agreement_editable_statuses());
}

/**
 * Fetch one sales agreement line by line id.
 *
 * @param int $line_id
 * @return array|false
 */
function get_sales_agreement_line($line_id)
{
	$sql = "SELECT *
		FROM " . TB_PREF . "sales_agreement_lines
		WHERE id = " . db_escape($line_id);

	$result = db_query($sql, 'Cannot fetch sales agreement line');

	return db_fetch($result);
}

/**
 * Validate one agreement line at the service layer.
 *
 * @param double $committed_qty
 * @param double $unit_price
 * @param double $discount_percent
 * @param double $min_qty_per_order
 * @param string $price_valid_until  SQL date or null
 * @return bool
 */
function is_sales_agreement_line_data_valid($committed_qty, $unit_price, $discount_percent,
	$min_qty_per_order = 0, $price_valid_until = null)
{
	if (!is_numeric($committed_qty) || (float)$committed_qty <= 0)
		return false;

	if (!is_numeric($unit_price) || (float)$unit_price < 0)
		return false;

	if (!is_numeric($discount_percent)
		|| (float)$discount_percent < 0
		|| (float)$discount_percent > 100)
		return false;

	if (!is_numeric($min_qty_per_order) || (float)$min_qty_per_order < 0)
		return false;

	if ((float)$min_qty_per_order > (float)$committed_qty)
		return false;

	if ($price_valid_until !== null && $price_valid_until !== '' && $price_valid_until != '0000-00-00') {
		if (!preg_match('/^(\d{4})-(\d{2})-(\d{2})$/', $price_valid_until, $matches))
			return false;

		if (!checkdate((int)$matches[2], (int)$matches[3], (int)$matches[1]))
			return false;
	}

	return true;
}

/**
 * Check whether the agreement already contains the stock id on another line.
 *
 * @param int    $agreement_id
 * @param string $stock_id
 * @param int    $exclude_line_id
 * @return bool
 */
function sales_agreement_has_duplicate_stock($agreement_id, $stock_id, $exclude_line_id = 0)
{
	$sql = "SELECT COUNT(*) AS duplicate_count
		FROM " . TB_PREF . "sales_agreement_lines
		WHERE agreement_id = " . db_escape($agreement_id) . "
			AND stock_id = " . db_escape($stock_id);

	if ($exclude_line_id > 0)
		$sql .= " AND id <> " . db_escape($exclude_line_id);

	$result = db_query($sql, 'Cannot check sales agreement line duplicates');
	$row = db_fetch($result);

	return $row && (int)$row['duplicate_count'] > 0;
}

/**
 * Return the sales agreement id linked to one sales order.
 *
 * @param int $order_no
 * @return int
 */
function get_sales_agreement_id_for_sales_order($order_no)
{
	$sql = "SELECT agreement_id
		FROM " . TB_PREF . "sales_orders
		WHERE order_no = " . db_escape($order_no) . "
			AND trans_type = " . db_escape(ST_SALESORDER);

	$result = db_query($sql, 'Cannot fetch sales agreement order link');
	$row = db_fetch($result);

	return $row ? (int)$row['agreement_id'] : 0;
}

/**
 * Return the badge color for a given agreement status.
 *
 * @param string $status
 * @return string  CSS color string.
 */
function get_sales_agreement_status_color($status)
{
	$colors = array(
		'draft'     => '#6c757d',
		'confirmed' => '#17a2b8',
		'active'    => '#28a745',
		'expired'   => '#dc3545',
		'cancelled' => '#343a40',
	);

	return isset($colors[$status]) ? $colors[$status] : '#6c757d';
}

/**
 * Render an HTML status badge for an agreement.
 *
 * @param string $status
 * @return string  HTML string.
 */
function sales_agreement_status_badge($status)
{
	$statuses = get_sales_agreement_statuses();
	$label    = isset($statuses[$status]) ? $statuses[$status] : $status;
	$color    = get_sales_agreement_status_color($status);

	return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;font-size:11px;'
		. 'font-weight:bold;color:#fff;background:' . $color . ';">' . $label . '</span>';
}

/**
 * Auto-generate a reference for a new agreement.
 *
 * @return string
 */
function generate_sales_agreement_reference()
{
	$sql = "SELECT MAX(id) AS max_id FROM " . TB_PREF . "sales_agreements";
	$result = db_query($sql, 'Cannot fetch max agreement id');
	$row    = db_fetch($result);
	$next   = $row['max_id'] ? (int)$row['max_id'] + 1 : 1;

	return 'SA-' . str_pad($next, 4, '0', STR_PAD_LEFT);
}

// ============================================================================
// AGREEMENT HEADER CRUD
// ============================================================================

/**
 * Add a new sales agreement header.
 *
 * @param string $agreement_type  'blanket_order', 'framework_agreement', or 'contract'
 * @param int    $debtor_no       Customer ID
 * @param int    $branch_code     Customer branch code
 * @param string $date_start      Start date (SQL format YYYY-MM-DD)
 * @param string $date_end        End date (SQL format) or null/empty
 * @param string $currency        Currency code
 * @param int    $payment_terms   Payment terms ID
 * @param int    $auto_renew      1 = auto-renew on expiry
 * @param int    $renewal_period_months  Renewal period in months
 * @param string $terms_and_conditions   Contract T&C text
 * @param string $notes           Internal notes
 * @param int    $dimension_id    Dimension 1
 * @param int    $dimension2_id   Dimension 2
 * @param string $reference       Reference string (auto-generated if empty)
 * @param int    $salesman_id     Salesman ID
 * @return int  New agreement ID, or 0 on failure.
 */
function add_sales_agreement($agreement_type, $debtor_no, $branch_code, $date_start, $date_end,
	$currency, $payment_terms, $auto_renew, $renewal_period_months,
	$terms_and_conditions, $notes, $dimension_id, $dimension2_id, $reference = '', $salesman_id = 0,
	$custom_data = null, $classification_intents = null)
{
    $next=array('dimension_id'=>$dimension_id,'dimension2_id'=>$dimension2_id);
    $next['custom_data']=sales_agreement_dimension_payload($custom_data);
    return sales_agreement_dimension_write('sales_agreements',null,$next,function($admitted) use ($agreement_type,$debtor_no,$branch_code,$date_start,$date_end,$currency,$payment_terms,$auto_renew,$renewal_period_months,$terms_and_conditions,$notes,$dimension_id,$dimension2_id,$reference,$salesman_id,$custom_data,$classification_intents) {
    $dimension_id=$admitted['dimension_id'];$dimension2_id=$admitted['dimension2_id'];
	global $_SESSION;

	$data = compact('agreement_type', 'debtor_no', 'branch_code', 'date_start', 'date_end',
		'currency', 'payment_terms', 'auto_renew', 'renewal_period_months',
		'terms_and_conditions', 'notes', 'dimension_id', 'dimension2_id', 'reference', 'salesman_id');

	hook_invoke_all('sales_agreement_before_save', $data);

    $generate_reference=empty($reference);
    if($generate_reference)$reference='';

	$created_by   = isset($_SESSION['wa_current_user']) ? $_SESSION['wa_current_user']->user : 0;
	$created_date = date('Y-m-d H:i:s');
	$date_end_sql = ($date_end && $date_end != '0000-00-00') ? db_escape($date_end) : 'NULL';

	$sql = "INSERT INTO " . TB_PREF . "sales_agreements
		(reference, agreement_type, debtor_no, branch_code, salesman_id, status,
		 date_start, date_end, currency, payment_terms,
		 auto_renew, renewal_period_months, terms_and_conditions, notes,
		 dimension_id, dimension2_id, created_by, created_date, custom_data)
		VALUES (
			" . db_escape($reference) . ",
			" . db_escape($agreement_type) . ",
			" . db_escape($debtor_no) . ",
			" . db_escape($branch_code) . ",
			" . db_escape($salesman_id) . ",
			'draft',
			" . db_escape($date_start) . ",
			" . $date_end_sql . ",
			" . db_escape($currency) . ",
			" . db_escape($payment_terms) . ",
			" . ($auto_renew ? 1 : 0) . ",
			" . db_escape($renewal_period_months) . ",
			" . db_escape($terms_and_conditions) . ",
			" . db_escape($notes) . ",
			" . db_escape($dimension_id) . ",
			" . db_escape($dimension2_id) . ",
			" . db_escape($created_by) . ",
			'" . $created_date . "',
            " . sales_agreement_dimension_payload_sql($admitted['custom_data']) . "
		)";

	if(!db_query($sql))return false;
	$agreement_id = db_insert_id();
	if ($classification_intents === null)
		$classification_intents = sales_classification_intents_from_values(
			$dimension_id, $dimension2_id, 0, 0, 'agreement_header');
	if ($classification_intents === false)
		throw new DimensionFinancialReferenceWriteException(_('The agreement classification selection is invalid.'));
	sales_classification_capture_agreement(ST_SALESAGREEMENT, $agreement_id,
		$classification_intents);
    if($generate_reference && !db_query('UPDATE '.TB_PREF.'sales_agreements SET reference='.db_escape('SA-'.str_pad($agreement_id,4,'0',STR_PAD_LEFT)).' WHERE id='.(int)$agreement_id))return false;

	hook_invoke_all('sales_agreement_before_save', $agreement_id);

	return $agreement_id;

    },false);
}

/**
 * Update an existing sales agreement header.
 *
 * @param int    $id              Agreement ID
 * @param string $agreement_type
 * @param int    $debtor_no
 * @param int    $branch_code
 * @param string $date_start      SQL format
 * @param string $date_end        SQL format or null
 * @param string $currency
 * @param int    $payment_terms
 * @param int    $auto_renew
 * @param int    $renewal_period_months
 * @param string $terms_and_conditions
 * @param string $notes
 * @param int    $dimension_id
 * @param int    $dimension2_id
 * @param string $reference
 * @param int    $salesman_id
 * @return bool
 */
function update_sales_agreement($id, $agreement_type, $debtor_no, $branch_code, $date_start, $date_end,
	$currency, $payment_terms, $auto_renew, $renewal_period_months,
	$terms_and_conditions, $notes, $dimension_id, $dimension2_id, $reference = '', $salesman_id = 0,
	$custom_data = null, $classification_intents = null)
{
    $next=array('dimension_id'=>$dimension_id,'dimension2_id'=>$dimension2_id);
    if(func_num_args()>=17)$next['custom_data']=sales_agreement_dimension_payload($custom_data);
    return sales_agreement_dimension_write('sales_agreements',$id,$next,function($admitted) use ($id,$agreement_type,$debtor_no,$branch_code,$date_start,$date_end,$currency,$payment_terms,$auto_renew,$renewal_period_months,$terms_and_conditions,$notes,$dimension_id,$dimension2_id,$reference,$salesman_id,$custom_data,$classification_intents) {
    $dimension_id=$admitted['dimension_id'];$dimension2_id=$admitted['dimension2_id'];
	$agreement = get_sales_agreement($id);
	if (!$agreement || !is_sales_agreement_editable_status($agreement['status']))
		return false;

	$data = compact('id', 'agreement_type', 'debtor_no');
	hook_invoke_all('sales_agreement_before_save', $data);

	$date_end_sql = ($date_end && $date_end != '0000-00-00') ? db_escape($date_end) : 'NULL';

	$sql = "UPDATE " . TB_PREF . "sales_agreements SET
		reference = " . db_escape($reference) . ",
		agreement_type = " . db_escape($agreement_type) . ",
		debtor_no = " . db_escape($debtor_no) . ",
		branch_code = " . db_escape($branch_code) . ",
		salesman_id = " . db_escape($salesman_id) . ",
		date_start = " . db_escape($date_start) . ",
		date_end = " . $date_end_sql . ",
		currency = " . db_escape($currency) . ",
		payment_terms = " . db_escape($payment_terms) . ",
		auto_renew = " . ($auto_renew ? 1 : 0) . ",
		renewal_period_months = " . db_escape($renewal_period_months) . ",
		terms_and_conditions = " . db_escape($terms_and_conditions) . ",
		notes = " . db_escape($notes) . ",
		dimension_id = " . db_escape($dimension_id) . ",
		dimension2_id = " . db_escape($dimension2_id) . (array_key_exists('custom_data',$admitted) ? ",custom_data=".sales_agreement_dimension_payload_sql($admitted['custom_data']) : '') . "
		WHERE id = " . db_escape($id) . "
		  AND status = 'draft'";

	if(!db_query($sql))return false;
	if ($classification_intents === null)
		$classification_intents = sales_classification_intents_from_values(
			$dimension_id, $dimension2_id, 0, 0, 'agreement_header');
	if ($classification_intents === false)
		throw new DimensionFinancialReferenceWriteException(_('The agreement classification selection is invalid.'));
	sales_classification_capture_agreement(ST_SALESAGREEMENT, $id, $classification_intents);
	$line_rows = db_query('SELECT id,stock_id,committed_qty FROM '.TB_PREF
		.'sales_agreement_lines WHERE agreement_id='.(int)$id.' ORDER BY id');
	if (!$line_rows)
		throw new DimensionFinancialReferenceWriteException(_('The agreement lines cannot be read.'));
	while ($line_row = db_fetch_assoc($line_rows))
		sales_classification_capture_agreement(ST_SALESAGREEMENT, $id, null, $line_row, null);

	hook_invoke_all('sales_agreement_before_save', $id);

	return true;

    },false);
}

/**
 * Delete a sales agreement (only if status is draft).
 *
 * @param int $id Agreement ID
 * @return bool  True on success, false if not deletable.
 */
function delete_sales_agreement($id)
{
    $next=array();
    return sales_agreement_dimension_write('sales_agreements',$id,$next,function($admitted) use ($id) {
	$agreement = get_sales_agreement($id);
	if (!$agreement || $agreement['status'] !== 'draft')
		return false;

    if(!db_query('DELETE FROM '.TB_PREF.'sales_agreement_lines WHERE agreement_id='.(int)$id))return false;
	$sql = "DELETE FROM " . TB_PREF . "sales_agreements WHERE id = " . db_escape($id);
	if(!db_query($sql))return false;

	return true;

    },true);
}

/**
 * Fetch a single sales agreement by ID.
 *
 * Returns an array with header columns plus computed display fields.
 *
 * @param int $id
 * @return array|false
 */
function get_sales_agreement($id)
{
	$sql = "SELECT sa.*,
		dm.name AS customer_name,
		cb.br_name AS branch_name,
		sm.salesman_name,
		pt.terms AS payment_terms_name,
		CASE WHEN sa.total_committed > 0
			THEN ROUND(sa.total_ordered / sa.total_committed * 100, 2)
			ELSE 0 END AS ordered_progress_pct,
		CASE WHEN sa.total_committed > 0
			THEN ROUND(sa.total_delivered / sa.total_committed * 100, 2)
			ELSE 0 END AS delivered_progress_pct,
		CASE WHEN sa.total_committed > 0
			THEN ROUND(sa.total_invoiced / sa.total_committed * 100, 2)
			ELSE 0 END AS invoiced_progress_pct
		FROM " . TB_PREF . "sales_agreements sa
		LEFT JOIN " . TB_PREF . "debtors_master dm ON dm.debtor_no = sa.debtor_no
		LEFT JOIN " . TB_PREF . "cust_branch cb ON cb.branch_code = sa.branch_code AND cb.debtor_no = sa.debtor_no
		LEFT JOIN " . TB_PREF . "salesman sm ON sm.salesman_code = sa.salesman_id
		LEFT JOIN " . TB_PREF . "payment_terms pt ON pt.terms_indicator = sa.payment_terms
		WHERE sa.id = " . db_escape($id);

	$result = db_query($sql, 'Cannot fetch sales agreement');

	return db_fetch($result);
}

/**
 * Fetch sales agreements with optional filters.
 *
 * @param int    $debtor_no    0 = all customers
 * @param string $status       '' = all statuses
 * @param bool   $active_only  When true, restrict to 'active' status
 * @param string $date_from    SQL date or ''
 * @param string $date_to      SQL date or ''
 * @return resource
 */
function get_sales_agreements($debtor_no = 0, $status = '', $active_only = false, $date_from = '', $date_to = '')
{
	$where = array();

	if ($active_only)
		$where[] = "sa.status = 'active'";
	elseif ($status !== '')
		$where[] = "sa.status = " . db_escape($status);

	if ($debtor_no > 0)
		$where[] = "sa.debtor_no = " . db_escape($debtor_no);

	if ($date_from !== '')
		$where[] = "sa.date_start >= '" . $date_from . "'";

	if ($date_to !== '')
		$where[] = "sa.date_start <= '" . $date_to . "'";

	$where_clause = $where ? 'WHERE ' . implode(' AND ', $where) : '';

	$sql = "SELECT sa.*,
		dm.name AS customer_name,
		cb.br_name AS branch_name,
		sm.salesman_name
		FROM " . TB_PREF . "sales_agreements sa
		LEFT JOIN " . TB_PREF . "debtors_master dm ON dm.debtor_no = sa.debtor_no
		LEFT JOIN " . TB_PREF . "cust_branch cb ON cb.branch_code = sa.branch_code AND cb.debtor_no = sa.debtor_no
		LEFT JOIN " . TB_PREF . "salesman sm ON sm.salesman_code = sa.salesman_id
		$where_clause
		ORDER BY sa.date_start DESC, sa.id DESC";

	return db_query($sql, 'Cannot fetch sales agreements');
}

// ============================================================================
// AGREEMENT STATUS TRANSITIONS
// ============================================================================

/**
 * Confirm a draft sales agreement (Draft → Confirmed).
 *
 * Agreement must have at least one line to be confirmed.
 *
 * @param int $id
 * @return bool
 */
function confirm_sales_agreement($id)
{
    return sales_agreement_lifecycle_transition($id,'confirmed','');
}

/**
 * Activate a confirmed sales agreement (Confirmed → Active).
 *
 * @param int $id
 * @return bool
 */
function activate_sales_agreement($id)
{
    return sales_agreement_lifecycle_transition($id,'active','');
}

/**
 * Expire an active sales agreement (Active → Expired).
 *
 * @param int $id
 * @return bool
 */
function expire_sales_agreement($id)
{
    return sales_agreement_lifecycle_transition($id,'expired','');
}

/**
 * Cancel a sales agreement (Confirmed or Active → Cancelled).
 *
 * @param int    $id
 * @param string $reason  Cancellation reason stored in notes.
 * @return bool
 */
function cancel_sales_agreement($id, $reason)
{
    return sales_agreement_lifecycle_transition($id,'cancelled',$reason);
}

// ============================================================================
// AGREEMENT LINE CRUD
// ============================================================================

/**
 * Add a line to a sales agreement.
 *
 * @param int    $agreement_id
 * @param string $stock_id
 * @param double $committed_qty
 * @param double $unit_price
 * @param double $discount_percent
 * @param string $description
 * @param double $min_qty_per_order
 * @param string $price_valid_until  SQL date or null
 * @return int  New line ID, or 0 on failure.
 */
function add_sales_agreement_line($agreement_id, $stock_id, $committed_qty, $unit_price,
	$discount_percent, $description = '', $min_qty_per_order = 0, $price_valid_until = null,
	$custom_data = null, $classification_intents = null)
{
    $next=array('agreement_id'=>$agreement_id);
    $next['custom_data']=sales_agreement_dimension_payload($custom_data);
    return sales_agreement_dimension_write('sales_agreement_lines',null,$next,function($admitted) use ($agreement_id,$stock_id,$committed_qty,$unit_price,$discount_percent,$description,$min_qty_per_order,$price_valid_until,$custom_data,$classification_intents) {
	$agreement = get_sales_agreement($agreement_id);
	if (!$agreement
		|| !is_sales_agreement_editable_status($agreement['status'])
		|| trim($stock_id) === ''
		|| sales_agreement_has_duplicate_stock($agreement_id, $stock_id)
		|| !is_sales_agreement_line_data_valid(
			$committed_qty,
			$unit_price,
			$discount_percent,
			$min_qty_per_order,
			$price_valid_until
		))
		return 0;

	$price_valid_sql = ($price_valid_until && $price_valid_until != '0000-00-00')
		? "'" . $price_valid_until . "'"
		: 'NULL';

	$sql = "INSERT INTO " . TB_PREF . "sales_agreement_lines
		(agreement_id, stock_id, description, committed_qty, min_qty_per_order,
		 unit_price, discount_percent, price_valid_until, custom_data)
		VALUES (
			" . db_escape($agreement_id) . ",
			" . db_escape($stock_id) . ",
			" . db_escape($description) . ",
			" . db_escape($committed_qty) . ",
			" . db_escape($min_qty_per_order) . ",
			" . db_escape($unit_price) . ",
			" . db_escape($discount_percent) . ",
			" . $price_valid_sql . "," . sales_agreement_dimension_payload_sql($admitted['custom_data']) . "
		)";

	if(!db_query($sql))return false;

	$line_id = db_insert_id();
	if ($classification_intents === null)
		$classification_intents = array(
			1 => dimension_resolution_inherit('sales_agreement_line')['intent'],
			2 => dimension_resolution_inherit('sales_agreement_line')['intent']);
	sales_classification_capture_agreement(ST_SALESAGREEMENT, $agreement_id, null,
		array('id' => $line_id, 'stock_id' => $stock_id, 'committed_qty' => $committed_qty),
		$classification_intents);

	// Update agreement committed total
	update_agreement_committed_total($agreement_id);

	return $line_id;

    },false);
}

/**
 * Update a sales agreement line.
 *
 * @param int    $line_id
 * @param string $stock_id
 * @param double $committed_qty
 * @param double $unit_price
 * @param double $discount_percent
 * @param string $description
 * @param double $min_qty_per_order
 * @param string $price_valid_until  SQL date or null
 * @return bool
 */
function update_sales_agreement_line($line_id, $stock_id, $committed_qty, $unit_price,
	$discount_percent, $description = '', $min_qty_per_order = 0, $price_valid_until = null,
	$custom_data = null, $classification_intents = null)
{
    $next=array();
    if(func_num_args()>=9)$next['custom_data']=sales_agreement_dimension_payload($custom_data);
    return sales_agreement_dimension_write('sales_agreement_lines',$line_id,$next,function($admitted) use ($line_id,$stock_id,$committed_qty,$unit_price,$discount_percent,$description,$min_qty_per_order,$price_valid_until,$custom_data,$classification_intents) {
	$line = get_sales_agreement_line($line_id);
	if (!$line)
		return false;

	$agreement_id = (int)$line['agreement_id'];
	$agreement = get_sales_agreement($agreement_id);
	if (!$agreement
		|| !is_sales_agreement_editable_status($agreement['status'])
		|| trim($stock_id) === ''
		|| sales_agreement_has_duplicate_stock($agreement_id, $stock_id, $line_id)
		|| !is_sales_agreement_line_data_valid(
			$committed_qty,
			$unit_price,
			$discount_percent,
			$min_qty_per_order,
			$price_valid_until
		))
		return false;

	$price_valid_sql = ($price_valid_until && $price_valid_until != '0000-00-00')
		? "'" . $price_valid_until . "'"
		: 'NULL';

	$sql = "UPDATE " . TB_PREF . "sales_agreement_lines SET
		stock_id = " . db_escape($stock_id) . ",
		description = " . db_escape($description) . ",
		committed_qty = " . db_escape($committed_qty) . ",
		min_qty_per_order = " . db_escape($min_qty_per_order) . ",
		unit_price = " . db_escape($unit_price) . ",
		discount_percent = " . db_escape($discount_percent) . ",
		price_valid_until = " . $price_valid_sql . (array_key_exists('custom_data',$admitted) ? ",custom_data=".sales_agreement_dimension_payload_sql($admitted['custom_data']) : '') . "
		WHERE id = " . db_escape($line_id);

	if(!db_query($sql))return false;
	if ($classification_intents === null)
		$classification_intents = array(
			1 => dimension_resolution_inherit('sales_agreement_line')['intent'],
			2 => dimension_resolution_inherit('sales_agreement_line')['intent']);
	sales_classification_capture_agreement(ST_SALESAGREEMENT, $agreement_id, null,
		array('id' => $line_id, 'stock_id' => $stock_id, 'committed_qty' => $committed_qty),
		$classification_intents);

	if ($agreement_id)
		update_agreement_committed_total($agreement_id);

	return true;

    },false);
}

/**
 * Delete a line from a sales agreement.
 *
 * @param int $line_id
 * @return bool
 */
function delete_sales_agreement_line($line_id)
{
    $next=array();
    return sales_agreement_dimension_write('sales_agreement_lines',$line_id,$next,function($admitted) use ($line_id) {
	$line = get_sales_agreement_line($line_id);
	if (!$line)
		return false;

	$agreement_id = (int)$line['agreement_id'];
	$agreement = get_sales_agreement($agreement_id);
	if (!$agreement || !is_sales_agreement_editable_status($agreement['status']))
		return false;

	$sql = "DELETE FROM " . TB_PREF . "sales_agreement_lines WHERE id = " . db_escape($line_id);
	if(!db_query($sql))return false;
    $deleted=db_num_affected_rows()>0;

	if ($agreement_id)
		update_agreement_committed_total($agreement_id);

	return $deleted;

    },true);
}

/**
 * Fetch all lines for a sales agreement.
 *
 * @param int $agreement_id
 * @return resource
 */
function get_sales_agreement_lines($agreement_id)
{
	$sql = "SELECT sal.*, si.description AS item_description, si.units AS stock_unit
		FROM " . TB_PREF . "sales_agreement_lines sal
		LEFT JOIN " . TB_PREF . "stock_master si ON si.stock_id = sal.stock_id 
		WHERE sal.agreement_id = " . db_escape($agreement_id) . "
		ORDER BY sal.id";

	return db_query($sql, 'Cannot fetch sales agreement lines');
}

/**
 * Recalculate and save the total committed value on the agreement header.
 *
 * @param int $agreement_id
 */
function update_agreement_committed_total($agreement_id)
{
    return sales_agreement_dimension_maintenance($agreement_id,function()use($agreement_id){
	$sql = "UPDATE " . TB_PREF . "sales_agreements sa
		SET total_committed = (
			SELECT IFNULL(SUM(sal.committed_qty * sal.unit_price * (1 - sal.discount_percent / 100)), 0)
			FROM " . TB_PREF . "sales_agreement_lines sal
			WHERE sal.agreement_id = sa.id
		)
		WHERE sa.id = " . db_escape($agreement_id);

	if(!db_query($sql))return false;

        return true;
    });
}

/**
 * Recalculate ordered, delivered, and invoiced totals from linked sales orders.
 *
 * Totals are derived from 0_sales_orders linked via agreement_id.
 *
 * @param int $agreement_id
 */
function update_sales_agreement_fulfillment($agreement_id)
{
    return sales_agreement_dimension_maintenance($agreement_id,function()use($agreement_id){
	// Update ordered total from linked SO lines using net line values.
	$sql_ordered = "UPDATE " . TB_PREF . "sales_agreements sa
		SET total_ordered = IFNULL((
			SELECT SUM(sod.quantity * sod.unit_price * (1 - sod.discount_percent))
			FROM " . TB_PREF . "sales_order_details sod
			INNER JOIN " . TB_PREF . "sales_orders so ON so.order_no = sod.order_no AND so.trans_type = sod.trans_type
			WHERE so.agreement_id = sa.id AND so.trans_type = 30
		), 0)
		WHERE sa.id = " . db_escape($agreement_id);

	if(!db_query($sql_ordered))return false;

	$sql_delivered = "UPDATE " . TB_PREF . "sales_agreements sa
		SET total_delivered = IFNULL((
			SELECT SUM(sod.qty_sent * sod.unit_price * (1 - sod.discount_percent))
			FROM " . TB_PREF . "sales_order_details sod
			INNER JOIN " . TB_PREF . "sales_orders so ON so.order_no = sod.order_no AND so.trans_type = sod.trans_type
			WHERE so.agreement_id = sa.id AND so.trans_type = " . db_escape(ST_SALESORDER) . "
		), 0)
		WHERE sa.id = " . db_escape($agreement_id);

	if(!db_query($sql_delivered))return false;

	$sql_invoiced = "UPDATE " . TB_PREF . "sales_agreements sa
		SET total_invoiced = IFNULL((
			SELECT SUM(dtd.quantity * dtd.unit_price * (1 - dtd.discount_percent))
			FROM " . TB_PREF . "debtor_trans_details dtd
			INNER JOIN " . TB_PREF . "debtor_trans dt
				ON dt.trans_no = dtd.debtor_trans_no
				AND dt.type = dtd.debtor_trans_type
			INNER JOIN " . TB_PREF . "sales_orders so ON so.order_no = dt.order_ AND so.trans_type = " . db_escape(ST_SALESORDER) . "
			LEFT JOIN " . TB_PREF . "voided v ON v.type = dt.type AND v.id = dt.trans_no
			WHERE so.agreement_id = sa.id
				AND dt.type = " . db_escape(ST_SALESINVOICE) . "
				AND v.id IS NULL
		), 0)
		WHERE sa.id = " . db_escape($agreement_id);

	if(!db_query($sql_invoiced))return false;

	// Update per-line ordered quantities from linked sales order lines.
	$lines = get_sales_agreement_lines($agreement_id);
	while ($line = db_fetch($lines)) {
		$sql_line_fulfillment = "UPDATE " . TB_PREF . "sales_agreement_lines sal
			SET ordered_qty = IFNULL((
				SELECT SUM(sod.quantity)
				FROM " . TB_PREF . "sales_order_details sod
				INNER JOIN " . TB_PREF . "sales_orders so ON so.order_no = sod.order_no AND so.trans_type = sod.trans_type
				WHERE so.agreement_id = " . db_escape($agreement_id) . "
				  AND so.trans_type = " . db_escape(ST_SALESORDER) . "
				  AND sod.stk_code = " . db_escape($line['stock_id']) . "
			), 0),
			delivered_qty = IFNULL((
				SELECT SUM(sod.qty_sent)
				FROM " . TB_PREF . "sales_order_details sod
				INNER JOIN " . TB_PREF . "sales_orders so ON so.order_no = sod.order_no AND so.trans_type = sod.trans_type
				WHERE so.agreement_id = " . db_escape($agreement_id) . "
				  AND so.trans_type = " . db_escape(ST_SALESORDER) . "
				  AND sod.stk_code = sal.stock_id
			), 0),
			invoiced_qty = IFNULL((
				SELECT SUM(dtd.quantity)
				FROM " . TB_PREF . "debtor_trans_details dtd
				INNER JOIN " . TB_PREF . "debtor_trans dt
					ON dt.trans_no = dtd.debtor_trans_no
					AND dt.type = dtd.debtor_trans_type
				INNER JOIN " . TB_PREF . "sales_orders so ON so.order_no = dt.order_ AND so.trans_type = " . db_escape(ST_SALESORDER) . "
				LEFT JOIN " . TB_PREF . "voided v ON v.type = dt.type AND v.id = dt.trans_no
				WHERE so.agreement_id = " . db_escape($agreement_id) . "
				  AND dt.type = " . db_escape(ST_SALESINVOICE) . "
				  AND v.id IS NULL
				  AND dtd.stock_id = sal.stock_id
			), 0)
			WHERE sal.id = " . db_escape($line['id']);

		if(!db_query($sql_line_fulfillment))return false;
	}

        return true;
    });
}

// ============================================================================
// AGREEMENT AVAILABILITY AND ORDER CREATION
// ============================================================================

/**
 * Check remaining available quantity for an item on an agreement.
 *
 * @param int    $agreement_id
 * @param string $stock_id
 * @return double  Remaining committed qty (committed - ordered). Returns 0 if none.
 */
function check_sales_agreement_availability($agreement_id, $stock_id)
{
	$sql = "SELECT IFNULL(SUM(GREATEST(0, committed_qty - ordered_qty)), 0) AS remaining
		FROM " . TB_PREF . "sales_agreement_lines
		WHERE agreement_id = " . db_escape($agreement_id) . "
		  AND stock_id = " . db_escape($stock_id);

	$result = db_query($sql, 'Cannot check agreement availability');
	$row    = db_fetch($result);

	return $row ? (double)$row['remaining'] : 0;
}

/**
 * Create a new sales order pre-populated from an active agreement.
 *
 * Sets the agreement reference on the new SO header and links the lines
 * with agreed prices. After creation, triggers fulfillment recalculation.
 *
 * @param int   $agreement_id
 * @param array $line_selections  Associative array: line_id => quantity_to_order.
 *                                Pass empty array to include all lines at committed qty.
 * @return int  New sales order number, or 0 on failure.
 */
function create_order_from_agreement($agreement_id, $line_selections = array())
{
    require_once __DIR__.'/sales_agreement_order_db.inc';
    $result=submit_agreement_sales_order($agreement_id,$line_selections);
    return $result && $result['status']==='posted' ? (int)$result['trans_no'] : 0;
}

// ============================================================================
// AUTO-RENEW AND EXPIRY UTILITIES
// ============================================================================

/**
 * Get agreements expiring within a given number of days.
 *
 * Used by dashboard / notification systems.
 *
 * @param int $days_ahead  Number of days to look ahead.
 * @return resource
 */
function get_expiring_sales_agreements($days_ahead = 30)
{
	$alert_date = date('Y-m-d', strtotime("+$days_ahead days"));

	$sql = "SELECT sa.*, dm.name AS customer_name
		FROM " . TB_PREF . "sales_agreements sa
		LEFT JOIN " . TB_PREF . "debtors_master dm ON dm.debtor_no = sa.debtor_no
		WHERE sa.status = 'active'
		  AND sa.date_end IS NOT NULL
		  AND sa.date_end <= '" . $alert_date . "'
		ORDER BY sa.date_end ASC";

	return db_query($sql, 'Cannot fetch expiring agreements');
}

/**
 * Auto-renew all active agreements with auto_renew = 1 that have expired or are about to expire.
 *
 * Creates a new draft agreement with dates offset by renewal_period_months.
 *
 * @return int  Number of agreements renewed.
 */
function auto_renew_sales_agreements()
{
    $caller_owned=!empty($GLOBALS['transaction_level']);
    $rows=db_query('SELECT id FROM '.TB_PREF.'sales_agreements WHERE status IN (\'active\',\'expired\') AND auto_renew=1 AND date_end IS NOT NULL AND date_end<'.db_escape(date2sql(Today())).' ORDER BY id');
    if(!$rows)return 0;$ids=array();while($row=db_fetch($rows))$ids[]=(int)$row['id'];$count=0;
    foreach($ids as $id){if(renew_sales_agreement($id))$count++;elseif($caller_owned && empty($GLOBALS['transaction_level']))return 0;}
    return $count;
}

/**
 * Render a compact fulfillment progress bar for an agreement line.
 *
 * @param double $current_qty
 * @param double $committed_qty
 * @param string $stock_id
 * @param string $color  CSS color.
 * @return string  HTML string.
 */
function render_sales_agreement_line_progress($current_qty, $committed_qty, $stock_id, $color)
{
	$decimals   = get_qty_dec($stock_id);
	$percentage = $committed_qty > 0 ? min(100, round(($current_qty / $committed_qty) * 100, 2)) : 0;

	return '<div style="min-width:130px;">'
		. '<div style="font-size:11px;color:#666;margin-bottom:2px;">'
		. number_format2($current_qty, $decimals) . ' / ' . number_format2($committed_qty, $decimals)
		. '</div>'
		. '<div style="height:8px;background:#e9ecef;border-radius:999px;overflow:hidden;">'
		. '<div style="width:' . $percentage . '%;height:8px;background:' . $color . ';"></div>'
		. '</div></div>';
}

/** Submit the current native Sales agreement to its registered shared-core policy. */
function submit_sales_agreement($id){return submit_sales_agreement_lifecycle($id);}
