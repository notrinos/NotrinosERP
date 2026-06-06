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
 * @return array
 */
function appraisal_statuses() {
    return array(
        0 => _('Draft'),
        1 => _('Submitted'),
        2 => _('Approved'),
        3 => _('Rejected')
    );
}

if (!isset($_POST['period_from']))
    $_POST['period_from'] = begin_month(Today());
if (!isset($_POST['period_to']))
    $_POST['period_to'] = end_month(Today());
if (!isset($_POST['appraisal_date']))
    $_POST['appraisal_date'] = Today();

if (isset($_POST['add_appraisal'])) {
    if (trim(get_post('employee_id')) == '' || get_post('employee_id') == ALL_TEXT)
        display_error(_('Please select an employee.'));
    elseif (!is_date(get_post('period_from')) || !is_date(get_post('period_to')) || !is_date(get_post('appraisal_date')))
        display_error(_('Please provide valid dates.'));
    else {
        $appraisal_id = add_employee_appraisal(array(
            'employee_id' => get_post('employee_id'),
            'reviewer_id' => get_post('reviewer_id', ''),
            'period_from' => get_post('period_from'),
            'period_to' => get_post('period_to'),
            'appraisal_date' => get_post('appraisal_date'),
            'overall_score' => input_num('overall_score', 0),
            'rating_scale' => input_num('rating_scale', 5),
            'status' => 1,
            'strengths' => get_post('strengths', ''),
            'improvements' => get_post('improvements', ''),
            'recommendation' => get_post('recommendation', '')
        ));

        $appraisal = get_employee_appraisal($appraisal_id);
        if (!$appraisal) {
            display_error(_('Appraisal was created but could not be loaded for approval submission.'));
        } else {
            $draft_data = array(
                'appraisal_id'   => (int)$appraisal['appraisal_id'],
                'employee_id'    => $appraisal['employee_id'],
                'employee_name'  => trim($appraisal['employee_name']),
                'reviewer_id'    => $appraisal['reviewer_id'],
                'reviewer_name'  => trim($appraisal['reviewer_name']),
                'period_from'    => sql2date($appraisal['period_from']),
                'period_to'      => sql2date($appraisal['period_to']),
                'appraisal_date' => sql2date($appraisal['appraisal_date']),
                'overall_score'  => (float)$appraisal['overall_score'],
                'rating_scale'   => (int)$appraisal['rating_scale'],
                'strengths'      => $appraisal['strengths'],
                'improvements'   => $appraisal['improvements'],
                'recommendation' => $appraisal['recommendation'],
            );

            $approval_result = approval_check_before_save(
                ST_EMPLOYEE_APPRAISAL,
                $draft_data,
                (float)$appraisal['overall_score'],
                array('summary' => sprintf(_('Employee Appraisal: %s (%s)'), $appraisal['employee_id'], sql2date($appraisal['appraisal_date'])))
            );

            if ($approval_result !== false && $approval_result['status'] === 'auto_approved') {
                display_notification(_('Appraisal has been added and automatically approved.'));
            } elseif ($approval_result !== false) {
                return;
            } else {
                update_employee_appraisal_status((int)$appraisal['appraisal_id'], 2);
                display_notification(_('Appraisal has been added and auto-approved (no active core workflow).'));
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
table_header(array(_('ID'), _('Employee'), _('Reviewer'), _('Period'), _('Date'), _('Score'), _('Status')));
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
    label_cell(isset($status_labels[(int)$row['status']]) ? $status_labels[(int)$row['status']] : $row['status']);
    end_row();
}
end_table(1);

end_form();
end_page();
