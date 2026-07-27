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

$recover_period_id = find_submit('Recover');
if ($recover_period_id > 0) {
	$recovery = reconcile_pending_payroll_period_approval($recover_period_id);
	if (!isset($recovery['status']) || $recovery['status'] === 'error')
		display_error(isset($recovery['message'])
			? $recovery['message']
			: _('The payroll period approval could not be recovered.'));
	else
		display_notification($recovery['message']);
	if (isset($Ajax))
		$Ajax->activate('_page_body');
}

foreach ($_POST as $name => $value) {
    if (strpos($name, 'Approve') === 0) {
        $period_id = (int)substr($name, 7);
        if ($period_id > 0) {
            begin_transaction();
            $period = get_payroll_period_for_update($period_id);
            if (!$period) {
                cancel_transaction();
                display_error(_('Payroll period not found.'));
                continue;
            }

            if ((int)$period['status'] !== 1) {
                cancel_transaction();
                display_error(_('Only calculated payroll periods can be approved.'));
                continue;
            }

            if (!update_payroll_period_totals($period_id)) {
                cancel_transaction();
                display_error(
                    _('Payroll period totals could not be reconciled. No approval draft was created.')
                );
                continue;
            }

            $period = get_payroll_period_for_update($period_id);
            if (!$period || (int)$period['status'] !== 1) {
                cancel_transaction();
                display_error(_('Only calculated payroll periods can be approved.'));
                continue;
            }

            $payroll_amount = (float)$period['total_net'];

            // Check if core approval workflow is required
            if ($approval_service->isApprovalRequired(ST_PAYROLL_PERIOD, $payroll_amount)) {
                $existing_core_draft = find_approval_draft_for_hrm_request(ST_PAYROLL_PERIOD, $period_id);
                if ($existing_core_draft && (int)$existing_core_draft['status'] === APPROVAL_STATUS_PENDING) {
                    cancel_transaction();
                    display_notification(_('Payroll period is already pending in the core approval workflow.'));
                    continue;
                }

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

                $approval_result = $approval_service->submit(
                    ST_PAYROLL_PERIOD,
                    $payroll_draft_data,
                    $payroll_amount,
                    array('summary' => sprintf(_('Payroll: %s, Net: %s'), $period['period_name'], number_format($payroll_amount, 2)))
                );

                if ($approval_result !== false && $approval_result['status'] === 'auto_approved') {
                    commit_transaction();
                    display_notification(_('Payroll period has been approved (auto-approved).'));
                } elseif ($approval_result !== false
                    && $approval_result['status'] === 'pending'
                ) {
                    commit_transaction();
                    display_notification(sprintf(
                        _(
                            'Payroll period has been submitted for approval. '
                            .'Draft #%d, Reference: %s'
                        ),
                        $approval_result['draft_id'],
                        $approval_result['reference']
                    ));
                } else {
                    cancel_transaction();
                    display_error(
                        _('Payroll approval submission failed. No approval draft was created.')
                    );
                }
            } else {
                cancel_transaction();
                display_error(
                    _('A configured core approval workflow is required before this payroll period can be approved.')
                );
            }
        }
    }
    if (strpos($name, 'Reopen') === 0) {
        $period_id = (int)substr($name, 6);
        if ($period_id > 0) {
            $reopen = reopen_payroll_period_without_financial_side_effects($period_id);
            if (!empty($reopen['ok']))
                display_notification($reopen['message']);
            else
                display_error($reopen['message']);
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
start_table(TABLESTYLE_DATA, 'class="extra-height-data-table"');
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

    if ((int)$row['status'] == 1) {
        $has_pending_core_approval = find_approval_draft_for_hrm_request(ST_PAYROLL_PERIOD, (int)$row['period_id']);
        if ($has_pending_core_approval && (int)$has_pending_core_approval['status'] === APPROVAL_STATUS_PENDING)
            label_cell(_('Pending Approval'));
        else
            submit_cells(
                'Recover' . $row['period_id'],
                _('Recover'),
                _('Create the missing approval draft from this calculated period.'),
                true
            );
    } else
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
