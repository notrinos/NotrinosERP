<?php
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

	return "<span style='display:inline-block;min-width:18px;padding:2px 6px;"
		. "border-radius:10px;background:#dc3545;color:#fff;font-size:10px;"
		. "font-weight:bold;text-align:center;line-height:14px;margin-left:4px;'>"
		. $count . "</span>";
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

	return "<span style='display:inline-block;min-width:18px;padding:2px 6px;"
		. "border-radius:10px;background:#007bff;color:#fff;font-size:10px;"
		. "font-weight:bold;text-align:center;line-height:14px;margin-left:4px;'>"
		. $count . "</span>";
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
	$cards = array(
		array(
			'label'   => _('My Pending Actions'),
			'count'   => isset($counts['my_pending']) ? (int)$counts['my_pending'] : 0,
			'bg'      => '#ffc107',
			'color'   => '#212529',
			'icon'    => 'fas fa-clock',
		),
		array(
			'label'   => _('Total Pending'),
			'count'   => isset($counts['pending']) ? (int)$counts['pending'] : 0,
			'bg'      => '#17a2b8',
			'color'   => '#fff',
			'icon'    => 'fas fa-hourglass-half',
		),
		array(
			'label'   => _('Approved'),
			'count'   => isset($counts['approved']) ? (int)$counts['approved'] : 0,
			'bg'      => '#28a745',
			'color'   => '#fff',
			'icon'    => 'fas fa-check-circle',
		),
		array(
			'label'   => _('Rejected'),
			'count'   => isset($counts['rejected']) ? (int)$counts['rejected'] : 0,
			'bg'      => '#dc3545',
			'color'   => '#fff',
			'icon'    => 'fas fa-times-circle',
		),
	);

	echo "<div style='display:flex;flex-wrap:wrap;gap:12px;margin:12px 0;'>\n";

	foreach ($cards as $card) {
		echo "<div style='flex:1;min-width:160px;padding:16px;border-radius:6px;"
			. "background:{$card['bg']};color:{$card['color']};"
			. "box-shadow:0 2px 4px rgba(0,0,0,0.1);'>\n";
		echo "<div style='font-size:24px;font-weight:bold;'>"
			. "<i class='{$card['icon']}' style='margin-right:8px;'></i>"
			. $card['count'] . "</div>\n";
		echo "<div style='font-size:12px;margin-top:4px;opacity:0.9;'>"
			. $card['label'] . "</div>\n";
		echo "</div>\n";
	}

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

	return "<span style='display:inline-block;padding:1px 8px;border-radius:3px;"
		. "background:{$m[0]};color:{$m[1]};font-size:11px;white-space:nowrap;'>"
		. $m[2] . "</span>";
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
		return "<span style='color:#dc3545;' title='" . sprintf(_('%d days pending'), (int)$days) . "'>"
			. "<i class='fas fa-exclamation-triangle'></i></span>";
	} elseif ($days > 3) {
		return "<span style='color:#ffc107;' title='" . sprintf(_('%d days pending'), (int)$days) . "'>"
			. "<i class='fas fa-exclamation-circle'></i></span>";
	}

	return '';
}
