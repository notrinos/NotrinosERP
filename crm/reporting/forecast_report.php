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
 * CRM Revenue Forecast Report
 *
 * Projects revenue based on weighted pipeline, historical conversion rates,
 * and expected close dates.
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

page(_($help_context = 'CRM Revenue Forecast'));

//--------------------------------------------------------------------------
// Historical Win Rate for comparison
//--------------------------------------------------------------------------

$hist_sql = "SELECT
    COUNT(CASE WHEN l.lead_status = 'won' THEN 1 END) as won,
    COUNT(*) as total,
    AVG(CASE WHEN l.lead_status = 'won' THEN l.expected_revenue END) as avg_won_value
    FROM " . TB_PREF . "crm_leads l
    WHERE l.lead_status IN ('won','lost') AND l.inactive = 0
    AND l.date_converted >= DATE_SUB(NOW(), INTERVAL 12 MONTH)";

$hist = db_fetch(db_query($hist_sql));
$historical_win_rate = $hist['total'] > 0 ? round(($hist['won'] / $hist['total']) * 100, 1) : 0;
$avg_won_value = (float)$hist['avg_won_value'];

//--------------------------------------------------------------------------
// Display Historical Benchmarks
//--------------------------------------------------------------------------

start_form();

display_heading(_('Historical Benchmarks (Last 12 Months)'));
start_table(TABLESTYLE2);
label_row(_('Historical Win Rate:'), $historical_win_rate . '%');
label_row(_('Average Won Deal Value:'), price_format($avg_won_value));
label_row(_('Total Won Deals:'), (int)$hist['won']);
end_table(1);

//--------------------------------------------------------------------------
// Forecast: Next 3 Months
//--------------------------------------------------------------------------

display_heading(_('Revenue Forecast â€” Next 3 Months'));

$forecast_sql = "SELECT
    CASE
        WHEN l.expected_close_date BETWEEN CURDATE() AND LAST_DAY(CURDATE()) THEN 'current'
        WHEN l.expected_close_date BETWEEN DATE_ADD(CURDATE(), INTERVAL 1 MONTH) AND LAST_DAY(DATE_ADD(CURDATE(), INTERVAL 1 MONTH)) THEN 'next1'
        WHEN l.expected_close_date BETWEEN DATE_ADD(CURDATE(), INTERVAL 2 MONTH) AND LAST_DAY(DATE_ADD(CURDATE(), INTERVAL 2 MONTH)) THEN 'next2'
        ELSE 'beyond'
    END as period,
    COUNT(l.id) as opp_count,
    SUM(l.expected_revenue) as total_revenue,
    SUM(l.expected_revenue * IFNULL(s.probability, 50) / 100) as weighted_revenue
    FROM " . TB_PREF . "crm_leads l
    LEFT JOIN " . TB_PREF . "crm_sales_stages s ON l.stage_id = s.id
    WHERE l.is_opportunity = 1 AND l.inactive = 0
    AND l.expected_close_date IS NOT NULL
    AND l.expected_close_date >= CURDATE()
    AND l.expected_close_date <= LAST_DAY(DATE_ADD(CURDATE(), INTERVAL 2 MONTH))
    AND l.lead_status NOT IN ('won','lost')
    GROUP BY period
    ORDER BY FIELD(period, 'current', 'next1', 'next2', 'beyond')";

$result = db_query($forecast_sql);

$period_labels = array(
    'current' => date('F Y'),
    'next1'   => date('F Y', strtotime('+1 month')),
    'next2'   => date('F Y', strtotime('+2 months')),
);

$forecast_data = array();
while ($row = db_fetch($result)) {
    $forecast_data[$row['period']] = $row;
}

start_table(TABLESTYLE, "width='95%'");
$th = array(_('Period'), _('Opportunities'), _('Best Case'), _('Weighted Forecast'), _('Conservative'));
table_header($th);

$total_best = 0;
$total_weighted = 0;
$total_conservative = 0;
$k = 0;

foreach (array('current', 'next1', 'next2') as $p) {
    alt_table_row_color($k);
    label_cell(isset($period_labels[$p]) ? $period_labels[$p] : $p);
    $d = isset($forecast_data[$p]) ? $forecast_data[$p] : null;

    if ($d) {
        label_cell((int)$d['opp_count'], 'align=right');
        amount_cell($d['total_revenue']); // Best case = 100% close rate
        amount_cell($d['weighted_revenue']); // Weighted by stage probability
        $conservative = (float)$d['weighted_revenue'] * ($historical_win_rate / 100);
        amount_cell($conservative); // Conservative = weighted * historical win rate
        $total_best += (float)$d['total_revenue'];
        $total_weighted += (float)$d['weighted_revenue'];
        $total_conservative += $conservative;
    } else {
        label_cell('0', 'align=right');
        amount_cell(0);
        amount_cell(0);
        amount_cell(0);
    }
    end_row();
}

// Totals
$th = array(
    '<strong>' . _('TOTAL') . '</strong>',
    '',
    '<strong>' . price_format($total_best) . '</strong>',
    '<strong>' . price_format($total_weighted) . '</strong>',
    '<strong>' . price_format($total_conservative) . '</strong>',
);
table_header($th);

end_table(1);

//--------------------------------------------------------------------------
// Forecast by Team
//--------------------------------------------------------------------------

display_heading(_('Forecast by Team'));

$team_sql = "SELECT
    IFNULL(t.name, " . db_escape(_('Unassigned')) . ") as team_name,
    COUNT(l.id) as opp_count,
    SUM(l.expected_revenue) as best_case,
    SUM(l.expected_revenue * IFNULL(s.probability, 50) / 100) as weighted
    FROM " . TB_PREF . "crm_leads l
    LEFT JOIN " . TB_PREF . "crm_sales_stages s ON l.stage_id = s.id
    LEFT JOIN " . TB_PREF . "crm_sales_teams t ON l.sales_team_id = t.id
    WHERE l.is_opportunity = 1 AND l.inactive = 0
    AND l.expected_close_date IS NOT NULL
    AND l.expected_close_date BETWEEN CURDATE() AND LAST_DAY(DATE_ADD(CURDATE(), INTERVAL 2 MONTH))
    AND l.lead_status NOT IN ('won','lost')
    GROUP BY l.sales_team_id
    ORDER BY weighted DESC";

$result = db_query($team_sql);

start_table(TABLESTYLE, "width='95%'");
$th = array(_('Team'), _('Opportunities'), _('Best Case'), _('Weighted Forecast'));
table_header($th);

$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['team_name']);
    label_cell((int)$row['opp_count'], 'align=right');
    amount_cell($row['best_case']);
    amount_cell($row['weighted']);
    end_row();
}
end_table(1);

//--------------------------------------------------------------------------
// At-Risk Deals (overdue close date)
//--------------------------------------------------------------------------

display_heading(_('At-Risk Deals (Past Expected Close Date)'));

$risk_sql = "SELECT l.lead_ref, l.title, l.company_name,
    l.expected_revenue, l.expected_close_date,
    s.name as stage_name, u.real_name as assigned_name,
    DATEDIFF(CURDATE(), l.expected_close_date) as days_overdue
    FROM " . TB_PREF . "crm_leads l
    LEFT JOIN " . TB_PREF . "crm_sales_stages s ON l.stage_id = s.id
    LEFT JOIN " . TB_PREF . "users u ON l.assigned_to = u.id
    WHERE l.is_opportunity = 1 AND l.inactive = 0
    AND l.expected_close_date < CURDATE()
    AND l.lead_status NOT IN ('won','lost')
    ORDER BY l.expected_revenue DESC
    LIMIT 20";

$result = db_query($risk_sql);

start_table(TABLESTYLE, "width='95%'");
$th = array(_('Ref'), _('Name'), _('Organization'), _('Stage'), _('Revenue'), _('Expected Close'), _('Days Overdue'), _('Assigned'));
table_header($th);

$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['lead_ref']);
    label_cell($row['title']);
    label_cell($row['company_name']);
    label_cell($row['stage_name']);
    amount_cell($row['expected_revenue']);
    label_cell(sql2date($row['expected_close_date']));
    label_cell('<span style="color:red;">' . (int)$row['days_overdue'] . '</span>', 'align=right');
    label_cell($row['assigned_name']);
    end_row();
}
end_table(1);

end_form();
end_page();

