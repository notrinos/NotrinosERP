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
include_once($path_to_root . '/includes/approval/db/approval_db.inc');
include_once($path_to_root . '/hrm/includes/hrm_db.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');

page(_("Employee Appraisals"));

/**
 * Get appraisal status labels.
 *
 * Legacy status 3 is shared by workflow rejection and submitter cancellation;
 * approval history and PAY-AUD evidence preserve the exact terminal action.
 *
 * @return array
 */
function appraisal_statuses() {
    return array(
        0 => _('Draft'),
        1 => _('Submitted'),
        2 => _('Approved'),
        3 => _('Rejected/Cancelled')
    );
}

if (!isset($_POST['period_from']))
    $_POST['period_from'] = begin_month(Today());
if (!isset($_POST['period_to']))
    $_POST['period_to'] = end_month(Today());
if (!isset($_POST['appraisal_date']))
    $_POST['appraisal_date'] = Today();

if (isset($_POST['add_appraisal'])) {
    $overall_score = input_num('overall_score', 0);
    $rating_scale = (int)input_num('rating_scale', 5);

    if (trim(get_post('employee_id')) == '' || get_post('employee_id') == ALL_TEXT) {
        display_error(_('Please select an employee.'));
        set_focus('employee_id');
    } elseif (!employee_exists_by_code(get_post('employee_id'))) {
        display_error(_('Selected employee was not found.'));
        set_focus('employee_id');
    } elseif (trim(get_post('reviewer_id', '')) !== ''
        && !employee_exists_by_code(get_post('reviewer_id'))
    ) {
        display_error(_('Selected reviewer was not found.'));
        set_focus('reviewer_id');
    } elseif (!is_date(get_post('period_from'))
        || !is_date(get_post('period_to'))
        || !is_date(get_post('appraisal_date'))
    ) {
        display_error(_('Please provide valid dates.'));
    } elseif (date1_greater_date2(get_post('period_from'), get_post('period_to'))) {
        display_error(_('Period From must be on or before Period To.'));
        set_focus('period_to');
    } elseif ($rating_scale <= 0) {
        display_error(_('Rating Scale must be greater than zero.'));
        set_focus('rating_scale');
    } elseif ($overall_score < 0 || $overall_score > $rating_scale) {
        display_error(_('Overall Score must be between zero and the Rating Scale.'));
        set_focus('overall_score');
    } else {
        begin_transaction();
        $appraisal_id = add_employee_appraisal(array(
            'employee_id' => get_post('employee_id'),
            'reviewer_id' => get_post('reviewer_id', ''),
            'period_from' => get_post('period_from'),
            'period_to' => get_post('period_to'),
            'appraisal_date' => get_post('appraisal_date'),
            'overall_score' => $overall_score,
            'rating_scale' => $rating_scale,
            'status' => 1,
            'strengths' => get_post('strengths', ''),
            'improvements' => get_post('improvements', ''),
            'recommendation' => get_post('recommendation', '')
        ));

        if (!$appraisal_id) {
            cancel_transaction();
            display_error(_('Appraisal could not be created. No changes were committed.'));
        } else {
            $approval_result = submit_employee_appraisal_for_core_approval($appraisal_id);
            if (isset($approval_result['status'])
                && $approval_result['status'] === 'pending'
            ) {
                commit_transaction();
                display_notification(_('Appraisal has been submitted to the core approval workflow.'));
            } elseif (isset($approval_result['status'])
                && $approval_result['status'] === 'auto_approved'
            ) {
                commit_transaction();
                display_notification(_('Appraisal has been automatically approved by the core approval policy.'));
            } else {
                cancel_transaction();
                display_error(_('Appraisal approval returned an unexpected result. No appraisal was created.'));
            }
        }
    }
}

foreach ($_POST as $name => $value) {
    if (strpos($name, 'Submit') === 0) {
        $appraisal_id = (int)substr($name, 6);
        if ($appraisal_id > 0) {
            $result = reconcile_pending_employee_appraisal_approval($appraisal_id);
            if (isset($result['message'])
                && isset($result['status'])
                && in_array(
                    $result['status'],
                    array('pending', 'auto_approved', 'already_pending'),
                    true
                )
            ) {
                display_notification($result['message']);
            } else {
                display_error(isset($result['message'])
                    ? $result['message']
                    : _('Appraisal could not be submitted to core approval workflow.'));
            }
        }
    }

    if (strpos($name, 'Cancel') === 0) {
        $appraisal_id = (int)substr($name, 6);
        if ($appraisal_id > 0) {
            $draft = find_approval_draft_for_employee_appraisal($appraisal_id);
            if (!$draft) {
                display_error(_('No pending core approval draft was found for this appraisal.'));
            } else {
                $approval_service = get_approval_workflow_service();
                $result = $approval_service->cancel(
                    (int)$draft['draft_id'],
                    _('Cancelled by the original appraisal submitter.')
                );
                if (isset($result['status']) && $result['status'] === 'cancelled') {
                    display_notification(_('The appraisal and its approval draft were cancelled.'));
                } else {
                    display_error(isset($result['message'])
                        ? $result['message']
                        : _('The appraisal approval could not be cancelled.'));
                }
            }
        }
    }
}

start_form();

start_outer_table();

table_section(1);
employees_list_row(_('Employee:'), 'employee_id', null, false, false, false);
employees_list_row(_('Reviewer:'), 'reviewer_id', null, true, false, false);
date_row(_('Period From:'), 'period_from');
date_row(_('Period To:'), 'period_to');
date_row(_('Appraisal Date:'), 'appraisal_date');
qty_row(_('Overall Score:'), 'overall_score', get_post('overall_score', 0));

table_section();
qty_row(_('Rating Scale:'), 'rating_scale', get_post('rating_scale', 5));
textarea_row(_('Strengths:'), 'strengths', get_post('strengths', ''), 50, 4);
textarea_row(_('Improvements:'), 'improvements', get_post('improvements', ''), 50, 4);
textarea_row(_('Recommendation:'), 'recommendation', get_post('recommendation', ''), 50, 4);
end_outer_table(1);
submit_center('add_appraisal', _('Add Appraisal'));

br();
start_table(TABLESTYLE, "width='100%'");
table_header(array(
    _('ID'), _('Employee'), _('Reviewer'), _('Period'), _('Date'),
    _('Score'), _('Status'), _('Approval Action')
));
$status_labels = appraisal_statuses();
$rows = get_employee_appraisals();
$k = 0;
while ($row = db_fetch($rows)) {
    alt_table_row_color($k);
    label_cell($row['appraisal_id']);
    label_cell($row['employee_name']);
    label_cell($row['reviewer_name']);
    label_cell(sql2date($row['period_from']).' - '.sql2date($row['period_to']));
    label_cell(sql2date($row['appraisal_date']));
    qty_cell($row['overall_score']);

    $status = (int)$row['status'];
    $pending_draft = in_array($status, array(0, 1), true)
        ? find_approval_draft_for_employee_appraisal((int)$row['appraisal_id'])
        : false;
    $status_text = isset($status_labels[$status])
        ? $status_labels[$status]
        : $row['status'];
    if ($pending_draft)
        $status_text .= ' ('._('Pending Core Approval').')';
    label_cell($status_text);

    if ($status === 1 && !$pending_draft) {
        submit_cells(
            'Submit'.$row['appraisal_id'],
            _('Submit For Core Approval')
        );
    } elseif ($pending_draft) {
        submit_cells(
            'Cancel'.$row['appraisal_id'],
            _('Cancel Pending Approval')
        );
    } else {
        label_cell('');
    }

    end_row();
}
end_table(1);

end_form();
end_page();
