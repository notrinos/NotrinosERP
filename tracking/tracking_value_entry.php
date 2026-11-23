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
$page_security = 'SA_DIMENSION';
$path_to_root = '..';
include_once($path_to_root.'/includes/session.inc');

include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');

include_once($path_to_root.'/admin/db/tags_db.inc');
include_once($path_to_root.'/tracking/includes/tracking_values_db.inc');
include_once($path_to_root.'/tracking/includes/tracking_values_ui.inc');
include_once($path_to_root.'/tracking/includes/tracking_inquiry.inc');
include_once($path_to_root.'/tracking/includes/tracking_category_policy.inc');
include_once($path_to_root.'/tracking/includes/tracking_business_mode.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();
$help_context = 'Dimension Entry';
page(_('New Tracking Value'), false, false, '', $js);

//---------------------------------------------------------------------------------------

if (isset($_GET['trans_no']))
	$selected_id = $_GET['trans_no'];
elseif(isset($_POST['selected_id']))
	$selected_id = $_POST['selected_id'];
else
	$selected_id = -1;

// DIM-LFC-002: no new Dimension may be entered while the company enables no posting axis. Historical
// masters remain reachable for authorized viewing/exit, but this page does not create a disabled-axis event.
// The completion redirects (Added/Updated/Deleted/Closed/Reopened) also carry no trans_no, so they
// must reach their confirmation blocks below instead of being replaced by this error.
if ($selected_id == -1 && (int)get_company_pref('use_dimension') < 1
	&& !isset($_GET['AddedID']) && !isset($_GET['UpdatedID']) && !isset($_GET['DeletedID'])
	&& !isset($_GET['ClosedID']) && !isset($_GET['ReopenedID'])) {
	display_error(_('No tracking category is enabled for new entries. Historical tracking value inquiry and reports remain available.'));
	// DIM-UX-001: say in plain words what is switched off and what to do, not only that it is.
	display_dimension_failure_state('not_enabled');
	hyperlink_no_params($path_to_root.'/tracking/getting_started.php', _('Open the getting started guide'));
	display_footer_exit();
}

//---------------------------------------------------------------------------------------

if (isset($_GET['AddedID'])) {
	$id = $_GET['AddedID'];
	display_notification_centered(_('The tracking value has been entered.'));
	safe_exit();
}

if (isset($_GET['UpdatedID'])) {
	$id = $_GET['UpdatedID'];
	display_notification_centered(_('The tracking value has been updated.'));
	safe_exit();
}

if (isset($_GET['DeletedID'])) {
	$id = $_GET['DeletedID'];
	display_notification_centered(_('The tracking value has been deleted.'));
	safe_exit();
}

if (isset($_GET['ClosedID'])) {
	$id = $_GET['ClosedID'];

	// DIM-REP-004: closing stops new postings only; history and reports are unchanged and the dimension can be re-opened.
	display_notification_centered(_('The tracking value has been closed. No new posting accepts it; its history and reports are unchanged and it can be re-opened.').' #'.(int)$id);
	safe_exit();
}

if (isset($_GET['ReopenedID'])) {
	$id = $_GET['ReopenedID'];

	display_notification_centered(_('The tracking value has been re-opened and accepts new postings again.').' #'.(int)$id);
	safe_exit();
}

//-------------------------------------------------------------------------------------------------

function safe_exit() {
	global $path_to_root, $id;

	hyperlink_no_params('', _('Enter a &new tracking value'));
	hyperlink_no_params($path_to_root.'/tracking/inquiry/search_tracking_values.php', _('&Select an existing tracking value'));
	// DIM-REP-004: the attachment link carries the real identity (the literal text "$id" was sent before) and is
	// offered only for a dimension that still exists.
	if (isset($_GET['DeletedID']) === false && (int)$id > 0)
		hyperlink_no_params($path_to_root.'/admin/attachments.php?filterType='.ST_DIMENSION.'&trans_no='.(int)$id, _('&Add Attachment'));

	display_footer_exit();
}

/** Names of the owners that denied the latest lifecycle decision (never any data). */
function dimension_denial_detail() {
	$denial = dimension_lifecycle_last_denial();
	if (!count($denial['owners']))
		return '';
	return ' ('._('Blocked by').': '.htmlspecialchars(implode(', ', array_keys($denial['owners'])), ENT_QUOTES, 'UTF-8').')';
}

function can_process() {
	global $selected_id, $Refs;

	if ($selected_id == -1) {
		if (!check_reference($_POST['ref'], ST_DIMENSION)) {
			set_focus('ref');
			return false;
		}
	}
	// DIM-LFC-002: a new master (or an axis change on an existing one) must use an axis that is enabled
	// for new entries; an unchanged historical axis on an existing master is never rejected here.
	$type_requested = isset($_POST['type_']) ? $_POST['type_'] : null;
	$stored_type = null;
	if ($selected_id != -1) {
		$stored = get_dimension($selected_id, true);
		$stored_type = is_array($stored) ? (int)$stored['type_'] : null;
	}
	if (!in_array($type_requested, array(1, 2, '1', '2'), true)
		|| (($stored_type === null || (int)$type_requested !== $stored_type) && !dimension_axis_open_for_new_entry($type_requested))) {
		display_error(_('The selected tracking category is not enabled for new entries.'));
		set_focus('type_');
		return false;
	}
	// DIM-REP-004: one shared rule judges the name/reference length in characters (the canonical column length, which the
	// form also uses as its maximum), and a required-by date before the start date is refused.
	$start_sql = is_date($_POST['date_']) ? date2sql($_POST['date_']) : null;
	$due_sql = is_date($_POST['due_date']) ? date2sql($_POST['due_date']) : null;
	$codes = dimension_inquiry_validate_master($selected_id == -1 ? $_POST['ref'] : '', $_POST['name'], $start_sql, $due_sql);
	if (count($codes)) {
		$focus = array('reference_too_long'=>'ref', 'name_required'=>'name', 'name_too_long'=>'name', 'start_invalid'=>'date_', 'due_invalid'=>'due_date', 'due_before_start'=>'due_date');
		foreach ($codes as $code)
			display_error(dimension_inquiry_reason_text($code));
		set_focus($focus[$codes[0]]);
		return false;
	}

	return true;
}

//-------------------------------------------------------------------------------------

if (isset($_POST['ADD_ITEM']) || isset($_POST['UPDATE_ITEM'])) {
	if (!isset($_POST['dimension_tags']))
		$_POST['dimension_tags'] = array();
		
	if (can_process()) {

		if ($selected_id == -1) {
			$id = add_dimension($_POST['ref'], $_POST['name'], $_POST['type_'], $_POST['date_'], $_POST['due_date'], $_POST['memo_']);
			if (!$id)
				display_error(_('The tracking value could not be entered. Its identity is still retained by an earlier tracking value.'));
			else {
				add_tag_associations($id, $_POST['dimension_tags']);
				meta_forward($_SERVER['PHP_SELF'], 'AddedID='.$id);
			}
		} 
		else {
			if (!update_dimension($selected_id, $_POST['name'], $_POST['type_'], $_POST['date_'], $_POST['due_date'], $_POST['memo_'])) {
				display_error(_('The tracking value could not be updated. The axis can only be reassigned while no record, draft, approval or extension references the tracking value.').dimension_denial_detail());
			} else {
				update_tag_associations(TAG_DIMENSION, $selected_id, $_POST['dimension_tags']);
				meta_forward($_SERVER['PHP_SELF'], 'UpdatedID='.$selected_id);
			}
		}
	}
}

//--------------------------------------------------------------------------------------

if (isset($_POST['delete'])) {

	$cancel_delete = false;

	// can't delete it there are productions or issues
	if (dimension_has_payments($selected_id) || dimension_has_deposits($selected_id)) {
		display_error(_('This tracking value cannot be deleted because it has already been processed.'));
		set_focus('ref');
		$cancel_delete = true;
	}
	if ($cancel_delete == false) { //ie not cancelled the delete as a result of above tests
		if (delete_dimension($selected_id))
			meta_forward($_SERVER['PHP_SELF'], 'DeletedID='.$selected_id);
		else
			display_error(_('This tracking value cannot be deleted while any record, draft, approval, attachment, tag or comment references it, or while ownership is unresolved. Close it to retain its identity and history.').dimension_denial_detail());
	}
}

//-------------------------------------------------------------------------------------

if (isset($_POST['close'])) {
	// update the closed flag
	if (close_dimension($selected_id))
		meta_forward($_SERVER['PHP_SELF'], 'ClosedID='.$selected_id);
	else
		display_error(_('The tracking value could not be closed.'));
}

if (isset($_POST['reopen'])) {
	// update the closed flag
	if (reopen_dimension($selected_id))
		meta_forward($_SERVER['PHP_SELF'], 'ReopenedID='.$selected_id);
	else
		display_error(_('The tracking value could not be reopened.'));
}

//-------------------------------------------------------------------------------------

start_form();

start_table(TABLESTYLE2);

if ($selected_id != -1) {
	$myrow = get_dimension($selected_id, true);

	if ($myrow === false) {
		display_error(_('The tracking value sent is not valid.'));
		display_footer_exit();
	}

	$_POST['ref'] = $myrow['reference'];
	$_POST['closed'] = $myrow['closed'];
	$_POST['name'] = $myrow['name'];
	$_POST['type_'] = $myrow['type_'];
	$_POST['date_'] = sql2date($myrow['date_']);
	$_POST['due_date'] = sql2date($myrow['due_date']);
	$_POST['memo_'] = get_comments_string(ST_DIMENSION, $selected_id);
	
	$tags_result = get_tags_associated_with_record(TAG_DIMENSION, $selected_id);
	$tagids = array();

	while ($tag = db_fetch($tags_result)) {
		$tagids[] = $tag['id'];
	}

	$_POST['dimension_tags'] = $tagids;	

	hidden('ref', $_POST['ref']);
	label_row(_('Tracking Value Reference:'), $_POST['ref']);
	hidden('selected_id', $selected_id);
} 
else {
	$_POST['dimension_tags'] = array();
	ref_row(_('Tracking Value Reference:'), 'ref', '', $Refs->get_next(ST_DIMENSION), false, ST_DIMENSION);
}

text_row_ex(_('Name').':', 'name', 50, dimension_inquiry_name_limit());
$dim = (int)get_company_pref('use_dimension');
// DIM-LFC-002: a disabled axis preference governs new events only. An existing historical master keeps
// displaying (and resubmitting) its stored axis instead of being silently reassigned by a shortened list.
if ($selected_id != -1 && (int)$myrow['type_'] > $dim) {
	label_row(_('Type').':', (int)$myrow['type_']);
	hidden('type_', (int)$myrow['type_']);
}
else {
	// DIM-CFG-001: an optional published category policy only supplies the display name of each
	// enabled category; the stored value is still the native axis 1..N and never a semantic key.
	$axis_choices = dimension_category_policy_axis_choices($dim);
	if (count($axis_choices) > 0)
		array_selector_row(_('Type').':', 'type_', null, $axis_choices, array('select_submit' => false, 'async' => false));
	else
		number_list_row(_('Type').':', 'type_', null, 1, max(1, $dim));
}
date_row(_('Start Date').':', 'date_');
// DIM-UX-001: the date is a deadline for a project and only a review date for an ongoing cost center; the caption follows the
// category of an existing master, or the shared caption of every category the form can create.
$deadline_axes = ($selected_id != -1) ? array((int)$myrow['type_']) : range(1, max(1, $dim));
date_row(dimension_business_mode_deadline_caption($deadline_axes).':', 'due_date', '', null, $SysPrefs->default_dimension_required_by());
tag_list_row(_('Tags:'), 'dimension_tags', 5, TAG_DIMENSION, true);
textarea_row(_('Memo:'), 'memo_', null, 40, 5);

end_table(1);

// DIM-UX-001: what each category is for and how its date behaves, in the words the company chose.
if ($selected_id != -1 && (int)$myrow['type_'] > $dim)
	display_dimension_failure_state('history_only');
else {
	foreach (dimension_business_mode_entry_guide($dim) as $guide_line)
		display_note($guide_line);
	$hrm_note = dimension_business_mode_hrm_note(1);
	if ($hrm_note !== '' && $dim >= 1)
		display_note($hrm_note);
}

if (isset($_POST['closed']) && $_POST['closed'] == 1)
	display_note(dimension_inquiry_closed_text(), 0, 0, "class='currentfg'");

if ($selected_id != -1) {
	echo '<br>';
	submit_center_first('UPDATE_ITEM', _('Update'), _('Save changes to tracking value'), 'default');
	if ($_POST['closed'] == 1)
		submit('reopen', _('Re-open This Tracking Value'), true, _('Mark this tracking value as re-opened'), true);
	else	
		submit('close', _('Close This Tracking Value'), true, _('Mark this tracking value as closed'), true);
	submit_center_last('delete', _('Delete This Tracking Value'), _('Delete unused tracking value'), true);
}
else
	submit_center('ADD_ITEM', _('Add'), true, '', 'default');

end_form();

end_page();
