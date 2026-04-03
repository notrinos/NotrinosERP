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
 * CRM Lead Source Analysis Report
 *
 * Analyzes lead generation and conversion by source, with cost-per-lead
 * and ROI metrics when campaign data is available.
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
include_once($path_to_root . '/crm/includes/ui/crm_ui.inc');

$js = '';
if (user_use_date_picker())
    $js .= get_js_date_picker();

page(_($help_context = 'CRM Lead Source Analysis'), false, false, '', $js);

//--------------------------------------------------------------------------
// Filters
//--------------------------------------------------------------------------

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();

date_cells(_('From:'), 'filter_from', '', null, -365, 0, 0);
date_cells(_('To:'), 'filter_to', '', null, 0, 0, 0);
submit_cells('Refresh', _('Generate'), '', '', 'default');

end_row();
end_table(1);

$f_from = get_post('filter_from', '');
$f_to   = get_post('filter_to', '');

$date_where = '';
if ($f_from && is_date($f_from)) $date_where .= " AND l.date_created >= " . db_escape(date2sql($f_from) . ' 00:00:00');
if ($f_to && is_date($f_to))   $date_where .= " AND l.date_created <= " . db_escape(date2sql($f_to) . ' 23:59:59');

//--------------------------------------------------------------------------
// Source Performance
//--------------------------------------------------------------------------

display_heading(_('Lead Source Performance'));

$source_sql = "SELECT
    IFNULL(ls.name, " . db_escape(_('Unknown')) . ") as source_name,
    COUNT(l.id) as total_leads,
    SUM(CASE WHEN l.is_opportunity = 0 THEN 1 ELSE 0 END) as leads_only,
    SUM(CASE WHEN l.is_opportunity = 1 THEN 1 ELSE 0 END) as opportunities,
    SUM(CASE WHEN l.is_opportunity = 1 THEN l.expected_revenue ELSE 0 END) as pipeline_value,
    SUM(CASE WHEN l.lead_status = 'won' THEN 1 ELSE 0 END) as won,
    SUM(CASE WHEN l.lead_status = 'won' THEN l.expected_revenue ELSE 0 END) as won_revenue,
    SUM(CASE WHEN l.lead_status = 'lost' THEN 1 ELSE 0 END) as lost,
    AVG(CASE WHEN l.lead_status = 'won' THEN DATEDIFF(l.date_converted, l.date_created) END) as avg_days_to_won
    FROM " . TB_PREF . "crm_leads l
    LEFT JOIN " . TB_PREF . "crm_lead_sources ls ON l.lead_source_id = ls.id
    WHERE l.inactive = 0" . $date_where . "
    GROUP BY l.lead_source_id
    ORDER BY total_leads DESC";

$result = db_query($source_sql);

start_table(TABLESTYLE, "width='95%'");
$th = array(
    _('Source'), _('Total Leads'), _('Leads'), _('Opportunities'),
    _('Won'), _('Lost'), _('Conversion %'), _('Pipeline Value'),
    _('Won Revenue'), _('Avg Days to Win')
);
table_header($th);

$grand_leads = 0;
$grand_opps = 0;
$grand_won = 0;
$grand_revenue = 0;
$k = 0;

while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['source_name']);
    label_cell((int)$row['total_leads'], 'align=right');
    label_cell((int)$row['leads_only'], 'align=right');
    label_cell((int)$row['opportunities'], 'align=right');
    label_cell((int)$row['won'], 'align=right');
    label_cell((int)$row['lost'], 'align=right');

    $conv_total = (int)$row['won'] + (int)$row['lost'];
    $conv_rate = $conv_total > 0 ? round(((int)$row['won'] / $conv_total) * 100, 1) : 0;
    label_cell($conv_rate . '%', 'align=right');

    amount_cell($row['pipeline_value']);
    amount_cell($row['won_revenue']);
    label_cell($row['avg_days_to_won'] ? round((float)$row['avg_days_to_won'], 1) : '-', 'align=right');
    end_row();

    $grand_leads += (int)$row['total_leads'];
    $grand_opps += (int)$row['opportunities'];
    $grand_won += (int)$row['won'];
    $grand_revenue += (float)$row['won_revenue'];
}

$th = array(
    '<strong>' . _('TOTAL') . '</strong>',
    '<strong>' . $grand_leads . '</strong>',
    '', '',
    '<strong>' . $grand_won . '</strong>',
    '', '',  '', 
    '<strong>' . price_format($grand_revenue) . '</strong>',
    ''
);
table_header($th);
end_table(1);

//--------------------------------------------------------------------------
// Source Quality Score
//--------------------------------------------------------------------------

display_heading(_('Source Quality Score'));

echo "<p>" . _('Quality score is calculated as: (Won Revenue / Total Leads from source) â€” higher is better.') . "</p>";

$quality_sql = "SELECT
    IFNULL(ls.name, " . db_escape(_('Unknown')) . ") as source_name,
    COUNT(l.id) as total_leads,
    SUM(CASE WHEN l.lead_status = 'won' THEN l.expected_revenue ELSE 0 END) as won_revenue
    FROM " . TB_PREF . "crm_leads l
    LEFT JOIN " . TB_PREF . "crm_lead_sources ls ON l.lead_source_id = ls.id
    WHERE l.inactive = 0" . $date_where . "
    GROUP BY l.lead_source_id
    HAVING total_leads > 0
    ORDER BY (SUM(CASE WHEN l.lead_status = 'won' THEN l.expected_revenue ELSE 0 END) / COUNT(l.id)) DESC";

$result = db_query($quality_sql);

start_table(TABLESTYLE, "width='95%'");
$th = array(_('Source'), _('Leads'), _('Won Revenue'), _('Revenue per Lead'), _('Quality Bar'));
table_header($th);

$max_quality = 0;
$quality_data = array();
while ($row = db_fetch($result)) {
    $row['quality'] = $row['total_leads'] > 0 ? (float)$row['won_revenue'] / (int)$row['total_leads'] : 0;
    if ($row['quality'] > $max_quality) $max_quality = $row['quality'];
    $quality_data[] = $row;
}

$k = 0;
foreach ($quality_data as $row) {
    alt_table_row_color($k);
    label_cell($row['source_name']);
    label_cell((int)$row['total_leads'], 'align=right');
    amount_cell($row['won_revenue']);
    amount_cell($row['quality']);

    $pct = $max_quality > 0 ? round(($row['quality'] / $max_quality) * 100) : 0;
    echo "<td><div style='background:#e0e0e0; border-radius:3px; overflow:hidden;'>";
    echo "<div style='background:#4CAF50; height:16px; width:" . $pct . "%;'></div>";
    echo "</div></td>";
    end_row();
}
end_table(1);

//--------------------------------------------------------------------------
// Monthly Source Trend
//--------------------------------------------------------------------------

display_heading(_('Monthly Lead Generation by Source'));

$trend_sql = "SELECT
    DATE_FORMAT(l.date_created, '%Y-%m') as period,
    IFNULL(ls.name, " . db_escape(_('Unknown')) . ") as source_name,
    COUNT(l.id) as cnt
    FROM " . TB_PREF . "crm_leads l
    LEFT JOIN " . TB_PREF . "crm_lead_sources ls ON l.lead_source_id = ls.id
    WHERE l.inactive = 0" . $date_where . "
    GROUP BY period, l.lead_source_id
    ORDER BY period DESC, cnt DESC";

$result = db_query($trend_sql);

start_table(TABLESTYLE, "width='95%'");
$th = array(_('Month'), _('Source'), _('Leads Generated'));
table_header($th);

$k = 0;
$prev_period = '';
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    if ($row['period'] != $prev_period) {
        label_cell('<strong>' . $row['period'] . '</strong>');
        $prev_period = $row['period'];
    } else {
        label_cell('');
    }
    label_cell($row['source_name']);
    label_cell((int)$row['cnt'], 'align=right');
    end_row();
}
end_table(1);

end_form();
end_page();

