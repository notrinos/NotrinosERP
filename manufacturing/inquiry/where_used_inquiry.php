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
$page_security = 'SA_WORKORDERANALYTIC';
$path_to_root = "../..";
include($path_to_root . "/includes/db_pager.inc");
include($path_to_root . "/includes/session.inc");

page(_($help_context = "Inventory Item Where Used Inquiry"));

include($path_to_root . "/includes/ui.inc");

check_db_has_stock_items(_("There are no items defined in the system."));

start_form(false, true);

if (!isset($_POST['stock_id']))
	$_POST['stock_id'] = get_global_stock_item();

echo "<center>" . _("Select an item to display its parent item(s).") . "&nbsp;";
echo stock_items_list('stock_id', $_POST['stock_id'], false, true);
echo "<hr></center>";

set_global_stock_item($_POST['stock_id']);
//-----------------------------------------------------------------------------
function select_link($row)
{
	return  pager_link( $row["parent"]. " - " . $row["description"],
    		"/manufacturing/manage/bom_edit.php?stock_id=" . $row["parent"]);
}

$sql = get_sql_for_where_used(get_post('stock_id'));

   $cols = array(
   	_("Parent Item") => array('fun'=>'select_link'), 
	_("Work Centre"), 
	_("Location"), 
	_("Quantity Required")
	);

$table =& new_db_pager('usage_table', $sql, $cols);

$table->width = "80%";
display_db_pager($table);

end_form();
end_page();

