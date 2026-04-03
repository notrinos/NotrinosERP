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

start_form(false, false, $_SERVER['PHP_SELF']);

start_table(TABLESTYLE_NOBORDER);
start_row();

crm_lead_status_list_cells(_('Status:'), 'filter_status', get_post('filter_status'), true);
crm_lead_source_list_cells(_('Source:'), 'filter_source', get_post('filter_source'), true);
crm_sales_team_list_cells(_('Team:'), 'filter_team', get_post('filter_team'), true);

echo "<td>" . _('Search:') . "</td><td>";
echo "<input type='text' name='filter_search' value='" . htmlspecialchars(get_post('filter_search', '')) . "' size='20'>";
echo "</td>";
submit_cells('Search', _('Search'), '', '', 'default');
submit_cells('Reset', _('Reset'), '', '', 'default');

end_row();
end_table();

if (isset($_POST['Reset'])) {
    $_POST['filter_status'] = '';
    $_POST['filter_source'] = '';
    $_POST['filter_team'] = '';
    $_POST['filter_search'] = '';
}

end_form();

//--------------------------------------------------------------------------
// Leads list table
//--------------------------------------------------------------------------

$filters = array(
    'is_opportunity' => 0,
    'inactive' => 0,
);

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
echo "<center><a href='" . $path_to_root . "/crm/transactions/lead_entry.php?sel_app=crm' class='ajaxsubmit'>"
    . "<button class='inputsubmit'>" . _('New Lead') . "</button></a></center>";

end_page();

