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
 * CRM Campaigns Management (list view)
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_CAMPAIGN';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/crm/includes/ui/crm_ui.inc');

include_crm_files();

page(_($help_context = 'CRM Campaigns'));

//--------------------------------------------------------------------------

if (isset($_GET['delete'])) {
    delete_crm_campaign((int)$_GET['delete']);
    display_notification(_('Campaign has been deleted.'));
}

//--------------------------------------------------------------------------

start_form(false, false, $_SERVER['PHP_SELF']);

start_table(TABLESTYLE_NOBORDER);
start_row();

$statuses = crm_campaign_statuses();
crm_filter_array_list_cells(null, 'filter_status', $statuses, null, true, _('All Statuses'), '');

submit_cells('Search', _('Apply Filter'), '', _('Apply filter'), 'default');
submit_cells('Reset', _('Reset'), '', '', 'default');
end_row();
end_table();

if (isset($_POST['Reset'])) {
    meta_forward($_SERVER['PHP_SELF']);
}

//--------------------------------------------------------------------------

$filters = array();
if (!empty($_POST['filter_status'])) {
    $filters['status'] = $_POST['filter_status'];
}

$result = get_crm_campaigns($filters);

div_start('campaigns_result');

start_table(TABLESTYLE, "width='100%'");

$th = array(
    _('ID'), _('Name'), _('Type'), _('Status'), _('Start'), _('End'),
    _('Budget'), _('Leads'), '', ''
);
table_header($th);

$k = 0;
while ($myrow = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($myrow['id']);
    label_cell("<a href='" . $path_to_root . "/crm/transactions/campaign_entry.php?id="
        . $myrow['id'] . crm_sel_app_param() . "'>" . htmlspecialchars($myrow['name']) . "</a>");
    label_cell(ucfirst($myrow['campaign_type'] ?: '-'));
    label_cell(crm_status_badge($myrow['status']));
    label_cell($myrow['start_date'] ? sql2date($myrow['start_date']) : '-');
    label_cell($myrow['end_date'] ? sql2date($myrow['end_date']) : '-');
    amount_cell($myrow['budget']);
    label_cell($myrow['lead_count'], "align='center'");

    echo "<td><a href='" . $path_to_root . "/crm/transactions/campaign_entry.php?id="
        . $myrow['id'] . crm_sel_app_param() . "'>" . _('Edit') . "</a></td>";
    echo "<td><a href='" . $_SERVER['PHP_SELF'] . "?delete=" . $myrow['id']
        . crm_sel_app_param() . "' onclick=\"return confirm('" . _('Delete this campaign?') . "');\">"
        . _('Delete') . "</a></td>";

    end_row();
}

end_table(1);

echo "<center><a href='" . $path_to_root . "/crm/transactions/campaign_entry.php?sel_app=crm' class='inputsubmit'>" . _('New Campaign') . "</a></center>";

div_end();

$Ajax->activate('campaigns_result');

end_form();
crm_page_scripts();

end_page();

