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
 * DIM-REP-003: Financial Tracking workspace.
 *
 * One read-only page that shows revenue, costs, result, provenance and coverage of a Dimension scope for a period and
 * for its lifetime, from the shared tracking reader. It needs the Dimension view area from the page security and, through
 * the reviewed `dimension_tracking_workspace` aggregate, the GL analytics area as well; a denied user reads no data.
 */
$page_security = 'SA_DIMTRANSVIEW';
$path_to_root = '../..';

include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/reporting/includes/report_access_scope.inc');
include_once($path_to_root.'/dimensions/includes/dimensions_db.inc');
include_once($path_to_root.'/dimensions/includes/dimensions_ui.inc');
include_once($path_to_root.'/dimensions/includes/dimension_tracking_workspace.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();
page(_($help_context = 'Financial Tracking'), false, false, '', $js);

//-----------------------------------------------------------------------------------

$access = report_access_aggregate_scope('dimension_tracking_workspace');
if (empty($access['allowed'])) {
	report_access_render_aggregate_state($access);
	end_page();
	exit;
}

foreach (array('TransFromDate', 'TransToDate', 'Dimension', 'Dimension2') as $field) {
	if (isset($_GET[$field]))
		$_POST[$field] = $_GET[$field];
}
if (!isset($_POST['TransFromDate']))
	$_POST['TransFromDate'] = begin_fiscalyear();
if (!isset($_POST['TransToDate']))
	$_POST['TransToDate'] = Today();

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();
date_cells(_('From:'), 'TransFromDate');
date_cells(_('To:'), 'TransToDate');
dimensions_history_list_cells(_('Dimension').' 1:', 'Dimension', null, true, _('All Dimensions'));
check_cells(_('Unassigned only:'), 'Unassigned1', null, false, _('Show only postings with no Dimension 1'));
dimensions_history_list_cells(_('Dimension').' 2:', 'Dimension2', null, true, _('All Dimensions'));
check_cells(_('Unassigned only:'), 'Unassigned2', null, false, _('Show only postings with no Dimension 2'));
submit_cells('Show', _('Show'), '', '', 'default');
end_row();
end_table();
end_form();

//-----------------------------------------------------------------------------------

if (!is_date($_POST['TransFromDate']) || !is_date($_POST['TransToDate']) || date1_greater_date2($_POST['TransFromDate'], $_POST['TransToDate'])) {
	display_error(_('The tracking period is not valid.'));
}
else {
	$scope1 = check_value('Unassigned1') ? -1 : (int)get_post('Dimension', 0);
	$scope2 = check_value('Unassigned2') ? -1 : (int)get_post('Dimension2', 0);
	$from = date2sql($_POST['TransFromDate']);
	$to = date2sql($_POST['TransToDate']);
	$data = dimension_tracking_workspace($from, $to, $scope1, $scope2);
	$drill = dimension_tracking_account_drilldown($from, $to, $scope1, $scope2);
	if ($data === false || $drill === false)
		display_error(_('The tracking workspace could not be read.'));
	else
		display_dimension_tracking_workspace($data, $drill, $_POST['TransFromDate'], $_POST['TransToDate']);
}

end_page();
