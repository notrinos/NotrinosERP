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
include_once($path_to_root . "/includes/db/connect_db.inc");

include_once($path_to_root . "/admin/db/transactions_db.inc");
include_once($path_to_root . "/includes/types.inc");
include_once($path_to_root . "/includes/references.inc");
include_once($path_to_root . "/includes/db/comments_db.inc");
include_once($path_to_root . "/includes/db/sql_functions.inc");
include_once($path_to_root . "/includes/db/audit_trail_db.inc");

include_once($path_to_root . "/admin/db/users_db.inc");
include_once($path_to_root . "/includes/ui/ui_view.inc");
include_once($path_to_root . "/includes/ui/ui_controls.inc");

$page_nested = -1;
// static js files path
$js_path = $path_to_root.'/js/';
// standard external js scripts included in all files
$js_static = array('JsHttpRequest.js', 'behaviour.js', 'utils.js', 'inserts.js');
// additional js source included in header
$js_lib = $js_userlib = array();

function page($title, $no_menu=false, $is_index=false, $onload="", $js="", $script_only=false, $css='')
{

   	global $path_to_root, $page_security, $page_nested;

	if (++$page_nested) return;

   	$hide_menu = $no_menu;

   	include_once($path_to_root . "/includes/page/header.inc");

   	page_header($title, $no_menu, $is_index, $onload, $js, $css);
	check_page_security($page_security);
//	error_box();
	if($script_only) {
		echo '<noscript>';
		echo display_heading(_('This page is usable only with javascript enabled browsers.'));
		echo '</noscript>';
		div_start('_page_body', null, true);
	} else {
		div_start('_page_body'); // whole page content for ajax reloading
	}
}

function end_page($no_menu=false, $is_index=false, $final_screen=false, $type_no=0, $trans_no=0)
{
	global $path_to_root, $page_nested;

	if ($page_nested-- > 0) return;

	if (!$is_index && function_exists('hyperlink_back'))
		hyperlink_back(true, $no_menu, $type_no, $trans_no, $final_screen);
	div_end();	// end of _page_body section

	include_once($path_to_root . "/includes/page/footer.inc");
	page_footer($no_menu, $is_index);
}

function css_files_ensure_init() {
	global $css_files, $path_to_root;

	if (!isset($css_files))
	{
		$theme = user_theme();
		$css_files = array();
		$css_files[] = $path_to_root . "/themes/$theme/default.css";
	}
}

function add_css_file($filename)
{
	global $css_files;
	css_files_ensure_init();
	$css_files[] = $filename;
}

function cache_js_file($fpath, $text) 
{
	global $SysPrefs;

	if (!$SysPrefs->go_debug) $text = js_compress($text);

    $file = force_open($fpath);
	if (!$file) return false;
	if (!fwrite($file, $text)) return false;
	return fclose($file);

}

/*
	Open file for writing with creration of subfolders if needed.
*/
function force_open($fname)
{
	$file = pathinfo($fname);

	$path = $fname[0] == '/' ? '/' : '';
	$tree = explode('/', $file['dirname']);
	foreach($tree as $level) {
		$path .= $level;
		if (!file_exists($path)) {
			if (!mkdir($path)) {
				return null;
			}
		}
		$path .= '/';
	}
	return fopen($fname, 'w');
}

function add_js_file($filename) 
{
	  global $js_static;

	  $search = array_search($filename, $js_static);
	  if ($search === false || $search === null) // php>4.2.0 returns null
		$js_static[] = $filename;	
}

function add_js_ufile($filename) 
{
	  global $js_userlib;

	  $search = array_search($filename, $js_userlib);
	  if ($search === false || $search === null) // php>4.2.0 returns null
		$js_userlib[] = $filename;
}

function add_js_source($text) 
{
	  global $js_lib;

	  $search = array_search($text, $js_lib);
	  if ($search === false || $search === null) // php>4.2.0 returns null
		$js_lib[] = $text;
}

/**
 * Compresses the Javascript code for more efficient delivery.
 * copyright (c) 2005 by Jared White & J. Max Wilson
 * http://www.xajaxproject.org
 * Added removing comments from output.
 * Warning: Fails on RegExp with quotes - use new RegExp() in this case.
 */
function js_compress($sJS)
{
	//remove windows cariage returns
	$sJS = str_replace("\r","",$sJS);
	
	//array to store replaced literal strings
	$literal_strings = array();
	
	//explode the string into lines
	$lines = explode("\n",$sJS);
	//loop through all the lines, building a new string at the same time as removing literal strings
	$clean = "";
	$inComment = false;
	$literal = "";
	$inQuote = false;
	$escaped = false;
	$quoteChar = "";
	
	for($i=0;$i<count($lines);$i++)
	{
		$line = $lines[$i];
		$inNormalComment = false;
	
		//loop through line's characters and take out any literal strings, replace them with ___i___ where i is the index of this string
		$len = strlen($line);
		if (version_compare(PHP_VERSION, '7.0.0') >= 0) // uninitialized string offser error fix. @Braath Waate
			$line .= chr(32);
		
		for($j=0;$j<$len;$j++)
		{
			$c = $line[$j];		// this is _really_ faster than subst
			$d = $c.$line[$j+1];
	
			//look for start of quote
			if(!$inQuote && !$inComment)
			{
				//is this character a quote or a comment
				if(($c=="\"" || $c=="'") && !$inComment && !$inNormalComment)
				{
					$inQuote = true;
					$inComment = false;
					$escaped = false;
					$quoteChar = $c;
					$literal = $c;
				}
				else if($d=="/*" && !$inNormalComment)
				{
					$inQuote = false;
					$inComment = true;
					$escaped = false;
					$quoteChar = $d;
					$literal = $d;	
					$j++;	
				}
				else if($d=="//") //ignore string markers that are found inside comments
				{
					$inNormalComment = true;
					$clean .= $c;
				}
				else
				{
					$clean .= $c;
				}
			}
			else //allready in a string so find end quote
			{
				if($c == $quoteChar && !$escaped && !$inComment)
				{
					$inQuote = false;
					$literal .= $c;
	
					//subsitute in a marker for the string
					$clean .= "___" . count($literal_strings) . "___";
	
					//push the string onto our array
					array_push($literal_strings,$literal);
	
				}
				else if($inComment && $d=="*/")
				{
					$inComment = false;
					$literal .= $d;
	
					//subsitute in a marker for the string
					$clean .= "___" . count($literal_strings) . "___";
	
					//push the string onto our array
					array_push($literal_strings,$literal);
	
					$j++;
				}
				else if($c == "\\" && !$escaped)
					$escaped = true;
				else
					$escaped = false;
	
				$literal .= $c;
			}
		}
		if($inComment) $literal .= "\n";
		$clean .= "\n";
	}
	//explode the clean string into lines again
	$lines = explode("\n",$clean);
	
	//now process each line at a time
	for($i=0;$i<count($lines);$i++)
	{
		$line = $lines[$i];
	
		//remove comments
		$line = preg_replace("/\/\/(.*)/","",$line);
	
		//strip leading and trailing whitespace
		$line = trim($line);
	
		//remove all whitespace with a single space
		$line = preg_replace("/\s+/"," ",$line);
	
		//remove any whitespace that occurs after/before an operator
		$line = preg_replace("/\s*([!\}\{;,&=\|\-\+\*\/\)\(:])\s*/","\\1",$line);
	
		$lines[$i] = $line;
	}
	
	//implode the lines
	$sJS = implode("\n",$lines);
	
	//make sure there is a max of 1 \n after each line
	$sJS = preg_replace("/[\n]+/","\n",$sJS);
	
	//strip out line breaks that immediately follow a semi-colon
	$sJS = preg_replace("/;\n/",";",$sJS);
	
	//curly brackets aren't on their own
	$sJS = preg_replace("/[\n]*\{[\n]*/","{",$sJS);
	
	//finally loop through and replace all the literal strings:
	for($i=0;$i<count($literal_strings);$i++) {
	    if (strpos($literal_strings[$i],"/*")!==false) 
		$literal_strings[$i]= '';
		$sJS = str_replace("___".$i."___",$literal_strings[$i],$sJS);
	}
	return $sJS;
}

/*
	Check if file can be updated, restoring subdirectories 
	if needed. Returns 1 when no confilcts, -1 when file exists and is writable
*/
function check_write($path)
{
	if ($path == ''//|| $path == '.' || $path == '..'
	) return 0;

	return is_writable($path) ? (is_dir($path) ? 1 : -1) 
		: (is_file($path) ? 0 : ($path == '.' || $path == '..' ? 0 : check_write(dirname($path))));
}

/*
	Copies set of files. When $strict is set
	also removes files from the $to which 
	does not exists in $from directory but arelisted in $flist.
*/
function copy_files($flist, $from, $to, $strict=false)
{
	foreach ($flist as $file) {
		if (file_exists($from.'/'.$file)) {
			if (!copy_file($file, $from, $to))
				return false;
		} else if ($strict) {
				unlink($to.'/'.$file);
		}
	}
	return true;
}

/*
	Copies file from base to target directory, restoring subdirectories 
	if needed.
*/
function copy_file($file, $from, $to)
{

	if (!is_dir(dirname($file=='.' ? $to : ($to.'/'.$file)))) {
		if (!copy_file(dirname($file), null, $to))
			return false;
	}
	if (!$from) {
	//		error_log( 'dodanie katalogu '.$to.'/'.$file);
		return @mkdir($file=='.' ? $to : ($to.'/'.$file));
	}
	else {
	//		error_log( 'skopiowanie '.$to.'/'.$file);
		return @copy($from.'/'.$file, $to.'/'.$file);
	}
}
/*
	Search for file, looking first for company specific version, then for 
	version provided by any extension module, finally in main FA directory.
	Also adds include path for any related files, and sets $local_path_to_root 
	to enable local translation domains.
	
	Returns found file path or null.
*/
function find_custom_file($rep)
{
	global $installed_extensions, $path_to_root, $local_path_to_root;

	// customized per company version
	$path = company_path();
	$file = $path.$rep;
	if (file_exists($file)) {
		// add local include path
		$local_path_to_root = $path;
		set_include_path(dirname($file).PATH_SEPARATOR.get_include_path());
		return $file;
	}
	// file added by active extension modules
	if (count($installed_extensions) > 0)
	{
		$extensions = $installed_extensions;
		foreach ($extensions as $ext)
			if (($ext['active'] && $ext['type'] == 'extension')) {
				$path = $path_to_root.'/'.$ext['path'];
				$file = $path.$rep;
				if (file_exists($file)) {
					set_include_path($path.PATH_SEPARATOR.get_include_path());
					$local_path_to_root = $path;
					return $file;
				}
			}
	}
	// standard location
	$file = $path_to_root.$rep;
	if (file_exists($file))
		return $file;

	return null;
}
/*
	
	Protect against directory traversal.
	Changes all not POSIX compatible chars to underscore.
*/
function clean_file_name($filename) {
    $filename = str_replace(chr(0), '', $filename);
    return preg_replace('/[^a-zA-Z0-9.\-_]/', '_', $filename);
}

/*
	Simple random password generator.
*/
function generate_password()
{
	if (PHP_VERSION >= '5.3')
		$bytes = openssl_random_pseudo_bytes(8, $cstrong);
	else
		$bytes = sprintf("08%x", mt_rand(0,0xffffffff));

	return  base64_encode($bytes);
}

if (!function_exists('array_fill_keys')) // since 5.2
{
	function array_fill_keys($keys, $value)
	{
		return (object)array_combine($keys, array_fill(count($keys), $value));
	}
}

/*
	This function aims to generate cryptographically strong random identifier.
	Result identifier has length 4[strength/8/3] 
*/
function random_id($strength = 128)
{
	$n = ceil($strength/8);

	if (function_exists('openssl_random_pseudo_bytes'))
		$bin = openssl_random_pseudo_bytes($n, $cstrong); 	// openssl on php 5.3 and up
	else if (file_exists('/dev/urandom'))
		$bin = file_get_contents('/dev/urandom', false, null, -1, $n);  // linux std random device
	else {
		$bin = '';
		for($i=0; $i < $n; $i++)
			$bin .= chr(mt_rand(0, 255));	// Mersene Twister generator
	}
	$id = strtr(base64_encode($bin), '+/=', '-_x');	// see RFC 4648 Section 5

	return $id;
}
