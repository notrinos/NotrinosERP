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
/**
 * CRM Sales Teams Management
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_TEAM';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/crm/includes/crm_constants.inc');
include_once($path_to_root . '/crm/includes/db/crm_settings_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_teams_db.inc');

page(_($help_context = 'CRM Sales Teams'));

simple_page_mode(false);

//--------------------------------------------------------------------------
// Handle Add/Update team
//--------------------------------------------------------------------------
if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    if (empty(trim($_POST['name']))) {
        display_error(_('Team name cannot be empty.'));
        set_focus('name');
    } else {
        $leader = !empty($_POST['team_leader_id']) ? $_POST['team_leader_id'] : null;
        if ($selected_id != '') {
            update_crm_sales_team($selected_id, $_POST['name'], $leader,
                $_POST['email_alias'], $_POST['invoicing_target'],
                check_value('use_leads'), check_value('active'));
            display_notification(_('Sales team has been updated.'));
        } else {
            $selected_id = add_crm_sales_team($_POST['name'], $leader,
                $_POST['email_alias'], $_POST['invoicing_target'],
                check_value('use_leads'));
            display_notification(_('New sales team has been added.'));
        }
        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    // Check if team is assigned to any leads
    $sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "crm_leads WHERE sales_team_id = " . db_escape($selected_id);
    $res = db_query($sql, "");
    $row = db_fetch($res);
    if ($row['cnt'] > 0) {
        display_error(_('This team has assigned leads and cannot be deleted.'));
    } else {
        delete_crm_sales_team($selected_id);
        display_notification(_('Sales team has been deleted.'));
    }
    $Mode = 'RESET';
}

//--------------------------------------------------------------------------
// Handle Add/Remove team member
//--------------------------------------------------------------------------
if (isset($_POST['AddMember']) && !empty($_POST['new_member_id']) && !empty($_POST['editing_team_id'])) {
    add_crm_team_member($_POST['editing_team_id'], $_POST['new_member_id'],
        (int)$_POST['member_max_leads']);
    display_notification(_('Team member added.'));
}

foreach ($_POST as $key => $val) {
    if (strpos($key, 'RemoveMember_') === 0 && !empty($_POST['editing_team_id'])) {
        $remove_uid = (int)substr($key, 13);
        if ($remove_uid > 0) {
            remove_crm_team_member($_POST['editing_team_id'], $remove_uid);
            display_notification(_('Team member removed.'));
        }
        break;
    }
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['name'] = '';
    $_POST['team_leader_id'] = '';
    $_POST['email_alias'] = '';
    $_POST['invoicing_target'] = '0';
    $_POST['use_leads'] = 1;
    unset($_POST['editing_team_id']);
}

//--------------------------------------------------------------------------
// Team listing
//--------------------------------------------------------------------------

start_form();

start_table(TABLESTYLE, "width='80%'");

$th = array(_('ID'), _('Team Name'), _('Leader'), _('Members'), _('Invoicing Target'), _('Active'), '', '', '');
table_header($th);

$result = get_crm_sales_teams(true);
$k = 0;
while ($myrow = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($myrow['id']);
    label_cell($myrow['name']);
    label_cell($myrow['leader_name'] ?: '-');
    label_cell($myrow['member_count'], "align='center'");
    amount_cell($myrow['invoicing_target']);
    label_cell($myrow['active'] ? _('Yes') : _('No'));
    edit_button_cell('Edit' . $myrow['id'], _('Edit'));
    button_cell('Members' . $myrow['id'], _('Members'), '', ICON_GL);
    delete_button_cell('Delete' . $myrow['id'], _('Delete'));
    end_row();
}
end_table(1);

//--------------------------------------------------------------------------
// Team edit form
//--------------------------------------------------------------------------

start_table(TABLESTYLE2);

if ($selected_id != '') {
    if ($Mode == 'Edit') {
        $myrow = get_crm_sales_team($selected_id);
        $_POST['name'] = $myrow['name'];
        $_POST['team_leader_id'] = $myrow['team_leader_id'];
        $_POST['email_alias'] = $myrow['email_alias'];
        $_POST['invoicing_target'] = $myrow['invoicing_target'];
        $_POST['use_leads'] = $myrow['use_leads'];
        $_POST['active'] = $myrow['active'];
    }
    hidden('selected_id', $selected_id);
}

text_row_ex(_('Team Name:'), 'name', 40, 100);

// User selector for team leader
$user_sql = "SELECT id, real_name FROM " . TB_PREF . "users WHERE inactive = 0";
label_row(_('Team Leader:'), combo_input('team_leader_id', get_post('team_leader_id'), $user_sql,
    'id', 'real_name', array('spec_option' => _('-- None --'), 'spec_id' => '')));

text_row_ex(_('Email Alias:'), 'email_alias', 40, 100);
amount_row(_('Invoicing Target:'), 'invoicing_target');
check_row(_('Use Leads:'), 'use_leads', null);

if ($selected_id != '') {
    check_row(_('Active:'), 'active', null);
}

end_table(1);

submit_add_or_update_center($selected_id == '', '', 'both');

//--------------------------------------------------------------------------
// Team members section (shown when "Members" button clicked)
//--------------------------------------------------------------------------

// Detect Members button click
foreach ($_POST as $key => $val) {
    if (strpos($key, 'Members') === 0) {
        $_POST['editing_team_id'] = substr($key, 7);
        $Ajax->activate('_page_body');
        break;
    }
}

if (!empty($_POST['editing_team_id'])) {
    $team_id = $_POST['editing_team_id'];
    $team = get_crm_sales_team($team_id);

    echo "<br>";
    display_heading(sprintf(_('Members of Team: %s'), $team['name']));

    hidden('editing_team_id', $team_id);

    start_table(TABLESTYLE, "width='60%'");
    $th = array(_('User'), _('Max Leads/30d'), _('Skip Auto-Assign'), '');
    table_header($th);

    $members = get_crm_team_members($team_id);
    $k = 0;
    while ($member = db_fetch($members)) {
        alt_table_row_color($k);
        label_cell($member['real_name']);
        label_cell($member['max_leads_30days'], "align='center'");
        label_cell($member['skip_auto_assign'] ? _('Yes') : _('No'), "align='center'");
        echo "<td>";
        submit('RemoveMember_' . $member['user_id'], _('Remove'), true, '', 'default');
        echo "</td>";
        end_row();
    }
    end_table(1);

    // Add member form
    start_table(TABLESTYLE2);
    label_row(_('Add User:'), combo_input('new_member_id', '', $user_sql,
        'id', 'real_name', array('spec_option' => _('-- Select User --'), 'spec_id' => '')));
    text_row_ex(_('Max Leads (30 days):'), 'member_max_leads', 10, 10, null, '30');
    end_table(1);
    submit_center('AddMember', _('Add Member'), true, '', 'default');
}

end_form();
end_page();

