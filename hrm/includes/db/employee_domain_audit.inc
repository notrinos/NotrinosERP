<?php
/**
 * PAY-AUD-001 minimized employee-profile and cascade audit adapter.
 */

include_once($path_to_root.'/includes/db/domain_audit_db.inc');

/**
 * Read one employee row without joined display fields.
 *
 * @param string $employee_id
 * @return array|false
 */
function hrm_employee_domain_audit_row($employee_id)
{
    $result = db_query(
        "SELECT * FROM ".TB_PREF."employees"
        ." WHERE employee_id=".db_escape($employee_id)." LIMIT 1",
        'could not read employee audit snapshot'
    );
    return $result ? db_fetch_assoc($result) : false;
}

/**
 * Return stable changed employee column names only.
 *
 * @param array|null $before
 * @param array|null $after
 * @return array
 */
function hrm_employee_domain_audit_changed_fields($before, $after)
{
    $before = is_array($before) ? $before : array();
    $after = is_array($after) ? $after : array();
    $fields = array_unique(array_merge(array_keys($before), array_keys($after)));
    $changed = array();
    foreach ($fields as $field) {
        $left = array_key_exists($field, $before) ? $before[$field] : null;
        $right = array_key_exists($field, $after) ? $after[$field] : null;
        if ($left !== $right) {
            $changed[] = (string)$field;
        }
    }
    sort($changed);
    return $changed;
}

/**
 * Append one employee create/update event with HMAC-only snapshots.
 *
 * @param string $kind created|updated
 * @param string $employee_id
 * @param array|null $before
 * @param array|null $after
 * @return array|false
 */
function append_employee_profile_domain_audit(
    $kind,
    $employee_id,
    $before,
    $after
) {
    if (!in_array($kind, array('created', 'updated'), true)
        || trim((string)$employee_id) === ''
        || ($kind === 'created' && !is_array($after))
        || ($kind === 'updated' && (!is_array($before) || !is_array($after)))
    ) {
        return false;
    }
    return append_domain_audit_event(array(
        'company_id' => function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id' => 0,
        'entity_type' => 'employee',
        'entity_key' => array('employee_id' => (string)$employee_id),
        'action' => 'employee.'.$kind,
        'reason_code' => 'employee_profile_'.$kind,
        'before' => $before,
        'after' => $after,
        'restricted_diff' => array(
            'changed_fields' => hrm_employee_domain_audit_changed_fields($before, $after),
            'mutation_kind' => $kind,
            'restricted_values_stored' => false,
        ),
    ));
}

/**
 * Build the HMAC-only whole-employee deletion snapshot.
 *
 * @param string $employee_id
 * @return array|false
 */
function hrm_employee_deletion_audit_snapshot($employee_id)
{
    $employee = hrm_employee_domain_audit_row($employee_id);
    if (!is_array($employee)) {
        return false;
    }
    $snapshot = array('employee' => $employee, 'related' => array());
    foreach (array(
        'employee_documents', 'employee_dependents', 'employee_history',
        'employee_salary', 'leave_balances',
    ) as $table) {
        if (!employee_table_exists($table)) {
            $snapshot['related'][$table] = array();
            continue;
        }
        $result = db_query(
            "SELECT * FROM ".TB_PREF.$table
            ." WHERE employee_id=".db_escape($employee_id),
            'could not read employee cascade audit snapshot'
        );
        if (!$result) {
            return false;
        }
        $rows = array();
        while ($row = db_fetch_assoc($result)) {
            $rows[] = $row;
        }
        $snapshot['related'][$table] = $rows;
    }
    return $snapshot;
}

/**
 * Append a whole-employee deletion event before the outer transaction commits.
 *
 * @param string $employee_id
 * @param array $snapshot
 * @return array|false
 */
function append_employee_deletion_domain_audit($employee_id, $snapshot)
{
    if (trim((string)$employee_id) === '' || !is_array($snapshot)
        || !isset($snapshot['employee'], $snapshot['related'])
    ) {
        return false;
    }
    $counts = array();
    foreach ($snapshot['related'] as $table => $rows) {
        $counts[$table] = count($rows);
    }
    ksort($counts);
    return append_domain_audit_event(array(
        'company_id' => function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id' => 0,
        'entity_type' => 'employee',
        'entity_key' => array('employee_id' => (string)$employee_id),
        'action' => 'employee.deleted',
        'reason_code' => 'employee_profile_deleted',
        'before' => $snapshot,
        'after' => null,
        'restricted_diff' => array(
            'history_preserved' => true,
            'mutation_kind' => 'deleted',
            'related_row_counts' => $counts,
            'restricted_values_stored' => false,
        ),
    ));
}
