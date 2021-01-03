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
//$path_to_root="..";
define('MAX_DEADLOCK_RETRY', 3);

if (function_exists('mysqli_connect'))
	include_once("connect_db_mysqli.inc");
else
	include_once("connect_db_mysql.inc"); // use deprecated mysql_* API as fallback on old servers

function db_get_version()
{
	$result = db_query("SELECT VERSION()");
	$row = db_fetch($result);
	return $row[0];
}

/*
	Converts encoding name to mysql standard.
*/
function get_mysql_encoding_name($encoding)
{
	$db_encoding = array(
		'UTF-8' => 'utf8',
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
function get_mysql_collation($lang=null)
{
	if (!$lang)
		$lang = 'utf8_'.substr($_SESSION['language']->code, 0, 2);

	$db_collation = array(
		'utf8_is' => 'utf8_icelandic_ci',
		'utf8_lv' => 'utf8_latvian_ci',
		'utf8_ro' => 'utf8_romanian_ci',
		'utf8_sl' => 'utf8_slovenian_ci',
		'utf8_pl' => 'utf8_polish_ci',
		'utf8_et' => 'utf8_estonian_ci',
		'utf8_es' => 'utf8_spanish_ci', // or 'spanish2',
		'utf8_sw' => 'utf8_swedish_ci',
		'utf8_tr' => 'utf8_turkish_ci',
		'utf8_cs' => 'utf8_czech_ci',
		'utf8_da' => 'utf8_danish_ci',
		'utf8_lt' => 'utf8_lithuanian_ci',
		'utf8_sk' => 'utf8_slovak_ci',
		'utf8_sp' => 'utf8_spanish2_ci',
		'utf8_fa' => 'utf8_persian_ci',
		'utf8_hu' => 'utf8_hungarian_ci',
		'utf8_fr' => 'utf8_roman_ci',
		'utf8_it' => 'utf8_roman_ci',
	);

	return isset($db_collation[$lang]) ? $db_collation[$lang] : 'utf8_unicode_ci';
}

/*
	Later we assume that database with version less than 2.4 is old database, 
	which is subject to invalid encodings on text columns,
	so no SET NAMES or equivalent should be used.
*/
function db_fixed()
{
	$result = db_query("SELECT value FROM ".TB_PREF."sys_prefs WHERE name='version_id'");
	$data = db_fetch($result);
	return !db_num_rows($result) // new database is fixed by default 
		|| ($data[0] > "2.3rc");
}

/*
	Check database default charset.
*/
function db_get_default_charset()
{
	$result = db_query("SELECT @@character_set_database");
	$var = db_fetch($result);
	return $var[0];
}

function db_get_view_schema($view)
{
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
if (!function_exists('db_profile'))
{
	function db_profile($sql=false)
	{
	}
}
