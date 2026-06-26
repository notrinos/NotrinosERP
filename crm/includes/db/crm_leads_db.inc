<?php
/**********************************************************************
Copyright (C) NotrinosERP.
Released under the terms of the GNU General Public License, GPL,
as published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/
/**
 * CRM Leads / Opportunities — Legacy Compatibility & Non-CRUD Helpers
 *
 * All CRUD operations have been migrated to entity classes:
 *   crm_leads_entity          — CRUD, JOIN queries, cascade delete, tags
 *   crm_lead_sources_entity   — lead sources CRUD
 *   crm_sales_stages_entity   — sales stages CRUD
 *   crm_lost_reasons_entity   — lost reasons CRUD
 *   admin/db/tags_db.inc      — shared tag CRUD (TAG_CRM type)
 *
 * This file is kept for backward-compat aliases (update_crm_entity_tags,
 * get_crm_entity_tags) and scoring config management.
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

// ================================================================
// ENTITY TAG ALIASES (backward-compat wrappers)
// ================================================================

/**
 * Replace all tags for a CRM entity.
 *
 * @deprecated Use crm_leads_entity::update_tags() for lead tags,
 *             or update_tag_associations(TAG_CRM, ...) for other entities.
 *
 * @param string $entity_type  e.g. 'lead', 'campaign'
 * @param int    $entity_id
 * @param array  $tag_ids
 * @return bool
 */
function update_crm_entity_tags($entity_type, $entity_id, $tag_ids)
{
    global $path_to_root;
    include_once($path_to_root . '/admin/db/tags_db.inc');

    $record_id = $entity_type . ':' . (int)$entity_id;
    update_tag_associations(TAG_CRM, $record_id, $tag_ids);
    return true;
}

/**
 * Get tags for a CRM entity.
 *
 * @param string $entity_type
 * @param int    $entity_id
 * @return array  Array of tag rows (each has id, name, color, etc.)
 */
function get_crm_entity_tags($entity_type, $entity_id)
{
    global $path_to_root;
    include_once($path_to_root . '/admin/db/tags_db.inc');

    $record_id = $entity_type . ':' . (int)$entity_id;
    $result = get_tags_associated_with_record(TAG_CRM, $record_id);
    $tags = array();
    while ($row = db_fetch($result)) {
        $tags[] = $row;
    }
    return $tags;
}

// ================================================================
// SCORING CONFIG (no entity — admin-only management table)
// ================================================================

/**
 * Get all scoring configuration entries.
 *
 * @return resource
 */
function get_crm_scoring_configs()
{
    $sql = "SELECT * FROM " . TB_PREF . "crm_scoring_config ORDER BY variable_name";
    return db_query($sql, "Could not get scoring configs");
}

/**
 * Update a scoring weight.
 *
 * @param int   $id
 * @param float $weight
 * @param int   $active
 * @return bool
 */
function update_crm_scoring_config($id, $weight, $active)
{
    $sql = "UPDATE " . TB_PREF . "crm_scoring_config SET "
        . "weight = " . db_escape($weight) . ", "
        . "active = " . db_escape($active)
        . " WHERE id = " . db_escape($id);
    return db_query($sql, "Could not update scoring config");
}
