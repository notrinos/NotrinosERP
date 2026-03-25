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
	NotrinosERP-1.0 Approval Workflow System
	Approval Status Badges UI Widget

	Provides reusable badge/label components for displaying approval
	statuses, counts, and notification indicators throughout the UI.

	PHP 5.6+ compatible.
***********************************************************************/

/**
 * Render a pending approval count badge for the navigation/sidebar.
 *
 * Displays a small colored badge with the number of pending approvals
 * for the current user's role. Returns empty string if count is zero.
 *
 * @param int $count Number of pending approvals (0 = no badge)
 * @return string    HTML badge markup or empty string
 */
function approval_count_badge($count)
{
	$count = (int)$count;
	if ($count <= 0)
		return '';

	return "<span class='approval-count-badge'>".$count."</span>";
}

/**
 * Render an unread notification count badge.
 *
 * Similar to approval_count_badge but uses a different color scheme
 * to distinguish from pending approval counts.
 *
 * @param int $count Number of unread notifications
 * @return string    HTML badge markup or empty string
 */
function notification_count_badge($count)
{
	$count = (int)$count;
	if ($count <= 0)
		return '';

	return "<span class='approval-notification-badge'>".$count."</span>";
}

/**
 * Display a row of dashboard summary cards (stat boxes).
 *
 * Shows counts for each approval status in a horizontal row of cards.
 *
 * @param array $counts Associative array with keys:
 *   'pending'   => int  Number of pending drafts
 *   'approved'  => int  Number of approved drafts
 *   'rejected'  => int  Number of rejected drafts
 *   'cancelled' => int  Number of cancelled drafts
 *   'my_pending' => int Number of drafts user can approve
 * @return void
 */
function display_approval_summary_cards($counts)
{
	global $path_to_root;
	include_once($path_to_root . '/dashboard/includes/dashboard_classes.inc');

	echo "<div class='approval-summary-cards'>\n";
	render_dashboard_small_stat_card(
		_('My Pending Actions'),
		isset($counts['my_pending']) ? (int)$counts['my_pending'] : 0,
		'clock',
		'success'
	);
	render_dashboard_small_stat_card(
		_('Total Pending'),
		isset($counts['pending']) ? (int)$counts['pending'] : 0,
		'hourglass',
		'info'
	);
	render_dashboard_small_stat_card(
		_('Approved'),
		isset($counts['approved']) ? (int)$counts['approved'] : 0,
		'check-circle',
		'primary'
	);
	render_dashboard_small_stat_card(
		_('Rejected'),
		isset($counts['rejected']) ? (int)$counts['rejected'] : 0,
		'x-circle',
		'danger'
	);
	echo "</div>\n";
}

/**
 * Display a compact status indicator suitable for use in table rows.
 *
 * Returns a small inline badge showing the status with appropriate color.
 * This is lighter than format_approval_status_badge() for use in dense tables.
 *
 * @param int $status Status constant (APPROVAL_STATUS_*)
 * @return string     HTML mini-badge markup
 */
function approval_status_cell($status)
{
	$status = (int)$status;

	$map = array(
		APPROVAL_STATUS_PENDING   => array('#ffc107', '#212529', _('Pending')),
		APPROVAL_STATUS_APPROVED  => array('#28a745', '#fff',    _('Approved')),
		APPROVAL_STATUS_REJECTED  => array('#dc3545', '#fff',    _('Rejected')),
		APPROVAL_STATUS_CANCELLED => array('#6c757d', '#fff',    _('Cancelled')),
		APPROVAL_STATUS_EXPIRED   => array('#fd7e14', '#fff',    _('Expired')),
		APPROVAL_STATUS_DELEGATED => array('#17a2b8', '#fff',    _('Delegated')),
	);

	if (!isset($map[$status]))
		return _('Unknown');

	$m = $map[$status];

	$class_name = 'approval-status-badge';
	if ($status == APPROVAL_STATUS_PENDING)
		$class_name .= ' is-pending';
	elseif ($status == APPROVAL_STATUS_APPROVED)
		$class_name .= ' is-approved';
	elseif ($status == APPROVAL_STATUS_REJECTED)
		$class_name .= ' is-rejected';
	elseif ($status == APPROVAL_STATUS_CANCELLED)
		$class_name .= ' is-cancelled';
	elseif ($status == APPROVAL_STATUS_EXPIRED)
		$class_name .= ' is-expired';
	elseif ($status == APPROVAL_STATUS_DELEGATED)
		$class_name .= ' is-delegated';

	return "<span class='".$class_name."'>".$m[2]."</span>";
}

/**
 * Display a priority/urgency indicator based on how long a draft has been pending.
 *
 * @param string $submitted_date SQL datetime of submission
 * @return string                HTML indicator markup
 */
function approval_urgency_indicator($submitted_date)
{
	$submitted = strtotime($submitted_date);
	$now = time();
	$days = ($now - $submitted) / 86400;

	if ($days > 7) {
		return "<span class='approval-urgency is-overdue' title='" . sprintf(_('%d days pending'), (int)$days) . "'>"
			. default_theme_icon('alert-triangle') . "</span>";
	} elseif ($days > 3) {
		return "<span class='approval-urgency is-warning' title='" . sprintf(_('%d days pending'), (int)$days) . "'>"
			. default_theme_icon('info-circle') . "</span>";
	}

	return '';
}
