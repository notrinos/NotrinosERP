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
function display_error($msg, $center=true)
{
	trigger_error($msg, E_USER_ERROR);
}

function display_notification($msg, $center=true)
{
	trigger_error($msg, E_USER_NOTICE);
}

function display_warning($msg, $center=true)
{
	trigger_error($msg, E_USER_WARNING);
}

function display_notification_centered($msg)
{
	display_notification($msg, true);
}

function display_caption($msg)
{
	echo "<caption>$msg</caption>\n";
}

function display_heading($msg)
{
	echo "<center><span class='headingtext'>$msg</span></center>\n";	
}

function display_heading2($msg)
{
	echo "<center><span class='headingtext2'>$msg</span></center>\n";	
}

function display_note($msg, $br=0, $br2=0, $extra="")
{
	for ($i = 0; $i < $br; $i++)
		echo "<br>";
	if ($extra != "")	
		echo "<center><span $extra>$msg</span></center>\n";	
	else	
		echo "<center><span>$msg</span></center>\n";	
	for ($i = 0; $i < $br2; $i++)
		echo "<br>";
}

function stock_item_heading($stock_id)
{
	if ($stock_id != "") 
	{
		$result = db_query("SELECT description, units FROM ".TB_PREF."stock_master WHERE stock_id=".db_escape($stock_id));
        $myrow = db_fetch_row($result);
        
    	display_heading("$stock_id - $myrow[0]"); 
    	$units = $myrow[1];
    	display_heading2(_("in units of : ") . $units);        	
	}
}

