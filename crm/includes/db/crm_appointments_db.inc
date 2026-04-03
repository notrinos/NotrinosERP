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
 * CRM Appointments Database Layer
 *
 * CRUD and query functions for scheduling and managing appointments.
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

/**
 * Add a new appointment.
 *
 * @param array $data
 * @return int|false
 */
function add_crm_appointment($data)
{
    $sql = "INSERT INTO " . TB_PREF . "crm_appointments
            (title, appointment_type_id, lead_id, customer_id, contact_person_id,
             date_time, duration_minutes, location, video_link,
             attendees_json, notes, status, created_by) VALUES ("
        . db_escape($data['title']) . ", "
        . (@$data['appointment_type_id'] ? db_escape($data['appointment_type_id']) : "NULL") . ", "
        . (@$data['lead_id'] ? db_escape($data['lead_id']) : "NULL") . ", "
        . (@$data['customer_id'] ? db_escape($data['customer_id']) : "NULL") . ", "
        . (@$data['contact_person_id'] ? db_escape($data['contact_person_id']) : "NULL") . ", "
        . db_escape($data['date_time']) . ", "
        . db_escape(@$data['duration_minutes'] ?: 60) . ", "
        . db_escape(@$data['location']) . ", "
        . db_escape(@$data['video_link']) . ", "
        . db_escape(@$data['attendees_json']) . ", "
        . db_escape(@$data['notes']) . ", "
        . db_escape(@$data['status'] ?: CRM_APPT_SCHEDULED) . ", "
        . (@$data['created_by'] ? db_escape($data['created_by']) : "NULL") . ")";

    db_query($sql, "Could not add CRM appointment");
    return db_insert_id();
}

/**
 * Update an appointment.
 *
 * @param int   $id
 * @param array $data
 * @return bool
 */
function update_crm_appointment($id, $data)
{
    $fields = array(
        'title', 'appointment_type_id', 'lead_id', 'customer_id',
        'contact_person_id', 'date_time', 'duration_minutes',
        'location', 'video_link', 'attendees_json', 'notes',
        'status', 'reminder_sent',
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

    $sql = "UPDATE " . TB_PREF . "crm_appointments SET "
        . implode(', ', $sets)
        . " WHERE id = " . db_escape($id);

    return db_query($sql, "Could not update CRM appointment");
}

/**
 * Delete an appointment.
 *
 * @param int $id
 * @return bool
 */
function delete_crm_appointment($id)
{
    $sql = "DELETE FROM " . TB_PREF . "crm_appointments WHERE id = " . db_escape($id);
    return db_query($sql, "Could not delete CRM appointment");
}

/**
 * Get a single appointment by ID.
 *
 * @param int $id
 * @return array|false
 */
function get_crm_appointment($id)
{
    $sql = "SELECT a.*, apt.name AS type_name, apt.default_duration AS type_duration,
                   l.title AS lead_title, d.name AS customer_name
            FROM " . TB_PREF . "crm_appointments a
            LEFT JOIN " . TB_PREF . "crm_appointment_types apt ON a.appointment_type_id = apt.id
            LEFT JOIN " . TB_PREF . "crm_leads l ON a.lead_id = l.id
            LEFT JOIN " . TB_PREF . "debtors_master d ON a.customer_id = d.debtor_no
            WHERE a.id = " . db_escape($id);
    return db_fetch(db_query($sql, "Could not get CRM appointment"));
}

/**
 * List appointments with optional filtering.
 *
 * @param array $filters
 * @param int   $limit
 * @param int   $offset
 * @return resource
 */
function get_crm_appointments($filters = array(), $limit = 0, $offset = 0)
{
    $sql = "SELECT a.*, apt.name AS type_name,
                   l.title AS lead_title, d.name AS customer_name
            FROM " . TB_PREF . "crm_appointments a
            LEFT JOIN " . TB_PREF . "crm_appointment_types apt ON a.appointment_type_id = apt.id
            LEFT JOIN " . TB_PREF . "crm_leads l ON a.lead_id = l.id
            LEFT JOIN " . TB_PREF . "debtors_master d ON a.customer_id = d.debtor_no
            WHERE 1=1";

    if (!empty($filters['status'])) {
        $sql .= " AND a.status = " . db_escape($filters['status']);
    }
    if (!empty($filters['lead_id'])) {
        $sql .= " AND a.lead_id = " . db_escape($filters['lead_id']);
    }
    if (!empty($filters['customer_id'])) {
        $sql .= " AND a.customer_id = " . db_escape($filters['customer_id']);
    }
    if (!empty($filters['date_from'])) {
        $sql .= " AND a.date_time >= " . db_escape($filters['date_from']);
    }
    if (!empty($filters['date_to'])) {
        $sql .= " AND a.date_time <= " . db_escape($filters['date_to'] . ' 23:59:59');
    }
    if (!empty($filters['created_by'])) {
        $sql .= " AND a.created_by = " . db_escape($filters['created_by']);
    }

    $sql .= " ORDER BY a.date_time ASC";

    if ($limit > 0) {
        $sql .= " LIMIT " . (int)$offset . ", " . (int)$limit;
    }

    return db_query($sql, "Could not get CRM appointments");
}

/**
 * Get upcoming appointments for the dashboard.
 *
 * @param int $user_id  Optional specific user.
 * @param int $days     Look-ahead days.
 * @param int $limit
 * @return resource
 */
function get_upcoming_appointments($user_id = null, $days = 7, $limit = 10)
{
    $sql = "SELECT a.*, apt.name AS type_name,
                   l.title AS lead_title, d.name AS customer_name
            FROM " . TB_PREF . "crm_appointments a
            LEFT JOIN " . TB_PREF . "crm_appointment_types apt ON a.appointment_type_id = apt.id
            LEFT JOIN " . TB_PREF . "crm_leads l ON a.lead_id = l.id
            LEFT JOIN " . TB_PREF . "debtors_master d ON a.customer_id = d.debtor_no
            WHERE a.status IN ('scheduled', 'confirmed')
              AND a.date_time BETWEEN NOW() AND DATE_ADD(NOW(), INTERVAL " . (int)$days . " DAY)";

    if ($user_id) {
        $sql .= " AND (a.created_by = " . db_escape($user_id)
            . " OR a.attendees_json LIKE " . db_escape('%"' . $user_id . '"%') . ")";
    }

    $sql .= " ORDER BY a.date_time ASC LIMIT " . (int)$limit;

    return db_query($sql, "Could not get upcoming appointments");
}

// ================================================================
// APPOINTMENT TYPES
// ================================================================

/**
 * Get all appointment types.
 *
 * @param bool $include_inactive
 * @return resource
 */
function get_crm_appointment_types($include_inactive = false)
{
    $sql = "SELECT * FROM " . TB_PREF . "crm_appointment_types";
    if (!$include_inactive) {
        $sql .= " WHERE active = 1";
    }
    $sql .= " ORDER BY name";
    return db_query($sql, "Could not get appointment types");
}

/**
 * Get a single appointment type.
 *
 * @param int $id
 * @return array|false
 */
function get_crm_appointment_type($id)
{
    $sql = "SELECT * FROM " . TB_PREF . "crm_appointment_types WHERE id = " . db_escape($id);
    return db_fetch(db_query($sql, "Could not get appointment type"));
}

/**
 * Add an appointment type.
 *
 * @param string $name
 * @param int    $default_duration
 * @param string $location
 * @param string $video_link
 * @return int
 */
function add_crm_appointment_type($name, $default_duration = 60, $location = null, $video_link = null)
{
    $sql = "INSERT INTO " . TB_PREF . "crm_appointment_types
            (name, default_duration, location, video_link) VALUES ("
        . db_escape($name) . ", "
        . db_escape($default_duration) . ", "
        . db_escape($location) . ", "
        . db_escape($video_link) . ")";
    db_query($sql, "Could not add appointment type");
    return db_insert_id();
}

/**
 * Update an appointment type.
 *
 * @param int    $id
 * @param string $name
 * @param int    $default_duration
 * @param string $location
 * @param string $video_link
 * @param int    $active
 * @return bool
 */
function update_crm_appointment_type($id, $name, $default_duration, $location, $video_link, $active)
{
    $sql = "UPDATE " . TB_PREF . "crm_appointment_types SET "
        . "name = " . db_escape($name) . ", "
        . "default_duration = " . db_escape($default_duration) . ", "
        . "location = " . db_escape($location) . ", "
        . "video_link = " . db_escape($video_link) . ", "
        . "active = " . db_escape($active)
        . " WHERE id = " . db_escape($id);
    return db_query($sql, "Could not update appointment type");
}

/**
 * Delete an appointment type.
 *
 * @param int $id
 * @return bool
 */
function delete_crm_appointment_type($id)
{
    $sql = "DELETE FROM " . TB_PREF . "crm_appointment_types WHERE id = " . db_escape($id);
    return db_query($sql, "Could not delete appointment type");
}
