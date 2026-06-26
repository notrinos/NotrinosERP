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
 * Entity class for crm_leads.
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 *
 * Complex JOIN queries are provided as static methods on this class.
 * Tag management and scoring config wrappers remain in crm_leads_db.inc as thin helpers.
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class crm_leads_entity extends Entity {
	function __construct() {
		parent::__construct(
			'crm_leads',
			array('lead_ref', 'title', 'company_name', 'contact_name',
				'email', 'phone', 'mobile', 'website',
				'address', 'city', 'state', 'country', 'postal_code',
				'lead_source_id', 'lead_status', 'priority',
				'is_opportunity', 'stage_id', 'assigned_to', 'sales_team_id',
				'expected_revenue', 'probability', 'expected_close_date',
				'date_qualified', 'date_converted', 'date_won', 'date_lost',
				'lost_reason_id', 'lost_notes',
				'linked_customer_id', 'linked_person_id', 'notes', 'inactive',
				'created_by', 'id'),
			'id'
		);
	}

	// ================================================================
	// JOIN-RICH QUERIES
	// ================================================================

	/**
	 * Find a single lead with JOINed source, stage, team, lost_reason names.
	 *
	 * @param int $id
	 * @return array|null
	 */
	public static function find_joined($id) {
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
	 * Get leads with JOINs, optional filtering, limit/offset.
	 *
	 * @param array $filters
	 * @param int   $limit
	 * @param int   $offset
	 * @return resource  DB result (use db_fetch())
	 */
	public static function get_all_joined($filters = array(), $limit = 0, $offset = 0) {
		$sql = "SELECT l.*, src.name AS source_name, stg.name AS stage_name,
					   stg.probability AS stage_probability, st.name AS team_name,
					   u.real_name AS assigned_name
				FROM " . TB_PREF . "crm_leads l
				LEFT JOIN " . TB_PREF . "crm_lead_sources src ON l.lead_source_id = src.id
				LEFT JOIN " . TB_PREF . "crm_sales_stages stg ON l.stage_id = stg.id
				LEFT JOIN " . TB_PREF . "crm_sales_teams st ON l.sales_team_id = st.id
				LEFT JOIN " . TB_PREF . "users u ON l.assigned_to = u.id
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
		if (isset($filters['priority'])) {
			$sql .= " AND l.priority = " . db_escape($filters['priority']);
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
	 * Count leads matching filter criteria.
	 *
	 * @param array $filters
	 * @return int
	 */
	public static function count_joined($filters = array()) {
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
		if (isset($filters['priority'])) {
			$sql .= " AND l.priority = " . db_escape($filters['priority']);
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

	// ================================================================
	// CASCADE DELETE
	// ================================================================

	/**
	 * Delete a lead (soft or hard), with cascade cleanup.
	 *
	 * @param int  $id
	 * @param bool $hard  If true, permanently delete with cascade.
	 * @return bool
	 */
	public static function delete_cascade($id, $hard = false) {
		begin_transaction();

		if ($hard) {
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

	// ================================================================
	// TAG HELPERS (shared 0_tags / 0_tag_associations, type=TAG_CRM)
	// ================================================================

	/**
	 * Replace all tags for this lead.
	 *
	 * @param int   $lead_id
	 * @param array $tag_ids
	 * @return bool
	 */
	public static function update_tags($lead_id, $tag_ids) {
		global $path_to_root;
		include_once($path_to_root . '/admin/db/tags_db.inc');
		$record_id = 'lead:' . (int)$lead_id;
		update_tag_associations(TAG_CRM, $record_id, $tag_ids);
		return true;
	}

	/**
	 * Get tags for this lead.
	 *
	 * @param int $lead_id
	 * @return array
	 */
	public static function get_tags($lead_id) {
		global $path_to_root;
		include_once($path_to_root . '/admin/db/tags_db.inc');
		$record_id = 'lead:' . (int)$lead_id;
		$result = get_tags_associated_with_record(TAG_CRM, $record_id);
		$tags = array();
		while ($row = db_fetch($result)) {
			$tags[] = $row;
		}
		return $tags;
	}

	// ================================================================
	// COMPLEX BUSINESS LOGIC
	// ================================================================

	/**
	 * Convert a lead to an opportunity (set is_opportunity=1 and assign stage).
	 *
	 * @param int $lead_id
	 * @param int $stage_id  Initial pipeline stage.
	 * @return bool
	 */
	public static function convert_to_opportunity($lead_id, $stage_id = null) {
		$lead = self::find_joined($lead_id);
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
			$sql = "SELECT id, probability FROM " . TB_PREF . "crm_sales_stages
					WHERE active = 1 ORDER BY sequence ASC LIMIT 1";
			$res = db_query($sql, "Could not get first stage");
			$stage = db_fetch($res);
			if ($stage) {
				$data['stage_id'] = $stage['id'];
				$data['probability'] = $stage['probability'];
			}
		}

		if (!empty($data['stage_id']) && empty($data['probability'])) {
			$sql = "SELECT probability FROM " . TB_PREF . "crm_sales_stages
					WHERE id = " . db_escape($data['stage_id']) . " LIMIT 1";
			$res = db_query($sql, "Could not get stage probability");
			$stage = db_fetch($res);
			if ($stage) {
				$data['probability'] = $stage['probability'];
			}
		}

		if (empty($lead['lead_ref']) || strpos($lead['lead_ref'], get_crm_setting('lead_ref_prefix', 'LD')) === 0) {
			$data['lead_ref'] = crm_next_opportunity_ref();
		}

		return self::modify($lead_id, $data);
	}

	/**
	 * Mark an opportunity as Won.
	 *
	 * @param int $lead_id
	 * @param int $customer_id  Optional linked customer.
	 * @return bool
	 */
	public static function mark_won($lead_id, $customer_id = null) {
		$data = array(
			'lead_status' => CRM_LEAD_WON,
			'date_won' => date('Y-m-d H:i:s'),
			'probability' => 100,
		);

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

		return self::modify($lead_id, $data);
	}

	/**
	 * Mark an opportunity as Lost.
	 *
	 * @param int    $lead_id
	 * @param int    $reason_id
	 * @param string $notes
	 * @return bool
	 */
	public static function mark_lost($lead_id, $reason_id = null, $notes = null) {
		$data = array(
			'lead_status' => CRM_LEAD_LOST,
			'date_converted' => date('Y-m-d H:i:s'),
			'date_lost' => date('Y-m-d H:i:s'),
			'probability' => 0,
		);

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

		return self::modify($lead_id, $data);
	}

	/**
	 * Move an opportunity to a different pipeline stage and update probability.
	 *
	 * @param int $lead_id
	 * @param int $stage_id
	 * @return bool
	 */
	public static function update_stage($lead_id, $stage_id) {
		$sql = "SELECT probability FROM " . TB_PREF . "crm_sales_stages
				WHERE id = " . db_escape($stage_id);
		$res = db_query($sql, "Could not get stage probability");
		$stage = db_fetch($res);

		$data = array('stage_id' => $stage_id);
		if ($stage) {
			$data['probability'] = $stage['probability'];
		}
		return self::modify($lead_id, $data);
	}
}
