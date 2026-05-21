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
$page_security = 'SA_ITEMSTRANSVIEW';
$path_to_root = '../..';

include($path_to_root.'/includes/session.inc');

page(_($help_context = 'View Inventory Adjustment'), true);

include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/inventory/includes/inventory_db.inc');
include_once($path_to_root.'/inventory/includes/db/serial_batch_db.inc');

/**
 * Renders tracked serial and batch identifiers for an adjustment line.
 *
 * @param int    $trans_no Transfer transaction number.
 * @param string $stock_id Inventory item code.
 * @param int    $colspan  Table column span.
 * @return void
 */
function display_adjustment_line_tracking($trans_no, $stock_id, $colspan)
{
	$tracking_mode = get_item_tracking_mode($stock_id);
	if ($tracking_mode === 'none')
		return;

	if (item_has_serial_tracking($stock_id)) {
		$serial_sql = "SELECT DISTINCT sn.id, sn.serial_no, sm.to_status"
			. " FROM " . TB_PREF . "serial_movements sm"
			. " INNER JOIN " . TB_PREF . "serial_numbers sn ON sn.id = sm.serial_id"
			. " WHERE sm.trans_type = " . ST_INVADJUST
			. " AND sm.trans_no = " . (int)$trans_no
			. " AND sn.stock_id = " . db_escape($stock_id)
			. " ORDER BY sn.serial_no";
		$serial_result = db_query($serial_sql, 'could not get adjustment serials');
		$serial_parts = array();
		while ($serial = db_fetch($serial_result)) {
			$serial_link = viewer_link(
				htmlspecialchars($serial['serial_no']),
				'inventory/inquiry/serial_lifecycle.php?serial_id=' . (int)$serial['id']
			);
			if (!empty($serial['to_status']))
				$serial_link .= ' <span style="color:#888;">(' . htmlspecialchars($serial['to_status']) . ')</span>';
			$serial_parts[] = $serial_link;
		}
		if (!empty($serial_parts)) {
			echo '<tr><td colspan="' . (int)$colspan . '" style="padding:2px 8px 4px 24px; border-left:3px solid #5b9bd5; background:#f7f9fc; font-size:11px;">';
			echo '<b style="color:#5b9bd5;">' . _('Serials:') . '</b> ' . implode(', ', $serial_parts);
			echo '</td></tr>';
		}
	}

	if (item_has_batch_tracking($stock_id)) {
		$batch_sql = "SELECT sb.id, sb.batch_no, ABS(SUM(bm.quantity)) AS qty, sb.expiry_date"
			. " FROM " . TB_PREF . "batch_movements bm"
			. " INNER JOIN " . TB_PREF . "stock_batches sb ON sb.id = bm.batch_id"
			. " WHERE bm.trans_type = " . ST_INVADJUST
			. " AND bm.trans_no = " . (int)$trans_no
			. " AND sb.stock_id = " . db_escape($stock_id)
			. " GROUP BY sb.id, sb.batch_no, sb.expiry_date"
			. " ORDER BY sb.batch_no";
		$batch_result = db_query($batch_sql, 'could not get adjustment batches');
		$batch_parts = array();
		while ($batch = db_fetch($batch_result)) {
			$batch_link = viewer_link(
				htmlspecialchars($batch['batch_no']),
				'inventory/inquiry/batch_lifecycle.php?batch_id=' . (int)$batch['id']
			) . ' ×' . number_format2((float)$batch['qty'], get_qty_dec($stock_id));
			if (!empty($batch['expiry_date']))
				$batch_link .= ' <span style="color:#888;">(' . _('Exp') . ': ' . sql2date($batch['expiry_date']) . ')</span>';
			$batch_parts[] = $batch_link;
		}
		if (!empty($batch_parts)) {
			echo '<tr><td colspan="' . (int)$colspan . '" style="padding:2px 8px 4px 24px; border-left:3px solid #e6a23c; background:#fdf6ec; font-size:11px;">';
			echo '<b style="color:#e6a23c;">' . _('Batches:') . '</b> ' . implode(', ', $batch_parts);
			echo '</td></tr>';
		}
	}
}

if (isset($_GET['trans_no']))
	$trans_no = $_GET['trans_no'];

display_heading($systypes_array[ST_INVADJUST] . " #$trans_no");

br(1);
$adjustment_items = get_stock_adjustment_items($trans_no);
$k = 0;
$header_shown = false;
while ($adjustment = db_fetch($adjustment_items)) {

	if (!$header_shown) {

		start_table(TABLESTYLE2, "width='90%'");
		start_row();
		label_cells(_('At Location'), $adjustment['location_name'], "class='tableheader2'");
		label_cells(_('Reference'), $adjustment['reference'], "class='tableheader2'", "colspan=6");
		label_cells(_('Date'), sql2date($adjustment['tran_date']), "class='tableheader2'");
		end_row();
		comments_display_row(ST_INVADJUST, $trans_no);

		end_table();
		$header_shown = true;

		echo '<br>';
		start_table(TABLESTYLE, "width='90%'");

		$th = array(_('Item Code'), _('Description'), _('Quantity'), _('Units'), _('Unit Cost'));
		table_header($th);
	}

	alt_table_row_color($k);

	label_cell($adjustment['stock_id']);
	label_cell($adjustment['description']);
	qty_cell($adjustment['qty'], false, get_qty_dec($adjustment['stock_id']));
	label_cell($adjustment['units']);
	amount_decimal_cell($adjustment['standard_cost']);
	end_row();

	display_adjustment_line_tracking($trans_no, $adjustment['stock_id'], 5);
}

end_table(1);

is_voided_display(ST_INVADJUST, $trans_no, _('This adjustment has been voided.'));

end_page(true, false, false, ST_INVADJUST, $trans_no);
