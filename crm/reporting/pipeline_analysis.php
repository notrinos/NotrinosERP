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
 * CRM Pipeline Analysis Report
 *
 * Shows pipeline value by stage, conversion rates, and velocity metrics.
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

page(_($help_context = 'CRM Pipeline Analysis'), false, false, '', $js);

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

//--------------------------------------------------------------------------
// Pipeline by Stage
//--------------------------------------------------------------------------

$f_team = get_post('filter_team', 0);
$f_from = get_post('filter_from', '');
$f_to   = get_post('filter_to', '');

$where = " WHERE l.is_opportunity = 1 AND l.inactive = 0";
if ($f_team > 0) $where .= " AND l.sales_team_id = " . db_escape((int)$f_team);
if ($f_from && is_date($f_from)) $where .= " AND l.date_created >= " . db_escape(date2sql($f_from) . ' 00:00:00');
if ($f_to && is_date($f_to))   $where .= " AND l.date_created <= " . db_escape(date2sql($f_to) . ' 23:59:59');

// Stage breakdown
$sql = "SELECT s.name, s.probability,
        COUNT(l.id) as opp_count,
        SUM(l.expected_revenue) as total_revenue,
        SUM(l.expected_revenue * s.probability / 100) as weighted_revenue,
        AVG(DATEDIFF(IFNULL(l.date_converted, NOW()), l.date_created)) as avg_days
    FROM " . TB_PREF . "crm_sales_stages s
    LEFT JOIN " . TB_PREF . "crm_leads l ON l.stage_id = s.id" . $where . "
    GROUP BY s.id
    ORDER BY s.sequence";

$result = db_query($sql);

display_heading(_('Pipeline by Stage'));
start_table(TABLESTYLE, "width='95%'");
$th = array(_('Stage'), _('Probability'), _('Count'), _('Total Value'), _('Weighted Value'), _('Avg Days in Stage'));
table_header($th);

$grand_total = 0;
$grand_weighted = 0;
$grand_count = 0;
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    $style = '';
    if ((int)$row['probability'] >= 100) $style = " style='color:green; font-weight:bold;'";
    if ((int)$row['probability'] == 0 && $row['opp_count'] > 0) $style = " style='color:red;'";

    echo "<td$style>" . htmlspecialchars($row['name']) . "</td>";
    echo "<td align='right'>" . (int)$row['probability'] . "%</td>";
    echo "<td align='right'>" . (int)$row['opp_count'] . "</td>";
    amount_cell($row['total_revenue']);
    amount_cell($row['weighted_revenue']);
    echo "<td align='right'>" . round((float)$row['avg_days'], 1) . "</td>";
    end_row();

    $grand_total += (float)$row['total_revenue'];
    $grand_weighted += (float)$row['weighted_revenue'];
    $grand_count += (int)$row['opp_count'];
}

// Grand totals
$th = array(
    '<strong>' . _('TOTAL') . '</strong>',
    '',
    '<strong>' . $grand_count . '</strong>',
    '<strong>' . price_format($grand_total) . '</strong>',
    '<strong>' . price_format($grand_weighted) . '</strong>',
    ''
);
table_header($th);

end_table(1);

//--------------------------------------------------------------------------
// Conversion Funnel
//--------------------------------------------------------------------------

display_heading(_('Conversion Funnel'));

$funnel_sql = "SELECT
    (SELECT COUNT(*) FROM " . TB_PREF . "crm_leads WHERE is_opportunity = 0 AND inactive = 0) as total_leads,
    (SELECT COUNT(*) FROM " . TB_PREF . "crm_leads WHERE is_opportunity = 1 AND inactive = 0) as total_opportunities,
    (SELECT COUNT(*) FROM " . TB_PREF . "crm_leads WHERE lead_status = 'won' AND inactive = 0) as won_deals,
    (SELECT COUNT(*) FROM " . TB_PREF . "crm_leads WHERE lead_status = 'lost' AND inactive = 0) as lost_deals";

$funnel = db_fetch(db_query($funnel_sql));

start_table(TABLESTYLE2);
label_row(_('Total Leads:'), '<strong>' . (int)$funnel['total_leads'] . '</strong>');
label_row(_('Total Opportunities:'), '<strong>' . (int)$funnel['total_opportunities'] . '</strong>');
label_row(_('Won Deals:'), '<strong style="color:green;">' . (int)$funnel['won_deals'] . '</strong>');
label_row(_('Lost Deals:'), '<strong style="color:red;">' . (int)$funnel['lost_deals'] . '</strong>');

$lead_to_opp = $funnel['total_leads'] > 0
    ? round(($funnel['total_opportunities'] / ($funnel['total_leads'] + $funnel['total_opportunities'])) * 100, 1) : 0;
$opp_to_won = $funnel['total_opportunities'] > 0
    ? round(($funnel['won_deals'] / $funnel['total_opportunities']) * 100, 1) : 0;

label_row(_('Lead â†’ Opportunity Rate:'), $lead_to_opp . '%');
label_row(_('Opportunity â†’ Won Rate:'), $opp_to_won . '%');
end_table(1);

//--------------------------------------------------------------------------
// Pipeline Velocity
//--------------------------------------------------------------------------

display_heading(_('Pipeline Velocity'));

$velocity_sql = "SELECT
    AVG(DATEDIFF(l.date_converted, l.date_created)) as avg_cycle_days,
    AVG(l.expected_revenue) as avg_deal_size
    FROM " . TB_PREF . "crm_leads l
    WHERE l.lead_status = 'won' AND l.date_converted IS NOT NULL AND l.inactive = 0";

$velocity = db_fetch(db_query($velocity_sql));

start_table(TABLESTYLE2);
label_row(_('Average Sales Cycle (days):'), round((float)$velocity['avg_cycle_days'], 1));
label_row(_('Average Deal Size:'), price_format((float)$velocity['avg_deal_size']));

// Pipeline velocity = (# deals * avg deal value * win rate) / avg cycle days
if ($velocity['avg_cycle_days'] > 0 && $opp_to_won > 0) {
    $pv = ($grand_count * (float)$velocity['avg_deal_size'] * ($opp_to_won / 100)) / (float)$velocity['avg_cycle_days'];
    label_row(_('Pipeline Velocity (daily):'), price_format($pv));
}
end_table(1);

end_form();
end_page();

