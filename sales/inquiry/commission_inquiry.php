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
$page_security = 'SA_SALESCOMMISSION';
$path_to_root  = '../..';
include_once($path_to_root . '/includes/session.inc');

page(_($help_context = 'Commission Inquiry'));

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/sales/includes/db/sales_commission_db.inc');
include_once($path_to_root . '/sales/includes/db/sales_groups_db.inc');

// ============================================================================
// ACTION HANDLING (Approve / Mark Paid / Batch Calculate)
// ============================================================================

if (isset($_POST['action_approve']) && !empty($_POST['chk'])) {
    $ids = array_map('intval', array_keys($_POST['chk']));
    approve_commissions($ids, (int)$_SESSION['wa_current_user']->user);
    display_notification(sprintf(_('%d commission entries approved.'), count($ids)));
}

if (isset($_POST['action_paid']) && !empty($_POST['chk'])) {
    $ids = array_map('intval', array_keys($_POST['chk']));
    $payment_ref  = get_post('payment_ref', '');
    $payment_date = get_post('payment_date') ? date2sql(get_post('payment_date')) : date('Y-m-d');
    if (!$payment_ref) {
        display_error(_('Please enter a payment reference before marking as paid.'));
    } else {
        mark_commissions_paid($ids, $payment_ref, $payment_date);
        display_notification(sprintf(_('%d commission entries marked as paid.'), count($ids)));
    }
}

if (isset($_POST['action_batch'])) {
    $batch_from = get_post('batch_from') ? date2sql(get_post('batch_from')) : date('Y-01-01');
    $batch_to   = get_post('batch_to')   ? date2sql(get_post('batch_to'))   : date('Y-m-d');
    $batch_rep  = (int)get_post('batch_salesman', 0);
    $count = batch_calculate_commissions($batch_from, $batch_to, $batch_rep);
    display_notification(sprintf(_('Batch calculation complete: %d entries processed.'), $count));
}

// ============================================================================
// FILTER FORM
// ============================================================================
$date_from   = get_post('date_from',   begin_month(Today()));
$date_to     = get_post('date_to',     Today());
$filter_rep  = (int)get_post('filter_salesman', 0);
$filter_status = get_post('filter_status', '');
$view_mode   = get_post('view_mode', 'entries'); // 'entries' or 'summary'

start_form();
start_table(TABLESTYLE2, "width='80%'");
$th = array(_('From'), _('To'), _('Sales Person'), _('Status'), _('View'), '');
table_header($th);
start_row();
date_cells('', 'date_from', '', null, -30);
date_cells('', 'date_to',   '');

// Salesman selector
$salesmen_arr = array(0 => _('-- All --'));
$salesmen_res = get_salesmen(false);
while ($s = db_fetch($salesmen_res))
    $salesmen_arr[$s['salesman_code']] = $s['salesman_name'];
label_cell(array_selector('filter_salesman', $filter_rep, $salesmen_arr, array('select_submit'=> false)));

$statuses_arr = array('' => _('-- All --'), 'calculated' => _('Calculated'),
    'approved' => _('Approved'), 'paid' => _('Paid'));
label_cell(array_selector('filter_status', $filter_status, $statuses_arr, array('select_submit'=> false)));

$view_arr = array('entries' => _('Entries'), 'summary' => _('Summary'));
label_cell(array_selector('view_mode', $view_mode, $view_arr, array('select_submit'=> false)));
submit_cells('search', _('Search'), '', '', true);
end_row();
end_table(1);
end_form();

// ============================================================================
// BATCH CALCULATE PANEL
// ============================================================================
start_form();
echo "<fieldset style='margin:8px 0;padding:8px'><legend>"._('Batch Calculate Commissions')."</legend>";
start_table(TABLESTYLE2, "width='60%'");
start_row();
date_cells(_('From:'), 'batch_from', '', null, -365);
date_cells(_('To:'),   'batch_to', '');
label_cell(array_selector('batch_salesman', 0, $salesmen_arr, array('select_submit'=> false)));
submit_cells('action_batch', _('Run Batch'), '', '', true);
end_row();
end_table();
echo "</fieldset>";
end_form();

// ============================================================================
// RESULTS
// ============================================================================
$sql_from = date2sql($date_from);
$sql_to   = date2sql($date_to);

if ($view_mode == 'summary') {
    // Summary view
    $result = get_commission_summary($filter_rep, $sql_from, $sql_to);
    start_table(TABLESTYLE, "width='70%'");
    $th = array(_('Sales Person'), _('Revenue'), _('Commission'), _('# Invoices'),
                _('Paid'), _('Unpaid'));
    table_header($th);
    $k = 0;
    $tot_rev = $tot_comm = $tot_paid = $tot_unpaid = 0;
    while ($row = db_fetch($result)) {
        alt_table_row_color($k);
        label_cell($row['salesman_name']);
        amount_cell($row['total_revenue']);
        amount_cell($row['total_commission']);
        label_cell($row['invoice_count'], 'align=right');
        amount_cell($row['paid_amount']);
        amount_cell($row['unpaid_amount']);
        end_row();
        $tot_rev    += $row['total_revenue'];
        $tot_comm   += $row['total_commission'];
        $tot_paid   += $row['paid_amount'];
        $tot_unpaid += $row['unpaid_amount'];
    }
    start_row("class='totalfooter'");
    label_cell(_('TOTAL'));
    amount_cell($tot_rev);
    amount_cell($tot_comm);
    label_cell('');
    amount_cell($tot_paid);
    amount_cell($tot_unpaid);
    end_row();
    end_table(1);
} else {
    // Entries view with approve/pay actions
    start_form();

    $result = get_commission_entries($filter_rep, $filter_status, $sql_from, $sql_to);
    start_table(TABLESTYLE, "width='90%'");
    $th = array('', _('Date'), _('Sales Person'), _('Plan'), _('Customer'),
                _('Trans'), _('Base Amount'), _('Rate %'), _('Commission'), _('Status'), '');
    table_header($th);
    $k = 0;
    $total_commission = 0;
    while ($row = db_fetch($result)) {
        alt_table_row_color($k);
        // Checkbox for bulk actions
        echo "<td><input type='checkbox' name='chk[".intval($row['id'])."]' value='1'></td>";
        label_cell(sql2date($row['trans_date']));
        label_cell(htmlspecialchars($row['salesman_name']));
        label_cell(htmlspecialchars($row['plan_name'] ?? '-'));
        label_cell(htmlspecialchars($row['customer_name']));
        $trans_label = $row['trans_type'] == ST_SALESINVOICE ? _('Invoice') : _('Credit');
        label_cell($trans_label.' #'.$row['trans_no']);
        amount_cell($row['base_amount']);
        label_cell(percent_format($row['commission_rate']).' %', 'align=right');
        amount_cell($row['commission_amount']);
        $status_colors = array('calculated'=>'orange', 'approved'=>'blue', 'paid'=>'green');
        $color = isset($status_colors[$row['status']]) ? $status_colors[$row['status']] : 'black';
        label_cell("<span style='color:{$color}'>"._($row['status'])."</span>");
        if ($row['status'] == 'paid')
            label_cell(htmlspecialchars($row['payment_reference']));
        else
            label_cell('');
        end_row();
        $total_commission += $row['commission_amount'];
    }
    start_row("class='totalfooter'");
    echo "<td colspan='8'>"._('TOTAL')."</td>";
    amount_cell($total_commission);
    echo "<td colspan='2'></td>";
    end_row();
    end_table(1);

    // Bulk action buttons
    echo "<div style='margin-top:8px'>";
    submit('action_approve', _('Approve Selected'), true, '', 'default');
    echo "&nbsp;&nbsp;";
    echo _('Payment Ref:').' ';
    echo "<input type='text' name='payment_ref' size='20' value='".get_post('payment_ref')."'>&nbsp;";
    date_cells(_('Payment Date:'), 'payment_date', '');
    echo "&nbsp;";
    submit('action_paid', _('Mark Selected as Paid'), true, '', 'default');
    echo "</div>";

    end_form();
}

end_page();
