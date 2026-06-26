<?php
/**
 * Entity class for crm_campaigns and related tables.
 *
 * Covers: crm_campaigns, crm_campaign_leads, crm_email_templates, crm_campaign_emails.
 *
 * Static API: ::find(), ::all(), ::all_db_resource(), ::create(), ::modify(), ::remove().
 *
 * ::remove() overrides the base to also delete campaign leads and emails (multi-table transaction).
 * ::create() returns the inserted ID (int|false), not just bool.
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class crm_campaigns_entity extends Entity {
	function __construct() {
		parent::__construct(
			'crm_campaigns',
			array('name', 'campaign_type', 'start_date', 'end_date', 'status', 'budget',
				'assigned_to', 'lead_source_id', 'description', 'created_by', 'created_at', 'id'),
			'id'
		);
	}

	// ================================================================
	// CAMPAIGNS
	// ================================================================

	/**
	 * Insert a campaign and return the new ID.
	 *
	 * @param array $data
	 * @return int|false
	 */
	public static function create($data) {
		$e = new static();
		$result = $e->insert($data);
		return $result ? db_insert_id() : false;
	}

	/**
	 * Update a campaign.
	 *
	 * @param int   $id
	 * @param array $data
	 * @return bool
	 */
	public static function modify($id, $data) {
		$e = new static();
		return $e->update($id, $data);
	}

	/** Override remove to also delete campaign leads and emails. */
	public static function remove($key) {
		begin_transaction();
		$e = new static();
		// Delete campaign leads
		db_query("DELETE FROM " . TB_PREF . "crm_campaign_leads WHERE campaign_id = " . db_escape($key),
			"Could not delete campaign leads");
		// Delete campaign emails
		db_query("DELETE FROM " . TB_PREF . "crm_campaign_emails WHERE campaign_id = " . db_escape($key),
			"Could not delete campaign emails");
		$result = $e->delete($key);
		if ($result) commit_transaction(); else rollback_transaction();
		return $result;
	}

	/**
	 * Get campaign statistics (leads enrolled, converted, revenue).
	 *
	 * @param int $campaign_id
	 * @return array
	 */
	public static function stats($campaign_id) {
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
	public static function enroll_lead($campaign_id, $lead_id) {
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
	public static function unenroll_lead($campaign_id, $lead_id) {
		$sql = "DELETE FROM " . TB_PREF . "crm_campaign_leads
				WHERE campaign_id = " . db_escape($campaign_id)
			. " AND lead_id = " . db_escape($lead_id);
		return db_query($sql, "Could not unenroll lead from campaign");
	}

	/**
	 * Get leads enrolled in a campaign (DB resource).
	 *
	 * @param int $campaign_id
	 * @return resource
	 */
	public static function enrolled_leads($campaign_id) {
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
	 * Get all email templates (DB resource).
	 *
	 * @param bool $include_inactive
	 * @return resource
	 */
	public static function email_templates($include_inactive = false) {
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
	public static function email_template($id) {
		$sql = "SELECT * FROM " . TB_PREF . "crm_email_templates WHERE id = " . db_escape($id);
		return db_fetch(db_query($sql, "Could not get email template"));
	}

	/**
	 * Add an email template.
	 *
	 * @param array $data Keys: name, subject, body_html, category, variables_json
	 * @return int|false
	 */
	public static function create_email_template($data) {
		$sql = "INSERT INTO " . TB_PREF . "crm_email_templates
				(name, subject, body_html, category, variables_json) VALUES ("
			. db_escape($data['name']) . ", "
			. db_escape($data['subject']) . ", "
			. db_escape($data['body_html']) . ", "
			. db_escape(@$data['category']) . ", "
			. db_escape(@$data['variables_json']) . ")";
		db_query($sql, "Could not add email template");
		return db_insert_id();
	}

	/**
	 * Update an email template.
	 *
	 * @param int   $id
	 * @param array $data Keys: name, subject, body_html, category, variables_json, active
	 * @return bool
	 */
	public static function modify_email_template($id, $data) {
		$sql = "UPDATE " . TB_PREF . "crm_email_templates SET "
			. "name = " . db_escape($data['name']) . ", "
			. "subject = " . db_escape($data['subject']) . ", "
			. "body_html = " . db_escape($data['body_html']) . ", "
			. "category = " . db_escape($data['category']) . ", "
			. "variables_json = " . db_escape($data['variables_json']) . ", "
			. "active = " . db_escape($data['active'])
			. " WHERE id = " . db_escape($id);
		return db_query($sql, "Could not update email template");
	}

	/**
	 * Delete an email template.
	 *
	 * @param int $id
	 * @return bool
	 */
	public static function remove_email_template($id) {
		$sql = "DELETE FROM " . TB_PREF . "crm_email_templates WHERE id = " . db_escape($id);
		return db_query($sql, "Could not delete email template");
	}

	// ================================================================
	// CAMPAIGN EMAILS (schedule)
	// ================================================================

	/**
	 * Get email schedule for a campaign (DB resource).
	 *
	 * @param int $campaign_id
	 * @return resource
	 */
	public static function campaign_emails($campaign_id) {
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
	 * @return int|false
	 */
	public static function create_campaign_email($campaign_id, $email_template_id, $day_offset, $subject, $body, $sequence) {
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
	 * Delete one campaign email step by id.
	 *
	 * @param int $id
	 * @param int $campaign_id
	 * @return bool
	 */
	public static function remove_campaign_email($id, $campaign_id) {
		$sql = "DELETE FROM " . TB_PREF . "crm_campaign_emails
				WHERE id = " . db_escape($id)
			. " AND campaign_id = " . db_escape($campaign_id);
		return db_query($sql, "Could not delete campaign email");
	}
}
