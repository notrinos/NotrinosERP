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
 * DIM-REP-006: Dimension comparisons, account matrix and exception work queue.
 *
 * One read-only page that renders the shared export dataset (the same one report 502 prints to PDF and Excel). It needs the
 * Dimension view area from the page security and, through the reviewed `dimension_exception_workspace` aggregate, the GL
 * analytics area as well; a denied user reads no data. The missing-customer-cost reason is read only when the user also passes the
 * `dimension_customer_economics` aggregate. A saved filter holds only the comparison mode, the two scopes, the matrix axis and the
 * reason list, in a cookie bound to the user, company and live grant fingerprint; money is always regenerated from the ledger.
 */
$page_security = 'SA_DIMTRANSVIEW';
$path_to_root = '../..';

include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/reporting/includes/report_access_scope.inc');
include_once($path_to_root.'/tracking/includes/tracking_values_db.inc');
include_once($path_to_root.'/tracking/includes/tracking_values_ui.inc');
include_once($path_to_root.'/tracking/includes/tracking_exception_workspace.inc');

//-----------------------------------------------------------------------------------
// The saved filter cookie is written before any output. It is a filter definition only: no period, no amount, no name.

$access = report_access_aggregate_scope('dimension_exception_workspace');
$saved_cookie = dimension_saved_filter_cookie_name(1);
if (!empty($access['allowed']) && isset($_POST['SaveFilter'])) {
	$to_save = array('mode' => get_post('Mode', 'previous'), 'scope1' => check_value('Unassigned1') ? -1 : (int)get_post('Dimension', 0),
		'scope2' => check_value('Unassigned2') ? -1 : (int)get_post('Dimension2', 0), 'axis' => (int)get_post('MatrixAxis', 1), 'reasons' => array());
	foreach (dimension_exception_reasons() as $reason)
		if (check_value('Reason_'.$reason))
			$to_save['reasons'][] = $reason;
	$encoded = dimension_saved_filter_encode($to_save);
	if ($encoded !== false)
		setcookie($saved_cookie, $encoded, time() + 60 * 60 * 24 * 30, '/', '', false, true);
}

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();
$help_context = 'Dimension Exceptions';
page(_('Missing or Invalid Tracking'), false, false, '', $js);

//-----------------------------------------------------------------------------------

if (empty($access['allowed'])) {
	report_access_render_aggregate_state($access);
	end_page();
	exit;
}

$modes = dimension_saved_filter_modes();
$mode_names = array('previous' => _('Previous period'), 'ytd' => _('Year to date vs last year'), 'budget' => _('Monthly budget'));
if (isset($_POST['LoadFilter']) && isset($_COOKIE['dimfilter'][report_access_selection_key('dimension_saved_filter', 1)])) {
	$loaded = dimension_saved_filter_decode($_COOKIE['dimfilter'][report_access_selection_key('dimension_saved_filter', 1)]);
	if ($loaded === false)
		display_error(_('The saved filter is not valid and was ignored.'));
	else {
		$_POST['Mode'] = $loaded['mode'];
		$_POST['Dimension'] = $loaded['scope1'] > 0 ? $loaded['scope1'] : 0;
		$_POST['Dimension2'] = $loaded['scope2'] > 0 ? $loaded['scope2'] : 0;
		$_POST['Unassigned1'] = $loaded['scope1'] < 0 ? 1 : 0;
		$_POST['Unassigned2'] = $loaded['scope2'] < 0 ? 1 : 0;
		$_POST['MatrixAxis'] = $loaded['axis'];
		foreach (dimension_exception_reasons() as $reason)
			$_POST['Reason_'.$reason] = in_array($reason, $loaded['reasons'], true) ? 1 : 0;
		$Ajax->activate('_page_body');
	}
}
if (!isset($_POST['Mode']) || !in_array($_POST['Mode'], $modes, true))
	$_POST['Mode'] = 'previous';
if (!isset($_POST['TransFromDate']))
	$_POST['TransFromDate'] = begin_fiscalyear();
if (!isset($_POST['TransToDate']))
	$_POST['TransToDate'] = Today();
if (!isset($_POST['Show']) && !isset($_POST['LoadFilter']) && !isset($_POST['SaveFilter']) && !isset($_POST['MatrixAxis'])) {
	foreach (dimension_exception_reasons() as $reason)
		$_POST['Reason_'.$reason] = 1;
	$_POST['IncludeMatrix'] = 1;
	$_POST['IncludeQueue'] = 1;
}

// DIM-G4: the filters post through AJAX and every block below depends on them. Without this the response was empty and
// Show or a changed Dimension silently left the previous figures on screen.
if (!empty($_POST))
	$Ajax->activate('_page_body');

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();
date_cells(_('From:'), 'TransFromDate');
date_cells(_('To:'), 'TransToDate');
filter_cell_open(_('Compare to:'));
echo array_selector('Mode', $_POST['Mode'], $mode_names);
filter_cell_close();
dimensions_history_list_cells(dimension_axis_label_html(1, ':'), 'Dimension', null, true, sprintf(_('%s: all'), dimension_axis_label_html(1)));
check_cells(_('Unassigned only:'), 'Unassigned1', null, false, sprintf(_('Show only postings with no %s value'), dimension_axis_label_html(1)));
dimensions_history_list_cells(dimension_axis_label_html(2, ':'), 'Dimension2', null, true, sprintf(_('%s: all'), dimension_axis_label_html(2)));
check_cells(_('Unassigned only:'), 'Unassigned2', null, false, sprintf(_('Show only postings with no %s value'), dimension_axis_label_html(2)));
end_row();
end_table();
start_table(TABLESTYLE_NOBORDER);
start_row();
filter_cell_open(_('Matrix category:'));
echo array_selector('MatrixAxis', get_post('MatrixAxis', 1), array(1 => _('Category 1'), 2 => _('Category 2')));
filter_cell_close();
check_cells(_('Account matrix:'), 'IncludeMatrix', null);
check_cells(_('Exception queue:'), 'IncludeQueue', null);
end_row();
start_row();
foreach (dimension_exception_reasons() as $reason)
	check_cells(dimension_exception_reason_text($reason), 'Reason_'.$reason, null);
end_row();
start_row();
submit_cells('Show', _('Show'), '', '', 'default');
submit_cells('SaveFilter', _('Save filter'), '', _('Saves the comparison, scopes, category and reasons only; amounts are always read again'), false);
submit_cells('LoadFilter', _('Load saved filter'), '', '', false);
end_row();
end_table();
end_form();

//-----------------------------------------------------------------------------------

if (!is_date($_POST['TransFromDate']) || !is_date($_POST['TransToDate']) || date1_greater_date2($_POST['TransFromDate'], $_POST['TransToDate'])) {
	display_error(_('The period is not valid.'));
}
else {
	$scope1 = check_value('Unassigned1') ? -1 : (int)get_post('Dimension', 0);
	$scope2 = check_value('Unassigned2') ? -1 : (int)get_post('Dimension2', 0);
	$reasons = array();
	foreach (dimension_exception_reasons() as $reason)
		if (check_value('Reason_'.$reason))
			$reasons[] = $reason;
	// DIM-PERM-001: the customer-based reason needs the sales view area. The queue itself drops it (and never reads it) when
	// `customer_access` is false, so an empty or customer-only selection cannot widen to every reason; the dataset states the omission.
	$customer_access = report_access_aggregate_scope('dimension_customer_economics');
	$enabled = (int)get_company_pref('use_dimension');
	$matrix_axis = (int)get_post('MatrixAxis', 1) === 2 ? 2 : 1;
	$dataset = dimension_export_dataset(array(
		'from' => date2sql($_POST['TransFromDate']), 'to' => date2sql($_POST['TransToDate']), 'scope1' => $scope1, 'scope2' => $scope2,
		'mode' => $_POST['Mode'], 'fiscal_start' => date2sql(begin_fiscalyear()), 'matrix' => check_value('IncludeMatrix'), 'queue' => check_value('IncludeQueue'),
		'axes' => array($matrix_axis), 'queue_axes' => $enabled >= 2 ? array(1, 2) : ($enabled === 1 ? array(1) : array()), 'reasons' => $reasons,
			'customer_access' => !empty($customer_access['allowed'])));
	if ($dataset === false)
		display_error(_('The comparison and exceptions could not be read. Check the period; a year-to-date comparison needs the fiscal year start on or before the end date.'));
	else {
		display_note(_('Posted GL measures in company base currency. The same figures are printed by the Tracking Comparison and Exceptions report (PDF and Excel). Exceptions are listed with their reason, source, age and the permission needed to resolve them; nothing is corrected here.'));
		display_dimension_export_dataset($dataset);
	}
}

end_page();
