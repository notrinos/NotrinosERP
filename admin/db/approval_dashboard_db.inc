<?php
/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	Dashboard & Inquiry DB Helper Functions

	Provides additional DB queries needed by the approval dashboard,
	inquiry, and draft viewer pages. These functions supplement the
	core DB functions in includes/approval/db/ but are only needed
	by the admin UI pages.

	PHP 5.6+ compatible.
***********************************************************************/

/**
 * Count approval drafts grouped by status.
 *
 * Returns an associative array with status counts for
 * dashboard summary cards.
 *
 * @param array $filters Optional filters: trans_type, from_date, to_date
 * @return array         Keys: pending, approved, rejected, cancelled, expired, total
 */
function get_approval_status_counts($filters = array())
{
	$sql = "SELECT
		SUM(CASE WHEN d.status = " . APPROVAL_STATUS_PENDING . " THEN 1 ELSE 0 END) AS pending,
		SUM(CASE WHEN d.status = " . APPROVAL_STATUS_APPROVED . " THEN 1 ELSE 0 END) AS approved,
		SUM(CASE WHEN d.status = " . APPROVAL_STATUS_REJECTED . " THEN 1 ELSE 0 END) AS rejected,
		SUM(CASE WHEN d.status = " . APPROVAL_STATUS_CANCELLED . " THEN 1 ELSE 0 END) AS cancelled,
		SUM(CASE WHEN d.status = " . APPROVAL_STATUS_EXPIRED . " THEN 1 ELSE 0 END) AS expired,
		COUNT(*) AS total
		FROM " . TB_PREF . "approval_drafts d";

	$where = array();

	if (!empty($filters['trans_type']))
		$where[] = "d.trans_type = " . db_escape($filters['trans_type']);

	if (!empty($filters['from_date']))
		$where[] = "d.submitted_date >= " . db_escape($filters['from_date']);

	if (!empty($filters['to_date']))
		$where[] = "d.submitted_date <= " . db_escape($filters['to_date'] . ' 23:59:59');

	if (count($where) > 0)
		$sql .= " WHERE " . implode(' AND ', $where);

	$result = db_query($sql, 'could not get approval status counts');
	$row = db_fetch($result);

	return array(
		'pending'   => (int)$row['pending'],
		'approved'  => (int)$row['approved'],
		'rejected'  => (int)$row['rejected'],
		'cancelled' => (int)$row['cancelled'],
		'expired'   => (int)$row['expired'],
		'total'     => (int)$row['total'],
	);
}

/**
 * Build SQL query for the approval dashboard pager.
 *
 * Returns a SQL string suitable for use with db_pager. Shows drafts
 * that the current user can act on (based on their role and delegations).
 *
 * @param int   $user_id   Current user ID
 * @param int   $role_id   Current user's role ID
 * @param array $filters   Optional filters: trans_type, status, from_date, to_date
 * @return string           SQL query string
 */
function get_sql_for_approval_dashboard($user_id, $role_id, $filters = array())
{
	$sql = "SELECT d.id, d.trans_type, d.reserved_trans_no, d.reference,
			d.amount, d.currency, d.status, d.current_level,
			d.submitted_date, d.completed_date, d.summary,
			w.name AS workflow_name,
			u.real_name AS submitted_by_name
		FROM " . TB_PREF . "approval_drafts d
		LEFT JOIN " . TB_PREF . "approval_workflows w ON w.id = d.workflow_id
		LEFT JOIN " . TB_PREF . "users u ON u.id = d.submitted_by
		INNER JOIN " . TB_PREF . "approval_levels al
			ON al.workflow_id = d.workflow_id
			AND al.level = d.current_level
			AND al.role_id = " . db_escape($role_id) . "
			AND al.is_active = 1
		WHERE d.status = " . APPROVAL_STATUS_PENDING;

	if (!empty($filters['trans_type']))
		$sql .= " AND d.trans_type = " . db_escape($filters['trans_type']);

	if (!empty($filters['from_date']))
		$sql .= " AND d.submitted_date >= " . db_escape($filters['from_date']);

	if (!empty($filters['to_date']))
		$sql .= " AND d.submitted_date <= " . db_escape($filters['to_date'] . ' 23:59:59');

	$sql .= " ORDER BY d.submitted_date DESC";

	return $sql;
}

/**
 * Build SQL query for the approval inquiry pager.
 *
 * Returns a SQL string that shows all drafts with flexible filtering.
 * Suitable for use with db_pager for paginated inquiry.
 *
 * @param array $filters Filters: trans_type, status, from_date, to_date,
 *                       submitter, reference, workflow_id
 * @return string         SQL query string
 */
function get_sql_for_approval_inquiry($filters = array())
{
	$sql = "SELECT d.id, d.trans_type, d.reserved_trans_no, d.reference,
			d.amount, d.currency, d.status, d.current_level,
			d.submitted_by, d.submitted_date, d.completed_date,
			d.summary, d.approved_trans_no, d.loc_code,
			w.name AS workflow_name,
			u.real_name AS submitted_by_name
		FROM " . TB_PREF . "approval_drafts d
		LEFT JOIN " . TB_PREF . "approval_workflows w ON w.id = d.workflow_id
		LEFT JOIN " . TB_PREF . "users u ON u.id = d.submitted_by
		WHERE 1=1";

	if (isset($filters['status']) && $filters['status'] !== '' && $filters['status'] !== '-1')
		$sql .= " AND d.status = " . db_escape($filters['status']);

	if (!empty($filters['trans_type']))
		$sql .= " AND d.trans_type = " . db_escape($filters['trans_type']);

	if (!empty($filters['from_date']))
		$sql .= " AND d.submitted_date >= " . db_escape($filters['from_date']);

	if (!empty($filters['to_date']))
		$sql .= " AND d.submitted_date <= " . db_escape($filters['to_date'] . ' 23:59:59');

	if (!empty($filters['submitter']))
		$sql .= " AND d.submitted_by = " . db_escape($filters['submitter']);

	if (!empty($filters['reference']))
		$sql .= " AND d.reference LIKE " . db_escape('%' . $filters['reference'] . '%');

	if (!empty($filters['workflow_id']))
		$sql .= " AND d.workflow_id = " . db_escape($filters['workflow_id']);

	$sql .= " ORDER BY d.submitted_date DESC";

	return $sql;
}

/**
 * Build SQL query for "My Submissions" tab on the dashboard.
 *
 * Shows all drafts submitted by the current user.
 *
 * @param int   $user_id Current user ID
 * @param array $filters Optional filters: trans_type, status, from_date, to_date
 * @return string         SQL query string
 */
function get_sql_for_my_submissions($user_id, $filters = array())
{
	$sql = "SELECT d.id, d.trans_type, d.reserved_trans_no, d.reference,
			d.amount, d.currency, d.status, d.current_level,
			d.submitted_date, d.completed_date, d.summary,
			w.name AS workflow_name,
			u.real_name AS submitted_by_name
		FROM " . TB_PREF . "approval_drafts d
		LEFT JOIN " . TB_PREF . "approval_workflows w ON w.id = d.workflow_id
		LEFT JOIN " . TB_PREF . "users u ON u.id = d.submitted_by
		WHERE d.submitted_by = " . db_escape($user_id);

	if (isset($filters['status']) && $filters['status'] !== '' && $filters['status'] !== '-1')
		$sql .= " AND d.status = " . db_escape($filters['status']);

	if (!empty($filters['trans_type']))
		$sql .= " AND d.trans_type = " . db_escape($filters['trans_type']);

	if (!empty($filters['from_date']))
		$sql .= " AND d.submitted_date >= " . db_escape($filters['from_date']);

	if (!empty($filters['to_date']))
		$sql .= " AND d.submitted_date <= " . db_escape($filters['to_date'] . ' 23:59:59');

	$sql .= " ORDER BY d.submitted_date DESC";

	return $sql;
}

/**
 * Get draft details with all related information for the viewer page.
 *
 * Returns a single draft row with full workflow and submitter information.
 *
 * @param int $draft_id Draft ID
 * @return array|false  Draft row with related data, or false
 */
function get_approval_draft_full($draft_id)
{
	$sql = "SELECT d.*,
			w.name AS workflow_name,
			w.description AS workflow_description,
			w.require_comments_on_reject,
			w.require_comments_on_approve,
			w.allow_edit_on_approve,
			w.allow_self_approve,
			u.real_name AS submitted_by_name,
			u.login_id AS submitted_by_login
		FROM " . TB_PREF . "approval_drafts d
		LEFT JOIN " . TB_PREF . "approval_workflows w ON w.id = d.workflow_id
		LEFT JOIN " . TB_PREF . "users u ON u.id = d.submitted_by
		WHERE d.id = " . db_escape($draft_id);

	$result = db_query($sql, 'could not get full approval draft');

	return db_fetch($result);
}

/**
 * Get approval levels with role names for a workflow.
 *
 * @param int $workflow_id Workflow ID
 * @return resource         DB result set
 */
function get_approval_levels_with_roles($workflow_id)
{
	$sql = "SELECT al.*, sr.role AS role_name, sr.description AS role_description
		FROM " . TB_PREF . "approval_levels al
		LEFT JOIN " . TB_PREF . "security_roles sr ON sr.id = al.role_id
		WHERE al.workflow_id = " . db_escape($workflow_id) . "
		AND al.is_active = 1
		ORDER BY al.level ASC";

	return db_query($sql, 'could not get approval levels with roles');
}

/**
 * Get recent approval actions across all drafts for activity feed.
 *
 * @param int $limit Maximum number of actions to return
 * @return resource   DB result set
 */
function get_recent_approval_activity($limit = 20)
{
	$limit = (int)$limit;

	$sql = "SELECT a.*, d.trans_type, d.reserved_trans_no, d.reference,
			d.summary, w.name AS workflow_name,
			u.real_name AS user_name
		FROM " . TB_PREF . "approval_actions a
		LEFT JOIN " . TB_PREF . "approval_drafts d ON d.id = a.draft_id
		LEFT JOIN " . TB_PREF . "approval_workflows w ON w.id = d.workflow_id
		LEFT JOIN " . TB_PREF . "users u ON u.id = a.user_id
		ORDER BY a.action_date DESC
		LIMIT $limit";

	return db_query($sql, 'could not get recent approval activity');
}
