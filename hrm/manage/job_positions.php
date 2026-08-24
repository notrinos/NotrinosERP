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

$page_security = 'SA_POSITION';
$path_to_root  = '../..';

include_once($path_to_root.'/includes/session.inc');

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/job_positions_entity.inc');
include_once($path_to_root.'/hrm/includes/db/job_classes_entity.inc');
include_once($path_to_root.'/hrm/includes/db/job_db.inc');

//--------------------------------------------------------------------------

page(_($help_context = 'Manage Job Positions'));

if(!job_classes_entity::has_records()) {
	display_error(_('No Job Class found in the system, please define Job Classes first.'));
	display_footer_exit();
}
if (!hrm_position_job_column_ready() || !hrm_job_table_ready()
    || !hrm_position_hierarchy_table_ready() || get_hrm_position_hierarchy_writer_activation_date() === false
    || !hrm_position_headcount_table_ready() || get_hrm_position_headcount_writer_activation_date() === false) {
    display_error(_('The normalized Position Job/hierarchy/headcount writers are not installed. Run the normal software upgrade first.'));
    display_footer_exit();
}
$legal_entity_id = get_hrm_default_legal_entity_binding_id(false);
if ($legal_entity_id === false) {
    display_error(_('The default HRM Legal Entity is missing or inconsistent. Position maintenance is blocked.'));
    display_footer_exit();
}
$job_options = array(0=>_('Unassigned'));
$job_labels = array(0=>_('Unassigned'));
$jobs = get_hrm_jobs_for_legal_entity((int)$legal_entity_id, false);
if ($jobs) {
    while ($job = db_fetch_assoc($jobs)) {
        if (!hrm_job_row_owned_by_admin_writer($job, (int)$legal_entity_id))
            continue;
        $label = $job['job_code'].' - '.$job['job_name'];
        if ($job['job_status'] !== 'active')
            $label .= ' '._('(Inactive)');
        $job_labels[(int)$job['job_id']] = $label;
        if ($job['job_status'] === 'active')
            $job_options[(int)$job['job_id']] = $label;
    }
}

$hierarchy_options = array(0=>_('Top level / Unassigned'));
$hierarchy_labels = array(0=>_('Top level / Unassigned'));
$hierarchy_positions = get_hrm_positions_for_hierarchy_selector((int)$legal_entity_id, 0);
if ($hierarchy_positions) {
    while ($position = db_fetch_assoc($hierarchy_positions)) {
        $label = $position['position_code'].' - '.$position['position_name'];
        if ($position['position_status'] !== 'active')
            $label .= ' '._('(Inactive)');
        $hierarchy_labels[(int)$position['position_id']] = $label;
        if ($position['position_status'] === 'active')
            $hierarchy_options[(int)$position['position_id']] = $label;
    }
}
$hierarchy_today_sql = date2sql(Today());
$headcount_today_sql = $hierarchy_today_sql;

simple_page_mode(false);

if ($Mode=='ADD_ITEM' || $Mode=='UPDATE_ITEM') {

    $job_id = normalize_hrm_position_job_target(get_post('job_id', '0'));
    $reports_to_position_id = normalize_hrm_position_hierarchy_target(get_post('reports_to_position_id', '0'));
    $hierarchy_effective_from = get_post('hierarchy_effective_from', Today());
    $hierarchy_date_sql = is_date($hierarchy_effective_from) ? date2sql($hierarchy_effective_from) : false;
    $original_parent = normalize_hrm_position_hierarchy_target(get_post('hierarchy_original_parent_id', '0'));
    $hierarchy_explicit = $selected_id == ''
        ? $reports_to_position_id !== null
        : ($original_parent !== false && $reports_to_position_id !== $original_parent);
    $headcount_raw = trim((string)get_post('position_budgeted_headcount', ''));
    $budgeted_headcount = normalize_hrm_position_headcount_target($headcount_raw);
    $headcount_effective_from = get_post('headcount_effective_from', Today());
    $headcount_date_sql = is_date($headcount_effective_from) ? date2sql($headcount_effective_from) : false;
    $original_headcount_raw = trim((string)get_post('headcount_original_budget', ''));
    $original_headcount = normalize_hrm_position_headcount_target($original_headcount_raw);
    $headcount_explicit = $headcount_raw !== '' && ($selected_id == ''
        || $original_headcount === null || $original_headcount === false
        || $budgeted_headcount !== $original_headcount);
	if(empty(trim($_POST['position_name']))) {
		display_error(_('Position name cannot be empty.'));
		set_focus('position_name');
	}
    elseif($job_id === false) {
        display_error(_('The selected Job is invalid.'));
        set_focus('job_id');
    }
    elseif($reports_to_position_id === false) {
        display_error(_('The selected reporting Position is invalid.'));
        set_focus('reports_to_position_id');
    }
    elseif($hierarchy_explicit && $hierarchy_date_sql === false) {
        display_error(_('The hierarchy effective date is invalid.'));
        set_focus('hierarchy_effective_from');
    }
    elseif($headcount_raw !== '' && $budgeted_headcount === false) {
        display_error(_('Budgeted headcount must be a whole number of seats greater than or equal to zero.'));
        set_focus('position_budgeted_headcount');
    }
    elseif($headcount_explicit && $headcount_date_sql === false) {
        display_error(_('The headcount effective date is invalid.'));
        set_focus('headcount_effective_from');
    }
	elseif(!check_num('basic_amount', 0)) {
		display_error(_('Amount field value must be a positive number.'));
		set_focus('basic_amount');
	}
	else {

		if ($selected_id != '') {
			if (job_positions_entity::modify($selected_id, array(
				'position_name' => $_POST['position_name'],
				'basic_amount' => input_num('basic_amount'),
				'job_class_id' => $_POST['job_class_id']
			), $job_id, true, $reports_to_position_id, $hierarchy_date_sql, $hierarchy_explicit,
                $budgeted_headcount, $headcount_date_sql, $headcount_explicit))
				display_notification(_('Selected job position has been updated'));
			else
				display_error(_('The Position could not be synchronized. No changes were committed.'));
		}
		else {
			if (job_positions_entity::create(array(
				'position_name' => $_POST['position_name'],
				'basic_amount' => input_num('basic_amount'),
				'job_class_id' => $_POST['job_class_id']
			), $job_id, true, $reports_to_position_id, $hierarchy_date_sql, $hierarchy_explicit,
                $budgeted_headcount, $headcount_date_sql, $headcount_explicit))
				display_notification(_('New job position has been added'));
			else
				display_error(_('The Position could not be synchronized. No changes were committed.'));
		}
		
		$Mode = 'RESET';
	}
}

if ($Mode == 'Delete') {

	if(key_in_foreign_table($selected_id, 'employees', 'position_id'))
		display_error(_('The Position cannot be deleted.'));
	else {
		if (job_positions_entity::remove($selected_id))
			display_notification(_('Selected job position has been deleted'));
		else
			display_error(_('Normalized Positions cannot be physically deleted. Inactivate the Position instead.'));
	}
	$Mode = 'RESET';
}

if($Mode == 'RESET') {
	$selected_id = '';
	$_POST['selected_id'] = '';
	$_POST['position_name'] = '';
	$_POST['basic_amount'] = '';
    $_POST['job_id'] = '0';
    $_POST['reports_to_position_id'] = '0';
    $_POST['hierarchy_original_parent_id'] = '0';
    $_POST['hierarchy_effective_from'] = Today();
    $_POST['position_budgeted_headcount'] = '';
    $_POST['headcount_original_budget'] = '';
    $_POST['headcount_effective_from'] = Today();
}

//--------------------------------------------------------------------------

start_form();

start_table(TABLESTYLE);

$th = array(_('Id'), _('Position Name'), _('Salary Basic Amount'), _('Class'), _('Job'), _('Reports To'), _('Budgeted Seats'), _('Occupied Seats'), '', '');

inactive_control_column($th);
table_header($th);

$result = job_positions_entity::all_db_resource(check_value('show_inactive') ? '' : '!inactive');

$k = 0;
while ($myrow = db_fetch($result)) {
	$job_class = job_classes_entity::find($myrow['job_class_id']);
	$class_name = $job_class ? $job_class['class_name'] : '';
	alt_table_row_color($k);
	label_cell($myrow['position_id']);
	label_cell($myrow['position_name']);
	amount_cell($myrow['basic_amount']);
	label_cell($class_name);
    $normalized_position = get_hrm_position_by_legacy_position((int)$myrow['position_id'], false);
    $bound_job_id = is_array($normalized_position) && isset($normalized_position['job_id']) && $normalized_position['job_id'] !== null
        ? (int)$normalized_position['job_id'] : 0;
    label_cell(isset($job_labels[$bound_job_id]) ? $job_labels[$bound_job_id] : _('Invalid/out-of-scope'));
    $hierarchy = is_array($normalized_position)
        ? get_hrm_position_hierarchy_as_of((int)$normalized_position['position_id'], $hierarchy_today_sql, false) : false;
    $parent_id = is_array($hierarchy) ? (int)$hierarchy['reports_to_position_id'] : 0;
    label_cell(isset($hierarchy_labels[$parent_id]) ? $hierarchy_labels[$parent_id] : _('Invalid/out-of-scope'));
    $headcount_snapshot = is_array($normalized_position)
        ? get_hrm_position_headcount_snapshot_as_of((int)$normalized_position['position_id'], $headcount_today_sql) : false;
    label_cell(is_array($headcount_snapshot) && $headcount_snapshot['budgeted_headcount'] !== null
        ? (string)(int)$headcount_snapshot['budgeted_headcount'] : _('Unknown'));
    label_cell(is_array($headcount_snapshot) && $headcount_snapshot['occupied_seats'] !== null
        ? (string)(int)$headcount_snapshot['occupied_seats'] : _('Unknown'));
	hrm_job_position_inactive_control_cell($myrow['position_id'], $myrow['inactive']);
	edit_button_cell('Edit'.$myrow['position_id'], _('Edit'));
	delete_button_cell('Delete'.$myrow['position_id'], _('Delete'));
	end_row();
}
inactive_control_row($th);
end_table(1);

start_table(TABLESTYLE2);

if($selected_id != '') {
	
	if($Mode == 'Edit') {
		
		$myrow = job_positions_entity::find($selected_id);
		$_POST['position_name']  = $myrow['position_name'];
		$_POST['basic_amount'] = price_format($myrow['basic_amount']);
		$_POST['job_class_id'] = $myrow['job_class_id'];
        $normalized_position = get_hrm_position_by_legacy_position((int)$selected_id, false);
        $_POST['job_id'] = is_array($normalized_position) && isset($normalized_position['job_id']) && $normalized_position['job_id'] !== null
            ? (string)(int)$normalized_position['job_id'] : '0';
        $current_job_id = (int)$_POST['job_id'];
        if ($current_job_id > 0 && isset($job_labels[$current_job_id]) && !isset($job_options[$current_job_id]))
            $job_options[$current_job_id] = $job_labels[$current_job_id];
        $hierarchy = is_array($normalized_position)
            ? get_hrm_position_hierarchy_as_of((int)$normalized_position['position_id'], $hierarchy_today_sql, false) : false;
        $current_parent_id = is_array($hierarchy) ? (int)$hierarchy['reports_to_position_id'] : 0;
        $_POST['reports_to_position_id'] = (string)$current_parent_id;
        $_POST['hierarchy_original_parent_id'] = (string)$current_parent_id;
        $_POST['hierarchy_effective_from'] = Today();
        if ($current_parent_id > 0 && isset($hierarchy_labels[$current_parent_id]) && !isset($hierarchy_options[$current_parent_id]))
            $hierarchy_options[$current_parent_id] = $hierarchy_labels[$current_parent_id];
        if (is_array($normalized_position))
            unset($hierarchy_options[(int)$normalized_position['position_id']]);
        $headcount = is_array($normalized_position)
            ? get_hrm_position_headcount_budget_as_of((int)$normalized_position['position_id'], $headcount_today_sql, false) : false;
        $current_headcount = is_array($headcount) ? (string)(int)$headcount['budgeted_headcount'] : '';
        $_POST['position_budgeted_headcount'] = $current_headcount;
        $_POST['headcount_original_budget'] = $current_headcount;
        $_POST['headcount_effective_from'] = Today();
	}
	hidden('selected_id', $selected_id);
    hidden('hierarchy_original_parent_id', get_post('hierarchy_original_parent_id', '0'));
    hidden('headcount_original_budget', get_post('headcount_original_budget', ''));
}

if (!isset($_POST['reports_to_position_id']))
    $_POST['reports_to_position_id'] = '0';
if (!isset($_POST['hierarchy_effective_from']))
    $_POST['hierarchy_effective_from'] = Today();
if (!isset($_POST['position_budgeted_headcount']))
    $_POST['position_budgeted_headcount'] = '';
if (!isset($_POST['headcount_effective_from']))
    $_POST['headcount_effective_from'] = Today();

text_row_ex(_('Position Name:'), 'position_name', 50, 60);
amount_row(_('Salary Basic Amount:'), 'basic_amount', null, null, null, null, true);
job_classes_list_row(_('Job Class:'), 'job_class_id');
label_row(_('Job:'), array_selector('job_id', get_post('job_id', '0'), $job_options));
label_row(_('Reports To:'), array_selector('reports_to_position_id', get_post('reports_to_position_id', '0'), $hierarchy_options));
date_row(_('Hierarchy Effective From:'), 'hierarchy_effective_from');
text_row_ex(_('Budgeted Headcount:'), 'position_budgeted_headcount', 10, 10);
date_row(_('Headcount Effective From:'), 'headcount_effective_from');
label_row(_('Occupancy / FTE:'), _('Occupied Seats is derived read-only from exact Assignment intervals; FTE is unknown/not modeled.'));

end_table(1);

submit_add_or_update_center($selected_id == '', '', 'both');

end_form();
end_page();
