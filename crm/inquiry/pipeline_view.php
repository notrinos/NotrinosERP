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
include_once($path_to_root . '/crm/includes/ui/crm_pipeline_kanban.inc');

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
// Render Pipeline Board using core kanban_board class
//--------------------------------------------------------------------------

$pipeline = new crm_pipeline_kanban($path_to_root);
$pipeline->load_pipeline_data($stages, $opps_by_stage);
$pipeline->render();

end_form();

// Include CRM JS (common + pipeline drag-drop)
crm_page_scripts();
echo "<script src='" . $path_to_root . "/crm/js/crm_pipeline.js?v=" . filemtime($path_to_root . '/crm/js/crm_pipeline.js') . "'></script>";

end_page();

