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
 * @return bool
 */
function update_purch_requisition_total($requisition_id)
{
	include_once($GLOBALS['path_to_root'].'/purchasing/includes/db/purch_requisition_progress_db.inc');
	return write_purch_requisition_progress($requisition_id, 'total_estimated');
}

/**
 * Sync requisition header status from line ordering progress.
 *
 * @param int $requisition_id
 * @return bool
 */
function update_purch_requisition_order_status($requisition_id)
{
	include_once($GLOBALS['path_to_root'].'/purchasing/includes/db/purch_requisition_progress_db.inc');
	return write_purch_requisition_progress($requisition_id, 'status');
}

/**
 * Return a submitted requisition to draft.
 *
 * @param int $requisition_id
 * @return bool
 */
function reopen_purch_requisition($requisition_id)
{
	include_once $GLOBALS['path_to_root'].'/purchasing/includes/db/purch_requisition_reopen_db.inc';
	return reopen_purch_requisition_from_core($requisition_id);
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
	require_once $GLOBALS['path_to_root'].'/purchasing/includes/db/purch_requisition_header_db.inc';
	return write_purch_requisition_header('add', null, array('requester_id'=>$requester_id,'department_id'=>$department_id,'required_date'=>$required_date,'purpose'=>$purpose,'location'=>$location,'priority'=>$priority,'notes'=>$notes,'dimension_id'=>$dimension_id,'dimension2_id'=>$dimension2_id,'reference'=>$reference,'request_date'=>$request_date,'material_request_id'=>$material_request_id,'custom_data'=>$custom_data));
}

/**
 * Validate requisition line numeric values before saving.
 *
 * @param float       $quantity
 * @param mixed       $estimated_price
 * @param string|null $error_message
 * @return bool
 */
function can_save_requisition_line_values($quantity, $estimated_price, &$error_message = null)
{
	$error_message = null;

	if (!is_numeric($quantity) || (float)$quantity <= 0) {
		$error_message = _('The quantity must be greater than zero.');
		return false;
	}

	if (!is_numeric($estimated_price) || (float)$estimated_price < 0) {
		$error_message = _('The estimated unit price must be zero or greater.');
		return false;
	}

	return true;
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
	$reference = '', $request_date = null, $custom_data = null)
{
	require_once $GLOBALS['path_to_root'].'/purchasing/includes/db/purch_requisition_header_db.inc';
	return write_purch_requisition_header('update', $requisition_id, array('department_id'=>$department_id,'required_date'=>$required_date,'purpose'=>$purpose,'location'=>$location,'priority'=>$priority,'notes'=>$notes,'dimension_id'=>$dimension_id,'dimension2_id'=>$dimension2_id,'reference'=>$reference,'request_date'=>$request_date,'custom_data'=>$custom_data));
}

/**
 * Delete a purchase requisition.
 *
 * @param int $requisition_id
 * @return bool
 */
function delete_purch_requisition($requisition_id)
{
	require_once $GLOBALS['path_to_root'].'/purchasing/includes/db/purch_requisition_header_db.inc';
	return write_purch_requisition_header('delete', $requisition_id, array());
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
	require_once $GLOBALS['path_to_root'].'/purchasing/includes/db/purch_requisition_line_db.inc';
	$error_message = null;
	$result = write_purch_requisition_line('add', $requisition_id, array('stock_id'=>$stock_id,'quantity'=>$quantity,'estimated_unit_price'=>$estimated_price,'preferred_supplier_id'=>$preferred_supplier_id,'description'=>$description,'unit_of_measure'=>$unit_of_measure,'notes'=>$notes,'custom_data'=>$custom_data));
	if (!$result) $error_message = _('The purchase requisition line could not be added. Reload the requisition and check its status, item and classifications.');
	return $result;
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
	$description = '', $unit_of_measure = '', $notes = '', $custom_data = null)
{
	require_once $GLOBALS['path_to_root'].'/purchasing/includes/db/purch_requisition_line_db.inc';
	$result = write_purch_requisition_line('update', $line_id, array('stock_id'=>$stock_id,'quantity'=>$quantity,'estimated_unit_price'=>$estimated_price,'preferred_supplier_id'=>$preferred_supplier_id,'description'=>$description,'unit_of_measure'=>$unit_of_measure,'notes'=>$notes,'custom_data'=>$custom_data));
	return $result;
}

/**
 * Delete a requisition line.
 *
 * @param int $line_id
 * @return bool
 */
function delete_requisition_line($line_id)
{
	require_once $GLOBALS['path_to_root'].'/purchasing/includes/db/purch_requisition_line_db.inc';
	return write_purch_requisition_line('delete', $line_id, array());
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

	$approval_registered = class_exists('ApprovalWorkflow') && ApprovalWorkflow::isRegistered(ST_PURCHREQUISITION);
	$approval_amount = (float)$requisition['total_estimated'];
	$approval_required = false;
	$approval = null;

	if ($approval_registered) {
		$approval = get_approval_workflow_service();
		$approval_required = $approval->isApprovalRequired(ST_PURCHREQUISITION, $approval_amount);
	}

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

	if (!$approval_registered || !$approval_required) {
		if (!approve_requisition($requisition_id, (int)$_SESSION['wa_current_user']->user))
			return false;

		return array('status' => 'auto_approved', 'requisition_id' => (int)$requisition_id);
	}

	$reloaded = get_purch_requisition($requisition_id);

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
    include_once($GLOBALS['path_to_root'].'/purchasing/includes/db/purch_requisition_reject_db.inc');
    return reject_purch_requisition_from_core($requisition_id, $rejected_by, $reason);
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
 include_once($GLOBALS['path_to_root'].'/purchasing/includes/db/purch_requisition_po_approval_db.inc');
 $results = submit_requisition_purchase_orders($requisition_id, $supplier_id, $line_ids, true);
 if ($results === false) return false;
 $numbers = array(); foreach ($results as $result) $numbers[] = (int)$result['trans_no'];
 return $numbers;
}

/**
 * Create a purchase requisition from an existing material request.
 *
 * @param int $material_request_id
 * @return int|false
 */
function create_requisition_from_material_request($material_request_id)
{
	require_once $GLOBALS['path_to_root'].'/purchasing/includes/db/purch_requisition_bridge_db.inc';
	return write_purch_requisition_bridge($material_request_id);
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
