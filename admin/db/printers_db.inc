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

function write_printer_def($id, $name, $descr, $queue, $host, $port, $timeout)
{
	if ($id>0)
		$sql = "UPDATE ".TB_PREF."printers SET description=".db_escape($descr)
		.",name=".db_escape($name).",queue=".db_escape($queue)
		.",host=".db_escape($host).",port=".db_escape($port).",timeout=".db_escape($timeout)
		." WHERE id=".db_escape($id);
	else 
		$sql = "INSERT INTO ".TB_PREF."printers ("
			."name,description,queue,host,port,timeout) "
			."VALUES (".db_escape($name).",".db_escape($descr).","
			.db_escape($queue).",".db_escape($host).",".db_escape($port).",".db_escape($timeout).")";

	return db_query($sql,"could not write printer definition");
}

function get_all_printers() 
{
	$sql = "SELECT * FROM ".TB_PREF."printers";
	return db_query($sql,"could not get printer definitions");
}

function get_printer($id)
{
		$sql = "SELECT * FROM ".TB_PREF."printers
			WHERE id=".db_escape($id);

		$result = db_query($sql,"could not get printer definition");
		return  db_fetch($result);
}

function delete_printer($id)
{
	$sql="DELETE FROM ".TB_PREF."printers WHERE id=".db_escape($id);
	db_query($sql,"could not delete printer definition");
}
//============================================================================
// printer profiles functions
//
function update_printer_profile($name, $dest)
{
	foreach( $dest as $rep => $printer) {
		if ($printer != '' || $rep == '') {
			$sql = "REPLACE INTO ".TB_PREF."print_profiles "
			."(profile, report, printer) VALUES ("
			.db_escape($name).","
			.db_escape($rep).","
			.db_escape($printer ? $printer: null, true).")";
		} else {
			$sql = "DELETE FROM ".TB_PREF."print_profiles WHERE ("
				."report=" . db_escape($rep)
				." AND profile=".db_escape($name).")";
		}
		$result = db_query($sql,"could not update printing profile");
		if(!$result) {
			return false;
		}
	}
	return true;
}
//
//	Get destination for report defined in given printing profile.
//
function get_report_printer($profile, $report)
{
	$sql = "SELECT printer FROM ".TB_PREF."print_profiles WHERE "
		."profile=".db_escape($profile)." AND report=";

	$result = db_query($sql.db_escape($report), 'report printer lookup failed');

	if (!$result) return false;
	$ret = db_fetch($result);
	if ($ret === false) {
		$result = db_query($sql."''", 'default report printer lookup failed');
		if (!$result) return false;

		$ret = db_fetch($result);
		if (!$ret) return false;
	}
	return get_printer($ret['printer']);
}

function delete_printer_profile($name)
{
	$sql="DELETE FROM ".TB_PREF."print_profiles WHERE profile=".db_escape($name);
	return db_query($sql,"could not delete printing profile");
}
//
// Get all report destinations for given profile.
//
function get_print_profile($name)
{
	$sql = "SELECT	* FROM ".TB_PREF."print_profiles WHERE profile=".db_escape($name);
	return db_query($sql,"could not get printing profile");
}

