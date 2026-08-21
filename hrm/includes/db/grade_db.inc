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
