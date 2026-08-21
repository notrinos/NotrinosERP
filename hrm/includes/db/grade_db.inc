<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

/**
 * HRM-FND-003 bounded Grade compatibility foundation.
 *
 * Legacy pay_grades is the only currently verified Grade identity source.
 * Salary ranges remain legacy compensation metadata and are deliberately not
 * copied into the normalized Grade object. Job, Position, Assignment binding,
 * occupancy/headcount, costing and manager semantics are not inferred here.
 */

/** @return bool */
function hrm_grade_table_ready()
{
    if (!function_exists('db_query') || !function_exists('db_escape_function') || !defined('TB_PREF'))
        return false;
    $result = db_query("SHOW TABLES LIKE '".TB_PREF.db_escape_function('hrm_grades')."'");
    return $result && db_num_rows($result) === 1;
}

/**
 * Normalize one legacy pay grade into the exact non-compensation projection.
 *
 * @param array $grade
 * @param int $legal_entity_id
 * @return array|false
 */
function hrm_grade_projection_from_pay_grade($grade, $legal_entity_id)
{
    if (!is_array($grade) || (int)$legal_entity_id <= 0)
        return false;
    $legacy_grade_id = isset($grade['grade_id']) ? (int)$grade['grade_id'] : 0;
    $name = isset($grade['grade_name']) ? trim((string)$grade['grade_name']) : '';
    if ($legacy_grade_id <= 0 || $name === '' || strlen($name) > 60)
        return false;

    $legacy_code = isset($grade['grade_code']) ? trim((string)$grade['grade_code']) : '';
    if (strlen($legacy_code) > 20)
        return false;
    if ($legacy_code === '')
        $legacy_code = null;

    $level_raw = isset($grade['grade_level']) ? trim((string)$grade['grade_level']) : '0';
    if ($level_raw === '' || !preg_match('/^-?[0-9]+$/', $level_raw))
        return false;
    $level = (int)$level_raw;

    $inactive = isset($grade['inactive']) ? (int)$grade['inactive'] : 0;
    if ($inactive !== 0 && $inactive !== 1)
        return false;

    return array(
        'legal_entity_id'=>(int)$legal_entity_id,
        'legacy_grade_id'=>$legacy_grade_id,
        'grade_code'=>'GRADE-'.$legacy_grade_id,
        'legacy_grade_code'=>$legacy_code,
        'grade_name'=>$name,
        'grade_level'=>$level,
        'grade_status'=>$inactive ? 'inactive' : 'active',
        'reference_quality'=>'legacy_pay_grade_only',
        'source'=>'legacy_pay_grade_seed',
    );
}

/** @return array|false|null */
function get_hrm_grade_by_legacy_grade($legacy_grade_id, $for_update = false)
{
    if (!hrm_grade_table_ready() || (int)$legacy_grade_id <= 0)
        return false;
    $sql = "SELECT grade_id,legal_entity_id,legacy_grade_id,grade_code,legacy_grade_code,grade_name,"
        ."grade_level,grade_status,reference_quality,source,created_at,created_by,updated_at,updated_by "
        ."FROM ".TB_PREF."hrm_grades WHERE legacy_grade_id=".(int)$legacy_grade_id;
    if ($for_update)
        $sql .= ' FOR UPDATE';
    $result = db_query($sql, 'could not read HRM Grade compatibility row');
    if (!$result || db_num_rows($result) > 1)
        return false;
    $row = db_fetch_assoc($result);
    return $row ? $row : null;
}

/** @return bool */
function hrm_grade_row_matches_projection($row, $projection)
{
    if (!is_array($row) || !is_array($projection))
        return false;
    $left_code = ($row['legacy_grade_code'] === null || $row['legacy_grade_code'] === '')
        ? null : (string)$row['legacy_grade_code'];
    $right_code = ($projection['legacy_grade_code'] === null || $projection['legacy_grade_code'] === '')
        ? null : (string)$projection['legacy_grade_code'];
    return (int)$row['legal_entity_id'] === (int)$projection['legal_entity_id']
        && (int)$row['legacy_grade_id'] === (int)$projection['legacy_grade_id']
        && (string)$row['grade_code'] === (string)$projection['grade_code']
        && $left_code === $right_code
        && (string)$row['grade_name'] === (string)$projection['grade_name']
        && (int)$row['grade_level'] === (int)$projection['grade_level']
        && (string)$row['grade_status'] === (string)$projection['grade_status']
        && (string)$row['reference_quality'] === 'legacy_pay_grade_only'
        && (string)$row['source'] === 'legacy_pay_grade_seed';
}

/**
 * Load one legacy Pay Grade row using the normalized writer's lock path.
 * Salary range fields remain legacy compensation metadata and are not projected.
 *
 * @param int $legacy_grade_id
 * @param bool $for_update
 * @return array|false|null
 */
function get_hrm_pay_grade_for_grade_binding($legacy_grade_id, $for_update = false)
{
    if ((int)$legacy_grade_id <= 0)
        return false;
    $sql = "SELECT grade_id,grade_code,grade_name,grade_level,inactive FROM ".TB_PREF."pay_grades "
        ."WHERE grade_id=".(int)$legacy_grade_id;
    if ($for_update)
        $sql .= ' FOR UPDATE';
    $result = db_query($sql, 'could not read legacy Pay Grade for normalized Grade binding');
    if (!$result || db_num_rows($result) > 1)
        return false;
    $row = db_fetch_assoc($result);
    return $row ? $row : null;
}

/**
 * Resolve an exact legacy-Pay-Grade-backed normalized Grade under persisted
 * Employment Legal Entity scope. Zero and accepted negative irregular legacy
 * references remain explicit unknown/unbound state.
 *
 * @param mixed $legacy_grade_id
 * @param int $legal_entity_id
 * @param bool $for_update
 * @param bool $allow_legacy_irregular
 * @return int|false|null
 */
function get_hrm_verified_grade_binding($legacy_grade_id, $legal_entity_id,
    $for_update = false, $allow_legacy_irregular = false)
{
    if (!is_numeric($legacy_grade_id)
        || (string)(int)$legacy_grade_id !== trim((string)$legacy_grade_id)
        || (int)$legal_entity_id <= 0)
        return false;
    $legacy_grade_id = (int)$legacy_grade_id;
    if ($legacy_grade_id === 0 || ($legacy_grade_id < 0 && $allow_legacy_irregular))
        return null;
    if ($legacy_grade_id < 0 || !hrm_grade_table_ready())
        return false;
    $legacy = get_hrm_pay_grade_for_grade_binding($legacy_grade_id, $for_update);
    $grade = get_hrm_grade_by_legacy_grade($legacy_grade_id, $for_update);
    if (!is_array($legacy) || !is_array($grade))
        return false;
    $projection = hrm_grade_projection_from_pay_grade($legacy, $legal_entity_id);
    if (!$projection || !hrm_grade_row_matches_projection($grade, $projection))
        return false;
    return (int)$grade['grade_id'] > 0 ? (int)$grade['grade_id'] : false;
}

/**
 * Append restricted-value-free evidence for one legacy Pay Grade writer sync.
 * Grade names/codes and salary values are deliberately excluded.
 *
 * @param string $action
 * @param int $grade_id
 * @param int $legacy_grade_id
 * @param int $legal_entity_id
 * @param array $changed_fields
 * @param string $reason_code
 * @return array|false
 */
function append_hrm_grade_writer_audit($action, $grade_id, $legacy_grade_id,
    $legal_entity_id, $changed_fields, $reason_code)
{
    if (!function_exists('append_domain_audit_event') || (int)$grade_id <= 0
        || (int)$legacy_grade_id <= 0 || (int)$legal_entity_id <= 0
        || !is_array($changed_fields) || trim((string)$action) === ''
        || trim((string)$reason_code) === '')
        return false;
    $scope = get_hrm_grade_by_legacy_grade($legacy_grade_id, true);
    if (!is_array($scope) || (int)$scope['grade_id'] !== (int)$grade_id
        || (int)$scope['legal_entity_id'] !== (int)$legal_entity_id)
        return false;
    $changed_fields = array_values(array_unique($changed_fields));
    sort($changed_fields);
    return append_domain_audit_event(array(
        'company_id'=>function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id'=>(int)$scope['legal_entity_id'],
        'entity_type'=>'hrm_grade',
        'entity_key'=>array(
            'grade_id'=>(int)$grade_id,
            'legacy_grade_id'=>(int)$legacy_grade_id,
        ),
        'action'=>(string)$action,
        'reason_code'=>(string)$reason_code,
        'before'=>null,
        'after'=>array('legacy_pay_grade_projection_synchronized'=>true),
        'restricted_diff'=>array(
            'changed_fields'=>$changed_fields,
            'salary_ranges_copied'=>false,
            'restricted_values_stored'=>false,
        ),
    ));
}

/**
 * Insert or update one exact legacy-Pay-Grade-backed Grade projection.
 * Caller owns authorization and surrounding transaction.
 *
 * @param int $legacy_grade_id
 * @param array $changed_fields
 * @param string $operation
 * @return bool
 */
function synchronize_hrm_grade_from_pay_grade($legacy_grade_id, $changed_fields, $operation)
{
    global $path_to_root;
    if ((int)$legacy_grade_id <= 0 || !is_array($changed_fields)
        || trim((string)$operation) === '' || !hrm_grade_table_ready())
        return false;
    if (!function_exists('get_hrm_default_legal_entity_binding_id'))
        include_once($path_to_root.'/hrm/includes/db/legal_entity_db.inc');
    if (!function_exists('append_domain_audit_event'))
        include_once($path_to_root.'/includes/db/domain_audit_db.inc');
    $legal_entity_id = get_hrm_default_legal_entity_binding_id(true);
    if ($legal_entity_id === false)
        return false;
    $legacy = get_hrm_pay_grade_for_grade_binding((int)$legacy_grade_id, true);
    if (!is_array($legacy))
        return false;
    $projection = hrm_grade_projection_from_pay_grade($legacy, (int)$legal_entity_id);
    if (!$projection)
        return false;
    $current = get_hrm_grade_by_legacy_grade((int)$legacy_grade_id, true);
    $actor = function_exists('user_id') ? (int)user_id() : 0;
    if ($current === null) {
        if ((string)$operation !== 'create')
            return false;
        $sql = "INSERT INTO ".TB_PREF."hrm_grades (legal_entity_id,legacy_grade_id,grade_code,"
            ."legacy_grade_code,grade_name,grade_level,grade_status,reference_quality,source,created_by) VALUES ("
            .(int)$projection['legal_entity_id'].",".(int)$projection['legacy_grade_id'].","
            .db_escape($projection['grade_code']).",".($projection['legacy_grade_code'] === null
                ? 'NULL' : db_escape($projection['legacy_grade_code'])).",".db_escape($projection['grade_name']).","
            .(int)$projection['grade_level'].",".db_escape($projection['grade_status']).","
            ."'legacy_pay_grade_only','legacy_pay_grade_seed',".(int)$actor.")";
        if (!db_query($sql, 'could not synchronize new normalized Grade from legacy Pay Grade'))
            return false;
        $current = get_hrm_grade_by_legacy_grade((int)$legacy_grade_id, true);
    } else {
        if (!is_array($current) || (int)$current['legal_entity_id'] !== (int)$legal_entity_id)
            return false;
        $sql = "UPDATE ".TB_PREF."hrm_grades SET legacy_grade_code="
            .($projection['legacy_grade_code'] === null ? 'NULL' : db_escape($projection['legacy_grade_code']))
            .",grade_name=".db_escape($projection['grade_name']).",grade_level=".(int)$projection['grade_level']
            .",grade_status=".db_escape($projection['grade_status'])
            .",updated_at=NOW(),updated_by=".(int)$actor
            ." WHERE grade_id=".(int)$current['grade_id'];
        if (!db_query($sql, 'could not synchronize normalized Grade from legacy Pay Grade'))
            return false;
        $current = get_hrm_grade_by_legacy_grade((int)$legacy_grade_id, true);
    }
    if (!is_array($current) || !hrm_grade_row_matches_projection($current, $projection))
        return false;
    return append_hrm_grade_writer_audit(
        'hrm.grade.legacy_pay_grade_synchronized', (int)$current['grade_id'],
        (int)$legacy_grade_id, (int)$legal_entity_id, $changed_fields,
        'hrm_fnd_003_pay_grade_writer_'.$operation
    ) !== false;
}
