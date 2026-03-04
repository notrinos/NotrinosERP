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
$page_security = 'SA_LEAVEINQUIRY';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
include_once($path_to_root . '/hrm/includes/db/leave_balance_db.inc');
page(_("Leave Balance"));

start_form();
start_table(TABLESTYLE2);

if (!isset($_POST['fiscal_year']) || (int)$_POST['fiscal_year'] <= 0)
    $_POST['fiscal_year'] = date('Y');

text_row(_('Fiscal Year:'), 'fiscal_year', $_POST['fiscal_year'], 6, 4);
employees_list_row(_('Employee:'), 'employee_id', null, true, false, false);
leave_types_list_row(_('Leave Type:'), 'leave_id', null, true, false);
end_table(1);
submit_center('Search', _('Search'));

$fiscal_year = (int)get_post('fiscal_year', date('Y'));
$employee_id = get_post('employee_id', '');
$leave_id = (int)get_post('leave_id', 0);

start_table(TABLESTYLE, "width='95%'");
$th = array(_('Employee'), _('Leave Type'), _('Year'), _('Entitled'), _('Carry Forward'), _('Adjusted'), _('Taken'), _('Pending'), _('Available'));
table_header($th);

$result = get_leave_balances($fiscal_year, $employee_id, $leave_id);
$k = 0;
while ($row = db_fetch($result)) {
    $available = (float)$row['entitled'] + (float)$row['carried_forward'] + (float)$row['adjusted'] - (float)$row['taken'] - (float)$row['pending'];

    alt_table_row_color($k);
    label_cell($row['employee_id'] . ' ' . $row['employee_name']);
    label_cell($row['leave_name']);
    label_cell($row['fiscal_year']);
    qty_cell($row['entitled']);
    qty_cell($row['carried_forward']);
    qty_cell($row['adjusted']);
    qty_cell($row['taken']);
    qty_cell($row['pending']);
    qty_cell($available);
    end_row();
}
end_table(1);
end_form();

end_page();

