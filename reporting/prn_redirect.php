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
/*
	Print request redirector. This file is fired via print link or 
	print button in reporting module. 
*/
$path_to_root = '..';
global $page_security;
$page_security = 'SA_OPEN';	// this level is later overriden in rep file
include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/reporting/includes/report_artifact_security.inc');

if (user_save_report_selections() > 0 && isset($_POST['REP_ID'])) {	// save parameters from Report Center
	for($i=0; $i<12; $i++) {
		if (isset($_POST['PARAM_'.$i]) && !is_array($_POST['PARAM_'.$i])) {
			$rep = $_POST['REP_ID'];
			setcookie("select[$rep][$i]", $_POST['PARAM_'.$i], time()+60*60*24*user_save_report_selections()); // days from user_save_report_selections()
		}	
	}
}	

if (isset($_GET['artifact'])) {
	$grant = report_artifact_consume((string)$_GET['artifact']);
	if ($grant === false) {
		http_response_code(404);
		header('Cache-Control: no-store, private');
		header('X-Content-Type-Options: nosniff');
		echo _('The report artifact is unavailable or no longer authorized.');
		exit();
	}

	while (ob_get_level() > 0)
		@ob_end_clean();
	$disposition = $grant['mime_type'] === 'application/pdf' ? 'inline' : 'attachment';
	header('Content-Type: '.$grant['mime_type']);
	header('Content-Disposition: '.$disposition.'; filename="'.$grant['filename'].'"');
	header('Content-Length: '.(int)$grant['bytes']);
	header('Cache-Control: no-store, private');
	header('Pragma: no-cache');
	header('X-Content-Type-Options: nosniff');
	readfile($grant['path']);
	@unlink($grant['path']);
	exit();
}

// Legacy filename-bearing downloads are intentionally invalidated. Generated
// artifacts must be private, principal-bound, permission-revalidated grants.
if (isset($_GET['xls']) || isset($_GET['xml']) || isset($_GET['unique'])) {
	report_artifact_log('legacy_download_denied', 'unknown', 'legacy_unbound');
	http_response_code(404);
	header('Cache-Control: no-store, private');
	header('X-Content-Type-Options: nosniff');
	echo _('The report artifact is unavailable or no longer authorized.');
	exit();
}

if (!isset($_POST['REP_ID'])) {	// print link clicked
	$def_pars = array(0, 0, '', '', 0, '', '', 0); //default values
	$rep = $_POST['REP_ID'] = $_GET['REP_ID'];
	for($i=0; $i<8; $i++) {
		$_POST['PARAM_'.$i] = isset($_GET['PARAM_'.$i]) ? $_GET['PARAM_'.$i] : $def_pars[$i];
	}
}

$rep = preg_replace('/[^a-z_0-9]/i', '', $_POST['REP_ID']);

$rep_file = find_custom_file('/reporting/rep'.$rep.'.php');

if ($rep_file)
	require($rep_file);
else
	display_error("Cannot find report file '$rep'");
exit();
