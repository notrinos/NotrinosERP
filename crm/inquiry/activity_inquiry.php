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
 * CRM Activity Inquiry - View and filter activities across all entities
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_ACTIVITY';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/crm/includes/crm_constants.inc');
include_once($path_to_root . '/crm/includes/db/crm_settings_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_activities_db.inc');
include_once($path_to_root . '/crm/includes/ui/crm_ui.inc');

page(_($help_context = 'CRM Activity Inquiry'));

// Mark overdue activities on page load
mark_overdue_crm_activities();

//--------------------------------------------------------------------------
// Filters
//--------------------------------------------------------------------------

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();

crm_activity_type_list_cells(_('Type:'), 'filter_type', null, true);

$statuses = array(
    -1                    => _('All'),
    CRM_ACTIVITY_PLANNED  => _('Planned'),
    CRM_ACTIVITY_DONE     => _('Done'),
    CRM_ACTIVITY_CANCELLED => _('Cancelled'),
    CRM_ACTIVITY_OVERDUE  => _('Overdue'),
);
echo '<td>' . _('Status:') . '</td><td>';
$sel_status = get_post('filter_status', -1);
echo "<select name='filter_status'>";
foreach ($statuses as $k => $v) {
    $s = ($k == $sel_status) ? ' selected' : '';
    echo "<option value='$k'$s>" . htmlspecialchars($v) . "</option>";
}
echo "</select></td>";

crm_priority_list_cells(_('Priority:'), 'filter_priority', null, true);

// Assigned To
echo '<td>' . _('Assigned To:') . '</td><td>';
$users_sql = "SELECT id, real_name FROM " . TB_PREF . "users ORDER BY real_name";
$users_result = db_query($users_sql);
echo "<select name='filter_user'>";
echo "<option value='0'>" . _('All') . "</option>";
while ($u = db_fetch($users_result)) {
    $sel = (get_post('filter_user') == $u['id']) ? ' selected' : '';
    echo "<option value='" . (int)$u['id'] . "'$sel>" . htmlspecialchars($u['real_name']) . "</option>";
}
echo "</select></td>";

submit_cells('RefreshList', _('Search'), '', '', 'default');

end_row();
end_table(1);

// Date range
start_table(TABLESTYLE_NOBORDER);
start_row();
date_cells(_('Due From:'), 'filter_from', '', null, -30, 0, 0);
date_cells(_('To:'), 'filter_to', '', null, 0, 0, 0);

// Show only my activities
check_cells(_('My Activities Only:'), 'filter_mine', null);

end_row();
end_table(1);

//--------------------------------------------------------------------------
// Build filters
//--------------------------------------------------------------------------

$filters = array();

$f_type     = get_post('filter_type', '');
$f_status   = (int)get_post('filter_status', -1);
$f_priority = get_post('filter_priority', '');
$f_user     = (int)get_post('filter_user', 0);
$f_from     = get_post('filter_from', '');
$f_to       = get_post('filter_to', '');
$f_mine     = check_value('filter_mine');

if ($f_type !== '' && $f_type != 0)        $filters['activity_type_id'] = (int)$f_type;
if ($f_status >= 0)                         $filters['status'] = $f_status;
if ($f_priority !== '' && $f_priority != -1) $filters['priority'] = (int)$f_priority;
if ($f_user > 0)                            $filters['assigned_to'] = $f_user;
if ($f_from && is_date($f_from))           $filters['date_from'] = date2sql($f_from);
if ($f_to && is_date($f_to))              $filters['date_to'] = date2sql($f_to);
if ($f_mine)                                $filters['assigned_to'] = $_SESSION['wa_current_user']->user;

//--------------------------------------------------------------------------
// Summary counts
//--------------------------------------------------------------------------

$count_sql = "SELECT status, COUNT(*) as cnt FROM " . TB_PREF . "crm_activities GROUP BY status";
$count_res = db_query($count_sql, "Could not count activities");
$counts = array();
while ($crow = db_fetch($count_res)) {
    $counts[$crow['status']] = (int)$crow['cnt'];
}

echo "<div style='margin:10px 0; padding:8px; background:#f0f0f0; border-radius:4px;'>";
echo "<strong>" . _('Summary') . ":</strong> ";
echo _('Planned') . ": " . (int)@$counts[CRM_ACTIVITY_PLANNED] . " &nbsp;|&nbsp; ";
echo _('Overdue') . ": <span style='color:red;'>" . (int)@$counts[CRM_ACTIVITY_OVERDUE] . "</span> &nbsp;|&nbsp; ";
echo _('Done') . ": " . (int)@$counts[CRM_ACTIVITY_DONE] . " &nbsp;|&nbsp; ";
echo _('Cancelled') . ": " . (int)@$counts[CRM_ACTIVITY_CANCELLED];
echo "</div>";

//--------------------------------------------------------------------------
// Activity Table
//--------------------------------------------------------------------------

start_table(TABLESTYLE, "width='95%'");
$th = array(
    _('ID'), _('Type'), _('Subject'), _('Entity'), _('Due Date'), _('Time'),
    _('Priority'), _('Status'), _('Assigned To'), _('Created'), ''
);
table_header($th);

$result = get_crm_activities($filters, 200, 0);
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['id']);
    label_cell($row['type_name']);
    label_cell($row['summary']);

    // Entity link
    $entity_label = '';
    if ($row['entity_type'] == CRM_ENTITY_LEAD && $row['entity_id'] > 0) {
        $entity_label = _('Lead') . ' #' . $row['entity_id'];
    } elseif ($row['entity_type'] == CRM_ENTITY_CUSTOMER && $row['entity_id'] > 0) {
        $entity_label = _('Customer') . ' #' . $row['entity_id'];
    }
    label_cell($entity_label);

    label_cell(sql2date(substr($row['date_scheduled'], 0, 10)));
    $time_part = substr($row['date_scheduled'], 11, 5);
    label_cell($time_part ? $time_part : '-');
    label_cell(crm_priority_badge(@$row['priority']));
    label_cell(crm_activity_status_badge($row['status']));
    label_cell(@$row['assigned_name'] ?: '-');
    label_cell(sql2date(substr($row['created_date'], 0, 10)));

    // Edit link
    if ((int)$row['status'] == CRM_ACTIVITY_PLANNED || (int)$row['status'] == CRM_ACTIVITY_OVERDUE) {
        echo '<td><a href="' . $path_to_root . '/crm/transactions/schedule_activity.php?';
        if ($row['entity_type'] && $row['entity_id']) {
            echo 'entity_type=' . urlencode($row['entity_type']) . '&entity_id=' . (int)$row['entity_id'];
        }
        echo crm_sel_app_param() . '">' . _('Edit') . '</a></td>';
    } else {
        label_cell('');
    }
    end_row();
}
end_table(1);

end_form();
end_page();

