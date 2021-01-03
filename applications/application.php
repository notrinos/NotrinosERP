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

define('MENU_ENTRY', 'menu_entry');
define('MENU_TRANSACTION', 'menu_transaction');
define('MENU_INQUIRY', 'menu_inquiry');
define('MENU_REPORT', 'menu_report');
define('MENU_MAINTENANCE', 'menu_maintenance');
define('MENU_UPDATE', 'menu_update');
define('MENU_SETTINGS', 'menu_settings');
define('MENU_SYSTEM', 'menu_system');

    class menu_item
	{
		var $label;
		var $link;
		
		function __construct($label, $link) 
		{
			$this->label = $label;
			$this->link = $link;
		}
	}

	class menu 
	{
		var $title;
		var $items;
		
		function __construct($title) 
		{
			$this->title = $title;
			$this->items = array();
		}
		
		function add_item($label, $link) 
		{
			$item = new menu_item($label,$link);
			array_push($this->items,$item);
			return $item;
		}
		
	}

	class app_function 
	{
		var $label;
		var $link;
		var $access;
        var $category;
		
		function __construct($label,$link,$access='SA_OPEN',$category='')
		{
			$this->label = $label;
			$this->link = $link;
			$this->access = $access;
            $this->category = $category;
		}
	}

	class module 
	{
		var $name;
		var $icon;
		var $lappfunctions;
		var $rappfunctions;
		
		function __construct($name,$icon = null) 
		{
			$this->name = $name;
			$this->icon = $icon;
			$this->lappfunctions = array();
			$this->rappfunctions = array();
		}
		
		function add_lapp_function($label,$link="",$access='SA_OPEN',$category='')
		{
			$appfunction = new app_function($label,$link,$access,$category);
			$this->lappfunctions[] = $appfunction;
			return $appfunction;
		}

		function add_rapp_function($label,$link="",$access='SA_OPEN',$category='')
		{
			$appfunction = new app_function($label,$link,$access,$category);
			$this->rappfunctions[] = $appfunction;
			return $appfunction;
		}
		
		
	}

	class application 
	{
		var $id;
		var $name;
		var $help_context;
		var $modules;
		var $enabled;
		
		function __construct($id, $name, $enabled=true) 
		{
			$this->id = $id;
			$this->name = $name;
			$this->enabled = $enabled;
			$this->modules = array();
		}
		
		function add_module($name, $icon = null) 
		{
			$module = new module($name,$icon);
			$this->modules[] = $module;
			return $module;
		}
		
		function add_lapp_function($level, $label,$link="",$access='SA_OPEN',$category='')
		{
			$this->modules[$level]->lappfunctions[] = new app_function($label, $link, $access, $category);
		}
		
		function add_rapp_function($level, $label,$link="",$access='SA_OPEN',$category='')
		{
			$this->modules[$level]->rappfunctions[] = new app_function($label, $link, $access, $category);
		}
		
		function add_extensions()
		{
			hook_invoke_all('install_options', $this);
		}
		//
		// Helper returning link to report class added by extension module.
		//
		function report_class_url($class)
		{
			return "reporting/reports_main.php?Class=".$class;
		}
	}


