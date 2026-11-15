<?php
/**
 * DIM-BUD-001: comparable monthly budget basis and absence semantics.
 *
 * Budget entry (`gl/gl_budget.php`) stores one `budget_trans` row per account, month and exact
 * Dimension pair, dated at the month start, in the same debit-positive sign as `gl_trans`. The reports
 * (`gl/inquiry/profit_loss.php`, `reporting/rep707.php`) summed raw rows between two arbitrary dates:
 *
 * - a partial period (for example the 1st to the 15th) picked up the whole month's budget, and the
 *   16th to the end picked up none, so the variance compared unlike periods;
 * - "no row" and an explicit zero row were both an empty or zero sum;
 * - with an "All" axis a company aggregate row and its detailed rows were added together.
 *
 * This authority is the one shared definition (read-only, no write, no schema):
 *
 * - **Eligibility:** a month is comparable only when the whole calendar month lies inside the period.
 *   Any partial month makes the period not comparable (`partial_start`, `partial_end`, `partial_both`);
 *   the whole-month window and the optional full-month figure are still exposed.
 * - **Existence:** each month is `present` (at least one matching row, even an explicit zero) or `absent`.
 *   A basis is `complete` (every month present), `incomplete`, `absent` or `not_comparable`.
 * - **Scope:** an axis scope of 0 means all, a negative value the unassigned coordinate 0 and a positive
 *   value the exact Dimension. When an axis is open, a row whose open coordinates are all 0 is the
 *   aggregate; when aggregate and detailed rows both exist in a month the aggregate is the budget basis
 *   and the detail is never added (`overlap`).
 * - **Sign:** amounts keep the stored sign; nothing is flipped. Variance is actual minus budget.
 *
 * Rows are interpreted by the calendar month of their date; a row not dated on the 1st is flagged
 * (`off_boundary`) because a fiscal year that starts mid-month cannot be a calendar-month budget.
 * PHP 5.6 compatible syntax. Dates are SQL `Y-m-d`.
 */

/** Largest number of months one comparison enumerates. */
function dimension_budget_max_months()
{
    return 600;
}

/**
 * Whether a string is a real SQL calendar date.
 *
 * @param mixed $date Candidate
 * @return bool
 */
function dimension_budget_valid_date($date)
{
    return is_string($date) && preg_match('/^([0-9]{4})-([0-9]{2})-([0-9]{2})$/D', $date, $m) === 1 && checkdate((int)$m[2], (int)$m[3], (int)$m[1]);
}

/**
 * Last day of the month of a valid SQL date.
 *
 * @param string $date SQL date
 * @return string SQL date
 */
function dimension_budget_month_end($date)
{
    return substr($date, 0, 8).sprintf('%02d', (int)date('t', mktime(0, 0, 0, (int)substr($date, 5, 2), 1, (int)substr($date, 0, 4))));
}

/**
 * Calendar months touched by a period, each with its whole-month flag.
 *
 * @param string $from SQL date
 * @param string $to SQL date
 * @return array|false Month key (Y-m) => array(start, end, whole); false when invalid or too long
 */
function dimension_budget_months($from, $to)
{
    if (!dimension_budget_valid_date($from) || !dimension_budget_valid_date($to) || $from > $to) return false;
    $year = (int)substr($from, 0, 4);
    $month = (int)substr($from, 5, 2);
    $months = array();
    while (true) {
        $start = sprintf('%04d-%02d-01', $year, $month);
        $end = dimension_budget_month_end($start);
        $months[substr($start, 0, 7)] = array('start'=>$start, 'end'=>$end, 'whole'=>($from <= $start && $to >= $end));
        if ($end >= $to) break;
        if (++$month > 12) { $month = 1; $year++; }
        if (count($months) > dimension_budget_max_months()) return false;
    }
    return $months;
}

/**
 * Comparability of a period against monthly budgets.
 *
 * @param string $from SQL date
 * @param string $to SQL date
 * @return array comparable, reason ok|invalid|partial_start|partial_end|partial_both, months, whole_from, whole_to
 */
function dimension_budget_period($from, $to)
{
    $period = array('from'=>$from, 'to'=>$to, 'comparable'=>false, 'reason'=>'invalid', 'months'=>array(), 'whole_from'=>null, 'whole_to'=>null);
    $months = dimension_budget_months($from, $to);
    if ($months === false) return $period;
    $period['months'] = $months;
    $start_partial = $from !== substr($from, 0, 8).'01';
    $end_partial = $to !== dimension_budget_month_end($to);
    $period['reason'] = $start_partial && $end_partial ? 'partial_both' : ($start_partial ? 'partial_start' : ($end_partial ? 'partial_end' : 'ok'));
    $period['comparable'] = $period['reason'] === 'ok';
    $whole = array();
    foreach ($months as $month) if ($month['whole']) $whole[] = $month;
    if (count($whole)) {
        $period['whole_from'] = $whole[0]['start'];
        $period['whole_to'] = $whole[count($whole) - 1]['end'];
    }
    return $period;
}

/**
 * Axis scope test: 0 = all, negative = unassigned (coordinate 0), positive = that exact Dimension.
 *
 * @param int|string $scope Reporting scope of one axis
 * @param int|string $coordinate Stored coordinate of a budget row
 * @return bool
 */
function dimension_budget_scope_matches($scope, $coordinate)
{
    $scope = (int)$scope;
    if ($scope === 0) return true;
    return (int)$coordinate === ($scope < 0 ? 0 : $scope);
}

/**
 * Summarize the budget of each month of a period from candidate rows (pure; no database).
 *
 * @param array $rows Rows with tran_date (SQL), dimension_id, dimension2_id, amount
 * @param string $from SQL date
 * @param string $to SQL date
 * @param int $scope1 Axis 1 scope
 * @param int $scope2 Axis 2 scope
 * @return array|false Month key => array(whole, state present|absent, amount, rows, overlap, off_boundary); false when the period is invalid
 */
function dimension_budget_month_basis($rows, $from, $to, $scope1, $scope2)
{
    $months = dimension_budget_months($from, $to);
    if ($months === false) return false;
    $summary = array();
    foreach ($months as $key=>$month) {
        $summary[$key] = array('whole'=>$month['whole'], 'state'=>'absent', 'amount'=>null, 'rows'=>0, 'overlap'=>false,
            'off_boundary'=>false, 'aggregate'=>array(), 'detail'=>array());
    }
    foreach ($rows as $row) {
        if (!isset($row['tran_date']) || !dimension_budget_valid_date($row['tran_date'])) continue;
        $key = substr($row['tran_date'], 0, 7);
        if (!isset($summary[$key])) continue;
        $d1 = isset($row['dimension_id']) ? (int)$row['dimension_id'] : 0;
        $d2 = isset($row['dimension2_id']) ? (int)$row['dimension2_id'] : 0;
        if (!dimension_budget_scope_matches($scope1, $d1) || !dimension_budget_scope_matches($scope2, $d2)) continue;
        $aggregate = ((int)$scope1 !== 0 || $d1 === 0) && ((int)$scope2 !== 0 || $d2 === 0);
        $summary[$key][$aggregate ? 'aggregate' : 'detail'][] = (float)$row['amount'];
        $summary[$key]['rows']++;
        if (substr($row['tran_date'], 8, 2) !== '01') $summary[$key]['off_boundary'] = true;
    }
    foreach ($summary as $key=>$month) {
        if ($month['rows'] > 0) {
            $chosen = count($month['aggregate']) ? $month['aggregate'] : $month['detail'];
            $summary[$key]['state'] = 'present';
            $summary[$key]['amount'] = round(array_sum($chosen), 6);
            $summary[$key]['overlap'] = count($month['aggregate']) > 0 && count($month['detail']) > 0;
        }
        unset($summary[$key]['aggregate'], $summary[$key]['detail']);
    }
    return $summary;
}

/**
 * Basis state and amount over a set of months.
 *
 * @param array $months Month summaries
 * @return array state complete|incomplete|absent, amount (null when absent), missing months, overlap
 */
function dimension_budget_fold($months)
{
    $present = 0;
    $sum = 0.0;
    $missing = array();
    $overlap = false;
    foreach ($months as $key=>$month) {
        if ($month['state'] === 'present') { $present++; $sum += $month['amount']; } else $missing[] = $key;
        if ($month['overlap']) $overlap = true;
    }
    $state = $present === 0 ? 'absent' : (count($missing) ? 'incomplete' : 'complete');
    return array('state'=>$state, 'amount'=>$present === 0 ? null : round($sum, 6), 'missing'=>$missing, 'overlap'=>$overlap);
}

/**
 * Comparable budget basis of one account/scope over one period (pure).
 *
 * @param array $rows Candidate rows (see dimension_budget_month_basis)
 * @param string $from SQL date
 * @param string $to SQL date
 * @param int $scope1 Axis 1 scope
 * @param int $scope2 Axis 2 scope
 * @return array period, months, state, amount, missing, overlap, full_month (state, amount, missing)
 */
function dimension_budget_basis_from_rows($rows, $from, $to, $scope1, $scope2)
{
    $period = dimension_budget_period($from, $to);
    $basis = array('period'=>$period, 'months'=>array(), 'state'=>'not_comparable', 'amount'=>null, 'missing'=>array(), 'overlap'=>false,
        'full_month'=>array('state'=>'absent', 'amount'=>null, 'missing'=>array()));
    $months = dimension_budget_month_basis($rows, $from, $to, $scope1, $scope2);
    if ($months === false) return $basis;
    $basis['months'] = $months;
    $full = dimension_budget_fold($months);
    $basis['full_month'] = array('state'=>$full['state'], 'amount'=>$full['amount'], 'missing'=>$full['missing']);
    $basis['overlap'] = $full['overlap'];
    if ($period['comparable']) {
        $basis['state'] = $full['state'];
        $basis['amount'] = $full['amount'];
        $basis['missing'] = $full['missing'];
    }
    return $basis;
}

/**
 * Achieved percentage with the reports' existing rule (0/0 = 0, no budget = 999, capped at 999).
 *
 * @param float $actual Actual
 * @param float $budget Budget
 * @return float
 */
function dimension_budget_achieved($actual, $budget)
{
    if ($actual == 0 && $budget == 0) return 0;
    if ($budget == 0) return 999;
    $value = $actual / $budget * 100.0;
    return $value > 999 ? 999 : $value;
}

/**
 * Variance of an actual against a basis, never over an incomparable or incomplete budget.
 *
 * @param array $basis Result of dimension_budget_basis_from_rows()
 * @param float $actual Actual for the same period and scope
 * @param bool $full_month Use the explicit full-month figure instead of the whole-month basis
 * @return array available, reason ok|not_comparable|no_budget|missing_months, budget, variance, achieved, full_month
 */
function dimension_budget_variance($basis, $actual, $full_month = false)
{
    $result = array('available'=>false, 'reason'=>'not_comparable', 'budget'=>null, 'variance'=>null, 'achieved'=>null, 'full_month'=>(bool)$full_month);
    if ($full_month) {
        $state = $basis['full_month']['state'];
        $budget = $basis['full_month']['amount'];
    } else {
        if ($basis['state'] === 'not_comparable') return $result;
        $state = $basis['state'];
        $budget = $basis['amount'];
    }
    $result['budget'] = $budget;
    if ($state === 'absent') { $result['reason'] = 'no_budget'; return $result; }
    if ($state === 'incomplete') { $result['reason'] = 'missing_months'; return $result; }
    $result['available'] = true;
    $result['reason'] = 'ok';
    $result['variance'] = round($actual - $budget, 6);
    $result['achieved'] = dimension_budget_achieved($actual, $budget);
    return $result;
}

/**
 * Read the candidate budget rows of one account for the calendar months a period touches.
 *
 * @param string $account Account code
 * @param int $scope1 Axis 1 scope
 * @param int $scope2 Axis 2 scope
 * @param string $from SQL date
 * @param string $to SQL date
 * @return array|false Rows, or false when the period is invalid or the read fails
 */
function dimension_budget_read_rows($account, $scope1, $scope2, $from, $to)
{
    $months = dimension_budget_months($from, $to);
    if ($months === false) return false;
    $keys = array_keys($months);
    $first = $months[$keys[0]]['start'];
    $last = $months[$keys[count($keys) - 1]]['end'];
    $sql = 'SELECT tran_date, dimension_id, dimension2_id, amount FROM '.TB_PREF.'budget_trans WHERE account='.db_escape($account)
        ." AND tran_date >= '".$first."' AND tran_date <= '".$last."'";
    if ((int)$scope1 !== 0) $sql .= ' AND dimension_id='.((int)$scope1 < 0 ? 0 : (int)$scope1);
    if ((int)$scope2 !== 0) $sql .= ' AND dimension2_id='.((int)$scope2 < 0 ? 0 : (int)$scope2);
    $result = db_query($sql, 'The budget rows could not be read');
    if (!$result) return false;
    $rows = array();
    while ($row = db_fetch_assoc($result)) $rows[] = $row;
    return $rows;
}

/**
 * Report-facing comparable budget for one account: the amount to show, or why none can be compared.
 *
 * @param string $account Account code
 * @param int $scope1 Axis 1 scope (0 = all, negative = unassigned)
 * @param int $scope2 Axis 2 scope
 * @param string $from SQL date of the budget period
 * @param string $to SQL date of the budget period
 * @param bool $full_month Show the full-month figure of the touched months
 * @return array available, amount (0.0 when none), state, reason, overlap, full_month
 */
function dimension_budget_report_amount($account, $scope1, $scope2, $from, $to, $full_month = false)
{
    $rows = dimension_budget_read_rows($account, $scope1, $scope2, $from, $to);
    $out = array('available'=>false, 'amount'=>0.0, 'state'=>'not_comparable', 'reason'=>'not_comparable', 'overlap'=>false, 'full_month'=>(bool)$full_month);
    if ($rows === false) return $out;
    $basis = dimension_budget_basis_from_rows($rows, $from, $to, $scope1, $scope2);
    $variance = dimension_budget_variance($basis, 0, $full_month);
    $out['state'] = $full_month ? $basis['full_month']['state'] : $basis['state'];
    $out['reason'] = $variance['reason'];
    $out['available'] = $variance['available'];
    $out['overlap'] = $basis['overlap'];
    $out['amount'] = $variance['budget'] === null ? 0.0 : (float)$variance['budget'];
    return $out;
}

/**
 * Same-scope drilldown: the budget month by month with existence and the exact rows' pair, for the
 * budget entry page. No payload; amounts and pairs only.
 *
 * @param string $account Account code
 * @param int $scope1 Axis 1 scope
 * @param int $scope2 Axis 2 scope
 * @param string $from SQL date
 * @param string $to SQL date
 * @return array|false basis result, or false when unreadable
 */
function dimension_budget_drilldown($account, $scope1, $scope2, $from, $to)
{
    $rows = dimension_budget_read_rows($account, $scope1, $scope2, $from, $to);
    return $rows === false ? false : dimension_budget_basis_from_rows($rows, $from, $to, $scope1, $scope2);
}

/**
 * Human reason for an N/A budget (untranslated key to a translated sentence).
 *
 * @param string $reason Reason from dimension_budget_period() or dimension_budget_variance()
 * @return string
 */
function dimension_budget_reason_text($reason)
{
    $texts = array(
        'partial_start'=>_('The period starts inside a month, so the monthly budget cannot be compared.'),
        'partial_end'=>_('The period ends inside a month, so the monthly budget cannot be compared.'),
        'partial_both'=>_('The period starts and ends inside a month, so the monthly budget cannot be compared.'),
        'not_comparable'=>_('The period contains a partial month, so the monthly budget cannot be compared.'),
        'invalid'=>_('The period is not valid for a budget comparison.'),
        'no_budget'=>_('No budget has been entered for this scope and period.'),
        'missing_months'=>_('Some months of this period have no budget entered.'));
    return isset($texts[$reason]) ? $texts[$reason] : '';
}
