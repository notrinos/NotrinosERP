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
include_once($path_to_root. '/includes/ui/simple_crud_class.inc');

/*
	View/Edit class for attachments
*/
class attachments extends simple_crud {
	var $selected_id;
	var $entity;
	var $sub_class;
	var $class;
	
	function __construct($name, $id, $class, $subclass=null) {
		$fields = array('type_no', 'trans_no','description','filename','tran_date' => 'date');
		parent::__construct($name, $fields);
		$this->class = $class;
		$this->sub_class = $subclass;
		$this->entity = $id;
		
	}

	function list_view() {
		global $Ajax, $path_to_root, $session_debtor_no, $session_supplier_id;
		br();
		if(isset($_POST['attachmentDownload'])){
			$keys =  array_keys($_POST['attachmentDownload']);
			$sql = "SELECT * FROM ".TB_PREF."attachments WHERE id = ".db_escape($keys[0]);
			$row = db_fetch(db_query($sql), "Cannot retrieve attachment $keys[0]");

			
			if ($row['unique_name'] != '') {
				if(in_ajax())
					$Ajax->redirect($path_to_root.'/admin/attachments.php?dl='.$keys[0]);
				else {					
					Header("Content-type: 'application/octet-stream' ");
					//header('Content-Length: '.$row['filesize']);
					header('Content-Disposition: attachment; filename="'.$row['filename'].'"');
					echo file_get_contents(company_path()."/attachments/".$row['unique_name']);
					exit();
				}
			}
		}
		if(isset($_POST['attachmentView'])){
			$keys =  array_keys($_POST['attachmentView']);
			$sql = "SELECT * FROM ".TB_PREF."attachments WHERE id = ".db_escape($keys[0]);
			$row = db_fetch(db_query($sql), "Cannot retrieve attachment $keys[0]");
			
			if ($row['unique_name'] != ''){

				$Ajax->redirect($path_to_root.'/admin/attachments.php?vw='.$keys[0]);	
				/*if(in_ajax()) {
					
				} else {					
					$type_no = ($row['filetype']) ? $row['filetype'] : 'application/octet-stream';	
					header("Content-type: ".$type_no);
					header("Content-Disposition: inline");
					echo file_get_contents(company_path(). "/attachments/".$row['unique_name']);
					exit();
				}*/
			}
		}
		$filters = array('trans_no' => $this->entity, 'type_no' => (isset($_GET['type_no']) ? $_GET['type_no'] : ST_CUSTOMER)); // default ST_CUSTOMER

		if($session_debtor_no || $session_supplier_id)
			$filters['visible'] = 1;

		$sql = "SELECT * FROM ".TB_PREF."attachments WHERE trans_no=".db_escape($this->entity)." AND type_no = ".db_escape(isset($_GET['type_no']) ? $_GET['type_no'] : ST_CUSTOMER);
		$result = db_query($sql, 'Could not get data from '.TB_PREF.'attachments table');
		$attachments = Array();
		while($row = db_fetch_assoc($result))
			$attachments[] = $row;

		start_table(TABLESTYLE, 'width=80%');
		$th = array(_('ID'), _('Doc Title'), _('Filename'), _('Size'), _('Filetype'), _('Doc Date'),  '',  '', '', '');	
		table_header($th);	
		if($attachments){	
			foreach($attachments as $de) {
				label_cell($de['id']);
				label_cell($de['description']);
				label_cell($de['filename']);
				label_cell($de['filesize']);
				label_cell($de['filetype']);
				label_cell(sql2date($de['tran_date']));
				edit_button_cell("{$this->name}Edit[{$de['id']}]", _('Edit'));
				label_cell('<a target="_blank" class="viewlink" href="'.$path_to_root.'/admin/attachments.php?vw='.$de['id'].'" onclick="javascript:openWindow(this.href,this.target); return false;" accesskey="V"><img src="'.$path_to_root.'/themes/default/images/view.gif" style="vertical-align:middle;width:12px;height:12px;border:0;"></a>', 'style="text-align: center;" ');
				button_cell("{$this->name}Download[{$de['id']}]", _('Download'), false, ICON_DOWN);
				
				delete_button_cell("{$this->name}Delete[{$de['id']}]", _('Delete'));
				end_row();	
			}
		}
		end_table(1);
	}
	
	function editor_view(){
		//global $Ajax;
		br();
		div_start('Attachment_Details');
		start_outer_table(TABLESTYLE);
	
		table_section(1);
		table_section_title(_('Attachment Detail'));
		
		if($this->selected_id != null && $this->selected_id != '') {
			$sql = "SELECT * FROM ".TB_PREF."attachments WHERE id = ".db_escape($this->selected_id);
			$row = db_fetch(db_query($sql), "Cannot retrieve attachment $this->selected_id");
			hidden('unique_name', $row['unique_name']);
			$_POST['tran_date'] = sql2date($row['tran_date']);
			$_POST['title'] = $row['description'];
			$_POST['trans_no'] = $row['trans_no'];
		}
		else {
			$_POST['trans_no'] = $this->entity;
			$_POST['tran_date'] = Today();
			hidden('unique_name', '');
		}

		$type_no= $_GET['type_no'];
		
		if($type_no==ST_CUSTOMER) // 5
			$id_word = _('Customer ID');
		elseif($type_no==ST_SUPPLIER) // 6
			$id_word = _('Supplier ID');
		elseif($type_no==ST_ITEM)
			$id_word = _('Item ID');
		elseif($type_no==ST_FIXEDASSET) 
			$id_word = _('Fixed Asset ID');
		elseif($type_no==ST_BANKACCOUNT) 
			$id_word = _('Bank Account ID');

		label_row(_($id_word), $_POST['trans_no']);	
		date_row(_('Date'), 'tran_date');
		text_row_ex(_('Doc Title').':', 'description', 40);
		if ($this->selected_id)
			label_row(_('File:'), sprintf(_("%s [%s bytes]"), $row['filename'], $row['filesize']));
		file_row( ($this->selected_id ? _('File change') : _('Select file')) . ':', 'file_attachment_name', 'file_attachment_name');
		hidden('type_no', $_GET['type_no']);
		end_outer_table(1);	
		div_end();
	}

	function _bottom_controls()	{
		$title=false;
		$async='both';
		$base=$this->name;

		$cancel = $async;

		if ($async === 'both') {
			$async = 'default';
			$cancel = 'cancel';
		}
		else if ($async === 'default')
			$cancel = true;
		else if ($async === 'cancel')
			$async = true;
		echo '<center>';

		if ($this->Mode == '' || $this->Mode == 'RESET')
			submit("{$base}NEW", _('Add new'), true, $title, $async);
		else {
			if ($this->Mode == 'NEW' || $this->selected_id==$this->_none)
				submit("{$base}ADD", _('Add'), true, $title, $async);
			else
				submit("{$base}UPDATE[{$this->selected_id}]", _('Update'), true, _('Submit changes'), $async);				
			submit("{$base}RESET", _('Cancel'), true, _('Cancel edition'), $cancel);
		}
		echo '</center>';
	}
	
	function db_insert() {
		$dat = $this->data;		
		if(!isset($max_image_size))
		$max_image_size = 5000;
		$upload_file = '';
		if (isset($_FILES['file_attachment_name']) && $_FILES['file_attachment_name']['name'] <> '') {
			
			$result = $_FILES['file_attachment_name']['error'];
			$upload_file = 'Yes'; 
			$attr_dir = company_path().'/attachments' ; 
			if (!file_exists($attr_dir))		
				mkdir($attr_dir);
			$attach_dir = $attr_dir;
			if (!file_exists($attach_dir))
				mkdir($attach_dir);
			
			if(isset($_POST['unique_name']) && $_POST['unique_name'] <> '')
				$attachment_file_name = $_POST['unique_name'];			
			else
				$attachment_file_name = uniqid(); 
				
			$filesize = $_FILES['file_attachment_name']['size'];
			$filetype = $_FILES['file_attachment_name']['type'];

			$filename = $attach_dir.'/'.$attachment_file_name; 
			
			if (file_exists($filename)){
				$result = unlink($filename);
				if (!$result) {
					display_error(_('The existing Docs could not be removed'));
					$upload_file = 'No';
				}
			}	
			if ($upload_file == 'Yes'){
				$actual_file_nam = $_FILES['file_attachment_name']['name'];
				$result  =  move_uploaded_file($_FILES['file_attachment_name']['tmp_name'], $filename);
				$sql = "INSERT INTO ".TB_PREF."attachments (trans_no, type_no, `description`, `filename`, unique_name, tran_date, `filesize`, `filetype`) VALUES ("
					. db_escape($this->entity) . ','
					. db_escape($dat['type_no']) . ','
					. db_escape($_POST['description']) . ','
					. db_escape($actual_file_nam) . ','
					. db_escape($attachment_file_name) . ','
					. db_escape(date2sql($_POST['tran_date'])) . ','
					. db_escape($filesize) . ','
					. db_escape($filetype) . ')';
				if(db_query($sql, 'Could not add Attachment'))
					display_notification(_('A New Attachment Added Successfully'));
			}
		}
	}

	function db_update() {
		$dat = $this->data;
		if(!isset($max_image_size))
			$max_image_size = 5000;
		$upload_file = '';
		if (isset($_FILES['file_attachment_name']) && $_FILES['file_attachment_name']['name'] != '') {
			
			$result = $_FILES['file_attachment_name']['error'];
			$upload_file = 'Yes'; 
			$attr_dir = company_path().'/attachments' ; 
			if (!file_exists($attr_dir))
				mkdir($attr_dir);
			$attach_dir = $attr_dir;
			if (!file_exists($attach_dir))
				mkdir($attach_dir);
			
			if(isset($_POST['unique_name']) && $_POST['unique_name'] == '')
				$attachment_file_name = uniqid();
			else
				$attachment_file_name = $_POST['unique_name']; 
				
			$filesize = $_FILES['file_attachment_name']['size'];
			$filetype = $_FILES['file_attachment_name']['type'];

			$filename = $attach_dir.'/'.$attachment_file_name; 
			if (file_exists($filename)){
				$result = unlink($filename);
				if (!$result) {
					display_error(_('The existing Docs could not be removed'));
					$upload_file = 'No';
				}
			}
			if ($upload_file == 'Yes'){
				$actual_file_nam = $_FILES['file_attachment_name']['name'];
				$result  =  move_uploaded_file($_FILES['file_attachment_name']['tmp_name'], $filename);
			}
		}
		$sql = "UPDATE ".TB_PREF."attachments SET "
			. "trans_no = " . db_escape($this->entity) . ","
			. "type_no = " . db_escape($dat['type_no']) . ","
			. "`description` = " . db_escape($_POST['description']) . ","
			. "tran_date = " . db_escape(date2sql($_POST['tran_date']));
		if ($upload_file == 'Yes')
			$sql .=  ", unique_name = " . db_escape($attachment_file_name) . ","
				. "`filename` = " . db_escape($actual_file_nam) . ","
				. "`filesize` = " . db_escape($filesize) . ","
				. "`filetype` = " . db_escape($filetype);
		$sql .=  " WHERE id = " . db_escape($this->selected_id);
		if(db_query($sql, "Could not Update Attachment $this->selected_id"))
			display_notification(_("The Selected Attachment $this->selected_id Updated Successfully"));
	}

	function insert_check() {
		if (strlen($_POST['description']) == 0) {
			display_error(_('Description cannot be empty.'));
			set_focus('description');
			return false;
		}
		return true; 
	}

	function db_read() {
		$sql = "SELECT * FROM ".TB_PREF."attachments WHERE id=".db_escape($this->selected_id);
		$result = db_query($sql, "Could not get all data from ".TB_PREF."attachments table");
		$attachments = Array();
		return db_fetch_assoc($result);
	}

	function delete_check() {
		return true;
	}
	//
	//	Delete all attachments for Customer / Supplier in current class/entity
	//
	function db_delete() {
		$sql = "SELECT * FROM ".TB_PREF."attachments WHERE id = ".db_escape($this->selected_id);
		$row = db_fetch(db_query($sql), "Cannot retrieve attachment $this->selected_id");

		$dir =  company_path().'/attachments';
		if ($row['unique_name'] && file_exists($dir.'/'.$row['unique_name']))
			unlink($dir.'/'.$row['unique_name']);

		$sql = "DELETE FROM ".TB_PREF."attachments WHERE id = ".db_escape($this->selected_id);
		if(db_query($sql, "Could not Delete Attachment $this->selected_id"))
			display_notification(_('The Selected Attachment is Deleted Successfully'));
		return false;
	}
}