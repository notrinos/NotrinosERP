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
	Core ApprovalWorkflow Service Class

	Provides centralized approval management for all transaction types.
	Modules register their transaction types and provide executor callbacks.

	PHP 5.6+ compatible.

	Usage:
		$approval = get_approval_workflow_service();
		if ($approval->isApprovalRequired(ST_SALESINVOICE, $amount)) {
			$result = $approval->submit(ST_SALESINVOICE, $data, $amount, $context);
		}
***********************************************************************/

include_once(dirname(__FILE__) . '/db/approval_db.inc');
include_once(dirname(__FILE__) . '/db/approval_rules_db.inc');
include_once(dirname(__FILE__) . '/db/approval_history_db.inc');
include_once(dirname(__FILE__) . '/approval_number.inc');
include_once(dirname(__FILE__) . '/approval_notify.inc');
include_once(dirname(__FILE__) . '/approval_conditions.inc');

class ApprovalWorkflow {

	/** @var array Registered transaction type configurations (static registry) */
	private static $registered_types = array();

	/** @var int Current user's security role ID */
	private $role_id;

	/** @var int Current user's ID */
	private $user_id;

	/** @var array Cached approval rules by trans_type */
	private $rules_cache = array();

	/** @var array Cached user approval levels by trans_type */
	private $user_levels_cache = array();

	// =====================================================
	// REGISTRATION API (Static)
	// =====================================================

	/**
	 * Register a transaction type for approval workflow.
	 *
	 * Called by each module to register its transaction types.
	 * Must be called before any approval checks are made.
	 *
	 * @param int   $trans_type Transaction type constant (ST_*)
	 * @param array $config     Configuration array with keys:
	 *   'label'          => string   Human-readable name
	 *   'application'    => string   Application code (orders, AP, GL, etc.)
	 *   'executor'       => callable Function to execute approved draft
	 *   'rejector'       => callable Function to sync business state on rejection
	 *   'canceler'       => callable Function to sync business state on cancellation
	 *   'amount_getter'  => callable Function to calculate draft amount from data
	 *   'draft_renderer' => callable Function to render draft for review UI
	 *   'edit_url'       => string   URL for editing the draft
	 *   'number_context' => array    Context for reference number generation (optional)
	 * @return void
	 */
	public static function registerTransactionType($trans_type, $config)
	{
		$defaults = array(
			'label'          => '',
			'application'    => '',
			'executor'       => null,
			'rejector'       => null,
			'canceler'       => null,
			'amount_getter'  => null,
			'draft_renderer' => null,
			'edit_url'       => '',
			'number_context' => array(),
		);

		self::$registered_types[$trans_type] = array_merge($defaults, $config);
	}

	/**
	 * Check if a transaction type is registered for approval.
	 *
	 * @param int $trans_type Transaction type constant
	 * @return bool
	 */
	public static function isRegistered($trans_type)
	{
		return isset(self::$registered_types[$trans_type]);
	}

	/**
	 * Get all registered transaction types and their configurations.
	 *
	 * @return array Keyed by trans_type constant
	 */
	public static function getRegisteredTypes()
	{
		return self::$registered_types;
	}

	/**
	 * Get configuration for a specific registered type.
	 *
	 * @param int $trans_type Transaction type constant
	 * @return array|null     Configuration array or null if not registered
	 */
	public static function getTypeConfig($trans_type)
	{
		return isset(self::$registered_types[$trans_type])
			? self::$registered_types[$trans_type]
			: null;
	}

	// =====================================================
	// CONSTRUCTOR
	// =====================================================

	/**
	 * Constructor — initializes service for the current user.
	 *
	 * @param int $role_id Security role ID of the current user
	 * @param int $user_id User ID (optional, defaults to session user)
	 */
	public function __construct($role_id, $user_id = null)
	{
		$this->role_id = (int)$role_id;
		$this->user_id = $user_id !== null ? (int)$user_id : (int)$_SESSION['wa_current_user']->user;
	}

	// =====================================================
	// WORKFLOW API — Submit / Approve / Reject
	// =====================================================

	/**
	 * Submit a transaction for approval.
	 *
	 * Checks if approval is required, reserves a transaction number,
	 * stores the draft as JSON, and notifies level-1 approvers.
	 *
	 * @param int    $trans_type Transaction type constant (ST_*)
	 * @param mixed  $data       Transaction data (will be JSON-encoded)
	 * @param float  $amount     Transaction amount for threshold checks
	 * @param array  $context    Additional context:
	 *   'person_type' => int    Person type (PT_CUSTOMER, PT_SUPPLIER, etc.)
	 *   'person_id'   => string Person identifier
	 *   'summary'     => string Human-readable summary
	 *   'currency'    => string Currency code
	 *   'loc_code'    => string Location code
	 * @return array ['draft_id' => int, 'trans_no' => int, 'reference' => string, 'status' => string]
	 */
	public function submit($trans_type, $data, $amount = 0, $context = array())
	{
		$workflow = $this->getWorkflowForType($trans_type);

		if (!$workflow)
			return array('draft_id' => 0, 'trans_no' => 0, 'reference' => '', 'status' => 'no_workflow');

		begin_transaction();

		// Reserve transaction number
		$number_context = isset(self::$registered_types[$trans_type]['number_context'])
			? self::$registered_types[$trans_type]['number_context']
			: array();
		$reserved = reserve_approval_trans_number($trans_type, array_merge($number_context, $context));

		// JSON-encode the transaction data
		$draft_data = json_encode($data);

		// Store the draft
		$draft_id = add_approval_draft(
			$workflow['id'],
			$trans_type,
			$reserved['trans_no'],
			$draft_data,
			$amount,
			isset($context['currency']) ? $context['currency'] : null,
			isset($context['person_type']) ? $context['person_type'] : null,
			isset($context['person_id']) ? $context['person_id'] : null,
			$this->user_id,
			(int)$workflow['version'],
			$reserved['reference'],
			isset($context['summary']) ? $context['summary'] : null,
			isset($context['loc_code']) ? $context['loc_code'] : null
		);

		// Record the submit action in audit trail
		add_approval_action($draft_id, 'submit', $this->user_id, 0, '');

		// Check auto-approval: if amount is within threshold of level 1
		$auto_approved = $this->checkAutoApproval($draft_id, $workflow, $amount, $data, $context);

		if ($auto_approved) {
			// Execute immediately
			$exec_result = $this->executeApprovedDraft($draft_id);
			commit_transaction();

			// Reload draft to get actual posted trans_no (executor may use different number)
			$posted_draft = get_approval_draft($draft_id);
			$actual_trans_no = ($posted_draft && (int)$posted_draft['approved_trans_no'] > 0)
				? (int)$posted_draft['approved_trans_no']
				: $reserved['trans_no'];

			return array(
				'draft_id'  => $draft_id,
				'trans_no'  => $actual_trans_no,
				'reference' => $reserved['reference'],
				'status'    => 'auto_approved',
			);
		}

		// Notify level-1 approvers
		notify_approval_level_approvers(
			$draft_id,
			$workflow['id'],
			1,
			'pending',
			isset($context['loc_code']) ? $context['loc_code'] : null
		);

		commit_transaction();

		return array(
			'draft_id'  => $draft_id,
			'trans_no'  => $reserved['trans_no'],
			'reference' => $reserved['reference'],
			'status'    => 'pending',
		);
	}

	/**
	 * Approve a draft at the current user's level.
	 *
	 * Records the approval action. If all required approvals at the current
	 * level have been collected, advances to the next level or executes
	 * the draft if it was the final level.
	 *
	 * @param int    $draft_id Draft ID
	 * @param string $comments Approval comments
	 * @param bool   $edited   Whether the draft data was edited before approval
	 * @return array ['status' => string, 'trans_no' => int|null, 'message' => string]
	 */
	public function approve($draft_id, $comments = '', $edited = false)
	{
		$draft = get_approval_draft($draft_id);

		if (!$draft)
			return array('status' => 'error', 'trans_no' => null, 'message' => _('Draft not found.'));

		if ((int)$draft['status'] !== APPROVAL_STATUS_PENDING)
			return array('status' => 'error', 'trans_no' => null, 'message' => _('Draft is not pending approval.'));

		// Verify user can approve at this level
		if (!$this->canActOnDraft($draft_id))
			return array('status' => 'error', 'trans_no' => null, 'message' => _('You do not have permission to approve this draft.'));

		// Check self-approval restriction
		if (!$draft['allow_self_approve'] && (int)$draft['submitted_by'] === $this->user_id)
			return array('status' => 'error', 'trans_no' => null, 'message' => _('You cannot approve your own submission.'));

		// Check if user already approved at this level
		$current_level = (int)$draft['current_level'];
		if (has_user_approved_draft($draft_id, $this->user_id, $current_level))
			return array('status' => 'error', 'trans_no' => null, 'message' => _('You have already approved this draft at this level.'));

		// Require comments if configured
		if ($draft['require_comments_on_approve'] && empty(trim($comments)))
			return array('status' => 'error', 'trans_no' => null, 'message' => _('Comments are required for approval.'));

		begin_transaction();

		// Record the approval action
		add_approval_action($draft_id, 'approve', $this->user_id, $current_level, $comments);

		// Check if minimum approvers threshold is met at this level
		$approvals_at_level = count_approval_actions_at_level($draft_id, $current_level);
		$min_required = get_min_approvers_for_level($draft['workflow_id'], $current_level);
		$max_level = get_max_approval_level($draft['workflow_id']);

		if ($approvals_at_level >= $min_required) {
			// Current level is complete
			if ($current_level >= $max_level) {
				// All levels approved — execute the draft
				update_approval_draft_status($draft_id, APPROVAL_STATUS_APPROVED);
				$exec_result = $this->executeApprovedDraft($draft_id);

				// Notify submitter
				notify_approval_submitter($draft_id, $draft['submitted_by'], 'approved');

				// Mark all notifications as read
				mark_all_draft_notifications_read($draft_id);

				commit_transaction();

				return array(
					'status'   => 'approved',
					'trans_no' => (int)$draft['reserved_trans_no'],
					'message'  => _('Draft has been approved and posted.'),
				);
			} else {
				// Advance to next level
				$next_level = $current_level + 1;
				update_approval_draft_level($draft_id, $next_level);

				// Check auto-approval at next level
				$workflow = get_approval_workflow($draft['workflow_id']);
				$auto_approved = $this->checkAutoApprovalAtLevel(
					$draft_id, $workflow, $draft['amount'], $next_level
				);

				if ($auto_approved) {
					update_approval_draft_status($draft_id, APPROVAL_STATUS_APPROVED);
					$this->executeApprovedDraft($draft_id);
					notify_approval_submitter($draft_id, $draft['submitted_by'], 'approved');
					mark_all_draft_notifications_read($draft_id);

					commit_transaction();

					return array(
						'status'   => 'approved',
						'trans_no' => (int)$draft['reserved_trans_no'],
						'message'  => _('Draft has been approved and posted (auto-approved at next level).'),
					);
				}

				// Notify next level approvers
				notify_approval_level_approvers(
					$draft_id,
					$draft['workflow_id'],
					$next_level,
					'pending',
					$draft['loc_code']
				);

				commit_transaction();

				return array(
					'status'   => 'pending_next_level',
					'trans_no' => null,
					'message'  => sprintf(_('Approved at level %d. Pending approval at level %d.'), $current_level, $next_level),
				);
			}
		}

		// Not enough approvals at this level yet (parallel approval scenario)
		commit_transaction();

		return array(
			'status'   => 'pending_more_approvers',
			'trans_no' => null,
			'message'  => sprintf(
				_('Approved. Waiting for %d more approval(s) at level %d.'),
				$min_required - $approvals_at_level,
				$current_level
			),
		);
	}

	/**
	 * Reject a draft.
	 *
	 * Records the rejection, updates draft status, voids the reserved
	 * transaction number, and notifies the submitter.
	 *
	 * @param int    $draft_id Draft ID
	 * @param string $comments Rejection reason
	 * @return array ['status' => string, 'message' => string]
	 */
	public function reject($draft_id, $comments)
	{
		$draft = get_approval_draft($draft_id);

		if (!$draft)
			return array('status' => 'error', 'message' => _('Draft not found.'));

		if ((int)$draft['status'] !== APPROVAL_STATUS_PENDING)
			return array('status' => 'error', 'message' => _('Draft is not pending approval.'));

		if (!$this->canActOnDraft($draft_id))
			return array('status' => 'error', 'message' => _('You do not have permission to reject this draft.'));

		// Require comments if configured
		if ($draft['require_comments_on_reject'] && empty(trim($comments)))
			return array('status' => 'error', 'message' => _('Comments are required for rejection.'));

		begin_transaction();

		$current_level = (int)$draft['current_level'];

		// Record the rejection action
		add_approval_action($draft_id, 'reject', $this->user_id, $current_level, $comments);

		// Update draft status
		update_approval_draft_status($draft_id, APPROVAL_STATUS_REJECTED);

		// Void the reserved transaction number permanently
		void_reserved_trans_number($draft['trans_type'], $draft['reserved_trans_no'], 'Rejected');

		// Notify submitter
		notify_approval_submitter($draft_id, $draft['submitted_by'], 'rejected');

		// Mark all notifications read
		mark_all_draft_notifications_read($draft_id);

		$this->invokeDraftLifecycleHandler('rejector', $draft, $comments);

		commit_transaction();

		return array('status' => 'rejected', 'message' => _('Draft has been rejected.'));
	}

	/**
	 * Cancel a draft (by the original submitter).
	 *
	 * @param int    $draft_id Draft ID
	 * @param string $comments Cancellation reason
	 * @return array ['status' => string, 'message' => string]
	 */
	public function cancel($draft_id, $comments = '')
	{
		$draft = get_approval_draft($draft_id);

		if (!$draft)
			return array('status' => 'error', 'message' => _('Draft not found.'));

		if ((int)$draft['status'] !== APPROVAL_STATUS_PENDING)
			return array('status' => 'error', 'message' => _('Draft is not pending.'));

		// Only submitter can cancel
		if ((int)$draft['submitted_by'] !== $this->user_id)
			return array('status' => 'error', 'message' => _('Only the submitter can cancel this draft.'));

		begin_transaction();

		add_approval_action($draft_id, 'cancel', $this->user_id, (int)$draft['current_level'], $comments);
		update_approval_draft_status($draft_id, APPROVAL_STATUS_CANCELLED);
		void_reserved_trans_number($draft['trans_type'], $draft['reserved_trans_no'], 'Cancelled');
		mark_all_draft_notifications_read($draft_id);
		$this->invokeDraftLifecycleHandler('canceler', $draft, $comments);

		commit_transaction();

		return array('status' => 'cancelled', 'message' => _('Draft has been cancelled.'));
	}

	/**
	 * Delegate approval to another user.
	 *
	 * @param int    $draft_id   Draft ID
	 * @param int    $to_user_id User to delegate to
	 * @param string $reason     Delegation reason
	 * @return array ['status' => string, 'message' => string]
	 */
	public function delegate($draft_id, $to_user_id, $reason = '')
	{
		$draft = get_approval_draft($draft_id);

		if (!$draft)
			return array('status' => 'error', 'message' => _('Draft not found.'));

		if ((int)$draft['status'] !== APPROVAL_STATUS_PENDING)
			return array('status' => 'error', 'message' => _('Draft is not pending approval.'));

		if (!$this->canActOnDraft($draft_id))
			return array('status' => 'error', 'message' => _('You do not have permission to delegate this draft.'));

		begin_transaction();

		add_approval_action(
			$draft_id, 'delegate', $this->user_id,
			(int)$draft['current_level'],
			sprintf(_('Delegated to user %d. %s'), $to_user_id, $reason)
		);

		// Create notification for delegate
		add_approval_notification($draft_id, $to_user_id, 'delegated');

		commit_transaction();

		return array('status' => 'delegated', 'message' => _('Approval has been delegated.'));
	}

	// =====================================================
	// QUERY API
	// =====================================================

	/**
	 * Check if approval is required for a transaction type and amount.
	 *
	 * Returns false if:
	 * - No workflow is configured for the transaction type
	 * - The workflow is inactive
	 * - No active levels exist
	 * - User has direct post permission
	 *
	 * @param int   $trans_type Transaction type constant
	 * @param float $amount     Transaction amount
	 * @return bool             True if approval is required
	 */
	public function isApprovalRequired($trans_type, $amount = 0)
	{
		// Check if user has direct post bypass
		if ($this->canDirectPost($trans_type))
			return false;

		$workflow = $this->getWorkflowForType($trans_type);

		if (!$workflow || !$workflow['is_active'])
			return false;

		if (!workflow_has_active_levels($workflow['id']))
			return false;

		return true;
	}

	/**
	 * Check if current user can directly post (bypass approval).
	 *
	 * @param int $trans_type Transaction type constant
	 * @return bool
	 */
	public function canDirectPost($trans_type)
	{
		return check_security_area('SA_APPROVALDIRECT');
	}

	/**
	 * Get current user's approval level for a specific workflow/trans type.
	 *
	 * @param int $trans_type Transaction type constant
	 * @return int            Level number (0 = no approval authority)
	 */
	public function getUserLevel($trans_type)
	{
		if (isset($this->user_levels_cache[$trans_type]))
			return $this->user_levels_cache[$trans_type];

		$workflow = $this->getWorkflowForType($trans_type);
		if (!$workflow)
			return 0;

		$level_row = get_user_approval_level_for_workflow($workflow['id'], $this->role_id);

		$level = $level_row ? (int)$level_row['level'] : 0;
		$this->user_levels_cache[$trans_type] = $level;

		return $level;
	}

	/**
	 * Check if current user can approve/reject a specific draft.
	 *
	 * @param int $draft_id Draft ID
	 * @return bool
	 */
	public function canActOnDraft($draft_id)
	{
		$draft = get_approval_draft($draft_id);

		if (!$draft || (int)$draft['status'] !== APPROVAL_STATUS_PENDING)
			return false;

		$current_level = (int)$draft['current_level'];

		// Check if user's role is an approver at the current level
		$approver_roles = get_approver_roles_at_level(
			$draft['workflow_id'],
			$current_level,
			$draft['loc_code']
		);

		if (in_array($this->role_id, $approver_roles))
			return true;

		// Check delegations
		$delegations = get_active_delegations_to_user($this->user_id, $draft['trans_type']);
		while ($delegation = db_fetch($delegations)) {
			$delegator_role = $this->getDelegatorRole($delegation['from_user_id']);
			if (in_array($delegator_role, $approver_roles))
				return true;
		}

		return false;
	}

	/**
	 * Get all pending drafts actionable by current user.
	 *
	 * @param array $filters Optional: trans_type, from_date, to_date
	 * @return resource       DB result set
	 */
	public function getPendingForUser($filters = array())
	{
		return get_pending_approval_drafts_for_user($this->user_id, $this->role_id, $filters);
	}

	/**
	 * Get approval history for a draft.
	 *
	 * @param int  $draft_id Draft ID
	 * @param bool $reverse  If true, oldest first
	 * @return resource       DB result set
	 */
	public function getHistory($draft_id, $reverse = false)
	{
		return get_approval_history($draft_id, $reverse);
	}

	/**
	 * Get a single draft with full details.
	 *
	 * @param int $draft_id Draft ID
	 * @return array|false
	 */
	public function getDraft($draft_id)
	{
		return get_approval_draft($draft_id);
	}

	// =====================================================
	// INTERNAL METHODS
	// =====================================================

	/**
	 * Get the active workflow configuration for a transaction type (cached).
	 *
	 * @param int $trans_type Transaction type constant
	 * @return array|false    Workflow row or false
	 */
	private function getWorkflowForType($trans_type)
	{
		if (isset($this->rules_cache[$trans_type]))
			return $this->rules_cache[$trans_type];

		$workflow = get_approval_workflow_by_type($trans_type);
		$this->rules_cache[$trans_type] = $workflow;

		return $workflow;
	}

	/**
	 * Check if a draft qualifies for auto-approval based on amount thresholds.
	 *
	 * Checks all levels from 1 to max. If the amount is within the
	 * auto-approve threshold of every level, the draft is auto-approved.
	 * Levels with conditions that don't match the draft data are skipped.
	 *
	 * @param int   $draft_id Draft ID
	 * @param array $workflow Workflow row
	 * @param float $amount   Transaction amount
	 * @param array $draft_data_decoded Optional decoded draft data for condition evaluation
	 * @param array $context  Optional context for condition evaluation
	 * @return bool           True if auto-approved
	 */
	private function checkAutoApproval($draft_id, $workflow, $amount, $draft_data_decoded = array(), $context = array())
	{
		$levels = get_approval_levels($workflow['id'], true);
		$all_auto = true;

		$condition_context = array_merge(array('amount' => $amount), $context);

		while ($level = db_fetch($levels)) {
			// Check conditions — skip levels whose conditions don't match
			if (!empty($level['conditions'])) {
				if (!evaluate_approval_conditions($level['conditions'], $draft_data_decoded, $condition_context)) {
					// Conditions don't match — this level is not applicable, skip it
					continue;
				}
			}

			$threshold = (float)$level['amount_threshold'];

			if ($threshold > 0 && $amount <= $threshold) {
				// This level auto-approves
				add_approval_action(
					$draft_id, 'approve', 0, (int)$level['level'],
					sprintf(_('Auto-approved: amount %.2f within threshold %.2f'), $amount, $threshold)
				);
			} else {
				$all_auto = false;
				break;
			}
		}

		if ($all_auto) {
			update_approval_draft_status($draft_id, APPROVAL_STATUS_APPROVED);
		}

		return $all_auto;
	}

	/**
	 * Check auto-approval starting from a specific level.
	 *
	 * Levels with conditions that don't match the draft data are skipped.
	 *
	 * @param int   $draft_id   Draft ID
	 * @param array $workflow   Workflow row
	 * @param float $amount     Transaction amount
	 * @param int   $from_level Starting level
	 * @return bool             True if all remaining levels auto-approve
	 */
	private function checkAutoApprovalAtLevel($draft_id, $workflow, $amount, $from_level)
	{
		$levels = get_approval_levels($workflow['id'], true);
		$all_auto = true;

		// Load draft data for condition evaluation
		$draft = get_approval_draft($draft_id);
		$draft_data_decoded = array();
		if ($draft && !empty($draft['draft_data'])) {
			$draft_data_decoded = json_decode($draft['draft_data'], true);
			if (!is_array($draft_data_decoded)) {
				$draft_data_decoded = array();
			}
		}
		$condition_context = array('amount' => $amount);

		while ($level = db_fetch($levels)) {
			if ((int)$level['level'] < $from_level)
				continue;

			// Check conditions
			if (!empty($level['conditions'])) {
				if (!evaluate_approval_conditions($level['conditions'], $draft_data_decoded, $condition_context)) {
					continue;
				}
			}

			$threshold = (float)$level['amount_threshold'];

			if ($threshold > 0 && $amount <= $threshold) {
				add_approval_action(
					$draft_id, 'approve', 0, (int)$level['level'],
					sprintf(_('Auto-approved: amount %.2f within threshold %.2f'), $amount, $threshold)
				);
			} else {
				$all_auto = false;
				break;
			}
		}

		return $all_auto;
	}

	/**
	 * Execute an approved draft by calling the registered executor callback.
	 *
	 * @param int $draft_id Draft ID
	 * @return bool         True on success
	 */
	private function executeApprovedDraft($draft_id)
	{
		$draft = get_approval_draft($draft_id);

		if (!$draft)
			return false;

		$trans_type = (int)$draft['trans_type'];
		$config = self::getTypeConfig($trans_type);

		if (!$config || !$config['executor'])
			return false;

		// Decode the JSON draft data
		$draft_data = json_decode($draft['draft_data'], true);

		// Fallback: try stripslashes if direct decode fails
		if ($draft_data === null && $draft['draft_data']) {
			$draft_data = json_decode(stripslashes($draft['draft_data']), true);
		}

		if ($draft_data === null)
			return false;

		// Release the reserved number so the executor can use it
		release_reserved_trans_number($trans_type, $draft['reserved_trans_no']);

		// Call the registered executor
		$executor = $config['executor'];
		$result = false;

		if (is_callable($executor)) {
			$result = call_user_func(
				$executor,
				$draft_data,
				(int)$draft['reserved_trans_no'],
				$draft_id
			);
		}

		if ($result !== false) {
			$posted_trans_no = is_int($result) ? $result : (int)$draft['reserved_trans_no'];
			update_approval_draft_trans_no($draft_id, $posted_trans_no);
		}

		return $result !== false;
	}

	/**
	 * Invoke a registered business-state lifecycle handler for one draft.
	 *
	 * @param string $config_key Config key containing the callable name
	 * @param array  $draft      Approval draft row
	 * @param string $comments   Optional comments passed to the handler
	 * @return mixed
	 */
	private function invokeDraftLifecycleHandler($config_key, $draft, $comments = '')
	{
		$trans_type = isset($draft['trans_type']) ? (int)$draft['trans_type'] : 0;
		$config = self::getTypeConfig($trans_type);

		if (!$config || empty($config[$config_key]) || !is_callable($config[$config_key]))
			return false;

		$draft_data = array();
		if (!empty($draft['draft_data'])) {
			$draft_data = json_decode($draft['draft_data'], true);
			if (!is_array($draft_data))
				$draft_data = json_decode(stripslashes($draft['draft_data']), true);
		}

		if (!is_array($draft_data))
			$draft_data = array();

		return call_user_func(
			$config[$config_key],
			$draft_data,
			isset($draft['id']) ? (int)$draft['id'] : 0,
			$comments
		);
	}

	/**
	 * Get the security role ID for a delegating user.
	 *
	 * @param int $user_id User ID
	 * @return int         Role ID
	 */
	private function getDelegatorRole($user_id)
	{
		$sql = "SELECT role_id FROM " . TB_PREF . "users
			WHERE id = " . db_escape($user_id);

		$result = db_query($sql, 'could not get delegator role');
		$row = db_fetch($result);

		return $row ? (int)$row['role_id'] : 0;
	}
}

/**
 * Check if a security area is granted to the current user.
 *
 * @param string $area_code Security area constant name (e.g., 'SA_APPROVALDIRECT')
 * @return bool
 */
function check_security_area($area_code)
{
	global $security_areas;

	if (!isset($security_areas[$area_code]))
		return false;

	return $_SESSION['wa_current_user']->can_access($area_code);
}
