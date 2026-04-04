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
 * CRM Appointments Management (list view)
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_APPOINTMENT';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/crm/includes/ui/crm_ui.inc');

include_crm_files();

page(_($help_context = 'CRM Appointments'));

//--------------------------------------------------------------------------

if (isset($_GET['delete'])) {
    delete_crm_appointment((int)$_GET['delete']);
    display_notification(_('Appointment has been deleted.'));
}

//--------------------------------------------------------------------------

start_form(false, false, $_SERVER['PHP_SELF']);

start_table(TABLESTYLE_NOBORDER);
start_row();

$statuses = crm_appointment_statuses();
crm_filter_array_list_cells(null, 'filter_status', $statuses, null, true, _('All Statuses'), '');

date_cells(_('From:'), 'filter_from', _('From'), null, -30, 0, 0);
date_cells(_('To:'), 'filter_to', _('To'), null, 0, 0, 0);

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
if (!empty($_POST['filter_from'])) {
    $filters['date_from'] = date2sql($_POST['filter_from']);
}
if (!empty($_POST['filter_to'])) {
    $filters['date_to'] = date2sql($_POST['filter_to']);
}

$result = get_crm_appointments($filters);

div_start('appointments_result');

start_table(TABLESTYLE, "width='90%'");

$th = array(
    _('ID'), _('Title'), _('Type'), _('Date/Time'), _('Duration'),
    _('Lead/Customer'), _('Status'), _('Location'), '', ''
);
table_header($th);

$k = 0;
while ($myrow = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($myrow['id']);
    label_cell($myrow['title']);
    label_cell($myrow['type_name'] ?: '-');
    label_cell(sql2date($myrow['date_time']) . ' ' . date('H:i', strtotime($myrow['date_time'])));
    label_cell($myrow['duration_minutes'] . ' ' . _('min'), "align='center'");
    label_cell($myrow['lead_title'] ?: ($myrow['customer_name'] ?: '-'));
    label_cell(crm_activity_status_badge($myrow['status']));
    label_cell($myrow['location'] ?: '-');

    echo "<td><a href='" . $path_to_root . "/crm/transactions/appointment_entry.php?id="
        . $myrow['id'] . crm_sel_app_param() . "'>" . _('Edit') . "</a></td>";
    echo "<td><a href='" . $_SERVER['PHP_SELF'] . "?delete=" . $myrow['id']
        . crm_sel_app_param() . "' onclick=\"return confirm('" . _('Delete this appointment?') . "');\">"
        . _('Delete') . "</a></td>";

    end_row();
}

end_table(1);

echo "<center><a href='" . $path_to_root . "/crm/transactions/appointment_entry.php?sel_app=crm' class='inputsubmit'>" . _('New Appointment') . "</a></center>";

div_end();

$Ajax->activate('appointments_result');

end_form();
crm_page_scripts();

end_page();

