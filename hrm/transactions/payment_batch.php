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
$page_security = 'SA_PAYMENTBATCH';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/payroll_db.inc');
include_once($path_to_root . '/hrm/includes/db/payslip_db.inc');
include_once($path_to_root . '/hrm/includes/db/payment_posting.inc');

$js = '';

if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Payment Batch'), false, false, '', $js);

$default_bank_account = get_default_bank_account();
if (!isset($_POST['bank_account']) || (int)$_POST['bank_account'] <= 0)
    $_POST['bank_account'] = $default_bank_account ? $default_bank_account['id'] : '';
if (!isset($_POST['payment_date']) || trim((string)$_POST['payment_date']) === '')
    $_POST['payment_date'] = Today();
if (!isset($_POST['payment_memo']))
    $_POST['payment_memo'] = '';

if (($period_id = find_submit('MarkPaid')) != -1) {
    $period_id = (int)$period_id;
    $period = get_payroll_period($period_id);

    if (!$period) {
        display_error(_('The selected payroll period was not found.'));
    } elseif ((int)$period['status'] === 4) {
        display_warning(_('The selected payroll period is already marked as paid.'));
    } elseif (!in_array((int)$period['status'], array(2, 3))) {
        display_error(_('Only approved or posted payroll periods can be marked as paid.'));
    } else {
        $bank_account = get_payroll_payment_bank_account(get_post('bank_account'));
        if (!$bank_account) {
            display_error(_('A valid source bank account is required.'));
        } elseif (get_payroll_payment_payable_account() === false) {
            display_error(_('Configure Payroll Payable Account in HR Settings before processing payroll batch payments.'));
        } else {
            $transactions = create_payment_transactions_for_period(
                $period_id,
                get_post('bank_account'),
                get_post('payment_date'),
                '',
                get_post('payment_memo')
            );
            if ($transactions === false) {
                display_error(_('Could not create payroll payment batch.'));
            } elseif (count($transactions) > 0) {
                display_notification(sprintf(_('Created %s bank payment transaction(s) and marked the payroll period as paid.'), count($transactions)));
                $_POST['payment_memo'] = '';
            } else {
                display_notification(_('Payroll period has been marked as paid. No positive payable payslips required bank payment.'));
                $_POST['payment_memo'] = '';
            }
        }
    }
}

$per_page = 50;
$page_no = max((int)get_post('page_no', 0), 0);
if (get_post('next_page'))
    $page_no++;
elseif (get_post('prev_page'))
    $page_no = max($page_no - 1, 0);

$total_periods = function_exists('count_payroll_periods') ? count_payroll_periods() : 0;
$page_count = $per_page > 0 ? (int)ceil($total_periods / $per_page) : 1;
$page_count = max($page_count, 1);
if ($page_no >= $page_count)
    $page_no = $page_count - 1;

$offset = $page_no * $per_page;

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
hidden('page_no', $page_no);
start_outer_table(TABLESTYLE, "data-order-header='1'");
table_section(1);
bank_accounts_list_row(_('Pay From:'), 'bank_account', null, true);
date_row(_('Payment Date:'), 'payment_date');
table_section(2);
textarea_row(_('Memo:'), 'payment_memo', null, 35, 5);
end_outer_table(1);

start_table(TABLESTYLE, "width='100%'");
$th = array(_('Period ID'), _('Name'), _('From'), _('To'), _('Total Net'), _('Status'), '');
table_header($th);

$result = get_payroll_periods(null, $per_page, $offset);
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['period_id']);
    label_cell($row['period_name']);
    label_cell(sql2date($row['from_date']));
    label_cell(sql2date($row['to_date']));
    amount_cell($row['total_net']);
    label_cell(isset($status_labels[(int)$row['status']]) ? $status_labels[(int)$row['status']] : $row['status']);

    if ((int)$row['status'] == 2 || (int)$row['status'] == 3)
        submit_cells('MarkPaid'.$row['period_id'], _('Process Batch'));
    else
        label_cell('');

    end_row();
}
end_table(1);

start_table(TABLESTYLE, "width='100%'");
start_row();
label_cell(sprintf(_('Showing %s to %s of %s payroll period(s)'), $total_periods ? ($offset + 1) : 0, min($offset + $per_page, $total_periods), $total_periods));
submit_cells('prev_page', _('Previous'), $page_no > 0, '', '', $page_no <= 0);
submit_cells('next_page', _('Next'), $page_no < ($page_count - 1), '', '', $page_no >= ($page_count - 1));
end_row();
end_table();
end_form();

end_page();

