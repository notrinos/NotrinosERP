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
 * Enhanced Recall Management Database Layer.
 *
 * Extends the base recall functions in warranty_recall_db.inc with:
 *   - Customer notification list generation
 *   - Batch status update operations on recall items
 *   - Recall analytics and progress calculations
 *   - Recall campaign search and filtering
 *
 * Tables used: 0_recall_campaigns, 0_recall_items, 0_serial_numbers,
 *   0_stock_batches, 0_debtors_master, 0_stock_master
 *
 * @package NotrinosERP
 * @subpackage Inventory/Recall
 */

include_once(dirname(__FILE__) . '/warranty_recall_db.inc');

// =====================================================================
// CUSTOMER NOTIFICATION LIST
// =====================================================================

/**
 * Generate the customer notification list for a recall campaign.
 *
 * Returns all unique customers who received affected items (via serial numbers
 * with customer_id set, or via delivery documents linked to affected batches).
 *
 * @param int $campaign_id Recall campaign id
 * @return array Array of customer rows: customer_id, name, address, serials_count, batches_count
 */
function get_recall_customer_notification_list($campaign_id)
{
	// Get customers from recall_items that have a customer_id
	$sql = "SELECT
		ri.customer_id,
		dm.name AS customer_name,
		dm.address,
		dm.debtor_ref,
		COUNT(DISTINCT CASE WHEN ri.serial_id IS NOT NULL THEN ri.serial_id END) AS serial_count,
		COUNT(DISTINCT CASE WHEN ri.batch_id IS NOT NULL AND ri.serial_id IS NULL THEN ri.batch_id END) AS batch_count,
		COUNT(*) AS total_items,
		GROUP_CONCAT(DISTINCT sn.serial_no ORDER BY sn.serial_no SEPARATOR ', ') AS serial_numbers,
		GROUP_CONCAT(DISTINCT sb.batch_no ORDER BY sb.batch_no SEPARATOR ', ') AS batch_numbers,
		MIN(CASE WHEN ri.status = 'notified' THEN ri.notification_date END) AS first_notified,
		MAX(CASE WHEN ri.status IN ('returned','repaired','replaced','refunded') THEN 1 ELSE 0 END) AS has_resolved
		FROM " . TB_PREF . "recall_items ri
		INNER JOIN " . TB_PREF . "debtors_master dm ON ri.customer_id = dm.debtor_no
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON ri.serial_id = sn.id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON ri.batch_id = sb.id
		WHERE ri.recall_id=" . (int)$campaign_id
		. " AND ri.customer_id IS NOT NULL
		GROUP BY ri.customer_id, dm.name, dm.address, dm.debtor_ref
		ORDER BY dm.name";

	$result = db_query($sql, 'could not get recall customer notification list');
	$customers = array();
	while ($row = db_fetch($result))
		$customers[] = $row;
	return $customers;
}

/**
 * Count customers requiring notification (those with status='identified' items).
 *
 * @param int $campaign_id Recall campaign id
 * @return int Number of customers pending notification
 */
function count_recall_pending_notifications($campaign_id)
{
	$sql = "SELECT COUNT(DISTINCT customer_id) FROM " . TB_PREF . "recall_items
		WHERE recall_id=" . (int)$campaign_id
		. " AND customer_id IS NOT NULL
		AND status='identified'";
	$result = db_query($sql, 'could not count pending notifications');
	$row = db_fetch_row($result);
	return (int)$row[0];
}

/**
 * Count customers already notified.
 *
 * @param int $campaign_id Recall campaign id
 * @return int
 */
function count_recall_notified_customers($campaign_id)
{
	$sql = "SELECT COUNT(DISTINCT customer_id) FROM " . TB_PREF . "recall_items
		WHERE recall_id=" . (int)$campaign_id
		. " AND customer_id IS NOT NULL
		AND status IN ('notified','returned','repaired','replaced','refunded')";
	$result = db_query($sql, 'could not count notified customers');
	$row = db_fetch_row($result);
	return (int)$row[0];
}

// =====================================================================
// BATCH STATUS OPERATIONS
// =====================================================================

/**
 * Batch-update status of all recall items for a given customer.
 *
 * @param int    $campaign_id Recall campaign id
 * @param int    $customer_id Customer debtor_no
 * @param string $new_status  New status code
 * @param string $notes       Optional notes
 * @return int Number of items updated
 */
function batch_update_recall_items_by_customer($campaign_id, $customer_id, $new_status, $notes = null)
{
	$sets = array("status=" . db_escape($new_status));

	if ($new_status === 'notified')
		$sets[] = "notification_date=CURDATE()";
	elseif ($new_status === 'returned')
		$sets[] = "return_date=CURDATE()";
	elseif (in_array($new_status, array('repaired', 'replaced', 'refunded')))
		$sets[] = "resolution_date=CURDATE()";

	if ($notes !== null)
		$sets[] = "notes=" . db_escape($notes);

	$sql = "UPDATE " . TB_PREF . "recall_items SET " . implode(',', $sets)
		. " WHERE recall_id=" . (int)$campaign_id
		. " AND customer_id=" . (int)$customer_id;
	db_query($sql, 'could not batch update recall items by customer');

	$affected = db_num_affected_rows();
	update_recall_campaign_totals($campaign_id);
	return $affected;
}

/**
 * Batch-update status of all recall items matching a specific current status.
 *
 * @param int    $campaign_id  Recall campaign id
 * @param string $from_status  Current status to match
 * @param string $to_status    New status code
 * @param string $notes        Optional notes
 * @return int Number of items updated
 */
function batch_update_recall_items_by_status($campaign_id, $from_status, $to_status, $notes = null)
{
	$sets = array("status=" . db_escape($to_status));

	if ($to_status === 'notified')
		$sets[] = "notification_date=CURDATE()";
	elseif ($to_status === 'returned')
		$sets[] = "return_date=CURDATE()";
	elseif (in_array($to_status, array('repaired', 'replaced', 'refunded')))
		$sets[] = "resolution_date=CURDATE()";

	if ($notes !== null)
		$sets[] = "notes=" . db_escape($notes);

	$sql = "UPDATE " . TB_PREF . "recall_items SET " . implode(',', $sets)
		. " WHERE recall_id=" . (int)$campaign_id
		. " AND status=" . db_escape($from_status);
	db_query($sql, 'could not batch update recall items by status');

	$affected = db_num_affected_rows();
	update_recall_campaign_totals($campaign_id);
	return $affected;
}

/**
 * Mark all identified items for a customer as 'notified'.
 *
 * @param int    $campaign_id Campaign id
 * @param int    $customer_id Customer debtor_no
 * @param string $notes       Notes about notification
 * @return int Number of items marked as notified
 */
function mark_customer_notified($campaign_id, $customer_id, $notes = null)
{
	$sets = array(
		"status='notified'",
		"notification_date=CURDATE()"
	);
	if ($notes !== null)
		$sets[] = "notes=" . db_escape($notes);

	$sql = "UPDATE " . TB_PREF . "recall_items SET " . implode(',', $sets)
		. " WHERE recall_id=" . (int)$campaign_id
		. " AND customer_id=" . (int)$customer_id
		. " AND status='identified'";
	db_query($sql, 'could not mark customer notified');

	$affected = db_num_affected_rows();
	update_recall_campaign_totals($campaign_id);
	return $affected;
}

// =====================================================================
// RECALL ANALYTICS & PROGRESS
// =====================================================================

/**
 * Get comprehensive recall campaign progress data.
 *
 * @param int $campaign_id Campaign id
 * @return array Progress metrics: total, identified, notified, returned, resolved, unreachable,
 *               pct_recovered, pct_notified, customers_total, customers_notified, customers_pending
 */
function get_recall_campaign_progress($campaign_id)
{
	$summary = get_recall_item_status_summary($campaign_id);

	$total = 0;
	foreach ($summary as $cnt)
		$total += $cnt;

	$identified = isset($summary['identified']) ? $summary['identified'] : 0;
	$notified = isset($summary['notified']) ? $summary['notified'] : 0;
	$returned = isset($summary['returned']) ? $summary['returned'] : 0;
	$repaired = isset($summary['repaired']) ? $summary['repaired'] : 0;
	$replaced = isset($summary['replaced']) ? $summary['replaced'] : 0;
	$refunded = isset($summary['refunded']) ? $summary['refunded'] : 0;
	$unreachable = isset($summary['unreachable']) ? $summary['unreachable'] : 0;

	$resolved = $returned + $repaired + $replaced + $refunded;
	$pct_recovered = $total > 0 ? round(($resolved / $total) * 100, 1) : 0;
	$pct_notified = $total > 0 ? round((($notified + $resolved + $unreachable) / $total) * 100, 1) : 0;

	// Customer stats
	$customers_total = 0;
	$customers_notified = 0;
	$customers_pending = 0;

	$sql = "SELECT
		COUNT(DISTINCT customer_id) AS total_customers,
		COUNT(DISTINCT CASE WHEN status IN ('notified','returned','repaired','replaced','refunded') THEN customer_id END) AS notified_customers,
		COUNT(DISTINCT CASE WHEN status = 'identified' THEN customer_id END) AS pending_customers
		FROM " . TB_PREF . "recall_items
		WHERE recall_id=" . (int)$campaign_id
		. " AND customer_id IS NOT NULL";
	$result = db_query($sql, 'could not get customer stats');
	$row = db_fetch($result);
	if ($row) {
		$customers_total = (int)$row['total_customers'];
		$customers_notified = (int)$row['notified_customers'];
		$customers_pending = (int)$row['pending_customers'];
	}

	return array(
		'total'               => $total,
		'identified'          => $identified,
		'notified'            => $notified,
		'returned'            => $returned,
		'repaired'            => $repaired,
		'replaced'            => $replaced,
		'refunded'            => $refunded,
		'unreachable'         => $unreachable,
		'resolved'            => $resolved,
		'pct_recovered'       => $pct_recovered,
		'pct_notified'        => $pct_notified,
		'customers_total'     => $customers_total,
		'customers_notified'  => $customers_notified,
		'customers_pending'   => $customers_pending,
	);
}

/**
 * Get all recall campaigns with progress data for reporting.
 *
 * @param string $stock_id    Filter by item code (optional)
 * @param string $status      Filter by campaign status (optional)
 * @param string $severity    Filter by severity (optional)
 * @param string $date_from   Filter by start_date >= (optional, SQL format)
 * @param string $date_to     Filter by start_date <= (optional, SQL format)
 * @return array Array of campaign rows with progress subarray
 */
function get_recall_campaigns_with_progress($stock_id = '', $status = '', $severity = '',
	$date_from = '', $date_to = '')
{
	$sql = "SELECT rc.*,
		sm.description AS item_description,
		CONCAT(u.real_name) AS created_by_name,
		(SELECT COUNT(*) FROM " . TB_PREF . "recall_items ri WHERE ri.recall_id = rc.id) AS total_items,
		(SELECT COUNT(*) FROM " . TB_PREF . "recall_items ri WHERE ri.recall_id = rc.id AND ri.status = 'identified') AS cnt_identified,
		(SELECT COUNT(*) FROM " . TB_PREF . "recall_items ri WHERE ri.recall_id = rc.id AND ri.status = 'notified') AS cnt_notified,
		(SELECT COUNT(*) FROM " . TB_PREF . "recall_items ri WHERE ri.recall_id = rc.id AND ri.status IN ('returned','repaired','replaced','refunded')) AS cnt_resolved,
		(SELECT COUNT(*) FROM " . TB_PREF . "recall_items ri WHERE ri.recall_id = rc.id AND ri.status = 'unreachable') AS cnt_unreachable,
		(SELECT COUNT(DISTINCT ri.customer_id) FROM " . TB_PREF . "recall_items ri WHERE ri.recall_id = rc.id AND ri.customer_id IS NOT NULL) AS total_customers,
		(SELECT COUNT(DISTINCT ri.customer_id) FROM " . TB_PREF . "recall_items ri WHERE ri.recall_id = rc.id AND ri.customer_id IS NOT NULL AND ri.status IN ('notified','returned','repaired','replaced','refunded')) AS notified_customers
		FROM " . TB_PREF . "recall_campaigns rc
		LEFT JOIN " . TB_PREF . "stock_master sm ON rc.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "users u ON rc.created_by = u.id
		WHERE 1=1";

	if ($stock_id != '')
		$sql .= " AND rc.stock_id=" . db_escape($stock_id);
	if ($status != '')
		$sql .= " AND rc.status=" . db_escape($status);
	if ($severity != '')
		$sql .= " AND rc.severity=" . db_escape($severity);
	if ($date_from != '')
		$sql .= " AND rc.start_date >= " . db_escape($date_from);
	if ($date_to != '')
		$sql .= " AND rc.start_date <= " . db_escape($date_to);

	$sql .= " ORDER BY rc.id DESC";

	$result = db_query($sql, 'could not get recall campaigns with progress');
	$campaigns = array();
	while ($row = db_fetch($result))
		$campaigns[] = $row;
	return $campaigns;
}

/**
 * Get recall items for a campaign grouped by their current location (in stock vs delivered).
 *
 * @param int $campaign_id Campaign id
 * @return array ['in_stock' => [...], 'delivered' => [...], 'other' => [...]]
 */
function get_recall_items_by_disposition($campaign_id)
{
	$items = get_recall_items($campaign_id);

	$in_stock = array();
	$delivered = array();
	$other = array();

	while ($item = db_fetch($items)) {
		if ($item['serial_status'] === 'available' || $item['serial_status'] === 'recalled'
			|| $item['serial_status'] === 'quarantine') {
			$in_stock[] = $item;
		} elseif ($item['customer_id']) {
			$delivered[] = $item;
		} else {
			$other[] = $item;
		}
	}

	return array(
		'in_stock'  => $in_stock,
		'delivered' => $delivered,
		'other'     => $other,
	);
}

/**
 * Get recall campaign history / timeline of status changes and actions.
 *
 * Constructs a timeline from item status changes and campaign status.
 *
 * @param int $campaign_id Campaign id
 * @return array Timeline entries: [date, action, detail, count]
 */
function get_recall_campaign_timeline($campaign_id)
{
	$timeline = array();

	$campaign = get_recall_campaign($campaign_id);
	if (!$campaign)
		return $timeline;

	// Campaign creation
	$timeline[] = array(
		'date'   => $campaign['created_at'],
		'action' => _('Campaign Created'),
		'detail' => sprintf(_('Recall campaign %s created for %s'), $campaign['reference'], $campaign['stock_id']),
		'type'   => 'create',
	);

	// Count identified items by date (notification_date, return_date, resolution_date)
	$sql = "SELECT 'notified' AS event,
		notification_date AS event_date,
		COUNT(*) AS cnt
		FROM " . TB_PREF . "recall_items
		WHERE recall_id=" . (int)$campaign_id
		. " AND notification_date IS NOT NULL
		GROUP BY notification_date

		UNION ALL

		SELECT 'returned' AS event,
		return_date AS event_date,
		COUNT(*) AS cnt
		FROM " . TB_PREF . "recall_items
		WHERE recall_id=" . (int)$campaign_id
		. " AND return_date IS NOT NULL
		GROUP BY return_date

		UNION ALL

		SELECT 'resolved' AS event,
		resolution_date AS event_date,
		COUNT(*) AS cnt
		FROM " . TB_PREF . "recall_items
		WHERE recall_id=" . (int)$campaign_id
		. " AND resolution_date IS NOT NULL
		GROUP BY resolution_date

		ORDER BY event_date";

	$result = db_query($sql, 'could not get recall timeline');
	$event_labels = array(
		'notified' => _('Customers Notified'),
		'returned' => _('Items Returned'),
		'resolved' => _('Items Resolved'),
	);
	while ($row = db_fetch($result)) {
		$label = isset($event_labels[$row['event']]) ? $event_labels[$row['event']] : $row['event'];
		$timeline[] = array(
			'date'   => $row['event_date'],
			'action' => $label,
			'detail' => sprintf(_('%d items'), (int)$row['cnt']),
			'type'   => $row['event'],
		);
	}

	return $timeline;
}
