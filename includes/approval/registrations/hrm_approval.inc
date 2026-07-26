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
		ORDER BY d.id DESC";

	$result = db_query($sql, 'could not find approval draft for HRM request');
	while ($row = db_fetch($result)) {
		$draft_source = html_entity_decode(
			(string)$row['draft_data'],
			ENT_QUOTES,
			isset($_SESSION['language']->encoding)
				? $_SESSION['language']->encoding
				: 'UTF-8'
		);
		$draft_data = json_decode($draft_source, true);
		if (is_array($draft_data)
			&& isset($draft_data[$key])
			&& (int)$draft_data[$key] === (int)$request_id
		) {
			return $row;
		}
	}

	return false;
}

/**
 * Recover one pending leave request that has no approval draft.
 *
 * The existing leave row is locked before exact draft lookup and submission,
 * so retries return the same pending draft and concurrent recovery cannot
 * create a duplicate. The recovery actor is recorded in approval history.
 *
 * @param int $request_id Leave request ID
 * @return array Recovery result with status and message
 */
function reconcile_pending_leave_request_approval($request_id)
{
	if (!user_check_access('SA_LEAVEAPPROVE')) {
		return array(
			'status' => 'error',
			'message' => _('You do not have permission to recover leave approvals.'),
		);
	}

	$request_id = (int)$request_id;
	if ($request_id <= 0) {
		return array(
			'status' => 'error',
			'message' => _('Leave request was not found.'),
		);
	}

	global $path_to_root;
	include_once($path_to_root . '/hrm/includes/db/leave_request_db.inc');

	begin_transaction();
	$request = get_leave_request_for_update($request_id);
	if (!$request || (int)$request['status'] !== 0) {
		cancel_transaction();
		return array(
			'status' => 'error',
			'message' => _('Only pending leave requests can be recovered.'),
		);
	}

	$existing = find_approval_draft_for_hrm_request(
		ST_LEAVE_REQUEST,
		$request_id
	);
	if ($existing) {
		commit_transaction();
		return array(
			'status' => 'already_pending',
			'draft_id' => (int)$existing['draft_id'],
			'message' => _('The leave request already has a pending approval draft.'),
		);
	}

	if (trim((string)$request['employee_id']) === ''
		|| (int)$request['leave_id'] <= 0
		|| (float)$request['days'] <= 0
		|| ((int)$request['half_day'] > 0
			&& $request['from_date'] !== $request['to_date'])
	) {
		cancel_transaction();
		return array(
			'status' => 'error',
			'message' => _('The pending leave request must be corrected before recovery.'),
		);
	}

	$display_request = get_leave_request($request_id);
	if (!$display_request) {
		cancel_transaction();
		return array(
			'status' => 'error',
			'message' => _('Leave request was not found.'),
		);
	}

	$draft_data = array(
		'request_id' => $request_id,
		'employee_id' => $request['employee_id'],
		'leave_id' => (int)$request['leave_id'],
		'from_date' => sql2date($request['from_date']),
		'to_date' => sql2date($request['to_date']),
		'days' => (float)$request['days'],
		'half_day' => (int)$request['half_day'],
		'reason' => $request['reason'],
		'employee_name' => $display_request['employee_name'],
		'leave_name' => $display_request['leave_name'],
	);
	$approval = get_approval_workflow_service();
	$result = $approval->submit(
		ST_LEAVE_REQUEST,
		$draft_data,
		(float)$request['days'],
		array(
			'summary' => sprintf(
				_('Recovered leave request #%d: %s days'),
				$request_id,
				(float)$request['days']
			),
		)
	);

	if (!isset($result['status'])
		|| !in_array($result['status'], array('pending', 'auto_approved'), true)
		|| empty($result['draft_id'])
	) {
		cancel_transaction();
		return array(
			'status' => 'error',
			'message' => _('The leave approval could not be recovered. No changes were committed.'),
		);
	}

	$action_id = add_approval_action(
		(int)$result['draft_id'],
		'resubmit',
		null,
		0,
		_('Recovered pending leave request without an approval draft.')
	);
	if (!$action_id) {
		cancel_transaction();
		return array(
			'status' => 'error',
			'message' => _('The leave approval recovery audit could not be recorded. No changes were committed.'),
		);
	}

	commit_transaction();
	$result['message'] = ($result['status'] === 'auto_approved')
		? _('The leave request was recovered and automatically approved.')
		: _('The leave request was recovered and submitted for approval.');
	return $result;
}

// ==============================================
// TRANSACTION TYPE REGISTRATIONS
// =====================================================

ApprovalWorkflow::registerTransactionType(ST_LEAVE_REQUEST, array(
	'label'          => _('Leave Request'),
	'application'    => 'HRM',
	'executor'       => 'execute_approved_leave_request',
	'rejector'       => 'reject_leave_request_draft',
	'canceler'       => 'cancel_leave_request_draft',
	'amount_getter'  => 'get_leave_request_draft_amount',
	'draft_renderer' => 'render_leave_request_draft',
	'edit_url'       => 'hrm/transactions/leave_request.php',
));

ApprovalWorkflow::registerTransactionType(ST_OVERTIME_REQUEST, array(
	'label'          => _('Overtime Request'),
	'application'    => 'HRM',
	'executor'       => 'execute_approved_overtime_request',
	'rejector'       => 'reject_overtime_request_draft',
	'canceler'       => 'cancel_overtime_request_draft',
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
	include_once($path_to_root . '/includes/db/domain_audit_db.inc');

	$request_id = isset($draft_data['request_id']) ? (int)$draft_data['request_id'] : 0;
	if ($request_id <= 0)
		return false;

	begin_transaction();

	$request = get_leave_request_for_update($request_id);
	if (!$request
		|| (int)$request['status'] !== 0
		|| trim((string)$request['employee_id']) === ''
		|| (int)$request['leave_id'] <= 0
		|| (float)$request['days'] <= 0
	) {
		cancel_transaction();
		return false;
	}

	// Get the final approver remarks from approval actions
	$remarks = isset($draft_data['approval_remarks']) ? $draft_data['approval_remarks'] : '';

	$fiscal_year = get_leave_balance_fiscal_year_for_date($request['from_date']);
	$balance_before = get_leave_balance_for_update(
		$request['employee_id'],
		(int)$request['leave_id'],
		$fiscal_year
	);
	$taken_before = $balance_before ? (float)$balance_before['taken'] : 0.0;

	if (!approve_leave_request($request_id, 'approval_system', $remarks)
		|| !apply_leave_balance_movement(
			$request['employee_id'],
			(int)$request['leave_id'],
			$fiscal_year,
			(float)$request['days'],
			0,
			0
		)
	) {
		cancel_transaction();
		return false;
	}

	$request_after = get_leave_request($request_id);
	$balance_after = get_leave_balance(
		$request['employee_id'],
		(int)$request['leave_id'],
		$fiscal_year
	);
	if (!$request_after
		|| (int)$request_after['status'] !== 1
		|| !$balance_after
		|| abs(
			(float)$balance_after['taken']
				- ($taken_before + (float)$request['days'])
		) > 0.000001
	) {
		cancel_transaction();
		return false;
	}

	// Record the verified terminal state before the same outer transaction commits.
	$audit_result = append_domain_audit_event(array(
		'legal_entity_id' => 0,
		'entity_type'     => 'leave_request',
		'entity_key'      => array('request_id' => $request_id),
		'action'          => 'leave_request.approved',
		'reason_code'     => (int)$draft_id > 0
			? 'approval_workflow_terminal'
			: 'approval_direct_policy_terminal',
		'legacy_source'   => (int)$draft_id > 0 ? 'approval_drafts' : null,
		'legacy_id'       => (int)$draft_id > 0 ? (int)$draft_id : null,
		'before'          => $request,
		'after'           => $request_after,
		'restricted_diff' => array(
			'approval_draft_id' => (int)$draft_id,
			'changed_fields' => array(
				'approval_date',
				'approval_remarks',
				'approved_by',
				'leave_balance.taken',
				'status',
			),
			'mutation_kind' => 'approved',
			'restricted_values_stored' => false,
		),
	));

	if ($audit_result === false) {
		error_log(sprintf(
			'PAY-AUD-001: Leave request %d approval audit append failed: %s',
			$request_id,
			isset($GLOBALS['domain_audit_append_result'])
				? $GLOBALS['domain_audit_append_result']
				: 'unknown'
		));
		cancel_transaction();
		return false;
	}

	commit_transaction();
	return $request_id;
}

/**
 * Apply one guarded leave rejection or cancellation with PAY-AUD evidence.
 *
 * @param array  $draft_data Decoded approval draft data
 * @param int    $draft_id Approval draft ID
 * @param string $comments Terminal-action comments
 * @param int    $target_status Leave request status
 * @param string $action Domain-audit action
 * @param string $reason_code Domain-audit reason code
 * @param string $mutation_kind Restricted mutation classification
 * @return bool
 */
function transition_leave_request_approval_terminal(
	$draft_data,
	$draft_id,
	$comments,
	$target_status,
	$action,
	$reason_code,
	$mutation_kind
) {
	global $path_to_root;

	include_once($path_to_root . '/hrm/includes/db/leave_request_db.inc');
	include_once($path_to_root . '/includes/db/domain_audit_db.inc');

	$request_id = isset($draft_data['request_id']) ? (int)$draft_data['request_id'] : 0;
	if ($request_id <= 0
		|| (int)$draft_id <= 0
		|| !in_array((int)$target_status, array(2, 3), true)
	) {
		return false;
	}

	begin_transaction();

	$request = get_leave_request_for_update($request_id);
	if (!$request || (int)$request['status'] !== 0) {
		cancel_transaction();
		return false;
	}

	$approved_by = isset($_SESSION['wa_current_user'])
		&& !empty($_SESSION['wa_current_user']->loginname)
			? $_SESSION['wa_current_user']->loginname
			: 'approval_system';
	$transitioned = ((int)$target_status === 2)
		? reject_leave_request($request_id, $approved_by, $comments)
		: cancel_leave_request($request_id, $approved_by, $comments);
	if (!$transitioned) {
		cancel_transaction();
		return false;
	}

	$request_after = get_leave_request_for_update($request_id);
	if (!$request_after || (int)$request_after['status'] !== (int)$target_status) {
		cancel_transaction();
		return false;
	}

	$audit_result = append_domain_audit_event(array(
		'legal_entity_id' => 0,
		'entity_type'     => 'leave_request',
		'entity_key'      => array('request_id' => $request_id),
		'action'          => $action,
		'reason_code'     => $reason_code,
		'legacy_source'   => 'approval_drafts',
		'legacy_id'       => (int)$draft_id,
		'before'          => $request,
		'after'           => $request_after,
		'restricted_diff' => array(
			'approval_draft_id' => (int)$draft_id,
			'changed_fields' => array(
				'approval_date',
				'approval_remarks',
				'approved_by',
				'leave_details.approval_status',
				'status',
			),
			'mutation_kind' => $mutation_kind,
			'restricted_values_stored' => false,
		),
	));
	if ($audit_result === false) {
		error_log(sprintf(
			'PAY-AUD-001: Leave request %d %s audit append failed: %s',
			$request_id,
			$mutation_kind,
			isset($GLOBALS['domain_audit_append_result'])
				? $GLOBALS['domain_audit_append_result']
				: 'unknown'
		));
		cancel_transaction();
		return false;
	}

	commit_transaction();
	return true;
}

/**
 * Reject one pending leave request through its approval draft.
 *
 * @param array  $draft_data Decoded approval draft data
 * @param int    $draft_id Approval draft ID
 * @param string $comments Rejection comments
 * @return bool
 */
function reject_leave_request_draft($draft_data, $draft_id, $comments)
{
	return transition_leave_request_approval_terminal(
		$draft_data,
		$draft_id,
		$comments,
		2,
		'leave_request.rejected',
		'approval_workflow_rejection_terminal',
		'rejected'
	);
}

/**
 * Cancel one pending leave request through its approval draft.
 *
 * @param array  $draft_data Decoded approval draft data
 * @param int    $draft_id Approval draft ID
 * @param string $comments Cancellation comments
 * @return bool
 */
function cancel_leave_request_draft($draft_data, $draft_id, $comments)
{
	return transition_leave_request_approval_terminal(
		$draft_data,
		$draft_id,
		$comments,
		3,
		'leave_request.cancelled',
		'approval_submitter_cancellation_terminal',
		'cancelled'
	);
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
	include_once($path_to_root . '/includes/db/domain_audit_db.inc');

	$request_id = isset($draft_data['request_id']) ? (int)$draft_data['request_id'] : 0;
	if ($request_id <= 0)
		return false;

	begin_transaction();

	$request = get_overtime_request_for_update($request_id);
	$draft_date = isset($draft_data['date'])
		? date2sql($draft_data['date'])
		: '';
	if (!$request
		|| (int)$request['status'] !== 0
		|| trim((string)$request['employee_id']) === ''
		|| (int)$request['overtime_id'] <= 0
		|| (float)$request['hours'] <= 0
		|| (float)$request['hours'] > 24
		|| !isset($draft_data['employee_id'])
		|| (string)$draft_data['employee_id'] !== (string)$request['employee_id']
		|| !isset($draft_data['overtime_id'])
		|| (int)$draft_data['overtime_id'] !== (int)$request['overtime_id']
		|| $draft_date !== (string)$request['date']
		|| !isset($draft_data['hours'])
		|| abs((float)$draft_data['hours'] - (float)$request['hours']) > 0.000001
		|| !isset($draft_data['reason'])
		|| (string)$draft_data['reason'] !== (string)$request['reason']
	) {
		cancel_transaction();
		return false;
	}

	if (!approve_overtime_request($request_id, 'approval_system')) {
		cancel_transaction();
		return false;
	}

	$request_after = get_overtime_request_for_update($request_id);
	$attendance_after = get_attendance_entry(
		$request['employee_id'],
		sql2date($request['date'])
	);
	if (!$request_after
		|| (int)$request_after['status'] !== 1
		|| !$attendance_after
		|| (int)$attendance_after['overtime_type_id']
			!== (int)$request['overtime_id']
		|| abs(
			(float)$attendance_after['overtime_hours']
				- (float)$request['hours']
		) > 0.000001
	) {
		cancel_transaction();
		return false;
	}

	$audit_result = append_domain_audit_event(array(
		'legal_entity_id' => 0,
		'entity_type'     => 'overtime_request',
		'entity_key'      => array('request_id' => $request_id),
		'action'          => 'overtime_request.approved',
		'reason_code'     => (int)$draft_id > 0
			? 'approval_workflow_terminal'
			: 'approval_direct_policy_terminal',
		'legacy_source'   => (int)$draft_id > 0 ? 'approval_drafts' : null,
		'legacy_id'       => (int)$draft_id > 0 ? (int)$draft_id : null,
		'before'          => $request,
		'after'           => $request_after,
		'restricted_diff' => array(
			'approval_draft_id' => (int)$draft_id,
			'changed_fields' => array(
				'approval_date',
				'approved_by',
				'attendance.overtime_hours',
				'attendance.overtime_type_id',
				'status',
			),
			'mutation_kind' => 'approved',
			'restricted_values_stored' => false,
		),
	));
	if ($audit_result === false) {
		error_log(sprintf(
			'PAY-AUD-001: Overtime request %d approval audit append failed: %s',
			$request_id,
			isset($GLOBALS['domain_audit_append_result'])
				? $GLOBALS['domain_audit_append_result']
				: 'unknown'
		));
		cancel_transaction();
		return false;
	}

	commit_transaction();
	return $request_id;
}

/**
 * Apply one guarded overtime rejection or cancellation with PAY-AUD evidence.
 *
 * @param array  $draft_data Decoded approval draft data
 * @param int    $draft_id Approval draft ID
 * @param string $comments Terminal-action comments
 * @param int    $target_status Overtime request status
 * @param string $action Domain-audit action
 * @param string $reason_code Domain-audit reason code
 * @param string $mutation_kind Restricted mutation classification
 * @return bool
 */
function transition_overtime_request_approval_terminal(
	$draft_data,
	$draft_id,
	$comments,
	$target_status,
	$action,
	$reason_code,
	$mutation_kind
) {
	global $path_to_root;

	include_once($path_to_root . '/hrm/includes/db/overtime_request_db.inc');
	include_once($path_to_root . '/includes/db/domain_audit_db.inc');

	$request_id = isset($draft_data['request_id'])
		? (int)$draft_data['request_id']
		: 0;
	if ($request_id <= 0
		|| (int)$draft_id <= 0
		|| !in_array((int)$target_status, array(2, 3), true)
	) {
		return false;
	}

	begin_transaction();

	$request = get_overtime_request_for_update($request_id);
	$draft_date = isset($draft_data['date'])
		? date2sql($draft_data['date'])
		: '';
	if (!$request
		|| (int)$request['status'] !== 0
		|| trim((string)$request['employee_id']) === ''
		|| (int)$request['overtime_id'] <= 0
		|| trim((string)$request['date']) === ''
		|| (float)$request['hours'] <= 0
		|| (float)$request['hours'] > 24
		|| !isset($draft_data['employee_id'])
		|| (string)$draft_data['employee_id']
			!== (string)$request['employee_id']
		|| !isset($draft_data['overtime_id'])
		|| (int)$draft_data['overtime_id']
			!== (int)$request['overtime_id']
		|| $draft_date !== (string)$request['date']
		|| !isset($draft_data['hours'])
		|| abs(
			(float)$draft_data['hours'] - (float)$request['hours']
		) > 0.000001
		|| !isset($draft_data['reason'])
		|| (string)$draft_data['reason'] !== (string)$request['reason']
	) {
		cancel_transaction();
		return false;
	}

	$terminal_actor = isset($_SESSION['wa_current_user'])
		&& !empty($_SESSION['wa_current_user']->loginname)
			? $_SESSION['wa_current_user']->loginname
			: 'approval_system';
	$transitioned = ((int)$target_status === 2)
		? reject_overtime_request(
			$request_id,
			$terminal_actor,
			0
		)
		: cancel_overtime_request(
			$request_id,
			$terminal_actor,
			0
		);
	if (!$transitioned) {
		cancel_transaction();
		return false;
	}

	$request_after = get_overtime_request_for_update($request_id);
	if (!$request_after
		|| (int)$request_after['status'] !== (int)$target_status
	) {
		cancel_transaction();
		return false;
	}

	$audit_result = append_domain_audit_event(array(
		'legal_entity_id' => 0,
		'entity_type'     => 'overtime_request',
		'entity_key'      => array('request_id' => $request_id),
		'action'          => $action,
		'reason_code'     => $reason_code,
		'legacy_source'   => 'approval_drafts',
		'legacy_id'       => (int)$draft_id,
		'before'          => $request,
		'after'           => $request_after,
		'restricted_diff' => array(
			'approval_draft_id' => (int)$draft_id,
			'changed_fields' => array(
				'approval_date',
				'approved_by',
				'status',
			),
			'mutation_kind' => $mutation_kind,
			'restricted_values_stored' => false,
		),
	));
	if ($audit_result === false) {
		error_log(sprintf(
			'PAY-AUD-001: Overtime request %d %s audit append failed: %s',
			$request_id,
			$mutation_kind,
			isset($GLOBALS['domain_audit_append_result'])
				? $GLOBALS['domain_audit_append_result']
				: 'unknown'
		));
		cancel_transaction();
		return false;
	}

	commit_transaction();
	return true;
}

/**
 * Reject one pending overtime request through its approval draft.
 *
 * @param array  $draft_data Decoded approval draft data
 * @param int    $draft_id Approval draft ID
 * @param string $comments Rejection comments
 * @return bool
 */
function reject_overtime_request_draft($draft_data, $draft_id, $comments)
{
	return transition_overtime_request_approval_terminal(
		$draft_data,
		$draft_id,
		$comments,
		2,
		'overtime_request.rejected',
		'approval_workflow_rejection_terminal',
		'rejected'
	);
}

/**
 * Cancel one pending overtime request through its approval draft.
 *
 * @param array  $draft_data Decoded approval draft data
 * @param int    $draft_id Approval draft ID
 * @param string $comments Cancellation comments
 * @return bool
 */
function cancel_overtime_request_draft($draft_data, $draft_id, $comments)
{
	return transition_overtime_request_approval_terminal(
		$draft_data,
		$draft_id,
		$comments,
		3,
		'overtime_request.cancelled',
		'approval_submitter_cancellation_terminal',
		'cancelled'
	);
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
