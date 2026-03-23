<?php
/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	Admin: Approval Delegation Management

	Allows administrators to manage approval delegations — temporary
	reassignment of approval authority from one user to another.

	Phase 2 of the Approval System Development Plan.
***********************************************************************/
$page_security = 'SA_APPROVALRULES';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');

page(_($help_context = 'Approval Delegations'));

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/approval/db/approval_history_db.inc');
include_once($path_to_root . '/admin/db/approval_rules_setup_db.inc');

simple_page_mode(true);

// =====================================================
// Validation
// =====================================================

/**
 * Validate delegation form.
 *
 * @return bool True if valid
 */
function can_process()
{
	if (get_post('from_user_id') == '' || get_post('from_user_id') == 0) {
		display_error(_('Please select the user to delegate from.'));
		set_focus('from_user_id');
		return false;
	}

	if (get_post('to_user_id') == '' || get_post('to_user_id') == 0) {
		display_error(_('Please select the user to delegate to.'));
		set_focus('to_user_id');
		return false;
	}

	if (get_post('from_user_id') == get_post('to_user_id')) {
		display_error(_('A user cannot delegate to themselves.'));
		set_focus('to_user_id');
		return false;
	}

	if (get_post('delegation_from_date') == '') {
		display_error(_('Please enter a start date.'));
		set_focus('delegation_from_date');
		return false;
	}

	$from_date = get_post('delegation_from_date');
	$to_date = get_post('delegation_to_date');

	if ($to_date != '' && date2sql($to_date) < date2sql($from_date)) {
		display_error(_('End date cannot be before start date.'));
		set_focus('delegation_to_date');
		return false;
	}

	return true;
}

// =====================================================
// Processing — Add
// =====================================================

if ($Mode == 'ADD_ITEM' && can_process()) {
	$from_date = date2sql(get_post('delegation_from_date'));
	$to_date = get_post('delegation_to_date') != '' ? date2sql(get_post('delegation_to_date')) : null;
	$trans_type = get_post('delegation_trans_type') != '' ? (int)get_post('delegation_trans_type') : null;

	add_approval_delegation(
		(int)get_post('from_user_id'),
		(int)get_post('to_user_id'),
		$trans_type,
		$from_date,
		$to_date,
		get_post('delegation_reason')
	);

	display_notification(_('New approval delegation has been added.'));
	$Mode = 'RESET';
}

// =====================================================
// Processing — Update
// =====================================================

if ($Mode == 'UPDATE_ITEM' && can_process()) {
	$from_date = date2sql(get_post('delegation_from_date'));
	$to_date = get_post('delegation_to_date') != '' ? date2sql(get_post('delegation_to_date')) : null;
	$trans_type = get_post('delegation_trans_type') != '' ? (int)get_post('delegation_trans_type') : null;

	// Deactivate old and create new (simpler than updating all fields)
	deactivate_approval_delegation($selected_id);

	add_approval_delegation(
		(int)get_post('from_user_id'),
		(int)get_post('to_user_id'),
		$trans_type,
		$from_date,
		$to_date,
		get_post('delegation_reason')
	);

	display_notification(_('Approval delegation has been updated.'));
	$Mode = 'RESET';
}

// =====================================================
// Processing — Delete (Deactivate)
// =====================================================

if ($Mode == 'Delete') {
	deactivate_approval_delegation($selected_id);
	display_notification(_('Approval delegation has been deactivated.'));
	$Mode = 'RESET';
}

// =====================================================
// Reset
// =====================================================

if ($Mode == 'RESET') {
	$selected_id = -1;
	$sav = get_post('show_inactive');
	unset($_POST);
	$_POST['show_inactive'] = $sav;
}

// =====================================================
// RENDERING — Existing Delegations Table
// =====================================================

$show_inactive = check_value('show_inactive');
$delegations = get_all_approval_delegations(!$show_inactive);

start_form();

start_table(TABLESTYLE, "width='90%'");

$th = array(
	_('From User'),
	_('To User'),
	_('Transaction Type'),
	_('From Date'),
	_('To Date'),
	_('Reason'),
	_('Active'),
	'', ''
);
table_header($th);

$k = 0;
while ($row = db_fetch($delegations)) {
	alt_table_row_color($k);
	label_cell($row['from_user_name']);
	label_cell($row['to_user_name']);
	label_cell($row['trans_type'] !== null ? get_trans_type_label($row['trans_type']) : _('All Types'));
	label_cell(sql2date($row['from_date']));
	label_cell($row['to_date'] ? sql2date($row['to_date']) : _('Indefinite'));
	label_cell($row['reason']);
	label_cell($row['is_active'] ? _('Yes') : _('No'), "align='center'");
	edit_button_cell('Edit' . $row['id'], _('Edit'));
	delete_button_cell('Delete' . $row['id'], _('Deactivate'));
	end_row();
}

end_table();

echo '<br>';
check_row(_('Show inactive delegations:'), 'show_inactive', $show_inactive, true);
echo '<br>';

// =====================================================
// RENDERING — Delegation Add/Edit Form
// =====================================================

start_table(TABLESTYLE2);

if ($selected_id != -1 && $Mode == 'Edit') {
	$delegation_row = get_approval_delegation($selected_id);
	if ($delegation_row) {
		$_POST['from_user_id'] = $delegation_row['from_user_id'];
		$_POST['to_user_id'] = $delegation_row['to_user_id'];
		$_POST['delegation_trans_type'] = $delegation_row['trans_type'];
		$_POST['delegation_from_date'] = sql2date($delegation_row['from_date']);
		$_POST['delegation_to_date'] = $delegation_row['to_date'] ? sql2date($delegation_row['to_date']) : '';
		$_POST['delegation_reason'] = $delegation_row['reason'];
	}
	hidden('selected_id', $selected_id);
}

// From User
$users = get_users_for_delegation();
echo "<tr><td class='label'>" . _('Delegate From:') . "</td><td>";
echo array_selector('from_user_id', get_post('from_user_id'), $users,
	array('spec_option' => _('-- Select User --'), 'spec_id' => 0));
echo "</td></tr>\n";

// To User
echo "<tr><td class='label'>" . _('Delegate To:') . "</td><td>";
echo array_selector('to_user_id', get_post('to_user_id'), $users,
	array('spec_option' => _('-- Select User --'), 'spec_id' => 0));
echo "</td></tr>\n";

// Transaction Type (optional — null = all)
$approvable_types = get_approvable_transaction_types();
echo "<tr><td class='label'>" . _('Transaction Type:') . "</td><td>";
echo array_selector('delegation_trans_type', get_post('delegation_trans_type'), $approvable_types,
	array('spec_option' => _('All Transaction Types'), 'spec_id' => ''));
echo "</td></tr>\n";

// From Date
date_row(_('From Date:'), 'delegation_from_date',
	isset($_POST['delegation_from_date']) ? $_POST['delegation_from_date'] : null);

// To Date (optional)
date_row(_('To Date (blank=indefinite):'), 'delegation_to_date',
	isset($_POST['delegation_to_date']) ? $_POST['delegation_to_date'] : null, false, 0, 0, 0, null, true);

// Reason
text_row(_('Reason:'), 'delegation_reason', get_post('delegation_reason'), 60, 255);

end_table(1);

submit_add_or_update_center($selected_id == -1, '', 'both');

end_form();
end_page();
