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
 * CRM Win/Loss Report
 *
 * Analyzes won vs lost deals by period, source, stage, and reason.
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

page(_($help_context = 'CRM Win/Loss Report'), false, false, '', $js);

//--------------------------------------------------------------------------
// Filters
//--------------------------------------------------------------------------

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();

date_cells(_('From:'), 'filter_from', '', null, -180, 0, 0);
date_cells(_('To:'), 'filter_to', '', null, 0, 0, 0);
crm_sales_team_list_cells(_('Team:'), 'filter_team', null, true);
submit_cells('Refresh', _('Generate'), '', '', 'default');

end_row();
end_table(1);

$f_from = get_post('filter_from', '');
$f_to   = get_post('filter_to', '');
$f_team = get_post('filter_team', 0);

$date_where = '';
if ($f_from && is_date($f_from)) $date_where .= " AND l.date_converted >= " . db_escape(date2sql($f_from) . ' 00:00:00');
if ($f_to && is_date($f_to))   $date_where .= " AND l.date_converted <= " . db_escape(date2sql($f_to) . ' 23:59:59');
$team_where = ($f_team > 0) ? " AND l.sales_team_id = " . db_escape((int)$f_team) : '';

//--------------------------------------------------------------------------
// Win/Loss Summary
//--------------------------------------------------------------------------

display_heading(_('Win/Loss Summary'));

$summary_sql = "SELECT
    SUM(CASE WHEN l.lead_status = 'won' THEN 1 ELSE 0 END) as won_count,
    SUM(CASE WHEN l.lead_status = 'lost' THEN 1 ELSE 0 END) as lost_count,
    SUM(CASE WHEN l.lead_status = 'won' THEN l.expected_revenue ELSE 0 END) as won_revenue,
    SUM(CASE WHEN l.lead_status = 'lost' THEN l.expected_revenue ELSE 0 END) as lost_revenue,
    AVG(CASE WHEN l.lead_status = 'won' THEN DATEDIFF(l.date_converted, l.date_created) ELSE NULL END) as avg_won_days,
    AVG(CASE WHEN l.lead_status = 'lost' THEN DATEDIFF(l.date_converted, l.date_created) ELSE NULL END) as avg_lost_days
    FROM " . TB_PREF . "crm_leads l
    WHERE l.lead_status IN ('won','lost') AND l.inactive = 0" . $date_where . $team_where;

$summary = db_fetch(db_query($summary_sql));

$won = (int)$summary['won_count'];
$lost = (int)$summary['lost_count'];
$total = $won + $lost;
$win_rate = $total > 0 ? round(($won / $total) * 100, 1) : 0;

start_table(TABLESTYLE2);
label_row(_('Won Deals:'), '<strong style="color:green;">' . $won . '</strong> (' . price_format($summary['won_revenue']) . ')');
label_row(_('Lost Deals:'), '<strong style="color:red;">' . $lost . '</strong> (' . price_format($summary['lost_revenue']) . ')');
label_row(_('Win Rate:'), '<strong>' . $win_rate . '%</strong>');
label_row(_('Avg Days to Win:'), round((float)$summary['avg_won_days'], 1));
label_row(_('Avg Days to Lose:'), round((float)$summary['avg_lost_days'], 1));
end_table(1);

//--------------------------------------------------------------------------
// Win/Loss by Source
//--------------------------------------------------------------------------

display_heading(_('Win/Loss by Lead Source'));

$source_sql = "SELECT ls.name as source_name,
    SUM(CASE WHEN l.lead_status = 'won' THEN 1 ELSE 0 END) as won,
    SUM(CASE WHEN l.lead_status = 'lost' THEN 1 ELSE 0 END) as lost,
    SUM(CASE WHEN l.lead_status = 'won' THEN l.expected_revenue ELSE 0 END) as won_rev,
    SUM(CASE WHEN l.lead_status = 'lost' THEN l.expected_revenue ELSE 0 END) as lost_rev
    FROM " . TB_PREF . "crm_leads l
    LEFT JOIN " . TB_PREF . "crm_lead_sources ls ON l.lead_source_id = ls.id
    WHERE l.lead_status IN ('won','lost') AND l.inactive = 0" . $date_where . $team_where . "
    GROUP BY ls.id
    ORDER BY won DESC";

$result = db_query($source_sql);

start_table(TABLESTYLE, "width='95%'");
$th = array(_('Source'), _('Won'), _('Lost'), _('Win Rate'), _('Won Revenue'), _('Lost Revenue'));
table_header($th);

$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['source_name'] ?: _('Unknown'));
    label_cell((int)$row['won'], 'align=right');
    label_cell((int)$row['lost'], 'align=right');
    $src_total = (int)$row['won'] + (int)$row['lost'];
    $rate = $src_total > 0 ? round(((int)$row['won'] / $src_total) * 100, 1) : 0;
    label_cell($rate . '%', 'align=right');
    amount_cell($row['won_rev']);
    amount_cell($row['lost_rev']);
    end_row();
}
end_table(1);

//--------------------------------------------------------------------------
// Lost Reasons Breakdown
//--------------------------------------------------------------------------

display_heading(_('Lost Reasons'));

$reason_sql = "SELECT lr.description as reason_name, COUNT(l.id) as cnt,
    SUM(l.expected_revenue) as lost_rev
    FROM " . TB_PREF . "crm_leads l
    LEFT JOIN " . TB_PREF . "crm_lost_reasons lr ON l.lost_reason_id = lr.id
    WHERE l.lead_status = 'lost' AND l.inactive = 0" . $date_where . $team_where . "
    GROUP BY lr.id
    ORDER BY cnt DESC";

$result = db_query($reason_sql);

start_table(TABLESTYLE, "width='95%'");
$th = array(_('Reason'), _('Count'), _('% of Lost'), _('Revenue Lost'));
table_header($th);

$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['reason_name'] ?: _('No reason specified'));
    label_cell((int)$row['cnt'], 'align=right');
    $pct = $lost > 0 ? round(((int)$row['cnt'] / $lost) * 100, 1) : 0;
    label_cell($pct . '%', 'align=right');
    amount_cell($row['lost_rev']);
    end_row();
}
end_table(1);

//--------------------------------------------------------------------------
// Monthly Win/Loss Trend
//--------------------------------------------------------------------------

display_heading(_('Monthly Win/Loss Trend'));

$trend_sql = "SELECT
    DATE_FORMAT(l.date_converted, '%Y-%m') as period,
    SUM(CASE WHEN l.lead_status = 'won' THEN 1 ELSE 0 END) as won,
    SUM(CASE WHEN l.lead_status = 'lost' THEN 1 ELSE 0 END) as lost,
    SUM(CASE WHEN l.lead_status = 'won' THEN l.expected_revenue ELSE 0 END) as won_rev,
    SUM(CASE WHEN l.lead_status = 'lost' THEN l.expected_revenue ELSE 0 END) as lost_rev
    FROM " . TB_PREF . "crm_leads l
    WHERE l.lead_status IN ('won','lost') AND l.inactive = 0 AND l.date_converted IS NOT NULL" . $date_where . $team_where . "
    GROUP BY DATE_FORMAT(l.date_converted, '%Y-%m')
    ORDER BY period DESC
    LIMIT 12";

$result = db_query($trend_sql);

start_table(TABLESTYLE, "width='95%'");
$th = array(_('Month'), _('Won'), _('Lost'), _('Win Rate'), _('Won Revenue'), _('Lost Revenue'));
table_header($th);

$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['period']);
    label_cell((int)$row['won'], 'align=right');
    label_cell((int)$row['lost'], 'align=right');
    $m_total = (int)$row['won'] + (int)$row['lost'];
    $m_rate = $m_total > 0 ? round(((int)$row['won'] / $m_total) * 100, 1) : 0;
    label_cell($m_rate . '%', 'align=right');
    amount_cell($row['won_rev']);
    amount_cell($row['lost_rev']);
    end_row();
}
end_table(1);

end_form();
end_page();

