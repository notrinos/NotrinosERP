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
//	Controler part of database table pager with column sort.
//	To display actual html object call display_db_pager($name) inside
//  any form.
//
//	View definition you will find in the following file:
include_once($path_to_root."/includes/ui/db_pager_view.inc");

class db_pager {
	var $sql;
	var $name;
	var $columns;		// column definitions (head, type, order)

	var $marker;		// marker check function
	var $marker_txt;	
	var $marker_class;
	var $notice_class;
	var $width;			// table width (default '95%')
	var $header_fun;	// additional row between title and body
	var $header_class;
    var $row_fun;       // Function for row preprocessing
	var $footer_fun;
	var $footer_class;
	var $data = array();

	var $curr_page,
		$max_page,
	    $last_page, 
	    $prev_page, 
	    $next_page,
	    $first_page;
	    
	var $page_len,
	    $rec_count;
	
	var $select,
		$where,
	    $from,
		$group,
		$order;
	var	$extra_where = array();
	
	var $ready = false; // this var is false after change in sql before first
						// and before first query.
	var $inactive_ctrl = false;
	var $main_tbl;		// table and key field name for inactive ctrl and edit/delete links
	var $key;	// key field name
	
	//  db_pager constructor
	//  accepts $sql query either as:
	//  a. string in form 'SELECT field_list FROM table_joins [WHERE conditions [GROUP group_list [ORDER order_list]]]'
	//		- if WHERE keyword is used in table_joins, WHERE conditions is obligatory
	//  b. associative array using select, where, group and order keys ex :
	//  	array('select' => 'SUM(quantity)', 'from' => TB_PREF."stock_moves", 'group' => 'location')
	//
	//	$name is base name for pager controls
	function __construct($sql, $name, $table = null, $page_len=0) 
	{
		$this->width = "95%";
		if ($page_len == 0) $page_len = user_query_size();
		$this->name = $name;
		$this->page_len = $page_len;
	 	$this->set_sql($sql);
	}
	//
	//	Parse base sql select query	or use an associative array.
	//
	function set_sql($sql)
	{
		global $SysPrefs;

		if ($sql != $this->sql) {
		    $this->sql = $sql;
		    $this->ready = false;

				if(is_array($sql)) {
					foreach(explode(' ', 'select from where group order') as $section) {
						$this->$section = @$sql[$section];
					}
					$this->select = "SELECT ".$this->select;
				}
				else {
					// parse the query
					$parts = preg_split('/\sFROM\s/si', $sql, 2);
					if (count($parts) == 2) {
						$this->select = $parts[0];
						$sql = $parts[1];
					} else {
						if ($SysPrefs->go_debug)
							display_error("Invalid sql input for db_pager");
					}

					$parts = preg_split('/\sWHERE(?!.*WHERE.*)\s/si', $sql, 2); // last occurence
					if(count($parts) == 2) {
						$this->from = $parts[0];
						$sql = $parts[1];

						$parts = preg_split('/\sORDER\s*BY\s/si', $sql, 2);
						if(count($parts) == 2) {
							$sql = $parts[0];
							$this->order = $parts[1];
						}
						$parts = preg_split('/\sGROUP\s*BY\s/si', $sql, 2);
						if(count($parts) == 2) {
							$sql = $parts[0];
							$this->group = $parts[1];
						}
						$this->where = $sql;
					}
			}
		}
	}
	//
	//	Set additional constraint on record set
	//
	function set_where($where = null)
	{
	 	if ($where) {
	    	if (!is_array($where))
			  $where = array($where);

		    if (count($where) == count($this->extra_where) &&
				!count(array_diff($this->extra_where, $where)))
				 return;
		}
		$this->extra_where = $where;
		$this->ready = false;
	}
	//
	//	Set query result page
	//
	function change_page($page=null) 
	{
	    $this->set_page($page);
	    $this->query();
	    return true;
	}
	//
	//	Change sort column direction 
	//	in order asc->desc->none->asc
	//
	function sort_table($col) 
	{

			$max_priority = 0;
			foreach($this->columns as $id => $_col) {
			if(!isset($_col['ord_priority'])) continue;
				$max_priority = max($max_priority, $_col['ord_priority']);
			};


	    $ord = $this->columns[$col]['ord'];
  		$this->columns[$col]['ord_priority']  = $max_priority+1; // set priority , higher than anything else
	    $ord = ($ord == '') ? 'asc' : (($ord == 'asc') ? 'desc' : '');
	    $this->columns[$col]['ord'] = $ord;
	    $this->set_page(1);
	    $this->query();
	    return true;
	}
	//
	// Query database
	//
	function query() 
	{
		global $Ajax;

		$Ajax->activate("_{$this->name}_span");
	    $this->data = array();
	    if (!$this->_init()) 
		  return false;

	    if ($this->rec_count == 0) return true;

	    $sql = $this->_sql_gen(false);

	    $result = db_query($sql, 'Error browsing database: '.$sql );

	    if ($result) {
		// setting field names for subsequent queries
			$c = 0;
		  	// add result field names to column defs for 
		  	// col value retrieve and sort purposes 
		   	$cnt = min(db_num_fields($result), count($this->columns));
			for ($c = $i = 0; $c < $cnt; $c++) {
				if (!(isset($this->columns[$c]['insert']) && $this->columns[$c]['insert'])) {
//					if (!@($this->columns[$c]['type']=='skip'))
			 			$this->columns[$c]['name']= db_field_name($result, $i);
					if (!@($this->columns[$c]['type']=='insert'))
			 			$i++;
			 	}
		  	}
		  	while ($row = db_fetch_assoc($result)) {
				$this->data[] = $row;
		  	}
		} else 
		  	return false;
		return true;
	}
	//
	//	Calculates page numbers for html controls.
	//
	function set_page($to) 
	{
	    switch($to) {
			case 'next':
			    $page = $this->curr_page+1; break;
			case 'prev':
			    $page = $this->curr_page-1; break;
			case 'last':
			    $page = $this->last_page; break;
			default:
			    if (is_numeric($to)) {
				 	$page = $to; break;
			    }
			case 'first':
			    $page = 1; break;
	    }
	  	if ($page < 1) 
	    	$page = 1;
	  	$max = $this->max_page;
	  	if ($page > $max) 
	    	$page = $max;
	  	$this->curr_page = $page;
	  	$this->next_page = ($page < $max) ? $page+1 : null;
	  	$this->prev_page = ($page > 1) ? ($page-1) : null;
	  	$this->last_page = ($page < $max) ? $max : null;
	  	$this->first_page = ($page != 1) ? 1: null;
	}
	//
	//	Set column definitions
	//  $flds: array( fldname1, fldname2=>type,...)
	function set_columns($flds)
	{
		$this->columns = array();
		if (!is_array($flds)) {
			$flds = array($flds);
		}
		foreach ($flds as $colnum=>$coldef) {
			if (is_string($colnum)) {	// 'colname'=>params
			  	$h = $colnum;
			  	$c = $coldef;
			} else {			//  n=>params
				if (is_array($coldef)) {
					$h = '';
					$c = $coldef;
				} else {
					$h = $coldef;
					$c = 'text';
				}
			}
			if (is_string($c)) 			// params is simple column type
			  	$c = array('type'=>$c);

			if (!isset($c['type']))
			  	$c['type'] = 'text';

			switch($c['type']) {
				case 'inactive': 
					$this->inactive_ctrl = true;
				case 'insert':
				default:
					$c['head'] = $h; break;
				case 'skip':		// skip the column (no header)
					unset($c['head']); break;
	  		}
			$this->columns[] = $c;	
		}
	}
	//
	// Generate db query from base sql
	// $count==false - for current page data retrieval 
	// $count==true  - for total records count
	//
	function _sql_gen($count=false) 
	{
		$select = $this->select;
		$from = $this->from;
		$where = $this->where;
		$group = $this->group;
		$order = $this->order;

		if(count($this->extra_where)) {
		    $where .= ($where=='' ? '' : ' AND ')
				.implode(' AND ', $this->extra_where);
		}
		if ($where) $where = " WHERE ($where)";

		if ($count)
			return "SELECT COUNT(*) FROM ($this->sql) tmp_count";

		$sql = "$select FROM $from $where";
		if ($group) $sql.= " GROUP BY $group";
	    $ord = array();

		// sort order column by priority instead of table order.
		$columns = array();
	    foreach ($this->columns as $col) {
			if(isset($col['ord_priority'])) {
				$columns[$col['ord_priority']] = $col;
			}
		}
		krsort($columns);

	    foreach ($columns as $col) {
	       	if (isset($col['ord'])) {
	        	if ( $col['ord'] != '' && isset($col['name'])) {
		    	    $ord[] = $col['name'] .' '. $col['ord'];
			    }
			}
	    }

	    if (count($ord)) {
			$ord = array_map('db_escape_function', $ord);
			$sql .= " ORDER BY " . implode(',', $ord);
		} else {
			if($order)
				$sql .= " ORDER BY $order"; // original base query order
		}

	    $page_len = $this->page_len;
	    $offset = ($this->curr_page - 1) * $page_len;

	    $sql .= " LIMIT $offset, $page_len";

		return $sql;
		
	}
	//
	//	Initialization after changing record set
	//
	function _init() 
	{
		global $SysPrefs;
		
	    if ($this->ready == false ) {
			$sql = $this->_sql_gen(true);
			$result = db_query($sql, 'Error reading record set');
			if ($result == false) 
				return false;
			$row = db_fetch_row($result);
			$this->rec_count = $row[0];
	  		$this->max_page = $this->page_len ?
				ceil($this->rec_count/$this->page_len) : 0;
		
			if ($SysPrefs->go_debug) { // FIX - need column name parsing, but for now:
				// check if field names are set explicite in col def
				// for all initially ordered columns
			    foreach ($this->columns as $col) {
		    	   	if (isset($col['ord']) && $col['ord'] != '' 
						&&  !isset($col['name'])) {
							display_warning("Result field names must be set
								for all initially ordered db_pager columns.");
			    	}
				}
	    	}
	  		$this->set_page(1);
	  		$this->ready = true;
	    }
    	return true;
	}
	//
	//	Set current page in response to user control.
	//
	function select_records() 
	{
		global $Ajax;
		
		$page = find_submit($this->name.'_page_', false);
		$sort = find_submit($this->name.'_sort_', true);
		if ($page) {
			$this->change_page($page);
			if ($page == 'next' && !$this->next_page ||
				$page == 'last' && !$this->last_page)
					set_focus($this->name.'_page_prev');
			if ($page == 'prev' && !$this->prev_page ||
				$page == 'first' && !$this->first_page)
					set_focus($this->name.'_page_next');
		} elseif ($sort != -1) {
			$this->sort_table($sort);
		} else
			$this->query();
	}
	//
	//	Set check function to mark some rows.
	//	
	function set_marker($func, $notice='', $markercl='overduebg', $msgclass='overduefg' )
	{
		$this->marker = $func;
		$this->marker_txt = $notice;
		$this->marker_class = $markercl;
		$this->notice_class = $msgclass;
	}
	//
	//	Set handler to display additional row between titles and pager body.
	//	Return array of column contents.
	//
	function set_header($func, $headercl='inquirybg')
	{
		$this->header_fun = $func;
		$this->header_class = $headercl;
	}
	//
	//	Set handler to display additional row between pager body and navibar.
	//	Return array of column contents.
	//
	function set_footer($func, $footercl='inquirybg')
	{
		$this->footer_fun = $func;
		$this->footer_class = $footercl;
	}
	//
	//	Setter for table editors with inactive cell control.
	//
	function set_inactive_ctrl($table, $key) {
		$this->inactive_ctrl = array('table'=>$table, 'key'=>$key);
	}
	//
	//	Helper for display inactive control cells
	//
	function inactive_control_cell(&$row)
	{
		if ($this->inactive_ctrl) {
				 
			global	$Ajax;

			$key = $this->key ?
				$this->key : $this->columns[0]['name'];		// TODO - support for complex keys
			$id = $row[$key];
			$table = $this->main_tbl;
			$name = "Inactive". $id;
			$value = $row['inactive'] ? 1:0;

			if (check_value('show_inactive')) {
				if (isset($_POST['LInact'][$id]) && (get_post('_Inactive'.$id.'_update') || 
					get_post('Update')) && (check_value('Inactive'.$id) != $value)) {
					update_record_status($id, !$value, $table, $key);
					$value = !$value;
				}
				echo '<td align="center">'. checkbox(null, $name, $value, true, '')
 				. hidden("LInact[$id]", $value, false) . '</td>';	
			}
		} else
			return '';
	}

};
//-----------------------------------------------------------------------------
//	Creates new db_pager $_SESSION object on first page call.
//  Retrieves from $_SESSION var on subsequent $_POST calls
//
//  $name - base name for pager controls and $_SESSION object name
//  $sql  - base sql for data inquiry. Order of fields implies
//		pager columns order.
//	$coldef - array of column definitions. Example definitions
//		Column with title 'User name' and default text format:
//				'User name'
//		Skipped field from sql query. Data for the field is not displayed:
//				'dummy' => 'skip'
//		Column without title, data retrieved form row data with function func():
//	 			array('fun'=>'func')
// 		Inserted column with title 'Some', formated with function rowfun().
//  	formated as date:
//				'Some' => array('type'=>'date, 'insert'=>true, 'fun'=>'rowfun')
// 		Column with name 'Another', formatted as date, 
// sortable with ascending start order (available orders: asc,desc, '').
//				'Another' => array('type'=>'date', 'ord'=>'asc')
//
//	All available column format types you will find in db_pager_view.inc file.
//		If query result has more fields than count($coldef), rest of data is ignored
//  during display, but can be used in format handlers for 'spec' and 'insert' 
//	type columns.

function &new_db_pager($name, $sql, $coldef, $table = null, $key = null, $page_len = 0)  {

    if (isset($_SESSION[$name]) &&
		 ($_SERVER['REQUEST_METHOD'] == 'GET' || $_SESSION[$name]->sql != $sql)) {
		unset($_SESSION[$name]); // kill pager if sql has changed
	}
	if (!isset($_SESSION[$name])) {
	    $_SESSION[$name] = new db_pager($sql, $name, $table, $page_len);
		$_SESSION[$name]->main_tbl = $table;
		$_SESSION[$name]->key = $key;
		$_SESSION[$name]->set_sql($sql);
		$_SESSION[$name]->set_columns($coldef);
	}
	
	return  $_SESSION[$name];

}
//
//	Force pager initialization.
//
function refresh_pager($name)
{
	if (isset($_SESSION[$name]))
		$_SESSION[$name]->ready = false;
}
