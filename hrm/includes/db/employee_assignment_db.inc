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
 * entity, create concurrency, execute rehire, or change payroll input. Effective-
 * dated transfer/promotion execution is added below without changing legacy
 * payroll selection or normalized organization semantics.
 */

function hrm_assignment_table_ready()
{
    if (!function_exists('db_query') || !function_exists('db_escape_function')
        || !defined('TB_PREF'))
        return false;
    // Keep the runtime TB_PREF placeholder outside value escaping. In browser
    // sessions TB_PREF is '&TB_PREF&' and db_query() replaces that token with
    // the selected company prefix. Escaping the full prefixed name first would
    // encode the placeholder and make an installed foundation table appear absent.
    $result = db_query("SHOW TABLES LIKE '".TB_PREF.db_escape_function('hrm_assignments')."'");
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

function hrm_assignment_nullable_int_equal($a, $b)
{
    if ($a === null || $a === '') $a = null;
    if ($b === null || $b === '') $b = null;
    if ($a === null || $b === null)
        return $a === $b;
    if (!is_numeric($a) || !is_numeric($b)
        || (string)(int)$a !== trim((string)$a)
        || (string)(int)$b !== trim((string)$b))
        return false;
    return (int)$a === (int)$b;
}

function hrm_assignment_normalize_effective_date($value)
{
    if ($value === null)
        return false;
    $value = trim((string)$value);
    if (preg_match('/^\d{4}-\d{2}-\d{2}$/D', $value) !== 1)
        return false;
    $date = DateTime::createFromFormat('!Y-m-d', $value);
    if (!$date || $date->format('Y-m-d') !== $value)
        return false;
    return $value;
}

function hrm_assignment_day_before($value)
{
    $value = hrm_assignment_normalize_effective_date($value);
    if ($value === false)
        return false;
    $date = DateTime::createFromFormat('!Y-m-d', $value);
    if (!$date)
        return false;
    $date->modify('-1 day');
    return $date->format('Y-m-d');
}

function hrm_assignment_effective_change_type($value)
{
    $value = trim((string)$value);
    return in_array($value, array('transfer', 'promotion'), true) ? $value : false;
}

function hrm_assignment_projection_matches_row($row, $projection, $include_interval = true)
{
    if (!is_array($row) || !is_array($projection))
        return false;
    $integer_fields = array('employment_id','legacy_employee_number','legacy_department_id',
        'legacy_position_id','legacy_grade_id','legacy_shift_id','legacy_cost_center_id','legacy_dimension2_id');
    foreach ($integer_fields as $field) {
        if (!array_key_exists($field, $row) || !array_key_exists($field, $projection)
            || !hrm_assignment_nullable_int_equal($row[$field], $projection[$field]))
            return false;
    }
    foreach (array('assignment_type_code','legacy_reporting_to','reference_quality') as $field) {
        if (!array_key_exists($field, $row) || !array_key_exists($field, $projection)
            || !hrm_assignment_nullable_equal($row[$field], $projection[$field]))
            return false;
    }
    if ((int)$row['primary_flag'] !== (int)$projection['primary_flag'])
        return false;
    if ($include_interval) {
        foreach (array('effective_from','effective_to','interval_quality') as $field) {
            if (!array_key_exists($field, $row) || !array_key_exists($field, $projection)
                || !hrm_assignment_nullable_equal($row[$field], $projection[$field]))
                return false;
        }
    }
    return true;
}

function execute_hrm_assignment_effective_change($employee, $effective_date, $change_type)
{
    if (!is_array($employee) || empty($employee['employee_number']) || !hrm_assignment_table_ready()
        || !function_exists('get_hrm_legacy_current_employment'))
        return false;
    $effective_date = hrm_assignment_normalize_effective_date($effective_date);
    $change_type = hrm_assignment_effective_change_type($change_type);
    if ($effective_date === false || $change_type === false)
        return false;

    $employee_number = (int)$employee['employee_number'];
    $employment = get_hrm_legacy_current_employment($employee_number, true);
    $current = get_hrm_legacy_current_assignment($employee_number, true);
    if (!$employment || !$current || (int)$current['employment_id'] !== (int)$employment['employment_id']
        || (int)$current['legacy_employee_number'] !== $employee_number
        || (int)$current['legacy_current'] !== 1 || (int)$current['assignment_sequence'] < 1
        || (string)$current['assignment_type_code'] !== 'legacy_primary' || (int)$current['primary_flag'] !== 1)
        return false;

    $start = hrm_employment_normalize_date(isset($employment['start_date']) ? $employment['start_date'] : null);
    $end = hrm_employment_normalize_date(isset($employment['end_date']) ? $employment['end_date'] : null);
    if ($start === false || $end === false || ($start !== null && strcmp($effective_date, $start) < 0)
        || ($end !== null && strcmp($effective_date, $end) > 0))
        return false;

    $allow_legacy_irregular = (string)$current['reference_quality'] === 'legacy_irregular';
    $projection = hrm_assignment_projection_from_employee($employee, $employment, $allow_legacy_irregular);
    if (!$projection)
        return false;
    $projection['effective_from'] = $effective_date;
    $projection['effective_to'] = $end;

    $source = 'runtime_'.$change_type;
    if (hrm_assignment_nullable_equal($current['effective_from'], $effective_date)) {
        return (string)$current['source'] === $source
            && hrm_assignment_projection_matches_row($current, $projection, true);
    }
    if ($current['effective_from'] !== null && $current['effective_from'] !== ''
        && strcmp($effective_date, (string)$current['effective_from']) <= 0)
        return false;

    $previous_end = hrm_assignment_day_before($effective_date);
    if ($previous_end === false || ($current['effective_from'] !== null && $current['effective_from'] !== ''
        && strcmp($previous_end, (string)$current['effective_from']) < 0))
        return false;

    $changed = array();
    $map = array(
        'legacy_department_id'=>'department','legacy_position_id'=>'position','legacy_grade_id'=>'grade',
        'legacy_shift_id'=>'shift','legacy_reporting_to'=>'reporting','legacy_cost_center_id'=>'cost_center',
        'legacy_dimension2_id'=>'dimension2','reference_quality'=>'reference_quality'
    );
    foreach ($map as $field=>$label) {
        $equal = in_array($field, array('legacy_department_id','legacy_position_id','legacy_grade_id',
            'legacy_shift_id','legacy_cost_center_id','legacy_dimension2_id'), true)
            ? hrm_assignment_nullable_int_equal($current[$field], $projection[$field])
            : hrm_assignment_nullable_equal($current[$field], $projection[$field]);
        if (!$equal)
            $changed[] = $label;
    }
    if (empty($changed))
        return false;

    $actor = hrm_assignment_actor_id();
    $close = "UPDATE ".TB_PREF."hrm_assignments SET legacy_current=NULL,effective_to=".db_escape($previous_end)
        .",source=".db_escape($source).",updated_at=NOW(),updated_by=".(int)$actor
        ." WHERE assignment_id=".(int)$current['assignment_id']." AND legacy_current=1";
    if (!db_query($close, 'could not close prior HRM assignment interval'))
        return false;
    if (!append_hrm_assignment_domain_audit(
        'hrm.assignment.closed_for_change', (int)$current['assignment_id'], (int)$employment['employment_id'],
        $employee_number, array('effective_interval','legacy_current'), 'hrm_fnd_002_assignment_'.$change_type.'_closed'
    ))
        return false;

    $next_sequence = (int)$current['assignment_sequence'] + 1;
    $sql = "INSERT INTO ".TB_PREF."hrm_assignments (employment_id,assignment_sequence,legacy_employee_number,"
        ."legacy_current,assignment_type_code,primary_flag,legacy_department_id,legacy_position_id,legacy_grade_id,"
        ."legacy_shift_id,legacy_reporting_to,legacy_cost_center_id,legacy_dimension2_id,reference_quality,"
        ."effective_from,effective_to,interval_quality,source,created_by) VALUES ("
        .(int)$projection['employment_id'].",".$next_sequence.",".$employee_number.",1,'legacy_primary',1,"
        .(int)$projection['legacy_department_id'].",".(int)$projection['legacy_position_id'].",".(int)$projection['legacy_grade_id'].","
        .($projection['legacy_shift_id'] === null ? 'NULL' : (int)$projection['legacy_shift_id']).","
        .($projection['legacy_reporting_to'] === null ? 'NULL' : db_escape($projection['legacy_reporting_to'])).","
        .($projection['legacy_cost_center_id'] === null ? 'NULL' : (int)$projection['legacy_cost_center_id']).",".(int)$projection['legacy_dimension2_id'].","
        .db_escape($projection['reference_quality']).",".db_escape($effective_date).","
        .($end === null ? 'NULL' : db_escape($end)).",".db_escape($projection['interval_quality']).","
        .db_escape($source).",".(int)$actor.")";
    if (!db_query($sql, 'could not create successor HRM assignment interval'))
        return false;
    $assignment_id = (int)db_insert_id();
    if ($assignment_id <= 0)
        return false;
    $changed[] = 'effective_interval';
    return append_hrm_assignment_domain_audit(
        'hrm.assignment.effective_change_created', $assignment_id, (int)$employment['employment_id'],
        $employee_number, $changed, 'hrm_fnd_002_assignment_'.$change_type.'_created'
    ) !== false;
}

/**
 * End the accepted current primary Assignment at an Employee Separation date.
 * No successor Assignment is created. Work-reference fields must remain exact;
 * a combined transfer/promotion plus termination attempt therefore fails closed.
 *
 * @param array $employee
 * @param string $effective_date SQL date (Y-m-d)
 * @return bool
 */
function execute_hrm_assignment_termination($employee, $effective_date)
{
    if (!is_array($employee) || empty($employee['employee_number'])
        || !hrm_assignment_table_ready() || !function_exists('get_hrm_legacy_current_employment'))
        return false;
    $effective_date = hrm_assignment_normalize_effective_date($effective_date);
    if ($effective_date === false)
        return false;

    $employee_number = (int)$employee['employee_number'];
    $employment = get_hrm_legacy_current_employment($employee_number, true);
    $current = get_hrm_legacy_current_assignment($employee_number, true);
    if (!$employment || !$current || (int)$current['employment_id'] !== (int)$employment['employment_id']
        || (int)$current['legacy_employee_number'] !== $employee_number
        || (int)$current['legacy_current'] !== 1 || (int)$current['assignment_sequence'] < 1
        || (string)$current['assignment_type_code'] !== 'legacy_primary' || (int)$current['primary_flag'] !== 1
        || (string)$employment['employment_status'] !== 'terminated'
        || !hrm_assignment_nullable_equal($employment['end_date'], $effective_date))
        return false;

    $allow_legacy_irregular = (string)$current['reference_quality'] === 'legacy_irregular';
    $projection = hrm_assignment_projection_from_employee($employee, $employment, $allow_legacy_irregular);
    if (!$projection)
        return false;
    $projection['effective_from'] = ($current['effective_from'] === null || $current['effective_from'] === '')
        ? null : (string)$current['effective_from'];
    $projection['effective_to'] = $effective_date;

    if ($projection['effective_from'] !== null
        && strcmp($effective_date, (string)$projection['effective_from']) < 0)
        return false;

    $integer_fields = array('legacy_department_id','legacy_position_id','legacy_grade_id','legacy_shift_id',
        'legacy_cost_center_id','legacy_dimension2_id');
    foreach ($integer_fields as $field) {
        if (!hrm_assignment_nullable_int_equal($current[$field], $projection[$field]))
            return false;
    }
    foreach (array('legacy_reporting_to','reference_quality') as $field) {
        if (!hrm_assignment_nullable_equal($current[$field], $projection[$field]))
            return false;
    }

    if (hrm_assignment_nullable_equal($current['effective_to'], $effective_date))
        return true;
    if ($current['effective_to'] !== null && $current['effective_to'] !== '')
        return false;

    $actor = hrm_assignment_actor_id();
    $sql = "UPDATE ".TB_PREF."hrm_assignments SET effective_to=".db_escape($effective_date)
        .",interval_quality=".db_escape($projection['interval_quality'])
        .",source='runtime_separation',updated_at=NOW(),updated_by=".(int)$actor
        ." WHERE assignment_id=".(int)$current['assignment_id']." AND legacy_current=1";
    if (!db_query($sql, 'could not end HRM assignment for separation'))
        return false;
    return append_hrm_assignment_domain_audit(
        'hrm.assignment.ended_for_separation', (int)$current['assignment_id'],
        (int)$employment['employment_id'], $employee_number, array('effective_interval'),
        'hrm_fnd_002_employee_separation'
    ) !== false;
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
        || (int)$current['assignment_sequence'] < 1 || (int)$current['legacy_current'] !== 1
        || (int)$current['legacy_employee_number'] !== $employee_number
        || (string)$current['assignment_type_code'] !== 'legacy_primary' || (int)$current['primary_flag'] !== 1)
        return false;

    $allow_legacy_irregular = (string)$current['reference_quality'] === 'legacy_irregular';
    $projection = hrm_assignment_projection_from_employee($employee, $employment, $allow_legacy_irregular);
    if (!$projection)
        return false;
    if ((int)$current['assignment_sequence'] > 1)
        $projection['effective_from'] = ($current['effective_from'] === null || $current['effective_from'] === '')
            ? null : (string)$current['effective_from'];
    $integer_fields = array('legacy_department_id','legacy_position_id','legacy_grade_id','legacy_shift_id',
        'legacy_cost_center_id','legacy_dimension2_id');
    if ($allow_legacy_irregular) {
        foreach ($integer_fields as $field) {
            if (!hrm_assignment_nullable_int_equal($current[$field], $projection[$field]))
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
        $equal = in_array($field, $integer_fields, true)
            ? hrm_assignment_nullable_int_equal($current[$field], $projection[$field])
            : hrm_assignment_nullable_equal($current[$field], $projection[$field]);
        if (!$equal)
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

function synchronize_hrm_assignment_from_employee_if_ready($employee, $create = false, $effective_change = null)
{
    if (!hrm_assignment_table_ready())
        return true;
    if ($create)
        return create_hrm_assignment_from_employee($employee, true, 'runtime_employee') !== false;
    if (is_array($effective_change)) {
        $effective_date = isset($effective_change['effective_date']) ? $effective_change['effective_date'] : null;
        $change_type = isset($effective_change['change_type']) ? $effective_change['change_type'] : null;
        if ((string)$change_type === 'separation')
            return execute_hrm_assignment_termination($employee, $effective_date);
        return execute_hrm_assignment_effective_change($employee, $effective_date, $change_type);
    }
    return synchronize_hrm_assignment_from_employee($employee);
}
