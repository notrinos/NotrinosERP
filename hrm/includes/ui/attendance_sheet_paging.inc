<?php
/** Attendance matrix paging and single-use server-held presentation contexts. */

/**
 * Undo exactly the HTML-special encoding applied by session.inc::html_cleanup().
 * Membership compares the exact DOM-submitted code with server-held DB codes.
 * Non-strings remain invalid rather than being coerced into employee identifiers.
 * @param mixed $value
 * @return mixed
 */
function attendance_sheet_post_employee_id($value) {
    return is_string($value) ? htmlspecialchars_decode($value, ENT_QUOTES) : $value;
}

/**
 * Count the eligible roster, clamp the requested page, then read at most 30 rows.
 * Uses the same count/LIMIT convention as db_pager; its generic column renderer
 * cannot render the attendance matrix or own its cell-edit context.
 * @param int $department_id
 * @param string $employee_id
 * @param mixed $requested_page
 * @return array
 */
function attendance_sheet_employee_page($department_id, $employee_id, $requested_page) {
    $ids_sql = get_attendance_sheet_employee_projection_sql($department_id, $employee_id, true);
    $result = db_query('SELECT COUNT(*) AS total FROM ('.$ids_sql.') attendance_roster_count',
        'could not count attendance sheet employees');
    $count = $result ? db_fetch($result) : false;
    if (!$count || !isset($count['total']))
        throw new RuntimeException('Attendance roster count unavailable.');
    $total = (int)$count['total'];
    $pages = max(1, (int)ceil($total / 30));
    $page = is_scalar($requested_page) && preg_match('/^[0-9]+$/D', (string)$requested_page)
        ? max(1, min($pages, (float)$requested_page)) : 1;
    $page = (int)$page;
    $sql = get_attendance_sheet_employee_projection_sql($department_id, $employee_id);
    $result = db_query($sql.' LIMIT '.(($page - 1) * 30).', 30', 'could not page attendance sheet employees');
    if (!$result)
        throw new RuntimeException('Attendance roster unavailable.');
    $employees = array();
    while ($row = db_fetch($result))
        $employees[] = $row;
    // A concurrent cohort shrink can empty the counted last page. Reconcile once.
    if ($total > 0 && empty($employees) && $page > 1)
        return attendance_sheet_employee_page($department_id, $employee_id, 1);
    return array('employees' => $employees, 'total' => $total, 'page' => $page, 'pages' => $pages);
}

/**
 * Render the shared db-pager appearance above or below the matrix.
 * Distinct control names keep both copies usable through the Ajax serializer.
 * @param array $page Current server-derived employee page.
 * @param string $position Top or bottom placement.
 * @return void
 */
function attendance_sheet_render_pagination($page, $position = 'top') {
    $bottom = $position === 'bottom';
    $prefix = $bottom ? 'sheet_bottom_page' : 'sheet_page';
    $first = empty($page['employees']) ? 0 : (($page['page'] - 1) * 30 + 1);
    $last = empty($page['employees']) ? 0 : $first + count($page['employees']) - 1;
    $label = $bottom ? _('Attendance employee pages, bottom') : _('Attendance employee pages, top');
    echo '<div class="att-pagination att-pagination-'.($bottom ? 'bottom' : 'top').'">';
    echo '<nav class="pager-nav" aria-label="'.$label.'"><div class="pager-nav-controls">';
    foreach (array('first'=>_('<<'), 'prev'=>_('<'), 'next'=>_('>'), 'last'=>_('>>')) as $action => $text) {
        if ($action === 'next') {
            echo '<span class="pager-select-wrap">';
            echo number_list($prefix.'_number', $page['page'], 1, $page['pages'], false, true, 'nosearch');
            echo '</span>';
        }
        $enabled = in_array($action, array('first', 'prev')) ? $page['page'] > 1 : $page['page'] < $page['pages'];
        echo navi_button($prefix.'_'.$action, $text, $enabled).' ';
    }
    echo '</div><div class="pager-summary">'.htmlspecialchars(sprintf(_('Records %d-%d of %d'),
        $first, $last, $page['total']), ENT_QUOTES).'</div></nav></div>';
}

/**
 * Bind presentation context to the actual actor, active connection and role.
 * Session/CSRF/page security and fresh command guards remain independently required.
 * @return string
 */
function attendance_sheet_context_principal() {
    $user = $_SESSION['wa_current_user'];
    return hash('sha256', serialize(array($user->user, $user->company, $user->cur_con, $user->role_set)));
}

/**
 * Resolve an unexpired server-held context; posted page/IDs never create authority.
 * @param mixed $token
 * @return array|false
 */
function attendance_sheet_context($token) {
    if (!is_string($token) || !isset($_SESSION['attendance_sheet_pages'][$token]))
        return false;
    $context = $_SESSION['attendance_sheet_pages'][$token];
    if ($context['expires'] < time() || $context['principal'] !== attendance_sheet_context_principal())
        return false;
    return $context;
}

/**
 * Retain a bounded number of contexts so independent browser tabs keep working.
 * @param array $filters
 * @param array $page
 * @return string
 */
function attendance_sheet_remember_page($filters, $page) {
    if (!isset($_SESSION['attendance_sheet_pages']))
        $_SESSION['attendance_sheet_pages'] = array();
    foreach ($_SESSION['attendance_sheet_pages'] as $token => $context)
        if ($context['expires'] < time()) unset($_SESSION['attendance_sheet_pages'][$token]);
    while (count($_SESSION['attendance_sheet_pages']) >= 8)
        array_shift($_SESSION['attendance_sheet_pages']);
    $token = random_id();
    $_SESSION['attendance_sheet_pages'][$token] = array('filters' => $filters,
        'page' => $page['page'], 'ids' => array_column($page['employees'], 'employee_id'),
        'expires' => time() + 1800, 'principal' => attendance_sheet_context_principal());
    return $token;
}

/**
 * Reject the entire command if any selection, filter or fresh page membership drifts.
 * Exact string comparisons distinguish numeric-looking employee codes.
 * @param array|false $context
 * @param array $filters
 * @param array $fresh_page
 * @param mixed $selected
 * @return array|false
 */
function attendance_sheet_selected_ids($context, $filters, $fresh_page, $selected) {
    if (!$context || $context['filters'] !== $filters || $context['page'] !== $fresh_page['page']
        || $context['ids'] !== array_column($fresh_page['employees'], 'employee_id')
        || !is_array($selected) || empty($selected) || count($selected) > 30)
        return false;
    $accepted = array();
    foreach ($selected as $id) {
        if (!is_string($id) || !in_array($id, $context['ids'], true))
            return false;
        if (!in_array($id, $accepted, true)) $accepted[] = $id;
    }
    sort($accepted, SORT_STRING);
    return $accepted;
}

/**
 * Build month metadata once for headers, employee rows and bulk-fill exclusions.
 * @param int $year
 * @param int $month
 * @return array
 */
function attendance_sheet_calendar($year, $month) {
    $calendar = array();
    $days = (int)date('t', mktime(0, 0, 0, $month, 1, $year));
    $today = date('Y-m-d');
    for ($day = 1; $day <= $days; $day++) {
        $ts = mktime(0, 0, 0, $month, $day, $year);
        $date = date('Y-m-d', $ts);
        $calendar[$day] = array('date' => $date, 'dow' => (int)date('w', $ts),
            'abbr' => substr(date('D', $ts), 0, 2), 'title' => date('l, M j', $ts),
            'future' => $date > $today, 'today' => $date === $today);
    }
    return $calendar;
}
