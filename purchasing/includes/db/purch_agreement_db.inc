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
include_once($path_to_root . '/purchasing/includes/po_class.inc');
include_once($path_to_root . '/purchasing/includes/db/po_db.inc');
include_once($path_to_root . '/purchasing/includes/db/suppliers_db.inc');

/**
 * Get available purchase agreement types.
 *
 * @return array
 */
function get_purch_agreement_types()
{
	return array(
		'blanket_order' => _('Blanket Order'),
		'framework_agreement' => _('Framework Agreement'),
		'contract' => _('Contract'),
	);
}

/**
 * Get available purchase agreement statuses.
 *
 * @return array
 */
function get_purch_agreement_statuses()
{
	return array(
		'draft' => _('Draft'),
		'confirmed' => _('Confirmed'),
		'active' => _('Active'),
		'expired' => _('Expired'),
		'cancelled' => _('Cancelled'),
	);
}

/**
 * Get the badge color for one agreement status.
 *
 * @param string $status
 * @return string
 */
function get_purch_agreement_status_color($status)
{
	$colors = array(
		'draft' => '#6c757d',
		'confirmed' => '#17a2b8',
		'active' => '#28a745',
		'expired' => '#dc3545',
		'cancelled' => '#343a40',
	);

	return isset($colors[$status]) ? $colors[$status] : '#6c757d';
}

/**
 * Render an agreement status badge.
 *
 * @param string $status
 * @return string
 */
function purch_agreement_status_badge($status)
{
	$statuses = get_purch_agreement_statuses();
	$label = isset($statuses[$status]) ? $statuses[$status] : $status;
	$color = get_purch_agreement_status_color($status);

	return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;font-size:11px;'
		. 'font-weight:bold;color:#fff;background:' . $color . ';">' . $label . '</span>';
}

/**
 * Format a purchase agreement reference.
 *
 * @param int $agreement_id
 * @return string
 */
function format_purch_agreement_reference($agreement_id)
{
	return 'AGR-' . str_pad((int)$agreement_id, 4, '0', STR_PAD_LEFT);
}

/**
 * Encode agreement custom data for JSON columns.
 *
 * @param mixed $custom_data
 * @return string|null
 */
function encode_purch_agreement_custom_data($custom_data)
{
	if ($custom_data === null || $custom_data === '' || $custom_data === array())
		return null;

	if (is_string($custom_data))
		return $custom_data;

	$encoded = json_encode($custom_data);
	return $encoded === false ? null : $encoded;
}

/**
 * Build a SQL-safe JSON literal for agreement custom data columns.
 *
 * @param mixed $custom_data
 * @return string
 */
function sql_purch_agreement_custom_data($custom_data)
{
	$encoded = encode_purch_agreement_custom_data($custom_data);
	return $encoded === null ? 'NULL' : "'" . db_escape_function($encoded) . "'";
}

/**
 * Decode agreement custom data from storage.
 *
 * @param string|null $custom_data
 * @return array
 */
function decode_purch_agreement_custom_data($custom_data)
{
	if (!$custom_data)
		return array();

	$decoded = json_decode($custom_data, true);
	if (!is_array($decoded))
		$decoded = json_decode(html_entity_decode($custom_data, ENT_QUOTES, 'UTF-8'), true);

	return is_array($decoded) ? $decoded : array();
}

/**
 * Get statuses that can still be edited.
 *
 * @return array
 */
function get_purch_agreement_editable_statuses()
{
	return array('draft');
}

/**
 * Get the current editable agreement row.
 *
 * @param int $agreement_id
 * @return array|false
 */
function get_editable_purch_agreement($agreement_id)
{
	$agreement = get_purch_agreement($agreement_id);
	if (!$agreement)
		return false;

	return in_array($agreement['status'], get_purch_agreement_editable_statuses())
		? $agreement
		: false;
}

/**
 * Calculate and persist the total committed value for one agreement.
 *
 * @param int $agreement_id
 * @return void
 */
function update_purch_agreement_total_committed($agreement_id)
{
	$sql = "SELECT COALESCE(SUM(committed_qty * (unit_price * (1 - (discount_percent / 100)))), 0) AS total_committed
		FROM " . TB_PREF . "purch_agreement_lines
		WHERE agreement_id = " . (int)$agreement_id;
	$result = db_query($sql, 'could not calculate purchase agreement committed total');
	$row = db_fetch($result);

	$sql = "UPDATE " . TB_PREF . "purch_agreements SET
		total_committed = " . db_escape($row ? (float)$row['total_committed'] : 0) . "
		WHERE id = " . (int)$agreement_id;
	db_query($sql, 'could not update purchase agreement committed total');
}

/**
 * Get one purchase agreement line.
 *
 * @param int $line_id
 * @return array|false
 */
function get_purch_agreement_line($line_id)
{
	$sql = "SELECT agreement_line.*,
		agreement.reference AS agreement_reference,
		COALESCE(agreement_line.description, stock.description) AS line_description,
		stock.description AS stock_description,
		stock.units AS stock_units
		FROM " . TB_PREF . "purch_agreement_lines agreement_line
		INNER JOIN " . TB_PREF . "purch_agreements agreement ON agreement.id = agreement_line.agreement_id
		LEFT JOIN " . TB_PREF . "stock_master stock ON stock.stock_id = agreement_line.stock_id
		WHERE agreement_line.id = " . (int)$line_id;
	$result = db_query($sql, 'could not get purchase agreement line');
	$row = db_fetch($result);

	if ($row) {
		$row['custom_data_array'] = decode_purch_agreement_custom_data($row['custom_data']);
		$row['remaining_qty'] = max(0, (float)$row['committed_qty'] - (float)$row['ordered_qty']);
		$row['effective_unit_price'] = round((float)$row['unit_price'] * (1 - ((float)$row['discount_percent'] / 100)), user_price_dec());
	}

	return $row;
}

/**
 * Get one purchase agreement header.
 *
 * @param int $agreement_id
 * @return array|false
 */
function get_purch_agreement($agreement_id)
{
	$sql = "SELECT agreement.*,
		supplier.supp_name,
		supplier.curr_code AS supplier_currency,
		creator.real_name AS created_by_name,
		buyer.real_name AS buyer_name,
		location.location_name AS delivery_location_name,
		payment_terms.terms AS payment_terms_name,
		rfq.reference AS rfq_reference,
		COUNT(DISTINCT agreement_line.id) AS line_count,
		COALESCE(SUM(GREATEST(agreement_line.committed_qty - agreement_line.ordered_qty, 0)), 0) AS remaining_qty
		FROM " . TB_PREF . "purch_agreements agreement
		INNER JOIN " . TB_PREF . "suppliers supplier ON supplier.supplier_id = agreement.supplier_id
		LEFT JOIN " . TB_PREF . "users creator ON creator.id = agreement.created_by
		LEFT JOIN " . TB_PREF . "users buyer ON buyer.id = agreement.buyer_id
		LEFT JOIN " . TB_PREF . "locations location ON location.loc_code = agreement.delivery_location
		LEFT JOIN " . TB_PREF . "payment_terms payment_terms ON payment_terms.terms_indicator = agreement.payment_terms
		LEFT JOIN " . TB_PREF . "purch_rfq rfq ON rfq.id = agreement.rfq_id
		LEFT JOIN " . TB_PREF . "purch_agreement_lines agreement_line ON agreement_line.agreement_id = agreement.id
		WHERE agreement.id = " . (int)$agreement_id . "
		GROUP BY agreement.id";
	$result = db_query($sql, 'could not get purchase agreement');
	$row = db_fetch($result);

	if ($row) {
		$row['custom_data_array'] = decode_purch_agreement_custom_data($row['custom_data']);
		$row['ordered_progress_pct'] = (float)$row['total_committed'] > 0
			? min(100, round(((float)$row['total_ordered'] / (float)$row['total_committed']) * 100, 2))
			: 0;
		$row['received_progress_pct'] = (float)$row['total_committed'] > 0
			? min(100, round(((float)$row['total_received'] / (float)$row['total_committed']) * 100, 2))
			: 0;
		$row['invoiced_progress_pct'] = (float)$row['total_committed'] > 0
			? min(100, round(((float)$row['total_invoiced'] / (float)$row['total_committed']) * 100, 2))
			: 0;
	}

	return $row;
}

/**
 * Get purchase agreements by filter.
 *
 * @param int    $supplier_id
 * @param string $status
 * @param bool   $active_only
 * @param string $agreement_type
 * @param string $date_from
 * @param string $date_to
 * @return resource
 */
function get_purch_agreements($supplier_id = 0, $status = '', $active_only = true,
	$agreement_type = '', $date_from = '', $date_to = '')
{
	$sql = "SELECT agreement.*,
		supplier.supp_name,
		location.location_name AS delivery_location_name,
		COUNT(DISTINCT agreement_line.id) AS line_count,
		COALESCE(SUM(GREATEST(agreement_line.committed_qty - agreement_line.ordered_qty, 0)), 0) AS remaining_qty
		FROM " . TB_PREF . "purch_agreements agreement
		INNER JOIN " . TB_PREF . "suppliers supplier ON supplier.supplier_id = agreement.supplier_id
		LEFT JOIN " . TB_PREF . "locations location ON location.loc_code = agreement.delivery_location
		LEFT JOIN " . TB_PREF . "purch_agreement_lines agreement_line ON agreement_line.agreement_id = agreement.id
		WHERE 1 = 1";

	if ((int)$supplier_id > 0)
		$sql .= " AND agreement.supplier_id = " . (int)$supplier_id;
	if ($status !== '')
		$sql .= " AND agreement.status = " . db_escape($status);
	if ($agreement_type !== '')
		$sql .= " AND agreement.agreement_type = " . db_escape($agreement_type);
	if ($date_from !== '')
		$sql .= " AND agreement.date_start >= " . db_escape($date_from);
	if ($date_to !== '')
		$sql .= " AND agreement.date_start <= " . db_escape($date_to);
	if ($active_only) {
		$sql .= " AND agreement.status = 'active'";
		$sql .= " AND agreement.inactive = 0";
		$sql .= " AND agreement.date_start <= " . db_escape(date2sql(Today()));
		$sql .= " AND (agreement.date_end IS NULL OR agreement.date_end = '0000-00-00' OR agreement.date_end >= " . db_escape(date2sql(Today())) . ")";
	}

	$sql .= " GROUP BY agreement.id ORDER BY agreement.date_start DESC, agreement.id DESC";
	return db_query($sql, 'could not get purchase agreements');
}

/**
 * Build agreement draft data for the approval workflow.
 *
 * @param int $agreement_id
 * @return array
 */
function get_purchase_agreement_submission_data($agreement_id)
{
	$agreement = get_purch_agreement($agreement_id);
	if (!$agreement)
		return array();

	$data = array(
		'agreement_id' => (int)$agreement['id'],
		'reference' => $agreement['reference'],
		'agreement_type' => $agreement['agreement_type'],
		'supplier_id' => (int)$agreement['supplier_id'],
		'supplier_name' => $agreement['supp_name'],
		'buyer_id' => (int)$agreement['buyer_id'],
		'buyer_name' => isset($agreement['buyer_name']) ? $agreement['buyer_name'] : '',
		'status' => $agreement['status'],
		'date_start' => $agreement['date_start'],
		'date_end' => $agreement['date_end'],
		'currency' => $agreement['currency'],
		'payment_terms' => (int)$agreement['payment_terms'],
		'payment_terms_name' => isset($agreement['payment_terms_name']) ? $agreement['payment_terms_name'] : '',
		'total_committed' => (float)$agreement['total_committed'],
		'delivery_location' => $agreement['delivery_location'],
		'delivery_location_name' => isset($agreement['delivery_location_name']) ? $agreement['delivery_location_name'] : '',
		'auto_renew' => (int)$agreement['auto_renew'],
		'renewal_period_months' => (int)$agreement['renewal_period_months'],
		'notes' => $agreement['notes'],
		'terms_and_conditions' => $agreement['terms_and_conditions'],
		'rfq_id' => (int)$agreement['rfq_id'],
		'rfq_reference' => isset($agreement['rfq_reference']) ? $agreement['rfq_reference'] : '',
		'dimension_id' => (int)$agreement['dimension_id'],
		'dimension2_id' => (int)$agreement['dimension2_id'],
		'custom_data' => decode_purch_agreement_custom_data($agreement['custom_data']),
		'line_items' => array(),
	);

	$lines = get_agreement_lines($agreement_id);
	while ($line = db_fetch($lines)) {
		$data['line_items'][] = array(
			'id' => (int)$line['id'],
			'stock_id' => $line['stock_id'],
			'description' => $line['line_description'],
			'committed_qty' => (float)$line['committed_qty'],
			'min_qty_per_order' => (float)$line['min_qty_per_order'],
			'ordered_qty' => (float)$line['ordered_qty'],
			'received_qty' => (float)$line['received_qty'],
			'invoiced_qty' => (float)$line['invoiced_qty'],
			'unit_price' => (float)$line['unit_price'],
			'discount_percent' => (float)$line['discount_percent'],
			'price_valid_until' => $line['price_valid_until'],
		);
	}

	return $data;
}

/**
 * Submit one draft agreement for approval or auto-confirm it when no approval is required.
 *
 * @param int $agreement_id
 * @return array|false
 */
function submit_purchase_agreement($agreement_id)
{
	$agreement = get_purch_agreement($agreement_id);
	if (!$agreement || $agreement['status'] !== 'draft' || (int)$agreement['line_count'] <= 0)
		return false;

	$approval_amount = (float)$agreement['total_committed'];
	if (!class_exists('ApprovalWorkflow') || !ApprovalWorkflow::isRegistered(ST_PURCHAGREEMENT)) {
		if (confirm_agreement($agreement_id))
			return array('status' => 'auto_approved', 'agreement_id' => (int)$agreement_id);
		return false;
	}

	$approval = get_approval_workflow_service();
	if (!$approval->isApprovalRequired(ST_PURCHAGREEMENT, $approval_amount)) {
		if (confirm_agreement($agreement_id))
			return array('status' => 'auto_approved', 'agreement_id' => (int)$agreement_id);
		return false;
	}

	$draft_data = get_purchase_agreement_submission_data($agreement_id);
	return $approval->submit(
		ST_PURCHAGREEMENT,
		$draft_data,
		$approval_amount,
		array(
			'summary' => sprintf(_('Purchase Agreement %s'), $agreement['reference']),
			'currency' => $agreement['currency'] ? $agreement['currency'] : get_company_pref('curr_default'),
			'person_type' => PT_SUPPLIER,
			'person_id' => $agreement['supplier_id'],
			'loc_code' => $agreement['delivery_location'],
		)
	);
}

/**
 * Add a purchase agreement.
 *
 * @param string $agreement_type
 * @param int    $supplier_id
 * @param string $date_start
 * @param string $date_end
 * @param string $currency
 * @param int    $payment_terms
 * @param string $delivery_location
 * @param int    $auto_renew
 * @param int    $renewal_period_months
 * @param string $terms_and_conditions
 * @param string $notes
 * @param int    $rfq_id
 * @param int    $dimension_id
 * @param int    $dimension2_id
 * @param string $reference
 * @param int    $buyer_id
 * @param mixed  $custom_data
 * @return int|false
 */
function add_purch_agreement($agreement_type, $supplier_id, $date_start, $date_end, $currency,
	$payment_terms = 0, $delivery_location = '', $auto_renew = 0, $renewal_period_months = 12,
	$terms_and_conditions = '', $notes = '', $rfq_id = 0, $dimension_id = 0, $dimension2_id = 0,
	$reference = '', $buyer_id = 0, $custom_data = array())
{
	if ((int)$supplier_id <= 0 || !get_supplier((int)$supplier_id))
		return false;

	$hook_data = array(
		'agreement_type' => $agreement_type,
		'supplier_id' => (int)$supplier_id,
		'buyer_id' => (int)$buyer_id,
		'date_start' => $date_start,
		'date_end' => $date_end,
		'currency' => $currency,
		'payment_terms' => (int)$payment_terms,
		'delivery_location' => $delivery_location,
		'auto_renew' => (int)$auto_renew,
		'renewal_period_months' => (int)$renewal_period_months,
		'terms_and_conditions' => $terms_and_conditions,
		'notes' => $notes,
		'rfq_id' => (int)$rfq_id,
		'dimension_id' => (int)$dimension_id,
		'dimension2_id' => (int)$dimension2_id,
		'reference' => $reference,
		'created_by' => (int)$_SESSION['wa_current_user']->user,
		'custom_data' => $custom_data,
	);
	hook_invoke_all('purch_agreement_before_save', $hook_data);

	$sql = "INSERT INTO " . TB_PREF . "purch_agreements (
		reference,
		agreement_type,
		supplier_id,
		buyer_id,
		status,
		date_start,
		date_end,
		currency,
		payment_terms,
		delivery_location,
		auto_renew,
		renewal_period_months,
		terms_and_conditions,
		notes,
		rfq_id,
		dimension_id,
		dimension2_id,
		created_by,
		created_date,
		custom_data
	) VALUES (
		'',
		" . db_escape($hook_data['agreement_type']) . ",
		" . (int)$hook_data['supplier_id'] . ",
		" . (int)$hook_data['buyer_id'] . ",
		'draft',
		" . db_escape($hook_data['date_start']) . ",
		" . ($hook_data['date_end'] ? db_escape($hook_data['date_end']) : 'NULL') . ",
		" . db_escape($hook_data['currency']) . ",
		" . (int)$hook_data['payment_terms'] . ",
		" . db_escape($hook_data['delivery_location']) . ",
		" . (int)$hook_data['auto_renew'] . ",
		" . max(1, (int)$hook_data['renewal_period_months']) . ",
		" . ($hook_data['terms_and_conditions'] !== '' ? db_escape($hook_data['terms_and_conditions']) : 'NULL') . ",
		" . ($hook_data['notes'] !== '' ? db_escape($hook_data['notes']) : 'NULL') . ",
		" . (int)$hook_data['rfq_id'] . ",
		" . (int)$hook_data['dimension_id'] . ",
		" . (int)$hook_data['dimension2_id'] . ",
		" . (int)$hook_data['created_by'] . ",
		NOW(),
		" . sql_purch_agreement_custom_data($hook_data['custom_data']) . "
	)";
	db_query($sql, 'could not insert purchase agreement');

	$agreement_id = db_insert_id();
	$generated_reference = $hook_data['reference'] !== '' ? $hook_data['reference'] : format_purch_agreement_reference($agreement_id);

	$sql = "UPDATE " . TB_PREF . "purch_agreements SET
		reference = " . db_escape($generated_reference) . "
		WHERE id = " . (int)$agreement_id;
	db_query($sql, 'could not update purchase agreement reference');

	return $agreement_id;
}

/**
 * Update a purchase agreement.
 *
 * @param int    $agreement_id
 * @param string $agreement_type
 * @param int    $supplier_id
 * @param string $date_start
 * @param string $date_end
 * @param string $currency
 * @param int    $payment_terms
 * @param string $delivery_location
 * @param int    $auto_renew
 * @param int    $renewal_period_months
 * @param string $terms_and_conditions
 * @param string $notes
 * @param int    $rfq_id
 * @param int    $dimension_id
 * @param int    $dimension2_id
 * @param string $reference
 * @param int    $buyer_id
 * @param mixed  $custom_data
 * @return bool
 */
function update_purch_agreement($agreement_id, $agreement_type, $supplier_id, $date_start, $date_end, $currency,
	$payment_terms = 0, $delivery_location = '', $auto_renew = 0, $renewal_period_months = 12,
	$terms_and_conditions = '', $notes = '', $rfq_id = 0, $dimension_id = 0, $dimension2_id = 0,
	$reference = '', $buyer_id = 0, $custom_data = array())
{
	$agreement = get_editable_purch_agreement($agreement_id);
	if (!$agreement || (int)$supplier_id <= 0 || !get_supplier((int)$supplier_id))
		return false;

	$hook_data = array(
		'id' => (int)$agreement_id,
		'agreement_type' => $agreement_type,
		'supplier_id' => (int)$supplier_id,
		'buyer_id' => (int)$buyer_id,
		'date_start' => $date_start,
		'date_end' => $date_end,
		'currency' => $currency,
		'payment_terms' => (int)$payment_terms,
		'delivery_location' => $delivery_location,
		'auto_renew' => (int)$auto_renew,
		'renewal_period_months' => (int)$renewal_period_months,
		'terms_and_conditions' => $terms_and_conditions,
		'notes' => $notes,
		'rfq_id' => (int)$rfq_id,
		'dimension_id' => (int)$dimension_id,
		'dimension2_id' => (int)$dimension2_id,
		'reference' => $reference !== '' ? $reference : $agreement['reference'],
		'custom_data' => $custom_data,
	);
	hook_invoke_all('purch_agreement_before_save', $hook_data);

	$sql = "UPDATE " . TB_PREF . "purch_agreements SET
		agreement_type = " . db_escape($hook_data['agreement_type']) . ",
		supplier_id = " . (int)$hook_data['supplier_id'] . ",
		buyer_id = " . (int)$hook_data['buyer_id'] . ",
		date_start = " . db_escape($hook_data['date_start']) . ",
		date_end = " . ($hook_data['date_end'] ? db_escape($hook_data['date_end']) : 'NULL') . ",
		currency = " . db_escape($hook_data['currency']) . ",
		payment_terms = " . (int)$hook_data['payment_terms'] . ",
		delivery_location = " . db_escape($hook_data['delivery_location']) . ",
		auto_renew = " . (int)$hook_data['auto_renew'] . ",
		renewal_period_months = " . max(1, (int)$hook_data['renewal_period_months']) . ",
		terms_and_conditions = " . ($hook_data['terms_and_conditions'] !== '' ? db_escape($hook_data['terms_and_conditions']) : 'NULL') . ",
		notes = " . ($hook_data['notes'] !== '' ? db_escape($hook_data['notes']) : 'NULL') . ",
		rfq_id = " . (int)$hook_data['rfq_id'] . ",
		dimension_id = " . (int)$hook_data['dimension_id'] . ",
		dimension2_id = " . (int)$hook_data['dimension2_id'] . ",
		reference = " . db_escape($hook_data['reference']) . ",
		custom_data = " . sql_purch_agreement_custom_data($hook_data['custom_data']) . "
		WHERE id = " . (int)$agreement_id;
	db_query($sql, 'could not update purchase agreement');

	update_purch_agreement_total_committed($agreement_id);
	update_agreement_fulfillment($agreement_id);

	return true;
}

/**
 * Delete a purchase agreement.
 *
 * @param int $agreement_id
 * @return bool
 */
function delete_purch_agreement($agreement_id)
{
	$agreement = get_purch_agreement($agreement_id);
	if (!$agreement || $agreement['status'] !== 'draft')
		return false;

	begin_transaction();

	$sql = "DELETE FROM " . TB_PREF . "purch_agreement_lines WHERE agreement_id = " . (int)$agreement_id;
	db_query($sql, 'could not delete purchase agreement lines');

	$sql = "DELETE FROM " . TB_PREF . "purch_agreements WHERE id = " . (int)$agreement_id;
	db_query($sql, 'could not delete purchase agreement');

	commit_transaction();
	return true;
}

/**
 * Set a purchase agreement status.
 *
 * @param int    $agreement_id
 * @param string $status
 * @param string $reason
 * @return bool
 */
function set_purch_agreement_status($agreement_id, $status, $reason = '')
{
	$agreement = get_purch_agreement($agreement_id);
	if (!$agreement)
		return false;

	$allowed_statuses = get_purch_agreement_statuses();
	if (!isset($allowed_statuses[$status]))
		return false;

	$custom_data = decode_purch_agreement_custom_data($agreement['custom_data']);
	if ($reason !== '')
		$custom_data['status_reason_' . $status] = $reason;

	$inactive = in_array($status, array('expired', 'cancelled')) ? 1 : 0;

	$sql = "UPDATE " . TB_PREF . "purch_agreements SET
		status = " . db_escape($status) . ",
		inactive = " . (int)$inactive . ",
		custom_data = " . sql_purch_agreement_custom_data($custom_data) . "
		WHERE id = " . (int)$agreement_id;
	db_query($sql, 'could not update purchase agreement status');

	if ($status === 'confirmed')
	{
		$hook_data = array('agreement_id' => (int)$agreement_id);
		hook_invoke_all('purch_agreement_confirmed', $hook_data);
	}
	elseif ($status === 'expired')
	{
		$hook_data = array('agreement_id' => (int)$agreement_id);
		hook_invoke_all('purch_agreement_expired', $hook_data);
	}

	return true;
}

/**
 * Confirm a draft purchase agreement.
 *
 * @param int $agreement_id
 * @return bool
 */
function confirm_agreement($agreement_id)
{
	$agreement = get_purch_agreement($agreement_id);
	if (!$agreement || $agreement['status'] !== 'draft')
		return false;
	if ((int)$agreement['line_count'] <= 0)
		return false;

	return set_purch_agreement_status($agreement_id, 'confirmed');
}

/**
 * Activate a confirmed purchase agreement.
 *
 * @param int $agreement_id
 * @return bool
 */
function activate_agreement($agreement_id)
{
	$agreement = get_purch_agreement($agreement_id);
	if (!$agreement || $agreement['status'] !== 'confirmed')
		return false;
	if ($agreement['date_end'] && $agreement['date_end'] !== '0000-00-00' && $agreement['date_end'] < date2sql(Today()))
		return false;

	return set_purch_agreement_status($agreement_id, 'active');
}

/**
 * Expire an active or confirmed purchase agreement.
 *
 * @param int $agreement_id
 * @return bool
 */
function expire_agreement($agreement_id)
{
	$agreement = get_purch_agreement($agreement_id);
	if (!$agreement || !in_array($agreement['status'], array('confirmed', 'active')))
		return false;

	return set_purch_agreement_status($agreement_id, 'expired');
}

/**
 * Cancel a purchase agreement.
 *
 * @param int    $agreement_id
 * @param string $reason
 * @return bool
 */
function cancel_agreement($agreement_id, $reason)
{
	$agreement = get_purch_agreement($agreement_id);
	if (!$agreement || in_array($agreement['status'], array('expired', 'cancelled')))
		return false;

	return set_purch_agreement_status($agreement_id, 'cancelled', $reason);
}

/**
 * Get agreement lines.
 *
 * @param int $agreement_id
 * @return resource
 */
function get_agreement_lines($agreement_id)
{
	$sql = "SELECT agreement_line.*,
		COALESCE(agreement_line.description, stock.description) AS line_description,
		stock.description AS stock_description,
		stock.units AS stock_units
		FROM " . TB_PREF . "purch_agreement_lines agreement_line
		LEFT JOIN " . TB_PREF . "stock_master stock ON stock.stock_id = agreement_line.stock_id
		WHERE agreement_line.agreement_id = " . (int)$agreement_id . "
		ORDER BY agreement_line.id";

	return db_query($sql, 'could not get purchase agreement lines');
}

/**
 * Get a stock item row for agreement lines without depending on tax-type joins.
 *
 * @param string $stock_id
 * @return array|false
 */
function get_purchase_agreement_item($stock_id)
{
	$item = get_item($stock_id);
	if ($item)
		return $item;

	$sql = "SELECT stock_id, description, units
		FROM " . TB_PREF . "stock_master
		WHERE stock_id = " . db_escape($stock_id);
	$result = db_query($sql, 'could not get purchase agreement item');

	return db_fetch_assoc($result);
}

/**
 * Add an agreement line.
 *
 * @param int    $agreement_id
 * @param string $stock_id
 * @param float  $committed_qty
 * @param float  $unit_price
 * @param float  $discount
 * @param string $description
 * @param float  $min_qty_per_order
 * @param string $price_valid_until
 * @param mixed  $custom_data
 * @return int|false
 */
function add_agreement_line($agreement_id, $stock_id, $committed_qty, $unit_price, $discount = 0,
	$description = '', $min_qty_per_order = 0, $price_valid_until = null, $custom_data = array())
{
	$agreement = get_editable_purch_agreement($agreement_id);
	$item = get_purchase_agreement_item($stock_id);
	if (!$agreement || $committed_qty <= 0 || !$item)
		return false;

	$sql = "SELECT id FROM " . TB_PREF . "purch_agreement_lines
		WHERE agreement_id = " . (int)$agreement_id . "
		AND stock_id = " . db_escape($stock_id);
	$result = db_query($sql, 'could not check duplicate purchase agreement line');
	if (db_num_rows($result) > 0)
		return false;

	$line_description = $description !== '' ? $description : $item['description'];

	$sql = "INSERT INTO " . TB_PREF . "purch_agreement_lines (
		agreement_id,
		stock_id,
		description,
		committed_qty,
		min_qty_per_order,
		unit_price,
		discount_percent,
		price_valid_until,
		custom_data
	) VALUES (
		" . (int)$agreement_id . ",
		" . db_escape($stock_id) . ",
		" . db_escape($line_description) . ",
		" . db_escape($committed_qty) . ",
		" . db_escape(max(0, $min_qty_per_order)) . ",
		" . db_escape($unit_price) . ",
		" . db_escape(max(0, $discount)) . ",
		" . ($price_valid_until ? db_escape($price_valid_until) : 'NULL') . ",
		" . sql_purch_agreement_custom_data($custom_data) . "
	)";
	db_query($sql, 'could not insert purchase agreement line');

	update_purch_agreement_total_committed($agreement_id);
	update_agreement_fulfillment($agreement_id);

	return db_insert_id();
}

/**
 * Update an agreement line.
 *
 * @param int    $line_id
 * @param string $stock_id
 * @param float  $committed_qty
 * @param float  $unit_price
 * @param float  $discount
 * @param string $description
 * @param float  $min_qty_per_order
 * @param string $price_valid_until
 * @param mixed  $custom_data
 * @return bool
 */
function update_agreement_line($line_id, $stock_id, $committed_qty, $unit_price, $discount = 0,
	$description = '', $min_qty_per_order = 0, $price_valid_until = null, $custom_data = array())
{
	$line = get_purch_agreement_line($line_id);
	if (!$line)
		return false;

	$agreement = get_editable_purch_agreement((int)$line['agreement_id']);
	$item = get_purchase_agreement_item($stock_id);
	if (!$agreement || $committed_qty <= 0 || !$item)
		return false;

	$sql = "SELECT id FROM " . TB_PREF . "purch_agreement_lines
		WHERE agreement_id = " . (int)$line['agreement_id'] . "
		AND stock_id = " . db_escape($stock_id) . "
		AND id <> " . (int)$line_id;
	$result = db_query($sql, 'could not check duplicate purchase agreement line on update');
	if (db_num_rows($result) > 0)
		return false;

	$line_description = $description !== '' ? $description : $item['description'];

	$sql = "UPDATE " . TB_PREF . "purch_agreement_lines SET
		stock_id = " . db_escape($stock_id) . ",
		description = " . db_escape($line_description) . ",
		committed_qty = " . db_escape($committed_qty) . ",
		min_qty_per_order = " . db_escape(max(0, $min_qty_per_order)) . ",
		unit_price = " . db_escape($unit_price) . ",
		discount_percent = " . db_escape(max(0, $discount)) . ",
		price_valid_until = " . ($price_valid_until ? db_escape($price_valid_until) : 'NULL') . ",
		custom_data = " . sql_purch_agreement_custom_data($custom_data) . "
		WHERE id = " . (int)$line_id;
	db_query($sql, 'could not update purchase agreement line');

	update_purch_agreement_total_committed((int)$line['agreement_id']);
	update_agreement_fulfillment((int)$line['agreement_id']);

	return true;
}

/**
 * Delete an agreement line.
 *
 * @param int $line_id
 * @return bool
 */
function delete_agreement_line($line_id)
{
	$line = get_purch_agreement_line($line_id);
	if (!$line)
		return false;

	$agreement = get_editable_purch_agreement((int)$line['agreement_id']);
	if (!$agreement)
		return false;

	$sql = "DELETE FROM " . TB_PREF . "purch_agreement_lines WHERE id = " . (int)$line_id;
	db_query($sql, 'could not delete purchase agreement line');

	update_purch_agreement_total_committed((int)$line['agreement_id']);
	update_agreement_fulfillment((int)$line['agreement_id']);

	return true;
}

/**
 * Recalculate line and header fulfillment totals from linked purchase orders.
 *
 * @param int $agreement_id
 * @return void
 */
function update_agreement_fulfillment($agreement_id)
{
	$sql = "SELECT agreement_line.id,
		COALESCE(SUM(po_line.quantity_ordered), 0) AS ordered_qty,
		COALESCE(SUM(po_line.quantity_received), 0) AS received_qty,
		COALESCE(SUM(po_line.qty_invoiced), 0) AS invoiced_qty
		FROM " . TB_PREF . "purch_agreement_lines agreement_line
		LEFT JOIN " . TB_PREF . "purch_orders purchase_order
			ON purchase_order.agreement_id = agreement_line.agreement_id
		LEFT JOIN " . TB_PREF . "purch_order_details po_line
			ON po_line.order_no = purchase_order.order_no
			AND po_line.item_code = agreement_line.stock_id
		WHERE agreement_line.agreement_id = " . (int)$agreement_id . "
		GROUP BY agreement_line.id";
	$result = db_query($sql, 'could not calculate purchase agreement line fulfillment');

	while ($row = db_fetch($result)) {
		$sql = "UPDATE " . TB_PREF . "purch_agreement_lines SET
			ordered_qty = " . db_escape((float)$row['ordered_qty']) . ",
			received_qty = " . db_escape((float)$row['received_qty']) . ",
			invoiced_qty = " . db_escape((float)$row['invoiced_qty']) . "
			WHERE id = " . (int)$row['id'];
		db_query($sql, 'could not update purchase agreement line fulfillment');
	}

	update_purch_agreement_total_committed($agreement_id);

	$sql = "SELECT
		COALESCE(SUM(po_line.quantity_ordered * po_line.unit_price), 0) AS total_ordered,
		COALESCE(SUM(po_line.quantity_received * po_line.unit_price), 0) AS total_received,
		COALESCE(SUM(po_line.qty_invoiced * po_line.unit_price), 0) AS total_invoiced
		FROM " . TB_PREF . "purch_orders purchase_order
		INNER JOIN " . TB_PREF . "purch_order_details po_line ON po_line.order_no = purchase_order.order_no
		WHERE purchase_order.agreement_id = " . (int)$agreement_id;
	$result = db_query($sql, 'could not calculate purchase agreement totals');
	$row = db_fetch($result);

	$sql = "UPDATE " . TB_PREF . "purch_agreements SET
		total_ordered = " . db_escape($row ? (float)$row['total_ordered'] : 0) . ",
		total_received = " . db_escape($row ? (float)$row['total_received'] : 0) . ",
		total_invoiced = " . db_escape($row ? (float)$row['total_invoiced'] : 0) . "
		WHERE id = " . (int)$agreement_id;
	db_query($sql, 'could not update purchase agreement totals');
}

/**
 * Check remaining agreement quantity for one stock item.
 *
 * @param int    $agreement_id
 * @param string $stock_id
 * @param float  $qty
 * @return array
 */
function check_agreement_availability($agreement_id, $stock_id, $qty)
{
	$sql = "SELECT * FROM " . TB_PREF . "purch_agreement_lines
		WHERE agreement_id = " . (int)$agreement_id . "
		AND stock_id = " . db_escape($stock_id);
	$result = db_query($sql, 'could not get purchase agreement availability');
	$line = db_fetch($result);

	if (!$line)
		return array('available' => false, 'remaining_qty' => 0, 'line' => false);

	$remaining_qty = max(0, (float)$line['committed_qty'] - (float)$line['ordered_qty']);
	$requested_qty = (float)$qty;

	return array(
		'available' => $requested_qty <= 0 || $remaining_qty >= $requested_qty,
		'remaining_qty' => $remaining_qty,
		'line' => $line,
	);
}

/**
 * Get the most relevant agreement price row for a supplier and item.
 *
 * @param int    $supplier_id
 * @param string $stock_id
 * @param float  $quantity
 * @param string $date
 * @param int    $agreement_id
 * @return array|false
 */
function get_purch_agreement_price_row($supplier_id, $stock_id, $quantity, $date, $agreement_id = 0)
{
	$effective_date = $date !== '' ? $date : date2sql(Today());

	$sql = "SELECT agreement.id AS agreement_id,
		agreement.reference AS agreement_reference,
		agreement.date_start,
		agreement.date_end,
		agreement.delivery_location,
		agreement_line.id AS agreement_line_id,
		agreement_line.committed_qty,
		agreement_line.ordered_qty,
		agreement_line.min_qty_per_order,
		agreement_line.unit_price,
		agreement_line.discount_percent,
		agreement_line.price_valid_until
		FROM " . TB_PREF . "purch_agreements agreement
		INNER JOIN " . TB_PREF . "purch_agreement_lines agreement_line ON agreement_line.agreement_id = agreement.id
		WHERE agreement.supplier_id = " . (int)$supplier_id . "
		AND agreement_line.stock_id = " . db_escape($stock_id) . "
		AND agreement.status = 'active'
		AND agreement.inactive = 0
		AND agreement.date_start <= " . db_escape($effective_date) . "
		AND (agreement.date_end IS NULL OR agreement.date_end = '0000-00-00' OR agreement.date_end >= " . db_escape($effective_date) . ")
		AND (agreement_line.price_valid_until IS NULL OR agreement_line.price_valid_until = '0000-00-00' OR agreement_line.price_valid_until >= " . db_escape($effective_date) . ")";

	if ((int)$agreement_id > 0)
		$sql .= " AND agreement.id = " . (int)$agreement_id;

	$sql .= " ORDER BY
		CASE WHEN agreement.id = " . (int)$agreement_id . " THEN 0 ELSE 1 END,
		COALESCE(agreement.date_end, '9999-12-31'),
		agreement.id";

	$result = db_query($sql, 'could not get purchase agreement price');
	while ($row = db_fetch($result)) {
		$row['remaining_qty'] = max(0, (float)$row['committed_qty'] - (float)$row['ordered_qty']);
		$row['effective_unit_price'] = round((float)$row['unit_price'] * (1 - ((float)$row['discount_percent'] / 100)), user_price_dec());
		if ($row['remaining_qty'] > 0 || (int)$agreement_id > 0)
			return $row;
	}

	return false;
}

/**
 * Get agreement price for one supplier item combination.
 *
 * @param int    $supplier_id
 * @param string $stock_id
 * @param float  $quantity
 * @param string $date
 * @param int    $agreement_id
 * @return float
 */
function get_agreement_price($supplier_id, $stock_id, $quantity, $date, $agreement_id = 0)
{
	$price_row = get_purch_agreement_price_row($supplier_id, $stock_id, $quantity, $date, $agreement_id);
	return $price_row ? (float)$price_row['effective_unit_price'] : 0;
}

/**
 * Get currently active agreement options for one supplier.
 *
 * @param int    $supplier_id
 * @param string $date
 * @return array
 */
function get_active_purch_agreement_options($supplier_id, $date = '')
{
	$options = array();
	$result = get_purch_agreements((int)$supplier_id, '', true, '', '', '');
	while ($agreement = db_fetch($result))
		$options[(int)$agreement['id']] = $agreement['reference'];

	return $options;
}

/**
 * Create a purchase order from a purchase agreement.
 *
 * @param int   $agreement_id
 * @param array $line_selections
 * @return int|false
 */
function create_po_from_agreement($agreement_id, $line_selections = array())
{
	global $Refs;

	$agreement = get_purch_agreement($agreement_id);
	if (!$agreement || $agreement['status'] !== 'active')
		return false;

	$purchase_order = new purch_order();
	$purchase_order->trans_type = ST_PURCHORDER;
	$purchase_order->orig_order_date = Today();
	$purchase_order->due_date = Today();
	$purchase_order->Location = $agreement['delivery_location'];
	$purchase_order->Comments = sprintf(_('Created from Purchase Agreement %s'), $agreement['reference']);
	$purchase_order->delivery_address = $agreement['delivery_location_name'] ? $agreement['delivery_location_name'] : $agreement['delivery_location'];

	get_supplier_details_to_order($purchase_order, (int)$agreement['supplier_id']);
	$purchase_order->reference = $Refs->get_next(ST_PURCHORDER, null, array(
		'supplier_id' => (int)$agreement['supplier_id'],
		'date' => $purchase_order->orig_order_date,
		'location' => $purchase_order->Location,
	));
	$purchase_order->agreement_id = (int)$agreement_id;
	$purchase_order->rfq_id = (int)$agreement['rfq_id'];
	$purchase_order->dimension = (int)$agreement['dimension_id'];
	$purchase_order->dimension2 = (int)$agreement['dimension2_id'];

	$agreement_lines = get_agreement_lines($agreement_id);
	while ($agreement_line = db_fetch($agreement_lines)) {
		$line_id = (int)$agreement_line['id'];
		$selection = isset($line_selections[$line_id]) ? $line_selections[$line_id] : array();
		$selected_quantity = 0;
		$selected_delivery_date = '';

		if (is_array($selection)) {
			if (!empty($selection['skip']))
				continue;
			$selected_quantity = isset($selection['quantity']) ? (float)$selection['quantity'] : 0;
			$selected_delivery_date = isset($selection['delivery_date']) ? $selection['delivery_date'] : '';
		} elseif ($selection !== null && $selection !== '') {
			$selected_quantity = (float)$selection;
		}

		$remaining_qty = max(0, (float)$agreement_line['committed_qty'] - (float)$agreement_line['ordered_qty']);
		if ($selected_quantity <= 0)
			$selected_quantity = $remaining_qty > 0 ? $remaining_qty : 0;
		if ($selected_quantity <= 0)
			continue;
		if ($remaining_qty > 0)
			$selected_quantity = min($selected_quantity, $remaining_qty);
		if ($selected_quantity <= 0)
			continue;

		if ((float)$agreement_line['min_qty_per_order'] > 0 && $selected_quantity < (float)$agreement_line['min_qty_per_order'])
			$selected_quantity = (float)$agreement_line['min_qty_per_order'];

		$effective_price = round((float)$agreement_line['unit_price'] * (1 - ((float)$agreement_line['discount_percent'] / 100)), user_price_dec());
		$line_delivery_date = $selected_delivery_date !== '' ? $selected_delivery_date : Today();

		$purchase_order->add_to_order(
			count($purchase_order->line_items),
			$agreement_line['stock_id'],
			$selected_quantity,
			$agreement_line['line_description'] !== '' ? $agreement_line['line_description'] : null,
			$effective_price,
			$agreement_line['stock_units'],
			$line_delivery_date,
			0,
			0
		);
	}

	if (!$purchase_order->order_has_items())
		return false;

	$po_number = add_po($purchase_order);
	if (!$po_number)
		return false;

	$sql = "UPDATE " . TB_PREF . "purch_orders SET
		agreement_id = " . (int)$agreement_id . ",
		rfq_id = " . (int)$agreement['rfq_id'] . "
		WHERE order_no = " . (int)$po_number;
	db_query($sql, 'could not link purchase order to agreement');

	update_agreement_fulfillment($agreement_id);

	$hook_data = array(
		'agreement_id' => (int)$agreement_id,
		'po_no' => (int)$po_number,
	);
	hook_invoke_all('purch_agreement_po_created', $hook_data);

	return (int)$po_number;
}

/**
 * Get agreements expiring within the configured lead time.
 *
 * @param int $days_ahead
 * @return resource
 */
function get_expiring_agreements($days_ahead = 30)
{
	$target_date = date('Y-m-d', strtotime(date2sql(Today()) . ' +' . max(0, (int)$days_ahead) . ' days'));
	$sql = "SELECT agreement.*,
		supplier.supp_name
		FROM " . TB_PREF . "purch_agreements agreement
		INNER JOIN " . TB_PREF . "suppliers supplier ON supplier.supplier_id = agreement.supplier_id
		WHERE agreement.status IN ('confirmed', 'active')
		AND agreement.date_end IS NOT NULL
		AND agreement.date_end <> '0000-00-00'
		AND agreement.date_end BETWEEN " . db_escape(date2sql(Today())) . " AND " . db_escape($target_date) . "
		ORDER BY agreement.date_end, agreement.id";

	return db_query($sql, 'could not get expiring purchase agreements');
}

/**
 * Auto-renew eligible purchase agreements.
 *
 * @return array
 */
function auto_renew_agreements()
{
	$new_agreement_ids = array();
	$sql = "SELECT *
		FROM " . TB_PREF . "purch_agreements
		WHERE auto_renew = 1
		AND status IN ('confirmed', 'active')
		AND date_end IS NOT NULL
		AND date_end <> '0000-00-00'
		AND date_end < " . db_escape(date2sql(Today()));
	$result = db_query($sql, 'could not get auto-renew purchase agreements');

	while ($agreement = db_fetch($result)) {
		$custom_data = decode_purch_agreement_custom_data($agreement['custom_data']);
		if (!empty($custom_data['renewed_to_agreement_id']))
			continue;

		$months = max(1, (int)$agreement['renewal_period_months']);
		$new_start = date('Y-m-d', strtotime($agreement['date_start'] . ' +' . $months . ' months'));
		$new_end = $agreement['date_end'] ? date('Y-m-d', strtotime($agreement['date_end'] . ' +' . $months . ' months')) : null;
		$new_custom_data = $custom_data;
		$new_custom_data['renewed_from_agreement_id'] = (int)$agreement['id'];

		$new_agreement_id = add_purch_agreement(
			$agreement['agreement_type'],
			(int)$agreement['supplier_id'],
			$new_start,
			$new_end,
			$agreement['currency'],
			(int)$agreement['payment_terms'],
			$agreement['delivery_location'],
			1,
			$months,
			$agreement['terms_and_conditions'],
			$agreement['notes'],
			(int)$agreement['rfq_id'],
			(int)$agreement['dimension_id'],
			(int)$agreement['dimension2_id'],
			'',
			(int)$agreement['buyer_id'],
			$new_custom_data
		);
		if (!$new_agreement_id)
			continue;

		$lines = get_agreement_lines((int)$agreement['id']);
		while ($line = db_fetch($lines)) {
			add_agreement_line(
				$new_agreement_id,
				$line['stock_id'],
				(float)$line['committed_qty'],
				(float)$line['unit_price'],
				(float)$line['discount_percent'],
				$line['line_description'],
				(float)$line['min_qty_per_order'],
				$line['price_valid_until'],
				decode_purch_agreement_custom_data($line['custom_data'])
			);
		}

		confirm_agreement($new_agreement_id);
		if ($new_start <= date2sql(Today()))
			activate_agreement($new_agreement_id);

		$custom_data['renewed_to_agreement_id'] = (int)$new_agreement_id;
		$sql = "UPDATE " . TB_PREF . "purch_agreements SET
			custom_data = " . sql_purch_agreement_custom_data($custom_data) . ",
			status = 'expired',
			inactive = 1
			WHERE id = " . (int)$agreement['id'];
		db_query($sql, 'could not mark renewed purchase agreement as expired');

		$hook_data = array(
			'old_id' => (int)$agreement['id'],
			'new_id' => (int)$new_agreement_id,
		);
		hook_invoke_all('purch_agreement_renewed', $hook_data);

		$new_agreement_ids[] = (int)$new_agreement_id;
	}

	return $new_agreement_ids;
}