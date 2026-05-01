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
include_once($path_to_root . '/includes/inventory.inc');
include_once($path_to_root . '/purchasing/includes/db/purch_requisition_db.inc');
include_once($path_to_root . '/purchasing/includes/po_class.inc');
include_once($path_to_root . '/purchasing/includes/db/po_db.inc');
include_once($path_to_root . '/purchasing/includes/db/suppliers_db.inc');

/**
 * Get available RFQ types.
 *
 * @return array
 */
function get_purch_rfq_types()
{
	return array(
		'standard' => _('Standard'),
		'call_for_tender' => _('Call for Tender'),
	);
}

/**
 * Get available RFQ statuses.
 *
 * @return array
 */
function get_purch_rfq_statuses()
{
	return array(
		'draft' => _('Draft'),
		'sent' => _('Sent'),
		'received' => _('Received'),
		'evaluated' => _('Evaluated'),
		'awarded' => _('Awarded'),
		'expired' => _('Expired'),
		'cancelled' => _('Cancelled'),
	);
}

/**
 * Get available RFQ vendor statuses.
 *
 * @return array
 */
function get_purch_rfq_vendor_statuses()
{
	return array(
		'invited' => _('Invited'),
		'responded' => _('Responded'),
		'declined' => _('Declined'),
		'awarded' => _('Awarded'),
		'rejected' => _('Rejected'),
	);
}

/**
 * Get the badge color for one RFQ status.
 *
 * @param string $status
 * @return string
 */
function get_purch_rfq_status_color($status)
{
	$colors = array(
		'draft' => '#6c757d',
		'sent' => '#17a2b8',
		'received' => '#007bff',
		'evaluated' => '#6f42c1',
		'awarded' => '#28a745',
		'expired' => '#dc3545',
		'cancelled' => '#343a40',
	);

	return isset($colors[$status]) ? $colors[$status] : '#6c757d';
}

/**
 * Get the badge color for one RFQ vendor status.
 *
 * @param string $status
 * @return string
 */
function get_purch_rfq_vendor_status_color($status)
{
	$colors = array(
		'invited' => '#17a2b8',
		'responded' => '#007bff',
		'declined' => '#6c757d',
		'awarded' => '#28a745',
		'rejected' => '#dc3545',
	);

	return isset($colors[$status]) ? $colors[$status] : '#6c757d';
}

/**
 * Render a status badge for one RFQ.
 *
 * @param string $status
 * @return string
 */
function purch_rfq_status_badge($status)
{
	$statuses = get_purch_rfq_statuses();
	$label = isset($statuses[$status]) ? $statuses[$status] : $status;
	$color = get_purch_rfq_status_color($status);

	return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;font-size:11px;'
		. 'font-weight:bold;color:#fff;background:' . $color . ';">' . $label . '</span>';
}

/**
 * Render a vendor status badge for one RFQ vendor.
 *
 * @param string $status
 * @return string
 */
function purch_rfq_vendor_status_badge($status)
{
	$statuses = get_purch_rfq_vendor_statuses();
	$label = isset($statuses[$status]) ? $statuses[$status] : $status;
	$color = get_purch_rfq_vendor_status_color($status);

	return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;font-size:11px;'
		. 'font-weight:bold;color:#fff;background:' . $color . ';">' . $label . '</span>';
}

/**
 * Format an RFQ reference.
 *
 * @param int $rfq_id
 * @return string
 */
function format_purch_rfq_reference($rfq_id)
{
	return 'RFQ-' . str_pad((int)$rfq_id, 4, '0', STR_PAD_LEFT);
}

/**
 * Encode RFQ custom data for storage.
 *
 * @param mixed $custom_data
 * @return string|null
 */
function encode_purch_rfq_custom_data($custom_data)
{
	if ($custom_data === null || $custom_data === '' || $custom_data === array())
		return null;

	if (is_string($custom_data))
		return $custom_data;

	$encoded = json_encode($custom_data);
	return $encoded === false ? null : $encoded;
}

/**
 * Build a SQL-safe JSON literal for RFQ custom data columns.
 *
 * @param mixed $custom_data
 * @return string
 */
function sql_purch_rfq_custom_data($custom_data)
{
	$encoded = encode_purch_rfq_custom_data($custom_data);
	return $encoded === null ? 'NULL' : "'" . db_escape_function($encoded) . "'";
}

/**
 * Decode RFQ custom data from storage.
 *
 * @param string|null $custom_data
 * @return array
 */
function decode_purch_rfq_custom_data($custom_data)
{
	if (!$custom_data)
		return array();

	$decoded = json_decode($custom_data, true);
	if (!is_array($decoded))
		$decoded = json_decode(html_entity_decode($custom_data, ENT_QUOTES, 'UTF-8'), true);

	return is_array($decoded) ? $decoded : array();
}

/**
 * Get editable RFQ statuses.
 *
 * @return array
 */
function get_purch_rfq_editable_statuses()
{
	return array('draft');
}

/**
 * Get the default RFQ deadline date in SQL format.
 *
 * @param string $created_date_sql
 * @return string
 */
function get_default_purch_rfq_deadline_date($created_date_sql = '')
{
	$deadline_days = (int)get_company_pref('rfq_default_deadline_days');
	if ($deadline_days <= 0)
		$deadline_days = 14;

	$display_date = $created_date_sql !== '' ? sql2date($created_date_sql) : Today();
	return date2sql(add_days($display_date, $deadline_days));
}

/**
 * Get the editable RFQ header row.
 *
 * @param int $rfq_id
 * @return array|false
 */
function get_editable_purch_rfq($rfq_id)
{
	$rfq = get_purch_rfq($rfq_id);
	if (!$rfq)
		return false;

	return in_array($rfq['status'], get_purch_rfq_editable_statuses()) ? $rfq : false;
}

/**
 * Get the normalized RFQ award map from stored custom data.
 *
 * @param mixed $custom_data
 * @return array
 */
function get_purch_rfq_award_map($custom_data)
{
	$award_map = array();
	$decoded = is_array($custom_data) ? $custom_data : decode_purch_rfq_custom_data($custom_data);

	if (!isset($decoded['line_awards']) || !is_array($decoded['line_awards']))
		return $award_map;

	foreach ($decoded['line_awards'] as $rfq_item_id => $rfq_vendor_id) {
		if ((int)$rfq_item_id > 0 && (int)$rfq_vendor_id > 0)
			$award_map[(int)$rfq_item_id] = (int)$rfq_vendor_id;
	}

	return $award_map;
}

/**
 * Update the stored RFQ award map.
 *
 * @param int   $rfq_id
 * @param array $award_map
 * @return void
 */
function update_purch_rfq_award_map($rfq_id, $award_map)
{
	$rfq = get_purch_rfq($rfq_id);
	if (!$rfq)
		return;

	$custom_data = decode_purch_rfq_custom_data($rfq['custom_data']);
	$custom_data['line_awards'] = array();

	foreach ($award_map as $rfq_item_id => $rfq_vendor_id) {
		if ((int)$rfq_item_id > 0 && (int)$rfq_vendor_id > 0)
			$custom_data['line_awards'][(int)$rfq_item_id] = (int)$rfq_vendor_id;
	}

	$sql = "UPDATE " . TB_PREF . "purch_rfq SET
		custom_data = " . sql_purch_rfq_custom_data($custom_data) . "
		WHERE id = " . (int)$rfq_id;
	db_query($sql, 'could not update RFQ award map');
}

/**
 * Get one RFQ header.
 *
 * @param int $rfq_id
 * @return array|false
 */
function get_purch_rfq($rfq_id)
{
	$sql = "SELECT rfq.*,
		creator.real_name AS creator_name,
		requisition.reference AS requisition_reference,
		requisition.status AS requisition_status,
		location.location_name AS delivery_location_name,
		(SELECT COUNT(*) FROM " . TB_PREF . "purch_rfq_items rfq_item WHERE rfq_item.rfq_id = rfq.id) AS item_count,
		(SELECT COUNT(*) FROM " . TB_PREF . "purch_rfq_vendors rfq_vendor WHERE rfq_vendor.rfq_id = rfq.id) AS vendor_count,
		(SELECT COUNT(*) FROM " . TB_PREF . "purch_rfq_vendors rfq_vendor WHERE rfq_vendor.rfq_id = rfq.id AND rfq_vendor.status IN ('responded', 'awarded', 'rejected')) AS responded_count,
		(SELECT COUNT(*) FROM " . TB_PREF . "purch_rfq_vendors rfq_vendor WHERE rfq_vendor.rfq_id = rfq.id AND rfq_vendor.is_winner = 1) AS winner_count,
		(SELECT COALESCE(SUM(rfq_item.quantity * rfq_item.target_price), 0) FROM " . TB_PREF . "purch_rfq_items rfq_item WHERE rfq_item.rfq_id = rfq.id) AS target_total
		FROM " . TB_PREF . "purch_rfq rfq
		LEFT JOIN " . TB_PREF . "users creator ON creator.id = rfq.created_by
		LEFT JOIN " . TB_PREF . "purch_requisitions requisition ON requisition.id = rfq.requisition_id
		LEFT JOIN " . TB_PREF . "locations location ON location.loc_code = rfq.delivery_location
		WHERE rfq.id = " . (int)$rfq_id;
	$result = db_query($sql, 'could not get purchase RFQ');
	$row = db_fetch($result);

	if ($row)
		$row['award_map'] = get_purch_rfq_award_map($row['custom_data']);

	return $row;
}

/**
 * Get RFQs by filter.
 *
 * @param string $status
 * @param string $date_from
 * @param string $date_to
 * @param string $rfq_type
 * @return resource
 */
function get_purch_rfqs($status = '', $date_from = '', $date_to = '', $rfq_type = '')
{
	$sql = "SELECT rfq.*,
		creator.real_name AS creator_name,
		requisition.reference AS requisition_reference,
		location.location_name AS delivery_location_name,
		COUNT(DISTINCT rfq_item.id) AS item_count,
		COUNT(DISTINCT rfq_vendor.id) AS vendor_count,
		SUM(CASE WHEN rfq_vendor.status IN ('responded', 'awarded', 'rejected') THEN 1 ELSE 0 END) AS responded_count,
		SUM(CASE WHEN rfq_vendor.is_winner = 1 THEN 1 ELSE 0 END) AS winner_count,
		COALESCE(SUM(DISTINCT rfq_item.quantity * rfq_item.target_price), 0) AS target_total
		FROM " . TB_PREF . "purch_rfq rfq
		LEFT JOIN " . TB_PREF . "users creator ON creator.id = rfq.created_by
		LEFT JOIN " . TB_PREF . "purch_requisitions requisition ON requisition.id = rfq.requisition_id
		LEFT JOIN " . TB_PREF . "locations location ON location.loc_code = rfq.delivery_location
		LEFT JOIN " . TB_PREF . "purch_rfq_items rfq_item ON rfq_item.rfq_id = rfq.id
		LEFT JOIN " . TB_PREF . "purch_rfq_vendors rfq_vendor ON rfq_vendor.rfq_id = rfq.id
		WHERE 1 = 1";

	if ($status !== '')
		$sql .= " AND rfq.status = " . db_escape($status);
	if ($rfq_type !== '')
		$sql .= " AND rfq.rfq_type = " . db_escape($rfq_type);
	if ($date_from !== '')
		$sql .= " AND rfq.created_date >= " . db_escape($date_from);
	if ($date_to !== '')
		$sql .= " AND rfq.created_date <= " . db_escape($date_to);

	$sql .= " GROUP BY rfq.id ORDER BY rfq.id DESC";
	return db_query($sql, 'could not get purchase RFQs');
}

/**
 * Add an RFQ header.
 *
 * @param string      $rfq_type
 * @param string      $description
 * @param string|null $deadline_date
 * @param int         $requisition_id
 * @param string      $notes
 * @param string      $terms_and_conditions
 * @param string      $delivery_location
 * @param string|null $required_delivery_date
 * @param string      $evaluation_criteria
 * @param string|null $validity_date
 * @param int         $dimension_id
 * @param int         $dimension2_id
 * @param string      $reference
 * @param string|null $created_date
 * @param mixed       $custom_data
 * @return int
 */
function add_purch_rfq($rfq_type, $description, $deadline_date, $requisition_id,
	$notes = '', $terms_and_conditions = '', $delivery_location = '',
	$required_delivery_date = null, $evaluation_criteria = '', $validity_date = null,
	$dimension_id = 0, $dimension2_id = 0, $reference = '', $created_date = null,
	$custom_data = array())
{
	$created_date = $created_date ? $created_date : date2sql(Today());
	$deadline_date = $deadline_date ? $deadline_date : get_default_purch_rfq_deadline_date($created_date);

	$hook_data = array(
		'rfq_type' => $rfq_type,
		'description' => $description,
		'deadline_date' => $deadline_date,
		'requisition_id' => (int)$requisition_id,
		'notes' => $notes,
		'terms_and_conditions' => $terms_and_conditions,
		'delivery_location' => $delivery_location,
		'required_delivery_date' => $required_delivery_date,
		'evaluation_criteria' => $evaluation_criteria,
		'validity_date' => $validity_date,
		'dimension_id' => (int)$dimension_id,
		'dimension2_id' => (int)$dimension2_id,
		'reference' => $reference,
		'created_date' => $created_date,
		'created_by' => (int)$_SESSION['wa_current_user']->user,
		'custom_data' => $custom_data,
	);
	hook_invoke_all('purch_rfq_before_save', $hook_data);

	$sql = "INSERT INTO " . TB_PREF . "purch_rfq (
		reference,
		rfq_type,
		status,
		created_by,
		created_date,
		deadline_date,
		validity_date,
		requisition_id,
		description,
		notes,
		terms_and_conditions,
		delivery_location,
		required_delivery_date,
		evaluation_criteria,
		dimension_id,
		dimension2_id,
		custom_data
	) VALUES (
		'',
		" . db_escape($hook_data['rfq_type']) . ",
		'draft',
		" . (int)$hook_data['created_by'] . ",
		" . db_escape($hook_data['created_date']) . ",
		" . ($hook_data['deadline_date'] ? db_escape($hook_data['deadline_date']) : 'NULL') . ",
		" . ($hook_data['validity_date'] ? db_escape($hook_data['validity_date']) : 'NULL') . ",
		" . (int)$hook_data['requisition_id'] . ",
		" . ($hook_data['description'] !== '' ? db_escape($hook_data['description']) : 'NULL') . ",
		" . ($hook_data['notes'] !== '' ? db_escape($hook_data['notes']) : 'NULL') . ",
		" . ($hook_data['terms_and_conditions'] !== '' ? db_escape($hook_data['terms_and_conditions']) : 'NULL') . ",
		" . db_escape($hook_data['delivery_location']) . ",
		" . ($hook_data['required_delivery_date'] ? db_escape($hook_data['required_delivery_date']) : 'NULL') . ",
		" . ($hook_data['evaluation_criteria'] !== '' ? db_escape($hook_data['evaluation_criteria']) : 'NULL') . ",
		" . (int)$hook_data['dimension_id'] . ",
		" . (int)$hook_data['dimension2_id'] . ",
		" . sql_purch_rfq_custom_data($hook_data['custom_data']) . "
	)";
	db_query($sql, 'could not insert purchase RFQ');

	$rfq_id = db_insert_id();
	$generated_reference = $hook_data['reference'] !== '' ? $hook_data['reference'] : format_purch_rfq_reference($rfq_id);

	$sql = "UPDATE " . TB_PREF . "purch_rfq SET
		reference = " . db_escape($generated_reference) . "
		WHERE id = " . (int)$rfq_id;
	db_query($sql, 'could not update purchase RFQ reference');

	return $rfq_id;
}

/**
 * Update an RFQ header.
 *
 * @param int         $rfq_id
 * @param string      $rfq_type
 * @param string      $description
 * @param string|null $deadline_date
 * @param int         $requisition_id
 * @param string      $notes
 * @param string      $terms_and_conditions
 * @param string      $delivery_location
 * @param string|null $required_delivery_date
 * @param string      $evaluation_criteria
 * @param string|null $validity_date
 * @param int         $dimension_id
 * @param int         $dimension2_id
 * @param string      $reference
 * @param string|null $created_date
 * @param mixed       $custom_data
 * @return bool
 */
function update_purch_rfq($rfq_id, $rfq_type, $description, $deadline_date, $requisition_id,
	$notes = '', $terms_and_conditions = '', $delivery_location = '',
	$required_delivery_date = null, $evaluation_criteria = '', $validity_date = null,
	$dimension_id = 0, $dimension2_id = 0, $reference = '', $created_date = null,
	$custom_data = array())
{
	$rfq = get_editable_purch_rfq($rfq_id);
	if (!$rfq)
		return false;

	$created_date = $created_date ? $created_date : $rfq['created_date'];
	$deadline_date = $deadline_date ? $deadline_date : get_default_purch_rfq_deadline_date($created_date);
	$reference = $reference !== '' ? $reference : $rfq['reference'];

	$hook_data = array(
		'id' => (int)$rfq_id,
		'rfq_type' => $rfq_type,
		'description' => $description,
		'deadline_date' => $deadline_date,
		'requisition_id' => (int)$requisition_id,
		'notes' => $notes,
		'terms_and_conditions' => $terms_and_conditions,
		'delivery_location' => $delivery_location,
		'required_delivery_date' => $required_delivery_date,
		'evaluation_criteria' => $evaluation_criteria,
		'validity_date' => $validity_date,
		'dimension_id' => (int)$dimension_id,
		'dimension2_id' => (int)$dimension2_id,
		'reference' => $reference,
		'created_date' => $created_date,
		'custom_data' => $custom_data,
	);
	hook_invoke_all('purch_rfq_before_save', $hook_data);

	$sql = "UPDATE " . TB_PREF . "purch_rfq SET
		rfq_type = " . db_escape($hook_data['rfq_type']) . ",
		created_date = " . db_escape($hook_data['created_date']) . ",
		deadline_date = " . ($hook_data['deadline_date'] ? db_escape($hook_data['deadline_date']) : 'NULL') . ",
		validity_date = " . ($hook_data['validity_date'] ? db_escape($hook_data['validity_date']) : 'NULL') . ",
		requisition_id = " . (int)$hook_data['requisition_id'] . ",
		description = " . ($hook_data['description'] !== '' ? db_escape($hook_data['description']) : 'NULL') . ",
		notes = " . ($hook_data['notes'] !== '' ? db_escape($hook_data['notes']) : 'NULL') . ",
		terms_and_conditions = " . ($hook_data['terms_and_conditions'] !== '' ? db_escape($hook_data['terms_and_conditions']) : 'NULL') . ",
		delivery_location = " . db_escape($hook_data['delivery_location']) . ",
		required_delivery_date = " . ($hook_data['required_delivery_date'] ? db_escape($hook_data['required_delivery_date']) : 'NULL') . ",
		evaluation_criteria = " . ($hook_data['evaluation_criteria'] !== '' ? db_escape($hook_data['evaluation_criteria']) : 'NULL') . ",
		dimension_id = " . (int)$hook_data['dimension_id'] . ",
		dimension2_id = " . (int)$hook_data['dimension2_id'] . ",
		reference = " . db_escape($hook_data['reference']) . ",
		custom_data = " . sql_purch_rfq_custom_data($hook_data['custom_data']) . "
		WHERE id = " . (int)$rfq_id;
	db_query($sql, 'could not update purchase RFQ');

	return true;
}

/**
 * Delete an RFQ.
 *
 * @param int $rfq_id
 * @return bool
 */
function delete_purch_rfq($rfq_id)
{
	$rfq = get_purch_rfq($rfq_id);
	if (!$rfq || $rfq['status'] !== 'draft')
		return false;

	begin_transaction();

	$sql = "DELETE rfq_vendor_line
		FROM " . TB_PREF . "purch_rfq_vendor_lines rfq_vendor_line
		INNER JOIN " . TB_PREF . "purch_rfq_vendors rfq_vendor ON rfq_vendor.id = rfq_vendor_line.rfq_vendor_id
		WHERE rfq_vendor.rfq_id = " . (int)$rfq_id;
	db_query($sql, 'could not delete RFQ vendor lines');

	$sql = "DELETE FROM " . TB_PREF . "purch_rfq_vendors WHERE rfq_id = " . (int)$rfq_id;
	db_query($sql, 'could not delete RFQ vendors');

	$sql = "DELETE FROM " . TB_PREF . "purch_rfq_items WHERE rfq_id = " . (int)$rfq_id;
	db_query($sql, 'could not delete RFQ items');

	$sql = "DELETE FROM " . TB_PREF . "purch_rfq WHERE id = " . (int)$rfq_id;
	db_query($sql, 'could not delete RFQ');

	commit_transaction();
	return true;
}

/**
 * Get the next sort order number for RFQ items.
 *
 * @param int $rfq_id
 * @return int
 */
function get_next_purch_rfq_item_sort_order($rfq_id)
{
	$sql = "SELECT COALESCE(MAX(sort_order), 0) + 1 AS next_sort_order
		FROM " . TB_PREF . "purch_rfq_items
		WHERE rfq_id = " . (int)$rfq_id;
	$result = db_query($sql, 'could not get RFQ item sort order');
	$row = db_fetch($result);

	return $row ? (int)$row['next_sort_order'] : 1;
}

/**
 * Add an RFQ item.
 *
 * @param int         $rfq_id
 * @param string      $stock_id
 * @param float       $quantity
 * @param float       $target_price
 * @param string      $specifications
 * @param string      $description
 * @param string      $unit_of_measure
 * @param int         $sort_order
 * @param mixed       $custom_data
 * @return int|false
 */
function add_rfq_item($rfq_id, $stock_id, $quantity, $target_price, $specifications = '',
	$description = '', $unit_of_measure = '', $sort_order = 0, $custom_data = array(), &$error_message = null)
{
	$error_message = null;

	$rfq = get_editable_purch_rfq($rfq_id);
	if (!$rfq) {
		$existing_rfq = get_purch_rfq($rfq_id);
		if (!$existing_rfq)
			$error_message = _('The selected RFQ could not be found.');
		else
			$error_message = _('Items can only be added while the RFQ is in Draft status.');
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
	$sort_order = $sort_order > 0 ? (int)$sort_order : get_next_purch_rfq_item_sort_order($rfq_id);

	$sql = "INSERT INTO " . TB_PREF . "purch_rfq_items (
		rfq_id,
		stock_id,
		description,
		quantity,
		unit_of_measure,
		target_price,
		specifications,
		sort_order,
		custom_data
	) VALUES (
		" . (int)$rfq_id . ",
		" . db_escape($stock_id) . ",
		" . db_escape($line_description) . ",
		" . db_escape($quantity) . ",
		" . db_escape($line_unit) . ",
		" . db_escape($target_price) . ",
		" . ($specifications !== '' ? db_escape($specifications) : 'NULL') . ",
		" . (int)$sort_order . ",
		" . sql_purch_rfq_custom_data($custom_data) . "
	)";
	db_query($sql, 'could not insert RFQ item');

	return db_insert_id();
}

/**
 * Update an RFQ item.
 *
 * @param int         $item_id
 * @param string      $stock_id
 * @param float       $quantity
 * @param float       $target_price
 * @param string      $specifications
 * @param string      $description
 * @param string      $unit_of_measure
 * @param int         $sort_order
 * @param mixed       $custom_data
 * @return bool
 */
function update_rfq_item($item_id, $stock_id, $quantity, $target_price, $specifications = '',
	$description = '', $unit_of_measure = '', $sort_order = 0, $custom_data = array())
{
	$sql = "SELECT * FROM " . TB_PREF . "purch_rfq_items WHERE id = " . (int)$item_id;
	$result = db_query($sql, 'could not get RFQ item');
	$item_row = db_fetch($result);
	if (!$item_row)
		return false;

	$rfq = get_editable_purch_rfq((int)$item_row['rfq_id']);
	if (!$rfq || $quantity <= 0)
		return false;

	$item = get_item($stock_id);
	if (!$item)
		return false;

	$line_description = $description !== '' ? $description : $item['description'];
	$line_unit = $unit_of_measure !== '' ? $unit_of_measure : $item['units'];
	$sort_order = $sort_order > 0 ? (int)$sort_order : (int)$item_row['sort_order'];

	$sql = "UPDATE " . TB_PREF . "purch_rfq_items SET
		stock_id = " . db_escape($stock_id) . ",
		description = " . db_escape($line_description) . ",
		quantity = " . db_escape($quantity) . ",
		unit_of_measure = " . db_escape($line_unit) . ",
		target_price = " . db_escape($target_price) . ",
		specifications = " . ($specifications !== '' ? db_escape($specifications) : 'NULL') . ",
		sort_order = " . (int)$sort_order . ",
		custom_data = " . sql_purch_rfq_custom_data($custom_data) . "
		WHERE id = " . (int)$item_id;
	db_query($sql, 'could not update RFQ item');

	return true;
}

/**
 * Delete an RFQ item.
 *
 * @param int $item_id
 * @return bool
 */
function delete_rfq_item($item_id)
{
	$sql = "SELECT * FROM " . TB_PREF . "purch_rfq_items WHERE id = " . (int)$item_id;
	$result = db_query($sql, 'could not get RFQ item for delete');
	$item_row = db_fetch($result);
	if (!$item_row)
		return false;

	$rfq = get_editable_purch_rfq((int)$item_row['rfq_id']);
	if (!$rfq)
		return false;

	begin_transaction();

	$sql = "DELETE FROM " . TB_PREF . "purch_rfq_vendor_lines WHERE rfq_item_id = " . (int)$item_id;
	db_query($sql, 'could not delete RFQ vendor line responses');

	$sql = "DELETE FROM " . TB_PREF . "purch_rfq_items WHERE id = " . (int)$item_id;
	db_query($sql, 'could not delete RFQ item');

	commit_transaction();
	return true;
}

/**
 * Get RFQ items.
 *
 * @param int $rfq_id
 * @return resource
 */
function get_rfq_items($rfq_id)
{
	$sql = "SELECT rfq_item.*,
		COALESCE(rfq_item.description, stock.description) AS line_description,
		COALESCE(NULLIF(rfq_item.unit_of_measure, ''), stock.units) AS line_unit_of_measure,
		stock.description AS stock_description,
		stock.units AS stock_units,
		(SELECT COUNT(*) FROM " . TB_PREF . "purch_rfq_vendor_lines vendor_line WHERE vendor_line.rfq_item_id = rfq_item.id) AS response_count,
		(SELECT MIN(vendor_line.quoted_price)
			FROM " . TB_PREF . "purch_rfq_vendor_lines vendor_line
			INNER JOIN " . TB_PREF . "purch_rfq_vendors vendor ON vendor.id = vendor_line.rfq_vendor_id
			WHERE vendor_line.rfq_item_id = rfq_item.id
			AND vendor.status IN ('responded', 'awarded', 'rejected')
			AND vendor_line.quoted_price > 0) AS best_quoted_price
		FROM " . TB_PREF . "purch_rfq_items rfq_item
		LEFT JOIN " . TB_PREF . "stock_master stock ON stock.stock_id = rfq_item.stock_id
		WHERE rfq_item.rfq_id = " . (int)$rfq_id . "
		ORDER BY rfq_item.sort_order, rfq_item.id";

	return db_query($sql, 'could not get RFQ items');
}

/**
 * Get one RFQ vendor row.
 *
 * @param int $rfq_vendor_id
 * @return array|false
 */
function get_purch_rfq_vendor($rfq_vendor_id)
{
	$sql = "SELECT rfq_vendor.*,
		supplier.supp_name,
		supplier.curr_code,
		(SELECT COUNT(*) FROM " . TB_PREF . "purch_rfq_vendor_lines vendor_line WHERE vendor_line.rfq_vendor_id = rfq_vendor.id) AS line_response_count
		FROM " . TB_PREF . "purch_rfq_vendors rfq_vendor
		INNER JOIN " . TB_PREF . "suppliers supplier ON supplier.supplier_id = rfq_vendor.supplier_id
		WHERE rfq_vendor.id = " . (int)$rfq_vendor_id;
	$result = db_query($sql, 'could not get RFQ vendor');
	$row = db_fetch($result);

	if ($row)
		$row['custom_data_array'] = decode_purch_rfq_custom_data($row['custom_data']);

	return $row;
}

/**
 * Get one RFQ vendor row by supplier.
 *
 * @param int $rfq_id
 * @param int $supplier_id
 * @return array|false
 */
function get_purch_rfq_vendor_by_supplier($rfq_id, $supplier_id)
{
	$sql = "SELECT rfq_vendor.id
		FROM " . TB_PREF . "purch_rfq_vendors rfq_vendor
		WHERE rfq_vendor.rfq_id = " . (int)$rfq_id . "
		AND rfq_vendor.supplier_id = " . (int)$supplier_id;
	$result = db_query($sql, 'could not get RFQ vendor by supplier');
	$row = db_fetch($result);

	return $row ? get_purch_rfq_vendor((int)$row['id']) : false;
}

/**
 * Resolve an RFQ vendor reference to the RFQ vendor row.
 *
 * @param int $rfq_id
 * @param int $vendor_reference
 * @return array|false
 */
function resolve_purch_rfq_vendor_reference($rfq_id, $vendor_reference)
{
	$vendor_reference = (int)$vendor_reference;
	if ($vendor_reference <= 0)
		return false;

	$rfq_vendor = get_purch_rfq_vendor($vendor_reference);
	if ($rfq_vendor && (int)$rfq_vendor['rfq_id'] === (int)$rfq_id)
		return $rfq_vendor;

	return get_purch_rfq_vendor_by_supplier($rfq_id, $vendor_reference);
}

/**
 * Add an RFQ vendor.
 *
 * @param int   $rfq_id
 * @param int   $supplier_id
 * @param mixed $custom_data
 * @return int|false
 */
function add_rfq_vendor($rfq_id, $supplier_id, $custom_data = array())
{
	$rfq = get_editable_purch_rfq($rfq_id);
	if (!$rfq || (int)$supplier_id <= 0)
		return false;

	if (!get_supplier((int)$supplier_id))
		return false;

	$existing = get_purch_rfq_vendor_by_supplier($rfq_id, $supplier_id);
	if ($existing)
		return (int)$existing['id'];

	$sql = "INSERT INTO " . TB_PREF . "purch_rfq_vendors (
		rfq_id,
		supplier_id,
		status,
		is_winner,
		custom_data
	) VALUES (
		" . (int)$rfq_id . ",
		" . (int)$supplier_id . ",
		'invited',
		0,
		" . sql_purch_rfq_custom_data($custom_data) . "
	)";
	db_query($sql, 'could not insert RFQ vendor');

	return db_insert_id();
}

/**
 * Remove an RFQ vendor.
 *
 * @param int $rfq_id
 * @param int $supplier_id
 * @return bool
 */
function remove_rfq_vendor($rfq_id, $supplier_id)
{
	$rfq = get_editable_purch_rfq($rfq_id);
	if (!$rfq)
		return false;

	$rfq_vendor = get_purch_rfq_vendor_by_supplier($rfq_id, $supplier_id);
	if (!$rfq_vendor || in_array($rfq_vendor['status'], array('responded', 'awarded')) || (int)$rfq_vendor['is_winner'] === 1)
		return false;

	begin_transaction();

	$sql = "DELETE FROM " . TB_PREF . "purch_rfq_vendor_lines WHERE rfq_vendor_id = " . (int)$rfq_vendor['id'];
	db_query($sql, 'could not delete RFQ vendor response lines');

	$sql = "DELETE FROM " . TB_PREF . "purch_rfq_vendors WHERE id = " . (int)$rfq_vendor['id'];
	db_query($sql, 'could not delete RFQ vendor');

	commit_transaction();
	return true;
}

/**
 * Get RFQ vendors.
 *
 * @param int $rfq_id
 * @return resource
 */
function get_rfq_vendors($rfq_id)
{
	$sql = "SELECT rfq_vendor.*,
		supplier.supp_name,
		supplier.curr_code,
		supplier.payment_terms,
		(SELECT COUNT(*) FROM " . TB_PREF . "purch_rfq_vendor_lines vendor_line WHERE vendor_line.rfq_vendor_id = rfq_vendor.id) AS line_response_count
		FROM " . TB_PREF . "purch_rfq_vendors rfq_vendor
		INNER JOIN " . TB_PREF . "suppliers supplier ON supplier.supplier_id = rfq_vendor.supplier_id
		WHERE rfq_vendor.rfq_id = " . (int)$rfq_id . "
		ORDER BY supplier.supp_name";

	return db_query($sql, 'could not get RFQ vendors');
}

/**
 * Get RFQ vendor line responses keyed by RFQ item id.
 *
 * @param int $rfq_vendor_id
 * @return array
 */
function get_rfq_vendor_line_map($rfq_vendor_id)
{
	$line_map = array();

	$sql = "SELECT vendor_line.*
		FROM " . TB_PREF . "purch_rfq_vendor_lines vendor_line
		WHERE vendor_line.rfq_vendor_id = " . (int)$rfq_vendor_id;
	$result = db_query($sql, 'could not get RFQ vendor response lines');

	while ($row = db_fetch($result)) {
		$row['custom_data_array'] = decode_purch_rfq_custom_data($row['custom_data']);
		$line_map[(int)$row['rfq_item_id']] = $row;
	}

	return $line_map;
}

/**
 * Refresh one RFQ vendor total from line responses.
 *
 * @param int $rfq_vendor_id
 * @return float
 */
function refresh_purch_rfq_vendor_total($rfq_vendor_id)
{
	$sql = "SELECT COALESCE(SUM(vendor_line.quoted_price * vendor_line.quoted_quantity), 0) AS total_quoted
		FROM " . TB_PREF . "purch_rfq_vendor_lines vendor_line
		WHERE vendor_line.rfq_vendor_id = " . (int)$rfq_vendor_id;
	$result = db_query($sql, 'could not calculate RFQ vendor total');
	$row = db_fetch($result);
	$total_quoted = $row ? (float)$row['total_quoted'] : 0;

	$sql = "UPDATE " . TB_PREF . "purch_rfq_vendors SET
		total_quoted = " . db_escape($total_quoted) . "
		WHERE id = " . (int)$rfq_vendor_id;
	db_query($sql, 'could not refresh RFQ vendor total');

	return $total_quoted;
}

/**
 * Update the RFQ header status from vendor activity.
 *
 * @param int $rfq_id
 * @return void
 */
function update_purch_rfq_status_from_activity($rfq_id)
{
	$rfq = get_purch_rfq($rfq_id);
	if (!$rfq || in_array($rfq['status'], array('cancelled', 'expired')))
		return;

	$sql = "SELECT
		COUNT(*) AS vendor_count,
		SUM(CASE WHEN status IN ('responded', 'awarded', 'rejected') THEN 1 ELSE 0 END) AS responded_count,
		SUM(CASE WHEN is_winner = 1 THEN 1 ELSE 0 END) AS winner_count,
		SUM(CASE WHEN evaluator_score <> 0 OR (evaluator_notes IS NOT NULL AND evaluator_notes <> '') THEN 1 ELSE 0 END) AS evaluated_count,
		SUM(CASE WHEN sent_date IS NOT NULL THEN 1 ELSE 0 END) AS sent_count
		FROM " . TB_PREF . "purch_rfq_vendors
		WHERE rfq_id = " . (int)$rfq_id;
	$result = db_query($sql, 'could not calculate RFQ activity status');
	$row = db_fetch($result);

	$new_status = 'draft';
	if ((int)$row['winner_count'] > 0)
		$new_status = 'awarded';
	elseif ((int)$row['evaluated_count'] > 0)
		$new_status = 'evaluated';
	elseif ((int)$row['responded_count'] > 0)
		$new_status = 'received';
	elseif ((int)$row['sent_count'] > 0)
		$new_status = 'sent';

	if ($new_status !== $rfq['status']) {
		$sql = "UPDATE " . TB_PREF . "purch_rfq SET
			status = " . db_escape($new_status) . "
			WHERE id = " . (int)$rfq_id;
		db_query($sql, 'could not update RFQ activity status');
	}
}

/**
 * Send the RFQ to vendors.
 *
 * @param int   $rfq_id
 * @param array $vendor_ids
 * @return bool
 */
function send_rfq_to_vendors($rfq_id, $vendor_ids = array())
{
	$rfq = get_purch_rfq($rfq_id);
	if (!$rfq || !in_array($rfq['status'], array('draft', 'sent', 'received', 'evaluated')))
		return false;

	$selected_vendor_ids = array();
	foreach ($vendor_ids as $vendor_id) {
		if ((int)$vendor_id > 0)
			$selected_vendor_ids[] = (int)$vendor_id;
	}

	if (empty($selected_vendor_ids)) {
		$vendors = get_rfq_vendors($rfq_id);
		while ($vendor = db_fetch($vendors))
			$selected_vendor_ids[] = (int)$vendor['id'];
	}

	if (empty($selected_vendor_ids))
		return false;

	$sql = "UPDATE " . TB_PREF . "purch_rfq_vendors SET
		sent_date = NOW(),
		status = CASE
			WHEN status IN ('responded', 'awarded') THEN status
			ELSE 'invited'
		END
		WHERE id IN (" . implode(',', $selected_vendor_ids) . ")
		AND rfq_id = " . (int)$rfq_id;
	db_query($sql, 'could not mark RFQ vendors as sent');

	$sql = "UPDATE " . TB_PREF . "purch_rfq SET
		status = 'sent'
		WHERE id = " . (int)$rfq_id . "
		AND status NOT IN ('awarded', 'cancelled', 'expired')";
	db_query($sql, 'could not mark RFQ as sent');

	$hook_data = array(
		'rfq_id' => (int)$rfq_id,
		'vendor_ids' => $selected_vendor_ids,
	);
	hook_invoke_all('purch_rfq_sent', $hook_data);

	return true;
}

/**
 * Record one vendor response.
 *
 * @param int    $rfq_id
 * @param int    $supplier_id
 * @param array  $lines
 * @param float  $total
 * @param int    $lead_days
 * @param string $notes
 * @param string $payment_terms
 * @param mixed  $custom_data
 * @return bool
 */
function record_vendor_response($rfq_id, $supplier_id, $lines, $total, $lead_days, $notes, $payment_terms = '', $custom_data = array())
{
	$rfq = get_purch_rfq($rfq_id);
	if (!$rfq || in_array($rfq['status'], array('awarded', 'cancelled', 'expired')))
		return false;

	$rfq_vendor = get_purch_rfq_vendor_by_supplier($rfq_id, $supplier_id);
	if (!$rfq_vendor)
		return false;

	$item_rows = array();
	$rfq_items = get_rfq_items($rfq_id);
	while ($item_row = db_fetch($rfq_items))
		$item_rows[(int)$item_row['id']] = $item_row;

	if (empty($item_rows))
		return false;

	begin_transaction();

	$sql = "DELETE FROM " . TB_PREF . "purch_rfq_vendor_lines WHERE rfq_vendor_id = " . (int)$rfq_vendor['id'];
	db_query($sql, 'could not clear RFQ vendor response lines');

	$computed_total = 0;
	foreach ((array)$lines as $line) {
		$rfq_item_id = isset($line['rfq_item_id']) ? (int)$line['rfq_item_id'] : 0;
		if ($rfq_item_id <= 0 || !isset($item_rows[$rfq_item_id]))
			continue;

		$quoted_price = isset($line['quoted_price']) ? (float)$line['quoted_price'] : 0;
		$quoted_quantity = isset($line['quoted_quantity']) && (float)$line['quoted_quantity'] > 0
			? (float)$line['quoted_quantity']
			: (float)$item_rows[$rfq_item_id]['quantity'];
		$line_lead_days = isset($line['delivery_lead_days']) ? (int)$line['delivery_lead_days'] : 0;
		$line_notes = isset($line['notes']) ? trim($line['notes']) : '';
		$line_custom_data = isset($line['custom_data']) ? $line['custom_data'] : array();

		$sql = "INSERT INTO " . TB_PREF . "purch_rfq_vendor_lines (
			rfq_vendor_id,
			rfq_item_id,
			quoted_price,
			quoted_quantity,
			delivery_lead_days,
			notes,
			custom_data
		) VALUES (
			" . (int)$rfq_vendor['id'] . ",
			" . (int)$rfq_item_id . ",
			" . db_escape($quoted_price) . ",
			" . db_escape($quoted_quantity) . ",
			" . (int)$line_lead_days . ",
			" . ($line_notes !== '' ? db_escape($line_notes) : 'NULL') . ",
			" . sql_purch_rfq_custom_data($line_custom_data) . "
		)";
		db_query($sql, 'could not insert RFQ vendor response line');

		$computed_total += $quoted_price * $quoted_quantity;
	}

	$total_quoted = (float)$total > 0 ? (float)$total : (float)$computed_total;

	$sql = "UPDATE " . TB_PREF . "purch_rfq_vendors SET
		status = 'responded',
		response_date = NOW(),
		total_quoted = " . db_escape($total_quoted) . ",
		delivery_lead_days = " . (int)$lead_days . ",
		payment_terms = " . ($payment_terms !== '' ? db_escape($payment_terms) : 'NULL') . ",
		vendor_notes = " . ($notes !== '' ? db_escape($notes) : 'NULL') . ",
		custom_data = " . sql_purch_rfq_custom_data($custom_data) . "
		WHERE id = " . (int)$rfq_vendor['id'];
	db_query($sql, 'could not update RFQ vendor response');

	commit_transaction();
	update_purch_rfq_status_from_activity($rfq_id);

	$hook_data = array(
		'rfq_id' => (int)$rfq_id,
		'supplier_id' => (int)$supplier_id,
		'rfq_vendor_id' => (int)$rfq_vendor['id'],
	);
	hook_invoke_all('purch_rfq_response_received', $hook_data);

	return true;
}

/**
 * Evaluate one vendor response.
 *
 * @param int    $rfq_vendor_id
 * @param float  $score
 * @param string $notes
 * @return bool
 */
function evaluate_vendor_response($rfq_vendor_id, $score, $notes)
{
	$rfq_vendor = get_purch_rfq_vendor($rfq_vendor_id);
	if (!$rfq_vendor)
		return false;

	$sql = "UPDATE " . TB_PREF . "purch_rfq_vendors SET
		evaluator_score = " . db_escape($score) . ",
		evaluator_notes = " . ($notes !== '' ? db_escape($notes) : 'NULL') . "
		WHERE id = " . (int)$rfq_vendor_id;
	db_query($sql, 'could not evaluate RFQ vendor response');

	update_purch_rfq_status_from_activity((int)$rfq_vendor['rfq_id']);
	return true;
}

/**
 * Compare vendor responses for one RFQ.
 *
 * @param int $rfq_id
 * @return array
 */
function compare_rfq_responses($rfq_id)
{
	$comparison = array(
		'rfq' => get_purch_rfq($rfq_id),
		'vendors' => array(),
		'items' => array(),
		'best_overall_vendor_id' => 0,
	);

	if (!$comparison['rfq'])
		return $comparison;

	$vendor_line_maps = array();
	$lowest_total = null;
	$vendors = get_rfq_vendors($rfq_id);
	while ($vendor = db_fetch($vendors)) {
		$line_map = get_rfq_vendor_line_map((int)$vendor['id']);
		if ((int)$vendor['line_response_count'] <= 0 && !in_array($vendor['status'], array('awarded', 'rejected')))
			continue;

		$vendor_line_maps[(int)$vendor['id']] = $line_map;
		$comparison['vendors'][(int)$vendor['id']] = $vendor;

		if ((float)$vendor['total_quoted'] > 0 && ($lowest_total === null || (float)$vendor['total_quoted'] < $lowest_total)) {
			$lowest_total = (float)$vendor['total_quoted'];
			$comparison['best_overall_vendor_id'] = (int)$vendor['id'];
		}
	}

	$rfq_items = get_rfq_items($rfq_id);
	while ($rfq_item = db_fetch($rfq_items)) {
		$rfq_item['quotes'] = array();
		$rfq_item['best_price'] = null;
		$rfq_item['best_vendor_ids'] = array();

		foreach ($comparison['vendors'] as $rfq_vendor_id => $vendor) {
			if (!isset($vendor_line_maps[$rfq_vendor_id][(int)$rfq_item['id']]))
				continue;

			$quote = $vendor_line_maps[$rfq_vendor_id][(int)$rfq_item['id']];
			$rfq_item['quotes'][$rfq_vendor_id] = $quote;

			if ((float)$quote['quoted_price'] > 0) {
				if ($rfq_item['best_price'] === null || (float)$quote['quoted_price'] < (float)$rfq_item['best_price']) {
					$rfq_item['best_price'] = (float)$quote['quoted_price'];
					$rfq_item['best_vendor_ids'] = array($rfq_vendor_id);
				} elseif ((float)$quote['quoted_price'] == (float)$rfq_item['best_price']) {
					$rfq_item['best_vendor_ids'][] = $rfq_vendor_id;
				}
			}
		}

		$comparison['items'][(int)$rfq_item['id']] = $rfq_item;
	}

	$hook_data = array(
		'comparison' => $comparison,
		'rfq_id' => (int)$rfq_id,
	);
	hook_invoke_all('purch_rfq_compare', $hook_data);

	return isset($hook_data['comparison']) && is_array($hook_data['comparison'])
		? $hook_data['comparison']
		: $comparison;
}

/**
 * Award one RFQ to the selected vendor or vendors.
 *
 * @param int   $rfq_id
 * @param int   $winning_vendor_id
 * @param array $line_awards
 * @return bool
 */
function award_rfq($rfq_id, $winning_vendor_id, $line_awards = array())
{
	$rfq = get_purch_rfq($rfq_id);
	if (!$rfq || in_array($rfq['status'], array('cancelled', 'expired')))
		return false;

	$item_ids = array();
	$rfq_items = get_rfq_items($rfq_id);
	while ($rfq_item = db_fetch($rfq_items))
		$item_ids[] = (int)$rfq_item['id'];

	if (empty($item_ids))
		return false;

	$award_map = array();
	foreach ((array)$line_awards as $rfq_item_id => $vendor_reference) {
		if ((int)$rfq_item_id <= 0 || !in_array((int)$rfq_item_id, $item_ids))
			continue;

		$rfq_vendor = resolve_purch_rfq_vendor_reference($rfq_id, $vendor_reference);
		if ($rfq_vendor)
			$award_map[(int)$rfq_item_id] = (int)$rfq_vendor['id'];
	}

	if (empty($award_map) && (int)$winning_vendor_id > 0) {
		$rfq_vendor = resolve_purch_rfq_vendor_reference($rfq_id, $winning_vendor_id);
		if (!$rfq_vendor)
			return false;

		$vendor_lines = get_rfq_vendor_line_map((int)$rfq_vendor['id']);
		foreach ($item_ids as $rfq_item_id) {
			if (isset($vendor_lines[$rfq_item_id]))
				$award_map[$rfq_item_id] = (int)$rfq_vendor['id'];
		}
	}

	if (empty($award_map))
		return false;

	$awarded_vendor_ids = array_values(array_unique(array_values($award_map)));

	begin_transaction();

	$sql = "UPDATE " . TB_PREF . "purch_rfq_vendors SET
		is_winner = 0,
		status = CASE
			WHEN status = 'declined' THEN status
			ELSE 'rejected'
		END
		WHERE rfq_id = " . (int)$rfq_id;
	db_query($sql, 'could not reset RFQ vendor award statuses');

	$sql = "UPDATE " . TB_PREF . "purch_rfq_vendors SET
		is_winner = 1,
		status = 'awarded'
		WHERE id IN (" . implode(',', $awarded_vendor_ids) . ")
		AND rfq_id = " . (int)$rfq_id;
	db_query($sql, 'could not set RFQ winning vendors');

	update_purch_rfq_award_map($rfq_id, $award_map);

	$sql = "UPDATE " . TB_PREF . "purch_rfq SET
		status = 'awarded'
		WHERE id = " . (int)$rfq_id;
	db_query($sql, 'could not award RFQ');

	commit_transaction();

	$hook_data = array(
		'rfq_id' => (int)$rfq_id,
		'vendor_id' => count($awarded_vendor_ids) === 1 ? reset($awarded_vendor_ids) : 0,
		'awarded_vendor_ids' => $awarded_vendor_ids,
		'line_awards' => $award_map,
	);
	hook_invoke_all('purch_rfq_awarded', $hook_data);

	return true;
}

/**
 * Create a purchase order from an awarded RFQ vendor.
 *
 * @param int   $rfq_id
 * @param int   $vendor_id
 * @param array $line_ids
 * @return int|false
 */
function create_po_from_rfq($rfq_id, $vendor_id, $line_ids = array())
{
	global $Refs;

	$rfq = get_purch_rfq($rfq_id);
	if (!$rfq || $rfq['status'] !== 'awarded')
		return false;

	$rfq_vendor = resolve_purch_rfq_vendor_reference($rfq_id, $vendor_id);
	if (!$rfq_vendor || (int)$rfq_vendor['is_winner'] !== 1)
		return false;

	$selected_line_ids = array();
	foreach ((array)$line_ids as $line_id) {
		if ((int)$line_id > 0)
			$selected_line_ids[] = (int)$line_id;
	}

	$award_map = get_purch_rfq_award_map($rfq['custom_data']);
	$vendor_lines = get_rfq_vendor_line_map((int)$rfq_vendor['id']);

	$purchase_order = new purch_order();
	$purchase_order->trans_type = ST_PURCHORDER;
	$purchase_order->orig_order_date = Today();
	$purchase_order->due_date = $rfq['required_delivery_date'] ? sql2date($rfq['required_delivery_date']) : Today();
	$purchase_order->Location = $rfq['delivery_location'];
	$purchase_order->Comments = sprintf(_('Created from RFQ %s'), $rfq['reference']);
	$purchase_order->delivery_address = $rfq['delivery_location_name'] ? $rfq['delivery_location_name'] : $rfq['delivery_location'];
	$purchase_order->rfq_id = (int)$rfq_id;

	get_supplier_details_to_order($purchase_order, (int)$rfq_vendor['supplier_id']);
	$purchase_order->reference = $Refs->get_next(ST_PURCHORDER, null, array(
		'supplier_id' => (int)$rfq_vendor['supplier_id'],
		'date' => $purchase_order->orig_order_date,
		'location' => $purchase_order->Location,
	));

	$rfq_items = get_rfq_items($rfq_id);
	while ($rfq_item = db_fetch($rfq_items)) {
		$rfq_item_id = (int)$rfq_item['id'];
		if (!empty($selected_line_ids) && !in_array($rfq_item_id, $selected_line_ids))
			continue;
		if (!empty($award_map) && isset($award_map[$rfq_item_id]) && (int)$award_map[$rfq_item_id] !== (int)$rfq_vendor['id'])
			continue;
		if (!isset($vendor_lines[$rfq_item_id]))
			continue;

		$vendor_line = $vendor_lines[$rfq_item_id];
		$quantity = (float)$vendor_line['quoted_quantity'] > 0 ? (float)$vendor_line['quoted_quantity'] : (float)$rfq_item['quantity'];
		$price = (float)$vendor_line['quoted_price'];
		if ($quantity <= 0 || $price < 0)
			continue;

		$description = $rfq_item['line_description'] !== '' ? $rfq_item['line_description'] : null;
		$purchase_order->add_to_order(
			count($purchase_order->line_items),
			$rfq_item['stock_id'],
			$quantity,
			$description,
			$price,
			$rfq_item['line_unit_of_measure'],
			$purchase_order->due_date,
			0,
			0
		);
	}

	if (!$purchase_order->order_has_items())
		return false;

	$po_number = add_po($purchase_order);
	if (!$po_number)
		return false;

	$vendor_custom_data = decode_purch_rfq_custom_data($rfq_vendor['custom_data']);
	if (!isset($vendor_custom_data['created_po_numbers']) || !is_array($vendor_custom_data['created_po_numbers']))
		$vendor_custom_data['created_po_numbers'] = array();
	$vendor_custom_data['created_po_numbers'][] = (int)$po_number;

	$sql = "UPDATE " . TB_PREF . "purch_rfq_vendors SET
		custom_data = " . sql_purch_rfq_custom_data($vendor_custom_data) . "
		WHERE id = " . (int)$rfq_vendor['id'];
	db_query($sql, 'could not update RFQ vendor PO linkage');

	$rfq_custom_data = decode_purch_rfq_custom_data($rfq['custom_data']);
	if (!isset($rfq_custom_data['created_po_numbers']) || !is_array($rfq_custom_data['created_po_numbers']))
		$rfq_custom_data['created_po_numbers'] = array();
	$rfq_custom_data['created_po_numbers'][] = (int)$po_number;

	$sql = "UPDATE " . TB_PREF . "purch_rfq SET
		custom_data = " . sql_purch_rfq_custom_data($rfq_custom_data) . "
		WHERE id = " . (int)$rfq_id;
	db_query($sql, 'could not update RFQ PO linkage');

	$hook_data = array(
		'rfq_id' => (int)$rfq_id,
		'po_no' => (int)$po_number,
		'rfq_vendor_id' => (int)$rfq_vendor['id'],
	);
	hook_invoke_all('purch_rfq_po_created', $hook_data);

	return (int)$po_number;
}

/**
 * Expire overdue RFQs.
 *
 * @return int
 */
function expire_overdue_rfqs()
{
	$sql = "UPDATE " . TB_PREF . "purch_rfq SET
		status = 'expired'
		WHERE status IN ('draft', 'sent', 'received', 'evaluated')
		AND deadline_date IS NOT NULL
		AND deadline_date < " . db_escape(date2sql(Today()));
	db_query($sql, 'could not expire overdue RFQs');

	return db_affected_rows();
}

/**
 * Create one RFQ from an approved requisition.
 *
 * @param int   $requisition_id
 * @param array $supplier_ids
 * @param array $line_ids
 * @return int|false
 */
function create_rfq_from_requisition($requisition_id, $supplier_ids = array(), $line_ids = array())
{
	$requisition = get_purch_requisition($requisition_id);
	if (!$requisition || !in_array($requisition['status'], array('approved', 'partially_ordered')))
		return false;

	$selected_line_ids = array();
	foreach ((array)$line_ids as $line_id) {
		if ((int)$line_id > 0)
			$selected_line_ids[] = (int)$line_id;
	}

	$resolved_supplier_ids = array();
	foreach ((array)$supplier_ids as $supplier_id) {
		if ((int)$supplier_id > 0)
			$resolved_supplier_ids[(int)$supplier_id] = (int)$supplier_id;
	}

	$rfq_id = add_purch_rfq(
		'standard',
		sprintf(_('Created from Purchase Requisition %s'), $requisition['reference']),
		get_default_purch_rfq_deadline_date(date2sql(Today())),
		(int)$requisition_id,
		$requisition['notes'] ? $requisition['notes'] : '',
		'',
		$requisition['location'],
		$requisition['required_date'],
		'',
		null,
		(int)$requisition['dimension_id'],
		(int)$requisition['dimension2_id'],
		'',
		date2sql(Today()),
		array('created_from_requisition' => 1)
	);

	$requisition_lines = get_requisition_lines($requisition_id);
	$inserted_items = 0;
	while ($line = db_fetch($requisition_lines)) {
		$remaining_quantity = round((float)$line['quantity'] - (float)$line['qty_ordered'], get_qty_dec($line['stock_id']));
		if ($remaining_quantity <= 0)
			continue;
		if (!empty($selected_line_ids) && !in_array((int)$line['id'], $selected_line_ids))
			continue;

		add_rfq_item(
			$rfq_id,
			$line['stock_id'],
			$remaining_quantity,
			(float)$line['estimated_unit_price'],
			$line['notes'] ? $line['notes'] : '',
			$line['line_description'],
			$line['line_unit_of_measure'],
			$inserted_items + 1,
			array('requisition_line_id' => (int)$line['id'])
		);
		$inserted_items++;

		if ((int)$line['preferred_supplier_id'] > 0)
			$resolved_supplier_ids[(int)$line['preferred_supplier_id']] = (int)$line['preferred_supplier_id'];
	}

	if ($inserted_items === 0) {
		delete_purch_rfq($rfq_id);
		return false;
	}

	foreach ($resolved_supplier_ids as $supplier_id)
		add_rfq_vendor($rfq_id, $supplier_id);

	return $rfq_id;
}