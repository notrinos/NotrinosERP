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
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');

page(_($help_context = 'Vendor Scorecard'));

/**
 * Render one scorecard card.
 *
 * @param string $label
 * @param string $value
 * @param string $subtext
 * @return string
 */
function render_vendor_scorecard_card($label, $value, $subtext = '')
{
	$html = '<div style="min-width:180px;padding:12px 14px;background:#f8fafc;border:1px solid #dbe4ee;border-radius:4px;">';
	$html .= '<div style="font-size:12px;color:#64748b;">' . $label . '</div>';
	$html .= '<div style="font-size:21px;font-weight:bold;color:#0f172a;">' . $value . '</div>';
	if ($subtext !== '')
		$html .= '<div style="font-size:11px;color:#64748b;margin-top:4px;">' . $subtext . '</div>';
	$html .= '</div>';

	return $html;
}

$selected_supplier_id = get_post('supplier_id', isset($_GET['supplier_id']) ? (int)$_GET['supplier_id'] : 0);
if (!$selected_supplier_id && get_global_supplier() != ALL_TEXT)
	$selected_supplier_id = get_global_supplier();

$scorecard = $selected_supplier_id ? get_vendor_scorecard($selected_supplier_id) : array();
$supplier = !empty($scorecard['supplier']) ? $scorecard['supplier'] : false;

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();
supplier_list_cells(_('Supplier:'), 'supplier_id', $selected_supplier_id, true, true);
submit_cells('RefreshScorecard', _('Apply Filter'), '', _('Refresh vendor scorecard'), 'default');
echo '<td>';
if ($selected_supplier_id > 0)
		hyperlink_params($path_to_root . '/purchasing/manage/vendor_evaluation.php', _('New Vendor Evaluation'), 'sel_app=AP&supplier_id=' . (int)$selected_supplier_id . '&New=1');
echo '</td>';
end_row();
end_table(1);

if ($supplier) {
	echo '<div style="display:flex;justify-content:space-between;align-items:center;gap:12px;flex-wrap:wrap;margin-bottom:12px;">';
	echo '<div><h2 style="margin:0;">' . htmlspecialchars($supplier['supp_name']) . '</h2>';
	echo '<div style="margin-top:6px;">' . vendor_tier_badge($supplier['vendor_tier']) . '</div></div>';
	echo '<div style="font-size:12px;color:#475569;">';
	echo '<strong>' . _('Vendor Category:') . '</strong> ' . (!empty($supplier['vendor_category']) ? htmlspecialchars($supplier['vendor_category']) : '-') . '<br>';
	echo '<strong>' . _('Last Evaluation:') . '</strong> ' . (!empty($supplier['last_evaluation_date']) ? sql2date($supplier['last_evaluation_date']) : '-') . '<br>';
	echo '<strong>' . _('Next Evaluation:') . '</strong> ' . (!empty($supplier['next_evaluation_date']) ? sql2date($supplier['next_evaluation_date']) : '-') . '</div>';
	echo '</div>';

	echo '<div style="display:flex;gap:10px;flex-wrap:wrap;margin-bottom:16px;">';
	echo render_vendor_scorecard_card(_('Overall Score'), number_format2($supplier['overall_score'], 2));
	echo render_vendor_scorecard_card(_('Quality Score'), number_format2($supplier['quality_score'], 2), _('Defect Rate: ') . number_format2($supplier['defect_rate_pct'], 2) . '%');
	echo render_vendor_scorecard_card(_('Delivery Score'), number_format2($supplier['delivery_score'], 2), _('On-Time: ') . number_format2($supplier['on_time_delivery_pct'], 2) . '%');
	echo render_vendor_scorecard_card(_('Price Score'), number_format2($supplier['price_score'], 2), _('Price Variance: ') . number_format2($scorecard['price_metrics']['average_variance_pct'], 2) . '%');
	echo render_vendor_scorecard_card(_('Service Score'), number_format2($supplier['service_score'], 2), _('Payment Reliability: ') . number_format2($scorecard['payment_reliability_score'], 2));
	echo render_vendor_scorecard_card(_('Average Lead Time'), number_format2($supplier['lead_time_average'], 2) . ' ' . _('days'), _('Deliveries: ') . (int)$scorecard['delivery_metrics']['delivery_count']);
	echo '</div>';

	start_table(TABLESTYLE2, "width='100%'");
	label_row(_('Certifications:'), !empty($supplier['certifications']) ? nl2br(htmlspecialchars($supplier['certifications'])) : '-');
	label_row(_('Approved Categories:'), !empty($supplier['approved_categories']) ? nl2br(htmlspecialchars($supplier['approved_categories'])) : '-');
	label_row(_('Inspection Pass Rate:'), number_format2($scorecard['quality_metrics']['inspection_pass_rate_pct'], 2) . '%');
	label_row(_('Comparable Price Items:'), (int)$scorecard['price_metrics']['comparable_item_count']);
	end_table(1);

	display_heading(_('Evaluation Trend'));
	start_table(TABLESTYLE, "width='100%'");
	table_header(array(_('Date'), _('Overall'), _('Quality'), _('Delivery'), _('Price'), _('Service')));
	$k = 0;
	foreach ($scorecard['trend'] as $trend_row) {
		alt_table_row_color($k);
		label_cell(sql2date($trend_row['evaluation_date']));
		label_cell(number_format2($trend_row['overall_score'], 2), 'align=right');
		label_cell(number_format2($trend_row['quality_score'], 2), 'align=right');
		label_cell(number_format2($trend_row['delivery_score'], 2), 'align=right');
		label_cell(number_format2($trend_row['price_score'], 2), 'align=right');
		label_cell(number_format2($trend_row['service_score'], 2), 'align=right');
		end_row();
		$k++;
	}
	if ($k == 0)
		label_row('', _('No approved vendor evaluations are available yet.'), 'colspan=6 align=center');
	end_table(1);

	display_heading(_('Recent Performance Events'));
	start_table(TABLESTYLE, "width='100%'");
	table_header(array(_('Date'), _('Event Type'), _('Details'), _('Impact'), _('Recorded By')));
	$k = 0;
	if (!empty($scorecard['performance_summary']['recent_events'])) {
		$event_labels = get_vendor_event_types();
		foreach ($scorecard['performance_summary']['recent_events'] as $event_row) {
			alt_table_row_color($k);
			label_cell(sql2date($event_row['event_date']));
			label_cell(isset($event_labels[$event_row['event_type']]) ? $event_labels[$event_row['event_type']] : $event_row['event_type']);
			label_cell($event_row['details'] ? htmlspecialchars($event_row['details']) : '-');
			label_cell(number_format2($event_row['impact_score'], 2), 'align=right');
			label_cell($event_row['recorded_by_name'] ? $event_row['recorded_by_name'] : '-');
			end_row();
			$k++;
		}
	}
	if ($k == 0)
		label_row('', _('No vendor performance events have been logged yet.'), 'colspan=5 align=center');
	end_table(1);
	br();
}

display_heading(_('Top Vendor Ranking'));
start_table(TABLESTYLE, "width='100%'");
table_header(array(_('Supplier'), _('Tier'), _('Overall'), _('Quality'), _('Delivery'), _('Price'), _('Service')));

$ranking = get_vendor_ranking(10, 'overall');
$k = 0;
while ($row = db_fetch($ranking)) {
	alt_table_row_color($k);
	label_cell('<a href="' . $_SERVER['PHP_SELF'] . '?sel_app=AP&supplier_id=' . (int)$row['supplier_id'] . '">' . htmlspecialchars($row['supp_name']) . '</a>');
	label_cell(vendor_tier_badge($row['vendor_tier']));
	label_cell(number_format2($row['overall_score'], 2), 'align=right');
	label_cell(number_format2($row['quality_score'], 2), 'align=right');
	label_cell(number_format2($row['delivery_score'], 2), 'align=right');
	label_cell(number_format2($row['price_score'], 2), 'align=right');
	label_cell(number_format2($row['service_score'], 2), 'align=right');
	end_row();
	$k++;
}
if ($k == 0)
	label_row('', _('No vendor ranking data is available yet.'), 'colspan=7 align=center');
end_table(1);

display_heading(_('Vendors Due for Evaluation'));
start_table(TABLESTYLE, "width='100%'");
table_header(array(_('Supplier'), _('Tier'), _('Next Evaluation'), _('Last Evaluation'), _('Overall Score')));

$due_rows = get_vendors_due_evaluation(30);
$k = 0;
while ($row = db_fetch($due_rows)) {
	alt_table_row_color($k);
	label_cell('<a href="' . $_SERVER['PHP_SELF'] . '?sel_app=AP&supplier_id=' . (int)$row['supplier_id'] . '">' . htmlspecialchars($row['supp_name']) . '</a>');
	label_cell(vendor_tier_badge($row['vendor_tier']));
	label_cell($row['next_evaluation_date'] ? sql2date($row['next_evaluation_date']) : '-');
	label_cell($row['last_evaluation_date'] ? sql2date($row['last_evaluation_date']) : '-');
	label_cell(number_format2($row['overall_score'], 2), 'align=right');
	end_row();
	$k++;
}
if ($k == 0)
	label_row('', _('No vendors are due for evaluation in the next 30 days.'), 'colspan=5 align=center');
end_table(1);

end_form();
end_page();