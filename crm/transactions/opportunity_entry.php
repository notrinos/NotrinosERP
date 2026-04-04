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
 * CRM Opportunity Entry - Create / Edit Opportunity
 *
 * Opportunities are leads promoted via convert_lead.php or created directly.
 * They track the sales pipeline with stages, probabilities and expected close dates.
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_OPPORTUNITY';
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
// Determine mode
//--------------------------------------------------------------------------

$lead_id = 0;
$is_new  = true;
$opp     = null;

$raw_lead_id = isset($_GET['LeadID']) ? (int)$_GET['LeadID'] : (int)get_post('LeadID', 0);
if ($raw_lead_id > 0) {
    $lead_id = $raw_lead_id;
    $opp = get_crm_lead($lead_id);
    if (!$opp || !$opp['is_opportunity']) {
        display_error(_('Opportunity not found.'));
        hyperlink_params($path_to_root . '/crm/manage/opportunities.php', _('Back to Opportunities'), 'sel_app=crm');
        end_page();
        exit;
    }
    $is_new = false;
}

$page_title = $is_new ? _('New Opportunity') : sprintf(_('Edit Opportunity #%s'), $opp['lead_ref']);
page(_($help_context = 'CRM Opportunity Entry'), false, false, '', $js);

//--------------------------------------------------------------------------
// Handle Won / Lost actions
//--------------------------------------------------------------------------

if (isset($_POST['MarkWon']) && !$is_new) {
    $customer_id = (int)$_POST['won_customer_id'];
    if ($customer_id <= 0) {
        display_error(_('Please select a customer for the won opportunity.'));
    } else {
        begin_transaction();
        mark_opportunity_won($lead_id, $customer_id);
        commit_transaction();
        display_notification(_('Opportunity marked as WON!'));
        $opp = get_crm_lead($lead_id);
    }
}

if (isset($_POST['MarkLost']) && !$is_new) {
    $reason_id = (int)$_POST['lost_reason_id'];
    begin_transaction();
    mark_opportunity_lost($lead_id, $reason_id, $_POST['lost_notes']);
    commit_transaction();
    display_notification(_('Opportunity marked as LOST.'));
    $opp = get_crm_lead($lead_id);
}

//--------------------------------------------------------------------------
// Handle Save
//--------------------------------------------------------------------------

if (isset($_POST['Save'])) {
    $input_error = 0;

    if (strlen(trim($_POST['lead_name'])) < 1) {
        display_error(_('Opportunity name is required.'));
        set_focus('lead_name');
        $input_error = 1;
    }

    if ($_POST['expected_revenue'] != '' && !check_num('expected_revenue', 0)) {
        display_error(_('Expected revenue must be numeric.'));
        set_focus('expected_revenue');
        $input_error = 1;
    }

    if ($input_error == 0) {
        $data = array(
            'title'              => $_POST['lead_name'],
            'company_name'       => $_POST['organization'],
            'email'              => $_POST['email'],
            'phone'              => $_POST['phone'],
            'mobile'             => $_POST['mobile'],
            'website'            => $_POST['website'],
            'address'            => $_POST['address'],
            'city'               => $_POST['city'],
            'state'              => $_POST['state'],
            'country'            => $_POST['country'],
            'postal_code'        => $_POST['postal_code'],
            'lead_source_id'     => (int)$_POST['source_id'],
            'stage_id'           => (int)$_POST['stage_id'],
            'priority'           => (int)$_POST['priority'],
            'assigned_to'        => (int)$_POST['assigned_to'],
            'sales_team_id'      => (int)$_POST['team_id'],
            'expected_revenue'   => input_num('expected_revenue', 0),
            'expected_close_date'=> $_POST['expected_close'] ? date2sql($_POST['expected_close']) : null,
            'linked_customer_id' => (int)$_POST['customer_id'],
            'notes'              => $_POST['notes'],
            'is_opportunity'     => 1,
        );

        if ($is_new) {
            $data['lead_ref']   = crm_next_opportunity_ref();
            $data['created_by'] = $_SESSION['wa_current_user']->user;
            $data['lead_status']= CRM_LEAD_QUALIFIED;
            $lead_id = add_crm_lead($data);

            // Update stage probability
            if ($data['stage_id'] > 0) {
                update_opportunity_stage($lead_id, $data['stage_id']);
            }
            display_notification(_('Opportunity has been created.'));
        } else {
            update_crm_lead($lead_id, $data);

            // Update stage
            if ($data['stage_id'] > 0 && $data['stage_id'] != $opp['stage_id']) {
                update_opportunity_stage($lead_id, $data['stage_id']);
            }
            display_notification(_('Opportunity has been updated.'));
        }

        // Tags
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
        // Reload data
        $opp = get_crm_lead($lead_id);
    }
}

//--------------------------------------------------------------------------
// Load data into POST
//--------------------------------------------------------------------------

if (!$is_new && !isset($_POST['Save'])) {
    $_POST['lead_name']        = $opp['title'];
    $_POST['organization']     = $opp['company_name'];
    $_POST['email']            = $opp['email'];
    $_POST['phone']            = $opp['phone'];
    $_POST['mobile']           = $opp['mobile'];
    $_POST['website']          = $opp['website'];
    $_POST['address']          = $opp['address'];
    $_POST['city']             = $opp['city'];
    $_POST['state']            = $opp['state'];
    $_POST['country']          = $opp['country'];
    $_POST['postal_code']      = $opp['postal_code'];
    $_POST['source_id']        = $opp['lead_source_id'];
    $_POST['stage_id']         = $opp['stage_id'];
    $_POST['priority']         = $opp['priority'];
    $_POST['assigned_to']      = $opp['assigned_to'];
    $_POST['team_id']          = $opp['sales_team_id'];
    $_POST['expected_revenue'] = $opp['expected_revenue'];
    $_POST['expected_close']   = $opp['expected_close_date'] ? sql2date($opp['expected_close_date']) : '';
    $_POST['customer_id']      = $opp['linked_customer_id'];
    $_POST['notes']            = $opp['notes'];
}

//--------------------------------------------------------------------------
// Display Form
//--------------------------------------------------------------------------

start_form();

if (!$is_new) {
    hidden('LeadID', $lead_id);

    // Status bar
    echo "<div style='margin-bottom:10px;'>";
    if ($opp['stage_name']) {
        echo "<strong>" . _('Stage') . ":</strong> " . htmlspecialchars($opp['stage_name']);
        echo " &nbsp; ";
        echo crm_probability_bar((int)$opp['probability']);
    }
    echo " &nbsp; ";
    echo crm_priority_badge($opp['priority']);
    if ($opp['probability'] > 0) {
        echo " &nbsp; <strong>" . _('Probability') . ":</strong> " . (int)$opp['probability'] . '%';
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
display_heading(_('Opportunity Information'));
start_table(TABLESTYLE2);

text_row(_('Opportunity Name:'), 'lead_name', null, 60, 100);
text_row(_('Organization:'), 'organization', null, 60, 100);
text_row(_('Email:'), 'email', null, 60, 100);
text_row(_('Phone:'), 'phone', null, 30, 30);
text_row(_('Mobile:'), 'mobile', null, 30, 30);
text_row(_('Website:'), 'website', null, 60, 200);

end_table(1);
echo "</div>";

// -- Pipeline ------------------------------------------------------------
echo "<div class='crm-section-block'>";
display_heading(_('Pipeline'));
start_table(TABLESTYLE2);

crm_sales_stage_list_row(_('Sales Stage:'), 'stage_id', null);
crm_lead_source_list_row(_('Source:'), 'source_id', null);
crm_priority_list_row(_('Priority:'), 'priority', null);
crm_sales_team_list_row(_('Sales Team:'), 'team_id', null, true);

// Assigned To
$users_sql = "SELECT id, real_name FROM " . TB_PREF . "users ORDER BY real_name";
$users_result = db_query($users_sql);
$user_options = array(0 => _('-- Unassigned --'));
while ($u = db_fetch($users_result)) {
    $user_options[$u['id']] = $u['real_name'];
}
array_selector_row(_('Assigned To:'), 'assigned_to', null, $user_options);

amount_row(_('Expected Revenue:'), 'expected_revenue', null, null, '', 0);
date_row(_('Expected Close Date:'), 'expected_close', '', null, 0, 0, 0);

// Link to existing customer
$sql = "SELECT debtor_no, name FROM " . TB_PREF . "debtors_master ORDER BY name";
$res = db_query($sql);
$cust_options = array(0 => _('-- None --'));
while ($c = db_fetch($res)) {
    $cust_options[$c['debtor_no']] = $c['name'];
}
array_selector_row(_('Linked Customer:'), 'customer_id', null, $cust_options);

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
submit('Save', $is_new ? _('Create Opportunity') : _('Update Opportunity'), true, '', 'default');

if (!$is_new) {
    // Won/Lost actions
    $won_stage = db_query("SELECT id FROM " . TB_PREF . "crm_sales_stages WHERE name='Won' LIMIT 1");
    $lost_stage = db_query("SELECT id FROM " . TB_PREF . "crm_sales_stages WHERE name='Lost' LIMIT 1");
    $won_stage_row = db_fetch($won_stage);
    $lost_stage_row = db_fetch($lost_stage);
    $is_won  = $won_stage_row && $opp['stage_id'] == $won_stage_row['id'];
    $is_lost = $lost_stage_row && $opp['stage_id'] == $lost_stage_row['id'];
    $is_open = (!$opp['stage_id'] || (!$is_won && !$is_lost));

    if ($is_open) {
        echo "</center><br>";

        // Won section
        display_heading(_('Mark as Won'));
        start_table(TABLESTYLE2);
        $cust_sql = "SELECT debtor_no, name FROM " . TB_PREF . "debtors_master ORDER BY name";
        $cust_res = db_query($cust_sql);
        $won_cust = array(0 => _('-- Select Customer --'));
        while ($c = db_fetch($cust_res)) {
            $won_cust[$c['debtor_no']] = $c['name'];
        }
        array_selector_row(_('Customer:'), 'won_customer_id', null, $won_cust);
        end_table(1);
        echo "<center>";
        submit('MarkWon', _('Won'), true, '', 'default');
        echo "</center><br>";

        // Lost section
        display_heading(_('Mark as Lost'));
        start_table(TABLESTYLE2);
        crm_lost_reason_list_row(_('Lost Reason:'), 'lost_reason_id', null);
        textarea_row(_('Notes:'), 'lost_notes', '', 50, 2);
        end_table(1);
        echo "<center>";
        submit('MarkLost', _('Lost'), true, '', 'cancel');
        echo "</center>";
    }
    echo "<br>";
}
echo "</center><br>";

//--------------------------------------------------------------------------
// Activities & Communication (existing only)
//--------------------------------------------------------------------------

if (!$is_new) {
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
        label_cell(@$row['assigned_name']);
        end_row();
    }
    end_table(1);

    hyperlink_params('schedule_activity.php', _('+ Schedule Activity'),
        'entity_type=' . CRM_ENTITY_LEAD . '&entity_id=' . $lead_id . crm_sel_app_param());
    echo '<br>';

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
crm_page_scripts();
end_page();

