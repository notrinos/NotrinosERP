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
/*
	If no context is set current page/menu screen is selected.
*/
function help_url($context=null) {

	global $SysPrefs, $help_context;

	$country = $_SESSION['language']->code;
	$clean = 0;
	
	if ($context!=null)
		$help_page_url = $context;
	elseif (isset($help_context))
		$help_page_url = $help_context;
	else {// main menu
		if (isset($_SESSION['sel_app'])
			&& isset($_SESSION['App'])
			&& is_object($_SESSION['App'])
			&& isset($_SESSION['App']->applications[$_SESSION['sel_app']])
			&& isset($_SESSION['App']->applications[$_SESSION['sel_app']]->help_context)) {
			$app = $_SESSION['sel_app'];
			$help_page_url = $_SESSION['App']->applications[$app]->help_context;
			$clean = 1;
		} else {
			$help_page_url = 'Help';
		}
	}

	if (@$SysPrefs->old_style_help)
		$help_page_url = _($help_page_url);

	if ($clean)
		$help_page_url = access_string($help_page_url, true);

	return html_specials_encode(
		$SysPrefs->help_base_url
		.urlencode(strtr(ucwords($help_page_url), array(' '=>'', '/'=>'', '&'=>'And')))
		.'&ctxhelp=1&lang='.$country);
}

function send_css($css='') {
	global $css_files;

	init_css();

	foreach ($css_files as $css_file) {
		echo "<link href='".$css_file."' rel='stylesheet' type='text/css'> \n";
	}
	if ($css)
		echo "<link href='".$css."' rel='stylesheet' type='text/css'> \n";
}

function send_scripts() {
	global $SysPrefs, $path_to_root, $js_static, $js_path, $js_userlib;

	$js ='';
	foreach($js_static as $jsfile) {
		$basename = basename($jsfile);
		$fpath = user_js_cache().'/'.$basename;
		$source_path = $js_path.$jsfile;
		$script_version = null;
		// compress also static files
		if (!file_exists($fpath) || $SysPrefs->go_debug || (file_exists($source_path) && filemtime($fpath) < filemtime($source_path))) {
			// don't need to compress minified files
			if(strpos($basename, '.min.js' ) !== false)
				copy($source_path, $fpath);
			else
				cache_js_file($fpath, file_get_contents($source_path));
		}

		if (file_exists($fpath))
			$script_version = filemtime($fpath);
		elseif (file_exists($source_path))
			$script_version = filemtime($source_path);

		$js .= '<script src="'.$fpath.($script_version ? '?v='.$script_version : '').'"></script>';
	}
	foreach($js_userlib as $jsfile) {
		$js .= '<script src="'.$jsfile.'"></script>';
	}

	echo $js;
}

function page_header($title, $no_menu=false, $is_index=false, $onload='', $js='', $css='') {
	// titles and screen header
	global $path_to_root, $SysPrefs, $db_connections;

	if (in_ajax() || isset($_POST['REP_ID']))
		return; // just for speed up

	$theme = user_theme();

	$renderer_path = dirname(__FILE__).'/../../themes/'.$theme.'/renderer.js';

	if(file_exists($renderer_path)) {
		$renderer_version = filemtime($renderer_path);
		add_js_ufile($path_to_root.'/themes/default/renderer.js?v='.$renderer_version);
	}

	if ($SysPrefs->help_base_url != null && $SysPrefs->use_popup_windows && $js == '') {
		if (!function_exists('get_js_open_window'))
			include_once($path_to_root.'/includes/ui/ui_view.inc');
		
		add_js_source(get_js_open_window(900, 500));
	}
	if ($js != '')
		add_js_source($js);

	if (!isset($no_menu))
		$no_menu = false;
	
	if (isset($_GET['sel_app']) && $_GET['sel_app'] != '')
		$sel_app = $_GET['sel_app'];
	elseif (isset($_GET['application']) && $_GET['application'] != '')
		$sel_app = $_GET['application'];
	elseif (isset($_SESSION['sel_app']) && $_SESSION['sel_app'] != '')
		$sel_app = $_SESSION['sel_app'];
	elseif (isset($_SESSION['App']) && is_object($_SESSION['App']) && isset($_SESSION['App']->selected_application) && $_SESSION['App']->selected_application != '')
		$sel_app = $_SESSION['App']->selected_application;
	else
		$sel_app = user_startup_tab();
	
	$_SESSION['sel_app'] = $sel_app;

	// When startup tab for current user was set to already 
	// removed/inactivated extension module select Sales tab as default.
	if (isset($_SESSION['App']) && is_object($_SESSION['App'])) {
		$sel_app = isset($_SESSION['App']->applications[$sel_app]) ? $sel_app : 'orders';
		$_SESSION['sel_app'] = $sel_app;
		$_SESSION['App']->selected_application = $sel_app;
	}

	$page_header_args = func_get_args();
	if(isset($db_connections))	// skip before installation
		hook_invoke_all('pre_header', $page_header_args);

	$encoding = $_SESSION['language']->encoding;

	if (!headers_sent()) {
		header("Content-type: text/html; charset=$encoding");
		header("X-Frame-Options: SAMEORIGIN");
	}
	
	echo "<!DOCTYPE html>\n";
	echo "<html dir='" . $_SESSION['language']->dir . "' >\n";
	echo "<head profile=\"http://www.w3.org/2005/10/profile\"><title>".$title."</title>";
	if (strpos($_SERVER['HTTP_USER_AGENT'], 'Trident/7.0; rv:11.0') != -1) // IE 11 doesn't work with input file type in form.
		echo "<meta http-equiv='X-UA-Compatible' content='IE=10'>\n";
	echo "<meta charset='".$encoding."'>";
	echo "<meta name='viewport' content='width=device-width,initial-scale=1'>";
	echo "<link href='".$path_to_root."/themes/default/images/favicon.ico' rel='icon' type='image/x-icon'> \n";

	send_css($css);

	send_scripts();

	echo "</head> \n";
	if ($onload == '')
		echo '<body>';
	else
		echo "<body onload=\"$onload\">";

	include_once($path_to_root.'/themes/'.$theme.'/renderer.php');
	$rend = new renderer();
	$rend->menu_header($title, $no_menu, $is_index);

	// Global CSRF token for AJAX requests (print/email links, etc.) on pages without forms.
	echo "<input type='hidden' id='global_csrf_token' name='_token' value='".ensure_csrf_token()."'>";
	
	error_box();
}
