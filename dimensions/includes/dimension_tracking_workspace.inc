<?php
/**
 * DIM-REP-003: shared scoped actuals reader for the Financial Tracking workspace.
 *
 * One read-only authority answers "what did this scope earn and cost" for the Dimension workspace
 * (`dimensions/inquiry/dimension_tracking.php`) and the single-Dimension view
 * (`dimensions/view/view_dimension.php`). It adds no ledger, table or writer; it reads `gl_trans`,
 * `chart_*`, `dimensions` and (through the DIM-BUD-001 basis) `budget_trans` of the active company only.
 *
 * - **Measures:** revenue is credit-positive (-amount of income accounts), costs are debit-positive
 *   (COGS and expense accounts), result = revenue - costs. This is the same sign and class rule as
 *   `dimension_top()` (DIM-REP-002), `gl/inquiry/profit_loss.php` and `reporting/rep707.php`; the same
 *   scope, period and cutoff therefore reconcile to those reports.
 * - **Scope:** an axis scope of 0 is all, a negative value the Unassigned coordinate 0 and a positive
 *   value the exact stored Dimension ID (the existing P&L/GL-inquiry convention). Both axes intersect.
 * - **Period and lifetime:** the period reads `from..to` inclusive; lifetime reads every posting up to the
 *   same cutoff. Both are the same query with an open lower bound.
 * - **Net GL balance:** balance-sheet accounts tagged to the scope are a net balance, never a result.
 *   They are returned separately and never added to revenue, costs or result.
 * - **Margin:** a margin exists only when the scope has positive revenue AND recorded costs. A cost-only
 *   or revenue-only scope has no margin (`cost_only`, `revenue_only`) instead of a meaningless 0% or 100%.
 * - **Completeness:** every eligible posting is classified `classified`, `unassigned` or `unknown` per axis
 *   (an ID with no master of that axis stays unknown, never dropped) and coverage is reported per source
 *   transaction family. The workspace is only `complete` when every listed family is fully classified.
 *   These are GL classification measures, not customer attribution and not fully allocated profit.
 *
 * Amounts are the stored company base-currency amounts; nothing is converted. PHP 5.6 compatible syntax.
 * Dates are SQL `Y-m-d`; an empty lower bound means lifetime.
 */
require_once __DIR__.'/dimension_budget_basis.inc';

/** Largest number of rows any one drilldown or group listing returns. */
function dimension_tracking_row_limit()
{
    return 500;
}

/**
 * Normalize one axis scope value.
 *
 * @param mixed $scope Requested scope
 * @return int 0 = all, negative = Unassigned, positive = exact Dimension ID
 */
function dimension_tracking_normalize_scope($scope)
{
    if (is_int($scope)) return $scope;
    if (is_string($scope) && preg_match('/^-?[0-9]{1,9}$/D', $scope) === 1) return (int)$scope;
    return 0;
}

/**
 * Whether a period or cutoff is acceptable: a valid upper bound and an optional valid lower bound not after it.
 *
 * @param string|null $from SQL date, or null/'' for lifetime
 * @param string $to SQL date cutoff
 * @return bool
 */
function dimension_tracking_valid_range($from, $to)
{
    if (!dimension_budget_valid_date($to)) return false;
    if ($from === null || $from === '') return true;
    return dimension_budget_valid_date($from) && $from <= $to;
}

/**
 * SQL predicate for one range and the two axis scopes on the `t` ledger alias.
 *
 * @param string|null $from SQL date or null for lifetime
 * @param string $to SQL date
 * @param int $scope1 Axis 1 scope
 * @param int $scope2 Axis 2 scope
 * @return string Predicate beginning with " AND "
 */
function dimension_tracking_range_sql($from, $to, $scope1, $scope2)
{
    $sql = " AND t.tran_date <= '".$to."'";
    if ($from !== null && $from !== '') $sql .= " AND t.tran_date >= '".$from."'";
    foreach (array('dimension_id'=>(int)$scope1, 'dimension2_id'=>(int)$scope2) as $column=>$scope) {
        if ($scope !== 0) $sql .= ' AND t.'.$column.'='.($scope < 0 ? 0 : $scope);
    }
    return $sql;
}

/** FROM clause shared by every reader: the ledger joined to its account class. */
function dimension_tracking_from_sql()
{
    return ' FROM '.TB_PREF.'gl_trans t
        INNER JOIN '.TB_PREF.'chart_master a ON a.account_code=t.account
        INNER JOIN '.TB_PREF.'chart_types ct ON ct.id=a.account_type
        INNER JOIN '.TB_PREF.'chart_class c ON c.cid=ct.class_id';
}

/** Measure expressions shared by every reader. */
function dimension_tracking_measure_sql()
{
    return 'SUM(CASE WHEN c.ctype='.CL_INCOME.' THEN -t.amount ELSE 0 END) AS revenue,
        SUM(CASE WHEN c.ctype='.CL_COGS.' THEN t.amount ELSE 0 END) AS cogs,
        SUM(CASE WHEN c.ctype='.CL_EXPENSE.' THEN t.amount ELSE 0 END) AS expense,
        SUM(CASE WHEN c.ctype IN ('.CL_COGS.','.CL_EXPENSE.') THEN 1 ELSE 0 END) AS cost_lines,
        COUNT(*) AS eligible_lines, SUM(ABS(t.amount)) AS eligible_absolute';
}

/**
 * Margin rule (pure): only positive revenue with recorded costs has a margin.
 *
 * @param float $revenue Credit-positive revenue
 * @param float $costs Debit-positive costs
 * @param int $cost_lines Number of COGS/expense lines in scope
 * @param int $eligible_lines Number of eligible P&L lines in scope
 * @return array state empty|cost_only|revenue_only|no_revenue|available, percent (null unless available)
 */
function dimension_tracking_margin($revenue, $costs, $cost_lines, $eligible_lines)
{
    $revenue = (float)$revenue;
    if ((int)$eligible_lines === 0) return array('state'=>'empty', 'percent'=>null);
    if ($revenue == 0 && (int)$cost_lines > 0) return array('state'=>'cost_only', 'percent'=>null);
    if ($revenue <= 0) return array('state'=>'no_revenue', 'percent'=>null);
    if ((int)$cost_lines === 0) return array('state'=>'revenue_only', 'percent'=>null);
    return array('state'=>'available', 'percent'=>round(($revenue - (float)$costs) / $revenue * 100.0, 4));
}

/**
 * Period or lifetime measures of one scope.
 *
 * @param string|null $from SQL date, or null for lifetime
 * @param string $to SQL date cutoff
 * @param int|string $scope1 Axis 1 scope
 * @param int|string $scope2 Axis 2 scope
 * @return array|false basis, from, to, scopes, revenue, cogs, expense, costs, result, margin, margin_state, eligible_lines,
 *                     eligible_absolute, balance_net, balance_lines; false when the range is invalid or unreadable
 */
function dimension_tracking_summary($from, $to, $scope1 = 0, $scope2 = 0)
{
    if (!dimension_tracking_valid_range($from, $to)) return false;
    $scope1 = dimension_tracking_normalize_scope($scope1);
    $scope2 = dimension_tracking_normalize_scope($scope2);
    $range = dimension_tracking_range_sql($from, $to, $scope1, $scope2);
    $pl = db_query('SELECT '.dimension_tracking_measure_sql().dimension_tracking_from_sql().'
        WHERE c.ctype IN ('.CL_INCOME.','.CL_COGS.','.CL_EXPENSE.')'.$range, 'The tracking measures could not be calculated');
    $bs = db_query('SELECT COALESCE(SUM(t.amount),0) AS balance_net, COUNT(*) AS balance_lines'.dimension_tracking_from_sql().'
        WHERE c.ctype IN ('.CL_ASSETS.','.CL_LIABILITIES.','.CL_EQUITY.')'.$range, 'The tracking balance could not be calculated');
    $row = $pl ? db_fetch_assoc($pl) : false;
    $balance = $bs ? db_fetch_assoc($bs) : false;
    if (!$row || !$balance) return false;
    $revenue = round((float)$row['revenue'], 6);
    $cogs = round((float)$row['cogs'], 6);
    $expense = round((float)$row['expense'], 6);
    $costs = round($cogs + $expense, 6);
    $margin = dimension_tracking_margin($revenue, $costs, (int)$row['cost_lines'], (int)$row['eligible_lines']);
    return array('basis'=>($from === null || $from === '') ? 'lifetime' : 'period', 'from'=>($from === '' ? null : $from), 'to'=>$to,
        'scope1'=>$scope1, 'scope2'=>$scope2, 'revenue'=>$revenue, 'cogs'=>$cogs, 'expense'=>$expense, 'costs'=>$costs,
        'result'=>round($revenue - $costs, 6), 'margin'=>$margin['percent'], 'margin_state'=>$margin['state'],
        'eligible_lines'=>(int)$row['eligible_lines'], 'eligible_absolute'=>round((float)$row['eligible_absolute'], 6),
        'balance_net'=>round((float)$balance['balance_net'], 6), 'balance_lines'=>(int)$balance['balance_lines']);
}

/**
 * P&L measures grouped by the stored Dimension of one axis, retaining Unassigned and unknown identities.
 *
 * @param int $axis 1 or 2
 * @param string|null $from SQL date or null for lifetime
 * @param string $to SQL date
 * @param int|string $scope1 Axis 1 scope
 * @param int|string $scope2 Axis 2 scope
 * @return array|false Rows: dimension_id, state classified|unassigned|unknown, reference, name, closed, revenue, costs, result,
 *                     eligible_lines, eligible_absolute; false when invalid
 */
function dimension_tracking_groups($axis, $from, $to, $scope1 = 0, $scope2 = 0)
{
    if (!in_array((int)$axis, array(1, 2), true) || !dimension_tracking_valid_range($from, $to)) return false;
    $column = (int)$axis === 1 ? 'dimension_id' : 'dimension2_id';
    $range = dimension_tracking_range_sql($from, $to, dimension_tracking_normalize_scope($scope1), dimension_tracking_normalize_scope($scope2));
    $result = db_query('SELECT t.'.$column.' AS dimension_id, d.id AS known_id, d.reference, d.name, d.closed, '.dimension_tracking_measure_sql()
        .dimension_tracking_from_sql().' LEFT JOIN '.TB_PREF.'dimensions d ON d.id=t.'.$column.' AND d.type_='.(int)$axis.'
        WHERE c.ctype IN ('.CL_INCOME.','.CL_COGS.','.CL_EXPENSE.")".$range.'
        GROUP BY t.'.$column.', d.id, d.reference, d.name, d.closed ORDER BY t.'.$column, 'The tracking groups could not be calculated');
    if (!$result) return false;
    $rows = array();
    while ($row = db_fetch_assoc($result)) {
        $id = (int)$row['dimension_id'];
        $costs = round((float)$row['cogs'] + (float)$row['expense'], 6);
        $rows[] = array('dimension_id'=>$id,
            'state'=>$id === 0 ? 'unassigned' : ($row['known_id'] === null ? 'unknown' : 'classified'),
            'reference'=>$row['known_id'] === null ? '' : $row['reference'], 'name'=>$row['known_id'] === null ? '' : $row['name'],
            'closed'=>$row['known_id'] !== null && (int)$row['closed'] === 1,
            'revenue'=>round((float)$row['revenue'], 6), 'costs'=>$costs, 'result'=>round((float)$row['revenue'] - $costs, 6),
            'eligible_lines'=>(int)$row['eligible_lines'], 'eligible_absolute'=>round((float)$row['eligible_absolute'], 6));
    }
    return $rows;
}

/**
 * Coverage of one axis over a set of group rows (pure).
 *
 * @param array $groups Rows of dimension_tracking_groups()
 * @return array lines, absolute, classified_lines, classified_absolute, unassigned_lines, unknown_lines, line_percent, absolute_percent (null when no lines)
 */
function dimension_tracking_group_coverage($groups)
{
    $out = array('lines'=>0, 'absolute'=>0.0, 'classified_lines'=>0, 'classified_absolute'=>0.0, 'unassigned_lines'=>0, 'unknown_lines'=>0,
        'line_percent'=>null, 'absolute_percent'=>null);
    foreach ($groups as $row) {
        $out['lines'] += $row['eligible_lines'];
        $out['absolute'] += $row['eligible_absolute'];
        if ($row['state'] === 'classified') {
            $out['classified_lines'] += $row['eligible_lines'];
            $out['classified_absolute'] += $row['eligible_absolute'];
        } elseif ($row['state'] === 'unassigned') $out['unassigned_lines'] += $row['eligible_lines'];
        else $out['unknown_lines'] += $row['eligible_lines'];
    }
    $out['absolute'] = round($out['absolute'], 6);
    $out['classified_absolute'] = round($out['classified_absolute'], 6);
    if ($out['lines'] > 0) $out['line_percent'] = round(100.0 * $out['classified_lines'] / $out['lines'], 4);
    if ($out['absolute'] > 0) $out['absolute_percent'] = round(100.0 * $out['classified_absolute'] / $out['absolute'], 4);
    return $out;
}

/**
 * Classification status of one family on one axis (pure).
 *
 * @param int $lines Eligible lines
 * @param int $classified Lines with a known master on the axis
 * @return string complete|partial|none
 */
function dimension_tracking_family_state($lines, $classified)
{
    if ((int)$lines > 0 && (int)$classified >= (int)$lines) return 'complete';
    return (int)$classified > 0 ? 'partial' : 'none';
}

/**
 * Classification coverage per source transaction family (`gl_trans.type`).
 *
 * Families that posted no eligible line in the scope are not listed. A family is `complete` on an axis only when
 * every eligible line carries a known master of that axis; the workspace is complete only when every listed family is.
 *
 * @param string|null $from SQL date or null for lifetime
 * @param string $to SQL date
 * @param int|string $scope1 Axis 1 scope
 * @param int|string $scope2 Axis 2 scope
 * @return array|false families (rows: type, lines, absolute, axis1_classified, axis2_classified, axis1_state, axis2_state), state empty|complete|incomplete
 */
function dimension_tracking_family_coverage($from, $to, $scope1 = 0, $scope2 = 0)
{
    if (!dimension_tracking_valid_range($from, $to)) return false;
    $range = dimension_tracking_range_sql($from, $to, dimension_tracking_normalize_scope($scope1), dimension_tracking_normalize_scope($scope2));
    $result = db_query('SELECT t.type, COUNT(*) AS family_lines, SUM(ABS(t.amount)) AS family_absolute,
        SUM(CASE WHEN d1.id IS NULL THEN 0 ELSE 1 END) AS axis1_classified, SUM(CASE WHEN d2.id IS NULL THEN 0 ELSE 1 END) AS axis2_classified'
        .dimension_tracking_from_sql().' LEFT JOIN '.TB_PREF.'dimensions d1 ON d1.id=t.dimension_id AND d1.type_=1
        LEFT JOIN '.TB_PREF.'dimensions d2 ON d2.id=t.dimension2_id AND d2.type_=2
        WHERE c.ctype IN ('.CL_INCOME.','.CL_COGS.','.CL_EXPENSE.')'.$range.' GROUP BY t.type ORDER BY t.type', 'The tracking coverage could not be calculated');
    if (!$result) return false;
    $families = array();
    $complete = true;
    while ($row = db_fetch_assoc($result)) {
        $family = array('type'=>(int)$row['type'], 'lines'=>(int)$row['family_lines'], 'absolute'=>round((float)$row['family_absolute'], 6),
            'axis1_classified'=>(int)$row['axis1_classified'], 'axis2_classified'=>(int)$row['axis2_classified']);
        $family['axis1_state'] = dimension_tracking_family_state($family['lines'], $family['axis1_classified']);
        $family['axis2_state'] = dimension_tracking_family_state($family['lines'], $family['axis2_classified']);
        if ($family['axis1_state'] !== 'complete' || $family['axis2_state'] !== 'complete') $complete = false;
        $families[] = $family;
    }
    return array('families'=>$families, 'state'=>count($families) === 0 ? 'empty' : ($complete ? 'complete' : 'incomplete'));
}

/**
 * Source drilldown by P&L account in the same scope, with the same sign rule.
 *
 * @param string|null $from SQL date or null for lifetime
 * @param string $to SQL date
 * @param int|string $scope1 Axis 1 scope
 * @param int|string $scope2 Axis 2 scope
 * @return array|false Rows: account, account_name, ctype, revenue, costs, result, eligible_lines (limited; `truncated` flag on the wrapper)
 */
function dimension_tracking_account_drilldown($from, $to, $scope1 = 0, $scope2 = 0)
{
    if (!dimension_tracking_valid_range($from, $to)) return false;
    $range = dimension_tracking_range_sql($from, $to, dimension_tracking_normalize_scope($scope1), dimension_tracking_normalize_scope($scope2));
    $result = db_query('SELECT t.account, a.account_name, c.ctype, '.dimension_tracking_measure_sql().dimension_tracking_from_sql().'
        WHERE c.ctype IN ('.CL_INCOME.','.CL_COGS.','.CL_EXPENSE.')'.$range.'
        GROUP BY t.account, a.account_name, c.ctype ORDER BY c.ctype, t.account LIMIT '.(dimension_tracking_row_limit() + 1), 'The tracking drilldown could not be calculated');
    if (!$result) return false;
    $rows = array();
    while ($row = db_fetch_assoc($result)) {
        $costs = round((float)$row['cogs'] + (float)$row['expense'], 6);
        $rows[] = array('account'=>$row['account'], 'account_name'=>$row['account_name'], 'ctype'=>(int)$row['ctype'],
            'revenue'=>round((float)$row['revenue'], 6), 'costs'=>$costs, 'result'=>round((float)$row['revenue'] - $costs, 6),
            'eligible_lines'=>(int)$row['eligible_lines']);
    }
    $truncated = count($rows) > dimension_tracking_row_limit();
    if ($truncated) array_pop($rows);
    return array('rows'=>$rows, 'truncated'=>$truncated);
}

/**
 * Budget comparison of the scope's P&L (DIM-BUD-001 basis, per account).
 *
 * Only a period of whole calendar months is comparable. Every P&L account that has an actual in the period or a budget
 * row in the months it touches is read through the shared basis; an account without a complete budget is counted and left
 * out of the totals, so the comparison is never silently partial.
 *
 * @param string $from SQL date
 * @param string $to SQL date
 * @param int|string $scope1 Axis 1 scope
 * @param int|string $scope2 Axis 2 scope
 * @return array state comparable|not_comparable|absent|incomplete|unreadable, reason, revenue_budget, cost_budget, result_budget, accounts, accounts_without_budget
 */
function dimension_tracking_budget_summary($from, $to, $scope1 = 0, $scope2 = 0)
{
    $out = array('state'=>'unreadable', 'reason'=>'invalid', 'revenue_budget'=>null, 'cost_budget'=>null, 'result_budget'=>null,
        'accounts'=>0, 'accounts_without_budget'=>0);
    if (!dimension_tracking_valid_range($from, $to) || $from === null || $from === '') return $out;
    $period = dimension_budget_period($from, $to);
    if (!$period['comparable']) { $out['state'] = 'not_comparable'; $out['reason'] = $period['reason']; return $out; }
    $scope1 = dimension_tracking_normalize_scope($scope1);
    $scope2 = dimension_tracking_normalize_scope($scope2);
    $months = dimension_budget_months($from, $to);
    $keys = array_keys($months);
    $window_from = $months[$keys[0]]['start'];
    $window_to = $months[$keys[count($keys) - 1]]['end'];
    $budget_scope = '';
    foreach (array('dimension_id'=>$scope1, 'dimension2_id'=>$scope2) as $column=>$scope) {
        if ($scope !== 0) $budget_scope .= ' AND b.'.$column.'='.($scope < 0 ? 0 : $scope);
    }
    $accounts = db_query('SELECT DISTINCT x.account, x.ctype FROM (
        SELECT t.account, c.ctype'.dimension_tracking_from_sql().' WHERE c.ctype IN ('.CL_INCOME.','.CL_COGS.','.CL_EXPENSE.")
            AND t.tran_date >= '".$from."' AND t.tran_date <= '".$to."'".dimension_tracking_range_sql(null, $to, $scope1, $scope2)."
        UNION
        SELECT b.account, c.ctype FROM ".TB_PREF.'budget_trans b
            INNER JOIN '.TB_PREF.'chart_master a ON a.account_code=b.account
            INNER JOIN '.TB_PREF.'chart_types ct ON ct.id=a.account_type
            INNER JOIN '.TB_PREF.'chart_class c ON c.cid=ct.class_id
            WHERE c.ctype IN ('.CL_INCOME.','.CL_COGS.','.CL_EXPENSE.")
            AND b.tran_date >= '".$window_from."' AND b.tran_date <= '".$window_to."'".$budget_scope.') x ORDER BY x.account', 'The budget accounts could not be read');
    if (!$accounts) return $out;
    $revenue = $cost = 0.0;
    $visited = array();
    while ($account = db_fetch_assoc($accounts)) $visited[] = $account;
    // DIM performance gate: the budget rows of 50 accounts are read in one statement instead of one statement per account
    // (3,600 statements for the exception queue of a 300-account chart before this change).
    foreach (array_chunk($visited, 50) as $chunk) {
        $codes = array();
        foreach ($chunk as $account) $codes[] = $account['account'];
        dimension_budget_prefetch_rows($codes, $scope1, $scope2, $from, $to);
        foreach ($chunk as $account) {
            $out['accounts']++;
            $amount = dimension_budget_report_amount($account['account'], $scope1, $scope2, $from, $to);
            if (!$amount['available']) { $out['accounts_without_budget']++; continue; }
            if ((int)$account['ctype'] === CL_INCOME) $revenue += -$amount['amount']; else $cost += $amount['amount'];
        }
    }
    dimension_budget_prefetch_clear();
    if ($out['accounts'] === 0 || $out['accounts_without_budget'] === $out['accounts']) {
        $out['state'] = 'absent'; $out['reason'] = 'no_budget'; return $out;
    }
    $out['revenue_budget'] = round($revenue, 6);
    $out['cost_budget'] = round($cost, 6);
    $out['result_budget'] = round($revenue - $cost, 6);
    $out['state'] = $out['accounts_without_budget'] > 0 ? 'incomplete' : 'comparable';
    $out['reason'] = $out['state'] === 'comparable' ? 'ok' : 'missing_accounts';
    return $out;
}

/**
 * Reviewed label of a margin state.
 *
 * @param string $state Margin state
 * @return string
 */
function dimension_tracking_margin_text($state)
{
    $texts = array(
        'empty'=>_('No eligible P&L postings in this scope.'),
        'cost_only'=>_('Costs only: there is no revenue, so no margin is shown.'),
        'revenue_only'=>_('Revenue only: no costs are recorded in this scope, so no margin is shown.'),
        'no_revenue'=>_('There is no positive revenue, so no margin is shown.'));
    return isset($texts[$state]) ? $texts[$state] : '';
}

/**
 * One workspace read: period and lifetime summaries, per-axis groups with coverage, family coverage and budget.
 *
 * @param string $from SQL period start
 * @param string $to SQL period end and lifetime cutoff
 * @param int|string $scope1 Axis 1 scope
 * @param int|string $scope2 Axis 2 scope
 * @return array|false period, lifetime, axis1, axis2 (groups, coverage), families, budget; false when any part is unreadable
 */
function dimension_tracking_workspace($from, $to, $scope1 = 0, $scope2 = 0)
{
    if (!dimension_tracking_valid_range($from, $to) || $from === null || $from === '') return false;
    $period = dimension_tracking_summary($from, $to, $scope1, $scope2);
    $lifetime = dimension_tracking_summary(null, $to, $scope1, $scope2);
    $families = dimension_tracking_family_coverage($from, $to, $scope1, $scope2);
    $axis1 = dimension_tracking_groups(1, $from, $to, $scope1, $scope2);
    $axis2 = dimension_tracking_groups(2, $from, $to, $scope1, $scope2);
    if ($period === false || $lifetime === false || $families === false || $axis1 === false || $axis2 === false) return false;
    return array('period'=>$period, 'lifetime'=>$lifetime, 'families'=>$families,
        'axis1'=>array('groups'=>$axis1, 'coverage'=>dimension_tracking_group_coverage($axis1)),
        'axis2'=>array('groups'=>$axis2, 'coverage'=>dimension_tracking_group_coverage($axis2)),
        'budget'=>dimension_tracking_budget_summary($from, $to, $scope1, $scope2));
}
