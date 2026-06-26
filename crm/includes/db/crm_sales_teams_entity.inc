<?php
/**
 * Entity class for crm_sales_teams.
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 *
 * ::remove() overrides the base to also delete team members (multi-table transaction).
 * ::create()/::modify() handle team members in a multi-table transaction.
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class crm_sales_teams_entity extends Entity {
	function __construct() {
		parent::__construct(
			'crm_sales_teams',
			array('name', 'team_leader_id', 'email_alias', 'invoicing_target',
				'use_leads', 'active', 'description', 'max_members', 'allow_auto_assign', 'id'),
			'id'
		);
	}

	/** Override create to also handle team members in a transaction. */
	public static function create($data) {
		begin_transaction();
		$e = new static();
		$result = $e->insert($data);
		if ($result && !empty($data['members'])) {
			$id = db_insert_id();
			foreach ($data['members'] as $member) {
				$e->_add_member($id, $member['user_id'], @$member['max_leads_30days'] ?: 30);
			}
		}
		if ($result) commit_transaction(); else rollback_transaction();
		return $result;
	}

	/** Override modify for team members. */
	public static function modify($key, $data) {
		begin_transaction();
		$e = new static();
		$result = $e->update($key, $data);
		if ($result && isset($data['members'])) {
			$e->_delete_members($key);
			foreach ($data['members'] as $member) {
				$e->_add_member($key, $member['user_id'], @$member['max_leads_30days'] ?: 30);
			}
		}
		if ($result) commit_transaction(); else rollback_transaction();
		return $result;
	}

	/** Override remove for team members. */
	public static function remove($key) {
		begin_transaction();
		$e = new static();
		$e->_delete_members($key);
		$result = $e->delete($key);
		if ($result) commit_transaction(); else rollback_transaction();
		return $result;
	}

	function _add_member($team_id, $user_id, $max_leads) {
		$sql = "INSERT IGNORE INTO " . TB_PREF . "crm_sales_team_members
			(team_id, user_id, max_leads_30days) VALUES ("
			. db_escape($team_id) . ", "
			. db_escape($user_id) . ", "
			. db_escape($max_leads) . ")";
		db_query($sql, "Could not add team member");
	}

	function _delete_members($team_id) {
		$sql = "DELETE FROM " . TB_PREF . "crm_sales_team_members WHERE team_id = " . db_escape($team_id);
		db_query($sql, "Could not delete team members");
	}

	/** Get team name by id. */
	public static function name($id) {
		$row = static::find($id);
		return $row ? $row['name'] : '';
	}

	/**
	 * Add a member to a sales team.
	 *
	 * @param int $team_id
	 * @param int $user_id
	 * @param int $max_leads_30days
	 * @return bool
	 */
	public static function add_member($team_id, $user_id, $max_leads_30days = 30) {
		$e = new static();
		$e->_add_member($team_id, $user_id, $max_leads_30days);
		return true;
	}

	/**
	 * Remove a member from a team.
	 *
	 * @param int $team_id
	 * @param int $user_id
	 * @return bool
	 */
	public static function remove_member($team_id, $user_id) {
		$sql = "DELETE FROM " . TB_PREF . "crm_sales_team_members
				WHERE team_id = " . db_escape($team_id)
			. " AND user_id = " . db_escape($user_id);
		return db_query($sql, "Could not remove team member");
	}

	/**
	 * Get members of a team (returns DB resource).
	 *
	 * @param int $team_id
	 * @return resource
	 */
	public static function get_members($team_id) {
		$sql = "SELECT m.*, u.real_name, u.login_id AS login_name
				FROM " . TB_PREF . "crm_sales_team_members m
				INNER JOIN " . TB_PREF . "users u ON m.user_id = u.id
				WHERE m.team_id = " . db_escape($team_id)
			. " ORDER BY u.real_name";
		return db_query($sql, "Could not get team members");
	}

	/**
	 * Get teams that a user belongs to (returns DB resource).
	 *
	 * @param int $user_id
	 * @return resource
	 */
	public static function get_user_teams($user_id) {
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
	public static function count_user_recent_leads($user_id) {
		$sql = "SELECT COUNT(*) as cnt FROM " . TB_PREF . "crm_leads
				WHERE assigned_to = " . db_escape($user_id)
			. " AND date_created >= DATE_SUB(NOW(), INTERVAL 30 DAY)";
		$result = db_query($sql, "Could not count recent leads");
		$row = db_fetch($result);
		return $row ? (int)$row['cnt'] : 0;
	}

	/**
	 * List all sales teams with leader name and member count (returns DB resource).
	 *
	 * Replaces the old get_crm_sales_teams() — enriches the base all() with JOINs.
	 *
	 * @param bool $include_inactive
	 * @return resource
	 */
	public static function all_with_counts($include_inactive = false) {
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
}
