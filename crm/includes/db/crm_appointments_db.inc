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
 * JOIN-based query helpers for the crm_appointments table.
 * Simple single-table CRUD has been migrated to crm_appointments_entity
 * and crm_appointment_types_entity.
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

/**
 * List appointments with optional filtering (JOINs with types, leads, customers).
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

