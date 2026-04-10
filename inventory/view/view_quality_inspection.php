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
 * Quality Inspection View / Print page.
 *
 * Displays a printer-friendly view of a quality inspection record
 * including header details, all parameter readings with pass/fail results,
 * and completion summary. Designed to open in a new window/tab.
 */
$page_security = 'SA_QC_INSPECTIONS';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'View Quality Inspection');

page($_SESSION['page_title'], true, false, '', '');

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/inventory/includes/db/quality_inspection_db.inc');
include_once($path_to_root . '/inventory/includes/inventory_db.inc');

$inspection_id = (int)(isset($_GET['inspection_id']) ? $_GET['inspection_id'] : get_post('inspection_id'));

if (!$inspection_id) {
	display_error(_('No inspection ID specified.'));
	end_page();
	exit;
}

$insp = get_quality_inspection($inspection_id);
if (!$insp) {
	display_error(_('Inspection not found.'));
	end_page();
	exit;
}

// Print CSS
echo '<style>
@media print {
	.noprint { display:none !important; }
	body { font-size:11pt; }
}
.qc-view-header { margin:10px 0; }
.qc-view-header h2 { margin:0 0 5px 0; }
.qc-detail-table { border-collapse:collapse; margin:10px 0; }
.qc-detail-table td { padding:4px 15px 4px 0; vertical-align:top; }
.qc-detail-table .label { font-weight:600; white-space:nowrap; }
.qc-separator { border:0; border-top:2px solid #333; margin:15px 0; }
.qc-signature { margin-top:40px; display:flex; gap:50px; }
.qc-signature div { flex:1; }
.qc-signature .line { border-bottom:1px solid #333; height:30px; }
.qc-signature .caption { font-size:10px; color:#666; margin-top:3px; }
</style>';

// Action buttons
echo '<div class="noprint" style="text-align:center;margin:10px 0;">';
echo '<button onclick="window.print();" style="padding:6px 15px;cursor:pointer;">'
	. '<i class="fa fa-print"></i> ' . _('Print') . '</button>';
echo '&nbsp;&nbsp;';
echo '<button onclick="window.close();" style="padding:6px 15px;cursor:pointer;">'
	. '<i class="fa fa-times"></i> ' . _('Close') . '</button>';
echo '&nbsp;&nbsp;';
if ($insp['result'] !== 'pending') {
	echo '<a href="' . $path_to_root . '/reporting/rep_quality_coa.php?inspection_id='
		. $inspection_id . '" target="_blank" style="padding:6px 15px;text-decoration:none;'
		. 'background:#007bff;color:#fff;border-radius:4px;">'
		. '<i class="fa fa-file-pdf-o"></i> ' . _('Certificate of Analysis') . '</a>';
}
echo '</div>';

// Company header
$company = get_company_prefs();

echo '<div class="qc-view-header" style="text-align:center;">';
echo '<h2>' . $company['coy_name'] . '</h2>';
echo '<h3>' . _('Quality Inspection Report') . '</h3>';
echo '<p style="font-size:14px;font-weight:600;">'
	. sprintf(_('Inspection #%d'), $inspection_id) . ' — '
	. qc_result_badge($insp['result']) . '</p>';
echo '</div>';

echo '<hr class="qc-separator">';

// Inspection header details
echo '<table class="qc-detail-table">';
echo '<tr><td class="label">' . _('Item Code') . ':</td><td>' . $insp['stock_id'] . '</td>';
echo '<td class="label">' . _('Description') . ':</td><td>' . $insp['item_description'] . '</td></tr>';

echo '<tr><td class="label">' . _('Inspection Type') . ':</td><td>' . qc_type_badge($insp['inspection_type']) . '</td>';
echo '<td class="label">' . _('Unit') . ':</td><td>' . $insp['units'] . '</td></tr>';

echo '<tr><td class="label">' . _('Quantity Inspected') . ':</td><td>'
	. number_format2($insp['inspected_qty'], get_qty_dec($insp['stock_id'])) . '</td>';
echo '<td class="label">' . _('Accepted') . ':</td><td>'
	. number_format2($insp['accepted_qty'], get_qty_dec($insp['stock_id'])) . '</td></tr>';

echo '<tr><td class="label">' . _('Rejected') . ':</td><td>'
	. number_format2($insp['rejected_qty'], get_qty_dec($insp['stock_id'])) . '</td>';
echo '<td class="label">' . _('Result') . ':</td><td>' . qc_result_badge($insp['result']) . '</td></tr>';

if ($insp['batch_no']) {
	echo '<tr><td class="label">' . _('Batch / Lot') . ':</td><td>' . $insp['batch_no'] . '</td>';
	echo '<td></td><td></td></tr>';
}
if ($insp['serial_no']) {
	echo '<tr><td class="label">' . _('Serial Number') . ':</td><td>' . $insp['serial_no'] . '</td>';
	echo '<td></td><td></td></tr>';
}
if ($insp['loc_code']) {
	echo '<tr><td class="label">' . _('Location') . ':</td><td>' . $insp['loc_code'] . '</td>';
	echo '<td></td><td></td></tr>';
}

// Source document
if ($insp['trans_type'] == ST_SUPPRECEIVE && $insp['trans_no']) {
	// Get supplier
	$sql = "SELECT s.supp_name FROM " . TB_PREF . "grn_batch gb "
		. "JOIN " . TB_PREF . "suppliers s ON gb.supplier_id = s.supplier_id "
		. "WHERE gb.id=" . (int)$insp['trans_no'];
	$sup_result = db_query($sql, 'could not get supplier');
	$sup_row = db_fetch($sup_result);

	echo '<tr><td class="label">' . _('Source') . ':</td><td>' . _('GRN') . ' #' . $insp['trans_no'] . '</td>';
	echo '<td class="label">' . _('Supplier') . ':</td><td>' . ($sup_row ? $sup_row['supp_name'] : '—') . '</td></tr>';
}

echo '<tr><td class="label">' . _('Inspector') . ':</td><td>' . $insp['inspector_name'] . '</td>';
echo '<td class="label">' . _('Inspection Date') . ':</td><td>' . $insp['inspection_date'] . '</td></tr>';

if ($insp['completion_date']) {
	echo '<tr><td class="label">' . _('Completed') . ':</td><td>' . $insp['completion_date'] . '</td>';
	echo '<td></td><td></td></tr>';
}

if ($insp['notes']) {
	echo '<tr><td class="label">' . _('Notes') . ':</td><td colspan="3">' . nl2br(htmlspecialchars($insp['notes'])) . '</td></tr>';
}

echo '</table>';

echo '<hr class="qc-separator">';

// Readings table
echo '<h3>' . _('Inspection Readings') . '</h3>';

$readings = get_inspection_readings($inspection_id);
$has_readings = false;

start_table(TABLESTYLE, "width='95%'");
$th = array(_('#'), _('Parameter'), _('Type'), _('Expected Range'), _('Reading'), _('Unit'), _('Result'), _('Notes'));
table_header($th);

$k = 0;
$seq = 1;
while ($r = db_fetch($readings)) {
	$has_readings = true;
	alt_table_row_color($k);

	label_cell($seq++);
	label_cell('<strong>' . $r['parameter_name'] . '</strong>'
		. ($r['mandatory'] ? ' <span style="color:red;">*</span>' : ''));
	label_cell(get_qc_parameter_type_label($r['parameter_type']));

	// Expected range
	if ($r['parameter_type'] === 'numeric') {
		$range = '';
		if ($r['min_value'] !== null)
			$range .= number_format2($r['min_value'], 4);
		else
			$range .= '—';
		$range .= ' to ';
		if ($r['max_value'] !== null)
			$range .= number_format2($r['max_value'], 4);
		else
			$range .= '—';
		label_cell($range);
	} elseif ($r['parameter_type'] === 'list') {
		$vals = '';
		if ($r['acceptable_values']) {
			$decoded = html_entity_decode($r['acceptable_values'], ENT_QUOTES, 'UTF-8');
			$list = json_decode($decoded, true);
			if (is_array($list)) $vals = implode(', ', $list);
		}
		label_cell($vals);
	} elseif ($r['parameter_type'] === 'boolean') {
		label_cell(_('Pass / Fail'));
	} else {
		label_cell('—');
	}

	// Reading value
	if ($r['parameter_type'] === 'boolean') {
		label_cell($r['reading_value'] === '1' ? _('Pass') : _('Fail'));
	} else {
		label_cell($r['reading_value']);
	}

	label_cell($r['unit'] ? $r['unit'] : '—');
	label_cell(qc_result_badge($r['result']));
	label_cell($r['notes'] ? $r['notes'] : '—');

	end_row();
}

if (!$has_readings) {
	label_row('', _('No readings recorded.'), 'colspan="8" style="text-align:center;"');
}

end_table(1);

// Summary
$counts = count_inspection_readings($inspection_id);
echo '<div style="margin:10px 0;padding:8px 15px;background:#f8f9fa;border-radius:4px;">';
echo '<strong>' . _('Summary') . ':</strong> '
	. sprintf(_('%d total readings — %d pass, %d fail'), $counts['total'], $counts['pass'], $counts['fail']);
echo '</div>';

// Signature area
echo '<div class="qc-signature">';
echo '<div>';
echo '<div class="line"></div>';
echo '<div class="caption">' . _('Inspected By / Date') . '</div>';
echo '</div>';
echo '<div>';
echo '<div class="line"></div>';
echo '<div class="caption">' . _('Verified By / Date') . '</div>';
echo '</div>';
echo '<div>';
echo '<div class="line"></div>';
echo '<div class="caption">' . _('Approved By / Date') . '</div>';
echo '</div>';
echo '</div>';

end_page(true);
