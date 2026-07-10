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
include_once($path_to_root.'/includes/attachment_service.inc');

/**
 * Validate that a stored unique_name resolves safely within the attachments directory.
 * Returns the resolved absolute path on success, or false if the path is unsafe.
 * This prevents path traversal attacks when reading, downloading, or deleting files.
 *
 * @param string $unique_name The stored attachment filename (must be a plain filename, not a path).
 * @return string|false The resolved safe absolute path, or false if the path is unsafe.
 */
function safe_attachment_file_path($unique_name) {
	if ($unique_name === '' || $unique_name === null) {
		return false;
	}
	// Reject any path separators, null bytes, or parent directory traversal sequences.
	if (strpbrk($unique_name, "\\/\0") !== false || strpos($unique_name, '..') !== false) {
		return false;
	}
	$attach_dir = company_path() . '/attachments';
	$full_path = realpath($attach_dir . '/' . $unique_name);
	if ($full_path === false) {
		return false;
	}
	$real_attach_dir = realpath($attach_dir);
	if ($real_attach_dir === false) {
		return false;
	}
	// Ensure the resolved path is strictly within the attachments directory.
	if (strpos($full_path, $real_attach_dir . DIRECTORY_SEPARATOR) !== 0) {
		return false;
	}
	return $full_path;
}

if (isset($_GET['vw']))
	$view_id = $_GET['vw'];
else
	$view_id = find_submit('view');

if ($view_id != -1) {

	$row = get_attachment($view_id);

	// Block generic access to employee document attachments.
	if (attachment_is_hr_employee_document($row)) {
		display_error(_('Employee documents must be accessed through the HR module.'));
		exit();
	}

	if ($row['filename'] != '') {
		if(in_ajax())
			$Ajax->popup($_SERVER['PHP_SELF'].'?vw='.$view_id);
		else {
			$safe_path = safe_attachment_file_path($row['unique_name']);
			if ($safe_path === false) {
				display_error(_('Invalid attachment path.'));
				exit();
			}
			$type = ($row['filetype']) ? $row['filetype'] : 'application/octet-stream';
			// Clean output buffer to prevent output_html callback from corrupting binary data
			while (ob_get_level())
				ob_end_clean();
			header('Content-type: '.$type);
			header('Content-Length: '.$row['filesize']);
			header('Content-Disposition: inline');
			echo file_get_contents($safe_path);
			exit();
		}
	}	
}
if (isset($_GET['dl']))
	$download_id = $_GET['dl'];
else
	$download_id = find_submit('download');

if ($download_id != -1) {
	$row = get_attachment($download_id);

	// Block generic access to employee document attachments.
	if (attachment_is_hr_employee_document($row)) {
		display_error(_('Employee documents must be accessed through the HR module.'));
		exit();
	}

	if ($row['filename'] != '') {
		if(in_ajax())
			$Ajax->redirect($_SERVER['PHP_SELF'].'?dl='.$download_id);
		else {
			$safe_path = safe_attachment_file_path($row['unique_name']);
			if ($safe_path === false) {
				display_error(_('Invalid attachment path.'));
				exit();
			}
			$type = ($row['filetype']) ? $row['filetype'] : 'application/octet-stream';
			// Clean output buffer to prevent output_html callback from corrupting binary data
			while (ob_get_level())
				ob_end_clean();
			header('Content-type: '.$type);
			header('Content-Length: '.$row['filesize']);
			header('Content-Disposition: attachment; filename="'.$row['filename'].'"');
			echo file_get_contents($safe_path);
			exit();
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
	
	$filename = basename($_FILES['filename']['name']);
	
	// Block generic add/update for ST_EMPLOYEE type.
	if ((int)get_post('filterType') === ST_EMPLOYEE) {
		display_error(_('Employee documents must be managed through the HR module.'));
		reset_form();
		$Mode = 'RESET';
	} elseif (($_POST['filterType'] == ST_ITEM || $_POST['filterType'] == ST_FIXEDASSET) && $Mode == 'ADD_ITEM')
		$_POST['trans_no'] = get_item_code_id($_POST['trans_no']);
	if (!transaction_exists($_POST['filterType'], $_POST['trans_no']) || !ctype_digit($_POST['trans_no']))
		display_error(_('Selected transaction does not exists.'));
	elseif ($Mode == 'ADD_ITEM' && !in_array(strtoupper(substr($filename, strlen($filename) - 3)), array('JPG', 'PNG', 'GIF', 'PDF', 'DOC', 'ODT'))) {
		display_error(_('Only graphics, pdf, doc and odt files are supported.'));
	}
	elseif ($Mode == 'ADD_ITEM' && !isset($_FILES['filename']))
		display_error(_('Select attachment file.'));
	elseif ($Mode == 'ADD_ITEM' && ($_FILES['filename']['error'] > 0)) {
		if ($_FILES['filename']['error'] == UPLOAD_ERR_INI_SIZE) 
			display_error(_('The file size is over the maximum allowed.'));
		else
			display_error(_('Select attachment file.'));
	}
	elseif ( strlen($filename) > 60)
		display_error(_('File name exceeds maximum of 60 chars. Please change filename and try again.'));
	else {

		$tmpname = $_FILES['filename']['tmp_name'];
		$dir = company_path().'/attachments';

		if (!file_exists($dir)) {
			mkdir ($dir,0777);
			$index_file = "<?php\nheader(\"Location: ../index.php\");\n";
			$fp = fopen($dir.'/index.php', 'w');
			fwrite($fp, $index_file);
			fclose($fp);
		}

		$filesize = $_FILES['filename']['size'];
		$filetype = $_FILES['filename']['type'];

		// file name compatible with POSIX
		// protect against directory traversal
		if ($Mode == 'UPDATE_ITEM') {
			$row = get_attachment($selected_id);
			if ($row['filename'] == '')
				exit();
			$unique_name = $row['unique_name'];
			if ($filename && file_exists($dir.'/'.$unique_name))
				unlink($dir.'/'.$unique_name);
		}
		else
			$unique_name = random_id();

		//save the file
		move_uploaded_file($tmpname, $dir.'/'.$unique_name);

		if ($Mode == 'ADD_ITEM') {
			add_attachment($_POST['filterType'], $_POST['trans_no'], $_POST['description'], $filename, $unique_name, $filesize, $filetype);
			display_notification(_('Attachment has been inserted.'));
		}
		else {
			update_attachment($selected_id, $_POST['filterType'], $_POST['trans_no'], $_POST['description'], $filename, $unique_name, $filesize, $filetype); 
			display_notification(_('Attachment has been updated.'));
		}
		reset_form();
	}
	refresh_pager('trans_tbl');
	$Ajax->activate('_page_body');
}

if ($Mode == 'Delete') {
	$row = get_attachment($selected_id);

	// Block generic deletion of employee document attachments.
	if (attachment_is_hr_employee_document($row)) {
		display_error(_('Employee documents must be managed through the HR module.'));
		reset_form();
		$Mode = 'RESET';
	} elseif ($row['unique_name']) {
		$safe_path = safe_attachment_file_path($row['unique_name']);
		if ($safe_path !== false && file_exists($safe_path))
			unlink($safe_path);
	}
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
	systypes_list_cells(_('Type:'), 'filterType', null, true, array(ST_EMPLOYEE));
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
	return "<div class='button-cell'>".button('Edit'.$row['id'], _('Edit'), _('Edit'), ICON_EDIT2)."</div>";
}

function view_link($row) {
	return "<div class='button-cell'>".button('view'.$row['id'], _('View'), _('View'), ICON_VIEW)."</div>";
}

function download_link($row) {
	return "<div class='button-cell'>".button('download'.$row['id'], _('Download'), _('Download'), ICON_DOWN)."</div>";
}

function delete_link($row) {
	return "<div class='button-cell'>".button('Delete'.$row['id'], _('Delete'), _('Delete'), ICON_THRASH)."</div>";
}

function display_rows($type, $trans_no) {

	// Do not list employee attachments in generic attachment page.
	if ((int)$type === ST_EMPLOYEE) {
		display_note(_('Employee documents are managed through the HR module.'));
		return;
	}

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
		// Do NOT expose unique_name to the client via hidden field.
		// Server-side update handler generates its own random filename.
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
