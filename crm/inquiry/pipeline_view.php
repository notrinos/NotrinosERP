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
 * CRM Pipeline View - Visual pipeline board (Kanban-style)
 *
 * Shows opportunities grouped by sales stage in a horizontal pipeline.
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

page(_($help_context = 'CRM Pipeline View'));

//--------------------------------------------------------------------------
// Filters
//--------------------------------------------------------------------------

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();

crm_sales_team_list_cells(null, 'filter_team', null, true, _('All Teams'));
crm_assignee_list_cells(null, 'filter_user', null, false, _('All Assignees'));
submit_cells('Refresh', _('Apply Filter'), '', _('Apply filter'), 'default');

end_row();
end_table(1);

//--------------------------------------------------------------------------
// Load stages and opportunities
//--------------------------------------------------------------------------

if (get_post('Refresh'))
    $Ajax->activate('_page_body');

$stages_result = db_query("SELECT * FROM " . TB_PREF . "crm_sales_stages WHERE active = 1 ORDER BY sequence");
$stages = array();
while ($s = db_fetch($stages_result)) {
    $stages[$s['id']] = $s;
}

// Build filters for opportunities
$filters = array('is_opportunity' => 1);
$f_team = get_post('filter_team', 0);
$f_user = (int)get_post('filter_user', 0);
if ($f_team > 0) $filters['sales_team_id'] = (int)$f_team;
if ($f_user > 0) $filters['assigned_to'] = $f_user;

$opps_result = get_crm_leads($filters, 500, 0);
$opps_by_stage = array();
foreach ($stages as $sid => $st) {
    $opps_by_stage[$sid] = array();
}
$opps_by_stage[0] = array(); // unassigned stage

while ($opp = db_fetch($opps_result)) {
    $sid = (int)$opp['stage_id'];
    if (!isset($opps_by_stage[$sid])) $sid = 0;
    $opps_by_stage[$sid][] = $opp;
}

//--------------------------------------------------------------------------
// Render Pipeline Board
//--------------------------------------------------------------------------

$total_pipeline = 0;
$weighted_pipeline = 0;

echo "<div class='crm-pipeline-board' style='display:flex; gap:10px; overflow-x:auto; padding:10px 0;'>";

foreach ($stages as $sid => $stage) {
    $stage_opps = $opps_by_stage[$sid];
    $stage_total = 0;
    foreach ($stage_opps as $o) {
        $stage_total += (float)$o['expected_revenue'];
    }
    $total_pipeline += $stage_total;
    $weighted_pipeline += $stage_total * ((int)$stage['probability'] / 100);

    echo "<div class='crm-pipeline-stage' data-stage-id='" . (int)$sid . "' style='min-width:220px; max-width:280px; flex:1; background:#f5f5f5; border-radius:6px; padding:8px;'>";
    echo "<div style='font-weight:bold; padding:4px 0; border-bottom:2px solid #4CAF50; margin-bottom:6px;'>";
    echo htmlspecialchars($stage['name']);
    echo " <span style='font-size:0.85em; color:#666;'>(" . count($stage_opps) . ")</span>";
    echo "</div>";
    echo "<div style='font-size:0.8em; color:#888; margin-bottom:6px;'>";
    echo _('Total') . ': ' . price_format($stage_total);
    echo " &middot; " . (int)$stage['probability'] . '%';
    echo "</div>";

    foreach ($stage_opps as $opp) {
        echo "<div class='crm-pipeline-card' data-lead-id='" . (int)$opp['id'] . "' style='background:#fff; border:1px solid #ddd; border-radius:4px; padding:8px; margin-bottom:6px; cursor:pointer;'";
        echo " onclick=\"window.location='" . $path_to_root . "/crm/transactions/opportunity_entry.php?LeadID=" . (int)$opp['id'] . crm_sel_app_param() . "'\">";
        echo "<div style='font-weight:bold; font-size:0.9em;'>" . htmlspecialchars($opp['title']) . "</div>";
        if ($opp['company_name']) {
            echo "<div style='font-size:0.8em; color:#666;'>" . htmlspecialchars($opp['company_name']) . "</div>";
        }
        echo "<div style='font-size:0.8em; margin-top:3px;'>";
        echo price_format($opp['expected_revenue']);
        echo " &nbsp; ";
        echo crm_priority_badge($opp['priority']);
        echo "</div>";
        if ($opp['expected_close_date']) {
            echo "<div style='font-size:0.75em; color:#888;'>" . _('Close') . ': ' . sql2date($opp['expected_close_date']) . "</div>";
        }
        echo "</div>"; // card
    }

    echo "</div>"; // stage column
}

echo "</div>"; // board

//--------------------------------------------------------------------------
// Summary Bar
//--------------------------------------------------------------------------

echo "<div style='margin-top:15px; padding:10px; background:#e8f5e9; border-radius:4px;'>";
echo "<strong>" . _('Pipeline Summary') . ":</strong> ";
echo _('Total') . ": " . price_format($total_pipeline);
echo " &nbsp;|&nbsp; ";
echo _('Weighted') . ": " . price_format($weighted_pipeline);

$total_opps = 0;
foreach ($opps_by_stage as $arr) $total_opps += count($arr);
echo " &nbsp;|&nbsp; ";
echo _('Opportunities') . ": " . $total_opps;
echo "</div>";

end_form();

// Include CRM JS (common + pipeline drag-drop)
crm_page_scripts();
echo "<script src='" . $path_to_root . "/crm/js/crm_pipeline.js?v=" . filemtime($path_to_root . '/crm/js/crm_pipeline.js') . "'></script>";

end_page();

