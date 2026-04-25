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

$page_security = 'SA_VENDOREVALUATION';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/ui/ui_lists.inc');
include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Vendor Evaluation Entry'), false, false, '', $js);

/**
 * Validate the vendor evaluation header inputs.
 *
 * @return bool
 */
function can_save_vendor_evaluation_header()
{
	if (get_post('supplier_id') === '' || get_post('supplier_id') == ALL_TEXT) {
		display_error(_('You must select a supplier.'));
		set_focus('supplier_id');
		return false;
	}

	if (get_post('evaluator_id') === '' || get_post('evaluator_id') == ALL_TEXT) {
		display_error(_('You must select an evaluator.'));
		set_focus('evaluator_id');
		return false;
	}

	if (!is_date(get_post('evaluation_date'))) {
		display_error(_('The evaluation date is invalid.'));
		set_focus('evaluation_date');
		return false;
	}

	if (!is_date(get_post('period_from')) || !is_date(get_post('period_to'))) {
		display_error(_('The evaluation period is invalid.'));
		set_focus('period_from');
		return false;
	}

	if (date1_greater_date2(get_post('period_from'), get_post('period_to'))) {
		display_error(_('The evaluation start date cannot be later than the end date.'));
		set_focus('period_from');
		return false;
	}

	return true;
}

/**
 * Resolve the computed score for one calculated criteria formula.
 *
 * @param string $formula
 * @param array  $metrics
 * @return float
 */
function resolve_vendor_formula_score($formula, $metrics)
{
	$formula = trim($formula);
	if ($formula === 'inspection_pass_rate_pct')
		return isset($metrics['quality']['inspection_pass_rate_pct']) ? (float)$metrics['quality']['inspection_pass_rate_pct'] : 0;
	if ($formula === '100 - defect_rate_pct')
		return max(0, 100 - (isset($metrics['quality']['defect_rate_pct']) ? (float)$metrics['quality']['defect_rate_pct'] : 0));
	if ($formula === 'on_time_delivery_pct')
		return isset($metrics['delivery']['on_time_pct']) ? (float)$metrics['delivery']['on_time_pct'] : 0;
	if ($formula === 'price_competitiveness_score')
		return isset($metrics['price']['score']) ? (float)$metrics['price']['score'] : 0;

	return 0;
}

/**
 * Build the operational metric bundle used by the evaluation form.
 *
 * @param int    $supplier_id
 * @param string $period_from
 * @param string $period_to
 * @return array
 */
function get_vendor_form_metrics($supplier_id, $period_from, $period_to)
{
	if ((int)$supplier_id <= 0 || !is_date($period_from) || !is_date($period_to)) {
		return array(
			'delivery' => array(),
			'quality' => array(),
			'price' => array(),
			'payment_reliability_score' => 0,
		);
	}

	$period_from_sql = date2sql($period_from);
	$period_to_sql = date2sql($period_to);

	return array(
		'delivery' => calculate_delivery_performance($supplier_id, $period_from_sql, $period_to_sql),
		'quality' => calculate_quality_performance($supplier_id, $period_from_sql, $period_to_sql),
		'price' => calculate_price_competitiveness($supplier_id, $period_from_sql, $period_to_sql),
		'payment_reliability_score' => calculate_payment_reliability_score($supplier_id),
	);
}

/**
 * Get an associative map of saved scores for one evaluation.
 *
 * @param int $evaluation_id
 * @return array
 */
function get_vendor_evaluation_score_map($evaluation_id)
{
	$score_map = array();
	if ((int)$evaluation_id <= 0)
		return $score_map;

	$result = get_evaluation_scores($evaluation_id);
	while ($row = db_fetch($result))
		$score_map[(int)$row['criteria_id']] = $row;

	return $score_map;
}

/**
 * Save the posted score rows for one evaluation.
 *
 * @param int   $evaluation_id
 * @param array $criteria_rows
 * @param array $metrics
 * @return void
 */
function save_vendor_evaluation_scores_from_post($evaluation_id, $criteria_rows, $metrics)
{
	foreach ($criteria_rows as $criteria) {
		$score_key = 'criteria_score_' . $criteria['id'];
		$evidence_key = 'criteria_evidence_' . $criteria['id'];
		$notes_key = 'criteria_notes_' . $criteria['id'];

		$raw_score = trim(get_post($score_key, ''));
		if ($raw_score === '' && $criteria['scoring_method'] === 'calculated')
			$raw_score = resolve_vendor_formula_score($criteria['calculation_formula'], $metrics);

		if ($raw_score === '')
			continue;

		set_evaluation_score(
			$evaluation_id,
			$criteria['id'],
			(float)user_numeric($raw_score),
			trim(get_post($evidence_key, '')),
			trim(get_post($notes_key, ''))
		);
	}
}

/**
 * Populate the form state from one evaluation record.
 *
 * @param array $evaluation
 * @return void
 */
function load_vendor_evaluation_into_post($evaluation)
{
	$_POST['supplier_id'] = $evaluation['supplier_id'];
	$_POST['evaluation_date'] = sql2date($evaluation['evaluation_date']);
	$_POST['evaluator_id'] = $evaluation['evaluator_id'];
	$_POST['period_from'] = sql2date($evaluation['period_from']);
	$_POST['period_to'] = sql2date($evaluation['period_to']);
	$_POST['recommendation'] = $evaluation['recommendation'];
	$_POST['action_plan'] = $evaluation['action_plan'];
	$_POST['notes'] = $evaluation['notes'];
}

/**
 * Build query parameters while preserving the purchases app context.
 *
 * @param string $params
 * @return string
 */
function get_vendor_evaluation_nav_params($params = '')
{
	return 'sel_app=AP' . ($params !== '' ? '&' . $params : '');
}

$selected_id = get_post('selected_id', 0);
if (!$selected_id && isset($_GET['evaluation_id']))
	$selected_id = (int)$_GET['evaluation_id'];
if (isset($_GET['New']))
	$selected_id = 0;

if (!isset($_POST['supplier_id']) && isset($_GET['supplier_id']))
	$_POST['supplier_id'] = (int)$_GET['supplier_id'];
if (!isset($_POST['evaluator_id']))
	$_POST['evaluator_id'] = isset($_SESSION['wa_current_user']) ? (int)$_SESSION['wa_current_user']->user : 0;
if (!isset($_POST['evaluation_date']))
	$_POST['evaluation_date'] = Today();
if (!isset($_POST['period_from']))
	$_POST['period_from'] = begin_month(Today());
if (!isset($_POST['period_to']))
	$_POST['period_to'] = end_month(Today());
if (!isset($_POST['recommendation']))
	$_POST['recommendation'] = 'maintain';

if (isset($_GET['notice'])) {
	$notices = array(
		'saved' => _('Vendor evaluation has been saved.'),
		'submitted' => _('Vendor evaluation has been submitted.'),
		'approved' => _('Vendor evaluation has been approved.'),
		'deleted' => _('Vendor evaluation has been deleted.'),
	);
	if (isset($notices[$_GET['notice']]))
		display_notification($notices[$_GET['notice']]);
}

$criteria_rows = array();
$criteria_result = get_evaluation_criteria(false);
while ($row = db_fetch($criteria_result))
	$criteria_rows[] = $row;

if (isset($_POST['delete_evaluation']) && $selected_id > 0) {
	$supplier_id = isset($_POST['supplier_id']) ? (int)$_POST['supplier_id'] : 0;
	if (delete_vendor_evaluation($selected_id))
		meta_forward($_SERVER['PHP_SELF'], get_vendor_evaluation_nav_params('supplier_id=' . $supplier_id . '&notice=deleted'));
	else
		display_error(_('The selected evaluation could not be deleted.'));
}

if ((isset($_POST['save_evaluation']) || isset($_POST['submit_evaluation']) || isset($_POST['approve_evaluation'])) && can_save_vendor_evaluation_header()) {
	if ($selected_id > 0) {
		update_vendor_evaluation(
			$selected_id,
			(int)get_post('supplier_id'),
			get_post('period_from'),
			get_post('period_to'),
			(int)get_post('evaluator_id'),
			get_post('evaluation_date'),
			get_post('recommendation', 'maintain'),
			trim(get_post('action_plan')),
			trim(get_post('notes')),
			'draft'
		);
	} else {
		$selected_id = add_vendor_evaluation(
			(int)get_post('supplier_id'),
			get_post('period_from'),
			get_post('period_to'),
			(int)get_post('evaluator_id'),
			get_post('evaluation_date'),
			get_post('recommendation', 'maintain'),
			trim(get_post('action_plan')),
			trim(get_post('notes')),
			'draft'
		);
	}

	$metrics = get_vendor_form_metrics((int)get_post('supplier_id'), get_post('period_from'), get_post('period_to'));
	save_vendor_evaluation_scores_from_post($selected_id, $criteria_rows, $metrics);
	calculate_evaluation_totals($selected_id);

	if (isset($_POST['submit_evaluation'])) {
		submit_evaluation($selected_id);
		meta_forward($_SERVER['PHP_SELF'], get_vendor_evaluation_nav_params('evaluation_id=' . $selected_id . '&notice=submitted'));
	}

	if (isset($_POST['approve_evaluation'])) {
		approve_evaluation($selected_id, isset($_SESSION['wa_current_user']) ? (int)$_SESSION['wa_current_user']->user : 0);
		meta_forward($_SERVER['PHP_SELF'], get_vendor_evaluation_nav_params('evaluation_id=' . $selected_id . '&notice=approved'));
	}

	meta_forward($_SERVER['PHP_SELF'], get_vendor_evaluation_nav_params('evaluation_id=' . $selected_id . '&notice=saved'));
}

if ($selected_id > 0 && !isset($_POST['save_evaluation']) && !isset($_POST['submit_evaluation']) && !isset($_POST['approve_evaluation'])) {
	$evaluation = get_vendor_evaluation($selected_id);
	if ($evaluation)
		load_vendor_evaluation_into_post($evaluation);
}

$current_evaluation = $selected_id > 0 ? get_vendor_evaluation($selected_id) : false;
$score_map = get_vendor_evaluation_score_map($selected_id);
$metrics = get_vendor_form_metrics((int)get_post('supplier_id', 0), get_post('period_from'), get_post('period_to'));
$recommendation_options = get_vendor_recommendations();
$status_options = get_vendor_evaluation_statuses();
$category_labels = get_vendor_evaluation_categories();

start_form();
hidden('selected_id', $selected_id);

if ($current_evaluation) {
	echo '<div style="display:flex;justify-content:space-between;align-items:center;gap:12px;flex-wrap:wrap;margin-bottom:10px;">';
	echo '<div><strong>' . _('Evaluation #') . (int)$current_evaluation['id'] . '</strong> &nbsp; ' . vendor_evaluation_status_badge($current_evaluation['status']) . '</div>';
	echo '<div>';
	hyperlink_params($_SERVER['PHP_SELF'], _('New Evaluation'), get_vendor_evaluation_nav_params('New=1&supplier_id=' . (int)$current_evaluation['supplier_id']));
	echo '&nbsp;&nbsp;';
	hyperlink_params($path_to_root . '/purchasing/inquiry/vendor_scorecard.php', _('View Scorecard'), 'sel_app=AP&supplier_id=' . (int)$current_evaluation['supplier_id']);
	echo '</div>';
	echo '</div>';
}

start_outer_table(TABLESTYLE2, "width='100%'");

table_section(1);
supplier_list_row(_('Supplier:'), 'supplier_id', get_post('supplier_id', null), false, false, false, true);
users_list_row(_('Evaluator:'), 'evaluator_id', get_post('evaluator_id'), false, false);
date_row(_('Evaluation Date:'), 'evaluation_date');
date_row(_('Period From:'), 'period_from');
date_row(_('Period To:'), 'period_to');

echo "<tr><td class='label'>" . _('Recommendation:') . "</td><td>";
echo array_selector('recommendation', get_post('recommendation', 'maintain'), $recommendation_options, array('class' => array('nosearch')));
echo "</td></tr>\n";

table_section(2);
textarea_row(_('Action Plan:'), 'action_plan', get_post('action_plan', ''), 45, 4);
textarea_row(_('Notes:'), 'notes', get_post('notes', ''), 45, 4);

if ($current_evaluation) {
	label_row(_('Overall Score:'), number_format2($current_evaluation['overall_score'], 2));
	label_row(_('Quality Score:'), number_format2($current_evaluation['quality_score'], 2));
	label_row(_('Delivery Score:'), number_format2($current_evaluation['delivery_score'], 2));
	label_row(_('Price Score:'), number_format2($current_evaluation['price_score'], 2));
	label_row(_('Service Score:'), number_format2($current_evaluation['service_score'], 2));
	label_row(_('Status:'), isset($status_options[$current_evaluation['status']]) ? $status_options[$current_evaluation['status']] : $current_evaluation['status']);
	label_row(_('Supplier Tier:'), vendor_tier_badge($current_evaluation['vendor_tier'] ? $current_evaluation['vendor_tier'] : 'standard'));
}

end_outer_table(1);

echo '<div style="display:flex;gap:10px;flex-wrap:wrap;margin:12px 0;">';
$metric_cards = array(
	array('label' => _('On-Time Delivery'), 'value' => isset($metrics['delivery']['on_time_pct']) ? $metrics['delivery']['on_time_pct'] . '%' : '0%'),
	array('label' => _('Average Lead Time'), 'value' => isset($metrics['delivery']['avg_lead_time']) ? round2($metrics['delivery']['avg_lead_time'], 2) . ' ' . _('days') : '0'),
	array('label' => _('Inspection Pass Rate'), 'value' => isset($metrics['quality']['inspection_pass_rate_pct']) ? $metrics['quality']['inspection_pass_rate_pct'] . '%' : '0%'),
	array('label' => _('Defect Rate'), 'value' => isset($metrics['quality']['defect_rate_pct']) ? $metrics['quality']['defect_rate_pct'] . '%' : '0%'),
	array('label' => _('Price Score'), 'value' => isset($metrics['price']['score']) ? $metrics['price']['score'] : 0),
	array('label' => _('Payment Reliability'), 'value' => isset($metrics['payment_reliability_score']) ? $metrics['payment_reliability_score'] : 0),
);
foreach ($metric_cards as $metric_card) {
	echo '<div style="min-width:170px;padding:10px 12px;background:#f8fafc;border:1px solid #dbe4ee;border-radius:4px;">';
	echo '<div style="font-size:12px;color:#64748b;">' . $metric_card['label'] . '</div>';
	echo '<div style="font-size:18px;font-weight:bold;color:#0f172a;">' . $metric_card['value'] . '</div>';
	echo '</div>';
}
echo '</div>';

display_heading(_('Criteria Scoring Grid'));
if (empty($criteria_rows)) {
	display_note(_('No criteria are configured yet. Create evaluation criteria first.'), 0, 1);
	hyperlink_no_params($path_to_root . '/purchasing/manage/vendor_evaluation_criteria.php', _('Open Vendor Evaluation Criteria'));
} else {
	start_table(TABLESTYLE, "width='100%'");
	table_header(array(_('Category'), _('Criteria'), _('Weight'), _('Method'), _('Score'), _('Evidence'), _('Notes')));

	$k = 0;
	foreach ($criteria_rows as $criteria) {
		alt_table_row_color($k);
		$saved_score = isset($score_map[$criteria['id']]) ? $score_map[$criteria['id']] : null;
		$display_score = $saved_score ? $saved_score['score'] : '';
		if ($display_score === '' && $criteria['scoring_method'] === 'calculated')
			$display_score = round2(resolve_vendor_formula_score($criteria['calculation_formula'], $metrics), 2);

		label_cell(isset($category_labels[$criteria['category']]) ? $category_labels[$criteria['category']] : $criteria['category']);
		label_cell('<strong>' . htmlspecialchars($criteria['name']) . '</strong><br><span style="font-size:11px;color:#64748b;">' . htmlspecialchars($criteria['description']) . '</span>');
		qty_cell($criteria['weight']);
		label_cell(ucfirst($criteria['scoring_method']) . ($criteria['calculation_formula'] ? '<br><span style="font-size:11px;color:#64748b;">' . htmlspecialchars($criteria['calculation_formula']) . '</span>' : ''));
		echo '<td><input type="text" name="criteria_score_' . $criteria['id'] . '" value="' . htmlspecialchars((string)$display_score, ENT_QUOTES, 'UTF-8') . '" size="8" class="amount"></td>';
		echo '<td><input type="text" name="criteria_evidence_' . $criteria['id'] . '" value="' . htmlspecialchars($saved_score ? $saved_score['evidence'] : '', ENT_QUOTES, 'UTF-8') . '" size="25"></td>';
		echo '<td><input type="text" name="criteria_notes_' . $criteria['id'] . '" value="' . htmlspecialchars($saved_score ? $saved_score['notes'] : '', ENT_QUOTES, 'UTF-8') . '" size="35"></td>';
		end_row();
	}

	end_table(1);
}

echo '<div style="text-align:center;margin:16px 0;">';
submit('save_evaluation', _('Save Evaluation'), true, _('Save the current vendor evaluation'), 'default');
echo '&nbsp;';
submit('submit_evaluation', _('Submit Evaluation'), true, _('Submit this evaluation for review'));
if ($selected_id > 0)
	echo '&nbsp;';
if ($selected_id > 0)
	submit('approve_evaluation', _('Approve Evaluation'), true, _('Approve this evaluation and update supplier scores'));
if ($selected_id > 0)
	echo '&nbsp;';
if ($selected_id > 0)
	submit('delete_evaluation', _('Delete Evaluation'), true, _('Delete this evaluation while still editable'));
echo '</div>';

display_heading(_('Recent Evaluations'));
start_table(TABLESTYLE, "width='100%'");
table_header(array(_('ID'), _('Date'), _('Supplier'), _('Evaluator'), _('Status'), _('Overall Score'), _('Recommendation'), ''));

$evaluation_rows = get_vendor_evaluations((int)get_post('supplier_id', 0));
$k = 0;
while ($row = db_fetch($evaluation_rows)) {
	alt_table_row_color($k);
	label_cell($row['id']);
	label_cell(sql2date($row['evaluation_date']));
	label_cell($row['supp_name']);
	label_cell($row['evaluator_name']);
	label_cell(vendor_evaluation_status_badge($row['status']));
	label_cell(number_format2($row['overall_score'], 2), 'align=right');
	label_cell(isset($recommendation_options[$row['recommendation']]) ? $recommendation_options[$row['recommendation']] : $row['recommendation']);
	label_cell('<a href="' . $_SERVER['PHP_SELF'] . '?' . get_vendor_evaluation_nav_params('evaluation_id=' . (int)$row['id']) . '">' . _('Open') . '</a>');
	end_row();
	if ($k > 14)
		break;
}
if ($k == 0)
	label_row('', _('No vendor evaluations have been created yet.'), 'colspan=8 align=center');
end_table(1);

end_form();
end_page();