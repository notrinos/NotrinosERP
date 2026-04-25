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
 * Notification provider for purchase-focused alerts.
 */
class PurchaseNotificationProvider implements NotificationProvider
{
	/** @var int */
	private $user_id;

	/** @var int */
	private $role_id;

	/** @var int|null */
	private $count_cache = null;

	/**
	 * Constructor.
	 *
	 * @param int $user_id
	 * @param int $role_id
	 */
	public function __construct($user_id, $role_id)
	{
		$this->user_id = (int)$user_id;
		$this->role_id = (int)$role_id;
	}

	/**
	 * {@inheritdoc}
	 */
	public function getSectionId()
	{
		return 'purchase';
	}

	/**
	 * {@inheritdoc}
	 */
	public function getSectionLabel()
	{
		return _('Purchase Alerts');
	}

	/**
	 * {@inheritdoc}
	 */
	public function getSectionIcon()
	{
		return 'shopping-cart';
	}

	/**
	 * {@inheritdoc}
	 */
	public function getCount()
	{
		if ($this->count_cache !== null)
			return $this->count_cache;

		$stats = $this->get_purchase_alert_counts();
		$this->count_cache = array_sum($stats);
		return $this->count_cache;
	}

	/**
	 * {@inheritdoc}
	 */
	public function getItems($limit)
	{
		global $path_to_root;

		$stats = $this->get_purchase_alert_counts();
		$items = array(
			array(
				'id' => 'purchase-requisition-pending',
				'title' => _('Pending Requisition Approvals'),
				'summary' => sprintf(_('%d requisitions awaiting approval'), (int)$stats['pending_requisition_approvals']),
				'time' => '',
				'url' => $path_to_root . '/purchasing/inquiry/purch_requisitions_view.php',
				'type' => 'pending',
				'is_read' => false,
			),
			array(
				'id' => 'purchase-rfq-awaiting',
				'title' => _('RFQs Awaiting Response'),
				'summary' => sprintf(_('%d vendor invitations still pending'), (int)$stats['rfq_waiting_response']),
				'time' => '',
				'url' => $path_to_root . '/purchasing/inquiry/purch_rfq_view.php',
				'type' => 'pending',
				'is_read' => false,
			),
			array(
				'id' => 'purchase-overdue-po',
				'title' => _('Overdue Open Purchase Orders'),
				'summary' => sprintf(_('%d purchase orders are still open beyond 30 days'), (int)$stats['overdue_purchase_orders']),
				'time' => '',
				'url' => $path_to_root . '/purchasing/inquiry/po_search_completed.php?OutstandingOnly=1',
				'type' => 'warning',
				'is_read' => false,
			),
			array(
				'id' => 'purchase-matching-open',
				'title' => _('Open Matching Exceptions'),
				'summary' => sprintf(_('%d 3-way matching exceptions need review'), (int)$stats['matching_exceptions_open']),
				'time' => '',
				'url' => $path_to_root . '/purchasing/inquiry/matching_exceptions.php',
				'type' => 'warning',
				'is_read' => false,
			),
			array(
				'id' => 'purchase-agreement-expiring',
				'title' => _('Expiring Purchase Agreements'),
				'summary' => sprintf(_('%d agreements expire in the next 30 days'), (int)$stats['agreements_expiring']),
				'time' => '',
				'url' => $path_to_root . '/purchasing/inquiry/purch_agreements_view.php',
				'type' => 'warning',
				'is_read' => false,
			),
			array(
				'id' => 'purchase-reorder-alerts',
				'title' => _('Reorder Alerts'),
				'summary' => sprintf(_('%d items are flagged for replenishment'), (int)$stats['below_reorder_alerts']),
				'time' => '',
				'url' => $path_to_root . '/purchasing/inquiry/reorder_status.php',
				'type' => 'pending',
				'is_read' => false,
			),
		);

		$filtered = array();
		foreach ($items as $item) {
			if (preg_match('/(\d+)/', $item['summary'], $matches) && (int)$matches[1] <= 0)
				continue;
			$filtered[] = $item;
		}

		if ((int)$limit > 0)
			$filtered = array_slice($filtered, 0, (int)$limit);

		return $filtered;
	}

	/**
	 * {@inheritdoc}
	 */
	public function getViewAllUrl()
	{
		global $path_to_root;
		return $path_to_root . '/purchasing/dashboard/purchase_dashboard.php';
	}

	/**
	 * Check table existence.
	 *
	 * @param string $table_name
	 * @return bool
	 */
	private function table_exists($table_name)
	{
		$sql = "SHOW TABLES LIKE " . db_escape($table_name);
		$result = db_query($sql, 'could not check purchase notification table');
		return db_num_rows($result) > 0;
	}

	/**
	 * Count purchase alert categories.
	 *
	 * @return array
	 */
	private function get_purchase_alert_counts()
	{
		$today = date2sql(Today());
		$thirty_days_ago = date('Y-m-d', strtotime($today . ' -30 days'));
		$thirty_days_ahead = date('Y-m-d', strtotime($today . ' +30 days'));

		$counts = array(
			'pending_requisition_approvals' => 0,
			'rfq_waiting_response' => 0,
			'overdue_purchase_orders' => 0,
			'matching_exceptions_open' => 0,
			'agreements_expiring' => 0,
			'below_reorder_alerts' => 0,
		);

		if ($this->table_exists(TB_PREF . 'approval_drafts') && $this->table_exists(TB_PREF . 'approval_levels')) {
			$sql = "SELECT COUNT(DISTINCT d.id)
				FROM " . TB_PREF . "approval_drafts d
				INNER JOIN " . TB_PREF . "approval_levels al
					ON al.workflow_id = d.workflow_id
					AND al.level = d.current_level
					AND al.role_id = " . db_escape($this->role_id) . "
					AND al.is_active = 1
				WHERE d.status = 0
					AND d.trans_type = " . ST_PURCHREQUISITION;
			$counts['pending_requisition_approvals'] = (int)$this->scalar($sql, 'could not count pending purchase requisition approvals');
		}

		if ($this->table_exists(TB_PREF . 'purch_rfq_vendors')) {
			$sql = "SELECT COUNT(*)
				FROM " . TB_PREF . "purch_rfq_vendors
				WHERE status = 'invited'";
			$counts['rfq_waiting_response'] = (int)$this->scalar($sql, 'could not count pending RFQ responses');
		}

		$sql = "SELECT COUNT(DISTINCT po.order_no)
			FROM " . TB_PREF . "purch_orders po
			INNER JOIN " . TB_PREF . "purch_order_details line ON line.order_no = po.order_no
			WHERE line.quantity_received < line.quantity_ordered
				AND po.ord_date <= " . db_escape($thirty_days_ago);
		$counts['overdue_purchase_orders'] = (int)$this->scalar($sql, 'could not count overdue purchase orders');

		if ($this->table_exists(TB_PREF . 'purch_matching_exceptions')) {
			$sql = "SELECT COUNT(*)
				FROM " . TB_PREF . "purch_matching_exceptions
				WHERE status = 'open'";
			$counts['matching_exceptions_open'] = (int)$this->scalar($sql, 'could not count open matching exceptions');
		}

		if ($this->table_exists(TB_PREF . 'purch_agreements')) {
			$sql = "SELECT COUNT(*)
				FROM " . TB_PREF . "purch_agreements
				WHERE inactive = 0
					AND status IN ('confirmed','active')
					AND date_end IS NOT NULL
					AND date_end >= " . db_escape($today) . "
					AND date_end <= " . db_escape($thirty_days_ahead);
			$counts['agreements_expiring'] = (int)$this->scalar($sql, 'could not count expiring purchase agreements');
		}

		if (function_exists('evaluate_replenishment_rules')) {
			$suggestions = evaluate_replenishment_rules(null, null);
			$counts['below_reorder_alerts'] = is_array($suggestions) ? count($suggestions) : 0;
		}

		return $counts;
	}

	/**
	 * Execute scalar query helper.
	 *
	 * @param string $sql
	 * @param string $error_message
	 * @return float
	 */
	private function scalar($sql, $error_message)
	{
		$result = db_query($sql, $error_message);
		$row = db_fetch_row($result);
		if (!$row)
			return 0;
		return (float)$row[0];
	}
}
