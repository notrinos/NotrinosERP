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
 * CRM Leads Management (list view with pager)
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_LEAD';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/db_pager.inc');
include_once($path_to_root . '/crm/includes/ui/crm_ui.inc');

include_crm_files();

page(_($help_context = 'CRM Leads'));

//--------------------------------------------------------------------------
// Delete handling
//--------------------------------------------------------------------------
if (isset($_GET['delete'])) {
    $id = (int)$_GET['delete'];
    delete_crm_lead($id, false);
    display_notification(_('Lead has been deactivated.'));
}

//--------------------------------------------------------------------------
// Filter form
//--------------------------------------------------------------------------

if (isset($_POST['Reset'])) {
    unset($_POST['filter_type']);
    unset($_POST['filter_status']);
    unset($_POST['filter_source']);
    unset($_POST['filter_team']);
    unset($_POST['filter_search']);
    unset($_POST['filter_date_from']);
    unset($_POST['filter_date_to']);
    unset($_POST['filter_tag']);
    meta_forward($_SERVER['PHP_SELF'], "sel_app=crm");
}

start_form(false, false, $_SERVER['PHP_SELF']);

start_table(TABLESTYLE_NOBORDER);
start_row();

// Type filter: Leads / Opportunities / All
$type_items = array('leads' => _('Leads Only'), 'opportunities' => _('Opportunities'), 'all' => _('All'));
crm_filter_array_list_cells(null, 'filter_type', $type_items, null, true);

crm_lead_status_list_cells(null, 'filter_status', get_post('filter_status'), true);
crm_lead_source_list_cells(null, 'filter_source', get_post('filter_source'), true, _('All Sources'));
crm_sales_team_list_cells(null, 'filter_team', get_post('filter_team'), true, _('All Teams'));

crm_filter_search_cells('filter_search', _('Search:'), 20);

date_cells(_('From:'), 'filter_date_from', _('From'), null, -30, 0, 0, null, true);
date_cells(_('To:'), 'filter_date_to', _('To'), null, 0, 0, 0, null, true);
tag_list_cells(null, 'filter_tag', 1, TAG_CRM, false, false, _('-- All Tags --'));

submit_cells('Search', _('Apply Filter'), '', _('Apply filter'), 'default');
submit_cells('Reset', _('Reset'), '', '', 'default');

end_row();
end_table();

//--------------------------------------------------------------------------
// Leads list table (inside form for AJAX updates)
//--------------------------------------------------------------------------

div_start('leads_result');

$type_val = get_post('filter_type', 'leads');
$filters = array(
    'inactive' => 0,
);
if ($type_val == 'leads') {
    $filters['is_opportunity'] = 0;
} elseif ($type_val == 'opportunities') {
    $filters['is_opportunity'] = 1;
}
// 'all' => no is_opportunity filter

if (!empty($_POST['filter_status'])) {
    $filters['lead_status'] = $_POST['filter_status'];
}
if (!empty($_POST['filter_source'])) {
    $filters['lead_source_id'] = $_POST['filter_source'];
}
if (!empty($_POST['filter_team'])) {
    $filters['sales_team_id'] = $_POST['filter_team'];
}
if (!empty($_POST['filter_search'])) {
    $filters['search'] = $_POST['filter_search'];
}
if (!empty($_POST['filter_date_from'])) {
    $filters['date_from'] = date2sql($_POST['filter_date_from']);
}
if (!empty($_POST['filter_date_to'])) {
    $filters['date_to'] = date2sql($_POST['filter_date_to']);
}
if (!empty($_POST['filter_tag']) && $_POST['filter_tag'] > 0) {
    $filters['tag_id'] = $_POST['filter_tag'];
}

$result = get_crm_leads($filters);

start_table(TABLESTYLE, "width='95%'");

$th = array(
    _('Ref'), _('Title'), _('Company'), _('Contact'), _('Email'),
    _('Source'), _('Status'), _('Priority'), _('Assigned To'), _('Created'), '', '', ''
);
table_header($th);

$k = 0;
while ($myrow = db_fetch($result)) {
    alt_table_row_color($k);

    // Ref with link to edit
    label_cell("<a href='" . $path_to_root . "/crm/transactions/lead_entry.php?LeadID="
        . $myrow['id'] . crm_sel_app_param() . "'>" . ($myrow['lead_ref'] ?: '#' . $myrow['id']) . "</a>");
    label_cell($myrow['title']);
    label_cell($myrow['company_name']);
    label_cell($myrow['contact_name']);
    label_cell($myrow['email']);
    label_cell($myrow['source_name']);
    label_cell(crm_status_badge($myrow['lead_status']));
    label_cell(crm_priority_badge($myrow['priority']));

    // Get assigned user name
    if ($myrow['assigned_to']) {
        $sql = "SELECT real_name FROM " . TB_PREF . "users WHERE id = " . db_escape($myrow['assigned_to']);
        $u = db_fetch(db_query($sql, ""));
        label_cell($u ? $u['real_name'] : '-');
    } else {
        label_cell('-');
    }

    label_cell(sql2date($myrow['date_created']));

    // Action buttons
    echo "<td><a href='" . $path_to_root . "/crm/transactions/lead_entry.php?LeadID="
        . $myrow['id'] . crm_sel_app_param() . "'>" . _('Edit') . "</a></td>";

    if ($myrow['lead_status'] !== CRM_LEAD_CONVERTED) {
        echo "<td><a href='" . $path_to_root . "/crm/transactions/convert_lead.php?LeadID="
            . $myrow['id'] . crm_sel_app_param() . "'>" . _('Convert') . "</a></td>";
    } else {
        echo "<td>-</td>";
    }

    echo "<td><a href='" . $_SERVER['PHP_SELF'] . "?delete=" . $myrow['id']
        . crm_sel_app_param() . "' onclick=\"return confirm('" . _('Are you sure?') . "');\">"
        . _('Deactivate') . "</a></td>";

    end_row();
}

end_table(1);

// New lead button
echo "<center><a href='" . $path_to_root . "/crm/transactions/lead_entry.php?sel_app=crm' class='inputsubmit'>" . _('New Lead') . "</a></center>";

div_end();

$Ajax->activate('leads_result');

end_form();
crm_page_scripts();

end_page();

