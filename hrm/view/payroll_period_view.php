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
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/payroll_db.inc');
page(_("Payroll Period View"));

if (!isset($_POST['status_filter']))
    $_POST['status_filter'] = -1;

/**
 * Resolve payroll period status label.
 *
 * @param int $status
 * @return string
 */
function payroll_period_status_label($status) {
    $labels = array(
        0 => _('Draft'),
        1 => _('Calculated'),
        2 => _('Approved'),
        3 => _('Posted'),
        4 => _('Paid'),
        5 => _('Closed'),
        6 => _('Voided')
    );

    return isset($labels[(int)$status]) ? $labels[(int)$status] : (string)$status;
}

start_form();

start_table(TABLESTYLE2);
$status_options = array(
    -1 => _('All Statuses'),
    0 => _('Draft'),
    1 => _('Calculated'),
    2 => _('Approved'),
    3 => _('Posted'),
    4 => _('Paid'),
    5 => _('Closed'),
    6 => _('Voided')
);
label_row(_('Status:'), array_selector('status_filter', get_post('status_filter', -1), $status_options));
end_table(1);
submit_center('Search', _('Search'));

$status_filter = (int)get_post('status_filter', -1);
$periods = $status_filter >= 0 ? get_payroll_periods($status_filter) : get_payroll_periods();

if ($periods && db_num_rows($periods) > 0) {
    start_table(TABLESTYLE, "width='98%'");
    table_header(array(
        _('Period #'),
        _('Period Name'),
        _('From Date'),
        _('To Date'),
        _('Status'),
        _('Total Gross'),
        _('Total Deductions'),
        _('Total Net')
    ));

    $k = 0;
    while ($row = db_fetch($periods)) {
        alt_table_row_color($k);
        label_cell($row['period_id']);
        label_cell($row['period_name']);
        label_cell(sql2date($row['from_date']));
        label_cell(sql2date($row['to_date']));
        label_cell(payroll_period_status_label($row['status']));
        amount_cell($row['total_gross']);
        amount_cell($row['total_deductions']);
        amount_cell($row['total_net']);
        end_row();
    }
    end_table(1);
} else {
    display_note(_('No payroll periods found for the selected filter.'), 0, 1);
}

end_form();

end_page();

