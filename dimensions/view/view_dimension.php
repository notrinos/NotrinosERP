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
$page_security = 'SA_DIMTRANSVIEW';
$path_to_root = '../..';

include_once($path_to_root.'/includes/session.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();
page(_($help_context = 'View Dimension'), true, false, '', $js);

include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');

include_once($path_to_root.'/dimensions/includes/dimensions_db.inc');
include_once($path_to_root.'/dimensions/includes/dimensions_ui.inc');
include_once($path_to_root.'/reporting/includes/report_access_scope.inc');
include_once($path_to_root.'/dimensions/includes/dimension_tracking_workspace.inc');
include_once($path_to_root.'/dimensions/includes/dimension_category_policy.inc');
include_once($path_to_root.'/dimensions/includes/dimension_business_mode.inc');

//-------------------------------------------------------------------------------------------------

if (isset($_GET['trans_no']) && $_GET['trans_no'] != '')
	$id = $_GET['trans_no'];

if (isset($_POST['Show'])) {
	$id = $_POST['trans_no'];
	$Ajax->activate('_page_body');
}

display_heading($systypes_array[ST_DIMENSION] . ' # ' . $id);

br(1);
$myrow = get_dimension($id, true);

if (is_array($myrow)) {
	start_table(TABLESTYLE);

	// DIM-UX-001: the date column carries the caption of the category (a deadline, an end date or only a review date).
	$th = array(_('#'), _('Reference'), _('Name'), _('Type'), _('Date'), dimension_business_mode_deadline_caption(array((int)$myrow['type_'])));
	table_header($th);

	start_row();
	label_cell($myrow['id']);
	label_cell($myrow['reference']);
	label_cell($myrow['name']);
	label_cell(dimension_business_mode_axis_text((int)$myrow['type_']));
	label_cell(sql2date($myrow['date_']));
	label_cell(sql2date($myrow['due_date']));
	end_row();

	comments_display_row(ST_DIMENSION, $id);

	end_table();
}

// DIM-LFC-002: a deleted master keeps its balance/history viewable by the original stored ID.
display_dimension_history_state($myrow, $id);

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();

if (!isset($_POST['TransFromDate']))
	$_POST['TransFromDate'] = begin_fiscalyear();
if (!isset($_POST['TransToDate']))
	$_POST['TransToDate'] = Today();
date_cells(_('from:'), 'TransFromDate');
date_cells(_('to:'), 'TransToDate');
submit_cells('Show',_('Show'), '', false, 'default');

end_row();

end_table();
hidden('trans_no', $id);
end_form();

display_dimension_balance($id, $_POST['TransFromDate'], $_POST['TransToDate']);

// DIM-REP-003: period and lifetime P&L tracking of this exact Dimension from the shared reader. The balance above
// is a net GL balance across every account; this block separates revenue, costs and result and states its coverage.
$tracking_access = report_access_aggregate_scope('dimension_tracking_workspace');
if (empty($tracking_access['allowed'])) {
	report_access_render_aggregate_state($tracking_access);
}
elseif (!is_array($myrow)) {
	display_note(_('The category of a deleted Dimension is unknown, so its P&L tracking is not shown.'));
}
elseif (is_date($_POST['TransFromDate']) && is_date($_POST['TransToDate']) && !date1_greater_date2($_POST['TransFromDate'], $_POST['TransToDate'])) {
	$tracking_from = date2sql($_POST['TransFromDate']);
	$tracking_to = date2sql($_POST['TransToDate']);
	$tracking_scope1 = (int)$myrow['type_'] === 1 ? (int)$id : 0;
	$tracking_scope2 = (int)$myrow['type_'] === 2 ? (int)$id : 0;
	$tracking_period = dimension_tracking_summary($tracking_from, $tracking_to, $tracking_scope1, $tracking_scope2);
	$tracking_lifetime = dimension_tracking_summary(null, $tracking_to, $tracking_scope1, $tracking_scope2);
	if ($tracking_period === false || $tracking_lifetime === false)
		display_error(_('The tracking measures could not be read.'));
	else {
		br(1);
		display_heading2(_('Financial tracking'));
		display_note(_('Posted GL measures in company base currency. These are GL classification measures, not customer attribution or fully allocated profit.'));
		display_dimension_tracking_summary($tracking_period, sprintf(_('Period %s - %s'), $_POST['TransFromDate'], $_POST['TransToDate']));
		display_dimension_tracking_summary($tracking_lifetime, sprintf(_('Lifetime to %s'), $_POST['TransToDate']));
		hyperlink_params($path_to_root.'/dimensions/inquiry/dimension_tracking.php', _('Open the Financial Tracking workspace for this Dimension'),
			'TransFromDate='.urlencode($_POST['TransFromDate']).'&TransToDate='.urlencode($_POST['TransToDate']).'&Dimension'.((int)$myrow['type_'] === 2 ? '2' : '').'='.(int)$id);
		// DIM-REP-005: the customer split of the same scope; its own page checks the customer economics aggregate.
		hyperlink_params($path_to_root.'/dimensions/inquiry/customer_economics.php', _('Open the customer economics for this Dimension'),
			'TransFromDate='.urlencode($_POST['TransFromDate']).'&TransToDate='.urlencode($_POST['TransToDate']).'&Dimension'.((int)$myrow['type_'] === 2 ? '2' : '').'='.(int)$id);
	}
}

// DIM-UX-001: the edit action is offered only to a user who holds the Dimension master authority; an inquiry-only user sees none.
if (is_array($myrow) && dimension_business_mode_can_edit())
	hyperlink_params($path_to_root.'/dimensions/dimension_entry.php', _('Edit this dimension'), 'trans_no='.(int)$id);

br(1);

end_page(true, false, false, ST_DIMENSION, $id);

