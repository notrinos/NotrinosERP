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
date validation and parsing functions

These functions refer to the global variable defining the date format
The date format is defined in config.php called dateformats
this can be a string either 'd/m/Y' for UK/Australia/New Zealand dates or
'm/d/Y' for US/Canada format dates depending on setting in preferences.

*/

function __date($year, $month, $day) {
	global $SysPrefs, $tmonths;
	
	$how = user_date_format();
	$sep = $SysPrefs->dateseps[user_date_sep()];
	$day = (int)$day;
	$month = (int)$month;
	if ($how < 3) {
		if ($day < 10)
			$day = '0'.$day;
		if ($month < 10)
			$month = '0'.$month;
	}		
	if ($how == 0)
		return $month.$sep.$day.$sep.$year;
	elseif ($how == 1)
		return $day.$sep.$month.$sep.$year;
	elseif ($how == 2)
		return $year.$sep.$month.$sep.$day;
	elseif ($how == 3)
		return $tmonths[$month].$sep.$day.$sep.$year;
	elseif ($how == 4)
		return $day.$sep.$tmonths[$month].$sep.$year;
	else
		return $year.$sep.$tmonths[$month].$sep.$day;
}

function is_date($date_) {
	global $SysPrefs;

	if ($date_ == null || $date_ == '')
		return 0;
	$how = user_date_format();
	$sep = $SysPrefs->dateseps[user_date_sep()];

	$date_ = trim($date_);
	$date = str_replace($sep, '', $date_);
	
	if ($how > 2) {
		global $tmonths;
		$dd = explode($sep, $date_);
		if ($how == 3) {
			$day = $dd[1];
			$month = array_search($dd[0], $tmonths);
			$year = $dd[2];
		}
		elseif ($how == 4) {
			$day = $dd[0];
			$month = array_search($dd[1], $tmonths);
			$year = $dd[2];
		}
		else {
			$day = $dd[2];
			$month = array_search($dd[1], $tmonths);
			$year = $dd[0];
		}
		if ($year < 1000)
			return 0;
	}
	elseif (strlen($date) == 6) {
		if ($how == 0) {
			$day = substr($date,2,2);
			$month = substr($date,0,2);
			$year = substr($date,4,2);
		} 
		elseif ($how == 1) {
			$day = substr($date,0,2);
			$month = substr($date,2,2);
			$year = substr($date,4,2);
		}
		else {
			$day = substr($date,4,2);
			$month = substr($date,2,2);
			$year = substr($date,0,2);
		}
	}
	elseif (strlen($date) == 8) {
		if ($how == 0) {
			$day = substr($date,2,2);
			$month = substr($date,0,2);
			$year = substr($date,4,4);
		} 
		elseif ($how == 1) {
			$day = substr($date,0,2);
			$month = substr($date,2,2);
			$year = substr($date,4,4);
		} 
		else {
			$day = substr($date,6,2);
			$month = substr($date,4,2);
			$year = substr($date,0,4);
		}
	}
	if (!isset($year)|| (int)$year > 9999)
		return 0;

	if (is_long((int)$day) && is_long((int)$month) && is_long((int)$year)) {
		global $SysPrefs;
		if ($SysPrefs->date_system == 1)
			list($year, $month, $day) = jalali_to_gregorian($year, $month, $day);  
		elseif ($SysPrefs->date_system == 2)
			list($year, $month, $day) = islamic_to_gregorian($year, $month, $day);  
		if (checkdate((int)$month, (int)$day, (int)$year))
			return 1;
		else
			return 0;
	}
	else /*Can't be in an appropriate DefaultDateFormat */
		return 0;
} //end of is_date function

function Today() {
	global $SysPrefs;

	$year = date('Y');
	$month = date('n');
	$day = date('j');
	if ($SysPrefs->date_system == 1)
		list($year, $month, $day) = gregorian_to_jalali($year, $month, $day);
	elseif ($SysPrefs->date_system == 2)	
		list($year, $month, $day) = gregorian_to_islamic($year, $month, $day);
	return __date($year, $month, $day);	
}

function Now() {
	if (user_date_format() == 0)
		return date('h:i a');
	else
		return date('H:i');
}
//
//	Retrieve and optionaly set default date for new document.
//
function new_doc_date($date=null) {
	if (isset($date) && $date != '')
		$_SESSION['_default_date'] = $date;

	if (!isset($_SESSION['_default_date']) || !sticky_doc_date())
		$_SESSION['_default_date'] = Today();

	return $_SESSION['_default_date'];
}

function is_date_in_fiscalyear($date, $convert=false) {
	global $path_to_root;
	include_once($path_to_root.'/admin/db/fiscalyears_db.inc');

	if ($convert)
		$date2 = sql2date($date);
	else
		$date2 = $date;

	if (is_date_closed($date2))
		return 0;

	if (user_check_access('SA_MULTIFISCALYEARS')) // allow all open years for this one
		return is_date_in_fiscalyears($date2, false);

	$myrow = get_current_fiscalyear();
	$begin = sql2date($myrow['begin']);
	$end = sql2date($myrow['end']);
	if (date1_greater_date2($begin, $date2) || date1_greater_date2($date2, $end))
		return 0;
	return 1;
}

function is_date_closed($date) {
	return !date1_greater_date2($date, sql2date(get_company_pref('gl_closing_date')));
}

function begin_fiscalyear() {
	global $path_to_root;
	include_once($path_to_root.'/admin/db/fiscalyears_db.inc');

	$myrow = get_current_fiscalyear();
	return sql2date($myrow['begin']);
}

function end_fiscalyear() {
	global $path_to_root;
	include_once($path_to_root.'/admin/db/fiscalyears_db.inc');

	$myrow = get_current_fiscalyear();
	return sql2date($myrow['end']);
}

function begin_month($date) {
	global $SysPrefs;
	list($day, $month, $year) = explode_date_to_dmy($date);
	if ($SysPrefs->date_system == 1)
		list($year, $month, $day) = gregorian_to_jalali($year, $month, $day);
	elseif ($SysPrefs->date_system == 2)	
		list($year, $month, $day) = gregorian_to_islamic($year, $month, $day);
	return __date($year, $month, 1);
}

function days_in_month($month, $year) {
	global $SysPrefs;

	if ($SysPrefs->date_system == 1)
		$days_in_month = array(31, 31, 31, 31, 31, 31, 30, 30, 30, 30, 30, ((((((($year - (($year > 0) ? 474 : 473)) % 2820) + 474) + 38) * 682) % 2816) < 682 ? 30 : 29));
	elseif ($SysPrefs->date_system == 2)
		$days_in_month = array(30, 29, 30, 29, 30, 29, 30, 29, 30, 29, 30, (((((11 * $year) + 14) % 30) < 11) ? 30 : 29));
	else // gregorian date
		$days_in_month = array(31, ((!($year % 4 ) && (($year % 100) || !($year % 400)))?29:28), 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);

	return $days_in_month[$month - 1];
}

function end_month($date) {
	global $SysPrefs;

	list($day, $month, $year) = explode_date_to_dmy($date);
	if ($SysPrefs->date_system == 1)
		list($year, $month, $day) = gregorian_to_jalali($year, $month, $day);
	elseif ($SysPrefs->date_system == 2)
		list($year, $month, $day) = gregorian_to_islamic($year, $month, $day);

	return __date($year, $month, days_in_month($month, $year));
}

function add_days($date, $days) {// accepts negative values as well
	global $SysPrefs;
	list($day, $month, $year) = explode_date_to_dmy($date);
	$timet = mktime(0,0,0, $month, $day + $days, $year);
	if ($SysPrefs->date_system == 1 || $SysPrefs->date_system == 2) {
		if ($SysPrefs->date_system == 1)
			list($year, $month, $day) = gregorian_to_jalali(date('Y', $timet), date('n', $timet), date('j', $timet));
		elseif ($SysPrefs->date_system == 2)
			list($year, $month, $day) = gregorian_to_islamic(date('Y', $timet), date('n', $timet), date('j', $timet));
		return __date($year, $month, $day);
	}
	list($year, $month, $day) = explode('-', date('Y-m-d', $timet));
	return __date($year, $month, $day);
}

function add_months($date, $months) { // accepts negative values as well
	global $SysPrefs;

	list($day, $month, $year) = explode_date_to_dmy($date);

	$months += $year*12+$month;
	$month = ($months-1)%12+1;
	$year = ($months-$month)/12;

	$timet = mktime(0,0,0, $month, min($day, days_in_month($month, $year)), $year);

	if ($SysPrefs->date_system == 1 || $SysPrefs->date_system == 2) {
		if ($SysPrefs->date_system == 1)
			list($year, $month, $day) = gregorian_to_jalali(date('Y', $timet), date('n', $timet), date('j', $timet));
		elseif ($SysPrefs->date_system == 2)
			list($year, $month, $day) = gregorian_to_islamic(date('Y', $timet), date('n', $timet), date('j', $timet));
		return __date($year, $month, $day);
	}
	list($year, $month, $day) = explode('-', date('Y-m-d', $timet));
	return __date($year, $month, $day);
}

function add_years($date, $years) {// accepts negative values as well
	global $SysPrefs;

	list($day, $month, $year) = explode_date_to_dmy($date);
	$timet = Mktime(0,0,0, $month, $day, $year + $years);
	if ($SysPrefs->date_system == 1 || $SysPrefs->date_system == 2) {
		if ($SysPrefs->date_system == 1)
			list($year, $month, $day) = gregorian_to_jalali(date('Y', $timet), date('n', $timet), date('j', $timet));
		elseif ($SysPrefs->date_system == 2) 	
			list($year, $month, $day) = gregorian_to_islamic(date('Y', $timet), date('n', $timet), date('j', $timet));
		return __date($year, $month, $day);
	}
	list($year, $month, $day) = explode('-', date('Y-m-d', $timet));
	return __date($year, $month, $day);
}

//_______________________________________________________________

function sql2date($date_) {
	global $SysPrefs;

	//for MySQL dates are in the format YYYY-mm-dd

	if (strpos($date_, '/')) // In MySQL it could be either / or -
		list($year, $month, $day) = explode('/', $date_);
	elseif (strpos ($date_, '-'))
		list($year, $month, $day) = explode('-', $date_);
	if (!isset($day)) // data format error
		return '';

	if (strlen($day) > 4) /*chop off the time stuff */
		$day = substr($day, 0, 2);
	
	if ($SysPrefs->date_system == 1)
		list($year, $month, $day) = gregorian_to_jalali($year, $month, $day);
	elseif ($SysPrefs->date_system == 2)
		list($year, $month, $day) = gregorian_to_islamic($year, $month, $day);
	return __date($year, $month, $day);	
} // end function sql2date


function date2sql($date_) {
	global $SysPrefs, $tmonths;
/* takes a date in a the format specified in $DefaultDateFormat
and converts to a yyyy/mm/dd format */

	$how = user_date_format();
	$sep = $SysPrefs->dateseps[user_date_sep()];

	$date_ = trim($date_);
	if ($date_ == null || strlen($date_) == 0)
		return '';

	$year = $month = $day = 0;
	// Split up the date by the separator based on 'how' to split it
	if ($how == 0 || $how == 3) // MMDDYYYY or MmmDDYYYY
		list($month, $day, $year) = explode($sep, $date_);
	elseif ($how == 1 || $how == 4) // DDMMYYYY or DDMmYYYY
		list($day, $month, $year) = explode($sep, $date_);
	else // $how == 2 || $how == 5, YYYYMMDD or YYYYMmmDD
		list($year, $month, $day) = explode($sep, $date_);

	if ($how > 2) {
		global $tmonths;
		$month = array_search($month, $tmonths);
	}
	if ($year+$day+$month) {
		//to modify assumption in 2030
		if ($SysPrefs->date_system == 0 || $SysPrefs->date_system == 3) {
			if ((int)$year < 60)
				$year = '20'.$year;
			elseif ((int)$year > 59 && (int)$year < 100)
				$year = '19'.$year;
		}
		if ((int)$year > 9999)
			return 0;
		if ($SysPrefs->date_system == 1)
			list($year, $month, $day) = jalali_to_gregorian($year, $month, $day); 
		elseif ($SysPrefs->date_system == 2)
			list($year, $month, $day) = islamic_to_gregorian($year, $month, $day); 
	}
	return sprintf('%04d-%02d-%02d', $year, $month, $day);
}// end of function

/**
 *	Compare dates in sql format.
 *	Return +1 if sql date1>date2, -1 if date1<date2,
 *  or 0 if dates are equal.
 */
function sql_date_comp($date1, $date2) {
	@list($year1, $month1, $day1) = explode('-', $date1);
	@list($year2, $month2, $day2) = explode('-', $date2);

	if ($year1 != $year2)
		return $year1 < $year2 ? -1 : +1;
	
	elseif ($month1 != $month2)
		return $month1 < $month2 ? -1 : +1;
	
	elseif ($day1 != $day2)
		return $day1 < $day2 ? -1 : +1;
	
	return 0;
}
/*
	Compare dates in user format.
*/
function date_comp($date1, $date2) {
	$date1 = date2sql($date1);
	$date2 = date2sql($date2);

	return sql_date_comp($date1, $date2);
}

function date1_greater_date2 ($date1, $date2) {

/* returns 1 true if date1 is greater than date_ 2 */

	$date1 = date2sql($date1);
	$date2 = date2sql($date2);

	@list($year1, $month1, $day1) = explode('-', $date1);
	@list($year2, $month2, $day2) = explode('-', $date2);

	if ($year1 > $year2)
		return 1;
	
	elseif ($year1 == $year2) {
		if ($month1 > $month2)
			return 1;
		
		elseif ($month1 == $month2) {
			if ($day1 > $day2)
				return 1;
		}
	}
	return 0;
}

function date_diff2 ($date1, $date2, $period) {

/* expects dates in the format specified in $DefaultDateFormat - period can be one of 'd','w','y','m'
months are assumed to be 30 days and years 365.25 days This only works
provided that both dates are after 1970. Also only works for dates up to the year 2035 ish */

	$date1 = date2sql($date1);
	$date2 = date2sql($date2);
	list($year1, $month1, $day1) = explode('-', $date1);
	list($year2, $month2, $day2) = explode('-', $date2);

	$stamp1 = mktime(0,0,0, (int)$month1, (int)$day1, (int)$year1);
	$stamp2 = mktime(0,0,0, (int)$month2, (int)$day2, (int)$year2);
	$difference = $stamp1 - $stamp2;

/* difference is the number of seconds between each date negative if date_ 2 > date_ 1 */

	switch ($period) {
		case 'd':
			return (int)($difference / (24 * 60 * 60));
		case 'w':
			return (int)($difference / (24 * 60 * 60 * 7));
		case 'm':
			return (int)($difference / (24 * 60 * 60 * 30));
		case 's':
			return $difference;
		case 'y':
			return (int)($difference / (24 * 60 * 60 * 365.25));
		default:
			Return 0;
	}
}

function explode_date_to_dmy($date_) {
	$date = date2sql($date_);
	if ($date == '')
		return array(0,0,0);
	
	list($year, $month, $day) = explode('-', $date);
	return array($day, $month, $year);
}

function div($a, $b) {
	return (int) ($a / $b);
}
/* Based on convertor to and from Gregorian and Jalali calendars.
   Copyright (C) 2000  Roozbeh Pournader and Mohammad Toossi 
   Released under GNU General Public License */

function gregorian_to_jalali ($g_y, $g_m, $g_d) {
	$g_days_in_month = array(31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);
	$j_days_in_month = array(31, 31, 31, 31, 31, 31, 30, 30, 30, 30, 30, 29);

	$gy = $g_y - 1600;
	$gm = $g_m - 1;
	$gd = $g_d - 1;

	$g_day_no = 365 * $gy + div($gy + 3, 4) - div($gy + 99, 100) + div($gy + 399, 400);

	for ($i = 0; $i < $gm; ++$i)
		$g_day_no += $g_days_in_month[$i];
	if ($gm > 1 && (($gy % 4 == 0 && $gy % 100 != 0) || ($gy % 400 == 0)))
		/* leap and after Feb */
		$g_day_no++;
	$g_day_no += $gd;
	$j_day_no = $g_day_no - 79;

	$j_np = div($j_day_no, 12053); /* 12053 = 365*33 + 32/4 */
	$j_day_no %= 12053;

	$jy = 979 + 33 * $j_np + 4 * div($j_day_no, 1461); /* 1461 = 365*4 + 4/4 */

	$j_day_no %= 1461;

	if ($j_day_no >= 366) {
		$jy += div($j_day_no - 1, 365);
		$j_day_no = ($j_day_no - 1) % 365;
	}

	for ($i = 0; $i < 11 && $j_day_no >= $j_days_in_month[$i]; ++$i)
		$j_day_no -= $j_days_in_month[$i];
	$jm = $i + 1;
	$jd = $j_day_no + 1;

	return array($jy, $jm, $jd);
}

function jalali_to_gregorian($j_y, $j_m, $j_d) {
	$g_days_in_month = array(31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);
	$j_days_in_month = array(31, 31, 31, 31, 31, 31, 30, 30, 30, 30, 30, 29);

	$jy = $j_y - 979;
	$jm = $j_m - 1;
	$jd = $j_d - 1;

	$j_day_no = 365 * $jy + div($jy, 33) * 8 + div($jy % 33 + 3, 4);
	for ($i = 0; $i < $jm; ++$i)
		$j_day_no += $j_days_in_month[$i];

	$j_day_no += $jd;

	$g_day_no = $j_day_no + 79;

	$gy = 1600 + 400 * div($g_day_no, 146097); /* 146097 = 365*400 + 400/4 - 400/100 + 400/400 */
	$g_day_no %= 146097;

	$leap = true;
	if ($g_day_no >= 36525) {/* 36525 = 365*100 + 100/4 */
		$g_day_no--;
		$gy += 100 * div($g_day_no,  36524); /* 36524 = 365*100 + 100/4 - 100/100 */
		$g_day_no %= 36524;

		if ($g_day_no >= 365)
			$g_day_no++;
		else
			$leap = false;
	}

	$gy += 4 * div($g_day_no, 1461); /* 1461 = 365*4 + 4/4 */
	$g_day_no %= 1461;

	if ($g_day_no >= 366) {
		$leap = false;

		$g_day_no--;
		$gy += div($g_day_no, 365);
		$g_day_no %= 365;
	}

	for ($i = 0; $g_day_no >= $g_days_in_month[$i] + ($i == 1 && $leap); $i++)
		$g_day_no -= $g_days_in_month[$i] + ($i == 1 && $leap);
	$gm = $i + 1;
	$gd = $g_day_no + 1;

	return array($gy, $gm, $gd);
}
/* Based on Hidri Date Script 
   Released under GNU General Public License */
function gregorian_to_islamic($g_y, $g_m, $g_d) {
	$y = $g_y;
	$m = $g_m;
	$d = $g_d;
	if (($y > 1582) || (($y == 1582) && ($m > 10)) || (($y == 1582) && ($m == 10) && ($d > 14)))
		$jd = (int)((1461 * ($y + 4800 + (int)(($m - 14) / 12)))/ 4) + (int)((367 * ($m - 2 - 12 * ((int)(($m - 14) / 12)))) / 12) - (int)((3 * ((int)(($y + 4900 + (int)(($m - 14) / 12)) / 100))) / 4) + $d - 32075;
	else
		$jd = 367 * $y - (int)((7 * ($y + 5001 + (int)(($m - 9) / 7))) / 4) + (int)((275 * $m) / 9) + $d + 1729777;
	
	$l = $jd - 1948440 + 10632;
	$n = (int)(($l - 1) / 10631);
	$l = $l - 10631 * $n + 354;
	$j = ((int)((10985 - $l) / 5316)) * ((int)((50 * $l) / 17719)) + ((int)($l / 5670)) * ((int)((43 * $l) / 15238));
	$l = $l - ((int)((30 - $j) / 15)) * ((int)((17719 * $j) / 50)) - ((int)($j / 16)) * ((int)((15238 * $j) / 43)) + 29;
	$m = (int)((24 * $l) / 709);
	$d = $l - (int)((709 * $m) / 24);
	$y = 30 * $n + $j - 30;
	return array($y, $m, $d);
}

function islamic_to_gregorian($i_y, $i_m, $i_d) {
	$y = $i_y;   
	$m = $i_m;
	$d = $i_d;

	$jd = (int)((11 * $y + 3) / 30) + 354 * $y + 30 * $m - (int)(($m - 1) / 2) + $d + 1948440 - 385;
	if ($jd > 2299160) {
		$l = $jd + 68569;
		$n = (int)((4 * $l) / 146097);
		$l = $l - (int)((146097 * $n + 3) / 4);
		$i = (int)((4000 * ($l + 1)) / 1461001);
		$l = $l - (int)((1461 * $i) / 4) + 31;
		$j = (int)((80 * $l) / 2447);
		$d = $l - (int)((2447 * $j) / 80);
		$l= (int)($j / 11);
		$m = $j + 2 - 12 * $l;
		$y = 100 * ($n - 49) + $i + $l;
	}
	else {
		$j = $jd + 1402;
		$k = (int)(($j - 1) / 1461);
		$l = $j - 1461 * $k;
		$n = (int)(($l - 1) / 365) - (int)($l / 1461);
		$i = $l - 365 * $n + 30;
		$j = (int)((80 * $i) / 2447);
		$d = $i - (int)((2447 * $j) / 80);
		$i = (int)($j / 11);
		$m = $j + 2 - 12 * $i;
		$y = 4 * $k + $n + $i - 4716;
	}
	return array($y, $m, $d);
}