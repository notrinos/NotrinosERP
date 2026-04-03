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
 * CRM Sales Teams Database Layer
 *
 * CRUD and query functions for sales teams and team members.
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

/**
 * Add a new sales team.
 *
 * @param string $name
 * @param int    $team_leader_id
 * @param string $email_alias
 * @param float  $invoicing_target
 * @param int    $use_leads
 * @return int
 */
function add_crm_sales_team($name, $team_leader_id = null, $email_alias = null,
    $invoicing_target = 0, $use_leads = 1)
{
    $sql = "INSERT INTO " . TB_PREF . "crm_sales_teams
            (name, team_leader_id, email_alias, invoicing_target, use_leads) VALUES ("
        . db_escape($name) . ", "
        . ($team_leader_id ? db_escape($team_leader_id) : "NULL") . ", "
        . db_escape($email_alias) . ", "
        . db_escape($invoicing_target) . ", "
        . db_escape($use_leads) . ")";
    db_query($sql, "Could not add sales team");
    return db_insert_id();
}

/**
 * Update a sales team.
 *
 * @param int    $id
 * @param string $name
 * @param int    $team_leader_id
 * @param string $email_alias
 * @param float  $invoicing_target
 * @param int    $use_leads
 * @param int    $active
 * @return bool
 */
function update_crm_sales_team($id, $name, $team_leader_id, $email_alias,
    $invoicing_target, $use_leads, $active)
{
    $sql = "UPDATE " . TB_PREF . "crm_sales_teams SET "
        . "name = " . db_escape($name) . ", "
        . "team_leader_id = " . ($team_leader_id ? db_escape($team_leader_id) : "NULL") . ", "
        . "email_alias = " . db_escape($email_alias) . ", "
        . "invoicing_target = " . db_escape($invoicing_target) . ", "
        . "use_leads = " . db_escape($use_leads) . ", "
        . "active = " . db_escape($active)
        . " WHERE id = " . db_escape($id);
    return db_query($sql, "Could not update sales team");
}

/**
 * Delete a sales team and its members.
 *
 * @param int $id
 * @return bool
 */
function delete_crm_sales_team($id)
{
    begin_transaction();

    $sql = "DELETE FROM " . TB_PREF . "crm_sales_team_members
            WHERE team_id = " . db_escape($id);
    db_query($sql, "Could not delete team members");

    $sql = "DELETE FROM " . TB_PREF . "crm_sales_teams WHERE id = " . db_escape($id);
    $ret = db_query($sql, "Could not delete sales team");

    commit_transaction();
    return $ret;
}

/**
 * Get a single sales team.
 *
 * @param int $id
 * @return array|false
 */
function get_crm_sales_team($id)
{
    $sql = "SELECT st.*, u.real_name AS leader_name
            FROM " . TB_PREF . "crm_sales_teams st
            LEFT JOIN " . TB_PREF . "users u ON st.team_leader_id = u.id
            WHERE st.id = " . db_escape($id);
    return db_fetch(db_query($sql, "Could not get sales team"));
}

/**
 * List all sales teams.
 *
 * @param bool $include_inactive
 * @return resource
 */
function get_crm_sales_teams($include_inactive = false)
{
    $sql = "SELECT st.*, u.real_name AS leader_name,
                   (SELECT COUNT(*) FROM " . TB_PREF . "crm_sales_team_members m
                    WHERE m.team_id = st.id) AS member_count
            FROM " . TB_PREF . "crm_sales_teams st
            LEFT JOIN " . TB_PREF . "users u ON st.team_leader_id = u.id";
    if (!$include_inactive) {
        $sql .= " WHERE st.active = 1";
    }
    $sql .= " ORDER BY st.name";
    return db_query($sql, "Could not get sales teams");
}

// ================================================================
// TEAM MEMBERS
// ================================================================

/**
 * Add a member to a sales team.
 *
 * @param int $team_id
 * @param int $user_id
 * @param int $max_leads_30days
 * @return int|false
 */
function add_crm_team_member($team_id, $user_id, $max_leads_30days = 30)
{
    $sql = "INSERT IGNORE INTO " . TB_PREF . "crm_sales_team_members
            (team_id, user_id, max_leads_30days) VALUES ("
        . db_escape($team_id) . ", "
        . db_escape($user_id) . ", "
        . db_escape($max_leads_30days) . ")";
    $ret = db_query($sql, "Could not add team member");
    return $ret ? db_insert_id() : false;
}

/**
 * Update a team member's settings.
 *
 * @param int $id              Member record ID.
 * @param int $max_leads_30days
 * @param int $skip_auto_assign
 * @param string $domain_filter JSON filter expression.
 * @return bool
 */
function update_crm_team_member($id, $max_leads_30days, $skip_auto_assign = 0, $domain_filter = null)
{
    $sql = "UPDATE " . TB_PREF . "crm_sales_team_members SET "
        . "max_leads_30days = " . db_escape($max_leads_30days) . ", "
        . "skip_auto_assign = " . db_escape($skip_auto_assign) . ", "
        . "domain_filter = " . db_escape($domain_filter)
        . " WHERE id = " . db_escape($id);
    return db_query($sql, "Could not update team member");
}

/**
 * Remove a member from a team.
 *
 * @param int $team_id
 * @param int $user_id
 * @return bool
 */
function remove_crm_team_member($team_id, $user_id)
{
    $sql = "DELETE FROM " . TB_PREF . "crm_sales_team_members
            WHERE team_id = " . db_escape($team_id)
        . " AND user_id = " . db_escape($user_id);
    return db_query($sql, "Could not remove team member");
}

/**
 * Get members of a team.
 *
 * @param int $team_id
 * @return resource
 */
function get_crm_team_members($team_id)
{
    $sql = "SELECT m.*, u.real_name, u.user_id AS login_name
            FROM " . TB_PREF . "crm_sales_team_members m
            INNER JOIN " . TB_PREF . "users u ON m.user_id = u.id
            WHERE m.team_id = " . db_escape($team_id)
        . " ORDER BY u.real_name";
    return db_query($sql, "Could not get team members");
}

/**
 * Get teams that a user belongs to.
 *
 * @param int $user_id
 * @return resource
 */
function get_user_crm_teams($user_id)
{
    $sql = "SELECT st.*
            FROM " . TB_PREF . "crm_sales_teams st
            INNER JOIN " . TB_PREF . "crm_sales_team_members m ON st.id = m.team_id
            WHERE m.user_id = " . db_escape($user_id) . " AND st.active = 1
            ORDER BY st.name";
    return db_query($sql, "Could not get user teams");
}

/**
 * Count leads assigned to a user in the last 30 days.
 *
 * @param int $user_id
 * @return int
 */
function count_user_recent_leads($user_id)
{
    $sql = "SELECT COUNT(*) as cnt FROM " . TB_PREF . "crm_leads
            WHERE assigned_to = " . db_escape($user_id)
        . " AND date_created >= DATE_SUB(NOW(), INTERVAL 30 DAY)";
    $result = db_query($sql, "Could not count recent leads");
    $row = db_fetch($result);
    return $row ? (int)$row['cnt'] : 0;
}

/**
 * Auto-assign a lead to a team member using round-robin.
 *
 * @param int $team_id
 * @return int|null  User ID of the assigned member, or null if none available.
 */
function auto_assign_crm_lead($team_id)
{
    $sql = "SELECT m.user_id, m.max_leads_30days,
                   (SELECT COUNT(*) FROM " . TB_PREF . "crm_leads l
                    WHERE l.assigned_to = m.user_id
                    AND l.date_created >= DATE_SUB(NOW(), INTERVAL 30 DAY)) AS recent_count
            FROM " . TB_PREF . "crm_sales_team_members m
            WHERE m.team_id = " . db_escape($team_id)
        . " AND m.skip_auto_assign = 0
            HAVING recent_count < m.max_leads_30days
            ORDER BY recent_count ASC, m.id ASC
            LIMIT 1";

    $result = db_query($sql, "Could not auto-assign lead");
    $row = db_fetch($result);
    return $row ? (int)$row['user_id'] : null;
}
