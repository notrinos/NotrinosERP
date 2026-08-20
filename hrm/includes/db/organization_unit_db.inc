<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

/**
 * HRM-FND-003 bounded Organization Unit compatibility foundation.
 *
 * Legacy departments are the only currently verified management-hierarchy
 * source. Financial dimensions/cost centres, warehouses/stock locations and
 * customer branches are deliberately not reinterpreted as HR organization,
 * establishment or work-location identities.
 */

/** @return bool */
function hrm_organization_unit_table_ready()
{
    if (!function_exists('db_query') || !function_exists('db_escape_function') || !defined('TB_PREF'))
        return false;
    $result = db_query("SHOW TABLES LIKE '".TB_PREF.db_escape_function('hrm_organization_units')."'");
    return $result && db_num_rows($result) === 1;
}

/**
 * Normalize one legacy department into the exact compatibility projection.
 * Cost-centre and manager values are retained only as unverified legacy
 * references; they are not promoted to normalized costing/manager semantics.
 *
 * @param array $department
 * @param int $legal_entity_id
 * @return array|false
 */
function hrm_organization_unit_projection_from_department($department, $legal_entity_id)
{
    if (!is_array($department) || (int)$legal_entity_id <= 0)
        return false;
    $department_id = isset($department['department_id']) ? (int)$department['department_id'] : 0;
    $name = isset($department['department_name']) ? trim((string)$department['department_name']) : '';
    if ($department_id <= 0 || $name === '')
        return false;

    $parent = isset($department['parent_department_id']) ? $department['parent_department_id'] : null;
    if ($parent === null || $parent === '' || (int)$parent === 0)
        $parent = null;
    elseif (!is_numeric($parent) || (string)(int)$parent !== trim((string)$parent) || (int)$parent <= 0)
        return false;
    else
        $parent = (int)$parent;
    if ($parent !== null && $parent === $department_id)
        return false;

    $code = isset($department['department_code']) ? trim((string)$department['department_code']) : '';
    if (strlen($code) > 20)
        return false;
    if ($code === '')
        $code = null;

    $manager = isset($department['manager_employee_id']) ? trim((string)$department['manager_employee_id']) : '';
    if (strlen($manager) > 20)
        return false;
    if ($manager === '')
        $manager = null;

    $cost_center = isset($department['cost_center_id']) ? $department['cost_center_id'] : 0;
    if (!is_numeric($cost_center) || (string)(int)$cost_center !== trim((string)$cost_center) || (int)$cost_center < 0)
        return false;

    $inactive = isset($department['inactive']) ? (int)$department['inactive'] : 0;
    if ($inactive !== 0 && $inactive !== 1)
        return false;

    return array(
        'legal_entity_id' => (int)$legal_entity_id,
        'legacy_department_id' => $department_id,
        'unit_code' => 'DEPT-'.$department_id,
        'legacy_department_code' => $code,
        'unit_name' => $name,
        'parent_legacy_department_id' => $parent,
        'legacy_manager_employee_id' => $manager,
        'legacy_cost_center_id' => (int)$cost_center,
        'unit_status' => $inactive ? 'inactive' : 'active',
        'reference_quality' => 'legacy_department_only',
        'source' => 'legacy_department_seed',
    );
}

/** @return array|false|null */
function get_hrm_organization_unit_by_legacy_department($department_id, $for_update = false)
{
    if (!hrm_organization_unit_table_ready() || (int)$department_id <= 0)
        return false;
    $sql = "SELECT organization_unit_id,legal_entity_id,legacy_department_id,unit_code,legacy_department_code,"
        ."unit_name,parent_organization_unit_id,legacy_manager_employee_id,legacy_cost_center_id,unit_status,"
        ."reference_quality,source,created_at,created_by,updated_at,updated_by FROM ".TB_PREF."hrm_organization_units "
        ."WHERE legacy_department_id=".(int)$department_id;
    if ($for_update)
        $sql .= ' FOR UPDATE';
    $result = db_query($sql, 'could not read HRM Organization Unit compatibility row');
    if (!$result)
        return false;
    if (db_num_rows($result) > 1)
        return false;
    $row = db_fetch_assoc($result);
    return $row ? $row : null;
}
