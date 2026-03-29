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
$page_security = 'SA_LEAVEPOLICY';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
include_once($path_to_root . '/hrm/includes/db/leave_policy_db.inc');

page(_("Leave Policies"));

simple_page_mode(false);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    if (trim($_POST['policy_name']) == '') {
        display_error(_('Policy name is required.'));
        set_focus('policy_name');
    } elseif ((int)$_POST['leave_id'] <= 0) {
        display_error(_('Leave type is required.'));
        set_focus('leave_id');
    } elseif (!is_date($_POST['effective_from'])) {
        display_error(_('Effective from date is invalid.'));
        set_focus('effective_from');
    } elseif (trim($_POST['effective_to']) != '' && !is_date($_POST['effective_to'])) {
        display_error(_('Effective to date is invalid.'));
        set_focus('effective_to');
    } else {
        $grade_id = (int)$_POST['grade_id'];
        if ($grade_id <= 0)
            $grade_id = null;

        $employment_type = ($_POST['employment_type'] === '' || $_POST['employment_type'] === ALL_TEXT)
            ? null
            : (int)$_POST['employment_type'];

        $effective_to = trim($_POST['effective_to']) == '' ? null : $_POST['effective_to'];

        if ($selected_id != '') {
            update_leave_policy(
                $selected_id,
                $_POST['policy_name'],
                (int)$_POST['leave_id'],
                $grade_id,
                $employment_type,
                input_num('annual_entitlement'),
                (int)$_POST['accrual_method'],
                empty($_POST['probation_applicable']) ? 0 : 1,
                (int)$_POST['min_service_months'],
                $_POST['effective_from'],
                $effective_to,
                0
            );
            display_notification(_('Leave policy has been updated.'));
        } else {
            add_leave_policy(
                $_POST['policy_name'],
                (int)$_POST['leave_id'],
                $grade_id,
                $employment_type,
                input_num('annual_entitlement'),
                (int)$_POST['accrual_method'],
                empty($_POST['probation_applicable']) ? 0 : 1,
                (int)$_POST['min_service_months'],
                $_POST['effective_from'],
                $effective_to
            );
            display_notification(_('Leave policy has been added.'));
        }

        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    delete_leave_policy($selected_id);
    display_notification(_('Selected leave policy has been deleted.'));
    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['selected_id'] = '';
    $_POST['policy_name'] = '';
    $_POST['leave_id'] = 0;
    $_POST['grade_id'] = 0;
    $_POST['employment_type'] = '';
    $_POST['annual_entitlement'] = 0;
    $_POST['accrual_method'] = 0;
    $_POST['probation_applicable'] = 0;
    $_POST['min_service_months'] = 0;
    $_POST['effective_from'] = Today();
    $_POST['effective_to'] = '';
}

start_form();

start_table(TABLESTYLE, "width='95%'");
$th = array(_('ID'), _('Policy Name'), _('Leave Type'), _('Grade'), _('Entitlement'), _('Accrual'), _('Effective From'), _('Effective To'), '', '');
table_header($th);

$result = get_leave_policies(check_value('show_inactive'));
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['policy_id']);
    label_cell($row['policy_name']);
    label_cell($row['leave_name']);
    label_cell($row['grade_name']);
    qty_cell($row['annual_entitlement']);
    label_cell($row['accrual_method'] == 0 ? _('Annual') : ($row['accrual_method'] == 1 ? _('Monthly') : _('Quarterly')));
    label_cell(sql2date($row['effective_from']));
    label_cell(empty($row['effective_to']) ? '' : sql2date($row['effective_to']));
    edit_button_cell('Edit' . $row['policy_id'], _('Edit'));
    delete_button_cell('Delete' . $row['policy_id'], _('Delete'));
    end_row();
}
end_table(1);

start_table(TABLESTYLE2);
if ($selected_id != '' && $Mode == 'Edit') {
    $myrow = get_leave_policy($selected_id);
    $_POST['policy_name'] = $myrow['policy_name'];
    $_POST['leave_id'] = $myrow['leave_id'];
    $_POST['grade_id'] = (int)$myrow['grade_id'];
    $_POST['employment_type'] = is_null($myrow['employment_type']) ? '' : (string)$myrow['employment_type'];
    $_POST['annual_entitlement'] = qty_format($myrow['annual_entitlement']);
    $_POST['accrual_method'] = (int)$myrow['accrual_method'];
    $_POST['probation_applicable'] = (int)$myrow['probation_applicable'];
    $_POST['min_service_months'] = (int)$myrow['min_service_months'];
    $_POST['effective_from'] = sql2date($myrow['effective_from']);
    $_POST['effective_to'] = empty($myrow['effective_to']) ? '' : sql2date($myrow['effective_to']);
    hidden('selected_id', $selected_id);
}

text_row_ex(_('Policy Name:'), 'policy_name', 50, 100);
leave_types_list_row(_('Leave Type:'), 'leave_id', null, false, false);
grades_list_row(_('Grade (optional):'), 'grade_id', null, _('-- All Grades --'));
employment_type_list_row(_('Employment Type (optional):'), 'employment_type');
amount_row(_('Annual Entitlement (days):'), 'annual_entitlement');

$accrual_methods = array(
    0 => _('Annual Grant'),
    1 => _('Monthly Accrual'),
    2 => _('Quarterly')
);
array_selector_row(_('Accrual Method:'), 'accrual_method', null, $accrual_methods);
check_row(_('Applicable during probation:'), 'probation_applicable');
small_amount_row(_('Minimum Service (months):'), 'min_service_months');
date_row(_('Effective From:'), 'effective_from');
date_row(_('Effective To:'), 'effective_to');

end_table(1);
submit_add_or_update_center($selected_id == '', '', 'both');

end_form();

end_page();

