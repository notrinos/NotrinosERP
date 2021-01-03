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
/*
	If no context is set current page/menu screen is selected.
*/
function help_url($context=null)
{

	global $SysPrefs, $help_context;

	$country = $_SESSION['language']->code;
	$clean = 0;
	
	if ($context!=null)
	{
		$help_page_url = $context;
	}
	elseif (isset($help_context))
	{
		$help_page_url = $help_context;
	} else // main menu
	{
		$app = $_SESSION['sel_app'];
		$help_page_url = $_SESSION['App']->applications[$app]->help_context;
		$clean = 1;
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

function send_css($css = '')
{
	global $css_files;

	css_files_ensure_init();

	foreach ($css_files as $css_file)
	{
		echo "<link href='$css_file' rel='stylesheet' type='text/css'> \n";
	}
	if ($css)
	{
		echo "<link href='$css' rel='stylesheet' type='text/css'> \n";
	}
}

function send_scripts()
{
    global $SysPrefs, $path_to_root, $js_static, $js_path, $js_userlib;

	$js ='';
	foreach($js_static as $jsfile)
	{
		$cached_name = basename($jsfile);
		$fpath = user_js_cache().'/'.$cached_name;
	    // compress also static files
	  	if (!file_exists($fpath) || $SysPrefs->go_debug)
			cache_js_file($fpath, file_get_contents($js_path.$jsfile));

	  	$js .= '<script language="javascript" type="text/javascript" src="'.
			$fpath . '"></script>';
	}
	foreach($js_userlib as $jsfile)
	{
	  	$js .= '<script language="javascript" type="text/javascript" src="'.
	    	$jsfile . '"></script>';
	}

    echo $js;
}

function page_header($title, $no_menu=false, $is_index=false, $onload="", $js="", $css='')
{
	// titles and screen header
	global $path_to_root, $SysPrefs, $db_connections;

	if (in_ajax() || isset($_POST['REP_ID']))
		return; // just for speed up

	$theme = user_theme();

	if ($SysPrefs->help_base_url != null && $SysPrefs->use_popup_windows && $js == '')
	{
		if (!function_exists('get_js_open_window'))
		{
			include_once($path_to_root.'/includes/ui/ui_view.inc');
		}
		add_js_source(get_js_open_window(900, 500));
	}
	if ($js!='')
	  	add_js_source($js);

	if (!isset($no_menu))
	{
		$no_menu = false;
	}
	if (isset($_SESSION["App"]) && is_object($_SESSION["App"]) && isset($_SESSION["App"]->selected_application) &&
		$_SESSION["App"]->selected_application != "")
		$sel_app = $_SESSION["App"]->selected_application;
	elseif (isset($_SESSION["sel_app"]) && $_SESSION["sel_app"] != "")
		$sel_app = $_SESSION["sel_app"];
	else
		$sel_app = user_startup_tab();
	$_SESSION["sel_app"] = $sel_app;

	// When startup tab for current user was set to already 
	// removed/inactivated extension module select Sales tab as default.
	if (isset($_SESSION["App"]) && is_object($_SESSION["App"]))
		$_SESSION["App"]->selected_application = 
			isset($_SESSION["App"]->applications[$sel_app]) ? $sel_app : 'orders';

	$page_header_args = func_get_args();
	if(isset($db_connections))	// skip before installation
		hook_invoke_all('pre_header', $page_header_args);

	$encoding = $_SESSION['language']->encoding;

	if (!headers_sent()){
		header("Content-type: text/html; charset=$encoding");
	}
	echo "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\n";
	echo "<html dir='" . $_SESSION['language']->dir . "' >\n";
	echo "<head profile=\"http://www.w3.org/2005/10/profile\"><title>$title</title>";
	if (strpos($_SERVER['HTTP_USER_AGENT'], 'Trident/7.0; rv:11.0') != -1) // IE 11 doesn't work with input file type in form.
		echo "<meta http-equiv='X-UA-Compatible' content='IE=10'>\n";
	echo "<meta http-equiv='Content-type' content='text/html; charset=$encoding'>";
	echo "<link href='$path_to_root/themes/default/images/favicon.ico' rel='icon' type='image/x-icon'> \n";

	send_css($css);

	send_scripts();

	echo "</head> \n";
	if ($onload == "")
		echo "<body>";
	else
		echo "<body onload=\"$onload\">";

	include_once($path_to_root . "/themes/$theme/renderer.php");
	$rend = new renderer();
	$rend->menu_header($title, $no_menu, $is_index);
	error_box();
}

