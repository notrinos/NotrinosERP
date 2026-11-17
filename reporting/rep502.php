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
$page_security = 'SA_DIMENSIONREP';
$path_to_root='..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/gl/includes/gl_db.inc');
include_once($path_to_root . '/reporting/includes/report_access_scope.inc');
include_once($path_to_root . '/tracking/includes/tracking_values_db.inc');

//----------------------------------------------------------------------------------------------------

print_dimension_exceptions();

/**
 * DIM-REP-006: Dimension comparison, account matrix and exception queue (PDF and Excel).
 *
 * The report authorizes the Dimension reports area (page security) and the reviewed `dimension_exception_workspace` aggregate
 * (Dimension view and GL analytics) before the first read, validates every parameter, builds the SAME dataset the Dimension
 * exceptions page renders (`dimension_export_dataset()`), and prints it section by section through the native PDF or Excel
 * renderer. Numbers therefore cannot differ between the page, the PDF and the Excel file.
 *
 * @return void
 */
function print_dimension_exceptions() {
	global $path_to_root;
	check_page_security('SA_DIMENSIONREP');

	$access = report_access_aggregate_scope('dimension_exception_workspace');
	if (empty($access['allowed'])) {
		display_error(report_access_aggregate_state_text($access));
		return;
	}
	include_once($path_to_root . '/tracking/includes/tracking_exception_workspace.inc');

	$parameter = array();
	for ($index = 0; $index <= 9; $index++)
		$parameter[$index] = isset($_POST['PARAM_'.$index]) ? $_POST['PARAM_'.$index] : null;
	foreach (array(0, 1, 7) as $index) {
		if (!is_string($parameter[$index])) {
			display_error($index === 7 ? _('Invalid report comments.') : _('Invalid report period.'));
			return;
		}
	}
	foreach (array(2, 3, 4, 5, 6, 8, 9) as $index) {
		if ((!is_int($parameter[$index]) && !is_string($parameter[$index])) || !preg_match('/^(?:0|[1-9][0-9]{0,9})$/D', (string)$parameter[$index])) {
			display_error($index === 2 ? _('Invalid comparison.') : ($index === 3 || $index === 4 ? _('Invalid tracking value.') : _('Invalid report option.')));
			return;
		}
	}
	$mode_index = (int)$parameter[2];
	foreach (array(5, 6, 8, 9) as $index) {
		if ((int)$parameter[$index] > 1) {
			display_error(_('Invalid report option.'));
			return;
		}
	}
	$modes = dimension_saved_filter_modes();
	if (!isset($modes[$mode_index])) {
		display_error(_('Invalid comparison.'));
		return;
	}
	$fiscal_start = begin_fiscalyear();
	if (!is_date($parameter[0]) || !is_date($parameter[1]) || !is_string($fiscal_start) || !is_date($fiscal_start) || date1_greater_date2($parameter[0], $parameter[1])) {
		display_error(_('Invalid report period.'));
		return;
	}
	$enabled = (int)get_company_pref('use_dimension');
	// DIM-PERM-001: customer names and amounts need the same three areas as the Customer Economics page. This report's own
	// aggregate does not include the sales view area, so the customer-based reason is withheld unless that aggregate also passes.
	$customer_access = report_access_aggregate_scope('dimension_customer_economics');
	$dataset = dimension_export_dataset(array(
		'customer_access' => !empty($customer_access['allowed']),
		'from' => date2sql($parameter[0]), 'to' => date2sql($parameter[1]), 'scope1' => (int)$parameter[3], 'scope2' => (int)$parameter[4],
		'mode' => $modes[$mode_index], 'fiscal_start' => date2sql($fiscal_start), 'matrix' => (int)$parameter[5] === 1, 'queue' => (int)$parameter[6] === 1,
		'axes' => array(1, 2), 'queue_axes' => $enabled >= 2 ? array(1, 2) : ($enabled === 1 ? array(1) : array())));
	if ($dataset === false) {
		display_error(_('The comparison and exceptions could not be calculated.'));
		return;
	}

	$destination = (int)$parameter[9];
	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');
	$orientation = ((int)$parameter[8] ? 'L' : 'P');
	$comparison_names = array(_('Previous period'), _('Year to date vs last year'), _('Monthly budget'));
	$dec = user_price_dec();

	$params = array(0 => $parameter[7],
		1 => array('text' => _('Period'), 'from' => $parameter[0], 'to' => $parameter[1]),
		2 => array('text' => dimension_axis_label(1), 'from' => get_dimension_string((int)$parameter[3]), 'to' => ''),
		3 => array('text' => dimension_axis_label(2), 'from' => get_dimension_string((int)$parameter[4]), 'to' => ''),
		4 => array('text' => _('Compare to'), 'from' => $comparison_names[$mode_index], 'to' => ''));

	$rep = new FrontReport(_('Tracking Comparison and Exceptions'), 'DimensionExceptions', user_pagesize(), 9, $orientation);
	$rep->Font();
	foreach ($dataset['sections'] as $section) {
		$cols = dimension_export_column_positions($section['aligns']);
		if ($orientation == 'L')
			recalculate_cols($cols);
		$headers = $section['headers'];
		$headers[0] = $section['title'].' - '.$headers[0];
		$rep->Info($params, $cols, $headers, $section['aligns']);
		$rep->NewPage();
		foreach ($section['rows'] as $row) {
			foreach ($row as $column => $cell) {
				if (is_float($cell))
					$rep->AmountCol($column, $column + 1, $cell, $dec);
				else
					$rep->TextCol($column, $column + 1, (string)$cell);
			}
			$rep->NewLine();
		}
		foreach ($section['notes'] as $note) {
			$rep->NewLine();
			$rep->TextCol(0, count($section['aligns']), (string)$note);
		}
	}
	$rep->Line($rep->row);
	$rep->End();
}
