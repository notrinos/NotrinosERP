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
include_once($path_to_root . '/includes/db_pager.inc');
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
include_once($path_to_root . '/hrm/includes/db/leave_balance_db.inc');

page(_("Leave Balance"));

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();

years_list_cells(_('Fiscal Year:'), 'fiscal_year', null);
employees_list_cells(_('Employee:'), 'employee_id', null, true, false, false);
filter_cell_open(_('Leave Type:'));
echo leave_types_list('leave_id', null, true, false);
filter_cell_close();
submit_cells('Search', _('Apply Filter'));
end_row();
end_table(1);

$default_fiscal_year = get_leave_balance_fiscal_year_for_date(date('Y-m-d'));
$fiscal_year = (int)get_post('fiscal_year', $default_fiscal_year);
$employee_id = get_post('employee_id', '');
$leave_id = (int)get_post('leave_id', 0);

ensure_leave_balance_entitlements_for_filters($fiscal_year, $employee_id, $leave_id);

$where = array('1=1');
if ($fiscal_year > 0)
    $where[] = 'lb.fiscal_year = '.db_escape($fiscal_year);
if ($employee_id !== '' && $employee_id !== ALL_TEXT)
    $where[] = 'lb.employee_id = '.db_escape($employee_id);
if ($leave_id > 0)
    $where[] = 'lb.leave_id = '.db_escape($leave_id);

$sql = "SELECT CONCAT(lb.employee_id, ' ', TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,'')))) employee_label,
        lt.leave_name,
        lb.fiscal_year,
        lb.entitled,
        lb.carried_forward,
        lb.adjusted,
        lb.taken,
        lb.pending,
        (IFNULL(lb.entitled,0) + IFNULL(lb.carried_forward,0) + IFNULL(lb.adjusted,0) - IFNULL(lb.taken,0) - IFNULL(lb.pending,0)) available
    FROM ".TB_PREF."leave_balances lb
    LEFT JOIN ".TB_PREF."leave_types lt ON lt.leave_id = lb.leave_id
    LEFT JOIN ".TB_PREF."employees e ON e.employee_id = lb.employee_id
    WHERE ".implode(' AND ', $where)."
    ORDER BY lb.employee_id, lb.fiscal_year DESC, lb.leave_id";

$cols = array(
    _('Employee') => array('name' => 'employee_label', 'ord' => 'asc'),
    _('Leave Type') => array('name' => 'leave_name', 'ord' => ''),
    _('Year') => array('name' => 'fiscal_year', 'ord' => ''),
    _('Entitled') => array('name' => 'entitled', 'type' => 'qty'),
    _('Carry Forward') => array('name' => 'carried_forward', 'type' => 'qty'),
    _('Adjusted') => array('name' => 'adjusted', 'type' => 'qty'),
    _('Taken') => array('name' => 'taken', 'type' => 'qty'),
    _('Pending') => array('name' => 'pending', 'type' => 'qty'),
    _('Available') => array('name' => 'available', 'type' => 'qty')
);

$table =& new_db_pager('leave_balance_inquiry_tbl', $sql, $cols);
$table->width = '100%';
display_db_pager($table);
end_form();

end_page();
