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
include_once($path_to_root . "/includes/remote_url.inc");
//---------------------------------------------------------------------------------------------
function get_exchange_rate($rate_id)
{
	$sql = "SELECT * FROM ".TB_PREF."exchange_rates WHERE id=".db_escape($rate_id);
	$result = db_query($sql, "could not get exchange rate for $rate_id");

	return db_fetch($result);
}

// Retrieves buy exchange rate for given currency/date, zero if no result
function get_date_exchange_rate($curr_code, $date_)
{
	$date = date2sql($date_);
	$sql = "SELECT rate_buy
			FROM ".TB_PREF."exchange_rates
			WHERE curr_code=".db_escape($curr_code)
				." AND date_='$date'";
	$result = db_query($sql, "could not get exchange rate for $curr_code - $date_");

	if(db_num_rows($result) == 0) 
		return 0;
	$row = db_fetch($result);
		return $row[0];
}

/*
	Return last exchange rate of $currency not later than $date.
*/
function get_last_exchange_rate($curr_code, $date_)
{
	$date = date2sql($date_);

	$sql = "SELECT rate_buy, max(date_) as date_
			FROM ".TB_PREF."exchange_rates
			WHERE curr_code = ".db_escape($curr_code)."
				AND date_ <= '$date' GROUP BY rate_buy ORDER BY date_ Desc LIMIT 1";

	$result = db_query($sql, "could not query exchange rates");


	if (db_num_rows($result) == 0)
		return false;

	return db_fetch_assoc($result);
}

//---------------------------------------------------------------------------------------------

function update_exchange_rate($curr_code, $date_, $buy_rate, $sell_rate)
{
	if (is_company_currency($curr_code))
		display_db_error("Exchange rates cannot be set for company currency", "", true);
			
	$date = date2sql($date_);
		
	$sql = "UPDATE ".TB_PREF."exchange_rates SET rate_buy=$buy_rate, rate_sell=".db_escape($sell_rate)
	." WHERE curr_code=".db_escape($curr_code)." AND date_='$date'";
				
	db_query($sql, "could not add exchange rate for $curr_code");
}

//---------------------------------------------------------------------------------------------

function add_exchange_rate($curr_code, $date_, $buy_rate, $sell_rate)
{
	if (is_company_currency($curr_code))
		display_db_error("Exchange rates cannot be set for company currency", "", true);

	$date = date2sql($date_);
		
	$sql = "INSERT INTO ".TB_PREF."exchange_rates (curr_code, date_, rate_buy, rate_sell)
		VALUES (".db_escape($curr_code).", '$date', ".db_escape($buy_rate)
		.", ".db_escape($sell_rate).")";
	db_query($sql, "could not add exchange rate for $curr_code");
}

//---------------------------------------------------------------------------------------------

function add_new_exchange_rate($curr_code, $date_, $ex_rate)
{
	if (is_company_currency($curr_code) || !$ex_rate)
		return;

	if (!get_date_exchange_rate($curr_code, $date_))
		add_exchange_rate($curr_code, $date_, $ex_rate, $ex_rate);
}

//---------------------------------------------------------------------------------------------

function delete_exchange_rate($rate_id)
{
	$sql = "DELETE FROM ".TB_PREF."exchange_rates WHERE id=".db_escape($rate_id);
	db_query($sql, "could not delete exchange rate $rate_id");
}

//-----------------------------------------------------------------------------
//	Retrieve exchange rate as of date $date from external source (usually inet)
//
//	Exchange rate for currency revaluation purposes is defined in FA as home_currency/curr_b ratio i.e.
//
//	amount [home] = amount [curr] * ex_rate
//
function retrieve_exrate($curr_b, $date)
{
	global $SysPrefs;

	$xchg_rate_provider = ((isset($SysPrefs->xr_providers) && isset($SysPrefs->dflt_xr_provider)) 
		? $SysPrefs->xr_providers[$SysPrefs->dflt_xr_provider] : 'ECB');

	$rate = hook_retrieve_exrate($curr_b, $date);
	if (is_numeric($rate))
		return $rate;
	return get_extern_rate($curr_b, $xchg_rate_provider, $date);
}
//-----------------------------------------------------------------------------

function get_extern_rate($curr_b, $provider = 'ECB', $date) 
{
	global	$path_to_root;

	if ($date != Today())	// no historical rates available
		return 0;

	$contents = '';
	$proto = 'http://';
	$curr_a = get_company_pref('curr_default');
	if ($provider == 'ECB')
    {
        $filename = "/stats/eurofxref/eurofxref-daily.xml";
        $site = "www.ecb.europa.eu";
        $proto = 'https://';
        $site_ip="172.230.157.137";
        $contents=file_get_contents($proto.$site.$filename);
    }
	elseif ($provider == 'YAHOO')
	{
		$filename = "/d/quotes.csv?s={$curr_a}{$curr_b}=X&f=sl1d1t1ba&e=.csv"; // new URL's for YAHOO
		$site = "download.finance.yahoo.com";
		$site_ip="203.84.220.151";
	}
	elseif ($provider == 'GOOGLE')
	{
		$filename = "/search?q={$curr_b}+to+{$curr_a}";
		$site = "https://www.google.com";
		$contents = file_get_contents($site.$filename);
	}
	elseif ($provider == 'BLOOMBERG')
	{
		$filename = "/quote/{$curr_b}{$curr_a}:CUR";
		$site = "www.bloomberg.com";
		$proto = 'https://';
		$contents=file_get_contents($proto.$site.$filename);
	}
	elseif ($provider == 'EXCHANGE-RATES.ORG')
	{
		$filename = "/converter/{$curr_b}/{$curr_a}/1";
		$site = "exchange-rates.org";
		$proto = 'https://';
		$contents=file_get_contents($proto.$site.$filename);
	}
	if (empty($contents)) {
		if (function_exists('curl_init'))
		{	// first check with curl as we can set short timeout;
			$retry = 1;
	 		do {
	       		$ch = curl_init();
    	   		curl_setopt ($ch, CURLOPT_URL, $proto.$site.$filename);
	       		curl_setopt ($ch, CURLOPT_COOKIEJAR, VARLIB_PATH."/cookie.txt");
    	   		curl_setopt ($ch, CURLOPT_HEADER, 0);
	       		curl_setopt ($ch, CURLOPT_RETURNTRANSFER, 1);
	       		// prevent warning while safe_mode/open_basedir on (redirection doesn't occur at least on ECB page)
		   		if (!ini_get('safe_mode') && !ini_get('open_basedir'))
    	   			curl_setopt ($ch, CURLOPT_FOLLOWLOCATION, 1);
	       		curl_setopt ($ch, CURLOPT_TIMEOUT, 3);
    	   		$contents = curl_exec ($ch);
	       		curl_close($ch);
				// due to resolver bug in some curl versions (e.g. 7.15.5) 
				// try again for constant IP.
				if (isset($site_ip))
		   			$site=$site_ip;
	   		} while( ($contents == '') && $retry--);
		} else {
			$contents = url_get_contents($proto.$site.$filename);
		}
	}
	$val = '';
	if ($provider == 'ECB')
	{
		$contents = str_replace ("<Cube currency='USD'", " <Cube currency='EUR' rate='1'/> <Cube currency='USD'", $contents);
		$from_mask = "|<Cube\s*currency=\'" . $curr_a . "\'\s*rate=\'([\d.,]*)\'\s*/>|i";
		preg_match ( $from_mask, $contents, $out );
		$val_a = isset($out[1]) ? $out[1] : 0;
		$val_a = str_replace ( ',', '', $val_a );
		$to_mask = "|<Cube\s*currency=\'" . $curr_b . "\'\s*rate=\'([\d.,]*)\'\s*/>|i";
		preg_match ( $to_mask, $contents, $out );
		$val_b = isset($out[1]) ? $out[1] : 0;
		$val_b = str_replace ( ',', '', $val_b );
		if ($val_b) 
		{
			$val = $val_a / $val_b;
		} 
		else 
		{
			$val = 0;
		}
	}
	elseif ($provider == 'YAHOO')
	{
		$array = explode(',',$contents); // New operations for YAHOO. Safer.
		$val = $array[1];
		if ($val != 0)
			$val = 1 / $val;
	}
	elseif ($provider == 'GOOGLE')
	{
		$data = preg_split('/\D\s(.*?)\s=\s/', $contents);
		$array = explode(" ", $data[1]);
		$exRate = $array[0];

		$val = str_replace (',', '', $exRate);
    }
	elseif ($provider == 'BLOOMBERG')
	{
		$val = getInnerStr($contents, ',"price":', ',"');
    }    
    elseif ($provider == 'EXCHANGE-RATES.ORG')
	{
		$val = getInnerStr($contents, '<span id="ctl00_M_lblToAmount">', '<');
		$val = str_replace (',', '', $val);
    }
	return $val;
}  /* end function get_extern_rate */

//-----------------------------------------------------------------------------

function get_sql_for_exchange_rates($curr)
{
	$sql = "SELECT date_, rate_buy, id
		FROM ".TB_PREF."exchange_rates "
		."WHERE curr_code=".db_escape($curr)."
		 ORDER BY date_ DESC";
	return $sql;	 
}

function getInnerStr($str, $start, $end) 
{
	// $start must be the first occurrence
	// $start must not be part of $end
	$val = '';
	$val = strstr($str, $start);
	$val = trim($val);
	$val = substr($val, strlen($start));
	$val = strtok($val, $end);
	return trim($val);
}
