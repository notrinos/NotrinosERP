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
 * CRM Lead Entry - Create / Edit Lead
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_LEAD';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/crm/includes/crm_constants.inc');
include_once($path_to_root . '/crm/includes/db/crm_settings_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_leads_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_activities_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_communication_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_teams_db.inc');
include_once($path_to_root . '/crm/includes/ui/crm_ui.inc');

$js = '';
if (user_use_date_picker())
    $js .= get_js_date_picker();

//--------------------------------------------------------------------------
// Determine mode: New or Edit
//--------------------------------------------------------------------------

$lead_id   = 0;
$is_new    = true;
$lead_data = null;

$raw_lead_id = isset($_GET['LeadID']) ? (int)$_GET['LeadID'] : (int)get_post('LeadID', 0);
if ($raw_lead_id > 0) {
    $lead_id = $raw_lead_id;
    $lead_data = get_crm_lead($lead_id);
    if (!$lead_data) {
        display_error(_('Lead not found.'));
        hyperlink_params('lead_entry.php', _('Create New Lead'), 'sel_app=crm');
        end_page();
        exit;
    }
    $is_new = false;
}

$page_title = $is_new ? _('New Lead') : sprintf(_('Edit Lead #%s'), $lead_data['lead_ref']);
page(_($help_context = 'CRM Lead Entry'), false, false, '', $js);

//--------------------------------------------------------------------------
// Handle Save / Update
//--------------------------------------------------------------------------

if (isset($_POST['Save'])) {

    // Validation
    $input_error = 0;

    if (strlen(trim($_POST['lead_name'])) < 1) {
        display_error(_('Lead name is required.'));
        set_focus('lead_name');
        $input_error = 1;
    }

    if ($_POST['email'] != '' && !preg_match('/^[^@\s]+@[^@\s]+\.[^@\s]+$/', $_POST['email'])) {
        display_error(_('Please enter a valid email address.'));
        set_focus('email');
        $input_error = 1;
    }

    if ($_POST['expected_revenue'] != '' && !is_numeric($_POST['expected_revenue'])) {
        display_error(_('Expected revenue must be numeric.'));
        set_focus('expected_revenue');
        $input_error = 1;
    }

    if ($input_error == 0) {
        $data = array(
            'title'            => $_POST['lead_name'],
            'company_name'     => $_POST['organization'],
            'email'            => $_POST['email'],
            'phone'            => $_POST['phone'],
            'mobile'           => $_POST['mobile'],
            'website'          => $_POST['website'],
            'address'          => $_POST['address'],
            'city'             => $_POST['city'],
            'state'            => $_POST['state'],
            'country'          => $_POST['country'],
            'postal_code'      => $_POST['postal_code'],
            'lead_source_id'   => (int)$_POST['source_id'],
            'lead_status'      => !empty($_POST['status']) ? $_POST['status'] : CRM_LEAD_NEW,
            'priority'         => (int)$_POST['priority'],
            'assigned_to'      => (int)$_POST['assigned_to'],
            'sales_team_id'    => (int)$_POST['team_id'],
            'expected_revenue' => $_POST['expected_revenue'] != '' ? (float)$_POST['expected_revenue'] : 0,
            'notes'            => $_POST['notes'],
        );

        if ($is_new) {
            $data['lead_ref']   = crm_next_lead_ref();
            $data['created_by'] = $_SESSION['wa_current_user']->user;
            $lead_id = add_crm_lead($data);
            display_notification(_('Lead has been created successfully.'));
        } else {
            update_crm_lead($lead_id, $data);
            display_notification(_('Lead has been updated.'));
            $lead_data = get_crm_lead($lead_id); // reload after update
        }

        // Save tags
        if ($lead_id) {
            $tag_ids = array();
            if (!empty($_POST['tags']) && is_array($_POST['tags'])) {
                foreach ($_POST['tags'] as $tid) {
                    $tag_ids[] = (int)$tid;
                }
            }
            update_crm_entity_tags(CRM_ENTITY_LEAD, $lead_id, $tag_ids);
        }

        if ($is_new) {
            meta_forward($_SERVER['PHP_SELF'], 'LeadID=' . $lead_id . crm_sel_app_param());
        }
    }
}

//--------------------------------------------------------------------------
// Handle Delete
//--------------------------------------------------------------------------

if (isset($_POST['Delete']) && !$is_new) {
    begin_transaction();
    delete_crm_lead($lead_id, false); // soft delete
    commit_transaction();
    display_notification(_('Lead has been deleted.'));
    meta_forward($path_to_root . '/crm/manage/leads.php', 'sel_app=crm');
    exit;
}

//--------------------------------------------------------------------------
// Display notification from redirect
//--------------------------------------------------------------------------

if (isset($_SESSION['crm_lead_saved'])) {
    unset($_SESSION['crm_lead_saved']);
    display_notification(_('Lead has been created successfully.'));
}

//--------------------------------------------------------------------------
// Load existing lead data into POST for form
//--------------------------------------------------------------------------

if (!$is_new && !isset($_POST['Save'])) {
    $_POST['lead_name']        = $lead_data['title'];
    $_POST['organization']     = $lead_data['company_name'];
    $_POST['email']            = $lead_data['email'];
    $_POST['phone']            = $lead_data['phone'];
    $_POST['mobile']           = $lead_data['mobile'];
    $_POST['website']          = $lead_data['website'];
    $_POST['address']          = $lead_data['address'];
    $_POST['city']             = $lead_data['city'];
    $_POST['state']            = $lead_data['state'];
    $_POST['country']          = $lead_data['country'];
    $_POST['postal_code']      = $lead_data['postal_code'];
    $_POST['source_id']        = $lead_data['lead_source_id'];
    $_POST['status']           = $lead_data['lead_status'];
    $_POST['priority']         = $lead_data['priority'];
    $_POST['assigned_to']      = $lead_data['assigned_to'];
    $_POST['team_id']          = $lead_data['sales_team_id'];
    $_POST['expected_revenue'] = $lead_data['expected_revenue'];
    $_POST['notes']            = $lead_data['notes'];
}

//--------------------------------------------------------------------------
// Display Form
//--------------------------------------------------------------------------

start_form();

if (!$is_new) {
    hidden('LeadID', $lead_id);
    // Status bar
    echo "<div style='margin-bottom:10px;'>";
    echo crm_status_badge($lead_data['lead_status'], crm_lead_statuses());
    echo " &nbsp; ";
    echo crm_priority_badge($lead_data['priority']);
    if ($lead_data['probability'] > 0) {
        echo " &nbsp; <strong>" . _('Probability') . ":</strong> " . (int)$lead_data['probability'] . '%';
    }
    echo "</div>";
}

// -----------------------------------------------------------------------
// Section grid: 2 columns on wide screens, 1 on narrow
// -----------------------------------------------------------------------
echo "<style>
.crm-form-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 0 24px; align-items: stretch; margin-bottom: 8px; }
@media (max-width: 900px) { .crm-form-grid { grid-template-columns: 1fr; } }
.crm-section-block { min-width: 0; display: flex; flex-direction: column; }
.crm-section-block .form-table { flex: 1; }
</style>";
echo "<div class='crm-form-grid'>";

// -- Basic Information ---------------------------------------------------
echo "<div class='crm-section-block'>";
display_heading(_('Basic Information'));
start_table(TABLESTYLE2);

text_row(_('Lead Name:'), 'lead_name', null, 60, 100);
text_row(_('Organization:'), 'organization', null, 60, 100);
text_row(_('Email:'), 'email', null, 60, 100);
text_row(_('Phone:'), 'phone', null, 30, 30);
text_row(_('Mobile:'), 'mobile', null, 30, 30);
text_row(_('Website:'), 'website', null, 60, 200);

end_table(1);
echo "</div>";

// -- Address -------------------------------------------------------------
echo "<div class='crm-section-block'>";
display_heading(_('Address'));
start_table(TABLESTYLE2);

text_row(_('Address:'), 'address', null, 60, 200);
text_row(_('City:'), 'city', null, 40, 60);
text_row(_('State/Province:'), 'state', null, 40, 60);
text_row(_('Country:'), 'country', null, 40, 60);
text_row(_('Postal Code:'), 'postal_code', null, 15, 15);

end_table(1);
echo "</div>";

// -- Classification ------------------------------------------------------
echo "<div class='crm-section-block'>";
display_heading(_('Classification'));
start_table(TABLESTYLE2);

crm_lead_source_list_row(_('Lead Source:'), 'source_id', null);
crm_lead_status_list_row(_('Status:'), 'status', null);
crm_priority_list_row(_('Priority:'), 'priority', null);
crm_sales_team_list_row(_('Sales Team:'), 'team_id', null, true);

// Assigned To â€“ simple user dropdown
$users_sql = "SELECT id, real_name FROM " . TB_PREF . "users ORDER BY real_name";
$users_result = db_query($users_sql);
$user_options = array(0 => _('-- Unassigned --'));
while ($u = db_fetch($users_result)) {
    $user_options[$u['id']] = $u['real_name'];
}
array_selector_row(_('Assigned To:'), 'assigned_to', null, $user_options);

amount_row(_('Expected Revenue:'), 'expected_revenue', null, null, '', 0);

end_table(1);
echo "</div>";

// -- Tags ----------------------------------------------------------------
echo "<div class='crm-section-block'>";
display_heading(_('Tags'));
start_table(TABLESTYLE2);

$existing_tags = array();
if (!$is_new) {
    $et = get_crm_entity_tags(CRM_ENTITY_LEAD, $lead_id);
    foreach ($et as $t) {
        $existing_tags[] = (int)$t['id'];
    }
}
crm_tag_checkboxes_row(_('Tags:'), 'tags', $existing_tags);

end_table(1);
echo "</div>";

echo "</div>"; // end .crm-form-grid

// -- Notes ---------------------------------------------------------------
display_heading(_('Notes'));
start_table(TABLESTYLE2);

textarea_row(_('Notes:'), 'notes', null, 70, 5);

end_table(1);

// -- Buttons -------------------------------------------------------------
echo "<center>";
if (!$is_new) {
    $convert_url = $path_to_root . '/crm/transactions/convert_lead.php?LeadID=' . $lead_id . crm_sel_app_param();
    echo "<a href='" . $convert_url . "' class='inputsubmit' style='display:inline-flex;align-items:center;gap:6px;padding:6px 14px;font-size:12px;border:1px solid var(--modern-color-primary);border-radius:var(--modern-radius-sm);font-weight:500;text-decoration:none;color:var(--modern-color-primary);background:transparent;margin-right:8px;'>" . _('Convert to Opportunity') . "</a>";
}
submit('Save', $is_new ? _('Create Lead') : _('Update Lead'), true, '', 'default');
if (!$is_new) {
    echo " &nbsp; ";
    submit('Delete', _('Delete Lead'), true, '', 'cancel');
}
echo "</center><br>";

//--------------------------------------------------------------------------
// Activities & Communication Log (only for existing leads)
//--------------------------------------------------------------------------

if (!$is_new) {
    // -- Recent Activities ---------------------------------------------------
    display_heading(_('Activities'));
    start_table(TABLESTYLE, "width='95%'");
    $th = array(_('Date'), _('Type'), _('Subject'), _('Status'), _('Assigned To'));
    table_header($th);

    $activities = get_crm_activities(array(
        'entity_type' => CRM_ENTITY_LEAD,
        'entity_id'   => $lead_id,
    ), 20, 0);
    $k = 0;
    while ($row = db_fetch($activities)) {
        alt_table_row_color($k);
        label_cell(sql2date(substr($row['date_scheduled'], 0, 10)));
        label_cell($row['type_name']);
        label_cell($row['summary']);
        label_cell(crm_activity_status_badge($row['status']));
        label_cell(@$row['assigned_name'] ?: '-');
        end_row();
    }
    end_table(1);

    hyperlink_params('schedule_activity.php', _('+ Schedule Activity'),
        'entity_type=' . CRM_ENTITY_LEAD . '&entity_id=' . $lead_id . crm_sel_app_param());

    echo '<br>';

    // -- Communication Log ---------------------------------------------------
    display_heading(_('Communication Log'));
    start_table(TABLESTYLE, "width='95%'");
    $th = array(_('Date'), _('Type'), _('Direction'), _('Subject'), _('From'));
    table_header($th);

    $comms = get_crm_communications(CRM_ENTITY_LEAD, $lead_id, 20, 0);
    $k = 0;
    while ($row = db_fetch($comms)) {
        alt_table_row_color($k);
        label_cell(sql2date(substr($row['date_time'], 0, 10)));
        label_cell($row['comm_type']);
        label_cell($row['direction']);
        label_cell($row['subject']);
        label_cell($row['created_by_name']);
        end_row();
    }
    end_table(1);
}

end_form();
end_page();

