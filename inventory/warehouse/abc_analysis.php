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
 * ABC Classification Analysis page.
 *
 * Calculates ABC classification based on stock movement velocity (Pareto analysis),
 * allows customization of thresholds and analysis period, and applies results
 * to the stock_master table.
 */
$page_security = 'SA_ABC_ANALYSIS';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'ABC Classification Analysis');

page($_SESSION['page_title']);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_locations_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/warehouse_ui.inc');

//-------------------------------------------------------------------------------------
// Handle "Run Analysis" action
//-------------------------------------------------------------------------------------

$analysis_results = null;

if (isset($_POST['RunAnalysis'])) {
	$months = (int)get_post('months', 12);
	$a_threshold = (float)get_post('a_threshold', 80);
	$b_threshold = (float)get_post('b_threshold', 95);
	$loc_code = get_post('loc_code');
	$analysis_method = in_array(get_post('analysis_method'), array('value', 'turnover'))
		? get_post('analysis_method') : 'value';

	// Basic validation
	if ($months < 1 || $months > 120) {
		display_error(_('Analysis period must be between 1 and 120 months.'));
	} elseif ($a_threshold <= 0 || $a_threshold >= 100) {
		display_error(_('Class A threshold must be between 0 and 100.'));
	} elseif ($b_threshold <= $a_threshold || $b_threshold >= 100) {
		display_error(_('Class A+B threshold must be greater than Class A threshold and less than 100.'));
	} else {
		$analysis_results = calculate_abc_classification($months, $a_threshold, $b_threshold, $loc_code, $analysis_method);
	}
}

//-------------------------------------------------------------------------------------
// Handle "Apply Classification" action
//-------------------------------------------------------------------------------------

if (isset($_POST['ApplyClassification'])) {
	$months = (int)get_post('months', 12);
	$a_threshold = (float)get_post('a_threshold', 80);
	$b_threshold = (float)get_post('b_threshold', 95);
	$loc_code = get_post('loc_code');
	$analysis_method = in_array(get_post('analysis_method'), array('value', 'turnover'))
		? get_post('analysis_method') : 'value';

	$classifications = calculate_abc_classification($months, $a_threshold, $b_threshold, $loc_code, $analysis_method);

	if (!empty($classifications)) {
		$count = apply_abc_classification($classifications);
		display_notification(sprintf(_('%d items have been classified and updated.'), $count));
	} else {
		display_warning(_('No items to classify.'));
	}
}

//-------------------------------------------------------------------------------------
// Display current ABC summary
//-------------------------------------------------------------------------------------

echo "<div class='section-header' style='margin-bottom:10px;'>";
echo "<h3 style='margin:0;'><i class='fa fa-bar-chart' style='margin-right:5px;'></i>" . _('Current ABC Classification') . "</h3>";
echo "</div>";

$summary = get_abc_classification_summary();
display_abc_summary_cards($summary);

//-------------------------------------------------------------------------------------
// Analysis parameters form
//-------------------------------------------------------------------------------------

start_form();

echo "<br>";
start_table(TABLESTYLE2);
table_section_title(_('Analysis Parameters'));

// Analysis Method
$method_options = array('value' => _('By Value (cumulative stock issue value — default Pareto)'),
	'turnover' => _('By Turnover Rate (cumulative quantity moved)'));
$current_method = get_post('analysis_method', 'value');
array_selector_row(_('Analysis Method:'), 'analysis_method', $current_method, $method_options);

// Period
text_row(_('Analysis Period (months):'), 'months', get_post('months', '12'), 6, 4);

// Thresholds
text_row(_('Class A Threshold (%):'), 'a_threshold', get_post('a_threshold', '80'), 6, 5,
	_('Items within this cumulative % of total value are Class A'));

text_row(_('Class A+B Threshold (%):'), 'b_threshold', get_post('b_threshold', '95'), 6, 5,
	_('Items within this cumulative % are Class A or B; remainder are Class C'));

// Optional location filter
locations_list_row(_('Filter by Location:'), 'loc_code', get_post('loc_code'), true);

end_table(1);

submit_center_first('RunAnalysis', _('Run Analysis'), _('Calculate ABC classification from movement data'), false, ICON_SUBMIT);
submit_center_last('ApplyClassification', _('Apply to Items'), _('Save classification results to item records'), false, ICON_UPDATE);

//-------------------------------------------------------------------------------------
// Display analysis results
//-------------------------------------------------------------------------------------

if ($analysis_results !== null) {

	echo "<br>";
	echo "<div class='section-header'>";
	echo "<h3 style='margin:0;'><i class='fa fa-list-ol' style='margin-right:5px;'></i>" . _('Analysis Results') . "</h3>";
	echo "</div>";

	// Results summary
	$class_counts = array('A' => 0, 'B' => 0, 'C' => 0);
	$class_values = array('A' => 0, 'B' => 0, 'C' => 0);
	foreach ($analysis_results as $item) {
		$class_counts[$item['abc_class']]++;
		$class_values[$item['abc_class']] += $item['total_value'];
	}
	$total_value = $class_values['A'] + $class_values['B'] + $class_values['C'];
	$total_items = count($analysis_results);

	echo "<div style='display:flex; flex-wrap:wrap; gap:10px; margin:10px 0;'>";

	// Class A summary
	echo "<div style='background:#fff; border:1px solid #ddd; border-left:4px solid " . get_abc_class_color('A') . "; border-radius:4px; padding:10px 20px; min-width:200px; flex:1;'>";
	echo "<strong style='color:" . get_abc_class_color('A') . ";'>" . _('Class A — High Value') . "</strong><br>";
	echo sprintf(_('%d items (%s%% of items)'), $class_counts['A'],
		($total_items > 0 ? round(($class_counts['A'] / $total_items) * 100, 1) : 0)) . "<br>";
	echo sprintf(_('Value: %s (%s%% of total)'), number_format2($class_values['A'], 2),
		($total_value > 0 ? round(($class_values['A'] / $total_value) * 100, 1) : 0));
	echo "</div>";

	// Class B summary
	echo "<div style='background:#fff; border:1px solid #ddd; border-left:4px solid " . get_abc_class_color('B') . "; border-radius:4px; padding:10px 20px; min-width:200px; flex:1;'>";
	echo "<strong style='color:" . get_abc_class_color('B') . ";'>" . _('Class B — Medium Value') . "</strong><br>";
	echo sprintf(_('%d items (%s%% of items)'), $class_counts['B'],
		($total_items > 0 ? round(($class_counts['B'] / $total_items) * 100, 1) : 0)) . "<br>";
	echo sprintf(_('Value: %s (%s%% of total)'), number_format2($class_values['B'], 2),
		($total_value > 0 ? round(($class_values['B'] / $total_value) * 100, 1) : 0));
	echo "</div>";

	// Class C summary
	echo "<div style='background:#fff; border:1px solid #ddd; border-left:4px solid " . get_abc_class_color('C') . "; border-radius:4px; padding:10px 20px; min-width:200px; flex:1;'>";
	echo "<strong style='color:" . get_abc_class_color('C') . ";'>" . _('Class C — Low Value') . "</strong><br>";
	echo sprintf(_('%d items (%s%% of items)'), $class_counts['C'],
		($total_items > 0 ? round(($class_counts['C'] / $total_items) * 100, 1) : 0)) . "<br>";
	echo sprintf(_('Value: %s (%s%% of total)'), number_format2($class_values['C'], 2),
		($total_value > 0 ? round(($class_values['C'] / $total_value) * 100, 1) : 0));
	echo "</div>";

	echo "</div>";

	// Results table
	echo "<br>";
	start_table(TABLESTYLE);

	$th = array(
		_('#'), _('Item Code'), _('Description'), _('Unit'),
		_('Movements'), _('Total Qty'), _('Total Value'),
		_('Cumulative %'), _('Class')
	);
	table_header($th);

	$k = 0;
	foreach ($analysis_results as $item) {
		alt_table_row_color($k);

		label_cell($item['rank'], 'class="center"');
		label_cell($item['stock_id']);
		label_cell($item['description']);
		label_cell(isset($item['units']) ? $item['units'] : '');
		label_cell($item['move_count'], 'class="right"');
		label_cell(number_format2($item['total_qty'], 2), 'class="right"');
		label_cell(number_format2($item['total_value'], 2), 'class="right"');

		// Cumulative % with mini bar
		echo "<td class='right'>";
		echo "<div style='display:flex; align-items:center; gap:5px; justify-content:flex-end;'>";
		echo "<div style='width:60px;'>" . utilization_bar($item['cumulative_pct'], 'ok', $item['cumulative_pct'] . '%', 14) . "</div>";
		echo "</div>";
		echo "</td>";

		label_cell(abc_class_badge($item['abc_class']), 'class="center"');

		end_row();
	}

	end_table(1);

	//-------------------------------------------------------------------------------------
	// Optimization recommendations per class
	//-------------------------------------------------------------------------------------

	$rec_a = $class_counts['A'];
	$rec_b = $class_counts['B'];
	$rec_c = $class_counts['C'];

	echo "<br>";
	echo "<div class='section-header'>";
	echo "<h3 style='margin:0;'><i class='fa fa-lightbulb-o' style='margin-right:5px;'></i>" . _('Optimization Recommendations') . "</h3>";
	echo "</div>";
	echo "<div style='display:flex; flex-wrap:wrap; gap:15px; margin:10px 0;'>";

	// Class A recommendations
	echo "<div style='background:#fff3f3; border:1px solid " . get_abc_class_color('A') . "; border-radius:4px; padding:15px; min-width:280px; flex:1;'>";
	echo "<strong style='color:" . get_abc_class_color('A') . ";'><i class='fa fa-star' style='margin-right:4px;'></i>" . _('Class A — High Value') . " ({$rec_a} " . _('items') . ")</strong>";
	echo "<ul style='margin:8px 0 0 0; padding-left:20px; font-size:13px;'>";
	echo "<li>" . _('Review stock levels weekly; set safety stock at 2–4 weeks demand.') . "</li>";
	echo "<li>" . _('Use tight cycle counting: count at least monthly.') . "</li>";
	echo "<li>" . _('Secure storage with access controls and audit trail.') . "</li>";
	echo "<li>" . _('Use accurate demand forecasting; review supplier lead times.') . "</li>";
	echo "<li>" . _('Negotiate volume pricing or service-level agreements with suppliers.') . "</li>";
	echo "</ul>";
	echo "</div>";

	// Class B recommendations
	echo "<div style='background:#fffdf3; border:1px solid " . get_abc_class_color('B') . "; border-radius:4px; padding:15px; min-width:280px; flex:1;'>";
	echo "<strong style='color:" . get_abc_class_color('B') . ";'><i class='fa fa-star-half-o' style='margin-right:4px;'></i>" . _('Class B — Medium Value') . " ({$rec_b} " . _('items') . ")</strong>";
	echo "<ul style='margin:8px 0 0 0; padding-left:20px; font-size:13px;'>";
	echo "<li>" . _('Review stock levels monthly; maintain standard reorder points.') . "</li>";
	echo "<li>" . _('Cycle count quarterly.') . "</li>";
	echo "<li>" . _('Use basic safety stock formula (demand × lead time).') . "</li>";
	echo "<li>" . _('Consider bulk ordering to reduce transaction costs.') . "</li>";
	echo "</ul>";
	echo "</div>";

	// Class C recommendations
	echo "<div style='background:#f3fff3; border:1px solid " . get_abc_class_color('C') . "; border-radius:4px; padding:15px; min-width:280px; flex:1;'>";
	echo "<strong style='color:" . get_abc_class_color('C') . ";'><i class='fa fa-star-o' style='margin-right:4px;'></i>" . _('Class C — Low Value') . " ({$rec_c} " . _('items') . ")</strong>";
	echo "<ul style='margin:8px 0 0 0; padding-left:20px; font-size:13px;'>";
	echo "<li>" . _('Review stock levels quarterly or on exception only.') . "</li>";
	echo "<li>" . _('Annual cycle count is sufficient.') . "</li>";
	echo "<li>" . _('Consider reducing SKU variety; consolidate slow movers.') . "</li>";
	echo "<li>" . _('Use simple reorder systems (e.g. two-bin or min-max).') . "</li>";
	echo "<li>" . _('Flag zero-movement items for disposal or reclassification.') . "</li>";
	echo "</ul>";
	echo "</div>";

	echo "</div>";
}

//-------------------------------------------------------------------------------------
// ABC Classification Guide
//-------------------------------------------------------------------------------------

echo "<br>";
echo "<div style='background:#f8f9fa; border:1px solid #dee2e6; border-radius:4px; padding:15px; margin:10px 0;'>";
echo "<h4 style='margin:0 0 10px 0;'><i class='fa fa-info-circle' style='margin-right:5px;'></i>" . _('ABC Classification Guide') . "</h4>";
echo "<div style='display:flex; gap:20px; flex-wrap:wrap;'>";

echo "<div style='flex:1; min-width:200px;'>";
echo "<p><strong style='color:" . get_abc_class_color('A') . ";'>" . _('Class A') . "</strong> — "
	. _('High-value items (typically ~20% of items, ~80% of value). These items require tight inventory control, accurate forecasting, frequent review, and secure storage.') . "</p>";
echo "</div>";

echo "<div style='flex:1; min-width:200px;'>";
echo "<p><strong style='color:" . get_abc_class_color('B') . ";'>" . _('Class B') . "</strong> — "
	. _('Medium-value items (typically ~30% of items, ~15% of value). These items need moderate controls and periodic review.') . "</p>";
echo "</div>";

echo "<div style='flex:1; min-width:200px;'>";
echo "<p><strong style='color:" . get_abc_class_color('C') . ";'>" . _('Class C') . "</strong> — "
	. _('Low-value items (typically ~50% of items, ~5% of value). These items need basic controls, can use simplified ordering.') . "</p>";
echo "</div>";

echo "</div>";
echo "</div>";

end_form();

end_page();
