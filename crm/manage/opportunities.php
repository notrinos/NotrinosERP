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
 * CRM Opportunities Management (list view)
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_OPPORTUNITY';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/crm/includes/ui/crm_ui.inc');

include_crm_files();

page(_($help_context = 'CRM Opportunities'));

//--------------------------------------------------------------------------
// Filter form
//--------------------------------------------------------------------------

start_form(false, false, $_SERVER['PHP_SELF']);

start_table(TABLESTYLE_NOBORDER);
start_row();

crm_sales_stage_list_row_cells(_('Stage:'), 'filter_stage', get_post('filter_stage'), true);
crm_sales_team_list_cells(_('Team:'), 'filter_team', get_post('filter_team'), true);

echo "<td>" . _('Search:') . "</td><td>";
echo "<input type='text' name='filter_search' value='" . htmlspecialchars(get_post('filter_search', '')) . "' size='20'>";
echo "</td>";
submit_cells('Search', _('Search'), '', '', 'default');
submit_cells('Reset', _('Reset'), '', '', 'default');

end_row();
end_table();

/**
 * Stage list cells helper for filter.
 *
 * @param string $label
 * @param string $name
 * @param mixed  $selected_id
 * @param bool   $submit_on_change
 */
function crm_sales_stage_list_row_cells($label, $name, $selected_id, $submit_on_change = false)
{
    echo "<td>$label</td><td>";
    echo crm_sales_stage_list($name, $selected_id, $submit_on_change);
    echo "</td>";
}

if (isset($_POST['Reset'])) {
    $_POST['filter_stage'] = '';
    $_POST['filter_team'] = '';
    $_POST['filter_search'] = '';
}

end_form();

//--------------------------------------------------------------------------

$filters = array(
    'is_opportunity' => 1,
    'inactive' => 0,
);

if (!empty($_POST['filter_stage'])) {
    $filters['stage_id'] = $_POST['filter_stage'];
}
if (!empty($_POST['filter_team'])) {
    $filters['sales_team_id'] = $_POST['filter_team'];
}
if (!empty($_POST['filter_search'])) {
    $filters['search'] = $_POST['filter_search'];
}

$result = get_crm_leads($filters);

start_table(TABLESTYLE, "width='95%'");

$th = array(
    _('Ref'), _('Title'), _('Company'), _('Stage'), _('Probability'),
    _('Expected Revenue'), _('Close Date'), _('Team'), _('Assigned To'), ''
);
table_header($th);

$k = 0;
$total_revenue = 0;
$total_weighted = 0;

while ($myrow = db_fetch($result)) {
    alt_table_row_color($k);

    label_cell("<a href='" . $path_to_root . "/crm/transactions/opportunity_entry.php?LeadID="
        . $myrow['id'] . crm_sel_app_param() . "'>" . ($myrow['lead_ref'] ?: '#' . $myrow['id']) . "</a>");
    label_cell($myrow['title']);
    label_cell($myrow['company_name']);
    label_cell($myrow['stage_name']);
    label_cell(crm_probability_bar($myrow['probability']));
    amount_cell($myrow['expected_revenue']);
    label_cell($myrow['expected_close_date'] ? sql2date($myrow['expected_close_date']) : '-');
    label_cell($myrow['team_name'] ?: '-');

    if ($myrow['assigned_to']) {
        $sql = "SELECT real_name FROM " . TB_PREF . "users WHERE id = " . db_escape($myrow['assigned_to']);
        $u = db_fetch(db_query($sql, ""));
        label_cell($u ? $u['real_name'] : '-');
    } else {
        label_cell('-');
    }

    echo "<td><a href='" . $path_to_root . "/crm/transactions/opportunity_entry.php?LeadID="
        . $myrow['id'] . crm_sel_app_param() . "'>" . _('Edit') . "</a></td>";

    end_row();

    $total_revenue += (float)$myrow['expected_revenue'];
    $total_weighted += (float)$myrow['expected_revenue'] * (int)$myrow['probability'] / 100;
}

// Totals row
$cols = count($th);
start_row("class='inquirybg'");
label_cell('<b>' . _('Totals') . '</b>', "colspan='5'");
amount_cell($total_revenue);
label_cell('');
label_cell('');
label_cell('<b>' . _('Weighted:') . '</b> ' . price_format($total_weighted));
label_cell('');
end_row();

end_table(1);

echo "<center><a href='" . $path_to_root . "/crm/transactions/opportunity_entry.php?sel_app=crm'>"
    . "<button class='inputsubmit'>" . _('New Opportunity') . "</button></a></center>";

end_page();

