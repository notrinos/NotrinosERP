<?php
//==========================================================================================
//
// Settings in this file can be automatically updated at any time during software update.
//

// Versions used by source/database version compatibility checks. Do not change.
$db_version = '1.1.0';
$src_version = '1.1.0';

// application version - can be overriden in config.php
$version = isset($SysPrefs->version) ? $SysPrefs->version : $src_version;

//======================================================================
// Extension packages repository settings
//
// Extensions repository. Can be overriden by $repo_auth in config.php.
// The public repository needs no login. A private repository sets its login and password in config.php;
// never put credentials in this file.

$repo_auth = isset($SysPrefs->repo_auth) ? $SysPrefs->repo_auth :
array(
	'host' => 'repo.notrinos.com', // repo server address
	'branch' => '1.0'	// Repository branch for current sources version
);
