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
$path_to_root = '..';
$page_security = 'SA_ATTACHDOCUMENT';

include_once($path_to_root.'/includes/db_pager.inc');
include_once($path_to_root.'/includes/session.inc');

include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/admin/db/attachments_db.inc');
include_once($path_to_root.'/admin/db/transactions_db.inc');
include_once($path_to_root.'/inventory/includes/db/items_db.inc');
include_once($path_to_root.'/includes/attachment_security.inc');

// Authorize before any attachment lookup or binary response.
check_page_security($page_security);

if (isset($_GET['vw']))
	$view_id = $_GET['vw'];
else
	$view_id = find_submit('view');

if ($view_id != -1) {

	$row = get_attachment($view_id);

	if ($row['filename'] != '') {
		if(in_ajax())
			$Ajax->popup($_SERVER['PHP_SELF'].'?vw='.$view_id);
		else {
			stream_general_attachment_file($row, true);
		}
	}	
}
if (isset($_GET['dl']))
	$download_id = $_GET['dl'];
else
	$download_id = find_submit('download');

if ($download_id != -1) {
	$row = get_attachment($download_id);
	if ($row['filename'] != '') {
		if(in_ajax())
			$Ajax->redirect($_SERVER['PHP_SELF'].'?dl='.$download_id);
		else {
			stream_general_attachment_file($row);
		}
	}	
}

$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(800, 500);
page(_($help_context = 'Attach Documents'), false, false, '', $js);

simple_page_mode(true);

//----------------------------------------------------------------------------------------

if (isset($_GET['filterType'])) // catch up external links
	$_POST['filterType'] = $_GET['filterType'];
if (isset($_GET['trans_no']))
	$_POST['trans_no'] = $_GET['trans_no'];

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    $has_upload = isset($_FILES['filename'])
        && (!isset($_FILES['filename']['error']) || $_FILES['filename']['error'] !== UPLOAD_ERR_NO_FILE);
    $validated_upload = false;
    $upload_error = '';
    if (($_POST['filterType'] == ST_ITEM || $_POST['filterType'] == ST_FIXEDASSET) && $Mode == 'ADD_ITEM')
        $_POST['trans_no'] = get_item_code_id($_POST['trans_no']);
    if (!transaction_exists($_POST['filterType'], $_POST['trans_no']) || !ctype_digit($_POST['trans_no']))
        $upload_error = _('Selected transaction does not exists.');
    elseif ($Mode == 'ADD_ITEM' && !$has_upload)
        $upload_error = _('Select attachment file.');
    elseif ($has_upload) {
        $validated_upload = validate_general_attachment_upload($_FILES['filename']);
        if (!$validated_upload['ok']) $upload_error = $validated_upload['error'];
    }
    $unique_name = '';
    if ($upload_error === '' && $has_upload) {
        if ($Mode == 'UPDATE_ITEM') {
            $row = get_attachment($selected_id);
            if (!$row || $row['filename'] === '') $upload_error = _('Attachment not found.');
            else $unique_name = $row['unique_name'];
        }
        if ($upload_error === '') {
            $unique_name = store_general_attachment_upload($validated_upload, $unique_name);
            if ($unique_name === false) $upload_error = _('The attachment could not be stored.');
        }
    }
    if ($upload_error !== '') display_error($upload_error);
    else {
        if ($Mode == 'ADD_ITEM') {
            add_attachment($_POST['filterType'], $_POST['trans_no'], $_POST['description'],
                $validated_upload['filename'], $unique_name, $validated_upload['size'], $validated_upload['mime']);
            display_notification(_('Attachment has been inserted.'));
        } else {
            update_attachment($selected_id, $_POST['filterType'], $_POST['trans_no'], $_POST['description'],
                $has_upload ? $validated_upload['filename'] : '', $unique_name,
                $has_upload ? $validated_upload['size'] : 0, $has_upload ? $validated_upload['mime'] : '');
            display_notification(_('Attachment has been updated.'));
        }
        reset_form();
    }
    refresh_pager('trans_tbl');
    $Ajax->activate('_page_body');
}

if ($Mode == 'Delete') {
	$row = get_attachment($selected_id);
	$dir =  company_path().'/attachments';
	if (file_exists($dir.'/'.$row['unique_name']))
		unlink($dir.'/'.$row['unique_name']);
	delete_attachment($selected_id);	
	display_notification(_('Attachment has been deleted.'));
	reset_form();
}

if ($Mode == 'RESET')
	reset_form();

function reset_form() {
	global $selected_id;
	unset($_POST['trans_no']);
	unset($_POST['description']);
	$selected_id = -1;
}

function viewing_controls() {
	global $selected_id;
	
	start_table(TABLESTYLE_NOBORDER);

	start_row();
	systypes_list_cells(_('Type:'), 'filterType', null, true);
	if (list_updated('filterType'))
		reset_form();

	if(get_post('filterType') == ST_CUSTOMER )
		customer_list_cells(_('Select a customer: '), 'trans_no', null, false, true, true);
	elseif(get_post('filterType') == ST_SUPPLIER)
		supplier_list_cells(_('Select a supplier: '), 'trans_no', null, false, true, true);
	elseif(get_post('filterType') == ST_ITEM)
		stock_items_list_cells(_('Select an Item: '), 'trans_no', null, false, true, true);
	elseif(get_post('filterType') == ST_FIXEDASSET)
		stock_items_list_cells(_('Select an Item: '), 'trans_no', null, false, true, false, false, array('fixed_asset' => 1));
	elseif(get_post('filterType') == ST_BANKACCOUNT)
		bank_accounts_list_cells(_('Select a Bank Account: '), 'trans_no', null,  true);

	end_row();
	end_table(1);
}

function trans_view($trans) {
	if ($trans['type_no']==ST_SUPPLIER || $trans['type_no']==ST_CUSTOMER || $trans['type_no']==ST_ITEM || $trans['type_no']==ST_FIXEDASSET || $trans['type_no']==ST_BANKACCOUNT)
		return $trans['id'];
	return get_trans_view_str($trans['type_no'], $trans['trans_no']);
}

function edit_link($row) {
	return button('Edit'.$row['id'], _('Edit'), _('Edit'), ICON_EDIT);
}

function view_link($row) {
	return button('view'.$row['id'], _('View'), _('View'), ICON_VIEW);
}

function download_link($row) {
	return button('download'.$row['id'], _('Download'), _('Download'), ICON_DOWN);
}

function delete_link($row) {
	return button('Delete'.$row['id'], _('Delete'), _('Delete'), ICON_DELETE);
}

function display_rows($type, $trans_no) {

	$sql = get_sql_for_attached_documents($type, $type==ST_SUPPLIER || $type==ST_CUSTOMER || $type==ST_BANKACCOUNT ? $trans_no : ($type==ST_ITEM || $type==ST_FIXEDASSET ? get_item_code_id($trans_no) : 0));

	$cols = array(
		_('#') => array('fun'=>'trans_view', 'ord'=>''), 
		_('Doc Title') => array('name'=>'description'),
		_('Filename') => array('name'=>'filename'),
		_('Size') => array('name'=>'filesize'),
		_('Filetype') => array('name'=>'filetype'),
		_('Doc Date') => array('name'=>'tran_date', 'type'=>'date', 'ord'=>''),
		array('insert'=>true, 'fun'=>'edit_link', 'align'=>'center'),
		array('insert'=>true, 'fun'=>'view_link', 'align'=>'center'),
		array('insert'=>true, 'fun'=>'download_link', 'align'=>'center'),
		array('insert'=>true, 'fun'=>'delete_link', 'align'=>'center')
	);

	if($type == ST_SUPPLIER || $type == ST_CUSTOMER)
		$cols[_('#')] = 'skip';

	$table =& new_db_pager('trans_tbl', $sql, $cols);

	$table->width = '60%';

	display_db_pager($table);
}

//----------------------------------------------------------------------------------------

if (list_updated('filterType') || list_updated('trans_no'))
	$Ajax->activate('_page_body');

start_form(true);

viewing_controls();

$type = get_post('filterType');

display_rows($type, get_post('trans_no'));

br(2);

start_table(TABLESTYLE2);

if ($selected_id != -1) {
	if ($Mode == 'Edit') {
		$row = get_attachment($selected_id);
		$_POST['trans_no']  = $row['trans_no'];
		$_POST['description']  = $row['description'];
		hidden('trans_no', $row['trans_no']);
		hidden('unique_name', $row['unique_name']);
		if ($type != ST_SUPPLIER && $type != ST_CUSTOMER && $type != ST_ITEM && $type != ST_BANKACCOUNT)
			label_row(_('Transaction #'), $row['trans_no']);
	}	
	hidden('selected_id', $selected_id);
}
else {
	if ($type != ST_SUPPLIER && $type != ST_CUSTOMER && $type != ST_ITEM && $type != ST_FIXEDASSET && $type != ST_BANKACCOUNT)
		text_row_ex(_('Transaction #').':', 'trans_no', 10);
}
text_row_ex(_('Doc Title').':', 'description', 40);
file_row(_('Attached File').':', 'filename', 'filename');

end_table(1);

submit_add_or_update_center($selected_id == -1, '', 'process');

end_form();
end_page();
