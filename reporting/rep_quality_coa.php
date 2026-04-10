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
 * Certificate of Analysis (COA) PDF Report.
 *
 * Generates a PDF Certificate of Analysis for a completed quality inspection.
 * Accepts ?inspection_id=N via GET parameter.
 *
 * Uses the FrontReport PDF framework.
 */
$page_security = 'SA_QC_INSPECTIONS';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/includes/db/quality_inspection_db.inc');
include_once($path_to_root . '/inventory/includes/inventory_db.inc');

// Get inspection ID from GET or POST
$inspection_id = 0;
if (isset($_GET['inspection_id']))
	$inspection_id = (int)$_GET['inspection_id'];
elseif (isset($_POST['PARAM_0']))
	$inspection_id = (int)$_POST['PARAM_0'];

if (!$inspection_id) {
	display_error(_('No inspection ID specified.'));
	exit;
}

print_coa_report($inspection_id);

/**
 * Generate and output the Certificate of Analysis PDF.
 *
 * @param int $inspection_id Quality inspection ID
 * @return void
 */
function print_coa_report($inspection_id)
{
	global $path_to_root;

	include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$coa = get_coa_data($inspection_id);
	if (!$coa) {
		display_error(_('Inspection data not found.'));
		return;
	}

	$insp = $coa['inspection'];
	$readings = $coa['readings'];
	$counts = $coa['counts'];
	$supplier_name = $coa['supplier_name'];

	// Setup report
	$orientation = 'P';
	$dec = user_qty_dec();

	$cols = array(0, 30, 180, 260, 350, 420, 480, 530);

	$headers = array(
		_('#'),
		_('Parameter'),
		_('Type'),
		_('Specification'),
		_('Result'),
		_('P/F'),
		_('Notes')
	);

	$aligns = array('left', 'left', 'left', 'left', 'left', 'center', 'left');

	// Build params display
	$result_label = get_qc_result_label($insp['result']);
	$type_label = get_qc_type_label($insp['inspection_type']);

	$params = array(
		0 => _('Certificate of Analysis'),
		1 => array('text' => _('Inspection'), 'from' => '#' . $inspection_id, 'to' => ''),
		2 => array('text' => _('Item'), 'from' => $insp['stock_id'] . ' — ' . $insp['item_description'], 'to' => ''),
		3 => array('text' => _('Result'), 'from' => $result_label, 'to' => ''),
	);

	$rep = new FrontReport(
		_('Certificate of Analysis'),
		'CertificateOfAnalysis_' . $inspection_id,
		user_pagesize(),
		9,
		$orientation
	);

	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	// Header section
	$rep->fontSize += 2;
	$rep->Font('bold');
	$rep->TextCol(0, 7, _('CERTIFICATE OF ANALYSIS'));
	$rep->Font();
	$rep->fontSize -= 2;
	$rep->NewLine(2);

	// Inspection details section
	$rep->Font('bold');
	$rep->TextCol(0, 2, _('Document No:'));
	$rep->Font();
	$rep->TextCol(2, 4, sprintf('COA-%06d', $inspection_id));
	$rep->NewLine();

	$rep->Font('bold');
	$rep->TextCol(0, 2, _('Item Code:'));
	$rep->Font();
	$rep->TextCol(2, 4, $insp['stock_id']);
	$rep->Font('bold');
	$rep->TextCol(4, 5, _('Description:'));
	$rep->Font();
	$rep->TextCol(5, 7, $insp['item_description']);
	$rep->NewLine();

	$rep->Font('bold');
	$rep->TextCol(0, 2, _('Inspection Type:'));
	$rep->Font();
	$rep->TextCol(2, 4, $type_label);
	$rep->Font('bold');
	$rep->TextCol(4, 5, _('Overall Result:'));
	$rep->Font('bold');
	$rep->TextCol(5, 7, strtoupper($result_label));
	$rep->Font();
	$rep->NewLine();

	$rep->Font('bold');
	$rep->TextCol(0, 2, _('Qty Inspected:'));
	$rep->Font();
	$rep->TextCol(2, 4, number_format2($insp['inspected_qty'], $dec) . ' ' . $insp['units']);
	$rep->Font('bold');
	$rep->TextCol(4, 5, _('Accepted:'));
	$rep->Font();
	$rep->TextCol(5, 6, number_format2($insp['accepted_qty'], $dec));
	$rep->Font('bold');
	$rep->TextCol(6, 7, _('Rejected:'));
	$rep->Font();
	// Print rejected on same line via text col is limited, put on next line
	$rep->NewLine();

	$rep->Font('bold');
	$rep->TextCol(0, 2, _('Rejected Qty:'));
	$rep->Font();
	$rep->TextCol(2, 4, number_format2($insp['rejected_qty'], $dec));
	$rep->NewLine();

	if ($insp['batch_no']) {
		$rep->Font('bold');
		$rep->TextCol(0, 2, _('Batch / Lot:'));
		$rep->Font();
		$rep->TextCol(2, 4, $insp['batch_no']);
		$rep->NewLine();
	}

	if ($insp['serial_no']) {
		$rep->Font('bold');
		$rep->TextCol(0, 2, _('Serial Number:'));
		$rep->Font();
		$rep->TextCol(2, 4, $insp['serial_no']);
		$rep->NewLine();
	}

	if ($supplier_name) {
		$rep->Font('bold');
		$rep->TextCol(0, 2, _('Supplier:'));
		$rep->Font();
		$rep->TextCol(2, 7, $supplier_name);
		$rep->NewLine();
	}

	if ($insp['trans_type'] == ST_SUPPRECEIVE) {
		$rep->Font('bold');
		$rep->TextCol(0, 2, _('GRN Reference:'));
		$rep->Font();
		$rep->TextCol(2, 4, '#' . $insp['trans_no']);
		$rep->NewLine();
	}

	$rep->Font('bold');
	$rep->TextCol(0, 2, _('Inspection Date:'));
	$rep->Font();
	$rep->TextCol(2, 4, $insp['inspection_date']);
	if ($insp['completion_date']) {
		$rep->Font('bold');
		$rep->TextCol(4, 5, _('Completed:'));
		$rep->Font();
		$rep->TextCol(5, 7, $insp['completion_date']);
	}
	$rep->NewLine();

	$rep->Font('bold');
	$rep->TextCol(0, 2, _('Inspector:'));
	$rep->Font();
	$rep->TextCol(2, 4, $insp['inspector_name']);
	$rep->NewLine(2);

	// Separator line
	$rep->Line($rep->row);
	$rep->NewLine(2);

	// Readings table header
	$rep->Font('bold');
	$rep->TextCol(0, 1, _('#'));
	$rep->TextCol(1, 3, _('Parameter'));
	$rep->TextCol(3, 4, _('Type'));
	$rep->TextCol(4, 5, _('Specification'));
	$rep->TextCol(5, 6, _('Reading'));
	$rep->TextCol(6, 7, _('P/F'));
	$rep->Font();
	$rep->NewLine();
	$rep->Line($rep->row);
	$rep->NewLine();

	// Readings
	$seq = 1;
	foreach ($readings as $r) {
		$rep->TextCol(0, 1, $seq++);
		$rep->TextCol(1, 3, $r['parameter_name']);
		$rep->TextCol(3, 4, get_qc_parameter_type_label($r['parameter_type']));

		// Specification column
		$spec = '';
		if ($r['parameter_type'] === 'numeric') {
			$min = ($r['min_value'] !== null) ? number_format2($r['min_value'], 4) : '—';
			$max = ($r['max_value'] !== null) ? number_format2($r['max_value'], 4) : '—';
			$spec = $min . ' - ' . $max;
			if ($r['unit'])
				$spec .= ' ' . $r['unit'];
		} elseif ($r['parameter_type'] === 'list') {
			if ($r['acceptable_values']) {
				$decoded = html_entity_decode($r['acceptable_values'], ENT_QUOTES, 'UTF-8');
				$list = json_decode($decoded, true);
				if (is_array($list)) $spec = implode(', ', $list);
			}
		} elseif ($r['parameter_type'] === 'boolean') {
			$spec = _('Pass / Fail');
		} else {
			$spec = '—';
		}
		$rep->TextCol(4, 5, $spec);

		// Reading value
		if ($r['parameter_type'] === 'boolean') {
			$rep->TextCol(5, 6, $r['reading_value'] === '1' ? _('Pass') : _('Fail'));
		} else {
			$rep->TextCol(5, 6, $r['reading_value']);
		}

		// Pass/Fail
		$rep->TextCol(6, 7, strtoupper($r['result']));
		$rep->NewLine();
	}

	if (empty($readings)) {
		$rep->TextCol(0, 7, _('No readings recorded.'));
		$rep->NewLine();
	}

	// Separator
	$rep->Line($rep->row);
	$rep->NewLine(2);

	// Summary
	$rep->Font('bold');
	$rep->TextCol(0, 2, _('Summary:'));
	$rep->Font();
	$rep->TextCol(2, 7, sprintf(
		_('%d readings — %d pass, %d fail'),
		$counts['total'], $counts['pass'], $counts['fail']
	));
	$rep->NewLine(2);

	// Overall conclusion
	$rep->Font('bold');
	$rep->TextCol(0, 2, _('Conclusion:'));
	$rep->Font();
	if ($insp['result'] === 'pass') {
		$rep->TextCol(2, 7, _('This material/product has been inspected and meets all specified quality requirements.'));
	} elseif ($insp['result'] === 'fail') {
		$rep->TextCol(2, 7, _('This material/product has been inspected and does NOT meet specified quality requirements.'));
	} elseif ($insp['result'] === 'partial') {
		$rep->TextCol(2, 7, _('This material/product has been inspected with partial compliance to quality requirements.'));
	} else {
		$rep->TextCol(2, 7, _('Inspection pending.'));
	}
	$rep->NewLine(3);

	// Signature lines
	$rep->Line($rep->row, 0, 180);
	$rep->NewLine();
	$rep->fontSize -= 2;
	$rep->TextCol(0, 3, _('Inspected By / Date'));
	$rep->fontSize += 2;
	$rep->NewLine(3);

	$rep->Line($rep->row, 0, 180);
	$rep->NewLine();
	$rep->fontSize -= 2;
	$rep->TextCol(0, 3, _('Approved By / Date'));
	$rep->fontSize += 2;

	// Notes
	if ($insp['notes']) {
		$rep->NewLine(2);
		$rep->Font('bold');
		$rep->TextCol(0, 2, _('Notes:'));
		$rep->Font();
		$rep->TextCol(2, 7, $insp['notes']);
	}

	$rep->End();
}
