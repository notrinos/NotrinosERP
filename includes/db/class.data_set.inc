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

// array contains optional/required params for all supported validator types
// 
$validator_types = array(
	'required' => array(),
	'range' => array('min'=>0, 'max' =>null, 'lt', 'gt', 'le', 'ge')
);

/*
	Generic record set object
*/
abstract class record_set {
	var $key;			// unique key fields
	var $fields;		// other fields
	var $name;			// table name (without prefix)
	var $checks = array();		// checks on data before insert/update/delete

	var $selected_id;	// current key
	var $data;			// current record data
	var $errors = array();
	var $debug = array();
	var $subset;		// optional where clause for record subset

	function __wakeup()
	{
		$this->errors = array();
	}

	function __construct($name, $fields, $key, $subset=null)
	{
		$this->name = $name;
		$this->fields = $fields;
		$this->key = $key;
		$this->subset = $subset;
	}

	function error($msg, $field=null) {
		global $db;

		// save error message
		if (!isset($field))
			$field = count($this->errors);
		$this->errors[$field] = $msg;

		// save db errors for debugging purposes
		if ($db && db_error_no()) $this->debug[] = db_error_msg($db);

		return false;
	}
	//==============================================================================================
	// 
	// Data valiation

	/**
	*	Set validator for insert/update/delete
	*
	*	@check - validator description string in format: 'field_name:uid_modes:check_type[[:param1][:param2]...]'
	*   parameters can be expressed with name in form: name=value
	*	@msg - message stored on error
	**/
	
	function set_validator($check, $msg=null)
	{
		$params = explode(':', $check);
		if (count($params)<3)
			display_error(_('Invalid validator string'));

		$fieldname = array_shift($params);
		$mode = array_shift($params);
		$type = array_shift($params);
		$options = array();
		if (count($params))
		{
			foreach ($params as $par) {
				if (($n = strpos($par, '=')) !== false) {
					$options[substr($par, 0, $n)] = substr($par, $n+1);
				} else
					$options[] = $par;
			}
		}
		$this->checks[] = array('fld'=>$fieldname, 'type' => $type, 'msg' => $msg, 'opts' => $options, 'mode'=>$mode);
	}

	/**
	*	Validate data
	*	@key - record key (u/d)
	*	@data - data to be validated (i/u)
	**/
	function _validate($key=null, $data)
	{
		$mode = isset($data) ? (isset($key) ? 'u' : 'i') : 'd';

		foreach($this->checks as $check) {

			if (strpos($check['mode'], $mode) !== false) {
				$msg = $check['msg'];
				$fld = $check['fld'];
				$opts = @$check['opts'];

				// precheck for existing 
				if ($mode == 'i' && $fld && !isset($data[$fld])) {
					$msg = sprintf(_("Input parameter '%s' have to be set."), $check['fld']);
					return $this->error($msg, $check['fld']);
				}

				switch($check['type']) {

					case 'required':
						if ($data[$fld]==='') {
							if (!$msg) $msg = sprintf(_("Parameter '%s' cannot be empty.", $check['fld']));
								return $this->error($msg, $check['fld']);
						}
						break;

					case 'clean_file_name':
						if (isset($data[$fld]) && ($data[$fld] != clean_file_name($data[$fld]))) {
							if (!$msg) $msg = sprintf(_("Parameter '%s' contains invalid characters.", $check['fld']));
							return $this->error($msg, $check['fld']);
						}
						break;

					case 'range':
						if (!$msg) $msg = sprintf(_("Parameter '%s' has invalid value.", $check['fld']));
						// TODO: check at least one named parameter is passed
						if (isset($opts['lt']) && !($data[$fld] < $opts['lt']) ||
							(isset($opts['gt']) && !($data[$fld] > $opts['gt'])) ||
							(isset($opts['min']) && !($data[$fld] < $opts['min'])) ||
							(isset($opts['max']) && !($data[$fld] > $opts['max'])) )
								return $this->error($msg, $check['fld']);

//					case 'match':
						break;

					// user defined checks
					default:
						$func = $check['type'];
						if (method_exists($this, $func)) {
							if (!$this->$func($data, $check['opts'], $key))
								return $this->error($msg, $check['fld']);
						} else if (function_exists($func)) {
							if (!$func($data, $key, $check['opts'], $key))
								return $this->error($msg, $check['fld']);
						}
				}
			}
		}
		return true;
	}

	/**
	*
	*	Returns editable status for selected record.
	*	Array contains true for editable, false for readonly fields.
	*	This looks redundant ('forbidden' update_check could be used)
	*	but in fact the constraints are related to exact record, so changes with key.
	**/
	function edit_status($key)
	{
		$editables = array();

		// default: all but key fields editable 
		foreach ($this->fields as $fld=>$val)
			$editables[$fld] = !in_array($fld, (array)$this->key);

		return $editables;
	}


	function delete_check($key)
	{
		return $this->_validate($key, null);
	}

	function insert_check($data)
	{
		return $this->_validate(null, $data);
	}

	function update_check($key, $data)
	{
		// Note: this does not allow change of key
		return $this->_validate( $key, $data);
	}

	//===========================================================================
	//	Database functions placeholders
	
	//
	//	Generic read record routine
	//
	function get($key=null)
	{
		$defaults = array();
		// return all defined default values
		foreach ($this->fields as $name => $def)
		{
			if(!is_numeric($name)) {
				if (is_string($def))
					$defaults[$name] = $def;
				elseif (isset($def['dflt']))
					$defaults[$name] = $def['dflt'];
			}
		}
		return $defaults;
	}
	//
	//	Generic list record routine
	//
	abstract function get_all();
	//
	//	Generic update record routine
	//
	function update($key, $data)
	{
		if (!$this->update_check($key, $data))
			return false;

		return true;
	}
	//
	//	Generic delete record routine
	//
	function delete($key)
	{
		if (!$this->delete_check($key))
			return false;

		return true;
	}
	//
	//	Insert record
	//
	function insert($data)
	{
		if (!$this->insert_check($data))
			return false;

		return true;
	}

}

class data_set extends record_set {

	function __construct($name, $fields, $key, $subset=null)
	{
		parent::__construct($name, $fields, $key, $subset);
	}

	//
	//	Generic read record routine
	//
	function get($key=null)
	{
		if ($key===null)
			return parent::get();

		$sql = "SELECT * FROM ".TB_PREF.$this->name." WHERE ";
		$where = $this->subset ? (array)$this->subset : array();

		if (is_array($this->key)) {
			foreach($this->key as $fld)
				if (isset($key[$fld]))
					$where[$fld] = "`$fld`=".db_escape($key[$fld]);
				else
					return $this->error(sprintf(_("Invalid key passed reading '%s'"), $this->name));
		} else {
			$where = array($this->key => "`".$this->key."`=".db_escape($key));
		}

		$sql .= implode(' AND ', $where);
		$result = db_query($sql);
		if (!$result)
			return $this->error("Cannot get record from ".$this->name);

		return $rec = db_num_rows($result) ? db_fetch_assoc($result) : null;
	}
	//
	//	Generic list record routine
	//
	function get_all($where=null, $order_by=null)
	{
		$fields = array();
		foreach($this->fields as $fld)
			$fields[] = '`'.$fld.'`';
		$sql = "SELECT ".implode(',', $fields)." FROM ".TB_PREF.$this->name;

		if ($where)
			$sql .= " WHERE ".($this->subset ? '('.$this->subset . ') AND ' : ''). $where;
		if ($order_by) {
			$order_by = (array)$order_by;
			foreach($order_by as $i => $fld)
				$order_by[$i] = '`'.$fld.'`';
			$sql .= " ORDER BY ".implode(',', (array)$order_by);
		}
		$result = db_query($sql);
		if ($result==false)
			return $this->error("Cannot get record from ".$this->name);

		return $result;
	}
	//
	//	Generic update record routine
	//
	function update($key, $data)
	{
		if (!parent::update($key, $data))	// validate data
			return false;

		$sql = "UPDATE ".TB_PREF.$this->name." SET ";
		$updates = array();

		foreach($data as $fld => $value) {	// select only data relevant for this model
			if (in_array($fld, $this->fields))
				$updates[$fld] = "`$fld`=".db_escape($value);
		}
		if (count($updates) == 0)
			return $this->error(_("Empty update data for table ").$this->name);

		$sql .= implode(',', $updates)." WHERE ";
		$where = $this->subset ? (array)$this->subset : array();

		if(is_array($this->key)) {			// construct key phrase
			foreach($this->key as $fld)
				if (isset($key[$fld]))
					$where[$fld] = "`$fld`=".db_escape($key[$fld]);
				else
					return $this->error(sprintf(_("Invalid key for update '%s'"), $this->name));
		} else {
			$where = array("`".$this->key."`=".db_escape($key));
		}

		$sql .= implode(' AND ', $where);
		$result = db_query($sql);

		if ($result===false)
			return $this->error("cannot update record in ".$this->name);

		return $result;
	}
	//
	//	Generic delete record routine
	//
	function delete($key)
	{
		if (!parent::delete_check($key))
			return false;

		$sql = "DELETE FROM ".TB_PREF.$this->name;
		$where = $this->subset ? (array)$this->subset : array();

		if(is_array($this->key)) {
			foreach($this->key as $fld)
				if (isset($key[$fld]))
					$where[$fld] = "`$fld`=".db_escape($key[$fld]);
				else
					return $this->error(sprintf(_("Invalid key for update '%s'"), $this->name));
		} else {
			$where = array("`".$this->key."`=".db_escape($key));
		}

		$sql .= " WHERE ".implode(' AND ', $where);
		$result = db_query($sql);
		if (!$result)
			return $this->error(_("Cannot update record in ").$this->name);

		return $result;
	}
	//
	//	Insert record
	//
	function insert($data)
	{
		if (!parent::insert_check($data))
			return false;

		$sql = "INSERT INTO ".TB_PREF.$this->name. ' (';
		$fields = array();
		foreach($data as $fld => $value) {
			if (in_array($fld, $this->fields) || (is_array($this->key) ? in_array($this->key) : $fld==$this->key))
				$fields["`$fld`"] = db_escape($value);
		}
		if (!count($fields))
			return $this->error(_("Empty data set for insertion into ".$this->name));

		$sql .= implode(',', array_keys($fields)) .') VALUES ('. implode(',', $fields).')';

		$result = db_query($sql);
		if (!$result)
			return $this->error(_("Cannot insert record into ").$this->name);

		return $result;
	}

}

/**
*
*	Data set as array of arrays/objects
*
*  TODO: default to: fields = ReflectionClass->getProperties
**/
class array_set extends record_set {

	var $array = array();

	var $object_class;	// name of record object class or null for arrays

	function __construct($name, $fields, $key=null, &$arr=array(), $class = null)
	{
		$this->array = &$arr;
		$this->object_class = $class;
		parent::__construct($name, $fields, $key);
	}

	//===========================================================================
	//	Database functions placeholders

	//
	//
	//
	function get($key=null)
	{
		if ($key===null)
			return parent::get();

		return @$this->array[$key];
	}
	//
	//	Generic list record routine
	//
	function get_all()
	{
		return $this->array;
	}

	function _set_record($data, $record = null)
	{
		if (!isset($record)) {
			if ($this->object_class) {
				$record = new $this->object_class;
			}
			else
				$record = array();
		}
		foreach(array_merge($this->fields, (array)$this->key) as $n => $fld)
		{
			if (!is_numeric($n))
				$fld = $n;
			if (array_key_exists($fld, $data))
			{
				if ($this->object_class)
					$record->$fld = $data[$fld];
				else
					$record[$fld] = $data[$fld];
				$updates = true;
			}
		}
		return $updates ? $record : null;
	}
	//
	//	Generic update record routine
	//
	function update($key, $data)
	{
		if (parent::update($key, $data) === false)
			return false;

		$record = $this->_set_record($data, $this->array[$key]);
		if (!$record)
			return $this->error(_("Empty update data for array ").$this->name);

		$this->array[$key] = $record;

		return true;
	}
	//
	//	Delete record
	//
	function delete($key)
	{
		if (!parent::delete($key))
			return false;

		unset($this->array[$key]);

		return true;
	}
	//
	//	Insert record
	//
	function insert($data)
	{
		if (parent::insert($data) === false)
			return false;

		$record = $this->_set_record($data);
		if (!$record)
			return $this->error(_("Empty data for array ").$this->name);

		$this->array[] = $record;

		$ret = array_keys($this->array);
		return end($ret);
	}
}

