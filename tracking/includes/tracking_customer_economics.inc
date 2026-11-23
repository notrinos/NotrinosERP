<?php
/**
 * DIM-REP-005: reconciled customer/project economics, coverage and the attribution queue.
 *
 * One read-only authority that splits the posted P&L of a Dimension scope (the same scope, period and sign rule as the
 * Financial Tracking reader of DIM-REP-003) by **customer**, using only evidence the ledger already holds. It adds no table,
 * no writer and no backfill: no customer Dimension is created and no cost is estimated.
 *
 * - **Customer source rule:** a P&L line belongs to a customer only when its own source transaction is a customer document
 *   (sales invoice 10, credit note 11, delivery 13) that has exactly ONE `debtor_trans` customer. The customer is read from
 *   that document; nothing is inferred from names, amounts or dates.
 * - **Revenue:** credit-positive income lines of customer documents, so a credit note/refund reduces the customer's revenue.
 * - **Matched (direct) cost:** COGS and expense lines posted by customer documents of the same customer in the same scope and
 *   period (typically the delivery that issued the stock). Cost recognized by a supplier invoice, payroll, a journal or any
 *   other non-customer source is **unattributed**: it is listed, never allocated or estimated.
 * - **Direct contribution:** revenue - matched cost, available only when the customer has matched cost lines. A customer with
 *   revenue and no matched cost has NO contribution or margin (`cost_missing`), never a 100% margin. A cost-only customer
 *   shows the negative amount and no margin (the DIM-REP-003 margin rule).
 * - **Fully allocated profit is unavailable** until an accepted allocation coverage exists (DIM-ALC-001); the label is a
 *   reviewed state, not a figure.
 * - **Reconciliation:** attributed + unattributed revenue and cost equal the DIM-REP-003 summary of the same scope, period and
 *   cutoff (`reconciled`). Single- versus multi-customer scopes are reported from the attributed revenue lines.
 * - **Attribution queue:** every unattributed source document of the scope with its reason, first and last posting date and
 *   age at the period end, bounded and reproducible from the same predicate.
 *
 * Amounts are the stored company base-currency amounts; nothing is converted. PHP 5.6 compatible syntax. Dates are SQL
 * `Y-m-d`; a null lower bound means lifetime.
 */
require_once __DIR__.'/tracking_explorer_workspace.inc';

/** Source transaction types that carry a customer (ST_SALESINVOICE, ST_CUSTCREDIT, ST_CUSTDELIVERY). */
function dimension_customer_source_types()
{
    return array(10, 11, 13);
}

/** Attribution states of a P&L line, in reporting order. */
function dimension_customer_states()
{
    return array('attributed', 'source_missing', 'ambiguous', 'non_customer_source');
}

/**
 * Reviewed label of an attribution state.
 *
 * @param string $state Attribution state
 * @return string
 */
function dimension_customer_state_text($state)
{
    $texts = array(
        'attributed'=>_('Attributed to a customer through its source document'),
        'source_missing'=>_('Customer document without a customer record'),
        'ambiguous'=>_('Customer document with more than one customer'),
        'non_customer_source'=>_('Not a customer document (journal, bank, supplier, payroll or other source)'));
    return isset($texts[$state]) ? $texts[$state] : $state;
}

/**
 * Reviewed next action for an unattributed state. It names the work to do, it grants no authority.
 *
 * @param string $state Attribution state
 * @return string
 */
function dimension_customer_state_action($state)
{
    $texts = array(
        'source_missing'=>_('Repair the customer document so exactly one customer is recorded, then the line attributes itself.'),
        'ambiguous'=>_('Review the customer document: one source transaction must belong to exactly one customer.'),
        'non_customer_source'=>_('Keep it as a project cost, or record the cost through a customer document. It is not allocated or estimated here.'));
    return isset($texts[$state]) ? $texts[$state] : '';
}

/**
 * SQL expression of the attribution state of one ledger line (alias `t` ledger, `src` document owner).
 *
 * @return string
 */
function dimension_customer_state_sql()
{
    return "CASE WHEN t.type NOT IN (".implode(',', dimension_customer_source_types()).") THEN 'non_customer_source'
        WHEN src.trans_no IS NULL THEN 'source_missing' WHEN src.customers > 1 THEN 'ambiguous' ELSE 'attributed' END";
}

/**
 * SQL join of the ledger to the one-customer-per-document owner of customer documents.
 *
 * @return string
 */
function dimension_customer_source_join_sql()
{
    return ' LEFT JOIN (SELECT dt.type, dt.trans_no, COUNT(DISTINCT dt.debtor_no) AS customers, MIN(dt.debtor_no) AS debtor_no
        FROM '.TB_PREF.'debtor_trans dt WHERE dt.type IN ('.implode(',', dimension_customer_source_types()).') GROUP BY dt.type, dt.trans_no) src
        ON src.type=t.type AND src.trans_no=t.type_no';
}

/**
 * Direct contribution rule of one customer (pure).
 *
 * @param float $revenue Credit-positive revenue
 * @param float $matched_cost Debit-positive matched cost
 * @param int $cost_lines Matched cost lines
 * @param int $lines Eligible lines
 * @return array state empty|cost_missing|cost_only|no_revenue|available, amount (null when no contribution is supported), margin (null unless available)
 */
function dimension_customer_contribution($revenue, $matched_cost, $cost_lines, $lines)
{
    $margin = dimension_tracking_margin($revenue, $matched_cost, $cost_lines, $lines);
    $state = $margin['state'] === 'revenue_only' ? 'cost_missing' : $margin['state'];
    $amount = in_array($state, array('available', 'cost_only', 'no_revenue'), true) ? round((float)$revenue - (float)$matched_cost, 6) : null;
    return array('state'=>$state, 'amount'=>$amount, 'margin'=>$margin['percent']);
}

/**
 * Single or multi customer classification of a scope from the number of customers with attributed revenue (pure).
 *
 * @param int $customers Customers with at least one attributed revenue line
 * @return string none|single|multi
 */
function dimension_customer_mix_state($customers)
{
    return (int)$customers <= 0 ? 'none' : ((int)$customers === 1 ? 'single' : 'multi');
}

/**
 * Fully allocated profit state (pure). It is available only after an accepted allocation coverage AND with no unattributed cost.
 *
 * @param float $unattributed_costs Costs that no customer document supports
 * @param bool $allocation_accepted Whether an accepted allocation coverage exists (none exists before DIM-ALC-001)
 * @return array state available|unavailable, reason
 */
function dimension_customer_fully_allocated_state($unattributed_costs, $allocation_accepted = false)
{
    if (!$allocation_accepted) return array('state'=>'unavailable', 'reason'=>'allocation_not_accepted');
    if (abs((float)$unattributed_costs) > 0.005) return array('state'=>'unavailable', 'reason'=>'unattributed_cost');
    return array('state'=>'available', 'reason'=>'ok');
}

/** Empty measure bucket. */
function dimension_customer_empty_bucket()
{
    return array('revenue'=>0.0, 'cogs'=>0.0, 'expense'=>0.0, 'costs'=>0.0, 'revenue_lines'=>0, 'cost_lines'=>0, 'lines'=>0, 'absolute'=>0.0);
}

/**
 * Customer economics of one scope, period (or lifetime) and cutoff.
 *
 * @param string|null $from SQL date, or null for lifetime
 * @param string $to SQL date cutoff
 * @param int|string $scope1 Axis 1 scope (0 all, negative unassigned, positive exact)
 * @param int|string $scope2 Axis 2 scope
 * @return array|false basis, from, to, customers, attributed, unattributed (by state), totals, summary, reconciled, mix, coverage,
 *                     missing, fully_allocated; false when the range is invalid or unreadable
 */
function dimension_customer_economics($from, $to, $scope1 = 0, $scope2 = 0)
{
    if (!dimension_tracking_valid_range($from, $to)) return false;
    $scope1 = dimension_tracking_normalize_scope($scope1);
    $scope2 = dimension_tracking_normalize_scope($scope2);
    $summary = dimension_tracking_summary($from, $to, $scope1, $scope2);
    if ($summary === false) return false;
    $result = db_query('SELECT '.dimension_customer_state_sql().' AS attribution_state,
        CASE WHEN '.dimension_customer_state_sql().' = \'attributed\' THEN src.debtor_no ELSE 0 END AS customer_no, dm.debtor_ref, dm.name AS customer_name, dm.debtor_no AS known_no,
        SUM(CASE WHEN c.ctype='.CL_INCOME.' THEN -t.amount ELSE 0 END) AS revenue,
        SUM(CASE WHEN c.ctype='.CL_COGS.' THEN t.amount ELSE 0 END) AS cogs,
        SUM(CASE WHEN c.ctype='.CL_EXPENSE.' THEN t.amount ELSE 0 END) AS expense,
        SUM(CASE WHEN c.ctype='.CL_INCOME.' THEN 1 ELSE 0 END) AS revenue_lines,
        SUM(CASE WHEN c.ctype IN ('.CL_COGS.','.CL_EXPENSE.') THEN 1 ELSE 0 END) AS cost_lines,
        COUNT(*) AS eligible_lines, SUM(ABS(t.amount)) AS eligible_absolute'
        .dimension_tracking_from_sql().dimension_customer_source_join_sql().'
        LEFT JOIN '.TB_PREF.'debtors_master dm ON dm.debtor_no=src.debtor_no AND '.dimension_customer_state_sql().' = \'attributed\'
        WHERE c.ctype IN ('.CL_INCOME.','.CL_COGS.','.CL_EXPENSE.')'.dimension_tracking_range_sql($from, $to, $scope1, $scope2).'
        GROUP BY attribution_state, customer_no, dm.debtor_ref, dm.name, dm.debtor_no ORDER BY attribution_state, customer_no', 'The customer economics could not be calculated');
    if (!$result) return false;
    $customers = array();
    $unattributed = array();
    foreach (dimension_customer_states() as $state)
        if ($state !== 'attributed') $unattributed[$state] = dimension_customer_empty_bucket();
    $attributed = dimension_customer_empty_bucket();
    while ($row = db_fetch_assoc($result)) {
        $bucket = array('revenue'=>round((float)$row['revenue'], 6), 'cogs'=>round((float)$row['cogs'], 6), 'expense'=>round((float)$row['expense'], 6),
            'revenue_lines'=>(int)$row['revenue_lines'], 'cost_lines'=>(int)$row['cost_lines'], 'lines'=>(int)$row['eligible_lines'],
            'absolute'=>round((float)$row['eligible_absolute'], 6));
        $bucket['costs'] = round($bucket['cogs'] + $bucket['expense'], 6);
        if ($row['attribution_state'] === 'attributed') {
            $contribution = dimension_customer_contribution($bucket['revenue'], $bucket['costs'], $bucket['cost_lines'], $bucket['lines']);
            $customers[] = array_merge($bucket, array('customer_no'=>(int)$row['customer_no'], 'known'=>$row['known_no'] !== null,
                'reference'=>$row['known_no'] === null ? '' : $row['debtor_ref'], 'name'=>$row['known_no'] === null ? '' : $row['customer_name'],
                'matched_cost'=>$bucket['costs'], 'contribution'=>$contribution['amount'], 'contribution_state'=>$contribution['state'],
                'margin'=>$contribution['margin'], 'revenue_share'=>null));
            foreach (array('revenue', 'cogs', 'expense', 'costs', 'absolute') as $key) $attributed[$key] = round($attributed[$key] + $bucket[$key], 6);
            foreach (array('revenue_lines', 'cost_lines', 'lines') as $key) $attributed[$key] += $bucket[$key];
        } else {
            $state = $row['attribution_state'];
            foreach (array('revenue', 'cogs', 'expense', 'costs', 'absolute') as $key) $unattributed[$state][$key] = round($unattributed[$state][$key] + $bucket[$key], 6);
            foreach (array('revenue_lines', 'cost_lines', 'lines') as $key) $unattributed[$state][$key] += $bucket[$key];
        }
    }
    foreach ($customers as $index => $customer)
        if ($attributed['revenue'] > 0 && $customer['revenue'] > 0)
            $customers[$index]['revenue_share'] = round(100.0 * $customer['revenue'] / $attributed['revenue'], 4);
    usort($customers, 'dimension_customer_compare_revenue');
    $un_total = dimension_customer_empty_bucket();
    foreach ($unattributed as $bucket) {
        foreach (array('revenue', 'cogs', 'expense', 'costs', 'absolute') as $key) $un_total[$key] = round($un_total[$key] + $bucket[$key], 6);
        foreach (array('revenue_lines', 'cost_lines', 'lines') as $key) $un_total[$key] += $bucket[$key];
    }
    $totals = array('revenue'=>round($attributed['revenue'] + $un_total['revenue'], 6), 'costs'=>round($attributed['costs'] + $un_total['costs'], 6));
    $totals['result'] = round($totals['revenue'] - $totals['costs'], 6);
    $reconciled = abs($totals['revenue'] - $summary['revenue']) < 0.005 && abs($totals['costs'] - $summary['costs']) < 0.005
        && abs($totals['result'] - $summary['result']) < 0.005 && $attributed['lines'] + $un_total['lines'] === $summary['eligible_lines'];
    $revenue_customers = 0;
    $without_cost = 0;
    foreach ($customers as $customer) {
        if ($customer['revenue_lines'] > 0) $revenue_customers++;
        if ($customer['contribution_state'] === 'cost_missing') $without_cost++;
    }
    return array('basis'=>($from === null || $from === '') ? 'lifetime' : 'period', 'from'=>($from === '' ? null : $from), 'to'=>$to, 'scope1'=>$scope1, 'scope2'=>$scope2,
        'customers'=>$customers, 'attributed'=>$attributed, 'unattributed'=>$unattributed, 'unattributed_total'=>$un_total, 'totals'=>$totals,
        'summary'=>$summary, 'reconciled'=>$reconciled,
        'mix'=>array('state'=>dimension_customer_mix_state($revenue_customers), 'customers'=>$revenue_customers),
        'coverage'=>dimension_customer_coverage($attributed, $un_total),
        'missing'=>array('customers_without_matched_cost'=>$without_cost, 'unattributed_cost_lines'=>$un_total['cost_lines'],
            'unattributed_cogs'=>$un_total['cogs'], 'unattributed_expense'=>$un_total['expense']),
        'fully_allocated'=>dimension_customer_fully_allocated_state($un_total['costs']));
}

/** Sort rows by revenue descending, then by customer number. */
function dimension_customer_compare_revenue($a, $b)
{
    if ($a['revenue'] == $b['revenue']) return $a['customer_no'] - $b['customer_no'];
    return $a['revenue'] < $b['revenue'] ? 1 : -1;
}

/**
 * Coverage of the attribution over revenue lines and cost lines, by count and by absolute amount (pure).
 *
 * @param array $attributed Attributed bucket
 * @param array $unattributed_total Unattributed bucket
 * @return array revenue_lines, revenue_attributed_lines, revenue_line_percent, cost_lines, cost_attributed_lines, cost_line_percent,
 *               absolute, attributed_absolute, absolute_percent (a percent is null when nothing exists to cover)
 */
function dimension_customer_coverage($attributed, $unattributed_total)
{
    $out = array('revenue_lines'=>$attributed['revenue_lines'] + $unattributed_total['revenue_lines'], 'revenue_attributed_lines'=>$attributed['revenue_lines'],
        'cost_lines'=>$attributed['cost_lines'] + $unattributed_total['cost_lines'], 'cost_attributed_lines'=>$attributed['cost_lines'],
        'absolute'=>round($attributed['absolute'] + $unattributed_total['absolute'], 6), 'attributed_absolute'=>$attributed['absolute'],
        'revenue_line_percent'=>null, 'cost_line_percent'=>null, 'absolute_percent'=>null);
    if ($out['revenue_lines'] > 0) $out['revenue_line_percent'] = round(100.0 * $out['revenue_attributed_lines'] / $out['revenue_lines'], 4);
    if ($out['cost_lines'] > 0) $out['cost_line_percent'] = round(100.0 * $out['cost_attributed_lines'] / $out['cost_lines'], 4);
    if ($out['absolute'] > 0) $out['absolute_percent'] = round(100.0 * $out['attributed_absolute'] / $out['absolute'], 4);
    return $out;
}

/**
 * Attribution queue: every unattributed source document of the scope, oldest first, bounded.
 *
 * Reproducible from the same predicate as the economics: a document is listed when any of its eligible P&L lines in the scope
 * and period is not attributed to exactly one customer.
 *
 * @param string|null $from SQL date, or null for lifetime
 * @param string $to SQL date cutoff (the age is measured to it)
 * @param int|string $scope1 Axis 1 scope
 * @param int|string $scope2 Axis 2 scope
 * @return array|false rows (state, type, type_no, first_date, last_date, age_days, revenue, costs, lines, absolute), truncated; false when invalid
 */
function dimension_customer_attribution_queue($from, $to, $scope1 = 0, $scope2 = 0)
{
    if (!dimension_tracking_valid_range($from, $to)) return false;
    $range = dimension_tracking_range_sql($from, $to, dimension_tracking_normalize_scope($scope1), dimension_tracking_normalize_scope($scope2));
    $result = db_query('SELECT '.dimension_customer_state_sql().' AS attribution_state, t.type, t.type_no, MIN(t.tran_date) AS first_date, MAX(t.tran_date) AS last_date,
        SUM(CASE WHEN c.ctype='.CL_INCOME.' THEN -t.amount ELSE 0 END) AS revenue,
        SUM(CASE WHEN c.ctype IN ('.CL_COGS.','.CL_EXPENSE.') THEN t.amount ELSE 0 END) AS costs,
        COUNT(*) AS eligible_lines, SUM(ABS(t.amount)) AS eligible_absolute'
        .dimension_tracking_from_sql().dimension_customer_source_join_sql().'
        WHERE c.ctype IN ('.CL_INCOME.','.CL_COGS.','.CL_EXPENSE.')'.$range.' AND '.dimension_customer_state_sql().' <> \'attributed\'
        GROUP BY attribution_state, t.type, t.type_no ORDER BY first_date, eligible_absolute DESC, t.type, t.type_no LIMIT '.(dimension_tracking_row_limit() + 1), 'The attribution queue could not be calculated');
    if (!$result) return false;
    $rows = array();
    $cutoff = strtotime($to.' 00:00:00 UTC');
    while ($row = db_fetch_assoc($result)) {
        $age = (int)floor(($cutoff - strtotime($row['first_date'].' 00:00:00 UTC')) / 86400);
        $rows[] = array('state'=>$row['attribution_state'], 'type'=>(int)$row['type'], 'type_no'=>(int)$row['type_no'], 'first_date'=>$row['first_date'],
            'last_date'=>$row['last_date'], 'age_days'=>max(0, $age), 'revenue'=>round((float)$row['revenue'], 6), 'costs'=>round((float)$row['costs'], 6),
            'lines'=>(int)$row['eligible_lines'], 'absolute'=>round((float)$row['eligible_absolute'], 6));
    }
    $truncated = count($rows) > dimension_tracking_row_limit();
    if ($truncated) array_pop($rows);
    return array('rows'=>$rows, 'truncated'=>$truncated);
}

/**
 * Reviewed label of a contribution state.
 *
 * @param string $state Contribution state
 * @return string
 */
function dimension_customer_contribution_text($state)
{
    $texts = array(
        'empty'=>_('No eligible P&L postings.'),
        'cost_missing'=>_('No supported matched cost: COGS, labor and overhead are not estimated, so there is no contribution or margin.'),
        'cost_only'=>_('Matched cost only: there is no revenue, so no margin is shown.'),
        'no_revenue'=>_('There is no positive revenue, so no margin is shown.'));
    return isset($texts[$state]) ? $texts[$state] : '';
}
