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
 * View Pick List — Print-friendly, path-optimized pick list.
 *
 * Opens in a new window/tab for printing. Shows picking wave header
 * and pick lines sorted by walking path (pick_sequence), grouped by zone.
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

$page_security = 'SA_WAREHOUSE_PICKING';
$path_to_root = '../../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_picking_db.inc');

$wave_id = isset($_GET['wave_id']) ? (int)$_GET['wave_id'] : 0;

if (!$wave_id) {
	display_error(_('No wave ID specified.'));
	end_page(true);
	exit;
}

$wave = get_picking_wave($wave_id);
if (!$wave) {
	display_error(_('Wave not found.'));
	end_page(true);
	exit;
}

$pick_lines = generate_pick_list($wave_id);
$progress = get_wave_progress($wave_id);

// Render print-friendly page
page(_($help_context = 'Pick List'), true, false, '', '', true);

echo '<style>
	@media print {
		.noprint { display: none !important; }
		body { font-size: 12px; }
		table { page-break-inside: auto; }
		tr { page-break-inside: avoid; page-break-after: auto; }
	}
	.pick-header { margin-bottom: 16px; }
	.pick-header h2 { margin: 0 0 8px 0; }
	.pick-header table { border-collapse: collapse; }
	.pick-header table td { padding: 2px 12px 2px 0; }
	.pick-header table td.label { font-weight: bold; color: #555; }
	.pick-table { width: 100%; border-collapse: collapse; font-size: 12px; }
	.pick-table th { background: #333; color: #fff; padding: 6px 8px; text-align: left; font-size: 11px; }
	.pick-table td { padding: 4px 8px; border-bottom: 1px solid #ddd; }
	.pick-table tr:nth-child(even) { background: #f9f9f9; }
	.pick-table .zone-header { background: #e3f2fd; font-weight: bold; font-size: 12px; }
	.pick-table .picked { background: #e8f5e9; }
	.pick-table .short-pick { background: #fff3e0; }
	.checkbox-col { width: 30px; text-align: center; }
	.qty-col { text-align: right; font-weight: bold; }
	.total-row { font-weight: bold; background: #e0e0e0; }
	.pick-summary { display: flex; gap: 24px; margin: 8px 0 16px 0; }
	.pick-summary .card { padding: 8px 16px; border: 1px solid #ddd; border-radius: 4px; }
</style>';

// =====================================================================
// Header
// =====================================================================

echo '<div class="pick-header">';
echo '<h2>' . sprintf(_('Pick List — Wave #%d'), $wave_id) . '</h2>';

echo '<table>';
echo '<tr><td class="label">' . _('Wave Name:') . '</td><td>' . htmlspecialchars($wave['wave_name']) . '</td>';
echo '<td class="label">' . _('Warehouse:') . '</td><td>' . htmlspecialchars($wave['warehouse_name'] ?: $wave['warehouse_loc_code']) . '</td></tr>';
echo '<tr><td class="label">' . _('Method:') . '</td><td>';
$methods = get_picking_methods();
echo isset($methods[$wave['picking_method']]) ? $methods[$wave['picking_method']] : $wave['picking_method'];
echo '</td>';
echo '<td class="label">' . _('Type:') . '</td><td>';
$types = get_wave_types();
echo isset($types[$wave['wave_type']]) ? $types[$wave['wave_type']] : $wave['wave_type'];
echo '</td></tr>';
echo '<tr><td class="label">' . _('Status:') . '</td><td>' . wave_status_badge($wave['status']) . '</td>';
echo '<td class="label">' . _('Released:') . '</td><td>' . ($wave['released_date'] ? sql2date(substr($wave['released_date'], 0, 10)) : '-') . '</td></tr>';
echo '<tr><td class="label">' . _('Orders:') . '</td><td>' . $wave['total_orders'] . '</td>';
echo '<td class="label">' . _('Lines:') . '</td><td>' . $wave['total_lines'] . '</td></tr>';

if (!empty($wave['assigned_to'])) {
	$user_sql = "SELECT real_name FROM " . TB_PREF . "users WHERE id=" . (int)$wave['assigned_to'];
	$user_result = db_query($user_sql, 'could not get user');
	$user = db_fetch($user_result);
	echo '<tr><td class="label">' . _('Assigned To:') . '</td><td colspan="3">' . ($user ? htmlspecialchars($user['real_name']) : '#' . $wave['assigned_to']) . '</td></tr>';
}

echo '</table>';
echo '</div>';

// =====================================================================
// Summary
// =====================================================================

$lines_progress = get_wave_lines_progress($wave_id);

echo '<div class="pick-summary">';
echo '<div class="card"><b>' . $progress['total'] . '</b> ' . _('Operations') . '</div>';
echo '<div class="card"><b>' . number_format2($lines_progress['total_planned'], 2) . '</b> ' . _('Total Qty') . '</div>';
echo '<div class="card"><b>' . number_format2($lines_progress['total_done'], 2) . '</b> ' . _('Picked') . '</div>';
echo '<div class="card"><b>' . $lines_progress['pct_lines_complete'] . '%</b> ' . _('Complete') . '</div>';
echo '</div>';

// =====================================================================
// Print / Close buttons
// =====================================================================

echo '<div class="noprint" style="margin:8px 0;">';
echo '<button onclick="window.print();" style="padding:6px 16px;background:#1976d2;color:#fff;border:none;border-radius:4px;cursor:pointer;font-size:13px;">'
	. '<i class="fa fa-print"></i> ' . _('Print') . '</button> ';
echo '<button onclick="window.close();" style="padding:6px 16px;background:#757575;color:#fff;border:none;border-radius:4px;cursor:pointer;font-size:13px;">'
	. _('Close') . '</button>';
echo '</div>';

// =====================================================================
// Pick Lines Table (sorted by walking path)
// =====================================================================

if (empty($pick_lines)) {
	display_heading2(_('No pick lines. Release the wave to generate pick operations.'));
} else {
	echo '<table class="pick-table">';
	echo '<thead><tr>';
	echo '<th class="checkbox-col">&#9744;</th>';
	echo '<th>#</th>';
	echo '<th>' . _('Bin Code') . '</th>';
	echo '<th>' . _('Bin Name') . '</th>';
	echo '<th>' . _('Seq') . '</th>';
	echo '<th>' . _('Item Code') . '</th>';
	echo '<th>' . _('Description') . '</th>';
	echo '<th>' . _('Unit') . '</th>';
	echo '<th style="text-align:right;">' . _('Qty to Pick') . '</th>';
	echo '<th style="text-align:right;">' . _('Qty Picked') . '</th>';
	echo '<th>' . _('Batch / Exp') . '</th>';
	echo '<th>' . _('Serial') . '</th>';
	echo '<th>' . _('Order') . '</th>';
	echo '</tr></thead>';
	echo '<tbody>';

	$current_zone = null;
	$seq = 1;
	$total_planned = 0;
	$total_done = 0;

	foreach ($pick_lines as $pl) {
		$zone = $pl['zone_type'] ?: _('Default');

		// Zone separator row
		if ($zone !== $current_zone) {
			$current_zone = $zone;
			echo '<tr class="zone-header"><td colspan="13" style="padding:6px 8px;">'
				. '<i class="fa fa-th-large"></i> ' . _('Zone:') . ' ' . htmlspecialchars($zone)
				. '</td></tr>';
		}

		$planned = (float)$pl['qty_planned'];
		$done = (float)$pl['qty_done'];
		$total_planned += $planned;
		$total_done += $done;

		$row_class = '';
		if ($done >= $planned) $row_class = ' class="picked"';
		elseif ($done > 0 && $done < $planned) $row_class = ' class="short-pick"';

		echo '<tr' . $row_class . '>';

		// Checkbox
		echo '<td class="checkbox-col">';
		if ($done >= $planned) {
			echo '&#9745;';
		} else {
			echo '&#9744;';
		}
		echo '</td>';

		// Sequence
		echo '<td>' . $seq++ . '</td>';

		// Bin code (bold)
		echo '<td><b>' . htmlspecialchars($pl['bin_code'] ?: '-') . '</b></td>';

		// Bin name
		echo '<td>' . htmlspecialchars($pl['bin_name'] ?: '-') . '</td>';

		// Pick sequence
		echo '<td>' . ($pl['pick_sequence'] !== null ? $pl['pick_sequence'] : '-') . '</td>';

		// Item code
		echo '<td>' . htmlspecialchars($pl['stock_id']) . '</td>';

		// Description
		echo '<td>' . htmlspecialchars($pl['item_description']) . '</td>';

		// Unit
		echo '<td>' . htmlspecialchars($pl['units'] ?: '') . '</td>';

		// Qty to pick
		echo '<td class="qty-col">' . number_format2($planned, 2) . '</td>';

		// Qty picked
		if ($done >= $planned) {
			echo '<td class="qty-col" style="color:#28a745;">' . number_format2($done, 2) . '</td>';
		} elseif ($done > 0) {
			echo '<td class="qty-col" style="color:#e65100;">' . number_format2($done, 2) . '</td>';
		} else {
			echo '<td class="qty-col">-</td>';
		}

		// Batch / Expiry
		$batch_info = '-';
		if (!empty($pl['batch_no'])) {
			$batch_info = $pl['batch_no'];
			if (!empty($pl['batch_expiry'])) {
				$batch_info .= ' (exp: ' . sql2date($pl['batch_expiry']) . ')';
			}
		}
		echo '<td>' . htmlspecialchars($batch_info) . '</td>';

		// Serial
		echo '<td>' . htmlspecialchars(!empty($pl['serial_no']) ? $pl['serial_no'] : '-') . '</td>';

		// Order reference
		echo '<td>' . (!empty($pl['delivery_no']) ? 'DN #' . $pl['delivery_no'] : '-') . '</td>';

		echo '</tr>';
	}

	// Total row
	echo '<tr class="total-row">';
	echo '<td></td><td></td><td colspan="6" style="text-align:right;">' . _('Total:') . '</td>';
	echo '<td class="qty-col">' . number_format2($total_planned, 2) . '</td>';
	echo '<td class="qty-col">' . number_format2($total_done, 2) . '</td>';
	echo '<td colspan="3"></td>';
	echo '</tr>';

	echo '</tbody></table>';
}

// =====================================================================
// Signature area for print
// =====================================================================

echo '<div style="margin-top:40px;border-top:1px solid #ccc;padding-top:16px;">';
echo '<table style="width:100%;">';
echo '<tr>';
echo '<td style="width:50%;"><p>' . _('Picked By:') . ' ____________________________</p><p>' . _('Date:') . ' ________________</p></td>';
echo '<td style="width:50%;"><p>' . _('Verified By:') . ' ____________________________</p><p>' . _('Date:') . ' ________________</p></td>';
echo '</tr>';
echo '</table>';
echo '</div>';

echo '<div style="margin-top:16px;font-size:10px;color:#999;text-align:center;">'
	. sprintf(_('Generated: %s | Wave #%d | %s'), date('Y-m-d H:i'), $wave_id, htmlspecialchars($wave['wave_name']))
	. '</div>';

end_page(true);
