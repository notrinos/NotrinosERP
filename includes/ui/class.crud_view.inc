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

include_once $path_to_root.'/includes/db/class.data_set.inc';

class UI {
	var $ui_mode;

	// formatters
	function text($value, $name='', $opts=array())
	{
		$text = array('label', 'size'=>"", 'max'=>"", 'title'=>false,	'labparams'=>"", 'post_label'=>"", 'inparams'=>"");
		$opts = array_merge($text, $opts);

		if (!$name)
			return $value;

		call_user_func_array('text_cells', $opts);
	}
}
/*
	TODO: for php5:
	. use __construct in base class to avoid need for implicit call to parent constructor.
	. use private instead _* function name convention
*/
//
//	User input-output conversions.
//
class user_view {

	var	$data; 		// data in php format
	var $fields;	// input fields format descriptions
	var $errors = array();
	var $dec;
	var $name;

	function __construct($name)
	{
		$this->name = $name;
	}

	function error($msg, $context=null)
	{
		// save error message
		if (!isset($context))
			$context = count($this->errors);
		$this->errors[$context] = $msg;

		return false;
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
			case 'check':
					return isset($value) ? 1 : 0;
			case 'text':
			case 'date':
			default:
				return $value;
		}
	}
	//
	// Returns formatted value
	//
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
			case 'check':
				return !empty($value);
			case 'stock':
				$this->dec = get_qty_dec($value); // retrieve dec for use in following qty fields
			case 'text':
			case 'date':
			default:
				return $value;
		}
	}
	/**
	*	Returns html element for given field
	*	@mode - true for edit, false for read-only
	**/
	function _format_cells($value, $fmt, $mode)
	{
		$value = $this->_format_output($fmt);

		// available formatters with parameters
		$formatters = array(
			'email_cell' => array('label', 'params'=>'', 'id'=>null),
			'amount_cell' => array('label'=>null, 'bold'=>false, 'params'=>'', 'id'=>null),
			'text_cells' => array('label', 'name', 'value'=>null, 'size'=>"", 'max'=>"", 'title'=>false,
				'labparams'=>"", 'post_label'=>"", 'inparams'=>"")
		);
		// format functions used in various modes
		$formats = array(
			// field format => (view [,edit])
			'' => array('label_cell', 'text_cells'), // default
			'price' => array('label_cell', 'amount_cell'),
			'qty',
			'number',
			'percent',
			'stock',
			'text',
			'date',
		);
	}

	/**
	*
	*	PHP->user format values convertion.
	*
	**/
	function set_output($data=null, &$output=null)
	{
		if (isset($data))
			$this->data = $data;

		if (!isset($output)) {
			$output = &$_POST;
			$prefix = $this->name;
		} else
			$prefix = '';

		foreach($this->fields as $name => $fmt) {

			if (is_int($name)) {
				$name = $fmt;
				$fmt = array();
			} elseif (!is_array($fmt))
				$fmt = array('fmt' => $fmt);

			$post = $prefix.(isset($fmt['post']) ? $fmt['post'] : $name);

			$fld = isset($fmt['fld']) ? $fmt['fld'] : $name;

			if (is_object($this->data))
				$value = isset($this->data->$fld) ?	$this->data->$fld : @$fmt['dflt'];
			else
				$value = isset($this->data[$fld]) ?	$this->data[$fld] : @$fmt['dflt'];

			if (isset($value))
				$output[$post] = $this->_format_output($value, @$fmt['fmt']);
		}
	}

	/**
	*
	*	User->php format values convertion.
	*	$input - data in user format
	*	$all - return also null values for non-existing input field.
	**/
	function get_input($input=null, $all=false)
	{
		if (!isset($input))
			$input = $_POST;

		if ($this->name)	// strip view name prefix
		    foreach($input as $postkey=>$postval )
    		{
				if (strpos($postkey, $this->name) === 0)
				{
					$id = substr($postkey, strlen($this->name));
					$input[$id] = $postval;
				}
				unset($input[$postkey]);
    		}

    	$data = array();
		foreach ($this->fields as $name => $fmt) {
			if (is_int($name)) {	// direct string passed: this is name of field
				$name = $fmt;
				$fmt = array(); // default format
			} elseif (!is_array($fmt))
				$fmt = array('fmt' => $fmt);
			$post = isset($fmt['post']) ? $fmt['post'] : $name; // input name (default to field name)
			$fld = isset($fmt['fld']) ? $fmt['fld'] : $name;	// input value (default to field name)


//			if ($all || array_key_exists($post, $input))
//			{
				if (@$fmt['fmt'] == 'check')
					$value = @$input[$post] ? 1 : 0;
				else
					$value = $this->_format_input(@$input[$post], @$fmt['fmt']);

//				if (is_array($data))
				if ($all || isset($value))
					$data[$fld] = $value;
//				else
//					$data->$fld = $value;
//			}
		}

		return $data;
	}

	//--------------------------------------------------------
	//
	//	Return data formatted according to field descriptions.
	//
	function get_fields_views($input=null, $mode=null)
	{

		if (!isset($input))
			$input = $_POST;

		$view = array();
		foreach ($this->fields as $name => $fmt) {
			if (is_int($name)) {
				$name = $fmt;
				$fmt = array();
			}
			$post = isset($fmt['post']) ? $fmt['post'] : $name;
			$fld = isset($fmt['fld']) ? $fmt['fld'] : $name;

			$value = $this->_format_cells(@$input[$post], @$fmt['fmt'], $mode);

			$view[$fld] = $value;
		}
		return $view;
	}

}

//
//	Template for simple table editors
//

class simple_crud_view extends user_view {
	var $name;
		// object status:
	var $Mode = 'RESET';
	var $selected_id;
	var $prev_id;

	var $_none = ''; // selector value when no item is selected
	var $pre_handlers; // control buttons and related methods called before view display
	var $views;
	var $data = array();
	var $fields;
	var $tool_buttons;
	var $options = array(
		'delete' => true,		// true or message for successfull action.
		'update' => true,
		'insert' => true,
		'clone' => true,
	);
	var $dec;
	var $data_set;
	var $display_both = false; //when set to true both list and editor are displayed all the time (eventually set in sub classes)
	//
	//
	function __construct($name, $data_set = null, $options=array())
	{
		parent::__construct($name);

		$this->options = array_merge($this->options, $options);

		$this->views[''] = 'list_view';			// default view

		if ($this->options['update'])
			$this->_add_action('Edit', '_edit', _('Edit'), _('Edit document line'), ICON_EDIT, '',
				'editor_view');

		if ($this->options['delete'])
			$this->_add_action('Delete', '_delete', _('Delete'), _('Remove line from document'), ICON_DELETE, '',
				'list_view');

		if ($this->options['update'])
			$this->_add_action('UPDATE', '_update', _('Update'), _('Submit changes'), ICON_UPDATE, 'default',
				'editor_view');

		$this->_add_action('RESET', '_cancel', _('Cancel'), _('Cancel changes'), ICON_ESCAPE, 'cancel',
			'list_view');

		if ($this->options['insert'])
			$this->_add_action('ADD', '_add', _('Add'), _('Add new'), ICON_ADD, 'default',
				'editor_view');

		if ($this->options['insert'])
			$this->_add_action('NEW', '_add', _('New'), _('Add new'), ICON_ADD, 'default',
				'editor_view');

		if ($this->options['clone'])
			$this->_add_action('CLONE', '_cloning', _('Clone'), _('Clone'), ICON_ADD, '',
				'editor_view');

		$this->data_set = $data_set;
		$this->fields = $data_set->fields;

//		$this->_prev_status();
	}

	function _add_action($name, $handler, $but_value=null, $but_title=false, $but_icon=false, $aspect='', $view=null)
	{
		$this->pre_handlers[$name] = $handler;

		if ($but_value)
			$this->tool_buttons[$name] = array($but_value, $but_title, $but_icon, $aspect);

		if ($view)
			$this->_add_mode($name, $view);
	}

	function _add_mode($name, $view)
	{
		$this->views[$name] = $view;
	}

	function _prev_status()
	{
		// Restore previous mode/key (obsolete for views stored in session)

		$mod = get_post($this->name.'Mode', $this->Mode);
		if ($mod) {
			if (is_array($mod)) {
				$val = key($mod);
				$this->selected_id = $val!==null ? @quoted_printable_decode($val) : $this->_none;
				$mod = $mod[$val];
			} else {
				$val = $mod;
				$this->selected_id = $this->_none;
			}
		}
		$this->Mode = $mod;
	}

	function _check_mode()
	{
		global $Ajax;

		$mod = '';//$this->Mode;
		// Detect action (mode change)
		foreach (array_keys($this->pre_handlers) as $m) { // check button controls

			if (isset($_POST[$this->name.$m])) {
				unset($_POST['_focus']); // focus on first form entry
				$Ajax->activate($this->name.'_div');
				$Ajax->activate($this->name.'_controls');
				$val = is_array($_POST[$this->name.$m]) ? key($_POST[$this->name.$m]) : null;
				$this->prev_id = $this->selected_id;
				$this->selected_id = $val!==null ? @quoted_printable_decode($val) : $this->_none;
				$mod = $m; break;
			}
		}
		if (!$mod && $_SERVER['REQUEST_METHOD'] == 'GET') // initialize on every GET
			$mod = 'RESET';

		return $mod;
	}

	function display_error()
	{
		$this->errors = array_merge($this->data_set->errors, $this->errors);
		$firsterr = reset($this->errors);
		$field = key($this->errors);

		if (!is_numeric($field))
			set_focus($this->name.$field);
		display_error($firsterr);

		$this->errors = array(); // clean up to prevent false errors on object reuse
	}
	//
	//	Set record for edition
	//
	function _edit($mode)
	{
		if ($this->selected_id != $this->prev_id || $mode != $this->Mode) { // get record for edition
			$this->data = $this->data_set->get($this->selected_id !== $this->_none ? $this->selected_id : null);
			$this->set_output();
		}
//		if ($this->Mode != $mode) {
//		}

//		else
//			$this->display_error();

		$this->Mode = $mode;
	}
	//
	//	Update record after edition
	//
	function _update($mode)
	{
		if (!$this->options['update'])
			return;

		$this->data = $this->get_input();
		if ($this->data_set->update_check($this->selected_id, $this->data) && $this->data_set->update($this->selected_id, $this->data)) {
				$this->selected_id = $this->_none;
				$this->Mode = 'RESET';
				$this->_cancel();
				if (is_string($this->options['update']))
					display_notification($this->options['update']);
				return;
		}
		else
			$this->display_error();

		$this->Mode = $mode;
	}
	//
	//	Add new record
	//
	function _add($mode)
	{
		if (!$this->options['insert'])
			return;

		if ($mode == 'ADD') {
			$this->data = $this->get_input();
			if ($this->data_set->insert_check($this->data) && ($this->data_set->insert($this->data) !== false)) {
				$this->_cancel();
				if (is_string($this->options['insert']))
					display_notification($this->options['insert']);
				$this->Mode = 'RESET';
				return;
			}
			else
				$this->display_error();
		} else {// mode==NEW
				$this->data = $this->data_set->get();
		}
		$this->Mode = 'ADD';
	}
	//
	//	Delete selected  record
	//
	function _delete()
	{
		if (!$this->options['delete'])
			return;

		if ($this->data_set->delete_check($this->selected_id) && $this->data_set->delete($this->selected_id))
		{
			if (is_string($this->options['delete']))
				display_notification($this->options['delete']);
		} else
			$this->display_error();
		$this->_cancel();
	}
	//
	//	Return to listing view
	//
	function _cancel()
	{
		global $Ajax;
		$this->selected_id = $this->_none;
		if ($this->display_both)
		{
			$this->data = $this->data_set->get();
			$this->set_output();
		}
		$this->cancel();
		if (!$this->display_both)
			$Ajax->activate($this->name.'_div');
		$this->Mode = 'RESET';
	}
	//
	// Clone record for new edition
	//
	function _cloning()
	{
		if (!$this->options['clone'])
			return;
		$this->Mode = 'ADD';
		$this->_edit('Edit');
		$this->selected_id = $this->_none;
	}
	/*
		Generate form controls
	*/

	function _record_controls($list_view = false)
	{
		$clone = $this->options['clone'] && $this->selected_id != $this->_none;

		div_start($this->name.'_controls');
		echo "<center>";

		if ($list_view)
			$this->action_button('NEW');
		else {
			if ($this->Mode == 'NEW' || $this->selected_id==$this->_none)
			{
				$this->action_button('ADD');
				$this->action_button('RESET');
			} else {
				$this->action_button('UPDATE', $this->selected_id);
				if ($clone && $this->display_both) 
					$this->action_button('CLONE', $this->selected_id);
				$this->action_button('RESET');
			}
		}

		echo "</center>";
		div_end();
	}

	//===========================================================================
	// Public functions
	//

	//
	//	Submit buttons for form actions
	//
	function action_button($action, $selected_id=null)
	{
		list($value, $title, $icon, $aspect) = $this->tool_buttons[$action];
		submit($this->name.$action.(isset($selected_id) ? "[$selected_id]" : ''), $value, true, $title, $aspect, $icon);
	}

	//
	//	Tool button for grid line actions.
	//
	function tool_button($name, $selected_id=null, $params='')
	{
		$b = $this->tool_buttons[$name];

		return "<td align='center' $params>"
			.button( "{$this->name}$name"
				.($selected_id === null || $selected_id === $this->_none ? '': "[$selected_id]"),
				$b[0], $b[1], $b[2], $b[3])."</td>";
	}
	//
	//	Main function - display current CRUD editor content
	//
	function show($Mode=null)
	{
		if (!isset($Mode))
			$Mode = $this->_check_mode(true);

		div_start($this->name.'_div');

		if (array_key_exists($Mode, $this->pre_handlers)) {
			$fun = $this->pre_handlers[$Mode];
			if (is_array($fun))
				call_user_func($fun, $Mode);
			else
				$this->$fun($Mode);
		}

		if (isset($this->views[$this->Mode]))
			$this->{$this->views[$this->Mode]}($Mode);
		else
			$this->{$this->views['']}($Mode); // default view

		// this is needed only when we use temporary crud object together with ajax screen updates
		hidden($this->name.'Mode'.'['.$this->selected_id.']', $this->Mode);
		div_end();
	}
	
	//
	//	Optional things like focus set performed on edition cancel.
	//
	function cancel()
	{
	}

	//
	//	Show database content in pager/table
	//	parameter $Mode contains new mode on change, or '' otherwise
	//
	function list_view($Mode) {
		display_notification(__FUNCTION__. ' is not defined...');
	}

	//
	//	Show record editor screen content
	//	parameter $Mode contains new mode on change, or '' otherwise
	//
	function editor_view($Mode) {
		display_notification(__FUNCTION__. ' is not defined...');
	}
};

class selector_crud_view extends simple_crud_view {

	function __construct($name, $data_set = null, $options=array())
	{
		$this->display_both = true;
		parent::__construct($name, $data_set, $options);
	}

	function _check_mode()
	{
		global $Ajax;

		// list controls lookup
		$prev_mode = $this->Mode;
		$mod = '';

		$list = $this->name.'_id';
		$this->prev_id = $this->selected_id;
		$this->selected_id = get_post($list);
		if (list_updated($list)) {
			$Ajax->activate($this->name);
			$Ajax->activate($this->name.'_controls');
			$mod = $this->selected_id == $this->_none ? 'RESET' : 'Edit';
		} else {
			$mod = simple_crud_view::_check_mode();
		}

		if ($mod != $prev_mode) {
			$Ajax->activate($this->name.'_controls');
		}
		if (get_post('_show_inactive_update'))
		{
			$Ajax->activate($this->name.'_id');
		}

		$_POST[$list] = $this->selected_id;
		return $mod;
	}

	function cancel()
	{
		global $Ajax;

		$_POST[$this->name.'_id'] = $this->selected_id = $this->_none;
		$Ajax->activate($this->name.'_id');
	}
}

//
//	Template for line table editors
//

class table_crud_view extends simple_crud_view {

	var $title;

	function __construct($name, &$data_set = null, $options=array())
	{
		$this->display_both = true;
		parent::__construct($name, $data_set, $options);
	}
	/**
	*
	*	Returns selector string from data or current selected_id
	*
	**/
	function curr_id($data=null)
	{
		if ($data)
			return $data ? implode('_', array_intersect_key($data, array_fill_keys($this->key, true))) : null; // php 5.2.0
		else
			return $this->selected_id;
	}
	/**
	*
	*	Show single line from record table
	*
	**/
	function show_record_row($line_no, $record=array())
	{
/* TBD
		$id = $this->curr_id($record); // extract key

		$view = array();
		$this->set_output($line, $view);

		$editables = array();
		if ($this->options['edit'] && $this->Mode=='Edit' && $this->selected_id==$id)
		{
			$editables = $this->data_set->edit_status($key);
		}
		$ctrls = $this->get_field_views($record, $editables);
		alt_table_row_color($k);
		foreach($ctrls as $key => $fld)
		{
			echo '<td>';
			echo $ctrls[$key];
			echo '</td>';
		}

		if ($this->options['edit']) {
			echo '<td>';
			$this->tool_button($this->selected_id==$id ? 'UPDATE' : 'Edit', $id);
			echo '</td>';
		}
		if ($this->options['delete']$this->selected_id==$id) {
			echo '<td>';
			$this->tool_button( ? 'Cancel' : 'Delete', $id);
			echo '</td>';
		}
		end_row();

		if ($this->options['insert'])
		{
			alt_table_row_color($k);
			$this->new_line($line_no, $line);
			end_row();
		}
*/
	}
	function show_list_headers()
	{
	}
	//
	//	Main function - display current CRUD table content
	//
	function show($Mode=null)
	{
		if (!isset($Mode))
			$Mode = $this->_check_mode(true);
		div_start($this->name.'_div');

		if (array_key_exists($Mode, $this->pre_handlers))
		{
			$fun = $this->pre_handlers[$Mode];
			if (is_array($fun))
			{
				if (is_object($fun[0]))
					$fun[0]->$fun[1]($Mode);
			} else
			$this->$fun($Mode);
		}

		if ($this->title)
			display_heading($this->title);

		start_table(TABLESTYLE, "width=95%");
		$this->show_list_headers();

		foreach($this->data_set->get_all() as $line_no => $line) {
			$this->show_record_row($line_no, $line);
		}
		$this->show_record_row();

		end_table();
		hidden($this->name.'Mode'.'['.$this->selected_id.']', $this->Mode);
		div_end();
	}

};
