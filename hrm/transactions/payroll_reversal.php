<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License GPL v3+.
***********************************************************************/
$page_security = 'SA_PAYROLLREVERSE';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/payroll_db.inc');
include_once($path_to_root . '/hrm/includes/payroll_engine.inc');

page(_($help_context = 'Payroll Reversal'));

$period_id = find_submit('Reverse');
$verify_only = false;
if ($period_id <= 0) {
    $period_id = find_submit('VerifyReversal');
    $verify_only = $period_id > 0;
}

if ($period_id > 0) {
    $reason_code = $verify_only
        ? 'OTHER_CONTROLLED'
        : get_post('Reason'.(int)$period_id, '');
    $reversal = reverse_posted_payroll_period($period_id, $reason_code);
    if (!empty($reversal['ok']))
        display_notification($reversal['message']);
    else
        display_error(isset($reversal['message'])
            ? $reversal['message']
            : _('The posted payroll period could not be reversed.'));

    if (isset($Ajax))
        $Ajax->activate('_page_body');
}

$status_labels = array(
    3 => _('Posted'),
    6 => _('Reversed'),
);
$reason_codes = payroll_linked_reversal_reason_codes();

start_form();
start_table(TABLESTYLE_DATA, 'class="extra-height-data-table"');
table_header(array(
    _('Period ID'), _('Name'), _('From'), _('To'), _('Net'), _('Status'),
    _('Controlled reason'), ''
));

$result = get_payroll_periods();
$k = 0;
while ($row = db_fetch_assoc($result)) {
    $status = isset($row['status']) ? (int)$row['status'] : -1;
    if (!isset($status_labels[$status]))
        continue;

    if ($status === 6) {
        $evidence = get_payroll_reversal_evidence_rows(
            (int)$row['period_id'],
            0,
            'gl_reversal',
            false
        );
        if ($evidence === false || empty($evidence))
            continue;
    }

    alt_table_row_color($k);
    label_cell((int)$row['period_id']);
    label_cell($row['period_name']);
    label_cell(sql2date($row['from_date']));
    label_cell(sql2date($row['to_date']));
    amount_cell($row['total_net']);
    label_cell($status_labels[$status]);
    if ($status === 3) {
        echo '<td>'.array_selector(
            'Reason'.(int)$row['period_id'],
            'POSTING_ERROR',
            $reason_codes
        ).'</td>';
        submit_cells(
            'Reverse'.(int)$row['period_id'],
            _('Reverse'),
            _('Append exact opposite GL and restore only immutable snapshot-proven loan and leave state.'),
            true
        );
    } else {
        label_cell(_('Recorded at reversal'));
        submit_cells(
            'VerifyReversal'.(int)$row['period_id'],
            _('Verify'),
            _('Verify linked reversal GL and restored balance evidence without writing rows.'),
            true
        );
    }
    end_row();
}
end_table(1);
submit_js_confirm('Reverse', _(
    'Reverse this Posted payroll period? Original payroll and accounting rows remain immutable. Exact opposite GL entries will be appended and snapshot-proven loan/leave state restored. This command does not reverse Paid or Closed payroll.'
));
end_form();

end_page();
