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
define('VARLIB_PATH', $path_to_root.'/tmp');
define('VARLOG_PATH', $path_to_root.'/tmp');

function output_html($text) {
	global $before_box, $Ajax, $messages;
	// Fatal errors are not send to error_handler,
	// so we must check the output
	if ($text && preg_match('/\bFatal error(<.*?>)?:(.*)/i', $text, $m)) {
		$Ajax->aCommands = array();  // Don't update page via ajax on errors
		$text = preg_replace('/\bFatal error(<.*?>)?:(.*)/i','', $text);
		$messages[] = array(E_ERROR, $m[2], null, null);
	}
	$Ajax->run();
	return  in_ajax() ? fmt_errors() : ($before_box.fmt_errors().$text);
}

//-----------------------------------------------------------------------------
//	Removing magic quotes from nested arrays/variables
//
function strip_quotes($data) {
	if(get_magic_quotes_gpc()) {
		if(is_array($data)) {
			foreach($data as $k => $v) {
				$data[$k] = strip_quotes($data[$k]);
			}
		}
		else
			return stripslashes($data);
	}
	return $data;
}

function html_specials_encode($str) {
	return htmlspecialchars($str, ENT_QUOTES, $_SESSION['language']->encoding=='iso-8859-2' ? 'ISO-8859-1' : $_SESSION['language']->encoding);
}

function html_cleanup(&$parms) {
	foreach($parms as $name => $value) {
		if (is_array($value))
			html_cleanup($parms[$name]);
		else
			$parms[$name] = html_specials_encode($value);
	}
	reset($parms); // needed for direct key() usage later throughout the sources
}

function check_page_security($page_security) {
}

/*
 Ensure file is re-read on next request if php caching is active
*/
function cache_invalidate($filename) {
	if (function_exists('opcache_invalidate')) // OpCode extension
		opcache_invalidate($filename);
}

//============================================================================

if (!isset($path_to_root))
	$path_to_root = '.';

// Prevent register_globals vulnerability
if (isset($_GET['path_to_root']) || isset($_POST['path_to_root']))
	die('Restricted access');

include_once($path_to_root . '/includes/errors.inc');
// collect all error msgs
set_error_handler('error_handler' /*, errtypes */);

include_once($path_to_root . '/includes/current_user.inc');
include_once($path_to_root . '/includes/lang/language.inc');
include_once($path_to_root . '/includes/ajax.inc');
include_once($path_to_root . '/includes/ui/ui_msgs.inc');
include_once($path_to_root . '/includes/prefs/sysprefs.inc');

session_name('FAinstall');
session_start();

// this is to fix the "back-do-you-want-to-refresh" issue - thanx PHPFreaks
header('Cache-control: private');

include_once($path_to_root . '/config.default.php');

$_SESSION['SysPrefs'] = new sys_prefs();
$SysPrefs = &$_SESSION['SysPrefs'];

$inst_langs = array(
	'C' => array (	   'name' => 'English',             	'code' => 'C',		'encoding' => 'utf-8'),
	'ar_EG' => array ( 'name' => 'Arabic',          	    'code' => 'ar_EG',	'encoding' => 'utf-8', 'rtl' => true),
	'da_DK' => array ( 'name' => 'Dansk',               	'code' => 'da_DK',	'encoding' => 'iso-8859-1'),
	'de_DE' => array ( 'name' => 'Deutsch',             	'code' => 'de_DE',	'encoding' => 'iso-8859-1'),
	'el_GR' => array ( 'name' => 'Greek',           	    'code' => 'el_GR',	'encoding' => 'utf-8'),
	'es_MX' => array ( 'name' => 'Spanish',             	'code' => 'es_MX',	'encoding' => 'iso-8859-1'),
	'fr_FR' => array ( 'name' => 'French',            	    'code' => 'fr_FR',	'encoding' => 'iso-8859-1'),
	'hi_IN' => array ( 'name' => 'Hindi',           	    'code' => 'hi_IN',	'encoding' => 'utf-8'),
	'id_ID' => array ( 'name' => 'Indonesian',              'code' => 'id_ID',	'encoding' => 'iso-8859-1'),
	'it_IT' => array ( 'name' => 'Italian',                 'code' => 'it_IT',	'encoding' => 'iso-8859-1'),
	'ka_GE' => array ( 'name' => 'Georgian',                'code' => 'ka_GE',	'encoding' => 'utf-8'),
	'nb_NO' => array ( 'name' => 'Norwegian',               'code' => 'nb_NO',	'encoding' => 'iso-8859-1'),
	'nl_BE' => array ( 'name' => 'Nederlands (Belgian)',    'code' => 'nl_BE',	'encoding' => 'iso-8859-1'),
	'nl_NL' => array ( 'name' => 'Nederlands (Netherlands)','code' => 'nl_NL',	'encoding' => 'iso-8859-1'),
	'pl_PL' => array ( 'name' => 'Polski',              	'code' => 'pl_PL',	'encoding' => 'iso-8859-2'),
	'pt_BR' => array ( 'name' => 'Portuguese (Brazil)',     'code' => 'pt_BR',	'encoding' => 'iso-8859-1'),
	'pt_PT' => array ( 'name' => 'Portuguese',              'code' => 'pt_PT',	'encoding' => 'iso-8859-1'),
	'sv_SE' => array ( 'name' => 'Svenska',             	'code' => 'sv_SE',	'encoding' => 'iso-8859-1'),
	'ta_IN' => array ( 'name' => 'Tamil',               	'code' => 'ta_IN',	'encoding' => 'utf-8'),
	'vi_VN' => array ( 'name' => 'Tiếng Việt',           	'code' => 'vi_VN',	'encoding' => 'utf-8'),
	'zh_CN' => array ( 'name' => 'Chinese Simplifed',   	'code' => 'zh_CN',	'encoding' => 'utf-8'),
);

$Ajax = new Ajax();

get_text_init();

$i_lang = isset($_POST['inst_lang']) ? $_POST['inst_lang'] : 
	(isset($_SESSION['inst_set']['inst_lang']) ? $_SESSION['inst_set']['inst_lang'] : 'C');

// Page Initialisation
if (!isset($_SESSION['language']) || !method_exists($_SESSION['language'], 'set_language') || $_SESSION['language']->code != $i_lang) {
	$l = array_search_value($i_lang, $inst_langs,  'code');
	$_SESSION['language'] = new language($l['name'], $l['code'], $l['encoding'],
	 @isset($l['rtl']) ? 'rtl' : 'ltr');

}
if (!isset($installed_languages))
	$installed_languages = array();

$_SESSION['language']->set_language($_SESSION['language']->code);
$GetText->add_domain( $_SESSION['language']->code, $path_to_root.'/install/lang');

include_once($path_to_root.'/version.php');
include_once($path_to_root.'/includes/main.inc');

// js/php validation rules container
$Validate = array();
// bindings for editors
$Editors = array();
// page help. Currently help for function keys.
$Pagehelp = array();

//$Ajax = new Ajax();

// intercept all output to destroy it in case of ajax call
register_shutdown_function('end_flush');
ob_start('output_html',0);

if (!isset($_SESSION['wa_current_user']))
	$_SESSION['wa_current_user'] = new current_user();

html_cleanup($_GET);
html_cleanup($_POST);
html_cleanup($_REQUEST);
html_cleanup($_SERVER);

// POST vars cleanup needed for direct reuse.
// We quote all values later with db_escape() before db update.
$_POST = strip_quotes($_POST);