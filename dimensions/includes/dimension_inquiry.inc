<?php
/**
 * DIM-REP-004: master inquiry filters, bounded paging and grouped performance of Dimension masters.
 *
 * One read-only authority behind the Dimension search page (`dimensions/inquiry/search_dimensions.php`), the Dimension
 * summary report 501 and the master entry validation. It adds no table and no writer; it reads `dimensions` and, for the
 * performance columns, `gl_trans`/`chart_*` of the active company only.
 *
 * - **Two different dates, never mixed:** the master **start date** (`dimensions.date_`) and the **activity period**
 *   (postings dated inside the period). A master that started long ago but posted this month appears when the activity
 *   period is chosen; the start-date filter narrows only by when the master was created.
 * - **Reference is a filter, not a bypass:** the reference/name text is combined (AND) with every other selected filter.
 *   It is a literal substring match, so `%` and `_` are ordinary characters, never wildcards.
 * - **Own axis only:** a master of type 1 is measured on `gl_trans.dimension_id`, a master of type 2 on
 *   `gl_trans.dimension2_id` - exactly the grouping of the Financial Tracking workspace (DIM-REP-003), so the list,
 *   the workspace groups and the P&L account reader reconcile for the same period. Revenue is credit-positive,
 *   costs debit-positive, result = revenue - costs; the net balance of balance-sheet accounts stays a separate figure.
 * - **Bounded cost:** a search is always three statements (count, one page of masters, one grouped aggregate restricted to
 *   the page's identities), whatever the number of masters. The aggregate reads the existing `dimension_id` /
 *   `dimension2_id` / `tran_date` indexes; no index or table is added.
 * - **Entry validation:** server and client share one length (the canonical `varchar(60)` columns, counted in characters),
 *   and a required-by date before the start date is refused.
 *
 * Dates are SQL `Y-m-d`; an empty bound is open. PHP 5.6 compatible syntax.
 */
require_once __DIR__.'/dimension_tracking_workspace.inc';

/** Canonical length of `dimensions.name` and `dimensions.reference`, in characters. */
function dimension_inquiry_name_limit()
{
    return 60;
}

/** Page size bounds: the user preference is clamped into this range. */
function dimension_inquiry_page_size_bounds()
{
    return array('min'=>5, 'max'=>200, 'default'=>25);
}

/**
 * Length of a text in characters (not bytes), so a multibyte name is judged like the column that stores it.
 *
 * @param mixed $text Text
 * @return int
 */
function dimension_inquiry_text_length($text)
{
    $text = (string)$text;
    if (function_exists('mb_strlen')) return (int)mb_strlen($text, 'UTF-8');
    $count = preg_match_all('/./su', $text, $unused);
    return $count === false ? strlen($text) : (int)$count;
}

/**
 * Validate the user-entered fields of a master (pure).
 *
 * @param string $reference Reference ('' when the reference is not being validated, e.g. an update)
 * @param string $name Name
 * @param string|null $start_sql Start date, SQL form, null when not a valid date
 * @param string|null $due_sql Required-by date, SQL form, null when not a valid date
 * @return array Reason codes, empty when valid: reference_too_long, name_required, name_too_long, start_invalid, due_invalid, due_before_start
 */
function dimension_inquiry_validate_master($reference, $name, $start_sql, $due_sql)
{
    $codes = array();
    if (dimension_inquiry_text_length($reference) > dimension_inquiry_name_limit()) $codes[] = 'reference_too_long';
    if (trim((string)$name) === '') $codes[] = 'name_required';
    elseif (dimension_inquiry_text_length($name) > dimension_inquiry_name_limit()) $codes[] = 'name_too_long';
    $start_ok = is_string($start_sql) && dimension_budget_valid_date($start_sql);
    $due_ok = is_string($due_sql) && dimension_budget_valid_date($due_sql);
    if (!$start_ok) $codes[] = 'start_invalid';
    if (!$due_ok) $codes[] = 'due_invalid';
    if ($start_ok && $due_ok && $due_sql < $start_sql) $codes[] = 'due_before_start';
    return $codes;
}

/**
 * Reviewed message of one validation or filter reason code.
 *
 * @param string $code Reason code
 * @return string
 */
function dimension_inquiry_reason_text($code)
{
    $texts = array(
        'reference_too_long'=>sprintf(_('The reference can have at most %d characters.'), dimension_inquiry_name_limit()),
        'name_required'=>_('The dimension name must be entered.'),
        'name_too_long'=>sprintf(_('The dimension name can have at most %d characters.'), dimension_inquiry_name_limit()),
        'start_invalid'=>_('The date entered is in an invalid format.'),
        'due_invalid'=>_('The required by date entered is in an invalid format.'),
        'due_before_start'=>_('The required by date cannot be before the start date.'),
        'type_invalid'=>_('The selected category is not valid.'),
        'start_range'=>_('The start date range is reversed: the first date is after the last.'),
        'activity_range'=>_('The activity period is reversed: the first date is after the last.'),
        'start_date_invalid'=>_('A start date filter is not a valid date.'),
        'activity_date_invalid'=>_('An activity date filter is not a valid date.'),
        'reference_filter_too_long'=>sprintf(_('The reference or name filter can have at most %d characters.'), dimension_inquiry_name_limit()));
    return isset($texts[$code]) ? $texts[$code] : $code;
}

/**
 * Wording of the closed state: a closed master takes no new posting; nothing is lost and it can be re-opened.
 *
 * @return string
 */
function dimension_inquiry_closed_text()
{
    return _('Closed: no new posting accepts this dimension. Its history and reports are unchanged and it can be re-opened.');
}

/**
 * Normalize the search filters (pure).
 *
 * @param array $raw reference, type (0 all, 1, 2), status (all|open|closed), overdue, start_from, start_to, active_from, active_to
 *                   (SQL dates or empty), sort, page, page_size, today (SQL date)
 * @return array ok, errors (reason codes), filters
 */
function dimension_inquiry_normalize_filters($raw)
{
    $errors = array();
    $reference = isset($raw['reference']) && is_string($raw['reference']) ? trim($raw['reference']) : '';
    if (dimension_inquiry_text_length($reference) > dimension_inquiry_name_limit()) { $errors[] = 'reference_filter_too_long'; $reference = ''; }
    $type = isset($raw['type']) ? (string)$raw['type'] : '0';
    if ($type === '' || $type === '-1') $type = '0';
    if (!in_array($type, array('0', '1', '2'), true)) { $errors[] = 'type_invalid'; $type = '0'; }
    $status = isset($raw['status']) && in_array($raw['status'], array('all', 'open', 'closed'), true) ? $raw['status'] : 'all';
    $dates = array();
    foreach (array('start_from'=>'start_date_invalid', 'start_to'=>'start_date_invalid', 'active_from'=>'activity_date_invalid', 'active_to'=>'activity_date_invalid') as $key=>$code) {
        $value = isset($raw[$key]) && is_string($raw[$key]) ? $raw[$key] : '';
        if ($value === '') { $dates[$key] = ''; continue; }
        if (!dimension_budget_valid_date($value)) { $errors[] = $code; $dates[$key] = ''; continue; }
        $dates[$key] = $value;
    }
    if ($dates['start_from'] !== '' && $dates['start_to'] !== '' && $dates['start_from'] > $dates['start_to']) $errors[] = 'start_range';
    if ($dates['active_from'] !== '' && $dates['active_to'] !== '' && $dates['active_from'] > $dates['active_to']) $errors[] = 'activity_range';
    $today = isset($raw['today']) && is_string($raw['today']) && dimension_budget_valid_date($raw['today']) ? $raw['today'] : '';
    $sort = isset($raw['sort']) && in_array($raw['sort'], array('reference', 'name', 'date_', 'due_date', 'id'), true) ? $raw['sort'] : 'reference';
    $bounds = dimension_inquiry_page_size_bounds();
    $size = isset($raw['page_size']) && preg_match('/^[0-9]{1,9}$/D', (string)$raw['page_size']) === 1 ? (int)$raw['page_size'] : $bounds['default'];
    $size = max($bounds['min'], min($bounds['max'], $size));
    $page = isset($raw['page']) && preg_match('/^[0-9]{1,9}$/D', (string)$raw['page']) === 1 ? max(1, (int)$raw['page']) : 1;
    return array('ok'=>count($errors) === 0, 'errors'=>array_values(array_unique($errors)), 'filters'=>array(
        'reference'=>$reference, 'type'=>(int)$type, 'status'=>$status, 'overdue'=>!empty($raw['overdue']), 'start_from'=>$dates['start_from'],
        'start_to'=>$dates['start_to'], 'active_from'=>$dates['active_from'], 'active_to'=>$dates['active_to'], 'today'=>$today,
        'sort'=>$sort, 'page'=>$page, 'page_size'=>$size));
}

/** Whether the filters select by activity (an activity period was chosen). */
function dimension_inquiry_has_activity_filter($filters)
{
    return $filters['active_from'] !== '' || $filters['active_to'] !== '';
}

/**
 * WHERE predicate of the master list on the `dim` alias (pure given db_escape).
 *
 * Every selected filter is combined with AND, the reference included. A master matches an activity period when at least one
 * posting of its own axis is dated inside it.
 *
 * @param array $filters Normalized filters
 * @return string Predicate, never empty
 */
function dimension_inquiry_where_sql($filters)
{
    $where = 'dim.id > 0';
    if ($filters['reference'] !== '') {
        $needle = db_escape($filters['reference']);
        $where .= ' AND (INSTR(dim.reference, '.$needle.') > 0 OR INSTR(dim.name, '.$needle.') > 0)';
    }
    if ((int)$filters['type'] > 0) $where .= ' AND dim.type_='.(int)$filters['type'];
    if ($filters['status'] === 'open') $where .= ' AND dim.closed=0';
    elseif ($filters['status'] === 'closed') $where .= ' AND dim.closed=1';
    // An overdue master is one still open past its required-by date; a closed master is never overdue.
    // DIM-UX-001: an optional `overdue_axes` list restricts "overdue" to the categories whose required-by date is a real deadline
    // (an ongoing cost center is never overdue); without the list every category keeps the legacy behavior.
    if (!empty($filters['overdue']) && $filters['today'] !== '') {
        $where .= " AND dim.closed=0 AND dim.due_date < '".$filters['today']."'";
        if (isset($filters['overdue_axes']) && is_array($filters['overdue_axes'])) {
            $axes = array_values(array_unique(array_filter(array_map('intval', $filters['overdue_axes']), function ($axis) { return $axis === 1 || $axis === 2; })));
            $where .= count($axes) ? ' AND dim.type_ IN ('.implode(',', $axes).')' : ' AND 1=0';
        }
    }
    if ($filters['start_from'] !== '') $where .= " AND dim.date_ >= '".$filters['start_from']."'";
    if ($filters['start_to'] !== '') $where .= " AND dim.date_ <= '".$filters['start_to']."'";
    if (dimension_inquiry_has_activity_filter($filters)) {
        $range = '';
        if ($filters['active_from'] !== '') $range .= " AND t.tran_date >= '".$filters['active_from']."'";
        if ($filters['active_to'] !== '') $range .= " AND t.tran_date <= '".$filters['active_to']."'";
        $where .= ' AND ((dim.type_=1 AND EXISTS (SELECT 1 FROM '.TB_PREF.'gl_trans t WHERE t.dimension_id=dim.id'.$range.'))
            OR (dim.type_=2 AND EXISTS (SELECT 1 FROM '.TB_PREF.'gl_trans t WHERE t.dimension2_id=dim.id'.$range.')))';
    }
    return $where;
}

/** ORDER BY of the master list: one master column plus the identity as a stable tiebreak. */
function dimension_inquiry_order_sql($sort)
{
    $columns = array('reference'=>'dim.reference', 'name'=>'dim.name', 'date_'=>'dim.date_', 'due_date'=>'dim.due_date', 'id'=>'dim.id');
    $column = isset($columns[$sort]) ? $columns[$sort] : 'dim.reference';
    return $column === 'dim.id' ? 'dim.id' : $column.', dim.id';
}

/**
 * SQL of the grouped own-axis measures of a set of masters (one UNION ALL statement, one grouped read per axis).
 *
 * @param array $axis_ids Map axis (1|2) => list of positive Dimension IDs
 * @param string $from SQL lower bound or ''
 * @param string $to SQL upper bound or ''
 * @return string SQL, or '' when there is no identity to measure
 */
function dimension_inquiry_measures_sql($axis_ids, $from, $to)
{
    $parts = array();
    foreach (array(1=>'dimension_id', 2=>'dimension2_id') as $axis=>$column) {
        $ids = isset($axis_ids[$axis]) ? array_values(array_unique(array_map('intval', $axis_ids[$axis]))) : array();
        $ids = array_values(array_filter($ids, function ($id) { return $id > 0; }));
        if (count($ids) === 0) continue;
        $range = ($from !== '' ? " AND t.tran_date >= '".$from."'" : '').($to !== '' ? " AND t.tran_date <= '".$to."'" : '');
        $parts[] = 'SELECT '.$axis.' AS axis, t.'.$column.' AS dimension_id,
            SUM(CASE WHEN c.ctype='.CL_INCOME.' THEN -t.amount ELSE 0 END) AS revenue,
            SUM(CASE WHEN c.ctype IN ('.CL_COGS.','.CL_EXPENSE.') THEN t.amount ELSE 0 END) AS costs,
            SUM(CASE WHEN c.ctype IN ('.CL_INCOME.','.CL_COGS.','.CL_EXPENSE.') THEN 1 ELSE 0 END) AS pl_lines,
            SUM(CASE WHEN c.ctype IN ('.CL_ASSETS.','.CL_LIABILITIES.','.CL_EQUITY.') THEN t.amount ELSE 0 END) AS balance_net,
            COUNT(*) AS posting_lines'
            .dimension_tracking_from_sql().' WHERE t.'.$column.' IN ('.implode(',', $ids).')'.$range.' GROUP BY t.'.$column;
    }
    return implode(' UNION ALL ', $parts);
}

/**
 * Period (or lifetime) measures of a set of masters on their own axis, in ONE grouped statement.
 *
 * @param array $axis_ids Map axis (1|2) => list of positive Dimension IDs
 * @param string $from SQL lower bound or ''
 * @param string $to SQL upper bound or ''
 * @return array|false Map "axis:id" => revenue, costs, result, pl_lines, balance_net, posting_lines; false when unreadable
 */
function dimension_inquiry_measures($axis_ids, $from, $to)
{
    $sql = dimension_inquiry_measures_sql($axis_ids, $from, $to);
    if ($sql === '') return array();
    $result = db_query($sql, 'The dimension performance could not be calculated');
    if (!$result) return false;
    $measures = array();
    while ($row = db_fetch_assoc($result)) {
        $revenue = round((float)$row['revenue'], 6);
        $costs = round((float)$row['costs'], 6);
        $measures[(int)$row['axis'].':'.(int)$row['dimension_id']] = array('revenue'=>$revenue, 'costs'=>$costs, 'result'=>round($revenue - $costs, 6),
            'pl_lines'=>(int)$row['pl_lines'], 'balance_net'=>round((float)$row['balance_net'], 6), 'posting_lines'=>(int)$row['posting_lines']);
    }
    return $measures;
}

/**
 * Search the Dimension masters: filtered, ordered, one page, with the page's grouped performance.
 *
 * Always three statements (count, page, grouped measures) whatever the number of masters. The performance period is the
 * activity period when one was chosen and the stored lifetime otherwise. The page number is clamped into the real range.
 *
 * @param array $filters Normalized filters (see dimension_inquiry_normalize_filters)
 * @param bool $with_measures Whether to read the performance columns (needs the tracking aggregate authority)
 * @return array|false total, page, pages, page_size, basis (period|lifetime), rows (master columns plus `measures` or null); false when unreadable
 */
function dimension_inquiry_search($filters, $with_measures = true)
{
    $where = dimension_inquiry_where_sql($filters);
    $count = db_query('SELECT COUNT(*) FROM '.TB_PREF.'dimensions dim WHERE '.$where, 'The dimensions could not be counted');
    if (!$count) return false;
    $row = db_fetch_row($count);
    $total = (int)$row[0];
    $size = (int)$filters['page_size'];
    $pages = max(1, (int)ceil($total / $size));
    $page = max(1, min((int)$filters['page'], $pages));
    $result = db_query('SELECT dim.id, dim.reference, dim.name, dim.type_, dim.date_, dim.due_date, dim.closed FROM '.TB_PREF.'dimensions dim WHERE '.$where
        .' ORDER BY '.dimension_inquiry_order_sql($filters['sort']).' LIMIT '.(($page - 1) * $size).', '.$size, 'The dimensions could not be retrieved');
    if (!$result) return false;
    $rows = array();
    $axis_ids = array(1=>array(), 2=>array());
    while ($master = db_fetch_assoc($result)) {
        $master['id'] = (int)$master['id'];
        $master['type_'] = (int)$master['type_'];
        $master['closed'] = (int)$master['closed'];
        $master['measures'] = null;
        if (isset($axis_ids[$master['type_']])) $axis_ids[$master['type_']][] = $master['id'];
        $rows[] = $master;
    }
    $basis = dimension_inquiry_has_activity_filter($filters) ? 'period' : 'lifetime';
    if ($with_measures && count($rows) > 0) {
        $measures = dimension_inquiry_measures($axis_ids, $filters['active_from'], $filters['active_to']);
        if ($measures === false) return false;
        $empty = array('revenue'=>0.0, 'costs'=>0.0, 'result'=>0.0, 'pl_lines'=>0, 'balance_net'=>0.0, 'posting_lines'=>0);
        foreach ($rows as $index=>$master) {
            $key = $master['type_'].':'.$master['id'];
            $rows[$index]['measures'] = isset($measures[$key]) ? $measures[$key] : $empty;
        }
    }
    return array('total'=>$total, 'page'=>$page, 'pages'=>$pages, 'page_size'=>$size, 'basis'=>$basis, 'rows'=>$rows);
}

/**
 * Net GL balance of every master in an identity range in ONE grouped statement (Dimension summary report 501).
 *
 * The as-posted balance of the legacy report: every account class, a posting counted once for a master even when the
 * same identity is stored on both axes. It replaces one statement per master with one statement per report.
 *
 * @param int $from_id First Dimension identity
 * @param int $to_id Last Dimension identity
 * @param string $from SQL lower bound
 * @param string $to SQL upper bound
 * @return array|false Map identity => net balance (identities without postings are absent); false when unreadable
 */
function dimension_inquiry_net_balances($from_id, $to_id, $from, $to)
{
    if (!is_int($from_id) && !(is_string($from_id) && preg_match('/^[0-9]{1,10}$/D', $from_id) === 1)) return false;
    if (!is_int($to_id) && !(is_string($to_id) && preg_match('/^[0-9]{1,10}$/D', $to_id) === 1)) return false;
    if (!dimension_budget_valid_date($from) || !dimension_budget_valid_date($to) || $from > $to) return false;
    $from_id = (int)$from_id;
    $to_id = (int)$to_id;
    $range = " AND t.tran_date >= '".$from."' AND t.tran_date <= '".$to."'";
    $result = db_query('SELECT x.dimension_id, SUM(x.amount) AS balance FROM (
        SELECT t.dimension_id AS dimension_id, t.amount FROM '.TB_PREF.'gl_trans t WHERE t.dimension_id >= '.$from_id.' AND t.dimension_id <= '.$to_id.$range.'
        UNION ALL
        SELECT t.dimension2_id, t.amount FROM '.TB_PREF.'gl_trans t WHERE t.dimension2_id >= '.$from_id.' AND t.dimension2_id <= '.$to_id
        .' AND t.dimension2_id <> t.dimension_id'.$range.') x GROUP BY x.dimension_id', 'The net balances could not be calculated');
    if (!$result) return false;
    $balances = array();
    while ($row = db_fetch_assoc($result)) $balances[(int)$row['dimension_id']] = (float)$row['balance'];
    return $balances;
}
