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
 * View Pack Order / Package Detail — Print-friendly packing slip.
 *
 * Can be opened by:
 *   ?op_id=N   — View pack operation with all lines and packages
 *   ?package_id=N — View single package detail with contents
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

$page_security = 'SA_WAREHOUSE_PACKING';
$path_to_root = '../../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_packing_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_locations_db.inc');

$op_id = isset($_GET['op_id']) ? (int)$_GET['op_id'] : 0;
$package_id = isset($_GET['package_id']) ? (int)$_GET['package_id'] : 0;

if (!$op_id && !$package_id) {
	display_error(_('No pack operation or package ID specified.'));
	end_page(true);
	exit;
}

page(_($help_context = 'Pack Order'), true, false, '', '', true);

echo '<style>
	@media print {
		.noprint { display: none !important; }
		body { font-size: 12px; }
		table { page-break-inside: auto; }
		tr { page-break-inside: avoid; page-break-after: auto; }
	}
	.pack-header { margin-bottom: 16px; }
	.pack-header h2 { margin: 0 0 8px 0; }
	.pack-header table { border-collapse: collapse; width: 100%; }
	.pack-header table td { padding: 3px 12px 3px 0; }
	.pack-header table td.lbl { font-weight: bold; color: #555; width: 120px; }
	.pack-table { width: 100%; border-collapse: collapse; font-size: 12px; margin-top: 8px; }
	.pack-table th { background: #333; color: #fff; padding: 6px 8px; text-align: left; font-size: 11px; }
	.pack-table td { padding: 4px 8px; border-bottom: 1px solid #ddd; }
	.pack-table tr:nth-child(even) { background: #f9f9f9; }
	.pack-table .total-row { font-weight: bold; background: #e0e0e0; }
	.pack-summary { display: flex; gap: 16px; margin: 10px 0; flex-wrap: wrap; }
	.pack-summary .card { padding: 8px 16px; border: 2px solid; border-radius: 6px; text-align: center; min-width: 80px; }
	.signature-area { margin-top: 30px; }
	.signature-area table { border-collapse: collapse; width: 60%; }
	.signature-area td { padding: 8px 0; border-bottom: 1px solid #999; }
	.signature-area .sig-label { font-weight: bold; width: 120px; }
	.pkg-section { margin: 16px 0; padding: 12px; border: 1px solid #ccc; border-radius: 6px; background: #fafafa; }
	.pkg-section h3 { margin: 0 0 8px 0; font-size: 14px; }
</style>';

// =====================================================================
// Print / Close buttons
// =====================================================================

echo '<div class="noprint" style="margin:0 0 12px 0;">';
echo '<button onclick="window.print();" style="padding:6px 16px;cursor:pointer;">'
	. '<i class="fa fa-print"></i> ' . _('Print') . '</button> ';
echo '<button onclick="window.close();" style="padding:6px 16px;cursor:pointer;">'
	. '<i class="fa fa-times"></i> ' . _('Close') . '</button>';
echo '</div>';

// =====================================================================
// Pack Operation View
// =====================================================================

if ($op_id > 0) {
	$pack_op = get_wh_operation($op_id);

	if (!$pack_op || $pack_op['op_type'] !== 'pack') {
		display_error(_('Pack operation not found.'));
		end_page(true);
		exit;
	}

	$progress = get_packing_progress($op_id);

	// Header
	echo '<div class="pack-header">';
	echo '<h2>' . sprintf(_('Pack Order #%d'), $op_id) . '</h2>';
	echo '<table>';
	echo '<tr><td class="lbl">' . _('Status:') . '</td><td>';
	$op_statuses = get_wh_operation_statuses();
	echo isset($op_statuses[$pack_op['op_status']]) ? $op_statuses[$pack_op['op_status']] : $pack_op['op_status'];
	echo '</td>';
	echo '<td class="lbl">' . _('Priority:') . '</td><td>' . (int)$pack_op['priority'] . '</td></tr>';

	if ($pack_op['source_doc_no']) {
		echo '<tr><td class="lbl">' . _('Source Doc:') . '</td><td colspan="3">';
		echo _('Type') . ': ' . (int)$pack_op['source_doc_type'] . ' #' . (int)$pack_op['source_doc_no'];
		echo '</td></tr>';
	}

	echo '<tr><td class="lbl">' . _('Created:') . '</td><td>' . sql2date(substr($pack_op['created_at'], 0, 10)) . '</td>';
	if ($pack_op['completed_at']) {
		echo '<td class="lbl">' . _('Completed:') . '</td><td>' . sql2date(substr($pack_op['completed_at'], 0, 10)) . '</td>';
	} else {
		echo '<td colspan="2"></td>';
	}
	echo '</tr>';

	if (!empty($pack_op['memo'])) {
		echo '<tr><td class="lbl">' . _('Notes:') . '</td><td colspan="3">' . htmlspecialchars($pack_op['memo']) . '</td></tr>';
	}
	echo '</table></div>';

	// Progress summary
	echo '<div class="pack-summary">';
	echo '<div class="card" style="border-color:#007bff;">'
		. '<div style="font-size:18px;font-weight:bold;color:#007bff;">' . $progress['total_lines'] . '</div>'
		. '<div style="font-size:11px;">' . _('Total Lines') . '</div></div>';
	echo '<div class="card" style="border-color:#28a745;">'
		. '<div style="font-size:18px;font-weight:bold;color:#28a745;">' . $progress['packed_lines'] . '</div>'
		. '<div style="font-size:11px;">' . _('Packed') . '</div></div>';
	$remaining = $progress['total_lines'] - $progress['packed_lines'];
	echo '<div class="card" style="border-color:' . ($remaining > 0 ? '#ffc107' : '#28a745') . ';">'
		. '<div style="font-size:18px;font-weight:bold;color:' . ($remaining > 0 ? '#ffc107' : '#28a745') . ';">' . $remaining . '</div>'
		. '<div style="font-size:11px;">' . _('Remaining') . '</div></div>';
	echo '<div class="card" style="border-color:#6c757d;">'
		. '<div style="font-size:18px;font-weight:bold;color:#6c757d;">' . $progress['pct'] . '%</div>'
		. '<div style="font-size:11px;">' . _('Complete') . '</div></div>';
	echo '</div>';

	// Pack lines table
	$lines = get_wh_operation_lines($op_id);
	echo '<table class="pack-table">';
	echo '<tr>';
	echo '<th class="checkbox-col" style="width:30px;text-align:center;">&#x2610;</th>';
	echo '<th>' . _('#') . '</th>';
	echo '<th>' . _('Item Code') . '</th>';
	echo '<th>' . _('Description') . '</th>';
	echo '<th>' . _('From Bin') . '</th>';
	echo '<th>' . _('Batch/Serial') . '</th>';
	echo '<th style="text-align:right;">' . _('Planned') . '</th>';
	echo '<th style="text-align:right;">' . _('Packed') . '</th>';
	echo '<th>' . _('Package') . '</th>';
	echo '</tr>';

	$line_num = 0;
	$total_planned = 0;
	$total_packed = 0;
	while ($line = db_fetch($lines)) {
		$line_num++;
		$is_packed = (float)$line['qty_done'] > 0;
		$row_class = $is_packed ? 'picked' : '';

		echo '<tr class="' . $row_class . '">';
		echo '<td class="checkbox-col">' . ($is_packed ? '&#x2611;' : '&#x2610;') . '</td>';
		echo '<td>' . $line_num . '</td>';
		echo '<td>' . htmlspecialchars($line['stock_id']) . '</td>';
		echo '<td>' . htmlspecialchars($line['item_description']) . '</td>';
		echo '<td>' . ($line['from_bin_code'] ? htmlspecialchars($line['from_bin_code']) : '-') . '</td>';

		$tracking = array();
		if (!empty($line['batch_no']))
			$tracking[] = _('Batch:') . ' ' . htmlspecialchars($line['batch_no']);
		if (!empty($line['serial_no']))
			$tracking[] = _('S/N:') . ' ' . htmlspecialchars($line['serial_no']);
		echo '<td>' . (!empty($tracking) ? implode(', ', $tracking) : '-') . '</td>';

		$dec = 2;
		echo '<td style="text-align:right;">' . number_format2((float)$line['qty_planned'], $dec) . '</td>';
		echo '<td style="text-align:right;">' . number_format2((float)$line['qty_done'], $dec) . '</td>';

		if ($line['package_id']) {
			$pkg = get_package((int)$line['package_id']);
			echo '<td>' . ($pkg ? htmlspecialchars($pkg['package_code']) : '#' . $line['package_id']) . '</td>';
		} else {
			echo '<td>-</td>';
		}
		echo '</tr>';

		$total_planned += (float)$line['qty_planned'];
		$total_packed += (float)$line['qty_done'];
	}

	// Total row
	echo '<tr class="total-row">';
	echo '<td colspan="6" style="text-align:right;">' . _('Total:') . '</td>';
	echo '<td style="text-align:right;">' . number_format2($total_planned, 2) . '</td>';
	echo '<td style="text-align:right;">' . number_format2($total_packed, 2) . '</td>';
	echo '<td></td></tr>';
	echo '</table>';

	// Signature area
	echo '<div class="signature-area">';
	echo '<table>';
	echo '<tr><td class="sig-label">' . _('Packed by:') . '</td><td>&nbsp;</td></tr>';
	echo '<tr><td class="sig-label">' . _('Verified by:') . '</td><td>&nbsp;</td></tr>';
	echo '<tr><td class="sig-label">' . _('Date:') . '</td><td>&nbsp;</td></tr>';
	echo '</table></div>';
}

// =====================================================================
// Single Package Detail View
// =====================================================================

if ($package_id > 0) {
	$pkg = get_package($package_id);

	if (!$pkg) {
		display_error(_('Package not found.'));
		end_page(true);
		exit;
	}

	echo '<div class="pack-header">';
	echo '<h2>' . sprintf(_('Package: %s'), htmlspecialchars($pkg['package_code'])) . '</h2>';
	echo '<table>';
	echo '<tr><td class="lbl">' . _('Type:') . '</td><td>';
	$types = get_package_types();
	echo isset($types[$pkg['package_type']]) ? $types[$pkg['package_type']] : $pkg['package_type'];
	echo '</td>';
	echo '<td class="lbl">' . _('Status:') . '</td><td>';
	$statuses = get_package_statuses();
	echo isset($statuses[$pkg['status']]) ? $statuses[$pkg['status']] : $pkg['status'];
	echo '</td></tr>';

	echo '<tr><td class="lbl">' . _('Weight:') . '</td><td>' . ($pkg['weight'] ? number_format2((float)$pkg['weight'], 2) . ' kg' : '-') . '</td>';
	echo '<td class="lbl">' . _('Location:') . '</td><td>' . ($pkg['location_name'] ? htmlspecialchars($pkg['location_name']) : '-') . '</td></tr>';

	$dims = array();
	if ($pkg['length']) $dims[] = number_format2((float)$pkg['length'], 1);
	if ($pkg['width']) $dims[] = number_format2((float)$pkg['width'], 1);
	if ($pkg['height']) $dims[] = number_format2((float)$pkg['height'], 1);
	echo '<tr><td class="lbl">' . _('Dimensions:') . '</td><td>' . (!empty($dims) ? implode(' x ', $dims) . ' cm' : '-') . '</td>';
	echo '<td class="lbl">' . _('Created:') . '</td><td>' . sql2date(substr($pkg['created_at'], 0, 10)) . '</td></tr>';

	if (!empty($pkg['carrier'])) {
		echo '<tr><td class="lbl">' . _('Carrier:') . '</td><td>' . htmlspecialchars($pkg['carrier']) . '</td>';
		echo '<td class="lbl">' . _('Tracking:') . '</td><td>' . htmlspecialchars($pkg['tracking_number'] ?: '-') . '</td></tr>';
	}

	echo '</table></div>';

	// Package contents
	$contents = get_package_contents($package_id);
	echo '<h3>' . _('Package Contents') . '</h3>';
	echo '<table class="pack-table">';
	echo '<tr>';
	echo '<th>' . _('#') . '</th>';
	echo '<th>' . _('Item Code') . '</th>';
	echo '<th>' . _('Description') . '</th>';
	echo '<th style="text-align:right;">' . _('Qty') . '</th>';
	echo '<th>' . _('Unit') . '</th>';
	echo '<th>' . _('Batch') . '</th>';
	echo '<th>' . _('Serial') . '</th>';
	echo '</tr>';

	$content_num = 0;
	$total_qty = 0;
	while ($c = db_fetch($contents)) {
		$content_num++;
		echo '<tr>';
		echo '<td>' . $content_num . '</td>';
		echo '<td>' . htmlspecialchars($c['stock_id']) . '</td>';
		echo '<td>' . htmlspecialchars($c['item_description']) . '</td>';
		echo '<td style="text-align:right;">' . number_format2((float)$c['qty'], 2) . '</td>';
		echo '<td>' . htmlspecialchars($c['units'] ?: '') . '</td>';
		echo '<td>' . ($c['batch_no'] ? htmlspecialchars($c['batch_no']) : '-') . '</td>';
		echo '<td>' . ($c['serial_no'] ? htmlspecialchars($c['serial_no']) : '-') . '</td>';
		echo '</tr>';
		$total_qty += (float)$c['qty'];
	}

	if ($content_num === 0) {
		echo '<tr><td colspan="7" style="text-align:center;padding:15px;color:#999;">' . _('Package is empty.') . '</td></tr>';
	} else {
		echo '<tr class="total-row">';
		echo '<td colspan="3" style="text-align:right;">' . _('Total:') . '</td>';
		echo '<td style="text-align:right;">' . number_format2($total_qty, 2) . '</td>';
		echo '<td colspan="3"></td></tr>';
	}
	echo '</table>';

	// Barcode area for label
	echo '<div style="margin-top:20px;text-align:center;">';
	echo '<div style="font-size:24px;font-family:monospace;letter-spacing:3px;">'
		. htmlspecialchars($pkg['package_code']) . '</div>';
	if (!empty($pkg['tracking_number'])) {
		echo '<div style="font-size:14px;margin-top:4px;">' . _('Tracking:') . ' '
			. htmlspecialchars($pkg['tracking_number']) . '</div>';
	}
	echo '</div>';

	// Shipping label area
	echo '<div class="signature-area">';
	echo '<table>';
	echo '<tr><td class="sig-label">' . _('Packed by:') . '</td><td>&nbsp;</td></tr>';
	echo '<tr><td class="sig-label">' . _('Date:') . '</td><td>&nbsp;</td></tr>';
	echo '</table></div>';
}

end_page(true);
