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

page(_("Payroll Summary"));

if (!isset($_POST['status_filter']))
    $_POST['status_filter'] = '';

$status_labels = array(
    '' => _('-- All --'),
    0 => _('Draft'),
    1 => _('Calculated'),
    2 => _('Approved'),
    3 => _('Posted'),
    4 => _('Paid'),
    5 => _('Closed'),
    6 => _('Voided')
);

start_form();
start_table(TABLESTYLE2);
array_selector_row(_('Status:'), 'status_filter', get_post('status_filter', ''), $status_labels);
end_table(1);
submit_center('Search', _('Search'));

$status = get_post('status_filter', '');
if ($status === '')
    $result = get_payroll_periods();
else
    $result = get_payroll_periods((int)$status);

start_table(TABLESTYLE, "width='95%'");
$th = array(_('Period ID'), _('Period Name'), _('From'), _('To'), _('Status'), _('Gross'), _('Deductions'), _('Net'), _('Employer Cost'));
table_header($th);

$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['period_id']);
    label_cell($row['period_name']);
    label_cell(sql2date($row['from_date']));
    label_cell(sql2date($row['to_date']));
    label_cell(isset($status_labels[(int)$row['status']]) ? $status_labels[(int)$row['status']] : $row['status']);
    amount_cell($row['total_gross']);
    amount_cell($row['total_deductions']);
    amount_cell($row['total_net']);
    amount_cell($row['total_employer_cost']);
    end_row();
}

end_table(1);
end_form();

end_page();

