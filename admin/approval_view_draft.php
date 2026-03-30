<?php
/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	Admin: Draft Detail Viewer

	Displays comprehensive details of a single approval draft, including:
	- Draft summary information (status, workflow, amounts)
	- Approval progress bar showing level completion
	- Full approval timeline/history
	- Approval level configuration for the workflow
	- Draft data (JSON decoded)
	- Action buttons (Approve/Reject) if user has permission

	Designed to open in a popup window from the dashboard or inquiry.

	Phase 3 of the Approval System Development Plan.
***********************************************************************/
$page_security = 'SA_APPROVALDASHBOARD';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');

$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 600);

page(_($help_context = 'View Approval Draft'), true, false, '', $js);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/approval/db/approval_db.inc');
include_once($path_to_root . '/includes/approval/db/approval_rules_db.inc');
include_once($path_to_root . '/includes/approval/db/approval_history_db.inc');
include_once($path_to_root . '/includes/approval/ui/approval_timeline.inc');
include_once($path_to_root . '/includes/approval/ui/approval_badges.inc');
include_once($path_to_root . '/admin/db/approval_rules_setup_db.inc');
include_once($path_to_root . '/admin/db/approval_dashboard_db.inc');
include_once($path_to_root . '/includes/approval/approval_delegation.inc');
include_once($path_to_root . '/includes/approval/approval_escalation.inc');

// =====================================================
// Get draft ID from request
// =====================================================

$draft_id = 0;
if (isset($_GET['draft_id']))
	$draft_id = (int)$_GET['draft_id'];
elseif (isset($_POST['draft_id']))
	$draft_id = (int)get_post('draft_id');

if ($draft_id <= 0) {
	display_error(_('No draft ID specified.'));
	end_page();
	exit;
}

// =====================================================
// Load draft data
// =====================================================

$draft = get_approval_draft_full($draft_id);

if (!$draft) {
	display_error(_('Draft not found.'));
	end_page();
	exit;
}

$current_user_id = $_SESSION['wa_current_user']->user;
$current_role_id = $_SESSION['wa_current_user']->access;

// =====================================================
// Handle approval/rejection actions
// =====================================================

if (isset($_POST['ApproveDraft'])) {
	$comments = get_post('approve_comments', '');
	$approval_service = get_approval_workflow_service();
	$result = $approval_service->approve($draft_id, $comments);

	if ($result['status'] === 'error') {
		display_error($result['message']);
	} else {
		display_notification($result['message']);
		// Reload draft after action
		$draft = get_approval_draft_full($draft_id);
	}
}

if (isset($_POST['RejectDraft'])) {
	$comments = get_post('reject_comments', '');
	$approval_service = get_approval_workflow_service();
	$result = $approval_service->reject($draft_id, $comments);

	if ($result['status'] === 'error') {
		display_error($result['message']);
	} else {
		display_notification($result['message']);
		$draft = get_approval_draft_full($draft_id);
	}
}

if (isset($_POST['CancelDraft'])) {
	$comments = get_post('cancel_comments', '');
	$approval_service = get_approval_workflow_service();
	$result = $approval_service->cancel($draft_id, $comments);

	if ($result['status'] === 'error') {
		display_error($result['message']);
	} else {
		display_notification($result['message']);
		$draft = get_approval_draft_full($draft_id);
	}
}

// =====================================================
// Display draft details
// =====================================================

start_form();
hidden('draft_id', $draft_id);

// --- Section 1: Draft Summary ---
echo "<h3 style='margin:12px 0 8px 0;'>"
	. default_theme_icon('file-text')
	. _('Draft Information') . "</h3>\n";

display_approval_summary_box($draft);

// --- Section 2: Approval Progress Bar ---
$max_level = get_max_approval_level($draft['workflow_id']);
if ($max_level > 0) {
	echo "<h3 style='margin:16px 0 8px 0;'>"
		. default_theme_icon('list-check')
		. _('Approval Progress') . "</h3>\n";

	display_approval_progress_bar($draft, $max_level);
}

// --- Section 2b: Escalation Status ---
if ((int)$draft['status'] === APPROVAL_STATUS_PENDING) {
	$escalation_status = get_draft_escalation_status($draft_id);
	if ($escalation_status) {
		echo "<h3 style='margin:16px 0 8px 0;'>"
			. default_theme_icon('alert-triangle')
			. _('Escalation Status') . "</h3>\n";

		$esc_color = '#28a745';
		$esc_icon = 'check-circle';
		$esc_label = _('OK — No Escalation Risk');
		if ($escalation_status['status'] === 'warning') {
			$esc_color = '#ffc107';
			$esc_icon = 'alert-triangle';
			$esc_label = sprintf(_('Warning — Escalation in %d day(s)'), $escalation_status['days_remaining']);
		} elseif ($escalation_status['status'] === 'overdue') {
			$esc_color = '#dc3545';
			$esc_icon = 'x-circle';
			$esc_label = sprintf(_('Overdue — Past escalation deadline by %d day(s)'), abs($escalation_status['days_remaining']));
		} elseif ($escalation_status['status'] === 'no_escalation') {
			$esc_color = '#6c757d';
			$esc_icon = 'minus-circle';
			$esc_label = _('No escalation configured for this level');
		}

		echo "<div style='border-left:4px solid {$esc_color};padding:8px 16px;margin:8px 0;background:#f8f9fa;border-radius:0 6px 6px 0;'>\n";
		echo default_theme_icon($esc_icon);
		echo "<strong style='color:{$esc_color};'>" . $esc_label . "</strong>";
		if (isset($escalation_status['escalation_days']) && $escalation_status['escalation_days'] > 0) {
			echo " <span style='color:#6c757d;margin-left:12px;'>(" . sprintf(_('Escalation after %d days'), $escalation_status['escalation_days']) . ")</span>";
		}
		echo "</div>\n";
	}
}

// --- Section 2c: Delegation Info ---
$delegation_history = get_draft_delegation_history($draft_id);
if (!empty($delegation_history)) {
	echo "<h3 style='margin:16px 0 8px 0;'>"
		. default_theme_icon('share')
		. _('Delegation History') . "</h3>\n";

	echo "<table class='tablestyle' style='width:100%;'>\n";
	echo "<tr>";
	echo "<th>" . _('Delegated From') . "</th>";
	echo "<th>" . _('Delegated To') . "</th>";
	echo "<th>" . _('Reason') . "</th>";
	echo "<th>" . _('Date') . "</th>";
	echo "</tr>\n";

	$row_index = 0;
	foreach ($delegation_history as $deleg) {
		$row_class = ($row_index % 2 == 0) ? 'oddrow' : 'evenrow';
		echo "<tr class='$row_class'>";
		echo "<td>" . htmlspecialchars($deleg['from_name'], ENT_QUOTES, 'UTF-8') . "</td>";
		echo "<td>" . htmlspecialchars($deleg['to_name'], ENT_QUOTES, 'UTF-8') . "</td>";
		echo "<td>" . htmlspecialchars($deleg['reason'], ENT_QUOTES, 'UTF-8') . "</td>";
		echo "<td>" . sql2date($deleg['delegated_date']) . "</td>";
		echo "</tr>\n";
		$row_index++;
	}

	echo "</table>\n";
}

// --- Section 3: Approval Levels Configuration ---
echo "<h3 style='margin:16px 0 8px 0;'>"
	. default_theme_icon('layers')
	. _('Workflow Levels') . "</h3>\n";

$levels = get_approval_levels_with_roles($draft['workflow_id']);
if ($levels && db_num_rows($levels) > 0) {
	echo "<table class='tablestyle' style='width:100%;'>\n";
	echo "<tr>";
	echo "<th>" . _('Level') . "</th>";
	echo "<th>" . _('Approver Role') . "</th>";
	echo "<th>" . _('Min Approvers') . "</th>";
	echo "<th>" . _('Auto-Approve Threshold') . "</th>";
	echo "<th>" . _('Status') . "</th>";
	echo "</tr>\n";

	$row_index = 0;
	while ($level = db_fetch($levels)) {
		$row_class = ($row_index % 2 == 0) ? 'oddrow' : 'evenrow';
		$level_num = (int)$level['level'];
		$current_level = (int)$draft['current_level'];
		$draft_status = (int)$draft['status'];

		// Determine this level's status
		if ($draft_status == APPROVAL_STATUS_APPROVED) {
			$level_status = "<span style='color:#28a745;'>".default_theme_icon('check-circle')." " . _('Completed') . "</span>";
		} elseif ($draft_status == APPROVAL_STATUS_REJECTED && $level_num == $current_level) {
			$level_status = "<span style='color:#dc3545;'>".default_theme_icon('x-circle')." " . _('Rejected') . "</span>";
		} elseif ($draft_status == APPROVAL_STATUS_CANCELLED) {
			$level_status = "<span style='color:#6c757d;'>".default_theme_icon('x-circle')." " . _('Cancelled') . "</span>";
		} elseif ($level_num < $current_level) {
			$level_status = "<span style='color:#28a745;'>".default_theme_icon('check-circle')." " . _('Completed') . "</span>";
		} elseif ($level_num == $current_level && $draft_status == APPROVAL_STATUS_PENDING) {
			$level_status = "<span style='color:#ffc107;'>".default_theme_icon('clock')." <strong>" . _('Current') . "</strong></span>";
		} else {
			$level_status = "<span style='color:#6c757d;'>".default_theme_icon('hourglass')." " . _('Waiting') . "</span>";
		}

		$role_name = $level['role_name']
			? htmlspecialchars($level['role_name'] . ' - ' . $level['role_description'], ENT_QUOTES, 'UTF-8')
			: _('Unknown Role');
		$threshold = (float)$level['amount_threshold'] > 0
			? number_format2($level['amount_threshold'], user_price_dec())
			: '-';

		echo "<tr class='$row_class'>";
		echo "<td align='center'><strong>" . _('Level') . " $level_num</strong></td>";
		echo "<td>$role_name</td>";
		echo "<td align='center'>" . (int)$level['min_approvers'] . "</td>";
		echo "<td align='right'>$threshold</td>";
		echo "<td align='center'>$level_status</td>";
		echo "</tr>\n";

		$row_index++;
	}

	echo "</table>\n";
} else {
	display_note(_('No approval levels configured for this workflow.'));
}

// --- Section 4: Approval Timeline ---
echo "<h3 style='margin:16px 0 8px 0;'>"
	. default_theme_icon('history')
	. _('Approval History') . "</h3>\n";

display_approval_timeline($draft_id, true);

// --- Section 5: Draft Data Preview ---
echo "<h3 style='margin:16px 0 8px 0;'>"
	. default_theme_icon('database')
	. _('Transaction Details') . "</h3>\n";

$draft_data = null;
$raw_draft_data = $draft['draft_data'];
if ($raw_draft_data) {
	$draft_data = json_decode($raw_draft_data, true);
	if ($draft_data === null)
		$draft_data = json_decode(stripslashes($raw_draft_data), true);
	if ($draft_data === null)
		$draft_data = json_decode(html_entity_decode($raw_draft_data, ENT_QUOTES, 'UTF-8'), true);
}
if ($draft_data && is_array($draft_data)) {
	// Friendly labels map
	$field_labels = array(
		'customer_name'     => _('Customer'),
		'customer_id'       => _('Customer ID'),
		'customer_currency' => _('Currency'),
		'document_date'     => _('Document Date'),
		'due_date'          => _('Due Date'),
		'reference'         => _('Reference'),
		'comments'          => _('Comments'),
		'Comments'          => _('Comments'),
		'location_name'     => _('Location'),
		'deliver_to'        => _('Deliver To'),
		'delivery_address'  => _('Delivery Address'),
		'phone'             => _('Phone'),
		'email'             => _('Email'),
		'freight_cost'      => _('Freight Cost'),
		'tax_included'      => _('Tax Included'),
		'credit'            => _('Credit Limit'),
		'ex_rate'           => _('Exchange Rate'),
		'ship_via'          => _('Shipping Method'),
		'salesman'          => _('Salesman'),
		'branch'            => _('Branch'),
		'prepaid'           => _('Prepaid'),
		'prep_amount'       => _('Prepaid Amount'),
		'supplier_name'     => _('Supplier'),
		'supplier_id'       => _('Supplier ID'),
		'supp_ref'          => _('Supplier Reference'),
		'requisition_no'    => _('Requisition No'),
		'into_stock_location' => _('Into Stock Location'),
		'grn_item_id'       => _('GRN Item'),
		'dimension_name'    => _('Dimension'),
		'dimension2_name'   => _('Dimension 2'),
	);

	// Fields to skip (internal/technical or duplicate code-level fields)
	$skip_fields = array(
		'trans_type', 'trans_no', 'tax_group_id', 'sales_type',
		'default_discount', 'price_factor', 'dimension_id', 'dimension2_id',
		'fixed_asset', 'bo_policy', 'so_type', 'cash_account', 'account_name',
		'payment', 'location', 'Location', 'branch', 'Branch', 'ship_via', 'salesman',
		'customer_id', 'supplier_id', 'person_type_id', 'person_id',
		'loc_code',
	);

	// Fields that are complex objects — extract a readable name
	$name_extract_fields = array(
		'pos'           => array('pos_name', 'name'),
		'payment_terms' => array('terms', 'name'),
	);

	// Boolean display fields
	$boolean_fields = array('tax_included', 'prepaid', 'fixed_asset', 'cash_sale', 'credit_sale');

	// --- Render key-value fields ---
	echo "<table class='tablestyle' style='width:100%;'>\n";
	echo "<tr><th style='width:200px;'>" . _('Field') . "</th><th>" . _('Value') . "</th></tr>\n";

	$row_index = 0;
	$line_items_data = null;

	foreach ($draft_data as $key => $value) {
		// Skip numeric keys (duplicate DB column indices)
		if (is_int($key))
			continue;

		// Separate line_items for rendering as a table below
		if ($key === 'line_items') {
			$line_items_data = $value;
			continue;
		}

		// Skip internal/technical fields
		if (in_array($key, $skip_fields))
			continue;

		// Handle complex object fields — extract readable name
		if (isset($name_extract_fields[$key]) && is_array($value)) {
			$found_name = '';
			foreach ($name_extract_fields[$key] as $name_key) {
				if (isset($value[$name_key]) && $value[$name_key] !== '') {
					$found_name = $value[$name_key];
					break;
				}
			}
			$display_value = $found_name ? htmlspecialchars($found_name, ENT_QUOTES, 'UTF-8') : '-';
		}
		// Handle any other arrays — show a summary or skip
		elseif (is_array($value)) {
			continue;
		}
		// Handle booleans
		elseif (in_array($key, $boolean_fields)) {
			$display_value = $value ? _('Yes') : _('No');
		}
		// Handle empty values
		elseif ($value === '' || $value === null) {
			$display_value = '-';
		}
		// Normal scalar value
		else {
			$display_value = htmlspecialchars((string)$value, ENT_QUOTES, 'UTF-8');
		}

		// Get friendly label
		$display_key = isset($field_labels[$key])
			? $field_labels[$key]
			: ucwords(str_replace('_', ' ', $key));

		$row_class = ($row_index % 2 == 0) ? 'oddrow' : 'evenrow';
		echo "<tr class='$row_class'>";
		echo "<td style='font-weight:bold;'>" . htmlspecialchars($display_key, ENT_QUOTES, 'UTF-8') . "</td>";
		echo "<td>$display_value</td>";
		echo "</tr>\n";
		$row_index++;
	}

	echo "</table>\n";

	// --- Render line items as a proper table ---
	if ($line_items_data && is_array($line_items_data) && count($line_items_data) > 0) {
		echo "<h3 style='margin:16px 0 8px 0;'>"
			. default_theme_icon('list-check')
			. _('Line Items') . "</h3>\n";

		start_table(TABLESTYLE, "width='100%'");
		table_header(array(
			_('#'),
			_('Item Code'),
			_('Description'),
			_('Quantity'),
			_('Unit'),
			_('Unit Price'),
			_('Discount %'),
			_('Total')
		));

		$line_num = 0;
		$grand_total = 0;
		foreach ($line_items_data as $item) {
			if (!is_array($item))
				continue;
			$line_num++;

			$stock_id = isset($item['stock_id']) ? htmlspecialchars($item['stock_id'], ENT_QUOTES, 'UTF-8') : '-';
			$description = isset($item['item_description']) ? htmlspecialchars($item['item_description'], ENT_QUOTES, 'UTF-8')
				: (isset($item['description']) ? htmlspecialchars($item['description'], ENT_QUOTES, 'UTF-8') : '-');
			$qty_dec = isset($item['stock_id']) ? get_qty_dec($item['stock_id']) : get_qty_dec();
			$qty = isset($item['quantity']) ? (float)$item['quantity'] : 0;
			$units = isset($item['units']) ? htmlspecialchars($item['units'], ENT_QUOTES, 'UTF-8') : '';
			$price = isset($item['price']) ? (float)$item['price'] : 0;
			$discount_percent = isset($item['discount_percent']) ? (float)$item['discount_percent'] : 0;
			$line_total = (isset($item['quantity']) && isset($item['price']))
				? $qty * $price * (1 - $discount_percent)
				: 0;
			$grand_total += $line_total;

			start_row();
			label_cell($line_num, "align='center'");
			label_cell($stock_id);
			label_cell($description);
			qty_cell($qty, false, $qty_dec);
			label_cell($units);
			amount_cell($price);
			label_cell(number_format2($discount_percent * 100, 1), "align='right' nowrap");
			amount_cell($line_total);
			end_row();
		}

		start_row("style='font-weight:bold;border-top:2px solid #dee2e6;'");
		label_cell(_('Total') . ':', "colspan='7' align='right'");
		amount_cell($grand_total);
		end_row();

		end_table();
	}
} else {
	display_note(_('Draft data is not available or could not be decoded.'));
}

// --- Section 6: Action Buttons ---
$is_pending = ((int)$draft['status'] === APPROVAL_STATUS_PENDING);

if ($is_pending) {
	echo "<h3 style='margin:16px 0 8px 0;'>"
		. default_theme_icon('scale')
		. _('Actions') . "</h3>\n";

	$approval_service = get_approval_workflow_service();
	$can_approve = $approval_service->canActOnDraft($draft_id);
	$is_submitter = ((int)$draft['submitted_by'] === $current_user_id);

	if ($can_approve) {
		echo "<div style='border:1px solid #dee2e6;border-radius:6px;padding:16px;margin:8px 0;"
			. "background:#f8f9fa;'>\n";

		start_table(TABLESTYLE2);
		textarea_row(_('Approval Comments') . ':', 'approve_comments', '', 50, 3);
		end_table(1);

		submit_center_first('ApproveDraft', _('Approve'), '', 'default');
		echo "&nbsp;&nbsp;";

		start_table(TABLESTYLE2);
		textarea_row(_('Rejection Comments') . ':', 'reject_comments', '', 50, 3);
		end_table(1);

		submit_center_last('RejectDraft', _('Reject'), '', 'cancel');

		echo "</div>\n";
	}

	if ($is_submitter) {
		echo "<div style='border:1px solid #dee2e6;border-radius:6px;padding:16px;margin:8px 0;"
			. "background:#fff3cd;'>\n";
		echo "<p style='margin:0 0 8px 0;'>"
			. default_theme_icon('info-circle')
			. _('As the submitter, you can cancel this draft.') . "</p>\n";

		start_table(TABLESTYLE2);
		textarea_row(_('Cancellation Reason') . ':', 'cancel_comments', '', 50, 3);
		end_table(1);

		submit_center('CancelDraft', _('Cancel Draft'), true, '', 'cancel');

		echo "</div>\n";
	}

	if (!$can_approve && !$is_submitter) {
		display_note(_('You do not have permission to take action on this draft at the current approval level.'));
	}
}

end_form();
end_page();
