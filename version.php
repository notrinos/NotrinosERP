<?php
//==========================================================================================
//
// Settings in this file can be automatically updated at any time during software update.
//

// Versions used by source/database version compatibility checks. Do not change.
$db_version = '0.1';
$src_version = '0.5';

// application version - can be overriden in config.php
$version = isset($SysPrefs->version) ? $SysPrefs->version : $src_version;

//======================================================================
// Extension packages repository settings 
//
// Extensions repository. Can be overriden in config.php

$repo_auth = isset($SysPrefs->repo_auth) ? $SysPrefs->repo_auth :
array(
	'login' => 'anonymous',
	'pass' => 'password',
	'host' => 'repo.notrinos.com', // repo server address
	'branch' => '0.1'	// Repository branch for current sources version
);
