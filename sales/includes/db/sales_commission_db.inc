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

/*
    Phase 6: Advanced Commission Engine DB functions.

    When use_advanced_commissions=0  →  existing provision/break_pt/provision2
    fields and rep106.php continue working unchanged.
    When use_advanced_commissions=1  →  plan-based commission entries are
    created on invoice post / reversed on credit note.
*/

// -------------------------------------------------------------------------
// Commission Plans
// -------------------------------------------------------------------------

/**
 * Add a new commission plan.
 *
 * @param  string $name             Plan display name.
 * @param  string $plan_type        'percentage'|'tiered'|'target_based'|'achievement'
 * @param  string $calculation_base 'revenue'|'margin'|'quantity'
 * @param  string $period_type      'per_transaction'|'monthly'|'quarterly'|'yearly'
 * @param  string $status           'draft'|'active'|'expired'
 * @param  string $date_start       SQL date or ''
 * @param  string $date_end         SQL date or ''
 * @return int    Inserted ID.
 */
function add_commission_plan($name, $plan_type, $calculation_base, $period_type, $status,
    $date_start, $date_end)
{
    $sql = "INSERT INTO ".TB_PREF."sales_commission_plans
                (name, plan_type, calculation_base, period_type, status, date_start, date_end)
            VALUES (".db_escape($name).", ".db_escape($plan_type).", ".db_escape($calculation_base).",
                ".db_escape($period_type).", ".db_escape($status).",
                ".($date_start ? db_escape($date_start) : 'NULL').",
                ".($date_end   ? db_escape($date_end)   : 'NULL').")";
    db_query($sql, 'Cannot add commission plan');
    return db_insert_id();
}

/**
 * Update an existing commission plan.
 *
 * @param  int    $id
 * @param  string $name
 * @param  string $plan_type
 * @param  string $calculation_base
 * @param  string $period_type
 * @param  string $status
 * @param  string $date_start SQL date or ''
 * @param  string $date_end   SQL date or ''
 * @param  int    $inactive
 */
function update_commission_plan($id, $name, $plan_type, $calculation_base, $period_type, $status,
    $date_start, $date_end, $inactive = 0)
{
    $sql = "UPDATE ".TB_PREF."sales_commission_plans SET
                name = ".db_escape($name).",
                plan_type = ".db_escape($plan_type).",
                calculation_base = ".db_escape($calculation_base).",
                period_type = ".db_escape($period_type).",
                status = ".db_escape($status).",
                date_start = ".($date_start ? db_escape($date_start) : 'NULL').",
                date_end = ".($date_end   ? db_escape($date_end)   : 'NULL').",
                inactive = ".db_escape((int)$inactive)."
            WHERE id = ".db_escape((int)$id);
    db_query($sql, 'Cannot update commission plan');
}

/**
 * Retrieve a single commission plan row.
 *
 * @param  int   $id
 * @return array|false
 */
function get_commission_plan($id)
{
    $sql = "SELECT * FROM ".TB_PREF."sales_commission_plans WHERE id = ".db_escape((int)$id);
    $result = db_query($sql, 'Cannot get commission plan');
    return db_fetch($result);
}

/**
 * Get all commission plans, optionally only active/non-inactive ones.
 *
 * @param  bool $active_only
 * @return resource
 */
function get_commission_plans($active_only = true)
{
    $sql = "SELECT * FROM ".TB_PREF."sales_commission_plans";
    if ($active_only)
        $sql .= " WHERE inactive = 0";
    $sql .= " ORDER BY name";
    return db_query($sql, 'Cannot get commission plans');
}

/**
 * Delete a commission plan (only if no entries reference it).
 *
 * @param  int  $id
 * @return bool
 */
function delete_commission_plan($id)
{
    $id = (int)$id;
    $check = db_query("SELECT id FROM ".TB_PREF."sales_commission_entries WHERE plan_id = ".$id." LIMIT 1",
        'Cannot check commission plan usage');
    if (db_num_rows($check) > 0)
        return false;
    db_query("DELETE FROM ".TB_PREF."sales_commission_tiers WHERE plan_id = ".$id, 'Cannot delete tiers');
    db_query("DELETE FROM ".TB_PREF."sales_commission_assignments WHERE plan_id = ".$id, 'Cannot delete assignments');
    db_query("DELETE FROM ".TB_PREF."sales_commission_plans WHERE id = ".$id, 'Cannot delete plan');
    return true;
}

// -------------------------------------------------------------------------
// Commission Tiers
// -------------------------------------------------------------------------

/**
 * Add a tier to a commission plan.
 *
 * @param  int    $plan_id
 * @param  double $from
 * @param  double $to        0 = unlimited
 * @param  double $rate      commission % (0-100)
 * @param  double $bonus     fixed bonus amount
 * @param  int    $sort_order
 * @return int
 */
function add_commission_tier($plan_id, $from, $to, $rate, $bonus, $sort_order = 0)
{
    $sql = "INSERT INTO ".TB_PREF."sales_commission_tiers
                (plan_id, threshold_from, threshold_to, commission_rate, fixed_bonus, sort_order)
            VALUES (".db_escape((int)$plan_id).", ".db_escape((float)$from).", ".db_escape((float)$to).",
                ".db_escape((float)$rate).", ".db_escape((float)$bonus).", ".db_escape((int)$sort_order).")";
    db_query($sql, 'Cannot add commission tier');
    return db_insert_id();
}

/**
 * Update a commission tier.
 *
 * @param  int    $id
 * @param  double $from
 * @param  double $to
 * @param  double $rate
 * @param  double $bonus
 * @param  int    $sort_order
 */
function update_commission_tier($id, $from, $to, $rate, $bonus, $sort_order = 0)
{
    $sql = "UPDATE ".TB_PREF."sales_commission_tiers SET
                threshold_from = ".db_escape((float)$from).",
                threshold_to = ".db_escape((float)$to).",
                commission_rate = ".db_escape((float)$rate).",
                fixed_bonus = ".db_escape((float)$bonus).",
                sort_order = ".db_escape((int)$sort_order)."
            WHERE id = ".db_escape((int)$id);
    db_query($sql, 'Cannot update commission tier');
}

/**
 * Delete a commission tier.
 *
 * @param  int $id
 */
function delete_commission_tier($id)
{
    db_query("DELETE FROM ".TB_PREF."sales_commission_tiers WHERE id = ".db_escape((int)$id),
        'Cannot delete commission tier');
}

/**
 * Get all tiers for a plan ordered by sort_order.
 *
 * @param  int $plan_id
 * @return resource
 */
function get_commission_tiers($plan_id)
{
    $sql = "SELECT * FROM ".TB_PREF."sales_commission_tiers
            WHERE plan_id = ".db_escape((int)$plan_id)."
            ORDER BY sort_order, threshold_from";
    return db_query($sql, 'Cannot get commission tiers');
}

// -------------------------------------------------------------------------
// Plan Assignments
// -------------------------------------------------------------------------

/**
 * Assign a commission plan to a salesman.
 * Replaces any existing assignment that overlaps the same date range for the salesman.
 *
 * @param  int    $plan_id
 * @param  int    $salesman_id
 * @param  string $date_start SQL date or ''
 * @param  string $date_end   SQL date or ''
 * @param  double $target_amount
 * @return int    Inserted ID.
 */
function assign_plan_to_salesman($plan_id, $salesman_id, $date_start, $date_end, $target_amount = 0)
{
    $sql = "INSERT INTO ".TB_PREF."sales_commission_assignments
                (plan_id, salesman_id, date_start, date_end, target_amount)
            VALUES (".db_escape((int)$plan_id).", ".db_escape((int)$salesman_id).",
                ".($date_start ? db_escape($date_start) : 'NULL').",
                ".($date_end   ? db_escape($date_end)   : 'NULL').",
                ".db_escape((float)$target_amount).")";
    db_query($sql, 'Cannot assign commission plan');
    return db_insert_id();
}

/**
 * Get the active assignment + plan for a salesman on a given date.
 *
 * @param  int    $salesman_id
 * @param  string $date SQL date
 * @return array|false
 */
function get_salesman_plan($salesman_id, $date)
{
    $sql = "SELECT a.*, p.plan_type, p.calculation_base, p.period_type
            FROM ".TB_PREF."sales_commission_assignments a
            JOIN ".TB_PREF."sales_commission_plans p ON p.id = a.plan_id
            WHERE a.salesman_id = ".db_escape((int)$salesman_id)."
              AND p.inactive = 0
              AND p.status = 'active'
              AND (a.date_start IS NULL OR a.date_start <= ".db_escape($date).")
              AND (a.date_end IS NULL OR a.date_end >= ".db_escape($date).")
            ORDER BY a.id DESC
            LIMIT 1";
    $result = db_query($sql, 'Cannot get salesman plan assignment');
    return db_fetch($result);
}

/**
 * Get all assignments for a salesman, joined with plan name.
 *
 * @param  int $salesman_id
 * @return resource
 */
function get_salesman_assignments($salesman_id)
{
    $sql = "SELECT a.*, p.name AS plan_name, p.plan_type, p.status AS plan_status
            FROM ".TB_PREF."sales_commission_assignments a
            JOIN ".TB_PREF."sales_commission_plans p ON p.id = a.plan_id
            WHERE a.salesman_id = ".db_escape((int)$salesman_id)."
            ORDER BY a.date_start DESC";
    return db_query($sql, 'Cannot get salesman assignments');
}

/**
 * Delete an assignment.
 *
 * @param  int $id
 */
function delete_commission_assignment($id)
{
    db_query("DELETE FROM ".TB_PREF."sales_commission_assignments WHERE id = ".db_escape((int)$id),
        'Cannot delete commission assignment');
}

// -------------------------------------------------------------------------
// Commission Calculation
// -------------------------------------------------------------------------

/**
 * Calculate commission for a salesman/transaction using the assigned plan.
 * Creates a commission entry in 0_sales_commission_entries.
 * Does nothing when use_advanced_commissions is disabled.
 *
 * @param  int    $salesman_id
 * @param  int    $trans_type  ST_SALESINVOICE or ST_CUSTCREDIT
 * @param  int    $trans_no
 * @param  int    $debtor_no
 * @param  double $base_amount Revenue or margin depending on plan base.
 * @param  string $date        SQL date
 * @return double Commission amount calculated (0 if not applicable).
 */
function calculate_commission($salesman_id, $trans_type, $trans_no, $debtor_no, $base_amount, $date)
{
    if (!get_company_pref('use_advanced_commissions'))
        return 0;

    if (!$salesman_id)
        return 0;

    $assignment = get_salesman_plan($salesman_id, $date);
    if (!$assignment)
        return 0;

    $plan_id      = (int)$assignment['plan_id'];
    $plan_type    = $assignment['plan_type'];

    $commission_rate   = 0;
    $commission_amount = 0;

    if ($plan_type === 'percentage') {
        // Single flat rate stored as first tier, or read from tiers
        $tiers = get_commission_tiers($plan_id);
        $first = db_fetch($tiers);
        if ($first) {
            $commission_rate   = (float)$first['commission_rate'];
            $commission_amount = $base_amount * $commission_rate / 100 + (float)$first['fixed_bonus'];
        }
    } elseif ($plan_type === 'tiered') {
        $commission_amount = _calculate_tiered_commission($plan_id, $base_amount, $commission_rate);
    } elseif ($plan_type === 'target_based' || $plan_type === 'achievement') {
        // For target_based, determine achievement % vs target, then find tier
        $target = (float)$assignment['target_amount'];
        if ($target > 0) {
            $achievement_pct = ($base_amount / $target) * 100;
            $commission_amount = _calculate_tiered_commission($plan_id, $achievement_pct, $commission_rate);
            // commission_amount here is the actual % of base_amount
            $commission_amount = $base_amount * $commission_amount / 100;
        }
    }

    $commission_amount = round($commission_amount, user_price_dec());

    // Hook point before saving
    $context = array(
        'salesman_id'  => $salesman_id,
        'plan_id'      => $plan_id,
        'trans_type'   => $trans_type,
        'trans_no'     => $trans_no,
        'debtor_no'    => $debtor_no,
        'base_amount'  => $base_amount,
        'date'         => $date,
    );
    hook_invoke_all('sales_commission_calculate', $commission_amount, $context);

    // Remove any existing entry for this transaction/salesman (idempotent on re-post)
    db_query("DELETE FROM ".TB_PREF."sales_commission_entries
              WHERE trans_type = ".db_escape($trans_type)."
                AND trans_no = ".db_escape((int)$trans_no)."
                AND salesman_id = ".db_escape((int)$salesman_id),
        'Cannot remove old commission entry');

    $sql = "INSERT INTO ".TB_PREF."sales_commission_entries
                (salesman_id, plan_id, trans_type, trans_no, debtor_no, trans_date,
                 base_amount, commission_rate, commission_amount, status)
            VALUES (".db_escape((int)$salesman_id).", ".db_escape($plan_id).",
                ".db_escape($trans_type).", ".db_escape((int)$trans_no).",
                ".db_escape((int)$debtor_no).", ".db_escape($date).",
                ".db_escape($base_amount).", ".db_escape($commission_rate).",
                ".db_escape($commission_amount).", 'calculated')";
    db_query($sql, 'Cannot save commission entry');

    return $commission_amount;
}

/**
 * Internal helper: apply tiered rate table to an amount.
 * Returns commission amount. Sets $rate by reference to the matched tier's rate.
 *
 * @param  int    $plan_id
 * @param  double $amount
 * @param  double &$matched_rate
 * @return double
 */
function _calculate_tiered_commission($plan_id, $amount, &$matched_rate)
{
    $matched_rate = 0;
    $commission   = 0;
    $tiers = get_commission_tiers($plan_id);
    $remaining = $amount;

    while ($tier = db_fetch($tiers)) {
        $from = (float)$tier['threshold_from'];
        $to   = (float)$tier['threshold_to'];
        $rate = (float)$tier['commission_rate'];
        $bonus = (float)$tier['fixed_bonus'];

        if ($amount < $from)
            break;

        if ($to == 0 || $amount <= $to) {
            // This is the matching tier for the full amount
            $matched_rate = $rate;
            $commission   = $amount * $rate / 100 + $bonus;
            break;
        }
    }
    return $commission;
}

/**
 * Reverse (negate) commission entries for a credit note.
 * Creates a negative commission entry linked to the credit transaction.
 *
 * @param  int    $salesman_id
 * @param  int    $credit_trans_no
 * @param  int    $debtor_no
 * @param  double $credit_amount   Positive credit amount (will be negated internally).
 * @param  string $date            SQL date
 */
function reverse_commission_for_credit($salesman_id, $credit_trans_no, $debtor_no, $credit_amount, $date)
{
    if (!get_company_pref('use_advanced_commissions'))
        return;

    if (!$salesman_id)
        return;

    // Calculate negative commission
    calculate_commission($salesman_id, ST_CUSTCREDIT, $credit_trans_no, $debtor_no,
        -abs($credit_amount), $date);
}

// -------------------------------------------------------------------------
// Batch Calculate
// -------------------------------------------------------------------------

/**
 * Batch-calculate commissions for all invoices/credits in a date period.
 * Skips transactions that already have commission entries.
 * Used from the inquiry page to backfill or recalculate.
 *
 * @param  string $period_start SQL date
 * @param  string $period_end   SQL date
 * @param  int    $salesman_id  0 = all salesman
 * @return int    Number of entries created/updated.
 */
function batch_calculate_commissions($period_start, $period_end, $salesman_id = 0)
{
    $count = 0;

    // Get all invoices/credits in period with salesman info
    $where_salesman = $salesman_id ? " AND so.salesman_code = ".db_escape((int)$salesman_id) : '';

    $sql = "SELECT dt.trans_no, dt.type, dt.debtor_no, dt.tran_date,
                   dt.ov_amount + dt.ov_discount AS total_amount,
                   so.salesman_code
            FROM ".TB_PREF."debtor_trans dt
            JOIN ".TB_PREF."sales_orders so ON so.order_no = dt.order_
                AND so.trans_type = ".ST_SALESORDER."
            WHERE dt.type IN (".ST_SALESINVOICE.", ".ST_CUSTCREDIT.")
              AND dt.tran_date >= ".db_escape($period_start)."
              AND dt.tran_date <= ".db_escape($period_end)."
              AND dt.ov_amount != 0
              ".$where_salesman."
            ORDER BY dt.tran_date";

    $result = db_query($sql, 'Cannot get transactions for batch commission');

    while ($row = db_fetch($result)) {
        $base = (float)$row['total_amount'];
        if ($row['type'] == ST_CUSTCREDIT)
            $base = -abs($base);
        calculate_commission((int)$row['salesman_code'], (int)$row['type'],
            (int)$row['trans_no'], (int)$row['debtor_no'], $base,
            $row['tran_date']);
        $count++;
    }

    return $count;
}

// -------------------------------------------------------------------------
// Inquiry / Approval / Payment
// -------------------------------------------------------------------------

/**
 * Get commission entries with optional filters.
 *
 * @param  int    $salesman_id 0 = all
 * @param  string $status      '' = all
 * @param  string $date_from   SQL date or ''
 * @param  string $date_to     SQL date or ''
 * @return resource
 */
function get_commission_entries($salesman_id = 0, $status = '', $date_from = '', $date_to = '')
{
    $where = array();
    if ($salesman_id)
        $where[] = "e.salesman_id = ".db_escape((int)$salesman_id);
    if ($status !== '')
        $where[] = "e.status = ".db_escape($status);
    if ($date_from !== '')
        $where[] = "e.trans_date >= ".db_escape($date_from);
    if ($date_to !== '')
        $where[] = "e.trans_date <= ".db_escape($date_to);

    $where_clause = count($where) ? 'WHERE '.implode(' AND ', $where) : '';

    $sql = "SELECT e.*, s.salesman_name, p.name AS plan_name,
                   c.name AS customer_name
            FROM ".TB_PREF."sales_commission_entries e
            LEFT JOIN ".TB_PREF."salesman s ON s.salesman_code = e.salesman_id
            LEFT JOIN ".TB_PREF."sales_commission_plans p ON p.id = e.plan_id
            LEFT JOIN ".TB_PREF."debtors_master c ON c.debtor_no = e.debtor_no
            ".$where_clause."
            ORDER BY e.trans_date DESC, e.id DESC";

    return db_query($sql, 'Cannot get commission entries');
}

/**
 * Get commission summary per salesman for a period.
 *
 * @param  int    $salesman_id 0 = all
 * @param  string $period_start SQL date
 * @param  string $period_end   SQL date
 * @return resource
 */
function get_commission_summary($salesman_id, $period_start, $period_end)
{
    $where = "WHERE e.trans_date >= ".db_escape($period_start)."
              AND e.trans_date <= ".db_escape($period_end);
    if ($salesman_id)
        $where .= " AND e.salesman_id = ".db_escape((int)$salesman_id);

    $sql = "SELECT e.salesman_id, s.salesman_name,
                   SUM(CASE WHEN e.base_amount > 0 THEN e.base_amount ELSE 0 END) AS total_revenue,
                   SUM(e.commission_amount) AS total_commission,
                   COUNT(CASE WHEN e.trans_type = ".ST_SALESINVOICE." THEN 1 END) AS invoice_count,
                   SUM(CASE WHEN e.status = 'paid' THEN e.commission_amount ELSE 0 END) AS paid_amount,
                   SUM(CASE WHEN e.status != 'paid' THEN e.commission_amount ELSE 0 END) AS unpaid_amount
            FROM ".TB_PREF."sales_commission_entries e
            LEFT JOIN ".TB_PREF."salesman s ON s.salesman_code = e.salesman_id
            ".$where."
            GROUP BY e.salesman_id, s.salesman_name
            ORDER BY s.salesman_name";

    return db_query($sql, 'Cannot get commission summary');
}

/**
 * Get salesman performance vs target for a period.
 *
 * @param  int    $salesman_id
 * @param  string $period_start SQL date
 * @param  string $period_end   SQL date
 * @return array
 */
function get_salesman_performance($salesman_id, $period_start, $period_end)
{
    $assignment = get_salesman_plan($salesman_id, $period_end);
    $target = $assignment ? (float)$assignment['target_amount'] : 0;

    $sql = "SELECT SUM(CASE WHEN base_amount > 0 THEN base_amount ELSE 0 END) AS revenue,
                   SUM(commission_amount) AS commission
            FROM ".TB_PREF."sales_commission_entries
            WHERE salesman_id = ".db_escape((int)$salesman_id)."
              AND trans_date >= ".db_escape($period_start)."
              AND trans_date <= ".db_escape($period_end);

    $result = db_query($sql, 'Cannot get salesman performance');
    $row = db_fetch($result);

    $revenue    = (float)($row['revenue']    ?? 0);
    $commission = (float)($row['commission'] ?? 0);
    $achievement = ($target > 0) ? round($revenue / $target * 100, 2) : null;

    return array(
        'salesman_id'      => $salesman_id,
        'revenue'          => $revenue,
        'commission'       => $commission,
        'target_amount'    => $target,
        'achievement_pct'  => $achievement,
    );
}

/**
 * Approve a set of commission entries.
 *
 * @param  array $entry_ids
 * @param  int   $approved_by  user ID (for audit; stored in future)
 */
function approve_commissions($entry_ids, $approved_by)
{
    if (empty($entry_ids))
        return;
    $ids = implode(',', array_map('intval', $entry_ids));
    db_query("UPDATE ".TB_PREF."sales_commission_entries
              SET status = 'approved'
              WHERE id IN (".$ids.") AND status = 'calculated'",
        'Cannot approve commissions');
    hook_invoke_all('sales_commission_approved', $entry_ids);
}

/**
 * Mark commission entries as paid.
 *
 * @param  array  $entry_ids
 * @param  string $payment_ref
 * @param  string $payment_date SQL date
 */
function mark_commissions_paid($entry_ids, $payment_ref, $payment_date)
{
    if (empty($entry_ids))
        return;
    $ids = implode(',', array_map('intval', $entry_ids));
    db_query("UPDATE ".TB_PREF."sales_commission_entries
              SET status = 'paid',
                  payment_reference = ".db_escape($payment_ref).",
                  payment_date = ".db_escape($payment_date)."
              WHERE id IN (".$ids.") AND status = 'approved'",
        'Cannot mark commissions paid');
    hook_invoke_all('sales_commission_paid', $entry_ids, $payment_ref);
}
