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
//---------------------------------------------------------------------------------------------

function update_currency($curr_abrev, $symbol, $currency, $country, 
	$hundreds_name, $auto_update)
{
	$sql = "UPDATE ".TB_PREF."currencies SET currency=".db_escape($currency)
		.", curr_symbol=".db_escape($symbol).",	country=".db_escape($country)
		.", hundreds_name=".db_escape($hundreds_name)
		.",auto_update = ".db_escape($auto_update)
			." WHERE curr_abrev = ".db_escape($curr_abrev);

	db_query($sql, "could not update currency for $curr_abrev");
}

//---------------------------------------------------------------------------------------------

function add_currency($curr_abrev, $symbol, $currency, $country, 
	$hundreds_name, $auto_update)
{
	$sql = "INSERT INTO ".TB_PREF."currencies (curr_abrev, curr_symbol, currency, 
			country, hundreds_name, auto_update)
		VALUES (".db_escape($curr_abrev).", ".db_escape($symbol).", "
		.db_escape($currency).", ".db_escape($country).", "
		.db_escape($hundreds_name).",".db_escape($auto_update).")";

	db_query($sql, "could not add currency for $curr_abrev");
}

//---------------------------------------------------------------------------------------------

function delete_currency($curr_code)
{
	$sql="DELETE FROM ".TB_PREF."currencies WHERE curr_abrev=".db_escape($curr_code);
	db_query($sql, "could not delete currency	$curr_code");

	$sql="DELETE FROM ".TB_PREF."exchange_rates WHERE curr_code='$curr_code'";
	db_query($sql, "could not delete exchange rates for currency $curr_code");
}

//---------------------------------------------------------------------------------------------

function get_currency($curr_code)
{
	$sql = "SELECT * FROM ".TB_PREF."currencies WHERE curr_abrev=".db_escape($curr_code);
	$result = db_query($sql, "could not get currency $curr_code");

	$row = db_fetch($result);
	return $row;
}

//---------------------------------------------------------------------------------------------

function get_currencies($all=false)
{
	$sql = "SELECT * FROM ".TB_PREF."currencies";
	if (!$all) $sql .= " WHERE !inactive";
	return db_query($sql, "could not get currencies");
}

