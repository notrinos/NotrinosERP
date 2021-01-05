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
$page_security = 'SA_STANDARDCOST';

if (@$_GET['page_level'] == 1)
	$path_to_root = '../..';
else	
	$path_to_root = '..';

include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/includes/banking.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/inventory/includes/inventory_db.inc');
include_once($path_to_root.'/includes/ui/items_cart.inc');

$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 500);

if (isset($_GET['FixedAsset'])) {
	$_SESSION['page_title'] = _($help_context = 'FA Revaluation');
	$_POST['fixed_asset'] = 1;
}
else
	$_SESSION['page_title'] = _($help_context = 'Inventory Item Cost Update');

page($_SESSION['page_title'], false, false, '', $js);

//--------------------------------------------------------------------------------------

if (get_post('fixed_asset') == 1)
	check_db_has_disposable_fixed_assets(_('There are no fixed assets defined in the system.'));
else
	check_db_has_costable_items(_('There are no costable inventory items defined in the system (Purchased or manufactured items).'));

if (isset($_GET['stock_id']))
	$_POST['stock_id'] = $_GET['stock_id'];

//--------------------------------------------------------------------------------------

$should_update = false;
if (isset($_POST['UpdateData'])) {
	$old_cost = get_unit_cost($_POST['stock_id']);

	$new_cost = input_num('material_cost') + input_num('labour_cost')
		 + input_num('overhead_cost');

	$should_update = true;

	if (!check_num('material_cost') || !check_num('labour_cost') || !check_num('overhead_cost')) {
		display_error( _('The entered cost is not numeric.'));
		set_focus('material_cost');
		$should_update = false;
	}
	elseif ($old_cost == $new_cost) {
		display_error( _('The new cost is the same as the old cost. Cost was not updated.'));
		$should_update = false;
	}

	if ($should_update) {
		$update_no = stock_cost_update($_POST['stock_id'], input_num('material_cost'), input_num('labour_cost'), input_num('overhead_cost'), $old_cost, $_POST['refline'], $_POST['memo_']);

		display_notification(_('Cost has been updated.'));

		if ($update_no > 0)
			display_notification(get_gl_view_str(ST_COSTUPDATE, $update_no, _('View the GL Journal Entries for this Cost Update')));
	}
}

if (list_updated('stock_id') || $should_update) {
	unset($_POST['memo_']);
	$Ajax->activate('cost_table');
}

//-----------------------------------------------------------------------------------------

$action = $_SERVER['PHP_SELF'];
if ($page_nested)
	$action .= '?stock_id='.get_post('stock_id');
start_form(false, false, $action);

hidden('fixed_asset');

if (!isset($_POST['stock_id']))
	$_POST['stock_id'] = get_global_stock_item();

if (!$page_nested) {
	echo '<center>' . _('Item:'). '&nbsp;';
	if (get_post('fixed_asset') == 1)
		echo stock_disposable_fa_list('stock_id', $_POST['stock_id'], false, true);
	else
		echo stock_items_list('stock_id', $_POST['stock_id'], false, true);

	echo '</center><hr>';
}
else
	br(2);

set_global_stock_item($_POST['stock_id']);

$myrow = get_item($_POST['stock_id']);

div_start('cost_table');

start_table(TABLESTYLE2);
$dec1 = 0;
$dec2 = 0;
$dec3 = 0;
if ($myrow) {
	$_POST['material_cost'] = price_decimal_format($myrow['material_cost'], $dec1);
	$_POST['labour_cost'] = price_decimal_format($myrow['labour_cost'], $dec2);
	$_POST['overhead_cost'] = price_decimal_format($myrow['overhead_cost'], $dec3);
}

amount_row(_('Unit cost'), 'material_cost', null, "class='tableheader2'", null, $dec1);

if ($myrow && $myrow['mb_flag']=='M') {
	amount_row(_('Standard Labour Cost Per Unit'), 'labour_cost', null, "class='tableheader2'", null, $dec2);
	amount_row(_('Standard Overhead Cost Per Unit'), 'overhead_cost', null, "class='tableheader2'", null, $dec3);
}
else {
	hidden('labour_cost', 0);
	hidden('overhead_cost', 0);
}
refline_list_row(_('Reference line:'), 'refline', ST_COSTUPDATE, null, false, get_post('fixed_asset'));
textarea_row(_('Memo'), 'memo_', null, 40, 4);

end_table(1);
div_end();
submit_center('UpdateData', _('Update'), true, false, 'default');

end_form();
end_page();