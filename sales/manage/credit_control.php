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

// ============================================================================
// Phase 8 – Credit Control Dashboard
// Provides: at-risk customers, active holds, place/release hold, credit review.
// Requires SA_CREDITCONTROL. Place/release hold also requires SA_CREDITCONTROL.
// Credit review (limit + status change) requires SA_CREDITCONTROL.
// ============================================================================

$page_security = 'SA_CREDITCONTROL';
$path_to_root  = '../..';
include_once($path_to_root . '/includes/session.inc');

page(_($help_context = 'Credit Control Dashboard'));

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/sales/includes/db/sales_credit_control_db.inc');
include_once($path_to_root . '/sales/includes/db/credit_status_db.inc');

// ============================================================================
// ACTION: Place hold
// ============================================================================
if (isset($_POST['action_place_hold'])) {
    $debtor_no = (int) get_post('hold_debtor_no');
    $hold_type = get_post('hold_type', 'manual');
    $reason    = trim(get_post('hold_reason', ''));
    if (!$debtor_no || !$reason) {
        display_error(_('Please select a customer and enter a reason before placing a hold.'));
    } else {
        $affects = array(
            'orders'     => get_post('hold_affects_orders',     0) ? 1 : 0,
            'deliveries' => get_post('hold_affects_deliveries', 0) ? 1 : 0,
            'invoices'   => get_post('hold_affects_invoices',   0) ? 1 : 0,
        );
        place_credit_hold($debtor_no, $hold_type, $reason, (int)$_SESSION['wa_current_user']->user, $affects);
        display_notification(_('Credit hold has been placed on the customer.'));
    }
}

// ============================================================================
// ACTION: Release hold
// ============================================================================
if (isset($_GET['release_hold'])) {
    $hold_id = (int) $_GET['release_hold'];
    if ($hold_id > 0) {
        release_credit_hold($hold_id, (int)$_SESSION['wa_current_user']->user);
        display_notification(_('Credit hold has been released.'));
    }
}

// ============================================================================
// ACTION: Batch evaluate risk
// ============================================================================
if (isset($_POST['action_batch_eval'])) {
    $count = batch_credit_evaluation();
    display_notification(sprintf(_('Credit risk evaluation complete for %d customers.'), $count));
}

// ============================================================================
// ACTION: Credit review (update limit + status)
// ============================================================================
if (isset($_POST['action_credit_review'])) {
    $debtor_no  = (int) get_post('review_debtor_no');
    $new_limit  = input_num('review_new_limit', 0);
    $new_status = (int) get_post('review_new_status');
    $risk_score = get_post('review_risk_score', 'medium');
    $notes      = trim(get_post('review_notes', ''));
    if (!$debtor_no) {
        display_error(_('Please select a customer for the credit review.'));
    } elseif ($new_limit < 0) {
        display_error(_('New credit limit must be zero or greater.'));
    } else {
        add_credit_review($debtor_no, (int)$_SESSION['wa_current_user']->user, $new_limit, $new_status, $risk_score, $notes);
        display_notification(_('Credit review has been recorded and customer limits updated.'));
    }
}

// ============================================================================
// ACTION: View customer hold history (popup via GET)
// ============================================================================
$view_history_debtor = isset($_GET['history_debtor']) ? (int) $_GET['history_debtor'] : 0;

// ============================================================================
// Dashboard data
// ============================================================================
$dashboard = get_credit_control_dashboard();

// ============================================================================
// PLACE HOLD FORM
// ============================================================================
echo "<h3>" . _('Place Credit Hold') . "</h3>";
start_form();
start_table(TABLESTYLE2, "width='80%'");
$th = array(_('Customer'), _('Hold Type'), _('Reason'), _('Affects Orders'),
            _('Affects Deliveries'), _('Affects Invoices'), '');
table_header($th);
start_row();

// Customer selector
customer_list_cells('', 'hold_debtor_no', null, true, false, false, false);

// Hold type
$hold_types = array(
    'manual'     => _('Manual'),
    'over_limit' => _('Over Limit'),
    'overdue'    => _('Overdue'),
    'risk'       => _('Risk'),
);
label_cell(array_selector('hold_type', 'manual', $hold_types));
text_cells('', 'hold_reason', '', 30, 100);
check_cells('', 'hold_affects_orders',     1);
check_cells('', 'hold_affects_deliveries', 1);
check_cells('', 'hold_affects_invoices',   0);
submit_cells('action_place_hold', _('Place Hold'), '', '', true);
end_row();
end_table(1);
end_form();

// ============================================================================
// CREDIT REVIEW FORM
// ============================================================================
echo "<h3>" . _('Credit Review (Update Limit & Status)') . "</h3>";
start_form();
start_table(TABLESTYLE2, "width='90%'");
$th = array(_('Customer'), _('New Limit'), _('New Status'), _('Risk Score'), _('Notes'), '');
table_header($th);
start_row();

customer_list_cells('', 'review_debtor_no', null, true, false, false, false);
amount_cells('', 'review_new_limit', 0);

$statuses = array();
$status_res = get_all_credit_status();
while ($cs = db_fetch($status_res)) {
    $statuses[$cs['id']] = $cs['reason_description'];
}
label_cell(array_selector('review_new_status', 0, $statuses));

$risk_options = array(
    'low'      => _('Low'),
    'medium'   => _('Medium'),
    'high'     => _('High'),
    'critical' => _('Critical'),
);
label_cell(array_selector('review_risk_score', 'medium', $risk_options));
text_cells('', 'review_notes', '', 40, 200);
submit_cells('action_credit_review', _('Save Review'), '', '', true);
end_row();
end_table(1);
end_form();

// ============================================================================
// BATCH EVALUATION
// ============================================================================
echo "<h3>" . _('Automated Risk Evaluation') . "</h3>";
start_form();
echo "<p>" . _('Re-evaluate payment behavior score and risk score for all active customers based on their 12-month invoice payment history.') . "</p>";
submit('action_batch_eval', _('Run Batch Evaluation'), true, '', true);
end_form();
br();

// ============================================================================
// DASHBOARD SECTIONS
// ============================================================================

// --- Customers over credit limit -----------------------------------------
echo "<h3>" . _('Customers Over Credit Limit') . " (" . count($dashboard['over_limit']) . ")</h3>";
if (empty($dashboard['over_limit'])) {
    display_note(_('No customers are currently over their credit limit.'));
} else {
    start_table(TABLESTYLE, "width='80%'");
    table_header(array(_('Customer'), _('Credit Limit'), _('Current Balance'), _('Over By')));
    $k = 0;
    foreach ($dashboard['over_limit'] as $row) {
        alt_table_row_color($k);
        label_cell('<a href="' . $path_to_root . '/sales/manage/customers.php?selected_id=' .
            $row['debtor_no'] . '">' . htmlspecialchars($row['name']) . '</a>');
        amount_cell($row['credit_limit']);
        amount_cell($row['current_balance']);
        amount_cell($row['current_balance'] - $row['credit_limit']);
        end_row();
    }
    end_table(1);
}

// --- Active credit holds -------------------------------------------------
echo "<h3>" . _('Active Credit Holds') . " (" . count($dashboard['active_holds']) . ")</h3>";
if (empty($dashboard['active_holds'])) {
    display_note(_('No customers currently have active credit holds.'));
} else {
    start_table(TABLESTYLE, "width='80%'");
    table_header(array(_('Customer'), _('Hold Type'), _('Placed On'), _('Reason'), _('History'), _('Release')));
    $k = 0;
    foreach ($dashboard['active_holds'] as $row) {
        $active = get_active_holds($row['debtor_no']);
        foreach ($active as $hold) {
            alt_table_row_color($k);
            label_cell(htmlspecialchars($row['name']));
            label_cell(htmlspecialchars($hold['hold_type']));
            label_cell(sql2date($hold['hold_date']));
            label_cell(htmlspecialchars($hold['reason']));
            label_cell('<a href="' . $_SERVER['PHP_SELF'] . '?history_debtor=' . $row['debtor_no'] . '">'
                . _('History') . '</a>');
            label_cell('<a href="' . $_SERVER['PHP_SELF'] . '?release_hold=' . $hold['id'] . '"'
                . ' onclick="return confirm(\'' . _('Release this hold?') . '\')">'
                . _('Release') . '</a>');
            end_row();
        }
    }
    end_table(1);
}

// --- Upcoming credit reviews ---------------------------------------------
echo "<h3>" . _('Upcoming Credit Reviews (Next 30 Days)') . " (" . count($dashboard['upcoming_reviews']) . ")</h3>";
if (empty($dashboard['upcoming_reviews'])) {
    display_note(_('No credit reviews are scheduled in the next 30 days.'));
} else {
    start_table(TABLESTYLE, "width='60%'");
    table_header(array(_('Customer'), _('Review Date'), _('Risk Score')));
    $k = 0;
    foreach ($dashboard['upcoming_reviews'] as $row) {
        alt_table_row_color($k);
        label_cell('<a href="' . $path_to_root . '/sales/manage/customers.php?selected_id=' .
            $row['debtor_no'] . '">' . htmlspecialchars($row['name']) . '</a>');
        label_cell($row['credit_review_date'] ? sql2date($row['credit_review_date']) : '—');
        $risk_colors = array('low' => 'green', 'medium' => 'darkorange', 'high' => 'red', 'critical' => 'darkred');
        $risk = $row['credit_risk_score'];
        $color = isset($risk_colors[$risk]) ? $risk_colors[$risk] : '';
        label_cell('<span style="color:' . $color . ';font-weight:bold">' . htmlspecialchars($risk) . '</span>');
        end_row();
    }
    end_table(1);
}

// --- High / critical risk customers --------------------------------------
echo "<h3>" . _('High & Critical Risk Customers') . " (" . count($dashboard['high_risk']) . ")</h3>";
if (empty($dashboard['high_risk'])) {
    display_note(_('No high or critical risk customers found.'));
} else {
    start_table(TABLESTYLE, "width='60%'");
    table_header(array(_('Customer'), _('Risk Score'), _('Behavior Score')));
    $k = 0;
    foreach ($dashboard['high_risk'] as $row) {
        alt_table_row_color($k);
        label_cell('<a href="' . $path_to_root . '/sales/manage/customers.php?selected_id=' .
            $row['debtor_no'] . '">' . htmlspecialchars($row['name']) . '</a>');
        $risk_colors = array('low' => 'green', 'medium' => 'darkorange', 'high' => 'red', 'critical' => 'darkred');
        $risk = $row['credit_risk_score'];
        $color = isset($risk_colors[$risk]) ? $risk_colors[$risk] : '';
        label_cell('<span style="color:' . $color . ';font-weight:bold">' . htmlspecialchars($risk) . '</span>');
        label_cell(number_format((float) $row['payment_behavior_score'], 1));
        end_row();
    }
    end_table(1);
}

// ============================================================================
// Customer hold history (shown when ?history_debtor= is set)
// ============================================================================
if ($view_history_debtor > 0) {
    $cust_name = '';
    $cust_row  = db_fetch(db_query(
        "SELECT name FROM " . TB_PREF . "debtors_master WHERE debtor_no = " . db_escape($view_history_debtor),
        'credit_control: fetch customer name'
    ));
    if ($cust_row) { $cust_name = $cust_row['name']; }

    echo "<h3>" . sprintf(_('Hold History for %s'), htmlspecialchars($cust_name)) . "</h3>";
    $history = get_hold_history($view_history_debtor);
    if (!db_num_rows($history)) {
        display_note(_('No hold history found for this customer.'));
    } else {
        start_table(TABLESTYLE, "width='80%'");
        table_header(array(_('Hold Type'), _('Placed On'), _('Held By'), _('Reason'),
                           _('Released On'), _('Released By')));
        $k = 0;
        while ($row = db_fetch($history)) {
            alt_table_row_color($k);
            label_cell(htmlspecialchars($row['hold_type']));
            label_cell(sql2date($row['hold_date']));
            label_cell(htmlspecialchars($row['held_by_name']));
            label_cell(htmlspecialchars($row['reason']));
            label_cell($row['release_date'] ? sql2date($row['release_date']) : '—');
            label_cell($row['release_date'] ? htmlspecialchars($row['released_by_name']) : '—');
            end_row();
        }
        end_table(1);
    }
}

end_page();
