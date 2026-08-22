<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

/**
 * HRM-FND-003 greenfield normalized Establishment foundation.
 *
 * The accepted dependency inventory found no trustworthy reusable legacy
 * Establishment identity. Customer branches remain sales entities, inventory
 * locations remain warehouse/stock locations, financial dimensions remain
 * accounting allocations, and Departments remain Organization Units. This
 * helper therefore exposes only the Legal-Entity-scoped normalized identity
 * table and deliberately provides no writer, legacy projection, Work Location
 * binding, Assignment binding, jurisdiction inference or payroll routing.
 */

/** @return bool */
function hrm_establishment_table_ready()
{
    if (!function_exists('db_query') || !function_exists('db_escape_function') || !defined('TB_PREF'))
        return false;
    $result = db_query("SHOW TABLES LIKE '".TB_PREF.db_escape_function('hrm_establishments')."'");
    return $result && db_num_rows($result) === 1;
}

/**
 * Read one normalized Establishment identity.
 *
 * @param int $establishment_id
 * @param bool $for_update
 * @return array|false|null
 */
function get_hrm_establishment($establishment_id, $for_update = false)
{
    if (!hrm_establishment_table_ready() || (int)$establishment_id <= 0)
        return false;
    $sql = "SELECT establishment_id,legal_entity_id,establishment_code,establishment_name,establishment_status,reference_quality,source,"
        ."created_at,created_by,updated_at,updated_by FROM ".TB_PREF."hrm_establishments WHERE establishment_id=".(int)$establishment_id;
    if ($for_update)
        $sql .= ' FOR UPDATE';
    $result = db_query($sql, 'could not read HRM Establishment');
    if (!$result || db_num_rows($result) > 1)
        return false;
    $row = db_fetch_assoc($result);
    return $row ? $row : null;
}

/**
 * Read normalized Establishments for one Legal Entity. This foundation is
 * intentionally read-only and introduces no public writer or permission.
 *
 * @param int $legal_entity_id
 * @param bool $active_only
 * @return resource|false
 */
function get_hrm_establishments_for_legal_entity($legal_entity_id, $active_only = true)
{
    if (!hrm_establishment_table_ready() || (int)$legal_entity_id <= 0)
        return false;
    $sql = "SELECT establishment_id,legal_entity_id,establishment_code,establishment_name,establishment_status,reference_quality,source "
        ."FROM ".TB_PREF."hrm_establishments WHERE legal_entity_id=".(int)$legal_entity_id;
    if ($active_only)
        $sql .= " AND establishment_status='active'";
    $sql .= ' ORDER BY establishment_code,establishment_id';
    return db_query($sql, 'could not list HRM Establishments');
}
