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
 * CRM Campaign Entry - Create / Edit Campaign
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_CAMPAIGN';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/crm/includes/crm_constants.inc');
include_once($path_to_root . '/crm/includes/db/crm_settings_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_leads_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_campaigns_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_teams_db.inc');
include_once($path_to_root . '/crm/includes/ui/crm_ui.inc');

$js = '';
if (user_use_date_picker())
    $js .= get_js_date_picker();

//--------------------------------------------------------------------------
// Determine mode
//--------------------------------------------------------------------------

$campaign_id = 0;
$is_new      = true;
$campaign    = null;

$raw_id = isset($_GET['CampaignID']) ? $_GET['CampaignID'] : get_post('CampaignID', 0);
if ((int)$raw_id > 0) {
    $campaign_id = (int)$raw_id;
    $campaign = get_crm_campaign($campaign_id);
    if (!$campaign) {
        display_error(_('Campaign not found.'));
        hyperlink_params($path_to_root . '/crm/manage/campaigns.php', _('Back to Campaigns'), 'sel_app=crm');
        end_page();
        exit;
    }
    $is_new = false;
}

$page_title = $is_new ? _('New Campaign') : sprintf(_('Edit Campaign: %s'), $campaign['name']);
page(_($help_context = 'CRM Campaign Entry'), false, false, '', $js);

//--------------------------------------------------------------------------
// Handle Save
//--------------------------------------------------------------------------

if (isset($_POST['Save'])) {
    $input_error = 0;

    if (strlen(trim($_POST['campaign_name'])) < 1) {
        display_error(_('Campaign name is required.'));
        set_focus('campaign_name');
        $input_error = 1;
    }

    if ($input_error == 0) {
        $data = array(
            'name'          => $_POST['campaign_name'],
            'campaign_type' => $_POST['campaign_type'],
            'status'        => $_POST['status'],
            'start_date'    => $_POST['start_date'] ? date2sql($_POST['start_date']) : null,
            'end_date'      => $_POST['end_date'] ? date2sql($_POST['end_date']) : null,
            'budget'        => $_POST['budget'] != '' ? input_num('budget', 0) : 0,
            'description'   => $_POST['description'],
        );

        begin_transaction();
        if ($is_new) {
            $data['created_by'] = $_SESSION['wa_current_user']->user;
            $campaign_id = add_crm_campaign($data);
            display_notification(_('Campaign has been created.'));
        } else {
            update_crm_campaign($campaign_id, $data);
            display_notification(_('Campaign has been updated.'));
        }
        commit_transaction();

        if ($is_new) {
            meta_forward($_SERVER['PHP_SELF'], 'CampaignID=' . $campaign_id . crm_sel_app_param());
        }
        $campaign = get_crm_campaign($campaign_id);
    }
}

//--------------------------------------------------------------------------
// Handle Lead Enrollment
//--------------------------------------------------------------------------

if (isset($_POST['EnrollLead']) && !$is_new) {
    $enroll_lead_id = (int)$_POST['enroll_lead_id'];
    if ($enroll_lead_id > 0) {
        begin_transaction();
        enroll_crm_campaign_lead($campaign_id, $enroll_lead_id);
        commit_transaction();
        display_notification(_('Lead enrolled in campaign.'));
    }
}

if (isset($_POST['UnenrollLead']) && !$is_new) {
    $unenroll_lead_id = (int)$_POST['unenroll_lead_id'];
    if ($unenroll_lead_id > 0) {
        begin_transaction();
        unenroll_crm_campaign_lead($campaign_id, $unenroll_lead_id);
        commit_transaction();
        display_notification(_('Lead removed from campaign.'));
    }
}

//--------------------------------------------------------------------------
// Handle Email Schedule
//--------------------------------------------------------------------------

if (isset($_POST['AddEmail']) && !$is_new) {
    $template_id = (int)$_POST['email_template_id'];
    $send_day = (int)$_POST['send_day'];
    if ($template_id > 0 && $send_day >= 0) {
        begin_transaction();
        add_crm_campaign_email(array(
            'campaign_id' => $campaign_id,
            'template_id' => $template_id,
            'send_day'    => $send_day,
            'sequence'    => $send_day,
        ));
        commit_transaction();
        display_notification(_('Email added to schedule.'));
    }
}

//--------------------------------------------------------------------------
// Load data into POST
//--------------------------------------------------------------------------

if (!$is_new && !isset($_POST['Save'])) {
    $_POST['campaign_name'] = $campaign['name'];
    $_POST['campaign_type'] = $campaign['campaign_type'];
    $_POST['status']        = $campaign['status'];
    $_POST['start_date']    = $campaign['start_date'] ? sql2date($campaign['start_date']) : '';
    $_POST['end_date']      = $campaign['end_date'] ? sql2date($campaign['end_date']) : '';
    $_POST['budget']        = $campaign['budget'];
    $_POST['description']   = $campaign['description'];
}

//--------------------------------------------------------------------------
// Display Form
//--------------------------------------------------------------------------

start_form();

if (!$is_new) {
    hidden('CampaignID', $campaign_id);
}

display_heading(_('Campaign Details'));
start_table(TABLESTYLE2);

text_row(_('Campaign Name:'), 'campaign_name', null, 60, 100);

$types = array('email' => _('Email'), 'social' => _('Social Media'), 'event' => _('Event'), 'other' => _('Other'));
array_selector_row(_('Type:'), 'campaign_type', null, $types);

$statuses = array(
    CRM_CAMPAIGN_DRAFT     => _('Draft'),
    CRM_CAMPAIGN_ACTIVE    => _('Active'),
    CRM_CAMPAIGN_COMPLETED => _('Completed'),
    CRM_CAMPAIGN_CANCELLED => _('Cancelled'),
);
array_selector_row(_('Status:'), 'status', null, $statuses);

date_row(_('Start Date:'), 'start_date', '', null, 0, 0, 0);
date_row(_('End Date:'), 'end_date', '', null, 0, 0, 0);
amount_row(_('Budget:'), 'budget', null, null, '', 0);
textarea_row(_('Description:'), 'description', null, 60, 4);

end_table(1);

echo "<center>";
submit('Save', $is_new ? _('Create Campaign') : _('Update Campaign'), true, '', 'default');
echo "</center><br>";

//--------------------------------------------------------------------------
// Campaign Stats, Leads, Email Schedule (existing only)
//--------------------------------------------------------------------------

if (!$is_new) {

    // -- Stats ---------------------------------------------------------------
    $stats = get_crm_campaign_stats($campaign_id);
    if ($stats) {
        display_heading(_('Campaign Statistics'));
        start_table(TABLESTYLE2);
        label_row(_('Total Leads:'), (int)$stats['total_leads']);
        label_row(_('Converted:'), (int)$stats['converted']);
        label_row(_('Total Revenue:'), price_format($stats['total_revenue']));
        end_table(1);
    }

    // -- Enrolled Leads ------------------------------------------------------
    display_heading(_('Enrolled Leads'));
    start_table(TABLESTYLE, "width='95%'");
    $th = array(_('Lead Name'), _('Organization'), _('Status'), _('Enrolled Date'), '');
    table_header($th);

    $enrolled = get_crm_campaign_leads($campaign_id);
    $k = 0;
    while ($row = db_fetch($enrolled)) {
        alt_table_row_color($k);
        label_cell($row['title']);
        label_cell($row['company_name']);
        label_cell($row['lead_status']);
        label_cell(sql2date(substr($row['enrolled_date'], 0, 10)));
        echo '<td>';
        echo "<button type='submit' name='UnenrollLead' value='1' class='ajaxsubmit'>"
            . _('Remove') . "</button>";
        echo "<input type='hidden' name='unenroll_lead_id' value='" . (int)$row['lead_id'] . "'>";
        echo '</td>';
        end_row();
    }
    end_table(1);

    // Enroll new lead
    start_table(TABLESTYLE2);
    // Simple lead selector
    $leads_sql = "SELECT id, CONCAT(lead_ref, ' - ', title) as label
                  FROM " . TB_PREF . "crm_leads
                  WHERE is_opportunity = 0 AND inactive = 0
                  ORDER BY title";
    $leads_res = db_query($leads_sql);
    $lead_options = array(0 => _('-- Select Lead --'));
    while ($l = db_fetch($leads_res)) {
        $lead_options[$l['id']] = $l['label'];
    }
    array_selector_row(_('Enroll Lead:'), 'enroll_lead_id', null, $lead_options);
    end_table(1);

    echo "<center>";
    submit('EnrollLead', _('Enroll Lead'), true);
    echo "</center><br>";

    // -- Email Schedule ------------------------------------------------------
    display_heading(_('Email Schedule'));
    start_table(TABLESTYLE, "width='95%'");
    $th = array(_('Day'), _('Template'), _('Subject'));
    table_header($th);

    $emails = get_crm_campaign_emails($campaign_id);
    $k = 0;
    while ($row = db_fetch($emails)) {
        alt_table_row_color($k);
        label_cell(_('Day') . ' ' . (int)$row['send_day']);
        label_cell($row['template_name']);
        label_cell($row['subject']);
        end_row();
    }
    end_table(1);

    // Add email to schedule
    start_table(TABLESTYLE2);
    crm_email_template_list_row(_('Email Template:'), 'email_template_id', null);
    text_row(_('Send on Day:'), 'send_day', '0', 5, 5);
    end_table(1);

    echo "<center>";
    submit('AddEmail', _('Add to Schedule'), true);
    echo "</center><br>";
}

end_form();
end_page();

