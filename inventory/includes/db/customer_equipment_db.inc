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
 * Customer Equipment Register & CRM-Inventory Integration DB Functions.
 *
 * Provides queries for displaying all serialized equipment delivered to a customer,
 * warranty status overviews, and linkage between warranty claims and customer records.
 *
 * Tables read: 0_serial_numbers, 0_stock_master, 0_warranty_claims, 0_debtors_master,
 *              0_serial_movements, 0_stock_batches
 *
 * @package NotrinosERP
 * @subpackage Inventory/CRMIntegration
 */

/**
 * Get all serialized equipment delivered to a customer.
 *
 * Returns serial numbers with item details, warranty status, and claim counts.
 *
 * @param int    $customer_id  Customer debtor_no
 * @param string $stock_id     Optional filter by item code
 * @param string $warranty_filter 'all'|'active'|'expired'|'expiring_soon'
 * @param string $status_filter   Serial status filter ('' for all delivered statuses)
 * @param int    $limit        Result limit (0 for all)
 * @param int    $offset       Result offset
 * @return resource DB result
 */
function get_customer_equipment($customer_id, $stock_id = '', $warranty_filter = 'all',
	$status_filter = '', $limit = 0, $offset = 0)
{
	$today = date('Y-m-d');
	$soon = date('Y-m-d', strtotime('+30 days'));

	$sql = "SELECT sn.id, sn.serial_no, sn.stock_id, sm.description AS item_description,
			sm.units, sn.status, sn.loc_code,
			sn.delivery_date, sn.delivery_id,
			sn.warranty_start, sn.warranty_end, sn.warranty_type,
			sn.purchase_cost, sn.actual_cost,
			sn.manufacturing_date, sn.expiry_date,
			sn.batch_id, sb.batch_no,
			CASE
				WHEN sn.warranty_end IS NULL THEN 'none'
				WHEN sn.warranty_end < '$today' THEN 'expired'
				WHEN sn.warranty_end <= '$soon' THEN 'expiring_soon'
				ELSE 'active'
			END AS warranty_status,
			DATEDIFF(sn.warranty_end, '$today') AS warranty_days_remaining,
			(SELECT COUNT(*) FROM " . TB_PREF . "warranty_claims wc
			 WHERE wc.serial_id = sn.id AND wc.customer_id = " . (int)$customer_id . "
			) AS claim_count,
			(SELECT wc2.status FROM " . TB_PREF . "warranty_claims wc2
			 WHERE wc2.serial_id = sn.id AND wc2.customer_id = " . (int)$customer_id . "
			 ORDER BY wc2.id DESC LIMIT 1
			) AS latest_claim_status
		FROM " . TB_PREF . "serial_numbers sn
		LEFT JOIN " . TB_PREF . "stock_master sm ON sn.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON sn.batch_id = sb.id
		WHERE sn.customer_id = " . (int)$customer_id
		. " AND sn.status IN ('delivered', 'in_repair', 'returned', 'recalled')";

	if ($stock_id !== '' && $stock_id !== null)
		$sql .= " AND sn.stock_id = " . db_escape($stock_id);

	if ($status_filter !== '' && $status_filter !== null)
		$sql .= " AND sn.status = " . db_escape($status_filter);

	switch ($warranty_filter) {
		case 'active':
			$sql .= " AND sn.warranty_end IS NOT NULL AND sn.warranty_end >= '$today'";
			break;
		case 'expired':
			$sql .= " AND sn.warranty_end IS NOT NULL AND sn.warranty_end < '$today'";
			break;
		case 'expiring_soon':
			$sql .= " AND sn.warranty_end IS NOT NULL AND sn.warranty_end >= '$today' AND sn.warranty_end <= '$soon'";
			break;
	}

	$sql .= " ORDER BY sn.delivery_date DESC, sn.serial_no";

	if ($limit > 0)
		$sql .= " LIMIT " . (int)$offset . ", " . (int)$limit;

	return db_query($sql, 'could not get customer equipment');
}

/**
 * Count customer equipment records.
 *
 * @param int    $customer_id  Customer debtor_no
 * @param string $stock_id     Optional item filter
 * @param string $warranty_filter 'all'|'active'|'expired'|'expiring_soon'
 * @return int
 */
function count_customer_equipment($customer_id, $stock_id = '', $warranty_filter = 'all')
{
	$today = date('Y-m-d');
	$soon = date('Y-m-d', strtotime('+30 days'));

	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "serial_numbers sn
		WHERE sn.customer_id = " . (int)$customer_id
		. " AND sn.status IN ('delivered', 'in_repair', 'returned', 'recalled')";

	if ($stock_id !== '' && $stock_id !== null)
		$sql .= " AND sn.stock_id = " . db_escape($stock_id);

	switch ($warranty_filter) {
		case 'active':
			$sql .= " AND sn.warranty_end IS NOT NULL AND sn.warranty_end >= '$today'";
			break;
		case 'expired':
			$sql .= " AND sn.warranty_end IS NOT NULL AND sn.warranty_end < '$today'";
			break;
		case 'expiring_soon':
			$sql .= " AND sn.warranty_end IS NOT NULL AND sn.warranty_end >= '$today' AND sn.warranty_end <= '$soon'";
			break;
	}

	$result = db_query($sql, 'could not count customer equipment');
	$row = db_fetch_row($result);
	return (int)$row[0];
}

/**
 * Get customer equipment summary (counts by warranty status).
 *
 * @param int $customer_id Customer debtor_no
 * @return array ['total', 'active_warranty', 'expired_warranty', 'expiring_soon', 'no_warranty', 'open_claims']
 */
function get_customer_equipment_summary($customer_id)
{
	$today = date('Y-m-d');
	$soon = date('Y-m-d', strtotime('+30 days'));

	$sql = "SELECT
		COUNT(*) AS total,
		SUM(CASE WHEN sn.warranty_end IS NOT NULL AND sn.warranty_end >= '$today' AND sn.warranty_end > '$soon' THEN 1 ELSE 0 END) AS active_warranty,
		SUM(CASE WHEN sn.warranty_end IS NOT NULL AND sn.warranty_end < '$today' THEN 1 ELSE 0 END) AS expired_warranty,
		SUM(CASE WHEN sn.warranty_end IS NOT NULL AND sn.warranty_end >= '$today' AND sn.warranty_end <= '$soon' THEN 1 ELSE 0 END) AS expiring_soon,
		SUM(CASE WHEN sn.warranty_end IS NULL THEN 1 ELSE 0 END) AS no_warranty
		FROM " . TB_PREF . "serial_numbers sn
		WHERE sn.customer_id = " . (int)$customer_id
		. " AND sn.status IN ('delivered', 'in_repair', 'returned', 'recalled')";

	$result = db_query($sql, 'could not get customer equipment summary');
	$row = db_fetch($result);

	// Count open warranty claims
	$sql2 = "SELECT COUNT(*) FROM " . TB_PREF . "warranty_claims
		WHERE customer_id = " . (int)$customer_id
		. " AND status NOT IN ('closed', 'rejected')";
	$result2 = db_query($sql2, 'could not count open warranty claims');
	$row2 = db_fetch_row($result2);

	return array(
		'total'            => (int)$row['total'],
		'active_warranty'  => (int)$row['active_warranty'],
		'expired_warranty' => (int)$row['expired_warranty'],
		'expiring_soon'    => (int)$row['expiring_soon'],
		'no_warranty'      => (int)$row['no_warranty'],
		'open_claims'      => (int)$row2[0],
	);
}

/**
 * Get warranty claims for a specific customer.
 *
 * @param int    $customer_id Customer debtor_no
 * @param string $status      Claim status filter ('' for all)
 * @param int    $limit       Result limit
 * @param int    $offset      Result offset
 * @return resource DB result
 */
function get_customer_warranty_claims($customer_id, $status = '', $limit = 50, $offset = 0)
{
	$sql = "SELECT wc.*, sm.description AS item_description,
			sn.serial_no, sb.batch_no,
			u.real_name AS assigned_to_name
		FROM " . TB_PREF . "warranty_claims wc
		LEFT JOIN " . TB_PREF . "stock_master sm ON wc.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON wc.serial_id = sn.id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON wc.batch_id = sb.id
		LEFT JOIN " . TB_PREF . "users u ON wc.assigned_to = u.id
		WHERE wc.customer_id = " . (int)$customer_id;

	if ($status !== '' && $status !== null)
		$sql .= " AND wc.status = " . db_escape($status);

	$sql .= " ORDER BY wc.claim_date DESC, wc.id DESC";

	if ($limit > 0)
		$sql .= " LIMIT " . (int)$offset . ", " . (int)$limit;

	return db_query($sql, 'could not get customer warranty claims');
}

/**
 * Get serial-specific cost for COGS.
 *
 * Returns actual_cost from serial_numbers if available, otherwise standard_cost from stock_master.
 *
 * @param int $serial_id Serial number id
 * @return float Cost value
 */
function get_serial_specific_cost($serial_id)
{
	$sql = "SELECT sn.actual_cost, sn.purchase_cost, sm.material_cost
		FROM " . TB_PREF . "serial_numbers sn
		LEFT JOIN " . TB_PREF . "stock_master sm ON sn.stock_id = sm.stock_id
		WHERE sn.id = " . (int)$serial_id;
	$result = db_query($sql, 'could not get serial cost');
	$row = db_fetch($result);

	if (!$row)
		return 0;

	// Priority: actual_cost > purchase_cost > material_cost
	if ($row['actual_cost'] !== null && $row['actual_cost'] > 0)
		return (float)$row['actual_cost'];
	if ($row['purchase_cost'] !== null && $row['purchase_cost'] > 0)
		return (float)$row['purchase_cost'];
	return (float)$row['material_cost'];
}

/**
 * Get batch-specific cost for COGS.
 *
 * Returns batch_cost from stock_batches if available, otherwise standard_cost from stock_master.
 *
 * @param int $batch_id Batch id
 * @return float Cost value
 */
function get_batch_specific_cost($batch_id)
{
	$sql = "SELECT sb.batch_cost, sm.material_cost
		FROM " . TB_PREF . "stock_batches sb
		LEFT JOIN " . TB_PREF . "stock_master sm ON sb.stock_id = sm.stock_id
		WHERE sb.id = " . (int)$batch_id;
	$result = db_query($sql, 'could not get batch cost');
	$row = db_fetch($result);

	if (!$row)
		return 0;

	if ($row['batch_cost'] !== null && $row['batch_cost'] > 0)
		return (float)$row['batch_cost'];
	return (float)$row['material_cost'];
}

/**
 * Get SQL query string for customer equipment pager.
 *
 * @param int    $customer_id    Customer debtor_no
 * @param string $stock_id       Item filter
 * @param string $warranty_filter Warranty status filter
 * @param string $status_filter  Serial status filter
 * @return string SQL query
 */
function get_sql_for_customer_equipment($customer_id, $stock_id = '', $warranty_filter = 'all',
	$status_filter = '')
{
	$today = date('Y-m-d');
	$soon = date('Y-m-d', strtotime('+30 days'));

	$sql = "SELECT sn.id, sn.serial_no, sn.stock_id, sm.description AS item_description,
			sm.units, sn.status, sn.loc_code,
			sn.delivery_date, sn.delivery_id,
			sn.warranty_start, sn.warranty_end, sn.warranty_type,
			sn.purchase_cost, sn.actual_cost,
			sn.batch_id,
			CASE
				WHEN sn.warranty_end IS NULL THEN 'none'
				WHEN sn.warranty_end < '$today' THEN 'expired'
				WHEN sn.warranty_end <= '$soon' THEN 'expiring_soon'
				ELSE 'active'
			END AS warranty_status,
			DATEDIFF(sn.warranty_end, '$today') AS warranty_days_remaining,
			(SELECT COUNT(*) FROM " . TB_PREF . "warranty_claims wc
			 WHERE wc.serial_id = sn.id AND wc.customer_id = " . (int)$customer_id . "
			) AS claim_count
		FROM " . TB_PREF . "serial_numbers sn
		LEFT JOIN " . TB_PREF . "stock_master sm ON sn.stock_id = sm.stock_id
		WHERE sn.customer_id = " . (int)$customer_id
		. " AND sn.status IN ('delivered', 'in_repair', 'returned', 'recalled')";

	if ($stock_id !== '' && $stock_id !== null)
		$sql .= " AND sn.stock_id = " . db_escape($stock_id);

	if ($status_filter !== '' && $status_filter !== null)
		$sql .= " AND sn.status = " . db_escape($status_filter);

	switch ($warranty_filter) {
		case 'active':
			$sql .= " AND sn.warranty_end IS NOT NULL AND sn.warranty_end >= '$today'";
			break;
		case 'expired':
			$sql .= " AND sn.warranty_end IS NOT NULL AND sn.warranty_end < '$today'";
			break;
		case 'expiring_soon':
			$sql .= " AND sn.warranty_end IS NOT NULL AND sn.warranty_end >= '$today' AND sn.warranty_end <= '$soon'";
			break;
	}

	return $sql;
}
