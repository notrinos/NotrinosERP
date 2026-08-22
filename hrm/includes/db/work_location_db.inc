<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

/**
 * HRM-FND-003 greenfield normalized Work Location foundation.
 *
 * The accepted dependency inventory found no trustworthy reusable legacy
 * Work Location identity. Customer branches remain sales entities, inventory
 * locations remain warehouse/stock locations, financial dimensions remain
 * accounting allocations, and Departments remain Organization Units. This
 * helper therefore exposes only the Legal-Entity-scoped normalized identity
 * table and deliberately provides no writer, legacy projection, Establishment
 * binding, Assignment binding, address/timezone/jurisdiction inference or payroll routing.
 */

/** @return bool */
function hrm_work_location_table_ready()
{
    if (!function_exists('db_query') || !function_exists('db_escape_function') || !defined('TB_PREF'))
        return false;
    $result = db_query("SHOW TABLES LIKE '".TB_PREF.db_escape_function('hrm_work_locations')."'");
    return $result && db_num_rows($result) === 1;
}

/**
 * Read one normalized Work Location identity.
 *
 * @param int $work_location_id
 * @param bool $for_update
 * @return array|false|null
 */
function get_hrm_work_location($work_location_id, $for_update = false)
{
    if (!hrm_work_location_table_ready() || (int)$work_location_id <= 0)
        return false;
    $sql = "SELECT work_location_id,legal_entity_id,work_location_code,work_location_name,work_location_status,reference_quality,source,"
        ."created_at,created_by,updated_at,updated_by FROM ".TB_PREF."hrm_work_locations WHERE work_location_id=".(int)$work_location_id;
    if ($for_update)
        $sql .= ' FOR UPDATE';
    $result = db_query($sql, 'could not read HRM Work Location');
    if (!$result || db_num_rows($result) > 1)
        return false;
    $row = db_fetch_assoc($result);
    return $row ? $row : null;
}

/**
 * Read normalized Work Locations for one Legal Entity. This foundation is
 * intentionally read-only and introduces no public writer or permission.
 *
 * @param int $legal_entity_id
 * @param bool $active_only
 * @return resource|false
 */
function get_hrm_work_locations_for_legal_entity($legal_entity_id, $active_only = true)
{
    if (!hrm_work_location_table_ready() || (int)$legal_entity_id <= 0)
        return false;
    $sql = "SELECT work_location_id,legal_entity_id,work_location_code,work_location_name,work_location_status,reference_quality,source "
        ."FROM ".TB_PREF."hrm_work_locations WHERE legal_entity_id=".(int)$legal_entity_id;
    if ($active_only)
        $sql .= " AND work_location_status='active'";
    $sql .= ' ORDER BY work_location_code,work_location_id';
    return db_query($sql, 'could not list HRM Work Locations');
}
