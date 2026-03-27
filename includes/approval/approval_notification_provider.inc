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

/**********************************************************************
	NotrinosERP-1.0 Notification System
	Approval Workflow Notification Provider

	Bridges the approval workflow system into the pluggable notification
	framework. Uses existing approval_notifications and approval_drafts
	tables to populate the notification bell dropdown.

	PHP 5.6+ compatible.
***********************************************************************/

/**
 * Notification provider for the approval workflow system.
 *
 * Queries approval_notifications table for unread items and pending
 * approval_drafts for the current user's role. Provides data for
 * the topbar notification dropdown.
 */
class ApprovalNotificationProvider implements NotificationProvider
{
	/** @var int Current user ID */
	private $user_id;

	/** @var int Current user's security role ID */
	private $role_id;

	/** @var int|null Cached count */
	private $count_cache = null;

	/**
	 * Constructor.
	 *
	 * @param int $user_id Current user ID
	 * @param int $role_id Current user's security role ID
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
		return 'approval';
	}

	/**
	 * {@inheritdoc}
	 */
	public function getSectionLabel()
	{
		return _('Pending Approvals');
	}

	/**
	 * {@inheritdoc}
	 */
	public function getSectionIcon()
	{
		return 'check-circle';
	}

	/**
	 * {@inheritdoc}
	 */
	public function getCount()
	{
		if ($this->count_cache !== null)
			return $this->count_cache;

		$this->count_cache = count_pending_approval_drafts($this->role_id);

		return $this->count_cache;
	}

	/**
	 * {@inheritdoc}
	 */
	public function getItems($limit = 5)
	{
		global $path_to_root, $systypes_array;

		$result = get_pending_approval_drafts_for_user(
			$this->user_id, $this->role_id, array()
		);

		$items = array();
		$count = 0;

		while ($row = db_fetch($result)) {
			if ($count >= $limit)
				break;

			$trans_type = (int)$row['trans_type'];
			$type_label = isset($systypes_array[$trans_type]) ? $systypes_array[$trans_type] : _('Transaction');

			$type_config = ApprovalWorkflow::getTypeConfig($trans_type);
			$application_code = $type_config ? $type_config['application'] : '';

			$reference = $row['reference'] ? $row['reference'] : '#' . $row['id'];
			$summary = $row['summary'] ? $row['summary'] : $type_label . ' ' . $reference;
			$submitted_by_name = isset($row['submitted_by_name']) ? $row['submitted_by_name'] : '';

			$title = $type_label . ' ' . $reference;
			if ($submitted_by_name) {
				$description = sprintf(_('Submitted by %s'), $submitted_by_name);
			} else {
				$description = $summary;
			}

			$amount_display = '';
			if (isset($row['amount']) && $row['amount'] > 0) {
				$amount_display = number_format2($row['amount'], user_price_dec());
				if (isset($row['currency']) && $row['currency'])
					$amount_display = $row['currency'] . ' ' . $amount_display;
			}

			$draft_url = $path_to_root . '/admin/approval_view_draft.php?draft_id=' . (int)$row['id'];

			$time_display = '';
			if (isset($row['submitted_date']))
				$time_display = $this->format_relative_time($row['submitted_date']);

			$items[] = array(
				'id'          => (int)$row['id'],
				'title'       => $title,
				'summary'     => $description,
				'amount'      => $amount_display,
				'time'        => $time_display,
				'url'         => $draft_url,
				'type'        => 'pending',
				'is_read'     => false,
				'application' => $application_code,
			);

			$count++;
		}

		return $items;
	}

	/**
	 * {@inheritdoc}
	 */
	public function getViewAllUrl()
	{
		global $path_to_root;
		return $path_to_root . '/admin/approval_inquiry.php';
	}

	/**
	 * Format a database datetime as a human-readable relative time string.
	 *
	 * @param string $datetime MySQL datetime string (Y-m-d H:i:s)
	 * @return string e.g., '2 hours ago', '3 days ago'
	 */
	private function format_relative_time($datetime)
	{
		$timestamp = strtotime($datetime);
		if ($timestamp === false)
			return '';

		$diff = time() - $timestamp;

		if ($diff < 0)
			return _('just now');

		if ($diff < 60)
			return _('just now');

		if ($diff < 3600) {
			$minutes = (int)floor($diff / 60);
			return $minutes == 1 ? _('1 minute ago') : sprintf(_('%d minutes ago'), $minutes);
		}

		if ($diff < 86400) {
			$hours = (int)floor($diff / 3600);
			return $hours == 1 ? _('1 hour ago') : sprintf(_('%d hours ago'), $hours);
		}

		if ($diff < 604800) {
			$days = (int)floor($diff / 86400);
			return $days == 1 ? _('1 day ago') : sprintf(_('%d days ago'), $days);
		}

		if ($diff < 2592000) {
			$weeks = (int)floor($diff / 604800);
			return $weeks == 1 ? _('1 week ago') : sprintf(_('%d weeks ago'), $weeks);
		}

		$months = (int)floor($diff / 2592000);
		return $months == 1 ? _('1 month ago') : sprintf(_('%d months ago'), $months);
	}
}
