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
	Print request redirector. This file is fired via print link or 
	print button in reporting module. 
*/
$path_to_root = "..";
global $page_security;
$page_security = 'SA_OPEN';	// this level is later overriden in rep file
include_once($path_to_root . "/includes/session.inc");

if (user_save_report_selections() > 0 && isset($_POST['REP_ID'])) {	// save parameters from Report Center
	for($i=0; $i<12; $i++) { // 2013-01-16 Joe Hunt
		if (isset($_POST['PARAM_'.$i]) && !is_array($_POST['PARAM_'.$i])) {
			$rep = $_POST['REP_ID'];
			setcookie("select[$rep][$i]", $_POST['PARAM_'.$i], time()+60*60*24*user_save_report_selections()); // days from user_save_report_selections()
		}	
	}
}	

if (isset($_GET['xls']) || isset($_GET['xml']))
{
	$filename = $_GET['filename'];
	$unique_name = preg_replace('/[^0-9_a-z.\-]/i', '', $_GET['unique']);
	$path =  company_path(). '/pdf_files/';
	header("Content-type: ". (isset($_GET['xls']) ? "application/vnd.ms-excel" : "text/xml"));
	header("Content-Disposition: attachment; filename=$filename" );
	header("Expires: 0");
	header("Cache-Control: must-revalidate, post-check=0,pre-check=0");
	header("Pragma: public");
	echo file_get_contents($path.$unique_name);
	exit();
}

if (!isset($_POST['REP_ID'])) {	// print link clicked
	$def_pars = array(0, 0, '', '', 0, '', '', 0); //default values
	$rep = $_POST['REP_ID'] = $_GET['REP_ID'];
	for($i=0; $i<8; $i++) {
		$_POST['PARAM_'.$i] = isset($_GET['PARAM_'.$i]) 
			? $_GET['PARAM_'.$i] : $def_pars[$i];
	}
}

$rep = preg_replace('/[^a-z_0-9]/i', '', $_POST['REP_ID']);

$rep_file = find_custom_file("/reporting/rep$rep.php");

if ($rep_file) {
	require($rep_file);
} else
	display_error("Cannot find report file '$rep'");
exit();

