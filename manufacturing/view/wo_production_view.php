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
$page_security = 'SA_MANUFTRANSVIEW';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');

$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 500);
page(_($help_context = 'View Work Order Production'), true, false, '', $js);

include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');

include_once($path_to_root.'/manufacturing/includes/manufacturing_db.inc');
include_once($path_to_root.'/manufacturing/includes/manufacturing_ui.inc');

// Load tracking modules if available
$has_tracking = false;
if (file_exists($path_to_root.'/inventory/includes/db/serial_batch_db.inc')) {
	include_once($path_to_root.'/inventory/includes/db/serial_batch_db.inc');
	include_once($path_to_root.'/inventory/includes/db/serial_numbers_db.inc');
	include_once($path_to_root.'/inventory/includes/db/stock_batches_db.inc');
	include_once($path_to_root.'/inventory/includes/serial_batch_ui.inc');
	include_once($path_to_root.'/manufacturing/includes/db/production_traceability_db.inc');
	$has_tracking = true;
}

//-------------------------------------------------------------------------------------------------

if ($_GET['trans_no'] != '')
	$wo_production = $_GET['trans_no'];

//-------------------------------------------------------------------------------------------------

function display_wo_production($prod_id) {
	$myrow = get_work_order_produce($prod_id);

	br(1);
	start_table(TABLESTYLE);
	$th = array(_('Production #'), _('Reference'), _('For Work Order #'), _('Item'), _('Quantity Manufactured'), _('Date'));
	table_header($th);

	start_row();
	label_cell($myrow['id']);
	label_cell($myrow['reference']);
	label_cell(get_trans_view_str(ST_WORKORDER,$myrow['workorder_id']));
	label_cell($myrow['stock_id'] . ' - ' . $myrow['StockDescription']);
	qty_cell($myrow['quantity'], false, get_qty_dec($myrow['stock_id']));
	label_cell(sql2date($myrow['date_']));
	end_row();

	comments_display_row(ST_MANURECEIVE, $prod_id);

	end_table(1);

	is_voided_display(ST_MANURECEIVE, $prod_id, _('This production has been voided.'));
}

//-------------------------------------------------------------------------------------------------

display_heading($systypes_array[ST_MANURECEIVE] . ' # ' . $wo_production);

display_wo_production($wo_production);

// --- Display serial/batch tracking for produced items ---
if ($has_tracking) {
	$prod_data = get_work_order_produce($wo_production);
	if ($prod_data) {
		$woid = $prod_data['workorder_id'];
		$stock_id = $prod_data['stock_id'];

		// Check if the finished product has tracking
		$tracking_mode = get_item_tracking_mode($stock_id);
		if ($tracking_mode !== 'none') {
			// Find serials created for this production receipt
			$serial_sql = "SELECT sn.serial_no, sn.status, sn.manufacturing_date
				FROM " . TB_PREF . "serial_movements smov
				INNER JOIN " . TB_PREF . "serial_numbers sn ON smov.serial_id = sn.id
				WHERE smov.trans_type = " . ST_MANURECEIVE . "
				AND smov.trans_no = " . (int)$wo_production . "
				AND sn.stock_id = " . db_escape($stock_id) . "
				ORDER BY sn.serial_no";
			$serial_result = db_query($serial_sql, 'could not get production serials');
			$has_serials = false;

			while ($sr = db_fetch($serial_result)) {
				if (!$has_serials) {
					$has_serials = true;
					br();
					display_heading2(_('Produced Serial Numbers'));
					start_table(TABLESTYLE, "width='60%'");
					$th = array(_('Serial Number'), _('Status'), _('Mfg Date'));
					table_header($th);
					$k = 0;
				}
				alt_table_row_color($k);
				label_cell($sr['serial_no']);
				label_cell(function_exists('serial_status_badge') ? serial_status_badge($sr['status']) : $sr['status']);
				label_cell($sr['manufacturing_date'] ? sql2date($sr['manufacturing_date']) : '-');
				end_row();
			}
			if ($has_serials) end_table();

			// Find batch created for this production
			$batch_sql = "SELECT sb.batch_no, sb.status, sb.manufacturing_date, sb.expiry_date, sb.initial_qty
				FROM " . TB_PREF . "batch_movements bm
				INNER JOIN " . TB_PREF . "stock_batches sb ON bm.batch_id = sb.id
				WHERE bm.trans_type = " . ST_MANURECEIVE . "
				AND bm.trans_no = " . (int)$wo_production . "
				AND sb.stock_id = " . db_escape($stock_id) . "
				AND bm.quantity > 0
				ORDER BY sb.batch_no";
			$batch_result = db_query($batch_sql, 'could not get production batches');
			$has_batches = false;

			while ($br_row = db_fetch($batch_result)) {
				if (!$has_batches) {
					$has_batches = true;
					br();
					display_heading2(_('Produced Batch / Lot'));
					start_table(TABLESTYLE, "width='60%'");
					$th = array(_('Batch #'), _('Status'), _('Mfg Date'), _('Expiry'), _('Qty'));
					table_header($th);
					$k = 0;
				}
				alt_table_row_color($k);
				label_cell($br_row['batch_no']);
				label_cell(function_exists('batch_status_badge') ? batch_status_badge($br_row['status']) : $br_row['status']);
				label_cell($br_row['manufacturing_date'] ? sql2date($br_row['manufacturing_date']) : '-');
				label_cell($br_row['expiry_date'] ? sql2date($br_row['expiry_date']) : '-');
				qty_cell($br_row['initial_qty'], false, get_qty_dec($stock_id));
				end_row();
			}
			if ($has_batches) end_table();
		}

		// Show component traceability for this WO
		$trace_result = get_wo_traceability($woid);
		$trace_rows = array();
		while ($trow = db_fetch($trace_result)) {
			$trace_rows[] = $trow;
		}
		if (!empty($trace_rows)) {
			br();
			display_heading2(_('Component Traceability'));
			start_table(TABLESTYLE, "width='80%'");
			$th = array(_('Component'), _('Component Serial'), _('Component Batch'), _('Qty Consumed'));
			table_header($th);
			$k = 0;
			foreach ($trace_rows as $trow) {
				alt_table_row_color($k);
				label_cell($trow['component_stock_id'] . ' - ' . $trow['component_description']);
				label_cell(!empty($trow['component_serial_no']) ? $trow['component_serial_no'] : '-');
				label_cell(!empty($trow['component_batch_no']) ? $trow['component_batch_no'] : '-');
				qty_cell($trow['component_qty'], false, 2);
				end_row();
			}
			end_table();
		}
	}
}

br(2);

end_page(true, false, false, ST_MANURECEIVE, $wo_production);
