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

$page_security = 'SA_REORDERRULES';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/ui/ui_lists.inc');
include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');

page(_($help_context = 'Reorder Status'));

/**
 * Classify how close stock is to a reorder threshold.
 *
 * @param float $current_stock
 * @param float $threshold
 * @return string
 */
function get_reorder_risk_level($current_stock, $threshold)
{
	if ($threshold <= 0)
		return 'ok';
	if ($current_stock <= $threshold)
		return 'below';
	if ($current_stock <= ($threshold * 1.2))
		return 'near';
	return 'ok';
}

$filter_location = get_post('filter_location', '');

$suggestions = evaluate_replenishment_rules($filter_location !== '' ? $filter_location : null, null);
$below_items = array();
$near_items = array();

foreach ($suggestions as $suggestion) {
	$threshold = 0;
	if ($suggestion['rule_type'] === 'min_max')
		$threshold = isset($suggestion['min_qty']) ? (float)$suggestion['min_qty'] : 0;
	elseif ($suggestion['rule_type'] === 'reorder_point')
		$threshold = isset($suggestion['reorder_qty']) ? (float)$suggestion['reorder_qty'] : 0;
	else
		$threshold = (float)$suggestion['suggested_qty'];

	$current_stock = isset($suggestion['current_qty']) ? (float)$suggestion['current_qty'] : 0;
	$risk_level = get_reorder_risk_level($current_stock, $threshold);

	$suggestion['risk_level'] = $risk_level;
	$suggestion['threshold'] = $threshold;
	if ($risk_level === 'below')
		$below_items[] = $suggestion;
	elseif ($risk_level === 'near')
		$near_items[] = $suggestion;
}

$no_rule_sql = "SELECT stock.stock_id, stock.description, stock.units
	FROM " . TB_PREF . "stock_master stock
	WHERE stock.inactive = 0
	AND stock.mb_flag <> 'D'
	AND stock.stock_id NOT IN (
		SELECT DISTINCT rule.stock_id
		FROM " . TB_PREF . "wh_replenishment_rules rule
		WHERE rule.stock_id IS NOT NULL AND rule.stock_id <> ''
	)
	ORDER BY stock.stock_id";
$no_rule_result = db_query($no_rule_sql, 'could not get items with no reorder rule');

start_form();

start_table(TABLESTYLE2, "width='100%'");
start_row();
	locations_list_cells(_('Warehouse:'), 'filter_location', $filter_location, true, true);
	submit_cells('SearchStatus', _('Refresh'), '', '', 'default');
end_row();
end_table(1);

display_heading(_('Items Below Reorder Point'));
start_table(TABLESTYLE, "width='100%'");
table_header(array(_('Rule'), _('Item'), _('Warehouse'), _('Current'), _('Suggested Qty'), _('Reason')));

$k = 0;
foreach ($below_items as $item) {
	alt_table_row_color($k);
	label_cell($item['rule_name']);
	label_cell($item['stock_id'] . ' - ' . $item['item_description']);
	label_cell($item['warehouse_name'] ? $item['warehouse_name'] : $item['warehouse_loc_code']);
	amount_cell((float)$item['current_qty']);
	amount_cell((float)$item['suggested_qty']);
	label_cell($item['reason']);
	end_row();
}
if ($k == 0)
	label_row('', _('No items are currently below reorder point.'), 'colspan=6 align=center');
end_table(2);

display_heading(_('Items Approaching Reorder Point'));
start_table(TABLESTYLE, "width='100%'");
table_header(array(_('Rule'), _('Item'), _('Warehouse'), _('Current'), _('Threshold'), _('Reason')));

$k = 0;
foreach ($near_items as $item) {
	alt_table_row_color($k);
	label_cell($item['rule_name']);
	label_cell($item['stock_id'] . ' - ' . $item['item_description']);
	label_cell($item['warehouse_name'] ? $item['warehouse_name'] : $item['warehouse_loc_code']);
	amount_cell((float)$item['current_qty']);
	amount_cell((float)$item['threshold']);
	label_cell($item['reason']);
	end_row();
}
if ($k == 0)
	label_row('', _('No items are currently approaching reorder point.'), 'colspan=6 align=center');
end_table(2);

display_heading(_('Items Without Reorder Rules'));
start_table(TABLESTYLE, "width='100%'");
table_header(array(_('Item'), _('Description'), _('UOM')));

$k = 0;
while ($row = db_fetch($no_rule_result)) {
	alt_table_row_color($k);
	label_cell($row['stock_id']);
	label_cell($row['description']);
	label_cell($row['units']);
	end_row();
}
if ($k == 0)
	label_row('', _('All active items are covered by a specific reorder rule.'), 'colspan=3 align=center');
end_table(1);

end_form();
end_page();
