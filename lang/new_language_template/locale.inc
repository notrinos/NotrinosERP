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
	This file contains templates for all country specific functions.
	If your locale needs special functionality provided by hook functions
	copy this file to respective lang/xx_XX directory and edit templates below.
	You can safely remove not used function templates.
	
	Name it after language code e.g. hooks_en_US
*/
class hooks_xx_XX extends hooks {
/*
	//
	// Price in words. $doc_type is set to document type and can be used to suppress 
	// price in words printing for selected document types.
	// Used instead of built in simple english price_in_words() function.
	//
	//	Returns: amount in words as string.
	
	function price_in_words($amount, $doc_type)
	{
	}
*/
/*
	//
	// Exchange rate currency $curr as on date $date.
	// Keep in mind FA has internally implemented 3 exrate providers
	// If any of them supports your currency, you can simply use function below
	// with apprioprate provider set, otherwise implement your own.
	// Returns: $curr value in home currency units as a real number.
	
	function retrieve_ex_rate($curr, $date)
	{
	 	$provider = 'ECB'; // 'ECB', 'YAHOO' or 'GOOGLE'
		return get_extern_rate($curr, $provider, $date);
	}
*/
/*
	// Generic function called at the end of Tax Report (report 709)
	// Can be used e.g. for special database updates on every report printing
	// or to print special tax report footer 
	//
	// Returns: nothing
	function tax_report_done()
	{
	}
*/
}
