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
include_once($path_to_root.'/includes/packages.inc');
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
	var $backup_created = false;	// preserve phase evidence if sql_install proves no mutation
	var $recovery_state = null;	// stable execution/recovery phase
	var $mutation_started = false;
	var $preflight_state = null;
	var $preflight_detail = '';

	var $errors = array();
	var	$max_upgrade_time = 300;
	var $companies = null;
	var $save_log;

	function __construct() {
		global $path_to_root;

		include $path_to_root.'/config_db.php';

		$this->companies = $db_connections;

		return $this->companies;
	}

	/*
		Collect/log messages generated during upgrade process.
	*/
	function log_error($msg, $type='Error') {
		if ($type == 'Error')
			$this->errors[] = $msg;
		error_log(sprintf('[%s] %s', $type, $msg));
		return false;
	}

	/*
		Record one stable upgrade/recovery phase. prepare() runs before the
		security backup, so a prepare failure must not imply a backup exists.
	*/
	function set_recovery_state($state) {
		$states = array(
			'PREPARE_FAILED_BEFORE_MUTATION',
			'BACKUP_FAILED',
			'INSTALL_FAILED_AFTER_BACKUP',
			'POST_INSTALL_FAILED_AFTER_MUTATION',
			'SUCCESS'
		);
		if (!in_array($state, $states, true))
			return false;
		$this->recovery_state = $state;
		$this->log_error($state, 'Info');
		return true;
	}

	/* Restore only when the standard framework actually created a backup. */
	function restore_security_backup() {
		if (!$this->backup)
			return false;
		if (!set_global_connection($this->cur_company))
			return $this->log_error(_('Cannot connect to company database for database restore.'));

		set_time_limit($this->max_upgrade_time);
		$result = db_import($this->backup, $this->companies[$this->cur_company], true, false);
		if ($result)
			$this->log_error(_('Upgrade failed. Original database content restored successfully.'), 'Info');
		else
			$this->log_error(sprintf(_('Database restore operation failed. Original database content is in %s file.'), $this->backup));
		$this->post_fail($this->cur_company);
		return $result;
	}

	/*
		Selectively extends access to selected security areas/sections.
	*/
	function update_security_roles($sec_updates) {
		global $security_areas, $security_sections;

		$roles = db_query('SELECT * FROM '.TB_PREF.'security_roles');

		while($role = db_fetch($roles)) {
			$role['areas'] = explode(';', $role['areas']);
			$role['sections'] = explode(';', $role['sections']);

			foreach($sec_updates as $has => $add) {

				if (in_array($security_areas[$has][0], $role['areas'])) {
					$sections = array();

					foreach($add as $area) {
						$role['areas'][] = $security_areas[$area][0];
						$role['sections'][] = $security_areas[$area][0]&~0xff;
					}
					sort($role['areas']);
					update_security_role($role['id'], $role['role'], $role['description'], array_values($role['sections']), array_values($role['areas']));
				}
			}
		}
		return true;
	}

	/*
		Check and disable incompatible extensions.
	*/
	function update_extensions() {
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
	function pre_install($company) {
		global $SysPrefs;

		$this->cur_company = $company;
		$this->errors = array();
		$this->backup = null;
		$this->backup_created = false;
		$this->recovery_state = null;
		$this->mutation_started = false;
		$this->preflight_state = null;
		$this->preflight_detail = '';

		$this->save_log = ini_set('error_log', VARLOG_PATH.'/upgrade.'.$this->cur_company.'.log');
		$this->log_error(sprintf(_("Upgrade started for company %s."), $this->cur_company), 'Info');

		if (!set_global_connection($this->cur_company)) {
			$this->set_recovery_state('PREPARE_FAILED_BEFORE_MUTATION');
			return $this->log_error(_('Cannot connect to company database.'));
		}

		$cur_ver = get_company_pref('version_id', true);
		if ($cur_ver != $this->previous) {
			$this->set_recovery_state('PREPARE_FAILED_BEFORE_MUTATION');
			return $this->log_error(sprintf(_("Cannot upgrade company %s: database version is incompatible ('%s' instead of '%s')."), $this->cur_company, $cur_ver, $this->previous));
		}

		$this->update_extensions();	// disable uncompatible extensions

		if (!$this->prepare()) { // fetch params, perform additional checks (if any)
			$this->set_recovery_state('PREPARE_FAILED_BEFORE_MUTATION');
			return false;
		}

		if (!$this->sql)
			return true;	// skip security backup if database content is not changed

		$this->backup = db_backup($this->companies[$this->cur_company], 'no', 'Security backup before upgrade',
			$SysPrefs->backup_dir($this->cur_company));

		if (!$this->backup) {
			$this->set_recovery_state('BACKUP_FAILED');
			return $this->log_error(_('Security backup failed.'));
		}
		$this->backup_created = true;

		$this->log_error(sprintf(_("Security backup in file %s done."), $this->backup), 'Info');
		return true;
	}

	/*
		Basic install procedure using sql file.
	*/
	function sql_install($company, $force=false) {
		global $path_to_root;

		if ($this->sql != '') {// perform basic upgrade operations defined in sql file
			$result = true;

			if ($result === true)
				$result = db_import($path_to_root. '/sql/'.$this->sql, $this->companies[$company],
					$force,	true, false, true);

			if ($result !== true) {
				if (is_array($result)) {
					foreach($result as $err)
						$this->log_error($err[1] . ':'. $err[0]);
				}
				else {
					$this->log_error($result);
					$this->backup = null; // prevent restore (database was not touched due to other errors)
				}
				return false;
			}
		}
		return true;
	}

	/*
		Post install procedures: update database version, or restore databse from backup file in case of errors
	*/
	function post_install($result=true) {
		global $db_version;

		if ($result === true) {
			if (!update_company_prefs(array('version_id' => $this->version))) {
				$this->set_recovery_state('POST_INSTALL_FAILED_AFTER_MUTATION');
				$result = false;
				$this->restore_security_backup();
			}
			else
				$this->set_recovery_state('SUCCESS');
		}
		else
			$this->restore_security_backup();

		$this->log_error(sprintf(_("Upgrade for company %s finished."), $this->cur_company), 'Info');

		set_global_connection();
		ini_set('error_log', $this->save_log);

		db_set_encoding();

		return $result;
	}

	/*
		Main routine for single company upgrade.
	*/
	function upgrade_company($comp, $force=false) {
		$result = $this->pre_install($comp);
		if ($result) {
			$sql_result = $this->sql_install($comp, $force);
			if ($sql_result)
				$this->mutation_started = $this->sql != '';
			$install_result = $sql_result && $this->install($comp, $force);
			$result = $sql_result && $install_result;
			if (!$result) {
				if ($this->backup_created)
					$this->set_recovery_state('INSTALL_FAILED_AFTER_BACKUP');
				else if ($this->recovery_state === null)
					$this->set_recovery_state('PREPARE_FAILED_BEFORE_MUTATION');
			}
		}

		$this->post_install($result);

		return count($this->errors) == 0;
	}

	/*
		Additional version specific php/sql upgrade procedures.
		This procedure is performed after basic sql upgrade script is run.
	*/
	function install($company, $force=false) {
		return true;
	}
	/*
		Optional cleanup procedure.
		This procedure is run in case of upgrade failure, before the backup is restored.
	*/
	function post_fail($company){
	}

	/*
		Present upgrade parameters to administrator
		This function presents upgrade choices, after selection company to be upgraded.
	*/
	function show_params($comp){
	}

	/*
		Fetch & check upgrade parameters, check additional upgrade pre-conditions.
		This function is run after successfull switching to target database connection.
	*/
	function prepare() {
		return true;
	}
}

/*
	Return databases status info.
*/
function company_pref_table_available($connection) {
	$probe = false;
	try {
		$probe = @mysqli_connect($connection['host'], $connection['dbuser'],
			$connection['dbpassword'], $connection['dbname'],
			!empty($connection['port']) ? $connection['port'] : 3306);
	} catch (Throwable $exception) {
		return false;
	}
	if (!$probe)
		return false;

	$physical_prefs_table = $connection['tbpref'].'sys_prefs';
	$result = @mysqli_query($probe, "SELECT 1 FROM INFORMATION_SCHEMA.TABLES "
		."WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='"
		.mysqli_real_escape_string($probe, $physical_prefs_table)."'");
	$available = $result && mysqli_num_rows($result) === 1;
	if ($result)
		mysqli_free_result($result);
	mysqli_close($probe);
	return $available;
}

function get_site_status($connections) {
		global $SysPrefs;

		$info = array();

		foreach($connections as $i => $conn) {
			$info[$i]['name'] = $conn['name'];
			$info[$i]['table_set'] = $conn['host'].'/'.$conn['dbname'].':'.$conn['tbpref'].'*';
			$info[$i]['version'] = null;
			$info[$i]['status'] = company_pref_table_available($conn)
				&& set_global_connection($i) !== false;

			if ($info[$i]['status'])
				$info[$i]['version'] = get_company_pref('version_id');
		}
		set_global_connection();
		$SysPrefs->refresh();

		return $info;
}

/*
	Creates table of installer objects sorted by applicable db scheme version.
*/
function get_installers() {
	global $path_to_root;

	$patchdir = $path_to_root.'/sql/';
	$upgrades = array();	
	$datadir = @opendir($patchdir);

	if ($datadir) {
		while(false !== ($fname = readdir($datadir))) { // check installer php files only
			if (!is_dir($patchdir . $fname) && strpos($fname, 'upgrade_') === 0
				&& substr($fname, -4) === '.php') {
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
