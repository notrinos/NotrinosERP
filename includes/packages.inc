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
include_once($path_to_root. "/includes/archive.inc");
include_once($path_to_root. "/includes/remote_url.inc");
include_once($path_to_root. "/includes/hooks.inc");

define('PKG_CACHE_PATH', $path_to_root.'/modules/_cache');
define('PUBKEY_PATH', $path_to_root);
//
// FrontAccounting package class
//
class package extends gzip_file {

	function __construct($filename, $basedir=null)
	{
		global $path_to_root;

		if (!$basedir) {
			$basedir = PKG_CACHE_PATH.'/'.substr(basename($filename), 0, -4);
			if (file_exists($basedir)) {
//				flush_dir($basedir, true); 
			} else
			mkdir($basedir);
		}
		parent::__construct($filename);
		$this->set_options(array('basedir'=> $basedir));
		$this->options['type'] = "pkg";
	}
	//
	//	Used by archive class. Use create_archive() instead.
	//	
	function create_pkg() 
	{
		return $this->create_gzip();
	}
	//
	//	Install package and clean temp directory.
	//
	function install()
	{
		global $path_to_root;
		
		$success = true;

		$this->set_options(array('overwrite' => 1));
		$this->extract_files(); // extract package in cache directory
		$cachepath = $this->options['basedir'];
		$ctrl = get_control_file("$cachepath/_init/config");

		$targetdir = $path_to_root.'/'.$ctrl['InstallPath'];

		if (!is_dir($targetdir))
			mkdir($targetdir);

		$dpackage = new package("$cachepath/_data", $targetdir);
		$dpackage->set_options(array('overwrite' => 1));

		$flist = $dpackage->extract_files(true);
		if (count($dpackage->error)) {
			$this->error = array_merge($this->error, $dpackage->error);
			return false;
		}
		copy_files($flist, $targetdir, "$cachepath/_back");
	
		$dpackage->extract_files(); //install package in target directory

		$install = hook_invoke($ctrl['Package'], 'install_extension', $dummy);
		$success &= $install===null || $install;
		$success &= count($dpackage->error) == 0;
		$this->error = array_merge($this->error, $dpackage->error);
		return $success;
	}
	//
	//	Removing package related sources
	//
	function uninstall()
	{
		global $path_to_root;

		$success = true;

		$cachepath = $this->options['basedir'];
		$ctrl = get_control_file("$cachepath/_init/config");

		$targetdir = $path_to_root.'/'.$ctrl['InstallPath'];

		$dpackage = new package("$cachepath/_data", $targetdir);

		$flist = $dpackage->extract_files(true);

		$success &= copy_files($flist, "$cachepath/_back", $targetdir, true);

		if (strpos($ctrl['InstallPath'], 'modules/') === 0) { // flush module directory
			flush_dir($targetdir, true);
			rmdir($targetdir);
		}

		$uninstall = hook_invoke($ctrl['Package'], 'uninstall_extension', $dummy);
		$success &= $uninstall===null || $uninstall;

		return $success;
	}
	//
	//	Purge all package related configuration and data.
	//
	function purge()
	{
		return true;
	}

}
//
// Changes field value read from control file (single, or multiline) into 
// arrays of subfields if needed.
//
function ufmt_property($key, $value)
{
	// indexes used in output arrays
	$sub_fields = array(
//		'MenuTabs' => array('url', 'access', 'tab_id', 'title', 'section'),
//		'MenuEntries' => array('url', 'access', 'tab_id', 'title'),
	);
	if (!isset($sub_fields[$key]))
		return $value==='' ? null : $value;

	$prop = array();

	if (!is_array($value))
		$value = array($value);
	foreach($value as $line) {
		$indexes = $sub_fields[$key];
		$ret = array();
		preg_match_all('/(["])(?:\\\\?+.)*?\1|[^"\s][\S]*/', $line, $match);
		foreach($match[0] as $n => $subf) {
			if ($match[1][$n])
				$val = strtr(substr($subf, 1, -1),
					array('\\"'=>'"'));
		else
				$val = $subf;
			if (count($indexes))
				$ret[array_shift($indexes)] = $val;
			else
				$ret[] = $val;
		}
		if (count($ret))
			$prop[] = $ret;
	}
	return $prop;
}
//=============================================================================
//
// Retrieve control file and return as associative array
//	$index is name of field used as key in result array, or null for numeric keys
//
function get_control_file($file, $index = false) {

	$list = gzopen($file, 'rb');
	if (!$list) return null;

	$repo = $pkg = array();
	$key = false; $value = '';
	$line = '';
	do {
		$line = rtrim($line);
		if ($line && ctype_space($line[0])) { // continuation of multiline property
			if (strlen(ltrim($line))) {
				if ($value !== '' && !is_array($value))
					$value = array($value);
				$value[] = ltrim($line);
				continue;
			}
		}
		if ($key) { // save previous property if any
			$pkg[$key] = ufmt_property($key, $value);
		}
		if (!strlen($line)) { // end of section
			if (count($pkg)) {
				if ($index !== true) {
					if ($index === false) break;
					if (!isset($pkg[$index])) {
						display_error(sprintf(_("No key field '%s' in file '%s'"), $index, $file));
						return null;
					}
					$repo[$pkg[$index]] = $pkg;
				} else
					$repo[] = $pkg;
			}
			$pkg = array(); 
			$key = null; $value = '';
			continue;
		} elseif (preg_match('/([^:]*):\s*(.*)/', $line, $m)) {
			$key = $m[1]; $value = $m[2];
			if (!strlen($key)) {
				display_error("Empty key in line $line");
				return null;
			}
		} else {
			display_error("File parse error in line $line");
			return null;
		}
		
	} while ((($line = fgets($list))!==false) || $key);
	fclose($list);

	return $index === false ? $pkg : $repo;
}
//
//	Save configuration data to control file.
//
function save_control_file($fname, $list, $zip=false) 
{
	$file = $zip ?  gzopen($fname, 'wb') : fopen($fname, 'wb');
	foreach($list as $section) {
		foreach($section as $key => $value) {
			if (is_array($value)) { // multiline value
				if (is_array(reset($value))) { // lines have subfields
					foreach($value as $i => $line) {
		// Subfields containing white spaces or double quotes are doublequoted 
		// with " escaped with backslash.
						foreach($line as $n => $subfield)
							if (preg_match('/[\s"]/', $subfield)) {
								$value[$i][$n] = 
									'"'.strtr($subfield, array('"'=>'\\"')).'"';
							}
						// Subfields are separated by white space.
						$value[$i] = implode(' ', $value[$i]);
					}
				}
				// array elements on subsequent lines starting with white space
				$value = implode("\n ", $value);
			}
			$zip ? gzwrite($file, "$key: $value\n") : fwrite($file, "$key: $value\n");
		}
		$zip ? gzwrite($file, "\n"): fwrite($file, "\n");
	}
	$zip ? gzclose($file) : fclose($file);
}
//
//	Retrieve text field in localized version or default one 
//	when the localized is not avaialable.
//
function pkg_prop($pkg, $property, $lang=false) 
{
	
	if ($lang && isset($pkg[$property.'-'.user_language()]))
		$prop = @$pkg[$pname];
	else
		$prop = @$pkg[$property];

	return is_array($prop) ? implode("\n ",$prop): $prop;
}
//
//	Retrieve list of packages from repository and return as table ($pkgname==null),
//	or find $pkgname package in repository and optionaly download
//
//	$type is type/s of package
//  $filter is optional field selection array in form field=>newkey
//	 	or (0=>field1, 1=>field2...)
//  $outkey - field used as a key in package list. If null 'Package' field is used.
//
function get_pkg_or_list($type = null, $pkgname = null, $filter=array(), $outkey=null, $download=true) {

	global $path_to_root, $repo_auth;

	$repo =  (isset($repo_auth['scheme']) ? $repo_auth['scheme'] : 'http://')
			.(isset($repo_auth['login'])  ? $repo_auth['login'].':' : '')
			.(isset($repo_auth['pass'])   ? $repo_auth['pass'].'@' : '')
			.(isset($repo_auth['host'])   ? $repo_auth['host'].'/' : '')
			.(isset($repo_auth['path'])   ? $repo_auth['path'].'/' : '')
			.$repo_auth['branch'];

	// first download local copy of repo release file
	// and check remote signature with local copy of public key
	//
	$loclist = PKG_CACHE_PATH.'/Release.gz';
	$target_dir = $download==true ? VARLIB_PATH."/" : $download;

	if (isset($type) && !is_array($type)) {
		$type = array($type);
	}
	$refresh = true;
	do{
		if (!file_exists($loclist)) {
			if (!url_copy($repo.'/Release.gz', $loclist))
			{
				display_error(_("Cannot download repo index file." ));
				return null;
			}
			$refresh = false;
		}
		$sig = url_get_contents($repo.'/Release.sig');
		$data = file_get_contents($loclist);
		$cert = file_get_contents(PUBKEY_PATH.'/FA.pem');
		if (!function_exists('openssl_verify')) {
			display_error(_("OpenSSL have to be available on your server to use extension repository system."));
			return null;
		}	
		if (openssl_verify($data, $sig, $cert) <= 0) {
			if ($refresh) {
				if (!@unlink($loclist))
				{
					display_error(sprintf(_("Cannot delete outdated '%s' file."), $loclist));
					return null;
				}
			} else {
				display_error(_('Release file in repository is invalid, or public key is outdated.'));
				return null;
			}
		} else
			$refresh = false;

	} while($refresh);

	$Release = get_control_file($loclist, 'Filename');
	// download and check all indexes containing given package types
	// then complete package list or seek for pkg
	$Packages = array();
	foreach($Release as $fname => $parms) {
		if ($type && !count(array_intersect(explode(' ', $parms['Type']), $type))) {
			unset($Release[$fname]); continue; // no packages of selected type in this index
		}
		if ($Release[$fname]['Version'] != $repo_auth['branch']) {
			display_warning(_('Repository version does not match application version.')); // ?
		}
		$remoteindex = $repo.'/'.$fname;
		$locindex = PKG_CACHE_PATH.'/'.$fname;
		$refresh = true;
		do{
			if (!file_exists($locindex)) { 
				if (!url_copy($remoteindex, $locindex)) {
					display_error(sprintf(_("Cannot download '%s' file." ), $fname));
					return null;
				}
				$refresh = false;
			}
			if ($parms['SHA1sum'] != sha1_file($locindex)) {	// check subdir index consistency
				if ($refresh) {
					if (!@unlink($locindex)) {
						display_error(sprintf(_("Cannot delete outdated '%s' file."), $locindex));
						return null;
					}
				} else {
					display_error(sprintf( _("Security alert: broken index file in repository '%s'. Please inform repository administrator about this issue."),
						$fname));
					return null;
				}
			} else
				$refresh = false;
		} while($refresh);
		
		 // scan subdir list and select packages of given type
		$pkglist = get_control_file($locindex, 'Package');
		foreach($pkglist as $name => $pkg) {
			$pkgfullname = $repo.'/'.$parms['Path']."/".$pkg['Filename'].'.pkg';
			if (!isset($type) || in_array($pkg['Type'], $type)) {
				if (empty($filter))
					$p = $pkg;
				else {
					foreach($filter as $field => $key) {
						if (is_numeric($field))
							$p[$field] = @$pkg[$field];
						else
							$p[$key] = @$pkg[$field];
					}
				}
				if ($pkgname == null) {
					$Packages[$outkey ? $outkey : $name] = $p;
				} elseif ($pkgname == $pkg['Package']) {
					//download package to temp directory
					if ($download) {
						$locname = $target_dir.$pkg['Filename'].'.pkg';
						if (!url_copy($pkgfullname, $locname)) {
							display_error(sprintf(_("Cannot download '%s' file." ), $pkgfullname));
							return null;
						}
						 // checking sha1 hash is expensive proces, so chekc the package
						 // consistency just before downloading
						if ($pkg['SHA1sum'] != sha1_file($locname)) {
							display_error(sprintf( _("Security alert: broken package '%s' in repository. Please inform repository administrator about this issue."),
								$pkgfullname));
							return null;
						}
					}
					return $p;
				}
			}
		}
	}

	return $Packages;
}

function get_package($pkgname, $type = null)
{
	return get_pkg_or_list($type, $pkgname);
}
/*
	Returns full name of installed package, or null if package is not installed.
*/
function installed_package($package)
{
	$cache = opendir(PKG_CACHE_PATH);

	while ($file = @readdir($cache)) {
		if (!is_dir(PKG_CACHE_PATH.'/'.$file))
			continue;
		if (strpos($file, $package.'-') === 0)
			return $file;
	}
	@closedir($cache);

	return null;
}
/*
	Remove package from system
*/
function uninstall_package($name)
{
	$name = installed_package($name);
	if (!$name) return true; // not installed
	$pkg = new package($name.'.pkg');
	$pkg->uninstall();
	if($name) {
		flush_dir(PKG_CACHE_PATH.'/'.$name, true);
		rmdir(PKG_CACHE_PATH.'/'.$name);
	}
	return count($pkg->error)==0;
}

//---------------------------------------------------------------------------------------
//
//	Return merged list of available and installed languages in inform of local 
// configuration array supplemented with installed versions information.
//
function get_languages_list()
{
	global $installed_languages;
	
	$pkgs = get_pkg_or_list('language', null, array(
				'Package' => 'package',
				'Version' => 'available',
				'Name' => 'name',
				'Language' => 'code',
				'Encoding' => 'encoding',
				'RTLDir' => 'rtl',
				'Description' => 'Descr',
				'InstallPath' => 'path'
			));

	// add/update languages already installed
	// 
	foreach($installed_languages as $id => $l) {
		$list = array_search_keys($l['code'], $pkgs, 'code');	// get all packages with this code
		foreach ($list as $name) {
			if ($l['encoding'] == $pkgs[$name]['encoding']) {	// if the same encoding
				$pkgs[$name]['version'] = @$l['version'];		// set installed version
				$pkgs[$name]['local_id'] = $id;		// index in installed_languages
				continue 2;
			}
		}
		$l['local_id'] = $id;
		if (!isset($l['package']) || $l['package'] == '' || !isset($pkgs[$l['package']]))
			$pkgs[] = $l;
		else
			$pkgs[$l['package']] = array_merge($pkgs[$l['package']], $l);
	}
	if ($pkgs)
		ksort($pkgs);
	return $pkgs;
}
//---------------------------------------------------------------------------------------
//
//	Return merged list of available and installed extensions as a local 
// configuration array supplemented with installed versions information.
//
function get_extensions_list($type = null)
{
	global $path_to_root;

	if (isset($type) || !is_array($type)) {
		$type = array($type);
	}

	$pkgs = get_pkg_or_list($type, null, array(
				'Package' => 'package',
				'Version' => 'available',
				'Name' => 'name',
				'Description' => 'Descr',
				'Type' => 'type',
				'DefaultStatus' => 'active',
//				'MenuTabs' => 'tabs',
//				'MenuEntries' => 'entries',
				'Encoding' => 'encoding',
//				'AccessExtensions' => 'acc_file',
				'InstallPath' => 'path'
			));

	// lookup for local extensions
	$path = $path_to_root.'/modules/';
	$loc = array();
	$moddir = opendir($path);

	while(false != ($fname = readdir($moddir)))
	{
		if(!in_array($fname, array('.','..','CVS','_cache')) && is_dir($path.$fname))
		{
			if (!isset($pkgs[$fname]))
				$pkgs[$fname] = array(
					'package' => $fname,
					'name' => $fname,
					'version' => '',
					'available' => '',
					'type' => 'extension',
					'path' => 'modules/'.$fname,
					'active' => false
					);
		}
	}

	// add/update extensions already installed
	// 
	$installed = get_company_extensions();
	foreach($installed as $extno => $ext) {
		if (!in_array($ext['type'], $type)) continue;
		$ext['local_id'] = $extno;
//		if (!isset($pkgs[$ext['package']]) || $ext['package'] == '')
//			$pkgs[] = $ext;
//		else
			$pkgs[$ext['package']] = array_merge($pkgs[$ext['package']], $ext);
	}
	if ($pkgs)
		ksort($pkgs);
	return $pkgs;
}
//
// Return merged list of available and installed extensions as a local
// configuration array supplemented with installed versions information.
//
function get_themes_list()
{
	$pkgs = get_pkg_or_list('theme', null, array(
				'Package' => 'package',
				'Version' => 'available',
				'Name' => 'name',
				'Description' => 'Descr'
			));

	// add/update extensions already installed
	// 
	$local = get_company_extensions();
	
	foreach($local as $extno => $ext) {
		if (isset($pkgs[@$ext['package']])) {
			$ext['local_id'] = $extno;
			$pkgs[$ext['package']] = array_merge($pkgs[$ext['package']], $ext);
		}
	}
	// TODO: Add other themes from themes directory
	if ($pkgs)
		ksort($pkgs);
	return $pkgs;
}
//---------------------------------------------------------------------------------------
//
//	Return merged list of available and installed COAs as a local 
// configuration array supplemented with installed versions information.
//
function get_charts_list()
{
	$pkgs = get_pkg_or_list('chart', null, array(
				'Package' => 'package',
				'Version' => 'available',
				'Name' => 'name',
				'Description' => 'Descr',
				'Type' => 'type',
				'InstallPath' => 'path',
				'Encoding' => 'encoding',
				'SqlScript' => 'sql'
			));

	// add/update default charts
	// 
	$local = get_company_extensions();

	foreach($local as $extno => $ext) {
		if ($ext['type'] != 'chart') continue;
		$ext['local_id'] = $extno;
		if (!isset($pkgs[$ext['package']]) || $ext['package'] == '')
			$pkgs[] = $ext;
		else
			$pkgs[$ext['package']] = array_merge($pkgs[$ext['package']], $ext);
	}
	if ($pkgs)
		ksort($pkgs);
	return $pkgs;
}
//---------------------------------------------------------------------------------------------
//	Install/update package from repository
//
function install_language($pkg_name)
{
	global $path_to_root, $installed_languages, $Ajax;
	
	$pkg = get_pkg_or_list('language', $pkg_name);

	if ($pkg) {
		$i = array_search_key($pkg['Language'], $installed_languages, 'code');
		if ($i === null)
			$i = count($installed_languages);
		else {	// remove another already installed package for this language 
			$old_pkg = @$installed_languages[$i]['package'];
			if ($old_pkg && ($pkg['Package'] != $old_pkg))
				uninstall_package($old_pkg);
		}

		$package = new package(VARLIB_PATH."/".$pkg['Filename'].'.pkg');
		if ($package->install()) {
			$lang = array(
				'name' => $pkg['Name'],
				'package' => $pkg['Package'],
				'code' => $pkg['Language'],
				'encoding' => $pkg['Encoding'],
				'version' => $pkg['Version'],
				'path' => $pkg['InstallPath']
			);
			if ($pkg['RTLDir']=='yes')
				$lang['rtl'] = true;
			$installed_languages[$i] = $lang;
			write_lang($installed_languages);
			unlink(VARLIB_PATH."/".$pkg['Filename'].'.pkg');
			$Ajax->activate('lang_tbl');
		} else {
			display_error(implode('<br>', $package->error));
			return false;
		}
	} else {
		display_error(sprintf(_("Package '%s' not found."), $pkg_name));
		return false;
	}
	return true;
}
//---------------------------------------------------------------------------------------------
//	Install/update extension or theme package from repository
//
function install_extension($pkg_name)
{
	global $path_to_root, $installed_extensions, $next_extension_id, $Ajax, $db_connections;
	
	$pkg = get_pkg_or_list(array('extension', 'theme', 'chart'), $pkg_name);
	if ($pkg) {
		$package = new package(VARLIB_PATH."/".$pkg['Filename'].'.pkg');
		$local_exts = get_company_extensions();
		if ($package->install()) {
			$ext_id = array_search_key($pkg['Package'], $local_exts, 'package');
			if ($ext_id === null)
				$ext_id = $next_extension_id++;
			else {	// remove another already installed package for this language 
				$old_pkg = $installed_extensions[$ext_id]['package'];
				if ($old_pkg)
					uninstall_package($old_pkg);
			}
			$ext = array(
				'name' => $pkg['Name'],
				'package' => $pkg['Package'],
				'version' => $pkg['Version'],
				'type' => $pkg['Type'],
				'active' => @$pkg['DefaultStatus'] == 'active' ? true : false,
				'path' => $pkg['InstallPath'],
			);
			if (isset($pkg['SqlScript']))
				$ext['sql'] = $pkg['SqlScript'];

			$local_exts[$ext_id] = $ext;
			$ret = update_extensions($local_exts);

			if (($ext['active'] == true) && file_exists($path_to_root.'/'.$ext['path'].'/hooks.php'))
			{
				// we need to include the new hooks file to activate extension
				include_once($path_to_root.'/'.$ext['path'].'/hooks.php');
				foreach($db_connections as $comp => $db)
					activate_hooks($ext['package'], $comp);
			}

			unlink(VARLIB_PATH."/".$pkg['Filename'].'.pkg');
			$Ajax->activate('ext_tbl');
			return $ret;
		} else {
			display_error(implode('<br>', $package->error));
			return false;
		}
	} else {
		display_error(sprintf(_("Package '%s' not found."), $pkg_name));
		return false;
	}
	return true;
}
/*
	Returns true if newer package version is available
*/
function check_pkg_upgrade($current, $available)
{
	preg_match_all('/[\d]+/', $available, $aver);
	if (!count($aver[0]))
		return false;
	preg_match_all('/[\d]+/', $current, $cver);
	if (!count($cver[0]))
		return true;
	foreach($aver[0] as $n => $ver)
		if ($ver>@$cver[0][$n]) 
			return true;
	return false;
}

//
//	Returns package info from index file
//
function get_package_info($pkg, $type=null, $filter=array(), $outkey=null, $download=true) {
	return get_pkg_or_list($type, $pkg, $filter, null, false);
}

/*
	Check basic extension source compatibility.
*/
function check_src_ext_version($ext_v)
{
    global $src_version;
    if ($ext_v != '-') {
        $compat_levels = 2;    // current policy is keeping compatibility on major version level.
        $app = explode('.', substr($src_version, 0, strspn($src_version, "0123456789.")));
        $pkg = explode('.', substr($ext_v, 0, strspn($ext_v, "0123456789.")));

        for ($i=0; $i < min($compat_levels, count($app)); $i++)
            if ($pkg[$i] < $app[$i])
                return false;
    }
    return true;
}
