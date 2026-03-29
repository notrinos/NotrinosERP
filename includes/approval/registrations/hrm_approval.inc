<?php
/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	HRM Module Registration & Executors

	Registers Leave Request, Overtime Request, and Payroll Period
	transaction types for the approval workflow. Provides executor
	callbacks that process approved HRM drafts.

	PHP 5.6+ compatible.
***********************************************************************/

include_once($path_to_root . '/includes/approval/ui/approval_ui.inc');

// =====================================================
// HELPER: Find approval draft linked to an HRM request
// =====================================================

/**
 * Find the active approval draft for an HRM request.
 *
 * HRM requests (leave, overtime) store the request_id inside the
 * draft_data JSON. This function searches for a pending approval
 * draft that references the given request_id for a specific type.
 *
 * @param int $trans_type  Transaction type (ST_LEAVE_REQUEST, ST_OVERTIME_REQUEST, ST_PAYROLL_PERIOD)
 * @param int $request_id  The HRM request/period ID
 * @return array|false     Draft row or false if not found
 */
function find_approval_draft_for_hrm_request($trans_type, $request_id)
{
	$key = ($trans_type == ST_PAYROLL_PERIOD) ? 'period_id' : 'request_id';

	$sql = "SELECT d.*, d.id AS draft_id FROM " . TB_PREF . "approval_drafts d
		WHERE d.trans_type = " . db_escape((int)$trans_type) . "
			AND d.status = " . APPROVAL_STATUS_PENDING . "
			AND d.draft_data LIKE " . db_escape('%"' . $key . '":' . (int)$request_id . '%') . "
		ORDER BY d.id DESC LIMIT 1";

	$result = db_query($sql, 'could not find approval draft for HRM request');
	$row = db_fetch($result);

	return $row ? $row : false;
}

// ==============================================
// TRANSACTION TYPE REGISTRATIONS
// =====================================================

ApprovalWorkflow::registerTransactionType(ST_LEAVE_REQUEST, array(
	'label'          => _('Leave Request'),
	'application'    => 'HRM',
	'executor'       => 'execute_approved_leave_request',
	'amount_getter'  => 'get_leave_request_draft_amount',
	'draft_renderer' => 'render_leave_request_draft',
	'edit_url'       => 'hrm/transactions/leave_request.php',
));

ApprovalWorkflow::registerTransactionType(ST_OVERTIME_REQUEST, array(
	'label'          => _('Overtime Request'),
	'application'    => 'HRM',
	'executor'       => 'execute_approved_overtime_request',
	'amount_getter'  => 'get_overtime_request_draft_amount',
	'draft_renderer' => 'render_overtime_request_draft',
	'edit_url'       => 'hrm/transactions/overtime_request.php',
));

ApprovalWorkflow::registerTransactionType(ST_PAYROLL_PERIOD, array(
	'label'          => _('Payroll Period'),
	'application'    => 'HRM',
	'executor'       => 'execute_approved_payroll_period',
	'amount_getter'  => 'get_payroll_period_draft_amount',
	'draft_renderer' => 'render_payroll_period_draft',
	'edit_url'       => 'hrm/transactions/payroll_approval.php',
));

// =====================================================
// EXECUTOR CALLBACKS
// =====================================================

/**
 * Execute an approved Leave Request draft.
 *
 * Approves the leave_request record, updates leave balance,
 * and syncs to leave_details table.
 *
 * @param array $draft_data         Decoded JSON draft data
 * @param int   $reserved_trans_no  Reserved transaction number
 * @param int   $draft_id           Approval draft ID
 * @return int|false Request ID or false on failure
 */
function execute_approved_leave_request($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;

	include_once($path_to_root . '/hrm/includes/db/leave_request_db.inc');
	include_once($path_to_root . '/hrm/includes/db/leave_balance_db.inc');

	$request_id = isset($draft_data['request_id']) ? (int)$draft_data['request_id'] : 0;
	if ($request_id <= 0)
		return false;

	$request = get_leave_request($request_id);
	if (!$request || (int)$request['status'] !== 0)
		return false;

	// Get the final approver remarks from approval actions
	$remarks = isset($draft_data['approval_remarks']) ? $draft_data['approval_remarks'] : '';

	// Approve the leave request
	approve_leave_request($request_id, 'approval_system', $remarks);

	// Apply leave balance movement
	$fiscal_year = (int)date('Y', strtotime($request['from_date']));
	apply_leave_balance_movement(
		$request['employee_id'],
		(int)$request['leave_id'],
		$fiscal_year,
		(float)$request['days'],
		0,
		0
	);

	return $request_id;
}

/**
 * Execute an approved Overtime Request draft.
 *
 * Approves the overtime_request record and writes attendance record.
 *
 * @param array $draft_data         Decoded JSON draft data
 * @param int   $reserved_trans_no  Reserved transaction number
 * @param int   $draft_id           Approval draft ID
 * @return int|false Request ID or false on failure
 */
function execute_approved_overtime_request($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;

	include_once($path_to_root . '/hrm/includes/db/overtime_request_db.inc');
	include_once($path_to_root . '/hrm/includes/db/attendance_db.inc');

	$request_id = isset($draft_data['request_id']) ? (int)$draft_data['request_id'] : 0;
	if ($request_id <= 0)
		return false;

	$request = get_overtime_request($request_id);
	if (!$request || (int)$request['status'] !== 0)
		return false;

	// Approve the overtime request (writes attendance record internally)
	approve_overtime_request($request_id, 'approval_system');

	return $request_id;
}

/**
 * Execute an approved Payroll Period draft.
 *
 * Updates the payroll period status to Approved (2) and
 * recalculates period totals.
 *
 * @param array $draft_data         Decoded JSON draft data
 * @param int   $reserved_trans_no  Reserved transaction number
 * @param int   $draft_id           Approval draft ID
 * @return int|false Period ID or false on failure
 */
function execute_approved_payroll_period($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;

	include_once($path_to_root . '/hrm/includes/db/payroll_db.inc');

	$period_id = isset($draft_data['period_id']) ? (int)$draft_data['period_id'] : 0;
	if ($period_id <= 0)
		return false;

	$period = get_payroll_period($period_id);
	if (!$period)
		return false;

	// Only approve if still in Draft (0) or Calculated (1) status
	$current_status = (int)$period['status'];
	if ($current_status !== 0 && $current_status !== 1)
		return false;

	// Approve the payroll period
	update_payroll_period_status($period_id, 2);
	update_payroll_period_totals($period_id);

	return $period_id;
}

// =====================================================
// AMOUNT GETTER CALLBACKS
// =====================================================

/**
 * Get the amount for a leave request draft (days as amount).
 *
 * Leave requests don't have a monetary value. Use the number
 * of leave days as the amount for threshold comparisons.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return float Number of leave days
 */
function get_leave_request_draft_amount($draft_data)
{
	return isset($draft_data['days']) ? (float)$draft_data['days'] : 0;
}

/**
 * Get the amount for an overtime request draft (hours as amount).
 *
 * Overtime requests don't have a monetary value. Use the number
 * of overtime hours as the amount for threshold comparisons.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return float Number of overtime hours
 */
function get_overtime_request_draft_amount($draft_data)
{
	return isset($draft_data['hours']) ? (float)$draft_data['hours'] : 0;
}

/**
 * Get the amount for a payroll period draft (total net salary).
 *
 * @param array $draft_data Decoded JSON draft data
 * @return float Total net salary amount
 */
function get_payroll_period_draft_amount($draft_data)
{
	return isset($draft_data['total_net']) ? (float)$draft_data['total_net'] : 0;
}

// =====================================================
// DRAFT RENDERER CALLBACKS
// =====================================================

/**
 * Render a leave request draft for the approval viewer.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return string HTML content
 */
function render_leave_request_draft($draft_data)
{
	$html = '<table class="tablestyle2">';
	$html .= '<tr><td class="label">' . _('Employee') . ':</td><td>'
		. htmlspecialchars(isset($draft_data['employee_name']) ? $draft_data['employee_name'] : $draft_data['employee_id'])
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('Leave Type') . ':</td><td>'
		. htmlspecialchars(isset($draft_data['leave_name']) ? $draft_data['leave_name'] : '')
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('From Date') . ':</td><td>'
		. htmlspecialchars($draft_data['from_date'])
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('To Date') . ':</td><td>'
		. htmlspecialchars($draft_data['to_date'])
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('Days') . ':</td><td>'
		. number_format((float)$draft_data['days'], 1)
		. '</td></tr>';

	$half_day_labels = array(0 => _('Full Day(s)'), 1 => _('First Half'), 2 => _('Second Half'));
	$half_day = isset($draft_data['half_day']) ? (int)$draft_data['half_day'] : 0;
	$html .= '<tr><td class="label">' . _('Half Day') . ':</td><td>'
		. (isset($half_day_labels[$half_day]) ? $half_day_labels[$half_day] : _('Full Day(s)'))
		. '</td></tr>';

	if (!empty($draft_data['reason'])) {
		$html .= '<tr><td class="label">' . _('Reason') . ':</td><td>'
			. nl2br(htmlspecialchars($draft_data['reason']))
			. '</td></tr>';
	}
	$html .= '</table>';

	return $html;
}

/**
 * Render an overtime request draft for the approval viewer.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return string HTML content
 */
function render_overtime_request_draft($draft_data)
{
	$html = '<table class="tablestyle2">';
	$html .= '<tr><td class="label">' . _('Employee') . ':</td><td>'
		. htmlspecialchars(isset($draft_data['employee_name']) ? $draft_data['employee_name'] : $draft_data['employee_id'])
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('Overtime Type') . ':</td><td>'
		. htmlspecialchars(isset($draft_data['overtime_name']) ? $draft_data['overtime_name'] : '')
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('Date') . ':</td><td>'
		. htmlspecialchars($draft_data['date'])
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('Hours') . ':</td><td>'
		. number_format((float)$draft_data['hours'], 2)
		. '</td></tr>';

	if (!empty($draft_data['reason'])) {
		$html .= '<tr><td class="label">' . _('Reason') . ':</td><td>'
			. nl2br(htmlspecialchars($draft_data['reason']))
			. '</td></tr>';
	}
	$html .= '</table>';

	return $html;
}

/**
 * Render a payroll period draft for the approval viewer.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return string HTML content
 */
function render_payroll_period_draft($draft_data)
{
	$html = '<table class="tablestyle2">';
	$html .= '<tr><td class="label">' . _('Period Name') . ':</td><td>'
		. htmlspecialchars(isset($draft_data['period_name']) ? $draft_data['period_name'] : '')
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('From Date') . ':</td><td>'
		. htmlspecialchars(isset($draft_data['from_date']) ? $draft_data['from_date'] : '')
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('To Date') . ':</td><td>'
		. htmlspecialchars(isset($draft_data['to_date']) ? $draft_data['to_date'] : '')
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('Gross') . ':</td><td>'
		. number_format((float)(isset($draft_data['total_gross']) ? $draft_data['total_gross'] : 0), 2)
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('Deductions') . ':</td><td>'
		. number_format((float)(isset($draft_data['total_deductions']) ? $draft_data['total_deductions'] : 0), 2)
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('Net') . ':</td><td>'
		. number_format((float)(isset($draft_data['total_net']) ? $draft_data['total_net'] : 0), 2)
		. '</td></tr>';

	if (isset($draft_data['department_name']) && $draft_data['department_name'] !== '') {
		$html .= '<tr><td class="label">' . _('Department') . ':</td><td>'
			. htmlspecialchars($draft_data['department_name'])
			. '</td></tr>';
	}
	$html .= '</table>';

	return $html;
}
