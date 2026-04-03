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
 * CRM Campaigns Database Layer
 *
 * CRUD and query functions for marketing campaigns, email templates,
 * campaign emails, and campaign-lead enrollment.
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

// ================================================================
// CAMPAIGNS
// ================================================================

/**
 * Add a new campaign.
 *
 * @param array $data
 * @return int|false
 */
function add_crm_campaign($data)
{
    $sql = "INSERT INTO " . TB_PREF . "crm_campaigns
            (name, campaign_type, start_date, end_date, status, budget,
             assigned_to, lead_source_id, description, created_by) VALUES ("
        . db_escape($data['name']) . ", "
        . db_escape(@$data['campaign_type']) . ", "
        . (!empty($data['start_date']) ? db_escape($data['start_date']) : "NULL") . ", "
        . (!empty($data['end_date']) ? db_escape($data['end_date']) : "NULL") . ", "
        . db_escape(@$data['status'] ?: CRM_CAMPAIGN_DRAFT) . ", "
        . db_escape(@$data['budget'] ?: 0) . ", "
        . (@$data['assigned_to'] ? db_escape($data['assigned_to']) : "NULL") . ", "
        . (@$data['lead_source_id'] ? db_escape($data['lead_source_id']) : "NULL") . ", "
        . db_escape(@$data['description']) . ", "
        . (@$data['created_by'] ? db_escape($data['created_by']) : "NULL") . ")";

    db_query($sql, "Could not add CRM campaign");
    return db_insert_id();
}

/**
 * Update a campaign.
 *
 * @param int   $id
 * @param array $data
 * @return bool
 */
function update_crm_campaign($id, $data)
{
    $fields = array(
        'name', 'campaign_type', 'start_date', 'end_date',
        'status', 'budget', 'assigned_to', 'lead_source_id', 'description',
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

    $sql = "UPDATE " . TB_PREF . "crm_campaigns SET "
        . implode(', ', $sets)
        . " WHERE id = " . db_escape($id);

    return db_query($sql, "Could not update CRM campaign");
}

/**
 * Delete a campaign and associated records.
 *
 * @param int $id
 * @return bool
 */
function delete_crm_campaign($id)
{
    begin_transaction();

    $sql = "DELETE FROM " . TB_PREF . "crm_campaign_leads
            WHERE campaign_id = " . db_escape($id);
    db_query($sql, "Could not delete campaign leads");

    $sql = "DELETE FROM " . TB_PREF . "crm_campaign_emails
            WHERE campaign_id = " . db_escape($id);
    db_query($sql, "Could not delete campaign emails");

    $sql = "DELETE FROM " . TB_PREF . "crm_campaigns WHERE id = " . db_escape($id);
    $ret = db_query($sql, "Could not delete CRM campaign");

    commit_transaction();
    return $ret;
}

/**
 * Retrieve a single campaign by ID.
 *
 * @param int $id
 * @return array|false
 */
function get_crm_campaign($id)
{
    $sql = "SELECT c.*, src.name AS source_name
            FROM " . TB_PREF . "crm_campaigns c
            LEFT JOIN " . TB_PREF . "crm_lead_sources src ON c.lead_source_id = src.id
            WHERE c.id = " . db_escape($id);
    return db_fetch(db_query($sql, "Could not get CRM campaign"));
}

/**
 * List campaigns with optional filtering.
 *
 * @param array $filters
 * @param int   $limit
 * @param int   $offset
 * @return resource
 */
function get_crm_campaigns($filters = array(), $limit = 0, $offset = 0)
{
    $sql = "SELECT c.*, src.name AS source_name,
                   (SELECT COUNT(*) FROM " . TB_PREF . "crm_campaign_leads cl
                    WHERE cl.campaign_id = c.id) AS lead_count
            FROM " . TB_PREF . "crm_campaigns c
            LEFT JOIN " . TB_PREF . "crm_lead_sources src ON c.lead_source_id = src.id
            WHERE 1=1";

    if (!empty($filters['status'])) {
        $sql .= " AND c.status = " . db_escape($filters['status']);
    }
    if (!empty($filters['campaign_type'])) {
        $sql .= " AND c.campaign_type = " . db_escape($filters['campaign_type']);
    }
    if (!empty($filters['assigned_to'])) {
        $sql .= " AND c.assigned_to = " . db_escape($filters['assigned_to']);
    }
    if (!empty($filters['search'])) {
        $search = db_escape('%' . $filters['search'] . '%');
        $sql .= " AND (c.name LIKE $search OR c.description LIKE $search)";
    }

    $sql .= " ORDER BY c.created_date DESC";

    if ($limit > 0) {
        $sql .= " LIMIT " . (int)$offset . ", " . (int)$limit;
    }

    return db_query($sql, "Could not get CRM campaigns");
}

/**
 * Get campaign statistics (leads enrolled, converted, revenue).
 *
 * @param int $campaign_id
 * @return array
 */
function get_crm_campaign_stats($campaign_id)
{
    $stats = array(
        'total_leads' => 0,
        'converted' => 0,
        'total_revenue' => 0,
    );

    $sql = "SELECT COUNT(*) as total,
                   SUM(CASE WHEN l.lead_status = 'converted' THEN 1 ELSE 0 END) as converted,
                   SUM(CASE WHEN l.lead_status = 'converted' THEN l.expected_revenue ELSE 0 END) as revenue
            FROM " . TB_PREF . "crm_campaign_leads cl
            INNER JOIN " . TB_PREF . "crm_leads l ON cl.lead_id = l.id
            WHERE cl.campaign_id = " . db_escape($campaign_id);
    $result = db_query($sql, "Could not get campaign stats");
    $row = db_fetch($result);
    if ($row) {
        $stats['total_leads'] = (int)$row['total'];
        $stats['converted'] = (int)$row['converted'];
        $stats['total_revenue'] = (float)$row['revenue'];
    }

    return $stats;
}

// ================================================================
// CAMPAIGN LEADS
// ================================================================

/**
 * Enroll a lead in a campaign.
 *
 * @param int $campaign_id
 * @param int $lead_id
 * @return bool
 */
function enroll_crm_campaign_lead($campaign_id, $lead_id)
{
    $sql = "INSERT IGNORE INTO " . TB_PREF . "crm_campaign_leads
            (campaign_id, lead_id) VALUES ("
        . db_escape($campaign_id) . ", "
        . db_escape($lead_id) . ")";
    return db_query($sql, "Could not enroll lead in campaign");
}

/**
 * Remove a lead from a campaign.
 *
 * @param int $campaign_id
 * @param int $lead_id
 * @return bool
 */
function unenroll_crm_campaign_lead($campaign_id, $lead_id)
{
    $sql = "DELETE FROM " . TB_PREF . "crm_campaign_leads
            WHERE campaign_id = " . db_escape($campaign_id)
        . " AND lead_id = " . db_escape($lead_id);
    return db_query($sql, "Could not unenroll lead from campaign");
}

/**
 * Get leads enrolled in a campaign.
 *
 * @param int $campaign_id
 * @return resource
 */
function get_crm_campaign_leads($campaign_id)
{
    $sql = "SELECT cl.*, l.title, l.company_name, l.contact_name, l.email,
                   l.lead_status, l.is_opportunity
            FROM " . TB_PREF . "crm_campaign_leads cl
            INNER JOIN " . TB_PREF . "crm_leads l ON cl.lead_id = l.id
            WHERE cl.campaign_id = " . db_escape($campaign_id)
        . " ORDER BY cl.enrolled_date DESC";
    return db_query($sql, "Could not get campaign leads");
}

// ================================================================
// EMAIL TEMPLATES
// ================================================================

/**
 * Get all email templates.
 *
 * @param bool $include_inactive
 * @return resource
 */
function get_crm_email_templates($include_inactive = false)
{
    $sql = "SELECT * FROM " . TB_PREF . "crm_email_templates";
    if (!$include_inactive) {
        $sql .= " WHERE active = 1";
    }
    $sql .= " ORDER BY name";
    return db_query($sql, "Could not get email templates");
}

/**
 * Get a single email template.
 *
 * @param int $id
 * @return array|false
 */
function get_crm_email_template($id)
{
    $sql = "SELECT * FROM " . TB_PREF . "crm_email_templates WHERE id = " . db_escape($id);
    return db_fetch(db_query($sql, "Could not get email template"));
}

/**
 * Add an email template.
 *
 * @param string $name
 * @param string $subject
 * @param string $body_html
 * @param string $category
 * @param string $variables_json
 * @return int
 */
function add_crm_email_template($name, $subject, $body_html, $category = null, $variables_json = null)
{
    $sql = "INSERT INTO " . TB_PREF . "crm_email_templates
            (name, subject, body_html, category, variables_json) VALUES ("
        . db_escape($name) . ", "
        . db_escape($subject) . ", "
        . db_escape($body_html) . ", "
        . db_escape($category) . ", "
        . db_escape($variables_json) . ")";
    db_query($sql, "Could not add email template");
    return db_insert_id();
}

/**
 * Update an email template.
 *
 * @param int    $id
 * @param string $name
 * @param string $subject
 * @param string $body_html
 * @param string $category
 * @param string $variables_json
 * @param int    $active
 * @return bool
 */
function update_crm_email_template($id, $name, $subject, $body_html, $category, $variables_json, $active)
{
    $sql = "UPDATE " . TB_PREF . "crm_email_templates SET "
        . "name = " . db_escape($name) . ", "
        . "subject = " . db_escape($subject) . ", "
        . "body_html = " . db_escape($body_html) . ", "
        . "category = " . db_escape($category) . ", "
        . "variables_json = " . db_escape($variables_json) . ", "
        . "active = " . db_escape($active)
        . " WHERE id = " . db_escape($id);
    return db_query($sql, "Could not update email template");
}

/**
 * Delete an email template.
 *
 * @param int $id
 * @return bool
 */
function delete_crm_email_template($id)
{
    $sql = "DELETE FROM " . TB_PREF . "crm_email_templates WHERE id = " . db_escape($id);
    return db_query($sql, "Could not delete email template");
}

// ================================================================
// CAMPAIGN EMAILS (schedule)
// ================================================================

/**
 * Get email schedule for a campaign.
 *
 * @param int $campaign_id
 * @return resource
 */
function get_crm_campaign_emails($campaign_id)
{
    $sql = "SELECT ce.*, et.name AS template_name
            FROM " . TB_PREF . "crm_campaign_emails ce
            LEFT JOIN " . TB_PREF . "crm_email_templates et ON ce.email_template_id = et.id
            WHERE ce.campaign_id = " . db_escape($campaign_id)
        . " ORDER BY ce.sequence";
    return db_query($sql, "Could not get campaign emails");
}

/**
 * Add a campaign email step.
 *
 * @param int    $campaign_id
 * @param int    $email_template_id
 * @param int    $day_offset
 * @param string $subject
 * @param string $body
 * @param int    $sequence
 * @return int
 */
function add_crm_campaign_email($campaign_id, $email_template_id, $day_offset, $subject, $body, $sequence)
{
    $sql = "INSERT INTO " . TB_PREF . "crm_campaign_emails
            (campaign_id, email_template_id, day_offset, subject, body, sequence) VALUES ("
        . db_escape($campaign_id) . ", "
        . ($email_template_id ? db_escape($email_template_id) : "NULL") . ", "
        . db_escape($day_offset) . ", "
        . db_escape($subject) . ", "
        . db_escape($body) . ", "
        . db_escape($sequence) . ")";
    db_query($sql, "Could not add campaign email");
    return db_insert_id();
}

/**
 * Delete all email steps for a campaign.
 *
 * @param int $campaign_id
 * @return bool
 */
function delete_crm_campaign_emails($campaign_id)
{
    $sql = "DELETE FROM " . TB_PREF . "crm_campaign_emails
            WHERE campaign_id = " . db_escape($campaign_id);
    return db_query($sql, "Could not delete campaign emails");
}
