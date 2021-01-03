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
// Prevent register_globals vulnerability
if (isset($_GET['path_to_root']) || isset($_POST['path_to_root']))
	die("Restricted access");
@include_once($path_to_root . "/lang/installed_languages.inc");
include_once($path_to_root . "/includes/lang/gettext.inc");

class language 
{
	var $name;
	var $code;			// eg. ar_EG, en_GB
	var $encoding;		// eg. UTF-8, CP1256, ISO8859-1
	var	$dir;			// Currently support for Left-to-Right (ltr) and
						// Right-To-Left (rtl)
	var $version; // lang package version
	var $is_locale_file;
	
	function __construct($name, $code, $encoding, $dir = 'ltr') 
	{
		global $dflt_lang;
		
		$this->name = $name;
		$this->code = $code ? $code : ($dflt_lang ? $dflt_lang : 'C');
		$this->encoding = $encoding;
		$this->dir = $dir;
	}

	function get_language_dir() 
	{
		return "lang/" . $this->code;
	}

	function get_current_language_dir() 
	{
		$lang = $_SESSION['language'];
		return "lang/" . $lang->code;
	}

	function set_language($code) 
	{
	    global $path_to_root, $installed_languages, $GetText;

		$lang = array_search_value($code, $installed_languages, 'code');
		$changed = $this->code != $code || $this->version != @$lang['version'];

		if ($lang && $changed)
		{
			$this->name = $lang['name'];
			$this->code = $lang['code'];
			$this->encoding = $lang['encoding'];
			$this->version = @$lang['version'];
			$this->dir = (isset($lang['rtl']) && $lang['rtl'] === true) ? 'rtl' : 'ltr';
			$locale = $path_to_root . "/lang/" . $this->code . "/locale.inc";
			$this->is_locale_file = file_exists($locale);
		}

		$GetText->set_language($this->code, $this->encoding);
		$GetText->add_domain($this->code, $path_to_root . "/lang", $this->version);

		// Necessary for ajax calls. Due to bug in php 4.3.10 for this 
		// version set globally in php.ini
		ini_set('default_charset', $this->encoding);

		if (isset($_SESSION['wa_current_user']) && $_SESSION['wa_current_user']->logged_in() && isset($_SESSION['App']) && $changed)
			$_SESSION['App']->init(); // refresh menu
	}
}

if (!function_exists("_")) 
{
	function _($text) 
	{
		global $GetText;
		if (!isset($GetText)) // Don't allow using gettext if not is net.
			return $text;

		$retVal = $GetText->gettext($text);
		if ($retVal == "")
			return $text;
		return $retVal;
	}
}
