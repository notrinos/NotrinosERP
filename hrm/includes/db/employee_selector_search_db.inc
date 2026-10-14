<?php
/** Bounded employee presentation search; never authority for a business command. */
include_once($path_to_root.'/hrm/includes/db/employee_person_worker_db.inc');

/**
 * Verify the actual connection and configured company before reading a cohort.
 * @return bool
 */
function hrm_employee_selector_search_scope_is_current()
{
    global $db, $db_connections;
    $company = hrm_person_worker_current_company_id();
    if ($company === false || !isset($db, $db_connections[$company]['dbname'], $db_connections[$company]['tbpref']))
        return false;
    $result = db_query('SELECT DATABASE() database_name', 'could not verify employee search scope');
    $scope = $result ? db_fetch_assoc($result) : false;
    return $scope && $scope['database_name'] === $db_connections[$company]['dbname'];
}

/**
 * Build a correlated canonical display expression with the existing evidence SQL.
 * Numeric-looking codes follow PHP is_numeric/int semantics, not employee_id.
 * No canonical expression is built for legacy-only or identity-denied controls.
 * @param string $as_of Normalized instant.
 * @param array $text_rules Validated actual mapping/name column character rules.
 * @return string
 */
function hrm_employee_selector_canonical_name_sql($as_of, $text_rules)
{
    // Match the running PHP reader, including pre-7 hexadecimal/scientific
    // strings and the later trailing-whitespace rule. PHP 5.6 casts '1e2' to
    // 1; modern PHP casts it to 100. Decimal SQL conversion avoids float
    // overflow for stored injection-shaped or enormous exponent codes.
    $numeric = '^[[:space:]]*([+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?'
        .(is_numeric('0x1') ? '|0[xX][0-9a-fA-F]+' : '').')'
        .(is_numeric('1 ') ? '[[:space:]]*$' : '$');
    $number = ((int)'1e2') === 100 ? 'CAST(e.employee_id AS DECIMAL(65,20))'
        : 'CAST(e.employee_id AS SIGNED)';
    $names = array();
    // Put the reference-kind branch outside the subquery so each lookup can
    // use the existing map number/code indexes rather than a CASE table scan.
    foreach (array('m.employee_number=IF('.$number.'>=0,FLOOR('.$number.'),CEIL('.$number.')) AND '.$number.'>=1',
        'm.employee_id=CONVERT(TRIM(e.employee_id) USING '.$text_rules['mapping_code']['charset']
            .') COLLATE '.$text_rules['mapping_code']['collation']) as $predicate) {
        $projection = hrm_person_worker_as_of_projection_sql(db_escape($as_of), $predicate);
        // Keep the complete shared evidence/interval predicates and ordering.
        $from = substr($projection, strpos($projection, 'FROM '));
        $names[] = "(SELECT CASE WHEN clw.link_worker_id IS NOT NULL AND (TRIM(n.first_name)<>'' OR TRIM(n.last_name)<>'') THEN "
            ."NULLIF(TRIM(CONCAT(TRIM(n.first_name),' ',"
            ."IF(TRIM(n.middle_name)='', '', CONCAT(TRIM(n.middle_name),' ')),TRIM(n.last_name))), '') END "
            .$from.' ORDER BY n.effective_from DESC,n.name_id DESC LIMIT 1)';
    }
    return '(CASE WHEN e.employee_id REGEXP '.db_escape($numeric).' THEN '.$names[0].' ELSE '.$names[1].' END)';
}

/**
 * Read the actual identity column collations for one authorized operation.
 * Scalar code lookup compares the mapping column with a coercible literal;
 * correlated search must give that column the same comparison authority even
 * when legacy and foundation tables were installed with different collations.
 * Metadata identifiers are validated before entering SQL, never client-owned.
 * @param bool $canonical Whether independent identity permission/readiness passed.
 * @return array|false Actual searchable column character rules.
 */
function hrm_employee_selector_text_rules($canonical)
{
    $columns = array('legacy_code' => array('employees', 'employee_id'),
        'legacy_name' => array('employees', 'first_name'), 'department' => array('departments', 'department_name'));
    if ($canonical) {
        $columns['mapping_code'] = array('hrm_employee_worker_map', 'employee_id');
        $columns['canonical_name'] = array('hrm_person_names', 'first_name');
    }
    $selects = array();
    foreach ($columns as $key => $column)
        $selects[] = 'SELECT '.db_escape($key).' rule_key,CHARACTER_SET_NAME,COLLATION_NAME FROM information_schema.COLUMNS'
            ." WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='".TB_PREF.db_escape_function($column[0])."'"
            .' AND COLUMN_NAME='.db_escape($column[1]);
    $result = db_query(implode(' UNION ALL ', $selects), 'could not verify employee search collations');
    if (!$result) return false;
    $rules = array();
    while ($row = db_fetch_assoc($result)) {
        if (!isset($row['CHARACTER_SET_NAME'], $row['COLLATION_NAME'])
            || preg_match('/^[a-zA-Z0-9_]+$/D', $row['CHARACTER_SET_NAME']) !== 1
            || preg_match('/^[a-zA-Z0-9_]+$/D', $row['COLLATION_NAME']) !== 1) {
            db_free_result($result);
            return false;
        }
        $rules[$row['rule_key']] = array('charset' => $row['CHARACTER_SET_NAME'],
            'collation' => $row['COLLATION_NAME']);
    }
    db_free_result($result);
    return count($rules) === count($columns) ? $rules : false;
}

/**
 * Compare a literal fragment using the actual target column's collation.
 * A lossless round trip prevents a wider Unicode term from becoming a '?'
 * match in a narrower legacy column. It can still match the canonical column.
 * @param string $expression Trusted cohort field/expression.
 * @param string $pattern Literal for equality, or escaped LIKE pattern.
 * @param array $rule Validated target charset/collation.
 * @param bool $exact Use the legacy exact-name comparison for inactive choices.
 * @return string
 */
function hrm_employee_selector_text_match_sql($expression, $pattern, $rule, $exact = false)
{
    $literal = db_escape($pattern);
    $converted = 'CONVERT('.$literal.' USING '.$rule['charset'].')';
    return '(HEX(CONVERT('.$converted.' USING utf8mb4))=HEX(CONVERT('.$literal.' USING utf8mb4))'
        .' AND '.$expression.($exact ? '=' : ' LIKE ').$converted.' COLLATE '.$rule['collation']
        .($exact ? ')' : " ESCAPE '!')");
}

/**
 * Build the least-data eligible employee projection before search and LIMIT.
 * Settings are server-owned, never request-supplied SQL or permissions.
 * @param array $settings where, manager, show_inactive, canonical, as_of.
 * @param array|false $text_rules Independently authorized, scope/readiness checked.
 * @return string
 */
function hrm_employee_selector_cohort_sql($settings, $text_rules)
{
    $legacy = "CONCAT(e.first_name, ' ', e.last_name)";
    $name = $legacy;
    if (is_array($text_rules) && isset($text_rules['canonical_name'])) {
        $canonical = hrm_employee_selector_canonical_name_sql($settings['as_of'], $text_rules);
        $name_rule = $text_rules['canonical_name'];
        // Keep canonical Unicode intact and give both fallback branches one
        // explicit collation; legacy-name ordering below remains unchanged.
        $name = 'COALESCE(CONVERT('.$canonical.' USING '.$name_rule['charset'].') COLLATE '
            .$name_rule['collation'].',CONVERT('.$legacy.' USING '.$name_rule['charset']
            .') COLLATE '.$name_rule['collation'].')';
    }
    $where = isset($settings['where']) ? $settings['where'] : array();
    if (!empty($settings['manager']))
        $where[] = '!e.inactive'; // Manager eligibility also applies to hydration.
    return 'SELECT e.employee_id,'.$name.' name,d.department_name,e.inactive,'
        .$legacy.' legacy_name FROM '.TB_PREF.'employees e LEFT JOIN '.TB_PREF
        .'departments d ON e.department_id=d.department_id'
        .(empty($where) ? '' : ' WHERE ('.implode(') AND (', $where).')');
}

/**
 * Search one bounded cohort or hydrate at most 50 exact employee IDs.
 * SQL filters names/code/department before LIMIT; no all-employee PHP scan.
 * Continuation is a keyset (legacy sort name, binary employee_id), never offset.
 * It is bound to the term by the caller's server-owned control contract. Each
 * request sees current data; rename/delete between pages can change membership.
 * The 51st lookahead row never enters the name batch or returned options.
 * Callers must authorize their page/control, run the projection-readable guard,
 * and independently refresh canonical permission before calling this helper.
 * @param array $settings Trusted server-owned control contract.
 * @param string $term Literal fragment; empty or * enumerates eligible choices.
 * @param array|null $after Previous sort_name/id, or null.
 * @param array|null $selected Exact IDs for hydration, or null for search.
 * @return array|false rows, more, after, batch; false on invalid input/scope.
 */
function hrm_employee_selector_search($settings, $term = '', $after = null, $selected = null)
{
    if (!is_string($term) || strlen($term) > 1024 || preg_match('//u', $term) !== 1
        || ($after !== null && (!is_array($after) || count($after) !== 2
            || !isset($after['sort_name'], $after['id']) || !is_string($after['sort_name'])
            || !is_string($after['id']) || strlen($after['sort_name']) > 1024 || strlen($after['id']) > 255))
        || ($selected !== null && (!is_array($selected) || count($selected) > 50)))
        return false;
    if ($selected !== null)
        foreach ($selected as $id)
            if (!is_string($id) || strlen($id) > 255 || preg_match('//u', $id) !== 1)
                return false;
    if (!hrm_employee_selector_search_scope_is_current())
        return false;
    $canonical = !empty($settings['canonical']) && hrm_person_worker_user_can_read()
        && isset($settings['as_of']) && hrm_person_worker_normalize_as_of($settings['as_of']) !== false
        && hrm_person_worker_tables_ready()
        && hrm_person_worker_table_exists('hrm_person_canonical_links')
        && hrm_person_worker_table_exists('hrm_person_canonical_link_workers');
    $fragment = $selected === null && trim($term) !== '' && trim($term) !== '*';
    $text_rules = $canonical || $fragment ? hrm_employee_selector_text_rules($canonical) : false;
    if (($canonical || $fragment) && $text_rules === false)
        return false;
    $cohort = hrm_employee_selector_cohort_sql($settings, $text_rules);
    // Original selectors sort by legacy name, not by the canonical display label.
    $sql = 'SELECT employee_id,name,department_name,inactive,legacy_name sort_name FROM ('.$cohort.') choices WHERE 1=1';
    if ($selected !== null) {
        if (empty($selected))
            $sql .= ' AND 1=0';
        else
            $sql .= ' AND BINARY employee_id IN ('.implode(',', array_map('db_escape', $selected)).')';
    } else {
        $term = trim($term);
        if (empty($settings['show_inactive']))
            $sql .= $term !== '' && $term !== '*'
                ? ' AND (!inactive OR BINARY employee_id=BINARY '.db_escape($term).' OR '
                    .hrm_employee_selector_text_match_sql('name', $term,
                        $canonical ? $text_rules['canonical_name'] : $text_rules['legacy_name'], true).')'
                : ' AND !inactive';
        if ($term !== '' && $term !== '*') {
            // Use an explicit escape character to make % and _ literal fragments.
            $pattern = '%'.str_replace(array('!', '%', '_'), array('!!', '!%', '!_'), $term).'%';
            $name_rule = $canonical ? $text_rules['canonical_name'] : $text_rules['legacy_name'];
            $sql .= ' AND ('.hrm_employee_selector_text_match_sql('employee_id', $pattern, $text_rules['legacy_code'])
                .' OR '.hrm_employee_selector_text_match_sql('name', $pattern, $name_rule)
                .' OR '.hrm_employee_selector_text_match_sql('department_name', $pattern, $text_rules['department']).')';
        }
        if ($after !== null)
            $sql .= ' AND (legacy_name>'.db_escape($after['sort_name'])
                .' OR (legacy_name='.db_escape($after['sort_name'])
                .' AND BINARY employee_id>BINARY '.db_escape($after['id']).'))';
    }
    $sql .= ' ORDER BY legacy_name,BINARY employee_id LIMIT '.($selected === null ? 51 : 50);
    $result = db_query($sql, 'could not search employee choices');
    if (!$result)
        return false;
    $rows = array();
    while ($row = db_fetch($result))
        $rows[] = $row;
    db_free_result($result);
    $more = count($rows) > 50;
    if ($more)
        array_pop($rows);
    $last = empty($rows) ? null : $rows[count($rows)-1];
    $batch = $canonical && !empty($rows)
        ? get_hrm_person_worker_report_names_batch_as_of(array_column($rows, 'employee_id'), $settings['as_of']) : false;
    return array('rows' => $rows, 'more' => $more,
        'after' => $more ? array('sort_name' => $last['sort_name'], 'id' => (string)$last['employee_id']) : null,
        'batch' => $batch);
}

/**
 * Format one bounded result using the existing canonical batch semantics.
 * @param array $row
 * @param array $settings
 * @param array|false $batch
 * @return string
 */
function hrm_employee_selector_result_label($row, $settings, $batch)
{
    $name = $row['sort_name'];
    if (!empty($settings['canonical'])) {
        $identity = hrm_person_worker_report_name_from_batch($batch, $row['employee_id'], $settings['as_of']);
        if (is_array($identity) && !empty($identity['canonical_linked'])) {
            $canonical = trim($identity['first_name'].' '.($identity['middle_name'] !== '' ? $identity['middle_name'].' ' : '').$identity['last_name']);
            if ($canonical !== '') $name = $canonical;
        }
    }
    return !empty($settings['manager']) ? $name.' ['.$row['employee_id'].']'
        : (!empty($settings['codes']) ? $row['employee_id'].' - ' : '').$name;
}
