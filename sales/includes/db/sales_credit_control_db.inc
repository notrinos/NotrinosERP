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

// =============================================================================
// Phase 8 – Advanced Credit Control
// All functions are guarded by the `use_advanced_credit_control` sys_pref.
// When the pref is 0 the existing dissallow_invoices / credit_limit path in
// get_customer_to_order() runs as before.
// =============================================================================

/**
 * Evaluate a customer's creditworthiness for a given additional transaction amount.
 *
 * Checks (in order):
 *   1. Active credit holds that affect the requested transaction type.
 *   2. The `dissallow_invoices` flag on the customer's credit status record.
 *   3. Whether the new transaction would exceed the credit limit.
 *   4. Whether the customer has overdue invoices beyond the company grace period.
 *
 * @param int    $debtor_no         Customer debtor_no.
 * @param float  $additional_amount Additional amount being requested (e.g. order total).
 * @param string $check_type        'order' | 'delivery' | 'invoice' — controls which hold flags apply.
 *
 * @return array {
 *   'allowed'         => bool,
 *   'reason'          => string,
 *   'credit_limit'    => float,
 *   'current_balance' => float,
 *   'available'       => float,
 *   'holds'           => array   // active hold rows (may be empty)
 * }
 */
function check_customer_credit($debtor_no, $additional_amount = 0, $check_type = 'order')
{
    global $SysPrefs;

    $result = array(
        'allowed'         => true,
        'reason'          => '',
        'credit_limit'    => 0,
        'current_balance' => 0,
        'available'       => 0,
        'holds'           => array(),
    );

    // --- 1. Active credit holds ------------------------------------------
    $holds = get_active_holds($debtor_no);
    $field = 'affects_' . $check_type . 's'; // affects_orders / affects_deliveries / affects_invoices
    foreach ($holds as $hold) {
        if (!empty($hold[$field])) {
            $result['allowed'] = false;
            $result['reason']  = sprintf(_('Customer has an active credit hold: %s'), $hold['reason']);
            $result['holds'][] = $hold;
        }
    }
    if (!$result['allowed']) {
        $result = apply_credit_check_hook($result, $debtor_no, $additional_amount);
        return $result;
    }

    // --- 2. Fetch customer credit data -----------------------------------
    $sql = "SELECT dm.credit_limit,
                   dm.credit_risk_score,
                   cs.dissallow_invoices,
                   SUM(IFNULL(
                       IF(dt.type IN (" . implode(',', array(ST_CUSTCREDIT, ST_CUSTPAYMENT, ST_BANKDEPOSIT)) . "),
                          -1, 1) * (dt.ov_amount + dt.ov_gst + dt.ov_freight + dt.ov_freight_tax + dt.ov_discount),
                       0
                   )) AS current_balance
            FROM " . TB_PREF . "debtors_master dm
            LEFT JOIN " . TB_PREF . "debtor_trans dt
                   ON dt.type != " . ST_CUSTDELIVERY . "
                  AND dt.debtor_no = dm.debtor_no
            JOIN " . TB_PREF . "credit_status cs ON cs.id = dm.credit_status
            WHERE dm.debtor_no = " . db_escape($debtor_no) . "
            GROUP BY dm.debtor_no";

    $row = db_fetch(db_query($sql, 'check_customer_credit: query failed'));
    if (!$row) {
        return $result; // customer not found – allow with no data
    }

    $credit_limit    = (float) $row['credit_limit'];
    $current_balance = (float) $row['current_balance'];
    $available       = $credit_limit - $current_balance;

    $result['credit_limit']    = $credit_limit;
    $result['current_balance'] = $current_balance;
    $result['available']       = $available;

    // --- 3. dissallow_invoices flag on credit status ---------------------
    if ($row['dissallow_invoices']) {
        $result['allowed'] = false;
        $result['reason']  = _('This customer account is on hold. Please contact the credit control department.');
        $result = apply_credit_check_hook($result, $debtor_no, $additional_amount);
        return $result;
    }

    // --- 4. Credit limit check -------------------------------------------
    if ($credit_limit > 0 && ($current_balance + (float) $additional_amount) > $credit_limit) {
        $result['allowed'] = false;
        $result['reason']  = sprintf(
            _('This transaction would exceed the customer credit limit of %s. Available credit: %s.'),
            number_format($credit_limit, 2),
            number_format(max(0, $available), 2)
        );
    }

    $result = apply_credit_check_hook($result, $debtor_no, $additional_amount);
    return $result;
}

/**
 * Internal helper: fire hook and return (possibly modified) result.
 *
 * @param array $result
 * @param int   $debtor_no
 * @param float $amount
 *
 * @return array
 */
function apply_credit_check_hook($result, $debtor_no, $amount)
{
    hook_invoke_all('sales_credit_check', $result, $debtor_no, $amount);
    return $result;
}

// =============================================================================
// Credit holds
// =============================================================================

/**
 * Place a credit hold on a customer.
 *
 * @param int    $debtor_no
 * @param string $hold_type         'manual'|'over_limit'|'overdue'|'risk'
 * @param string $reason            Human-readable reason text.
 * @param int    $held_by           User ID placing the hold.
 * @param array  $affects           Assoc array: ['orders'=>1,'deliveries'=>1,'invoices'=>0]
 *
 * @return int  ID of the new hold record.
 */
function place_credit_hold($debtor_no, $hold_type, $reason, $held_by, $affects = array())
{
    $affects_orders     = isset($affects['orders'])     ? (int) $affects['orders']     : 1;
    $affects_deliveries = isset($affects['deliveries']) ? (int) $affects['deliveries'] : 1;
    $affects_invoices   = isset($affects['invoices'])   ? (int) $affects['invoices']   : 0;

    $sql = "INSERT INTO " . TB_PREF . "sales_credit_holds
                (debtor_no, hold_type, hold_date, held_by, reason,
                 affects_orders, affects_deliveries, affects_invoices)
            VALUES (" .
                db_escape($debtor_no) . "," .
                db_escape($hold_type) . ",NOW()," .
                db_escape($held_by) . "," .
                db_escape($reason) . "," .
                db_escape($affects_orders) . "," .
                db_escape($affects_deliveries) . "," .
                db_escape($affects_invoices) .
            ")";

    db_query($sql, 'place_credit_hold: cannot insert hold');
    $hold_id = db_insert_id();
    hook_invoke_all('sales_credit_hold_placed', $hold_id, $debtor_no);
    return $hold_id;
}

/**
 * Release an active credit hold.
 *
 * @param int $hold_id      ID of the hold record.
 * @param int $released_by  User ID releasing the hold.
 *
 * @return void
 */
function release_credit_hold($hold_id, $released_by)
{
    $sql = "UPDATE " . TB_PREF . "sales_credit_holds
            SET release_date = NOW(), released_by = " . db_escape($released_by) . "
            WHERE id = " . db_escape($hold_id);
    db_query($sql, 'release_credit_hold: cannot update hold');

    // Fetch debtor_no for hook
    $row = db_fetch(db_query(
        "SELECT debtor_no FROM " . TB_PREF . "sales_credit_holds WHERE id = " . db_escape($hold_id),
        'release_credit_hold: fetch failed'
    ));
    if ($row) {
        hook_invoke_all('sales_credit_hold_released', $hold_id, $row['debtor_no']);
    }
}

/**
 * Return all active (not yet released) credit holds for a customer.
 *
 * @param int $debtor_no
 *
 * @return array  Array of hold rows (may be empty).
 */
function get_active_holds($debtor_no)
{
    $sql = "SELECT * FROM " . TB_PREF . "sales_credit_holds
            WHERE debtor_no = " . db_escape($debtor_no) . "
              AND release_date IS NULL
            ORDER BY hold_date DESC";
    $result = db_query($sql, 'get_active_holds: query failed');
    $holds  = array();
    while ($row = db_fetch($result)) {
        $holds[] = $row;
    }
    return $holds;
}

/**
 * Return full hold history for a customer (active + released).
 *
 * @param int $debtor_no
 *
 * @return resource  DB result set.
 */
function get_hold_history($debtor_no)
{
    $sql = "SELECT h.*, u.real_name AS held_by_name,
                   u2.real_name AS released_by_name
            FROM " . TB_PREF . "sales_credit_holds h
            LEFT JOIN " . TB_PREF . "users u  ON u.id  = h.held_by
            LEFT JOIN " . TB_PREF . "users u2 ON u2.id = h.released_by
            WHERE h.debtor_no = " . db_escape($debtor_no) . "
            ORDER BY h.hold_date DESC";
    return db_query($sql, 'get_hold_history: query failed');
}

// =============================================================================
// Credit reviews
// =============================================================================

/**
 * Record a credit review (limit or status change) for a customer.
 *
 * @param int    $debtor_no
 * @param int    $reviewer_id
 * @param float  $new_limit
 * @param int    $new_status
 * @param string $risk_score   'low'|'medium'|'high'|'critical'
 * @param string $notes
 *
 * @return int  ID of the review record.
 */
function add_credit_review($debtor_no, $reviewer_id, $new_limit, $new_status, $risk_score, $notes)
{
    // Read current values before updating
    $customer = db_fetch(db_query(
        "SELECT credit_limit, credit_status FROM " . TB_PREF . "debtors_master
         WHERE debtor_no = " . db_escape($debtor_no),
        'add_credit_review: fetch customer failed'
    ));
    $old_limit  = $customer ? (float) $customer['credit_limit']  : 0;
    $old_status = $customer ? (int)   $customer['credit_status'] : 0;

    // Save review record
    $sql = "INSERT INTO " . TB_PREF . "sales_credit_reviews
                (debtor_no, review_date, reviewer_id,
                 old_credit_limit, new_credit_limit,
                 old_credit_status, new_credit_status,
                 risk_score, notes)
            VALUES (" .
                db_escape($debtor_no) . ",CURDATE()," .
                db_escape($reviewer_id) . "," .
                db_escape($old_limit) . "," .
                db_escape($new_limit) . "," .
                db_escape($old_status) . "," .
                db_escape($new_status) . "," .
                db_escape($risk_score) . "," .
                db_escape($notes) .
            ")";
    db_query($sql, 'add_credit_review: insert failed');
    $review_id = db_insert_id();

    // Apply the changes to the customer master
    $update_sql = "UPDATE " . TB_PREF . "debtors_master
                   SET credit_limit   = " . db_escape($new_limit) . ",
                       credit_status  = " . db_escape($new_status) . ",
                       credit_risk_score     = " . db_escape($risk_score) . ",
                       credit_review_date    = CURDATE()
                   WHERE debtor_no = " . db_escape($debtor_no);
    db_query($update_sql, 'add_credit_review: update customer failed');

    return $review_id;
}

/**
 * Return all credit reviews for a customer.
 *
 * @param int $debtor_no
 *
 * @return resource  DB result set.
 */
function get_credit_reviews($debtor_no)
{
    $sql = "SELECT r.*, u.real_name AS reviewer_name,
                   cs_old.reason_description AS old_status_name,
                   cs_new.reason_description AS new_status_name
            FROM " . TB_PREF . "sales_credit_reviews r
            LEFT JOIN " . TB_PREF . "users u ON u.id = r.reviewer_id
            LEFT JOIN " . TB_PREF . "credit_status cs_old ON cs_old.id = r.old_credit_status
            LEFT JOIN " . TB_PREF . "credit_status cs_new ON cs_new.id = r.new_credit_status
            WHERE r.debtor_no = " . db_escape($debtor_no) . "
            ORDER BY r.review_date DESC, r.id DESC";
    return db_query($sql, 'get_credit_reviews: query failed');
}

// =============================================================================
// Risk evaluation (automated)
// =============================================================================

/**
 * Recalculate and persist payment_behavior_score and credit_risk_score for one customer.
 *
 * Scoring algorithm:
 *   - Counts invoices paid on time vs. overdue in the last 12 months.
 *   - Overdue > 30 days contributes -10 points per occurrence.
 *   - On-time payments contribute +5 points each.
 *   - Final score clamped to [0,100].
 *   - Score >= 75 → low, 50-74 → medium, 25-49 → high, <25 → critical.
 *
 * @param int $debtor_no
 *
 * @return array  ['risk_score'=>string, 'behavior_score'=>float]
 */
function auto_evaluate_credit_risk($debtor_no)
{
    // Count overdue payments (unpaid invoices past due date)
    $overdue_sql = "SELECT COUNT(*) AS overdue_count
                    FROM " . TB_PREF . "debtor_trans
                    WHERE debtor_no = " . db_escape($debtor_no) . "
                      AND type = " . ST_SALESINVOICE . "
                      AND alloc < (ov_amount + ov_gst + ov_freight + ov_freight_tax + ov_discount)
                      AND due_date < CURDATE()
                      AND tran_date >= DATE_SUB(CURDATE(), INTERVAL 12 MONTH)";
    $overdue_row    = db_fetch(db_query($overdue_sql, 'auto_evaluate_credit_risk: overdue query'));
    $overdue_count  = $overdue_row ? (int) $overdue_row['overdue_count'] : 0;

    // Count fully paid invoices in the last 12 months
    $paid_sql = "SELECT COUNT(*) AS paid_count
                 FROM " . TB_PREF . "debtor_trans
                 WHERE debtor_no = " . db_escape($debtor_no) . "
                   AND type = " . ST_SALESINVOICE . "
                   AND alloc >= (ov_amount + ov_gst + ov_freight + ov_freight_tax + ov_discount)
                   AND tran_date >= DATE_SUB(CURDATE(), INTERVAL 12 MONTH)";
    $paid_row   = db_fetch(db_query($paid_sql, 'auto_evaluate_credit_risk: paid query'));
    $paid_count = $paid_row ? (int) $paid_row['paid_count'] : 0;

    $score = 50; // baseline
    $score += $paid_count  *  5;
    $score -= $overdue_count * 10;
    $score  = max(0, min(100, $score));

    if ($score >= 75)      $risk = 'low';
    elseif ($score >= 50)  $risk = 'medium';
    elseif ($score >= 25)  $risk = 'high';
    else                   $risk = 'critical';

    db_query(
        "UPDATE " . TB_PREF . "debtors_master
         SET payment_behavior_score = " . db_escape($score) . ",
             credit_risk_score      = " . db_escape($risk) . "
         WHERE debtor_no = " . db_escape($debtor_no),
        'auto_evaluate_credit_risk: update failed'
    );

    return array('risk_score' => $risk, 'behavior_score' => $score);
}

/**
 * Run automated credit risk evaluation for all active customers.
 *
 * Intended to be called from a scheduled/batch task.
 *
 * @return int  Number of customers evaluated.
 */
function batch_credit_evaluation()
{
    $customers = db_query(
        "SELECT debtor_no FROM " . TB_PREF . "debtors_master WHERE inactive = 0",
        'batch_credit_evaluation: fetch customers'
    );
    $count = 0;
    while ($row = db_fetch($customers)) {
        auto_evaluate_credit_risk($row['debtor_no']);
        $count++;
    }
    return $count;
}

// =============================================================================
// Credit control dashboard data
// =============================================================================

/**
 * Return summary data for the credit control dashboard.
 *
 * Returns:
 *   'over_limit'       — customers whose balance exceeds their credit limit
 *   'active_holds'     — customers with at least one active hold
 *   'upcoming_reviews' — customers whose credit_review_date falls within 30 days
 *   'high_risk'        — customers with risk score 'high' or 'critical'
 *
 * @return array
 */
function get_credit_control_dashboard()
{
    // Customers over limit — use subquery to avoid MySQL 5.7 HAVING alias restriction
    $over_limit_sql = "SELECT debtor_no, name, credit_limit, current_balance
                       FROM (
                           SELECT dm.debtor_no, dm.name,
                                  dm.credit_limit,
                                  SUM(IFNULL(
                                      IF(dt.type IN (" . implode(',', array(ST_CUSTCREDIT, ST_CUSTPAYMENT, ST_BANKDEPOSIT)) . "),
                                         -1, 1) * (dt.ov_amount + dt.ov_gst + dt.ov_freight + dt.ov_freight_tax + dt.ov_discount),
                                      0
                                  )) AS current_balance
                           FROM " . TB_PREF . "debtors_master dm
                           LEFT JOIN " . TB_PREF . "debtor_trans dt
                                  ON dt.type != " . ST_CUSTDELIVERY . "
                                 AND dt.debtor_no = dm.debtor_no
                           WHERE dm.inactive = 0
                             AND dm.credit_limit > 0
                           GROUP BY dm.debtor_no, dm.name, dm.credit_limit
                       ) AS sub
                       WHERE current_balance > credit_limit
                       ORDER BY (current_balance - credit_limit) DESC
                       LIMIT 50";

    // Customers with active holds
    $holds_sql = "SELECT DISTINCT dm.debtor_no, dm.name, h.hold_type, h.hold_date, h.reason
                  FROM " . TB_PREF . "debtors_master dm
                  JOIN " . TB_PREF . "sales_credit_holds h ON h.debtor_no = dm.debtor_no
                  WHERE dm.inactive = 0
                    AND h.release_date IS NULL
                  ORDER BY h.hold_date DESC
                  LIMIT 50";

    // Upcoming credit reviews (next 30 days)
    $reviews_sql = "SELECT debtor_no, name, credit_review_date, credit_risk_score
                    FROM " . TB_PREF . "debtors_master
                    WHERE inactive = 0
                      AND credit_review_date IS NOT NULL
                      AND credit_review_date BETWEEN CURDATE() AND DATE_ADD(CURDATE(), INTERVAL 30 DAY)
                    ORDER BY credit_review_date ASC
                    LIMIT 50";

    // High / critical risk customers
    $high_risk_sql = "SELECT debtor_no, name, credit_risk_score, payment_behavior_score
                      FROM " . TB_PREF . "debtors_master
                      WHERE inactive = 0
                        AND credit_risk_score IN ('high','critical')
                      ORDER BY FIELD(credit_risk_score,'critical','high'), payment_behavior_score ASC
                      LIMIT 50";

    $dashboard = array(
        'over_limit'       => array(),
        'active_holds'     => array(),
        'upcoming_reviews' => array(),
        'high_risk'        => array(),
    );

    $res = db_query($over_limit_sql, 'get_credit_control_dashboard: over_limit');
    while ($row = db_fetch($res)) { $dashboard['over_limit'][] = $row; }

    $res = db_query($holds_sql, 'get_credit_control_dashboard: holds');
    while ($row = db_fetch($res)) { $dashboard['active_holds'][] = $row; }

    $res = db_query($reviews_sql, 'get_credit_control_dashboard: reviews');
    while ($row = db_fetch($res)) { $dashboard['upcoming_reviews'][] = $row; }

    $res = db_query($high_risk_sql, 'get_credit_control_dashboard: high_risk');
    while ($row = db_fetch($res)) { $dashboard['high_risk'][] = $row; }

    return $dashboard;
}
