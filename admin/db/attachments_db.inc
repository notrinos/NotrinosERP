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

function add_attachment($filterType, $trans_no, $description, $filename, $unique_name, $filesize, $filetype) {
	
	$sql = "INSERT INTO ".TB_PREF."attachments (type_no, trans_no, description, filename, unique_name, filesize, filetype, tran_date) VALUES (".db_escape($filterType).", ".db_escape($trans_no).",".db_escape($description).", ".db_escape($filename).", ".db_escape($unique_name).", ".db_escape($filesize).", ".db_escape($filetype).", '".date2sql(Today())."')";
	db_query($sql, 'Attachment could not be inserted');
}

function update_attachment($selected_id, $filterType, $trans_no, $description, $filename, $unique_name, $filesize, $filetype) {

	$sql = "UPDATE ".TB_PREF."attachments SET
		type_no=".db_escape($filterType).",
		trans_no=".db_escape($trans_no).",
		description=".db_escape($description).", ";
	if ($filename != "") {
		$sql .= "filename=".db_escape($filename).",
		unique_name=".db_escape($unique_name).",
		filesize=".db_escape($filesize).",
		filetype=".db_escape($filetype).",";
	}	
	$sql .= "tran_date='".date2sql(Today())."' WHERE id=".db_escape($selected_id);
	db_query($sql, 'Attachment could not be updated');
}

function delete_attachment($id) {
	$sql = "DELETE FROM ".TB_PREF."attachments WHERE id = ".db_escape($id);
	db_query($sql, 'Could not delete attachment');
}

function get_attachment_string($type, $id) {
	global $path_to_root;

	$str = '';    
	$sql = "SELECT * FROM ".TB_PREF."attachments WHERE type_no=".db_escape($type)." AND trans_no=".db_escape($id)." ORDER BY trans_no";

	$return = db_query($sql, 'Could not retrieve attachments');

	while ($attachment = db_fetch($return)) {
		if (strlen($str))
			$str = $str . " \n";    
		$str .= _('Attached File:')." <a href='$path_to_root/admin/attachments.php?vw=".$attachment['id']." ' target='blanc_'> ".$attachment['id'].' '.$attachment['description'].' - '.$attachment['filename']."</a><br>";
	}
	return $str."<br>";
}

function get_attached_documents($type, $trans_no=false) {
	$sql = "SELECT * FROM ".TB_PREF."attachments WHERE type_no=".db_escape($type);
	if ($trans_no)
		$sql .= " AND trans_no=".db_escape($trans_no);
	$sql .= " ORDER BY trans_no";
	return db_query($sql, 'Could not retrieve attachments');
}

function get_attachment($id) {
	$sql = "SELECT * FROM ".TB_PREF."attachments WHERE id=".db_escape($id);
	$result = db_query($sql, 'Could not retrieve attachments');
	return db_fetch($result);
}

function has_attachment($type, $id) {
	$sql = "SELECT DISTINCT * FROM ".TB_PREF."attachments WHERE type_no=".db_escape($type)." AND trans_no=".db_escape($id);
	$result = db_query($sql, 'Could not retrieve attachments');
	$myrow = db_fetch($result);

	return $myrow === false ? 0 : $myrow['id'];
}

function get_sql_for_attached_documents($type, $id_no) {
	// $_POST['trans_no'] will be used to store the customer_id or supplier_id for them
	$sql = "SELECT trans_no, description, filename, filesize, filetype, tran_date, id, type_no FROM ".TB_PREF."attachments WHERE type_no=".db_escape($type);

	if(($type == ST_CUSTOMER || $type == ST_SUPPLIER) && $id_no != null)
		$sql .=" AND trans_no = ".db_escape($id_no);

	$sql .= " ORDER BY trans_no DESC";

	return $sql;
}

function move_trans_attachments($type, $trans_from, $trans_to) {
	$sql = "UPDATE ".TB_PREF."attachments SET trans_no=".db_escape($trans_to)." WHERE type_no=".db_escape($type)." AND trans_no=".db_escape($trans_from);
	db_query($sql, 'cannot move atachments');
}
