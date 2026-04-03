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
 * CRM Activities Database Layer
 *
 * CRUD and query functions for CRM activities (calls, emails,
 * meetings, to-dos) linked to leads, contacts, or customers.
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

/**
 * Add a new activity.
 *
 * @param array $data  Associative array of column => value.
 * @return int|false   The new activity ID.
 */
function add_crm_activity($data)
{
    $fields = array(
        'entity_type', 'entity_id', 'activity_type_id',
        'summary', 'description', 'date_scheduled', 'date_completed',
        'assigned_to', 'created_by', 'status', 'outcome',
        'duration_minutes', 'next_activity_id',
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

    $sql = "INSERT INTO " . TB_PREF . "crm_activities (" . implode(', ', $cols) . ")
            VALUES (" . implode(', ', $vals) . ")";

    begin_transaction();
    $ret = db_query($sql, "Could not add CRM activity");
    $id = $ret ? db_insert_id() : false;
    commit_transaction();
    return $id;
}

/**
 * Update an existing activity.
 *
 * @param int   $id
 * @param array $data
 * @return bool
 */
function update_crm_activity($id, $data)
{
    $fields = array(
        'entity_type', 'entity_id', 'activity_type_id',
        'summary', 'description', 'date_scheduled', 'date_completed',
        'assigned_to', 'status', 'outcome',
        'duration_minutes', 'next_activity_id',
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

    $sql = "UPDATE " . TB_PREF . "crm_activities SET "
        . implode(', ', $sets)
        . " WHERE id = " . db_escape($id);

    return db_query($sql, "Could not update CRM activity");
}

/**
 * Mark an activity as done.
 *
 * @param int    $id
 * @param string $outcome  Optional outcome note.
 * @return bool
 */
function complete_crm_activity($id, $outcome = null)
{
    $data = array(
        'status' => CRM_ACTIVITY_DONE,
        'date_completed' => date('Y-m-d H:i:s'),
    );
    if ($outcome !== null) {
        $data['outcome'] = $outcome;
    }

    begin_transaction();
    $ret = update_crm_activity($id, $data);

    // Handle activity chaining
    if ($ret) {
        $activity = get_crm_activity($id);
        if ($activity && $activity['activity_type_id']) {
            $type = get_crm_activity_type($activity['activity_type_id']);
            if ($type && $type['chaining_type'] === CRM_CHAIN_TRIGGER && $type['chained_activity_type_id']) {
                $days = (int)$type['schedule_days'];
                $schedule_from = ($type['schedule_type'] === 'deadline')
                    ? $activity['date_scheduled']
                    : date('Y-m-d H:i:s');
                $new_date = date('Y-m-d H:i:s', strtotime($schedule_from . " +$days days"));

                $new_activity_id = add_crm_activity(array(
                    'entity_type' => $activity['entity_type'],
                    'entity_id' => $activity['entity_id'],
                    'activity_type_id' => $type['chained_activity_type_id'],
                    'summary' => $type['default_summary'] ?: 'Follow-up',
                    'date_scheduled' => $new_date,
                    'assigned_to' => $activity['assigned_to'],
                    'created_by' => $activity['created_by'],
                    'status' => CRM_ACTIVITY_PLANNED,
                ));

                if ($new_activity_id) {
                    $sql = "UPDATE " . TB_PREF . "crm_activities SET
                            next_activity_id = " . db_escape($new_activity_id)
                        . " WHERE id = " . db_escape($id);
                    db_query($sql, "Could not link chained activity");
                }
            }
        }
    }

    commit_transaction();
    return $ret;
}

/**
 * Cancel an activity.
 *
 * @param int $id
 * @return bool
 */
function cancel_crm_activity($id)
{
    return update_crm_activity($id, array('status' => CRM_ACTIVITY_CANCELLED));
}

/**
 * Delete an activity.
 *
 * @param int $id
 * @return bool
 */
function delete_crm_activity($id)
{
    $sql = "DELETE FROM " . TB_PREF . "crm_activities WHERE id = " . db_escape($id);
    return db_query($sql, "Could not delete CRM activity");
}

/**
 * Retrieve a single activity by ID.
 *
 * @param int $id
 * @return array|false
 */
function get_crm_activity($id)
{
    $sql = "SELECT a.*, at.name AS type_name, at.category AS type_category,
                   at.icon AS type_icon
            FROM " . TB_PREF . "crm_activities a
            LEFT JOIN " . TB_PREF . "crm_activity_types at ON a.activity_type_id = at.id
            WHERE a.id = " . db_escape($id);
    return db_fetch(db_query($sql, "Could not get CRM activity"));
}

/**
 * Retrieve activities with optional filtering.
 *
 * @param array $filters
 * @param int   $limit
 * @param int   $offset
 * @return resource
 */
function get_crm_activities($filters = array(), $limit = 0, $offset = 0)
{
    $sql = "SELECT a.*, at.name AS type_name, at.category AS type_category,
                   at.icon AS type_icon
            FROM " . TB_PREF . "crm_activities a
            LEFT JOIN " . TB_PREF . "crm_activity_types at ON a.activity_type_id = at.id
            WHERE 1=1";

    if (!empty($filters['entity_type'])) {
        $sql .= " AND a.entity_type = " . db_escape($filters['entity_type']);
    }
    if (!empty($filters['entity_id'])) {
        $sql .= " AND a.entity_id = " . db_escape($filters['entity_id']);
    }
    if (!empty($filters['activity_type_id'])) {
        $sql .= " AND a.activity_type_id = " . db_escape($filters['activity_type_id']);
    }
    if (!empty($filters['assigned_to'])) {
        $sql .= " AND a.assigned_to = " . db_escape($filters['assigned_to']);
    }
    if (!empty($filters['status'])) {
        $sql .= " AND a.status = " . db_escape($filters['status']);
    }
    if (!empty($filters['date_from'])) {
        $sql .= " AND a.date_scheduled >= " . db_escape($filters['date_from']);
    }
    if (!empty($filters['date_to'])) {
        $sql .= " AND a.date_scheduled <= " . db_escape($filters['date_to'] . ' 23:59:59');
    }
    if (!empty($filters['category'])) {
        $sql .= " AND at.category = " . db_escape($filters['category']);
    }

    $sql .= " ORDER BY a.date_scheduled ASC";

    if ($limit > 0) {
        $sql .= " LIMIT " . (int)$offset . ", " . (int)$limit;
    }

    return db_query($sql, "Could not get CRM activities");
}

/**
 * Get upcoming activities (for dashboard / reminders).
 *
 * @param int    $user_id  Filter to specific user.
 * @param int    $days     Look-ahead days.
 * @param int    $limit
 * @return resource
 */
function get_upcoming_crm_activities($user_id = null, $days = 7, $limit = 20)
{
    $sql = "SELECT a.*, at.name AS type_name, at.category AS type_category,
                   at.icon AS type_icon,
                   l.title AS lead_title
            FROM " . TB_PREF . "crm_activities a
            LEFT JOIN " . TB_PREF . "crm_activity_types at ON a.activity_type_id = at.id
            LEFT JOIN " . TB_PREF . "crm_leads l ON (a.entity_type = 'lead' AND a.entity_id = l.id)
            WHERE a.status IN ('planned', 'overdue')
              AND a.date_scheduled <= DATE_ADD(NOW(), INTERVAL " . (int)$days . " DAY)";

    if ($user_id) {
        $sql .= " AND a.assigned_to = " . db_escape($user_id);
    }

    $sql .= " ORDER BY a.date_scheduled ASC LIMIT " . (int)$limit;

    return db_query($sql, "Could not get upcoming activities");
}

/**
 * Get overdue activities and mark them.
 *
 * @return int  Number of activities marked overdue.
 */
function mark_overdue_crm_activities()
{
    $sql = "UPDATE " . TB_PREF . "crm_activities SET status = 'overdue'
            WHERE status = 'planned' AND date_scheduled < NOW()";
    db_query($sql, "Could not mark overdue activities");
    return db_num_affected_rows();
}

/**
 * Count activities by status for an entity.
 *
 * @param string $entity_type
 * @param int    $entity_id
 * @return array  status => count
 */
function count_crm_activities_by_status($entity_type, $entity_id)
{
    $sql = "SELECT status, COUNT(*) as cnt
            FROM " . TB_PREF . "crm_activities
            WHERE entity_type = " . db_escape($entity_type)
        . " AND entity_id = " . db_escape($entity_id)
        . " GROUP BY status";
    $result = db_query($sql, "Could not count activities");
    $counts = array();
    while ($row = db_fetch($result)) {
        $counts[$row['status']] = (int)$row['cnt'];
    }
    return $counts;
}

// ================================================================
// ACTIVITY TYPES
// ================================================================

/**
 * Get all activity types.
 *
 * @param bool $include_inactive
 * @return resource
 */
function get_crm_activity_types($include_inactive = false)
{
    $sql = "SELECT * FROM " . TB_PREF . "crm_activity_types";
    if (!$include_inactive) {
        $sql .= " WHERE active = 1";
    }
    $sql .= " ORDER BY name";
    return db_query($sql, "Could not get activity types");
}

/**
 * Get a single activity type.
 *
 * @param int $id
 * @return array|false
 */
function get_crm_activity_type($id)
{
    $sql = "SELECT * FROM " . TB_PREF . "crm_activity_types WHERE id = " . db_escape($id);
    return db_fetch(db_query($sql, "Could not get activity type"));
}

/**
 * Add an activity type.
 *
 * @param string $name
 * @param string $category
 * @param string $icon
 * @param string $chaining_type
 * @param int    $chained_activity_type_id
 * @param int    $schedule_days
 * @param string $schedule_type
 * @return int
 */
function add_crm_activity_type($name, $category, $icon = null, $chaining_type = 'none',
    $chained_activity_type_id = null, $schedule_days = 0, $schedule_type = 'completion')
{
    $sql = "INSERT INTO " . TB_PREF . "crm_activity_types
            (name, category, icon, chaining_type, chained_activity_type_id,
             schedule_days, schedule_type) VALUES ("
        . db_escape($name) . ", "
        . db_escape($category) . ", "
        . db_escape($icon) . ", "
        . db_escape($chaining_type) . ", "
        . ($chained_activity_type_id ? db_escape($chained_activity_type_id) : "NULL") . ", "
        . db_escape($schedule_days) . ", "
        . db_escape($schedule_type) . ")";
    db_query($sql, "Could not add activity type");
    return db_insert_id();
}

/**
 * Update an activity type.
 *
 * @param int    $id
 * @param string $name
 * @param string $category
 * @param string $icon
 * @param string $chaining_type
 * @param int    $chained_activity_type_id
 * @param int    $schedule_days
 * @param string $schedule_type
 * @param int    $active
 * @return bool
 */
function update_crm_activity_type($id, $name, $category, $icon, $chaining_type,
    $chained_activity_type_id, $schedule_days, $schedule_type, $active)
{
    $sql = "UPDATE " . TB_PREF . "crm_activity_types SET "
        . "name = " . db_escape($name) . ", "
        . "category = " . db_escape($category) . ", "
        . "icon = " . db_escape($icon) . ", "
        . "chaining_type = " . db_escape($chaining_type) . ", "
        . "chained_activity_type_id = " . ($chained_activity_type_id ? db_escape($chained_activity_type_id) : "NULL") . ", "
        . "schedule_days = " . db_escape($schedule_days) . ", "
        . "schedule_type = " . db_escape($schedule_type) . ", "
        . "active = " . db_escape($active)
        . " WHERE id = " . db_escape($id);
    return db_query($sql, "Could not update activity type");
}

/**
 * Delete an activity type.
 *
 * @param int $id
 * @return bool
 */
function delete_crm_activity_type($id)
{
    $sql = "DELETE FROM " . TB_PREF . "crm_activity_types WHERE id = " . db_escape($id);
    return db_query($sql, "Could not delete activity type");
}

// ================================================================
// ACTIVITY PLANS
// ================================================================

/**
 * Get all activity plans.
 *
 * @param bool $include_inactive
 * @return resource
 */
function get_crm_activity_plans($include_inactive = false)
{
    $sql = "SELECT * FROM " . TB_PREF . "crm_activity_plans";
    if (!$include_inactive) {
        $sql .= " WHERE active = 1";
    }
    $sql .= " ORDER BY plan_name";
    return db_query($sql, "Could not get activity plans");
}

/**
 * Get a single plan with its lines.
 *
 * @param int $id
 * @return array|false
 */
function get_crm_activity_plan($id)
{
    $sql = "SELECT * FROM " . TB_PREF . "crm_activity_plans WHERE id = " . db_escape($id);
    $plan = db_fetch(db_query($sql, "Could not get activity plan"));
    if ($plan) {
        $sql = "SELECT pl.*, at.name AS type_name
                FROM " . TB_PREF . "crm_activity_plan_lines pl
                LEFT JOIN " . TB_PREF . "crm_activity_types at ON pl.activity_type_id = at.id
                WHERE pl.plan_id = " . db_escape($id) . " ORDER BY pl.sequence";
        $result = db_query($sql, "Could not get plan lines");
        $plan['lines'] = array();
        while ($row = db_fetch($result)) {
            $plan['lines'][] = $row;
        }
    }
    return $plan;
}

/**
 * Add an activity plan.
 *
 * @param string $plan_name
 * @param string $description
 * @param array  $lines  Array of line arrays.
 * @return int
 */
function add_crm_activity_plan($plan_name, $description, $lines = array())
{
    begin_transaction();

    $sql = "INSERT INTO " . TB_PREF . "crm_activity_plans (plan_name, description)
            VALUES (" . db_escape($plan_name) . ", " . db_escape($description) . ")";
    db_query($sql, "Could not add activity plan");
    $plan_id = db_insert_id();

    foreach ($lines as $seq => $line) {
        add_crm_activity_plan_line($plan_id, $line, $seq);
    }

    commit_transaction();
    return $plan_id;
}

/**
 * Add a single plan line.
 *
 * @param int   $plan_id
 * @param array $line
 * @param int   $sequence
 * @return int
 */
function add_crm_activity_plan_line($plan_id, $line, $sequence = 0)
{
    $sql = "INSERT INTO " . TB_PREF . "crm_activity_plan_lines
            (plan_id, activity_type_id, summary, assignment, assigned_to,
             interval_value, interval_unit, trigger_timing, sequence) VALUES ("
        . db_escape($plan_id) . ", "
        . db_escape($line['activity_type_id']) . ", "
        . db_escape(@$line['summary']) . ", "
        . db_escape(@$line['assignment'] ?: 'ask_at_launch') . ", "
        . (@$line['assigned_to'] ? db_escape($line['assigned_to']) : "NULL") . ", "
        . db_escape(@$line['interval_value'] ?: 0) . ", "
        . db_escape(@$line['interval_unit'] ?: 'days') . ", "
        . db_escape(@$line['trigger_timing'] ?: 'after') . ", "
        . db_escape($sequence) . ")";
    db_query($sql, "Could not add plan line");
    return db_insert_id();
}

/**
 * Update an activity plan.
 *
 * @param int    $id
 * @param string $plan_name
 * @param string $description
 * @param int    $active
 * @param array  $lines  Replace all lines.
 * @return bool
 */
function update_crm_activity_plan($id, $plan_name, $description, $active, $lines = null)
{
    begin_transaction();

    $sql = "UPDATE " . TB_PREF . "crm_activity_plans SET "
        . "plan_name = " . db_escape($plan_name) . ", "
        . "description = " . db_escape($description) . ", "
        . "active = " . db_escape($active)
        . " WHERE id = " . db_escape($id);
    $ret = db_query($sql, "Could not update activity plan");

    if ($ret && $lines !== null) {
        $sql = "DELETE FROM " . TB_PREF . "crm_activity_plan_lines
                WHERE plan_id = " . db_escape($id);
        db_query($sql, "Could not delete old plan lines");

        foreach ($lines as $seq => $line) {
            add_crm_activity_plan_line($id, $line, $seq);
        }
    }

    commit_transaction();
    return $ret;
}

/**
 * Delete an activity plan and its lines.
 *
 * @param int $id
 * @return bool
 */
function delete_crm_activity_plan($id)
{
    begin_transaction();
    $sql = "DELETE FROM " . TB_PREF . "crm_activity_plan_lines
            WHERE plan_id = " . db_escape($id);
    db_query($sql, "Could not delete plan lines");

    $sql = "DELETE FROM " . TB_PREF . "crm_activity_plans WHERE id = " . db_escape($id);
    $ret = db_query($sql, "Could not delete activity plan");
    commit_transaction();
    return $ret;
}

/**
 * Launch an activity plan for an entity, creating scheduled activities.
 *
 * @param int    $plan_id
 * @param string $entity_type
 * @param int    $entity_id
 * @param int    $default_assigned_to  User to assign when line says ask_at_launch.
 * @return array  Created activity IDs.
 */
function launch_crm_activity_plan($plan_id, $entity_type, $entity_id, $default_assigned_to = null)
{
    $plan = get_crm_activity_plan($plan_id);
    if (!$plan || empty($plan['lines'])) {
        return array();
    }

    $created_ids = array();
    $base_date = date('Y-m-d H:i:s');

    begin_transaction();

    foreach ($plan['lines'] as $line) {
        $interval = (int)$line['interval_value'];
        $unit = $line['interval_unit'];
        $timing = $line['trigger_timing'];

        if ($timing === 'before') {
            $schedule_date = date('Y-m-d H:i:s', strtotime("$base_date -$interval $unit"));
        } else {
            $schedule_date = date('Y-m-d H:i:s', strtotime("$base_date +$interval $unit"));
        }

        $assigned = ($line['assignment'] === 'default_user' && $line['assigned_to'])
            ? $line['assigned_to']
            : $default_assigned_to;

        $activity_id = add_crm_activity(array(
            'entity_type' => $entity_type,
            'entity_id' => $entity_id,
            'activity_type_id' => $line['activity_type_id'],
            'summary' => $line['summary'] ?: 'Planned activity',
            'date_scheduled' => $schedule_date,
            'assigned_to' => $assigned,
            'created_by' => $default_assigned_to,
            'status' => CRM_ACTIVITY_PLANNED,
        ));

        if ($activity_id) {
            $created_ids[] = $activity_id;
        }
    }

    commit_transaction();
    return $created_ids;
}
