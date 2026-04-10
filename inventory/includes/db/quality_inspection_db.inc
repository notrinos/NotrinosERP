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
 * Quality Inspection Database Layer.
 *
 * CRUD operations for quality parameters, inspections, and readings.
 * Handles inspection lifecycle: pending -> pass/fail/partial.
 * Integration with GRN 3-step receipt flow (QC is step 2 of 3).
 *
 * Actual DB columns:
 *   0_quality_parameters: id, name, description, parameter_type, unit,
 *       min_value, max_value, acceptable_values, stock_id, category_id,
 *       mandatory, sort_order, inactive
 *   0_quality_inspections: id, reference, stock_id, inspection_type,
 *       trans_type, trans_no, batch_id, serial_id, loc_code,
 *       inspected_qty, accepted_qty, rejected_qty, result,
 *       inspector_id, inspection_date, completion_date, notes,
 *       custom_data, created_by, created_at, updated_at
 *   0_quality_inspection_readings: id, inspection_id, parameter_id,
 *       reading_value, result, notes
 *
 * @package NotrinosERP
 * @subpackage Inventory/QualityInspection
 */

// =====================================================================
// STATUS / TYPE HELPERS
// =====================================================================

/**
 * Get available inspection result statuses.
 *
 * @return array Associative array of status_code => display_label
 */
function get_qc_result_statuses()
{
	return array(
		'pending'  => _('Pending'),
		'pass'     => _('Pass'),
		'fail'     => _('Fail'),
		'partial'  => _('Partial'),
	);
}

/**
 * Get the display label for a QC result status.
 *
 * @param string $status Status code
 * @return string Translated display label
 */
function get_qc_result_label($status)
{
	$statuses = get_qc_result_statuses();
	return isset($statuses[$status]) ? $statuses[$status] : $status;
}

/**
 * Get the color associated with a QC result status.
 *
 * @param string $status Status code
 * @return string CSS color hex code
 */
function get_qc_result_color($status)
{
	$colors = array(
		'pending'  => '#ffc107',
		'pass'     => '#28a745',
		'fail'     => '#dc3545',
		'partial'  => '#fd7e14',
	);
	return isset($colors[$status]) ? $colors[$status] : '#6c757d';
}

/**
 * Render an HTML badge for a QC result status.
 *
 * @param string $status Status code
 * @return string HTML span element with colored badge
 */
function qc_result_badge($status)
{
	$label = get_qc_result_label($status);
	$color = get_qc_result_color($status);
	return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;'
		. 'background:' . $color . ';color:#fff;font-size:11px;font-weight:600;">'
		. $label . '</span>';
}

/**
 * Get available inspection types.
 *
 * @return array Associative array of type_code => display_label
 */
function get_qc_inspection_types()
{
	return array(
		'incoming'   => _('Incoming (GRN)'),
		'outgoing'   => _('Outgoing (Delivery)'),
		'in_process' => _('In-Process'),
		'periodic'   => _('Periodic'),
	);
}

/**
 * Get the display label for a QC inspection type.
 *
 * @param string $type Type code
 * @return string Translated display label
 */
function get_qc_type_label($type)
{
	$types = get_qc_inspection_types();
	return isset($types[$type]) ? $types[$type] : $type;
}

/**
 * Get the color associated with a QC inspection type.
 *
 * @param string $type Type code
 * @return string CSS color hex code
 */
function get_qc_type_color($type)
{
	$colors = array(
		'incoming'   => '#17a2b8',
		'outgoing'   => '#6f42c1',
		'in_process' => '#fd7e14',
		'periodic'   => '#20c997',
	);
	return isset($colors[$type]) ? $colors[$type] : '#6c757d';
}

/**
 * Render an HTML badge for a QC inspection type.
 *
 * @param string $type Type code
 * @return string HTML span element with colored badge
 */
function qc_type_badge($type)
{
	$label = get_qc_type_label($type);
	$color = get_qc_type_color($type);
	return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;'
		. 'background:' . $color . ';color:#fff;font-size:11px;font-weight:600;">'
		. $label . '</span>';
}

/**
 * Get available parameter types.
 *
 * @return array Associative array of type_code => display_label
 */
function get_qc_parameter_types()
{
	return array(
		'numeric' => _('Numeric'),
		'text'    => _('Text'),
		'boolean' => _('Pass / Fail'),
		'list'    => _('Selection List'),
	);
}

/**
 * Get the display label for a parameter type.
 *
 * @param string $type Parameter type code
 * @return string Translated display label
 */
function get_qc_parameter_type_label($type)
{
	$types = get_qc_parameter_types();
	return isset($types[$type]) ? $types[$type] : $type;
}

// =====================================================================
// QUALITY PARAMETERS CRUD
// =====================================================================

/**
 * Add a new quality parameter definition.
 *
 * @param string      $name             Parameter name
 * @param string      $parameter_type   numeric|text|boolean|list
 * @param string|null $stock_id         Item code (NULL for global)
 * @param int|null    $category_id      Item category (NULL for all)
 * @param float|null  $min_value        Minimum acceptable value (numeric type)
 * @param float|null  $max_value        Maximum acceptable value (numeric type)
 * @param string|null $unit             Unit (e.g., mm, kg, C)
 * @param string|null $acceptable_values JSON for list type
 * @param int         $mandatory        1=required, 0=optional
 * @param int         $sort_order       Display order
 * @param string|null $description      Description/instructions
 * @return int|false  New parameter ID or false on failure
 */
function add_quality_parameter($name, $parameter_type, $stock_id = null, $category_id = null,
	$min_value = null, $max_value = null, $unit = null, $acceptable_values = null,
	$mandatory = 1, $sort_order = 0, $description = null)
{
	$sql = "INSERT INTO " . TB_PREF . "quality_parameters ("
		. "name, parameter_type, stock_id, category_id, "
		. "min_value, max_value, unit, acceptable_values, "
		. "mandatory, sort_order, description, inactive"
		. ") VALUES ("
		. db_escape($name) . ", "
		. db_escape($parameter_type) . ", "
		. ($stock_id ? db_escape($stock_id) : "NULL") . ", "
		. ($category_id ? (int)$category_id : "NULL") . ", "
		. ($min_value !== null && $min_value !== '' ? (float)$min_value : "NULL") . ", "
		. ($max_value !== null && $max_value !== '' ? (float)$max_value : "NULL") . ", "
		. ($unit ? db_escape($unit) : "NULL") . ", "
		. ($acceptable_values ? db_escape($acceptable_values) : "NULL") . ", "
		. (int)$mandatory . ", "
		. (int)$sort_order . ", "
		. ($description ? db_escape($description) : "NULL") . ", "
		. "0)";

	db_query($sql, 'could not add quality parameter');
	return db_insert_id();
}

/**
 * Update an existing quality parameter.
 *
 * @param int         $parameter_id     Parameter ID (maps to column `id`)
 * @param string      $name             Parameter name
 * @param string      $parameter_type   numeric|text|boolean|list
 * @param string|null $stock_id         Item code (NULL for global)
 * @param int|null    $category_id      Item category (NULL for all)
 * @param float|null  $min_value        Minimum acceptable value
 * @param float|null  $max_value        Maximum acceptable value
 * @param string|null $unit             Unit
 * @param string|null $acceptable_values JSON for list type
 * @param int         $mandatory        1=required, 0=optional
 * @param int         $sort_order       Display order
 * @param string|null $description      Description/instructions
 * @return void
 */
function update_quality_parameter($parameter_id, $name, $parameter_type, $stock_id = null,
	$category_id = null, $min_value = null, $max_value = null, $unit = null,
	$acceptable_values = null, $mandatory = 1, $sort_order = 0, $description = null)
{
	$sql = "UPDATE " . TB_PREF . "quality_parameters SET "
		. "name=" . db_escape($name) . ", "
		. "parameter_type=" . db_escape($parameter_type) . ", "
		. "stock_id=" . ($stock_id ? db_escape($stock_id) : "NULL") . ", "
		. "category_id=" . ($category_id ? (int)$category_id : "NULL") . ", "
		. "min_value=" . ($min_value !== null && $min_value !== '' ? (float)$min_value : "NULL") . ", "
		. "max_value=" . ($max_value !== null && $max_value !== '' ? (float)$max_value : "NULL") . ", "
		. "unit=" . ($unit ? db_escape($unit) : "NULL") . ", "
		. "acceptable_values=" . ($acceptable_values ? db_escape($acceptable_values) : "NULL") . ", "
		. "mandatory=" . (int)$mandatory . ", "
		. "sort_order=" . (int)$sort_order . ", "
		. "description=" . ($description ? db_escape($description) : "NULL")
		. " WHERE id=" . (int)$parameter_id;

	db_query($sql, 'could not update quality parameter');
}

/**
 * Get a single quality parameter by ID.
 *
 * @param int $parameter_id Parameter ID
 * @return array|false Parameter row (with `parameter_id` alias) or false
 */
function get_quality_parameter($parameter_id)
{
	$sql = "SELECT *, id AS parameter_id FROM " . TB_PREF . "quality_parameters"
		. " WHERE id=" . (int)$parameter_id;
	$result = db_query($sql, 'could not get quality parameter');
	return db_fetch($result);
}

/**
 * Get quality parameters with optional filters.
 *
 * @param string|null $stock_id    Filter by item (also returns global params)
 * @param int|null    $category_id Filter by category
 * @param string|null $type        Filter by parameter_type
 * @param bool        $show_inactive Include inactive params
 * @return resource   DB result set
 */
function get_quality_parameters($stock_id = null, $category_id = null, $type = null, $show_inactive = false)
{
	$sql = "SELECT qp.*, qp.id AS parameter_id, "
		. "IFNULL(sm.description, " . db_escape(_('Global')) . ") AS item_description, "
		. "IFNULL(sc.description, " . db_escape(_('All Categories')) . ") AS category_description "
		. "FROM " . TB_PREF . "quality_parameters qp "
		. "LEFT JOIN " . TB_PREF . "stock_master sm ON qp.stock_id = sm.stock_id "
		. "LEFT JOIN " . TB_PREF . "stock_category sc ON qp.category_id = sc.category_id "
		. "WHERE 1=1";

	if (!$show_inactive)
		$sql .= " AND qp.inactive=0";

	if ($stock_id)
		$sql .= " AND (qp.stock_id=" . db_escape($stock_id) . " OR qp.stock_id IS NULL)";

	if ($category_id)
		$sql .= " AND (qp.category_id=" . (int)$category_id . " OR qp.category_id IS NULL)";

	if ($type)
		$sql .= " AND qp.parameter_type=" . db_escape($type);

	$sql .= " ORDER BY qp.sort_order, qp.name";

	return db_query($sql, 'could not get quality parameters');
}

/**
 * Get parameters applicable to a specific item (item-specific + global + category match).
 *
 * @param string $stock_id Item code
 * @return resource DB result set
 */
function get_quality_parameters_for_item($stock_id)
{
	$item = get_item($stock_id);
	$category_id = $item ? $item['category_id'] : 0;

	$sql = "SELECT *, id AS parameter_id FROM " . TB_PREF . "quality_parameters "
		. "WHERE inactive=0 AND ("
		. "stock_id=" . db_escape($stock_id)
		. " OR (stock_id IS NULL AND category_id=" . (int)$category_id . ")"
		. " OR (stock_id IS NULL AND (category_id IS NULL OR category_id <= 0))"
		. ") ORDER BY sort_order, name";

	return db_query($sql, 'could not get parameters for item');
}

/**
 * Count applicable parameters for a specific item.
 *
 * @param string $stock_id Item code
 * @return int Number of applicable parameters
 */
function count_quality_parameters_for_item($stock_id)
{
	$result = get_quality_parameters_for_item($stock_id);
	$count = 0;
	while (db_fetch($result))
		$count++;
	return $count;
}

/**
 * Check if a quality parameter can be deleted (not referenced by readings).
 *
 * @param int $parameter_id Parameter ID
 * @return bool True if safe to delete
 */
function can_delete_quality_parameter($parameter_id)
{
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "quality_inspection_readings"
		. " WHERE parameter_id=" . (int)$parameter_id;
	$result = db_query($sql, 'could not check parameter usage');
	$row = db_fetch($result);
	return ($row['cnt'] == 0);
}

/**
 * Delete a quality parameter.
 *
 * @param int $parameter_id Parameter ID
 * @return void
 */
function delete_quality_parameter($parameter_id)
{
	$sql = "DELETE FROM " . TB_PREF . "quality_parameters"
		. " WHERE id=" . (int)$parameter_id;
	db_query($sql, 'could not delete quality parameter');
}

/**
 * Set quality parameter inactive/active status.
 *
 * @param int $parameter_id Parameter ID
 * @param int $inactive     1=inactive, 0=active
 * @return void
 */
function set_quality_parameter_inactive($parameter_id, $inactive)
{
	$sql = "UPDATE " . TB_PREF . "quality_parameters SET inactive=" . (int)$inactive
		. " WHERE id=" . (int)$parameter_id;
	db_query($sql, 'could not set parameter inactive status');
}

// =====================================================================
// QUALITY INSPECTIONS CRUD
// =====================================================================

/**
 * Generate the next inspection reference number (QCI-NNNNNN).
 *
 * @return string Reference string
 */
function get_next_inspection_reference()
{
	$sql = "SELECT MAX(id) AS max_id FROM " . TB_PREF . "quality_inspections";
	$result = db_query($sql, 'could not get max inspection id');
	$row = db_fetch($result);
	$next = ($row && $row['max_id']) ? (int)$row['max_id'] + 1 : 1;
	return 'QCI-' . str_pad($next, 6, '0', STR_PAD_LEFT);
}

/**
 * Create a new quality inspection record.
 *
 * @param string      $stock_id        Item code
 * @param string      $inspection_type incoming|outgoing|in_process|periodic
 * @param int|null    $trans_type      Source transaction type (e.g. ST_SUPPRECEIVE)
 * @param int|null    $trans_no        Source transaction number (e.g. GRN#)
 * @param float       $inspected_qty   Quantity to inspect
 * @param string|null $loc_code        Warehouse location code
 * @param int|null    $batch_id        Batch FK
 * @param int|null    $serial_id       Serial FK
 * @param string|null $notes           Inspector notes
 * @return int|false  New inspection ID or false on failure
 */
function add_quality_inspection($stock_id, $inspection_type, $trans_type = null, $trans_no = null,
	$inspected_qty = 0, $loc_code = null, $batch_id = null, $serial_id = null, $notes = null)
{
	$reference = get_next_inspection_reference();

	$sql = "INSERT INTO " . TB_PREF . "quality_inspections ("
		. "reference, stock_id, inspection_type, trans_type, trans_no, "
		. "inspected_qty, accepted_qty, rejected_qty, "
		. "loc_code, batch_id, serial_id, "
		. "result, inspection_date, inspector_id, notes, "
		. "created_by, created_at"
		. ") VALUES ("
		. db_escape($reference) . ", "
		. db_escape($stock_id) . ", "
		. db_escape($inspection_type) . ", "
		. ($trans_type !== null ? (int)$trans_type : "NULL") . ", "
		. ($trans_no !== null ? (int)$trans_no : "NULL") . ", "
		. (float)$inspected_qty . ", "
		. "0, 0, "
		. ($loc_code ? db_escape($loc_code) : "NULL") . ", "
		. ($batch_id ? (int)$batch_id : "NULL") . ", "
		. ($serial_id ? (int)$serial_id : "NULL") . ", "
		. "'pending', "
		. "CURDATE(), "
		. (int)$_SESSION['wa_current_user']->user . ", "
		. ($notes ? db_escape($notes) : "NULL") . ", "
		. (int)$_SESSION['wa_current_user']->user . ", "
		. "NOW()"
		. ")";

	db_query($sql, 'could not add quality inspection');
	return db_insert_id();
}

/**
 * Update a quality inspection header.
 *
 * @param int         $inspection_id Inspection ID
 * @param float       $inspected_qty Quantity inspected
 * @param string|null $notes         Notes
 * @return void
 */
function update_quality_inspection($inspection_id, $inspected_qty, $notes = null)
{
	$sql = "UPDATE " . TB_PREF . "quality_inspections SET "
		. "inspected_qty=" . (float)$inspected_qty . ", "
		. "notes=" . ($notes ? db_escape($notes) : "NULL") . ", "
		. "updated_at=NOW()"
		. " WHERE id=" . (int)$inspection_id;

	db_query($sql, 'could not update quality inspection');
}

/**
 * Get a single inspection by ID with joined data.
 *
 * @param int $inspection_id Inspection ID
 * @return array|false Inspection row (with `inspection_id` alias) or false
 */
function get_quality_inspection($inspection_id)
{
	$sql = "SELECT qi.*, qi.id AS inspection_id, "
		. "sm.description AS item_description, "
		. "sm.units, "
		. "IFNULL(sb.batch_no, '') AS batch_no, "
		. "IFNULL(sn.serial_no, '') AS serial_no, "
		. "u.real_name AS inspector_name "
		. "FROM " . TB_PREF . "quality_inspections qi "
		. "LEFT JOIN " . TB_PREF . "stock_master sm ON qi.stock_id = sm.stock_id "
		. "LEFT JOIN " . TB_PREF . "stock_batches sb ON qi.batch_id = sb.id "
		. "LEFT JOIN " . TB_PREF . "serial_numbers sn ON qi.serial_id = sn.id "
		. "LEFT JOIN " . TB_PREF . "users u ON qi.inspector_id = u.id "
		. "WHERE qi.id=" . (int)$inspection_id;

	$result = db_query($sql, 'could not get quality inspection');
	return db_fetch($result);
}

/**
 * Get inspections with optional filters and pagination.
 *
 * @param array $filters  Associative filter array
 * @param int   $limit    Max rows
 * @param int   $offset   Offset
 * @return resource DB result set
 */
function get_quality_inspections($filters = array(), $limit = 50, $offset = 0)
{
	$sql = "SELECT qi.*, qi.id AS inspection_id, "
		. "sm.description AS item_description, "
		. "sm.units, "
		. "IFNULL(sb.batch_no, '') AS batch_no, "
		. "IFNULL(sn.serial_no, '') AS serial_no, "
		. "u.real_name AS inspector_name "
		. "FROM " . TB_PREF . "quality_inspections qi "
		. "LEFT JOIN " . TB_PREF . "stock_master sm ON qi.stock_id = sm.stock_id "
		. "LEFT JOIN " . TB_PREF . "stock_batches sb ON qi.batch_id = sb.id "
		. "LEFT JOIN " . TB_PREF . "serial_numbers sn ON qi.serial_id = sn.id "
		. "LEFT JOIN " . TB_PREF . "users u ON qi.inspector_id = u.id "
		. "WHERE 1=1";

	if (!empty($filters['stock_id']))
		$sql .= " AND qi.stock_id=" . db_escape($filters['stock_id']);

	if (!empty($filters['result']))
		$sql .= " AND qi.result=" . db_escape($filters['result']);

	if (!empty($filters['inspection_type']))
		$sql .= " AND qi.inspection_type=" . db_escape($filters['inspection_type']);

	if (!empty($filters['trans_type']))
		$sql .= " AND qi.trans_type=" . (int)$filters['trans_type'];

	if (!empty($filters['trans_no']))
		$sql .= " AND qi.trans_no=" . (int)$filters['trans_no'];

	if (!empty($filters['loc_code']))
		$sql .= " AND qi.loc_code=" . db_escape($filters['loc_code']);

	if (!empty($filters['date_from']))
		$sql .= " AND qi.inspection_date >= " . db_escape($filters['date_from']);

	if (!empty($filters['date_to']))
		$sql .= " AND qi.inspection_date <= " . db_escape($filters['date_to']);

	$sql .= " ORDER BY qi.inspection_date DESC, qi.id DESC";

	if ($limit > 0)
		$sql .= " LIMIT " . (int)$offset . ", " . (int)$limit;

	return db_query($sql, 'could not get quality inspections');
}

/**
 * Count inspections matching filters.
 *
 * @param array $filters Associative filter array
 * @return int Count
 */
function count_quality_inspections($filters = array())
{
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "quality_inspections qi WHERE 1=1";

	if (!empty($filters['stock_id']))
		$sql .= " AND qi.stock_id=" . db_escape($filters['stock_id']);

	if (!empty($filters['result']))
		$sql .= " AND qi.result=" . db_escape($filters['result']);

	if (!empty($filters['inspection_type']))
		$sql .= " AND qi.inspection_type=" . db_escape($filters['inspection_type']);

	if (!empty($filters['trans_type']))
		$sql .= " AND qi.trans_type=" . (int)$filters['trans_type'];

	if (!empty($filters['trans_no']))
		$sql .= " AND qi.trans_no=" . (int)$filters['trans_no'];

	$result = db_query($sql, 'could not count quality inspections');
	$row = db_fetch($result);
	return (int)$row['cnt'];
}

/**
 * Check if an inspection can be deleted.
 *
 * @param int $inspection_id Inspection ID
 * @return bool True if pending (no completed readings)
 */
function can_delete_quality_inspection($inspection_id)
{
	$insp = get_quality_inspection($inspection_id);
	if (!$insp) return false;
	return ($insp['result'] === 'pending');
}

/**
 * Delete a quality inspection and its readings.
 *
 * @param int $inspection_id Inspection ID
 * @return void
 */
function delete_quality_inspection($inspection_id)
{
	begin_transaction();

	$sql = "DELETE FROM " . TB_PREF . "quality_inspection_readings"
		. " WHERE inspection_id=" . (int)$inspection_id;
	db_query($sql, 'could not delete inspection readings');

	$sql = "DELETE FROM " . TB_PREF . "quality_inspections"
		. " WHERE id=" . (int)$inspection_id;
	db_query($sql, 'could not delete quality inspection');

	commit_transaction();
}

/**
 * Get inspections for a GRN (by trans_type=ST_SUPPRECEIVE and trans_no).
 *
 * @param int $grn_no GRN number
 * @return resource DB result set
 */
function get_inspections_for_grn($grn_no)
{
	return get_quality_inspections(array(
		'trans_type' => ST_SUPPRECEIVE,
		'trans_no'   => $grn_no,
	), 100);
}

/**
 * Get inspection status summary (counts by result).
 *
 * @return array Associative array of result => count
 */
function get_inspection_status_summary()
{
	$sql = "SELECT result, COUNT(*) AS cnt FROM " . TB_PREF . "quality_inspections"
		. " GROUP BY result";
	$result = db_query($sql, 'could not get inspection status summary');

	$summary = array('pending' => 0, 'pass' => 0, 'fail' => 0, 'partial' => 0);
	while ($row = db_fetch($result)) {
		$summary[$row['result']] = (int)$row['cnt'];
	}
	return $summary;
}

// =====================================================================
// QUALITY INSPECTION READINGS
// =====================================================================

/**
 * Record a reading for an inspection parameter (upsert).
 *
 * @param int         $inspection_id Inspection ID
 * @param int         $parameter_id  Parameter ID
 * @param string      $reading_value The measured/observed value
 * @param string      $result        pass|fail
 * @param string|null $notes         Reading notes
 * @return void
 */
function record_inspection_reading($inspection_id, $parameter_id, $reading_value, $result, $notes = null)
{
	$sql = "SELECT id FROM " . TB_PREF . "quality_inspection_readings"
		. " WHERE inspection_id=" . (int)$inspection_id
		. " AND parameter_id=" . (int)$parameter_id;
	$existing = db_query($sql, 'could not check existing reading');
	$row = db_fetch($existing);

	if ($row) {
		$sql = "UPDATE " . TB_PREF . "quality_inspection_readings SET "
			. "reading_value=" . db_escape($reading_value) . ", "
			. "result=" . db_escape($result) . ", "
			. "notes=" . ($notes ? db_escape($notes) : "NULL")
			. " WHERE id=" . (int)$row['id'];
	} else {
		$sql = "INSERT INTO " . TB_PREF . "quality_inspection_readings ("
			. "inspection_id, parameter_id, reading_value, result, notes"
			. ") VALUES ("
			. (int)$inspection_id . ", "
			. (int)$parameter_id . ", "
			. db_escape($reading_value) . ", "
			. db_escape($result) . ", "
			. ($notes ? db_escape($notes) : "NULL")
			. ")";
	}

	db_query($sql, 'could not record inspection reading');
}

/**
 * Get all readings for an inspection, joined with parameter details.
 *
 * @param int $inspection_id Inspection ID
 * @return resource DB result set
 */
function get_inspection_readings($inspection_id)
{
	$sql = "SELECT r.*, r.parameter_id, "
		. "p.name AS parameter_name, "
		. "p.parameter_type, "
		. "p.min_value, "
		. "p.max_value, "
		. "p.unit, "
		. "p.acceptable_values, "
		. "p.mandatory "
		. "FROM " . TB_PREF . "quality_inspection_readings r "
		. "JOIN " . TB_PREF . "quality_parameters p ON r.parameter_id = p.id "
		. "WHERE r.inspection_id=" . (int)$inspection_id
		. " ORDER BY p.sort_order, p.name";

	return db_query($sql, 'could not get inspection readings');
}

/**
 * Count readings for an inspection.
 *
 * @param int $inspection_id Inspection ID
 * @return array Array with 'total', 'pass', 'fail' counts
 */
function count_inspection_readings($inspection_id)
{
	$sql = "SELECT "
		. "COUNT(*) AS total, "
		. "SUM(IF(result='pass',1,0)) AS pass_count, "
		. "SUM(IF(result='fail',1,0)) AS fail_count "
		. "FROM " . TB_PREF . "quality_inspection_readings "
		. "WHERE inspection_id=" . (int)$inspection_id;

	$result = db_query($sql, 'could not count inspection readings');
	$row = db_fetch($result);
	return array(
		'total' => (int)$row['total'],
		'pass'  => (int)$row['pass_count'],
		'fail'  => (int)$row['fail_count'],
	);
}

// =====================================================================
// INSPECTION WORKFLOW / LIFECYCLE
// =====================================================================

/**
 * Auto-evaluate a reading result for a numeric parameter.
 *
 * @param float      $value     Reading value
 * @param float|null $min_value Minimum acceptable (NULL = no min)
 * @param float|null $max_value Maximum acceptable (NULL = no max)
 * @return string 'pass' or 'fail'
 */
function evaluate_numeric_reading($value, $min_value, $max_value)
{
	if ($min_value !== null && $min_value !== '' && (float)$value < (float)$min_value)
		return 'fail';
	if ($max_value !== null && $max_value !== '' && (float)$value > (float)$max_value)
		return 'fail';
	return 'pass';
}

/**
 * Auto-evaluate a reading result for a list parameter.
 *
 * @param string      $value             Reading value
 * @param string|null $acceptable_values  JSON array of acceptable values
 * @return string 'pass' or 'fail'
 */
function evaluate_list_reading($value, $acceptable_values)
{
	if (!$acceptable_values)
		return 'pass';

	$decoded = html_entity_decode($acceptable_values, ENT_QUOTES, 'UTF-8');
	$list = json_decode($decoded, true);
	if (!is_array($list))
		return 'pass';

	$trimmed_list = array_map('trim', $list);
	return in_array(trim($value), $trimmed_list) ? 'pass' : 'fail';
}

/**
 * Complete an inspection: determine overall result, update accepted/rejected quantities,
 * update GRN inspection_status, and handle batch/serial status changes.
 *
 * @param int    $inspection_id Inspection ID
 * @param string $result        'pass' or 'fail' (or auto-determined if empty)
 * @param float  $accepted_qty  Accepted quantity
 * @param float  $rejected_qty  Rejected quantity
 * @return array Result array with 'success', 'result', 'message'
 */
function complete_quality_inspection($inspection_id, $result = '', $accepted_qty = 0, $rejected_qty = 0)
{
	$return = array('success' => false, 'result' => '', 'message' => '');

	$insp = get_quality_inspection($inspection_id);
	if (!$insp) {
		$return['message'] = _('Inspection not found.');
		return $return;
	}

	begin_transaction();

	// Auto-determine result from readings if not specified
	if (empty($result)) {
		$counts = count_inspection_readings($inspection_id);
		if ($counts['total'] == 0) {
			$return['message'] = _('No readings recorded. Enter at least one reading before completing.');
			return $return;
		}
		if ($counts['fail'] > 0 && $counts['pass'] > 0) {
			$result = 'partial';
		} elseif ($counts['fail'] > 0) {
			$result = 'fail';
		} else {
			$result = 'pass';
		}
	}

	// Auto-calculate accepted/rejected if not explicitly set
	if ($accepted_qty == 0 && $rejected_qty == 0) {
		if ($result === 'pass') {
			$accepted_qty = $insp['inspected_qty'];
			$rejected_qty = 0;
		} elseif ($result === 'fail') {
			$accepted_qty = 0;
			$rejected_qty = $insp['inspected_qty'];
		} elseif ($result === 'partial') {
			$accepted_qty = $insp['inspected_qty'];
			$rejected_qty = 0;
		}
	}

	// Update inspection record
	$sql = "UPDATE " . TB_PREF . "quality_inspections SET "
		. "result=" . db_escape($result) . ", "
		. "accepted_qty=" . (float)$accepted_qty . ", "
		. "rejected_qty=" . (float)$rejected_qty . ", "
		. "completion_date=CURDATE(), "
		. "updated_at=NOW()"
		. " WHERE id=" . (int)$inspection_id;
	db_query($sql, 'could not complete inspection');

	// Update GRN inspection_status if this is an incoming inspection
	if ($insp['trans_type'] == ST_SUPPRECEIVE && $insp['trans_no']) {
		$grn_status = ($result === 'pass') ? 'pass' : (($result === 'fail') ? 'fail' : 'pass');
		$sql = "UPDATE " . TB_PREF . "grn_items SET inspection_status=" . db_escape($grn_status)
			. " WHERE grn_batch_id=" . (int)$insp['trans_no']
			. " AND item_code=" . db_escape($insp['stock_id']);
		db_query($sql, 'could not update grn inspection status');
	}

	// Update batch status if batch-tracked and failed
	if ($insp['batch_id'] && $result === 'fail') {
		$batch_db_path = dirname(__FILE__) . '/stock_batches_db.inc';
		if (file_exists($batch_db_path)) {
			include_once($batch_db_path);
			if (function_exists('update_batch_status')) {
				update_batch_status($insp['batch_id'], 'quarantine',
					'quality_check', ST_SUPPRECEIVE, $insp['trans_no'],
					$insp['loc_code'], $insp['loc_code'],
					sprintf(_('QC failed - Inspection #%d'), $inspection_id));
			}
		}
	}

	// Update serial status if serial-tracked and failed
	if ($insp['serial_id'] && $result === 'fail') {
		$serial_db_path = dirname(__FILE__) . '/serial_numbers_db.inc';
		if (file_exists($serial_db_path)) {
			include_once($serial_db_path);
			if (function_exists('update_serial_status')) {
				update_serial_status($insp['serial_id'], 'defective',
					'quality_check', ST_SUPPRECEIVE, $insp['trans_no'],
					$insp['loc_code'], $insp['loc_code'],
					sprintf(_('QC failed - Inspection #%d'), $inspection_id));
			}
		}
	}

	// Complete the WMS quality_check operation if exists
	$wh_ops_db = dirname(dirname(dirname(__FILE__))) . '/warehouse/includes/db/warehouse_operations_db.inc';
	if (file_exists($wh_ops_db)) {
		include_once($wh_ops_db);
		if (function_exists('get_wh_operations_for_document') && function_exists('complete_wh_operation')) {
			$ops = get_wh_operations_for_document(ST_SUPPRECEIVE, $insp['trans_no']);
			while ($op = db_fetch($ops)) {
				if ($op['op_type'] === 'quality_check'
					&& in_array($op['op_status'], array('draft', 'ready', 'in_progress')))
				{
					if ($result === 'pass' || $result === 'partial') {
						complete_wh_operation($op['op_id']);
					} else {
						cancel_wh_operation($op['op_id']);
					}
				}
			}
		}
	}

	commit_transaction();

	$return['success'] = true;
	$return['result'] = $result;
	$return['message'] = sprintf(_('Inspection #%d completed with result: %s'),
		$inspection_id, get_qc_result_label($result));

	return $return;
}

/**
 * Auto-create inspections from a GRN for items that require QC.
 *
 * @param int    $grn_no    GRN batch ID
 * @param string $loc_code  Warehouse location code
 * @return array Created inspection IDs
 */
function auto_create_inspections_from_grn($grn_no, $loc_code)
{
	$created = array();

	$sql = "SELECT gi.id AS grn_item_id, gi.item_code, gi.qty_recd, "
		. "gi.batch_id, gi.serial_numbers_json, "
		. "sm.quality_inspection_required, sm.track_by "
		. "FROM " . TB_PREF . "grn_items gi "
		. "JOIN " . TB_PREF . "stock_master sm ON gi.item_code = sm.stock_id "
		. "WHERE gi.grn_batch_id=" . (int)$grn_no
		. " AND sm.quality_inspection_required=1";

	$result = db_query($sql, 'could not get GRN items for QC');

	while ($row = db_fetch($result)) {
		// Check if inspection already exists for this GRN + item
		$check_sql = "SELECT id FROM " . TB_PREF . "quality_inspections "
			. "WHERE trans_type=" . ST_SUPPRECEIVE
			. " AND trans_no=" . (int)$grn_no
			. " AND stock_id=" . db_escape($row['item_code']);
		$existing = db_query($check_sql, 'could not check existing inspection');
		if (db_fetch($existing))
			continue;

		$batch_id = !empty($row['batch_id']) ? (int)$row['batch_id'] : null;
		$serial_id = null;

		// For serial-tracked items, create one inspection per serial
		if (in_array($row['track_by'], array('serial', 'serial_batch')) && !empty($row['serial_numbers_json'])) {
			$json = html_entity_decode($row['serial_numbers_json'], ENT_QUOTES, 'UTF-8');
			$serial_ids = json_decode($json, true);
			if (is_array($serial_ids)) {
				foreach ($serial_ids as $sid) {
					$insp_id = add_quality_inspection(
						$row['item_code'],
						'incoming',
						ST_SUPPRECEIVE,
						$grn_no,
						1,
						$loc_code,
						$batch_id,
						(int)$sid
					);
					if ($insp_id)
						$created[] = $insp_id;
				}
				continue;
			}
		}

		// Non-serial: one inspection for the full quantity
		$insp_id = add_quality_inspection(
			$row['item_code'],
			'incoming',
			ST_SUPPRECEIVE,
			$grn_no,
			$row['qty_recd'],
			$loc_code,
			$batch_id,
			$serial_id
		);
		if ($insp_id)
			$created[] = $insp_id;
	}

	return $created;
}

/**
 * Get pending inspections count (for dashboard/badge display).
 *
 * @return int Number of pending inspections
 */
function get_pending_inspections_count()
{
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "quality_inspections"
		. " WHERE result='pending'";
	$result = db_query($sql, 'could not count pending inspections');
	$row = db_fetch($result);
	return (int)$row['cnt'];
}

/**
 * Get quality inspection data formatted for Certificate of Analysis PDF.
 *
 * @param int $inspection_id Inspection ID
 * @return array|false COA data array or false
 */
function get_coa_data($inspection_id)
{
	$insp = get_quality_inspection($inspection_id);
	if (!$insp) return false;

	$readings = get_inspection_readings($inspection_id);
	$reading_list = array();
	while ($r = db_fetch($readings))
		$reading_list[] = $r;

	$counts = count_inspection_readings($inspection_id);

	// Get supplier info if GRN-based
	$supplier_name = '';
	if ($insp['trans_type'] == ST_SUPPRECEIVE && $insp['trans_no']) {
		$sql = "SELECT s.supp_name "
			. "FROM " . TB_PREF . "grn_batch gb "
			. "JOIN " . TB_PREF . "suppliers s ON gb.supplier_id = s.supplier_id "
			. "WHERE gb.id=" . (int)$insp['trans_no'];
		$sup_result = db_query($sql, 'could not get supplier');
		$sup_row = db_fetch($sup_result);
		if ($sup_row) $supplier_name = $sup_row['supp_name'];
	}

	return array(
		'inspection'    => $insp,
		'readings'      => $reading_list,
		'counts'        => $counts,
		'supplier_name' => $supplier_name,
	);
}

