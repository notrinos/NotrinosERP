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

// Type of requirement for positive test result
$test_level = array(
	0 => _('Info'),
	1 => _('Optional'),
	2 => _('Recommended'),
	3 => _('Required ')
);

// test mysql server version
function tst_mysql() 
{
	$test['descr'] = _('MySQL version'). ' >=4.1';
	$test['type'] = 3;
	$test['test'] = db_get_version();
	if (!($test['result'] = ($test['test']>='4.1'))) {
		$db_str = explode('-', $test['test']);
		$test['result'] = ($db_str[1] == 'MariaDB');
	}
	$test['comments'] = _('Upgrade MySQL server to version at least 4.1');

	return $test;
}
// test php mysql extension
function tst_phpmysql() 
{
	$test['descr'] = _('PHP MySQL extension');
	$test['type'] = 3;
	$test['result'] = db_extension_exists();
	$test['test'] = $test['result'] ? _('Yes'): _('No');
	
	$test['comments'] = _('Your PHP has to have MySQL extension enabled.');
	return $test;
}

function tst_php() 
{
	$test['descr'] = _('PHP version').' >=5.0.0';
	$test['type'] = 3;
	$test['test'] = phpversion();
	$test['result'] = $test['test']>='5.0.0';
	$test['comments'] = _('Upgrade PHP to version at least 5.0.0');

	return $test;
}

function tst_system() 
{
	$test['descr'] = _('Server system');
	$test['type'] = 0;
	$test['test'] = PHP_OS;
	$test['result'] = true;

	return $test;
}

function tst_sessionpath() 
{
	$test['descr'] = _('Session save path');
	$test['type'] = 0;
	$test['test'] = session_save_path();
	$test['result'] = true;

	return $test;
}

function tst_install() 
{
	global $path_to_root;

	$test['descr'] = _('Removed install wizard folder');
	$test['type'] = 2;
	$test['result'] = !is_dir($path_to_root.'/install');
	$test['test'] = _('Not removed');
	$test['comments'] = _('Remove or rename install wizard folder for security reasons.');

	return $test;
}

function tst_browser() 
{
	$test['descr'] = _('Browser type');
	$test['type'] = 0;
	$test['test'] = $_SERVER['HTTP_USER_AGENT'];
	$test['result'] = true;
	$test['comments'] = _('Any browser is supported');

	return $test;
}

function tst_server() 
{
	$test['descr'] = _('Http server type');
	$test['test'] = $_SERVER['SERVER_SOFTWARE'];
	$test['type'] = 0;
	$test['result'] = true;
	$test['comments'] = _('Any server is supported');

	return $test;
}

function tst_gettext() 
{
	$test['descr'] = _('Native gettext');
	$test['test'] = function_exists('gettext') ? _('Yes'): _('No');
	$test['type'] = 1;
	$test['result'] = true;
	$test['comments'] = _('In case of no gettext support, php emulation is used');

	return $test;
}

function tst_debug() 
{
	global $SysPrefs;
	$test['descr'] = _('Debugging mode');
	$test['type'] = 0;
	$test['test'] = $SysPrefs->go_debug ? _("Yes") : _("No");
	$test['result'] = $SysPrefs->go_debug != 0;
	$test['comments'] = _('To switch debugging on set $go_debug>0 in config.php file');

	return $test;
}

function tst_logging() 
{
	global $SysPrefs;
	
	$error_logfile = $SysPrefs->error_logfile;
	$test['descr'] = _('Error logging');
	$test['type'] = 2;
	// if error lgging is on, but log file does not exists try write
	if ($error_logfile && !is_file($error_logfile)) 
	{
		@fclose(@fopen($error_logfile, 'w'));
	}
	$test['result'] = @$error_logfile != '' && is_writable($error_logfile);
	$test['test'] = @$error_logfile == '' ? _("Disabled") : $error_logfile;
	
	if (@$error_logfile == '')
		$test['comments'] = _('To switch error logging set $error_logging in config.php file');
	else
	if (!is_writable($error_logfile))
		$test['comments'] = _('Log file is not writeable');
	
	return $test;
}
//
//	Installed FA database structure version
//
function tst_dbversion()
{
	global $db_version;
	$test['descr'] = _('Current database version');
	$test['type'] = 3;
	$test['test'] = get_company_pref('version_id');
	$test['result'] = $test['test'] == $db_version;
	$test['comments'] = _('Database structure seems to be not upgraded to current version')
		." ($db_version)";

	return $test;
}


function tst_subdirs($install=false)
{
	global $db_connections;

	$comps = $install ? array('0') : array_keys($db_connections);

	$comp_subdirs = array('images', 'pdf_files', 'backup','js_cache');

	$test['descr'] = _('Company subdirectories consistency');
	$test['type'] = 3;
	$test['test'] = array(company_path().'/*');
	foreach($comp_subdirs as $sub) {
		$test['test'][] = company_path().'/*/'.$sub;
	}
	$test['result'] = true;
	
	$comp_path = company_path();
	foreach ($comps as $n) {
		$path = company_path($n);
		if (!is_dir($path) || !is_writable($path) ) {
			$test['result'] = false;
			$test['comments'][] = sprintf(_("'%s' is not writeable"), $path);
			continue;
		};
		foreach($comp_subdirs as $sub) {
			$spath = $path.'/'.$sub;
			if (!is_dir($spath) || !is_writable($spath) ) {
				$test['result'] = false;
				$test['comments'][] = sprintf(_("'%s' is not writeable"), $spath);
			} else {
				$dir = opendir($spath);
				while (false !== ($fname = readdir($dir))) {
					// check only *.js files. Manually installed package can contain other
					// non-writable files which are non-crucial for normal operations
					if (preg_match('/.*(\.js)/', $fname) && !is_writable("$spath/$fname")) {
						$test['result'] = false;
						$test['comments'][] = sprintf(_("'%s' is not writeable"), "$spath/$fname");
					}
				}
			}
		}
	}
	return $test;
}

function tst_tmpdir()
{
	global $path_to_root;
	
	$test['descr'] = _('Temporary directory');
	$test['type'] = 3;
	$test['test'] = VARLIB_PATH;
	$test['result'] = is_dir($test['test']) && is_writable($test['test']);
	$test['comments'][] = sprintf(_("'%s' is not writeable"), $test['test']);
	return $test;
}

function tst_langs($install)
{
	global $installed_languages, $path_to_root, $GetText;

	$test['descr'] = _('Language configuration consistency');
	$test['type'] = 3;
	$test['result'] = true;
	$test['comments'] = array();

	$fname =  $path_to_root.'/lang';
	$test['test'] = $fname;
	if (!(is_dir($fname) && is_writable($fname))) {
		$test['result'] = false;
		$test['comments'][] = _("Languages folder should be writeable.");
		return $test;
	}
	
	if (!$install) {
		$fname =  $path_to_root.'/lang/installed_languages.inc';
		$test['test'] = $fname;
		if (!(is_file($fname) && is_writable($fname))) {
			$test['result'] = false;
			$test['comments'][] = _("Languages configuration file should be writeable.");
			return $test;
		}
	}

	$langs = array();
	
	foreach ($installed_languages as $lang) {

		if ($lang['code'] == 'C') continue; // no translation (English)
		$langs[] = $lang['code'];

		$file = $path_to_root.'/lang/'.$lang['code'].'/LC_MESSAGES/'.$lang['code'];
		if (@$lang['version'])
			$file .= '-'.$lang['version'];
		$file .= function_exists('gettext') ? '.mo' : '.po';

		if (!is_file($file)) {
			$test['result'] = false;
			$test['comments'][] = sprintf( _('Missing %s translation file.'), $file);
		}
		if (!$GetText->check_support($lang['code'], $lang['encoding']))
		{
			$test['result'] = false;
			$test['comments'][] = sprintf(_('Missing system locale: %s'), $lang['code'].".".$lang['encoding']);
		};
	}

	$test['test'] = $langs;

	return $test;
}

function tst_config($install)
{
	global $path_to_root;

	$test['descr'] = _('Main config file');
	$test['test'] = $path_to_root.'/config.php';
	if ($install) {
		$test['type'] = 3;
		$writable = check_write($test['test']);
		$test['result'] = $writable==1;
		$test['comments'][] = $writable == 0 ?
			sprintf(_("Can't write '%s' file. Check FA directory write permissions."), $test['test'])
			: sprintf(_("'%s' file exists."), $test['test']);
	} else {
		$test['type'] = 2;
		$test['result'] = is_file($test['test']) && !is_writable($test['test']);
		$test['comments'][] = sprintf(_("'%s' file should be read-only"), $test['test']);
	}
	return $test;
}

function tst_dbconfig($install)
{
	global $path_to_root;

	$test['descr'] = _('Database auth file');
	$test['test'] = $path_to_root.'/config_db.php';

	if ($install) {
		$test['type'] = 3;
		$writable = check_write($test['test']);
		$test['result'] = $writable==1;
		$test['comments'][] = $writable == 0 ?
			sprintf(_("Can't write '%s' file. Check FA directory write permissions."), $test['test'])
			: sprintf(_("'%s' file exists."), $test['test']);
	} else {
		$test['type'] = 2;
		$test['result'] = is_file($test['test']) && !is_writable($test['test']);
		$test['comments'][] = sprintf(_("'%s' file should be read-only if you do not plan to add or change companies"), $test['test']);
	}
	return $test;
}

function tst_extconfig($install)
{
	global $path_to_root, $db_connections;

	$comps = $install ? array('0') : array_keys($db_connections);
	
	$test['descr'] = _('Extensions system');
	$test['type'] = 3;
	$fname =  $path_to_root.'/installed_extensions.php';
	$test['test'][] = $fname;
	$test['result'] = ($install || is_file($fname)) && check_write($fname);
	$test['test'][] = company_path().'/*/installed_extensions.php';
	if (!$test['result'])
		$test['comments'][] = sprintf(_("'%s' is not writeable"), $fname);

	foreach ($comps as $n) {
		$path = company_path($n);
		if (!is_dir($path)) continue;

		$path .= "/installed_extensions.php";
		if ((!$install && !is_file($path)) || !check_write($path) ) {
			$test['result'] = false;
			$test['comments'][] = sprintf(_("'%s' is not writeable"), $path);
			continue;
		};
	}
	foreach(array('modules', 'modules/_cache', 'themes', 'sql') as $dir) {
		$fname =  $path_to_root.'/'.$dir;
		$test['test'][] = $fname;
		$t = is_dir($fname) && is_writable($fname);
		if (!$t)
			$test['comments'][] = sprintf(_("'%s' is not writeable"), $fname);
		$test['result'] &= $t;
	}

	foreach(array('Release', 'Themes', 'Languages', 'Extensions', 'Charts') as $file) {
		$fname = $path_to_root."/modules/_cache/".$file.".gz";
		$t = !file_exists($fname) || is_writable($fname);
		if (!$t)
			$test['comments'][] = sprintf(_("'%s' is not writeable"), $fname);
		$test['result'] &= $t;
	}

	if(!$test['result'])
		$test['comments'][] = _("Extensions configuration files and directories should be writeable");

	$fname = $path_to_root."/themes";
	$themedir = opendir($fname);
	while (false !== ($fname = readdir($themedir)))
	{
		if ($fname!='.' && $fname!='..' && is_dir($path_to_root.'/themes/'.$fname)
			&& !in_array($fname, array('canvas', 'default', 'dropdown')))
		{
			$test['test'][] = $fname;
			$test['result'] = is_writable($path_to_root.'/themes/'.$fname);
			if (!$test['result']) {
				$test['comments'][] = 
					sprintf(_("Non-standard theme directory '%s' is not writable"), $fname);
				break;
			}
		}
	}
	closedir($themedir);

	$test['test'][] = 'OpenSSL PHP extension';
	if (!extension_loaded('openssl')) {
		$test['result'] = false;
		$test['comments'][] = _("OpenSSL PHP extension have to be enabled to use extension repository system.");
	} elseif (!function_exists('openssl_verify')) {
		$test['result'] = false;
		$test['comments'][] = _("OpenSSL have to be available on your server to use extension repository system.");
	}
	return $test;
}

function display_system_tests($install = false)
{
	global $test_level;

	if ($install)
		$system_tests = array('tst_php', 'tst_phpmysql', 'tst_system', 'tst_dbconfig', 
			'tst_config',
			'tst_subdirs', 'tst_langs', 'tst_tmpdir', 'tst_sessionpath', 'tst_extconfig'
		);
	else
		$system_tests = array('tst_mysql', 'tst_php', 'tst_server', 'tst_system', 'tst_browser',
			'tst_gettext', 'tst_debug', 'tst_logging',
			'tst_dbversion', 'tst_subdirs', 'tst_langs', 'tst_tmpdir', 'tst_sessionpath',
			'tst_install', 'tst_dbconfig', 'tst_config', 'tst_extconfig'
		);


	start_table(TABLESTYLE, "width='80%'");
	$th = array(_("Test"), _('Test type'), _("Value"), _("Comments"));
	table_header($th);

	$ret = true;
	$k = 0; //row colour counter
	foreach ($system_tests as $test) 
	{
		alt_table_row_color($k);
		$result = $test($install);
		if (!$result) continue;
		
		label_cell($result['descr']);
		label_cell($test_level[$result['type']]);

		$res = is_array(@$result['test']) ? implode('<br>', $result['test']) 
			: $result['test'];
		label_cell($res);

		$comm = is_array(@$result['comments']) ? implode('<br>', $result['comments']) 
			: @$result['comments'];
		$color = ($result['result'] ? 'green': 
			($result['type']==3 ? 'red' :
			 ($result['type']==2 ? 'orange' : 'green')));
		label_cell("<span style='color:$color'>".
			($result['result'] ? _('Ok') : '<b>'.$comm.'</b>').'</span>');
		end_row();
		$ret = $ret && (($result['result']!=0) || ($result['type'] < 3));
	}
	end_table();

	return $ret;
}
