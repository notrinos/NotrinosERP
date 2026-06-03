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
$page_security = 'SA_PAYROLLSUMMARY';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/db_pager.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/payroll_db.inc');

page(_("Payroll Summary"));

if (!isset($_POST['status_filter']))
    $_POST['status_filter'] = '';

$status_labels = array(
    '' => _('-- All Statuses --'),
    0 => _('Draft'),
    1 => _('Calculated'),
    2 => _('Approved'),
    3 => _('Posted'),
    4 => _('Paid'),
    5 => _('Closed'),
    6 => _('Voided')
);

/**
 * Format payroll status code for pager output.
 *
 * @param array $row
 * @param string $cell
 * @return string
 */
function payroll_summary_status_label($row, $cell) {
    global $status_labels;

    $status = (int)$cell;
    return isset($status_labels[$status]) ? $status_labels[$status] : $cell;
}

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();
echo "<div class = 'filter-field'>";
echo array_selector('status_filter', get_post('status_filter', ''), $status_labels);
echo "</div>";
submit_cells('Search', _('Apply Filter'));
end_row();
end_table(1);

$status = get_post('status_filter', '');

if (!payroll_table_exists('payroll_periods')) {
    display_note(_('No payroll period records found.'), 0, 1);
    end_form();
    end_page();
    return;
}

$sql = "SELECT * FROM ".TB_PREF."payroll_periods WHERE 1=1";
if ($status !== '')
    $sql .= " AND status = ".db_escape((int)$status);
$sql .= " ORDER BY from_date DESC, period_id DESC";

$cols = array(
    _('Period ID') => array('name' => 'period_id', 'ord' => 'desc'),
    _('Period Name') => array('name' => 'period_name', 'ord' => ''),
    _('From') => array('name' => 'from_date', 'type' => 'date', 'ord' => ''),
    _('To') => array('name' => 'to_date', 'type' => 'date', 'ord' => ''),
    _('Status') => array('name' => 'status', 'fun' => 'payroll_summary_status_label', 'ord' => ''),
    _('Gross') => array('name' => 'total_gross', 'type' => 'amount'),
    _('Deductions') => array('name' => 'total_deductions', 'type' => 'amount'),
    _('Net') => array('name' => 'total_net', 'type' => 'amount'),
    _('Employer Cost') => array('name' => 'total_employer_cost', 'type' => 'amount')
);

$table =& new_db_pager('payroll_summary_tbl', $sql, $cols);
$table->width = '100%';
display_db_pager($table);
end_form();

end_page();

