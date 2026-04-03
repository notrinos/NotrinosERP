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
 * CRM Communication Log Database Layer
 *
 * CRUD and query functions for tracking all communications
 * (emails, calls, meetings, notes) related to leads, contacts, and customers.
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

/**
 * Add a communication log entry.
 *
 * @param array $data
 * @return int|false
 */
function add_crm_communication($data)
{
    $sql = "INSERT INTO " . TB_PREF . "crm_communication_log
            (entity_type, entity_id, comm_type, subject, body,
             direction, from_address, to_address, date_time,
             created_by, duration_minutes) VALUES ("
        . db_escape($data['entity_type']) . ", "
        . db_escape($data['entity_id']) . ", "
        . db_escape($data['comm_type']) . ", "
        . db_escape(@$data['subject']) . ", "
        . db_escape(@$data['body']) . ", "
        . db_escape(@$data['direction']) . ", "
        . db_escape(@$data['from_address']) . ", "
        . db_escape(@$data['to_address']) . ", "
        . (!empty($data['date_time']) ? db_escape($data['date_time']) : "NOW()") . ", "
        . (@$data['created_by'] ? db_escape($data['created_by']) : "NULL") . ", "
        . (@$data['duration_minutes'] ? db_escape($data['duration_minutes']) : "NULL") . ")";

    db_query($sql, "Could not add communication log");
    return db_insert_id();
}

/**
 * Update a communication log entry.
 *
 * @param int   $id
 * @param array $data
 * @return bool
 */
function update_crm_communication($id, $data)
{
    $fields = array(
        'comm_type', 'subject', 'body', 'direction',
        'from_address', 'to_address', 'date_time', 'duration_minutes',
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

    $sql = "UPDATE " . TB_PREF . "crm_communication_log SET "
        . implode(', ', $sets)
        . " WHERE id = " . db_escape($id);

    return db_query($sql, "Could not update communication log");
}

/**
 * Delete a communication log entry.
 *
 * @param int $id
 * @return bool
 */
function delete_crm_communication($id)
{
    $sql = "DELETE FROM " . TB_PREF . "crm_communication_log
            WHERE id = " . db_escape($id);
    return db_query($sql, "Could not delete communication log");
}

/**
 * Get a single communication entry.
 *
 * @param int $id
 * @return array|false
 */
function get_crm_communication($id)
{
    $sql = "SELECT * FROM " . TB_PREF . "crm_communication_log
            WHERE id = " . db_escape($id);
    return db_fetch(db_query($sql, "Could not get communication log"));
}

/**
 * Get communications for an entity.
 *
 * @param string $entity_type
 * @param int    $entity_id
 * @param int    $limit
 * @param int    $offset
 * @return resource
 */
function get_crm_communications($entity_type, $entity_id, $limit = 0, $offset = 0)
{
    $sql = "SELECT cl.*, u.real_name AS created_by_name
            FROM " . TB_PREF . "crm_communication_log cl
            LEFT JOIN " . TB_PREF . "users u ON cl.created_by = u.id
            WHERE cl.entity_type = " . db_escape($entity_type)
        . " AND cl.entity_id = " . db_escape($entity_id)
        . " ORDER BY cl.date_time DESC";

    if ($limit > 0) {
        $sql .= " LIMIT " . (int)$offset . ", " . (int)$limit;
    }

    return db_query($sql, "Could not get communications");
}

/**
 * Search communications across all entities.
 *
 * @param array $filters
 * @param int   $limit
 * @param int   $offset
 * @return resource
 */
function search_crm_communications($filters = array(), $limit = 50, $offset = 0)
{
    $sql = "SELECT cl.*, u.real_name AS created_by_name
            FROM " . TB_PREF . "crm_communication_log cl
            LEFT JOIN " . TB_PREF . "users u ON cl.created_by = u.id
            WHERE 1=1";

    if (!empty($filters['comm_type'])) {
        $sql .= " AND cl.comm_type = " . db_escape($filters['comm_type']);
    }
    if (!empty($filters['entity_type'])) {
        $sql .= " AND cl.entity_type = " . db_escape($filters['entity_type']);
    }
    if (!empty($filters['direction'])) {
        $sql .= " AND cl.direction = " . db_escape($filters['direction']);
    }
    if (!empty($filters['search'])) {
        $search = db_escape('%' . $filters['search'] . '%');
        $sql .= " AND (cl.subject LIKE $search OR cl.body LIKE $search)";
    }
    if (!empty($filters['date_from'])) {
        $sql .= " AND cl.date_time >= " . db_escape($filters['date_from']);
    }
    if (!empty($filters['date_to'])) {
        $sql .= " AND cl.date_time <= " . db_escape($filters['date_to'] . ' 23:59:59');
    }
    if (!empty($filters['created_by'])) {
        $sql .= " AND cl.created_by = " . db_escape($filters['created_by']);
    }

    $sql .= " ORDER BY cl.date_time DESC";

    if ($limit > 0) {
        $sql .= " LIMIT " . (int)$offset . ", " . (int)$limit;
    }

    return db_query($sql, "Could not search communications");
}

/**
 * Count communications for an entity.
 *
 * @param string $entity_type
 * @param int    $entity_id
 * @return int
 */
function count_crm_communications($entity_type, $entity_id)
{
    $sql = "SELECT COUNT(*) as cnt FROM " . TB_PREF . "crm_communication_log
            WHERE entity_type = " . db_escape($entity_type)
        . " AND entity_id = " . db_escape($entity_id);
    $result = db_query($sql, "Could not count communications");
    $row = db_fetch($result);
    return $row ? (int)$row['cnt'] : 0;
}

/**
 * Copy all communications from a lead to a customer (used during conversion).
 *
 * @param int $lead_id
 * @param int $customer_id
 * @return int  Number of records copied.
 */
function copy_lead_communications_to_customer($lead_id, $customer_id)
{
    $sql = "INSERT INTO " . TB_PREF . "crm_communication_log
            (entity_type, entity_id, comm_type, subject, body,
             direction, from_address, to_address, date_time,
             created_by, duration_minutes)
            SELECT 'customer', " . db_escape($customer_id) . ",
                   comm_type, subject, body, direction,
                   from_address, to_address, date_time,
                   created_by, duration_minutes
            FROM " . TB_PREF . "crm_communication_log
            WHERE entity_type = 'lead' AND entity_id = " . db_escape($lead_id);

    db_query($sql, "Could not copy lead communications to customer");
    return db_num_affected_rows();
}
