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

page(_($help_context = 'View Inventory Transfer'), true);

include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/gl/includes/gl_db.inc');
include_once($path_to_root.'/inventory/includes/db/serial_batch_db.inc');

/**
 * Renders tracked serial and batch identifiers for a transfer line.
 *
 * @param int    $trans_no Transfer transaction number.
 * @param string $stock_id Inventory item code.
 * @param int    $colspan  Table column span.
 * @return void
 */
function display_transfer_line_tracking($trans_no, $stock_id, $colspan)
{
	$tracking_mode = get_item_tracking_mode($stock_id);
	if ($tracking_mode === 'none')
		return;

	if (item_has_serial_tracking($stock_id)) {
		$serial_sql = "SELECT DISTINCT sn.id, sn.serial_no, sm.to_status"
			. " FROM " . TB_PREF . "serial_movements sm"
			. " INNER JOIN " . TB_PREF . "serial_numbers sn ON sn.id = sm.serial_id"
			. " WHERE sm.trans_type = " . ST_LOCTRANSFER
			. " AND sm.trans_no = " . (int)$trans_no
			. " AND sn.stock_id = " . db_escape($stock_id)
			. " ORDER BY sn.serial_no";
		$serial_result = db_query($serial_sql, 'could not get transfer serials');
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
		$batch_sql = "SELECT DISTINCT sb.id, sb.batch_no, ABS(bm.quantity) AS qty, sb.expiry_date"
			. " FROM " . TB_PREF . "batch_movements bm"
			. " INNER JOIN " . TB_PREF . "stock_batches sb ON sb.id = bm.batch_id"
			. " WHERE bm.trans_type = " . ST_LOCTRANSFER
			. " AND bm.trans_no = " . (int)$trans_no
			. " AND sb.stock_id = " . db_escape($stock_id)
			. " AND bm.quantity > 0"
			. " ORDER BY sb.batch_no";
		$batch_result = db_query($batch_sql, 'could not get transfer batches');
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

$trans = get_stock_transfer($trans_no);

display_heading($systypes_array[ST_LOCTRANSFER]." #$trans_no");

echo '<br>';
start_table(TABLESTYLE2, "width='90%'");

start_row();
label_cells(_('Reference'), $trans['reference'], "class='tableheader2'");
label_cells(_('Date'), sql2date($trans['tran_date']), "class='tableheader2'");
end_row();
start_row();
label_cells(_('From Location'), $trans['from_name'], "class='tableheader2'");
label_cells(_('To Location'), $trans['to_name'], "class='tableheader2'");
end_row();

comments_display_row(ST_LOCTRANSFER, $trans_no);

end_table(2);

start_table(TABLESTYLE, "width='90%'");

$th = array(_('Item Code'), _('Description'), _('Quantity'), _('Units'));
table_header($th);
$transfer_items = get_stock_moves(ST_LOCTRANSFER, $trans_no);
$k = 0;
while ($item = db_fetch($transfer_items)) {
	if ($item['loc_code'] == $trans['to_loc']) {
		alt_table_row_color($k);

		label_cell($item['stock_id']);
		label_cell($item['description']);
		qty_cell($item['qty'], false, get_qty_dec($item['stock_id']));
		label_cell($item['units']);
		end_row();

		display_transfer_line_tracking($trans_no, $item['stock_id'], 4);
	}
}

end_table(1);

is_voided_display(ST_LOCTRANSFER, $trans_no, _('This transfer has been voided.'));

end_page(true, false, false, ST_LOCTRANSFER, $trans_no);
