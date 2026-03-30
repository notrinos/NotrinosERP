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
	Admin: Approval Workflow Rules Configuration

	Manages approval workflows and their multi-level approval chains.
	Each workflow is tied to a transaction type and contains one or more
	approval levels with role assignments and amount thresholds.
***********************************************************************/
$page_security = 'SA_APPROVALRULES';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');

page(_($help_context = 'Approval Workflow Rules'));

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/access_levels.inc');
include_once($path_to_root . '/includes/approval/db/approval_rules_db.inc');
include_once($path_to_root . '/admin/db/approval_rules_setup_db.inc');

// =====================================================
// Page Mode Management
// =====================================================

$selected_workflow = get_post('workflow_id', -1);
$edit_workflow = false;
$selected_level = get_post('selected_level_id', -1);

// Detect Edit/Delete button clicks for workflows
// find_submit returns -1 (truthy) when not found, so check > 0
$workflow_edit_id = find_submit('EditWorkflow');
if ($workflow_edit_id > 0) {
	$selected_workflow = $workflow_edit_id;
	$edit_workflow = true;
	$Ajax->activate('_page_body');
}

$workflow_delete_id = find_submit('DeleteWorkflow');
if ($workflow_delete_id > 0) {
	$selected_workflow = $workflow_delete_id;
	$Ajax->activate('_page_body');
}

// Detect Edit/Delete button clicks for levels
$level_edit_id = find_submit('EditLevel');
if ($level_edit_id > 0) {
	$selected_level = $level_edit_id;
	// Auto-select the correct workflow for this level
	$level_info = get_approval_level($level_edit_id);
	if ($level_info) {
		$selected_workflow = $level_info['workflow_id'];
		$_POST['workflow_select'] = $level_info['workflow_id'];
	}
	$Ajax->activate('_page_body');
}

$level_delete_id = find_submit('DeleteLevel');
if ($level_delete_id > 0) {
	$selected_level = $level_delete_id;
}

// Track which workflow is selected for level management
if (list_updated('workflow_select')) {
	$selected_workflow = get_post('workflow_select');
	$Ajax->activate('_page_body');
}

// =====================================================
// Workflow Processing — Add / Update
// =====================================================

/**
 * Validate workflow form fields.
 *
 * @return bool True if valid
 */
function validate_workflow_form()
{
	if (strlen(get_post('workflow_name')) == 0) {
		display_error(_('Workflow name cannot be empty.'));
		set_focus('workflow_name');
		return false;
	}

	if (get_post('workflow_trans_type') === '' || get_post('workflow_trans_type') === null) {
		display_error(_('Please select a transaction type.'));
		set_focus('workflow_trans_type');
		return false;
	}

	return true;
}

if (get_post('add_workflow')) {
	if (validate_workflow_form()) {
		$workflow_id = add_approval_workflow(
			get_post('workflow_trans_type'),
			get_post('workflow_name'),
			get_post('workflow_description'),
			check_value('require_comments_reject'),
			check_value('require_comments_approve'),
			check_value('allow_edit_approve'),
			check_value('allow_self_approve')
		);
		display_notification(_('New approval workflow has been added.'));
		$selected_workflow = $workflow_id;
		$_POST['workflow_name'] = '';
		$_POST['workflow_description'] = '';
		$Ajax->activate('_page_body');
	}
}

if (get_post('update_workflow') && $selected_workflow > 0) {
	if (validate_workflow_form()) {
		update_approval_workflow(
			$selected_workflow,
			get_post('workflow_name'),
			get_post('workflow_description'),
			check_value('workflow_active'),
			check_value('require_comments_reject'),
			check_value('require_comments_approve'),
			check_value('allow_edit_approve'),
			check_value('allow_self_approve')
		);
		display_notification(_('Approval workflow has been updated.'));
		$edit_workflow = false;
		$Ajax->activate('_page_body');
	}
}

// =====================================================
// Workflow Processing — Delete
// =====================================================

if ($workflow_delete_id > 0) {
	$pending_count = count_workflow_pending_drafts($workflow_delete_id);

	if ($pending_count > 0) {
		display_error(sprintf(
			_('Cannot delete this workflow because there are %d pending draft(s) using it.'),
			$pending_count
		));
	} else {
		$result = delete_approval_workflow($workflow_delete_id);
		if ($result) {
			display_notification(_('Approval workflow has been deleted.'));
			$selected_workflow = -1;
		} else {
			display_error(_('Cannot delete workflow: it has associated draft records.'));
		}
	}
	$Ajax->activate('_page_body');
}

if (get_post('cancel_workflow')) {
	$edit_workflow = false;
	$selected_workflow = -1;
	$Ajax->activate('_page_body');
}

// =====================================================
// Level Processing — Add / Update / Delete
// =====================================================

/**
 * Validate level form fields.
 *
 * @return bool True if valid
 */
function validate_level_form()
{
	if (get_post('level_role_id') === '' || get_post('level_role_id') == 0) {
		display_error(_('Please select a security role for this approval level.'));
		set_focus('level_role_id');
		return false;
	}

	$min_approvers = (int)get_post('level_min_approvers');
	if ($min_approvers < 1) {
		display_error(_('Minimum approvers must be at least 1.'));
		set_focus('level_min_approvers');
		return false;
	}

	$amount_threshold = input_num('level_amount_threshold', 0);
	if ($amount_threshold < 0) {
		display_error(_('Amount threshold cannot be negative.'));
		set_focus('level_amount_threshold');
		return false;
	}

	return true;
}

if (get_post('add_level') && $selected_workflow > 0) {
	if (validate_level_form()) {
		$max_level = get_max_approval_level($selected_workflow);
		$new_level = $max_level + 1;

		$conditions_json = get_post('level_conditions');
		if ($conditions_json != '') {
			$validation = validate_approval_conditions($conditions_json);
			if (is_array($validation) && !empty($validation['valid']) === false) {
				$error_message = _('Invalid conditions.');
				if (!empty($validation['errors']) && is_array($validation['errors'])) {
					$error_message = implode(' ', $validation['errors']);
				}
				display_error($error_message);
				$conditions_json = null;
			} else {
				$normalized_conditions = normalize_approval_conditions_json($conditions_json);
				if ($normalized_conditions !== null) {
					$conditions_json = $normalized_conditions;
				}
			}
		} else {
			$conditions_json = null;
		}

		add_approval_level(
			$selected_workflow,
			$new_level,
			get_post('level_role_id'),
			(int)get_post('level_min_approvers'),
			input_num('level_amount_threshold', 0),
			input_num('level_amount_upper', 0),
			(int)get_post('level_escalation_days'),
			get_post('level_escalation_to') ? (int)get_post('level_escalation_to') : null,
			get_post('level_loc_code') != '' ? get_post('level_loc_code') : null,
			$conditions_json
		);
		display_notification(sprintf(_('Approval level %d has been added.'), $new_level));
		$selected_level = -1;
		$Ajax->activate('_page_body');
	}
}

if (get_post('update_level') && $selected_level > 0) {
	if (validate_level_form()) {
		$conditions_json = get_post('level_conditions');
		if ($conditions_json != '') {
			$validation = validate_approval_conditions($conditions_json);
			if (is_array($validation) && !empty($validation['valid']) === false) {
				$error_message = _('Invalid conditions.');
				if (!empty($validation['errors']) && is_array($validation['errors'])) {
					$error_message = implode(' ', $validation['errors']);
				}
				display_error($error_message);
				$conditions_json = null;
			} else {
				$normalized_conditions = normalize_approval_conditions_json($conditions_json);
				if ($normalized_conditions !== null) {
					$conditions_json = $normalized_conditions;
				}
			}
		} else {
			$conditions_json = null;
		}

		update_approval_level(
			$selected_level,
			get_post('level_role_id'),
			(int)get_post('level_min_approvers'),
			input_num('level_amount_threshold', 0),
			input_num('level_amount_upper', 0),
			(int)get_post('level_escalation_days'),
			get_post('level_escalation_to') ? (int)get_post('level_escalation_to') : null,
			get_post('level_loc_code') != '' ? get_post('level_loc_code') : null,
			check_value('level_active'),
			$conditions_json
		);
		display_notification(_('Approval level has been updated.'));
		$selected_level = -1;
		$Ajax->activate('_page_body');
	}
}

if ($level_delete_id > 0) {
	delete_approval_level($level_delete_id);
	display_notification(_('Approval level has been deleted.'));
	$selected_level = -1;
	$Ajax->activate('_page_body');
}

if (get_post('cancel_level')) {
	$selected_level = -1;
	$Ajax->activate('_page_body');
}

// =====================================================
// RENDERING — Workflow List
// =====================================================

start_form();

// --- Section 1: Existing Workflows Table ---

display_heading(_('Approval Workflows'));
echo '<br>';

$workflows = get_all_approval_workflows();

start_table(TABLESTYLE, "width='90%'");
$th = array(
	_('Workflow Name'),
	_('Transaction Type'),
	_('Levels'),
	_('Active'),
	_('Req. Comments Reject'),
	_('Req. Comments Approve'),
	_('Allow Edit'),
	_('Self-Approve'),
	'', ''
);
table_header($th);

$k = 0;
while ($row = db_fetch($workflows)) {
	alt_table_row_color($k);
	label_cell($row['name']);
	label_cell(get_trans_type_label($row['trans_type']));
	label_cell($row['level_count'], "align='center'");
	label_cell($row['is_active'] ? _('Yes') : _('No'), "align='center'");
	label_cell($row['require_comments_on_reject'] ? _('Yes') : _('No'), "align='center'");
	label_cell($row['require_comments_on_approve'] ? _('Yes') : _('No'), "align='center'");
	label_cell($row['allow_edit_on_approve'] ? _('Yes') : _('No'), "align='center'");
	label_cell($row['allow_self_approve'] ? _('Yes') : _('No'), "align='center'");
	edit_button_cell('EditWorkflow' . $row['id'], _('Edit'));
	delete_button_cell('DeleteWorkflow' . $row['id'], _('Delete'));
	end_row();
}

end_table(1);

// =====================================================
// RENDERING — Workflow Add/Edit Form
// =====================================================

display_heading($edit_workflow ? _('Edit Approval Workflow') : _('Add New Approval Workflow'));
echo '<br>';

start_table(TABLESTYLE2);

if ($edit_workflow && $selected_workflow > 0) {
	$workflow_row = get_approval_workflow($selected_workflow);
	if ($workflow_row) {
		$_POST['workflow_name'] = $workflow_row['name'];
		$_POST['workflow_description'] = $workflow_row['description'];
		$_POST['workflow_trans_type'] = $workflow_row['trans_type'];
		$_POST['require_comments_reject'] = $workflow_row['require_comments_on_reject'];
		$_POST['require_comments_approve'] = $workflow_row['require_comments_on_approve'];
		$_POST['allow_edit_approve'] = $workflow_row['allow_edit_on_approve'];
		$_POST['allow_self_approve'] = $workflow_row['allow_self_approve'];
		$_POST['workflow_active'] = $workflow_row['is_active'];
	}
	hidden('workflow_id', $selected_workflow);
}

// Transaction type dropdown
if ($edit_workflow && $selected_workflow > 0) {
	// Show the transaction type as label when editing (cannot change)
	label_row(_('Transaction Type:'), get_trans_type_label(get_post('workflow_trans_type')));
	hidden('workflow_trans_type', get_post('workflow_trans_type'));
} else {
	// Available types dropdown for new workflow
	$available_types = get_available_transaction_types_for_workflow();
	if (empty($available_types)) {
		label_row(_('Transaction Type:'), '<b>' . _('All transaction types already have workflows configured.') . '</b>');
	} else {
		echo "<tr><td class='label'>" . _('Transaction Type:') . "</td><td>";
		echo array_selector('workflow_trans_type', get_post('workflow_trans_type'), $available_types);
		echo "</td></tr>\n";
	}
}

text_row(_('Workflow Name:'), 'workflow_name', get_post('workflow_name'), 50, 100);
text_row(_('Description:'), 'workflow_description', get_post('workflow_description'), 60, 255);

if ($edit_workflow) {
	check_row(_('Active:'), 'workflow_active', get_post('workflow_active'));
}

check_row(_('Require Comments on Reject:'), 'require_comments_reject',
	isset($_POST['require_comments_reject']) ? $_POST['require_comments_reject'] : 1);
check_row(_('Require Comments on Approve:'), 'require_comments_approve',
	isset($_POST['require_comments_approve']) ? $_POST['require_comments_approve'] : 0);
check_row(_('Allow Edit on Approve:'), 'allow_edit_approve',
	isset($_POST['allow_edit_approve']) ? $_POST['allow_edit_approve'] : 0);
check_row(_('Allow Self-Approve:'), 'allow_self_approve',
	isset($_POST['allow_self_approve']) ? $_POST['allow_self_approve'] : 0);

end_table(1);

if ($edit_workflow) {
	echo "<div class='form-actions' style='justify-content:center;'>";
	submit('update_workflow', _('Save Workflow'), true, '', 'default');
	submit('cancel_workflow', _('Cancel'), true, _('Cancel Edition'), 'cancel');
	echo "</div>";
} else {
	if (!empty($available_types)) {
		submit_center('add_workflow', _('Add New Workflow'), true, '', 'default');
	}
}

// =====================================================
// RENDERING — Approval Levels for Selected Workflow
// =====================================================

// Show level management for either the selected workflow or the one being edited
$level_workflow_id = 0;
if ($edit_workflow && $selected_workflow > 0) {
	$level_workflow_id = $selected_workflow;
} elseif ($selected_workflow > 0 && !$edit_workflow) {
	$level_workflow_id = $selected_workflow;
}

// Also allow selecting a workflow to manage levels (when not in edit mode)
if (!$edit_workflow) {
	echo '<hr>';
	display_heading(_('Manage Approval Levels'));
	echo '<br>';

	start_table(TABLESTYLE_NOBORDER);
	start_row();

	// Build a simple workflow selector
	$all_workflows = get_all_approval_workflows();
	$workflow_options = array();
	while ($wf_row = db_fetch($all_workflows)) {
		$workflow_options[$wf_row['id']] = $wf_row['name'] . ' (' . get_trans_type_label($wf_row['trans_type']) . ')';
	}

	if (!empty($workflow_options)) {
		echo "<td class='label'>" . _('Select Workflow:') . "</td><td>";
		echo array_selector('workflow_select', $selected_workflow, $workflow_options,
			array('spec_option' => _('-- Select --'), 'spec_id' => -1, 'select_submit' => true));
		echo "</td>";
	} else {
		echo "<td>" . _('No workflows defined yet. Please add a workflow first.') . "</td>";
	}

	end_row();
	end_table();
	echo '<br>';
}

if ($level_workflow_id > 0) {
	$workflow_info = get_approval_workflow($level_workflow_id);
	if ($workflow_info) {
		if ($edit_workflow) {
			echo '<hr>';
			display_heading(sprintf(_('Approval Levels for: %s'), $workflow_info['name']));
			echo '<br>';
		}

		// --- Existing Levels Table ---
		$levels = get_approval_levels($level_workflow_id, false);

		start_table(TABLESTYLE, "width='90%'");
		$th = array(
			_('Level'),
			_('Security Role'),
			_('Min. Approvers'),
			_('Auto-Approve Threshold'),
			_('Amount Upper Bound'),
			_('Escalation Days'),
			_('Location'),
			_('Conditions'),
			_('Active'),
			'', ''
		);
		table_header($th);

		$k = 0;
		$has_levels = false;
		while ($level_row = db_fetch($levels)) {
			$has_levels = true;
			alt_table_row_color($k);
			label_cell($level_row['level'], "align='center'");
			label_cell($level_row['role_name'] ? $level_row['role_name'] : _('N/A'));
			label_cell($level_row['min_approvers'], "align='center'");
			label_cell(number_format2($level_row['amount_threshold'], user_price_dec()), "align='right'");
			label_cell(number_format2($level_row['amount_upper'], user_price_dec()), "align='right'");
			label_cell($level_row['escalation_days'] > 0 ? $level_row['escalation_days'] : '-', "align='center'");
			label_cell($level_row['loc_code'] ? $level_row['loc_code'] : _('All'), "align='center'");
			$conditions_display = '-';
			if (!empty($level_row['conditions'])) {
				$conditions_display = format_conditions_summary($level_row['conditions']);
			}
			label_cell($conditions_display);
			label_cell($level_row['is_active'] ? _('Yes') : _('No'), "align='center'");
			edit_button_cell('EditLevel' . $level_row['id'], _('Edit'));
			delete_button_cell('DeleteLevel' . $level_row['id'], _('Delete'));
			end_row();
		}

		if (!$has_levels) {
			label_row('', _('No approval levels configured for this workflow.'), "colspan='11' align='center'");
		}

		end_table(1);

		// --- Level Add/Edit Form ---
		$editing_level = false;
		if ($selected_level > 0) {
			$level_data = get_approval_level($selected_level);
			if ($level_data) {
				$editing_level = true;
			}
		}

		display_heading($editing_level ? _('Edit Approval Level') : _('Add New Approval Level'));
		echo '<br>';

		start_table(TABLESTYLE2);

		if ($editing_level) {
			$_POST['level_role_id'] = $level_data['role_id'];
			$_POST['level_min_approvers'] = $level_data['min_approvers'];
			$_POST['level_amount_threshold'] = number_format2($level_data['amount_threshold'], user_price_dec());
			$_POST['level_amount_upper'] = number_format2($level_data['amount_upper'], user_price_dec());
			$_POST['level_escalation_days'] = $level_data['escalation_days'];
			$_POST['level_escalation_to'] = $level_data['escalation_to_level'];
			$_POST['level_loc_code'] = $level_data['loc_code'];
			$_POST['level_conditions'] = isset($level_data['conditions']) ? $level_data['conditions'] : '';
			$_POST['level_active'] = $level_data['is_active'];
			hidden('selected_level_id', $selected_level);
		}

		// Security role dropdown
		$roles = get_security_roles_for_approval();
		echo "<tr><td class='label'>" . _('Security Role:') . "</td><td>";
		echo array_selector('level_role_id', get_post('level_role_id'), $roles,
			array('spec_option' => _('-- Select Role --'), 'spec_id' => 0));
		echo "</td></tr>\n";

		// Min approvers
		small_amount_row(_('Min. Approvers Required:'), 'level_min_approvers',
			isset($_POST['level_min_approvers']) ? $_POST['level_min_approvers'] : 1, null, null, 0);

		// Amount threshold
		amount_row(_('Auto-Approve Threshold:'), 'level_amount_threshold',
			isset($_POST['level_amount_threshold']) ? $_POST['level_amount_threshold'] : null);

		// Amount upper bound
		amount_row(_('Amount Upper Bound:'), 'level_amount_upper',
			isset($_POST['level_amount_upper']) ? $_POST['level_amount_upper'] : null);

		// Escalation days
		small_amount_row(_('Escalation Days (0=none):'), 'level_escalation_days',
			isset($_POST['level_escalation_days']) ? $_POST['level_escalation_days'] : 0, null, null, 0);

		// Escalation to level
		$max_level = get_max_approval_level($level_workflow_id);
		$escalation_options = array('' => _('N/A'));
		for ($i = 1; $i <= $max_level + 1; $i++) {
			$escalation_options[$i] = sprintf(_('Level %d'), $i);
		}
		echo "<tr><td class='label'>" . _('Escalate To Level:') . "</td><td>";
		echo array_selector('level_escalation_to', get_post('level_escalation_to'), $escalation_options);
		echo "</td></tr>\n";

		// Location restriction
		locations_list_row(_('Location:'), 'level_loc_code',
			get_post('level_loc_code'), _('All Locations'));

		// Conditional rules (JSON)
		$operators = get_condition_operators();
		$operator_hint = implode(', ', array_keys($operators));
		$fields = get_common_condition_fields();
		$field_hint = implode(', ', array_keys($fields));

		textarea_row(_('Conditions (JSON):'), 'level_conditions',
			get_post('level_conditions', ''), 60, 4);
		label_row('', '<small style="color:#6c757d;">'
			. _('JSON condition object or array. Object: {"field":"...","operator":"...","value":"..."} | Array: [{"field":"...","operator":"...","value":"..."}]')
			. '<br>' . _('Available operators:') . ' ' . htmlspecialchars($operator_hint, ENT_QUOTES, 'UTF-8')
			. '<br>' . _('Common fields:') . ' ' . htmlspecialchars($field_hint, ENT_QUOTES, 'UTF-8')
			. '<br>' . _('Example:') . ' <code>[{"field":"amount","operator":"&gt;","value":"10000"},{"field":"currency","operator":"==","value":"USD"}]</code>'
			. '</small>', "colspan='1'");

		if ($editing_level) {
			check_row(_('Active:'), 'level_active', get_post('level_active'));
		}

		end_table(1);

		hidden('workflow_id', $level_workflow_id);

		if ($editing_level) {
			echo "<div class='form-actions' style='justify-content:center;'>";
			submit('update_level', _('Update Level'), true, '', 'default');
			submit('cancel_level', _('Cancel'), true, _('Cancel Edition'), 'cancel');
			echo "</div>";
		} else {
			submit_center('add_level', _('Add New Level'), true, '', 'default');
		}
	}
}

end_form();
end_page();
