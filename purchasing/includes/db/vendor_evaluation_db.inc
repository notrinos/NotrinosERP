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

include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/hooks.inc');
include_once($path_to_root . '/purchasing/includes/db/suppliers_db.inc');

/**
 * Get supported vendor tier labels.
 *
 * @return array
 */
function get_vendor_tiers()
{
	return array(
		'standard' => _('Standard'),
		'preferred' => _('Preferred'),
		'strategic' => _('Strategic'),
		'critical' => _('Critical'),
	);
}

/**
 * Get supported vendor evaluation categories.
 *
 * @return array
 */
function get_vendor_evaluation_categories()
{
	return array(
		'quality' => _('Quality'),
		'delivery' => _('Delivery'),
		'price' => _('Price'),
		'service' => _('Service'),
		'compliance' => _('Compliance'),
	);
}

/**
 * Get supported vendor evaluation scoring methods.
 *
 * @return array
 */
function get_vendor_evaluation_scoring_methods()
{
	return array(
		'manual' => _('Manual'),
		'calculated' => _('Calculated'),
	);
}

/**
 * Get vendor evaluation status labels.
 *
 * @return array
 */
function get_vendor_evaluation_statuses()
{
	return array(
		'draft' => _('Draft'),
		'submitted' => _('Submitted'),
		'approved' => _('Approved'),
	);
}

/**
 * Get supported vendor evaluation recommendations.
 *
 * @return array
 */
function get_vendor_recommendations()
{
	return array(
		'maintain' => _('Maintain'),
		'upgrade' => _('Upgrade'),
		'downgrade' => _('Downgrade'),
		'remove' => _('Remove'),
		'probation' => _('Probation'),
	);
}

/**
 * Get supported vendor performance log event types.
 *
 * @return array
 */
function get_vendor_event_types()
{
	return array(
		'delivery_received' => _('Delivery Received'),
		'quality_issue' => _('Quality Issue'),
		'price_change' => _('Price Change'),
		'late_delivery' => _('Late Delivery'),
		'early_delivery' => _('Early Delivery'),
		'complaint' => _('Complaint'),
		'resolution' => _('Resolution'),
	);
}

/**
 * Get the display color for a vendor tier.
 *
 * @param string $tier
 * @return string
 */
function get_vendor_tier_color($tier)
{
	$colors = array(
		'standard' => '#6c757d',
		'preferred' => '#0d6efd',
		'strategic' => '#fd7e14',
		'critical' => '#dc3545',
	);

	return isset($colors[$tier]) ? $colors[$tier] : '#6c757d';
}

/**
 * Get the display color for a vendor evaluation status.
 *
 * @param string $status
 * @return string
 */
function get_vendor_evaluation_status_color($status)
{
	$colors = array(
		'draft' => '#6c757d',
		'submitted' => '#17a2b8',
		'approved' => '#28a745',
	);

	return isset($colors[$status]) ? $colors[$status] : '#6c757d';
}

/**
 * Render a vendor tier badge.
 *
 * @param string $tier
 * @return string
 */
function vendor_tier_badge($tier)
{
	$tiers = get_vendor_tiers();
	$label = isset($tiers[$tier]) ? $tiers[$tier] : $tier;
	$color = get_vendor_tier_color($tier);

	return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;font-size:11px;'
		. 'font-weight:bold;color:#fff;background:' . $color . ';">' . $label . '</span>';
}

/**
 * Render a vendor evaluation status badge.
 *
 * @param string $status
 * @return string
 */
function vendor_evaluation_status_badge($status)
{
	$statuses = get_vendor_evaluation_statuses();
	$label = isset($statuses[$status]) ? $statuses[$status] : $status;
	$color = get_vendor_evaluation_status_color($status);

	return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;font-size:11px;'
		. 'font-weight:bold;color:#fff;background:' . $color . ';">' . $label . '</span>';
}

/**
 * Normalize one date value into SQL format.
 *
 * @param string $date
 * @param string $fallback
 * @return string
 */
function normalize_vendor_evaluation_sql_date($date, $fallback = '')
{
	if ($date === null || $date === '')
		return $fallback;

	if (preg_match('/^\d{4}-\d{2}-\d{2}$/', $date))
		return $date;

	return is_date($date) ? date2sql($date) : $fallback;
}

/**
 * Encode custom data for JSON storage.
 *
 * @param mixed $custom_data
 * @return string|null
 */
function encode_vendor_evaluation_custom_data($custom_data)
{
	if ($custom_data === null || $custom_data === '' || $custom_data === array())
		return null;

	if (is_string($custom_data))
		return $custom_data;

	$encoded = json_encode($custom_data);
	return $encoded === false ? null : $encoded;
}

/**
 * Build a SQL-safe JSON literal for custom data columns.
 *
 * @param mixed $custom_data
 * @return string
 */
function sql_vendor_evaluation_custom_data($custom_data)
{
	$encoded = encode_vendor_evaluation_custom_data($custom_data);
	return $encoded === null ? 'NULL' : "'" . db_escape_function($encoded) . "'";
}

/**
 * Decode custom data from storage.
 *
 * @param string|null $custom_data
 * @return array
 */
function decode_vendor_evaluation_custom_data($custom_data)
{
	if (!$custom_data)
		return array();

	$decoded = json_decode($custom_data, true);
	if (!is_array($decoded))
		$decoded = json_decode(html_entity_decode($custom_data, ENT_QUOTES, 'UTF-8'), true);

	return is_array($decoded) ? $decoded : array();
}

/**
 * Derive a vendor tier from an overall score.
 *
 * @param float $overall_score
 * @return string
 */
function get_vendor_tier_for_score($overall_score)
{
	if ($overall_score >= 90)
		return 'critical';
	if ($overall_score >= 75)
		return 'strategic';
	if ($overall_score >= 60)
		return 'preferred';

	return 'standard';
}

/**
 * Get one vendor evaluation criteria row.
 *
 * @param int $criteria_id
 * @return array|false
 */
function get_vendor_evaluation_criteria_row($criteria_id)
{
	$sql = "SELECT * FROM " . TB_PREF . "vendor_evaluation_criteria
		WHERE id = " . (int)$criteria_id;
	$result = db_query($sql, 'could not retrieve vendor evaluation criteria');

	return db_fetch($result);
}

/**
 * Add one evaluation criteria row.
 *
 * @param string $name
 * @param string $category
 * @param float  $weight
 * @param string $description
 * @param string $scoring_method
 * @param string $calculation_formula
 * @param int    $inactive
 * @return int
 */
function add_evaluation_criteria($name, $category, $weight, $description, $scoring_method, $calculation_formula = '', $inactive = 0)
{
	$sql = "INSERT INTO " . TB_PREF . "vendor_evaluation_criteria
		(name, category, weight, description, scoring_method, calculation_formula, inactive)
		VALUES (
			" . db_escape($name) . ",
			" . db_escape($category) . ",
			" . db_escape($weight) . ",
			" . db_escape($description) . ",
			" . db_escape($scoring_method) . ",
			" . db_escape($calculation_formula) . ",
			" . db_escape($inactive) . ")";

	db_query($sql, 'could not add vendor evaluation criteria');
	return db_insert_id();
}

/**
 * Update one evaluation criteria row.
 *
 * @param int    $criteria_id
 * @param string $name
 * @param string $category
 * @param float  $weight
 * @param string $description
 * @param string $scoring_method
 * @param string $calculation_formula
 * @param int    $inactive
 * @return bool
 */
function update_evaluation_criteria($criteria_id, $name, $category, $weight, $description, $scoring_method, $calculation_formula = '', $inactive = 0)
{
	$sql = "UPDATE " . TB_PREF . "vendor_evaluation_criteria SET
		name = " . db_escape($name) . ",
		category = " . db_escape($category) . ",
		weight = " . db_escape($weight) . ",
		description = " . db_escape($description) . ",
		scoring_method = " . db_escape($scoring_method) . ",
		calculation_formula = " . db_escape($calculation_formula) . ",
		inactive = " . db_escape($inactive) . "
		WHERE id = " . (int)$criteria_id;

	db_query($sql, 'could not update vendor evaluation criteria');
	return true;
}

/**
 * Delete one evaluation criteria row.
 *
 * @param int $criteria_id
 * @return bool
 */
function delete_evaluation_criteria($criteria_id)
{
	$sql = "DELETE FROM " . TB_PREF . "vendor_evaluation_criteria
		WHERE id = " . (int)$criteria_id;
	db_query($sql, 'could not delete vendor evaluation criteria');

	return true;
}

/**
 * Get all evaluation criteria.
 *
 * @param bool $active_only
 * @return resource
 */
function get_evaluation_criteria($active_only = true)
{
	$sql = "SELECT criteria.*,
		COALESCE(score_usage.usage_count, 0) AS usage_count
		FROM " . TB_PREF . "vendor_evaluation_criteria criteria
		LEFT JOIN (
			SELECT criteria_id, COUNT(*) AS usage_count
			FROM " . TB_PREF . "vendor_evaluation_scores
			GROUP BY criteria_id
		) score_usage ON score_usage.criteria_id = criteria.id";

	if ($active_only)
		$sql .= " WHERE criteria.inactive = 0";

	$sql .= " ORDER BY criteria.category, criteria.name";

	return db_query($sql, 'could not retrieve vendor evaluation criteria list');
}

/**
 * Get criteria rows for one category.
 *
 * @param string $category
 * @return resource
 */
function get_criteria_by_category($category)
{
	$sql = "SELECT * FROM " . TB_PREF . "vendor_evaluation_criteria
		WHERE category = " . db_escape($category) . "
		ORDER BY inactive, name";

	return db_query($sql, 'could not retrieve vendor evaluation criteria category list');
}

/**
 * Get one vendor evaluation header row.
 *
 * @param int $evaluation_id
 * @return array|false
 */
function get_vendor_evaluation($evaluation_id)
{
	$sql = "SELECT evaluation.*,
		supplier.supp_name,
		supplier.vendor_tier,
		supplier.vendor_category,
		evaluator.real_name AS evaluator_name,
		COALESCE(score_counts.score_count, 0) AS score_count
		FROM " . TB_PREF . "vendor_evaluations evaluation
		LEFT JOIN " . TB_PREF . "suppliers supplier ON supplier.supplier_id = evaluation.supplier_id
		LEFT JOIN " . TB_PREF . "users evaluator ON evaluator.id = evaluation.evaluator_id
		LEFT JOIN (
			SELECT evaluation_id, COUNT(*) AS score_count
			FROM " . TB_PREF . "vendor_evaluation_scores
			GROUP BY evaluation_id
		) score_counts ON score_counts.evaluation_id = evaluation.id
		WHERE evaluation.id = " . (int)$evaluation_id;

	$result = db_query($sql, 'could not retrieve vendor evaluation');
	$row = db_fetch($result);
	if ($row)
		$row['custom_data_decoded'] = decode_vendor_evaluation_custom_data($row['custom_data']);

	return $row;
}

/**
 * Get one editable vendor evaluation row.
 *
 * @param int $evaluation_id
 * @return array|false
 */
function get_editable_vendor_evaluation($evaluation_id)
{
	$evaluation = get_vendor_evaluation($evaluation_id);
	if (!$evaluation)
		return false;

	return $evaluation['status'] === 'approved' ? false : $evaluation;
}

/**
 * Get vendor evaluation rows filtered by supplier and date range.
 *
 * @param int    $supplier_id
 * @param string $date_from
 * @param string $date_to
 * @return resource
 */
function get_vendor_evaluations($supplier_id = 0, $date_from = '', $date_to = '')
{
	$sql = "SELECT evaluation.*,
		supplier.supp_name,
		evaluator.real_name AS evaluator_name,
		COALESCE(score_counts.score_count, 0) AS score_count
		FROM " . TB_PREF . "vendor_evaluations evaluation
		LEFT JOIN " . TB_PREF . "suppliers supplier ON supplier.supplier_id = evaluation.supplier_id
		LEFT JOIN " . TB_PREF . "users evaluator ON evaluator.id = evaluation.evaluator_id
		LEFT JOIN (
			SELECT evaluation_id, COUNT(*) AS score_count
			FROM " . TB_PREF . "vendor_evaluation_scores
			GROUP BY evaluation_id
		) score_counts ON score_counts.evaluation_id = evaluation.id
		WHERE 1=1";

	if ((int)$supplier_id > 0)
		$sql .= " AND evaluation.supplier_id = " . (int)$supplier_id;
	if ($date_from !== '')
		$sql .= " AND evaluation.evaluation_date >= " . db_escape($date_from);
	if ($date_to !== '')
		$sql .= " AND evaluation.evaluation_date <= " . db_escape($date_to);

	$sql .= " ORDER BY evaluation.evaluation_date DESC, evaluation.id DESC";

	return db_query($sql, 'could not retrieve vendor evaluations');
}

/**
 * Add one vendor evaluation header row.
 *
 * @param int    $supplier_id
 * @param string $period_from
 * @param string $period_to
 * @param int    $evaluator_id
 * @param string $evaluation_date
 * @param string $recommendation
 * @param string $action_plan
 * @param string $notes
 * @param string $status
 * @param mixed  $custom_data
 * @return int
 */
function add_vendor_evaluation($supplier_id, $period_from, $period_to, $evaluator_id, $evaluation_date = '', $recommendation = 'maintain', $action_plan = '', $notes = '', $status = 'draft', $custom_data = null)
{
	$evaluation_date = normalize_vendor_evaluation_sql_date($evaluation_date, date2sql(Today()));
	$period_from = normalize_vendor_evaluation_sql_date($period_from, $evaluation_date);
	$period_to = normalize_vendor_evaluation_sql_date($period_to, $evaluation_date);

	$hook_data = array(
		'supplier_id' => (int)$supplier_id,
		'period_from' => $period_from,
		'period_to' => $period_to,
		'evaluator_id' => (int)$evaluator_id,
		'evaluation_date' => $evaluation_date,
		'recommendation' => $recommendation,
		'action_plan' => $action_plan,
		'notes' => $notes,
		'status' => $status,
		'custom_data' => $custom_data,
	);
	hook_invoke_all('vendor_evaluation_before_save', $hook_data);

	$sql = "INSERT INTO " . TB_PREF . "vendor_evaluations
		(supplier_id, evaluation_date, evaluator_id, period_from, period_to, status, recommendation, action_plan, notes, custom_data)
		VALUES (
			" . (int)$hook_data['supplier_id'] . ",
			" . db_escape($hook_data['evaluation_date']) . ",
			" . (int)$hook_data['evaluator_id'] . ",
			" . db_escape($hook_data['period_from']) . ",
			" . db_escape($hook_data['period_to']) . ",
			" . db_escape($hook_data['status']) . ",
			" . db_escape($hook_data['recommendation']) . ",
			" . db_escape($hook_data['action_plan']) . ",
			" . db_escape($hook_data['notes']) . ",
			" . sql_vendor_evaluation_custom_data($hook_data['custom_data']) . ")";

	db_query($sql, 'could not add vendor evaluation');
	return db_insert_id();
}

/**
 * Update one vendor evaluation header row.
 *
 * @param int    $evaluation_id
 * @param int    $supplier_id
 * @param string $period_from
 * @param string $period_to
 * @param int    $evaluator_id
 * @param string $evaluation_date
 * @param string $recommendation
 * @param string $action_plan
 * @param string $notes
 * @param string $status
 * @param mixed  $custom_data
 * @return bool
 */
function update_vendor_evaluation($evaluation_id, $supplier_id, $period_from, $period_to, $evaluator_id, $evaluation_date, $recommendation = 'maintain', $action_plan = '', $notes = '', $status = 'draft', $custom_data = null)
{
	$existing = get_editable_vendor_evaluation($evaluation_id);
	if (!$existing)
		return false;

	$hook_data = array(
		'evaluation_id' => (int)$evaluation_id,
		'supplier_id' => (int)$supplier_id,
		'period_from' => normalize_vendor_evaluation_sql_date($period_from, $existing['period_from']),
		'period_to' => normalize_vendor_evaluation_sql_date($period_to, $existing['period_to']),
		'evaluator_id' => (int)$evaluator_id,
		'evaluation_date' => normalize_vendor_evaluation_sql_date($evaluation_date, $existing['evaluation_date']),
		'recommendation' => $recommendation,
		'action_plan' => $action_plan,
		'notes' => $notes,
		'status' => $status,
		'custom_data' => $custom_data,
	);
	hook_invoke_all('vendor_evaluation_before_save', $hook_data);

	$sql = "UPDATE " . TB_PREF . "vendor_evaluations SET
		supplier_id = " . (int)$hook_data['supplier_id'] . ",
		evaluation_date = " . db_escape($hook_data['evaluation_date']) . ",
		evaluator_id = " . (int)$hook_data['evaluator_id'] . ",
		period_from = " . db_escape($hook_data['period_from']) . ",
		period_to = " . db_escape($hook_data['period_to']) . ",
		status = " . db_escape($hook_data['status']) . ",
		recommendation = " . db_escape($hook_data['recommendation']) . ",
		action_plan = " . db_escape($hook_data['action_plan']) . ",
		notes = " . db_escape($hook_data['notes']) . ",
		custom_data = " . sql_vendor_evaluation_custom_data($hook_data['custom_data']) . "
		WHERE id = " . (int)$evaluation_id;

	db_query($sql, 'could not update vendor evaluation');
	calculate_evaluation_totals($evaluation_id);

	return true;
}

/**
 * Delete one vendor evaluation row when still editable.
 *
 * @param int $evaluation_id
 * @return bool
 */
function delete_vendor_evaluation($evaluation_id)
{
	$existing = get_editable_vendor_evaluation($evaluation_id);
	if (!$existing)
		return false;

	$sql = "DELETE FROM " . TB_PREF . "vendor_evaluations WHERE id = " . (int)$evaluation_id;
	db_query($sql, 'could not delete vendor evaluation');

	return true;
}

/**
 * Get one vendor evaluation score row.
 *
 * @param int $evaluation_id
 * @param int $criteria_id
 * @return array|false
 */
function get_vendor_evaluation_score_row($evaluation_id, $criteria_id)
{
	$sql = "SELECT * FROM " . TB_PREF . "vendor_evaluation_scores
		WHERE evaluation_id = " . (int)$evaluation_id . "
		AND criteria_id = " . (int)$criteria_id;
	$result = db_query($sql, 'could not retrieve vendor evaluation score');

	return db_fetch($result);
}

/**
 * Create or update one criteria score for an evaluation.
 *
 * @param int    $evaluation_id
 * @param int    $criteria_id
 * @param float  $score
 * @param string $evidence
 * @param string $notes
 * @return bool
 */
function set_evaluation_score($evaluation_id, $criteria_id, $score, $evidence = '', $notes = '')
{
	$existing = get_vendor_evaluation_score_row($evaluation_id, $criteria_id);
	if ($existing) {
		$sql = "UPDATE " . TB_PREF . "vendor_evaluation_scores SET
			score = " . db_escape($score) . ",
			evidence = " . db_escape($evidence) . ",
			notes = " . db_escape($notes) . "
			WHERE id = " . (int)$existing['id'];
	} else {
		$sql = "INSERT INTO " . TB_PREF . "vendor_evaluation_scores
			(evaluation_id, criteria_id, score, evidence, notes)
			VALUES (
				" . (int)$evaluation_id . ",
				" . (int)$criteria_id . ",
				" . db_escape($score) . ",
				" . db_escape($evidence) . ",
				" . db_escape($notes) . ")";
	}

	db_query($sql, 'could not save vendor evaluation score');
	calculate_evaluation_totals($evaluation_id);

	return true;
}

/**
 * Get the saved score rows for one evaluation.
 *
 * @param int $evaluation_id
 * @return resource
 */
function get_evaluation_scores($evaluation_id)
{
	$sql = "SELECT score.*, criteria.name AS criteria_name, criteria.category, criteria.weight, criteria.scoring_method
		FROM " . TB_PREF . "vendor_evaluation_scores score
		INNER JOIN " . TB_PREF . "vendor_evaluation_criteria criteria ON criteria.id = score.criteria_id
		WHERE score.evaluation_id = " . (int)$evaluation_id . "
		ORDER BY criteria.category, criteria.name";

	return db_query($sql, 'could not retrieve vendor evaluation scores');
}

/**
 * Persist computed category scores inside evaluation custom data.
 *
 * @param int   $evaluation_id
 * @param array $category_scores
 * @return void
 */
function update_vendor_evaluation_category_snapshot($evaluation_id, $category_scores)
{
	$evaluation = get_vendor_evaluation($evaluation_id);
	if (!$evaluation)
		return;

	$custom_data = decode_vendor_evaluation_custom_data($evaluation['custom_data']);
	$custom_data['category_scores'] = $category_scores;

	$sql = "UPDATE " . TB_PREF . "vendor_evaluations SET
		custom_data = " . sql_vendor_evaluation_custom_data($custom_data) . "
		WHERE id = " . (int)$evaluation_id;
	db_query($sql, 'could not update vendor evaluation category snapshot');
}

/**
 * Calculate and persist evaluation total scores.
 *
 * @param int $evaluation_id
 * @return array
 */
function calculate_evaluation_totals($evaluation_id)
{
	$category_scores = array(
		'quality' => 0,
		'delivery' => 0,
		'price' => 0,
		'service' => 0,
		'compliance' => 0,
	);
	$overall_weight = 0;
	$overall_score = 0;

	$sql = "SELECT criteria.category,
		SUM(criteria.weight) AS total_weight,
		SUM(score.score * criteria.weight) AS weighted_score
		FROM " . TB_PREF . "vendor_evaluation_scores score
		INNER JOIN " . TB_PREF . "vendor_evaluation_criteria criteria ON criteria.id = score.criteria_id
		WHERE score.evaluation_id = " . (int)$evaluation_id . "
		GROUP BY criteria.category";
	$result = db_query($sql, 'could not calculate vendor evaluation totals');

	while ($row = db_fetch($result)) {
		if ((float)$row['total_weight'] <= 0)
			continue;

		$score = round2($row['weighted_score'] / $row['total_weight'], 4);
		$category_scores[$row['category']] = $score;
		$overall_score += $row['weighted_score'];
		$overall_weight += $row['total_weight'];
	}

	$overall_score = $overall_weight > 0 ? round2($overall_score / $overall_weight, 4) : 0;

	$sql = "UPDATE " . TB_PREF . "vendor_evaluations SET
		overall_score = " . db_escape($overall_score) . ",
		quality_score = " . db_escape($category_scores['quality']) . ",
		delivery_score = " . db_escape($category_scores['delivery']) . ",
		price_score = " . db_escape($category_scores['price']) . ",
		service_score = " . db_escape($category_scores['service']) . "
		WHERE id = " . (int)$evaluation_id;
	db_query($sql, 'could not update vendor evaluation totals');

	update_vendor_evaluation_category_snapshot($evaluation_id, $category_scores);

	return array(
		'overall_score' => $overall_score,
		'category_scores' => $category_scores,
	);
}

/**
 * Submit one vendor evaluation for review.
 *
 * @param int $evaluation_id
 * @return bool
 */
function submit_evaluation($evaluation_id)
{
	$existing = get_editable_vendor_evaluation($evaluation_id);
	if (!$existing)
		return false;

	calculate_evaluation_totals($evaluation_id);
	$sql = "UPDATE " . TB_PREF . "vendor_evaluations SET status = 'submitted'
		WHERE id = " . (int)$evaluation_id;
	db_query($sql, 'could not submit vendor evaluation');

	return true;
}

/**
 * Calculate delivery performance metrics.
 *
 * @param int    $supplier_id
 * @param string $period_from
 * @param string $period_to
 * @return array
 */
function calculate_delivery_performance($supplier_id, $period_from = '', $period_to = '')
{
	$sql = "SELECT
		COUNT(*) AS delivery_count,
		SUM(CASE WHEN DATEDIFF(batch.delivery_date, detail.delivery_date) <= 0 THEN 1 ELSE 0 END) AS on_time_count,
		SUM(CASE WHEN DATEDIFF(batch.delivery_date, detail.delivery_date) > 0 THEN 1 ELSE 0 END) AS late_count,
		SUM(CASE WHEN DATEDIFF(batch.delivery_date, detail.delivery_date) < 0 THEN 1 ELSE 0 END) AS early_count,
		AVG(DATEDIFF(batch.delivery_date, purchase_order.ord_date)) AS avg_lead_time,
		AVG(DATEDIFF(batch.delivery_date, detail.delivery_date)) AS avg_variance_days
		FROM " . TB_PREF . "grn_items item
		INNER JOIN " . TB_PREF . "grn_batch batch ON batch.id = item.grn_batch_id
		INNER JOIN " . TB_PREF . "purch_order_details detail ON detail.po_detail_item = item.po_detail_item
		INNER JOIN " . TB_PREF . "purch_orders purchase_order ON purchase_order.order_no = detail.order_no
		WHERE batch.supplier_id = " . (int)$supplier_id;

	if ($period_from !== '')
		$sql .= " AND batch.delivery_date >= " . db_escape($period_from);
	if ($period_to !== '')
		$sql .= " AND batch.delivery_date <= " . db_escape($period_to);

	$result = db_query($sql, 'could not calculate vendor delivery performance');
	$row = db_fetch($result);

	$delivery_count = $row ? (int)$row['delivery_count'] : 0;
	$on_time_count = $row ? (int)$row['on_time_count'] : 0;

	return array(
		'delivery_count' => $delivery_count,
		'on_time_count' => $on_time_count,
		'late_count' => $row ? (int)$row['late_count'] : 0,
		'early_count' => $row ? (int)$row['early_count'] : 0,
		'on_time_pct' => $delivery_count > 0 ? round2(($on_time_count / $delivery_count) * 100, 2) : 0,
		'avg_lead_time' => $row && $row['avg_lead_time'] !== null ? round2($row['avg_lead_time'], 2) : 0,
		'avg_variance_days' => $row && $row['avg_variance_days'] !== null ? round2($row['avg_variance_days'], 2) : 0,
	);
}

/**
 * Calculate quality performance metrics.
 *
 * @param int    $supplier_id
 * @param string $period_from
 * @param string $period_to
 * @return array
 */
function calculate_quality_performance($supplier_id, $period_from = '', $period_to = '')
{
	$sql = "SELECT
		COUNT(*) AS inspected_count,
		SUM(CASE WHEN item.inspection_status = 'pass' THEN 1 ELSE 0 END) AS pass_count,
		SUM(CASE WHEN item.inspection_status = 'fail' THEN 1 ELSE 0 END) AS fail_count,
		SUM(CASE WHEN item.inspection_status = 'pending' THEN 1 ELSE 0 END) AS pending_count
		FROM " . TB_PREF . "grn_items item
		INNER JOIN " . TB_PREF . "grn_batch batch ON batch.id = item.grn_batch_id
		WHERE batch.supplier_id = " . (int)$supplier_id;

	if ($period_from !== '')
		$sql .= " AND batch.delivery_date >= " . db_escape($period_from);
	if ($period_to !== '')
		$sql .= " AND batch.delivery_date <= " . db_escape($period_to);

	$result = db_query($sql, 'could not calculate vendor quality performance');
	$row = db_fetch($result);

	$inspected_count = $row ? (int)$row['inspected_count'] : 0;
	$pass_count = $row ? (int)$row['pass_count'] : 0;
	$fail_count = $row ? (int)$row['fail_count'] : 0;

	$sql = "SELECT COUNT(*) AS credit_count
		FROM " . TB_PREF . "supp_trans
		WHERE supplier_id = " . (int)$supplier_id . "
		AND type = " . ST_SUPPCREDIT;
	if ($period_from !== '')
		$sql .= " AND tran_date >= " . db_escape($period_from);
	if ($period_to !== '')
		$sql .= " AND tran_date <= " . db_escape($period_to);
	$credit_result = db_query($sql, 'could not calculate supplier credit count');
	$credit_row = db_fetch($credit_result);

	return array(
		'inspected_count' => $inspected_count,
		'pass_count' => $pass_count,
		'fail_count' => $fail_count,
		'pending_count' => $row ? (int)$row['pending_count'] : 0,
		'inspection_pass_rate_pct' => $inspected_count > 0 ? round2(($pass_count / $inspected_count) * 100, 2) : 0,
		'defect_rate_pct' => $inspected_count > 0 ? round2(($fail_count / $inspected_count) * 100, 2) : 0,
		'credit_count' => $credit_row ? (int)$credit_row['credit_count'] : 0,
	);
}

/**
 * Calculate price competitiveness metrics.
 *
 * @param int    $supplier_id
 * @param string $period_from
 * @param string $period_to
 * @return array
 */
function calculate_price_competitiveness($supplier_id, $period_from = '', $period_to = '')
{
	$supplier_period_sql = '';
	$market_period_sql = '';
	if ($period_from !== '') {
		$supplier_period_sql .= " AND trans.tran_date >= " . db_escape($period_from);
		$market_period_sql .= " AND trans.tran_date >= " . db_escape($period_from);
	}
	if ($period_to !== '') {
		$supplier_period_sql .= " AND trans.tran_date <= " . db_escape($period_to);
		$market_period_sql .= " AND trans.tran_date <= " . db_escape($period_to);
	}

	$sql = "SELECT supplier_prices.stock_id, supplier_prices.supplier_avg_price, market_prices.market_avg_price
		FROM (
			SELECT item.stock_id, AVG(item.unit_price) AS supplier_avg_price
			FROM " . TB_PREF . "supp_invoice_items item
			INNER JOIN " . TB_PREF . "supp_trans trans
				ON trans.trans_no = item.supp_trans_no
				AND trans.type = item.supp_trans_type
			WHERE trans.type = " . ST_SUPPINVOICE . "
				AND trans.supplier_id = " . (int)$supplier_id . "
				AND item.stock_id <> ''" . $supplier_period_sql . "
			GROUP BY item.stock_id
		) supplier_prices
		INNER JOIN (
			SELECT item.stock_id, AVG(item.unit_price) AS market_avg_price
			FROM " . TB_PREF . "supp_invoice_items item
			INNER JOIN " . TB_PREF . "supp_trans trans
				ON trans.trans_no = item.supp_trans_no
				AND trans.type = item.supp_trans_type
			WHERE trans.type = " . ST_SUPPINVOICE . "
				AND item.stock_id <> ''" . $market_period_sql . "
			GROUP BY item.stock_id
		) market_prices ON market_prices.stock_id = supplier_prices.stock_id";

	$result = db_query($sql, 'could not calculate vendor price competitiveness');

	$total_score = 0;
	$total_variance = 0;
	$item_count = 0;
	while ($row = db_fetch($result)) {
		if ((float)$row['market_avg_price'] <= 0)
			continue;

		$variance_pct = (($row['supplier_avg_price'] - $row['market_avg_price']) / $row['market_avg_price']) * 100;
		$score = 100 - max($variance_pct, 0);
		$score = max(0, min(100, $score));

		$total_score += $score;
		$total_variance += $variance_pct;
		$item_count++;
	}

	return array(
		'comparable_item_count' => $item_count,
		'average_variance_pct' => $item_count > 0 ? round2($total_variance / $item_count, 2) : 0,
		'score' => $item_count > 0 ? round2($total_score / $item_count, 2) : 0,
	);
}

/**
 * Calculate a simple payment reliability score for one supplier.
 *
 * @param int $supplier_id
 * @return float
 */
function calculate_payment_reliability_score($supplier_id)
{
	$sql = "SELECT
		COUNT(*) AS invoice_count,
		SUM(CASE WHEN due_date < " . db_escape(date2sql(Today())) . " AND (ABS(ov_amount + ov_gst + ov_discount) - alloc) > 0 THEN 1 ELSE 0 END) AS overdue_count
		FROM " . TB_PREF . "supp_trans
		WHERE supplier_id = " . (int)$supplier_id . "
		AND type = " . ST_SUPPINVOICE;
	$result = db_query($sql, 'could not calculate payment reliability score');
	$row = db_fetch($result);

	$invoice_count = $row ? (int)$row['invoice_count'] : 0;
	$overdue_count = $row ? (int)$row['overdue_count'] : 0;

	if ($invoice_count <= 0)
		return 100;

	return round2(max(0, 100 - (($overdue_count / $invoice_count) * 100)), 2);
}

/**
 * Add one vendor performance log entry.
 *
 * @param int    $supplier_id
 * @param string $event_type
 * @param string $event_date
 * @param int    $reference_type
 * @param int    $reference_no
 * @param string $details
 * @param float  $impact_score
 * @param mixed  $custom_data
 * @return int
 */
function log_vendor_event($supplier_id, $event_type, $event_date, $reference_type, $reference_no, $details, $impact_score = 0, $custom_data = null)
{
	$event_date = normalize_vendor_evaluation_sql_date($event_date, date2sql(Today()));
	$recorded_by = isset($_SESSION['wa_current_user']) ? (int)$_SESSION['wa_current_user']->user : 0;

	$sql = "INSERT INTO " . TB_PREF . "vendor_performance_log
		(supplier_id, event_type, event_date, reference_type, reference_no, details, impact_score, recorded_by, custom_data)
		VALUES (
			" . (int)$supplier_id . ",
			" . db_escape($event_type) . ",
			" . db_escape($event_date) . ",
			" . (int)$reference_type . ",
			" . (int)$reference_no . ",
			" . db_escape($details) . ",
			" . db_escape($impact_score) . ",
			" . (int)$recorded_by . ",
			" . sql_vendor_evaluation_custom_data($custom_data) . ")";

	db_query($sql, 'could not insert vendor performance log');
	$event_id = db_insert_id();

	$event_data = array(
		'id' => $event_id,
		'supplier_id' => (int)$supplier_id,
		'event_type' => $event_type,
		'event_date' => $event_date,
		'reference_type' => (int)$reference_type,
		'reference_no' => (int)$reference_no,
		'details' => $details,
		'impact_score' => (float)$impact_score,
		'custom_data' => $custom_data,
	);
	hook_invoke_all('vendor_event_logged', $event_data);

	return $event_id;
}

/**
 * Get vendor performance log rows.
 *
 * @param int    $supplier_id
 * @param string $date_from
 * @param string $date_to
 * @param string $event_type
 * @return resource
 */
function get_vendor_events($supplier_id, $date_from = '', $date_to = '', $event_type = '')
{
	$sql = "SELECT log.*, user.real_name AS recorded_by_name
		FROM " . TB_PREF . "vendor_performance_log log
		LEFT JOIN " . TB_PREF . "users user ON user.id = log.recorded_by
		WHERE log.supplier_id = " . (int)$supplier_id;

	if ($date_from !== '')
		$sql .= " AND log.event_date >= " . db_escape($date_from);
	if ($date_to !== '')
		$sql .= " AND log.event_date <= " . db_escape($date_to);
	if ($event_type !== '')
		$sql .= " AND log.event_type = " . db_escape($event_type);

	$sql .= " ORDER BY log.event_date DESC, log.id DESC";

	return db_query($sql, 'could not retrieve vendor events');
}

/**
 * Get a grouped vendor performance summary.
 *
 * @param int $supplier_id
 * @param int $months
 * @return array
 */
function get_vendor_performance_summary($supplier_id, $months = 12)
{
	$date_from = date('Y-m-d', strtotime('-' . (int)$months . ' months'));
	$sql = "SELECT event_type, COUNT(*) AS event_count, SUM(impact_score) AS total_impact
		FROM " . TB_PREF . "vendor_performance_log
		WHERE supplier_id = " . (int)$supplier_id . "
		AND event_date >= " . db_escape($date_from) . "
		GROUP BY event_type";
	$result = db_query($sql, 'could not retrieve vendor performance summary');

	$summary = array();
	while ($row = db_fetch($result))
		$summary[$row['event_type']] = $row;

	$recent_events = array();
	$events_result = get_vendor_events($supplier_id, $date_from, date2sql(Today()));
	while ($row = db_fetch($events_result)) {
		$recent_events[] = $row;
		if (count($recent_events) >= 10)
			break;
	}

	return array(
		'counts' => $summary,
		'recent_events' => $recent_events,
	);
}

/**
 * Get a complete vendor scorecard payload.
 *
 * @param int $supplier_id
 * @return array
 */
function get_vendor_scorecard($supplier_id)
{
	$supplier = get_supplier($supplier_id);
	if (!$supplier)
		return array();

	$period_from = date('Y-m-d', strtotime('-12 months'));
	$period_to = date2sql(Today());

	$approved_evaluations = array();
	$trend = array();
	$result = get_vendor_evaluations($supplier_id, '', '');
	while ($row = db_fetch($result)) {
		if ($row['status'] !== 'approved' && count($approved_evaluations) >= 10)
			continue;

		if ($row['status'] === 'approved') {
			$approved_evaluations[] = $row;
			$trend[] = array(
				'evaluation_date' => $row['evaluation_date'],
				'overall_score' => $row['overall_score'],
				'quality_score' => $row['quality_score'],
				'delivery_score' => $row['delivery_score'],
				'price_score' => $row['price_score'],
				'service_score' => $row['service_score'],
			);
		}
	}

	return array(
		'supplier' => $supplier,
		'delivery_metrics' => calculate_delivery_performance($supplier_id, $period_from, $period_to),
		'quality_metrics' => calculate_quality_performance($supplier_id, $period_from, $period_to),
		'price_metrics' => calculate_price_competitiveness($supplier_id, $period_from, $period_to),
		'payment_reliability_score' => calculate_payment_reliability_score($supplier_id),
		'performance_summary' => get_vendor_performance_summary($supplier_id, 12),
		'approved_evaluations' => $approved_evaluations,
		'trend' => array_reverse($trend),
	);
}

/**
 * Get the highest-ranked vendors for one score category.
 *
 * @param int    $top_n
 * @param string $category
 * @return resource
 */
function get_vendor_ranking($top_n = 20, $category = 'overall')
{
	$field_map = array(
		'overall' => 'overall_score',
		'quality' => 'quality_score',
		'delivery' => 'delivery_score',
		'price' => 'price_score',
		'service' => 'service_score',
	);
	$field = isset($field_map[$category]) ? $field_map[$category] : $field_map['overall'];

	$sql = "SELECT supplier_id, supp_name, vendor_tier, vendor_category,
		overall_score, quality_score, delivery_score, price_score, service_score,
		on_time_delivery_pct, defect_rate_pct, lead_time_average
		FROM " . TB_PREF . "suppliers
		WHERE inactive = 0
		ORDER BY " . $field . " DESC, supp_name
		LIMIT " . (int)$top_n;

	return db_query($sql, 'could not retrieve vendor ranking');
}

/**
 * Get suppliers with evaluations due soon.
 *
 * @param int $days_ahead
 * @return resource
 */
function get_vendors_due_evaluation($days_ahead = 30)
{
	$due_date = date('Y-m-d', strtotime('+' . (int)$days_ahead . ' days'));
	$sql = "SELECT supplier_id, supp_name, vendor_tier, next_evaluation_date, last_evaluation_date,
		evaluation_frequency_months, overall_score
		FROM " . TB_PREF . "suppliers
		WHERE inactive = 0
		AND next_evaluation_date IS NOT NULL
		AND next_evaluation_date <= " . db_escape($due_date) . "
		ORDER BY next_evaluation_date, supp_name";

	return db_query($sql, 'could not retrieve vendors due for evaluation');
}

/**
 * Update supplier operational metrics and score fields.
 *
 * @param int    $supplier_id
 * @param array  $metrics
 * @param array  $score_overrides
 * @param string $evaluation_date
 * @return bool
 */
function update_supplier_vendor_metrics($supplier_id, $metrics, $score_overrides = array(), $evaluation_date = '')
{
	$supplier = get_supplier($supplier_id);
	if (!$supplier)
		return false;

	$updates = array(
		"lead_time_average = " . db_escape((int)round($metrics['lead_time_average'])),
		"on_time_delivery_pct = " . db_escape($metrics['on_time_delivery_pct']),
		"defect_rate_pct = " . db_escape($metrics['defect_rate_pct']),
		"payment_reliability_score = " . db_escape($metrics['payment_reliability_score']),
	);

	if (isset($score_overrides['overall_score']))
		$updates[] = "overall_score = " . db_escape($score_overrides['overall_score']);
	if (isset($score_overrides['quality_score']))
		$updates[] = "quality_score = " . db_escape($score_overrides['quality_score']);
	if (isset($score_overrides['delivery_score']))
		$updates[] = "delivery_score = " . db_escape($score_overrides['delivery_score']);
	if (isset($score_overrides['price_score']))
		$updates[] = "price_score = " . db_escape($score_overrides['price_score']);
	if (isset($score_overrides['service_score']))
		$updates[] = "service_score = " . db_escape($score_overrides['service_score']);
	if (isset($score_overrides['vendor_tier']))
		$updates[] = "vendor_tier = " . db_escape($score_overrides['vendor_tier']);

	if ($evaluation_date !== '') {
		$next_evaluation_date = $evaluation_date;
		if (!empty($supplier['evaluation_frequency_months']))
			$next_evaluation_date = date('Y-m-d', strtotime('+' . (int)$supplier['evaluation_frequency_months'] . ' months', strtotime($evaluation_date)));

		$updates[] = "last_evaluation_date = " . db_escape($evaluation_date);
		$updates[] = "next_evaluation_date = " . db_escape($next_evaluation_date);
	}

	$sql = "UPDATE " . TB_PREF . "suppliers SET\n\t\t" . implode(",\n\t\t", $updates) . "\n\t\tWHERE supplier_id = " . (int)$supplier_id;
	db_query($sql, 'could not update supplier vendor metrics');

	return true;
}

/**
 * Approve one vendor evaluation and copy its summary to the supplier.
 *
 * @param int $evaluation_id
 * @param int $approved_by
 * @return bool
 */
function approve_evaluation($evaluation_id, $approved_by)
{
	$evaluation = get_vendor_evaluation($evaluation_id);
	if (!$evaluation)
		return false;

	$totals = calculate_evaluation_totals($evaluation_id);
	$delivery_metrics = calculate_delivery_performance($evaluation['supplier_id'], $evaluation['period_from'], $evaluation['period_to']);
	$quality_metrics = calculate_quality_performance($evaluation['supplier_id'], $evaluation['period_from'], $evaluation['period_to']);
	$price_metrics = calculate_price_competitiveness($evaluation['supplier_id'], $evaluation['period_from'], $evaluation['period_to']);
	$payment_reliability_score = calculate_payment_reliability_score($evaluation['supplier_id']);

	$sql = "UPDATE " . TB_PREF . "vendor_evaluations SET status = 'approved'
		WHERE id = " . (int)$evaluation_id;
	db_query($sql, 'could not approve vendor evaluation');

	$new_tier = get_vendor_tier_for_score($totals['overall_score']);
	$old_tier = $evaluation['vendor_tier'];
	update_supplier_vendor_metrics(
		$evaluation['supplier_id'],
		array(
			'lead_time_average' => $delivery_metrics['avg_lead_time'],
			'on_time_delivery_pct' => $delivery_metrics['on_time_pct'],
			'defect_rate_pct' => $quality_metrics['defect_rate_pct'],
			'payment_reliability_score' => $payment_reliability_score,
		),
		array(
			'overall_score' => $totals['overall_score'],
			'quality_score' => $totals['category_scores']['quality'],
			'delivery_score' => $totals['category_scores']['delivery'],
			'price_score' => $totals['category_scores']['price'],
			'service_score' => $totals['category_scores']['service'],
			'vendor_tier' => $new_tier,
		),
		$evaluation['evaluation_date']
	);

	if ($old_tier !== $new_tier) {
		$tier_change_data = array(
			'supplier_id' => (int)$evaluation['supplier_id'],
			'old_tier' => $old_tier,
			'new_tier' => $new_tier,
			'approved_by' => (int)$approved_by,
		);
		hook_invoke_all('vendor_tier_changed', $tier_change_data);
	}

	$approval_data = array(
		'evaluation_id' => (int)$evaluation_id,
		'supplier_id' => (int)$evaluation['supplier_id'],
		'approved_by' => (int)$approved_by,
		'overall_score' => $totals['overall_score'],
		'category_scores' => $totals['category_scores'],
		'delivery_metrics' => $delivery_metrics,
		'quality_metrics' => $quality_metrics,
		'price_metrics' => $price_metrics,
	);
	hook_invoke_all('vendor_evaluation_approved', $approval_data);

	return true;
}

/**
 * Recalculate operational metrics for all active vendors.
 *
 * @return int
 */
function batch_calculate_vendor_scores()
{
	$sql = "SELECT supplier_id, overall_score, quality_score, delivery_score, price_score, service_score
		FROM " . TB_PREF . "suppliers
		WHERE inactive = 0";
	$result = db_query($sql, 'could not retrieve suppliers for vendor score batch');

	$updated_count = 0;
	$period_from = date('Y-m-d', strtotime('-12 months'));
	$period_to = date2sql(Today());
	while ($supplier = db_fetch($result)) {
		$delivery_metrics = calculate_delivery_performance($supplier['supplier_id'], $period_from, $period_to);
		$quality_metrics = calculate_quality_performance($supplier['supplier_id'], $period_from, $period_to);
		$price_metrics = calculate_price_competitiveness($supplier['supplier_id'], $period_from, $period_to);
		$payment_reliability_score = calculate_payment_reliability_score($supplier['supplier_id']);

		$scores = array(
			'delivery_score' => $supplier['delivery_score'] > 0 ? $supplier['delivery_score'] : $delivery_metrics['on_time_pct'],
			'quality_score' => $supplier['quality_score'] > 0 ? $supplier['quality_score'] : max(0, 100 - $quality_metrics['defect_rate_pct']),
			'price_score' => $supplier['price_score'] > 0 ? $supplier['price_score'] : $price_metrics['score'],
			'overall_score' => $supplier['overall_score'],
			'service_score' => $supplier['service_score'],
		);
		hook_invoke_all('vendor_score_calculated', $scores, $supplier['supplier_id']);

		update_supplier_vendor_metrics(
			$supplier['supplier_id'],
			array(
				'lead_time_average' => $delivery_metrics['avg_lead_time'],
				'on_time_delivery_pct' => $delivery_metrics['on_time_pct'],
				'defect_rate_pct' => $quality_metrics['defect_rate_pct'],
				'payment_reliability_score' => $payment_reliability_score,
			),
			array(
				'delivery_score' => $scores['delivery_score'],
				'quality_score' => $scores['quality_score'],
				'price_score' => $scores['price_score'],
			)
		);
		$updated_count++;
	}

	return $updated_count;
}