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
//$path_to_root="..";
define('MAX_DEADLOCK_RETRY', 3);
include_once(__DIR__.'/json_sql.inc');

// PHP 8.1 is the minimum (owner decision D-04); the mysql_* API no longer exists there.
// Without PDO_MySQL the mysqli wrapper is loaded, whose db_extension_exists() reports a missing extension.
if (class_exists('PDO') && in_array('mysql', PDO::getAvailableDrivers()))
	include_once('connect_db_pdo.inc');
else
	include_once('connect_db_mysqli.inc');

function db_get_version() {
	$result = db_query("SELECT VERSION()");
	$row = db_fetch($result);
	return $row[0];
}

/*
	Converts encoding name to mysql standard.
*/
function get_mysql_encoding_name($encoding) {
	$db_encoding = array(
		'UTF-8' => 'utf8mb4',
		'ISO-8859-1' => 'latin1',
		'ISO-8859-2' => 'latin2',
		'ISO-8859-7' => 'greek',
		'ISO-8859-8' => 'hebrew',
		'ISO-8859-9' => 'latin5',
		'ISO-8859-13' => 'latin7',
		'KOI8-R' => 'koi8r',
		'KOI8-U' => 'koi8u',
		'CP850' => 'cp850',
		'CP866' => 'cp866',
		'CP932' => 'cp932',
		'CP1250' => 'cp1250',
		'CP1251' => 'cp1251',
		'CP1252' => 'latin1',
		'CP1256' => 'cp1256',
		'CP1257' => 'cp1257',
		'GB2312' => 'gb2312',
		'EUC-JP' => 'ujis',
		'EUC-KR' => 'euckr',
		'BIG5' => 'big5',
		'GBK' => 'gbk',
		'SHIFT_JIS' => 'sjis',
		'TIS-620' => 'tis620',
		'ASCII' => 'ascii',
	);
	$encoding = strtoupper($encoding);

	return isset($db_encoding[$encoding]) ? $db_encoding[$encoding] : null;
}

/*
	Returns 'best' mysql collation for various FA backend language codes.
*/
/**
 * Database collation of a NotrinosERP collation key (config_db.php 'collation', for example utf8_xx or utf8_is).
 * Keys keep their old names; every collation is utf8mb4.
 */
function get_mysql_collation($lang=null) {
	if (!$lang)
		$lang = 'utf8_'.substr($_SESSION['language']->code, 0, 2);

	$db_collation = array(
		'utf8_is' => 'utf8mb4_icelandic_ci',
		'utf8_lv' => 'utf8mb4_latvian_ci',
		'utf8_ro' => 'utf8mb4_romanian_ci',
		'utf8_sl' => 'utf8mb4_slovenian_ci',
		'utf8_pl' => 'utf8mb4_polish_ci',
		'utf8_et' => 'utf8mb4_estonian_ci',
		'utf8_es' => 'utf8mb4_spanish_ci', // or 'spanish2',
		'utf8_sw' => 'utf8mb4_swedish_ci',
		'utf8_tr' => 'utf8mb4_turkish_ci',
		'utf8_cs' => 'utf8mb4_czech_ci',
		'utf8_da' => 'utf8mb4_danish_ci',
		'utf8_lt' => 'utf8mb4_lithuanian_ci',
		'utf8_sk' => 'utf8mb4_slovak_ci',
		'utf8_sp' => 'utf8mb4_spanish2_ci',
		'utf8_fa' => 'utf8mb4_persian_ci',
		'utf8_hu' => 'utf8mb4_hungarian_ci',
		'utf8_fr' => 'utf8mb4_roman_ci',
		'utf8_it' => 'utf8mb4_roman_ci',
	);

	return isset($db_collation[$lang]) ? $db_collation[$lang] : 'utf8mb4_unicode_ci';
}

/*
	Check database default charset.
*/
function db_get_default_charset() {
	$result = db_query("SELECT @@character_set_database");
	$var = db_fetch($result);
	return $var[0];
}

function db_get_view_schema($view) {
	global $db;
	$schema = NULL;
	$qry = "select view_definition from information_schema.views where table_schema=DATABASE() and table_name='$view'";
	$res = db_query($qry,'Failed to select view schema');
	if ($res && db_num_rows($res) == 1) {
		$row = db_fetch_assoc($res);
		$schema = $row['view_definition'];
	}
	return $schema;
}

/*
	SQL db profiling stub
*/
if (!function_exists('db_profile')) {
	function db_profile($sql=false){
	}
}
