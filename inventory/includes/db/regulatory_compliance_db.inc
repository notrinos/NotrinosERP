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
 * Regulatory Compliance Database Layer.
 *
 * Core CRUD for regulatory profiles (item ↔ framework mapping).
 * Shared helpers for all frameworks (DSCSA, FSMA 204, UDI).
 *
 * Tables: 0_regulatory_profiles
 * Dependencies: includes/db.inc (db_query, db_fetch, TB_PREF)
 *
 * @package NotrinosERP
 * @subpackage Inventory/Regulatory
 */

// =====================================================================
// FRAMEWORK CONSTANTS
// =====================================================================

define('REG_DSCSA',   'dscsa');
define('REG_FMD',     'fmd');
define('REG_FSMA204', 'fsma204');
define('REG_UDI',     'udi');

/**
 * Get valid regulatory framework options.
 *
 * @return array Associative array of framework_code => display_label
 */
function get_regulatory_frameworks()
{
	return array(
		REG_DSCSA   => _('DSCSA (US Pharma)'),
		REG_FMD     => _('FMD (EU Pharma)'),
		REG_FSMA204 => _('FSMA 204 (US Food Safety)'),
		REG_UDI     => _('UDI (Medical Devices)'),
	);
}

/**
 * Get framework label.
 *
 * @param string $framework Framework code
 * @return string Translated label
 */
function get_regulatory_framework_label($framework)
{
	$frameworks = get_regulatory_frameworks();
	return isset($frameworks[$framework]) ? $frameworks[$framework] : $framework;
}

/**
 * Get CSS badge color for a framework.
 *
 * @param string $framework Framework code
 * @return string CSS color
 */
function get_regulatory_framework_color($framework)
{
	$colors = array(
		REG_DSCSA   => '#0d6efd',
		REG_FMD     => '#6f42c1',
		REG_FSMA204 => '#198754',
		REG_UDI     => '#dc3545',
	);
	return isset($colors[$framework]) ? $colors[$framework] : '#6c757d';
}

// =====================================================================
// REGULATORY PROFILE CRUD
// =====================================================================

/**
 * Add a regulatory profile (link an item or category to a framework).
 *
 * @param string|null $stock_id    Item stock_id (null for category-level)
 * @param int|null    $category_id Category id (null for item-level)
 * @param string      $framework   Framework code (dscsa|fmd|fsma204|udi)
 * @param array       $settings    Framework-specific settings (stored as JSON)
 * @param string      $notes       Optional notes
 * @return int|false Inserted id or false on failure
 */
function add_regulatory_profile($stock_id, $category_id, $framework, $settings = array(), $notes = '')
{
	$sql = "INSERT INTO " . TB_PREF . "regulatory_profiles
		(stock_id, category_id, framework, enabled, settings, notes, created_by, created_date)
		VALUES ("
		. db_escape($stock_id) . ", "
		. db_escape($category_id) . ", "
		. db_escape($framework) . ", "
		. "1, "
		. db_escape(json_encode($settings)) . ", "
		. db_escape($notes) . ", "
		. (int)$_SESSION['wa_current_user']->user . ", "
		. "NOW())";

	db_query($sql, 'could not add regulatory profile');
	return db_insert_id();
}

/**
 * Update a regulatory profile.
 *
 * @param int    $id       Profile id
 * @param bool   $enabled  Enabled flag
 * @param array  $settings Framework-specific settings
 * @param string $notes    Notes
 * @return void
 */
function update_regulatory_profile($id, $enabled, $settings = array(), $notes = '')
{
	$sql = "UPDATE " . TB_PREF . "regulatory_profiles SET "
		. "enabled=" . ($enabled ? 1 : 0) . ", "
		. "settings=" . db_escape(json_encode($settings)) . ", "
		. "notes=" . db_escape($notes)
		. " WHERE id=" . (int)$id;

	db_query($sql, 'could not update regulatory profile');
}

/**
 * Delete a regulatory profile.
 *
 * @param int $id Profile id
 * @return void
 */
function delete_regulatory_profile($id)
{
	$sql = "DELETE FROM " . TB_PREF . "regulatory_profiles WHERE id=" . (int)$id;
	db_query($sql, 'could not delete regulatory profile');
}

/**
 * Get a single regulatory profile by id.
 *
 * @param int $id Profile id
 * @return array|null Row or null
 */
function get_regulatory_profile($id)
{
	$sql = "SELECT rp.*, sm.description AS item_name, sc.description AS category_name
		FROM " . TB_PREF . "regulatory_profiles rp
		LEFT JOIN " . TB_PREF . "stock_master sm ON rp.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "stock_category sc ON rp.category_id = sc.category_id
		WHERE rp.id=" . (int)$id;

	$result = db_query($sql, 'could not get regulatory profile');
	return db_fetch($result);
}

/**
 * Get all regulatory profiles, optionally filtered.
 *
 * @param string|null $framework   Filter by framework
 * @param string|null $stock_id    Filter by item
 * @param int|null    $category_id Filter by category
 * @param bool        $enabled_only Only enabled profiles
 * @return resource DB result set
 */
function get_regulatory_profiles($framework = null, $stock_id = null, $category_id = null, $enabled_only = false)
{
	$sql = "SELECT rp.*, sm.description AS item_name, sc.description AS category_name
		FROM " . TB_PREF . "regulatory_profiles rp
		LEFT JOIN " . TB_PREF . "stock_master sm ON rp.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "stock_category sc ON rp.category_id = sc.category_id
		WHERE 1=1";

	if ($framework !== null)
		$sql .= " AND rp.framework=" . db_escape($framework);
	if ($stock_id !== null)
		$sql .= " AND rp.stock_id=" . db_escape($stock_id);
	if ($category_id !== null)
		$sql .= " AND rp.category_id=" . (int)$category_id;
	if ($enabled_only)
		$sql .= " AND rp.enabled=1";

	$sql .= " ORDER BY rp.framework, COALESCE(sm.description, sc.description, '')";
	return db_query($sql, 'could not get regulatory profiles');
}

/**
 * Check if an item is subject to a specific regulatory framework.
 *
 * Checks both item-level and category-level profiles.
 *
 * @param string $stock_id  Item stock_id
 * @param string $framework Framework code
 * @return bool True if the item is under this framework
 */
function is_item_regulated($stock_id, $framework)
{
	// Check item-level profile
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "regulatory_profiles
		WHERE stock_id=" . db_escape($stock_id)
		. " AND framework=" . db_escape($framework)
		. " AND enabled=1";
	$result = db_query($sql, 'could not check item regulation');
	$row = db_fetch_row($result);
	if ((int)$row[0] > 0)
		return true;

	// Check category-level profile
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "regulatory_profiles rp
		INNER JOIN " . TB_PREF . "stock_master sm ON rp.category_id = sm.category_id
		WHERE sm.stock_id=" . db_escape($stock_id)
		. " AND rp.framework=" . db_escape($framework)
		. " AND rp.enabled=1
		AND rp.stock_id IS NULL";
	$result = db_query($sql, 'could not check category regulation');
	$row = db_fetch_row($result);
	return (int)$row[0] > 0;
}

/**
 * Get the regulatory profile settings for an item under a framework.
 *
 * Returns item-level settings if they exist, else category-level.
 *
 * @param string $stock_id  Item stock_id
 * @param string $framework Framework code
 * @return array|null Settings array or null if not regulated
 */
function get_item_regulatory_settings($stock_id, $framework)
{
	// Item-level first
	$sql = "SELECT settings FROM " . TB_PREF . "regulatory_profiles
		WHERE stock_id=" . db_escape($stock_id)
		. " AND framework=" . db_escape($framework)
		. " AND enabled=1 LIMIT 1";
	$result = db_query($sql, 'could not get item regulatory settings');
	$row = db_fetch($result);
	if ($row) {
		$s = json_decode($row['settings'], true);
		return is_array($s) ? $s : array();
	}

	// Category-level
	$sql = "SELECT rp.settings FROM " . TB_PREF . "regulatory_profiles rp
		INNER JOIN " . TB_PREF . "stock_master sm ON rp.category_id = sm.category_id
		WHERE sm.stock_id=" . db_escape($stock_id)
		. " AND rp.framework=" . db_escape($framework)
		. " AND rp.enabled=1
		AND rp.stock_id IS NULL
		LIMIT 1";
	$result = db_query($sql, 'could not get category regulatory settings');
	$row = db_fetch($result);
	if ($row) {
		$s = json_decode($row['settings'], true);
		return is_array($s) ? $s : array();
	}

	return null;
}

/**
 * Get all active frameworks for an item.
 *
 * @param string $stock_id Item stock_id
 * @return array Array of framework codes
 */
function get_item_active_frameworks($stock_id)
{
	$frameworks = array();

	$sql = "SELECT DISTINCT rp.framework
		FROM " . TB_PREF . "regulatory_profiles rp
		LEFT JOIN " . TB_PREF . "stock_master sm ON rp.category_id = sm.category_id AND rp.stock_id IS NULL
		WHERE (rp.stock_id=" . db_escape($stock_id)
		. " OR sm.stock_id=" . db_escape($stock_id) . ")
		AND rp.enabled=1
		ORDER BY rp.framework";

	$result = db_query($sql, 'could not get item active frameworks');
	while ($row = db_fetch($result))
		$frameworks[] = $row['framework'];

	return $frameworks;
}

// =====================================================================
// COMPLIANCE STATISTICS
// =====================================================================

/**
 * Get compliance summary counts for the dashboard.
 *
 * @return array Counts by framework and status
 */
function get_regulatory_compliance_summary()
{
	$summary = array(
		'profiles_total'    => 0,
		'profiles_by_framework' => array(),
		'dscsa_pending'     => 0,
		'dscsa_verified'    => 0,
		'dscsa_suspect'     => 0,
		'fsma_events_today' => 0,
		'udi_registered'    => 0,
	);

	// Profile counts
	$sql = "SELECT framework, COUNT(*) AS cnt FROM " . TB_PREF . "regulatory_profiles
		WHERE enabled=1 GROUP BY framework";
	$result = db_query($sql, 'could not get profile counts');
	while ($row = db_fetch($result)) {
		$summary['profiles_by_framework'][$row['framework']] = (int)$row['cnt'];
		$summary['profiles_total'] += (int)$row['cnt'];
	}

	// DSCSA verification counts
	$sql = "SELECT verification_status, COUNT(*) AS cnt FROM " . TB_PREF . "dscsa_transactions
		GROUP BY verification_status";
	$result = db_query($sql, 'could not get DSCSA counts');
	while ($row = db_fetch($result)) {
		if ($row['verification_status'] === 'pending')
			$summary['dscsa_pending'] = (int)$row['cnt'];
		if ($row['verification_status'] === 'verified')
			$summary['dscsa_verified'] = (int)$row['cnt'];
		if ($row['verification_status'] === 'suspect')
			$summary['dscsa_suspect'] = (int)$row['cnt'];
	}

	// FSMA events today
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "fsma_tracking_events
		WHERE DATE(event_date) = CURDATE()";
	$result = db_query($sql, 'could not get FSMA event count');
	$row = db_fetch_row($result);
	$summary['fsma_events_today'] = (int)$row[0];

	// UDI registrations
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "udi_registrations";
	$result = db_query($sql, 'could not get UDI count');
	$row = db_fetch_row($result);
	$summary['udi_registered'] = (int)$row[0];

	return $summary;
}
