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

require_once __DIR__.'/../../../dimensions/includes/dimension_reference_writer_fence.inc';
include_once($path_to_root . '/includes/date_functions.inc');
include_once(__DIR__.'/purch_agreement_po_approval_db.inc');
include_once(__DIR__.'/purch_rfq_po_approval_db.inc');
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
 * Validate one SQL-format RFQ date value.
 *
 * @param string|null $date_value
 * @return bool
 */
function is_valid_purch_rfq_sql_date($date_value)
{
	$date_value = trim((string)$date_value);
	if ($date_value === '')
		return false;

	if (!preg_match('/^\d{4}-\d{2}-\d{2}$/', $date_value))
		return false;

	$date_parts = explode('-', $date_value);
	if (count($date_parts) !== 3)
		return false;

	return checkdate((int)$date_parts[1], (int)$date_parts[2], (int)$date_parts[0]);
}

/**
 * Validate RFQ header values before persistence.
 *
 * @param string      $description
 * @param string      $created_date
 * @param string|null $deadline_date
 * @param string|null $validity_date
 * @param string|null $required_delivery_date
 * @param string      $error_message
 * @return bool
 */
function can_save_purch_rfq_header_data($description, $created_date, $deadline_date = null,
	$validity_date = null, $required_delivery_date = null, &$error_message = '')
{
	$description = trim((string)$description);
	$created_date = trim((string)$created_date);
	$deadline_date = trim((string)$deadline_date);
	$validity_date = trim((string)$validity_date);
	$required_delivery_date = trim((string)$required_delivery_date);

	if ($description === '') {
		$error_message = _('Enter a short RFQ description.');
		return false;
	}

	if (!is_valid_purch_rfq_sql_date($created_date)) {
		$error_message = _('The RFQ date is invalid.');
		return false;
	}

	if ($deadline_date !== '' && !is_valid_purch_rfq_sql_date($deadline_date)) {
		$error_message = _('The deadline date is invalid.');
		return false;
	}

	if ($validity_date !== '' && !is_valid_purch_rfq_sql_date($validity_date)) {
		$error_message = _('The validity date is invalid.');
		return false;
	}

	if ($required_delivery_date !== '' && !is_valid_purch_rfq_sql_date($required_delivery_date)) {
		$error_message = _('The required delivery date is invalid.');
		return false;
	}

	if ($deadline_date !== '' && strcmp($deadline_date, $created_date) < 0) {
		$error_message = _('The deadline date cannot be earlier than the RFQ date.');
		return false;
	}

	if ($validity_date !== '' && strcmp($validity_date, $created_date) < 0) {
		$error_message = _('The validity date cannot be earlier than the RFQ date.');
		return false;
	}

	if ($validity_date !== '' && $deadline_date !== '' && strcmp($validity_date, $deadline_date) < 0) {
		$error_message = _('The validity date cannot be earlier than the deadline date.');
		return false;
	}

	if ($required_delivery_date !== '' && strcmp($required_delivery_date, $created_date) < 0) {
		$error_message = _('The required delivery date cannot be earlier than the RFQ date.');
		return false;
	}

	if ($required_delivery_date !== '' && $deadline_date !== '' && strcmp($required_delivery_date, $deadline_date) < 0) {
		$error_message = _('The required delivery date cannot be earlier than the deadline date.');
		return false;
	}

	return true;
}

/**
 * Validate RFQ item values before persistence.
 *
 * @param string $stock_id
 * @param float  $quantity
 * @param float  $target_price
 * @param string $error_message
 * @return bool
 */
function can_save_purch_rfq_item_data($stock_id, $quantity, $target_price, &$error_message = '')
{
	$stock_id = trim((string)$stock_id);

	if ($stock_id === '') {
		$error_message = _('You must select an item.');
		return false;
	}

	if (!is_numeric($quantity) || (float)$quantity <= 0) {
		$error_message = _('The quantity must be greater than zero.');
		return false;
	}

	if (!is_numeric($target_price) || (float)$target_price < 0) {
		$error_message = _('The target price must be numeric and not less than zero.');
		return false;
	}

	return true;
}

/**
 * Normalize RFQ vendor response lines before persistence.
 *
 * @param array  $item_rows
 * @param array  $lines
 * @param array  $normalized_lines
 * @param string $error_message
 * @return bool
 */
function normalize_purch_rfq_response_lines($item_rows, $lines, &$normalized_lines, &$error_message = '')
{
	$normalized_lines = array();

	foreach ((array)$lines as $line) {
		$rfq_item_id = isset($line['rfq_item_id']) ? dimension_writer_reference_id($line['rfq_item_id']) : 0;
		if ($rfq_item_id <= 0 || !isset($item_rows[$rfq_item_id])) return false;

		$quoted_price_value = isset($line['quoted_price']) ? trim((string)$line['quoted_price']) : '';
		$quoted_quantity_value = isset($line['quoted_quantity']) ? trim((string)$line['quoted_quantity']) : '';
		$lead_days_value = isset($line['delivery_lead_days']) ? trim((string)$line['delivery_lead_days']) : '';
		$line_notes = isset($line['notes']) ? trim((string)$line['notes']) : '';
		$line_custom_data = array_key_exists('custom_data', $line) ? $line['custom_data'] : null;

		if ($quoted_price_value === '')
			$quoted_price = 0;
		elseif (!is_numeric($quoted_price_value)) {
			$error_message = _('Each quoted price must be numeric.');
			return false;
		} else
			$quoted_price = (float)$quoted_price_value;

		if ($quoted_quantity_value === '')
			$quoted_quantity = 0;
		elseif (!is_numeric($quoted_quantity_value)) {
			$error_message = _('Each quoted quantity must be numeric.');
			return false;
		} else
			$quoted_quantity = (float)$quoted_quantity_value;

		if ($lead_days_value === '')
			$line_lead_days = 0;
		elseif (!preg_match('/^-?\d+$/', $lead_days_value)) {
			$error_message = _('Each quoted lead time must be a whole number of days.');
			return false;
		} else
			$line_lead_days = (int)$lead_days_value;

		if (!is_finite($quoted_price) || !is_finite($quoted_quantity) || $quoted_price < 0) {
			$error_message = _('Each quoted price must be zero or greater.');
			return false;
		}

		if ($line_lead_days < 0) {
			$error_message = _('Each quoted lead time must be zero or greater.');
			return false;
		}

		if ($quoted_price == 0) {
			if ($quoted_quantity > 0 || $line_lead_days > 0 || $line_notes !== '') {
				$error_message = _('Enter a positive quoted price for each vendor response line you save.');
				return false;
			}
			continue;
		}

		if ($quoted_quantity <= 0) {
			$error_message = _('Each quoted line must have a quantity greater than zero.');
			return false;
		}

		$normalized_lines[] = array(
			'rfq_item_id' => $rfq_item_id,
			'quoted_price' => $quoted_price,
			'quoted_quantity' => $quoted_quantity,
			'delivery_lead_days' => $line_lead_days,
			'notes' => $line_notes,
			'custom_data' => $line_custom_data,
		);
	}

	if (empty($normalized_lines)) {
		$error_message = _('Enter at least one priced vendor response line before saving.');
		return false;
	}

	return true;
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
 * @return bool
 */
function update_purch_rfq_award_map($rfq_id, $award_map)
{
	return dimension_ordinary_master_write('purch_rfq', $rfq_id, array(), function($next) use ($rfq_id, $award_map) {
		$rows = db_query('SELECT custom_data FROM '.TB_PREF.'purch_rfq WHERE id='.(int)$rfq_id.' FOR UPDATE');
		$row = $rows ? db_fetch($rows) : false;
		if (!$row || !is_array($award_map)) return false;
		$custom_data = decode_purch_rfq_custom_data($row['custom_data']);
		$custom_data['line_awards'] = array();
		foreach ($award_map as $rfq_item_id=>$rfq_vendor_id)
			if ((int)$rfq_item_id > 0 && (int)$rfq_vendor_id > 0)
				$custom_data['line_awards'][(int)$rfq_item_id] = (int)$rfq_vendor_id;
		$encoded = encode_purch_rfq_custom_data($custom_data);
		if ($encoded === null || dimension_master_payload_transition($row['custom_data'], $encoded) === false) return false;
		return db_query('UPDATE '.TB_PREF.'purch_rfq SET custom_data='.sql_purch_rfq_custom_data($encoded).' WHERE id='.(int)$rfq_id);
	}, true);
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
 * @param string|null $error_message
 * @return int|false
 */
function add_purch_rfq($rfq_type, $description, $deadline_date, $requisition_id,
	$notes = '', $terms_and_conditions = '', $delivery_location = '',
	$required_delivery_date = null, $evaluation_criteria = '', $validity_date = null,
	$dimension_id = 0, $dimension2_id = 0, $reference = '', $created_date = null,
	$custom_data = array(), &$error_message = null)
{
	try {
		if (!begin_transaction()) return false;
		$error_message = '';
		$rfq_type = trim((string)$rfq_type) !== '' ? trim((string)$rfq_type) : 'standard';
		$description = trim((string)$description);
		$notes = trim((string)$notes);
		$terms_and_conditions = trim((string)$terms_and_conditions);
		$delivery_location = trim((string)$delivery_location);
		$evaluation_criteria = trim((string)$evaluation_criteria);
		$reference = trim((string)$reference);
		$created_date = $created_date ? trim((string)$created_date) : date2sql(Today());
		$deadline_date = $deadline_date ? trim((string)$deadline_date) : '';
		$validity_date = $validity_date ? trim((string)$validity_date) : '';
		$required_delivery_date = $required_delivery_date ? trim((string)$required_delivery_date) : '';

		if ($deadline_date === '' && is_valid_purch_rfq_sql_date($created_date))
			$deadline_date = get_default_purch_rfq_deadline_date($created_date);

		if (!can_save_purch_rfq_header_data(
			$description,
			$created_date,
			$deadline_date,
			$validity_date,
			$required_delivery_date,
			$error_message
		))
			{ cancel_transaction(); return false; }

		$hook_data = array(
			'rfq_type' => $rfq_type,
			'description' => $description,
			'deadline_date' => $deadline_date !== '' ? $deadline_date : null,
			'requisition_id' => (int)$requisition_id,
			'notes' => $notes,
			'terms_and_conditions' => $terms_and_conditions,
			'delivery_location' => $delivery_location,
			'required_delivery_date' => $required_delivery_date !== '' ? $required_delivery_date : null,
			'evaluation_criteria' => $evaluation_criteria,
			'validity_date' => $validity_date !== '' ? $validity_date : null,
			'dimension_id' => $dimension_id,
			'dimension2_id' => $dimension2_id,
			'reference' => $reference,
			'created_date' => $created_date,
			'created_by' => (int)$_SESSION['wa_current_user']->user,
			'custom_data' => $custom_data,
		);
		hook_invoke_all('purch_rfq_before_save', $hook_data);
		$encoded = encode_purch_rfq_custom_data($hook_data['custom_data']);
		if ($encoded === null && !in_array($hook_data['custom_data'], array(null, '', array()), true)) { cancel_transaction(); return false; }
		$next = array('dimension_id'=>$hook_data['dimension_id'], 'dimension2_id'=>$hook_data['dimension2_id'], 'custom_data'=>$encoded);
		$result = dimension_ordinary_master_write('purch_rfq', null, $next, function($admitted) use ($hook_data) {
			$hook_data['dimension_id'] = $admitted['dimension_id'];
			$hook_data['dimension2_id'] = $admitted['dimension2_id'];

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
			if (!db_query($sql)) return false;

			$rfq_id = db_insert_id();
			$generated_reference = $hook_data['reference'] !== '' ? $hook_data['reference'] : format_purch_rfq_reference($rfq_id);

			$sql = "UPDATE " . TB_PREF . "purch_rfq SET
				reference = " . db_escape($generated_reference) . "
				WHERE id = " . (int)$rfq_id;
			if (!db_query($sql)) return false;

			if (!db_preserve_insert_id($rfq_id)) return false;
			return $rfq_id;
		});
		if (!$result || !commit_transaction()) { cancel_transaction(); return false; }
		return $result;
	} catch (Exception $exception) { cancel_transaction(); return false;
	} catch (Throwable $exception) { cancel_transaction(); return false; }
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
 * @param string|null $error_message
 * @return bool
 */
function update_purch_rfq($rfq_id, $rfq_type, $description, $deadline_date, $requisition_id,
	$notes = '', $terms_and_conditions = '', $delivery_location = '',
	$required_delivery_date = null, $evaluation_criteria = '', $validity_date = null,
	$dimension_id = 0, $dimension2_id = 0, $reference = '', $created_date = null,
	$custom_data = null, &$error_message = null)
{
	try {
		if (!begin_transaction()) return false;
		$error_message = '';
		if (dimension_writer_reference_id($rfq_id) === false || !$rfq_id) { cancel_transaction(); return false; }
		$rows = db_query('SELECT * FROM '.TB_PREF.'purch_rfq WHERE id='.(int)$rfq_id.' FOR UPDATE');
		$rfq = $rows ? db_fetch($rows) : false;
		if (!$rfq || !in_array($rfq['status'], get_purch_rfq_editable_statuses(), true))
			{ cancel_transaction(); return false; }

		$rfq_type = trim((string)$rfq_type) !== '' ? trim((string)$rfq_type) : $rfq['rfq_type'];
		$description = trim((string)$description);
		$notes = trim((string)$notes);
		$terms_and_conditions = trim((string)$terms_and_conditions);
		$delivery_location = trim((string)$delivery_location);
		$evaluation_criteria = trim((string)$evaluation_criteria);
		$reference = trim((string)$reference);
		$created_date = $created_date ? trim((string)$created_date) : $rfq['created_date'];
		$deadline_date = $deadline_date ? trim((string)$deadline_date) : '';
		$validity_date = $validity_date ? trim((string)$validity_date) : '';
		$required_delivery_date = $required_delivery_date ? trim((string)$required_delivery_date) : '';

		if ($deadline_date === '' && is_valid_purch_rfq_sql_date($created_date))
			$deadline_date = get_default_purch_rfq_deadline_date($created_date);
		if ($reference === '')
			$reference = $rfq['reference'];

		if (!can_save_purch_rfq_header_data(
			$description,
			$created_date,
			$deadline_date,
			$validity_date,
			$required_delivery_date,
			$error_message
		))
			{ cancel_transaction(); return false; }

		if ($custom_data === null) $custom_data = $rfq['custom_data'];
		$hook_data = array(
			'id' => (int)$rfq_id,
			'rfq_type' => $rfq_type,
			'description' => $description,
			'deadline_date' => $deadline_date !== '' ? $deadline_date : null,
			'requisition_id' => (int)$requisition_id,
			'notes' => $notes,
			'terms_and_conditions' => $terms_and_conditions,
			'delivery_location' => $delivery_location,
			'required_delivery_date' => $required_delivery_date !== '' ? $required_delivery_date : null,
			'evaluation_criteria' => $evaluation_criteria,
			'validity_date' => $validity_date !== '' ? $validity_date : null,
			'dimension_id' => $dimension_id,
			'dimension2_id' => $dimension2_id,
			'reference' => $reference,
			'created_date' => $created_date,
			'custom_data' => $custom_data,
		);
		hook_invoke_all('purch_rfq_before_save', $hook_data);
		$encoded = encode_purch_rfq_custom_data($hook_data['custom_data']);
		if ($encoded === null && !in_array($hook_data['custom_data'], array(null, '', array()), true)) { cancel_transaction(); return false; }
		$next = array('dimension_id'=>$hook_data['dimension_id'], 'dimension2_id'=>$hook_data['dimension2_id'], 'custom_data'=>$encoded);
		$result = dimension_ordinary_master_write('purch_rfq', $rfq_id, $next, function($admitted) use ($hook_data, $rfq_id) {
			$hook_data['dimension_id'] = $admitted['dimension_id'];
			$hook_data['dimension2_id'] = $admitted['dimension2_id'];

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
			if (!db_query($sql)) return false;

			return true;
		});
		if (!$result || !commit_transaction()) { cancel_transaction(); return false; }
		return $result;
	} catch (Exception $exception) { cancel_transaction(); return false;
	} catch (Throwable $exception) { cancel_transaction(); return false; }
}

/**
 * Delete an RFQ.
 *
 * @param int $rfq_id
 * @return bool
 */
function delete_purch_rfq($rfq_id)
{
	return dimension_ordinary_master_write('purch_rfq', $rfq_id,
		array('dimension_id'=>0, 'dimension2_id'=>0), function($next) use ($rfq_id) {
		$rows = db_query('SELECT status FROM '.TB_PREF.'purch_rfq WHERE id='.(int)$rfq_id.' FOR UPDATE');
		$rfq = $rows ? db_fetch($rows) : false;
		if (!$rfq || $rfq['status'] !== 'draft') return false;
		// Lock every exact cascade target before erasing any retained evidence.
		$targets = array(
			'SELECT l.custom_data FROM '.TB_PREF.'purch_rfq_vendor_lines l INNER JOIN '.TB_PREF.'purch_rfq_vendors v ON v.id=l.rfq_vendor_id WHERE v.rfq_id='.(int)$rfq_id,
			'SELECT custom_data FROM '.TB_PREF.'purch_rfq_vendors WHERE rfq_id='.(int)$rfq_id,
			'SELECT custom_data FROM '.TB_PREF.'purch_rfq_items WHERE rfq_id='.(int)$rfq_id);
		foreach ($targets as $sql) {
			$rows = db_query($sql.' FOR UPDATE');
			if (!$rows) return false;
			while ($row = db_fetch($rows))
				if (!array_key_exists('custom_data', $row)
					|| dimension_master_payload_transition($row['custom_data'], null, true) === false) return false;
		}
		$deletes = array(
			'DELETE l FROM '.TB_PREF.'purch_rfq_vendor_lines l INNER JOIN '.TB_PREF.'purch_rfq_vendors v ON v.id=l.rfq_vendor_id WHERE v.rfq_id='.(int)$rfq_id,
			'DELETE FROM '.TB_PREF.'purch_rfq_vendors WHERE rfq_id='.(int)$rfq_id,
			'DELETE FROM '.TB_PREF.'purch_rfq_items WHERE rfq_id='.(int)$rfq_id,
			'DELETE FROM '.TB_PREF.'purch_rfq WHERE id='.(int)$rfq_id);
		foreach ($deletes as $sql) if (!db_query($sql)) return false;
		return true;
	}, false, true);
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
 * @param string|null $error_message
 * @return int|false
 */
function add_rfq_item($rfq_id, $stock_id, $quantity, $target_price, $specifications = '',
	$description = '', $unit_of_measure = '', $sort_order = 0, $custom_data = array(), &$error_message = null)
{
	$encoded = encode_purch_rfq_custom_data($custom_data);
	if ($encoded === null && !in_array($custom_data, array(null, '', array()), true)) return false;
	return dimension_rfq_child_write('purch_rfq_items', null,
		array('rfq_id'=>$rfq_id, 'custom_data'=>$encoded), function($admitted) use ($rfq_id, $stock_id, $quantity, $target_price, $specifications, $description, $unit_of_measure, $sort_order, $custom_data, &$error_message) {

		$error_message = '';
		$stock_id = trim((string)$stock_id);
		$specifications = trim((string)$specifications);
		$description = trim((string)$description);
		$unit_of_measure = trim((string)$unit_of_measure);

		$rfq = get_editable_purch_rfq($rfq_id);
		if (!$rfq) {
			$existing_rfq = get_purch_rfq($rfq_id);
			if (!$existing_rfq)
				$error_message = _('The selected RFQ could not be found.');
			else
				$error_message = _('Items can only be added while the RFQ is in Draft status.');
			return false;
		}

		if (!can_save_purch_rfq_item_data($stock_id, $quantity, $target_price, $error_message))
			return false;

		$item = stock_master_entity::find($stock_id);
		if (!$item) {
			$error_message = _('The selected item could not be found.');
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
		if (!db_query($sql)) return false;

		return db_insert_id();
	});
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
 * @param string|null $error_message
 * @return bool
 */
function update_rfq_item($item_id, $stock_id, $quantity, $target_price, $specifications = '',
	$description = '', $unit_of_measure = '', $sort_order = 0, $custom_data = null, &$error_message = null)
{
	$next = array();
	if ($custom_data !== null) {
		$next['custom_data'] = encode_purch_rfq_custom_data($custom_data);
		if ($next['custom_data'] === null && !in_array($custom_data, array('', array()), true)) return false;
	}
	return dimension_rfq_child_write('purch_rfq_items', $item_id, $next,
		function($admitted) use ($item_id, $stock_id, $quantity, $target_price, $specifications, $description, $unit_of_measure, $sort_order, $custom_data, &$error_message) {

		$error_message = '';
		$stock_id = trim((string)$stock_id);
		$specifications = trim((string)$specifications);
		$description = trim((string)$description);
		$unit_of_measure = trim((string)$unit_of_measure);

		$sql = "SELECT * FROM " . TB_PREF . "purch_rfq_items WHERE id = " . (int)$item_id . ' FOR UPDATE';
		$result = db_query($sql, 'could not get RFQ item');
		$item_row = db_fetch($result);
		if (!$item_row)
			return false;

		if ($custom_data === null) $custom_data = $item_row['custom_data'];
		$rfq = get_editable_purch_rfq((int)$item_row['rfq_id']);
		if (!$rfq)
			return false;

		if (!can_save_purch_rfq_item_data($stock_id, $quantity, $target_price, $error_message))
			return false;

		$item = stock_master_entity::find($stock_id);
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
		return (bool)db_query($sql);
	});
}

/**
 * Delete an RFQ item.
 *
 * @param int $item_id
 * @return bool
 */
function delete_rfq_item($item_id)
{
	return dimension_rfq_child_write('purch_rfq_items', $item_id, array(), function($admitted) use ($item_id) {
		if (!db_query('DELETE FROM '.TB_PREF.'purch_rfq_vendor_lines WHERE rfq_item_id='.(int)$item_id)) return false;
		return (bool)db_query('DELETE FROM '.TB_PREF.'purch_rfq_items WHERE id='.(int)$item_id);
	}, true);
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
	$encoded = encode_purch_rfq_custom_data($custom_data);
	if ($encoded === null && !in_array($custom_data, array(null, '', array()), true)) return false;
	return dimension_rfq_child_write('purch_rfq_vendors', null,
		array('rfq_id'=>$rfq_id, 'custom_data'=>$encoded), function($admitted) use ($rfq_id, $supplier_id, $custom_data) {

		$rfq = get_editable_purch_rfq($rfq_id);
		if (!$rfq || (int)$supplier_id <= 0)
			return false;

		if (!suppliers_entity::find((int)$supplier_id))
			return false;

		$existing = get_purch_rfq_vendor_by_supplier($rfq_id, $supplier_id);
		if ($existing) {
			if (!db_preserve_insert_id((int)$existing['id'])) return false;
			return (int)$existing['id'];
		}

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
		if (!db_query($sql)) return false;

		return db_insert_id();
	});
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
	// Parent ownership is established before resolving a supplier's current row.
	return dimension_ordinary_master_write('purch_rfq', $rfq_id, array(), function($admitted) use ($rfq_id, $supplier_id) {
		$rows = db_query('SELECT id FROM '.TB_PREF.'purch_rfq_vendors WHERE rfq_id='.(int)$rfq_id.' AND supplier_id='.(int)$supplier_id.' FOR UPDATE');
		$row = $rows ? db_fetch($rows) : false;
		if (!$row) return false;
		$vendor_id = (int)$row['id'];
		return dimension_rfq_child_write('purch_rfq_vendors', $vendor_id, array(), function($admitted) use ($vendor_id) {
			$rows = db_query('SELECT status, is_winner FROM '.TB_PREF.'purch_rfq_vendors WHERE id='.$vendor_id.' FOR UPDATE');
			$vendor = $rows ? db_fetch($rows) : false;
			if (!$vendor || in_array($vendor['status'], array('responded', 'awarded'), true) || (int)$vendor['is_winner'] === 1) return false;
			if (!db_query('DELETE FROM '.TB_PREF.'purch_rfq_vendor_lines WHERE rfq_vendor_id='.$vendor_id)) return false;
			return (bool)db_query('DELETE FROM '.TB_PREF.'purch_rfq_vendors WHERE id='.$vendor_id);
		}, true);
	}, true);
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

/** Preserve native supplier-join admission using a current lock, including inactive history. */
function lock_purch_rfq_activity_supplier($supplier_id)
{
    $rows = db_query('SELECT supplier_id FROM '.TB_PREF.'suppliers WHERE supplier_id='.(int)$supplier_id.' FOR UPDATE');
    return $rows && (bool)db_fetch($rows);
}

/**
 * Own an RFQ activity command under the current parent, item and vendor locks.
 * Native page/session authority is still required. Current locking reads defeat
 * repeatable-read snapshots; every callback failure cancels the whole command.
 * This is sourcing activity, not approval of a resulting purchase order.
 */
function write_purch_rfq_activity($rfq_id, $callback)
{
    return dimension_ordinary_master_write('purch_rfq', $rfq_id, array(),
        function($admitted) use ($rfq_id, $callback) {
            $rows = db_query('SELECT * FROM '.TB_PREF.'purch_rfq WHERE id='.(int)$rfq_id.' FOR UPDATE');
            $rfq = $rows ? db_fetch($rows) : false;
            if (!$rfq || !in_array($rfq['status'], array('draft','sent','received','evaluated'), true)) return false;
            $items = array(); $vendors = array();
            $rows = db_query('SELECT * FROM '.TB_PREF.'purch_rfq_items WHERE rfq_id='.(int)$rfq_id.' ORDER BY id FOR UPDATE');
            if (!$rows) return false;
            while ($row = db_fetch($rows)) $items[(int)$row['id']] = $row;
            $rows = db_query('SELECT * FROM '.TB_PREF.'purch_rfq_vendors WHERE rfq_id='.(int)$rfq_id.' ORDER BY id FOR UPDATE');
            if (!$rows) return false;
            while ($row = db_fetch($rows)) $vendors[(int)$row['id']] = $row;
            return call_user_func($callback, $rfq, $items, $vendors);
        }, true);
}

/** Resolve a vendor parent hint, then recheck the exact identity under its lock. */
function write_purch_rfq_vendor_activity($vendor_id, $callback)
{
    $vendor_id = dimension_writer_reference_id($vendor_id);
    if (!$vendor_id) return false;
    $rows = db_query('SELECT rfq_id FROM '.TB_PREF.'purch_rfq_vendors WHERE id='.$vendor_id);
    $hint = $rows ? db_fetch($rows) : false;
    if (!$hint) return false;
    return write_purch_rfq_activity($hint['rfq_id'], function($rfq, $items, $vendors) use ($vendor_id, $callback) {
        if (!isset($vendors[$vendor_id]) || !lock_purch_rfq_activity_supplier($vendors[$vendor_id]['supplier_id'])) return false;
        return dimension_ordinary_master_write('purch_rfq_vendors', $vendor_id, array(),
            function($admitted) use ($rfq, $items, $vendors, $vendor_id, $callback) {
                return call_user_func($callback, $rfq, $items, $vendors[$vendor_id]);
            }, true);
    });
}

/**
 * Refresh one RFQ vendor total from line responses.
 *
 * @param int $rfq_vendor_id
 * @return float|false
 */
function refresh_purch_rfq_vendor_total($rfq_vendor_id)
{
    $total = 0;
    $ok = write_purch_rfq_vendor_activity($rfq_vendor_id, function($rfq, $items, $vendor) use (&$total) {
        $rows = db_query('SELECT quoted_price, quoted_quantity FROM '.TB_PREF.'purch_rfq_vendor_lines WHERE rfq_vendor_id='.(int)$vendor['id'].' ORDER BY id FOR UPDATE');
        if (!$rows) return false;
        while ($row = db_fetch($rows)) $total += (float)$row['quoted_price'] * (float)$row['quoted_quantity'];
        return (bool)db_query('UPDATE '.TB_PREF.'purch_rfq_vendors SET total_quoted='.db_escape($total).' WHERE id='.(int)$vendor['id']);
    });
    return $ok ? $total : false;
}

/**
 * Update the RFQ header status from vendor activity.
 *
 * @param int $rfq_id
 * @return bool
 */
function update_purch_rfq_status_from_activity($rfq_id)
{
    return write_purch_rfq_activity($rfq_id, function($rfq, $items, $vendors) {
        $sent = false; $responded = false; $evaluated = false; $winner = false;
        foreach ($vendors as $vendor) {
            $winner = $winner || (int)$vendor['is_winner'] === 1;
            $evaluated = $evaluated || (float)$vendor['evaluator_score'] != 0 || (string)$vendor['evaluator_notes'] !== '';
            $responded = $responded || in_array($vendor['status'], array('responded','awarded','rejected'), true);
            $sent = $sent || $vendor['sent_date'] !== null;
        }
        $status = $winner ? 'awarded' : ($evaluated ? 'evaluated' : ($responded ? 'received' : ($sent ? 'sent' : 'draft')));
        return $status === $rfq['status'] || (bool)db_query('UPDATE '.TB_PREF.'purch_rfq SET status='.db_escape($status).' WHERE id='.(int)$rfq['id']);
    });
}

/**
 * Send the RFQ to vendors.
 *
 * @param int   $rfq_id
 * @param array $vendor_ids
 * @param string|null $error_message
 * @return bool
 */
function send_rfq_to_vendors($rfq_id, $vendor_ids = array(), &$error_message = null)
{
    $error_message = '';
    return write_purch_rfq_activity($rfq_id, function($rfq, $items, $vendors) use ($vendor_ids, &$error_message) {
        if (!$items || !$vendors) {
            $error_message = _('Add at least one RFQ item and vendor before sending.'); return false;
        }
        $selected = array();
        foreach ((array)$vendor_ids as $id) {
            $id = dimension_writer_reference_id($id);
            if (!$id || !isset($vendors[$id])) return false;
            $selected[$id] = $id;
        }
        if (!$selected) {
            foreach ($vendors as $id=>$vendor)
                if (lock_purch_rfq_activity_supplier($vendor['supplier_id'])) $selected[] = $id;
        }
        if (!$selected) return false;
        foreach ($selected as $id) {
            if (!lock_purch_rfq_activity_supplier($vendors[$id]['supplier_id'])) return false;
            if (!dimension_ordinary_master_write('purch_rfq_vendors', $id, array(), function($admitted) use ($id) {
                return (bool)db_query("UPDATE ".TB_PREF."purch_rfq_vendors SET sent_date=NOW(), status=CASE WHEN status IN ('responded','awarded') THEN status ELSE 'invited' END WHERE id=".$id);
            }, true)) return false;
        }
        if (!update_purch_rfq_status_from_activity($rfq['id'])) return false;
        $data = array('rfq_id'=>(int)$rfq['id'], 'vendor_ids'=>array_values($selected));
        hook_invoke_all('purch_rfq_sent', $data);
        return true;
    });
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
 * @param string|null $error_message
 * @return bool
 */
function record_vendor_response($rfq_id, $supplier_id, $lines, $total, $lead_days, $notes,
    $payment_terms = '', $custom_data = null, &$error_message = null)
{
    $error_message = '';
    return write_purch_rfq_activity($rfq_id, function($rfq, $items, $vendors) use ($supplier_id, $lines, $total, $lead_days, $notes, $payment_terms, $custom_data, &$error_message) {
        $vendor = false;
        foreach ($vendors as $row) if ((int)$row['supplier_id'] === (int)$supplier_id) $vendor = $row;
        if (!$vendor || (int)$vendor['is_winner'] || $vendor['status'] === 'awarded' || !$items
            || !lock_purch_rfq_activity_supplier($vendor['supplier_id'])) return false;
        if (!is_numeric($total) || !is_finite((float)$total) || (float)$total < 0) return false;
        $lead = trim((string)$lead_days);
        if ($lead !== '' && !preg_match('/^[0-9]+$/D', $lead)) return false;
        $normalized = array();
        if (!normalize_purch_rfq_response_lines($items, $lines, $normalized, $error_message)) return false;
        $rows = db_query('SELECT * FROM '.TB_PREF.'purch_rfq_vendor_lines WHERE rfq_vendor_id='.(int)$vendor['id'].' ORDER BY id FOR UPDATE');
        if (!$rows) return false;
        $old = array();
        while ($row = db_fetch($rows)) {
            $item = (int)$row['rfq_item_id'];
            // A retained response with a missing/foreign item has ambiguous
            // ownership. Replacement must not silently erase that history.
            if (!isset($items[$item]) || isset($old[$item])) return false;
            $old[$item] = $row;
        }
        $seen = array(); $computed = 0;
        foreach ($normalized as $line) {
            $item = (int)$line['rfq_item_id'];
            if (isset($seen[$item])) return false;
            $seen[$item] = true;
            $previous = isset($old[$item]) ? $old[$item] : null;
            $payload = $line['custom_data'] === null ? ($previous ? $previous['custom_data'] : null) : encode_purch_rfq_custom_data($line['custom_data']);
            if ($payload === null && $line['custom_data'] !== null && !in_array($line['custom_data'], array('',array()), true)) return false;
            $ok = dimension_ordinary_master_write('purch_rfq_vendor_lines', $previous ? $previous['id'] : null,
                array('custom_data'=>$payload), function($admitted) use ($previous, $vendor, $item, $line, $payload) {
                    $values = 'quoted_price='.db_escape($line['quoted_price']).', quoted_quantity='.db_escape($line['quoted_quantity'])
                        .', delivery_lead_days='.(int)$line['delivery_lead_days'].', notes='.db_escape($line['notes'])
                        .', custom_data='.sql_purch_rfq_custom_data($payload);
                    $sql = $previous ? 'UPDATE '.TB_PREF.'purch_rfq_vendor_lines SET '.$values.' WHERE id='.(int)$previous['id']
                        : 'INSERT INTO '.TB_PREF.'purch_rfq_vendor_lines SET rfq_vendor_id='.(int)$vendor['id'].', rfq_item_id='.$item.', '.$values;
                    return (bool)db_query($sql);
                }, true);
            if (!$ok) return false;
            $computed += $line['quoted_price'] * $line['quoted_quantity'];
        }
        foreach ($old as $item=>$previous) if (!isset($seen[$item])) {
            if (!dimension_ordinary_master_write('purch_rfq_vendor_lines', $previous['id'], array(), function($admitted) use ($previous) {
                return (bool)db_query('DELETE FROM '.TB_PREF.'purch_rfq_vendor_lines WHERE id='.(int)$previous['id']);
            }, true, true)) return false;
        }
        $payload = $custom_data === null ? $vendor['custom_data'] : encode_purch_rfq_custom_data($custom_data);
        if ($payload === null && $custom_data !== null && !in_array($custom_data, array('',array()), true)) return false;
        $amount = (float)$total > 0 ? (float)$total : $computed;
        if (!is_finite($amount)) return false;
        if (!dimension_ordinary_master_write('purch_rfq_vendors', $vendor['id'], array('custom_data'=>$payload),
            function($admitted) use ($vendor, $amount, $lead, $payment_terms, $notes, $payload) {
                return (bool)db_query("UPDATE ".TB_PREF."purch_rfq_vendors SET status='responded', response_date=NOW(), total_quoted=".db_escape($amount)
                    .', delivery_lead_days='.(int)$lead.', payment_terms='.db_escape($payment_terms).', vendor_notes='.db_escape($notes)
                    .', custom_data='.sql_purch_rfq_custom_data($payload).' WHERE id='.(int)$vendor['id']);
            }, true)) return false;
        if (!update_purch_rfq_status_from_activity($rfq['id'])) return false;
        $data = array('rfq_id'=>(int)$rfq['id'], 'supplier_id'=>(int)$supplier_id, 'rfq_vendor_id'=>(int)$vendor['id']);
        hook_invoke_all('purch_rfq_response_received', $data);
        return true;
    });
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
    if (!is_numeric($score) || !is_finite((float)$score)) return false;
    return write_purch_rfq_vendor_activity($rfq_vendor_id, function($rfq, $items, $vendor) use ($score, $notes) {
        if ($vendor['status'] !== 'responded' || (int)$vendor['is_winner']) return false;
        if (!db_query('UPDATE '.TB_PREF.'purch_rfq_vendors SET evaluator_score='.db_escape($score)
            .', evaluator_notes='.db_escape($notes).' WHERE id='.(int)$vendor['id'])) return false;
        return update_purch_rfq_status_from_activity($rfq['id']);
    });
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
	$vendor_line_maps = array();
	foreach ((array)$line_awards as $rfq_item_id => $vendor_reference) {
		if ((int)$rfq_item_id <= 0 || !in_array((int)$rfq_item_id, $item_ids))
			continue;

		$rfq_vendor = resolve_purch_rfq_vendor_reference($rfq_id, $vendor_reference);
		if (!$rfq_vendor)
			continue;

		$rfq_vendor_id = (int)$rfq_vendor['id'];
		if (!isset($vendor_line_maps[$rfq_vendor_id]))
			$vendor_line_maps[$rfq_vendor_id] = get_rfq_vendor_line_map($rfq_vendor_id);

		if (isset($vendor_line_maps[$rfq_vendor_id][(int)$rfq_item_id]))
			$award_map[(int)$rfq_item_id] = $rfq_vendor_id;
	}

	if (empty($award_map) && (int)$winning_vendor_id > 0) {
		$rfq_vendor = resolve_purch_rfq_vendor_reference($rfq_id, $winning_vendor_id);
		if (!$rfq_vendor)
			return false;

		$rfq_vendor_id = (int)$rfq_vendor['id'];
		if (!isset($vendor_line_maps[$rfq_vendor_id]))
			$vendor_line_maps[$rfq_vendor_id] = get_rfq_vendor_line_map($rfq_vendor_id);

		$vendor_lines = $vendor_line_maps[$rfq_vendor_id];
		foreach ($item_ids as $rfq_item_id) {
			if (isset($vendor_lines[$rfq_item_id]))
				$award_map[$rfq_item_id] = $rfq_vendor_id;
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

	if (!update_purch_rfq_award_map($rfq_id, $award_map)) { cancel_transaction(); return false; }

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
	if (!isset($_SESSION['wa_current_user'])
		|| !$_SESSION['wa_current_user']->can_access('SA_PURCHASEORDER')
		|| !$_SESSION['wa_current_user']->can_access('SA_PURCHRFQ'))
		return false;
	if (!begin_transaction())
		return false;
	try {
		include_once($GLOBALS['path_to_root'].'/purchasing/includes/db/purch_agreement_po_approval_db.inc');
		include_once($GLOBALS['path_to_root'].'/purchasing/includes/db/purch_rfq_po_approval_db.inc');
		$snapshot = prepare_rfq_purchase_order($rfq_id, $vendor_id, $line_ids);
		$purchase_order = $snapshot['cart'];
		if (rfq_purchase_order_has_pending($rfq_id) || !purchase_order_conversion_direct_authorized($purchase_order))
			throw new RuntimeException('RFQ purchase approval is required or pending.');
		$po_number = write_rfq_purchase_order($snapshot);
		if (!purchase_order_conversion_direct_authorized($purchase_order))
			throw new RuntimeException('RFQ purchase policy changed.');

		if (!commit_transaction()) {
			if (!empty($GLOBALS['transaction_level']))
				throw new RuntimeException('Purchase order conversion commit failed.');
			// SQL is committed: publish its identities so callers cannot mistake
			// resource cleanup failure for a rolled-back conversion and retry.
			display_error(_('The purchase order conversion was committed, but resource cleanup needs recovery. Reload the source before continuing.'));
		}
		return (int)$po_number;
	} catch (Exception $exception) {
		cancel_transaction();
		return false;
	} catch (Throwable $exception) {
		cancel_transaction();
		throw $exception;
	}
}

/**
 * Expire overdue RFQs.
 *
 * @return int|false Number expired, or whole-command failure.
 */
function expire_overdue_rfqs()
{
    try {
        if (!begin_transaction()) return false;
        // Enumeration is a hint only. Recheck deadline/status under each owner
        // lock, in ascending identity order, before changing any current row.
        $today = date2sql(Today());
        $rows = db_query("SELECT id FROM ".TB_PREF."purch_rfq WHERE status IN ('draft','sent','received','evaluated') AND deadline_date IS NOT NULL AND deadline_date < ".db_escape($today).' ORDER BY id');
        if (!$rows) { cancel_transaction(); return false; }
        $ids = array();
        while ($row = db_fetch($rows)) $ids[] = (int)$row['id'];
        $count = 0;
        foreach ($ids as $id) {
            if (!dimension_ordinary_master_write('purch_rfq', $id, array(), function($admitted) use ($id, $today, &$count) {
                $rows = db_query('SELECT status, deadline_date FROM '.TB_PREF.'purch_rfq WHERE id='.$id.' FOR UPDATE');
                $row = $rows ? db_fetch($rows) : false;
                if (!$row) return false;
                if (!in_array($row['status'], array('draft','sent','received','evaluated'), true)
                    || $row['deadline_date'] === null || $row['deadline_date'] >= $today) return true;
                if (!db_query("UPDATE ".TB_PREF."purch_rfq SET status='expired' WHERE id=".$id)) return false;
                $count++;
                return true;
            }, true)) { cancel_transaction(); return false; }
        }
        if (!commit_transaction()) { cancel_transaction(); return false; }
        return $count;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
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

	if (!$rfq_id) return false;

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
