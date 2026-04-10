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
 * Warehouse Counting DB Layer — Cycle Count Plans, Count Sessions, Count Lines
 *
 * Provides CRUD + lifecycle management for cycle counting programs.
 * Tables: 0_wh_cycle_count_plans, 0_wh_cycle_counts, 0_wh_cycle_count_lines
 *
 * Session 16 of UNIFIED_INVENTORY_IMPLEMENTATION_PLAN.md
 */

//----------------------------------------------------------------------
// Status / type helpers
//----------------------------------------------------------------------

/**
 * Get all possible count statuses.
 *
 * @return array status_key => display_label
 */
function get_count_statuses() {
    return array(
        'draft'       => _('Draft'),
        'in_progress' => _('In Progress'),
        'review'      => _('Review'),
        'approved'    => _('Approved'),
        'posted'      => _('Posted'),
    );
}

/**
 * Get color for a count status.
 *
 * @param string $status
 * @return string hex color
 */
function get_count_status_color($status) {
    $colors = array(
        'draft'       => '#6c757d',
        'in_progress' => '#17a2b8',
        'review'      => '#ffc107',
        'approved'    => '#28a745',
        'posted'      => '#007bff',
    );
    return isset($colors[$status]) ? $colors[$status] : '#6c757d';
}

/**
 * Render a colored status badge.
 *
 * @param string $status
 * @return string HTML span
 */
function count_status_badge($status) {
    $statuses = get_count_statuses();
    $label = isset($statuses[$status]) ? $statuses[$status] : $status;
    $color = get_count_status_color($status);
    return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;color:#fff;'
        . 'background-color:' . $color . ';font-size:11px;font-weight:600;">'
        . $label . '</span>';
}

/**
 * Get count plan methods.
 *
 * @return array method_key => display_label
 */
function get_count_methods() {
    return array(
        'location_based' => _('Location Based'),
        'item_based'     => _('Item Based'),
        'abc_based'      => _('ABC Classification'),
        'full'           => _('Full Count'),
    );
}

/**
 * Get color for a count method.
 *
 * @param string $method
 * @return string hex color
 */
function get_count_method_color($method) {
    $colors = array(
        'location_based' => '#17a2b8',
        'item_based'     => '#6f42c1',
        'abc_based'      => '#fd7e14',
        'full'           => '#dc3545',
    );
    return isset($colors[$method]) ? $colors[$method] : '#6c757d';
}

/**
 * Render a colored method badge.
 *
 * @param string $method
 * @return string HTML span
 */
function count_method_badge($method) {
    $methods = get_count_methods();
    $label = isset($methods[$method]) ? $methods[$method] : $method;
    $color = get_count_method_color($method);
    return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;color:#fff;'
        . 'background-color:' . $color . ';font-size:11px;font-weight:600;">'
        . $label . '</span>';
}

//----------------------------------------------------------------------
// Cycle Count Plan CRUD
//----------------------------------------------------------------------

/**
 * Add a new cycle count plan.
 *
 * @param string      $plan_name
 * @param string      $count_method         location_based|item_based|abc_based|full
 * @param string|null $warehouse_loc_code   NULL for all warehouses
 * @param int         $frequency_days       Days between counts (default 30)
 * @param string|null $abc_class            A|B|C (for abc_based method)
 * @return int new plan_id
 */
function add_cycle_count_plan($plan_name, $count_method, $warehouse_loc_code = null,
    $frequency_days = 30, $abc_class = null) {

    $next_date = date('Y-m-d', strtotime('+' . intval($frequency_days) . ' days'));

    $sql = "INSERT INTO " . TB_PREF . "wh_cycle_count_plans
        (plan_name, count_method, warehouse_loc_code, frequency_days, abc_class, next_count_date, active)
        VALUES ("
        . db_escape($plan_name) . ", "
        . db_escape($count_method) . ", "
        . ($warehouse_loc_code ? db_escape($warehouse_loc_code) : "NULL") . ", "
        . db_escape(intval($frequency_days)) . ", "
        . ($abc_class ? db_escape($abc_class) : "NULL") . ", "
        . "'" . $next_date . "', 1)";

    db_query($sql, 'could not add cycle count plan');
    return db_insert_id();
}

/**
 * Update an existing cycle count plan.
 *
 * @param int         $plan_id
 * @param string      $plan_name
 * @param string      $count_method
 * @param string|null $warehouse_loc_code
 * @param int         $frequency_days
 * @param string|null $abc_class
 * @param int         $active
 */
function update_cycle_count_plan($plan_id, $plan_name, $count_method, $warehouse_loc_code = null,
    $frequency_days = 30, $abc_class = null, $active = 1) {

    $sql = "UPDATE " . TB_PREF . "wh_cycle_count_plans SET "
        . "plan_name = " . db_escape($plan_name) . ", "
        . "count_method = " . db_escape($count_method) . ", "
        . "warehouse_loc_code = " . ($warehouse_loc_code ? db_escape($warehouse_loc_code) : "NULL") . ", "
        . "frequency_days = " . db_escape(intval($frequency_days)) . ", "
        . "abc_class = " . ($abc_class ? db_escape($abc_class) : "NULL") . ", "
        . "active = " . db_escape(intval($active))
        . " WHERE plan_id = " . db_escape(intval($plan_id));

    db_query($sql, 'could not update cycle count plan');
}

/**
 * Get a single cycle count plan.
 *
 * @param int $plan_id
 * @return array|false
 */
function get_cycle_count_plan($plan_id) {
    $sql = "SELECT p.*, l.location_name
        FROM " . TB_PREF . "wh_cycle_count_plans p
        LEFT JOIN " . TB_PREF . "locations l ON p.warehouse_loc_code = l.loc_code
        WHERE p.plan_id = " . db_escape(intval($plan_id));
    $result = db_query($sql, 'could not get cycle count plan');
    return db_fetch($result);
}

/**
 * Get cycle count plans with optional filters.
 *
 * @param string|null $warehouse_loc_code
 * @param string|null $count_method
 * @param bool        $show_inactive
 * @return resource
 */
function get_cycle_count_plans($warehouse_loc_code = null, $count_method = null, $show_inactive = false) {
    $sql = "SELECT p.*, l.location_name,
            (SELECT COUNT(*) FROM " . TB_PREF . "wh_cycle_counts c WHERE c.plan_id = p.plan_id) AS total_counts
        FROM " . TB_PREF . "wh_cycle_count_plans p
        LEFT JOIN " . TB_PREF . "locations l ON p.warehouse_loc_code = l.loc_code
        WHERE 1=1";

    if ($warehouse_loc_code)
        $sql .= " AND p.warehouse_loc_code = " . db_escape($warehouse_loc_code);
    if ($count_method)
        $sql .= " AND p.count_method = " . db_escape($count_method);
    if (!$show_inactive)
        $sql .= " AND p.active = 1";

    $sql .= " ORDER BY p.plan_name";
    return db_query($sql, 'could not get cycle count plans');
}

/**
 * Check if a plan can be deleted (no associated counts).
 *
 * @param int $plan_id
 * @return bool
 */
function can_delete_cycle_count_plan($plan_id) {
    $sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "wh_cycle_counts
        WHERE plan_id = " . db_escape(intval($plan_id));
    $result = db_query($sql, 'could not check plan usage');
    $row = db_fetch($result);
    return ($row['cnt'] == 0);
}

/**
 * Delete a cycle count plan.
 *
 * @param int $plan_id
 */
function delete_cycle_count_plan($plan_id) {
    $sql = "DELETE FROM " . TB_PREF . "wh_cycle_count_plans
        WHERE plan_id = " . db_escape(intval($plan_id));
    db_query($sql, 'could not delete cycle count plan');
}

//----------------------------------------------------------------------
// Cycle Count Session CRUD
//----------------------------------------------------------------------

/**
 * Add a new cycle count session.
 *
 * @param string      $count_date           Display-format date
 * @param string      $warehouse_loc_code
 * @param int|null    $plan_id
 * @param string      $memo
 * @return int new count_id
 */
function add_cycle_count($count_date, $warehouse_loc_code, $plan_id = null, $memo = '') {
    global $path_to_root;

    $sql = "INSERT INTO " . TB_PREF . "wh_cycle_counts
        (count_date, status, warehouse_loc_code, plan_id, memo)
        VALUES ("
        . "'" . date2sql($count_date) . "', "
        . "'draft', "
        . db_escape($warehouse_loc_code) . ", "
        . ($plan_id ? db_escape(intval($plan_id)) : "NULL") . ", "
        . db_escape($memo) . ")";

    db_query($sql, 'could not add cycle count');
    return db_insert_id();
}

/**
 * Update cycle count header.
 *
 * @param int         $count_id
 * @param string      $count_date
 * @param string      $warehouse_loc_code
 * @param int|null    $plan_id
 * @param string      $memo
 */
function update_cycle_count($count_id, $count_date, $warehouse_loc_code, $plan_id = null, $memo = '') {
    $sql = "UPDATE " . TB_PREF . "wh_cycle_counts SET "
        . "count_date = '" . date2sql($count_date) . "', "
        . "warehouse_loc_code = " . db_escape($warehouse_loc_code) . ", "
        . "plan_id = " . ($plan_id ? db_escape(intval($plan_id)) : "NULL") . ", "
        . "memo = " . db_escape($memo)
        . " WHERE count_id = " . db_escape(intval($count_id));

    db_query($sql, 'could not update cycle count');
}

/**
 * Get a single cycle count with JOINs.
 *
 * @param int $count_id
 * @return array|false
 */
function get_cycle_count($count_id) {
    $sql = "SELECT c.*, l.location_name, p.plan_name,
            (SELECT COUNT(*) FROM " . TB_PREF . "wh_cycle_count_lines cl WHERE cl.count_id = c.count_id) AS total_lines,
            (SELECT COUNT(*) FROM " . TB_PREF . "wh_cycle_count_lines cl2
                WHERE cl2.count_id = c.count_id AND cl2.counted_qty IS NOT NULL) AS counted_lines,
            (SELECT COUNT(*) FROM " . TB_PREF . "wh_cycle_count_lines cl3
                WHERE cl3.count_id = c.count_id AND cl3.variance_qty <> 0 AND cl3.counted_qty IS NOT NULL) AS variance_lines
        FROM " . TB_PREF . "wh_cycle_counts c
        LEFT JOIN " . TB_PREF . "locations l ON c.warehouse_loc_code = l.loc_code
        LEFT JOIN " . TB_PREF . "wh_cycle_count_plans p ON c.plan_id = p.plan_id
        WHERE c.count_id = " . db_escape(intval($count_id));
    $result = db_query($sql, 'could not get cycle count');
    return db_fetch($result);
}

/**
 * Get cycle counts with optional filters.
 *
 * @param string|null $warehouse_loc_code
 * @param string|null $status
 * @param int|null    $plan_id
 * @param int         $limit
 * @param int         $offset
 * @return resource
 */
function get_cycle_counts($warehouse_loc_code = null, $status = null, $plan_id = null,
    $limit = 50, $offset = 0) {

    $sql = "SELECT c.*, l.location_name, p.plan_name,
            (SELECT COUNT(*) FROM " . TB_PREF . "wh_cycle_count_lines cl WHERE cl.count_id = c.count_id) AS total_lines,
            (SELECT COUNT(*) FROM " . TB_PREF . "wh_cycle_count_lines cl2
                WHERE cl2.count_id = c.count_id AND cl2.counted_qty IS NOT NULL) AS counted_lines
        FROM " . TB_PREF . "wh_cycle_counts c
        LEFT JOIN " . TB_PREF . "locations l ON c.warehouse_loc_code = l.loc_code
        LEFT JOIN " . TB_PREF . "wh_cycle_count_plans p ON c.plan_id = p.plan_id
        WHERE 1=1";

    if ($warehouse_loc_code)
        $sql .= " AND c.warehouse_loc_code = " . db_escape($warehouse_loc_code);

    if ($status) {
        if (is_array($status)) {
            $escaped = array();
            foreach ($status as $s) $escaped[] = db_escape($s);
            $sql .= " AND c.status IN (" . implode(',', $escaped) . ")";
        } else {
            $sql .= " AND c.status = " . db_escape($status);
        }
    }

    if ($plan_id)
        $sql .= " AND c.plan_id = " . db_escape(intval($plan_id));

    $sql .= " ORDER BY c.count_id DESC";
    $sql .= " LIMIT " . intval($limit) . " OFFSET " . intval($offset);

    return db_query($sql, 'could not get cycle counts');
}

/**
 * Count cycle count records (for pagination / summaries).
 *
 * @param string|null $warehouse_loc_code
 * @param string|null $status   Can be string or array
 * @return int
 */
function count_cycle_counts($warehouse_loc_code = null, $status = null) {
    $sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "wh_cycle_counts c WHERE 1=1";

    if ($warehouse_loc_code)
        $sql .= " AND c.warehouse_loc_code = " . db_escape($warehouse_loc_code);

    if ($status) {
        if (is_array($status)) {
            $escaped = array();
            foreach ($status as $s) $escaped[] = db_escape($s);
            $sql .= " AND c.status IN (" . implode(',', $escaped) . ")";
        } else {
            $sql .= " AND c.status = " . db_escape($status);
        }
    }

    $result = db_query($sql, 'could not count cycle counts');
    $row = db_fetch($result);
    return intval($row['cnt']);
}

/**
 * Check if a count can be deleted (only draft status).
 *
 * @param int $count_id
 * @return bool
 */
function can_delete_cycle_count($count_id) {
    $count = get_cycle_count($count_id);
    if (!$count) return false;
    return ($count['status'] == 'draft');
}

/**
 * Delete a cycle count and its lines (cascade via FK).
 *
 * @param int $count_id
 */
function delete_cycle_count($count_id) {
    $sql = "DELETE FROM " . TB_PREF . "wh_cycle_counts
        WHERE count_id = " . db_escape(intval($count_id));
    db_query($sql, 'could not delete cycle count');
}

//----------------------------------------------------------------------
// Cycle Count Line Operations
//----------------------------------------------------------------------

/**
 * Add a count line.
 *
 * @param int         $count_id
 * @param int         $wh_loc_id       Bin location id
 * @param string      $stock_id
 * @param float       $system_qty      Expected qty from system
 * @param int|null    $batch_id
 * @param int|null    $serial_id
 * @return int new line_id
 */
function add_cycle_count_line($count_id, $wh_loc_id, $stock_id, $system_qty,
    $batch_id = null, $serial_id = null) {

    $sql = "INSERT INTO " . TB_PREF . "wh_cycle_count_lines
        (count_id, wh_loc_id, stock_id, system_qty, batch_id, serial_id)
        VALUES ("
        . db_escape(intval($count_id)) . ", "
        . db_escape(intval($wh_loc_id)) . ", "
        . db_escape($stock_id) . ", "
        . db_escape($system_qty) . ", "
        . ($batch_id ? db_escape(intval($batch_id)) : "NULL") . ", "
        . ($serial_id ? db_escape(intval($serial_id)) : "NULL") . ")";

    db_query($sql, 'could not add cycle count line');
    return db_insert_id();
}

/**
 * Get lines for a count session with JOINs to item, location, batch, serial.
 *
 * @param int  $count_id
 * @param bool $only_variance  If true, only lines with variance
 * @return resource
 */
function get_cycle_count_lines($count_id, $only_variance = false) {
    $sql = "SELECT cl.*, sm.description AS item_description, sm.units,
            wl.loc_code AS bin_code, wl.loc_name AS bin_name,
            sb.batch_no, sb.expiry_date AS batch_expiry,
            sn.serial_no
        FROM " . TB_PREF . "wh_cycle_count_lines cl
        LEFT JOIN " . TB_PREF . "stock_master sm ON cl.stock_id = sm.stock_id
        LEFT JOIN " . TB_PREF . "wh_locations wl ON cl.wh_loc_id = wl.loc_id
        LEFT JOIN " . TB_PREF . "stock_batches sb ON cl.batch_id = sb.id
        LEFT JOIN " . TB_PREF . "serial_numbers sn ON cl.serial_id = sn.id
        WHERE cl.count_id = " . db_escape(intval($count_id));

    if ($only_variance)
        $sql .= " AND cl.counted_qty IS NOT NULL AND cl.variance_qty <> 0";

    $sql .= " ORDER BY wl.loc_code, cl.stock_id, cl.batch_id, cl.serial_id";
    return db_query($sql, 'could not get cycle count lines');
}

/**
 * Get a single count line.
 *
 * @param int $line_id
 * @return array|false
 */
function get_cycle_count_line($line_id) {
    $sql = "SELECT cl.*, sm.description AS item_description,
            wl.loc_code AS bin_code, wl.loc_name AS bin_name
        FROM " . TB_PREF . "wh_cycle_count_lines cl
        LEFT JOIN " . TB_PREF . "stock_master sm ON cl.stock_id = sm.stock_id
        LEFT JOIN " . TB_PREF . "wh_locations wl ON cl.wh_loc_id = wl.loc_id
        WHERE cl.line_id = " . db_escape(intval($line_id));
    $result = db_query($sql, 'could not get cycle count line');
    return db_fetch($result);
}

/**
 * Record a counted quantity for a line.
 *
 * @param int   $line_id
 * @param float $counted_qty
 * @param string $memo
 */
function record_count_line($line_id, $counted_qty, $memo = '') {
    $line = get_cycle_count_line($line_id);
    if (!$line) return;

    $variance = $counted_qty - $line['system_qty'];

    $sql = "UPDATE " . TB_PREF . "wh_cycle_count_lines SET "
        . "counted_qty = " . db_escape($counted_qty) . ", "
        . "variance_qty = " . db_escape($variance) . ", "
        . "memo = " . db_escape($memo)
        . " WHERE line_id = " . db_escape(intval($line_id));

    db_query($sql, 'could not record count line');
}

//----------------------------------------------------------------------
// Count Session Lifecycle
//----------------------------------------------------------------------

/**
 * Generate count lines from current bin stock for a count session.
 *
 * Populates count_lines based on the count's warehouse and optionally
 * a plan's method (location-based, item-based, ABC-based, or full).
 *
 * @param int      $count_id
 * @param int|null $plan_id   If provided, uses the plan's method to filter
 * @param bool     $blind     If true, system_qty is set to 0 (blind count)
 * @return int number of lines generated
 */
function generate_count_lines($count_id, $plan_id = null, $blind = false) {
    $count = get_cycle_count($count_id);
    if (!$count) return 0;

    $warehouse_loc_code = $count['warehouse_loc_code'];
    $method = 'full';
    $abc_class = null;

    if ($plan_id) {
        $plan = get_cycle_count_plan($plan_id);
        if ($plan) {
            $method = $plan['count_method'];
            $abc_class = $plan['abc_class'];
        }
    }

    // Build query to get current bin stock for the warehouse
    $sql = "SELECT bs.wh_loc_id, bs.stock_id, bs.qty_on_hand, bs.batch_id, bs.serial_id
        FROM " . TB_PREF . "wh_bin_stock bs
        INNER JOIN " . TB_PREF . "wh_locations wl ON bs.wh_loc_id = wl.loc_id
        WHERE wl.warehouse_loc_code = " . db_escape($warehouse_loc_code)
        . " AND wl.is_active = 1";

    // Method-specific filters
    if ($method == 'abc_based' && $abc_class) {
        $sql .= " AND EXISTS (SELECT 1 FROM " . TB_PREF . "stock_master sm2
            WHERE sm2.stock_id = bs.stock_id AND sm2.abc_class = " . db_escape($abc_class) . ")";
    }

    $sql .= " ORDER BY wl.loc_code, bs.stock_id";
    $result = db_query($sql, 'could not query bin stock for count generation');

    $lines_created = 0;
    while ($row = db_fetch($result)) {
        $system_qty = $blind ? 0 : $row['qty_on_hand'];
        add_cycle_count_line(
            $count_id,
            $row['wh_loc_id'],
            $row['stock_id'],
            $system_qty,
            $row['batch_id'] ? $row['batch_id'] : null,
            $row['serial_id'] ? $row['serial_id'] : null
        );
        $lines_created++;
    }

    // For location-based or full count, also add lines for storable bins that have NO stock
    // (so counters can report unexpected items)
    if ($method == 'location_based' || $method == 'full') {
        $sql2 = "SELECT wl.loc_id
            FROM " . TB_PREF . "wh_locations wl
            INNER JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id
            WHERE wl.warehouse_loc_code = " . db_escape($warehouse_loc_code)
            . " AND lt.can_store = 1 AND wl.is_active = 1
            AND NOT EXISTS (SELECT 1 FROM " . TB_PREF . "wh_bin_stock bs WHERE bs.wh_loc_id = wl.loc_id)";
        $result2 = db_query($sql2, 'could not query empty bins');

        // Empty bins don't auto-generate lines; counters will add lines manually if they find items
    }

    return $lines_created;
}

/**
 * Start a count session (draft → in_progress).
 *
 * @param int $count_id
 * @return bool success
 */
function start_cycle_count($count_id) {
    $count = get_cycle_count($count_id);
    if (!$count || $count['status'] != 'draft') return false;

    $sql = "UPDATE " . TB_PREF . "wh_cycle_counts SET "
        . "status = 'in_progress', "
        . "counted_by = " . db_escape($_SESSION['wa_current_user']->user)
        . " WHERE count_id = " . db_escape(intval($count_id));
    db_query($sql, 'could not start cycle count');
    return true;
}

/**
 * Submit a count for review (in_progress → review).
 *
 * @param int $count_id
 * @return bool success
 */
function submit_cycle_count_for_review($count_id) {
    $count = get_cycle_count($count_id);
    if (!$count || $count['status'] != 'in_progress') return false;

    // Check all lines have been counted
    $sql = "SELECT COUNT(*) AS uncounted FROM " . TB_PREF . "wh_cycle_count_lines
        WHERE count_id = " . db_escape(intval($count_id)) . " AND counted_qty IS NULL";
    $result = db_query($sql, 'could not check uncounted lines');
    $row = db_fetch($result);
    if ($row['uncounted'] > 0) return false;

    $sql = "UPDATE " . TB_PREF . "wh_cycle_counts SET status = 'review'
        WHERE count_id = " . db_escape(intval($count_id));
    db_query($sql, 'could not submit cycle count for review');
    return true;
}

/**
 * Approve a count session (review → approved).
 *
 * @param int $count_id
 * @return bool success
 */
function approve_cycle_count($count_id) {
    $count = get_cycle_count($count_id);
    if (!$count || $count['status'] != 'review') return false;

    $sql = "UPDATE " . TB_PREF . "wh_cycle_counts SET "
        . "status = 'approved', "
        . "approved_by = " . db_escape($_SESSION['wa_current_user']->user)
        . " WHERE count_id = " . db_escape(intval($count_id));
    db_query($sql, 'could not approve cycle count');
    return true;
}

/**
 * Send an approved count back for recounting (approved → in_progress).
 *
 * @param int $count_id
 * @return bool success
 */
function recount_cycle_count($count_id) {
    $count = get_cycle_count($count_id);
    if (!$count || !in_array($count['status'], array('review', 'approved'))) return false;

    // Clear counted quantities so counter must re-enter
    $sql = "UPDATE " . TB_PREF . "wh_cycle_count_lines SET
        counted_qty = NULL, variance_qty = NULL
        WHERE count_id = " . db_escape(intval($count_id));
    db_query($sql, 'could not reset count lines');

    $sql = "UPDATE " . TB_PREF . "wh_cycle_counts SET "
        . "status = 'in_progress', approved_by = NULL"
        . " WHERE count_id = " . db_escape(intval($count_id));
    db_query($sql, 'could not recount cycle count');
    return true;
}

/**
 * Cancel a count session. Allowed from draft, in_progress, or review.
 *
 * @param int $count_id
 * @return bool success
 */
function cancel_cycle_count($count_id) {
    $count = get_cycle_count($count_id);
    if (!$count || in_array($count['status'], array('posted'))) return false;

    // Simply delete the count (cascade deletes lines via FK)
    delete_cycle_count($count_id);
    return true;
}

/**
 * Post approved adjustments — create stock moves + GL entries for all variance lines.
 *
 * This is the MAIN posting function. It:
 * 1. Creates a single ST_INVADJUST transaction for the whole count
 * 2. For each line with variance, creates a stock_move + GL entries
 * 3. Updates bin stock quantities
 * 4. Updates the plan's last/next count dates
 * 5. Marks the count as 'posted'
 *
 * @param int $count_id
 * @return int|false adjustment transaction number, or false on failure
 */
function post_cycle_count_adjustments($count_id) {
    global $path_to_root, $Refs;

    $count = get_cycle_count($count_id);
    if (!$count || $count['status'] != 'approved') return false;

    // Get only variance lines
    $lines = get_cycle_count_lines($count_id, true);
    $has_variances = false;

    // Pre-check: any variances to post?
    $check_sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "wh_cycle_count_lines
        WHERE count_id = " . db_escape(intval($count_id))
        . " AND counted_qty IS NOT NULL AND variance_qty <> 0 AND adjustment_posted = 0";
    $check_result = db_query($check_sql, 'could not check variances');
    $check_row = db_fetch($check_result);

    if ($check_row['cnt'] == 0) {
        // No variances — just mark as posted
        $sql = "UPDATE " . TB_PREF . "wh_cycle_counts SET
            status = 'posted',
            posted_date = NOW()
            WHERE count_id = " . db_escape(intval($count_id));
        db_query($sql, 'could not mark count as posted');

        // Update plan dates if plan-linked
        if ($count['plan_id'])
            update_plan_count_dates($count['plan_id']);

        return 0;
    }

    begin_transaction();

    // Create an inventory adjustment transaction
    $adj_id = get_next_trans_no(ST_INVADJUST);
    $date_ = sql2date($count['count_date']);
    $reference = $Refs->get_next(ST_INVADJUST, null, $date_);
    $memo = sprintf(_("Cycle Count #%d adjustment"), $count_id);

    // Re-query variance lines for processing
    $var_lines = get_cycle_count_lines($count_id, true);

    while ($line = db_fetch($var_lines)) {
        if ($line['adjustment_posted']) continue;

        $variance = $line['variance_qty'];
        if ($variance == 0) continue;

        $has_variances = true;

        // Get item standard cost
        $item_row = get_item($line['stock_id']);
        $standard_cost = $item_row ? $item_row['material_cost'] : 0;

        // Update average material cost for positive adjustments
        update_average_material_cost(null, $line['stock_id'], $standard_cost, $variance, $date_);

        // Create stock move
        add_stock_move(ST_INVADJUST, $line['stock_id'], $adj_id,
            $count['warehouse_loc_code'], $date_, $reference, $variance, $standard_cost);

        // GL entries (inventory + adjustment accounts)
        if ($standard_cost > 0) {
            $stock_gl = get_stock_gl_code($line['stock_id']);
            $inv_value = $standard_cost * $variance;
            $adj_value = $standard_cost * -($variance);

            add_gl_trans_std_cost(ST_INVADJUST, $adj_id, $date_,
                $stock_gl['adjustment_account'],
                $stock_gl['dimension_id'], $stock_gl['dimension2_id'],
                $memo, $adj_value);

            add_gl_trans_std_cost(ST_INVADJUST, $adj_id, $date_,
                $stock_gl['inventory_account'],
                $stock_gl['dimension_id'], $stock_gl['dimension2_id'],
                $memo, $inv_value);
        }

        // Update bin stock
        $update_bin_sql = "UPDATE " . TB_PREF . "wh_bin_stock SET
            qty_on_hand = qty_on_hand + " . db_escape($variance) . ",
            qty_available = qty_available + " . db_escape($variance)
            . " WHERE wh_loc_id = " . db_escape(intval($line['wh_loc_id']))
            . " AND stock_id = " . db_escape($line['stock_id']);

        if ($line['batch_id'])
            $update_bin_sql .= " AND batch_id = " . db_escape(intval($line['batch_id']));
        else
            $update_bin_sql .= " AND (batch_id IS NULL OR batch_id = 0)";

        if ($line['serial_id'])
            $update_bin_sql .= " AND serial_id = " . db_escape(intval($line['serial_id']));
        else
            $update_bin_sql .= " AND (serial_id IS NULL OR serial_id = 0)";

        db_query($update_bin_sql, 'could not update bin stock');

        // If variance is positive and no bin_stock row exists, insert one
        if ($variance > 0) {
            $exists_sql = "SELECT id FROM " . TB_PREF . "wh_bin_stock
                WHERE wh_loc_id = " . db_escape(intval($line['wh_loc_id']))
                . " AND stock_id = " . db_escape($line['stock_id']);
            if ($line['batch_id'])
                $exists_sql .= " AND batch_id = " . db_escape(intval($line['batch_id']));
            else
                $exists_sql .= " AND (batch_id IS NULL OR batch_id = 0)";
            if ($line['serial_id'])
                $exists_sql .= " AND serial_id = " . db_escape(intval($line['serial_id']));
            else
                $exists_sql .= " AND (serial_id IS NULL OR serial_id = 0)";

            $exists_result = db_query($exists_sql, 'could not check bin stock');
            if (db_num_rows($exists_result) == 0) {
                $ins_sql = "INSERT INTO " . TB_PREF . "wh_bin_stock
                    (wh_loc_id, stock_id, qty_on_hand, qty_available, batch_id, serial_id)
                    VALUES ("
                    . db_escape(intval($line['wh_loc_id'])) . ", "
                    . db_escape($line['stock_id']) . ", "
                    . db_escape($variance) . ", "
                    . db_escape($variance) . ", "
                    . ($line['batch_id'] ? db_escape(intval($line['batch_id'])) : "NULL") . ", "
                    . ($line['serial_id'] ? db_escape(intval($line['serial_id'])) : "NULL") . ")";
                db_query($ins_sql, 'could not insert bin stock');
            }
        }

        // Mark line as posted
        $sql = "UPDATE " . TB_PREF . "wh_cycle_count_lines SET adjustment_posted = 1
            WHERE line_id = " . db_escape(intval($line['line_id']));
        db_query($sql, 'could not mark line as posted');
    }

    if ($has_variances) {
        add_comments(ST_INVADJUST, $adj_id, $date_, $memo);
        $Refs->save(ST_INVADJUST, $adj_id, $reference);
        add_audit_trail(ST_INVADJUST, $adj_id, $date_);
    }

    // Mark count as posted
    $sql = "UPDATE " . TB_PREF . "wh_cycle_counts SET
        status = 'posted',
        posted_date = NOW()
        WHERE count_id = " . db_escape(intval($count_id));
    db_query($sql, 'could not mark count as posted');

    // Update plan dates if plan-linked
    if ($count['plan_id'])
        update_plan_count_dates($count['plan_id']);

    commit_transaction();

    return $has_variances ? $adj_id : 0;
}

/**
 * Update a plan's last_count_date and next_count_date after a count is posted.
 *
 * @param int $plan_id
 */
function update_plan_count_dates($plan_id) {
    $plan = get_cycle_count_plan($plan_id);
    if (!$plan) return;

    $today = date('Y-m-d');
    $next = date('Y-m-d', strtotime('+' . intval($plan['frequency_days']) . ' days'));

    $sql = "UPDATE " . TB_PREF . "wh_cycle_count_plans SET
        last_count_date = '" . $today . "',
        next_count_date = '" . $next . "'
        WHERE plan_id = " . db_escape(intval($plan_id));
    db_query($sql, 'could not update plan count dates');
}

//----------------------------------------------------------------------
// Auto-generate counts from plans
//----------------------------------------------------------------------

/**
 * Auto-generate count sessions for all plans that are due (next_count_date <= today).
 *
 * @param bool $blind   If true, generated counts use blind mode
 * @return int number of counts generated
 */
function auto_generate_due_counts($blind = false) {
    $today = date('Y-m-d');

    $sql = "SELECT * FROM " . TB_PREF . "wh_cycle_count_plans
        WHERE active = 1 AND next_count_date <= '" . $today . "'";
    $result = db_query($sql, 'could not get due plans');

    $count = 0;
    while ($plan = db_fetch($result)) {
        // Determine warehouse — if plan is global, generate for each WMS-enabled warehouse
        if ($plan['warehouse_loc_code']) {
            $warehouses = array($plan['warehouse_loc_code']);
        } else {
            $warehouses = array();
            $wh_sql = "SELECT loc_code FROM " . TB_PREF . "locations WHERE wh_enabled = 1";
            $wh_result = db_query($wh_sql, 'could not get warehouses');
            while ($wh = db_fetch($wh_result)) {
                $warehouses[] = $wh['loc_code'];
            }
        }

        foreach ($warehouses as $wh_code) {
            $count_date = Today();
            $count_id = add_cycle_count($count_date, $wh_code, $plan['plan_id'],
                sprintf(_("Auto-generated from plan: %s"), $plan['plan_name']));
            generate_count_lines($count_id, $plan['plan_id'], $blind);
            $count++;
        }
    }

    return $count;
}

//----------------------------------------------------------------------
// Summary / reporting helpers
//----------------------------------------------------------------------

/**
 * Get count status summary counts.
 *
 * @param string|null $warehouse_loc_code
 * @return array status => count
 */
function get_count_status_summary($warehouse_loc_code = null) {
    $sql = "SELECT status, COUNT(*) AS cnt FROM " . TB_PREF . "wh_cycle_counts";
    if ($warehouse_loc_code)
        $sql .= " WHERE warehouse_loc_code = " . db_escape($warehouse_loc_code);
    $sql .= " GROUP BY status";

    $result = db_query($sql, 'could not get count status summary');

    $summary = array('draft' => 0, 'in_progress' => 0, 'review' => 0, 'approved' => 0, 'posted' => 0);
    while ($row = db_fetch($result)) {
        $summary[$row['status']] = intval($row['cnt']);
    }
    return $summary;
}

/**
 * Get overall variance summary for a count.
 *
 * @param int $count_id
 * @return array with total_lines, counted_lines, variance_lines, total_variance_qty, total_variance_value
 */
function get_count_variance_summary($count_id) {
    $sql = "SELECT
        COUNT(*) AS total_lines,
        SUM(CASE WHEN cl.counted_qty IS NOT NULL THEN 1 ELSE 0 END) AS counted_lines,
        SUM(CASE WHEN cl.variance_qty <> 0 AND cl.counted_qty IS NOT NULL THEN 1 ELSE 0 END) AS variance_lines,
        SUM(ABS(COALESCE(cl.variance_qty, 0))) AS total_variance_qty,
        SUM(ABS(COALESCE(cl.variance_qty, 0)) * COALESCE(sm.material_cost, 0)) AS total_variance_value
        FROM " . TB_PREF . "wh_cycle_count_lines cl
        LEFT JOIN " . TB_PREF . "stock_master sm ON cl.stock_id = sm.stock_id
        WHERE cl.count_id = " . db_escape(intval($count_id));

    $result = db_query($sql, 'could not get variance summary');
    $row = db_fetch($result);

    return array(
        'total_lines'        => intval($row['total_lines']),
        'counted_lines'      => intval($row['counted_lines']),
        'variance_lines'     => intval($row['variance_lines']),
        'total_variance_qty' => floatval($row['total_variance_qty']),
        'total_variance_value' => floatval($row['total_variance_value']),
    );
}

/**
 * Get due plans summary (plans whose next_count_date is past or today).
 *
 * @return int count of due plans
 */
function get_due_plans_count() {
    $today = date('Y-m-d');
    $sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "wh_cycle_count_plans
        WHERE active = 1 AND next_count_date <= '" . $today . "'";
    $result = db_query($sql, 'could not get due plans count');
    $row = db_fetch($result);
    return intval($row['cnt']);
}

/**
 * Add a count line manually (for unexpected items found during counting).
 *
 * Use when counter finds an item in a bin that wasn't in the generated list.
 *
 * @param int    $count_id
 * @param int    $wh_loc_id
 * @param string $stock_id
 * @param float  $counted_qty
 * @param int|null $batch_id
 * @param int|null $serial_id
 * @param string $memo
 * @return int new line_id
 */
function add_manual_count_line($count_id, $wh_loc_id, $stock_id, $counted_qty,
    $batch_id = null, $serial_id = null, $memo = '') {

    // System qty is 0 since this item wasn't expected at this bin
    $system_qty = 0;
    $variance = $counted_qty - $system_qty;

    $sql = "INSERT INTO " . TB_PREF . "wh_cycle_count_lines
        (count_id, wh_loc_id, stock_id, system_qty, counted_qty, variance_qty, batch_id, serial_id, memo)
        VALUES ("
        . db_escape(intval($count_id)) . ", "
        . db_escape(intval($wh_loc_id)) . ", "
        . db_escape($stock_id) . ", "
        . db_escape($system_qty) . ", "
        . db_escape($counted_qty) . ", "
        . db_escape($variance) . ", "
        . ($batch_id ? db_escape(intval($batch_id)) : "NULL") . ", "
        . ($serial_id ? db_escape(intval($serial_id)) : "NULL") . ", "
        . db_escape($memo) . ")";

    db_query($sql, 'could not add manual count line');
    return db_insert_id();
}
