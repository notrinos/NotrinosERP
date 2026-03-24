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
$page_security = 'SA_PAYROLLAPPROVE';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/payroll_db.inc');
include_once($path_to_root . '/includes/approval/db/approval_db.inc');

page(_("Payroll Approval"));

$approval_service = get_approval_workflow_service();

foreach ($_POST as $name => $value) {
    if (strpos($name, 'Approve') === 0) {
        $period_id = (int)substr($name, 7);
        if ($period_id > 0) {
            $period = get_payroll_period($period_id);
            if (!$period) {
                display_error(_('Payroll period not found.'));
                continue;
            }

            $payroll_amount = (float)$period['total_net'];

            // Check if core approval workflow is required
            if ($approval_service->isApprovalRequired(ST_PAYROLL_PERIOD, $payroll_amount)) {
                $payroll_draft_data = array(
                    'period_id'        => $period_id,
                    'period_name'      => $period['period_name'],
                    'from_date'        => $period['from_date'],
                    'to_date'          => $period['to_date'],
                    'total_gross'      => (float)$period['total_gross'],
                    'total_deductions' => (float)$period['total_deductions'],
                    'total_net'        => $payroll_amount,
                    'department_id'    => isset($period['department_id']) ? $period['department_id'] : null,
                );

                $approval_result = approval_check_before_save(
                    ST_PAYROLL_PERIOD,
                    $payroll_draft_data,
                    $payroll_amount,
                    array('summary' => sprintf(_('Payroll: %s, Net: %s'), $period['period_name'], number_format($payroll_amount, 2)))
                );

                if ($approval_result !== false && $approval_result['status'] === 'auto_approved') {
                    display_notification(_('Payroll period has been approved (auto-approved).'));
                } elseif ($approval_result !== false) {
                    // Pending approval — page already stopped by display_footer_exit()
                    return;
                } else {
                    // No workflow — fallback to direct approval
                    update_payroll_period_status($period_id, 2);
                    update_payroll_period_totals($period_id);
                    display_notification(_('Payroll period has been approved.'));
                }
            } else {
                // No approval required — direct approve
                update_payroll_period_status($period_id, 2);
                update_payroll_period_totals($period_id);
                display_notification(_('Payroll period has been approved.'));
            }
        }
    }
    if (strpos($name, 'Reopen') === 0) {
        $period_id = (int)substr($name, 6);
        if ($period_id > 0) {
            update_payroll_period_status($period_id, 0);
            display_notification(_('Payroll period has been reopened as Draft.'));
        }
    }
}

$status_labels = array(
    0 => _('Draft'),
    1 => _('Calculated'),
    2 => _('Approved'),
    3 => _('Posted'),
    4 => _('Paid'),
    5 => _('Closed'),
    6 => _('Voided')
);

start_form();
start_table(TABLESTYLE, "width='95%'");
$th = array(_('Period ID'), _('Name'), _('From'), _('To'), _('Gross'), _('Deductions'), _('Net'), _('Status'), '', '');
table_header($th);

$result = get_payroll_periods();
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['period_id']);
    label_cell($row['period_name']);
    label_cell(sql2date($row['from_date']));
    label_cell(sql2date($row['to_date']));
    amount_cell($row['total_gross']);
    amount_cell($row['total_deductions']);
    amount_cell($row['total_net']);
    label_cell(isset($status_labels[(int)$row['status']]) ? $status_labels[(int)$row['status']] : $row['status']);

    if ((int)$row['status'] == 1 || (int)$row['status'] == 0)
        submit_cells('Approve'.$row['period_id'], _('Approve'));
    else
        label_cell('');

    if ((int)$row['status'] == 2 || (int)$row['status'] == 3)
        submit_cells('Reopen'.$row['period_id'], _('Reopen'));
    else
        label_cell('');

    end_row();
}
end_table(1);
end_form();

end_page();

