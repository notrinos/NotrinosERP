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
 * CRM Contracts Management (list view)
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_CONTRACT';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/crm/includes/ui/crm_ui.inc');

include_crm_files();

page(_($help_context = 'CRM Contracts'));

//--------------------------------------------------------------------------

if (isset($_GET['delete'])) {
    delete_crm_contract((int)$_GET['delete']);
    display_notification(_('Contract has been deleted.'));
}

//--------------------------------------------------------------------------

start_form(false, false, $_SERVER['PHP_SELF']);

start_table(TABLESTYLE_NOBORDER);
start_row();

$statuses = crm_contract_statuses();
crm_filter_array_list_cells(null, 'filter_status', $statuses, null, true, _('All Statuses'), '');

$expiring_items = array(7 => sprintf(_('%d days'), 7), 14 => sprintf(_('%d days'), 14), 30 => sprintf(_('%d days'), 30), 60 => sprintf(_('%d days'), 60), 90 => sprintf(_('%d days'), 90));
crm_filter_array_list_cells(null, 'filter_expiring', $expiring_items, null, true, _('-- No Filter --'), '');

crm_filter_search_cells('filter_search', _('Search:'), 20);
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
if (!empty($_POST['filter_search'])) {
    $filters['search'] = $_POST['filter_search'];
}
if (!empty($_POST['filter_expiring'])) {
    $filters['expiring_within_days'] = (int)$_POST['filter_expiring'];
}

$result = get_crm_contracts($filters);

div_start('contracts_result');

start_table(TABLESTYLE, "width='100%'");

$th = array(
    _('Ref'), _('Title'), _('Customer'), _('Value'), _('Start'), _('End'),
    _('Status'), '', ''
);
table_header($th);

$k = 0;
while ($myrow = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($myrow['contract_ref'] ?: '#' . $myrow['id']);
    label_cell("<a href='" . $path_to_root . "/crm/transactions/contract_entry.php?id="
        . $myrow['id'] . crm_sel_app_param() . "'>" . htmlspecialchars($myrow['title']) . "</a>");
    label_cell($myrow['customer_name'] ?: '-');
    amount_cell($myrow['contract_value']);
    label_cell($myrow['start_date'] ? sql2date($myrow['start_date']) : '-');
    label_cell($myrow['end_date'] ? sql2date($myrow['end_date']) : '-');
    label_cell(crm_status_badge($myrow['status']));

    echo "<td><a href='" . $path_to_root . "/crm/transactions/contract_entry.php?id="
        . $myrow['id'] . crm_sel_app_param() . "'>" . _('Edit') . "</a></td>";
    echo "<td><a href='" . $_SERVER['PHP_SELF'] . "?delete=" . $myrow['id']
        . crm_sel_app_param() . "' onclick=\"return confirm('" . _('Delete this contract?') . "');\">"
        . _('Delete') . "</a></td>";

    end_row();
}

end_table(1);

echo "<center><a href='" . $path_to_root . "/crm/transactions/contract_entry.php?sel_app=crm' class='inputsubmit'>" . _('New Contract') . "</a></center>";

div_end();

$Ajax->activate('contracts_result');

end_form();
crm_page_scripts();

end_page();

