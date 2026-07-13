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

/**
 * Ensure HRM core approval transaction types have numbering metadata.
 *
 * @return void
 */
function ensure_hrm_approval_transaction_registration()
{
	static $is_registered = false;

	if ($is_registered || !function_exists('register_systype_db_info'))
		return;

	register_systype_db_info(ST_OVERTIME_REQUEST, array(
		TB_PREF . 'overtime_requests',
		null,
		'request_id',
		'request_id',
		'request_date'
	));

	register_systype_db_info(ST_PAYROLL_PERIOD, array(
		TB_PREF . 'payroll_periods',
		null,
		'period_id',
		'period_name',
		'created_date'
	));

		register_systype_db_info(ST_EMPLOYEE_APPRAISAL, array(
			TB_PREF . 'employee_appraisals',
			null,
			'appraisal_id',
			null,
			'appraisal_date'
		));

	$is_registered = true;
}

ensure_hrm_approval_transaction_registration();

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

ApprovalWorkflow::registerTransactionType(ST_LOAN_REQUEST, array(
	'label'          => _('Loan Request'),
	'application'    => 'HRM',
	'executor'       => 'execute_approved_loan_request',
	'rejector'       => 'reject_loan_request_draft',
	'canceler'       => 'cancel_loan_request_draft',
	'amount_getter'  => 'get_loan_request_draft_amount',
	'draft_renderer' => 'render_loan_request_draft',
	'edit_url'       => 'hrm/transactions/loan_request.php',
));

ApprovalWorkflow::registerTransactionType(ST_EMPLOYEE_APPRAISAL, array(
	'label'          => _('Employee Appraisal'),
	'application'    => 'HRM',
	'executor'       => 'execute_approved_employee_appraisal',
	'rejector'       => 'reject_employee_appraisal_draft',
	'canceler'       => 'cancel_employee_appraisal_draft',
	'amount_getter'  => 'get_employee_appraisal_draft_amount',
	'draft_renderer' => 'render_employee_appraisal_draft',
	'edit_url'       => 'hrm/manage/appraisals.php',
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
	$fiscal_year = get_leave_balance_fiscal_year_for_date($request['from_date']);
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

/**
 * Execute an approved Loan Request draft.
 *
 * @param array $draft_data         Decoded JSON draft data
 * @param int   $reserved_trans_no  Reserved transaction number
 * @param int   $draft_id           Approval draft ID
 * @return int|false Loan ID or false on failure
 */
function execute_approved_loan_request($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;

	include_once($path_to_root . '/hrm/includes/db/loan_db.inc');

	$loan_id = isset($draft_data['loan_id']) ? (int)$draft_data['loan_id'] : 0;
	if ($loan_id <= 0)
		return false;

	$loan = get_employee_loan($loan_id);
	if (!$loan || (int)$loan['status'] !== 0)
		return false;

	$approved_by = isset($_SESSION['wa_current_user']) && !empty($_SESSION['wa_current_user']->loginname)
		? $_SESSION['wa_current_user']->loginname
		: 'approval_system';

	return approve_employee_loan($loan_id, $approved_by) ? $loan_id : false;
}

/**
 * Sync a rejected loan approval draft back to the loan status.
 *
 * @param array  $draft_data Decoded JSON draft data
 * @param int    $draft_id Approval draft ID
 * @param string $comments Rejection comments
 * @return bool
 */
function reject_loan_request_draft($draft_data, $draft_id, $comments)
{
	global $path_to_root;

	include_once($path_to_root . '/hrm/includes/db/loan_db.inc');

	$loan_id = isset($draft_data['loan_id']) ? (int)$draft_data['loan_id'] : 0;
	if ($loan_id <= 0)
		return false;

	return mark_employee_loan_cancelled_by_approval($loan_id);
}

/**
 * Sync a cancelled loan approval draft back to the loan status.
 *
 * @param array  $draft_data Decoded JSON draft data
 * @param int    $draft_id Approval draft ID
 * @param string $comments Cancellation comments
 * @return bool
 */
function cancel_loan_request_draft($draft_data, $draft_id, $comments)
{
	return reject_loan_request_draft($draft_data, $draft_id, $comments);
}

/**
 * Execute an approved Employee Appraisal draft.
 *
 * @param array $draft_data
 * @param int   $reserved_trans_no
 * @param int   $draft_id
 * @return int|false
 */
function execute_approved_employee_appraisal($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;

	include_once($path_to_root . '/hrm/includes/db/appraisal_db.inc');

	$appraisal_id = isset($draft_data['appraisal_id']) ? (int)$draft_data['appraisal_id'] : 0;
	if ($appraisal_id <= 0)
		return false;

	$appraisal = get_employee_appraisal($appraisal_id);
	if (!$appraisal)
		return false;

	if ((int)$appraisal['status'] === 2)
		return $appraisal_id;

	if (!in_array((int)$appraisal['status'], array(0, 1), true))
		return false;

	return update_employee_appraisal_status($appraisal_id, 2) ? $appraisal_id : false;
}

/**
 * Sync rejected appraisal draft back to business status.
 *
 * @param array  $draft_data
 * @param int    $draft_id
 * @param string $comments
 * @return bool
 */
function reject_employee_appraisal_draft($draft_data, $draft_id, $comments)
{
	global $path_to_root;

	include_once($path_to_root . '/hrm/includes/db/appraisal_db.inc');

	$appraisal_id = isset($draft_data['appraisal_id']) ? (int)$draft_data['appraisal_id'] : 0;
	if ($appraisal_id <= 0)
		return false;

	return update_employee_appraisal_status($appraisal_id, 3);
}

/**
 * Sync cancelled appraisal draft back to business status.
 *
 * @param array  $draft_data
 * @param int    $draft_id
 * @param string $comments
 * @return bool
 */
function cancel_employee_appraisal_draft($draft_data, $draft_id, $comments)
{
	return reject_employee_appraisal_draft($draft_data, $draft_id, $comments);
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

/**
 * Get the amount for a loan request draft (loan amount).
 *
 * @param array $draft_data Decoded JSON draft data
 * @return float
 */
function get_loan_request_draft_amount($draft_data)
{
	return isset($draft_data['loan_amount']) ? (float)$draft_data['loan_amount'] : 0;
}

/**
 * Get amount for employee appraisal draft.
 *
 * @param array $draft_data
 * @return float
 */
function get_employee_appraisal_draft_amount($draft_data)
{
	return isset($draft_data['overall_score']) ? (float)$draft_data['overall_score'] : 0;
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
 * Render an employee appraisal draft for approval review.
 *
 * @param array $draft_data
 * @return string
 */
function render_employee_appraisal_draft($draft_data)
{
	$html = '<table class="tablestyle2">';
	$html .= '<tr><td class="label">' . _('Employee') . ':</td><td>'
		. htmlspecialchars(isset($draft_data['employee_name']) ? $draft_data['employee_name'] : '')
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('Reviewer') . ':</td><td>'
		. htmlspecialchars(isset($draft_data['reviewer_name']) ? $draft_data['reviewer_name'] : '')
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('Period') . ':</td><td>'
		. htmlspecialchars(isset($draft_data['period_from']) ? $draft_data['period_from'] : '')
		. ' - '
		. htmlspecialchars(isset($draft_data['period_to']) ? $draft_data['period_to'] : '')
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('Appraisal Date') . ':</td><td>'
		. htmlspecialchars(isset($draft_data['appraisal_date']) ? $draft_data['appraisal_date'] : '')
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('Overall Score') . ':</td><td>'
		. number_format((float)(isset($draft_data['overall_score']) ? $draft_data['overall_score'] : 0), 2)
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('Rating Scale') . ':</td><td>'
		. (int)(isset($draft_data['rating_scale']) ? $draft_data['rating_scale'] : 0)
		. '</td></tr>';

	if (!empty($draft_data['strengths'])) {
		$html .= '<tr><td class="label">' . _('Strengths') . ':</td><td>'
			. nl2br(htmlspecialchars($draft_data['strengths']))
			. '</td></tr>';
	}

	if (!empty($draft_data['improvements'])) {
		$html .= '<tr><td class="label">' . _('Improvements') . ':</td><td>'
			. nl2br(htmlspecialchars($draft_data['improvements']))
			. '</td></tr>';
	}

	if (!empty($draft_data['recommendation'])) {
		$html .= '<tr><td class="label">' . _('Recommendation') . ':</td><td>'
			. nl2br(htmlspecialchars($draft_data['recommendation']))
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

/**
 * Render a loan request draft for the approval viewer.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return string HTML content
 */
function render_loan_request_draft($draft_data)
{
	$html = '<table class="tablestyle2">';
	$html .= '<tr><td class="label">' . _('Employee') . ':</td><td>'
		. htmlspecialchars(isset($draft_data['employee_name']) ? $draft_data['employee_name'] : (isset($draft_data['employee_id']) ? $draft_data['employee_id'] : ''))
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('Loan Type') . ':</td><td>'
		. htmlspecialchars(isset($draft_data['loan_type_name']) ? $draft_data['loan_type_name'] : '')
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('Loan Amount') . ':</td><td>'
		. number_format((float)(isset($draft_data['loan_amount']) ? $draft_data['loan_amount'] : 0), 2)
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('Interest Rate') . ':</td><td>'
		. number_format((float)(isset($draft_data['interest_rate']) ? $draft_data['interest_rate'] : 0), 2) . '%'
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('Installments') . ':</td><td>'
		. (int)(isset($draft_data['installments']) ? $draft_data['installments'] : 0)
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('Loan Date') . ':</td><td>'
		. htmlspecialchars(isset($draft_data['loan_date']) ? $draft_data['loan_date'] : '')
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('First Repayment') . ':</td><td>'
		. htmlspecialchars(isset($draft_data['first_repayment']) ? $draft_data['first_repayment'] : '')
		. '</td></tr>';

	if (!empty($draft_data['notes'])) {
		$html .= '<tr><td class="label">' . _('Notes') . ':</td><td>'
			. nl2br(htmlspecialchars($draft_data['notes']))
			. '</td></tr>';
	}
	$html .= '</table>';

	return $html;
}
