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

/** @return bool */
function hrm_assignment_organization_unit_column_ready()
{
    if (!hrm_assignment_table_ready())
        return false;
    $result = db_query("SHOW COLUMNS FROM ".TB_PREF."hrm_assignments LIKE 'organization_unit_id'");
    return $result && db_num_rows($result) === 1;
}

/** @return bool */
function hrm_assignment_grade_column_ready()
{
    if (!hrm_assignment_table_ready())
        return false;
    $result = db_query("SHOW COLUMNS FROM ".TB_PREF."hrm_assignments LIKE 'grade_id'");
    return $result && db_num_rows($result) === 1;
}

/** @return bool */
function hrm_assignment_position_column_ready()
{
    if (!hrm_assignment_table_ready())
        return false;
    $result = db_query("SHOW COLUMNS FROM ".TB_PREF."hrm_assignments LIKE 'position_id'");
    return $result && db_num_rows($result) === 1;
}

/** @return bool */
function hrm_assignment_job_column_ready()
{
    if (!hrm_assignment_table_ready())
        return false;
    $result = db_query("SHOW COLUMNS FROM ".TB_PREF."hrm_assignments LIKE 'job_id'");
    return $result && db_num_rows($result) === 1;
}

/** @return bool */
function hrm_assignment_work_location_column_ready()
{
    if (!hrm_assignment_table_ready())
        return false;
    $result = db_query("SHOW COLUMNS FROM ".TB_PREF."hrm_assignments LIKE 'work_location_id'");
    return $result && db_num_rows($result) === 1;
}

/** @return bool */
function hrm_assignment_manager_column_ready()
{
    if (!hrm_assignment_table_ready())
        return false;
    $result = db_query("SHOW COLUMNS FROM ".TB_PREF."hrm_assignments LIKE 'manager_assignment_id'");
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
        'organization_unit_id' => null,
        'legacy_position_id' => $position,
        'position_id' => null,
        'job_id' => null,
        'work_location_id' => null,
        'manager_assignment_id' => null,
        'legacy_grade_id' => $grade,
        'grade_id' => null,
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

/**
 * Bind a pure legacy Assignment projection to the exact Organization Unit.
 * Older upgrade steps run before the 1.0.212 column exists and intentionally
 * retain the pre-binding projection until this migration activates.
 *
 * @param array $projection
 * @param array $employment
 * @param bool $for_update
 * @param bool $allow_legacy_irregular
 * @return array|false
 */
function hrm_assignment_bind_organization_unit($projection, $employment,
    $for_update = false, $allow_legacy_irregular = false)
{
    if (!is_array($projection) || !is_array($employment))
        return false;
    if (!hrm_assignment_organization_unit_column_ready())
        return $projection;
    if (!function_exists('get_hrm_verified_organization_unit_binding')
        || empty($employment['legal_entity_id']))
        return false;
    $organization_unit_id = get_hrm_verified_organization_unit_binding(
        $projection['legacy_department_id'], (int)$employment['legal_entity_id'],
        $for_update, $allow_legacy_irregular
    );
    if ($organization_unit_id === false)
        return false;
    $projection['organization_unit_id'] = $organization_unit_id;
    return $projection;
}

/**
 * Bind a pure legacy Assignment projection to its exact normalized Position.
 * Older upgrade steps intentionally retain legacy_position_id until 1.0.216.
 *
 * @param array $projection
 * @param array $employment
 * @param bool $for_update
 * @param bool $allow_legacy_irregular
 * @return array|false
 */
function hrm_assignment_bind_position($projection, $employment,
    $for_update = false, $allow_legacy_irregular = false)
{
    global $path_to_root;
    if (!is_array($projection) || !is_array($employment))
        return false;
    if (!hrm_assignment_position_column_ready())
        return $projection;
    if (!function_exists('get_hrm_verified_position_binding'))
        include_once($path_to_root.'/hrm/includes/db/position_db.inc');
    if (!function_exists('get_hrm_verified_position_binding') || empty($employment['legal_entity_id']))
        return false;
    $position_id = get_hrm_verified_position_binding(
        $projection['legacy_position_id'], (int)$employment['legal_entity_id'],
        $for_update, $allow_legacy_irregular
    );
    if ($position_id === false)
        return false;
    $projection['position_id'] = $position_id;
    return $projection;
}

/**
 * Bind or preserve an Assignment Job through one explicit administrator-owned
 * Job identity. New explicit targets must be active; an already-bound historic
 * target may be preserved after later inactivation. Current Position-to-Job
 * state is never inferred into Assignment history by this helper.
 */
function hrm_assignment_bind_job($projection, $employment, $job_id,
    $for_update = false, $require_active = true)
{
    global $path_to_root;
    if (!is_array($projection) || !is_array($employment))
        return false;
    if (!hrm_assignment_job_column_ready())
        return ($job_id === null || $job_id === '') ? $projection : false;
    if ($job_id === null || $job_id === '') {
        $projection['job_id'] = null;
        return $projection;
    }
    if (!function_exists('get_hrm_verified_assignment_job_binding'))
        include_once($path_to_root.'/hrm/includes/db/job_db.inc');
    if (!function_exists('get_hrm_verified_assignment_job_binding')
        || empty($employment['legal_entity_id']))
        return false;
    $verified = get_hrm_verified_assignment_job_binding(
        $job_id, (int)$employment['legal_entity_id'], $for_update, $require_active
    );
    if ($verified === false)
        return false;
    $projection['job_id'] = $verified;
    return $projection;
}

/**
 * Bind a pure legacy Assignment projection to its exact normalized Grade.
 * Older upgrade steps intentionally retain legacy_grade_id until 1.0.214.
 *
 * @param array $projection
 * @param array $employment
 * @param bool $for_update
 * @param bool $allow_legacy_irregular
 * @return array|false
 */
function hrm_assignment_bind_grade($projection, $employment,
    $for_update = false, $allow_legacy_irregular = false)
{
    global $path_to_root;
    if (!is_array($projection) || !is_array($employment))
        return false;
    if (!hrm_assignment_grade_column_ready())
        return $projection;
    if (!function_exists('get_hrm_verified_grade_binding'))
        include_once($path_to_root.'/hrm/includes/db/grade_db.inc');
    if (!function_exists('get_hrm_verified_grade_binding') || empty($employment['legal_entity_id']))
        return false;
    $grade_id = get_hrm_verified_grade_binding(
        $projection['legacy_grade_id'], (int)$employment['legal_entity_id'],
        $for_update, $allow_legacy_irregular
    );
    if ($grade_id === false)
        return false;
    $projection['grade_id'] = $grade_id;
    return $projection;
}

/**
 * Bind or preserve an Assignment Work Location through the reviewed explicit
 * administrator identity. New targets must be active; an already-bound historic
 * target may be preserved after later inactivation so ordinary edits/termination
 * never rewrite Assignment history.
 *
 * @param array $projection
 * @param array $employment
 * @param mixed $work_location_id
 * @param bool $for_update
 * @param bool $require_active
 * @return array|false
 */
function hrm_assignment_bind_work_location($projection, $employment, $work_location_id,
    $for_update = false, $require_active = true)
{
    global $path_to_root;
    if (!is_array($projection) || !is_array($employment))
        return false;
    if (!hrm_assignment_work_location_column_ready())
        return ($work_location_id === null || $work_location_id === '') ? $projection : false;
    if ($work_location_id === null || $work_location_id === '') {
        $projection['work_location_id'] = null;
        return $projection;
    }
    if (!function_exists('get_hrm_verified_assignment_work_location_binding'))
        include_once($path_to_root.'/hrm/includes/db/work_location_db.inc');
    if (!function_exists('get_hrm_verified_assignment_work_location_binding')
        || empty($employment['legal_entity_id']))
        return false;
    $verified = get_hrm_verified_assignment_work_location_binding(
        $work_location_id, (int)$employment['legal_entity_id'], $for_update, $require_active
    );
    if ($verified === false)
        return false;
    $projection['work_location_id'] = $verified;
    return $projection;
}

/**
 * Prove that one existing manager Assignment belongs to the same Legal Entity.
 * This preservation path deliberately does not require current active status:
 * a relationship already written by the reviewed effective-dated writer must
 * survive later manager inactivation/termination and ordinary Employee edits.
 *
 * @param array $projection
 * @param array $employment
 * @param mixed $manager_assignment_id
 * @param bool $for_update
 * @return array|false
 */
function hrm_assignment_preserve_manager_binding($projection, $employment,
    $manager_assignment_id, $for_update = false)
{
    if (!is_array($projection) || !is_array($employment))
        return false;
    if (!hrm_assignment_manager_column_ready())
        return ($manager_assignment_id === null || $manager_assignment_id === '') ? $projection : false;
    if ($manager_assignment_id === null || $manager_assignment_id === '') {
        $projection['manager_assignment_id'] = null;
        return $projection;
    }
    $manager_assignment_id = trim((string)$manager_assignment_id);
    if (preg_match('/^[1-9][0-9]*$/D', $manager_assignment_id) !== 1
        || empty($employment['legal_entity_id']))
        return false;
    $sql = "SELECT a.assignment_id,a.employment_id,h.legal_entity_id FROM ".TB_PREF."hrm_assignments a "
        ."INNER JOIN ".TB_PREF."hrm_employments h ON h.employment_id=a.employment_id "
        ."WHERE a.assignment_id=".(int)$manager_assignment_id." LIMIT 1";
    if ($for_update)
        $sql .= " FOR UPDATE";
    $result = db_query($sql, 'could not preserve Assignment manager binding');
    $row = $result ? db_fetch_assoc($result) : false;
    if (!$row || (int)$row['legal_entity_id'] !== (int)$employment['legal_entity_id']
        || (int)$row['employment_id'] === (int)$employment['employment_id'])
        return false;
    $projection['manager_assignment_id'] = (int)$row['assignment_id'];
    return $projection;
}

/**
 * Follow an existing manager chain under row locks. Reaching any Assignment
 * owned by the subordinate Employment would create a direct/indirect cycle.
 * Cross-Legal-Entity or already-cyclic/tampered chains fail closed.
 */
function hrm_assignment_manager_chain_is_safe($manager_assignment_id,
    $subordinate_employment_id, $legal_entity_id, $for_update = false)
{
    $manager_assignment_id = (int)$manager_assignment_id;
    $subordinate_employment_id = (int)$subordinate_employment_id;
    $legal_entity_id = (int)$legal_entity_id;
    if ($manager_assignment_id <= 0 || $subordinate_employment_id <= 0 || $legal_entity_id <= 0)
        return false;
    $visited = array();
    while ($manager_assignment_id > 0) {
        if (isset($visited[$manager_assignment_id]) || count($visited) >= 1000)
            return false;
        $visited[$manager_assignment_id] = true;
        $sql = "SELECT a.assignment_id,a.employment_id,a.manager_assignment_id,h.legal_entity_id "
            ."FROM ".TB_PREF."hrm_assignments a INNER JOIN ".TB_PREF."hrm_employments h "
            ."ON h.employment_id=a.employment_id WHERE a.assignment_id=".$manager_assignment_id." LIMIT 1";
        if ($for_update)
            $sql .= " FOR UPDATE";
        $result = db_query($sql, 'could not verify Assignment manager chain');
        $row = $result ? db_fetch_assoc($result) : false;
        if (!$row || (int)$row['legal_entity_id'] !== $legal_entity_id
            || (int)$row['employment_id'] === $subordinate_employment_id)
            return false;
        if ($row['manager_assignment_id'] === null || $row['manager_assignment_id'] === '')
            return true;
        if (!is_numeric($row['manager_assignment_id']) || (int)$row['manager_assignment_id'] <= 0)
            return false;
        $manager_assignment_id = (int)$row['manager_assignment_id'];
    }
    return false;
}

/**
 * Resolve an explicitly selected legacy reporting_to employee key to exactly
 * one primary manager Assignment interval at the requested effective date.
 * New targets must be currently active, same-Legal-Entity, non-self and free
 * of direct/indirect cycles. No Position hierarchy is consulted.
 *
 * @return int|false
 */
function get_hrm_verified_assignment_manager_target($manager_employee_id,
    $effective_date, $employment, $for_update = false)
{
    if (!hrm_assignment_manager_column_ready() || !is_array($employment)
        || empty($employment['employment_id']) || empty($employment['legal_entity_id']))
        return false;
    $manager_employee_id = trim((string)$manager_employee_id);
    $effective_date = hrm_assignment_normalize_effective_date($effective_date);
    if ($manager_employee_id === '' || strlen($manager_employee_id) > 20
        || $effective_date === false)
        return false;
    $legal_entity_id = (int)$employment['legal_entity_id'];
    $subordinate_employment_id = (int)$employment['employment_id'];
    $sql = "SELECT a.assignment_id,a.employment_id,a.legacy_employee_number,a.manager_assignment_id,"
        ."a.effective_from,a.effective_to,h.legal_entity_id,h.employment_status,h.start_date,h.end_date,"
        ."e.employee_number,e.employee_id,e.inactive FROM ".TB_PREF."employees e "
        ."INNER JOIN ".TB_PREF."hrm_employments h ON h.legacy_employee_number=e.employee_number AND h.legacy_current=1 "
        ."INNER JOIN ".TB_PREF."hrm_assignments a ON a.employment_id=h.employment_id "
        ."WHERE e.employee_id=".db_escape($manager_employee_id)." AND e.inactive=0 "
        ."AND h.legal_entity_id=".$legal_entity_id." AND h.employment_status='active' AND h.end_date IS NULL "
        ."AND (h.start_date IS NULL OR h.start_date<=".db_escape($effective_date).") "
        ."AND a.legacy_employee_number=e.employee_number AND a.assignment_type_code='legacy_primary' AND a.primary_flag=1 "
        ."AND (a.effective_from IS NULL OR a.effective_from<=".db_escape($effective_date).") "
        ."AND (a.effective_to IS NULL OR a.effective_to>=".db_escape($effective_date).") "
        ."ORDER BY a.assignment_id";
    if ($for_update)
        $sql .= " FOR UPDATE";
    $result = db_query($sql, 'could not resolve explicit Assignment manager target');
    if (!$result || db_num_rows($result) !== 1)
        return false;
    $row = db_fetch_assoc($result);
    if (!$row || (int)$row['employment_id'] === $subordinate_employment_id
        || (string)$row['employee_id'] !== $manager_employee_id
        || (int)$row['legal_entity_id'] !== $legal_entity_id)
        return false;
    $manager_assignment_id = (int)$row['assignment_id'];
    if ($manager_assignment_id <= 0 || !hrm_assignment_manager_chain_is_safe(
        $manager_assignment_id, $subordinate_employment_id, $legal_entity_id, $for_update
    ))
        return false;
    return $manager_assignment_id;
}

/**
 * Bind one explicit legacy reporting_to key to its reviewed manager Assignment
 * interval. Explicit unassignment is intentionally not introduced.
 */
function hrm_assignment_bind_manager($projection, $employment, $manager_employee_id,
    $effective_date, $for_update = false)
{
    if (!is_array($projection) || !is_array($employment))
        return false;
    $manager_employee_id = trim((string)$manager_employee_id);
    if ($manager_employee_id === '' || !isset($projection['legacy_reporting_to'])
        || (string)$projection['legacy_reporting_to'] !== $manager_employee_id)
        return false;
    $manager_assignment_id = get_hrm_verified_assignment_manager_target(
        $manager_employee_id, $effective_date, $employment, $for_update
    );
    if ($manager_assignment_id === false)
        return false;
    $projection['manager_assignment_id'] = (int)$manager_assignment_id;
    return $projection;
}

function get_hrm_legacy_current_assignment($employee_number, $for_update = false)
{
    if (!hrm_assignment_table_ready() || (int)$employee_number <= 0)
        return false;
    $organization_unit = hrm_assignment_organization_unit_column_ready()
        ? 'a.organization_unit_id' : 'NULL AS organization_unit_id';
    $position = hrm_assignment_position_column_ready()
        ? 'a.position_id' : 'NULL AS position_id';
    $job = hrm_assignment_job_column_ready()
        ? 'a.job_id' : 'NULL AS job_id';
    $grade = hrm_assignment_grade_column_ready()
        ? 'a.grade_id' : 'NULL AS grade_id';
    $work_location = hrm_assignment_work_location_column_ready()
        ? 'a.work_location_id' : 'NULL AS work_location_id';
    $manager_assignment = hrm_assignment_manager_column_ready()
        ? 'a.manager_assignment_id' : 'NULL AS manager_assignment_id';
    $sql = "SELECT a.assignment_id,a.employment_id,a.assignment_sequence,a.legacy_employee_number,"
        ."a.legacy_current,a.assignment_type_code,a.primary_flag,a.legacy_department_id,"
        .$organization_unit.","
        ."a.legacy_position_id,".$position.",".$job.",".$work_location.",".$manager_assignment.",a.legacy_grade_id,".$grade.",a.legacy_shift_id,a.legacy_reporting_to,"
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


/**
 * Inclusive Assignment interval overlap. NULL starts are explicit unknown
 * history (past-unbounded) and NULL ends are open current intervals. Invalid
 * dates fail closed as overlapping.
 */
function hrm_assignment_intervals_overlap($left_from, $left_to, $right_from, $right_to)
{
    $left_from = ($left_from === null || $left_from === '') ? null : hrm_assignment_normalize_effective_date($left_from);
    $left_to = ($left_to === null || $left_to === '') ? null : hrm_assignment_normalize_effective_date($left_to);
    $right_from = ($right_from === null || $right_from === '') ? null : hrm_assignment_normalize_effective_date($right_from);
    $right_to = ($right_to === null || $right_to === '') ? null : hrm_assignment_normalize_effective_date($right_to);
    if ($left_from === false || $left_to === false || $right_from === false || $right_to === false)
        return true;
    return ($left_to === null || $right_from === null || strcmp($right_from, $left_to) <= 0)
        && ($right_to === null || $left_from === null || strcmp($left_from, $right_to) <= 0);
}

/** @return bool */
function hrm_assignment_interval_rows_are_non_overlapping($rows)
{
    if (!is_array($rows))
        return false;
    $count = count($rows);
    for ($i = 0; $i < $count; $i++) {
        if (!is_array($rows[$i]) || empty($rows[$i]['assignment_id']))
            return false;
        $raw_from = isset($rows[$i]['effective_from']) ? $rows[$i]['effective_from'] : null;
        $raw_to = isset($rows[$i]['effective_to']) ? $rows[$i]['effective_to'] : null;
        $from = ($raw_from === null || $raw_from === '') ? null : hrm_assignment_normalize_effective_date($raw_from);
        $to = ($raw_to === null || $raw_to === '') ? null : hrm_assignment_normalize_effective_date($raw_to);
        if ($from === false || $to === false)
            return false;
        // Preserve one accepted legacy-invalid shadow interval, but deny it
        // once multiple Assignment episodes would share the Employment.
        if ($count > 1 && $from !== null && $to !== null && strcmp($to, $from) < 0)
            return false;
        for ($j = $i + 1; $j < $count; $j++) {
            if (!is_array($rows[$j]) || empty($rows[$j]['assignment_id']))
                return false;
            if (hrm_assignment_intervals_overlap(
                $raw_from, $raw_to,
                isset($rows[$j]['effective_from']) ? $rows[$j]['effective_from'] : null,
                isset($rows[$j]['effective_to']) ? $rows[$j]['effective_to'] : null
            ))
                return false;
        }
    }
    return true;
}

/**
 * Lock all Assignment rows for one Employment and prove pairwise chronology.
 * An optional replacement validates a pending update before it is written.
 */
function hrm_assignment_employment_interval_policy_allows($employment_id,
    $replacement_assignment_id = 0, $replacement_from = null, $replacement_to = null)
{
    $employment_id = (int)$employment_id;
    $replacement_assignment_id = (int)$replacement_assignment_id;
    if ($employment_id <= 0)
        return false;
    $result = db_query("SELECT assignment_id,effective_from,effective_to FROM ".TB_PREF."hrm_assignments "
        ."WHERE employment_id=".$employment_id." ORDER BY assignment_id FOR UPDATE",
        'could not verify HRM Assignment interval chronology');
    if (!$result)
        return false;
    $rows = array();
    $replaced = false;
    while ($row = db_fetch_assoc($result)) {
        if ($replacement_assignment_id > 0 && (int)$row['assignment_id'] === $replacement_assignment_id) {
            $row['effective_from'] = $replacement_from;
            $row['effective_to'] = $replacement_to;
            $replaced = true;
        }
        $rows[] = $row;
    }
    if ($replacement_assignment_id > 0 && !$replaced)
        return false;
    return hrm_assignment_interval_rows_are_non_overlapping($rows);
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
    $integer_fields = array('employment_id','legacy_employee_number','legacy_department_id','organization_unit_id',
        'legacy_position_id','position_id','job_id','work_location_id','manager_assignment_id','legacy_grade_id','grade_id','legacy_shift_id','legacy_cost_center_id','legacy_dimension2_id');
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

function execute_hrm_assignment_effective_change($employee, $effective_date, $change_type, $requested_work_location_id = null, $work_location_explicit = false, $requested_job_id = null, $job_explicit = false, $requested_manager_employee_id = null, $manager_explicit = false)
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
    if (!hrm_assignment_employment_interval_policy_allows((int)$employment['employment_id']))
        return false;

    $start = hrm_employment_normalize_date(isset($employment['start_date']) ? $employment['start_date'] : null);
    $end = hrm_employment_normalize_date(isset($employment['end_date']) ? $employment['end_date'] : null);
    if ($start === false || $end === false || ($start !== null && strcmp($effective_date, $start) < 0)
        || ($end !== null && strcmp($effective_date, $end) > 0))
        return false;

    $allow_legacy_irregular = (string)$current['reference_quality'] === 'legacy_irregular';
    $projection = hrm_assignment_projection_from_employee($employee, $employment, $allow_legacy_irregular);
    $projection = hrm_assignment_bind_organization_unit(
        $projection, $employment, true, $allow_legacy_irregular
    );
    $projection = hrm_assignment_bind_position(
        $projection, $employment, true, $allow_legacy_irregular
    );
    $projection = hrm_assignment_bind_grade(
        $projection, $employment, true, $allow_legacy_irregular
    );
    if ($job_explicit)
        $projection = hrm_assignment_bind_job(
            $projection, $employment, $requested_job_id, true, true
        );
    else
        $projection = hrm_assignment_bind_job(
            $projection, $employment, isset($current['job_id']) ? $current['job_id'] : null, true, false
        );
    if ($work_location_explicit)
        $projection = hrm_assignment_bind_work_location(
            $projection, $employment, $requested_work_location_id, true, true
        );
    else
        $projection = hrm_assignment_bind_work_location(
            $projection, $employment, isset($current['work_location_id']) ? $current['work_location_id'] : null, true, false
        );
    if ($manager_explicit)
        $projection = hrm_assignment_bind_manager(
            $projection, $employment, $requested_manager_employee_id, $effective_date, true
        );
    else
        $projection = hrm_assignment_preserve_manager_binding(
            $projection, $employment, isset($current['manager_assignment_id']) ? $current['manager_assignment_id'] : null, true
        );
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
        'legacy_department_id'=>'department','organization_unit_id'=>'organization_unit',
        'legacy_position_id'=>'position','position_id'=>'normalized_position','job_id'=>'job','work_location_id'=>'work_location',
        'manager_assignment_id'=>'manager_assignment','legacy_grade_id'=>'grade','grade_id'=>'normalized_grade',
        'legacy_shift_id'=>'shift','legacy_reporting_to'=>'reporting','legacy_cost_center_id'=>'cost_center',
        'legacy_dimension2_id'=>'dimension2','reference_quality'=>'reference_quality'
    );
    foreach ($map as $field=>$label) {
        $equal = in_array($field, array('legacy_department_id','organization_unit_id','legacy_position_id','position_id','job_id','work_location_id','manager_assignment_id','legacy_grade_id','grade_id',
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
    $organization_column = hrm_assignment_organization_unit_column_ready()
        ? ',organization_unit_id' : '';
    $organization_value = hrm_assignment_organization_unit_column_ready()
        ? ','.($projection['organization_unit_id'] === null ? 'NULL' : (int)$projection['organization_unit_id']) : '';
    $position_column = hrm_assignment_position_column_ready() ? ',position_id' : '';
    $position_value = hrm_assignment_position_column_ready()
        ? ','.($projection['position_id'] === null ? 'NULL' : (int)$projection['position_id']) : '';
    $job_column = hrm_assignment_job_column_ready() ? ',job_id' : '';
    $job_value = hrm_assignment_job_column_ready()
        ? ','.($projection['job_id'] === null ? 'NULL' : (int)$projection['job_id']) : '';
    $work_location_column = hrm_assignment_work_location_column_ready() ? ',work_location_id' : '';
    $work_location_value = hrm_assignment_work_location_column_ready()
        ? ','.($projection['work_location_id'] === null ? 'NULL' : (int)$projection['work_location_id']) : '';
    $manager_column = hrm_assignment_manager_column_ready() ? ',manager_assignment_id' : '';
    $manager_value = hrm_assignment_manager_column_ready()
        ? ','.($projection['manager_assignment_id'] === null ? 'NULL' : (int)$projection['manager_assignment_id']) : '';
    $grade_column = hrm_assignment_grade_column_ready() ? ',grade_id' : '';
    $grade_value = hrm_assignment_grade_column_ready()
        ? ','.($projection['grade_id'] === null ? 'NULL' : (int)$projection['grade_id']) : '';
    $sql = "INSERT INTO ".TB_PREF."hrm_assignments (employment_id,assignment_sequence,legacy_employee_number,"
        ."legacy_current,assignment_type_code,primary_flag,legacy_department_id".$organization_column.",legacy_position_id".$position_column.$job_column.$work_location_column.$manager_column.",legacy_grade_id".$grade_column.","
        ."legacy_shift_id,legacy_reporting_to,legacy_cost_center_id,legacy_dimension2_id,reference_quality,"
        ."effective_from,effective_to,interval_quality,source,created_by) VALUES ("
        .(int)$projection['employment_id'].",".$next_sequence.",".$employee_number.",1,'legacy_primary',1,"
        .(int)$projection['legacy_department_id'].$organization_value.",".(int)$projection['legacy_position_id'].$position_value.$job_value.$work_location_value.$manager_value.",".(int)$projection['legacy_grade_id'].$grade_value.","
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
    if (!hrm_assignment_employment_interval_policy_allows((int)$employment['employment_id']))
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
    if (!hrm_assignment_employment_interval_policy_allows((int)$employment['employment_id']))
        return false;

    $allow_legacy_irregular = (string)$current['reference_quality'] === 'legacy_irregular';
    $projection = hrm_assignment_projection_from_employee($employee, $employment, $allow_legacy_irregular);
    $projection = hrm_assignment_bind_organization_unit(
        $projection, $employment, true, $allow_legacy_irregular
    );
    $projection = hrm_assignment_bind_position(
        $projection, $employment, true, $allow_legacy_irregular
    );
    $projection = hrm_assignment_bind_grade(
        $projection, $employment, true, $allow_legacy_irregular
    );
    $projection = hrm_assignment_bind_job(
        $projection, $employment, isset($current['job_id']) ? $current['job_id'] : null, true, false
    );
    $projection = hrm_assignment_bind_work_location(
        $projection, $employment, isset($current['work_location_id']) ? $current['work_location_id'] : null, true, false
    );
    if (!$projection)
        return false;
    $projection['effective_from'] = ($current['effective_from'] === null || $current['effective_from'] === '')
        ? null : (string)$current['effective_from'];
    $projection['effective_to'] = $effective_date;

    if ($projection['effective_from'] !== null
        && strcmp($effective_date, (string)$projection['effective_from']) < 0)
        return false;

    $integer_fields = array('legacy_department_id','organization_unit_id','legacy_position_id','position_id','job_id','work_location_id','legacy_grade_id','grade_id','legacy_shift_id',
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
    if (!hrm_assignment_employment_interval_policy_allows(
        (int)$employment['employment_id'], (int)$current['assignment_id'],
        $projection['effective_from'], $effective_date
    ))
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
    $scope = db_query("SELECT legal_entity_id,legacy_employee_number FROM ".TB_PREF."hrm_employments "
        ."WHERE employment_id=".(int)$employment_id." LIMIT 1",
        'could not resolve HRM Assignment Legal Entity audit scope');
    $scope = $scope ? db_fetch_assoc($scope) : false;
    if (!$scope || (int)$scope['legacy_employee_number'] !== (int)$employee_number)
        return false;
    $changed_fields = array_values(array_unique($changed_fields));
    sort($changed_fields);
    return append_domain_audit_event(array(
        'company_id' => function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id' => (int)$scope['legal_entity_id'] > 0 ? (int)$scope['legal_entity_id'] : 0,
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
    $projection = hrm_assignment_bind_organization_unit(
        $projection, $employment, true, $allow_legacy_irregular
    );
    $projection = hrm_assignment_bind_position(
        $projection, $employment, true, $allow_legacy_irregular
    );
    $projection = hrm_assignment_bind_grade(
        $projection, $employment, true, $allow_legacy_irregular
    );
    // No trusted hire-time Job or manager-Assignment source exists. Current
    // Position Job state and legacy reporting_to are not automatic history.
    $projection = hrm_assignment_bind_job($projection, $employment, null, true, false);
    $projection = hrm_assignment_bind_work_location($projection, $employment, null, true, false);
    $projection = hrm_assignment_preserve_manager_binding($projection, $employment, null, true);
    if (!$projection || get_hrm_legacy_current_assignment($employee_number, true))
        return false;
    if (!hrm_assignment_employment_interval_policy_allows((int)$employment['employment_id']))
        return false;

    $actor = hrm_assignment_actor_id();
    $organization_column = hrm_assignment_organization_unit_column_ready()
        ? ',organization_unit_id' : '';
    $organization_value = hrm_assignment_organization_unit_column_ready()
        ? ','.($projection['organization_unit_id'] === null ? 'NULL' : (int)$projection['organization_unit_id']) : '';
    $position_column = hrm_assignment_position_column_ready() ? ',position_id' : '';
    $position_value = hrm_assignment_position_column_ready()
        ? ','.($projection['position_id'] === null ? 'NULL' : (int)$projection['position_id']) : '';
    $job_column = hrm_assignment_job_column_ready() ? ',job_id' : '';
    $job_value = hrm_assignment_job_column_ready()
        ? ','.($projection['job_id'] === null ? 'NULL' : (int)$projection['job_id']) : '';
    $work_location_column = hrm_assignment_work_location_column_ready() ? ',work_location_id' : '';
    $work_location_value = hrm_assignment_work_location_column_ready()
        ? ','.($projection['work_location_id'] === null ? 'NULL' : (int)$projection['work_location_id']) : '';
    $manager_column = hrm_assignment_manager_column_ready() ? ',manager_assignment_id' : '';
    $manager_value = hrm_assignment_manager_column_ready()
        ? ','.($projection['manager_assignment_id'] === null ? 'NULL' : (int)$projection['manager_assignment_id']) : '';
    $grade_column = hrm_assignment_grade_column_ready() ? ',grade_id' : '';
    $grade_value = hrm_assignment_grade_column_ready()
        ? ','.($projection['grade_id'] === null ? 'NULL' : (int)$projection['grade_id']) : '';
    $sql = "INSERT INTO ".TB_PREF."hrm_assignments (employment_id,assignment_sequence,legacy_employee_number,"
        ."legacy_current,assignment_type_code,primary_flag,legacy_department_id".$organization_column.",legacy_position_id".$position_column.$job_column.$work_location_column.$manager_column.",legacy_grade_id".$grade_column.","
        ."legacy_shift_id,legacy_reporting_to,legacy_cost_center_id,legacy_dimension2_id,reference_quality,"
        ."effective_from,effective_to,interval_quality,source,created_by) VALUES ("
        .(int)$projection['employment_id'].",1,".$employee_number.",1,'legacy_primary',1,"
        .(int)$projection['legacy_department_id'].$organization_value.",".(int)$projection['legacy_position_id'].$position_value.$job_value.$work_location_value.$manager_value.",".(int)$projection['legacy_grade_id'].$grade_value.","
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
    if (!hrm_assignment_employment_interval_policy_allows((int)$employment['employment_id']))
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
    if (!hrm_assignment_employment_interval_policy_allows((int)$employment['employment_id']))
        return false;

    $allow_legacy_irregular = (string)$current['reference_quality'] === 'legacy_irregular';
    $projection = hrm_assignment_projection_from_employee($employee, $employment, $allow_legacy_irregular);
    $projection = hrm_assignment_bind_organization_unit(
        $projection, $employment, true, $allow_legacy_irregular
    );
    $projection = hrm_assignment_bind_position(
        $projection, $employment, true, $allow_legacy_irregular
    );
    $projection = hrm_assignment_bind_grade(
        $projection, $employment, true, $allow_legacy_irregular
    );
    $projection = hrm_assignment_bind_job(
        $projection, $employment, isset($current['job_id']) ? $current['job_id'] : null, true, false
    );
    $projection = hrm_assignment_bind_work_location(
        $projection, $employment, isset($current['work_location_id']) ? $current['work_location_id'] : null, true, false
    );
    $projection = hrm_assignment_preserve_manager_binding(
        $projection, $employment, isset($current['manager_assignment_id']) ? $current['manager_assignment_id'] : null, true
    );
    if (!$projection)
        return false;
    if ((int)$current['assignment_sequence'] > 1)
        $projection['effective_from'] = ($current['effective_from'] === null || $current['effective_from'] === '')
            ? null : (string)$current['effective_from'];
    $integer_fields = array('legacy_department_id','organization_unit_id','legacy_position_id','position_id','job_id','work_location_id','manager_assignment_id','legacy_grade_id','grade_id','legacy_shift_id',
        'legacy_cost_center_id','legacy_dimension2_id');
    if ($allow_legacy_irregular) {
        foreach ($integer_fields as $field) {
            if (!hrm_assignment_nullable_int_equal($current[$field], $projection[$field]))
                return false;
        }
    }

    $changed = array();
    $map = array(
        'legacy_department_id' => 'department', 'organization_unit_id' => 'organization_unit',
        'legacy_position_id' => 'position', 'position_id' => 'normalized_position', 'job_id' => 'job', 'work_location_id' => 'work_location',
        'manager_assignment_id' => 'manager_assignment', 'legacy_grade_id' => 'grade', 'grade_id' => 'normalized_grade',
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
    if (!hrm_assignment_employment_interval_policy_allows(
        (int)$employment['employment_id'], (int)$current['assignment_id'],
        $projection['effective_from'], $projection['effective_to']
    ))
        return false;

    $actor = hrm_assignment_actor_id();
    $organization_update = hrm_assignment_organization_unit_column_ready()
        ? ",organization_unit_id=".($projection['organization_unit_id'] === null
            ? 'NULL' : (int)$projection['organization_unit_id']) : '';
    $position_update = hrm_assignment_position_column_ready()
        ? ",position_id=".($projection['position_id'] === null ? 'NULL' : (int)$projection['position_id']) : '';
    $job_update = hrm_assignment_job_column_ready()
        ? ",job_id=".($projection['job_id'] === null ? 'NULL' : (int)$projection['job_id']) : '';
    $work_location_update = hrm_assignment_work_location_column_ready()
        ? ",work_location_id=".($projection['work_location_id'] === null ? 'NULL' : (int)$projection['work_location_id']) : '';
    $manager_update = hrm_assignment_manager_column_ready()
        ? ",manager_assignment_id=".($projection['manager_assignment_id'] === null ? 'NULL' : (int)$projection['manager_assignment_id']) : '';
    $grade_update = hrm_assignment_grade_column_ready()
        ? ",grade_id=".($projection['grade_id'] === null ? 'NULL' : (int)$projection['grade_id']) : '';
    $sql = "UPDATE ".TB_PREF."hrm_assignments SET legacy_department_id=".(int)$projection['legacy_department_id']
        .$organization_update
        .",legacy_position_id=".(int)$projection['legacy_position_id']
        .$position_update
        .$job_update
        .$work_location_update
        .$manager_update
        .",legacy_grade_id=".(int)$projection['legacy_grade_id']
        .$grade_update
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
        $work_location_explicit = array_key_exists('work_location_id', $effective_change);
        $work_location_id = $work_location_explicit ? $effective_change['work_location_id'] : null;
        $job_explicit = array_key_exists('job_id', $effective_change);
        $job_id = $job_explicit ? $effective_change['job_id'] : null;
        $manager_explicit = array_key_exists('manager_employee_id', $effective_change);
        $manager_employee_id = $manager_explicit ? $effective_change['manager_employee_id'] : null;
        return execute_hrm_assignment_effective_change(
            $employee, $effective_date, $change_type, $work_location_id, $work_location_explicit,
            $job_id, $job_explicit, $manager_employee_id, $manager_explicit
        );
    }
    return synchronize_hrm_assignment_from_employee($employee);
}
