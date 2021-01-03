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
include_once($path_to_root."/includes/packages.inc");
//
//	Utility class contains basic database upgrade routines.
//
class fa_patch {
	var $previous;				// previous database version 
	var $version;		 		// version after upgrade
	var $description;			// short patch description

	var $sql;					// basic sql file

	var $cur_company;
	var $backup;				// pre-upgrade backup filename

	var $errors = array();
	var	$max_upgrade_time = 300;

	function __construct()
	{
		global $path_to_root;

		include $path_to_root."/config_db.php";

		$this->companies = $db_connections;

		return $this->companies;
	}

	/*
		Collect/log messages generated during upgrade process.
	*/
	function log_error($msg, $type='Error')
	{
		if ($type == 'Error')
			$this->errors[] = $msg;
		error_log(sprintf('[%s] %s', $type, $msg));
		return false;
	}

	/*
		Selectively extends access to selected security areas/sections.
	*/
	function update_security_roles($sec_updates)
	{
		global $security_areas, $security_sections;

		$roles = db_query("SELECT * FROM ".TB_PREF."security_roles");

		while($role = db_fetch($roles))
		{
			$role['areas'] = explode(';', $role['areas']);
			$role['sections'] = explode(';', $role['sections']);
			foreach($sec_updates as $has => $add)
			{
				if (in_array($security_areas[$has][0], $role['areas']))
				{
					$sections = array();
					foreach($add as $area)
					{
						$role['areas'][] = $security_areas[$area][0];
						$role['sections'][] = $security_areas[$area][0]&~0xff;
					}
					sort($role['areas']);
					update_security_role($role['id'], $role['role'], $role['description'], 
						array_values($role['sections']), array_values($role['areas']));
				}
			}
		}
		return true;
	}

	/*
		Check and disable incompatible extensions.
	*/
	function update_extensions()
	{
		global $version;

		$mods = get_company_extensions();
		$exts = get_company_extensions($this->cur_company);

		$fixed = false;
		foreach($mods as $key => $ins) {
			foreach($exts as $ext)
				if ($ext['name'] == $ins['name'] && (!check_src_ext_version($ins['version']))) {
					$mods[$key]['active'] = false;
					$this->log_error(sprintf(_("Uncompatible extension '%s' disabled for company %d."), $ins['name'], $this->cur_company), 'Notice');
					$fixed = true;
					continue 2;
				}
		}
		if ($fixed)
			write_extensions($mods, $this->cur_company);
	}

	/*
		Pre-install maintenance: login to company, open upgrade log, make a backup
	*/
	function pre_install($company)
	{
		global $SysPrefs;

		$this->cur_company = $company;
		$this->errors = array();
		$this->backup = null;

		$this->save_log = ini_set('error_log', VARLOG_PATH.'/upgrade.'.$this->cur_company.'.log');
		$this->log_error(sprintf(_("Upgrade started for company %s."), $this->cur_company), 'Info');

		if (!set_global_connection($this->cur_company))
			return $this->log_error(_("Cannot connect to company database."));

		$cur_ver = get_company_pref('version_id', true);
		if ($cur_ver != $this->previous)
			return $this->log_error(sprintf(_("Cannot upgrade company %s: database version is incompatible ('%s' instead of '%s')."),
				$this->cur_company, $cur_ver, $this->previous));

		$this->update_extensions();	// disable uncompatible extensions

		if (!$this->prepare())	// fetch params, perform additional checks (if any)
		  return false;

		if (!$this->sql)
			return true;	// skip security backup if database content is not changed

		$this->backup = db_backup($this->companies[$this->cur_company], 'no', 'Security backup before upgrade',
			$SysPrefs->backup_dir($this->cur_company));

		if (!$this->backup)
		  return $this->log_error(_("Security backup failed."));

		$this->log_error(sprintf(_("Security backup in file %s done."), $this->backup), 'Info');
		return true;
	}

	/*
		Basic install procedure using sql file.
	*/
	function sql_install($company, $force=false) 
	{
		global $path_to_root;

		if ($this->sql != '')	// perform basic upgrade operations defined in sql file
		{
			$result = true;

			if ($result === true)
				$result = db_import($path_to_root. '/sql/'.$this->sql, $this->companies[$company],
					$force,	true, false, true);

			if ($result !== true)
			{
				if (is_array($result))
				{
					foreach($result as $err)
						$this->log_error($err[1] . ':'. $err[0]);
				} else
				{
					$this->log_error($result);
					unset($this->backup); // prevent restore (database was not touched due to other errors)
				}
				return false;
			}
		}
		return true;
	}

	/*
		Post install procedures: update database version, or restore databse from backup file in case of errors
	*/
	function post_install($result=true)
	{
		global $db_version;

		if ($result !== true)
		{
			if ($this->backup)
			{
				if (!set_global_connection($this->cur_company)) // reset connection to clear encoding
					return $this->log_error(_("Cannot connect to company database for database restore."));

				set_time_limit($this->max_upgrade_time);
				$result = db_import($this->backup, $this->companies[$this->cur_company], true, false);
				if ($result)
					$this->log_error(_("Upgrade failed. Original database content restored successfully."), 'Info');
				else
					$this->log_error(sprintf(_("Database restore operation failed. Original database content is in %s file."), $this->backup));
				$this->post_fail($this->cur_company);
			}
		} else {
			update_company_prefs(array('version_id' => $this->version));
		}

		$this->log_error(sprintf(_("Upgrade for company %s finished."), $this->cur_company), 'Info');

		set_global_connection();
		ini_set('error_log', $this->save_log);

		if (db_fixed())
			db_set_encoding();

		return $result;
	}

	/*
		Main routine for single company upgrade.
	*/
	function upgrade_company($comp, $force=false)
	{
		$result = $this->pre_install($comp) && $this->sql_install($comp, $force) && $this->install($comp, $force);

		$this->post_install($result);

		return count($this->errors) == 0;
	}

	/*
		Additional version specific php/sql upgrade procedures.
		This procedure is performed after basic sql upgrade script is run.
	*/
	function install($company, $force=false) 
	{
		return true;
	}
	/*
		Optional cleanup procedure.
		This procedure is run in case of upgrade failure, before the backup is restored.
	*/
	function post_fail($company)
	{
	}

    /*
		Present upgrade parameters to administrator
		This function presents upgrade choices, after selection company to be upgraded.
    */
	function show_params($comp)
    {
	}

    /*
	    Fetch & check upgrade parameters, check additional upgrade pre-conditions.
		This function is run after successfull switching to target database connection.
    */
	function prepare()
    {
		return true;
	}

}

/*
	Return databases status info.
*/
function get_site_status($connections)
{
		global $SysPrefs;

		$info = array();

		foreach($connections as $i => $conn)
		{
			$info[$i]['status'] = set_global_connection($i) !== false;

			$info[$i]['name'] = $conn['name'];
			$info[$i]['table_set'] = $conn['host'].'/'.$conn['dbname'].':'.$conn['tbpref'].'*';
			if ($info[$i]['status'])
			{
				$info[$i]['version'] = get_company_pref('version_id');
			}
		}
		set_global_connection();
		$SysPrefs->refresh();

		return $info;
}

/*
	Creates table of installer objects sorted by applicable db scheme version.
*/
function get_installers()
{
	global $path_to_root;

	$patchdir = $path_to_root."/sql/";
	$upgrades = array();	
	$datadir = @opendir($patchdir);

	if ($datadir)
	{
		while(false !== ($fname = readdir($datadir)))
		{ // check all php files but index.php
			if (!is_dir($patchdir . $fname) && ($fname != 'index.php')
				&& stristr($fname, '.php') != false && $fname[0] != '.')
			{
				unset($install);
				include_once($patchdir . $fname);
				if (isset($install)) // add installer if found
					$upgrades[$install->previous] =  $install;
			}
		}
		ksort($upgrades); // sort by file name
	}
	return $upgrades;
}
