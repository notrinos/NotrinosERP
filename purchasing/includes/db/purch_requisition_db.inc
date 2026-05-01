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
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_material_requests_db.inc');
include_once($path_to_root . '/purchasing/includes/po_class.inc');
include_once($path_to_root . '/purchasing/includes/db/po_db.inc');
include_once($path_to_root . '/purchasing/includes/db/suppliers_db.inc');

/**
 * Get available purchase requisition statuses.
 *
 * @return array
 */
function get_purch_requisition_statuses()
{
	return array(
		'draft' => _('Draft'),
		'submitted' => _('Submitted'),
		'approved' => _('Approved'),
		'partially_ordered' => _('Partially Ordered'),
		'ordered' => _('Ordered'),
		'rejected' => _('Rejected'),
		'cancelled' => _('Cancelled'),
	);
}

/**
 * Get available purchase requisition priorities.
 *
 * @return array
 */
function get_purch_requisition_priorities()
{
	return array(
		'low' => _('Low'),
		'normal' => _('Normal'),
		'high' => _('High'),
		'urgent' => _('Urgent'),
	);
}

/**
 * Get the badge color for one requisition status.
 *
 * @param string $status
 * @return string
 */
function get_purch_requisition_status_color($status)
{
	$colors = array(
		'draft' => '#6c757d',
		'submitted' => '#17a2b8',
		'approved' => '#007bff',
		'partially_ordered' => '#fd7e14',
		'ordered' => '#28a745',
		'rejected' => '#dc3545',
		'cancelled' => '#343a40',
	);

	return isset($colors[$status]) ? $colors[$status] : '#6c757d';
}

/**
 * Get the badge color for one requisition priority.
 *
 * @param string $priority
 * @return string
 */
function get_purch_requisition_priority_color($priority)
{
	$colors = array(
		'low' => '#6c757d',
		'normal' => '#0d6efd',
		'high' => '#fd7e14',
		'urgent' => '#dc3545',
	);

	return isset($colors[$priority]) ? $colors[$priority] : '#0d6efd';
}

/**
 * Render a status badge for one purchase requisition.
 *
 * @param string $status
 * @return string
 */
function purch_requisition_status_badge($status)
{
	$statuses = get_purch_requisition_statuses();
	$label = isset($statuses[$status]) ? $statuses[$status] : $status;
	$color = get_purch_requisition_status_color($status);

	return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;font-size:11px;'
		. 'font-weight:bold;color:#fff;background:' . $color . ';">' . $label . '</span>';
}

/**
 * Render a priority badge for one purchase requisition.
 *
 * @param string $priority
 * @return string
 */
function purch_requisition_priority_badge($priority)
{
	$priorities = get_purch_requisition_priorities();
	$label = isset($priorities[$priority]) ? $priorities[$priority] : $priority;
	$color = get_purch_requisition_priority_color($priority);

	return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;font-size:11px;'
		. 'font-weight:bold;color:#fff;background:' . $color . ';">' . $label . '</span>';
}

/**
 * Format a purchase requisition reference.
 *
 * @param int $requisition_id
 * @return string
 */
function format_purch_requisition_reference($requisition_id)
{
	return 'PR-' . str_pad((int)$requisition_id, 4, '0', STR_PAD_LEFT);
}

/**
 * Encode custom data for storage.
 *
 * @param mixed $custom_data
 * @return string|null
 */
function encode_purch_requisition_custom_data($custom_data)
{
	if ($custom_data === null || $custom_data === '' || $custom_data === array())
		return null;

	if (is_string($custom_data))
		return $custom_data;

	$encoded = json_encode($custom_data);
	return $encoded === false ? null : $encoded;
}

/**
 * Decode custom data from storage.
 *
 * @param string|null $custom_data
 * @return array
 */
function decode_purch_requisition_custom_data($custom_data)
{
	if (!$custom_data)
		return array();

	$decoded = json_decode($custom_data, true);
	return is_array($decoded) ? $decoded : array();
}

/**
 * Get allowed editable statuses.
 *
 * @return array
 */
function get_purch_requisition_editable_statuses()
{
	return array('draft', 'rejected');
}

/**
 * Get the current editable requisition row.
 *
 * @param int $requisition_id
 * @return array|false
 */
function get_editable_purch_requisition($requisition_id)
{
	$requisition = get_purch_requisition($requisition_id);
	if (!$requisition)
		return false;

	return in_array($requisition['status'], get_purch_requisition_editable_statuses())
		? $requisition
		: false;
}

/**
 * Get a default supplier and price for one item from purchase pricing.
 *
 * @param string $stock_id
 * @return array
 */
function get_default_requisition_supplier_data($stock_id)
{
	$sql = "SELECT supplier_id, price, conversion_factor
		FROM " . TB_PREF . "purch_data
		WHERE stock_id = " . db_escape($stock_id) . "
		ORDER BY supplier_id";
	$result = db_query($sql, 'could not get default purchase requisition supplier data');
	$row = db_fetch($result);

	if (!$row)
		return array('supplier_id' => 0, 'price' => 0);

	$conversion_factor = (float)$row['conversion_factor'] > 0 ? (float)$row['conversion_factor'] : 1;

	return array(
		'supplier_id' => (int)$row['supplier_id'],
		'price' => round((float)$row['price'] / $conversion_factor, user_price_dec()),
	);
}

/**
 * Map a material request status to a purchase requisition status.
 *
 * @param string $material_request_status
 * @return string
 */
function map_material_request_status_to_purch_requisition_status($material_request_status)
{
	switch ($material_request_status) {
		case 'submitted':
			return 'submitted';
		case 'approved':
			return 'approved';
		case 'ordered':
		case 'fulfilled':
			return 'ordered';
		case 'cancelled':
			return 'cancelled';
		default:
			return 'draft';
	}
}

/**
 * Update the requisition total estimated amount from line items.
 *
 * @param int $requisition_id
 * @return void
 */
function update_purch_requisition_total($requisition_id)
{
	$sql = "SELECT COALESCE(SUM(quantity * estimated_unit_price), 0) AS total_estimated
		FROM " . TB_PREF . "purch_requisition_lines
		WHERE requisition_id = " . (int)$requisition_id;
	$result = db_query($sql, 'could not calculate purchase requisition total');
	$row = db_fetch($result);

	$sql = "UPDATE " . TB_PREF . "purch_requisitions SET
		total_estimated = " . db_escape((float)$row['total_estimated']) . "
		WHERE id = " . (int)$requisition_id;
	db_query($sql, 'could not update purchase requisition total');
}

/**
 * Sync requisition header status from line ordering progress.
 *
 * @param int $requisition_id
 * @return void
 */
function update_purch_requisition_order_status($requisition_id)
{
	$requisition = get_purch_requisition($requisition_id);
	if (!$requisition || !in_array($requisition['status'], array('approved', 'partially_ordered', 'ordered')))
		return;

	$sql = "SELECT
		COUNT(*) AS total_lines,
		SUM(CASE WHEN qty_ordered > 0 THEN 1 ELSE 0 END) AS ordered_lines,
		SUM(CASE WHEN qty_ordered >= quantity THEN 1 ELSE 0 END) AS fully_ordered_lines
		FROM " . TB_PREF . "purch_requisition_lines
		WHERE requisition_id = " . (int)$requisition_id;
	$result = db_query($sql, 'could not calculate purchase requisition order status');
	$row = db_fetch($result);

	$status = 'approved';
	if ((int)$row['total_lines'] > 0 && (int)$row['fully_ordered_lines'] === (int)$row['total_lines'])
		$status = 'ordered';
	elseif ((int)$row['ordered_lines'] > 0)
		$status = 'partially_ordered';

	$sql = "UPDATE " . TB_PREF . "purch_requisitions SET
		status = " . db_escape($status) . "
		WHERE id = " . (int)$requisition_id;
	db_query($sql, 'could not update purchase requisition order status');
}

/**
 * Return a submitted requisition to draft.
 *
 * @param int $requisition_id
 * @return bool
 */
function reopen_purch_requisition($requisition_id)
{
	$requisition = get_purch_requisition($requisition_id);
	if (!$requisition || !in_array($requisition['status'], array('submitted', 'rejected')))
		return false;

	begin_transaction();

	if (!empty($requisition['material_request_id'])) {
		$sql = "UPDATE " . TB_PREF . "wh_material_requests SET
			status = 'draft',
			updated_at = NOW()
			WHERE request_id = " . (int)$requisition['material_request_id'];
		db_query($sql, 'could not reopen linked material request');
	}

	$sql = "UPDATE " . TB_PREF . "purch_requisitions SET
		status = 'draft',
		approved_by = 0,
		approved_date = NULL,
		rejected_by = 0,
		rejected_date = NULL,
		rejection_reason = NULL
		WHERE id = " . (int)$requisition_id;
	db_query($sql, 'could not reopen purchase requisition');

	$sql = "UPDATE " . TB_PREF . "purch_requisition_lines SET
		status = 'pending'
		WHERE requisition_id = " . (int)$requisition_id
		. " AND status <> 'ordered'";
	db_query($sql, 'could not reopen purchase requisition lines');

	commit_transaction();
	return true;
}

/**
 * Build requisition draft data for the approval workflow.
 *
 * @param int $requisition_id
 * @return array
 */
function get_purch_requisition_submission_data($requisition_id)
{
	$requisition = get_purch_requisition($requisition_id);
	if (!$requisition)
		return array();

	$data = array(
		'requisition_id' => (int)$requisition['id'],
		'reference' => $requisition['reference'],
		'material_request_id' => (int)$requisition['material_request_id'],
		'requester_id' => (int)$requisition['requester_id'],
		'requester_name' => isset($requisition['requester_name']) ? $requisition['requester_name'] : '',
		'department_id' => (int)$requisition['department_id'],
		'department_name' => isset($requisition['department_name']) ? $requisition['department_name'] : '',
		'request_date' => $requisition['request_date'],
		'required_date' => $requisition['required_date'],
		'status' => $requisition['status'],
		'priority' => $requisition['priority'],
		'purpose' => $requisition['purpose'],
		'notes' => $requisition['notes'],
		'total_estimated' => (float)$requisition['total_estimated'],
		'location' => $requisition['location'],
		'location_name' => isset($requisition['location_name']) ? $requisition['location_name'] : '',
		'dimension_id' => (int)$requisition['dimension_id'],
		'dimension2_id' => (int)$requisition['dimension2_id'],
		'custom_data' => decode_purch_requisition_custom_data($requisition['custom_data']),
		'line_items' => array(),
	);

	$lines = get_requisition_lines($requisition_id);
	while ($line = db_fetch($lines)) {
		$data['line_items'][] = array(
			'id' => (int)$line['id'],
			'stock_id' => $line['stock_id'],
			'description' => $line['line_description'],
			'quantity' => (float)$line['quantity'],
			'unit_of_measure' => $line['line_unit_of_measure'],
			'estimated_unit_price' => (float)$line['estimated_unit_price'],
			'preferred_supplier_id' => (int)$line['preferred_supplier_id'],
			'preferred_supplier_name' => isset($line['preferred_supplier_name']) ? $line['preferred_supplier_name'] : '',
			'notes' => $line['notes'],
			'qty_ordered' => (float)$line['qty_ordered'],
			'status' => $line['status'],
		);
	}

	return $data;
}

/**
 * Get the requisition header row.
 *
 * @param int $requisition_id
 * @return array|false
 */
function get_purch_requisition_header($requisition_id)
{
	return get_purch_requisition($requisition_id);
}

/**
 * Get one purchase requisition.
 *
 * @param int $requisition_id
 * @return array|false
 */
function get_purch_requisition($requisition_id)
{
	$sql = "SELECT pr.*,
		requester.real_name AS requester_name,
		department.department_name,
		approver.real_name AS approved_by_name,
		rejector.real_name AS rejected_by_name,
		location.location_name,
		mr.request_no AS material_request_no,
		mr.status AS material_request_status,
		mr.linked_doc_type,
		mr.linked_doc_no
		FROM " . TB_PREF . "purch_requisitions pr
		LEFT JOIN " . TB_PREF . "users requester ON requester.id = pr.requester_id
		LEFT JOIN " . TB_PREF . "departments department ON department.department_id = pr.department_id
		LEFT JOIN " . TB_PREF . "users approver ON approver.id = pr.approved_by
		LEFT JOIN " . TB_PREF . "users rejector ON rejector.id = pr.rejected_by
		LEFT JOIN " . TB_PREF . "locations location ON location.loc_code = pr.location
		LEFT JOIN " . TB_PREF . "wh_material_requests mr ON mr.request_id = pr.material_request_id
		WHERE pr.id = " . (int)$requisition_id;
	$result = db_query($sql, 'could not get purchase requisition');
	return db_fetch($result);
}

/**
 * Get purchase requisitions by filter.
 *
 * @param int    $requester_id
 * @param string $status
 * @param string $date_from
 * @param string $date_to
 * @param int    $department_id
 * @return resource
 */
function get_purch_requisitions($requester_id = 0, $status = '', $date_from = '', $date_to = '', $department_id = 0)
{
	$sql = "SELECT pr.*,
		requester.real_name AS requester_name,
		department.department_name,
		location.location_name,
		COUNT(prl.id) AS line_count,
		COALESCE(SUM(prl.quantity - prl.qty_ordered), 0) AS outstanding_quantity
		FROM " . TB_PREF . "purch_requisitions pr
		LEFT JOIN " . TB_PREF . "users requester ON requester.id = pr.requester_id
		LEFT JOIN " . TB_PREF . "departments department ON department.department_id = pr.department_id
		LEFT JOIN " . TB_PREF . "locations location ON location.loc_code = pr.location
		LEFT JOIN " . TB_PREF . "purch_requisition_lines prl ON prl.requisition_id = pr.id
		WHERE 1 = 1";

	if ($requester_id > 0)
		$sql .= " AND pr.requester_id = " . (int)$requester_id;
	if ($department_id > 0)
		$sql .= " AND pr.department_id = " . (int)$department_id;
	if ($status !== '')
		$sql .= " AND pr.status = " . db_escape($status);
	if ($date_from !== '')
		$sql .= " AND pr.request_date >= " . db_escape($date_from);
	if ($date_to !== '')
		$sql .= " AND pr.request_date <= " . db_escape($date_to);

	$sql .= " GROUP BY pr.id ORDER BY pr.id DESC";
	return db_query($sql, 'could not get purchase requisitions');
}

/**
 * Add a purchase requisition header.
 *
 * @param int         $requester_id
 * @param int         $department_id
 * @param string|null $required_date
 * @param string      $purpose
 * @param string      $location
 * @param string      $priority
 * @param string      $notes
 * @param int         $dimension_id
 * @param int         $dimension2_id
 * @param string      $reference
 * @param string|null $request_date
 * @param int         $material_request_id
 * @param mixed       $custom_data
 * @return int
 */
function add_purch_requisition($requester_id, $department_id, $required_date, $purpose, $location,
	$priority = 'normal', $notes = '', $dimension_id = 0, $dimension2_id = 0,
	$reference = '', $request_date = null, $material_request_id = 0, $custom_data = array())
{
	$request_date = $request_date ? $request_date : date2sql(Today());
	$requester_id = $requester_id ? (int)$requester_id : (int)$_SESSION['wa_current_user']->user;

	$hook_data = array(
		'requester_id' => $requester_id,
		'department_id' => (int)$department_id,
		'required_date' => $required_date,
		'purpose' => $purpose,
		'location' => $location,
		'priority' => $priority,
		'notes' => $notes,
		'dimension_id' => (int)$dimension_id,
		'dimension2_id' => (int)$dimension2_id,
		'reference' => $reference,
		'request_date' => $request_date,
		'material_request_id' => (int)$material_request_id,
		'custom_data' => $custom_data,
	);
	hook_invoke_all('purch_requisition_before_save', $hook_data);

	begin_transaction();

	if ((int)$hook_data['material_request_id'] > 0) {
		$material_request_id = (int)$hook_data['material_request_id'];
	} else {
		$material_request_id = add_material_request(
			'purchase',
			$hook_data['location'],
			$hook_data['request_date'],
			$hook_data['required_date'],
			$hook_data['reference'],
			$hook_data['notes']
		);
	}

	$sql = "INSERT INTO " . TB_PREF . "purch_requisitions (
		reference,
		material_request_id,
		requester_id,
		department_id,
		request_date,
		required_date,
		status,
		priority,
		purpose,
		notes,
		total_estimated,
		location,
		dimension_id,
		dimension2_id,
		custom_data
	) VALUES (
		'',
		" . (int)$material_request_id . ",
		" . (int)$hook_data['requester_id'] . ",
		" . (int)$hook_data['department_id'] . ",
		" . db_escape($hook_data['request_date']) . ",
		" . ($hook_data['required_date'] ? db_escape($hook_data['required_date']) : 'NULL') . ",
		'draft',
		" . db_escape($hook_data['priority']) . ",
		" . ($hook_data['purpose'] !== '' ? db_escape($hook_data['purpose']) : 'NULL') . ",
		" . ($hook_data['notes'] !== '' ? db_escape($hook_data['notes']) : 'NULL') . ",
		0,
		" . db_escape($hook_data['location']) . ",
		" . (int)$hook_data['dimension_id'] . ",
		" . (int)$hook_data['dimension2_id'] . ",
		" . ($hook_data['custom_data'] ? db_escape(encode_purch_requisition_custom_data($hook_data['custom_data'])) : 'NULL') . "
	)";
	db_query($sql, 'could not insert purchase requisition');

	$requisition_id = db_insert_id();
	$generated_reference = $hook_data['reference'] !== '' ? $hook_data['reference'] : format_purch_requisition_reference($requisition_id);

	$sql = "UPDATE " . TB_PREF . "purch_requisitions SET
		reference = " . db_escape($generated_reference) . "
		WHERE id = " . (int)$requisition_id;
	db_query($sql, 'could not update purchase requisition reference');

	commit_transaction();
	return $requisition_id;
}

/**
 * Update a purchase requisition header.
 *
 * @param int         $requisition_id
 * @param int         $department_id
 * @param string|null $required_date
 * @param string      $purpose
 * @param string      $location
 * @param string      $priority
 * @param string      $notes
 * @param int         $dimension_id
 * @param int         $dimension2_id
 * @param string      $reference
 * @param string|null $request_date
 * @param mixed       $custom_data
 * @return bool
 */
function update_purch_requisition($requisition_id, $department_id, $required_date, $purpose, $location,
	$priority = 'normal', $notes = '', $dimension_id = 0, $dimension2_id = 0,
	$reference = '', $request_date = null, $custom_data = array())
{
	$requisition = get_editable_purch_requisition($requisition_id);
	if (!$requisition)
		return false;

	$request_date = $request_date ? $request_date : $requisition['request_date'];
	$reference = $reference !== '' ? $reference : $requisition['reference'];

	$hook_data = array(
		'id' => (int)$requisition_id,
		'department_id' => (int)$department_id,
		'required_date' => $required_date,
		'purpose' => $purpose,
		'location' => $location,
		'priority' => $priority,
		'notes' => $notes,
		'dimension_id' => (int)$dimension_id,
		'dimension2_id' => (int)$dimension2_id,
		'reference' => $reference,
		'request_date' => $request_date,
		'custom_data' => $custom_data,
	);
	hook_invoke_all('purch_requisition_before_save', $hook_data);

	begin_transaction();

	$sql = "UPDATE " . TB_PREF . "purch_requisitions SET
		department_id = " . (int)$hook_data['department_id'] . ",
		request_date = " . db_escape($hook_data['request_date']) . ",
		required_date = " . ($hook_data['required_date'] ? db_escape($hook_data['required_date']) : 'NULL') . ",
		priority = " . db_escape($hook_data['priority']) . ",
		purpose = " . ($hook_data['purpose'] !== '' ? db_escape($hook_data['purpose']) : 'NULL') . ",
		notes = " . ($hook_data['notes'] !== '' ? db_escape($hook_data['notes']) : 'NULL') . ",
		location = " . db_escape($hook_data['location']) . ",
		dimension_id = " . (int)$hook_data['dimension_id'] . ",
		dimension2_id = " . (int)$hook_data['dimension2_id'] . ",
		reference = " . db_escape($hook_data['reference']) . ",
		custom_data = " . ($hook_data['custom_data'] ? db_escape(encode_purch_requisition_custom_data($hook_data['custom_data'])) : 'NULL') . "
		WHERE id = " . (int)$requisition_id;
	db_query($sql, 'could not update purchase requisition');

	update_material_request(
		(int)$requisition['material_request_id'],
		'purchase',
		$hook_data['location'],
		$hook_data['request_date'],
		$hook_data['required_date'],
		$hook_data['reference'],
		$hook_data['notes']
	);

	$sql = "UPDATE " . TB_PREF . "wh_material_request_lines SET
		required_date = " . ($hook_data['required_date'] ? db_escape($hook_data['required_date']) : 'NULL') . "
		WHERE request_id = " . (int)$requisition['material_request_id'];
	db_query($sql, 'could not sync material request line dates');

	commit_transaction();
	return true;
}

/**
 * Delete a purchase requisition.
 *
 * @param int $requisition_id
 * @return bool
 */
function delete_purch_requisition($requisition_id)
{
	$requisition = get_purch_requisition($requisition_id);
	if (!$requisition || $requisition['status'] !== 'draft')
		return false;

	begin_transaction();

	$sql = "DELETE FROM " . TB_PREF . "purch_requisitions WHERE id = " . (int)$requisition_id;
	db_query($sql, 'could not delete purchase requisition');

	delete_material_request((int)$requisition['material_request_id']);

	commit_transaction();
	return true;
}

/**
 * Add a requisition line.
 *
 * @param int         $requisition_id
 * @param string      $stock_id
 * @param float       $quantity
 * @param float       $estimated_price
 * @param int         $preferred_supplier_id
 * @param string      $description
 * @param string      $unit_of_measure
 * @param string      $notes
 * @param mixed       $custom_data
 * @param string|null $error_message
 * @return int|false
 */
function add_requisition_line($requisition_id, $stock_id, $quantity, $estimated_price, $preferred_supplier_id,
	$description = '', $unit_of_measure = '', $notes = '', $custom_data = array(), &$error_message = null)
{
	$error_message = null;

	$requisition = get_editable_purch_requisition($requisition_id);
	if (!$requisition) {
		$existing_requisition = get_purch_requisition($requisition_id);
		if (!$existing_requisition)
			$error_message = _('The selected purchase requisition could not be found.');
		else
			$error_message = _('Lines can only be added while the purchase requisition is in Draft or Rejected status.');
		return false;
	}

	$item = get_item($stock_id);
	if (!$item) {
		$error_message = _('The selected item could not be found.');
		return false;
	}

	if ($quantity <= 0) {
		$error_message = _('The quantity must be greater than zero.');
		return false;
	}

	$line_description = $description !== '' ? $description : $item['description'];
	$line_unit = $unit_of_measure !== '' ? $unit_of_measure : $item['units'];

	begin_transaction();

	$material_request_line_id = add_material_request_line(
		(int)$requisition['material_request_id'],
		$stock_id,
		$quantity,
		$requisition['required_date'],
		$notes
	);

	$sql = "INSERT INTO " . TB_PREF . "purch_requisition_lines (
		requisition_id,
		material_request_line_id,
		stock_id,
		description,
		quantity,
		unit_of_measure,
		estimated_unit_price,
		preferred_supplier_id,
		notes,
		qty_ordered,
		po_number,
		status,
		custom_data
	) VALUES (
		" . (int)$requisition_id . ",
		" . (int)$material_request_line_id . ",
		" . db_escape($stock_id) . ",
		" . db_escape($line_description) . ",
		" . db_escape($quantity) . ",
		" . db_escape($line_unit) . ",
		" . db_escape($estimated_price) . ",
		" . (int)$preferred_supplier_id . ",
		" . ($notes !== '' ? db_escape($notes) : 'NULL') . ",
		0,
		0,
		'pending',
		" . ($custom_data ? db_escape(encode_purch_requisition_custom_data($custom_data)) : 'NULL') . "
	)";
	db_query($sql, 'could not insert purchase requisition line');

	$line_id = db_insert_id();
	update_purch_requisition_total($requisition_id);

	commit_transaction();
	return $line_id;
}

/**
 * Update a requisition line.
 *
 * @param int         $line_id
 * @param string      $stock_id
 * @param float       $quantity
 * @param float       $estimated_price
 * @param int         $preferred_supplier_id
 * @param string      $description
 * @param string      $unit_of_measure
 * @param string      $notes
 * @param mixed       $custom_data
 * @return bool
 */
function update_requisition_line($line_id, $stock_id, $quantity, $estimated_price, $preferred_supplier_id,
	$description = '', $unit_of_measure = '', $notes = '', $custom_data = array())
{
	$sql = "SELECT * FROM " . TB_PREF . "purch_requisition_lines WHERE id = " . (int)$line_id;
	$result = db_query($sql, 'could not get purchase requisition line');
	$line = db_fetch($result);
	if (!$line)
		return false;

	$requisition = get_editable_purch_requisition((int)$line['requisition_id']);
	if (!$requisition || $quantity <= 0)
		return false;

	$item = get_item($stock_id);
	if (!$item)
		return false;

	$line_description = $description !== '' ? $description : $item['description'];
	$line_unit = $unit_of_measure !== '' ? $unit_of_measure : $item['units'];

	begin_transaction();

	$sql = "UPDATE " . TB_PREF . "purch_requisition_lines SET
		stock_id = " . db_escape($stock_id) . ",
		description = " . db_escape($line_description) . ",
		quantity = " . db_escape($quantity) . ",
		unit_of_measure = " . db_escape($line_unit) . ",
		estimated_unit_price = " . db_escape($estimated_price) . ",
		preferred_supplier_id = " . (int)$preferred_supplier_id . ",
		notes = " . ($notes !== '' ? db_escape($notes) : 'NULL') . ",
		custom_data = " . ($custom_data ? db_escape(encode_purch_requisition_custom_data($custom_data)) : 'NULL') . "
		WHERE id = " . (int)$line_id;
	db_query($sql, 'could not update purchase requisition line');

	$sql = "UPDATE " . TB_PREF . "wh_material_request_lines SET
		stock_id = " . db_escape($stock_id) . ",
		qty_requested = " . db_escape($quantity) . ",
		required_date = " . ($requisition['required_date'] ? db_escape($requisition['required_date']) : 'NULL') . ",
		memo = " . ($notes !== '' ? db_escape($notes) : 'NULL') . "
		WHERE line_id = " . (int)$line['material_request_line_id'];
	db_query($sql, 'could not sync purchase requisition line to material request');

	update_purch_requisition_total((int)$line['requisition_id']);

	commit_transaction();
	return true;
}

/**
 * Delete a requisition line.
 *
 * @param int $line_id
 * @return bool
 */
function delete_requisition_line($line_id)
{
	$sql = "SELECT * FROM " . TB_PREF . "purch_requisition_lines WHERE id = " . (int)$line_id;
	$result = db_query($sql, 'could not get purchase requisition line for delete');
	$line = db_fetch($result);
	if (!$line)
		return false;

	$requisition = get_editable_purch_requisition((int)$line['requisition_id']);
	if (!$requisition)
		return false;

	begin_transaction();

	$sql = "DELETE FROM " . TB_PREF . "purch_requisition_lines WHERE id = " . (int)$line_id;
	db_query($sql, 'could not delete purchase requisition line');

	if ((int)$line['material_request_line_id'] > 0)
		delete_material_request_line((int)$line['material_request_line_id']);

	update_purch_requisition_total((int)$line['requisition_id']);

	commit_transaction();
	return true;
}

/**
 * Get requisition lines.
 *
 * @param int $requisition_id
 * @return resource
 */
function get_requisition_lines($requisition_id)
{
	$sql = "SELECT prl.*,
		COALESCE(prl.description, sm.description) AS line_description,
		COALESCE(NULLIF(prl.unit_of_measure, ''), sm.units) AS line_unit_of_measure,
		sm.description AS stock_description,
		sm.units AS stock_units,
		supplier.supp_name AS preferred_supplier_name
		FROM " . TB_PREF . "purch_requisition_lines prl
		LEFT JOIN " . TB_PREF . "stock_master sm ON sm.stock_id = prl.stock_id
		LEFT JOIN " . TB_PREF . "suppliers supplier ON supplier.supplier_id = prl.preferred_supplier_id
		WHERE prl.requisition_id = " . (int)$requisition_id
		. " ORDER BY prl.id";
	return db_query($sql, 'could not get purchase requisition lines');
}

/**
 * Submit a requisition.
 *
 * @param int $requisition_id
 * @return array|bool
 */
function submit_requisition($requisition_id)
{
	$requisition = get_purch_requisition($requisition_id);
	if (!$requisition || !in_array($requisition['status'], array('draft', 'rejected')))
		return false;

	if (count_material_request_lines((int)$requisition['material_request_id']) === 0)
		return false;

	begin_transaction();

	if (!$requisition['material_request_status'] || $requisition['material_request_status'] === 'draft') {
		if (!submit_material_request((int)$requisition['material_request_id'])) {
			cancel_transaction();
			return false;
		}
	}

	$sql = "UPDATE " . TB_PREF . "purch_requisitions SET
		status = 'submitted',
		rejected_by = 0,
		rejected_date = NULL,
		rejection_reason = NULL
		WHERE id = " . (int)$requisition_id;
	db_query($sql, 'could not submit purchase requisition');

	$sql = "UPDATE " . TB_PREF . "purch_requisition_lines SET
		status = 'pending'
		WHERE requisition_id = " . (int)$requisition_id
		. " AND status <> 'ordered'";
	db_query($sql, 'could not set requisition lines to pending');

	commit_transaction();

	$submitted_requisition_id = (int)$requisition_id;
	hook_invoke_all('purch_requisition_submitted', $submitted_requisition_id);

	$reloaded = get_purch_requisition($requisition_id);
	$auto_approval_limit = (float)get_company_pref('requisition_auto_approval_limit');
	if ($auto_approval_limit > 0 && (float)$reloaded['total_estimated'] <= $auto_approval_limit) {
		approve_requisition($requisition_id, (int)$_SESSION['wa_current_user']->user);
		return array('status' => 'auto_approved', 'requisition_id' => (int)$requisition_id);
	}

	if (!class_exists('ApprovalWorkflow') || !ApprovalWorkflow::isRegistered(ST_PURCHREQUISITION))
		return array('status' => 'submitted', 'requisition_id' => (int)$requisition_id);

	$approval = get_approval_workflow_service();
	$approval_amount = (float)$reloaded['total_estimated'];
	if (!$approval->isApprovalRequired(ST_PURCHREQUISITION, $approval_amount))
		return array('status' => 'submitted', 'requisition_id' => (int)$requisition_id);

	$draft_data = get_purch_requisition_submission_data($requisition_id);
	return $approval->submit(
		ST_PURCHREQUISITION,
		$draft_data,
		$approval_amount,
		array(
			'summary' => sprintf(_('Purchase Requisition %s'), $reloaded['reference']),
			'currency' => get_company_pref('curr_default'),
			'person_type' => PT_MISC,
			'person_id' => $reloaded['requester_id'],
			'loc_code' => $reloaded['location'],
		)
	);
}

/**
 * Approve a requisition.
 *
 * @param int $requisition_id
 * @param int $approved_by
 * @return bool
 */
function approve_requisition($requisition_id, $approved_by = 0)
{
	$requisition = get_purch_requisition($requisition_id);
	if (!$requisition || !in_array($requisition['status'], array('submitted', 'approved')))
		return false;

	begin_transaction();

	if ($requisition['material_request_status'] === 'submitted') {
		if (!approve_material_request((int)$requisition['material_request_id'])) {
			cancel_transaction();
			return false;
		}
	}

	$sql = "UPDATE " . TB_PREF . "purch_requisitions SET
		status = 'approved',
		approved_by = " . (int)($approved_by ? $approved_by : $_SESSION['wa_current_user']->user) . ",
		approved_date = NOW(),
		rejected_by = 0,
		rejected_date = NULL,
		rejection_reason = NULL
		WHERE id = " . (int)$requisition_id;
	db_query($sql, 'could not approve purchase requisition');

	$sql = "UPDATE " . TB_PREF . "purch_requisition_lines SET
		status = CASE WHEN qty_ordered >= quantity THEN 'ordered' ELSE 'approved' END
		WHERE requisition_id = " . (int)$requisition_id
		. " AND status <> 'ordered'";
	db_query($sql, 'could not approve purchase requisition lines');

	commit_transaction();

	$approved_requisition_id = (int)$requisition_id;
	hook_invoke_all('purch_requisition_approved', $approved_requisition_id);
	return true;
}

/**
 * Reject a requisition.
 *
 * @param int    $requisition_id
 * @param int    $rejected_by
 * @param string $reason
 * @return bool
 */
function reject_requisition($requisition_id, $rejected_by, $reason)
{
	$requisition = get_purch_requisition($requisition_id);
	if (!$requisition || $requisition['status'] !== 'submitted')
		return false;

	begin_transaction();

	if ($requisition['material_request_status'] === 'submitted') {
		if (!reject_material_request((int)$requisition['material_request_id'])) {
			cancel_transaction();
			return false;
		}
	}

	$sql = "UPDATE " . TB_PREF . "purch_requisitions SET
		status = 'rejected',
		rejected_by = " . (int)($rejected_by ? $rejected_by : $_SESSION['wa_current_user']->user) . ",
		rejected_date = NOW(),
		rejection_reason = " . ($reason !== '' ? db_escape($reason) : 'NULL') . ",
		approved_by = 0,
		approved_date = NULL
		WHERE id = " . (int)$requisition_id;
	db_query($sql, 'could not reject purchase requisition');

	$sql = "UPDATE " . TB_PREF . "purch_requisition_lines SET
		status = 'rejected'
		WHERE requisition_id = " . (int)$requisition_id
		. " AND status <> 'ordered'";
	db_query($sql, 'could not reject purchase requisition lines');

	commit_transaction();

	$hook_data = array('requisition_id' => (int)$requisition_id, 'reason' => $reason);
	hook_invoke_all('purch_requisition_rejected', $hook_data);
	return true;
}

/**
 * Create purchase orders from approved requisition lines.
 *
 * @param int   $requisition_id
 * @param int   $supplier_id
 * @param array $line_ids
 * @return array|false
 */
function create_po_from_requisition($requisition_id, $supplier_id, $line_ids = array())
{
	global $Refs;

	$requisition = get_purch_requisition($requisition_id);
	if (!$requisition || !in_array($requisition['status'], array('approved', 'partially_ordered')))
		return false;

	$selected_line_ids = array();
	if (!empty($line_ids)) {
		foreach ($line_ids as $line_id) {
			if ((int)$line_id > 0)
				$selected_line_ids[] = (int)$line_id;
		}
	}

	$grouped_lines = array();
	$lines = get_requisition_lines($requisition_id);
	while ($line = db_fetch($lines)) {
		$remaining_quantity = round((float)$line['quantity'] - (float)$line['qty_ordered'], get_qty_dec($line['stock_id']));
		if ($remaining_quantity <= 0)
			continue;
		if (!empty($selected_line_ids) && !in_array((int)$line['id'], $selected_line_ids))
			continue;

		$line_supplier_id = (int)$supplier_id > 0 ? (int)$supplier_id : (int)$line['preferred_supplier_id'];
		if ($line_supplier_id <= 0)
			continue;

		if (!isset($grouped_lines[$line_supplier_id]))
			$grouped_lines[$line_supplier_id] = array();

		$line['remaining_quantity'] = $remaining_quantity;
		$grouped_lines[$line_supplier_id][] = $line;
	}

	if (empty($grouped_lines))
		return false;

	$po_numbers = array();
	begin_transaction();

	foreach ($grouped_lines as $group_supplier_id => $supplier_lines) {
		$purchase_order = new purch_order();
		$purchase_order->trans_type = ST_PURCHORDER;
		$purchase_order->orig_order_date = Today();
		$purchase_order->due_date = $requisition['required_date'] ? sql2date($requisition['required_date']) : Today();
		$purchase_order->Location = $requisition['location'];
		$purchase_order->Comments = sprintf(_('Created from Purchase Requisition %s'), $requisition['reference']);
		$purchase_order->delivery_address = get_location_name($requisition['location']);
		$purchase_order->requisition_id = (int)$requisition_id;
		get_supplier_details_to_order($purchase_order, $group_supplier_id);
		$purchase_order->reference = $Refs->get_next(ST_PURCHORDER, null, array(
			'supplier_id' => $group_supplier_id,
			'date' => $purchase_order->orig_order_date,
		));

		foreach ($supplier_lines as $line) {
			$price = (float)$line['estimated_unit_price'] > 0
				? (float)$line['estimated_unit_price']
				: (float)get_purchase_price($group_supplier_id, $line['stock_id']);
			$description = $line['line_description'] !== '' ? $line['line_description'] : null;

			$purchase_order->add_to_order(
				count($purchase_order->line_items),
				$line['stock_id'],
				$line['remaining_quantity'],
				$description,
				$price,
				$line['line_unit_of_measure'],
				$purchase_order->due_date,
				0,
				0
			);
		}

		if (!$purchase_order->order_has_items())
			continue;

		$po_number = add_po($purchase_order);
		if (!$po_number)
			continue;

		$po_numbers[] = (int)$po_number;

		foreach ($supplier_lines as $line) {
			$sql = "UPDATE " . TB_PREF . "purch_requisition_lines SET
				qty_ordered = qty_ordered + " . db_escape($line['remaining_quantity']) . ",
				po_number = " . (int)$po_number . ",
				status = 'ordered'
				WHERE id = " . (int)$line['id'];
			db_query($sql, 'could not update ordered requisition line');
		}

		if ((int)$requisition['material_request_id'] > 0 && $requisition['material_request_status'] === 'approved')
			mark_material_request_ordered((int)$requisition['material_request_id'], ST_PURCHORDER, $po_number);
	}

	update_purch_requisition_order_status($requisition_id);
	commit_transaction();

	$hook_data = array('requisition_id' => (int)$requisition_id, 'po_numbers' => $po_numbers);
	hook_invoke_all('purch_requisition_po_created', $hook_data);

	return $po_numbers;
}

/**
 * Create a purchase requisition from an existing material request.
 *
 * @param int $material_request_id
 * @return int|false
 */
function create_requisition_from_material_request($material_request_id)
{
	$material_request = get_material_request($material_request_id);
	if (!$material_request || $material_request['request_type'] !== 'purchase')
		return false;

	$sql = "SELECT id FROM " . TB_PREF . "purch_requisitions WHERE material_request_id = " . (int)$material_request_id;
	$result = db_query($sql, 'could not check existing purchase requisition bridge');
	$existing = db_fetch($result);
	if ($existing)
		return (int)$existing['id'];

	$requisition_id = add_purch_requisition(
		(int)$material_request['requested_by'],
		0,
		$material_request['required_date'],
		$material_request['memo'] ? $material_request['memo'] : '',
		$material_request['warehouse_loc_code'],
		'normal',
		$material_request['memo'] ? $material_request['memo'] : '',
		0,
		0,
		'',
		$material_request['request_date'],
		(int)$material_request_id,
		array('bridged_from_material_request' => 1)
	);

	$sql = "UPDATE " . TB_PREF . "purch_requisitions SET
		status = " . db_escape(map_material_request_status_to_purch_requisition_status($material_request['status'])) . "
		WHERE id = " . (int)$requisition_id;
	db_query($sql, 'could not sync bridged purchase requisition status');

	$lines = get_material_request_lines($material_request_id);
	while ($line = db_fetch($lines)) {
		$supplier_data = get_default_requisition_supplier_data($line['stock_id']);
		$line_custom_data = array('bridged_from_material_request_line' => (int)$line['line_id']);

		$sql = "INSERT INTO " . TB_PREF . "purch_requisition_lines (
			requisition_id,
			material_request_line_id,
			stock_id,
			description,
			quantity,
			unit_of_measure,
			estimated_unit_price,
			preferred_supplier_id,
			notes,
			qty_ordered,
			po_number,
			status,
			custom_data
		) VALUES (
			" . (int)$requisition_id . ",
			" . (int)$line['line_id'] . ",
			" . db_escape($line['stock_id']) . ",
			" . db_escape($line['item_description']) . ",
			" . db_escape($line['qty_requested']) . ",
			" . db_escape($line['item_units']) . ",
			" . db_escape($supplier_data['price']) . ",
			" . (int)$supplier_data['supplier_id'] . ",
			" . ($line['memo'] ? db_escape($line['memo']) : 'NULL') . ",
			0,
			0,
			" . db_escape(in_array($material_request['status'], array('approved', 'ordered', 'fulfilled')) ? 'approved' : 'pending') . ",
			" . db_escape(encode_purch_requisition_custom_data($line_custom_data)) . "
		)";
		db_query($sql, 'could not insert bridged purchase requisition line');
	}

	update_purch_requisition_total($requisition_id);

	$hook_data = array(
		'requisition_id' => (int)$requisition_id,
		'material_request_id' => (int)$material_request_id,
	);
	hook_invoke_all('purch_requisition_from_mr', $hook_data);

	return $requisition_id;
}

/**
 * Get outstanding requisition lines.
 *
 * @param int $supplier_id
 * @return resource
 */
function get_outstanding_requisition_lines($supplier_id = 0)
{
	$sql = "SELECT pr.reference,
		pr.request_date,
		pr.required_date,
		pr.location,
		prl.*,
		COALESCE(prl.description, sm.description) AS line_description,
		supplier.supp_name AS preferred_supplier_name
		FROM " . TB_PREF . "purch_requisition_lines prl
		INNER JOIN " . TB_PREF . "purch_requisitions pr ON pr.id = prl.requisition_id
		LEFT JOIN " . TB_PREF . "stock_master sm ON sm.stock_id = prl.stock_id
		LEFT JOIN " . TB_PREF . "suppliers supplier ON supplier.supplier_id = prl.preferred_supplier_id
		WHERE pr.status IN ('approved', 'partially_ordered')
		AND prl.qty_ordered < prl.quantity";

	if ($supplier_id > 0)
		$sql .= " AND prl.preferred_supplier_id = " . (int)$supplier_id;

	$sql .= " ORDER BY pr.request_date DESC, pr.id DESC, prl.id";
	return db_query($sql, 'could not get outstanding purchase requisition lines');
}