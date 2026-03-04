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
$page_security = 'SA_STATUTORY';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_constants.inc');
include_once($path_to_root . '/hrm/includes/db/statutory_db.inc');

page(_("Statutory Deductions"));

simple_page_mode(false);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    if (trim($_POST['statutory_name']) == '') {
        display_error(_('Statutory name is required.'));
        set_focus('statutory_name');
    } elseif (trim($_POST['statutory_code']) == '') {
        display_error(_('Statutory code is required.'));
        set_focus('statutory_code');
    } elseif (!is_date($_POST['effective_from'])) {
        display_error(_('Effective from date is required.'));
        set_focus('effective_from');
    } elseif (trim($_POST['effective_to']) !== '' && !is_date($_POST['effective_to'])) {
        display_error(_('Effective to date is invalid.'));
        set_focus('effective_to');
    } else {
        $effective_to = trim($_POST['effective_to']) == '' ? '' : $_POST['effective_to'];
        $ceiling = trim($_POST['ceiling_amount']) == '' ? null : input_num('ceiling_amount');
        $floor = trim($_POST['floor_amount']) == '' ? null : input_num('floor_amount');

        if ($selected_id != '') {
            update_statutory_deduction(
                $selected_id,
                $_POST['statutory_name'],
                $_POST['statutory_code'],
                input_num('employee_rate'),
                input_num('employer_rate'),
                input_num('employee_fixed'),
                input_num('employer_fixed'),
                $ceiling,
                $floor,
                (int)$_POST['calculation_base'],
                $_POST['employee_account'],
                $_POST['employer_account'],
                $_POST['effective_from'],
                $effective_to,
                check_value('inactive') ? 1 : 0
            );
            display_notification(_('Statutory deduction has been updated.'));
        } else {
            add_statutory_deduction(
                $_POST['statutory_name'],
                $_POST['statutory_code'],
                input_num('employee_rate'),
                input_num('employer_rate'),
                input_num('employee_fixed'),
                input_num('employer_fixed'),
                $ceiling,
                $floor,
                (int)$_POST['calculation_base'],
                $_POST['employee_account'],
                $_POST['employer_account'],
                $_POST['effective_from'],
                $effective_to,
                check_value('inactive') ? 1 : 0
            );
            display_notification(_('Statutory deduction has been added.'));
        }

        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    delete_statutory_deduction($selected_id);
    display_notification(_('Selected statutory deduction has been deleted.'));
    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['selected_id'] = '';
    $_POST['statutory_name'] = '';
    $_POST['statutory_code'] = '';
    $_POST['employee_rate'] = 0;
    $_POST['employer_rate'] = 0;
    $_POST['employee_fixed'] = 0;
    $_POST['employer_fixed'] = 0;
    $_POST['ceiling_amount'] = '';
    $_POST['floor_amount'] = '';
    $_POST['calculation_base'] = HRM_STAT_BASE_BASIC;
    $_POST['employee_account'] = '';
    $_POST['employer_account'] = '';
    $_POST['effective_from'] = Today();
    $_POST['effective_to'] = '';
    $_POST['inactive'] = 0;
}

start_form();

start_table(TABLESTYLE, "width='95%'");
$th = array(_('ID'), _('Name'), _('Code'), _('Emp Rate'), _('Er Rate'), _('Base'), _('Effective From'), _('Inactive'), '', '');
table_header($th);

$result = get_statutory_deductions(true);
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['statutory_id']);
    label_cell($row['statutory_name']);
    label_cell($row['statutory_code']);
    qty_cell($row['employee_rate']);
    qty_cell($row['employer_rate']);
    label_cell((int)$row['calculation_base'] == HRM_STAT_BASE_GROSS ? _('Gross') : _('Basic'));
    label_cell(sql2date($row['effective_from']));
    label_cell($row['inactive'] ? _('Yes') : _('No'));
    edit_button_cell('Edit'.$row['statutory_id'], _('Edit'));
    delete_button_cell('Delete'.$row['statutory_id'], _('Delete'));
    end_row();
}
end_table(1);

start_table(TABLESTYLE2);
if ($selected_id != '' && $Mode == 'Edit') {
    $myrow = get_statutory_deduction($selected_id);
    $_POST['statutory_name'] = $myrow['statutory_name'];
    $_POST['statutory_code'] = $myrow['statutory_code'];
    $_POST['employee_rate'] = qty_format($myrow['employee_rate']);
    $_POST['employer_rate'] = qty_format($myrow['employer_rate']);
    $_POST['employee_fixed'] = qty_format($myrow['employee_fixed']);
    $_POST['employer_fixed'] = qty_format($myrow['employer_fixed']);
    $_POST['ceiling_amount'] = is_null($myrow['ceiling_amount']) ? '' : qty_format($myrow['ceiling_amount']);
    $_POST['floor_amount'] = is_null($myrow['floor_amount']) ? '' : qty_format($myrow['floor_amount']);
    $_POST['calculation_base'] = (int)$myrow['calculation_base'];
    $_POST['employee_account'] = $myrow['employee_account'];
    $_POST['employer_account'] = $myrow['employer_account'];
    $_POST['effective_from'] = sql2date($myrow['effective_from']);
    $_POST['effective_to'] = empty($myrow['effective_to']) ? '' : sql2date($myrow['effective_to']);
    $_POST['inactive'] = (int)$myrow['inactive'];
    hidden('selected_id', $selected_id);
}

text_row_ex(_('Name:'), 'statutory_name', 40, 100);
text_row_ex(_('Code:'), 'statutory_code', 20, 20);
amount_row(_('Employee Rate (%):'), 'employee_rate');
amount_row(_('Employer Rate (%):'), 'employer_rate');
amount_row(_('Employee Fixed:'), 'employee_fixed');
amount_row(_('Employer Fixed:'), 'employer_fixed');
amount_row(_('Ceiling Amount:'), 'ceiling_amount');
amount_row(_('Floor Amount:'), 'floor_amount');
array_selector_row(_('Calculation Base:'), 'calculation_base', null, array(HRM_STAT_BASE_BASIC => _('Basic Salary'), HRM_STAT_BASE_GROSS => _('Gross Salary')));
gl_all_accounts_list_row(_('Employee Account:'), 'employee_account', null, true, true, _('Optional'));
gl_all_accounts_list_row(_('Employer Account:'), 'employer_account', null, true, true, _('Optional'));
date_row(_('Effective From:'), 'effective_from');
date_row(_('Effective To:'), 'effective_to');
check_row(_('Inactive:'), 'inactive');

end_table(1);
submit_add_or_update_center($selected_id == '', '', 'both');
end_form();

end_page();

