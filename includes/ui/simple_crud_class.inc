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
//
//	Template for simple table editors
//
class simple_crud {
	var $name;
	var $Mode;
	var $selected_id;
	var $_none = ''; // selector value when no item is selected
	var $pre_handlers; // control buttons and related methods called before view display
	var $views;
	var $data = array();
	var $fields;
	var $tool_buttons;
	var $options;
	var $dec;
	//
	//
	function __construct($name, $fields = null)
	{
		$this->name = $name;
		$this->pre_handlers = array(
			'Edit' => '_edit',
			'Delete' => '_delete',
			'NEW' => '_edit',
			'ADD' => '_add',
			'UPDATE' => '_update',
			'RESET' => '_cancel',
			'CLONE' => '_cloning'
			);
		$this->views = array(
			'' => 'list_view',			// default view
			'Edit' => 'editor_view',
			'Delete' => 'list_view',
			'NEW' => 'editor_view',
			'ADD' => 'editor_view',
			'UPDATE' => 'editor_view',
			'RESET' => 'list_view',
			'CLONE' => 'editor_view'
			);
		$this->tool_buttons['Edit'] =
			array(_('Edit'), _('Edit document line'), ICON_EDIT, '');
		$this->tool_buttons['Delete'] =
			array(_('Delete'), _('Remove line from document'), ICON_DELETE, '');
		$this->tool_buttons['UPDATE'] =
			array(_('Update'), _('Confirm changes'), ICON_UPDATE, '');
		$this->tool_buttons['RESET'] =
			array(_('Cancel'), _('Cancel changes'), ICON_CANCEL, '');

		$this->fields = $fields;
		$this->selected_id = $this->_none;
	}
	/*
		Input/output formatters - convert values between php/user domains.
	*/
	function _format_input($value, $fmt)
	{
		switch($fmt) {
			case 'stock':
				$this->dec = get_qty_dec($value);
				return $value;
			case 'price':
			case 'qty':
			case 'number':
				    return user_numeric($value);
			case 'percent':
					return user_numeric($value)/100;
			case 'text':
			case 'date':
			default:
				return $value;
		}
	}
	
	function _format_output($value, $fmt)
	{
		switch($fmt) {
			case 'price':
				return price_format($value);
			case 'qty':
				return number_format2($value, $this->dec);
			case 'number':
				return number_format2($value);
			case 'percent':
				return percent_format($value*100);
			case 'stock':
				$this->dec = get_qty_dec($value); // retrieve dec for use in later qty fields
			case 'text':
			case 'date':
			default:
				return $value;
		}
	}

	function _check_mode()
	{
		global $Ajax;

		// list controls lookup
		foreach (array_keys($this->pre_handlers) as $m) {
			if (isset($_POST[$this->name.$m])) {
				unset($_POST['_focus']); // focus on first form entry
				$Ajax->activate($this->name.'_div');
				$val = @key($_POST[$this->name.$m]);
				$this->selected_id = $val!==null ? @quoted_printable_decode($val) : $this->_none;
				return $m;
			}
		}
		$mod = get_post($this->name.'Mode', '');
		if ($mod) {
			$val = @key($mod);
			$this->selected_id = $val!==null ? @quoted_printable_decode($val) : $this->_none;
			return $mod[$val];
		}
		return '';
	}

	//
	//	Set record for edition
	//
	function _edit($mode)
	{
		if ($this->Mode != $mode) {
			if ($this->selected_id != $this->_none) {
				$this->data = $this->db_read();
			}
			$this->set_posts($this->data);
		}
		$this->Mode = $mode;
	}
	//
	//	Update record after edition
	//
	function _update($mode)
	{
		$this->get_posts();
		if ($this->update_check()) {
			if ($this->db_update()) {
				$this->selected_id = $this->_none;
				$this->Mode = '';
				return;
			}
		}
		$this->Mode = $mode;
	}
	//
	//	Add new record
	//
	function _add($mode)
	{
		$this->get_posts();
		if ($this->insert_check()) {
			$this->db_insert();
			$this->_cancel();
			return;
		}
		$this->Mode = $mode;
	}
	//
	//	Delete selected  record
	//
	function _delete()
	{
		if ($this->delete_check())
			$this->db_delete();
		$this->_cancel();
	}
	//
	//	Return to listing view
	//
	function _cancel()
	{
		$this->selected_id = $this->_none;
		$this->db_cancel();
		$this->Mode = '';
	}
	//
	// Clone record for new edition
	//
	function _cloning()
	{
		$this->Mode = '';
		$this->_edit('Edit');
		$this->selected_id = $this->_none;
	}
	/*
		Generate form controls
	*/
	function _bottom_controls()
	{
		$clone = $this->selected_id != $this->_none;

		$title=false;
		$async='both';
		$base=$this->name;

		$cancel = $async;

		if ($async === 'both') {
			$async = 'default'; $cancel = 'cancel';
		} 
		elseif ($async === 'default')
			$cancel = true;
		elseif ($async === 'cancel')
			$async = true;
		echo "<center>";

		if ($this->Mode == '' || $this->Mode == 'RESET')
			submit("{$base}NEW", _("Add new"), true, $title, $async);
		else {
			if ($this->Mode == 'NEW' || $this->selected_id==$this->_none)
				
				submit("{$base}ADD", _("Add"), true, $title, $async);
			else {
				submit("{$base}UPDATE[{$this->selected_id}]", _("Update"), true, _('Submit changes'), $async);
				if ($clone) 
					submit("{$base}CLONE[{$this->selected_id}]", _("Clone"), true, _('Edit new record with current data'), $async);
			}
			submit("{$base}RESET", _("Cancel"), true, _('Cancel edition'), $cancel);
		}
		echo "</center>";
	}
	//===========================================================================
	// Public functions
	//
	
	function tool_button($name, $selected_id=null, $params='')
	{
		$b = $this->tool_buttons[$name];

		return "<td align='center' $params>"
			.button( "{$this->name}$name"
				.($selected_id === null || $selected_id === $this->_none ? '': "[$selected_id]"),
				$b[0], $b[1], $b[2], $b[3])."</td>";
	}
	
	function set_posts()
	{
		foreach($this->fields as $name => $fmt) {
			if (is_int($name)) {
				$name = $fmt;
				$fmt = array();
			}
			$post = isset($fmt['post']) ? $fmt['post'] : $name;
			$fld = isset($fmt['fld']) ? $fmt['fld'] : $name;

			$value = $this->selected_id == $this->_none ? @$fmt['dflt'] :
				(is_array($this->data) ? $this->data[$fld]: $this->data->$fld);

			$_POST[$post] = $this->_format_output($value, @$fmt['fmt']);
		}
	}
	//--------------------------
	//
	//	Get editor POST variables. 
	//
	function get_posts() {
		foreach ($this->fields as $name => $fmt) {
			if (is_int($name)) {
				$name = $fmt;
				$fmt = array();
			}
			$post = isset($fmt['post']) ? $fmt['post'] : $name;
			$fld = isset($fmt['fld']) ? $fmt['fld'] : $name;

			$value = $this->_format_input(@$_POST[$post], @$fmt['fmt']);
			if (is_array($this->data))
				$this->data[$fld] = $value;
			else
				$this->data->$fld = $value;
		}
	}
	//	Main function - display current CRUD editor content
	//
	function show()
	{
		if (!isset($_POST[$this->name.'Mode']))
			$this->set_posts();

		$Mode = $this->_check_mode(true);
		div_start($this->name.'_div');

		if (array_key_exists($Mode, $this->pre_handlers)) {
			$fun = $this->pre_handlers[$Mode];
			$this->$fun($Mode);
		}

		if (isset($this->views[$this->Mode]))
			$this->{$this->views[$this->Mode]}();
		else
			$this->{$this->views['']}(); // default view

		$this->_bottom_controls();
		// this is needed only when we use temporary crud object together with ajax screen updates
		hidden($this->name.'Mode'.'['.$this->selected_id.']', $this->Mode);
		div_end();
	}
	
	//===========================================================================
	//	Database functions placeholders
	
	//
	//	Read record from db for edition
	//
	function db_read() {
		display_notification(__FUNCTION__. ' is not defined...');
		return array();
	}
	//
	//	Update record in db after edition
	//
	function db_update()
	{
		$this->db_insert();
	}
	//
	//	Delete record
	//
	function db_delete() {
		display_notification(__FUNCTION__. ' is not defined...');
	}
	//
	//	Insert record
	//
	function db_insert()
	{
		display_notification(__FUNCTION__. ' is not defined...');
	}
	//
	//	Cancel edition
	//	Optional things like focus set.
	//
	function db_cancel()
	{
	}


	function delete_check() {
		display_notification(__FUNCTION__. ' is not defined...');
		return true; 
	}

	function insert_check() {
		return true;
	}

	function update_check() 
	{
		return $this->insert_check(); 
	}

	//
	//	Show database content in pager/table
	//
	function list_view() {
		display_notification(__FUNCTION__. ' is not defined...');
	}

	//
	//	Show record editor screen content
	//
	function editor_view() {
		display_notification(__FUNCTION__. ' is not defined...');
	}
};

