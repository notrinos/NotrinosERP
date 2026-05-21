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

include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');

include_once($path_to_root.'/manufacturing/includes/manufacturing_db.inc');
include_once($path_to_root.'/manufacturing/includes/manufacturing_ui.inc');

$has_tracking = false;
if (file_exists($path_to_root.'/inventory/includes/db/serial_batch_db.inc')) {
	include_once($path_to_root.'/inventory/includes/db/serial_batch_db.inc');
	include_once($path_to_root.'/inventory/includes/db/serial_numbers_db.inc');
	include_once($path_to_root.'/inventory/includes/db/stock_batches_db.inc');
	include_once($path_to_root.'/manufacturing/includes/db/production_traceability_db.inc');
	$has_tracking = true;
}

/**
 * Display work-order-level production traceability when tracked genealogy exists.
 *
 * @param int $woid
 * @return void
 */
function display_wo_traceability_summary($woid) {
	$trace_result = get_wo_traceability($woid);
	$trace_rows = array();
	while ($trow = db_fetch($trace_result))
		$trace_rows[] = $trow;

	if (empty($trace_rows))
		return;

	br(1);
	display_heading2(_('Component Traceability'));
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
$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(800, 500);
page(_($help_context = 'View Work Order'), true, false, '', $js);

//-------------------------------------------------------------------------------------------------
$woid = 0;
if ($_GET['trans_no'] != '')
	$woid = $_GET['trans_no'];

display_heading($systypes_array[ST_WORKORDER] . ' # ' . $woid);

br(1);
$myrow = get_work_order($woid);

if ($myrow['type']  == WO_ADVANCED)
	display_wo_details($woid, true);
else
	display_wo_details_quick($woid, true);

echo '<center>';

// display the WO requirements
br(1);
if ($myrow['released'] == false) {
	display_heading2(_('BOM for item:') . ' ' . $myrow['StockItemName']);
	display_bom($myrow['stock_id']);
}
else {
	display_heading2(_('Work Order Requirements'));
	display_wo_requirements($woid, $myrow['units_reqd']);
	if ($myrow['type'] == WO_ADVANCED) {
		start_view_columns();
		view_column_start();
		display_heading2(_('Issues'));
		display_wo_issues($woid);
		view_column_next();
		display_heading2(_('Productions'));
		display_wo_productions($woid);
		view_column_next();
		display_heading2(_('Additional Costs'));
		display_wo_payments($woid);
		end_view_columns();
	}
	else {
		start_view_columns();
		view_column_start();
		display_heading2(_('Additional Costs'));
		display_wo_payments($woid);
		end_view_columns();
	}
	if ($has_tracking)
		display_wo_traceability_summary($woid);
}

echo '<br></center>';

is_voided_display(ST_WORKORDER, $woid, _('This work order has been voided.'));

end_page(true, false, false, ST_WORKORDER, $woid);
