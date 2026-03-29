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
$page_security = 'SA_EMPLOYEE';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_db.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');

page(_("Employee Asset Allocation"));

/**
 * Get asset allocation status labels.
 *
 * @return array
 */
function asset_statuses() {
    return array(0 => _('Allocated'), 1 => _('Returned'), 2 => _('Lost'), 3 => _('Damaged'));
}

if (!isset($_POST['allocation_date']))
    $_POST['allocation_date'] = Today();

if (isset($_POST['add_allocation'])) {
    if (trim(get_post('employee_id')) == '' || get_post('employee_id') == ALL_TEXT)
        display_error(_('Please select an employee.'));
    elseif (trim(get_post('asset_name')) == '')
        display_error(_('Asset name is required.'));
    elseif (!is_date(get_post('allocation_date')))
        display_error(_('Allocation date is invalid.'));
    else {
        add_employee_asset_allocation(array(
            'employee_id' => get_post('employee_id'),
            'asset_name' => get_post('asset_name'),
            'asset_code' => get_post('asset_code', ''),
            'serial_no' => get_post('serial_no', ''),
            'allocation_date' => get_post('allocation_date'),
            'expected_return' => get_post('expected_return', ''),
            'return_date' => get_post('return_date', ''),
            'status' => get_post('asset_status', 0),
            'notes' => get_post('asset_notes', '')
        ));
        display_notification(_('Asset allocation has been added.'));
    }
}

start_form();

start_table(TABLESTYLE2, "width='80%'");
employees_list_row(_('Employee:'), 'employee_id', null, false, false, false);
text_row_ex(_('Asset Name:'), 'asset_name', 40, 140);
text_row_ex(_('Asset Code:'), 'asset_code', 30, 60);
text_row_ex(_('Serial No:'), 'serial_no', 30, 80);
date_row(_('Allocation Date:'), 'allocation_date');
date_row(_('Expected Return:'), 'expected_return');
date_row(_('Returned Date:'), 'return_date');
label_row(_('Status:'), array_selector('asset_status', get_post('asset_status', 0), asset_statuses()));
textarea_row(_('Notes:'), 'asset_notes', get_post('asset_notes', ''), 50, 2);
end_table(1);
submit_center('add_allocation', _('Add Allocation'));

start_table(TABLESTYLE, "width='98%'");
table_header(array(_('ID'), _('Employee'), _('Asset'), _('Code'), _('Serial'), _('Allocation Date'), _('Status')));
$labels = asset_statuses();
$rows = get_employee_asset_allocations();
$k = 0;
while ($row = db_fetch($rows)) {
    alt_table_row_color($k);
    label_cell($row['allocation_id']);
    label_cell($row['employee_name']);
    label_cell($row['asset_name']);
    label_cell($row['asset_code']);
    label_cell($row['serial_no']);
    label_cell(sql2date($row['allocation_date']));
    label_cell(isset($labels[(int)$row['status']]) ? $labels[(int)$row['status']] : $row['status']);
    end_row();
}
end_table(1);

end_form();
end_page();
