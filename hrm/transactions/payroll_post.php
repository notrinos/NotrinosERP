<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License GPL v3+.
***********************************************************************/
$page_security = 'SA_PAYROLLPOST';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/payroll_db.inc');
include_once($path_to_root . '/hrm/includes/payroll_engine.inc');

page(_($help_context = 'Payroll Posting'));

$period_id = find_submit('Post');
if ($period_id <= 0)
    $period_id = find_submit('Verify');

if ($period_id > 0) {
    $posting = post_approved_payroll_period($period_id);
    if (!empty($posting['ok']))
        display_notification($posting['message']);
    else
        display_error(isset($posting['message'])
            ? $posting['message']
            : _('The approved payroll period could not be posted.'));

    if (isset($Ajax))
        $Ajax->activate('_page_body');
}

$status_labels = array(
    2 => _('Approved'),
    3 => _('Posted'),
);

start_form();
start_table(TABLESTYLE_DATA, 'class="extra-height-data-table"');
table_header(array(
    _('Period ID'), _('Name'), _('From'), _('To'), _('Net'), _('Status'), ''
));

$result = get_payroll_periods();
$k = 0;
while ($row = db_fetch_assoc($result)) {
    $status = isset($row['status']) ? (int)$row['status'] : -1;
    if (!isset($status_labels[$status]))
        continue;

    alt_table_row_color($k);
    label_cell((int)$row['period_id']);
    label_cell($row['period_name']);
    label_cell(sql2date($row['from_date']));
    label_cell(sql2date($row['to_date']));
    amount_cell($row['total_net']);
    label_cell($status_labels[$status]);
    if ($status === 2) {
        submit_cells(
            'Post'.(int)$row['period_id'],
            _('Post'),
            _('Create the approved payroll financial effects exactly once.'),
            true
        );
    } else {
        submit_cells(
            'Verify'.(int)$row['period_id'],
            _('Verify'),
            _('Verify the durable posted payroll evidence without writing rows.'),
            true
        );
    }
    end_row();
}
end_table(1);
submit_js_confirm('Post', _(
    'Post this approved payroll period? This creates GL, loan, and leave effects and cannot be undone without a linked reversal.'
));
end_form();

end_page();
