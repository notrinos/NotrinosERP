<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

/**
 * HRM-FND-003 bounded Position compatibility foundation.
 *
 * The legacy positions row is accepted only as current Position identity and
 * active/inactive evidence. Job class, Department, reporting hierarchy,
 * compensation, headcount, manager and description semantics remain legacy
 * metadata and are deliberately not copied into the normalized Position.
 */

/** @return bool */
function hrm_position_table_ready()
{
    if (!function_exists('db_query') || !function_exists('db_escape_function') || !defined('TB_PREF'))
        return false;
    $result = db_query("SHOW TABLES LIKE '".TB_PREF.db_escape_function('hrm_positions')."'");
    return $result && db_num_rows($result) === 1;
}

/**
 * Normalize one legacy positions row into identity/status only.
 *
 * @param array $position
 * @param int $legal_entity_id
 * @return array|false
 */
function hrm_position_projection_from_legacy_position($position, $legal_entity_id)
{
    if (!is_array($position) || (int)$legal_entity_id <= 0)
        return false;
    $legacy_position_id = isset($position['position_id']) ? (int)$position['position_id'] : 0;
    $name = isset($position['position_name']) ? trim((string)$position['position_name']) : '';
    if ($legacy_position_id <= 0 || $name === '')
        return false;

    $legacy_code = isset($position['position_code']) ? trim((string)$position['position_code']) : '';
    if (strlen($legacy_code) > 20)
        return false;
    if ($legacy_code === '')
        $legacy_code = null;

    $inactive = isset($position['inactive']) ? (int)$position['inactive'] : 0;
    if ($inactive !== 0 && $inactive !== 1)
        return false;

    return array(
        'legal_entity_id'=>(int)$legal_entity_id,
        'legacy_position_id'=>$legacy_position_id,
        'position_code'=>'POSITION-'.$legacy_position_id,
        'legacy_position_code'=>$legacy_code,
        'position_name'=>$name,
        'position_status'=>$inactive ? 'inactive' : 'active',
        'reference_quality'=>'legacy_position_identity_only',
        'source'=>'legacy_position_seed',
    );
}

/**
 * Read one normalized Position by its immutable legacy compatibility key.
 *
 * @param int $legacy_position_id
 * @param bool $for_update
 * @return array|false|null
 */
function get_hrm_position_by_legacy_position($legacy_position_id, $for_update = false)
{
    if (!hrm_position_table_ready() || (int)$legacy_position_id <= 0)
        return false;
    $sql = "SELECT position_id,legal_entity_id,legacy_position_id,position_code,legacy_position_code,"
        ."position_name,position_status,reference_quality,source,created_at,created_by,updated_at,updated_by "
        ."FROM ".TB_PREF."hrm_positions WHERE legacy_position_id=".(int)$legacy_position_id;
    if ($for_update)
        $sql .= ' FOR UPDATE';
    $result = db_query($sql, 'could not read HRM Position compatibility row');
    if (!$result || db_num_rows($result) > 1)
        return false;
    $row = db_fetch_assoc($result);
    return $row ? $row : null;
}

/**
 * Verify an exact identity/status-only Position projection.
 *
 * @param array $row
 * @param array $projection
 * @return bool
 */
function hrm_position_row_matches_projection($row, $projection)
{
    if (!is_array($row) || !is_array($projection))
        return false;
    $left_code = ($row['legacy_position_code'] === null || $row['legacy_position_code'] === '')
        ? null : (string)$row['legacy_position_code'];
    $right_code = ($projection['legacy_position_code'] === null || $projection['legacy_position_code'] === '')
        ? null : (string)$projection['legacy_position_code'];
    return (int)$row['legal_entity_id'] === (int)$projection['legal_entity_id']
        && (int)$row['legacy_position_id'] === (int)$projection['legacy_position_id']
        && (string)$row['position_code'] === (string)$projection['position_code']
        && $left_code === $right_code
        && (string)$row['position_name'] === (string)$projection['position_name']
        && (string)$row['position_status'] === (string)$projection['position_status']
        && (string)$row['reference_quality'] === 'legacy_position_identity_only'
        && (string)$row['source'] === 'legacy_position_seed';
}
