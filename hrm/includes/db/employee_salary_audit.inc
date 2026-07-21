<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

include_once($path_to_root.'/includes/db/domain_audit_db.inc');

/**
 * Build the restricted salary snapshot that is HMACed but never stored.
 *
 * @param array|null $row
 * @return array|null
 */
function hrm_employee_salary_audit_snapshot($row) {
    if (!is_array($row)) {
        return null;
    }
    return array(
        'salary_id' => isset($row['salary_id']) ? (int)$row['salary_id'] : 0,
        'employee_id' => isset($row['employee_id']) ? (string)$row['employee_id'] : '',
        'element_id' => isset($row['element_id']) ? (int)$row['element_id'] : 0,
        'amount' => isset($row['amount']) ? (string)$row['amount'] : '0',
        'effective_from' => isset($row['effective_from']) ? (string)$row['effective_from'] : '',
        'effective_to' => isset($row['effective_to']) ? (string)$row['effective_to'] : '',
        'formula' => isset($row['formula']) ? (string)$row['formula'] : '',
        'reference' => isset($row['reference']) ? (string)$row['reference'] : '',
        'is_active' => isset($row['is_active']) ? (int)$row['is_active'] : 1,
    );
}

/**
 * Return only changed field names; restricted values never enter audit JSON.
 *
 * @param array|null $before
 * @param array|null $after
 * @return array
 */
function hrm_employee_salary_changed_fields($before, $after) {
    $fields = array('employee_id', 'element_id', 'amount', 'effective_from', 'effective_to', 'formula', 'reference', 'is_active');
    $changed = array();
    foreach ($fields as $field) {
        $old = is_array($before) && array_key_exists($field, $before) ? $before[$field] : null;
        $new = is_array($after) && array_key_exists($field, $after) ? $after[$field] : null;
        if ($old !== $new) {
            $changed[] = $field;
        }
    }
    return $changed;
}

/**
 * Append the salary-component domain event inside the caller transaction.
 *
 * @param string $kind create|update|delete
 * @param string $employee_id
 * @param int $salary_id
 * @param array|null $before_row
 * @param array|null $after_row
 * @param int $history_id
 * @return array|false
 */
function append_employee_salary_domain_audit($kind, $employee_id, $salary_id, $before_row, $after_row, $history_id) {
    $actions = array(
        'create' => 'hr.salary_component.created',
        'update' => 'hr.salary_component.updated',
        'delete' => 'hr.salary_component.deleted',
    );
    if (!isset($actions[$kind]) || trim((string)$employee_id) === '' || (int)$salary_id <= 0 || (int)$history_id <= 0) {
        return false;
    }
    $before = hrm_employee_salary_audit_snapshot($before_row);
    $after = hrm_employee_salary_audit_snapshot($after_row);
    $element_id = is_array($after) ? (int)$after['element_id'] : (is_array($before) ? (int)$before['element_id'] : 0);
    return append_domain_audit_event(array(
        'company_id' => function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id' => 0,
        'entity_type' => 'employee_salary',
        'entity_key' => array('employee_id' => (string)$employee_id, 'salary_id' => (int)$salary_id),
        'action' => $actions[$kind],
        'reason_code' => 'employee_salary_element_'.$kind,
        'legacy_source' => 'employee_history',
        'legacy_id' => (int)$history_id,
        'before' => $before,
        'after' => $after,
        'restricted_diff' => array(
            'change_kind' => $kind,
            'salary_id' => (int)$salary_id,
            'element_id' => $element_id,
            'history_id' => (int)$history_id,
            'changed_fields' => hrm_employee_salary_changed_fields($before, $after),
            'restricted_values_stored' => false,
        ),
    ));
}
