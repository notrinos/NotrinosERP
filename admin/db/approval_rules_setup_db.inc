<?php
/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	Admin Setup DB Helper Functions

	Provides additional DB helpers used by the approval rules admin pages
	that are not needed by the core approval service.
***********************************************************************/

/**
 * Get all transaction types that can have approval workflows.
 *
 * Returns array of trans_type => label pairs from $systypes_array,
 * filtered to only include transaction types that are typically
 * user-initiated (excludes system-generated types).
 *
 * @return array Associative array of trans_type => label
 */
function get_approvable_transaction_types()
{
	global $systypes_array;

	// Transaction types that support approval workflows
	$approvable_types = array(
		ST_JOURNAL,
		ST_BANKPAYMENT,
		ST_BANKDEPOSIT,
		ST_SALESINVOICE,
		ST_CUSTCREDIT,
		ST_CUSTPAYMENT,
		ST_CUSTDELIVERY,
		ST_LOCTRANSFER,
		ST_INVADJUST,
		ST_PURCHORDER,
		ST_SUPPINVOICE,
		ST_SUPPCREDIT,
		ST_SUPPAYMENT,
		ST_SUPPRECEIVE,
		ST_WORKORDER,
		ST_MANUISSUE,
		ST_MANURECEIVE,
		ST_SALESORDER,
		ST_SALESQUOTE,
		ST_PAYSLIP,
		ST_LEAVE_REQUEST,
		ST_OVERTIME_REQUEST,
		ST_PAYROLL_PERIOD,
	);

	$result = array();
	foreach ($approvable_types as $type) {
		if (isset($systypes_array[$type])) {
			$result[$type] = $systypes_array[$type];
		}
	}

	return $result;
}

/**
 * Get transaction types that do NOT yet have a workflow defined.
 *
 * Used to populate the "Add New Workflow" dropdown so that each
 * transaction type can only have one workflow.
 *
 * @return array Associative array of trans_type => label
 */
function get_available_transaction_types_for_workflow()
{
	$all_types = get_approvable_transaction_types();

	$sql = "SELECT trans_type FROM " . TB_PREF . "approval_workflows";
	$result = db_query($sql, 'could not get existing workflow types');

	$used_types = array();
	while ($row = db_fetch($result)) {
		$used_types[] = (int)$row['trans_type'];
	}

	$available = array();
	foreach ($all_types as $type => $label) {
		if (!in_array($type, $used_types)) {
			$available[$type] = $label;
		}
	}

	return $available;
}

/**
 * Get a formatted label for a transaction type constant.
 *
 * @param int $trans_type Transaction type constant
 * @return string         Human-readable label
 */
function get_trans_type_label($trans_type)
{
	global $systypes_array;

	return isset($systypes_array[$trans_type]) ? $systypes_array[$trans_type] : _('Unknown');
}

/**
 * Check if a workflow has any pending (unresolved) drafts.
 *
 * @param int $workflow_id Workflow ID
 * @return int             Number of pending drafts
 */
function count_workflow_pending_drafts($workflow_id)
{
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "approval_drafts
		WHERE workflow_id = " . db_escape($workflow_id) . "
		AND status = 0";

	$result = db_query($sql, 'could not count pending drafts');
	$row = db_fetch_row($result);

	return (int)$row[0];
}

/**
 * Get all security roles for the approval level dropdown.
 *
 * @param bool $active_only Only return active roles
 * @return array            Array of id => role name
 */
function get_security_roles_for_approval($active_only = true)
{
	$sql = "SELECT id, role, description FROM " . TB_PREF . "security_roles";
	if ($active_only) {
		$sql .= " WHERE inactive = 0";
	}
	$sql .= " ORDER BY role";

	$result = db_query($sql, 'could not get security roles');

	$roles = array();
	while ($row = db_fetch($result)) {
		$roles[$row['id']] = $row['role'] . ' - ' . $row['description'];
	}

	return $roles;
}

/**
 * Get all active delegations with user names.
 *
 * @param bool $active_only Only show active delegations
 * @return resource          DB result set
 */
function get_all_approval_delegations($active_only = true)
{
	$sql = "SELECT d.*,
		u_from.real_name AS from_user_name,
		u_to.real_name AS to_user_name
		FROM " . TB_PREF . "approval_delegations d
		LEFT JOIN " . TB_PREF . "users u_from ON u_from.id = d.from_user_id
		LEFT JOIN " . TB_PREF . "users u_to ON u_to.id = d.to_user_id";

	if ($active_only) {
		$sql .= " WHERE d.is_active = 1";
	}

	$sql .= " ORDER BY d.from_date DESC";

	return db_query($sql, 'could not get approval delegations');
}

/**
 * Get a single delegation record by ID.
 *
 * @param int $delegation_id Delegation ID
 * @return array|false       Delegation row or false
 */
function get_approval_delegation($delegation_id)
{
	$sql = "SELECT d.*,
		u_from.real_name AS from_user_name,
		u_to.real_name AS to_user_name
		FROM " . TB_PREF . "approval_delegations d
		LEFT JOIN " . TB_PREF . "users u_from ON u_from.id = d.from_user_id
		LEFT JOIN " . TB_PREF . "users u_to ON u_to.id = d.to_user_id
		WHERE d.id = " . db_escape($delegation_id);

	$result = db_query($sql, 'could not get delegation');

	return db_fetch($result);
}

/**
 * Delete a delegation record.
 *
 * @param int $delegation_id Delegation ID
 * @return void
 */
function delete_approval_delegation_record($delegation_id)
{
	$sql = "DELETE FROM " . TB_PREF . "approval_delegations
		WHERE id = " . db_escape($delegation_id);

	db_query($sql, 'could not delete delegation');
}

/**
 * Get all users for delegation dropdowns.
 *
 * @param bool $active_only Only return active users
 * @return array            Array of user_id => real_name
 */
function get_users_for_delegation($active_only = true)
{
	$sql = "SELECT id, real_name, login_id FROM " . TB_PREF . "users";
	if ($active_only) {
		$sql .= " WHERE inactive = 0";
	}
	$sql .= " ORDER BY real_name";

	$result = db_query($sql, 'could not get users');

	$users = array();
	while ($row = db_fetch($result)) {
		$users[$row['id']] = $row['real_name'] . ' (' . $row['login_id'] . ')';
	}

	return $users;
}
