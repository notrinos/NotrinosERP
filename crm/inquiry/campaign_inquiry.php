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
 * CRM Campaign Inquiry - View campaign performance and details
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_CAMPAIGN';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/crm/includes/crm_constants.inc');
include_once($path_to_root . '/crm/includes/db/crm_settings_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_campaigns_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_leads_db.inc');
include_once($path_to_root . '/crm/includes/ui/crm_ui.inc');

page(_($help_context = 'CRM Campaign Inquiry'));

//--------------------------------------------------------------------------
// Filters
//--------------------------------------------------------------------------

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();

$statuses = array(
    -1                     => _('All'),
    CRM_CAMPAIGN_DRAFT     => _('Draft'),
    CRM_CAMPAIGN_ACTIVE    => _('Active'),
    CRM_CAMPAIGN_COMPLETED => _('Completed'),
    CRM_CAMPAIGN_CANCELLED => _('Cancelled'),
);
echo '<td>' . _('Status:') . '</td><td>';
$sel_status = (int)get_post('filter_status', -1);
echo "<select name='filter_status'>";
foreach ($statuses as $k => $v) {
    $s = ($k == $sel_status) ? ' selected' : '';
    echo "<option value='$k'$s>" . htmlspecialchars($v) . "</option>";
}
echo "</select></td>";

$types = array('' => _('All'), 'email' => _('Email'), 'social' => _('Social'), 'event' => _('Event'), 'other' => _('Other'));
echo '<td>' . _('Type:') . '</td><td>';
$sel_type = get_post('filter_type', '');
echo "<select name='filter_type'>";
foreach ($types as $k => $v) {
    $s = ($k == $sel_type) ? ' selected' : '';
    echo "<option value='$k'$s>" . htmlspecialchars($v) . "</option>";
}
echo "</select></td>";

text_cells(_('Search:'), 'filter_search', null, 30, 100);
submit_cells('Refresh', _('Search'), '', '', 'default');

end_row();
end_table(1);

//--------------------------------------------------------------------------
// Build filter and query
//--------------------------------------------------------------------------

$where = " WHERE 1=1";
$f_status = (int)get_post('filter_status', -1);
$f_type   = get_post('filter_type', '');
$f_search = get_post('filter_search', '');

if ($f_status >= 0) $where .= " AND c.status = " . db_escape($f_status);
if ($f_type)        $where .= " AND c.campaign_type = " . db_escape($f_type);
if ($f_search)      $where .= " AND c.name LIKE " . db_escape('%' . $f_search . '%');

$sql = "SELECT c.*,
        (SELECT COUNT(*) FROM " . TB_PREF . "crm_campaign_leads cl WHERE cl.campaign_id = c.id) as lead_count,
        (SELECT COUNT(*) FROM " . TB_PREF . "crm_campaign_leads cl3 WHERE cl3.campaign_id = c.id AND cl3.status = 'converted') as converted_count
    FROM " . TB_PREF . "crm_campaigns c" . $where . " ORDER BY c.created_date DESC";

$result = db_query($sql);

//--------------------------------------------------------------------------
// Summary
//--------------------------------------------------------------------------

$total_budget = 0;
$total_leads = 0;
$campaigns = array();
while ($row = db_fetch($result)) {
    $campaigns[] = $row;
    $total_budget += (float)$row['budget'];
    $total_leads += (int)$row['lead_count'];
}

echo "<div style='margin:10px 0; padding:8px; background:#f0f0f0; border-radius:4px;'>";
echo "<strong>" . _('Summary') . ":</strong> ";
echo _('Campaigns') . ": " . count($campaigns) . " &nbsp;|&nbsp; ";
echo _('Total Budget') . ": " . price_format($total_budget) . " &nbsp;|&nbsp; ";
echo _('Total Leads Enrolled') . ": " . $total_leads;
echo "</div>";

//--------------------------------------------------------------------------
// Campaign Table
//--------------------------------------------------------------------------

start_table(TABLESTYLE, "width='95%'");
$th = array(
    _('ID'), _('Campaign Name'), _('Type'), _('Status'),
    _('Start'), _('End'), _('Budget'), _('Leads'),
    _('Converted'), _('Conv. Rate'), ''
);
table_header($th);

$k = 0;
foreach ($campaigns as $row) {
    alt_table_row_color($k);
    label_cell($row['id']);
    label_cell($row['name']);
    label_cell(ucfirst($row['campaign_type']));

    $status_labels = array(
        CRM_CAMPAIGN_DRAFT     => _('Draft'),
        CRM_CAMPAIGN_ACTIVE    => _('Active'),
        CRM_CAMPAIGN_COMPLETED => _('Completed'),
        CRM_CAMPAIGN_CANCELLED => _('Cancelled'),
    );
    label_cell(isset($status_labels[$row['status']]) ? $status_labels[$row['status']] : '');

    label_cell($row['start_date'] ? sql2date($row['start_date']) : '-');
    label_cell($row['end_date'] ? sql2date($row['end_date']) : '-');
    amount_cell($row['budget']);
    label_cell((int)$row['lead_count'], 'align=right');
    label_cell((int)$row['converted_count'], 'align=right');

    // Conversion rate
    $rate = $row['lead_count'] > 0
        ? round(($row['converted_count'] / $row['lead_count']) * 100, 1) . '%'
        : '0%';
    label_cell($rate, 'align=right');

    echo '<td><a href="' . $path_to_root . '/crm/transactions/campaign_entry.php?CampaignID=' . (int)$row['id'] . crm_sel_app_param() . '">' . _('View') . '</a></td>';
    end_row();
}
end_table(1);

end_form();
end_page();

