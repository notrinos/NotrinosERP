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
include_once($path_to_root.'/includes/session.inc');

$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 500);
page(_($help_context = 'View Work Order Issue'), true, false, '', $js);

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
	$wo_issue_no = $_GET['trans_no'];

//-------------------------------------------------------------------------------------------------

function display_wo_issue($issue_no) {
	$myrow = get_work_order_issue($issue_no);

	br(1);
	start_table(TABLESTYLE);
	$th = array(_('Issue #'), _('Reference'), _('For Work Order #'), _('Item'), _('From Location'), _('To Work Centre'), _('Date of Issue'));
	table_header($th);

	start_row();
	label_cell($myrow['issue_no']);
	label_cell($myrow['reference']);
	label_cell(get_trans_view_str(ST_WORKORDER,$myrow['workorder_id']));
	label_cell($myrow['stock_id'] . ' - ' . $myrow['description']);
	label_cell($myrow['location_name']);
	label_cell($myrow['WorkCentreName']);
	label_cell(sql2date($myrow['issue_date']));
	end_row();

	comments_display_row(ST_MANUISSUE, $issue_no);

	end_table(1);

	is_voided_display(ST_MANUISSUE, $issue_no, _('This issue has been voided.'));
}

//-------------------------------------------------------------------------------------------------

function display_wo_issue_details($issue_no) {
	$result = get_work_order_issue_details($issue_no);

	if (db_num_rows($result) == 0)
		display_note(_('There are no items for this issue.'));
	else {
		start_table(TABLESTYLE);
		$th = array(_('Component'), _('Quantity'), _('Units'), _('Unit Cost'));

		table_header($th);

		$j = 1;
		$k = 0; //row colour counter

		$total_cost = 0;

		while ($myrow = db_fetch($result)) {

			alt_table_row_color($k);

			label_cell($myrow['stock_id']  . ' - ' . $myrow['description']);
			qty_cell($myrow['qty_issued'], false, get_qty_dec($myrow['stock_id']));
			label_cell($myrow['units']);
			amount_cell($myrow['unit_cost']);
			end_row();

			$j++;
			if ($j == 12) {
				$j = 1;
				table_header($th);
			}
		}

		end_table();
	}
}

//-------------------------------------------------------------------------------------------------

display_heading($systypes_array[ST_MANUISSUE] . ' # ' . $wo_issue_no);

display_wo_issue($wo_issue_no);

display_heading2(_('Items for this Issue'));

display_wo_issue_details($wo_issue_no);

// --- Display serial/batch tracking info for this issue ---
if ($has_tracking) {
	$issue_data = get_work_order_issue($wo_issue_no);
	if ($issue_data) {
		$woid = $issue_data['workorder_id'];
		// Get issue items with tracking info from stock_moves
		$track_sql = "SELECT DISTINCT sm.stock_id, sm.serial_id, sm.batch_id, sm.expiry_date, sm.qty,
				sn.serial_no, sb.batch_no, item.description
			FROM " . TB_PREF . "stock_moves sm
			LEFT JOIN " . TB_PREF . "serial_numbers sn ON sm.serial_id = sn.id
			LEFT JOIN " . TB_PREF . "stock_batches sb ON sm.batch_id = sb.id
			LEFT JOIN " . TB_PREF . "stock_master item ON sm.stock_id = item.stock_id
			WHERE sm.type = " . ST_MANUISSUE . "
			AND sm.trans_no = " . (int)$wo_issue_no . "
			AND (sm.serial_id IS NOT NULL OR sm.batch_id IS NOT NULL)
			ORDER BY sm.stock_id, sn.serial_no, sb.batch_no";
		$track_result = db_query($track_sql, 'could not get tracking info');

		$has_rows = false;
		$current_stock = '';
		while ($tr = db_fetch($track_result)) {
			if (!$has_rows) {
				$has_rows = true;
				echo '<br>';
				display_heading2(_('Serial/Batch Tracking'));
				start_table(TABLESTYLE, "width='80%'");
				$th = array(_('Component'), _('Serial #'), _('Batch #'), _('Expiry'), _('Qty'));
				table_header($th);
			}
			alt_table_row_color($k);
			if ($tr['stock_id'] !== $current_stock) {
				label_cell($tr['stock_id'] . ' - ' . $tr['description']);
				$current_stock = $tr['stock_id'];
			} else {
				label_cell('');
			}
			label_cell(!empty($tr['serial_no']) ? $tr['serial_no'] : '-');
			label_cell(!empty($tr['batch_no']) ? $tr['batch_no'] : '-');
			label_cell(!empty($tr['expiry_date']) ? sql2date($tr['expiry_date']) : '-');
			qty_cell(abs($tr['qty']), false, 2);
			end_row();
		}
		if ($has_rows)
			end_table();

		// Show production traceability
		$trace_result = get_wo_traceability($woid);
		$trace_rows = array();
		while ($trow = db_fetch($trace_result)) {
			$trace_rows[] = $trow;
		}
		if (!empty($trace_rows)) {
			echo '<br>';
			display_heading2(_('Production Traceability'));
			start_table(TABLESTYLE, "width='80%'");
			$th = array(_('Component'), _('Component Serial'), _('Component Batch'), _('Qty'), _('Finished Serial'), _('Finished Batch'));
			table_header($th);
			$k = 0;
			foreach ($trace_rows as $trow) {
				alt_table_row_color($k);
				label_cell($trow['component_stock_id'] . ' - ' . $trow['component_description']);
				label_cell(!empty($trow['component_serial_no']) ? $trow['component_serial_no'] : '-');
				label_cell(!empty($trow['component_batch_no']) ? $trow['component_batch_no'] : '-');
				qty_cell($trow['component_qty'], false, 2);
				label_cell(!empty($trow['finished_serial_no']) ? $trow['finished_serial_no'] : '-');
				label_cell(!empty($trow['finished_batch_no']) ? $trow['finished_batch_no'] : '-');
				end_row();
			}
			end_table();
		}
	}
}

echo '<br>';

end_page(true, false, false, ST_MANUISSUE, $wo_issue_no);
