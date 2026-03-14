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

function display_error($msg) {
	fa_trigger_error($msg, E_USER_ERROR);
}

function display_notification($msg) {
	fa_trigger_error($msg, E_USER_NOTICE);
}

function display_warning($msg) {
	fa_trigger_error($msg, E_USER_WARNING);
}

function display_notification_centered($msg) {
	display_notification($msg);
}

function display_caption($msg) {
	echo '<caption>'.$msg."</caption>\n";
}

function display_heading($msg) {
	echo "<h2 class='page-heading'>".$msg."</h2>\n";
}

function display_heading2($msg) {
	echo "<h3 class='page-heading' style='font-size:15px;'>".$msg."</h3>\n";
}

function display_note($msg, $br=0, $br2=0, $extra='') {
	for ($i = 0; $i < $br; $i++)
		echo '<br>';
	echo "<div class='alert alert-info'".($extra != '' ? ' '.$extra : '')."'>".$msg."</div>\n";
	for ($i = 0; $i < $br2; $i++)
		echo '<br>';
}

/**
 * Emit a semantic success alert block.
 *
 * @param string $msg
 * @return void
 */
function display_notification_alert($msg) {
	echo "<div class='alert alert-success'>".$msg."</div>\n";
}

/**
 * Emit a semantic warning alert block.
 *
 * @param string $msg
 * @return void
 */
function display_warning_alert($msg) {
	echo "<div class='alert alert-warning'>".$msg."</div>\n";
}

/**
 * Emit a semantic danger alert block.
 *
 * @param string $msg
 * @return void
 */
function display_error_alert($msg) {
	echo "<div class='alert alert-danger'>".$msg."</div>\n";
}

function stock_item_heading($stock_id) {
	if ($stock_id != '') {
		$sql = "SELECT description, units FROM ".TB_PREF."stock_master WHERE stock_id=".db_escape($stock_id);
		$result = db_query($sql, 'could not get stock data');
		$myrow = db_fetch_row($result);
		
		display_heading($stock_id.' - '.$myrow[0]);
		display_heading2(_('in units of : ').$myrow[1]);
	}
}
