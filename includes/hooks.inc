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
// NotrinosERP extension modules integration.
// This file is included in session.inc even before session is started,
// and includes hooks.php connector files from all installed extensions.
// To make hooks active install_hooks() have to be called after interface
// language is set.
//
// To find how various hooks are processed look into respective hook_* functions below.
//
class hooks {
	var $module_name; // extension module name.
	var $path = null;

	// 
	// Helper for updating databases with extension scheme
	//
	// $comp can be company number, -1 for all, 
	// $updates - table of filename => array(table, field, property)
	// $check_only - don't update database, check table/field/property existence only
	//
	function update_databases($comp, $updates, $check_only=false) {
		global $db_connections, $path_to_root;
	
		if ($comp == -1) 
			$conn = $db_connections;
		else
			$conn = array( $comp => $db_connections[$comp]);
		$result = true;

		foreach($conn as $comp => $con) {
			set_global_connection($comp);
			foreach($updates as $file => $update) {
				$table = @$update[0];
				$field = @$update[1];
				$properties = @$update[2];

				$ok = check_table($con['tbpref'], $table, $field, $properties) == 0;

				if (!$check_only && !$ok)
					$ok = db_import($path_to_root.'/modules/'.$this->module_name.'/sql/'.$file, $con);
				
				$result &= $ok;
				if (!$result)
					break;
			}
			db_close();
			if (!$result)
				break;
		}
		set_global_connection(0); // return to siteadmin account

		return $result;
	}
	//
	//	Install additional tabs provided by extension
	//
	function install_tabs($app) {
		// $app->add_application(new example_class); // add menu tab defined by example_class
	}
	//
	//	Install additonal menu options provided by extension
	//
	function install_options($app) {
		// global $path_to_root;

		// switch($app->id) {
		// 	case 'orders':
		// 		$app->add_rapp_function( 0, _("&Example option"), 
		// 			$path_to_root.'/modules/example/example.php?', 'SA_OPEN');
		// }
	}
	
	function install_access() {
		// $security_areas['SA_EXAMPLE'] = array(SS_EXAMPLE|100, _("Example security area."));

		// $security_sections = array(SS_EXAMPLE => _("Example module implementation"));

		// return array($security_areas, $security_sections);
	}

	//
	//	Invoked for all modules before page header is displayed
	//
	function pre_header($fun_args) {
	}
	//
	//	Invoked for all modules before page footer is displayed
	//
	function pre_footer($fun_args) {
	}

	//
	// Price in words. $doc_type is set to document type and can be used to suppress 
	// price in words printing for selected document types.
	// Used instead of built in simple english price_in_words() function.
	//
	//	Returns: amount in words as string.

	function price_in_words($amount, $doc_type) {
	}

	//
	// Exchange rate currency $curr as on date $date.
	// Keep in mind FA has internally implemented 3 exrate providers
	// If any of them supports your currency, you can simply use function below
	// with apprioprate provider set, otherwise implement your own.
	// Returns: $curr value in home currency units as a real number.

	function retrieve_exrate($curr, $date) {
		// $provider = 'ECB'; // 'ECB', 'YAHOO' or 'GOOGLE'
		// return get_extern_rate($curr, $provider, $date);
		return null;
	}

	// External authentication
	// If used should return true after successfull athentication, false otherwise.
	function authenticate($login, $password) {
		return null;
	}
	// Generic function called at the end of Tax Report (report 709)
	// Can be used e.g. for special database updates on every report printing
	// or to print special tax report footer 
	//
	// Returns: nothing
	function tax_report_done() {
	}
	// Following database transaction hooks akcepts array of parameters:
	// 'cart' => transaction data
	// 'trans_type' => transaction type

	function db_prewrite(&$cart, $trans_type) {
		return true;
	}

	function db_postwrite(&$cart, $trans_type) {
		return true;
	}

	function db_prevoid($trans_type, $trans_no) {
		return true;
	}
	/*
		This method is called after module install.
	*/
	function install_extension($check_only=true) {
		return true;
	}
	/*
		This method is called after module uninstall.
	*/
	function uninstall_extension($check_only=true) {
		return true;
	}
	/*
		This method is called on extension activation for company.
	*/
	function activate_extension($company, $check_only=true) {
		return true;
	}
	/*
		This method is called when extension is deactivated for company.
	*/
	function deactivate_extension($company, $check_only=true) {
		return true;
	}

	/*
	 * Returns the quantity allowed to be dispatched for a particular item 
	 * and a status (which can be used to style the row).
	 * This quantity would be the default value on the delivery note.
	 * The usual use case for this is when a item is in stock,
	 * but has been reserved by someone else.
	 * This allows extensions to implements its own priority algorithm.
	 * This function is by detail_id and not item in case the item is present
	 * more than one in  the cart.
	 */
	/* Default behavior check if there is enough quantity on hand and change the css
	 * class if needed */
	static function  default_get_dispatchable_quantity($line_item, $location, $date, $qoh) {
		global $SysPrefs;

		if ($SysPrefs->allow_negative_stock() || ($line_item->qty_dispatched <= $qoh))
			return true;
		return array($qoh, 'stockmankobg');
	}
}

/*
	Installs hooks provided by extension modules
*/
function install_hooks($lang_code=null) {
	global $path_to_root, $Hooks, $installed_extensions;

	$Hooks = array();
	// include current language related $Hooks object if locale file exists
	if (!$lang_code)
		$lang_code = $_SESSION['language']->code;
	$lang_code  = clean_file_name($lang_code);

	if (file_exists($path_to_root . "/lang/" . $lang_code . "/locale.inc")) {
		include_once($path_to_root . "/lang/" . $lang_code . "/locale.inc");
		$code = $lang_code;
		$hook_class = 'hooks_'.$code;
		$Hooks[$code] = new $hook_class;
		$Hooks[$code]->path = "/lang/".$lang_code."/locale.inc";
		unset($code, $hook_class);
	}
	// install hooks provided by active extensions
	if (isset($installed_extensions) && !empty($installed_extensions)) {
		foreach($installed_extensions as $ext) {
			$hook_class = 'hooks_'.$ext['package'];
			if ($ext['active'] && class_exists($hook_class)) {
				$Hooks[$ext['package']] = new $hook_class;
				$Hooks[$ext['package']]->path = $ext['path'];
			}
		}	
	}
}
/*
	Non active hooks are not included in $Hooks array, so we can use special function to 
	activate.
*/
function activate_hooks($ext, $comp, $on=true) {
	global $Hooks;

	$hooks = @$Hooks[$ext];
	if (!$hooks) {
		$hookclass = 'hooks_'.$ext;
		if (class_exists($hookclass))
			$hooks = new $hookclass;
		else
			return true; // extension does not have hooks file
	}
	if (!$hooks)
		return false;
	elseif ($on)
		return $hooks->activate_extension($comp, false);
	else
		return $hooks->deactivate_extension($comp, false);
}
/*
	Calls hook $method defined in extension $ext (if any)
*/
function hook_invoke($ext, $method, &$data, $opts=null) {

	global $Hooks;

	$ret = null;
	if (isset($Hooks[$ext]) && method_exists($Hooks[$ext], $method)) {
		set_ext_domain($Hooks[$ext]->path);
		$ret = $Hooks[$ext]->$method($data, $opts);
		set_ext_domain();
	} 
	return $ret;
}

/*
	Calls hook $methods defined in all extensions (if any)
*/
function hook_invoke_all($method, &$data, $opts=null) {

	global $Hooks;
	
	$return = array();
	if (isset($Hooks)) {
		foreach($Hooks as $ext => $hook)
			if (method_exists($hook, $method)) {
				set_ext_domain($hook->path);
				$result = $hook->$method($data, $opts);
				if (isset($result) && is_array($result))
					$return = array_merge_recursive($return, $result);
				else if (isset($result))
					$return[] = $result;
				
				set_ext_domain();
			}
	}
	return $return;
}
/*
	Returns first non-null result returned from hook.
*/
function hook_invoke_first($method, &$data, $opts=null) {

	global $Hooks;

	$result = null;
	foreach($Hooks as $ext => $hook) {
		if (method_exists($hook, $method)) {
			set_ext_domain($hook->path);
			$result = $hook->$method($data, $opts);
			set_ext_domain();
			if (isset($result))
				break;
		}
	}
	return $result;
}
/*
	Returns last non-null result returned from modules method. Helps implement hooks overriding by 
	extensions installed later.
*/
function hook_invoke_last($method, &$data, $opts=null) {

	global $Hooks;

	$result = null;
	$Reverse = array_reverse($Hooks);
	foreach($Reverse as $ext => $hook) {
		if (method_exists($hook, $method)) {
			set_ext_domain($hook->path);
			$result = $hook->$method($data, $opts);
			set_ext_domain();
			if (isset($result))
				break;
		}
	}
	return $result;
}
//------------------------------------------------------------------------------------------
//	Database transaction hooks.
//	$type - type of transaction (simplifies cart processing)
//	$cart - transaction cart
//	$args is optional array of parameters
//
// For FA 2.3 prewrite, postwrite and prevoid hooks are implemented for following transaction types:
//
// ST_BANKPAYMENT, ST_BANKDEPOSIT, ST_BANKTRANSFER,
// ST_SALESORDER, ST_SALESQUOTE, ST_SALESINVOICE, ST_CUSTCREDIT, ST_CUSTPAYMENT, ST_CUSTDELIVERY,
// ST_LOCTRANSFER, ST_INVADJUST, 
// ST_PURCHORDER, ST_SUPPINVOICE, ST_SUPPCREDIT, ST_SUPPAYMENT, ST_SUPPRECEIVE,
// ST_WORKORDER, ST_MANUISSUE, ST_MANURECEIVE, 

/*
	Invoked after transaction has been read from database to cart.
	Not implemented yet.
*/
//function hook_db_postread(&$cart, $type)
//{
//	hook_invoke_all('db_postread', $cart, $type);
//}

/*
	Invoked before transaction is written to database.
*/
function hook_db_prewrite(&$cart, $type) {
	return hook_invoke_all('db_prewrite', $cart, $type);
}

/*
	Invoked after transaction has been written to database.
*/
function hook_db_postwrite(&$cart, $type) {
	return hook_invoke_all('db_postwrite', $cart, $type);
}
/*
	Invoked before transaction is voided
*/
function hook_db_prevoid($type, $type_no) {
	return hook_invoke_all('db_prevoid', $type, $type_no);
}

//-------------------------------------------------------------------------------------------
//
//	Various hooks
//
//	Alternative exchange rates feeds.
//
function hook_retrieve_exrate($currency, $date) {
	return hook_invoke_last('retrieve_exrate', $currency, $date);
}
//
// Generic function called at the end of Tax Report (report 709)
//
function hook_tax_report_done() {
	return hook_invoke_all('tax_report_done', $dummy);
}
//
//	Amount in words displayed on various documents (especially sales invoice)
//
function hook_price_in_words($amount, $document) {
	return hook_invoke_last('price_in_words', $amount, $document);
}
//
//	Session handling hook. This is special case of hook class which have to be run before session is started.
//	If fa_session_manager class is defined in any installed extension, this class provides session handling
//	for application, otherwise standard php session handling is used.
//
function hook_session_start($company) {
	if (class_exists('fa_session_manager')) {
		global $SessionManager;
		$SessionManager = new fa_session_manager($company);
		return $SessionManager->installed;
	}
	return false;
}
//
//	Third party authentication modules.
//	Returns true after successfull authentication, false otherwise, null if no login hook is defined.
//
function hook_authenticate($login, $password) {
	return hook_invoke_last('authenticate', $login, $password);
}

	/*
	 * Returns the quantity allowed to be dispatched for a particular item 
	 * and a "reason" (css classes).
	 * This quantity would be the default value on the delivery note.
	 * The usual use case for this is when a item is in stock,
	 * but has been reserved by someone else.
	 * This allows extensions to implements its own priority algorithm.
	 * This function is by detail_id and not item in case the item is present
	 * more than one in  the cart.
	 * If 'skip' is returned, the line will be skipped and not displayed
	 */
function hook_get_dispatchable_quantity($line_item, $location, $date, $qoh) {
	$result =  hook_invoke_first('get_dispatchable_quantity', $line_item, array($location, $date, $qoh));
	return $result !== null ? $result : hooks::default_get_dispatchable_quantity($line_item, $location, $date, $qoh);
}