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
 * CRM Lead Inquiry - Searchable, filterable lead list with details
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_PIPELINE';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/crm/includes/crm_constants.inc');
include_once($path_to_root . '/crm/includes/db/crm_settings_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_leads_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_teams_db.inc');
include_once($path_to_root . '/crm/includes/ui/crm_ui.inc');

page(_($help_context = 'CRM Lead Inquiry'));

//--------------------------------------------------------------------------
// Filters
//--------------------------------------------------------------------------

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();

crm_lead_status_list_cells(null, 'filter_status', null, true);
crm_lead_source_list_cells(null, 'filter_source', null, true, _('All Sources'));
crm_priority_list_cells(null, 'filter_priority', null, true, _("All Priorities"));
crm_sales_team_list_cells(null, 'filter_team', null, true, _('All Teams'));

crm_filter_search_cells('filter_search', _('Search:'), 20);

date_cells(_('From:'), 'filter_from', _('Created From'), null, -30, 0, 0);
date_cells(_('To:'), 'filter_to', _('To'), null, 0, 0, 0);

$show_type = array(0 => _('Leads Only'), 1 => _('Opportunities Only'));
crm_filter_array_list_cells(null, 'filter_type', $show_type, null, true, _('Leads & Opportunities'), 2);

submit_cells('Search', _('Apply Filter'), '', _('Apply Filter'), 'default');

end_row();
end_table(1);

//--------------------------------------------------------------------------
// Build filters and query
//--------------------------------------------------------------------------

if (get_post('Search'))
    $Ajax->activate('_page_body');

$filters = array();
$f_status   = get_post('filter_status', '');
$f_source   = get_post('filter_source', '');
$f_priority = get_post('filter_priority', '');
$f_team     = get_post('filter_team', '');
$f_search   = get_post('filter_search', '');
$f_from     = get_post('filter_from', '');
$f_to       = get_post('filter_to', '');
$f_type     = (int)get_post('filter_type', 2);

if ($f_status !== '' && $f_status != -1)   $filters['lead_status'] = $f_status;
if ($f_source !== '' && $f_source != 0)    $filters['lead_source_id'] = (int)$f_source;
if ($f_priority !== '' && $f_priority != -1) $filters['priority'] = (int)$f_priority;
if ($f_team !== '' && $f_team != 0)        $filters['sales_team_id'] = (int)$f_team;
if ($f_search)                              $filters['search'] = $f_search;
if ($f_from && is_date($f_from))           $filters['date_from'] = date2sql($f_from);
if ($f_to && is_date($f_to))              $filters['date_to'] = date2sql($f_to);
if ($f_type == 0) $filters['is_opportunity'] = 0;
elseif ($f_type == 1) $filters['is_opportunity'] = 1;

$total_count = count_crm_leads($filters);
$result = get_crm_leads($filters, 200, 0);

//--------------------------------------------------------------------------
// Display Results
//--------------------------------------------------------------------------

display_heading(sprintf(_('Results: %d record(s)'), $total_count));

start_table(TABLESTYLE, "width='95%'");
$th = array(
    _('Ref'), _('Name'), _('Organization'), _('Email'), _('Phone'),
    _('Source'), _('Status'), _('Priority'), _('Score'), _('Team'),
    _('Assigned To'), _('Created'), ''
);
table_header($th);

$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['lead_ref']);
    label_cell($row['title']);
    label_cell($row['company_name']);
    label_cell($row['email']);
    label_cell($row['phone']);
    label_cell($row['source_name']);
    label_cell(crm_status_badge($row['lead_status'], crm_lead_statuses()));
    label_cell(crm_priority_badge($row['priority']));
    label_cell((int)$row['probability']);
    label_cell($row['team_name']);
    label_cell(@$row['assigned_name'] ?: '-');
    label_cell(sql2date(substr($row['date_created'], 0, 10)));

    // View link â€“ preserve CRM sidebar context via sel_app
    $sel_app_param = isset($_SESSION['sel_app']) ? '&amp;sel_app=' . urlencode($_SESSION['sel_app']) : '';
    if ($row['is_opportunity']) {
        $link = $path_to_root . '/crm/transactions/opportunity_entry.php?LeadID=' . $row['id'] . $sel_app_param;
    } else {
        $link = $path_to_root . '/crm/transactions/lead_entry.php?LeadID=' . $row['id'] . $sel_app_param;
    }
    echo '<td><a href="' . $link . '">' . _('View') . '</a></td>';
    end_row();
}
end_table(1);

end_form();
crm_page_scripts();
end_page();

