<?php
/**
 * DIM-REP-006: unified comparisons, single-axis account matrix, exception work queue and the shared export dataset.
 *
 * One read-only authority behind the Dimension exceptions page (`dimensions/inquiry/dimension_exceptions.php`) and report 502
 * (PDF and Excel). The page and both export formats render the SAME dataset (`dimension_export_dataset()`), so a number cannot
 * differ between them. It adds no ledger and no writer and stores no monetary data.
 *
 * - **Saved filters hold filters only:** comparison mode, the two axis scopes, the matrix axis and the reason list - never a
 *   period, an amount or a name. They are encoded as a short strict string, decoded with a whitelist, and kept in a cookie
 *   whose key carries the live grant fingerprint (`report_access_selection_key`), so a revoked or changed role never reads a
 *   stale saved scope. The money is always regenerated from the ledger.
 * - **Comparisons:** previous period (the same number of whole months, or the same number of days, immediately before),
 *   year to date against the same dates one year earlier, and budget through the DIM-BUD-001 basis (partial months and absent
 *   budgets are N/A with a reason). Revenue, costs and result come from the DIM-REP-003 summary of each range.
 * - **Single-axis account matrix:** P&L accounts by the Dimensions of ONE axis, read in ONE grouped statement without any tag
 *   join (a tag join would duplicate ledger rows). A cell is the result contribution of the account in the Dimension
 *   (credit-positive revenue, debit-positive cost: result = -amount); the busiest Dimensions get their own column, the rest
 *   fall into Unassigned and Other, so the table always adds up to the scope result.
 * - **Exception queue:** unassigned, invalid, closed, source-vs-GL, overrun and missing customer cost. Every item carries its
 *   reason, source, first and last posting, age at the period end and the action authority needed to resolve it. Counts and
 *   absolute-value coverage are both reported; a line can have more than one reason, so the reasons are not added together.
 *
 * Amounts are the stored company base-currency amounts. PHP 5.6 compatible syntax. Dates are SQL `Y-m-d`.
 */
require_once __DIR__.'/dimension_customer_economics.inc';

/** Comparison modes. */
function dimension_saved_filter_modes()
{
    return array('previous', 'ytd', 'budget');
}

/** Exception reasons in reporting order. */
function dimension_exception_reasons()
{
    return array('unassigned', 'invalid', 'closed', 'source_vs_gl', 'overrun', 'missing_customer_cost');
}

/**
 * Normalize a saved filter (pure). Unknown keys, unknown values and malformed scopes reject the whole filter.
 *
 * @param mixed $raw Array of mode, scope1, scope2, axis, reasons
 * @return array|false The normalized filter or false
 */
function dimension_saved_filter_normalize($raw)
{
    if (!is_array($raw) || count(array_diff(array_keys($raw), array('mode', 'scope1', 'scope2', 'axis', 'reasons'))) > 0) return false;
    $mode = isset($raw['mode']) ? $raw['mode'] : 'previous';
    $axis = isset($raw['axis']) ? (string)$raw['axis'] : '1';
    if (!in_array($mode, dimension_saved_filter_modes(), true) || !in_array($axis, array('1', '2'), true)) return false;
    $scopes = array();
    foreach (array('scope1', 'scope2') as $key) {
        $value = isset($raw[$key]) ? $raw[$key] : 0;
        if (is_string($value) && preg_match('/^-?[0-9]{1,9}$/D', $value) === 1) $value = (int)$value;
        if (!is_int($value) || $value < -1) return false;
        $scopes[$key] = $value === -1 ? -1 : $value;
    }
    $reasons = isset($raw['reasons']) ? $raw['reasons'] : array();
    if (!is_array($reasons) || count($reasons) > count(dimension_exception_reasons())) return false;
    foreach ($reasons as $reason) if (!in_array($reason, dimension_exception_reasons(), true)) return false;
    $ordered = array();
    foreach (dimension_exception_reasons() as $reason) if (in_array($reason, $reasons, true)) $ordered[] = $reason;
    return array('mode'=>$mode, 'scope1'=>$scopes['scope1'], 'scope2'=>$scopes['scope2'], 'axis'=>(int)$axis, 'reasons'=>$ordered);
}

/**
 * Encode a saved filter as a short strict string.
 *
 * @param array $filter Normalized filter
 * @return string|false
 */
function dimension_saved_filter_encode($filter)
{
    $filter = dimension_saved_filter_normalize($filter);
    if ($filter === false) return false;
    return 'v1|'.$filter['mode'].'|'.$filter['scope1'].'|'.$filter['scope2'].'|'.$filter['axis'].'|'.implode(',', $filter['reasons']);
}

/**
 * Decode a saved filter string; anything not produced by the encoder is refused.
 *
 * @param mixed $text Stored text (for example a cookie value)
 * @return array|false
 */
function dimension_saved_filter_decode($text)
{
    if (!is_string($text) || strlen($text) > 200 || !preg_match('/^v1\|([a-z]+)\|(-?[0-9]{1,9})\|(-?[0-9]{1,9})\|([12])\|([a-z_,]*)$/D', $text, $m)) return false;
    return dimension_saved_filter_normalize(array('mode'=>$m[1], 'scope1'=>$m[2], 'scope2'=>$m[3], 'axis'=>$m[4], 'reasons'=>$m[5] === '' ? array() : explode(',', $m[5])));
}

/**
 * Cookie name of a saved filter slot, bound to the user, company and live grant fingerprint.
 *
 * @param int $slot Slot number
 * @return string
 */
function dimension_saved_filter_cookie_name($slot)
{
    return 'dimfilter['.report_access_selection_key('dimension_saved_filter', (int)$slot).']';
}

/**
 * Shift a SQL date by whole years; 29 February falls back to 28 February in a non-leap year (pure).
 *
 * @param string $date SQL date
 * @param int $years Years to add (negative for earlier)
 * @return string|false
 */
function dimension_comparison_shift_year($date, $years)
{
    if (!dimension_budget_valid_date($date)) return false;
    $year = (int)substr($date, 0, 4) + (int)$years;
    $month = (int)substr($date, 5, 2);
    $day = (int)substr($date, 8, 2);
    if ($year < 1 || $year > 9999) return false;
    $last = (int)substr(dimension_budget_month_end(sprintf('%04d-%02d-01', $year, $month)), 8, 2);
    return sprintf('%04d-%02d-%02d', $year, $month, min($day, $last));
}

/**
 * Previous period of a range (pure): the same number of whole months immediately before a whole-month range, otherwise the
 * same number of days immediately before.
 *
 * @param string $from SQL date
 * @param string $to SQL date
 * @return array|false from, to
 */
function dimension_comparison_previous_range($from, $to)
{
    if (!dimension_tracking_valid_range($from, $to) || $from === null || $from === '') return false;
    $period = dimension_budget_period($from, $to);
    $start = new DateTime($from.' 00:00:00 UTC');
    if ($period['comparable']) {
        $months = count($period['months']);
        $base_from = clone $start;
        $base_from->modify('-'.$months.' months');
        $base_to = clone $start;
        $base_to->modify('-1 day');
        return array('from'=>$base_from->format('Y-m-d'), 'to'=>$base_to->format('Y-m-d'));
    }
    $days = (int)round((strtotime($to.' 00:00:00 UTC') - strtotime($from.' 00:00:00 UTC')) / 86400) + 1;
    $base_to = clone $start;
    $base_to->modify('-1 day');
    $base_from = clone $base_to;
    $base_from->modify('-'.($days - 1).' days');
    return array('from'=>$base_from->format('Y-m-d'), 'to'=>$base_to->format('Y-m-d'));
}

/**
 * Variance of a measure against its base (pure). A zero base has no percentage.
 *
 * @param float|null $current Current amount
 * @param float|null $base Comparison amount, null when unavailable
 * @return array amount, percent (null when the base is zero or unavailable)
 */
function dimension_comparison_variance($current, $base)
{
    if ($base === null || $current === null) return array('amount'=>null, 'percent'=>null);
    $amount = round((float)$current - (float)$base, 6);
    return array('amount'=>$amount, 'percent'=>abs((float)$base) < 0.005 ? null : round($amount / abs((float)$base) * 100.0, 4));
}

/**
 * Run one comparison.
 *
 * @param string $mode previous|ytd|budget
 * @param string $from SQL period start (for ytd the fiscal start is used instead)
 * @param string $to SQL period end
 * @param int|string $scope1 Axis 1 scope
 * @param int|string $scope2 Axis 2 scope
 * @param string $fiscal_start SQL fiscal year start (ytd only)
 * @return array|false mode, current (from, to, summary), base (state, reason, from, to, summary or budget), rows (measure, current, base, amount, percent), or false when invalid
 */
function dimension_comparison_run($mode, $from, $to, $scope1 = 0, $scope2 = 0, $fiscal_start = '')
{
    if (!in_array($mode, dimension_saved_filter_modes(), true) || !dimension_tracking_valid_range($from, $to) || $from === null || $from === '') return false;
    if ($mode === 'ytd') {
        if (!dimension_budget_valid_date($fiscal_start) || $fiscal_start > $to) return false;
        $from = $fiscal_start;
    }
    $current = dimension_tracking_summary($from, $to, $scope1, $scope2);
    if ($current === false) return false;
    $out = array('mode'=>$mode, 'current'=>array('from'=>$from, 'to'=>$to, 'summary'=>$current), 'base'=>array('state'=>'ok', 'reason'=>'ok', 'from'=>null, 'to'=>null, 'values'=>null));
    $keys = array('revenue', 'costs', 'result');
    if ($mode === 'budget') {
        $budget = dimension_tracking_budget_summary($from, $to, $scope1, $scope2);
        $out['base']['from'] = $from;
        $out['base']['to'] = $to;
        $out['base']['state'] = $budget['state'];
        $out['base']['reason'] = $budget['reason'];
        if ($budget['revenue_budget'] !== null)
            $out['base']['values'] = array('revenue'=>$budget['revenue_budget'], 'costs'=>$budget['cost_budget'], 'result'=>$budget['result_budget']);
    } else {
        $range = $mode === 'ytd' ? array('from'=>dimension_comparison_shift_year($from, -1), 'to'=>dimension_comparison_shift_year($to, -1))
            : dimension_comparison_previous_range($from, $to);
        if ($range === false || $range['from'] === false || $range['to'] === false) { $out['base']['state'] = 'unreadable'; $out['base']['reason'] = 'invalid'; }
        else {
            $base = dimension_tracking_summary($range['from'], $range['to'], $scope1, $scope2);
            if ($base === false) return false;
            $out['base']['from'] = $range['from'];
            $out['base']['to'] = $range['to'];
            $out['base']['values'] = array('revenue'=>$base['revenue'], 'costs'=>$base['costs'], 'result'=>$base['result']);
        }
    }
    $out['rows'] = array();
    foreach ($keys as $key) {
        $base_value = $out['base']['values'] === null ? null : $out['base']['values'][$key];
        $variance = dimension_comparison_variance($current[$key], $base_value);
        $out['rows'][] = array('measure'=>$key, 'current'=>$current[$key], 'base'=>$base_value, 'amount'=>$variance['amount'], 'percent'=>$variance['percent']);
    }
    return $out;
}

/**
 * Single-axis account matrix of the P&L, in ONE grouped statement and without any tag join.
 *
 * @param int $axis 1 or 2: the axis shown as columns
 * @param string $from SQL period start
 * @param string $to SQL period end
 * @param int|string $other_scope Scope of the OTHER axis (the matrix axis itself is never narrowed)
 * @param int $columns Number of Dimensions that get their own column (the busiest by absolute amount)
 * @return array|false columns (key, id, label), rows (account, account_name, ctype, cells, total), totals, truncated; false when invalid
 */
function dimension_comparison_account_matrix($axis, $from, $to, $other_scope = 0, $columns = 5)
{
    if (!in_array((int)$axis, array(1, 2), true) || !dimension_tracking_valid_range($from, $to) || $from === null || $from === '') return false;
    $column = (int)$axis === 1 ? 'dimension_id' : 'dimension2_id';
    $range = dimension_tracking_range_sql($from, $to, (int)$axis === 1 ? 0 : dimension_tracking_normalize_scope($other_scope), (int)$axis === 2 ? 0 : dimension_tracking_normalize_scope($other_scope));
    $busiest = db_query('SELECT t.'.$column.' AS dimension_id, d.reference, d.name, SUM(ABS(t.amount)) AS absolute'.dimension_tracking_from_sql()
        .' INNER JOIN '.TB_PREF.'dimensions d ON d.id=t.'.$column.' AND d.type_='.(int)$axis.'
        WHERE c.ctype IN ('.CL_INCOME.','.CL_COGS.','.CL_EXPENSE.')'.$range.' GROUP BY t.'.$column.', d.reference, d.name ORDER BY absolute DESC, t.'.$column.' LIMIT '.max(1, min(8, (int)$columns)), 'The matrix columns could not be read');
    if (!$busiest) return false;
    $cols = array();
    $ids = array();
    while ($row = db_fetch_assoc($busiest)) {
        $cols[] = array('key'=>'d'.(int)$row['dimension_id'], 'id'=>(int)$row['dimension_id'], 'label'=>$row['reference'].' '.$row['name']);
        $ids[] = (int)$row['dimension_id'];
    }
    $cols[] = array('key'=>'unassigned', 'id'=>0, 'label'=>_('Unassigned'));
    $cols[] = array('key'=>'other', 'id'=>-1, 'label'=>_('Other'));
    $bucket = count($ids) ? 'CASE WHEN t.'.$column.' IN ('.implode(',', $ids).') THEN t.'.$column.' WHEN t.'.$column.'=0 THEN 0 ELSE -1 END' : 'CASE WHEN t.'.$column.'=0 THEN 0 ELSE -1 END';
    $result = db_query('SELECT t.account, a.account_name, c.ctype, '.$bucket.' AS bucket, -SUM(t.amount) AS result_amount'.dimension_tracking_from_sql()
        .' WHERE c.ctype IN ('.CL_INCOME.','.CL_COGS.','.CL_EXPENSE.')'.$range.' GROUP BY t.account, a.account_name, c.ctype, bucket ORDER BY c.ctype, t.account LIMIT '
        .((dimension_tracking_row_limit() + 1) * (count($cols))), 'The account matrix could not be calculated');
    if (!$result) return false;
    $keys = array();
    foreach ($cols as $col) $keys[$col['id']] = $col['key'];
    $rows = array();
    while ($row = db_fetch_assoc($result)) {
        $account = (string)$row['account'];
        if (!isset($rows[$account])) {
            $cells = array();
            foreach ($cols as $col) $cells[$col['key']] = 0.0;
            $rows[$account] = array('account'=>$account, 'account_name'=>$row['account_name'], 'ctype'=>(int)$row['ctype'], 'cells'=>$cells, 'total'=>0.0);
        }
        $rows[$account]['cells'][$keys[(int)$row['bucket']]] = round((float)$row['result_amount'], 6);
    }
    $truncated = count($rows) > dimension_tracking_row_limit();
    $rows = array_slice(array_values($rows), 0, dimension_tracking_row_limit());
    $totals = array();
    foreach ($cols as $col) $totals[$col['key']] = 0.0;
    $totals['total'] = 0.0;
    foreach ($rows as $index => $row) {
        $total = round(array_sum($row['cells']), 6);
        $rows[$index]['total'] = $total;
        foreach ($row['cells'] as $key => $amount) $totals[$key] = round($totals[$key] + $amount, 6);
        $totals['total'] = round($totals['total'] + $total, 6);
    }
    return array('axis'=>(int)$axis, 'columns'=>$cols, 'rows'=>$rows, 'totals'=>$totals, 'truncated'=>$truncated);
}

/** Reviewed label of an exception reason. */
function dimension_exception_reason_text($reason)
{
    $texts = array(
        'unassigned'=>_('Unassigned: a P&L posting without a Dimension on an enabled category'),
        'invalid'=>_('Invalid: a posting whose Dimension has no master of that category'),
        'closed'=>_('Closed: postings to a Dimension that is now closed'),
        'source_vs_gl'=>_('Source vs GL: a customer document whose GL lines differ from its header classification'),
        'overrun'=>_('Overrun: costs above the comparable monthly budget'),
        'missing_customer_cost'=>_('Missing customer cost: a customer with revenue and no matched cost'));
    return isset($texts[$reason]) ? $texts[$reason] : $reason;
}

/** Reviewed next step of an exception reason. It names the work; it grants no authority. */
function dimension_exception_reason_action($reason)
{
    $texts = array(
        'unassigned'=>_('Classify it through its source document or a reviewed correcting journal.'),
        'invalid'=>_('Restore the missing master or correct the classification with a reviewed journal; history is not rewritten.'),
        'closed'=>_('Re-open the Dimension if the postings are legitimate, or correct the classification.'),
        'source_vs_gl'=>_('Compare the document header with its GL lines and correct the one that is wrong.'),
        'overrun'=>_('Review the monthly budget or the costs of this Dimension.'),
        'missing_customer_cost'=>_('Record the cost through a customer delivery, or review the customer attribution. It is not estimated.'));
    return isset($texts[$reason]) ? $texts[$reason] : '';
}

/** Security area that authorizes resolving the reason (names the authority; nothing is granted here). */
function dimension_exception_reason_authority($reason)
{
    $areas = array('unassigned'=>'SA_JOURNALENTRY', 'invalid'=>'SA_JOURNALENTRY', 'closed'=>'SA_DIMENSION', 'source_vs_gl'=>'SA_JOURNALENTRY',
        'overrun'=>'SA_BUDGETENTRY', 'missing_customer_cost'=>'SA_SALESDELIVERY');
    return isset($areas[$reason]) ? $areas[$reason] : '';
}

/** Age in whole days from a SQL date to the period end (never negative). */
function dimension_exception_age_days($first_date, $to)
{
    if (!is_string($first_date) || $first_date === '') return null;
    return max(0, (int)floor((strtotime($to.' 00:00:00 UTC') - strtotime($first_date.' 00:00:00 UTC')) / 86400));
}

/** Build one queue item. */
function dimension_exception_item($reason, $axis, $label, $lines, $absolute, $first_date, $last_date, $to, $source = null)
{
    return array('reason'=>$reason, 'axis'=>(int)$axis, 'subject'=>$label, 'source'=>$source, 'lines'=>(int)$lines, 'absolute'=>round((float)$absolute, 6),
        'first_date'=>$first_date, 'last_date'=>$last_date, 'age_days'=>dimension_exception_age_days($first_date, $to),
        'action'=>dimension_exception_reason_action($reason), 'authority'=>dimension_exception_reason_authority($reason));
}

/** Aggregate columns shared by the ledger-based reasons. */
function dimension_exception_measure_sql()
{
    return 'COUNT(*) AS item_lines, SUM(ABS(t.amount)) AS item_absolute, MIN(t.tran_date) AS first_date, MAX(t.tran_date) AS last_date';
}

/**
 * Items of the ledger-based reasons (unassigned, invalid, closed, source_vs_gl) for one axis list.
 *
 * @return array|false Map reason => items
 */
function dimension_exception_ledger_items($from, $to, $scope1, $scope2, $axes)
{
    $range = dimension_tracking_range_sql($from, $to, $scope1, $scope2);
    $pl = ' WHERE c.ctype IN ('.CL_INCOME.','.CL_COGS.','.CL_EXPENSE.')'.$range;
    $limit = ' LIMIT '.(dimension_tracking_row_limit() + 1);
    $items = array('unassigned'=>array(), 'invalid'=>array(), 'closed'=>array(), 'source_vs_gl'=>array());
    foreach ($axes as $axis) {
        $column = $axis === 1 ? 'dimension_id' : 'dimension2_id';
        $unassigned = db_query('SELECT t.type, '.dimension_exception_measure_sql().dimension_tracking_from_sql().$pl.' AND t.'.$column.'=0 GROUP BY t.type ORDER BY first_date, t.type'.$limit, 'The unassigned exceptions could not be read');
        if (!$unassigned) return false;
        while ($row = db_fetch_assoc($unassigned))
            $items['unassigned'][] = dimension_exception_item('unassigned', $axis, sprintf(_('Category %d, source type %d'), $axis, (int)$row['type']), $row['item_lines'], $row['item_absolute'], $row['first_date'], $row['last_date'], $to, (int)$row['type']);
        $invalid = db_query('SELECT t.'.$column.' AS dimension_id, x.id AS other_id, '.dimension_exception_measure_sql().dimension_tracking_from_sql()
            .' LEFT JOIN '.TB_PREF.'dimensions d ON d.id=t.'.$column.' AND d.type_='.$axis.' LEFT JOIN '.TB_PREF.'dimensions x ON x.id=t.'.$column
            .$pl.' AND t.'.$column.'>0 AND d.id IS NULL GROUP BY t.'.$column.', x.id ORDER BY first_date, t.'.$column.$limit, 'The invalid exceptions could not be read');
        if (!$invalid) return false;
        while ($row = db_fetch_assoc($invalid))
            $items['invalid'][] = dimension_exception_item('invalid', $axis, ($row['other_id'] === null ? sprintf(_('Unknown Dimension #%d'), (int)$row['dimension_id'])
                : sprintf(_('Dimension #%d belongs to the other category'), (int)$row['dimension_id'])).sprintf(_(' (category %d)'), $axis), $row['item_lines'], $row['item_absolute'], $row['first_date'], $row['last_date'], $to);
        $closed = db_query('SELECT t.'.$column.' AS dimension_id, d.reference, d.name, '.dimension_exception_measure_sql().dimension_tracking_from_sql()
            .' INNER JOIN '.TB_PREF.'dimensions d ON d.id=t.'.$column.' AND d.type_='.$axis.' AND d.closed=1'.$pl.' GROUP BY t.'.$column.', d.reference, d.name ORDER BY first_date, t.'.$column.$limit, 'The closed exceptions could not be read');
        if (!$closed) return false;
        while ($row = db_fetch_assoc($closed))
            $items['closed'][] = dimension_exception_item('closed', $axis, $row['reference'].' '.$row['name'], $row['item_lines'], $row['item_absolute'], $row['first_date'], $row['last_date'], $to);
    }
    $mismatch = array();
    if (in_array(1, $axes, true)) $mismatch[] = '(hdr.d1 > 0 AND t.dimension_id <> hdr.d1)';
    if (in_array(2, $axes, true)) $mismatch[] = '(hdr.d2 > 0 AND t.dimension2_id <> hdr.d2)';
    if (count($mismatch)) {
        $source = db_query('SELECT t.type, t.type_no, '.dimension_exception_measure_sql().dimension_tracking_from_sql()
            .' INNER JOIN (SELECT dt.type, dt.trans_no, MAX(dt.dimension_id) AS d1, MAX(dt.dimension2_id) AS d2 FROM '.TB_PREF.'debtor_trans dt
                WHERE dt.type IN ('.implode(',', dimension_customer_source_types()).') GROUP BY dt.type, dt.trans_no HAVING COUNT(*)=1) hdr ON hdr.type=t.type AND hdr.trans_no=t.type_no'
            .$pl.' AND ('.implode(' OR ', $mismatch).') GROUP BY t.type, t.type_no ORDER BY first_date, t.type, t.type_no'.$limit, 'The source-vs-GL exceptions could not be read');
        if (!$source) return false;
        while ($row = db_fetch_assoc($source))
            $items['source_vs_gl'][] = dimension_exception_item('source_vs_gl', 0, sprintf(_('Source type %d #%d'), (int)$row['type'], (int)$row['type_no']), $row['item_lines'], $row['item_absolute'], $row['first_date'], $row['last_date'], $to, (int)$row['type']);
    }
    $distinct = dimension_exception_distinct_coverage($from, $to, $scope1, $scope2, $axes);
    if ($distinct === false) return false;
    return array('items'=>$items, 'distinct'=>$distinct);
}

/**
 * Distinct line counts and absolute amounts of the unassigned, invalid and closed reasons.
 *
 * One posting can be an exception on both axes; the queue lists it per axis, but the coverage counts every posting once per reason.
 *
 * @return array|false Map reason => lines, absolute
 */
function dimension_exception_distinct_coverage($from, $to, $scope1, $scope2, $axes)
{
    $unassigned = $invalid = $closed = array();
    if (in_array(1, $axes, true)) {
        $unassigned[] = 't.dimension_id=0';
        $invalid[] = '(t.dimension_id>0 AND d1.id IS NULL)';
        $closed[] = 'd1.closed=1';
    }
    if (in_array(2, $axes, true)) {
        $unassigned[] = 't.dimension2_id=0';
        $invalid[] = '(t.dimension2_id>0 AND d2.id IS NULL)';
        $closed[] = 'd2.closed=1';
    }
    $conditions = array('unassigned'=>'('.implode(' OR ', $unassigned).')', 'invalid'=>'('.implode(' OR ', $invalid).')', 'closed'=>'('.implode(' OR ', $closed).')');
    $select = array();
    foreach ($conditions as $reason => $condition)
        $select[] = 'SUM(CASE WHEN '.$condition.' THEN 1 ELSE 0 END) AS '.$reason.'_lines, SUM(CASE WHEN '.$condition.' THEN ABS(t.amount) ELSE 0 END) AS '.$reason.'_absolute';
    $result = db_query('SELECT '.implode(', ', $select).dimension_tracking_from_sql().' LEFT JOIN '.TB_PREF.'dimensions d1 ON d1.id=t.dimension_id AND d1.type_=1
        LEFT JOIN '.TB_PREF.'dimensions d2 ON d2.id=t.dimension2_id AND d2.type_=2 WHERE c.ctype IN ('.CL_INCOME.','.CL_COGS.','.CL_EXPENSE.')'
        .dimension_tracking_range_sql($from, $to, $scope1, $scope2), 'The exception coverage could not be read');
    $row = $result ? db_fetch_assoc($result) : false;
    if (!$row) return false;
    $out = array();
    foreach (array_keys($conditions) as $reason)
        $out[$reason] = array('lines'=>(int)$row[$reason.'_lines'], 'absolute'=>round((float)$row[$reason.'_absolute'], 6));
    return $out;
}

/**
 * Budget overrun items: Dimensions whose actual costs exceed their comparable monthly cost budget.
 *
 * @return array state ok|not_comparable|no_budget, reason, items
 */
function dimension_exception_overrun_items($from, $to, $axes, $scope1 = 0, $scope2 = 0)
{
    $out = array('state'=>'ok', 'reason'=>'ok', 'items'=>array());
    $period = dimension_budget_period($from, $to);
    if (!$period['comparable']) { $out['state'] = 'not_comparable'; $out['reason'] = $period['reason']; return $out; }
    foreach ($axes as $axis) {
        $column = $axis === 1 ? 'dimension_id' : 'dimension2_id';
        // The scope narrows the Dimensions that are read: an exact Dimension keeps only itself, Unassigned has no master, the other axis scope applies to every reading.
        $own_scope = $axis === 1 ? $scope1 : $scope2;
        $other_scope = $axis === 1 ? $scope2 : $scope1;
        if ($own_scope < 0) continue;
        $keys = array_keys($period['months']);
        $window_from = $period['months'][$keys[0]]['start'];
        $window_to = $period['months'][$keys[count($keys) - 1]]['end'];
        $candidates = db_query('SELECT DISTINCT b.'.$column." AS dimension_id, d.reference, d.name FROM ".TB_PREF.'budget_trans b INNER JOIN '.TB_PREF.'dimensions d ON d.id=b.'.$column.' AND d.type_='.$axis
            ." WHERE b.tran_date >= '".$window_from."' AND b.tran_date <= '".$window_to."' AND b.".$column.'>0'.($own_scope > 0 ? ' AND b.'.$column.'='.(int)$own_scope : '').' ORDER BY b.'.$column.' LIMIT 100', 'The budget candidates could not be read');
        if (!$candidates) { $out['state'] = 'unreadable'; $out['reason'] = 'invalid'; return $out; }
        while ($row = db_fetch_assoc($candidates)) {
            $id = (int)$row['dimension_id'];
            $summary = dimension_tracking_summary($from, $to, $axis === 1 ? $id : $other_scope, $axis === 2 ? $id : $other_scope);
            $budget = dimension_tracking_budget_summary($from, $to, $axis === 1 ? $id : $other_scope, $axis === 2 ? $id : $other_scope);
            if ($summary === false || $budget['state'] !== 'comparable') continue;
            $over = round($summary['costs'] - $budget['cost_budget'], 6);
            if ($over > 0.005) {
                $item = dimension_exception_item('overrun', $axis, $row['reference'].' '.$row['name'], $summary['eligible_lines'], $over, $from, $to, $to);
                $item['age_days'] = dimension_exception_age_days($from, $to);
                $out['items'][] = $item;
            }
        }
    }
    return $out;
}

/**
 * Missing customer cost items from the DIM-REP-005 reader, dated from the customers' own revenue lines.
 *
 * @return array|false items
 */
function dimension_exception_customer_items($from, $to, $scope1, $scope2)
{
    $economics = dimension_customer_economics($from, $to, $scope1, $scope2);
    if ($economics === false) return false;
    $dates = db_query('SELECT src.debtor_no AS customer_no, MIN(t.tran_date) AS first_date, MAX(t.tran_date) AS last_date'.dimension_tracking_from_sql().dimension_customer_source_join_sql()
        .' WHERE c.ctype='.CL_INCOME.dimension_tracking_range_sql($from, $to, dimension_tracking_normalize_scope($scope1), dimension_tracking_normalize_scope($scope2))
        .' AND '.dimension_customer_state_sql()." = 'attributed' GROUP BY src.debtor_no", 'The customer dates could not be read');
    if (!$dates) return false;
    $by_customer = array();
    while ($row = db_fetch_assoc($dates)) $by_customer[(int)$row['customer_no']] = $row;
    $items = array();
    foreach ($economics['customers'] as $customer) {
        if ($customer['contribution_state'] !== 'cost_missing') continue;
        $dates_row = isset($by_customer[$customer['customer_no']]) ? $by_customer[$customer['customer_no']] : array('first_date'=>null, 'last_date'=>null);
        $items[] = dimension_exception_item('missing_customer_cost', 0, dimension_customer_label_text($customer), $customer['lines'], $customer['absolute'], $dates_row['first_date'], $dates_row['last_date'], $to);
    }
    return $items;
}

/** Plain-text customer label that keeps an unknown identity visible. */
function dimension_customer_label_text($customer)
{
    if (empty($customer['known'])) return sprintf(_('Unknown customer #%d'), (int)$customer['customer_no']);
    return $customer['reference'].' '.$customer['name'];
}

/**
 * The exception queue of one scope and period.
 *
 * @param string $from SQL period start
 * @param string $to SQL period end (ages are measured to it)
 * @param int|string $scope1 Axis 1 scope
 * @param int|string $scope2 Axis 2 scope
 * @param array $axes Enabled axes (1 and/or 2)
 * @param array $reasons Reasons to read (default all)
 * @return array|false summary (per reason: state, items, lines, absolute, line_percent, absolute_percent), items, eligible_lines, eligible_absolute, truncated; false when invalid or unreadable
 */
function dimension_exception_queue($from, $to, $scope1 = 0, $scope2 = 0, $axes = array(1, 2), $reasons = array())
{
    if (!dimension_tracking_valid_range($from, $to) || $from === null || $from === '') return false;
    $axes = array_values(array_intersect(array(1, 2), array_map('intval', (array)$axes)));
    if (!count($axes)) return false;
    $reasons = count($reasons) ? array_values(array_intersect(dimension_exception_reasons(), $reasons)) : dimension_exception_reasons();
    $scope1 = dimension_tracking_normalize_scope($scope1);
    $scope2 = dimension_tracking_normalize_scope($scope2);
    $summary = dimension_tracking_summary($from, $to, $scope1, $scope2);
    if ($summary === false) return false;
    $ledger = dimension_exception_ledger_items($from, $to, $scope1, $scope2, $axes);
    if ($ledger === false) return false;
    $by_reason = array();
    $state = array();
    foreach ($ledger['items'] as $reason => $items) { $by_reason[$reason] = $items; $state[$reason] = array('state'=>'ok', 'reason'=>'ok'); }
    $overrun = dimension_exception_overrun_items($from, $to, $axes, $scope1, $scope2);
    $by_reason['overrun'] = $overrun['items'];
    $state['overrun'] = array('state'=>$overrun['state'], 'reason'=>$overrun['reason']);
    $customers = dimension_exception_customer_items($from, $to, $scope1, $scope2);
    if ($customers === false) return false;
    $by_reason['missing_customer_cost'] = $customers;
    $state['missing_customer_cost'] = array('state'=>'ok', 'reason'=>'ok');
    $out = array('summary'=>array(), 'items'=>array(), 'eligible_lines'=>$summary['eligible_lines'], 'eligible_absolute'=>$summary['eligible_absolute'], 'truncated'=>false, 'axes'=>$axes);
    foreach (dimension_exception_reasons() as $reason) {
        if (!in_array($reason, $reasons, true)) continue;
        $items = $by_reason[$reason];
        if (count($items) > dimension_tracking_row_limit()) { $out['truncated'] = true; $items = array_slice($items, 0, dimension_tracking_row_limit()); }
        $lines = 0;
        $absolute = 0.0;
        foreach ($items as $item) { $lines += $item['lines']; $absolute += $item['absolute']; }
        $absolute = round($absolute, 6);
        if (isset($ledger['distinct'][$reason])) { $lines = $ledger['distinct'][$reason]['lines']; $absolute = $ledger['distinct'][$reason]['absolute']; }
        $out['summary'][$reason] = array('state'=>$state[$reason]['state'], 'reason_state'=>$state[$reason]['reason'], 'items'=>count($items), 'lines'=>$lines, 'absolute'=>$absolute,
            'line_percent'=>$summary['eligible_lines'] > 0 ? round(100.0 * $lines / $summary['eligible_lines'], 4) : null,
            'absolute_percent'=>$summary['eligible_absolute'] > 0 ? round(100.0 * $absolute / $summary['eligible_absolute'], 4) : null);
        foreach ($items as $item) $out['items'][] = $item;
    }
    return $out;
}

/**
 * Whether a user can act on an item: names the authority only.
 *
 * @param array $item Queue item
 * @return bool
 */
function dimension_exception_can_act($item)
{
    return $item['authority'] !== '' && isset($_SESSION['wa_current_user']) && $_SESSION['wa_current_user']->can_access($item['authority']);
}

/**
 * Column left edges of an export section, wider for text columns than for numbers (pure).
 *
 * @param array $aligns Column alignments, left or right
 * @param int $width Total width in report units
 * @return array Left edges plus the right edge (count($aligns) + 1 integers)
 */
function dimension_export_column_positions($aligns, $width = 515)
{
    $weights = array();
    foreach ($aligns as $align) $weights[] = $align === 'left' ? 3.0 : 1.5;
    $total = array_sum($weights);
    $positions = array(0);
    $used = 0.0;
    foreach ($weights as $index => $weight) {
        $used += $weight;
        $positions[] = $index === count($weights) - 1 ? (int)$width : (int)round($width * $used / $total);
    }
    return $positions;
}

/** One export section. */
function dimension_export_section($key, $title, $headers, $aligns, $rows, $notes = array())
{
    return array('key'=>$key, 'title'=>$title, 'headers'=>$headers, 'aligns'=>$aligns, 'rows'=>$rows, 'notes'=>$notes);
}

/** Display text of a percentage cell. */
function dimension_export_percent($percent)
{
    return $percent === null ? _('N/A') : number_format($percent, 2, '.', '').'%';
}

/**
 * The shared dataset behind the page, the PDF and the Excel export.
 *
 * Cells are plain strings, or floats for amounts. Every consumer renders exactly these cells, so the three outputs cannot disagree.
 *
 * @param array $params from, to (SQL), scope1, scope2, mode, fiscal_start, matrix (bool), queue (bool), axes, reasons
 * @return array|false sections (key, title, headers, aligns, rows, notes), meta; false when the parameters are invalid or a read fails
 */
function dimension_export_dataset($params)
{
    $from = isset($params['from']) ? $params['from'] : '';
    $to = isset($params['to']) ? $params['to'] : '';
    $scope1 = isset($params['scope1']) ? $params['scope1'] : 0;
    $scope2 = isset($params['scope2']) ? $params['scope2'] : 0;
    $mode = isset($params['mode']) ? $params['mode'] : 'previous';
    $axes = isset($params['axes']) ? $params['axes'] : array(1, 2);
    if (!dimension_tracking_valid_range($from, $to) || $from === '') return false;
    $sections = array();
    $comparison = dimension_comparison_run($mode, $from, $to, $scope1, $scope2, isset($params['fiscal_start']) ? $params['fiscal_start'] : '');
    if ($comparison === false) return false;
    $labels = array('revenue'=>_('Revenue (credit-positive)'), 'costs'=>_('Costs (debit-positive)'), 'result'=>_('Result (revenue - costs)'));
    $rows = array();
    foreach ($comparison['rows'] as $row) {
        $rows[] = array($labels[$row['measure']], (float)$row['current'], $row['base'] === null ? _('N/A') : (float)$row['base'],
            $row['amount'] === null ? _('N/A') : (float)$row['amount'], dimension_export_percent($row['percent']));
    }
    $titles = array('previous'=>_('Comparison with the previous period'), 'ytd'=>_('Year to date against the same dates one year earlier'), 'budget'=>_('Comparison with the monthly budget'));
    $notes = array(sprintf(_('Current: %s to %s.'), $comparison['current']['from'], $comparison['current']['to']));
    if ($comparison['base']['values'] !== null && $mode !== 'budget')
        $notes[] = sprintf(_('Comparison: %s to %s.'), $comparison['base']['from'], $comparison['base']['to']);
    if ($mode === 'budget') {
        $notes[] = $comparison['base']['values'] === null ? sprintf(_('Budget is not comparable: %s.'), dimension_budget_reason_text($comparison['base']['reason']))
            : ($comparison['base']['state'] === 'incomplete' ? _('Some accounts have no complete monthly budget and are left out of the budget figures.') : _('Budget through the comparable monthly basis.'));
    }
    $sections[] = dimension_export_section('comparison', $titles[$mode], array(_('Measure'), _('Current'), _('Comparison'), _('Variance'), _('Variance %')),
        array('left', 'right', 'right', 'right', 'right'), $rows, $notes);
    if (!empty($params['matrix'])) {
        foreach ($axes as $axis) {
            $matrix = dimension_comparison_account_matrix($axis, $from, $to, (int)$axis === 1 ? $scope2 : $scope1);
            if ($matrix === false) return false;
            $headers = array(_('Account'));
            $aligns = array('left');
            foreach ($matrix['columns'] as $col) { $headers[] = $col['label']; $aligns[] = 'right'; }
            $headers[] = _('Total');
            $aligns[] = 'right';
            $mrows = array();
            foreach ($matrix['rows'] as $row) {
                $cells = array($row['account'].' '.$row['account_name']);
                foreach ($matrix['columns'] as $col) $cells[] = (float)$row['cells'][$col['key']];
                $cells[] = (float)$row['total'];
                $mrows[] = $cells;
            }
            $total_cells = array(_('Total result'));
            foreach ($matrix['columns'] as $col) $total_cells[] = (float)$matrix['totals'][$col['key']];
            $total_cells[] = (float)$matrix['totals']['total'];
            $mrows[] = $total_cells;
            $mnotes = array(_('Each cell is the result contribution of the account in the Dimension (revenue credit-positive, costs debit-positive); the row and column totals add up to the scope result.'));
            if ($matrix['truncated']) $mnotes[] = sprintf(_('Only the first %d accounts are listed.'), dimension_tracking_row_limit());
            $sections[] = dimension_export_section('matrix'.(int)$axis, sprintf(_('Category %d - account matrix'), (int)$axis), $headers, $aligns, $mrows, $mnotes);
        }
    }
    $queue_axes = isset($params['queue_axes']) ? $params['queue_axes'] : $axes;
    if (!empty($params['queue']) && !count($queue_axes)) {
        $sections[] = dimension_export_section('queue_summary', _('Exception queue - counts and coverage'), array(_('Reason'), _('Items')), array('left', 'right'), array(),
            array(_('No Dimension category is enabled for new postings, so no posting is an exception. Historical Dimension reports stay available.')));
    }
    elseif (!empty($params['queue'])) {
        $queue = dimension_exception_queue($from, $to, $scope1, $scope2, $queue_axes, isset($params['reasons']) ? $params['reasons'] : array());
        if ($queue === false) return false;
        $srows = array();
        foreach ($queue['summary'] as $reason => $s) {
            $srows[] = array(dimension_exception_reason_text($reason), $s['state'] === 'ok' ? (string)$s['items'] : _('N/A'), (string)$s['lines'], (float)$s['absolute'],
                dimension_export_percent($s['line_percent']), dimension_export_percent($s['absolute_percent']), dimension_exception_reason_authority($reason));
        }
        $sections[] = dimension_export_section('queue_summary', _('Exception queue - counts and coverage'),
            array(_('Reason'), _('Items'), _('Lines'), _('Absolute amount'), _('Line coverage'), _('Absolute coverage'), _('Action authority')),
            array('left', 'right', 'right', 'right', 'right', 'right', 'left'), $srows,
            array(sprintf(_('Eligible P&L lines in scope: %d, absolute amount %s. A line can have more than one reason, so reasons are not added together.'), $queue['eligible_lines'], number_format($queue['eligible_absolute'], 2, '.', ''))));
        $irows = array();
        foreach ($queue['items'] as $item) {
            $irows[] = array(dimension_exception_reason_text($item['reason']), $item['subject'], $item['source'] === null ? '' : (string)$item['source'], $item['first_date'] === null ? '' : $item['first_date'],
                $item['age_days'] === null ? _('N/A') : (string)$item['age_days'], (string)$item['lines'], (float)$item['absolute'], $item['action']);
        }
        $inotes = array();
        foreach ($queue['summary'] as $reason => $s)
            if ($s['state'] !== 'ok') $inotes[] = dimension_exception_reason_text($reason).' - '.sprintf(_('not available: %s.'), dimension_budget_reason_text($s['reason_state']));
        if ($queue['truncated']) $inotes[] = sprintf(_('Only the first %d items of a reason are listed.'), dimension_tracking_row_limit());
        $sections[] = dimension_export_section('queue_items', _('Exception queue - items'),
            array(_('Reason'), _('Subject'), _('Source type'), _('First posting'), _('Age (days)'), _('Lines'), _('Absolute amount'), _('Next step')),
            array('left', 'left', 'right', 'left', 'right', 'right', 'right', 'left'), $irows, $inotes);
    }
    return array('sections'=>$sections, 'meta'=>array('from'=>$from, 'to'=>$to, 'scope1'=>dimension_tracking_normalize_scope($scope1), 'scope2'=>dimension_tracking_normalize_scope($scope2), 'mode'=>$mode));
}
