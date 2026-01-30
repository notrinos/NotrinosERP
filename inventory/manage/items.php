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
$page_security = 'SA_ITEM';
$path_to_root = '../..';
include($path_to_root.'/includes/session.inc');
include($path_to_root.'/reporting/includes/tcpdf.php');

$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 500);
if (user_use_date_picker())
	$js .= get_js_date_picker();

if (isset($_GET['FixedAsset'])) {
	$page_security = 'SA_ASSET';
	$_SESSION['page_title'] = _($help_context = 'Fixed Assets');
	$_POST['mb_flag'] = 'F';
	$_POST['fixed_asset']  = 1;
}
else {
	$_SESSION['page_title'] = _($help_context = 'Items');
	if (!get_post('fixed_asset'))
		$_POST['fixed_asset']  = 0;
}

page($_SESSION['page_title'], @$_REQUEST['popup'], false, '', $js);

include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/includes/ui/attachment.inc');

include_once($path_to_root.'/inventory/includes/inventory_db.inc');
include_once($path_to_root.'/fixed_assets/includes/fixed_assets_db.inc');

$new_item = get_post('stock_id') == '' || get_post('cancel') || get_post('clone');

//------------------------------------------------------------------------------------

function set_edit($stock_id) {
	$_POST = array_merge($_POST, get_item($stock_id));

	$_POST['depreciation_rate'] = number_format2($_POST['depreciation_rate'], 1);
	$_POST['depreciation_factor'] = number_format2($_POST['depreciation_factor'], 1);
	$_POST['depreciation_start'] = sql2date($_POST['depreciation_start']);
	$_POST['depreciation_date'] = sql2date($_POST['depreciation_date']);
	$_POST['del_image'] = 0;
}

function del_image($stock_id) {
	foreach (array('jpg', 'png', 'gif') as $ext) {
		$filename = company_path().'/images/stock_items/'.item_img_name($stock_id).".".$ext;
		if (file_exists($filename) && !unlink($filename))
			return false;
	}
	return true;
}

function show_image($stock_id) {
	global $SysPrefs;

	$check_remove_image = false;
	$stock_img_link = _('No image');

	if (@$stock_id) {
		foreach (array('jpg', 'png', 'gif') as $ext) {
			$file = company_path().'/images/stock_items/'.item_img_name($stock_id).'.'.$ext;
			if (file_exists($file)) {
				// rand() call is necessary here to avoid caching problems.
				$check_remove_image = true; // fixme
				$stock_img_link = "<a target='_blank' href='".$file."?nocache=".rand()."' class='viewlink' onclick = \"javascript:openWindow(this.href,this.target); return false;\">";
				$stock_img_link .= "<img id='item_img' alt = '[".$stock_id.".$ext"."]' src='".$file."?nocache=".rand()."' height='".$SysPrefs->pic_height."' border='0'>";
				$stock_img_link .= "</a>";
				break;
			}
		}
	}
	label_row("&nbsp;", $stock_img_link);
	if ($check_remove_image)
		check_row(_('Delete Image:'), 'del_image');
}

if (isset($_GET['stock_id']))
	$_POST['stock_id'] = $_GET['stock_id'];

$stock_id = get_post('stock_id');
if (list_updated('stock_id')) {
	$_POST['NewStockID'] = get_post('stock_id');
	$stock_id = get_post('stock_id');
	clear_data();
	$Ajax->activate('details');
	$Ajax->activate('controls');
}
if (get_post('cancel')) {
	$_POST['NewStockID'] = '';
	$stock_id = '';
	$_POST['stock_id'] = '';
	clear_data();
	set_focus('stock_id');
	$Ajax->activate('_page_body');
}
if (list_updated('category_id') || list_updated('mb_flag') || list_updated('fa_class_id') || list_updated('depreciation_method'))
	$Ajax->activate('details');

$upload_file = '';
if (isset($_FILES['pic']) && $_FILES['pic']['name'] != '') {
	$stock_id = $_POST['NewStockID'];
	$result = $_FILES['pic']['error'];
	$upload_file = 'Yes'; //Assume all is well to start off with
	$filename = company_path().'/images/stock_items';
	if (!file_exists($filename))
		mkdir($filename);
	
	$filename .= '/'.item_img_name($stock_id).(substr(trim($_FILES['pic']['name']), strrpos($_FILES['pic']['name'], '.')));

	if ($_FILES['pic']['error'] == UPLOAD_ERR_INI_SIZE) {
		display_error(_('The file size is over the maximum allowed.'));
		$upload_file = 'No';
	}
	elseif ($_FILES['pic']['error'] > 0) {
		display_error(_('Error uploading file.'));
		$upload_file = 'No';
	}
	
	//But check for the worst 
	if ((list($width, $height, $type, $attr) = getimagesize($_FILES['pic']['tmp_name'])) !== false)
		$imagetype = $type;
	else
		$imagetype = false;

	if ($imagetype != IMAGETYPE_GIF && $imagetype != IMAGETYPE_JPEG && $imagetype != IMAGETYPE_PNG) {
		display_warning( _('Only graphics files can be uploaded'));
		$upload_file = 'No';
	}
	elseif (!in_array(strtoupper(substr(trim($_FILES['pic']['name']), strlen($_FILES['pic']['name']) - 3)), array('JPG','PNG','GIF'))) {
		display_warning(_('Only graphics files are supported - a file extension of .jpg, .png or .gif is expected'));
		$upload_file = 'No';
	} 
	elseif ( $_FILES['pic']['size'] > ($SysPrefs->max_image_size * 1024)) { //File Size Check
		display_warning(_('The file size is over the maximum allowed. The maximum size allowed in KB is').' '.$SysPrefs->max_image_size);
		$upload_file = 'No';
	} 
	elseif ( $_FILES['pic']['type'] == 'text/plain' ) {  //File type Check
		display_warning( _('Only graphics files can be uploaded'));
		$upload_file = 'No';
	} 
	elseif (!del_image($stock_id)) {
		display_error(_('The existing image could not be removed'));
		$upload_file = 'No';
	}

	if ($upload_file == 'Yes') {
		$result  =  move_uploaded_file($_FILES['pic']['tmp_name'], $filename);
		if ($msg = check_image_file($filename)) {
			display_error($msg);
			unlink($filename);
			$upload_file = 'No';
		}
	}
	$Ajax->activate('details');
}

if (get_post('fixed_asset')) {
	check_db_has_fixed_asset_categories(_('There are no fixed asset categories defined in the system. At least one fixed asset category is required to add a fixed asset.'));
	check_db_has_fixed_asset_classes(_('There are no fixed asset classes defined in the system. At least one fixed asset class is required to add a fixed asset.'));
}
else
	check_db_has_stock_categories(_('There are no item categories defined in the system. At least one item category is required to add a item.'));

check_db_has_item_tax_types(_('There are no item tax types defined in the system. At least one item tax type is required to add a item.'));

function clear_data() {
	unset($_POST['long_description']);
	unset($_POST['description']);
	unset($_POST['category_id']);
	unset($_POST['tax_type_id']);
	unset($_POST['units']);
	unset($_POST['mb_flag']);
	unset($_POST['NewStockID']);
	unset($_POST['dimension_id']);
	unset($_POST['dimension2_id']);
	unset($_POST['no_sale']);
	unset($_POST['no_purchase']);
	unset($_POST['depreciation_method']);
	unset($_POST['depreciation_rate']);
	unset($_POST['depreciation_factor']);
	unset($_POST['depreciation_start']);
}

//------------------------------------------------------------------------------------

if (isset($_POST['addupdate'])) {

	$input_error = 0;
	if ($upload_file == 'No')
		$input_error = 1;
	if (strlen($_POST['description']) == 0) {
		$input_error = 1;
		display_error( _('The item name must be entered.'));
		set_focus('description');
	} 
	elseif (strlen($_POST['NewStockID']) == 0) {
		$input_error = 1;
		display_error( _('The item code cannot be empty'));
		set_focus('NewStockID');
	}
	elseif (strstr($_POST['NewStockID'], ' ') || strstr($_POST['NewStockID'],"'") || strstr($_POST['NewStockID'], '+') || strstr($_POST['NewStockID'], "\"") || strstr($_POST['NewStockID'], '&') || strstr($_POST['NewStockID'], "\t")) {
		$input_error = 1;
		display_error( _('The item code cannot contain any of the following characters -  & + OR a space OR quotes'));
		set_focus('NewStockID');
	}
	elseif ($new_item && db_num_rows(get_item_kit($_POST['NewStockID']))) {
		$input_error = 1;
		display_error( _('This item code is already assigned to stock item or sale kit.'));
		set_focus('NewStockID');
	}
	
	if (get_post('fixed_asset')) {
		if ($_POST['depreciation_rate'] > 100)
			$_POST['depreciation_rate'] = 100;
		elseif ($_POST['depreciation_rate'] < 0)
			$_POST['depreciation_rate'] = 0;
		$move_row = get_fixed_asset_move($_POST['NewStockID'], ST_SUPPRECEIVE);
		if ($move_row && isset($_POST['depreciation_start']) && strtotime($_POST['depreciation_start']) < strtotime($move_row['tran_date']))
			display_warning(_('The depracation cannot start before the fixed asset purchase date'));
	}
	
	if ($input_error != 1) {
		if (check_value('del_image'))
			del_image($_POST['NewStockID']);
		
		if (!$new_item) {
			update_item($_POST['NewStockID'], $_POST['description'],
				$_POST['long_description'], $_POST['category_id'], 
				$_POST['tax_type_id'], get_post('units'),
				get_post('fixed_asset') ? 'F' : get_post('mb_flag'), $_POST['sales_account'],
				$_POST['inventory_account'], $_POST['cogs_account'],
				$_POST['adjustment_account'], $_POST['wip_account'], 
				$_POST['dimension_id'], $_POST['dimension2_id'],
				check_value('no_sale'), check_value('editable'), check_value('no_purchase'),
				get_post('depreciation_method'), input_num('depreciation_rate'), input_num('depreciation_factor'), get_post('depreciation_start', null),
				get_post('fa_class_id'));

			update_record_status($_POST['NewStockID'], $_POST['inactive'], 'stock_master', 'stock_id');
			update_record_status($_POST['NewStockID'], $_POST['inactive'], 'item_codes', 'item_code');
			set_focus('stock_id');
			$Ajax->activate('stock_id'); // in case of status change
			display_notification(_('Item has been updated.'));
		} 
		else {

			add_item($_POST['NewStockID'], $_POST['description'],
				$_POST['long_description'], $_POST['category_id'], $_POST['tax_type_id'],
				$_POST['units'], get_post('fixed_asset') ? 'F' : get_post('mb_flag'), $_POST['sales_account'],
				$_POST['inventory_account'], $_POST['cogs_account'],
				$_POST['adjustment_account'], $_POST['wip_account'], 
				$_POST['dimension_id'], $_POST['dimension2_id'],
				check_value('no_sale'), check_value('editable'), check_value('no_purchase'),
				get_post('depreciation_method'), input_num('depreciation_rate'), input_num('depreciation_factor'), get_post('depreciation_start', null),
				get_post('fa_class_id'));

			display_notification(_('A new item has been added.'));
			$_POST['stock_id'] = '';
			$_POST['NewStockID'] = '';
			$_POST['description'] = '';
			$_POST['long_description'] = '';
			$_POST['no_sale'] = 0;
			$_POST['editable'] = 0;
			$_POST['no_purchase'] = 0;
			set_focus('NewStockID');
		}
		$Ajax->activate('_page_body');
	}
}

if (get_post('clone')) {
	set_edit($_POST['stock_id']); // restores data for disabled inputs too
	unset($_POST['stock_id']);
	$stock_id = '';
	unset($_POST['inactive']);
	set_focus('NewStockID');
	$Ajax->activate('_page_body');
}

//------------------------------------------------------------------------------------

function check_usage($stock_id, $dispmsg=true) {
	$msg = item_in_foreign_codes($stock_id);

	if ($msg != '')	{
		if($dispmsg)
			display_error($msg);
		return false;
	}
	return true;
}

//------------------------------------------------------------------------------------

if (isset($_POST['delete']) && strlen($_POST['delete']) > 1) {

	if (check_usage($_POST['NewStockID'])) {

		$stock_id = $_POST['NewStockID'];
		delete_item($stock_id);
		del_image($stock_id);
		display_notification(_('Selected item has been deleted.'));
		$_POST['stock_id'] = '';
		clear_data();
		set_focus('stock_id');
		$new_item = true;
		$Ajax->activate('_page_body');
	}
}

function item_settings(&$stock_id, $new_item) {
	global $SysPrefs, $path_to_root, $page_nested, $depreciation_methods;

	start_outer_table(TABLESTYLE2);

	table_section(1);

	table_section_title(_('General Settings'));

	if ($new_item) {
		$tmpCodeID=null;
		$post_label = null;
		if (!empty($SysPrefs->prefs['barcodes_on_stock'])) {
			$post_label = '<button class="ajaxsubmit" type="submit" aspect=\'default\'  name="generateBarcode"  id="generateBarcode" value="Generate Barcode EAN8"> '._('Generate EAN-8 Barcode').' </button>';
			if (isset($_POST['generateBarcode'])) {
				$tmpCodeID=generateBarcode();
				$_POST['NewStockID'] = $tmpCodeID;
			}
		}	
		text_row(_('Item Code:'), 'NewStockID', $tmpCodeID, 21, 20, null, '', $post_label);
		$_POST['inactive'] = 0;
	} 
	else { // Must be modifying an existing item
		if (get_post('NewStockID') != get_post('stock_id') || get_post('addupdate')) { // first item display

			$_POST['NewStockID'] = $_POST['stock_id'];
			set_edit($_POST['stock_id']);
		}
		label_row(_('Item Code:'),$_POST['NewStockID']);
		hidden('NewStockID', $_POST['NewStockID']);
		set_focus('description');
	}
	$fixed_asset = get_post('fixed_asset');

	text_row(_('Name:'), 'description', null, 52, 200);

	textarea_row(_('Description:'), 'long_description', null, 42, 3);

	stock_categories_list_row(_('Category:'), 'category_id', null, false, $new_item, $fixed_asset);

	if ($new_item && (list_updated('category_id') || !isset($_POST['sales_account']))) { // changed category for new item or first page view

		$category_record = get_item_category($_POST['category_id']);

		$_POST['tax_type_id'] = $category_record['dflt_tax_type'];
		$_POST['units'] = $category_record['dflt_units'];
		$_POST['mb_flag'] = $category_record['dflt_mb_flag'];
		$_POST['inventory_account'] = $category_record['dflt_inventory_act'];
		$_POST['cogs_account'] = $category_record['dflt_cogs_act'];
		$_POST['sales_account'] = $category_record['dflt_sales_act'];
		$_POST['adjustment_account'] = $category_record['dflt_adjustment_act'];
		$_POST['wip_account'] = $category_record['dflt_wip_act'];
		$_POST['dimension_id'] = $category_record['dflt_dim1'];
		$_POST['dimension2_id'] = $category_record['dflt_dim2'];
		$_POST['no_sale'] = $category_record['dflt_no_sale'];
		$_POST['no_purchase'] = $category_record['dflt_no_purchase'];
		$_POST['editable'] = 0;
	}
	$fresh_item = !isset($_POST['NewStockID']) || $new_item || check_usage($_POST['stock_id'], false);

	// show inactive item tax type in selector only if already set.
	item_tax_types_list_row(_('Item Tax Type:'), 'tax_type_id', null, !$new_item && item_type_inactive(get_post('tax_type_id')));

	if (!get_post('fixed_asset'))
		stock_item_types_list_row(_('Item Type:'), 'mb_flag', null, $fresh_item);

	stock_units_list_row(_('Units of Measure:'), 'units', null, $fresh_item);

	if (!get_post('fixed_asset')) {
		check_row(_('Editable description:'), 'editable');
		check_row(_('Exclude from sales:'), 'no_sale');
		check_row(_('Exclude from purchases:'), 'no_purchase');
	}

	if (get_post('fixed_asset')) {
		table_section_title(_('Depreciation'));

		fixed_asset_classes_list_row(_('Fixed Asset Class').':', 'fa_class_id', null, false, true);

		array_selector_row(_('Depreciation Method').':', 'depreciation_method', null, $depreciation_methods, array('select_submit'=> true));

		if (!isset($_POST['depreciation_rate']) || (list_updated('fa_class_id') || list_updated('depreciation_method'))) {
			$class_row = get_fixed_asset_class($_POST['fa_class_id']);
			$_POST['depreciation_rate'] = get_post('depreciation_method') == 'N' ? ceil(100/$class_row['depreciation_rate']) : $class_row['depreciation_rate'];
		}

		if ($_POST['depreciation_method'] == 'O') {
			hidden('depreciation_rate', 100);
			label_row(_('Depreciation Rate').':', '100 %');
		}
		elseif ($_POST['depreciation_method'] == 'N')
			small_amount_row(_('Depreciation Years').':', 'depreciation_rate', null, null, _('years'), 0);
		elseif ($_POST['depreciation_method'] == 'D')
			small_amount_row(_('Base Rate').':', 'depreciation_rate', null, null, '%', user_percent_dec());
		else
			small_amount_row(_('Depreciation Rate').':', 'depreciation_rate', null, null, '%', user_percent_dec());

		if ($_POST['depreciation_method'] == 'D')
			small_amount_row(_('Rate multiplier').':', 'depreciation_factor', null, null, '', 2);

		// do not allow to change the depreciation start after this item has been depreciated
		if ($new_item || $_POST['depreciation_start'] == $_POST['depreciation_date'])
			date_row(_('Depreciation Start').':', 'depreciation_start', null, null, 1 - date('j'));
		else {
			hidden('depreciation_start');
			label_row(_('Depreciation Start').':', $_POST['depreciation_start']);
			label_row(_('Last Depreciation').':', $_POST['depreciation_date']==$_POST['depreciation_start'] ? _('None') :  $_POST['depreciation_date']);
		}
		hidden('depreciation_date');
	}

	$dim = get_company_pref('use_dimension');
	if ($dim >= 1) {
		table_section_title(_('Dimensions'));

		dimensions_list_row(_('Dimension').' 1', 'dimension_id', null, true, ' ', false, 1);
		if ($dim > 1)
			dimensions_list_row(_('Dimension').' 2', 'dimension2_id', null, true, ' ', false, 2);
	}
	if ($dim < 1)
		hidden('dimension_id', 0);
	if ($dim < 2)
		hidden('dimension2_id', 0);

	table_section(2);

	table_section_title(_('GL Accounts'));

	gl_all_accounts_list_row(_('Sales Account:'), 'sales_account', $_POST['sales_account']);

	if (get_post('fixed_asset')) {
		gl_all_accounts_list_row(_('Asset account:'), 'inventory_account', $_POST['inventory_account']);
		gl_all_accounts_list_row(_('Depreciation cost account:'), 'cogs_account', $_POST['cogs_account']);
		gl_all_accounts_list_row(_('Depreciation/Disposal account:'), 'adjustment_account', $_POST['adjustment_account']);
	}
	elseif (!is_service(get_post('mb_flag'))) {
		gl_all_accounts_list_row(_('Inventory Account:'), 'inventory_account', $_POST['inventory_account']);
		gl_all_accounts_list_row(_('C.O.G.S. Account:'), 'cogs_account', $_POST['cogs_account']);
		gl_all_accounts_list_row(_('Inventory Adjustments Account:'), 'adjustment_account', $_POST['adjustment_account']);
	}
	else {
		gl_all_accounts_list_row(_('C.O.G.S. Account:'), 'cogs_account', $_POST['cogs_account']);
		hidden('inventory_account', $_POST['inventory_account']);
		hidden('adjustment_account', $_POST['adjustment_account']);
	}

	if (is_manufactured(get_post('mb_flag')))
		gl_all_accounts_list_row(_('WIP Account:'), 'wip_account', $_POST['wip_account']);
	else
		hidden('wip_account', $_POST['wip_account']);

	table_section_title(_('Other'));

	file_row(_('Image File (.jpg)').':', 'pic', 'pic'); // fixme: png/gif

	show_image(@$_POST['NewStockID']);

	record_status_list_row(_('Item status:'), 'inactive');
	if (get_post('fixed_asset')) {
		table_section_title(_('Values'));
		if (!$new_item) {
			hidden('material_cost');
			hidden('purchase_cost');
			label_row(_('Initial Value').':', price_format($_POST['purchase_cost']), '', "align='right'");
			label_row(_('Depreciations').':', price_format($_POST['purchase_cost'] - $_POST['material_cost']), '', "align='right'");
			label_row(_('Current Value').':', price_format($_POST['material_cost']), '', "align='right'");
		}
	}
	end_outer_table(1);

	div_start('controls');
	if (@$_REQUEST['popup'])
		hidden('popup', 1);
	if (!isset($_POST['NewStockID']) || $new_item)
		submit_center('addupdate', _('Insert New Item'), true, '', 'default');
	else {
		submit_center_first('addupdate', _('Update Item'), '', $page_nested ? true : 'default');
		submit_return('select', get_post('stock_id'), _('Select this items and return to document entry.'));
		submit('clone', _('Clone This Item'), true, '', true);
		submit('delete', _('Delete This Item'), true, '', true);
		submit_center_last('cancel', _('Cancel'), _('Cancel Edition'), 'cancel');
	}

	div_end();
}

//-------------------------------------------------------------------------------------------- 

start_form(true);

if (db_has_stock_items()) {
	start_table(TABLESTYLE_NOBORDER);
	start_row();
	stock_items_list_cells(_('Select an item:'), 'stock_id', null, _('New item'), true, check_value('show_inactive'), false, array('fixed_asset' => get_post('fixed_asset'), 'search_submit'=>true));
	$new_item = get_post('stock_id') == '';
	check_cells(_('Show inactive:'), 'show_inactive', null, true);
	end_row();
	end_table();

	if (get_post('_show_inactive_update')) {
		$Ajax->activate('stock_id');
		set_focus('stock_id');
	}
}
else
	hidden('stock_id', get_post('stock_id'));

div_start('details');

$stock_id = get_post('stock_id');
if (!$stock_id)
	unset($_POST['_tabs_sel']); // force settings tab for new customer

$tabs = (get_post('fixed_asset'))
	? array(
		'settings' => array(_('&General settings'), $stock_id),
		'movement' => array(_('&Transactions'), $stock_id),
		'attachments' => array(_('Attachments'), (user_check_access('SA_ATTACHDOCUMENT') ? get_item_code_id($stock_id) : null)))
	: array(
		'settings' => array(_('&General settings'), $stock_id),
		'sales_pricing' => array(_('S&ales Pricing'), (user_check_access('SA_SALESPRICE') ? $stock_id : null)),
		'purchase_pricing' => array(_('&Purchasing Pricing'), (user_check_access('SA_PURCHASEPRICING') ? $stock_id : null)),
		'standard_cost' => array(_('Standard &Costs'), (user_check_access('SA_STANDARDCOST') ? $stock_id : null)),
		'reorder_level' => array(_('&Reorder Levels'), (is_inventory_item($stock_id) && user_check_access('SA_REORDER') ? $stock_id : null)),
		'movement' => array(_('&Transactions'), (user_check_access('SA_ITEMSTRANSVIEW') && is_inventory_item($stock_id) ? $stock_id : null)),
		'status' => array(_('&Status'), (user_check_access('SA_ITEMSSTATVIEW') ? $stock_id : null)),
		'attachments' => array(_('Attachments'), (user_check_access('SA_ATTACHDOCUMENT') ? get_item_code_id($stock_id) : null)),
	);

tabbed_content_start('tabs', $tabs);

switch (get_post('_tabs_sel')) {
	default:
	case 'settings':
		item_settings($stock_id, $new_item);
		break;
	case 'sales_pricing':
		$_GET['stock_id'] = $stock_id;
		$_GET['page_level'] = 1;
		include_once($path_to_root.'/inventory/prices.php');
		break;
	case 'purchase_pricing':
		$_GET['stock_id'] = $stock_id;
		$_GET['page_level'] = 1;
		include_once($path_to_root.'/inventory/purchasing_data.php');
		break;
	case 'standard_cost':
		$_GET['stock_id'] = $stock_id;
		$_GET['page_level'] = 1;
		include_once($path_to_root.'/inventory/cost_update.php');
		break;
	case 'reorder_level':
		if (!is_inventory_item($stock_id))
			break;
		$_GET['page_level'] = 1;
		$_GET['stock_id'] = $stock_id;
		include_once($path_to_root.'/inventory/reorder_level.php');
		break;
	case 'movement':
		if (!is_inventory_item($stock_id))
			break;
		$_GET['stock_id'] = $stock_id;
		include_once($path_to_root.'/inventory/inquiry/stock_movements.php');
		break;
	case 'status':
		$_GET['stock_id'] = $stock_id;
		include_once($path_to_root.'/inventory/inquiry/stock_status.php');
		break;
	case 'attachments':
		$id = get_item_code_id($stock_id);
		$_GET['trans_no'] = $id;
		$_GET['type_no'] = get_post('fixed_asset') ? ST_FIXEDASSET : ST_ITEM;
		$attachments = new attachments('attachment', $id, 'items');
		$attachments->show();
};

br();
tabbed_content_end();

div_end();

hidden('fixed_asset', get_post('fixed_asset'));

if (get_post('fixed_asset'))
	hidden('mb_flag', 'F');

end_form();

end_page(@$_REQUEST['popup']);

function generateBarcode() {
	$tmpBarcodeID = '';
	$tmpCountTrys = 0;
	while ($tmpBarcodeID == '')	{
		srand ((int) microtime( )*1000000);
		$random_1  = rand(1,9);
		$random_2  = rand(0,9);
		$random_3  = rand(0,9);
		$random_4  = rand(0,9);
		$random_5  = rand(0,9);
		$random_6  = rand(0,9);
		$random_7  = rand(0,9);
		//$random_8  = rand(0,9);

		// http://stackoverflow.com/questions/1136642/ean-8-how-to-calculate-checksum-digit
		$sum1 = $random_2 + $random_4 + $random_6;
		$sum2 = 3 * ($random_1  + $random_3  + $random_5  + $random_7 );
		$checksum_value = $sum1 + $sum2;

		$checksum_digit = 10 - ($checksum_value % 10);
		if ($checksum_digit == 10)
			$checksum_digit = 0;

		$random_8 = $checksum_digit;

		$tmpBarcodeID = $random_1.$random_2.$random_3.$random_4.$random_5.$random_6.$random_7.$random_8;

		// LETS CHECK TO SEE IF THIS NUMBER HAS EVER BEEN USED
		$query = "SELECT stock_id FROM ".TB_PREF."stock_master WHERE stock_id = '".$tmpBarcodeID."'";
		$arr_stock = db_fetch(db_query($query, 'could not get stock_id'));
		
		if (!$arr_stock || !$arr_stock['stock_id'])
			return $tmpBarcodeID;
		
		$tmpBarcodeID = '';	 
	}
}
