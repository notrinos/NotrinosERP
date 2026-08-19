<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

/**
 * HRM-FND-002 bounded Assignment compatibility foundation.
 *
 * Legacy employees remain authoritative for current UI/payroll behavior. This
 * shadow preserves the current row's organization/work identifiers as one
 * primary legacy Assignment bound to the accepted Employment interval. It does
 * not normalize Job/Position/Org/Location/Manager semantics, infer FTE or legal
 * entity, create concurrency, execute transfer/rehire, or change payroll input.
 */

function hrm_assignment_table_ready()
{
    if (!function_exists('db_query') || !defined('TB_PREF'))
        return false;
    $result = db_query("SHOW TABLES LIKE ".db_escape(TB_PREF.'hrm_assignments'));
    return $result && db_num_rows($result) === 1;
}

function hrm_assignment_actor_id()
{
    if (function_exists('user_id'))
        return (int)user_id();
    if (isset($_SESSION['wa_current_user']) && isset($_SESSION['wa_current_user']->user))
        return (int)$_SESSION['wa_current_user']->user;
    return 0;
}

function hrm_assignment_required_legacy_int($value, $allow_legacy_irregular = false)
{
    if (!is_numeric($value) || (string)(int)$value !== trim((string)$value))
        return false;
    $value = (int)$value;
    if ($value < 0 && !$allow_legacy_irregular)
        return false;
    return $value;
}

function hrm_assignment_nullable_legacy_int($value, $allow_legacy_irregular = false)
{
    if ($value === null || $value === '')
        return null;
    return hrm_assignment_required_legacy_int($value, $allow_legacy_irregular);
}

function hrm_assignment_projection_from_employee($employee, $employment, $allow_legacy_irregular = false)
{
    if (!is_array($employee) || !is_array($employment)
        || empty($employee['employee_number']) || empty($employment['employment_id']))
        return false;
    $employee_number = (int)$employee['employee_number'];
    if ((int)$employment['legacy_employee_number'] !== $employee_number
        || (int)$employment['legacy_current'] !== 1
        || (int)$employment['employment_sequence'] !== 1)
        return false;

    $department = hrm_assignment_required_legacy_int(isset($employee['department_id']) ? $employee['department_id'] : 0, $allow_legacy_irregular);
    $position = hrm_assignment_required_legacy_int(isset($employee['position_id']) ? $employee['position_id'] : 0, $allow_legacy_irregular);
    $grade = hrm_assignment_required_legacy_int(isset($employee['grade_id']) ? $employee['grade_id'] : 0, $allow_legacy_irregular);
    $shift = hrm_assignment_nullable_legacy_int(isset($employee['shift_id']) ? $employee['shift_id'] : null, $allow_legacy_irregular);
    $cost_center = hrm_assignment_nullable_legacy_int(isset($employee['cost_center_id']) ? $employee['cost_center_id'] : null, $allow_legacy_irregular);
    $dimension2 = hrm_assignment_required_legacy_int(isset($employee['dimension2_id']) ? $employee['dimension2_id'] : 0, $allow_legacy_irregular);
    if ($department === false || $position === false || $grade === false || $shift === false
        || $cost_center === false || $dimension2 === false)
        return false;

    $reporting_to = isset($employee['reporting_to']) ? trim((string)$employee['reporting_to']) : '';
    if (strlen($reporting_to) > 20)
        return false;
    if ($reporting_to === '')
        $reporting_to = null;

    $quality = ($department < 0 || $position < 0 || $grade < 0
        || ($shift !== null && $shift < 0) || ($cost_center !== null && $cost_center < 0)
        || $dimension2 < 0) ? 'legacy_irregular' : 'legacy_unverified';

    return array(
        'employment_id' => (int)$employment['employment_id'],
        'legacy_employee_number' => $employee_number,
        'assignment_type_code' => 'legacy_primary',
        'primary_flag' => 1,
        'legacy_department_id' => $department,
        'legacy_position_id' => $position,
        'legacy_grade_id' => $grade,
        'legacy_shift_id' => $shift,
        'legacy_reporting_to' => $reporting_to,
        'legacy_cost_center_id' => $cost_center,
        'legacy_dimension2_id' => $dimension2,
        'reference_quality' => $quality,
        'effective_from' => ($employment['start_date'] === null || $employment['start_date'] === '') ? null : (string)$employment['start_date'],
        'effective_to' => ($employment['end_date'] === null || $employment['end_date'] === '') ? null : (string)$employment['end_date'],
        'interval_quality' => (string)$employment['interval_quality'],
    );
}

function get_hrm_legacy_current_assignment($employee_number, $for_update = false)
{
    if (!hrm_assignment_table_ready() || (int)$employee_number <= 0)
        return false;
    $sql = "SELECT a.assignment_id,a.employment_id,a.assignment_sequence,a.legacy_employee_number,"
        ."a.legacy_current,a.assignment_type_code,a.primary_flag,a.legacy_department_id,"
        ."a.legacy_position_id,a.legacy_grade_id,a.legacy_shift_id,a.legacy_reporting_to,"
        ."a.legacy_cost_center_id,a.legacy_dimension2_id,a.reference_quality,a.effective_from,"
        ."a.effective_to,a.interval_quality,a.source,a.created_by,a.updated_by "
        ."FROM ".TB_PREF."hrm_assignments a INNER JOIN ".TB_PREF."hrm_employments h "
        ."ON h.employment_id=a.employment_id WHERE a.legacy_employee_number=".(int)$employee_number." "
        ."AND a.legacy_current=1 AND h.legacy_employee_number=".(int)$employee_number." "
        ."AND h.legacy_current=1 ORDER BY a.assignment_id DESC LIMIT 1";
    if ($for_update)
        $sql .= " FOR UPDATE";
    $result = db_query($sql, 'could not read HRM assignment compatibility row');
    return $result ? db_fetch_assoc($result) : false;
}

function hrm_assignment_nullable_equal($a, $b)
{
    if ($a === null || $a === '') $a = null;
    if ($b === null || $b === '') $b = null;
    return $a === $b;
}

function append_hrm_assignment_domain_audit($action, $assignment_id, $employment_id,
    $employee_number, $changed_fields, $reason_code)
{
    if (!function_exists('append_domain_audit_event') || (int)$assignment_id <= 0
        || (int)$employment_id <= 0 || (int)$employee_number <= 0
        || !is_array($changed_fields) || trim((string)$action) === ''
        || trim((string)$reason_code) === '')
        return false;
    $changed_fields = array_values(array_unique($changed_fields));
    sort($changed_fields);
    return append_domain_audit_event(array(
        'company_id' => function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id' => 0,
        'entity_type' => 'hrm_assignment_shadow',
        'entity_key' => array(
            'assignment_id' => (int)$assignment_id,
            'employment_id' => (int)$employment_id,
            'employee_number' => (int)$employee_number,
        ),
        'action' => (string)$action,
        'reason_code' => (string)$reason_code,
        'before' => null,
        'after' => array('assignment_projection_changed' => true),
        'restricted_diff' => array(
            'changed_fields' => $changed_fields,
            'restricted_values_stored' => false,
        ),
    ));
}

function create_hrm_assignment_from_employee($employee, $audit = true,
    $source = 'runtime_employee', $allow_legacy_irregular = false)
{
    if (!hrm_assignment_table_ready() || !function_exists('get_hrm_legacy_current_employment'))
        return false;
    $employee_number = isset($employee['employee_number']) ? (int)$employee['employee_number'] : 0;
    $employment = get_hrm_legacy_current_employment($employee_number, true);
    if (!$employment)
        return false;
    $projection = hrm_assignment_projection_from_employee($employee, $employment, $allow_legacy_irregular);
    if (!$projection || get_hrm_legacy_current_assignment($employee_number, true))
        return false;

    $actor = hrm_assignment_actor_id();
    $sql = "INSERT INTO ".TB_PREF."hrm_assignments (employment_id,assignment_sequence,legacy_employee_number,"
        ."legacy_current,assignment_type_code,primary_flag,legacy_department_id,legacy_position_id,legacy_grade_id,"
        ."legacy_shift_id,legacy_reporting_to,legacy_cost_center_id,legacy_dimension2_id,reference_quality,"
        ."effective_from,effective_to,interval_quality,source,created_by) VALUES ("
        .(int)$projection['employment_id'].",1,".$employee_number.",1,'legacy_primary',1,"
        .(int)$projection['legacy_department_id'].",".(int)$projection['legacy_position_id'].",".(int)$projection['legacy_grade_id'].","
        .($projection['legacy_shift_id'] === null ? 'NULL' : (int)$projection['legacy_shift_id'])."," 
        .($projection['legacy_reporting_to'] === null ? 'NULL' : db_escape($projection['legacy_reporting_to'])).","
        .($projection['legacy_cost_center_id'] === null ? 'NULL' : (int)$projection['legacy_cost_center_id']).",".(int)$projection['legacy_dimension2_id'].","
        .db_escape($projection['reference_quality']).","
        .($projection['effective_from'] === null ? 'NULL' : db_escape($projection['effective_from'])).","
        .($projection['effective_to'] === null ? 'NULL' : db_escape($projection['effective_to'])).","
        .db_escape($projection['interval_quality']).",".db_escape((string)$source).",".(int)$actor.")";
    if (!db_query($sql, 'could not create HRM assignment shadow'))
        return false;
    $assignment_id = (int)db_insert_id();
    if ($assignment_id <= 0)
        return false;
    if ($audit && !append_hrm_assignment_domain_audit(
        'hrm.assignment.created', $assignment_id, (int)$projection['employment_id'], $employee_number,
        array('legacy_work_references', 'effective_interval', 'primary_projection'),
        'hrm_fnd_002_assignment_created'
    ))
        return false;
    return $assignment_id;
}

function synchronize_hrm_assignment_from_employee($employee)
{
    if (!is_array($employee) || empty($employee['employee_number']) || !hrm_assignment_table_ready())
        return false;
    $employee_number = (int)$employee['employee_number'];
    $employment = get_hrm_legacy_current_employment($employee_number, true);
    if (!$employment)
        return false;
    $current = get_hrm_legacy_current_assignment($employee_number, true);
    if (!$current)
        return create_hrm_assignment_from_employee($employee, true, 'runtime_employee') !== false;
    if ((int)$current['employment_id'] !== (int)$employment['employment_id']
        || (int)$current['assignment_sequence'] !== 1 || (int)$current['legacy_current'] !== 1
        || (int)$current['legacy_employee_number'] !== $employee_number
        || (string)$current['assignment_type_code'] !== 'legacy_primary' || (int)$current['primary_flag'] !== 1)
        return false;

    $allow_legacy_irregular = (string)$current['reference_quality'] === 'legacy_irregular';
    $projection = hrm_assignment_projection_from_employee($employee, $employment, $allow_legacy_irregular);
    if (!$projection)
        return false;
    if ($allow_legacy_irregular) {
        foreach (array('legacy_department_id','legacy_position_id','legacy_grade_id','legacy_shift_id','legacy_cost_center_id','legacy_dimension2_id') as $field) {
            if (!hrm_assignment_nullable_equal($current[$field], $projection[$field]))
                return false;
        }
    }

    $changed = array();
    $map = array(
        'legacy_department_id' => 'department', 'legacy_position_id' => 'position', 'legacy_grade_id' => 'grade',
        'legacy_shift_id' => 'shift', 'legacy_reporting_to' => 'reporting', 'legacy_cost_center_id' => 'cost_center',
        'legacy_dimension2_id' => 'dimension2', 'reference_quality' => 'reference_quality',
        'effective_from' => 'effective_interval', 'effective_to' => 'effective_interval', 'interval_quality' => 'effective_interval',
    );
    foreach ($map as $field => $label) {
        if (!hrm_assignment_nullable_equal($current[$field], $projection[$field]))
            $changed[] = $label;
    }
    if (empty($changed))
        return true;

    $actor = hrm_assignment_actor_id();
    $sql = "UPDATE ".TB_PREF."hrm_assignments SET legacy_department_id=".(int)$projection['legacy_department_id']
        .",legacy_position_id=".(int)$projection['legacy_position_id'].",legacy_grade_id=".(int)$projection['legacy_grade_id']
        .",legacy_shift_id=".($projection['legacy_shift_id'] === null ? 'NULL' : (int)$projection['legacy_shift_id'])
        .",legacy_reporting_to=".($projection['legacy_reporting_to'] === null ? 'NULL' : db_escape($projection['legacy_reporting_to']))
        .",legacy_cost_center_id=".($projection['legacy_cost_center_id'] === null ? 'NULL' : (int)$projection['legacy_cost_center_id'])
        .",legacy_dimension2_id=".(int)$projection['legacy_dimension2_id']
        .",reference_quality=".db_escape($projection['reference_quality'])
        .",effective_from=".($projection['effective_from'] === null ? 'NULL' : db_escape($projection['effective_from']))
        .",effective_to=".($projection['effective_to'] === null ? 'NULL' : db_escape($projection['effective_to']))
        .",interval_quality=".db_escape($projection['interval_quality']).",source='runtime_employee',updated_at=NOW(),updated_by=".(int)$actor
        ." WHERE assignment_id=".(int)$current['assignment_id'];
    if (!db_query($sql, 'could not synchronize HRM assignment shadow'))
        return false;
    return append_hrm_assignment_domain_audit(
        'hrm.assignment.updated', (int)$current['assignment_id'], (int)$employment['employment_id'], $employee_number,
        $changed, 'hrm_fnd_002_assignment_updated'
    ) !== false;
}

function synchronize_hrm_assignment_from_employee_if_ready($employee, $create = false)
{
    if (!hrm_assignment_table_ready())
        return true;
    if ($create)
        return create_hrm_assignment_from_employee($employee, true, 'runtime_employee') !== false;
    return synchronize_hrm_assignment_from_employee($employee);
}
