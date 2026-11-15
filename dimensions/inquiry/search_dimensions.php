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

include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/reporting/includes/reporting.inc');
include_once($path_to_root.'/reporting/includes/report_access_scope.inc');
include_once($path_to_root.'/dimensions/includes/dimensions_db.inc');
include_once($path_to_root.'/dimensions/includes/dimensions_ui.inc');
include_once($path_to_root.'/dimensions/includes/dimension_inquiry.inc');
$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(800, 500);
if (user_use_date_picker())
	$js .= get_js_date_picker();

if (isset($_GET['outstanding_only']) && $_GET['outstanding_only']) {
	$outstanding_only = 1;
	page(_($help_context = 'Search Outstanding Dimensions'), false, false, '', $js);
}
else {
	$outstanding_only = 0;
	page(_($help_context = 'Search Dimensions'), false, false, '', $js);
}

//-----------------------------------------------------------------------------------
// DIM-REP-004: the master start date and the activity period are two different filters, the reference text is combined
// with every other filter, and the result is one bounded page with grouped performance columns.
//
if (get_post('SearchOrders') || get_post('dim_prev') || get_post('dim_next'))
	$Ajax->activate('dim_table');

if (isset($_GET['stock_id']))
	$_POST['SelectedStockItem'] = $_GET['stock_id'];

if (!isset($_POST['dim_page']) || !preg_match('/^[0-9]{1,9}$/D', (string)$_POST['dim_page']))
	$_POST['dim_page'] = 1;
if (get_post('SearchOrders'))
	$_POST['dim_page'] = 1;
elseif (get_post('dim_prev'))
	$_POST['dim_page'] = max(1, (int)$_POST['dim_page'] - 1);
elseif (get_post('dim_next'))
	$_POST['dim_page'] = (int)$_POST['dim_page'] + 1;

start_form(false, $_SERVER['PHP_SELF'].'?outstanding_only='.$outstanding_only);

start_table(TABLESTYLE_NOBORDER);
start_row();

text_cells(_('Reference or name:'), 'OrderNumber', null, 20, dimension_inquiry_name_limit());
number_list_cells(_('Type'), 'type_', null, 1, 2, _('All'));
date_cells(_('Started from:'), 'StartFrom', _('Filters by the date the dimension was created, not by its postings'), null, 0, 0, 1001);
date_cells(_('Started to:'), 'StartTo', '', null, 0, 0, 1001);
date_cells(_('Active from:'), 'ActiveFrom', _('Filters by postings dated in this period, whatever the start date of the dimension'), null, 0, 0, 1001);
date_cells(_('Active to:'), 'ActiveTo', '', null, 0, 0, 1001);
check_cells(_('Only Overdue:'), 'OverdueOnly', null);
if (!$outstanding_only)
	check_cells(_('Only Open:'), 'OpenOnly', null);
else
	$_POST['OpenOnly'] = 1;
submit_cells('SearchOrders', _('Apply Filter'), '', '', 'default');

end_row();
end_table();

foreach (array('StartFrom', 'StartTo', 'ActiveFrom', 'ActiveTo') as $date_field) {
	if (isset($_POST[$date_field]) && $_POST[$date_field] !== '' && !is_date($_POST[$date_field])) {
		display_error(_('A date filter is not a valid date.'));
		$_POST[$date_field] = '';
	}
}
$filter_input = array(
	'reference' => isset($_POST['OrderNumber']) ? $_POST['OrderNumber'] : '',
	'type' => isset($_POST['type_']) ? $_POST['type_'] : 0,
	'status' => check_value('OpenOnly') ? 'open' : 'all',
	'overdue' => check_value('OverdueOnly'),
	'start_from' => (isset($_POST['StartFrom']) && $_POST['StartFrom'] !== '') ? date2sql($_POST['StartFrom']) : '',
	'start_to' => (isset($_POST['StartTo']) && $_POST['StartTo'] !== '') ? date2sql($_POST['StartTo']) : '',
	'active_from' => (isset($_POST['ActiveFrom']) && $_POST['ActiveFrom'] !== '') ? date2sql($_POST['ActiveFrom']) : '',
	'active_to' => (isset($_POST['ActiveTo']) && $_POST['ActiveTo'] !== '') ? date2sql($_POST['ActiveTo']) : '',
	'today' => date2sql(Today()),
	'sort' => 'reference',
	'page' => $_POST['dim_page']);
$normalized = dimension_inquiry_normalize_filters($filter_input);

div_start('dim_table');
if (!$normalized['ok']) {
	foreach ($normalized['errors'] as $reason)
		display_error(dimension_inquiry_reason_text($reason));
}
else {
	$performance = report_access_aggregate_scope('dimension_inquiry_performance');
	$show_measures = !empty($performance['allowed']);
	$search = dimension_inquiry_search($normalized['filters'], $show_measures);
	if ($search === false)
		display_error(_('The dimensions could not be searched.'));
	else {
		$_POST['dim_page'] = $search['page'];
		hidden('dim_page', $search['page']);
		display_dimension_inquiry_results($search, $show_measures, $normalized['filters']['today']);
		if ($search['pages'] > 1) {
			start_table(TABLESTYLE_NOBORDER);
			start_row();
			if ($search['page'] > 1)
				submit_cells('dim_prev', _('Previous page'), '', '', true);
			label_cell(sprintf(_('Page %d of %d'), $search['page'], $search['pages']));
			if ($search['page'] < $search['pages'])
				submit_cells('dim_next', _('Next page'), '', '', true);
			end_row();
			end_table();
		}
		if (!$show_measures)
			display_note(_('Revenue, costs and result are not shown: they need both the Dimension view and the GL analytics permissions.'));
	}
}
div_end();

end_form();
end_page();
