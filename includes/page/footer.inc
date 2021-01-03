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
function page_footer($no_menu=false, $is_index=false)
{
	global $path_to_root, $js_lib, $Validate, $Editors, $Ajax, $sql_queries;

	$page_header_args = func_get_args();
	hook_invoke_all('pre_footer', $page_header_args);

	include_once($path_to_root."/themes/".user_theme()."/renderer.php");
	$rend = new renderer();
	$rend->menu_footer($no_menu, $is_index);

	$edits = "editors = ".$Ajax->php2js($Editors).";";
	$Ajax->addScript('editors', $edits);

	echo "<script type='text/javascript'>
		_focus = '" . get_post('_focus') . "';
		_validate = " . $Ajax->php2js($Validate).";
		var $edits
	</script>";

   	add_user_js_data();
	echo "\n<script type=\"text/javascript\"><!--\n";

	foreach($js_lib as $text)
	  	echo $text;
	echo "\n--></script>\n";

	if ($sql_queries) {
		div_start('footer_debug');
		 echo "<div class='note_msg'>$sql_queries</div>";
		div_end();
	}
    echo "</body></html>\n";
}

