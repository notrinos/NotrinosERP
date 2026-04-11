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
 * Warehouse Dashboard — Central WMS real-time visibility page.
 *
 * Features:
 * - Summary KPI cards (stock value, units, pending ops, alerts)
 * - Operations pipeline (To Receive → To Put Away → To Pick → To Pack → To Ship)
 * - Low stock alerts + expiring soon alerts
 * - Capacity heatmap by zone
 * - Drill-down: warehouse → zone → bin → items
 * - Projected stock forecast (PO, SO, WO)
 * - Recent activity feed
 * - Top stock items by value
 *
 * Session 21 of UNIFIED_INVENTORY_IMPLEMENTATION_PLAN.md
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

$page_security = 'SA_WAREHOUSE_DASHBOARD';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_dashboard_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_locations_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_operations_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/warehouse_ui.inc');
include_once($path_to_root . '/inventory/includes/db/stock_batches_db.inc');
include_once($path_to_root . '/inventory/includes/serial_batch_ui.inc');

page(_($help_context = 'Warehouse Dashboard'), false, false, '', '');

//======================================================================
// WAREHOUSE SELECTOR & REFRESH
//======================================================================
start_form();

if (list_updated('warehouse_filter'))
	$Ajax->activate('_page_body');

// Handle drill-down navigation
$drill_loc_id = get_post('drill_loc_id', 0);
if (isset($_POST['drill_back'])) {
	$drill_loc_id = 0;
	$Ajax->activate('_page_body');
}
if ($drill_loc_id > 0) {
	$Ajax->activate('_page_body');
}

start_table(TABLESTYLE_NOBORDER);
start_row();

// Warehouse selector
$wms_locations = get_wms_enabled_locations();
if ($wms_locations && db_num_rows($wms_locations) > 0) {
	warehouse_list_cells(_('Warehouse:'), 'warehouse_filter', null, true, true);
} else {
	label_cell(_('No WMS-enabled warehouses found. Enable WMS on Inventory Locations first.'));
}

submit_cells('RefreshDashboard', _('Refresh'), '', _('Refresh dashboard data'), true);
end_row();
end_table();

$warehouse = get_post('warehouse_filter', '');

//======================================================================
// DRILL-DOWN VIEW
//======================================================================
if ($drill_loc_id > 0) {
	display_drilldown_view($drill_loc_id, $warehouse);
	end_form();
	end_page();
	return;
}

//======================================================================
// MAIN DASHBOARD
//======================================================================

// Gather all data
$stock_summary = get_dashboard_stock_summary($warehouse);
$pipeline = get_dashboard_operations_pipeline($warehouse);
$alerts = get_dashboard_alert_counts($warehouse);
$forecast = get_dashboard_stock_forecast($warehouse, 30);
$total_alerts = $alerts['low_stock'] + $alerts['expiring_soon'] + $alerts['expired']
	+ $alerts['capacity_critical'] + $alerts['pending_inspections'] + $alerts['overdue_counts'];

//----------------------------------------------------------------------
// 1. SUMMARY KPI CARDS
//----------------------------------------------------------------------
echo "<h3 style='margin:15px 0 5px 0;'><i class='fa fa-tachometer' style='margin-right:6px;'></i>" . _('Overview') . "</h3>";
echo "<div style='display:flex; flex-wrap:wrap; gap:12px; margin:10px 0 20px 0;'>";

// Stock Value
display_kpi_card(
	price_format($stock_summary['total_value']),
	_('Total Stock Value'),
	'fa-money', '#28a745'
);

// Available Units
display_kpi_card(
	number_format2($stock_summary['total_units'], 0),
	_('Total Units'),
	'fa-cubes', '#007bff'
);

// Distinct Items
display_kpi_card(
	$stock_summary['distinct_items'],
	_('Distinct Items'),
	'fa-tags', '#6f42c1'
);

// Pending Operations
$op_color = $pipeline['total_pending'] > 0 ? '#ffc107' : '#28a745';
display_kpi_card(
	$pipeline['total_pending'],
	_('Pending Operations'),
	'fa-cogs', $op_color
);

// Active Alerts
$alert_color = $total_alerts > 0 ? '#dc3545' : '#28a745';
display_kpi_card(
	$total_alerts,
	_('Active Alerts'),
	'fa-exclamation-triangle', $alert_color
);

// Bins in Use
display_kpi_card(
	$stock_summary['total_bins_used'],
	_('Bins In Use'),
	'fa-cube', '#17a2b8'
);

echo "</div>";

//----------------------------------------------------------------------
// 2. OPERATIONS PIPELINE
//----------------------------------------------------------------------
echo "<h3 style='margin:15px 0 5px 0;'><i class='fa fa-arrow-right' style='margin-right:6px;'></i>" . _('Operations Pipeline') . "</h3>";
echo "<div style='display:flex; flex-wrap:wrap; align-items:center; gap:0; margin:10px 0 20px 0; background:#fff; border:1px solid #ddd; border-radius:6px; padding:15px; overflow-x:auto;'>";

$pipeline_stages = array(
	array('key' => 'to_receive',  'label' => _('To Receive'),  'icon' => 'fa-truck',         'color' => '#28a745', 'link' => 'receipt_operations.php'),
	array('key' => 'to_inspect',  'label' => _('To Inspect'),  'icon' => 'fa-check-circle',  'color' => '#17a2b8', 'link' => ''),
	array('key' => 'to_putaway',  'label' => _('To Put Away'), 'icon' => 'fa-arrow-down',    'color' => '#007bff', 'link' => 'receipt_operations.php'),
	array('key' => 'to_pick',     'label' => _('To Pick'),     'icon' => 'fa-hand-paper-o',  'color' => '#ffc107', 'link' => 'picking.php'),
	array('key' => 'to_pack',     'label' => _('To Pack'),     'icon' => 'fa-gift',          'color' => '#fd7e14', 'link' => 'packing.php'),
	array('key' => 'to_ship',     'label' => _('To Ship'),     'icon' => 'fa-paper-plane',   'color' => '#6f42c1', 'link' => 'shipping.php'),
);

$stage_count = count($pipeline_stages);
$stage_idx = 0;
foreach ($pipeline_stages as $stage) {
	$count = $pipeline[$stage['key']];
	$bg = $count > 0 ? $stage['color'] : '#e9ecef';
	$text_color = $count > 0 ? '#fff' : '#adb5bd'; // use #adb5bd (medium gray) for visibility at zero

	echo "<div style='flex:1; min-width:100px; text-align:center; padding:10px;'>";
	if ($stage['link'] && $count > 0) {
		echo "<a href='" . htmlspecialchars($stage['link']) . "' style='text-decoration:none; color:inherit;'>";
	}
	echo "<div style='width:50px; height:50px; border-radius:50%; background:{$bg}; color:{$text_color};"
		. " display:inline-flex; align-items:center; justify-content:center; font-size:18px; margin-bottom:5px;'>";
	echo "<i class='fa {$stage['icon']}'></i>";
	echo "</div>";
	echo "<div style='font-size:22px; font-weight:bold; color:" . ($count > 0 ? $stage['color'] : '#999') . ";'>" . $count . "</div>";
	echo "<div style='font-size:11px; color:#666;'>" . $stage['label'] . "</div>";
	if ($stage['link'] && $count > 0) {
		echo "</a>";
	}
	echo "</div>";

	$stage_idx++;
	if ($stage_idx < $stage_count) {
		echo "<div style='font-size:20px; color:#ccc; padding:0 5px;'><i class='fa fa-chevron-right'></i></div>";
	}
}

echo "</div>";

//----------------------------------------------------------------------
// 3. ALERTS PANEL
//----------------------------------------------------------------------
if ($total_alerts > 0) {
	echo "<h3 style='margin:15px 0 5px 0;'><i class='fa fa-exclamation-triangle' style='margin-right:6px; color:#dc3545;'></i>"
		. _('Alerts & Warnings') . "</h3>";
	echo "<div style='display:flex; flex-wrap:wrap; gap:12px; margin:10px 0 20px 0;'>";

	if ($alerts['low_stock'] > 0) {
		display_alert_card($alerts['low_stock'], _('Low Stock Items'), 'fa-arrow-down', '#dc3545',
			_('Items below reorder level'));
	}
	if ($alerts['expiring_soon'] > 0) {
		display_alert_card($alerts['expiring_soon'], _('Expiring Soon'), 'fa-clock-o', '#fd7e14',
			_('Batches expiring within 30 days'));
	}
	if ($alerts['expired'] > 0) {
		display_alert_card($alerts['expired'], _('Expired Active'), 'fa-times-circle', '#dc3545',
			_('Expired batches still active'));
	}
	if ($alerts['capacity_critical'] > 0) {
		display_alert_card($alerts['capacity_critical'], _('Capacity Critical'), 'fa-warning', '#ffc107',
			_('Bins above 85% capacity'));
	}
	if ($alerts['pending_inspections'] > 0) {
		display_alert_card($alerts['pending_inspections'], _('Pending QC'), 'fa-check-circle', '#17a2b8',
			_('Inspections awaiting completion'));
	}
	if ($alerts['overdue_counts'] > 0) {
		display_alert_card($alerts['overdue_counts'], _('Overdue Counts'), 'fa-calculator', '#6c757d',
			_('Cycle count plans past due date'));
	}

	echo "</div>";
}

//----------------------------------------------------------------------
// 4. TWO-COLUMN LAYOUT: Capacity + Forecast
//----------------------------------------------------------------------
echo "<div style='display:flex; flex-wrap:wrap; gap:20px; margin:10px 0 20px 0;'>";

// LEFT: Capacity Heatmap
echo "<div style='flex:1; min-width:350px;'>";
echo "<h3 style='margin:0 0 10px 0;'><i class='fa fa-th' style='margin-right:6px;'></i>" . _('Capacity Heatmap') . "</h3>";

if ($warehouse) {
	$heatmap = get_dashboard_capacity_heatmap($warehouse);
	if (!empty($heatmap)) {
		echo "<div style='display:flex; flex-wrap:wrap; gap:10px;'>";
		foreach ($heatmap as $zone) {
			$color = get_utilization_color($zone['status']);
			$border_color = $zone['overall_pct'] > 85 ? '#dc3545' : ($zone['overall_pct'] > 60 ? '#ffc107' : '#ddd');

			echo "<div style='background:#fff; border:1px solid {$border_color}; border-left:4px solid {$color};"
				. " border-radius:4px; padding:12px; min-width:180px; max-width:260px; flex:1; cursor:pointer;'"
				. " title='" . _('Click to drill down') . "'>";

			// Zone header
			echo "<div style='display:flex; justify-content:space-between; align-items:center; margin-bottom:6px;'>";
			$icon = get_location_type_icon($zone['type_code']);
			echo "<span><i class='{$icon}' style='margin-right:4px;'></i><strong>" . htmlspecialchars($zone['loc_name']) . "</strong></span>";
			echo utilization_badge($zone['overall_pct'], $zone['status']);
			echo "</div>";

			// Progress bar
			echo utilization_bar($zone['overall_pct'], $zone['status']);

			// Stats row
			echo "<div style='display:flex; justify-content:space-between; margin-top:6px; font-size:11px; color:#666;'>";
			echo "<span>" . sprintf(_('Bins: %d'), $zone['bin_count']) . "</span>";
			if ($zone['critical_bins'] > 0)
				echo "<span style='color:#dc3545;'>" . sprintf(_('Critical: %d'), $zone['critical_bins']) . "</span>";
			else
				echo "<span style='color:#28a745;'>" . sprintf(_('OK: %d'), $zone['ok_bins']) . "</span>";
			echo "</div>";

			// Drill-down button
			echo "<div style='text-align:center; margin-top:8px;'>";
			echo "<button type='submit' name='drill_loc_id' value='" . $zone['loc_id'] . "' class='ajaxsubmit'"
				. " style='border:none; background:none; color:#007bff; cursor:pointer; font-size:12px; text-decoration:underline;'>"
				. "<i class='fa fa-search-plus' style='margin-right:3px;'></i>" . _('Drill Down') . "</button>";
			echo "</div>";

			echo "</div>";
		}
		echo "</div>";
	} else {
		echo "<div style='padding:15px; color:#999; text-align:center; background:#fff; border:1px solid #ddd; border-radius:4px;'>";
		echo _('No zones found. Create zones under the warehouse first.');
		echo "</div>";
	}
} else {
	echo "<div style='padding:15px; color:#999; text-align:center; background:#fff; border:1px solid #ddd; border-radius:4px;'>";
	echo _('Select a warehouse to view capacity heatmap.');
	echo "</div>";
}
echo "</div>";

// RIGHT: Stock Forecast
echo "<div style='flex:1; min-width:350px;'>";
echo "<h3 style='margin:0 0 10px 0;'><i class='fa fa-line-chart' style='margin-right:6px;'></i>" . _('30-Day Stock Forecast') . "</h3>";

echo "<div style='background:#fff; border:1px solid #ddd; border-radius:6px; padding:15px;'>";

// Incoming
echo "<div style='display:flex; align-items:center; padding:10px 0; border-bottom:1px solid #eee;'>";
echo "<div style='width:40px; height:40px; border-radius:50%; background:#28a745; color:#fff;"
	. " display:flex; align-items:center; justify-content:center; font-size:16px; margin-right:12px;'>";
echo "<i class='fa fa-arrow-down'></i></div>";
echo "<div style='flex:1;'>";
echo "<div style='font-weight:bold;'>" . _('Incoming (PO)') . "</div>";
echo "<div style='font-size:12px; color:#666;'>"
	. sprintf(_('%s orders, %s units'), $forecast['incoming']['order_count'], number_format2($forecast['incoming']['total_qty'], 0))
	. "</div>";
echo "</div>";
echo "<div style='text-align:right;'>";
echo "<div style='font-size:18px; font-weight:bold; color:#28a745;'>" . price_format($forecast['incoming']['total_value']) . "</div>";
echo "</div></div>";

// Outgoing
echo "<div style='display:flex; align-items:center; padding:10px 0; border-bottom:1px solid #eee;'>";
echo "<div style='width:40px; height:40px; border-radius:50%; background:#dc3545; color:#fff;"
	. " display:flex; align-items:center; justify-content:center; font-size:16px; margin-right:12px;'>";
echo "<i class='fa fa-arrow-up'></i></div>";
echo "<div style='flex:1;'>";
echo "<div style='font-weight:bold;'>" . _('Outgoing (SO)') . "</div>";
echo "<div style='font-size:12px; color:#666;'>"
	. sprintf(_('%s orders, %s units'), $forecast['outgoing']['order_count'], number_format2($forecast['outgoing']['total_qty'], 0))
	. "</div>";
echo "</div>";
echo "<div style='text-align:right;'>";
echo "<div style='font-size:18px; font-weight:bold; color:#dc3545;'>" . price_format($forecast['outgoing']['total_value']) . "</div>";
echo "</div></div>";

// Production
echo "<div style='display:flex; align-items:center; padding:10px 0;'>";
echo "<div style='width:40px; height:40px; border-radius:50%; background:#007bff; color:#fff;"
	. " display:flex; align-items:center; justify-content:center; font-size:16px; margin-right:12px;'>";
echo "<i class='fa fa-industry'></i></div>";
echo "<div style='flex:1;'>";
echo "<div style='font-weight:bold;'>" . _('Production (WO)') . "</div>";
echo "<div style='font-size:12px; color:#666;'>"
	. sprintf(_('%s orders, %s units'), $forecast['production']['order_count'], number_format2($forecast['production']['total_qty'], 0))
	. "</div>";
echo "</div>";
echo "<div style='text-align:right;'>";
echo "<div style='font-size:18px; font-weight:bold; color:#007bff;'>"
	. number_format2($forecast['production']['total_qty'], 0) . " " . _('units') . "</div>";
echo "</div></div>";

// Net projection
$net_qty = $forecast['incoming']['total_qty'] + $forecast['production']['total_qty'] - $forecast['outgoing']['total_qty'];
$net_color = $net_qty >= 0 ? '#28a745' : '#dc3545';
$net_icon = $net_qty >= 0 ? 'fa-arrow-up' : 'fa-arrow-down';
echo "<div style='margin-top:10px; padding-top:10px; border-top:2px solid #333; text-align:center;'>";
echo "<span style='font-size:12px; color:#666;'>" . _('Net Projected Change:') . " </span>";
echo "<span style='font-size:18px; font-weight:bold; color:{$net_color};'>"
	. "<i class='fa {$net_icon}' style='margin-right:3px;'></i>"
	. ($net_qty >= 0 ? '+' : '') . number_format2($net_qty, 0) . " " . _('units')
	. "</span>";
echo "</div>";

echo "</div>";
echo "</div>";

echo "</div>"; // end two-column layout

//----------------------------------------------------------------------
// 5. TWO-COLUMN LAYOUT: Low Stock + Expiring
//----------------------------------------------------------------------
echo "<div style='display:flex; flex-wrap:wrap; gap:20px; margin:10px 0 20px 0;'>";

// LEFT: Low Stock Alerts
echo "<div style='flex:1; min-width:350px;'>";
echo "<h3 style='margin:0 0 10px 0;'><i class='fa fa-arrow-down' style='margin-right:6px; color:#dc3545;'></i>"
	. _('Low Stock Items') . "</h3>";

$low_stock = get_dashboard_low_stock_alerts($warehouse, 10);
if (db_num_rows($low_stock) > 0) {
	echo "<div style='background:#fff; border:1px solid #ddd; border-radius:6px; overflow:hidden;'>";
	echo "<table style='width:100%; border-collapse:collapse; font-size:12px;'>";
	echo "<tr style='background:#f8f9fa; font-weight:bold;'>";
	echo "<td style='padding:8px;'>" . _('Item') . "</td>";
	echo "<td style='padding:8px; text-align:right;'>" . _('On Hand') . "</td>";
	echo "<td style='padding:8px; text-align:right;'>" . _('Reorder') . "</td>";
	echo "<td style='padding:8px; text-align:right;'>" . _('Deficit') . "</td>";
	echo "</tr>";

	$i = 0;
	while ($row = db_fetch($low_stock)) {
		$deficit = $row['reorder_level'] - $row['qty_on_hand'];
		$bg = ($i++ % 2 == 0) ? '#fff' : '#f8f9fa';
		$pct = $row['reorder_level'] > 0 ? round(($row['qty_on_hand'] / $row['reorder_level']) * 100) : 0;
		$pct_color = $pct < 25 ? '#dc3545' : ($pct < 50 ? '#fd7e14' : '#ffc107');

		echo "<tr style='background:{$bg}; border-bottom:1px solid #eee;'>";
		echo "<td style='padding:8px;'>"
			. "<strong>" . htmlspecialchars($row['stock_id']) . "</strong><br>"
			. "<span style='font-size:11px; color:#666;'>" . htmlspecialchars($row['description']) . "</span>"
			. "</td>";
		echo "<td style='padding:8px; text-align:right;'>"
			. "<span style='color:{$pct_color}; font-weight:bold;'>" . number_format2($row['qty_on_hand'], 0) . "</span>"
			. " " . $row['units']
			. "</td>";
		echo "<td style='padding:8px; text-align:right;'>" . number_format2($row['reorder_level'], 0) . "</td>";
		echo "<td style='padding:8px; text-align:right;'>"
			. "<span style='color:#dc3545; font-weight:bold;'>-" . number_format2($deficit, 0) . "</span>"
			. "</td>";
		echo "</tr>";
	}
	echo "</table></div>";
} else {
	echo "<div style='padding:15px; color:#28a745; text-align:center; background:#fff; border:1px solid #ddd; border-radius:4px;'>";
	echo "<i class='fa fa-check-circle' style='margin-right:6px;'></i>" . _('All items above reorder levels.');
	echo "</div>";
}
echo "</div>";

// RIGHT: Expiring Soon
echo "<div style='flex:1; min-width:350px;'>";
echo "<h3 style='margin:0 0 10px 0;'><i class='fa fa-clock-o' style='margin-right:6px; color:#fd7e14;'></i>"
	. _('Expiring Soon (30 Days)') . "</h3>";

$expiring = get_expiring_batches(30, '', 10);
if (db_num_rows($expiring) > 0) {
	echo "<div style='background:#fff; border:1px solid #ddd; border-radius:6px; overflow:hidden;'>";
	echo "<table style='width:100%; border-collapse:collapse; font-size:12px;'>";
	echo "<tr style='background:#f8f9fa; font-weight:bold;'>";
	echo "<td style='padding:8px;'>" . _('Batch') . "</td>";
	echo "<td style='padding:8px;'>" . _('Item') . "</td>";
	echo "<td style='padding:8px; text-align:center;'>" . _('Expiry') . "</td>";
	echo "<td style='padding:8px; text-align:center;'>" . _('Days Left') . "</td>";
	echo "</tr>";

	$i = 0;
	while ($row = db_fetch($expiring)) {
		$bg = ($i++ % 2 == 0) ? '#fff' : '#f8f9fa';
		$days = (int)$row['days_until_expiry'];
		$day_color = $days <= 7 ? '#dc3545' : ($days <= 14 ? '#fd7e14' : '#ffc107');

		echo "<tr style='background:{$bg}; border-bottom:1px solid #eee;'>";
		echo "<td style='padding:8px;'><strong>" . htmlspecialchars($row['batch_no']) . "</strong></td>";
		echo "<td style='padding:8px;'>"
			. "<span style='font-size:11px;'>" . htmlspecialchars($row['item_description']) . "</span></td>";
		echo "<td style='padding:8px; text-align:center;'>" . sql2date($row['expiry_date']) . "</td>";
		echo "<td style='padding:8px; text-align:center;'>"
			. "<span style='display:inline-block; padding:2px 8px; border-radius:3px; background:{$day_color}; color:#fff; font-weight:bold; font-size:11px;'>"
			. $days . "d</span></td>";
		echo "</tr>";
	}
	echo "</table></div>";
} else {
	echo "<div style='padding:15px; color:#28a745; text-align:center; background:#fff; border:1px solid #ddd; border-radius:4px;'>";
	echo "<i class='fa fa-check-circle' style='margin-right:6px;'></i>" . _('No batches expiring within 30 days.');
	echo "</div>";
}
echo "</div>";

echo "</div>"; // end two-column

//----------------------------------------------------------------------
// 6. TWO-COLUMN LAYOUT: Top Items + Recent Activity
//----------------------------------------------------------------------
echo "<div style='display:flex; flex-wrap:wrap; gap:20px; margin:10px 0 20px 0;'>";

// LEFT: Top Stock Items
echo "<div style='flex:1; min-width:350px;'>";
echo "<h3 style='margin:0 0 10px 0;'><i class='fa fa-star' style='margin-right:6px; color:#ffc107;'></i>"
	. _('Top Stock by Value') . "</h3>";

$top_items = get_dashboard_top_stock_items($warehouse, 8);
if (db_num_rows($top_items) > 0) {
	echo "<div style='background:#fff; border:1px solid #ddd; border-radius:6px; overflow:hidden;'>";
	echo "<table style='width:100%; border-collapse:collapse; font-size:12px;'>";
	echo "<tr style='background:#f8f9fa; font-weight:bold;'>";
	echo "<td style='padding:8px;'>#</td>";
	echo "<td style='padding:8px;'>" . _('Item') . "</td>";
	echo "<td style='padding:8px; text-align:right;'>" . _('Qty') . "</td>";
	echo "<td style='padding:8px; text-align:right;'>" . _('Value') . "</td>";
	echo "<td style='padding:8px; text-align:right;'>" . _('Bins') . "</td>";
	echo "</tr>";

	$i = 0;
	$rank = 1;
	while ($row = db_fetch($top_items)) {
		$bg = ($i++ % 2 == 0) ? '#fff' : '#f8f9fa';
		echo "<tr style='background:{$bg}; border-bottom:1px solid #eee;'>";
		echo "<td style='padding:8px; font-weight:bold; color:#999;'>" . $rank++ . "</td>";
		echo "<td style='padding:8px;'>"
			. "<strong>" . htmlspecialchars($row['stock_id']) . "</strong><br>"
			. "<span style='font-size:11px; color:#666;'>" . htmlspecialchars($row['description']) . "</span></td>";
		echo "<td style='padding:8px; text-align:right;'>" . number_format2($row['total_qty'], 0) . " " . $row['units'] . "</td>";
		echo "<td style='padding:8px; text-align:right; font-weight:bold;'>" . price_format($row['total_value']) . "</td>";
		echo "<td style='padding:8px; text-align:right;'>" . $row['bin_count'] . "</td>";
		echo "</tr>";
	}
	echo "</table></div>";
} else {
	echo "<div style='padding:15px; color:#999; text-align:center; background:#fff; border:1px solid #ddd; border-radius:4px;'>";
	echo _('No bin-level stock data available.');
	echo "</div>";
}
echo "</div>";

// RIGHT: Recent Activity
echo "<div style='flex:1; min-width:350px;'>";
echo "<h3 style='margin:0 0 10px 0;'><i class='fa fa-history' style='margin-right:6px;'></i>"
	. _('Recent Activity') . "</h3>";

$recent = get_dashboard_recent_operations($warehouse, 10);
$op_types = get_wh_operation_types();
if (db_num_rows($recent) > 0) {
	echo "<div style='background:#fff; border:1px solid #ddd; border-radius:6px; overflow:hidden;'>";
	echo "<table style='width:100%; border-collapse:collapse; font-size:12px;'>";
	echo "<tr style='background:#f8f9fa; font-weight:bold;'>";
	echo "<td style='padding:8px;'>" . _('Operation') . "</td>";
	echo "<td style='padding:8px; text-align:right;'>" . _('Lines') . "</td>";
	echo "<td style='padding:8px; text-align:right;'>" . _('Qty') . "</td>";
	echo "<td style='padding:8px;'>" . _('Completed') . "</td>";
	echo "</tr>";

	$i = 0;
	while ($row = db_fetch($recent)) {
		$bg = ($i++ % 2 == 0) ? '#fff' : '#f8f9fa';
		$type_label = isset($op_types[$row['op_type']]) ? $op_types[$row['op_type']] : $row['op_type'];
		$type_color = get_wh_operation_type_color($row['op_type']);
		$completed = $row['completed_at'] ? sql2date(substr($row['completed_at'], 0, 10)) : '-';

		echo "<tr style='background:{$bg}; border-bottom:1px solid #eee;'>";
		echo "<td style='padding:8px;'>"
			. "<span style='display:inline-block; padding:1px 6px; border-radius:3px; background:{$type_color}; color:#fff; font-size:10px; font-weight:bold;'>"
			. $type_label . "</span>"
			. " <span style='color:#999;'>#" . $row['op_id'] . "</span>"
			. "</td>";
		echo "<td style='padding:8px; text-align:right;'>" . (int)$row['line_count'] . "</td>";
		echo "<td style='padding:8px; text-align:right;'>" . number_format2((float)$row['total_qty_done'], 0) . "</td>";
		echo "<td style='padding:8px;'>" . $completed . "</td>";
		echo "</tr>";
	}
	echo "</table></div>";
} else {
	echo "<div style='padding:15px; color:#999; text-align:center; background:#fff; border:1px solid #ddd; border-radius:4px;'>";
	echo _('No completed operations yet.');
	echo "</div>";
}
echo "</div>";

echo "</div>"; // end two-column

//----------------------------------------------------------------------
// 7. OPERATIONS SUMMARY (7-day)
//----------------------------------------------------------------------
$ops_by_type = get_dashboard_operations_by_type($warehouse, 7);
if (!empty($ops_by_type)) {
	echo "<h3 style='margin:15px 0 5px 0;'><i class='fa fa-bar-chart' style='margin-right:6px;'></i>"
		. _('Operations (Last 7 Days)') . "</h3>";
	echo "<div style='display:flex; flex-wrap:wrap; gap:10px; margin:10px 0 20px 0;'>";

	$max_ops = max($ops_by_type);
	foreach ($ops_by_type as $type => $count) {
		$type_label = isset($op_types[$type]) ? $op_types[$type] : $type;
		$type_color = get_wh_operation_type_color($type);
		$bar_width = $max_ops > 0 ? round(($count / $max_ops) * 100) : 0;

		echo "<div style='flex:1; min-width:150px; max-width:250px; background:#fff; border:1px solid #ddd; border-radius:4px; padding:10px;'>";
		echo "<div style='display:flex; justify-content:space-between; margin-bottom:5px;'>";
		echo "<span style='font-size:12px; font-weight:bold;'>" . $type_label . "</span>";
		echo "<span style='font-size:14px; font-weight:bold; color:{$type_color};'>" . $count . "</span>";
		echo "</div>";
		echo "<div style='background:#e9ecef; border-radius:3px; overflow:hidden; height:8px;'>";
		echo "<div style='background:{$type_color}; height:100%; width:{$bar_width}%; border-radius:3px;'></div>";
		echo "</div>";
		echo "</div>";
	}

	echo "</div>";
}

end_form();
end_page();

//======================================================================
// HELPER FUNCTIONS
//======================================================================

/**
 * Display a KPI summary card.
 *
 * @param string $value     The main value to display
 * @param string $label     Label text below the value
 * @param string $icon      Font Awesome icon class (without fa prefix)
 * @param string $color     CSS color
 */
function display_kpi_card($value, $label, $icon, $color) {
	echo "<div style='flex:1; min-width:140px; max-width:200px; padding:15px; border-radius:6px;"
		. " background:{$color}; color:#fff; text-align:center;'>";
	echo "<div style='font-size:14px; opacity:0.8; margin-bottom:5px;'>"
		. "<i class='fa {$icon}'></i></div>";
	echo "<div style='font-size:22px; font-weight:bold;'>" . $value . "</div>";
	echo "<div style='font-size:11px; opacity:0.9;'>" . $label . "</div>";
	echo "</div>";
}

/**
 * Display an alert card.
 *
 * @param int    $count       Alert count
 * @param string $label       Alert label
 * @param string $icon        Font Awesome icon
 * @param string $color       CSS color
 * @param string $description Description tooltip
 */
function display_alert_card($count, $label, $icon, $color, $description = '') {
	echo "<div style='flex:0 0 auto; min-width:160px; padding:12px 18px; border-radius:6px;"
		. " background:#fff; border:1px solid {$color}; border-left:4px solid {$color};'"
		. ($description ? " title='" . htmlspecialchars($description) . "'" : "") . ">";
	echo "<div style='display:flex; align-items:center; gap:10px;'>";
	echo "<i class='fa {$icon}' style='font-size:20px; color:{$color};'></i>";
	echo "<div>";
	echo "<div style='font-size:20px; font-weight:bold; color:{$color};'>" . $count . "</div>";
	echo "<div style='font-size:11px; color:#666;'>" . $label . "</div>";
	echo "</div></div></div>";
}

/**
 * Display the drill-down view for a specific warehouse location.
 *
 * @param int    $loc_id     Location ID to drill into
 * @param string $warehouse  Warehouse code for context
 */
function display_drilldown_view($loc_id, $warehouse) {
	// Get location info
	$location = get_warehouse_location($loc_id);
	if (!$location) {
		display_error(_('Location not found.'));
		return;
	}

	// Breadcrumb
	$breadcrumb = get_warehouse_location_breadcrumb($loc_id);
	echo "<div style='margin:10px 0; padding:8px 12px; background:#f8f9fa; border-radius:4px; font-size:13px;'>";
	echo "<button type='submit' name='drill_back' value='1' class='ajaxsubmit'"
		. " style='border:none; background:none; color:#007bff; cursor:pointer; margin-right:8px;'>"
		. "<i class='fa fa-arrow-left'></i> " . _('Back to Dashboard') . "</button>";
	echo " <i class='fa fa-chevron-right' style='color:#ccc; margin:0 5px;'></i> ";
	$crumb_parts = array();
	foreach ($breadcrumb as $crumb) {
		$icon = get_location_type_icon($crumb['type_code']);
		$crumb_parts[] = "<span><i class='{$icon}' style='margin-right:3px;'></i>"
			. htmlspecialchars($crumb['loc_name']) . "</span>";
	}
	echo implode(" <i class='fa fa-chevron-right' style='color:#ccc; margin:0 5px;'></i> ", $crumb_parts);
	echo "</div>";

	// Location header
	$icon = get_location_type_icon($location['type_code']);
	echo "<h3><i class='{$icon}' style='margin-right:6px;'></i>"
		. htmlspecialchars($location['loc_name'])
		. " <span style='color:#999; font-weight:normal;'>(" . htmlspecialchars($location['loc_code']) . ")</span></h3>";

	// Child locations
	$children = get_dashboard_location_children($loc_id);
	if (db_num_rows($children) > 0) {
		echo "<h4 style='margin:15px 0 5px 0;'>" . _('Sub-Locations') . "</h4>";
		echo "<div style='display:flex; flex-wrap:wrap; gap:10px; margin-bottom:20px;'>";

		while ($child = db_fetch($children)) {
			$child_icon = get_location_type_icon($child['type_code']);
			$weight_pct = ($child['max_weight'] > 0)
				? round(($child['current_weight'] / $child['max_weight']) * 100, 1) : 0;
			if ($weight_pct > 95) $child_status = 'full';
			elseif ($weight_pct > 85) $child_status = 'critical';
			elseif ($weight_pct > 60) $child_status = 'warning';
			else $child_status = 'ok';

			$border_color = get_utilization_color($child_status);

			echo "<div style='background:#fff; border:1px solid #ddd; border-left:4px solid {$border_color};"
				. " border-radius:4px; padding:12px; min-width:180px; max-width:280px; flex:1;'>";

			echo "<div style='display:flex; justify-content:space-between; align-items:center; margin-bottom:6px;'>";
			echo "<span><i class='{$child_icon}' style='margin-right:4px;'></i><strong>"
				. htmlspecialchars($child['loc_name']) . "</strong></span>";
			echo "<span style='font-size:11px; color:#999;'>" . htmlspecialchars($child['loc_code']) . "</span>";
			echo "</div>";

			if ($child['can_store'] && $child['max_weight'] > 0) {
				echo utilization_bar($weight_pct, $child_status, '', 16);
			}

			echo "<div style='display:flex; justify-content:space-between; margin-top:6px; font-size:11px; color:#666;'>";
			echo "<span>" . sprintf(_('Stock: %s'), number_format2($child['total_stock_qty'], 0)) . "</span>";
			if ($child['child_count'] > 0) {
				echo "<span>" . sprintf(_('Children: %d'), $child['child_count']) . "</span>";
			}
			echo "</div>";

			// Drill deeper button
			if ($child['child_count'] > 0 || $child['can_store']) {
				echo "<div style='text-align:center; margin-top:6px;'>";
				echo "<button type='submit' name='drill_loc_id' value='" . $child['loc_id'] . "' class='ajaxsubmit'"
					. " style='border:none; background:none; color:#007bff; cursor:pointer; font-size:11px;'>"
					. "<i class='fa fa-search-plus'></i> " . _('View') . "</button>";
				echo "</div>";
			}

			echo "</div>";
		}
		echo "</div>";
	}

	// Bin contents (stock at this location or direct children)
	$contents = get_dashboard_bin_contents($loc_id);
	if (db_num_rows($contents) > 0) {
		echo "<h4 style='margin:15px 0 5px 0;'>" . _('Stock in this Location') . "</h4>";
		echo "<div style='background:#fff; border:1px solid #ddd; border-radius:6px; overflow:hidden;'>";
		echo "<table style='width:100%; border-collapse:collapse; font-size:12px;'>";
		echo "<tr style='background:#f8f9fa; font-weight:bold;'>";
		echo "<td style='padding:8px;'>" . _('Bin') . "</td>";
		echo "<td style='padding:8px;'>" . _('Item') . "</td>";
		echo "<td style='padding:8px; text-align:right;'>" . _('On Hand') . "</td>";
		echo "<td style='padding:8px; text-align:right;'>" . _('Reserved') . "</td>";
		echo "<td style='padding:8px; text-align:right;'>" . _('Available') . "</td>";
		echo "<td style='padding:8px;'>" . _('Batch') . "</td>";
		echo "<td style='padding:8px;'>" . _('Serial') . "</td>";
		echo "<td style='padding:8px; text-align:right;'>" . _('Value') . "</td>";
		echo "</tr>";

		$i = 0;
		$total_value = 0;
		while ($row = db_fetch($contents)) {
			$bg = ($i++ % 2 == 0) ? '#fff' : '#f8f9fa';
			$total_value += (float)$row['line_value'];

			echo "<tr style='background:{$bg}; border-bottom:1px solid #eee;'>";
			echo "<td style='padding:8px;'>" . htmlspecialchars($row['bin_code']) . "</td>";
			echo "<td style='padding:8px;'>"
				. "<strong>" . htmlspecialchars($row['stock_id']) . "</strong>"
				. "<br><span style='font-size:11px; color:#666;'>" . htmlspecialchars($row['item_description']) . "</span></td>";
			echo "<td style='padding:8px; text-align:right; font-weight:bold;'>" . number_format2($row['qty_on_hand'], 0) . "</td>";
			echo "<td style='padding:8px; text-align:right;'>" . number_format2($row['qty_reserved'], 0) . "</td>";
			echo "<td style='padding:8px; text-align:right;'>" . number_format2($row['qty_available'], 0) . "</td>";
			echo "<td style='padding:8px;'>" . ($row['batch_no'] ? htmlspecialchars($row['batch_no']) : '-') . "</td>";
			echo "<td style='padding:8px;'>" . ($row['serial_no'] ? htmlspecialchars($row['serial_no']) : '-') . "</td>";
			echo "<td style='padding:8px; text-align:right;'>" . price_format($row['line_value']) . "</td>";
			echo "</tr>";
		}

		// Total row
		echo "<tr style='background:#e9ecef; font-weight:bold;'>";
		echo "<td colspan='7' style='padding:8px; text-align:right;'>" . _('Total Value:') . "</td>";
		echo "<td style='padding:8px; text-align:right;'>" . price_format($total_value) . "</td>";
		echo "</tr>";

		echo "</table></div>";
	} else {
		echo "<div style='padding:15px; color:#999; text-align:center; background:#fff; border:1px solid #ddd; border-radius:4px;'>";
		echo _('No stock found at this location.');
		echo "</div>";
	}
}
