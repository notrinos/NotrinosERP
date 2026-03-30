<?php
/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	Approval Timeline UI Widget

	Renders a visual timeline of approval actions for a draft,
	showing each step with user name, action, timestamp, comments,
	and level information.

	PHP 5.6+ compatible.
***********************************************************************/

/**
 * Display a visual approval timeline for a draft.
 *
 * Renders the complete history of approval actions as a vertical
 * timeline with icons, colors, and status indicators for each step.
 *
 * @param int  $draft_id   Draft ID to display timeline for
 * @param bool $reverse    If true, show oldest first (default: newest first)
 * @return void
 */
function display_approval_timeline($draft_id, $reverse = false)
{
	$history = get_approval_history($draft_id, $reverse);

	if (!$history || db_num_rows($history) == 0) {
		display_note(_('No approval history available for this draft.'));
		return;
	}

	echo "<div class='approval-timeline'>\n";
	echo "<table class='tablestyle' style='width:100%;'>\n";
	echo "<tr><th colspan='5' class='tableheader'>" . _('Approval Timeline') . "</th></tr>\n";
	echo "<tr>";
	echo "<th>" . _('Date/Time') . "</th>";
	echo "<th>" . _('Action') . "</th>";
	echo "<th>" . _('User') . "</th>";
	echo "<th>" . _('Level') . "</th>";
	echo "<th>" . _('Comments') . "</th>";
	echo "</tr>\n";

	$row_index = 0;
	while ($action = db_fetch($history)) {
		$row_class = ($row_index % 2 == 0) ? 'oddrow' : 'evenrow';
		$action_badge = format_approval_action_badge($action['action_type']);
		$user_name = $action['user_name'] ? $action['user_name'] : _('System');
		$level_display = (int)$action['level'] > 0
			? sprintf(_('Level %d'), (int)$action['level'])
			: '-';
		$comments = $action['comments'] ? htmlspecialchars($action['comments'], ENT_QUOTES, 'UTF-8') : '-';
		$action_date = sql2date($action['action_date']);
		$action_time = date('H:i', strtotime($action['action_date']));

		echo "<tr class='$row_class'>";
		echo "<td>" . $action_date . " " . $action_time . "</td>";
		echo "<td>" . $action_badge . "</td>";
		echo "<td>" . htmlspecialchars($user_name, ENT_QUOTES, 'UTF-8') . "</td>";
		echo "<td align='center'>" . $level_display . "</td>";
		echo "<td>" . $comments . "</td>";
		echo "</tr>\n";

		$row_index++;
	}

	echo "</table>\n";
	echo "</div>\n";
}

/**
 * Display a compact inline timeline showing the approval progress.
 *
 * Shows a horizontal progress bar with level indicators
 * and the current position in the approval chain.
 *
 * @param array $draft   Draft row from approval_drafts
 * @param int   $max_level Maximum level number in the workflow
 * @return void
 */
function display_approval_progress_bar($draft, $max_level)
{
	$current_level = (int)$draft['current_level'];
	$status = (int)$draft['status'];

	echo "<div class='approval-progress' style='margin:8px 0;'>\n";
	echo "<table style='width:100%;border-collapse:collapse;'><tr>\n";

	for ($level = 1; $level <= $max_level; $level++) {
		$width = round(100 / $max_level, 1);

		if ($status == APPROVAL_STATUS_APPROVED) {
			$bg_color = '#28a745';
			$text_color = '#fff';
			$label = _('Approved');
		} elseif ($status == APPROVAL_STATUS_REJECTED) {
			if ($level <= $current_level) {
				$bg_color = ($level == $current_level) ? '#dc3545' : '#28a745';
				$text_color = '#fff';
				$label = ($level == $current_level) ? _('Rejected') : _('Approved');
			} else {
				$bg_color = '#e9ecef';
				$text_color = '#6c757d';
				$label = _('Skipped');
			}
		} elseif ($status == APPROVAL_STATUS_CANCELLED) {
			$bg_color = '#6c757d';
			$text_color = '#fff';
			$label = _('Cancelled');
		} elseif ($level < $current_level) {
			$bg_color = '#28a745';
			$text_color = '#fff';
			$label = _('Approved');
		} elseif ($level == $current_level) {
			$bg_color = '#ffc107';
			$text_color = '#212529';
			$label = _('Pending');
		} else {
			$bg_color = '#e9ecef';
			$text_color = '#6c757d';
			$label = _('Waiting');
		}

		echo "<td style='width:{$width}%;background:{$bg_color};color:{$text_color};"
			. "text-align:center;padding:6px 4px;border:1px solid #dee2e6;font-size:11px;'>"
			. sprintf(_('L%d: %s'), $level, $label) . "</td>\n";
	}

	echo "</tr></table>\n";
	echo "</div>\n";
}

/**
 * Format an action type as a colored badge/label.
 *
 * @param string $action_type Action type string (submit, approve, reject, etc.)
 * @return string             HTML badge markup
 */
function format_approval_action_badge($action_type)
{
	$badges = array(
		'submit'    => array('bg' => '#007bff', 'icon' => 'send',       'label' => _('Submitted')),
		'approve'   => array('bg' => '#28a745', 'icon' => 'check',      'label' => _('Approved')),
		'reject'    => array('bg' => '#dc3545', 'icon' => 'x',          'label' => _('Rejected')),
		'cancel'    => array('bg' => '#6c757d', 'icon' => 'x-circle',   'label' => _('Cancelled')),
		'delegate'  => array('bg' => '#17a2b8', 'icon' => 'share',      'label' => _('Delegated')),
		'escalate'  => array('bg' => '#fd7e14', 'icon' => 'arrow-up',   'label' => _('Escalated')),
		'edit'      => array('bg' => '#ffc107', 'icon' => 'edit',       'label' => _('Edited')),
		'resubmit'  => array('bg' => '#6610f2', 'icon' => 'undo',       'label' => _('Resubmitted')),
	);

	if (isset($badges[$action_type])) {
		$badge = $badges[$action_type];
		return "<span style='display:inline-block;padding:2px 8px;border-radius:3px;"
			. "background:{$badge['bg']};color:#fff;font-size:11px;white-space:nowrap;'>"
			. default_theme_icon($badge['icon'])
			. $badge['label'] . "</span>";
	}

	return htmlspecialchars($action_type, ENT_QUOTES, 'UTF-8');
}

/**
 * Display a summary info box for a draft's approval status.
 *
 * Shows the current status, submitted by, submitted date,
 * workflow name, current level, and amount in a compact format.
 *
 * @param array $draft Draft row (with joined workflow data)
 * @return void
 */
function display_approval_summary_box($draft)
{
	global $approval_status_labels;

	$status = (int)$draft['status'];
	$status_label = isset($approval_status_labels[$status])
		? $approval_status_labels[$status]
		: _('Unknown');
	$status_badge = format_approval_status_badge($status);

	echo "<table class='tablestyle2' style='width:100%;'>\n";

	echo "<tr><td class='label'>" . _('Status') . ":</td>"
		. "<td>" . $status_badge . "</td></tr>\n";

	echo "<tr><td class='label'>" . _('Workflow') . ":</td>"
		. "<td>" . htmlspecialchars($draft['workflow_name'], ENT_QUOTES, 'UTF-8') . "</td></tr>\n";

	echo "<tr><td class='label'>" . _('Transaction Type') . ":</td>"
		. "<td>" . get_trans_type_label((int)$draft['trans_type']) . "</td></tr>\n";

	$draft_ref = 'DRF-' . str_pad((int)$draft['id'], 4, '0', STR_PAD_LEFT);
	echo "<tr><td class='label'>" . _('Draft #') . ":</td>"
		. "<td><strong>" . $draft_ref . "</strong></td></tr>\n";

	if ($draft['reference']) {
		echo "<tr><td class='label'>" . _('Trans. Reference') . ":</td>"
			. "<td>" . htmlspecialchars($draft['reference'], ENT_QUOTES, 'UTF-8') . "</td></tr>\n";
	}

	if ($draft['amount'] > 0) {
		echo "<tr><td class='label'>" . _('Amount') . ":</td>"
			. "<td>" . number_format2($draft['amount'], user_price_dec())
			. ($draft['currency'] ? ' ' . $draft['currency'] : '') . "</td></tr>\n";
	}

	if ($draft['summary']) {
		echo "<tr><td class='label'>" . _('Summary') . ":</td>"
			. "<td>" . htmlspecialchars($draft['summary'], ENT_QUOTES, 'UTF-8') . "</td></tr>\n";
	}

	echo "<tr><td class='label'>" . _('Submitted Date') . ":</td>"
		. "<td>" . sql2date($draft['submitted_date']) . "</td></tr>\n";

	echo "<tr><td class='label'>" . _('Current Level') . ":</td>"
		. "<td>" . sprintf(_('Level %d'), (int)$draft['current_level']) . "</td></tr>\n";

	if ($draft['completed_date']) {
		echo "<tr><td class='label'>" . _('Completed Date') . ":</td>"
			. "<td>" . sql2date($draft['completed_date']) . "</td></tr>\n";
	}

	echo "</table>\n";
}

/**
 * Format a status integer as a colored badge.
 *
 * @param int $status Status constant (APPROVAL_STATUS_*)
 * @return string     HTML badge markup
 */
function format_approval_status_badge($status)
{
	$colors = array(
		APPROVAL_STATUS_PENDING   => array('bg' => '#ffc107', 'color' => '#212529', 'label' => _('Pending')),
		APPROVAL_STATUS_APPROVED  => array('bg' => '#28a745', 'color' => '#fff',    'label' => _('Approved')),
		APPROVAL_STATUS_REJECTED  => array('bg' => '#dc3545', 'color' => '#fff',    'label' => _('Rejected')),
		APPROVAL_STATUS_CANCELLED => array('bg' => '#6c757d', 'color' => '#fff',    'label' => _('Cancelled')),
		APPROVAL_STATUS_EXPIRED   => array('bg' => '#fd7e14', 'color' => '#fff',    'label' => _('Expired')),
		APPROVAL_STATUS_DELEGATED => array('bg' => '#17a2b8', 'color' => '#fff',    'label' => _('Delegated')),
	);

	if (isset($colors[$status])) {
		$c = $colors[$status];
		return "<span style='display:inline-block;padding:2px 10px;border-radius:3px;"
			. "background:{$c['bg']};color:{$c['color']};font-size:11px;font-weight:bold;'>"
			. $c['label'] . "</span>";
	}

	return _('Unknown');
}
