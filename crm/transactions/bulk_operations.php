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
 * CRM Bulk Operations on Leads/Opportunities
 *
 * Supports: bulk assign, bulk tag, bulk status change, bulk delete
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_LEAD';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/crm/includes/crm_constants.inc');
include_once($path_to_root . '/crm/includes/db/crm_settings_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_leads_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_teams_db.inc');
include_once($path_to_root . '/crm/includes/ui/crm_ui.inc');

page(_($help_context = 'CRM Bulk Operations'));

//--------------------------------------------------------------------------
// Handle Bulk Actions
//--------------------------------------------------------------------------

if (isset($_POST['ApplyBulk'])) {
    $action = $_POST['bulk_action'];
    $ids = array();

    // Collect checked lead IDs
    foreach ($_POST as $key => $val) {
        if (strpos($key, 'sel_') === 0 && $val) {
            $ids[] = (int)substr($key, 4);
        }
    }

    if (empty($ids)) {
        display_error(_('No leads selected.'));
    } elseif ($action == '') {
        display_error(_('Please select an action.'));
    } else {
        begin_transaction();
        $count = 0;

        switch ($action) {
            case 'assign':
                $assign_to = (int)$_POST['bulk_assign_to'];
                foreach ($ids as $id) {
                    $sql = "UPDATE " . TB_PREF . "crm_leads SET assigned_to = " . db_escape($assign_to) . " WHERE id = " . db_escape($id);
                    db_query($sql);
                    $count++;
                }
                display_notification(sprintf(_('%d lead(s) assigned.'), $count));
                break;

            case 'status':
                $new_status = $_POST['bulk_status'];
                foreach ($ids as $id) {
                    $sql = "UPDATE " . TB_PREF . "crm_leads SET lead_status = " . db_escape($new_status) . " WHERE id = " . db_escape($id);
                    db_query($sql);
                    $count++;
                }
                display_notification(sprintf(_('%d lead(s) status updated.'), $count));
                break;

            case 'tag':
                $tag_id = (int)$_POST['bulk_tag_id'];
                if ($tag_id > 0) {
                    foreach ($ids as $id) {
                        $record_id = CRM_ENTITY_LEAD . ':' . (int)$id;
                        $check = "SELECT tag_id FROM " . TB_PREF . "tag_associations WHERE record_id = " . db_escape($record_id) . " AND tag_id = " . db_escape($tag_id);
                        $exists = db_query($check);
                        if (!db_fetch($exists)) {
                            $sql = "INSERT INTO " . TB_PREF . "tag_associations (record_id, tag_id) VALUES (" . db_escape($record_id) . ", " . db_escape($tag_id) . ")";
                            db_query($sql);
                        }
                        $count++;
                    }
                    display_notification(sprintf(_('Tag applied to %d lead(s).'), $count));
                }
                break;

            case 'delete':
                foreach ($ids as $id) {
                    delete_crm_lead($id, false); // soft delete
                    $count++;
                }
                display_notification(sprintf(_('%d lead(s) deleted.'), $count));
                break;
        }

        commit_transaction();
    }
}

//--------------------------------------------------------------------------
// Filters
//--------------------------------------------------------------------------

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();

crm_lead_status_list_cells(null, 'filter_status', null, true);
crm_lead_source_list_cells(null, 'filter_source', null, true, _('All Sources'));
crm_sales_team_list_cells(null, 'filter_team', null, true, _('All Teams'));

$show_opps = array(0 => _('Leads Only'), 1 => _('Opportunities Only'), 2 => _('All'));
crm_filter_array_list_cells(null, 'filter_type', $show_opps, null, true, _('All'), 2);

submit_cells('RefreshList', _('Apply Filter'), '', _('Apply filter'), 'default');

end_row();
end_table(1);

//--------------------------------------------------------------------------
// Bulk Action Bar
//--------------------------------------------------------------------------

display_heading(_('Bulk Action'));
start_table(TABLESTYLE_NOBORDER);
start_row();

$actions = array(
    ''       => _('-- Select Action --'),
    'assign' => _('Assign To'),
    'status' => _('Change Status'),
    'tag'    => _('Apply Tag'),
    'delete' => _('Delete (soft)'),
);
crm_filter_array_list_cells(_('Action'), 'bulk_action', $actions, get_post('bulk_action', ''));

// Assign To
crm_assignee_list_cells(_('Assign To'), 'bulk_assign_to', get_post('bulk_assign_to', 0), false, _('-- User --'));

// Status
crm_filter_array_list_cells(_('Status'), 'bulk_status', crm_lead_statuses(), get_post('bulk_status', ''),
    false, _('-- Status --'), '');

// Tag
$tag_items = array();
$tags_result = get_crm_tags();
while ($t = db_fetch($tags_result)) {
    $tag_items[(int)$t['id']] = $t['name'];
}
crm_filter_array_list_cells(_('Tag'), 'bulk_tag_id', $tag_items, get_post('bulk_tag_id', 0),
    false, _('-- Tag --'), 0);

submit_cells('ApplyBulk', _('Apply'), '', '', 'default');

end_row();
end_table(1);

//--------------------------------------------------------------------------
// Lead List with Checkboxes
//--------------------------------------------------------------------------

$filters = array();
$f_status = get_post('filter_status', '');
$f_source = get_post('filter_source', '');
$f_team   = get_post('filter_team', '');
$f_type   = (int)get_post('filter_type', 0);

if ($f_status !== '' && $f_status != -1) $filters['lead_status'] = (int)$f_status;
if ($f_source !== '' && $f_source != 0)  $filters['lead_source_id'] = (int)$f_source;
if ($f_team !== '' && $f_team != 0)      $filters['sales_team_id'] = (int)$f_team;
if ($f_type == 0) $filters['is_opportunity'] = 0;
elseif ($f_type == 1) $filters['is_opportunity'] = 1;

start_table(TABLESTYLE, "width='100%'");
$th = array('', _('Ref'), _('Name'), _('Organization'), _('Source'), _('Status'), _('Priority'), _('Assigned To'));
table_header($th);

$result = get_crm_leads($filters, 200, 0);
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    check_cells(null, 'sel_' . $row['id'], false);
    label_cell($row['lead_ref']);
    label_cell($row['title']);
    label_cell($row['company_name']);
    label_cell($row['source_name']);
    label_cell(crm_status_badge($row['lead_status'], crm_lead_statuses()));
    label_cell(crm_priority_badge($row['priority']));
    label_cell(@$row['assigned_name'] ?: '-');
    end_row();
}
end_table(1);

end_form();
crm_page_scripts();
end_page();

