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

/**
 * Entity Pattern — PK-Agnostic CRUD Base Class with Static Convenience API
 * ========================================================================
 *
 * This file provides the Entity pattern for NotrinosERP: a generic, validation-aware
 * CRUD layer that works with ANY primary key naming convention — no PK column renaming
 * required, no global PK map needed.
 *
 * ## Class Hierarchy
 *
 * ```
 * abstract class record_set          ← Core CRUD engine + validation framework
 *     └── class Entity               ← Concrete base with SQL-backed CRUD
 *         ├── Instance API: get(), get_all(), insert(), update(), delete()
 *         ├── Static API:  ::find(), ::all(), ::create(), ::modify(), ::remove()
 *         ├── class credit_status_entity
 *         ├── class payment_terms_entity
 *         ├── class shippers_entity
 *         ├── class printers_entity
 *         ├── class work_centres_entity
 *         ├── class sales_types_entity
 *         ├── class sales_points_entity
 *         ├── class reflines_db
 *         └── ... (26+ subclasses planned)
 * ```
 *
 * ## Key Features
 *
 * - **PK-Agnostic**: Works with `'id'`, `'debtor_no'`, `'supplier_id'`, composite keys
 *   `array('col1','col2')`, varchar natural keys like `'stock_id'` — no renaming needed.
 * - **Built-in Validation**: Declarative `set_validator()` calls run automatically before
 *   insert/update/delete. Supports required fields, numeric ranges, and custom check methods.
 * - **Static Convenience API**: Every entity subclass inherits five static methods —
 *   `::find()`, `::all()`, `::create()`, `::modify()`, `::remove()` — eliminating the
 *   need for boilerplate wrapper functions in `*_db.inc` files.
 * - **Dual Fetch Modes**: `::all()` returns a plain PHP array (foreach-ready, modern).
 *   `::all_db_resource()` returns a DB resource for streaming large datasets or
 *   backward-compatible `while(db_fetch())` iteration.
 *
 * ## Usage — Static API (preferred for simple CRUD)
 *
 * ```php
 * // Fetch single record — returns assoc array or null
 * $row = credit_status_entity::find($id);
 *
 * // Fetch multiple records — returns plain PHP array (foreach-ready)
 * $rows = credit_status_entity::all('!inactive');
 * foreach ($rows as $myrow) { ... }
 *
 * // Fetch as DB resource — for streaming or backward-compat while(db_fetch())
 * $result = credit_status_entity::all_db_resource('!inactive');
 * while ($myrow = db_fetch($result)) { ... }
 *
 * // Insert / Update / Delete — each runs validators, returns bool
 * credit_status_entity::create(['reason_description' => 'New Status', 'dissallow_invoices' => 0]);
 * credit_status_entity::modify($id, ['reason_description' => 'Updated']);
 * credit_status_entity::remove($id);
 * ```
 *
 * ## Usage — Instance API (for complex multi-table operations)
 *
 * ```php
 * $e = new credit_status_entity();
 * $row = $e->get($id);
 * $result = $e->get_all('!inactive');
 * $e->insert($data);
 * $e->update($id, $data);
 * $e->delete($id);
 *
 * ## PK Column Types Supported
 *
 * | PK Type        | Constructor Example                                    |
 * |----------------|--------------------------------------------------------|
 * | Standard `id`  | `parent::__construct('table', [...], 'id')`            |
 * | Named int PK   | `parent::__construct('table', [...], 'debtor_no')`     |
 * | Natural varchar| `parent::__construct('table', [...], 'stock_id')`      |
 * | Composite PK   | `parent::__construct('table', [...], array('a','b'))`  |
 *
 * ## Validator Syntax
 *
 * Format: `field_name:mode:type[:param1:param2...]`
 *
 * Modes: `i`=insert, `u`=update, `d`=delete, `iu`=both insert+update
 *
 * Built-in types:
 * - `required` — Field must not be empty
 * - `range:min=N:max=M` — Numeric range check
 * - `clean_file_name` — Filename contains no invalid chars
 * - `_check_<name>` — Calls `$this->_check_<name>($data, $opts, $key)` method
 *
 * ```php
 * $this->set_validator('name:iu:required', _("Name cannot be empty."));
 * $this->set_validator('quantity:iu:range:min=0:max=100');
 * $this->set_validator('email:iu:_check_email_unique');
 * ```
 */

// Validator type definitions — optional/required params for each supported validator
// 
$validator_types = array(
	'required' => array(),
	'range' => array('min'=>0, 'max' =>null, 'lt', 'gt', 'le', 'ge')
);

/**
 * Abstract base class for the Entity pattern — provides the core CRUD engine,
 * validation framework, and PK resolution logic. All concrete entity classes
 * extend Entity (which extends this), not this class directly.
 *
 * ## Lifecycle
 *
 * Each CRUD operation follows a validate-then-execute pattern:
 * - `insert($data)`  → `insert_check($data)` → runs validators → `INSERT INTO`
 * - `update($key, $data)` → `update_check($key, $data)` → runs validators → `UPDATE ... WHERE`
 * - `delete($key)`   → `delete_check($key)` → runs validators → `DELETE FROM ... WHERE`
 *
 * Subclasses define validators in their constructor via `set_validator()` before
 * calling `parent::__construct(...)`.
 */
abstract class record_set {
	/**
	 * Primary key column(s) for this entity.
	 * - String: single column name (e.g., 'id', 'debtor_no', 'stock_id')
	 * - Array:  composite key columns (e.g., array('branch_code', 'debtor_no'))
	 * @var string|array
	 */
	var $key;

	/**
	 * List of field names belonging to this entity's table.
	 * Only fields listed here are read/written by get_all/insert/update.
	 * @var array
	 */
	var $fields;

	/**
	 * Table name WITHOUT the TB_PREF prefix.
	 * The prefix is added automatically in SQL generation.
	 * @var string
	 */
	var $name;

	/**
	 * Array of validator definitions registered via set_validator().
	 * Each entry: array('fld'=>..., 'type'=>..., 'msg'=>..., 'opts'=>..., 'mode'=>...)
	 * @var array
	 */
	var $checks = array();

	/**
	 * Currently selected record key (set during get/update/delete operations).
	 * @var mixed
	 */
	var $selected_id;

	/**
	 * Currently loaded record data (populated by get()).
	 * @var array|null
	 */
	var $data;

	/**
	 * Validation errors collected during the current operation.
	 * Keys are field names or numeric indices; values are error messages.
	 * @var array
	 */
	var $errors = array();

	/**
	 * Debug information — database error messages collected during operations.
	 * @var array
	 */
	var $debug = array();

	/**
	 * Optional WHERE clause fragment that restricts ALL queries for this entity.
	 * Example: "company_id = " . user_company() to scope by company.
	 * @var string|null
	 */
	var $subset;

	/**
	 * Reset errors on unserialization (prevents stale error state).
	 */
	function __wakeup() {
		$this->errors = array();
	}

	/**
	 * Construct a new entity instance.
	 *
	 * @param string          $name   Table name WITHOUT TB_PREF prefix (e.g., 'credit_status')
	 * @param array           $fields List of column names belonging to this entity
	 * @param string|array    $key    Primary key column(s): 'id', 'debtor_no', or array('col1','col2')
	 * @param string|null     $subset Optional WHERE clause fragment applied to all queries
	 */
	function __construct($name, $fields, $key, $subset=null) {
		$this->name = $name;
		$this->fields = $fields;
		$this->key = $key;
		$this->subset = $subset;
	}

	/**
	 * Record a validation or database error.
	 *
	 * @param string      $msg   Human-readable error message
	 * @param string|null $field Field name the error relates to, or null for general error
	 * @return bool Always returns false (convenient for `return $this->error(...)`)
	 */
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

	/**
	 * Register a validator that runs before insert/update/delete.
	 *
	 * ## Validator String Format
	 *
	 * `field_name:mode:type[:param1[:param2...]]`
	 *
	 * - **field_name**: Column name to validate
	 * - **mode**: When to run — `i` (insert), `u` (update), `d` (delete), `iu` (insert+update)
	 * - **type**: Built-in type (`required`, `range`, `clean_file_name`) or custom method name `_check_<name>`
	 * - **params**: Optional parameters — `name=value` for named params, bare value for positional
	 *
	 * ## Examples
	 *
	 * ```php
	 * // Required field on insert and update
	 * $this->set_validator('name:iu:required', _("Name cannot be empty."));
	 *
	 * // Numeric range on insert only
	 * $this->set_validator('quantity:i:range:min=0:max=1000');
	 *
	 * // Custom uniqueness check (calls $this->_check_code_unique())
	 * $this->set_validator('code:iu:_check_code_unique', _("Code must be unique."));
	 * ```
	 *
	 * @param string      $check Validator definition string
	 * @param string|null $msg   Error message displayed when validation fails
	 */
	function set_validator($check, $msg=null) {
		$params = explode(':', $check);
		if (count($params)<3)
			display_error(_('Invalid validator string'));

		$fieldname = array_shift($params);
		$mode = array_shift($params);
		$type = array_shift($params);
		$options = array();
		if (count($params)) {
			foreach ($params as $par) {
				if (($n = strpos($par, '=')) !== false)
					$options[substr($par, 0, $n)] = substr($par, $n+1);
				else
					$options[] = $par;
			}
		}
		$this->checks[] = array('fld'=>$fieldname, 'type' => $type, 'msg' => $msg, 'opts' => $options, 'mode'=>$mode);
	}

	/**
	 * Run all registered validators for the given operation mode.
	 *
	 * Automatically determines mode from arguments:
	 * - $data set, $key null  → insert mode ('i')
	 * - $data set, $key set   → update mode ('u')
	 * - $data null, $key set  → delete mode ('d')
	 *
	 * @param mixed       $key  Record key (for update/delete) or null (for insert)
	 * @param array|null  $data Data being validated (for insert/update) or null (for delete)
	 * @return bool True if all validators pass, false on first failure
	 */
	function _validate($key, $data) {
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

					// case 'match':
						break;

					// user defined checks
					default:
						$func = $check['type'];
						if (method_exists($this, $func)) {
							if (!$this->$func($data, $check['opts'], $key))
								return $this->error($msg, $check['fld']);
						}
						else if (function_exists($func)) {
							if (!$func($data, $key, $check['opts'], $key))
								return $this->error($msg, $check['fld']);
						}
				}
			}
		}
		return true;
	}

	/**
	 * Return which fields of the given record are editable.
	 *
	 * By default, all fields EXCEPT the primary key column(s) are editable.
	 * Subclasses may override this to implement row-level edit restrictions.
	 *
	 * @param mixed $key Record key
	 * @return array Associative array of field_name => bool (true=editable, false=readonly)
	 */
	function edit_status($key) {
		$editables = array();

		// default: all but key fields editable 
		foreach ($this->fields as $fld=>$val)
			$editables[$fld] = !in_array($fld, (array)$this->key);

		return $editables;
	}

	/**
	 * Check whether the given record can be deleted.
	 * Runs validators registered with 'd' mode.
	 *
	 * @param mixed $key Record key
	 * @return bool True if deletion is allowed
	 */
	function delete_check($key) {
		return $this->_validate($key, null);
	}

	/**
	 * Check whether the given data can be inserted.
	 * Runs validators registered with 'i' mode.
	 *
	 * @param array $data Data to validate for insertion
	 * @return bool True if data passes all insert validators
	 */
	function insert_check($data) {
		return $this->_validate(null, $data);
	}

	/**
	 * Check whether the given record can be updated with the provided data.
	 * Runs validators registered with 'u' mode.
	 *
	 * Note: This does NOT allow changing the primary key value.
	 *
	 * @param mixed $key  Record key
	 * @param array $data Proposed new data
	 * @return bool True if data passes all update validators
	 */
	function update_check($key, $data) {
		return $this->_validate($key, $data);
	}

	//===========================================================================
	// Database operation stubs — overridden by Entity class
	
	/**
	 * Get a single record by primary key.
	 *
	 * Abstract base version returns only default field values.
	 * Overridden by Entity with full SELECT query.
	 *
	 * @param mixed|null $key Primary key value(s), or null for defaults only
	 * @return array|null Associative array of field_name => value, or null if not found
	 */
	function get($key=null) {
		$defaults = array();
		foreach ($this->fields as $name => $def) {
			if(!is_numeric($name)) {
				if (is_string($def))
					$defaults[$name] = $def;
				elseif (isset($def['dflt']))
					$defaults[$name] = $def['dflt'];
			}
		}
		return $defaults;
	}

	/**
	 * Get multiple records with optional filtering and ordering.
	 * @abstract Must be implemented by concrete subclass.
	 *
	 * @param string|null          $where    SQL WHERE clause (without WHERE keyword)
	 * @param string|array|null    $order_by SQL ORDER BY clause or array of column names
	 * @return mixed Database result set
	 */
	abstract function get_all();

	/**
	 * Update a record — validates then executes UPDATE.
	 *
	 * @param mixed $key  Record key
	 * @param array $data New field values (only fields listed in $this->fields are updated)
	 * @return bool True on success, false on validation failure or DB error
	 */
	function update($key, $data) {
		if (!$this->update_check($key, $data))
			return false;

		return true;
	}

	/**
	 * Delete a record — validates then executes DELETE.
	 *
	 * @param mixed $key Record key
	 * @return bool True on success, false if deletion is not allowed
	 */
	function delete($key) {
		if (!$this->delete_check($key))
			return false;

		return true;
	}

	/**
	 * Insert a record — validates then executes INSERT.
	 *
	 * @param array $data Field values to insert
	 * @return bool True on success, false on validation failure
	 */
	function insert($data) {
		if (!$this->insert_check($data))
			return false;

		return true;
	}

}

/**
 * Concrete Entity base class — provides full CRUD implementation backed by SQL queries.
 *
 * This is the class that ALL module entities MUST extend. It implements both an
 * instance API (for complex multi-table operations) and a static convenience API
 * (for simple single-table CRUD call sites).
 *
 * ## Instance API (Use for Complex Operations)
 *
 * - `get($key)` — SELECT single record by PK (supports single + composite keys)
 * - `get_all($where, $order_by)` — SELECT multiple records with WHERE/ORDER BY
 * - `insert($data)` — INSERT with validation + auto-column filtering
 * - `update($key, $data)` — UPDATE by PK with validation + auto-column filtering
 * - `delete($key)` — DELETE by PK with validation
 *
 * ## Static Convenience API (Use for Simple CRUD Call Sites)
 *
 * - `::find($key)` — Fetch single record by PK; returns assoc array or null
 * - `::all($where, $order_by)` — Fetch multiple records as a **plain PHP array** (foreach-ready)
 * - `::all_db_resource($where, $order_by)` — Fetch multiple records as a **DB resource**
 *   for backward-compatible `while(db_fetch())` iteration or streaming large datasets
 * - `::create($data)` — Insert a new record with validation; returns bool
 * - `::modify($key, $data)` — Update a record by PK with validation; returns bool
 * - `::remove($key)` — Delete a record by PK (runs delete_check first); returns bool
 *
 * ## Usage Pattern (Static API Recommended)
 *
 * ```php
 * // Entity subclass — one file per table, no wrapper functions needed
 * class my_entity extends Entity {
 *     function __construct() {
 *         $this->set_validator('name:iu:required');
 *         parent::__construct('table_name', array('name', 'value', 'id', 'inactive'), 'id');
 *     }
 * }
 *
 * // Call sites — clean static calls, no global get_* / add_* / update_* / delete_* functions
 * $row  = my_entity::find($id);
 * $rows = my_entity::all('!inactive');          // plain array, foreach-ready
 * $res  = my_entity::all_db_resource('!inactive'); // DB resource, while(db_fetch())
 * my_entity::create(['name' => 'Foo', 'value' => 42]);
 * my_entity::modify($id, ['name' => 'Bar']);
 * my_entity::remove($id);
 * ```
 *
 * ## Multi-Table Transactions
 *
 * For entities that span multiple tables (e.g., tax groups with child items),
 * override `::create()`, `::modify()`, and `::remove()` in the subclass to wrap
 * the parent call in `begin_transaction()`/`commit_transaction()`:
 *
 * ```php
 * class tax_group_entity extends Entity {
 *     function __construct() {
 *         parent::__construct('tax_groups', array('name', 'id', 'inactive'), 'id');
 *     }
 *
 *     public static function create($data) {
 *         begin_transaction();
 *         $e = new static();
 *         $result = $e->insert($data);
 *         if ($result && !empty($data['taxes'])) {
 *             $e->_add_items(db_insert_id(), $data['taxes']);
 *         }
 *         $result ? commit_transaction() : rollback_transaction();
 *         return $result;
 *     }
 * }
 * ```
 */
class Entity extends record_set {

	/**
	 * @param string          $name   Table name without TB_PREF prefix
	 * @param array           $fields Column names belonging to this entity
	 * @param string|array    $key    PK column(s): 'id', 'debtor_no', or array('col1','col2')
	 * @param string|null     $subset Optional WHERE clause applied to all queries
	 */
	function __construct($name, $fields, $key, $subset=null) {
		parent::__construct($name, $fields, $key, $subset);
	}

	/**
	 * Get a single record by primary key.
	 *
	 * For composite keys, pass an associative array: array('col1'=>val1, 'col2'=>val2).
	 * For single-column keys, pass the scalar value directly.
	 *
	 * @param mixed|null $key Primary key value(s). Null returns default field values.
	 * @return array|null Associative array of field_name => value, or null if not found
	 */
	function get($key=null) {
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
		}
		else
			$where = array($this->key => "`".$this->key."`=".db_escape($key));

		$sql .= implode(' AND ', $where);
		$result = db_query($sql);
		if (!$result)
			return $this->error("Cannot get record from ".$this->name);

		return $rec = db_num_rows($result) ? db_fetch_assoc($result) : null;
	}

	/**
	 * Get multiple records with optional filtering and ordering.
	 *
	 * @param string|null          $where    SQL WHERE clause WITHOUT the WHERE keyword.
	 *                                        Example: "!inactive" or "category_id = 5"
	 * @param string|string[]|null $order_by Column name, array of column names, or null
	 * @return mixed Database result set (pass to db_fetch() in a loop), or false on error
	 */
	function get_all($where=null, $order_by=null) {
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

	/**
	 * Update a record by primary key.
	 *
	 * Only fields listed in $this->fields are written to the database.
	 * Fields in $data that are not in the fields list are silently ignored.
	 *
	 * @param mixed $key  Primary key value(s)
	 * @param array $data Associative array of field_name => new_value
	 * @return bool True on success, false on validation failure or DB error
	 */
	function update($key, $data) {
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
		}
		else
			$where = array("`".$this->key."`=".db_escape($key));

		$sql .= implode(' AND ', $where);
		$result = db_query($sql);

		if ($result===false)
			return $this->error("cannot update record in ".$this->name);

		return $result;
	}

	/**
	 * Delete a record by primary key.
	 *
	 * Runs delete_check() first — if validators reject, deletion is blocked.
	 *
	 * @param mixed $key Primary key value(s)
	 * @return bool True on success, false if deletion is not allowed or on DB error
	 */
	function delete($key) {
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
		}
		else
			$where = array("`".$this->key."`=".db_escape($key));

		$sql .= " WHERE ".implode(' AND ', $where);
		$result = db_query($sql);
		if (!$result)
			return $this->error(_("Cannot update record in ").$this->name);

		return $result;
	}

	/**
	 * Insert a new record.
	 *
	 * Only fields listed in $this->fields (and the PK column) are written.
	 * Other keys in $data are silently ignored.
	 *
	 * @param array $data Associative array of field_name => value
	 * @return bool True on success, false on validation failure or DB error
	 */
	function insert($data) {
		if (!parent::insert_check($data))
			return false;

		$sql = "INSERT INTO ".TB_PREF.$this->name. ' (';
		$fields = array();
		foreach($data as $fld => $value) {
			if (in_array($fld, $this->fields) || (is_array($this->key) ? in_array($this->key) : $fld==$this->key))
				$fields["`$fld`"] = db_escape($value);
		}
		if (!count($fields))
			return $this->error(_("Empty data set for insertion into ").$this->name);

		$sql .= implode(',', array_keys($fields)) .') VALUES ('. implode(',', $fields).')';

		$result = db_query($sql);
		if (!$result)
			return $this->error(_("Cannot insert record into ").$this->name);

		return $result;
	}

	/**
	 * Fetch a single record by primary key. Returns assoc array or null.
	 *
	 * @param mixed|null $key Primary key value(s)
	 * @return array|null Associative array of field_name => value, or null if not found
	 */
	public static function find($key=null) {
		$e = new static();
		return $e->get($key);
	}

	/**
	 * Fetch multiple records. Returns a plain PHP array (foreach-ready).
	 *
	 * @param string|null          $where    SQL WHERE clause (without WHERE keyword)
	 * @param string|string[]|null $order_by Column name or array of column names
	 * @return array Array of associative arrays
	 */
	public static function all($where=null, $order_by=null) {
		$e = new static();
		$result = $e->get_all($where, $order_by);
		if (!$result) return array();
		$rows = array();
		while ($row = db_fetch($result)) {
			$rows[] = $row;
		}
		return $rows;
	}

	/**
	 * Fetch multiple records as a DB resource, for streaming/backward-compat
	 * with while(db_fetch()) loops.
	 *
	 * @param string|null          $where    SQL WHERE clause
	 * @param string|string[]|null $order_by Column name or array of column names
	 * @return mixed DB result set, or false on error
	 */
	public static function all_db_resource($where=null, $order_by=null) {
		$e = new static();
		return $e->get_all($where, $order_by);
	}

	/**
	 * Insert a new record. Validates before insert. Returns bool.
	 *
	 * @param array $data Associative array of field_name => value
	 * @return bool True on success, false on validation failure or DB error
	 */
	public static function create($data) {
		$e = new static();
		return $e->insert($data);
	}

	/**
	 * Update a record by primary key. Validates before update. Returns bool.
	 *
	 * @param mixed $key  Primary key value(s)
	 * @param array $data Associative array of field_name => new_value
	 * @return bool True on success, false on validation failure or DB error
	 */
	public static function modify($key, $data) {
		$e = new static();
		return $e->update($key, $data);
	}

	/**
	 * Delete a record by primary key. Runs delete_check() first. Returns bool.
	 *
	 * @param mixed $key Primary key value(s)
	 * @return bool True on success, false if deletion is not allowed or on DB error
	 */
	public static function remove($key) {
		$e = new static();
		return $e->delete($key);
	}

	/**
	 * Check whether any records exist in this entity's table.
	 *
	 * Returns true if the table contains at least one row.
	 * Useful for gating forms that require seed data (e.g. "define X before Y").
	 *
	 * @return bool True if at least one record exists.
	 */
	public static function has_records() {
		$e = new static();
		return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF.$e->name);
	}
}

/**
 * Array-backed data set — stores records in a PHP array instead of a database table.
 *
 * TODO: default to fields = ReflectionClass->getProperties
 */
class array_set extends record_set {

	var $array = array();

	var $object_class;	// name of record object class or null for arrays

	function __construct($name, $fields, $key=null, &$arr=array(), $class = null) {
		$this->array = &$arr;
		$this->object_class = $class;
		parent::__construct($name, $fields, $key);
	}

	//
	//	Get a record by key from the in-memory array
	//
	function get($key=null) {
		if ($key===null)
			return $this->array;
		return isset($this->array[$key]) ? $this->array[$key] : null;
	}

	//
	//	List all records (optionally filtered)
	//
	function get_all($where=null, $order=null) {
		return $this->array;
	}

	//
	//	Insert a record into the in-memory array
	//
	function insert($data, $key=null) {
		if (!parent::insert_check($data))
			return false;
		if ($key !== null) {
			$this->array[$key] = $data;
			return $key;
		}
		$this->array[] = $data;
		return count($this->array) - 1;
	}

	//
	//	Update a record in the in-memory array by key
	//
	function update($key, $data) {
		if (!parent::update($key, $data))
			return false;
		$this->array[$key] = $data;
		return true;
	}

	//
	//	Delete a record from the in-memory array by key
	//
	function delete($key) {
		if (!parent::delete_check($key))
			return false;
		unset($this->array[$key]);
		return true;
	}
}
