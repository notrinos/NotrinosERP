<?php
/**********************************************************************
	Copyright (C) FrontAccounting, LLC.
	Released under the terms of the GNU General Public License, GPL, 
	as published by the Free Software Foundation, either version 3 
	of the License, or (at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
	See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/
$page_security = 'SA_REORDER';

if (@$_GET['page_level'] == 1)
	$path_to_root = '../..';
else	
	$path_to_root = '..';

include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/inventory/includes/inventory_db.inc');

$js = '';
if ($SysPrefs->use_popup_windows && $SysPrefs->use_popup_search)
	$js .= get_js_open_window(900, 500);
page(_($help_context = 'Reorder Levels'), false, false, '', $js);

check_db_has_costable_items(_('There are no inventory items defined in the system (Purchased or manufactured items).'));

//------------------------------------------------------------------------------------

if (isset($_GET['stock_id']))
	$_POST['stock_id'] = $_GET['stock_id'];

if (list_updated('stock_id')) {
	$Ajax->activate('show_heading');
	$Ajax->activate('reorders');
}

//------------------------------------------------------------------------------------

$action = $_SERVER['PHP_SELF'];
if ($page_nested)
	$action .= '?stock_id='.get_post('stock_id');
start_form(false, false, $action);

if (!isset($_POST['stock_id']))
	$_POST['stock_id'] = get_global_stock_item();

if (!$page_nested) {
	echo '<center>'._('Item:').'&nbsp;';
	echo stock_costable_items_list('stock_id', $_POST['stock_id'], false, true);

	echo '<hr></center>';
}
else
	br(2);
div_start('show_heading');
stock_item_heading($_POST['stock_id']);
br();
div_end();

set_global_stock_item($_POST['stock_id']);

div_start('reorders');
start_table(TABLESTYLE);

$th = array(_('Location'), _('Quantity On Hand'), _('Re-Order Level'));
table_header($th);

$j = 1;
$k=0; //row colour counter

$result = get_loc_details($_POST['stock_id']);

while ($myrow = db_fetch($result)) {

	alt_table_row_color($k);

	if (isset($_POST['UpdateData']) && check_num($myrow['loc_code'])) {

		$myrow['reorder_level'] = input_num($myrow['loc_code']);
		set_reorder_level($_POST['stock_id'], $myrow['loc_code'], input_num($myrow['loc_code']));
		display_notification(_('Reorder levels has been updated.'));
	}

	$qoh = get_qoh_on_date($_POST['stock_id'], $myrow['loc_code']);

	label_cell($myrow['location_name']);

	$_POST[$myrow['loc_code']] = qty_format($myrow['reorder_level'], $_POST['stock_id'], $dec);

	qty_cell($qoh, false, $dec);
	qty_cells(null, $myrow['loc_code'], null, null, null, $dec);
	end_row();
	$j++;
	if ($j == 12) {
		$j = 1;
		table_header($th);
	}
}

end_table(1);
div_end();
submit_center('UpdateData', _('Update'), true, false, 'default');

end_form();
end_page();