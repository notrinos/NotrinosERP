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
 * CRM Team Performance Report
 *
 * Analyzes sales team and individual salesperson performance metrics.
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_REPORT';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/crm/includes/crm_constants.inc');
include_once($path_to_root . '/crm/includes/db/crm_settings_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_leads_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_activities_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_teams_db.inc');
include_once($path_to_root . '/crm/includes/ui/crm_ui.inc');

$js = '';
if (user_use_date_picker())
    $js .= get_js_date_picker();

page(_($help_context = 'CRM Team Performance'), false, false, '', $js);

//--------------------------------------------------------------------------
// Filters
//--------------------------------------------------------------------------

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();

crm_sales_team_list_cells(_('Team:'), 'filter_team', null, true);
date_cells(_('From:'), 'filter_from', '', null, -90, 0, 0);
date_cells(_('To:'), 'filter_to', '', null, 0, 0, 0);
submit_cells('Refresh', _('Generate'), '', '', 'default');

end_row();
end_table(1);

$f_team = get_post('filter_team', 0);
$f_from = get_post('filter_from', '');
$f_to   = get_post('filter_to', '');

$date_where = '';
if ($f_from && is_date($f_from)) $date_where .= " AND l.date_created >= " . db_escape(date2sql($f_from) . ' 00:00:00');
if ($f_to && is_date($f_to))   $date_where .= " AND l.date_created <= " . db_escape(date2sql($f_to) . ' 23:59:59');
$team_where = ($f_team > 0) ? " AND l.sales_team_id = " . db_escape((int)$f_team) : '';

//--------------------------------------------------------------------------
// Team Summary
//--------------------------------------------------------------------------

display_heading(_('Team Performance Summary'));

$team_sql = "SELECT
    IFNULL(t.name, " . db_escape(_('Unassigned')) . ") as team_name,
    COUNT(l.id) as total_leads,
    SUM(CASE WHEN l.is_opportunity = 1 THEN 1 ELSE 0 END) as opportunities,
    SUM(CASE WHEN l.lead_status = 'won' THEN 1 ELSE 0 END) as won,
    SUM(CASE WHEN l.lead_status = 'lost' THEN 1 ELSE 0 END) as lost,
    SUM(CASE WHEN l.lead_status = 'won' THEN l.expected_revenue ELSE 0 END) as won_revenue,
    SUM(l.expected_revenue) as pipeline_value,
    AVG(CASE WHEN l.lead_status = 'won' THEN DATEDIFF(l.date_converted, l.date_created) END) as avg_cycle
    FROM " . TB_PREF . "crm_leads l
    LEFT JOIN " . TB_PREF . "crm_sales_teams t ON l.sales_team_id = t.id
    WHERE l.inactive = 0" . $date_where . $team_where . "
    GROUP BY l.sales_team_id
    ORDER BY won_revenue DESC";

$result = db_query($team_sql);

start_table(TABLESTYLE, "width='95%'");
$th = array(
    _('Team'), _('Total Leads'), _('Opportunities'), _('Won'), _('Lost'),
    _('Win Rate'), _('Won Revenue'), _('Pipeline Value'), _('Avg Cycle (days)')
);
table_header($th);

$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['team_name']);
    label_cell((int)$row['total_leads'], 'align=right');
    label_cell((int)$row['opportunities'], 'align=right');
    label_cell((int)$row['won'], 'align=right');
    label_cell((int)$row['lost'], 'align=right');

    $conv_total = (int)$row['won'] + (int)$row['lost'];
    $rate = $conv_total > 0 ? round(((int)$row['won'] / $conv_total) * 100, 1) : 0;
    label_cell($rate . '%', 'align=right');

    amount_cell($row['won_revenue']);
    amount_cell($row['pipeline_value']);
    label_cell($row['avg_cycle'] ? round((float)$row['avg_cycle'], 1) : '-', 'align=right');
    end_row();
}
end_table(1);

//--------------------------------------------------------------------------
// Individual Performance
//--------------------------------------------------------------------------

display_heading(_('Individual Salesperson Performance'));

$person_sql = "SELECT
    IFNULL(u.real_name, " . db_escape(_('Unassigned')) . ") as salesperson,
    IFNULL(t.name, '-') as team_name,
    COUNT(l.id) as total_leads,
    SUM(CASE WHEN l.is_opportunity = 1 THEN 1 ELSE 0 END) as opportunities,
    SUM(CASE WHEN l.lead_status = 'won' THEN 1 ELSE 0 END) as won,
    SUM(CASE WHEN l.lead_status = 'lost' THEN 1 ELSE 0 END) as lost,
    SUM(CASE WHEN l.lead_status = 'won' THEN l.expected_revenue ELSE 0 END) as won_revenue,
    SUM(CASE WHEN l.is_opportunity = 1 AND l.lead_status NOT IN ('won','lost') THEN l.expected_revenue ELSE 0 END) as open_pipeline,
    AVG(CASE WHEN l.lead_status = 'won' THEN DATEDIFF(l.date_converted, l.date_created) END) as avg_cycle
    FROM " . TB_PREF . "crm_leads l
    LEFT JOIN " . TB_PREF . "users u ON l.assigned_to = u.id
    LEFT JOIN " . TB_PREF . "crm_sales_teams t ON l.sales_team_id = t.id
    WHERE l.inactive = 0 AND l.assigned_to > 0" . $date_where . $team_where . "
    GROUP BY l.assigned_to
    ORDER BY won_revenue DESC";

$result = db_query($person_sql);

start_table(TABLESTYLE, "width='95%'");
$th = array(
    _('Salesperson'), _('Team'), _('Leads'), _('Opportunities'),
    _('Won'), _('Lost'), _('Win Rate'), _('Won Revenue'),
    _('Open Pipeline'), _('Avg Cycle (days)')
);
table_header($th);

$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['salesperson']);
    label_cell($row['team_name']);
    label_cell((int)$row['total_leads'], 'align=right');
    label_cell((int)$row['opportunities'], 'align=right');
    label_cell((int)$row['won'], 'align=right');
    label_cell((int)$row['lost'], 'align=right');

    $conv_total = (int)$row['won'] + (int)$row['lost'];
    $rate = $conv_total > 0 ? round(((int)$row['won'] / $conv_total) * 100, 1) : 0;
    label_cell($rate . '%', 'align=right');

    amount_cell($row['won_revenue']);
    amount_cell($row['open_pipeline']);
    label_cell($row['avg_cycle'] ? round((float)$row['avg_cycle'], 1) : '-', 'align=right');
    end_row();
}
end_table(1);

//--------------------------------------------------------------------------
// Activity Metrics by User
//--------------------------------------------------------------------------

display_heading(_('Activity Metrics by Salesperson'));

$act_date_where = '';
if ($f_from && is_date($f_from)) $act_date_where .= " AND a.created_date >= " . db_escape(date2sql($f_from) . ' 00:00:00');
if ($f_to && is_date($f_to))   $act_date_where .= " AND a.created_date <= " . db_escape(date2sql($f_to) . ' 23:59:59');

$activity_sql = "SELECT
    u.real_name as salesperson,
    COUNT(a.id) as total_activities,
    SUM(CASE WHEN a.status = '" . CRM_ACTIVITY_DONE . "' THEN 1 ELSE 0 END) as completed,
    SUM(CASE WHEN a.status = '" . CRM_ACTIVITY_PLANNED . "' THEN 1 ELSE 0 END) as pending,
    SUM(CASE WHEN a.status = '" . CRM_ACTIVITY_OVERDUE . "' THEN 1 ELSE 0 END) as overdue
    FROM " . TB_PREF . "crm_activities a
    INNER JOIN " . TB_PREF . "users u ON a.assigned_to = u.id
    WHERE 1=1" . $act_date_where . "
    GROUP BY a.assigned_to
    ORDER BY total_activities DESC";

$result = db_query($activity_sql);

start_table(TABLESTYLE, "width='95%'");
$th = array(_('Salesperson'), _('Total Activities'), _('Completed'), _('Pending'), _('Overdue'), _('Completion Rate'));
table_header($th);

$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['salesperson']);
    label_cell((int)$row['total_activities'], 'align=right');
    label_cell((int)$row['completed'], 'align=right');
    label_cell((int)$row['pending'], 'align=right');

    $overdue = (int)$row['overdue'];
    $overdue_html = $overdue > 0
        ? '<span style="color:red;">' . $overdue . '</span>'
        : '0';
    label_cell($overdue_html, 'align=right');

    $completion_rate = $row['total_activities'] > 0
        ? round(((int)$row['completed'] / (int)$row['total_activities']) * 100, 1) : 0;
    label_cell($completion_rate . '%', 'align=right');
    end_row();
}
end_table(1);

end_form();
end_page();

