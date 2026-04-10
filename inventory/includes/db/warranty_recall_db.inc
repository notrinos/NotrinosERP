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
 * Warranty Claims & Recall Campaigns Database Layer.
 *
 * Tables: 0_warranty_claims, 0_warranty_claim_parts, 0_recall_campaigns, 0_recall_items
 * Dependencies: serial_numbers_db.inc, stock_batches_db.inc
 *
 * @package NotrinosERP
 * @subpackage Inventory/WarrantyRecall
 */

// =====================================================================
// WARRANTY CLAIM STATUS / TYPE HELPERS
// =====================================================================

/**
 * Get valid warranty claim statuses.
 *
 * @return array Associative array of status_code => display_label
 */
function get_warranty_claim_statuses()
{
	return array(
		'open'         => _('Open'),
		'acknowledged' => _('Acknowledged'),
		'in_repair'    => _('In Repair'),
		'replaced'     => _('Replaced'),
		'resolved'     => _('Resolved'),
		'rejected'     => _('Rejected'),
		'closed'       => _('Closed'),
	);
}

/**
 * Get display label for a warranty claim status.
 *
 * @param string $status Status code
 * @return string Translated label
 */
function get_warranty_claim_status_label($status)
{
	$statuses = get_warranty_claim_statuses();
	return isset($statuses[$status]) ? $statuses[$status] : $status;
}

/**
 * Get CSS color for a warranty claim status badge.
 *
 * @param string $status Status code
 * @return string CSS color
 */
function get_warranty_claim_status_color($status)
{
	$colors = array(
		'open'         => '#dc3545',
		'acknowledged' => '#fd7e14',
		'in_repair'    => '#ffc107',
		'replaced'     => '#17a2b8',
		'resolved'     => '#28a745',
		'rejected'     => '#6c757d',
		'closed'       => '#343a40',
	);
	return isset($colors[$status]) ? $colors[$status] : '#6c757d';
}

/**
 * Get valid warranty issue types.
 *
 * @return array Associative array of type_code => display_label
 */
function get_warranty_issue_types()
{
	return array(
		'defective'     => _('Defective'),
		'damaged'       => _('Damaged'),
		'malfunction'   => _('Malfunction'),
		'missing_parts' => _('Missing Parts'),
		'other'         => _('Other'),
	);
}

/**
 * Get display label for a warranty issue type.
 *
 * @param string $type Issue type code
 * @return string Translated label
 */
function get_warranty_issue_type_label($type)
{
	$types = get_warranty_issue_types();
	return isset($types[$type]) ? $types[$type] : $type;
}

// =====================================================================
// WARRANTY CLAIM CRUD
// =====================================================================

/**
 * Generate the next warranty claim reference number.
 *
 * Format: WC-{YYYY}-{5-digit-sequence}
 *
 * @return string Next reference
 */
function get_next_warranty_claim_reference()
{
	$year = date('Y');
	$sql = "SELECT reference FROM " . TB_PREF . "warranty_claims
		WHERE reference LIKE " . db_escape('WC-' . $year . '-%')
		. " ORDER BY id DESC LIMIT 1";
	$result = db_query($sql, 'could not get last warranty claim reference');
	$row = db_fetch_row($result);
	if ($row) {
		$parts = explode('-', $row[0]);
		$seq = (int)end($parts) + 1;
	} else {
		$seq = 1;
	}
	return 'WC-' . $year . '-' . str_pad($seq, 5, '0', STR_PAD_LEFT);
}

/**
 * Add a new warranty claim.
 *
 * @param string $reference      Claim reference
 * @param string $stock_id       Item code
 * @param int    $customer_id    Customer debtor_no
 * @param string $claim_date     Claim date
 * @param string $issue_type     Issue type code
 * @param string $issue_description Issue description text
 * @param int    $serial_id      Serial number id (nullable)
 * @param int    $batch_id       Batch id (nullable)
 * @param int    $warranty_valid Whether claim is within warranty period
 * @param int    $is_chargeable  Whether to bill customer
 * @param int    $assigned_to    Technician user id (nullable)
 * @param string $notes          Additional notes
 * @return int   Inserted claim id
 */
function add_warranty_claim($reference, $stock_id, $customer_id, $claim_date,
	$issue_type, $issue_description, $serial_id = null, $batch_id = null,
	$warranty_valid = 1, $is_chargeable = 0, $assigned_to = null, $notes = null)
{
	$sql = "INSERT INTO " . TB_PREF . "warranty_claims (
		reference, stock_id, customer_id, claim_date, issue_type, issue_description,
		serial_id, batch_id, warranty_valid, is_chargeable, assigned_to, notes,
		status, created_by, created_at
	) VALUES ("
		. db_escape($reference) . ","
		. db_escape($stock_id) . ","
		. (int)$customer_id . ","
		. db_escape(date2sql($claim_date)) . ","
		. db_escape($issue_type) . ","
		. db_escape($issue_description) . ","
		. ($serial_id ? (int)$serial_id : 'NULL') . ","
		. ($batch_id ? (int)$batch_id : 'NULL') . ","
		. (int)$warranty_valid . ","
		. (int)$is_chargeable . ","
		. ($assigned_to ? (int)$assigned_to : 'NULL') . ","
		. db_escape($notes) . ","
		. "'open',"
		. (int)$_SESSION['wa_current_user']->user . ","
		. "NOW()"
		. ")";
	db_query($sql, 'could not add warranty claim');
	return db_insert_id();
}

/**
 * Update an existing warranty claim.
 *
 * @param int    $id                Claim id
 * @param string $stock_id          Item code
 * @param int    $customer_id       Customer debtor_no
 * @param string $claim_date        Claim date
 * @param string $issue_type        Issue type code
 * @param string $issue_description Issue description
 * @param int    $serial_id         Serial number id (nullable)
 * @param int    $batch_id          Batch id (nullable)
 * @param int    $warranty_valid    Within warranty period
 * @param int    $is_chargeable     Bill customer
 * @param int    $assigned_to       Technician user id (nullable)
 * @param string $notes             Additional notes
 */
function update_warranty_claim($id, $stock_id, $customer_id, $claim_date,
	$issue_type, $issue_description, $serial_id = null, $batch_id = null,
	$warranty_valid = 1, $is_chargeable = 0, $assigned_to = null, $notes = null)
{
	$sql = "UPDATE " . TB_PREF . "warranty_claims SET "
		. "stock_id=" . db_escape($stock_id) . ","
		. "customer_id=" . (int)$customer_id . ","
		. "claim_date=" . db_escape(date2sql($claim_date)) . ","
		. "issue_type=" . db_escape($issue_type) . ","
		. "issue_description=" . db_escape($issue_description) . ","
		. "serial_id=" . ($serial_id ? (int)$serial_id : 'NULL') . ","
		. "batch_id=" . ($batch_id ? (int)$batch_id : 'NULL') . ","
		. "warranty_valid=" . (int)$warranty_valid . ","
		. "is_chargeable=" . (int)$is_chargeable . ","
		. "assigned_to=" . ($assigned_to ? (int)$assigned_to : 'NULL') . ","
		. "notes=" . db_escape($notes)
		. " WHERE id=" . (int)$id;
	db_query($sql, 'could not update warranty claim');
}

/**
 * Update warranty claim status with optional resolution fields.
 *
 * @param int    $id                    Claim id
 * @param string $new_status            New status code
 * @param string $resolution_description Resolution description (nullable)
 * @param string $resolution_date       Resolution date (nullable)
 * @param float  $repair_cost           Total repair cost
 * @param int    $replacement_serial_id  Replacement serial id (nullable)
 */
function update_warranty_claim_status($id, $new_status, $resolution_description = null,
	$resolution_date = null, $repair_cost = null, $replacement_serial_id = null)
{
	$sets = array("status=" . db_escape($new_status));

	if ($resolution_description !== null)
		$sets[] = "resolution_description=" . db_escape($resolution_description);
	if ($resolution_date !== null && $resolution_date !== '')
		$sets[] = "resolution_date=" . db_escape(date2sql($resolution_date));
	elseif ($resolution_date === '')
		$sets[] = "resolution_date=NULL";
	if ($repair_cost !== null)
		$sets[] = "repair_cost=" . db_escape($repair_cost);
	if ($replacement_serial_id !== null)
		$sets[] = "replacement_serial_id=" . ($replacement_serial_id ? (int)$replacement_serial_id : 'NULL');

	$sql = "UPDATE " . TB_PREF . "warranty_claims SET "
		. implode(',', $sets) . " WHERE id=" . (int)$id;
	db_query($sql, 'could not update warranty claim status');
}

/**
 * Get a single warranty claim by id.
 *
 * @param int $id Claim id
 * @return array|false Claim row with joins or false
 */
function get_warranty_claim($id)
{
	$sql = "SELECT wc.*,
		sm.description AS item_description,
		dm.name AS customer_name,
		sn.serial_no,
		sb.batch_no,
		rsn.serial_no AS replacement_serial_no,
		CONCAT(u.real_name) AS assigned_to_name,
		CONCAT(uc.real_name) AS created_by_name
		FROM " . TB_PREF . "warranty_claims wc
		LEFT JOIN " . TB_PREF . "stock_master sm ON wc.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "debtors_master dm ON wc.customer_id = dm.debtor_no
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON wc.serial_id = sn.id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON wc.batch_id = sb.id
		LEFT JOIN " . TB_PREF . "serial_numbers rsn ON wc.replacement_serial_id = rsn.id
		LEFT JOIN " . TB_PREF . "users u ON wc.assigned_to = u.id
		LEFT JOIN " . TB_PREF . "users uc ON wc.created_by = uc.id
		WHERE wc.id=" . (int)$id;
	$result = db_query($sql, 'could not get warranty claim');
	return db_fetch($result);
}

/**
 * Get warranty claims list with filters and pagination.
 *
 * @param string $stock_id      Filter by item code
 * @param int    $customer_id   Filter by customer
 * @param string $status        Filter by status
 * @param string $date_from     Filter from date
 * @param string $date_to       Filter to date
 * @param int    $limit         Rows per page (0 = all)
 * @param int    $offset        Pagination offset
 * @return resource DB result set
 */
function get_warranty_claims($stock_id = '', $customer_id = 0, $status = '',
	$date_from = '', $date_to = '', $limit = 0, $offset = 0)
{
	$sql = "SELECT wc.*, sm.description AS item_description,
		dm.name AS customer_name,
		sn.serial_no, sb.batch_no,
		CONCAT(u.real_name) AS assigned_to_name
		FROM " . TB_PREF . "warranty_claims wc
		LEFT JOIN " . TB_PREF . "stock_master sm ON wc.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "debtors_master dm ON wc.customer_id = dm.debtor_no
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON wc.serial_id = sn.id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON wc.batch_id = sb.id
		LEFT JOIN " . TB_PREF . "users u ON wc.assigned_to = u.id
		WHERE 1=1";

	if ($stock_id != '')
		$sql .= " AND wc.stock_id=" . db_escape($stock_id);
	if ($customer_id > 0)
		$sql .= " AND wc.customer_id=" . (int)$customer_id;
	if ($status != '')
		$sql .= " AND wc.status=" . db_escape($status);
	if ($date_from != '')
		$sql .= " AND wc.claim_date >= " . db_escape(date2sql($date_from));
	if ($date_to != '')
		$sql .= " AND wc.claim_date <= " . db_escape(date2sql($date_to));

	$sql .= " ORDER BY wc.id DESC";

	if ($limit > 0)
		$sql .= " LIMIT " . (int)$offset . "," . (int)$limit;

	return db_query($sql, 'could not get warranty claims');
}

/**
 * Count warranty claims matching filters.
 *
 * @param string $stock_id    Filter by item
 * @param int    $customer_id Filter by customer
 * @param string $status      Filter by status
 * @return int Count
 */
function count_warranty_claims($stock_id = '', $customer_id = 0, $status = '')
{
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "warranty_claims WHERE 1=1";
	if ($stock_id != '')
		$sql .= " AND stock_id=" . db_escape($stock_id);
	if ($customer_id > 0)
		$sql .= " AND customer_id=" . (int)$customer_id;
	if ($status != '')
		$sql .= " AND status=" . db_escape($status);
	$result = db_query($sql, 'could not count warranty claims');
	$row = db_fetch_row($result);
	return (int)$row[0];
}

/**
 * Get warranty claim status summary counts.
 *
 * @return array Associative array of status => count
 */
function get_warranty_claim_summary()
{
	$sql = "SELECT status, COUNT(*) AS cnt FROM " . TB_PREF . "warranty_claims GROUP BY status";
	$result = db_query($sql, 'could not get warranty claim summary');
	$summary = array();
	while ($row = db_fetch($result))
		$summary[$row['status']] = (int)$row['cnt'];
	return $summary;
}

/**
 * Check if a warranty claim can be deleted (only open claims with no parts).
 *
 * @param int $id Claim id
 * @return bool|string True if deletable, error message if not
 */
function can_delete_warranty_claim($id)
{
	$claim = get_warranty_claim($id);
	if (!$claim)
		return _('Warranty claim not found.');

	if ($claim['status'] !== 'open' && $claim['status'] !== 'acknowledged')
		return _('Only open or acknowledged claims can be deleted.');

	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "warranty_claim_parts WHERE claim_id=" . (int)$id;
	$result = db_query($sql, 'could not check warranty claim parts');
	$row = db_fetch_row($result);
	if ($row[0] > 0)
		return _('Cannot delete a claim that has repair parts recorded.');

	return true;
}

/**
 * Delete a warranty claim and its parts.
 *
 * @param int $id Claim id
 */
function delete_warranty_claim($id)
{
	begin_transaction();
	$sql = "DELETE FROM " . TB_PREF . "warranty_claim_parts WHERE claim_id=" . (int)$id;
	db_query($sql, 'could not delete warranty claim parts');
	$sql = "DELETE FROM " . TB_PREF . "warranty_claims WHERE id=" . (int)$id;
	db_query($sql, 'could not delete warranty claim');
	commit_transaction();
}

/**
 * Check warranty validity for a serial number based on warranty end date.
 *
 * @param int $serial_id Serial number id
 * @return array ['valid' => bool, 'warranty_end' => date string or null, 'days_remaining' => int or null]
 */
function check_warranty_validity($serial_id)
{
	$sql = "SELECT warranty_end FROM " . TB_PREF . "serial_numbers WHERE id=" . (int)$serial_id;
	$result = db_query($sql, 'could not check warranty');
	$row = db_fetch($result);

	if (!$row || !$row['warranty_end'])
		return array('valid' => false, 'warranty_end' => null, 'days_remaining' => null);

	$today = date('Y-m-d');
	$warranty_end = $row['warranty_end'];
	$days_remaining = (int)((strtotime($warranty_end) - strtotime($today)) / 86400);

	return array(
		'valid' => ($days_remaining >= 0),
		'warranty_end' => sql2date($warranty_end),
		'days_remaining' => $days_remaining,
	);
}

/**
 * Get customer's delivered serials for warranty claim selection.
 *
 * Returns serials where customer_id matches and status is 'delivered'.
 *
 * @param int    $customer_id Customer debtor_no
 * @param string $stock_id    Optional item filter
 * @return resource DB result
 */
function get_customer_delivered_serials($customer_id, $stock_id = '')
{
	$sql = "SELECT sn.id, sn.serial_no, sn.stock_id, sm.description,
		sn.warranty_end, sn.warranty_start, sn.warranty_type
		FROM " . TB_PREF . "serial_numbers sn
		LEFT JOIN " . TB_PREF . "stock_master sm ON sn.stock_id = sm.stock_id
		WHERE sn.customer_id=" . (int)$customer_id
		. " AND sn.status IN ('delivered', 'in_repair', 'returned')";
	if ($stock_id != '')
		$sql .= " AND sn.stock_id=" . db_escape($stock_id);
	$sql .= " ORDER BY sn.serial_no";
	return db_query($sql, 'could not get customer delivered serials');
}

// =====================================================================
// WARRANTY CLAIM PARTS
// =====================================================================

/**
 * Add a part used in warranty repair.
 *
 * @param int    $claim_id  Claim id
 * @param string $stock_id  Part item code
 * @param float  $quantity  Quantity used
 * @param float  $unit_cost Unit cost
 * @param int    $serial_id Part serial id (nullable)
 * @return int Inserted part id
 */
function add_warranty_claim_part($claim_id, $stock_id, $quantity, $unit_cost, $serial_id = null)
{
	$sql = "INSERT INTO " . TB_PREF . "warranty_claim_parts (
		claim_id, stock_id, quantity, unit_cost, serial_id
	) VALUES ("
		. (int)$claim_id . ","
		. db_escape($stock_id) . ","
		. db_escape($quantity) . ","
		. db_escape($unit_cost) . ","
		. ($serial_id ? (int)$serial_id : 'NULL')
		. ")";
	db_query($sql, 'could not add warranty claim part');
	return db_insert_id();
}

/**
 * Delete a warranty claim part.
 *
 * @param int $id Part record id
 */
function delete_warranty_claim_part($id)
{
	$sql = "DELETE FROM " . TB_PREF . "warranty_claim_parts WHERE id=" . (int)$id;
	db_query($sql, 'could not delete warranty claim part');
}

/**
 * Get all parts for a warranty claim.
 *
 * @param int $claim_id Claim id
 * @return resource DB result
 */
function get_warranty_claim_parts($claim_id)
{
	$sql = "SELECT wcp.*, sm.description AS part_description,
		sn.serial_no AS part_serial_no
		FROM " . TB_PREF . "warranty_claim_parts wcp
		LEFT JOIN " . TB_PREF . "stock_master sm ON wcp.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON wcp.serial_id = sn.id
		WHERE wcp.claim_id=" . (int)$claim_id
		. " ORDER BY wcp.id";
	return db_query($sql, 'could not get warranty claim parts');
}

/**
 * Get total repair cost for a warranty claim (sum of parts cost).
 *
 * @param int $claim_id Claim id
 * @return float Total cost
 */
function get_warranty_claim_parts_cost($claim_id)
{
	$sql = "SELECT COALESCE(SUM(quantity * unit_cost), 0) FROM "
		. TB_PREF . "warranty_claim_parts WHERE claim_id=" . (int)$claim_id;
	$result = db_query($sql, 'could not get warranty claim parts cost');
	$row = db_fetch_row($result);
	return (float)$row[0];
}

/**
 * Update the repair_cost on a warranty claim from its parts.
 *
 * @param int $claim_id Claim id
 */
function recalculate_warranty_claim_cost($claim_id)
{
	$cost = get_warranty_claim_parts_cost($claim_id);
	$sql = "UPDATE " . TB_PREF . "warranty_claims SET repair_cost=" . db_escape($cost)
		. " WHERE id=" . (int)$claim_id;
	db_query($sql, 'could not update warranty claim cost');
}

// =====================================================================
// RECALL CAMPAIGN STATUS / TYPE HELPERS
// =====================================================================

/**
 * Get valid recall campaign statuses.
 *
 * @return array
 */
function get_recall_campaign_statuses()
{
	return array(
		'draft'       => _('Draft'),
		'active'      => _('Active'),
		'in_progress' => _('In Progress'),
		'completed'   => _('Completed'),
		'cancelled'   => _('Cancelled'),
	);
}

/**
 * Get display label for a recall campaign status.
 *
 * @param string $status Status code
 * @return string
 */
function get_recall_campaign_status_label($status)
{
	$statuses = get_recall_campaign_statuses();
	return isset($statuses[$status]) ? $statuses[$status] : $status;
}

/**
 * Get CSS color for recall campaign status.
 *
 * @param string $status Status code
 * @return string
 */
function get_recall_campaign_status_color($status)
{
	$colors = array(
		'draft'       => '#6c757d',
		'active'      => '#dc3545',
		'in_progress' => '#fd7e14',
		'completed'   => '#28a745',
		'cancelled'   => '#343a40',
	);
	return isset($colors[$status]) ? $colors[$status] : '#6c757d';
}

/**
 * Get valid recall types.
 *
 * @return array
 */
function get_recall_types()
{
	return array(
		'voluntary'         => _('Voluntary'),
		'mandatory'         => _('Mandatory'),
		'market_withdrawal' => _('Market Withdrawal'),
	);
}

/**
 * Get valid recall severity levels.
 *
 * @return array
 */
function get_recall_severity_levels()
{
	return array(
		'critical' => _('Critical'),
		'high'     => _('High'),
		'medium'   => _('Medium'),
		'low'      => _('Low'),
	);
}

/**
 * Get CSS color for recall severity.
 *
 * @param string $severity
 * @return string
 */
function get_recall_severity_color($severity)
{
	$colors = array(
		'critical' => '#dc3545',
		'high'     => '#fd7e14',
		'medium'   => '#ffc107',
		'low'      => '#28a745',
	);
	return isset($colors[$severity]) ? $colors[$severity] : '#6c757d';
}

/**
 * Get valid recall item statuses.
 *
 * @return array
 */
function get_recall_item_statuses()
{
	return array(
		'identified'  => _('Identified'),
		'notified'    => _('Notified'),
		'returned'    => _('Returned'),
		'repaired'    => _('Repaired'),
		'replaced'    => _('Replaced'),
		'refunded'    => _('Refunded'),
		'unreachable' => _('Unreachable'),
	);
}

/**
 * Get CSS color for recall item status.
 *
 * @param string $status
 * @return string
 */
function get_recall_item_status_color($status)
{
	$colors = array(
		'identified'  => '#6c757d',
		'notified'    => '#17a2b8',
		'returned'    => '#ffc107',
		'repaired'    => '#28a745',
		'replaced'    => '#28a745',
		'refunded'    => '#28a745',
		'unreachable' => '#dc3545',
	);
	return isset($colors[$status]) ? $colors[$status] : '#6c757d';
}

// =====================================================================
// RECALL CAMPAIGN CRUD
// =====================================================================

/**
 * Generate the next recall campaign reference number.
 *
 * Format: RC-{YYYY}-{5-digit-sequence}
 *
 * @return string
 */
function get_next_recall_campaign_reference()
{
	$year = date('Y');
	$sql = "SELECT reference FROM " . TB_PREF . "recall_campaigns
		WHERE reference LIKE " . db_escape('RC-' . $year . '-%')
		. " ORDER BY id DESC LIMIT 1";
	$result = db_query($sql, 'could not get last recall reference');
	$row = db_fetch_row($result);
	if ($row) {
		$parts = explode('-', $row[0]);
		$seq = (int)end($parts) + 1;
	} else {
		$seq = 1;
	}
	return 'RC-' . $year . '-' . str_pad($seq, 5, '0', STR_PAD_LEFT);
}

/**
 * Add a new recall campaign.
 *
 * @param string $reference         Campaign reference
 * @param string $title             Campaign title
 * @param string $description       Description of the recall
 * @param string $stock_id          Affected item code
 * @param string $recall_type       Type code (voluntary/mandatory/market_withdrawal)
 * @param string $severity          Severity code (critical/high/medium/low)
 * @param string $start_date        Start date
 * @param string $end_date          End date (nullable)
 * @param string $affected_batch_ids     CSV batch IDs
 * @param string $affected_serial_from   Serial range start
 * @param string $affected_serial_to     Serial range end
 * @param string $affected_date_from     Manufacturing date from
 * @param string $affected_date_to       Manufacturing date to
 * @param string $regulatory_reference   Regulatory reference
 * @param string $resolution             Resolution approach
 * @param string $notes                  Notes
 * @return int Inserted campaign id
 */
function add_recall_campaign($reference, $title, $description, $stock_id,
	$recall_type, $severity, $start_date, $end_date = null,
	$affected_batch_ids = null, $affected_serial_from = null, $affected_serial_to = null,
	$affected_date_from = null, $affected_date_to = null,
	$regulatory_reference = null, $resolution = null, $notes = null)
{
	$sql = "INSERT INTO " . TB_PREF . "recall_campaigns (
		reference, title, description, stock_id, recall_type, severity,
		status, start_date, end_date, affected_batch_ids,
		affected_serial_from, affected_serial_to,
		affected_date_from, affected_date_to,
		regulatory_reference, resolution, notes, created_by, created_at
	) VALUES ("
		. db_escape($reference) . ","
		. db_escape($title) . ","
		. db_escape($description) . ","
		. db_escape($stock_id) . ","
		. db_escape($recall_type) . ","
		. db_escape($severity) . ","
		. "'draft',"
		. db_escape(date2sql($start_date)) . ","
		. ($end_date ? db_escape(date2sql($end_date)) : 'NULL') . ","
		. db_escape($affected_batch_ids) . ","
		. db_escape($affected_serial_from) . ","
		. db_escape($affected_serial_to) . ","
		. ($affected_date_from ? db_escape(date2sql($affected_date_from)) : 'NULL') . ","
		. ($affected_date_to ? db_escape(date2sql($affected_date_to)) : 'NULL') . ","
		. db_escape($regulatory_reference) . ","
		. db_escape($resolution) . ","
		. db_escape($notes) . ","
		. (int)$_SESSION['wa_current_user']->user . ","
		. "NOW()"
		. ")";
	db_query($sql, 'could not add recall campaign');
	return db_insert_id();
}

/**
 * Update a recall campaign.
 *
 * @param int    $id                     Campaign id
 * @param string $title                  Title
 * @param string $description            Description
 * @param string $stock_id               Item code
 * @param string $recall_type            Type
 * @param string $severity               Severity
 * @param string $start_date             Start date
 * @param string $end_date               End date (nullable)
 * @param string $affected_batch_ids     CSV batch IDs
 * @param string $affected_serial_from   Serial range start
 * @param string $affected_serial_to     Serial range end
 * @param string $affected_date_from     Mfg date from
 * @param string $affected_date_to       Mfg date to
 * @param string $regulatory_reference   Regulatory ref
 * @param string $resolution             Resolution approach
 * @param string $notes                  Notes
 */
function update_recall_campaign($id, $title, $description, $stock_id,
	$recall_type, $severity, $start_date, $end_date = null,
	$affected_batch_ids = null, $affected_serial_from = null, $affected_serial_to = null,
	$affected_date_from = null, $affected_date_to = null,
	$regulatory_reference = null, $resolution = null, $notes = null)
{
	$sql = "UPDATE " . TB_PREF . "recall_campaigns SET "
		. "title=" . db_escape($title) . ","
		. "description=" . db_escape($description) . ","
		. "stock_id=" . db_escape($stock_id) . ","
		. "recall_type=" . db_escape($recall_type) . ","
		. "severity=" . db_escape($severity) . ","
		. "start_date=" . db_escape(date2sql($start_date)) . ","
		. "end_date=" . ($end_date ? db_escape(date2sql($end_date)) : 'NULL') . ","
		. "affected_batch_ids=" . db_escape($affected_batch_ids) . ","
		. "affected_serial_from=" . db_escape($affected_serial_from) . ","
		. "affected_serial_to=" . db_escape($affected_serial_to) . ","
		. "affected_date_from=" . ($affected_date_from ? db_escape(date2sql($affected_date_from)) : 'NULL') . ","
		. "affected_date_to=" . ($affected_date_to ? db_escape(date2sql($affected_date_to)) : 'NULL') . ","
		. "regulatory_reference=" . db_escape($regulatory_reference) . ","
		. "resolution=" . db_escape($resolution) . ","
		. "notes=" . db_escape($notes)
		. " WHERE id=" . (int)$id;
	db_query($sql, 'could not update recall campaign');
}

/**
 * Update recall campaign status.
 *
 * @param int    $id     Campaign id
 * @param string $status New status code
 */
function update_recall_campaign_status($id, $status)
{
	$sql = "UPDATE " . TB_PREF . "recall_campaigns SET status=" . db_escape($status)
		. " WHERE id=" . (int)$id;
	db_query($sql, 'could not update recall campaign status');
}

/**
 * Get a single recall campaign.
 *
 * @param int $id Campaign id
 * @return array|false
 */
function get_recall_campaign($id)
{
	$sql = "SELECT rc.*,
		sm.description AS item_description,
		CONCAT(u.real_name) AS created_by_name
		FROM " . TB_PREF . "recall_campaigns rc
		LEFT JOIN " . TB_PREF . "stock_master sm ON rc.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "users u ON rc.created_by = u.id
		WHERE rc.id=" . (int)$id;
	$result = db_query($sql, 'could not get recall campaign');
	return db_fetch($result);
}

/**
 * Get recall campaigns list with filters.
 *
 * @param string $stock_id Filter by item
 * @param string $status   Filter by status
 * @param int    $limit    Limit (0=all)
 * @param int    $offset   Offset
 * @return resource DB result
 */
function get_recall_campaigns($stock_id = '', $status = '', $limit = 0, $offset = 0)
{
	$sql = "SELECT rc.*, sm.description AS item_description
		FROM " . TB_PREF . "recall_campaigns rc
		LEFT JOIN " . TB_PREF . "stock_master sm ON rc.stock_id = sm.stock_id
		WHERE 1=1";
	if ($stock_id != '')
		$sql .= " AND rc.stock_id=" . db_escape($stock_id);
	if ($status != '')
		$sql .= " AND rc.status=" . db_escape($status);
	$sql .= " ORDER BY rc.id DESC";
	if ($limit > 0)
		$sql .= " LIMIT " . (int)$offset . "," . (int)$limit;
	return db_query($sql, 'could not get recall campaigns');
}

/**
 * Count recall campaigns matching filters.
 *
 * @param string $stock_id Filter by item
 * @param string $status   Filter by status
 * @return int
 */
function count_recall_campaigns($stock_id = '', $status = '')
{
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "recall_campaigns WHERE 1=1";
	if ($stock_id != '')
		$sql .= " AND stock_id=" . db_escape($stock_id);
	if ($status != '')
		$sql .= " AND status=" . db_escape($status);
	$result = db_query($sql, 'could not count recall campaigns');
	$row = db_fetch_row($result);
	return (int)$row[0];
}

/**
 * Check if a recall campaign can be deleted (only draft status).
 *
 * @param int $id Campaign id
 * @return bool|string True or error message
 */
function can_delete_recall_campaign($id)
{
	$campaign = get_recall_campaign($id);
	if (!$campaign)
		return _('Recall campaign not found.');
	if ($campaign['status'] !== 'draft')
		return _('Only draft recall campaigns can be deleted.');
	return true;
}

/**
 * Delete a recall campaign and all its items.
 *
 * @param int $id Campaign id
 */
function delete_recall_campaign($id)
{
	begin_transaction();
	$sql = "DELETE FROM " . TB_PREF . "recall_items WHERE recall_id=" . (int)$id;
	db_query($sql, 'could not delete recall items');
	$sql = "DELETE FROM " . TB_PREF . "recall_campaigns WHERE id=" . (int)$id;
	db_query($sql, 'could not delete recall campaign');
	commit_transaction();
}

/**
 * Get recall campaign status summary counts.
 *
 * @return array
 */
function get_recall_campaign_summary()
{
	$sql = "SELECT status, COUNT(*) AS cnt FROM " . TB_PREF . "recall_campaigns GROUP BY status";
	$result = db_query($sql, 'could not get recall campaign summary');
	$summary = array();
	while ($row = db_fetch($result))
		$summary[$row['status']] = (int)$row['cnt'];
	return $summary;
}

// =====================================================================
// RECALL ITEMS (affected serials/batches per campaign)
// =====================================================================

/**
 * Add a recall item (affected serial or batch).
 *
 * @param int    $recall_id   Campaign id
 * @param int    $serial_id   Serial id (nullable)
 * @param int    $batch_id    Batch id (nullable)
 * @param int    $customer_id Customer who has the item (nullable)
 * @param string $notes       Notes
 * @return int Inserted id
 */
function add_recall_item($recall_id, $serial_id = null, $batch_id = null,
	$customer_id = null, $notes = null)
{
	$sql = "INSERT INTO " . TB_PREF . "recall_items (
		recall_id, serial_id, batch_id, customer_id, status, notes
	) VALUES ("
		. (int)$recall_id . ","
		. ($serial_id ? (int)$serial_id : 'NULL') . ","
		. ($batch_id ? (int)$batch_id : 'NULL') . ","
		. ($customer_id ? (int)$customer_id : 'NULL') . ","
		. "'identified',"
		. db_escape($notes)
		. ")";
	db_query($sql, 'could not add recall item');
	return db_insert_id();
}

/**
 * Update a recall item's status.
 *
 * @param int    $id     Recall item id
 * @param string $status New status
 * @param string $notes  Optional notes
 */
function update_recall_item_status($id, $status, $notes = null)
{
	$sets = array("status=" . db_escape($status));

	if ($status === 'notified')
		$sets[] = "notification_date=CURDATE()";
	elseif ($status === 'returned')
		$sets[] = "return_date=CURDATE()";
	elseif (in_array($status, array('repaired', 'replaced', 'refunded')))
		$sets[] = "resolution_date=CURDATE()";

	if ($notes !== null)
		$sets[] = "notes=" . db_escape($notes);

	$sql = "UPDATE " . TB_PREF . "recall_items SET " . implode(',', $sets)
		. " WHERE id=" . (int)$id;
	db_query($sql, 'could not update recall item status');
}

/**
 * Delete a recall item.
 *
 * @param int $id Recall item id
 */
function delete_recall_item($id)
{
	$sql = "DELETE FROM " . TB_PREF . "recall_items WHERE id=" . (int)$id;
	db_query($sql, 'could not delete recall item');
}

/**
 * Get all recall items for a campaign with joins.
 *
 * @param int    $recall_id Campaign id
 * @param string $status    Filter by status (optional)
 * @return resource DB result
 */
function get_recall_items($recall_id, $status = '')
{
	$sql = "SELECT ri.*,
		sn.serial_no, sn.stock_id AS serial_stock_id, sn.status AS serial_status,
		sb.batch_no, sb.stock_id AS batch_stock_id,
		dm.name AS customer_name
		FROM " . TB_PREF . "recall_items ri
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON ri.serial_id = sn.id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON ri.batch_id = sb.id
		LEFT JOIN " . TB_PREF . "debtors_master dm ON ri.customer_id = dm.debtor_no
		WHERE ri.recall_id=" . (int)$recall_id;
	if ($status != '')
		$sql .= " AND ri.status=" . db_escape($status);
	$sql .= " ORDER BY ri.id";
	return db_query($sql, 'could not get recall items');
}

/**
 * Count recall items by status for a campaign.
 *
 * @param int $recall_id Campaign id
 * @return array Associative status => count
 */
function get_recall_item_status_summary($recall_id)
{
	$sql = "SELECT status, COUNT(*) AS cnt FROM " . TB_PREF . "recall_items
		WHERE recall_id=" . (int)$recall_id . " GROUP BY status";
	$result = db_query($sql, 'could not get recall item status summary');
	$summary = array();
	while ($row = db_fetch($result))
		$summary[$row['status']] = (int)$row['cnt'];
	return $summary;
}

/**
 * Identify affected serial numbers for a recall campaign.
 *
 * Searches by: serial range, batch association, manufacturing date range.
 * Returns list of serial ids that match the criteria.
 *
 * @param int $campaign_id Recall campaign id
 * @return array Array of serial rows with id, serial_no, status, customer_id, etc.
 */
function identify_recall_affected_serials($campaign_id)
{
	$campaign = get_recall_campaign($campaign_id);
	if (!$campaign)
		return array();

	$conditions = array("sn.stock_id=" . db_escape($campaign['stock_id']));

	// Serial range filter
	if ($campaign['affected_serial_from'] && $campaign['affected_serial_to']) {
		$conditions[] = "sn.serial_no >= " . db_escape($campaign['affected_serial_from']);
		$conditions[] = "sn.serial_no <= " . db_escape($campaign['affected_serial_to']);
	}

	// Batch filter
	if ($campaign['affected_batch_ids']) {
		$batch_ids = array_map('intval', explode(',', $campaign['affected_batch_ids']));
		if (!empty($batch_ids))
			$conditions[] = "sn.batch_id IN (" . implode(',', $batch_ids) . ")";
	}

	// Manufacturing date range filter
	if ($campaign['affected_date_from'])
		$conditions[] = "sn.manufacturing_date >= " . db_escape($campaign['affected_date_from']);
	if ($campaign['affected_date_to'])
		$conditions[] = "sn.manufacturing_date <= " . db_escape($campaign['affected_date_to']);

	$sql = "SELECT sn.id, sn.serial_no, sn.stock_id, sn.status, sn.loc_code,
		sn.customer_id, sn.batch_id, sn.manufacturing_date,
		dm.name AS customer_name
		FROM " . TB_PREF . "serial_numbers sn
		LEFT JOIN " . TB_PREF . "debtors_master dm ON sn.customer_id = dm.debtor_no
		WHERE " . implode(' AND ', $conditions)
		. " AND sn.status NOT IN ('scrapped')
		ORDER BY sn.serial_no";

	$result = db_query($sql, 'could not identify recall affected serials');
	$serials = array();
	while ($row = db_fetch($result))
		$serials[] = $row;
	return $serials;
}

/**
 * Identify affected batches for a recall campaign.
 *
 * @param int $campaign_id
 * @return array Array of batch rows
 */
function identify_recall_affected_batches($campaign_id)
{
	$campaign = get_recall_campaign($campaign_id);
	if (!$campaign)
		return array();

	$conditions = array("sb.stock_id=" . db_escape($campaign['stock_id']));

	if ($campaign['affected_batch_ids']) {
		$batch_ids = array_map('intval', explode(',', $campaign['affected_batch_ids']));
		if (!empty($batch_ids))
			$conditions[] = "sb.id IN (" . implode(',', $batch_ids) . ")";
	}

	if ($campaign['affected_date_from'])
		$conditions[] = "sb.manufacturing_date >= " . db_escape($campaign['affected_date_from']);
	if ($campaign['affected_date_to'])
		$conditions[] = "sb.manufacturing_date <= " . db_escape($campaign['affected_date_to']);

	$sql = "SELECT sb.id, sb.batch_no, sb.stock_id, sb.status,
		sb.initial_qty, sb.manufacturing_date, sb.expiry_date
		FROM " . TB_PREF . "stock_batches sb
		WHERE " . implode(' AND ', $conditions)
		. " AND sb.status NOT IN ('scrapped', 'consumed')
		ORDER BY sb.batch_no";

	$result = db_query($sql, 'could not identify recall affected batches');
	$batches = array();
	while ($row = db_fetch($result))
		$batches[] = $row;
	return $batches;
}

/**
 * Execute recall: identify all affected items, add them to recall_items, quarantine in-stock serials.
 *
 * @param int $campaign_id
 * @return array ['serials_added' => int, 'serials_quarantined' => int, 'batches_added' => int]
 */
function execute_recall_campaign($campaign_id)
{
	$result = array('serials_added' => 0, 'serials_quarantined' => 0, 'batches_added' => 0);

	begin_transaction();

	// Find affected serials
	$serials = identify_recall_affected_serials($campaign_id);
	foreach ($serials as $serial) {
		// Check if already in recall items
		$check = "SELECT COUNT(*) FROM " . TB_PREF . "recall_items
			WHERE recall_id=" . (int)$campaign_id . " AND serial_id=" . (int)$serial['id'];
		$chk = db_query($check, 'check existing recall item');
		$row = db_fetch_row($chk);
		if ($row[0] > 0) continue;

		add_recall_item($campaign_id, $serial['id'], $serial['batch_id'],
			$serial['customer_id'], _('Auto-identified by recall campaign'));
		$result['serials_added']++;

		// Quarantine in-stock serials
		if ($serial['status'] === 'available') {
			include_once(dirname(__FILE__) . '/serial_numbers_db.inc');
			update_serial_status($serial['id'], 'recalled', 0, 0,
				$serial['loc_code'], $serial['loc_code'], null,
				'RC-' . $campaign_id, _('Auto-quarantined by recall campaign'));
			$result['serials_quarantined']++;
		}
	}

	// Find affected batches
	$batches = identify_recall_affected_batches($campaign_id);
	foreach ($batches as $batch) {
		$check = "SELECT COUNT(*) FROM " . TB_PREF . "recall_items
			WHERE recall_id=" . (int)$campaign_id . " AND batch_id=" . (int)$batch['id']
			. " AND serial_id IS NULL";
		$chk = db_query($check, 'check existing recall batch item');
		$row = db_fetch_row($chk);
		if ($row[0] > 0) continue;

		add_recall_item($campaign_id, null, $batch['id'], null,
			_('Auto-identified batch by recall campaign'));
		$result['batches_added']++;
	}

	// Update campaign totals
	$total_affected = $result['serials_added'] + $result['batches_added'];
	$sql = "UPDATE " . TB_PREF . "recall_campaigns SET total_affected_units="
		. "(SELECT COUNT(*) FROM " . TB_PREF . "recall_items WHERE recall_id=" . (int)$campaign_id . ")"
		. " WHERE id=" . (int)$campaign_id;
	db_query($sql, 'could not update recall totals');

	// Transition to active/in_progress
	update_recall_campaign_status($campaign_id, 'in_progress');

	commit_transaction();

	return $result;
}

/**
 * Update recall campaign recovered count from recall_items.
 *
 * @param int $campaign_id
 */
function update_recall_campaign_totals($campaign_id)
{
	$resolved_statuses = "('returned','repaired','replaced','refunded')";

	$sql = "UPDATE " . TB_PREF . "recall_campaigns SET
		total_affected_units = (SELECT COUNT(*) FROM " . TB_PREF . "recall_items WHERE recall_id=" . (int)$campaign_id . "),
		total_recovered_units = (SELECT COUNT(*) FROM " . TB_PREF . "recall_items
			WHERE recall_id=" . (int)$campaign_id . " AND status IN " . $resolved_statuses . ")
		WHERE id=" . (int)$campaign_id;
	db_query($sql, 'could not update recall campaign totals');
}

// =====================================================================
// UI HELPER FUNCTIONS
// =====================================================================

/**
 * Render a warranty claim status badge.
 *
 * @param string $status Status code
 * @return string HTML
 */
function warranty_claim_status_badge($status)
{
	$label = get_warranty_claim_status_label($status);
	$color = get_warranty_claim_status_color($status);
	return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;font-size:11px;'
		. 'color:#fff;background-color:' . $color . ';">' . $label . '</span>';
}

/**
 * Render a recall campaign status badge.
 *
 * @param string $status Status code
 * @return string HTML
 */
function recall_campaign_status_badge($status)
{
	$label = get_recall_campaign_status_label($status);
	$color = get_recall_campaign_status_color($status);
	return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;font-size:11px;'
		. 'color:#fff;background-color:' . $color . ';">' . $label . '</span>';
}

/**
 * Render a recall severity badge.
 *
 * @param string $severity Severity code
 * @return string HTML
 */
function recall_severity_badge($severity)
{
	$levels = get_recall_severity_levels();
	$label = isset($levels[$severity]) ? $levels[$severity] : $severity;
	$color = get_recall_severity_color($severity);
	return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;font-size:11px;'
		. 'color:#fff;background-color:' . $color . ';">' . $label . '</span>';
}

/**
 * Render a recall item status badge.
 *
 * @param string $status Status code
 * @return string HTML
 */
function recall_item_status_badge($status)
{
	$statuses = get_recall_item_statuses();
	$label = isset($statuses[$status]) ? $statuses[$status] : $status;
	$color = get_recall_item_status_color($status);
	return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;font-size:11px;'
		. 'color:#fff;background-color:' . $color . ';">' . $label . '</span>';
}
