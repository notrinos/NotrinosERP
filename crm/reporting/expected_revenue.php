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
 * CRM Expected Revenue Report
 *
 * Forecasts revenue by close date, stage, and team.
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
include_once($path_to_root . '/crm/includes/db/crm_teams_db.inc');
include_once($path_to_root . '/crm/includes/ui/crm_ui.inc');

$js = '';
if (user_use_date_picker())
    $js .= get_js_date_picker();

page(_($help_context = 'CRM Expected Revenue'), false, false, '', $js);

//--------------------------------------------------------------------------
// Filters
//--------------------------------------------------------------------------

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();

crm_sales_team_list_cells(_('Team:'), 'filter_team', null, true);
date_cells(_('Close Date From:'), 'filter_from', '', null, 0, 0, 0);
date_cells(_('To:'), 'filter_to', '', null, 90, 0, 0);
submit_cells('Refresh', _('Generate'), '', '', 'default');

end_row();
end_table(1);

$f_team = get_post('filter_team', 0);
$f_from = get_post('filter_from', '');
$f_to   = get_post('filter_to', '');

$where = " WHERE l.is_opportunity = 1 AND l.inactive = 0 AND l.expected_close_date IS NOT NULL";
$where .= " AND l.lead_status NOT IN ('won','lost')";
if ($f_team > 0) $where .= " AND l.sales_team_id = " . db_escape((int)$f_team);
if ($f_from && is_date($f_from)) $where .= " AND l.expected_close_date >= " . db_escape(date2sql($f_from));
if ($f_to && is_date($f_to))   $where .= " AND l.expected_close_date <= " . db_escape(date2sql($f_to));

//--------------------------------------------------------------------------
// Revenue by Month
//--------------------------------------------------------------------------

display_heading(_('Expected Revenue by Month'));

$monthly_sql = "SELECT
    DATE_FORMAT(l.expected_close_date, '%Y-%m') as close_month,
    COUNT(l.id) as opp_count,
    SUM(l.expected_revenue) as total_revenue,
    SUM(l.expected_revenue * IFNULL(s.probability, 50) / 100) as weighted_revenue
    FROM " . TB_PREF . "crm_leads l
    LEFT JOIN " . TB_PREF . "crm_sales_stages s ON l.stage_id = s.id" . $where . "
    GROUP BY DATE_FORMAT(l.expected_close_date, '%Y-%m')
    ORDER BY close_month";

$result = db_query($monthly_sql);

start_table(TABLESTYLE, "width='95%'");
$th = array(_('Month'), _('Opportunities'), _('Total Revenue'), _('Weighted Revenue'));
table_header($th);

$grand_total = 0;
$grand_weighted = 0;
$grand_count = 0;
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['close_month']);
    label_cell((int)$row['opp_count'], 'align=right');
    amount_cell($row['total_revenue']);
    amount_cell($row['weighted_revenue']);
    end_row();

    $grand_total += (float)$row['total_revenue'];
    $grand_weighted += (float)$row['weighted_revenue'];
    $grand_count += (int)$row['opp_count'];
}

$th = array(
    '<strong>' . _('TOTAL') . '</strong>',
    '<strong>' . $grand_count . '</strong>',
    '<strong>' . price_format($grand_total) . '</strong>',
    '<strong>' . price_format($grand_weighted) . '</strong>',
);
table_header($th);
end_table(1);

//--------------------------------------------------------------------------
// Revenue by Team
//--------------------------------------------------------------------------

display_heading(_('Expected Revenue by Team'));

$team_sql = "SELECT
    IFNULL(t.name, " . db_escape(_('Unassigned')) . ") as team_name,
    COUNT(l.id) as opp_count,
    SUM(l.expected_revenue) as total_revenue,
    SUM(l.expected_revenue * IFNULL(s.probability, 50) / 100) as weighted_revenue
    FROM " . TB_PREF . "crm_leads l
    LEFT JOIN " . TB_PREF . "crm_sales_stages s ON l.stage_id = s.id
    LEFT JOIN " . TB_PREF . "crm_sales_teams t ON l.sales_team_id = t.id" . $where . "
    GROUP BY l.sales_team_id
    ORDER BY weighted_revenue DESC";

$result = db_query($team_sql);

start_table(TABLESTYLE, "width='95%'");
$th = array(_('Team'), _('Opportunities'), _('Total Revenue'), _('Weighted Revenue'));
table_header($th);

$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['team_name']);
    label_cell((int)$row['opp_count'], 'align=right');
    amount_cell($row['total_revenue']);
    amount_cell($row['weighted_revenue']);
    end_row();
}
end_table(1);

//--------------------------------------------------------------------------
// Revenue by Salesperson
//--------------------------------------------------------------------------

display_heading(_('Expected Revenue by Salesperson'));

$person_sql = "SELECT
    IFNULL(u.real_name, " . db_escape(_('Unassigned')) . ") as salesperson,
    COUNT(l.id) as opp_count,
    SUM(l.expected_revenue) as total_revenue,
    SUM(l.expected_revenue * IFNULL(s.probability, 50) / 100) as weighted_revenue
    FROM " . TB_PREF . "crm_leads l
    LEFT JOIN " . TB_PREF . "crm_sales_stages s ON l.stage_id = s.id
    LEFT JOIN " . TB_PREF . "users u ON l.assigned_to = u.id" . $where . "
    GROUP BY l.assigned_to
    ORDER BY weighted_revenue DESC";

$result = db_query($person_sql);

start_table(TABLESTYLE, "width='95%'");
$th = array(_('Salesperson'), _('Opportunities'), _('Total Revenue'), _('Weighted Revenue'));
table_header($th);

$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['salesperson']);
    label_cell((int)$row['opp_count'], 'align=right');
    amount_cell($row['total_revenue']);
    amount_cell($row['weighted_revenue']);
    end_row();
}
end_table(1);

//--------------------------------------------------------------------------
// Detailed Opportunity List
//--------------------------------------------------------------------------

display_heading(_('Opportunity Details'));

$detail_sql = "SELECT l.lead_ref, l.title, l.company_name, l.expected_revenue,
    l.expected_close_date, s.name as stage_name, s.probability,
    IFNULL(u.real_name, '-') as assigned_name,
    IFNULL(t.name, '-') as team_name
    FROM " . TB_PREF . "crm_leads l
    LEFT JOIN " . TB_PREF . "crm_sales_stages s ON l.stage_id = s.id
    LEFT JOIN " . TB_PREF . "users u ON l.assigned_to = u.id
    LEFT JOIN " . TB_PREF . "crm_sales_teams t ON l.sales_team_id = t.id" . $where . "
    ORDER BY l.expected_close_date, l.expected_revenue DESC";

$result = db_query($detail_sql);

start_table(TABLESTYLE, "width='95%'");
$th = array(_('Ref'), _('Name'), _('Organization'), _('Stage'), _('Probability'),
    _('Revenue'), _('Weighted'), _('Close Date'), _('Team'), _('Assigned'));
table_header($th);

$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['lead_ref']);
    label_cell($row['title']);
    label_cell($row['company_name']);
    label_cell($row['stage_name']);
    label_cell((int)$row['probability'] . '%', 'align=right');
    amount_cell($row['expected_revenue']);
    amount_cell((float)$row['expected_revenue'] * (int)$row['probability'] / 100);
    label_cell(sql2date($row['expected_close_date']));
    label_cell($row['team_name']);
    label_cell($row['assigned_name']);
    end_row();
}
end_table(1);

end_form();
end_page();

