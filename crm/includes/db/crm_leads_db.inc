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
 * CRM Leads / Opportunities Database Layer
 *
 * CRUD and query functions for the unified lead/opportunity table.
 * Leads and opportunities share the same table; the is_opportunity
 * flag distinguishes them.
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

/**
 * Add a new lead record.
 *
 * @param array $data  Associative array of column => value.
 * @return int|false   The new lead ID or false on failure.
 */
function add_crm_lead($data)
{
    $fields = array(
        'lead_ref', 'title', 'company_name', 'contact_name',
        'email', 'phone', 'mobile', 'website',
        'address', 'city', 'state', 'country', 'postal_code',
        'lead_source_id', 'lead_status', 'priority',
        'is_opportunity', 'stage_id', 'assigned_to', 'sales_team_id',
        'expected_revenue', 'probability', 'expected_close_date',
        'linked_customer_id', 'linked_person_id', 'created_by', 'notes',
    );

    $cols = array();
    $vals = array();
    foreach ($fields as $f) {
        if (array_key_exists($f, $data)) {
            $cols[] = $f;
            $vals[] = ($data[$f] === null || $data[$f] === '')
                ? "NULL"
                : db_escape($data[$f]);
        }
    }

    $sql = "INSERT INTO " . TB_PREF . "crm_leads (" . implode(', ', $cols) . ")
            VALUES (" . implode(', ', $vals) . ")";

    begin_transaction();
    $ret = db_query($sql, "Could not add CRM lead");
    $id = $ret ? db_insert_id() : false;

    if ($id && !empty($data['tag_ids']) && is_array($data['tag_ids'])) {
        update_crm_entity_tags(CRM_ENTITY_LEAD, $id, $data['tag_ids']);
    }

    commit_transaction();
    return $id;
}

/**
 * Update an existing lead/opportunity record.
 *
 * @param int   $id    Lead ID.
 * @param array $data  Associative array of column => value.
 * @return bool
 */
function update_crm_lead($id, $data)
{
    $fields = array(
        'lead_ref', 'title', 'company_name', 'contact_name',
        'email', 'phone', 'mobile', 'website',
        'address', 'city', 'state', 'country', 'postal_code',
        'lead_source_id', 'lead_status', 'priority',
        'is_opportunity', 'stage_id', 'assigned_to', 'sales_team_id',
        'expected_revenue', 'probability', 'expected_close_date',
        'date_qualified', 'date_converted', 'date_won', 'date_lost',
        'lost_reason_id', 'lost_notes',
        'linked_customer_id', 'linked_person_id', 'notes', 'inactive',
    );

    $sets = array();
    foreach ($fields as $f) {
        if (array_key_exists($f, $data)) {
            $sets[] = "$f = " . (($data[$f] === null || $data[$f] === '')
                ? "NULL"
                : db_escape($data[$f]));
        }
    }

    if (empty($sets)) {
        return true;
    }

    $sql = "UPDATE " . TB_PREF . "crm_leads SET "
        . implode(', ', $sets)
        . " WHERE id = " . db_escape($id);

    begin_transaction();
    $ret = db_query($sql, "Could not update CRM lead");

    if ($ret && array_key_exists('tag_ids', $data) && is_array($data['tag_ids'])) {
        update_crm_entity_tags(CRM_ENTITY_LEAD, $id, $data['tag_ids']);
    }

    commit_transaction();
    return $ret;
}

/**
 * Delete a lead by ID (soft-delete by setting inactive=1).
 *
 * @param int  $id
 * @param bool $hard  If true, permanently delete the record.
 * @return bool
 */
function delete_crm_lead($id, $hard = false)
{
    begin_transaction();

    if ($hard) {
        // Remove related records
        $sql = "DELETE ta FROM " . TB_PREF . "tag_associations ta
                INNER JOIN " . TB_PREF . "tags t ON t.id = ta.tag_id
                WHERE ta.record_id = " . db_escape('lead:' . (int)$id)
            . " AND t.type = " . TAG_CRM;
        db_query($sql, "Could not delete lead tags");

        $sql = "DELETE FROM " . TB_PREF . "crm_activities
                WHERE entity_type = 'lead' AND entity_id = " . db_escape($id);
        db_query($sql, "Could not delete lead activities");

        $sql = "DELETE FROM " . TB_PREF . "crm_communication_log
                WHERE entity_type = 'lead' AND entity_id = " . db_escape($id);
        db_query($sql, "Could not delete lead communications");

        $sql = "DELETE FROM " . TB_PREF . "crm_campaign_leads
                WHERE lead_id = " . db_escape($id);
        db_query($sql, "Could not delete lead campaign links");

        $sql = "DELETE FROM " . TB_PREF . "crm_leads WHERE id = " . db_escape($id);
        $ret = db_query($sql, "Could not delete CRM lead");
    } else {
        $sql = "UPDATE " . TB_PREF . "crm_leads SET inactive = 1 WHERE id = " . db_escape($id);
        $ret = db_query($sql, "Could not deactivate CRM lead");
    }

    commit_transaction();
    return $ret;
}

/**
 * Retrieve a single lead/opportunity by ID.
 *
 * @param int $id
 * @return array|false
 */
function get_crm_lead($id)
{
    $sql = "SELECT l.*, src.name AS source_name, stg.name AS stage_name,
                   stg.probability AS stage_probability,
                   lr.description AS lost_reason_text,
                   st.name AS team_name
            FROM " . TB_PREF . "crm_leads l
            LEFT JOIN " . TB_PREF . "crm_lead_sources src ON l.lead_source_id = src.id
            LEFT JOIN " . TB_PREF . "crm_sales_stages stg ON l.stage_id = stg.id
            LEFT JOIN " . TB_PREF . "crm_lost_reasons lr ON l.lost_reason_id = lr.id
            LEFT JOIN " . TB_PREF . "crm_sales_teams st ON l.sales_team_id = st.id
            WHERE l.id = " . db_escape($id);
    $result = db_query($sql, "Could not get CRM lead");
    return db_fetch($result);
}

/**
 * Retrieve leads/opportunities with optional filtering.
 *
 * @param array $filters  Associative array of filter criteria.
 * @param int   $limit    Max records to return (0 = all).
 * @param int   $offset   Record offset for pagination.
 * @return resource        DB result resource.
 */
function get_crm_leads($filters = array(), $limit = 0, $offset = 0)
{
    $sql = "SELECT l.*, src.name AS source_name, stg.name AS stage_name,
                   stg.probability AS stage_probability, st.name AS team_name
            FROM " . TB_PREF . "crm_leads l
            LEFT JOIN " . TB_PREF . "crm_lead_sources src ON l.lead_source_id = src.id
            LEFT JOIN " . TB_PREF . "crm_sales_stages stg ON l.stage_id = stg.id
            LEFT JOIN " . TB_PREF . "crm_sales_teams st ON l.sales_team_id = st.id
            WHERE 1=1";

    if (isset($filters['is_opportunity'])) {
        $sql .= " AND l.is_opportunity = " . db_escape($filters['is_opportunity']);
    }
    if (!empty($filters['lead_status'])) {
        $sql .= " AND l.lead_status = " . db_escape($filters['lead_status']);
    }
    if (!empty($filters['stage_id'])) {
        $sql .= " AND l.stage_id = " . db_escape($filters['stage_id']);
    }
    if (!empty($filters['assigned_to'])) {
        $sql .= " AND l.assigned_to = " . db_escape($filters['assigned_to']);
    }
    if (!empty($filters['sales_team_id'])) {
        $sql .= " AND l.sales_team_id = " . db_escape($filters['sales_team_id']);
    }
    if (!empty($filters['lead_source_id'])) {
        $sql .= " AND l.lead_source_id = " . db_escape($filters['lead_source_id']);
    }
    if (!empty($filters['search'])) {
        $search = db_escape('%' . $filters['search'] . '%');
        $sql .= " AND (l.title LIKE $search OR l.company_name LIKE $search
                  OR l.contact_name LIKE $search OR l.email LIKE $search)";
    }
    if (!empty($filters['date_from'])) {
        $sql .= " AND l.date_created >= " . db_escape($filters['date_from']);
    }
    if (!empty($filters['date_to'])) {
        $sql .= " AND l.date_created <= " . db_escape($filters['date_to'] . ' 23:59:59');
    }
    if (!empty($filters['tag_id'])) {
        $sql .= " AND CONCAT('lead:', l.id) IN (SELECT ta.record_id FROM " . TB_PREF . "tag_associations ta
                  WHERE ta.tag_id=" . db_escape($filters['tag_id']) . ")";
    }
    if (isset($filters['inactive'])) {
        $sql .= " AND l.inactive = " . db_escape($filters['inactive']);
    } else {
        $sql .= " AND l.inactive = 0";
    }

    $sql .= " ORDER BY l.date_created DESC";

    if ($limit > 0) {
        $sql .= " LIMIT " . (int)$offset . ", " . (int)$limit;
    }

    return db_query($sql, "Could not get CRM leads");
}

/**
 * Count leads/opportunities matching filter criteria.
 *
 * @param array $filters
 * @return int
 */
function count_crm_leads($filters = array())
{
    $sql = "SELECT COUNT(*) as cnt FROM " . TB_PREF . "crm_leads l WHERE 1=1";

    if (isset($filters['is_opportunity'])) {
        $sql .= " AND l.is_opportunity = " . db_escape($filters['is_opportunity']);
    }
    if (!empty($filters['lead_status'])) {
        $sql .= " AND l.lead_status = " . db_escape($filters['lead_status']);
    }
    if (!empty($filters['stage_id'])) {
        $sql .= " AND l.stage_id = " . db_escape($filters['stage_id']);
    }
    if (!empty($filters['assigned_to'])) {
        $sql .= " AND l.assigned_to = " . db_escape($filters['assigned_to']);
    }
    if (!empty($filters['sales_team_id'])) {
        $sql .= " AND l.sales_team_id = " . db_escape($filters['sales_team_id']);
    }
    if (!empty($filters['lead_source_id'])) {
        $sql .= " AND l.lead_source_id = " . db_escape($filters['lead_source_id']);
    }
    if (!empty($filters['search'])) {
        $search = db_escape('%' . $filters['search'] . '%');
        $sql .= " AND (l.title LIKE $search OR l.company_name LIKE $search
                  OR l.contact_name LIKE $search OR l.email LIKE $search)";
    }
    if (!empty($filters['date_from'])) {
        $sql .= " AND l.date_created >= " . db_escape($filters['date_from']);
    }
    if (!empty($filters['date_to'])) {
        $sql .= " AND l.date_created <= " . db_escape($filters['date_to'] . ' 23:59:59');
    }
    if (!empty($filters['tag_id'])) {
        $sql .= " AND CONCAT('lead:', l.id) IN (SELECT ta.record_id FROM " . TB_PREF . "tag_associations ta
                  WHERE ta.tag_id=" . db_escape($filters['tag_id']) . ")";
    }
    if (isset($filters['inactive'])) {
        $sql .= " AND l.inactive = " . db_escape($filters['inactive']);
    } else {
        $sql .= " AND l.inactive = 0";
    }

    $result = db_query($sql, "Could not count CRM leads");
    $row = db_fetch($result);
    return $row ? (int)$row['cnt'] : 0;
}

/**
 * Convert a lead to an opportunity (set is_opportunity=1 and assign stage).
 *
 * @param int $lead_id
 * @param int $stage_id  Initial pipeline stage.
 * @return bool
 */
function convert_lead_to_opportunity($lead_id, $stage_id = null)
{
    $lead = get_crm_lead($lead_id);
    if (!$lead) {
        return false;
    }

    $data = array(
        'is_opportunity' => 1,
        'lead_status' => CRM_LEAD_QUALIFIED,
        'date_qualified' => date('Y-m-d H:i:s'),
    );

    if ($stage_id) {
        $data['stage_id'] = $stage_id;
    } else {
        // Assign first active stage
        $sql = "SELECT id, probability FROM " . TB_PREF . "crm_sales_stages
                WHERE active = 1 ORDER BY sequence ASC LIMIT 1";
        $res = db_query($sql, "Could not get first stage");
        $stage = db_fetch($res);
        if ($stage) {
            $data['stage_id'] = $stage['id'];
            $data['probability'] = $stage['probability'];
        }
    }

    if (empty($lead['lead_ref']) || strpos($lead['lead_ref'], get_crm_setting('lead_ref_prefix', 'LD')) === 0) {
        $data['lead_ref'] = crm_next_opportunity_ref();
    }

    return update_crm_lead($lead_id, $data);
}

/**
 * Mark an opportunity as Won.
 *
 * @param int $lead_id
 * @param int $customer_id  Optional linked customer.
 * @return bool
 */
function mark_opportunity_won($lead_id, $customer_id = null)
{
    $data = array(
        'lead_status' => CRM_LEAD_CONVERTED,
        'date_won' => date('Y-m-d H:i:s'),
        'probability' => 100,
    );

    // Set stage to Won
    $sql = "SELECT id FROM " . TB_PREF . "crm_sales_stages WHERE name = 'Won' LIMIT 1";
    $res = db_query($sql, "Could not find Won stage");
    $stage = db_fetch($res);
    if ($stage) {
        $data['stage_id'] = $stage['id'];
    }

    if ($customer_id) {
        $data['linked_customer_id'] = $customer_id;
        $data['date_converted'] = date('Y-m-d H:i:s');
    }

    return update_crm_lead($lead_id, $data);
}

/**
 * Mark an opportunity as Lost.
 *
 * @param int    $lead_id
 * @param int    $reason_id
 * @param string $notes
 * @return bool
 */
function mark_opportunity_lost($lead_id, $reason_id = null, $notes = null)
{
    $data = array(
        'lead_status' => CRM_LEAD_LOST,
        'date_lost' => date('Y-m-d H:i:s'),
        'probability' => 0,
    );

    // Set stage to Lost
    $sql = "SELECT id FROM " . TB_PREF . "crm_sales_stages WHERE name = 'Lost' LIMIT 1";
    $res = db_query($sql, "Could not find Lost stage");
    $stage = db_fetch($res);
    if ($stage) {
        $data['stage_id'] = $stage['id'];
    }

    if ($reason_id) {
        $data['lost_reason_id'] = $reason_id;
    }
    if ($notes) {
        $data['lost_notes'] = $notes;
    }

    return update_crm_lead($lead_id, $data);
}

/**
 * Move an opportunity to a different pipeline stage and update probability.
 *
 * @param int $lead_id
 * @param int $stage_id
 * @return bool
 */
function update_opportunity_stage($lead_id, $stage_id)
{
    $sql = "SELECT probability FROM " . TB_PREF . "crm_sales_stages
            WHERE id = " . db_escape($stage_id);
    $res = db_query($sql, "Could not get stage probability");
    $stage = db_fetch($res);

    $data = array('stage_id' => $stage_id);
    if ($stage) {
        $data['probability'] = $stage['probability'];
    }
    return update_crm_lead($lead_id, $data);
}

// ================================================================
// ENTITY TAGS (shared 0_tags + 0_tag_associations, type=TAG_CRM)
// ================================================================

/**
 * Replace all tags for a CRM entity.
 *
 * Uses shared tag_associations with record_id = "entity_type:entity_id".
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
// LEAD SOURCES
// ================================================================

/**
 * Get all active lead sources.
 *
 * @param bool $include_inactive
 * @return resource
 */
function get_crm_lead_sources($include_inactive = false)
{
    $sql = "SELECT * FROM " . TB_PREF . "crm_lead_sources";
    if (!$include_inactive) {
        $sql .= " WHERE active = 1";
    }
    $sql .= " ORDER BY name";
    return db_query($sql, "Could not get lead sources");
}

/**
 * Get a single lead source by ID.
 *
 * @param int $id
 * @return array|false
 */
function get_crm_lead_source($id)
{
    $sql = "SELECT * FROM " . TB_PREF . "crm_lead_sources WHERE id = " . db_escape($id);
    return db_fetch(db_query($sql, "Could not get lead source"));
}

/**
 * Add a lead source.
 *
 * @param string $name
 * @param string $description
 * @return int
 */
function add_crm_lead_source($name, $description = '')
{
    $sql = "INSERT INTO " . TB_PREF . "crm_lead_sources (name, description)
            VALUES (" . db_escape($name) . ", " . db_escape($description) . ")";
    db_query($sql, "Could not add lead source");
    return db_insert_id();
}

/**
 * Update a lead source.
 *
 * @param int    $id
 * @param string $name
 * @param string $description
 * @param int    $active
 * @return bool
 */
function update_crm_lead_source($id, $name, $description, $active)
{
    $sql = "UPDATE " . TB_PREF . "crm_lead_sources SET "
        . "name = " . db_escape($name) . ", "
        . "description = " . db_escape($description) . ", "
        . "active = " . db_escape($active)
        . " WHERE id = " . db_escape($id);
    return db_query($sql, "Could not update lead source");
}

/**
 * Delete a lead source.
 *
 * @param int $id
 * @return bool
 */
function delete_crm_lead_source($id)
{
    $sql = "DELETE FROM " . TB_PREF . "crm_lead_sources WHERE id = " . db_escape($id);
    return db_query($sql, "Could not delete lead source");
}

// ================================================================
// SALES STAGES
// ================================================================

/**
 * Get all sales stages.
 *
 * @param bool $include_inactive
 * @return resource
 */
function get_crm_sales_stages($include_inactive = false)
{
    $sql = "SELECT * FROM " . TB_PREF . "crm_sales_stages";
    if (!$include_inactive) {
        $sql .= " WHERE active = 1";
    }
    $sql .= " ORDER BY sequence, name";
    return db_query($sql, "Could not get sales stages");
}

/**
 * Get a single stage.
 *
 * @param int $id
 * @return array|false
 */
function get_crm_sales_stage($id)
{
    $sql = "SELECT * FROM " . TB_PREF . "crm_sales_stages WHERE id = " . db_escape($id);
    return db_fetch(db_query($sql, "Could not get sales stage"));
}

/**
 * Add a sales stage.
 *
 * @param string $name
 * @param int    $sequence
 * @param int    $probability
 * @param string $description
 * @return int
 */
function add_crm_sales_stage($name, $sequence, $probability, $description = '')
{
    $sql = "INSERT INTO " . TB_PREF . "crm_sales_stages
            (name, sequence, probability, description) VALUES ("
        . db_escape($name) . ", "
        . db_escape($sequence) . ", "
        . db_escape($probability) . ", "
        . db_escape($description) . ")";
    db_query($sql, "Could not add sales stage");
    return db_insert_id();
}

/**
 * Update a sales stage.
 *
 * @param int    $id
 * @param string $name
 * @param int    $sequence
 * @param int    $probability
 * @param string $description
 * @param int    $active
 * @return bool
 */
function update_crm_sales_stage($id, $name, $sequence, $probability, $description, $active)
{
    $sql = "UPDATE " . TB_PREF . "crm_sales_stages SET "
        . "name = " . db_escape($name) . ", "
        . "sequence = " . db_escape($sequence) . ", "
        . "probability = " . db_escape($probability) . ", "
        . "description = " . db_escape($description) . ", "
        . "active = " . db_escape($active)
        . " WHERE id = " . db_escape($id);
    return db_query($sql, "Could not update sales stage");
}

/**
 * Delete a sales stage.
 *
 * @param int $id
 * @return bool
 */
function delete_crm_sales_stage($id)
{
    $sql = "DELETE FROM " . TB_PREF . "crm_sales_stages WHERE id = " . db_escape($id);
    return db_query($sql, "Could not delete sales stage");
}

// ================================================================
// LOST REASONS
// ================================================================

/**
 * Get all lost reasons.
 *
 * @param bool $include_inactive
 * @return resource
 */
function get_crm_lost_reasons($include_inactive = false)
{
    $sql = "SELECT * FROM " . TB_PREF . "crm_lost_reasons";
    if (!$include_inactive) {
        $sql .= " WHERE active = 1";
    }
    $sql .= " ORDER BY description";
    return db_query($sql, "Could not get lost reasons");
}

/**
 * Get a single lost reason.
 *
 * @param int $id
 * @return array|false
 */
function get_crm_lost_reason($id)
{
    $sql = "SELECT * FROM " . TB_PREF . "crm_lost_reasons WHERE id = " . db_escape($id);
    return db_fetch(db_query($sql, "Could not get lost reason"));
}

/**
 * Add a lost reason.
 *
 * @param string $description
 * @return int
 */
function add_crm_lost_reason($description)
{
    $sql = "INSERT INTO " . TB_PREF . "crm_lost_reasons (description)
            VALUES (" . db_escape($description) . ")";
    db_query($sql, "Could not add lost reason");
    return db_insert_id();
}

/**
 * Update a lost reason.
 *
 * @param int    $id
 * @param string $description
 * @param int    $active
 * @return bool
 */
function update_crm_lost_reason($id, $description, $active)
{
    $sql = "UPDATE " . TB_PREF . "crm_lost_reasons SET "
        . "description = " . db_escape($description) . ", "
        . "active = " . db_escape($active)
        . " WHERE id = " . db_escape($id);
    return db_query($sql, "Could not update lost reason");
}

/**
 * Delete a lost reason.
 *
 * @param int $id
 * @return bool
 */
function delete_crm_lost_reason($id)
{
    $sql = "DELETE FROM " . TB_PREF . "crm_lost_reasons WHERE id = " . db_escape($id);
    return db_query($sql, "Could not delete lost reason");
}

// ================================================================
// TAGS (thin wrappers around shared 0_tags system, type=TAG_CRM)
// ================================================================

/**
 * Get all CRM tags (type = TAG_CRM).
 *
 * @return resource
 */
function get_crm_tags()
{
    global $path_to_root;
    include_once($path_to_root . '/admin/db/tags_db.inc');
    return get_tags(TAG_CRM);
}

/**
 * Get a single CRM tag.
 *
 * @param int $id
 * @return array|false
 */
function get_crm_tag($id)
{
    global $path_to_root;
    include_once($path_to_root . '/admin/db/tags_db.inc');
    return get_tag($id);
}

/**
 * Add a CRM tag.
 *
 * @param string $name
 * @param string $color  Hex color code.
 * @return bool
 */
function add_crm_tag($name, $color = '#2196F3')
{
    global $path_to_root;
    include_once($path_to_root . '/admin/db/tags_db.inc');
    return add_tag(TAG_CRM, $name, $name, $color);
}

/**
 * Update a CRM tag.
 *
 * @param int    $id
 * @param string $name
 * @param string $color
 * @return bool
 */
function update_crm_tag($id, $name, $color)
{
    global $path_to_root;
    include_once($path_to_root . '/admin/db/tags_db.inc');
    return update_tag($id, $name, $name, null, $color);
}

/**
 * Delete a CRM tag and all its associations.
 *
 * @param int $id
 * @return bool
 */
function delete_crm_tag($id)
{
    global $path_to_root;
    include_once($path_to_root . '/admin/db/tags_db.inc');

    // Remove all associations for this tag
    $sql = "DELETE FROM " . TB_PREF . "tag_associations WHERE tag_id = " . db_escape($id);
    db_query($sql, "Could not delete tag associations");

    // Remove the tag itself
    delete_tag($id);
}

// ================================================================
// SCORING CONFIG
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
