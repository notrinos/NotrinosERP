<?php
/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	Approval Rules DB Functions

	Provides CRUD operations for approval_workflows and approval_levels tables.
***********************************************************************/

/**
 * Add a new approval workflow definition
 *
 * @param int    $trans_type        Transaction type constant (ST_*)
 * @param string $name              Workflow name
 * @param string $description       Workflow description
 * @param bool   $require_comments_on_reject  Require comments when rejecting
 * @param bool   $require_comments_on_approve Require comments when approving
 * @param bool   $allow_edit_on_approve       Allow editing draft during approval
 * @param bool   $allow_self_approve          Allow submitter to approve own draft
 * @return int   Inserted workflow ID
 */
function add_approval_workflow($trans_type, $name, $description = '',
	$require_comments_on_reject = true, $require_comments_on_approve = false,
	$allow_edit_on_approve = false, $allow_self_approve = false)
{
	$sql = "INSERT INTO " . TB_PREF . "approval_workflows
		(trans_type, name, description, is_active,
		 require_comments_on_reject, require_comments_on_approve,
		 allow_edit_on_approve, allow_self_approve,
		 created_by, created_date, version)
		VALUES ("
		. db_escape($trans_type) . ", "
		. db_escape($name) . ", "
		. db_escape($description) . ", "
		. "1, "
		. db_escape($require_comments_on_reject ? 1 : 0) . ", "
		. db_escape($require_comments_on_approve ? 1 : 0) . ", "
		. db_escape($allow_edit_on_approve ? 1 : 0) . ", "
		. db_escape($allow_self_approve ? 1 : 0) . ", "
		. db_escape($_SESSION['wa_current_user']->user) . ", "
		. "NOW(), 1)";

	db_query($sql, 'could not insert approval workflow');

	return db_insert_id();
}

/**
 * Update an existing approval workflow
 *
 * @param int    $workflow_id       Workflow ID
 * @param string $name              Workflow name
 * @param string $description       Workflow description
 * @param bool   $is_active         Active flag
 * @param bool   $require_comments_on_reject  Require comments when rejecting
 * @param bool   $require_comments_on_approve Require comments when approving
 * @param bool   $allow_edit_on_approve       Allow editing during approval
 * @param bool   $allow_self_approve          Allow self-approval
 * @return void
 */
function update_approval_workflow($workflow_id, $name, $description = '',
	$is_active = true, $require_comments_on_reject = true,
	$require_comments_on_approve = false, $allow_edit_on_approve = false,
	$allow_self_approve = false)
{
	$sql = "UPDATE " . TB_PREF . "approval_workflows SET "
		. "name = " . db_escape($name) . ", "
		. "description = " . db_escape($description) . ", "
		. "is_active = " . db_escape($is_active ? 1 : 0) . ", "
		. "require_comments_on_reject = " . db_escape($require_comments_on_reject ? 1 : 0) . ", "
		. "require_comments_on_approve = " . db_escape($require_comments_on_approve ? 1 : 0) . ", "
		. "allow_edit_on_approve = " . db_escape($allow_edit_on_approve ? 1 : 0) . ", "
		. "allow_self_approve = " . db_escape($allow_self_approve ? 1 : 0) . ", "
		. "modified_by = " . db_escape($_SESSION['wa_current_user']->user) . ", "
		. "modified_date = NOW(), "
		. "version = version + 1 "
		. "WHERE id = " . db_escape($workflow_id);

	db_query($sql, 'could not update approval workflow');
}

/**
 * Get a workflow definition by ID
 *
 * @param int $workflow_id Workflow ID
 * @return array|false     Workflow row or false
 */
function get_approval_workflow($workflow_id)
{
	$sql = "SELECT * FROM " . TB_PREF . "approval_workflows
		WHERE id = " . db_escape($workflow_id);

	$result = db_query($sql, 'could not retrieve approval workflow');

	return db_fetch($result);
}

/**
 * Get a workflow definition by transaction type
 *
 * @param int $trans_type Transaction type constant (ST_*)
 * @return array|false    Workflow row or false
 */
function get_approval_workflow_by_type($trans_type)
{
	$sql = "SELECT * FROM " . TB_PREF . "approval_workflows
		WHERE trans_type = " . db_escape($trans_type) . "
		AND is_active = 1";

	$result = db_query($sql, 'could not retrieve approval workflow by type');

	return db_fetch($result);
}

/**
 * Get all approval workflows
 *
 * @param bool $active_only Only return active workflows
 * @return resource          DB result set
 */
function get_all_approval_workflows($active_only = false)
{
	$sql = "SELECT w.*, COUNT(al.id) AS level_count
		FROM " . TB_PREF . "approval_workflows w
		LEFT JOIN " . TB_PREF . "approval_levels al ON al.workflow_id = w.id AND al.is_active = 1";

	if ($active_only)
		$sql .= " WHERE w.is_active = 1";

	$sql .= " GROUP BY w.id ORDER BY w.name";

	return db_query($sql, 'could not retrieve approval workflows');
}

/**
 * Delete a workflow and its levels (only if no drafts exist)
 *
 * @param int  $workflow_id Workflow ID
 * @return bool             True on success, false if drafts exist
 */
function delete_approval_workflow($workflow_id)
{
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "approval_drafts
		WHERE workflow_id = " . db_escape($workflow_id);

	$result = db_query($sql, 'could not check workflow drafts');
	$row = db_fetch_row($result);

	if ($row[0] > 0)
		return false;

	begin_transaction();

	$sql = "DELETE FROM " . TB_PREF . "approval_levels
		WHERE workflow_id = " . db_escape($workflow_id);
	db_query($sql, 'could not delete approval levels');

	$sql = "DELETE FROM " . TB_PREF . "approval_workflows
		WHERE id = " . db_escape($workflow_id);
	db_query($sql, 'could not delete approval workflow');

	commit_transaction();

	return true;
}

// =====================================================
// Approval Levels CRUD
// =====================================================

/**
 * Add a new approval level to a workflow
 *
 * @param int    $workflow_id       Workflow ID
 * @param int    $level             Level number (1-based sequential)
 * @param int    $role_id           Security role ID
 * @param int    $min_approvers     Minimum approvers required at this level
 * @param float  $amount_threshold  Auto-approve if amount <= this value
 * @param float  $amount_upper      Level required only if amount > this
 * @param int    $escalation_days   Days before auto-escalation (0 = disabled)
 * @param int    $escalation_to_level Level to escalate to
 * @param string $loc_code          Location restriction (null = all)
 * @param string $conditions        JSON-encoded conditions for conditional routing (null = no conditions)
 * @return int   Inserted level ID
 */
function add_approval_level($workflow_id, $level, $role_id, $min_approvers = 1,
	$amount_threshold = 0, $amount_upper = 0, $escalation_days = 0,
	$escalation_to_level = null, $loc_code = null, $conditions = null)
{
	$sql = "INSERT INTO " . TB_PREF . "approval_levels
		(workflow_id, level, role_id, min_approvers, amount_threshold,
		 amount_upper, escalation_days, escalation_to_level, loc_code, conditions, is_active)
		VALUES ("
		. db_escape($workflow_id) . ", "
		. db_escape($level) . ", "
		. db_escape($role_id) . ", "
		. db_escape($min_approvers) . ", "
		. db_escape($amount_threshold) . ", "
		. db_escape($amount_upper) . ", "
		. db_escape($escalation_days) . ", "
		. db_escape($escalation_to_level, true) . ", "
		. db_escape($loc_code, true) . ", "
		. db_escape($conditions, true) . ", 1)";

	db_query($sql, 'could not insert approval level');

	return db_insert_id();
}

/**
 * Update an existing approval level
 *
 * @param int    $level_id          Level record ID
 * @param int    $role_id           Security role ID
 * @param int    $min_approvers     Minimum approvers required
 * @param float  $amount_threshold  Auto-approve threshold
 * @param float  $amount_upper      Upper bound for level applicability
 * @param int    $escalation_days   Escalation timeout in days
 * @param int    $escalation_to_level Escalation target level
 * @param string $loc_code          Location code restriction
 * @param bool   $is_active         Active flag
 * @param string $conditions        JSON-encoded conditions for conditional routing (null = no conditions)
 * @return void
 */
function update_approval_level($level_id, $role_id, $min_approvers = 1,
	$amount_threshold = 0, $amount_upper = 0, $escalation_days = 0,
	$escalation_to_level = null, $loc_code = null, $is_active = true,
	$conditions = null)
{
	$sql = "UPDATE " . TB_PREF . "approval_levels SET "
		. "role_id = " . db_escape($role_id) . ", "
		. "min_approvers = " . db_escape($min_approvers) . ", "
		. "amount_threshold = " . db_escape($amount_threshold) . ", "
		. "amount_upper = " . db_escape($amount_upper) . ", "
		. "escalation_days = " . db_escape($escalation_days) . ", "
		. "escalation_to_level = " . db_escape($escalation_to_level, true) . ", "
		. "loc_code = " . db_escape($loc_code, true) . ", "
		. "conditions = " . db_escape($conditions, true) . ", "
		. "is_active = " . db_escape($is_active ? 1 : 0) . " "
		. "WHERE id = " . db_escape($level_id);

	db_query($sql, 'could not update approval level');
}

/**
 * Get all levels for a workflow, ordered by level number
 *
 * @param int  $workflow_id  Workflow ID
 * @param bool $active_only  Only return active levels
 * @return resource           DB result set
 */
function get_approval_levels($workflow_id, $active_only = true)
{
	$sql = "SELECT al.*, sr.role AS role_name
		FROM " . TB_PREF . "approval_levels al
		LEFT JOIN " . TB_PREF . "security_roles sr ON sr.id = al.role_id
		WHERE al.workflow_id = " . db_escape($workflow_id);

	if ($active_only)
		$sql .= " AND al.is_active = 1";

	$sql .= " ORDER BY al.level ASC";

	return db_query($sql, 'could not retrieve approval levels');
}

/**
 * Get a specific approval level record
 *
 * @param int $level_id Level record ID
 * @return array|false  Level row or false
 */
function get_approval_level($level_id)
{
	$sql = "SELECT al.*, sr.role AS role_name
		FROM " . TB_PREF . "approval_levels al
		LEFT JOIN " . TB_PREF . "security_roles sr ON sr.id = al.role_id
		WHERE al.id = " . db_escape($level_id);

	$result = db_query($sql, 'could not retrieve approval level');

	return db_fetch($result);
}

/**
 * Get the maximum level number in a workflow
 *
 * @param int $workflow_id Workflow ID
 * @return int             Maximum level number (0 if no levels)
 */
function get_max_approval_level($workflow_id)
{
	$sql = "SELECT MAX(level) AS max_level
		FROM " . TB_PREF . "approval_levels
		WHERE workflow_id = " . db_escape($workflow_id) . "
		AND is_active = 1";

	$result = db_query($sql, 'could not get max approval level');
	$row = db_fetch_row($result);

	return $row[0] ? (int)$row[0] : 0;
}

/**
 * Get the minimum required approvers for a specific workflow level
 *
 * @param int $workflow_id Workflow ID
 * @param int $level       Level number
 * @return int             Minimum approvers required
 */
function get_min_approvers_for_level($workflow_id, $level)
{
	$sql = "SELECT min_approvers
		FROM " . TB_PREF . "approval_levels
		WHERE workflow_id = " . db_escape($workflow_id) . "
		AND level = " . db_escape($level) . "
		AND is_active = 1
		LIMIT 1";

	$result = db_query($sql, 'could not get min approvers for level');
	$row = db_fetch_row($result);

	return $row ? (int)$row[0] : 1;
}

/**
 * Get the approval level configuration for a user's role in a specific workflow
 *
 * @param int $workflow_id Workflow ID
 * @param int $role_id     Security role ID
 * @return array|false     Level row or false
 */
function get_user_approval_level_for_workflow($workflow_id, $role_id)
{
	$sql = "SELECT * FROM " . TB_PREF . "approval_levels
		WHERE workflow_id = " . db_escape($workflow_id) . "
		AND role_id = " . db_escape($role_id) . "
		AND is_active = 1
		ORDER BY level ASC
		LIMIT 1";

	$result = db_query($sql, 'could not get user approval level');

	return db_fetch($result);
}

/**
 * Get all role IDs that are approvers at a given level of a workflow
 *
 * @param int    $workflow_id Workflow ID
 * @param int    $level       Level number
 * @param string $loc_code    Optional location filter
 * @return array              Array of role IDs
 */
function get_approver_roles_at_level($workflow_id, $level, $loc_code = null)
{
	$sql = "SELECT DISTINCT role_id FROM " . TB_PREF . "approval_levels
		WHERE workflow_id = " . db_escape($workflow_id) . "
		AND level = " . db_escape($level) . "
		AND is_active = 1";

	if ($loc_code !== null)
		$sql .= " AND (loc_code IS NULL OR loc_code = " . db_escape($loc_code) . ")";

	$result = db_query($sql, 'could not get approver roles at level');

	$roles = array();
	while ($row = db_fetch($result)) {
		$roles[] = (int)$row['role_id'];
	}

	return $roles;
}

/**
 * Delete a specific approval level record
 *
 * @param int $level_id Level record ID
 * @return void
 */
function delete_approval_level($level_id)
{
	$sql = "DELETE FROM " . TB_PREF . "approval_levels
		WHERE id = " . db_escape($level_id);

	db_query($sql, 'could not delete approval level');
}

/**
 * Delete all levels for a workflow
 *
 * @param int $workflow_id Workflow ID
 * @return void
 */
function delete_all_approval_levels($workflow_id)
{
	$sql = "DELETE FROM " . TB_PREF . "approval_levels
		WHERE workflow_id = " . db_escape($workflow_id);

	db_query($sql, 'could not delete all approval levels');
}

/**
 * Check if a workflow has any active levels configured
 *
 * @param int $workflow_id Workflow ID
 * @return bool            True if at least one active level exists
 */
function workflow_has_active_levels($workflow_id)
{
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "approval_levels
		WHERE workflow_id = " . db_escape($workflow_id) . "
		AND is_active = 1";

	$result = db_query($sql, 'could not check workflow levels');
	$row = db_fetch_row($result);

	return $row[0] > 0;
}
